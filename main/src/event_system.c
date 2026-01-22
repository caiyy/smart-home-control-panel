/*
 * SPDX-FileCopyrightText: 2022-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "cJSON.h"
#include "event_system.h"
#include "esp_log.h"
#include "lvgl.h"
#include "ntp_time.h"
#include "esp_task_wdt.h"
#include "esp32_mqtt_client.h"
#include "http_service.h" // Added as per instruction
#include "homeassistant.h"
#include "album_art_manager.h"
#include "ui_common.h"

static const char *TAG = "event_system";

// 外部WiFi状态检查函数
extern bool is_wifi_connected(void);

// 事件队列句柄
QueueHandle_t g_system_event_queue = NULL;    // 系统事件队列
QueueHandle_t g_ui_event_queue = NULL;        // UI事件队列

// 外部变量声明
extern lv_subject_t song_lyrics_subject;
extern lv_subject_t song_name_subject;
extern lv_subject_t song_artist_subject;
extern lv_subject_t song_time_subject;
extern lv_subject_t play_progress_subject;
extern lv_subject_t volume_subject_value;
extern lv_subject_t power_subject;
extern lv_subject_t daily_energy_subject;
extern lv_subject_t monthly_energy_subject;
extern lv_subject_t cover_img_subject;

// 天气和室内相关的Subject声明
extern lv_subject_t weather_desc_subject;
extern lv_subject_t weather_temp_subject;
extern lv_subject_t weather_hum_subject;
extern lv_subject_t indoor_temp_subject;
extern lv_subject_t indoor_hum_subject;

// 当前值存储，用于数据比对
static char current_song_lyrics[256] = "";
static char current_song_name[128] = "";
static char current_song_artist[128] = "";
static char current_song_time[32] = "";
static int current_play_progress = -1;
static int current_volume = -1;
static float current_power = -1.0;
static float current_daily_energy = -1.0;
static float current_monthly_energy = -1.0;

// 天气和室内数据缓存，用于比对更新
static char current_indoor_temp[16] = "";
static char current_indoor_hum[16] = "";

// 24小时天气数据存储
static hourly_weather_t g_24h_weather_data[24];
static int g_24h_weather_count = 0;

/**
 * @brief 将 Open-Meteo (WMO) 天气代码转换为中文描述
 * 
 * @param code WMO 天气代码
 * @return const char* 中文天气描述
 */
static const char* get_weather_desc(int code)
{
    switch (code) {
        case 0:  return "晴";
        case 1:  return "晴间多云";
        case 2:  return "多云";
        case 3:  return "阴天";
        case 45: return "雾";
        case 48: return "雾凇";
        case 51: return "轻微毛毛雨";
        case 53: return "中度毛毛雨";
        case 55: return "重度毛毛雨";
        case 56: return "轻微冻毛毛雨";
        case 57: return "重度冻毛毛雨";
        case 61: return "小雨";
        case 63: return "中雨";
        case 65: return "大雨";
        case 66: return "轻微冻雨";
        case 67: return "重度冻雨";
        case 71: return "小雪";
        case 73: return "中雪";
        case 75: return "大雪";
        case 77: return "雪粒";
        case 80: return "间歇性小雨";
        case 81: return "间歇性中雨";
        case 82: return "间歇性大雨";
        case 85: return "间歇性小雪";
        case 86: return "间歇性大雪";
        case 95: return "雷阵雨";
        case 96: return "雷阵雨伴有小冰雹";
        case 99: return "雷阵雨伴有大冰雹";
        default: return "未知天气";
    }
}

// 事件队列长度
#define EVENT_QUEUE_LENGTH 10

// 事件队列项大小
#define EVENT_ITEM_SIZE sizeof(event_t)

// 事件处理任务句柄
static TaskHandle_t g_event_handler_tasks[EVENT_TYPE_MAX] = {NULL};

