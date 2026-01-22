/*
 * SPDX-FileCopyrightText: 2022-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <string.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "lvgl.h"
#include <Vernon_GT911.h>
#include "nvs_flash.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "smart_control_panel_config.h"
#include "smart_control_panel_init.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "event_system.h"
#include "ntp_time.h"
#include "esp_task_wdt.h"
#include "esp32_mqtt_client.h"
#include "screens/main/main_screen.h"
#include "album_art_manager.h"
#include "ui_common.h"

esp_lcd_panel_handle_t panel_handle = NULL;
Vernon_GT911 gt911;

touch_data_t g_touch_data = {0};
// 互斥锁，保护触摸数据访问
SemaphoreHandle_t g_touch_mutex = NULL;

// 外部变量声明
extern lv_subject_t song_lyrics_subject;
extern lv_subject_t weather_desc_subject;
extern lv_subject_t weather_temp_subject;
extern lv_subject_t weather_hum_subject;
extern lv_subject_t indoor_temp_subject;
extern lv_subject_t indoor_hum_subject;
extern lv_subject_t daily_energy_subject;
extern lv_subject_t monthly_energy_subject;
extern lv_subject_t monthly_energy_subject;
extern lv_subject_t cover_img_subject;
extern lv_subject_t switch_1_state;
extern lv_subject_t switch_2_state;
extern lv_subject_t switch_3_state;
extern lv_subject_t switch_4_state;

// LVGL相关变量
lv_disp_t *lv_disp = NULL;
lv_indev_t *lv_indev = NULL;
static lv_image_dsc_t cover_img_dsc;

const char *TAG = "smart_control_panel";



// WiFi配置
#define WIFI_SSID "root_2.4G"  // 替换为你的WiFi名称
#define WIFI_PASS "yangyong1229"  // 替换为你的WiFi密码

// WiFi重连配置
#define WIFI_MAX_RETRY 15           // 最大重连次数
#define WIFI_RETRY_BASE_DELAY 1000  // 基础重连延迟(ms)
#define WIFI_RETRY_MAX_DELAY 30000  // 最大重连延迟(ms)

// WiFi连接状态
bool g_wifi_connected = false;
static int g_wifi_retry_count = 0;
static TickType_t g_last_disconnect_time = 0;

// WiFi状态检查函数,供其他模块调用
bool is_wifi_connected(void)
{
    return g_wifi_connected;
}

// WiFi事件处理回调函数
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "WiFi STA启动,开始连接...");
        g_wifi_retry_count = 0;
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t* disconnected = (wifi_event_sta_disconnected_t*) event_data;
        g_wifi_connected = false;
        g_last_disconnect_time = xTaskGetTickCount();
        
        ESP_LOGW(TAG, "WiFi断开连接,原因: %d, 重试次数: %d/%d", 
                 disconnected->reason, g_wifi_retry_count, WIFI_MAX_RETRY);
        
        // 发送WiFi断开事件
        event_system_post(EVENT_TYPE_WIFI_DISCONNECTED, NULL, 0);
        
        if (g_wifi_retry_count < WIFI_MAX_RETRY) {
            // 计算退避延迟(指数退避)
            uint32_t delay_ms = WIFI_RETRY_BASE_DELAY * (1 << g_wifi_retry_count);
            if (delay_ms > WIFI_RETRY_MAX_DELAY) {
                delay_ms = WIFI_RETRY_MAX_DELAY;
            }
            
            ESP_LOGI(TAG, "将在 %u ms 后尝试重新连接...", (unsigned int)delay_ms);
            vTaskDelay(pdMS_TO_TICKS(delay_ms));
            
            esp_wifi_connect();
            g_wifi_retry_count++;
        } else {
            ESP_LOGE(TAG, "WiFi重连失败,已达到最大重试次数 %d", WIFI_MAX_RETRY);
            // 重置重试计数器,等待下一次尝试
            g_wifi_retry_count = 0;
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        char ip_str[16];
        sprintf(ip_str, "%u.%u.%u.%u", 
                (unsigned int)((event->ip_info.ip.addr >> 0) & 0xFF), 
                (unsigned int)((event->ip_info.ip.addr >> 8) & 0xFF), 
                (unsigned int)((event->ip_info.ip.addr >> 16) & 0xFF), 
                (unsigned int)((event->ip_info.ip.addr >> 24) & 0xFF));
        ESP_LOGI(TAG, "WiFi连接成功，IP地址: %s", ip_str);
        g_wifi_connected = true;
        g_wifi_retry_count = 0;  // 重置重连计数器
        
        // 发送WiFi连接成功事件
        event_system_post(EVENT_TYPE_WIFI_CONNECTED, NULL, 0);
        
        // WiFi连接成功后，初始化和启动NTP时间模块
        ntp_time_config_t ntp_config = {
            .ntp_server = "192.168.1.1",
            .timezone = 8, // 东八区
            .enable_daylight_saving = false
        };
        
        // 初始化NTP时间模块
        esp_err_t err = ntp_time_init(&ntp_config);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "NTP时间模块初始化失败: %s", esp_err_to_name(err));
        } else {
            // 启动NTP时间同步
            err = ntp_time_start();
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "NTP时间同步启动失败: %s", esp_err_to_name(err));
            }
        }
        
        // 启动MQTT客户端连接
        err = mqtt_client_start();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "启动MQTT客户端失败: %s", esp_err_to_name(err));
        }

    }
}

// WiFi初始化函数
static void wifi_init(void)
{
    ESP_LOGI(TAG, "初始化WiFi连接");
    
    // 初始化TCP/IP协议栈
    ESP_ERROR_CHECK(esp_netif_init());
    
    // 创建事件循环
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    // 创建默认的WiFi STA网络接口
    esp_netif_create_default_wifi_sta();
    
    // 初始化WiFi驱动
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    // 注册WiFi事件处理
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));
    
    // 配置WiFi为STA模式
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "",
            .password = "",
        },
    };
    strcpy((char*)wifi_config.sta.ssid, WIFI_SSID);
    strcpy((char*)wifi_config.sta.password, WIFI_PASS);
    
    // 设置WiFi模式为STA
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    
    // 启动WiFi
    ESP_ERROR_CHECK(esp_wifi_start());
    
    ESP_LOGI(TAG, "WiFi初始化完成，等待连接...");
    
    // 等待WiFi连接成功，最多等待10秒
    int retry_count = 0;
    while (!g_wifi_connected && retry_count < 100) {
        vTaskDelay(pdMS_TO_TICKS(100));
        retry_count++;
    }
    
    if (!g_wifi_connected) {
        ESP_LOGE(TAG, "WiFi连接超时");
    }
}

// 包含UI管理器头文件
#include "../ui/ui_manager.h"
// 包含主屏幕头文件
#include "../screens/main/main_screen.h"
// 包含NTP时间模块头文件
#include "ntp_time.h"
// 包含MQTT客户端头文件
#include "esp32_mqtt_client.h"



// 更新歌词显示
void update_lyrics_display(const char *lyrics)
{
    ESP_LOGI(TAG, "更新歌词显示: %s", lyrics);
    // 更新歌词Subject
    lv_subject_snprintf(&song_lyrics_subject, "%s", lyrics);
}

// 更新时间显示
void update_time_display(void)
{
    struct tm now;
    time_t now_sec;
    
    // 获取当前时间
    time(&now_sec);
    localtime_r(&now_sec, &now);
    
    // 静态变量，记录上一次更新的时间（只记录小时和分钟）
    static int last_hour = -1;
    static int last_minute = -1;
    static int last_day = -1;
    static int last_month = -1;
    static int last_year = -1;
    
    // 检查时间是否发生变化（只比较小时、分钟、日、月、年，不比较秒）
    bool time_changed = (now.tm_hour != last_hour) || (now.tm_min != last_minute);
    bool date_changed = (now.tm_mday != last_day) || (now.tm_mon != last_month) || (now.tm_year != last_year);
    
    // 如果时间没有变化，直接返回，不更新UI
    if (!time_changed && !date_changed) {
        return;
    }
    
    // 更新最后更新时间
    last_hour = now.tm_hour;
    last_minute = now.tm_min;
    last_day = now.tm_mday;
    last_month = now.tm_mon;
    last_year = now.tm_year;
    
    // 格式化日期、时间和星期
    char date_str[128];
    char time_str[64];
    char weekday_str[64];
    
    // 公历日期转换为LVGL日历日期结构体
    lv_calendar_date_t gregorian;
    gregorian.year = now.tm_year + 1900;
    gregorian.month = now.tm_mon + 1;
    gregorian.day = now.tm_mday;
    
    // 公历转农历
    lv_calendar_chinese_t chinese;
    lv_calendar_gregorian_to_chinese(&gregorian, &chinese);
    
    // 干支年份计算
    // 十天干：甲、乙、丙、丁、戊、己、庚、辛、壬、癸
    const char *heavenly_stems[] = {"甲", "乙", "丙", "丁", "戊", "己", "庚", "辛", "壬", "癸"};
    // 十二地支：子、丑、寅、卯、辰、巳、午、未、申、酉、戌、亥
    const char *earthly_branches[] = {"子", "丑", "寅", "卯", "辰", "巳", "午", "未", "申", "酉", "戌", "亥"};
    
    // 计算干支年份（以2024年为甲辰年为基准）
    // 干支纪年每60年一个周期，2024年是甲辰年
    int base_year = 2024;
    int base_stem = 0; // 甲
    int base_branch = 4; // 辰
    
    int year_diff = chinese.today.year - base_year;
    int stem_index = (base_stem + year_diff) % 10;
    int branch_index = (base_branch + year_diff) % 12;
    
    // 处理负数情况
    if (stem_index < 0) stem_index += 10;
    if (branch_index < 0) branch_index += 12;
    
    // 农历月份名称
    const char *chinese_month_names[] = {
        "正月", "二月", "三月", "四月", "五月", "六月",
        "七月", "八月", "九月", "十月", "冬月", "腊月"
    };
    
    // 农历日期名称
    const char *chinese_day_names[] = {
        "初一", "初二", "初三", "初四", "初五", "初六", "初七", "初八", "初九", "初十",
        "十一", "十二", "十三", "十四", "十五", "十六", "十七", "十八", "十九", "二十",
        "廿一", "廿二", "廿三", "廿四", "廿五", "廿六", "廿七", "廿八", "廿九", "三十"
    };
    
    // 格式化农历日期
    char chinese_date_str[64];
    if (chinese.leep_month) {
        sprintf(chinese_date_str, "%s%s年闰%s#FF0000 %s#", 
                heavenly_stems[stem_index], earthly_branches[branch_index],
                chinese_month_names[chinese.today.month - 1], 
                chinese_day_names[chinese.today.day - 1]);
    } else {
        sprintf(chinese_date_str, "%s%s年%s#FF0000 %s#", 
                heavenly_stems[stem_index], earthly_branches[branch_index],
                chinese_month_names[chinese.today.month - 1], 
                chinese_day_names[chinese.today.day - 1]);
    }
    
    // 日期显示格式: 01-#FF0000 17# | 农历（如：乙巳年腊月十六）
    sprintf(date_str, "%02d-#FF0000 %02d# | %s", now.tm_mon + 1, now.tm_mday, chinese_date_str);

    
    // 时间显示格式: 21:#FF0000 05#
    sprintf(time_str, "%02d:#FF0000 %02d#", now.tm_hour, now.tm_min);
    
    // 星期显示格式: 星期#FF0000 六#
    const char *weekday_chars[] = {"日", "一", "二", "三", "四", "五", "六"};
    sprintf(weekday_str, "星期#FF0000 %s#", weekday_chars[now.tm_wday]);
    
    // 输出调试日志，显示当前时间
    ESP_LOGI(TAG, "当前时间 - 日期: %s, 时间: %s, 星期: %s", date_str, time_str, weekday_str);
    
    // 更新日期显示
    lv_subject_snprintf(&date_subject, "%s", date_str);
    
    // 更新时间显示
    lv_subject_snprintf(&time_subject, "%s", time_str);
    
    // 更新星期显示
    lv_subject_snprintf(&weekday_subject, "%s", weekday_str);
}

// LVGL任务函数，用于处理LVGL的主循环
static void lvgl_task(void *arg)
{
    ESP_LOGI(TAG, "启动LVGL任务，显示Hello World");
    
    // 初始化主屏幕Subject
    init_main_screen_subjects();
    
    // 初始化UI管理器
    ui_manager_init();
    
    // 创建主屏幕
    ui_create_screen(SCREEN_MAIN);
    
    // 将当前任务订阅到看门狗
    esp_task_wdt_add(NULL);
    
    // LVGL主循环
    uint32_t time_update_count = 0;
    while (1) {
        // 检查UI事件队列中是否有事件
        event_t event;
        while (xQueueReceive(g_ui_event_queue, &event, 0) == pdPASS) {
            // 在处理大事件（如 XML 加载）前后重置看门狗
            esp_task_wdt_reset();

            // 计算事件从发送到接收的耗时
            TickType_t receive_time = xTaskGetTickCount();
            TickType_t delay_ticks = receive_time - event.send_time;
            uint32_t delay_ms = pdTICKS_TO_MS(delay_ticks);
            
            if (event.type == EVENT_TYPE_XML_LOADED) {
                ESP_LOGI(TAG, "LVGL任务收到XML加载完成事件，耗时: %u ms", delay_ms);
                // 调用XML加载完成回调函数
                on_xml_loaded((xml_load_result_t *)event.data);
                // 释放事件数据内存
                free(event.data);
            } else if (event.type == EVENT_TYPE_NTP_TIME_UPDATED) {
                ESP_LOGI(TAG, "LVGL任务收到NTP时间更新事件，耗时: %u ms", delay_ms);
                // 更新时间显示
                update_time_display();
                // 释放事件数据内存
                if (event.data != NULL) {
                    free(event.data);
                }
            } else if (event.type == EVENT_TYPE_UI_UPDATE) {
                ESP_LOGI(TAG, "LVGL任务收到UI更新事件，耗时: %u ms", delay_ms);
                // 处理UI更新事件
                ui_update_t *ui_update = (ui_update_t *)event.data;
                uint32_t sub_start = esp_log_timestamp();
                switch (ui_update->type) {
                    case UI_UPDATE_TYPE_LYRICS:
                        lv_subject_snprintf(&song_lyrics_subject, "%s", ui_update->value.str_value);
                        break;
                    case UI_UPDATE_TYPE_SONG_NAME:
                        lv_subject_snprintf(&song_name_subject, "%s", ui_update->value.str_value);
                        break;
                    case UI_UPDATE_TYPE_ARTIST:
                        lv_subject_snprintf(&song_artist_subject, "%s", ui_update->value.str_value);
                        break;
                    case UI_UPDATE_TYPE_PLAY_PROGRESS:
                        lv_subject_set_int(&play_progress_subject, ui_update->value.int_value);
                        break;
                    case UI_UPDATE_TYPE_VOLUME:
                        lv_subject_set_int(&volume_subject_value, ui_update->value.int_value);
                        break;
                    case UI_UPDATE_TYPE_SONG_TIME:
                        lv_subject_snprintf(&song_time_subject, "%s", ui_update->value.str_value);
                        break;
                    case UI_UPDATE_TYPE_POWER:
                        update_power_display(ui_update->value.int_value / 10.0);
                        break;
                    case UI_UPDATE_TYPE_DAILY_ENERGY:
                        lv_subject_snprintf(&daily_energy_subject, "%s", ui_update->value.str_value);
                        break;
                    case UI_UPDATE_TYPE_MONTHLY_ENERGY:
                        lv_subject_snprintf(&monthly_energy_subject, "%s", ui_update->value.str_value);
                        break;
                    case UI_UPDATE_TYPE_ENERGY:
                        // 处理原始能耗（如果需要）
                        break;
                    case UI_UPDATE_TYPE_WEATHER_DESC:
                        lv_subject_snprintf(&weather_desc_subject, "%s", ui_update->value.str_value);
                        break;
                    case UI_UPDATE_TYPE_WEATHER_TEMP:
                        lv_subject_snprintf(&weather_temp_subject, "%s", ui_update->value.str_value);
                        break;
                    case UI_UPDATE_TYPE_WEATHER_HUM:
                        lv_subject_snprintf(&weather_hum_subject, "%s", ui_update->value.str_value);
                        break;
                    case UI_UPDATE_TYPE_INDOOR_TEMP:
                        lv_subject_snprintf(&indoor_temp_subject, "%s", ui_update->value.str_value);
                        break;
                    case UI_UPDATE_TYPE_INDOOR_HUM:
                        lv_subject_snprintf(&indoor_hum_subject, "%s", ui_update->value.str_value);
                        break;
                    case UI_UPDATE_TYPE_PLAY_STATE:
                        {
                            lv_obj_t *play_btn = lv_obj_find_by_name(lv_screen_active(), "play_btn");
                            if (play_btn) {
                                ui_set_icon(play_btn, ui_update->value.int_value ? ICON_PAUSE : ICON_PLAY);
                            }
                        }
                        break;
                    case UI_UPDATE_TYPE_ALBUM_ART:
                        ESP_LOGI(TAG, "处理 UI 封面更新事件，数据指针: %p", ui_update->value.ptr_value);
                        if (ui_update->value.ptr_value != NULL) {
                            cover_img_dsc.header.cf = LV_COLOR_FORMAT_RGB565;
                            cover_img_dsc.header.w = 100;
                            cover_img_dsc.header.h = 100;
                            cover_img_dsc.header.stride = 100 * 2;
                            cover_img_dsc.data_size = 100 * 100 * 2;
                            cover_img_dsc.data = (const uint8_t *)ui_update->value.ptr_value;
                            // 设为 NULL 再设回 &cover_img_dsc，强制触发 LVGL 通知
                            lv_subject_set_pointer(&cover_img_subject, NULL);
                            lv_subject_set_pointer(&cover_img_subject, &cover_img_dsc);
                        } else {
                            lv_subject_set_pointer(&cover_img_subject, NULL);
                        }
                        break;
                    case UI_UPDATE_TYPE_SWITCH_STATE:
                        {
                            int index = (ui_update->value.int_value >> 8) & 0xFF;
                            int state = ui_update->value.int_value & 0xFF;
                            switch (index) {
                                case 1: lv_subject_set_int(&switch_1_state, state); break;
                                case 2: lv_subject_set_int(&switch_2_state, state); break;
                                case 3: lv_subject_set_int(&switch_3_state, state); break;
                                case 4: lv_subject_set_int(&switch_4_state, state); break;
                                default: break;
                            }
                        }
                        break;
                    default:
                        break;
                }
                uint32_t sub_end = esp_log_timestamp();
                if (sub_end - sub_start > 50) {
                    ESP_LOGW(TAG, "UI更新类型 %d 耗时过长: %u ms", ui_update->type, (unsigned int)(sub_end - sub_start));
                }
                // 释放事件数据内存
                free(event.data);
            }
            // 处理完一个事件后喂狗
            esp_task_wdt_reset();
        }
        
        // 定期更新时间显示，每秒更新一次
        time_update_count++;
        if (time_update_count >= 100) { // 100 * 10ms = 1s
            time_update_count = 0;
            update_time_display();
        }
        
        // 执行 LVGL 定时器，返回下一次需要唤醒的时间
        uint32_t delay_ms = lv_timer_handler();
        
        // 限制最大延迟时间，防止因为没有渲染任务导致任务长时间不喂狗
        if (delay_ms > 20) delay_ms = 20;
        if (delay_ms < 1)  delay_ms = 1;

        // 显式重置看门狗
        esp_task_wdt_reset();
        
        // 使用动态延时以平衡 FPS 和负载
        vTaskDelay(pdMS_TO_TICKS(delay_ms)); 
    }
}

void app_main(void)
{
    // 初始化NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // 创建互斥锁，保护触摸数据访问
    g_touch_mutex = xSemaphoreCreateMutex();
    if (g_touch_mutex == NULL) {
        ESP_LOGE(TAG, "创建触摸互斥锁失败");
        return;
    }
    
    lcd_init();
    touch_init();
    lvgl_init();
    
    // 初始化事件系统
    event_system_init();
    
    // 初始化MQTT客户端
    mqtt_client_init();
    
    // 初始化WiFi连接
    wifi_init();
    
    xTaskCreate(touch_task, "touch_task", 4096, NULL, 3, NULL);
    
    // 将 LVGL 任务固定在核心 1，并降低优先级到 6
    xTaskCreatePinnedToCore(lvgl_task, "lvgl_task", 24576, NULL, 6, NULL, 1);
    
    xTaskCreate(event_system_task, "event_system_task", 8192, NULL, 5, NULL);
    xTaskCreate(ha_monitor_task, "ha_monitor_task", 8192, NULL, 4, NULL);
    
    ESP_LOGI(TAG, "应用主函数完成，任务正在运行");
    
    // 主任务可以退出，其他任务继续运行
    vTaskDelete(NULL);
}