// 事件队列初始化
esp_err_t event_system_init(void)
{
    ESP_LOGI(TAG, "初始化事件系统");
    
    // 创建系统事件队列
    g_system_event_queue = xQueueCreate(EVENT_QUEUE_LENGTH, EVENT_ITEM_SIZE);
    if (g_system_event_queue == NULL) {
        ESP_LOGE(TAG, "创建系统事件队列失败");
        return ESP_FAIL;
    }
    
    // 创建UI事件队列
    g_ui_event_queue = xQueueCreate(EVENT_QUEUE_LENGTH, EVENT_ITEM_SIZE);
    if (g_ui_event_queue == NULL) {
        ESP_LOGE(TAG, "创建UI事件队列失败");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "事件系统初始化成功");
    return ESP_OK;
}

// 发送事件
esp_err_t event_system_post(event_type_t type, void *data, size_t data_len)
{
    if (type >= EVENT_TYPE_MAX) {
        ESP_LOGE(TAG, "无效的事件类型: %d", type);
        return ESP_ERR_INVALID_ARG;
    }
    
    event_t event;
    event.type = type;
    event.data_len = data_len;
    
    // 记录事件发送时间戳（单位：tick）
    event.send_time = xTaskGetTickCount();
    
    // 如果有数据，复制数据
    if (data != NULL && data_len > 0) {
        event.data = malloc(data_len);
        if (event.data == NULL) {
            ESP_LOGE(TAG, "分配事件数据内存失败");
            return ESP_ERR_NO_MEM;
        }
        memcpy(event.data, data, data_len);
    } else {
        event.data = NULL;
    }
    
    // 根据事件类型选择目标队列
    QueueHandle_t target_queue;
    if (type == EVENT_TYPE_XML_LOADED || type == EVENT_TYPE_NTP_TIME_UPDATED || type == EVENT_TYPE_UI_UPDATE) {
        // UI相关事件发送到UI事件队列
        target_queue = g_ui_event_queue;
    } else {
        // 其他事件发送到系统事件队列
        target_queue = g_system_event_queue;
    }
    
    // 将事件发送到队列
    if (xQueueSend(target_queue, &event, portMAX_DELAY) != pdPASS) {
        ESP_LOGE(TAG, "发送事件到队列失败");
        if (event.data != NULL) {
            free(event.data);
        }
        return ESP_FAIL;
    }
    
    ESP_LOGD(TAG, "成功发送事件: %d", type);
    return ESP_OK;
}

// 注册事件处理函数
esp_err_t event_system_register_handler(event_type_t type, TaskHandle_t handler_task)
{
    if (type >= EVENT_TYPE_MAX) {
        ESP_LOGE(TAG, "无效的事件类型: %d", type);
        return ESP_ERR_INVALID_ARG;
    }
    
    g_event_handler_tasks[type] = handler_task;
    ESP_LOGI(TAG, "注册事件处理函数成功: %d -> 任务句柄: %p", type, handler_task);
    return ESP_OK;
}

// HomeAssistant 监控任务：处理所有同步的 HTTP 轮询
void ha_monitor_task(void *arg)
{
    ESP_LOGI(TAG, "启动 HA 监控任务");
    
    // 初始化定时器
    TickType_t last_energy_update = xTaskGetTickCount() - pdMS_TO_TICKS(30000);
    TickType_t last_switch_update = xTaskGetTickCount() - pdMS_TO_TICKS(30000);
    TickType_t last_weather_update = 0;
    
    const TickType_t energy_update_interval = pdMS_TO_TICKS(30000);
    const TickType_t switch_update_interval = pdMS_TO_TICKS(30000); // 开关刷新频率降至 30s
    const TickType_t weather_update_interval = pdMS_TO_TICKS(30 * 60 * 1000);

    // 开关状态缓存
    static int cached_switch_states[5] = {-1, -1, -1, -1, -1};
    const char *switch_entities[5] = {NULL, "switch.tasmota", "switch.new_dc1_3", "switch.new_dc1_4", "switch.tasmota_2"};
    
    // 注册到看门狗
    esp_task_wdt_add(NULL);
    
    while (1) {
        esp_task_wdt_reset();
        
        // 检查WiFi连接状态,如果未连接则跳过所有HTTP请求
        if (!is_wifi_connected()) {
            ESP_LOGD(TAG, "WiFi未连接,跳过HA监控任务");
            vTaskDelay(pdMS_TO_TICKS(5000));  // WiFi断开时延长等待时间
            continue;
        }
        
        TickType_t now = xTaskGetTickCount();

        // 1. 更新能耗数据 (30秒)
        if ((now - last_energy_update) >= energy_update_interval) {
            last_energy_update = now;
            char *d_s = get_daily_energy();
            if (d_s) {
                float d_v = atof(d_s);
                if (fabs(d_v - current_daily_energy) > 0.01) {
                    ui_update_t *uu = malloc(sizeof(ui_update_t));
                    if (uu) {
                        uu->type = UI_UPDATE_TYPE_DAILY_ENERGY;
                        sprintf(uu->value.str_value, " #FF0000 %.1f# #5F6777 kW##04905E $%.2f#", d_v, d_v * 1.2);
                        event_system_post(EVENT_TYPE_UI_UPDATE, uu, sizeof(ui_update_t));
                    }
                    current_daily_energy = d_v;
                }
                free(d_s);
            }
            char *m_s = get_monthly_energy();
            if (m_s) {
                float m_v = atof(m_s);
                if (fabs(m_v - current_monthly_energy) > 0.01) {
                    ui_update_t *uu = malloc(sizeof(ui_update_t));
                    if (uu) {
                        uu->type = UI_UPDATE_TYPE_MONTHLY_ENERGY;
                        sprintf(uu->value.str_value, " #FF0000 %.1f# #5F6777 kW##04905E $%.2f#", m_v, m_v * 1.2);
                        event_system_post(EVENT_TYPE_UI_UPDATE, uu, sizeof(ui_update_t));
                    }
                    current_monthly_energy = m_v;
                }
                free(m_s);
            }

            // 获取室内温湿度
            char *i_t = get_entity_state("sensor.zhimi_cn_94444656_ma2_temperature_p_3_3");
            if (i_t) {
                if (strcmp(i_t, current_indoor_temp) != 0) {
                    ui_update_t *uu = malloc(sizeof(ui_update_t));
                    if (uu) {
                        uu->type = UI_UPDATE_TYPE_INDOOR_TEMP;
                        sprintf(uu->value.str_value, "%s°C", i_t);
                        event_system_post(EVENT_TYPE_UI_UPDATE, uu, sizeof(ui_update_t));
                    }
                    strncpy(current_indoor_temp, i_t, sizeof(current_indoor_temp)-1);
                }
                free(i_t);
            }
            char *i_h = get_entity_state("sensor.zhimi_cn_94444656_ma2_relative_humidity_p_3_1");
            if (i_h) {
                if (strcmp(i_h, current_indoor_hum) != 0) {
                    ui_update_t *uu = malloc(sizeof(ui_update_t));
                    if (uu) {
                        uu->type = UI_UPDATE_TYPE_INDOOR_HUM;
                        sprintf(uu->value.str_value, "%s%%", i_h);
                        event_system_post(EVENT_TYPE_UI_UPDATE, uu, sizeof(ui_update_t));
                    }
                    strncpy(current_indoor_hum, i_h, sizeof(current_indoor_hum)-1);
                }
                free(i_h);
            }
            esp_task_wdt_reset();
        }

        // 2. 更新开关状态 (30秒)
        now = xTaskGetTickCount();
        if ((now - last_switch_update) >= switch_update_interval) {
            last_switch_update = now;
            for (int i = 1; i <= 4; i++) {
                char *state_str = get_entity_state(switch_entities[i]);
                if (state_str) {
                    int state = (strcmp(state_str, "on") == 0 || strcmp(state_str, "ON") == 0) ? 1 : 0;
                    if (state != cached_switch_states[i]) {
                        ui_update_t *uu = malloc(sizeof(ui_update_t));
                        if (uu) {
                            uu->type = UI_UPDATE_TYPE_SWITCH_STATE;
                            uu->value.int_value = (i << 8) | state;
                            event_system_post(EVENT_TYPE_UI_UPDATE, uu, sizeof(ui_update_t));
                        }
                        cached_switch_states[i] = state;
                        ESP_LOGI(TAG, "开关 %d (%s) 状态更新: %d", i, switch_entities[i], state);
                    }
                    free(state_str);
                }
                esp_task_wdt_reset();
            }
        }

        // 3. 获取室外天气 (30分钟)
        now = xTaskGetTickCount();
        bool time_synced = (time(NULL) > 1700000000); 
        if (time_synced && (last_weather_update == 0 || (now - last_weather_update) >= weather_update_interval)) {
            last_weather_update = now;
            
            struct tm timeinfo;
            time_t now_time;
            time(&now_time);
            localtime_r(&now_time, &timeinfo);
            char date_str[16];
            strftime(date_str, sizeof(date_str), "%Y-%m-%d", &timeinfo);

            char weather_url[512];
            snprintf(weather_url, sizeof(weather_url), 
                "http://api.open-meteo.com/v1/forecast?latitude=22.495&longitude=113.2678&start_date=%s&end_date=%s&hourly=temperature_2m,weather_code,relative_humidity_2m,apparent_temperature&timezone=Asia/Shanghai",
                date_str, date_str);

            http_config_t http_cfg = {
                .url = weather_url,
                .method = HTTP_METHOD_GET,
                .timeout_ms = 8000
            };

            char *response = http_send_request(&http_cfg);
            if (response) {
                cJSON *root = cJSON_Parse(response);
                if (root) {
                    cJSON *hourly = cJSON_GetObjectItem(root, "hourly");
                    if (hourly) {
                        cJSON *times = cJSON_GetObjectItem(hourly, "time");
                        cJSON *codes = cJSON_GetObjectItem(hourly, "weather_code");
                        cJSON *temps = cJSON_GetObjectItem(hourly, "temperature_2m");
                        cJSON *hums = cJSON_GetObjectItem(hourly, "relative_humidity_2m");
                        cJSON *apparent_temps = cJSON_GetObjectItem(hourly, "apparent_temperature");
                        
                        if (times && codes && temps && hums && apparent_temps) {
                            int size = cJSON_GetArraySize(times);
                            int current_hour = timeinfo.tm_hour;
                            int idx = -1;
                            
                            // 保存24小时天气数据
                            g_24h_weather_count = (size > 24) ? 24 : size;
                            for (int i = 0; i < g_24h_weather_count; i++) {
                                cJSON *t = cJSON_GetArrayItem(temps, i);
                                cJSON *c = cJSON_GetArrayItem(codes, i);
                                cJSON *h = cJSON_GetArrayItem(hums, i);
                                cJSON *at = cJSON_GetArrayItem(apparent_temps, i);
                                
                                if (t && c && h && at) {
                                    g_24h_weather_data[i].temperature = t->valuedouble;
                                    g_24h_weather_data[i].apparent_temperature = at->valuedouble;
                                    g_24h_weather_data[i].weather_code = c->valueint;
                                    g_24h_weather_data[i].humidity = (int)h->valuedouble;
                                }
                            }
                            ESP_LOGI(TAG, "已保存 %d 小时天气数据", g_24h_weather_count);
                            
                            // 查找当前小时的索引用于UI更新
                            for (int i = 0; i < size; i++) {
                                cJSON *time_item = cJSON_GetArrayItem(times, i);
                                if (time_item && cJSON_IsString(time_item)) {
                                    int h_val;
                                    if (sscanf(time_item->valuestring, "%*[^T]T%d", &h_val) == 1) {
                                        if (h_val == current_hour) {
                                            idx = i;
                                            break;
                                        }
                                    }
                                }
                            }
                            
                            // 更新当前小时的UI显示
                            if (idx != -1) {
                                cJSON *c = cJSON_GetArrayItem(codes, idx);
                                cJSON *t = cJSON_GetArrayItem(temps, idx);
                                cJSON *h = cJSON_GetArrayItem(hums, idx);
                                if (c && t && h) {
                                    const char *desc = get_weather_desc(c->valueint);
                                    
                                    ui_update_t *uu;
                                    uu = malloc(sizeof(ui_update_t));
                                    if (uu) { uu->type = UI_UPDATE_TYPE_WEATHER_DESC; sprintf(uu->value.str_value, "%s", desc); event_system_post(EVENT_TYPE_UI_UPDATE, uu, sizeof(ui_update_t)); }
                                    cJSON *at = cJSON_GetArrayItem(apparent_temps, idx);
                                uu = malloc(sizeof(ui_update_t));
                                if (uu) { uu->type = UI_UPDATE_TYPE_WEATHER_TEMP; sprintf(uu->value.str_value, "%d|%dC", (int)t->valuedouble, (int)at->valuedouble); event_system_post(EVENT_TYPE_UI_UPDATE, uu, sizeof(ui_update_t)); }
                                    uu = malloc(sizeof(ui_update_t));
                                    if (uu) { uu->type = UI_UPDATE_TYPE_WEATHER_HUM; sprintf(uu->value.str_value, "%.0f%%", h->valuedouble); event_system_post(EVENT_TYPE_UI_UPDATE, uu, sizeof(ui_update_t)); }
                                }
                            }
                        }
                        cJSON_Delete(root);
                    }
                }
                free(response);
            }
            esp_task_wdt_reset();
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// 事件处理任务
void event_system_task(void *arg)
{
    ESP_LOGI(TAG, "启动事件处理任务");
    
    // 注册到看门狗
    esp_task_wdt_add(NULL);
    
    while (1) {
        // 等待系统事件队列中的事件
        event_t event;
        BaseType_t ret = xQueueReceive(g_system_event_queue, &event, pdMS_TO_TICKS(10));
        
        // 喂狗
        esp_task_wdt_reset();

        if (ret == pdPASS) {
            // 计算事件从发送到接收的耗时
            TickType_t receive_time = xTaskGetTickCount();
            TickType_t delay_ticks = receive_time - event.send_time;
            uint32_t delay_ms = pdTICKS_TO_MS(delay_ticks);
            ESP_LOGD(TAG, "收到系统事件: %d, 耗时: %u ms", event.type, delay_ms);
            
            // 根据事件类型处理系统事件
            switch (event.type) {
                case EVENT_TYPE_WIFI_CONNECTED:
                    ESP_LOGI(TAG, "WiFi连接成功事件处理");
                    break;
                case EVENT_TYPE_WIFI_DISCONNECTED:
                    ESP_LOGI(TAG, "WiFi断开事件处理");
                    break;
                case EVENT_TYPE_BUTTON_CLICK:
                    ESP_LOGI(TAG, "按钮点击事件处理");
                    break;
                case EVENT_TYPE_TOUCH_EVENT:
                    ESP_LOGI(TAG, "触摸事件处理");
                    break;
                case EVENT_TYPE_LOAD_XML:
                    ESP_LOGI(TAG, "加载XML事件处理");
                    break;
                case EVENT_TYPE_REFRESH_XML:
                    ESP_LOGI(TAG, "刷新XML事件处理");
                    break;
                case EVENT_TYPE_MQTT_CONNECTED:
                    ESP_LOGI(TAG, "MQTT连接成功事件处理");
                    break;
                case EVENT_TYPE_MQTT_DISCONNECTED:
                    ESP_LOGI(TAG, "MQTT断开连接事件处理");
                    break;
                case EVENT_TYPE_MQTT_MESSAGE_RECEIVED:
                    // 只有在有数据时才处理
                    if (event.data != NULL) {
                        mqtt_message_t *mqtt_msg = (mqtt_message_t *)event.data;
                        if (mqtt_msg->topic != NULL && mqtt_msg->data != NULL) {
                            if (strcmp(mqtt_msg->topic, "homeassistant/number/esp32_music_player/volume/set") == 0) {
                                ESP_LOGI(TAG, "忽略自己发布的音量设置消息");
                            } else if (strcmp(mqtt_msg->topic, "homeassistant/sensor/esp32_music_player/lyrics/state") == 0) {
                                if (strcmp(mqtt_msg->data, current_song_lyrics) != 0) {
                                    ui_update_t *ui_update = malloc(sizeof(ui_update_t));
                                    if (ui_update != NULL) {
                                        ui_update->type = UI_UPDATE_TYPE_LYRICS;
                                        strncpy(ui_update->value.str_value, mqtt_msg->data, sizeof(ui_update->value.str_value) - 1);
                                        ui_update->value.str_value[sizeof(ui_update->value.str_value)-1] = '\0';
                                        event_system_post(EVENT_TYPE_UI_UPDATE, ui_update, sizeof(ui_update_t));
                                    }
                                    strncpy(current_song_lyrics, mqtt_msg->data, sizeof(current_song_lyrics)-1);
                                }
                            } else if (strcmp(mqtt_msg->topic, "homeassistant/sensor/esp32_music_player/song/state") == 0) {
                                if (strcmp(mqtt_msg->data, current_song_name) != 0) {
                                    ui_update_t *ui_update = malloc(sizeof(ui_update_t));
                                    if (ui_update != NULL) {
                                        ui_update->type = UI_UPDATE_TYPE_SONG_NAME;
                                        strncpy(ui_update->value.str_value, mqtt_msg->data, sizeof(ui_update->value.str_value) - 1);
                                        ui_update->value.str_value[sizeof(ui_update->value.str_value)-1] = '\0';
                                        event_system_post(EVENT_TYPE_UI_UPDATE, ui_update, sizeof(ui_update_t));
                                    }
                                    strncpy(current_song_name, mqtt_msg->data, sizeof(current_song_name)-1);
                                }
                            } else if (strcmp(mqtt_msg->topic, "homeassistant/sensor/esp32_music_player/artist/state") == 0) {
                                if (strcmp(mqtt_msg->data, current_song_artist) != 0) {
                                    ui_update_t *ui_update = malloc(sizeof(ui_update_t));
                                    if (ui_update != NULL) {
                                        ui_update->type = UI_UPDATE_TYPE_ARTIST;
                                        strncpy(ui_update->value.str_value, mqtt_msg->data, sizeof(ui_update->value.str_value) - 1);
                                        ui_update->value.str_value[sizeof(ui_update->value.str_value)-1] = '\0';
                                        event_system_post(EVENT_TYPE_UI_UPDATE, ui_update, sizeof(ui_update_t));
                                    }
                                    strncpy(current_song_artist, mqtt_msg->data, sizeof(current_song_artist)-1);
                                }
                            } else if (strcmp(mqtt_msg->topic, "homeassistant/number/esp32_music_player/position/state") == 0) {
                                float p_f = atof(mqtt_msg->data);
                                int p = (int)(p_f * 100);
                                if (p != current_play_progress) {
                                    ui_update_t *ui_update = malloc(sizeof(ui_update_t));
                                    if (ui_update != NULL) {
                                        ui_update->type = UI_UPDATE_TYPE_PLAY_PROGRESS;
                                        ui_update->value.int_value = p;
                                        event_system_post(EVENT_TYPE_UI_UPDATE, ui_update, sizeof(ui_update_t));
                                    }
                                    current_play_progress = p;
                                }
                            } else if (strcmp(mqtt_msg->topic, "homeassistant/number/esp32_music_player/volume/state") == 0) {
                                int vol = atoi(mqtt_msg->data);
                                if (vol != current_volume) {
                                    ui_update_t *ui_update = malloc(sizeof(ui_update_t));
                                    if (ui_update != NULL) {
                                        ui_update->type = UI_UPDATE_TYPE_VOLUME;
                                        ui_update->value.int_value = vol;
                                        event_system_post(EVENT_TYPE_UI_UPDATE, ui_update, sizeof(ui_update_t));
                                    }
                                    current_volume = vol;
                                }
                            } else if (strcmp(mqtt_msg->topic, "homeassistant/sensor/esp32_music_player/progress/state") == 0) {
                                if (strcmp(mqtt_msg->data, current_song_time) != 0) {
                                    ui_update_t *ui_update = malloc(sizeof(ui_update_t));
                                    if (ui_update != NULL) {
                                        ui_update->type = UI_UPDATE_TYPE_SONG_TIME;
                                        strncpy(ui_update->value.str_value, mqtt_msg->data, sizeof(ui_update->value.str_value) - 1);
                                        ui_update->value.str_value[sizeof(ui_update->value.str_value)-1] = '\0';
                                        event_system_post(EVENT_TYPE_UI_UPDATE, ui_update, sizeof(ui_update_t));
                                    }
                                    strncpy(current_song_time, mqtt_msg->data, sizeof(current_song_time)-1);
                                }
                            } else if (strcmp(mqtt_msg->topic, "tele/tasmota_A0DA50/SENSOR") == 0) {
                                cJSON *root = cJSON_Parse(mqtt_msg->data);
                                if (root != NULL) {
                                    cJSON *energy = cJSON_GetObjectItem(root, "ENERGY");
                                    if (energy != NULL) {
                                        cJSON *power = cJSON_GetObjectItem(energy, "Power");
                                        if (power != NULL && cJSON_IsNumber(power)) {
                                            float pv = power->valuedouble;
                                            if (fabs(pv - current_power) > 0.1) {
                                                ui_update_t *ui_update = malloc(sizeof(ui_update_t));
                                                if (ui_update != NULL) {
                                                    ui_update->type = UI_UPDATE_TYPE_POWER;
                                                    ui_update->value.int_value = (int)(pv * 10.0);
                                                    event_system_post(EVENT_TYPE_UI_UPDATE, ui_update, sizeof(ui_update_t));
                                                }
                                                current_power = pv;
                                            }
                                        }
                                    }
                                    cJSON_Delete(root);
                                }
                            } else if (strcmp(mqtt_msg->topic, "homeassistant/sensor/esp32_music_player/url/state") == 0) {
                                ESP_LOGI(TAG, "收到专辑封面 URL: %s", mqtt_msg->data);
                                request_album_art_update(mqtt_msg->data, 100, 100, false);
                            } else if (strcmp(mqtt_msg->topic, "homeassistant/switch/esp32_music_player/play/state") == 0) {
                                bool is_on = (strcmp(mqtt_msg->data, "ON") == 0);
                                ui_update_t *ui_update = malloc(sizeof(ui_update_t));
                                if (ui_update != NULL) {
                                    ui_update->type = UI_UPDATE_TYPE_PLAY_STATE;
                                    ui_update->value.int_value = is_on ? 1 : 0;
                                    event_system_post(EVENT_TYPE_UI_UPDATE, ui_update, sizeof(ui_update_t));
                                }
                            }
                        }
                        // 统一释放子内存
                        if (mqtt_msg->topic) free(mqtt_msg->topic);
                        if (mqtt_msg->data) free(mqtt_msg->data);
                    }
                    break;
                default:
                    ESP_LOGE(TAG, "未知系统事件类型: %d", event.type);
                    break;
            }
            
            // 释放事件容器内存 (mqtt_msg 等)
            if (event.data != NULL) {
                free(event.data);
            }
        }
        // 核心任务只负责处理队列，不再包含同步 HTTP 轮询逻辑
        // 轮询逻辑已迁移至 ha_monitor_task
    }
}

// 获取24小时天气数据
const hourly_weather_t* get_24h_weather_data(int *count)
{
    if (count != NULL) {
        *count = g_24h_weather_count;
    }
    return g_24h_weather_data;
}
