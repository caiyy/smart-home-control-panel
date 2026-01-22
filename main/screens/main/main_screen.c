/*
 * SPDX-FileCopyrightText: 2022-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "lvgl.h"
#include "event_system.h"
#include "smart_control_panel_init.h"
#include "esp_task_wdt.h"
#include "esp32_mqtt_client.h"
#include "ui_common.h"
#include "http_service.h"
#include "homeassistant.h"

static const char *TAG = "main_screen";

// 播放状态
static bool g_current_play_state = false;

// 声明外部变量
extern bool g_wifi_connected;
// 声明ht16字体外部变量
extern const lv_font_t ht16;

// 时间相关的Subject和缓冲区定义
lv_subject_t date_subject;
char date_buf[64];
char prev_date_buf[64];

lv_subject_t time_subject;
char time_buf[32];
char prev_time_buf[32];

lv_subject_t weekday_subject;
char weekday_buf[32];
char prev_weekday_buf[32];

// 歌词相关的Subject定义
lv_subject_t song_lyrics_subject;
char song_lyrics_buf[256];
char prev_song_lyrics_buf[256];

// 歌曲相关的Subject定义
lv_subject_t song_name_subject;
char song_name_buf[128];
char prev_song_name_buf[128];

lv_subject_t song_artist_subject;
char song_artist_buf[128];
char prev_song_artist_buf[128];

lv_subject_t song_time_subject;
char song_time_buf[32];
char prev_song_time_buf[32];

lv_subject_t play_progress_subject;

// 控制相关的Subject定义
lv_subject_t brightness_subject_value;
lv_subject_t volume_subject_value;

// 能耗相关的Subject定义
lv_subject_t power_subject;
char power_buf[32];
char prev_power_buf[32];

lv_subject_t daily_energy_subject;
char daily_energy_buf[64];
char prev_daily_energy_buf[64];

lv_subject_t monthly_energy_subject;
char monthly_energy_buf[64];
char prev_monthly_energy_buf[64];

// 天气相关的Subject定义
lv_subject_t weather_desc_subject;
char weather_desc_buf[32];
char prev_weather_desc_buf[32];

lv_subject_t weather_temp_subject;
char weather_temp_buf[16];
char prev_weather_temp_buf[16];

lv_subject_t weather_hum_subject;
char weather_hum_buf[16];
char prev_weather_hum_buf[16];

// 室内温湿度相关的Subject定义
lv_subject_t indoor_temp_subject;
char indoor_temp_buf[16];
char prev_indoor_temp_buf[16];

lv_subject_t indoor_hum_subject;
char indoor_hum_buf[16];
char prev_indoor_hum_buf[16];

lv_subject_t cover_img_subject;

// 智能家居开关状态Subject定义
lv_subject_t switch_1_state;
lv_subject_t switch_2_state;
lv_subject_t switch_3_state;
lv_subject_t switch_4_state;
lv_subject_t switch_5_state;
lv_subject_t switch_6_state;

// 亮度变化观察者回调函数
static void brightness_observer_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    // 获取当前亮度值
    int32_t brightness_value = lv_subject_get_int(subject);
    ESP_LOGI(TAG, "亮度值变化：%d", brightness_value);
    
    // 将0-100范围转换为65-255范围
    uint8_t actual_brightness = 65 + (brightness_value * 190) / 100;
    
    // 设置实际的背光亮度
    set_backlight_brightness(actual_brightness);
}

// 音量滑块松开事件回调函数
static void volume_slider_release_cb(lv_event_t *e)
{
    // 获取滑块对象
    lv_obj_t *slider = lv_event_get_target(e);
    
    // 获取当前音量值
    int32_t volume_value = lv_slider_get_value(slider);
    
    // 发布MQTT消息，只在松开时推送，减少性能开销
    ESP_LOGI(TAG, "音量滑块松开：%d，推送MQTT消息", volume_value);
    
    // 将音量值转换为字符串
    char volume_str[8];
    sprintf(volume_str, "%" PRId32 "", volume_value);
    
    // 发布MQTT消息到音量设置主题
    esp_err_t err = mqtt_client_publish("homeassistant/number/esp32_music_player/volume/set", volume_str, 0, false);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "发布音量MQTT消息失败: %s", esp_err_to_name(err));
    }
}

// 播放控制回调函数
void play_prev_cb(lv_event_t *e)
{
    ESP_LOGI(TAG, "上一曲按钮点击 (play_prev_cb)");
    mqtt_client_publish("homeassistant/button/esp32_music_player/prev/set", "PRESS", 0, false);
}

void play_pause_cb(lv_event_t *e)
{
    g_current_play_state = !g_current_play_state;
    const char *payload = g_current_play_state ? "ON" : "OFF";
    ESP_LOGI(TAG, "播放/暂停按钮点击 (play_pause_cb): %s", payload);
    mqtt_client_publish("homeassistant/switch/esp32_music_player/play/set", payload, 0, false);
    
    // 立即更新图标状态（反馈）
    lv_obj_t *target = lv_event_get_target(e);
    // 查找其子对象 image (play_btn)
    lv_obj_t *img = lv_obj_find_by_name(target, "play_btn");
    if (img == NULL && lv_obj_check_type(target, &lv_image_class)) {
        img = target;
    }
    
    if (img) {
        ui_set_icon(img, g_current_play_state ? ICON_PAUSE : ICON_PLAY);
    }
}

void play_next_cb(lv_event_t *e)
{
    ESP_LOGI(TAG, "下一曲按钮点击 (play_next_cb)");
    mqtt_client_publish("homeassistant/button/esp32_music_player/next/set", "PRESS", 0, false);
}

// 智能家居开关点击事件回调函数
void switch_1_cb(lv_event_t *e)
{
    int32_t current_state = lv_subject_get_int(&switch_1_state);
    int32_t new_state = current_state ? 0 : 1;
    lv_subject_set_int(&switch_1_state, new_state);
    ESP_LOGI(TAG, "开关1点击 (HA)，状态: %d -> %d", current_state, new_state);
    call_ha_service("switch", new_state ? "turn_on" : "turn_off", "switch.tasmota");
}

void switch_2_cb(lv_event_t *e)
{
    int32_t current_state = lv_subject_get_int(&switch_2_state);
    int32_t new_state = current_state ? 0 : 1;
    lv_subject_set_int(&switch_2_state, new_state);
    ESP_LOGI(TAG, "开关2点击 (HA)，状态: %d -> %d", current_state, new_state);
    call_ha_service("switch", new_state ? "turn_on" : "turn_off", "switch.new_dc1_3");
}

void switch_3_cb(lv_event_t *e)
{
    int32_t current_state = lv_subject_get_int(&switch_3_state);
    int32_t new_state = current_state ? 0 : 1;
    lv_subject_set_int(&switch_3_state, new_state);
    ESP_LOGI(TAG, "开关3点击 (HA)，状态: %d -> %d", current_state, new_state);
    call_ha_service("switch", new_state ? "turn_on" : "turn_off", "switch.new_dc1_4");
}

void switch_4_cb(lv_event_t *e)
{
    int32_t current_state = lv_subject_get_int(&switch_4_state);
    int32_t new_state = current_state ? 0 : 1;
    lv_subject_set_int(&switch_4_state, new_state);
    ESP_LOGI(TAG, "开关4点击 (HA)，状态: %d -> %d", current_state, new_state);
    call_ha_service("switch", new_state ? "turn_on" : "turn_off", "switch.tasmota_2");
}

void switch_5_cb(lv_event_t *e)
{
    int32_t current_state = lv_subject_get_int(&switch_5_state);
    int32_t new_state = current_state ? 0 : 1;
    lv_subject_set_int(&switch_5_state, new_state);
    ESP_LOGI(TAG, "开关5点击，状态: %d -> %d", current_state, new_state);
    mqtt_client_publish("homeassistant/switch/monitor/set", new_state ? "ON" : "OFF", 0, false);
}

void switch_6_cb(lv_event_t *e)
{
    int32_t current_state = lv_subject_get_int(&switch_6_state);
    int32_t new_state = current_state ? 0 : 1;
    lv_subject_set_int(&switch_6_state, new_state);
    ESP_LOGI(TAG, "开关6点击，状态: %d -> %d", current_state, new_state);
    mqtt_client_publish("homeassistant/switch/speaker/set", new_state ? "ON" : "OFF", 0, false);
}

// 点击背景关闭模态窗口的回调函数
static void close_modal_bg_cb(lv_event_t *e)
{
    if (lv_event_get_target(e) == lv_event_get_current_target(e)) {
        lv_obj_delete(lv_event_get_target(e));
    }
}

// 根据天气代码获取天气描述
static const char* get_weather_desc(int weather_code)
{
    switch(weather_code) {
        case 0: return "晴";
        case 1: return "晴间多云";
        case 2: return "多云";
        case 3: return "阴";
        case 45: case 48: return "雾";
        case 51: case 53: case 55: return "小雨";
        case 56: case 57: return "冻雨";
        case 61: case 63: case 65: return "中雨";
        case 66: case 67: return "冻雨";
        case 71: case 73: case 75: return "小雪";
        case 77: return "雪粒";
        case 80: case 81: case 82: return "阵雨";
        case 85: case 86: return "阵雪";
        case 95: case 96: case 99: return "雷暴";
        default: return "未知";
    }
}

// 图表点击事件回调函数
static void chart_click_cb(lv_event_t *e)
{
    lv_obj_t *chart = lv_event_get_target(e);
    lv_indev_t *indev = lv_indev_get_act();
    lv_point_t point;
    
    // 获取点击位置
    lv_indev_get_point(indev, &point);
    
    // 获取图表容器
    lv_obj_t *chart_container = lv_obj_get_parent(chart);
    
    // 查找并删除现有的详情标签（保留标题和图例）
    lv_obj_t *child = NULL;
    uint32_t child_cnt = lv_obj_get_child_cnt(chart_container);
    for (uint32_t i = 0; i < child_cnt; i++) {
        child = lv_obj_get_child(chart_container, i);
        if (child && lv_obj_check_type(child, &lv_label_class)) {
            // 检查是否是标题或图例（通过位置判断）
            lv_coord_t y = lv_obj_get_y(child);
            if (y > 50) {  // 标题和图例在顶部，详情标签会在下方
                lv_obj_delete(child);
            }
        }
    }
    
    // 确保图表容器没有滚动条
    lv_obj_clear_flag(chart_container, LV_OBJ_FLAG_SCROLLABLE);
    
    // 创建新的详情标签
    lv_obj_t *detail_label = lv_label_create(chart_container);
    lv_obj_set_style_bg_color(detail_label, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(detail_label, LV_OPA_80, 0);
    lv_obj_set_style_text_color(detail_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_pad_all(detail_label, 10, 0);
    lv_obj_set_style_radius(detail_label, 5, 0);
    lv_obj_set_style_text_font(detail_label, &ht16, 0);
    
    // 简单获取数据点索引（基于x坐标计算）
    lv_coord_t chart_width = lv_obj_get_width(chart);
    int data_count = 0;
    get_24h_weather_data(&data_count);
    int point_index = (point.x - lv_obj_get_x(chart)) * data_count / chart_width;
    if (point_index < 0) point_index = 0;
    if (point_index >= data_count) point_index = data_count - 1;
    
    // 获取天气数据
    const hourly_weather_t *weather_data = get_24h_weather_data(&data_count);
    if (point_index >= data_count) return;
    
    // 设置详情文本，包含天气描述而非天气代码
    char detail_text[128];
    sprintf(detail_text, "时间: %d时\n温度: %.1f°C\n体感温度: %.1f°C\n湿度: %d%%\n天气: %s", 
            point_index, weather_data[point_index].temperature, 
            weather_data[point_index].apparent_temperature, 
            weather_data[point_index].humidity, get_weather_desc(weather_data[point_index].weather_code));
    lv_label_set_text(detail_label, detail_text);
    
    // 刷新标签尺寸，确保获取正确的宽度和高度
    lv_obj_update_layout(detail_label);
    
    // 获取图表容器尺寸（确保标签在容器内）
    lv_coord_t container_x = lv_obj_get_x(chart_container);
    lv_coord_t container_y = lv_obj_get_y(chart_container);
    lv_coord_t container_width = lv_obj_get_width(chart_container);
    lv_coord_t container_height = lv_obj_get_height(chart_container);
    
    // 获取标签尺寸
    lv_coord_t label_width = lv_obj_get_width(detail_label);
    lv_coord_t label_height = lv_obj_get_height(detail_label);
    
    // 计算标签位置，基于容器坐标系
    lv_coord_t label_x = point.x - container_x + 10;
    lv_coord_t label_y = point.y - container_y - label_height / 2;
    
    // 调整X坐标（确保在容器内）
    if (label_x + label_width > container_width - 20) {
        label_x = container_width - label_width - 20;
    }
    if (label_x < 20) {
        label_x = 20;
    }
    
    // 调整Y坐标（确保在容器内）
    if (label_y < 20) {
        label_y = 20;
    } else if (label_y + label_height > container_height - 20) {
        label_y = container_height - label_height - 20;
    }
    
    // 设置标签位置（相对于容器）
    lv_obj_align(detail_label, LV_ALIGN_TOP_LEFT, label_x, label_y);
}

// 天气点击回调函数 - 显示24小时天气图表
void weather_click_cb(lv_event_t *e)
{
    ESP_LOGI(TAG, "天气区域被点击,显示24小时天气图表");
    
    // 获取24小时天气数据
    int data_count = 0;
    const hourly_weather_t *weather_data = get_24h_weather_data(&data_count);
    
    if (data_count == 0) {
        ESP_LOGW(TAG, "暂无24小时天气数据");
        return;
    }
    
    // 创建模态背景(不透明,节省资源)
    lv_obj_t *modal_bg = lv_obj_create(lv_screen_active());
    lv_obj_set_size(modal_bg, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(modal_bg, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(modal_bg, LV_OPA_0, 0);  // 0%不透明度
    lv_obj_set_style_border_width(modal_bg, 0, 0);
    lv_obj_set_style_pad_all(modal_bg, 0, 0);
    lv_obj_clear_flag(modal_bg, LV_OBJ_FLAG_SCROLLABLE);
    
    // 创建图表容器
    lv_obj_t *chart_container = lv_obj_create(modal_bg);
    lv_obj_set_size(chart_container, 420, 320);
    lv_obj_center(chart_container);
    lv_obj_set_style_bg_color(chart_container, lv_color_hex(0xF8F2E6), 0);
    lv_obj_set_style_border_color(chart_container, lv_color_hex(0xff3b80), 0);
    lv_obj_set_style_border_width(chart_container, 2, 0);
    lv_obj_set_style_radius(chart_container, 20, 0);
    lv_obj_set_style_pad_all(chart_container, 15, 0);
    lv_obj_set_style_text_font(chart_container, &ht16, 0);
    
    // 添加标题
    lv_obj_t *title = lv_label_create(chart_container);
    lv_label_set_recolor(title, true);
    lv_label_set_text(title, "24小时天气预报   #FF6B6B 温度 (°C)# / #4ECDC4 体感温度 (°C)# / #6BCB77 湿度 (%)#");
    lv_obj_set_style_text_color(title, lv_color_hex(0xff3b80), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_text_font(title, &ht16, 0);
    
    // 创建图表
    lv_obj_t *chart = lv_chart_create(chart_container);
    lv_obj_set_size(chart, 380, 250);
    lv_obj_align(chart, LV_ALIGN_TOP_MID, 0, 30);
    lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(chart, data_count);
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, -10, 40);  // 温度范围-10到40度
    lv_chart_set_range(chart, LV_CHART_AXIS_SECONDARY_Y, 0, 100);  // 湿度范围0到100%
    
    // 设置图表样式
    lv_obj_set_style_bg_color(chart, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_color(chart, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_border_width(chart, 1, 0);
    lv_obj_set_style_radius(chart, 10, 0);
    lv_obj_set_style_pad_all(chart, 10, 0);
    
    // 添加温度数据系列
    lv_chart_series_t *temp_series = lv_chart_add_series(chart, lv_color_hex(0xFF6B6B), LV_CHART_AXIS_PRIMARY_Y);
    
    // 添加体感温度数据系列
    lv_chart_series_t *apparent_temp_series = lv_chart_add_series(chart, lv_color_hex(0x4ECDC4), LV_CHART_AXIS_PRIMARY_Y);
    
    // 添加湿度数据系列
    lv_chart_series_t *hum_series = lv_chart_add_series(chart, lv_color_hex(0x6BCB77), LV_CHART_AXIS_SECONDARY_Y);
    
    // 填充温度、体感温度和湿度数据
    for (int i = 0; i < data_count; i++) {
        lv_chart_set_next_value(chart, temp_series, (int32_t)weather_data[i].temperature);
        lv_chart_set_next_value(chart, apparent_temp_series, (int32_t)weather_data[i].apparent_temperature);
        lv_chart_set_next_value(chart, hum_series, (int32_t)weather_data[i].humidity);
    }
    
    // 刷新图表
    lv_chart_refresh(chart);
    // 图表点击事件
    lv_obj_add_event_cb(chart, chart_click_cb, LV_EVENT_CLICKED, NULL);
    
    // 点击背景关闭
    lv_obj_add_event_cb(modal_bg, close_modal_bg_cb, LV_EVENT_CLICKED, NULL);
}


// 初始化主屏幕Subject
void init_main_screen_subjects(void)
{
    ESP_LOGI(TAG, "初始化主屏幕Subject");
    
    // 初始化时间相关的Subject
    lv_subject_init_string(&date_subject, date_buf, prev_date_buf, sizeof(date_buf), "01-#FF0000 01#");
    lv_subject_init_string(&time_subject, time_buf, prev_time_buf, sizeof(time_buf), "00:#FF0000 00#");
    lv_subject_init_string(&weekday_subject, weekday_buf, prev_weekday_buf, sizeof(weekday_buf), "星期#FF0000 一#");
    
    // 初始化歌词相关的Subject
    lv_subject_init_string(&song_lyrics_subject, song_lyrics_buf, prev_song_lyrics_buf, sizeof(song_lyrics_buf), "暂无歌词");
    
    // 初始化歌曲相关的Subject
    lv_subject_init_string(&song_name_subject, song_name_buf, prev_song_name_buf, sizeof(song_name_buf), "不能说的秘密");
    lv_subject_init_string(&song_artist_subject, song_artist_buf, prev_song_artist_buf, sizeof(song_artist_buf), "周杰伦");
    lv_subject_init_string(&song_time_subject, song_time_buf, prev_song_time_buf, sizeof(song_time_buf), "00:00 / 00:00");
    lv_subject_init_int(&play_progress_subject, 0);
    
    // 从NVS读取亮度值
    uint8_t actual_brightness = read_brightness_from_nvs();
    // 将65-255范围转换为0-100范围
    int32_t brightness_value = ((actual_brightness - 65) * 100) / 190;
    if (brightness_value < 0) brightness_value = 0;
    if (brightness_value > 100) brightness_value = 100;
    
    // 初始化控制相关的Subject
    lv_subject_init_int(&brightness_subject_value, brightness_value);
    lv_subject_init_int(&volume_subject_value, 50);
    
    // 为亮度Subject添加观察者
    lv_subject_add_observer(&brightness_subject_value, brightness_observer_cb, NULL);
    
    // 移除直接的观察者，改为在滑块松开事件中发布MQTT消息
    // 这样可以减少性能开销，只在操作结束时推送一次
    
    // 注册时间相关的Subject到XML全局作用域
    lv_xml_register_subject(NULL, "date_subject", &date_subject);
    lv_xml_register_subject(NULL, "time_subject", &time_subject);
    lv_xml_register_subject(NULL, "weekday_subject", &weekday_subject);
    
    // 注册歌曲相关的Subject到XML全局作用域
    lv_xml_register_subject(NULL, "song_lyrics_subject", &song_lyrics_subject);
    lv_xml_register_subject(NULL, "song_name_subject", &song_name_subject);
    lv_xml_register_subject(NULL, "song_artist_subject", &song_artist_subject);
    lv_xml_register_subject(NULL, "song_time_subject", &song_time_subject);
    lv_xml_register_subject(NULL, "play_progress_subject", &play_progress_subject);
    
    // 初始化能耗相关的Subject
    lv_subject_init_string(&power_subject, power_buf, prev_power_buf, sizeof(power_buf), "#04905E --##5F6777W#");
    lv_subject_init_string(&daily_energy_subject, daily_energy_buf, prev_daily_energy_buf, sizeof(daily_energy_buf), " #FF0000 0.0# #5F6777 kW##04905E $0.00#");
    lv_subject_init_string(&monthly_energy_subject, monthly_energy_buf, prev_monthly_energy_buf, sizeof(monthly_energy_buf), " #FF0000 0.0# #5F6777 kW##04905E $0.00#");
    
    // 注册控制相关的Subject到XML全局作用域
    lv_xml_register_subject(NULL, "brightness_subject_value", &brightness_subject_value);
    lv_xml_register_subject(NULL, "volume_subject_value", &volume_subject_value);
    
    // 注册能耗相关的Subject到XML全局作用域
    lv_xml_register_subject(NULL, "power_subject", &power_subject);
    lv_xml_register_subject(NULL, "daily_energy_subject", &daily_energy_subject);
    lv_xml_register_subject(NULL, "monthly_energy_subject", &monthly_energy_subject);
    
    // 初始化天气相关的Subject
    lv_subject_init_string(&weather_desc_subject, weather_desc_buf, prev_weather_desc_buf, sizeof(weather_desc_buf), "晴");
    lv_subject_init_string(&weather_temp_subject, weather_temp_buf, prev_weather_temp_buf, sizeof(weather_temp_buf), "--|--C");
    lv_subject_init_string(&weather_hum_subject, weather_hum_buf, prev_weather_hum_buf, sizeof(weather_hum_buf), "--%");
    
    // 初始化室内相关的Subject
    lv_subject_init_string(&indoor_temp_subject, indoor_temp_buf, prev_indoor_temp_buf, sizeof(indoor_temp_buf), "--°C");
    lv_subject_init_string(&indoor_hum_subject, indoor_hum_buf, prev_indoor_hum_buf, sizeof(indoor_hum_buf), "--%");
    
    // 注册天气和室内Subject到XML全局作用域
    lv_xml_register_subject(NULL, "weather_desc_subject", &weather_desc_subject);
    lv_xml_register_subject(NULL, "weather_temp_subject", &weather_temp_subject);
    lv_xml_register_subject(NULL, "weather_hum_subject", &weather_hum_subject);
    lv_xml_register_subject(NULL, "indoor_temp_subject", &indoor_temp_subject);
    lv_xml_register_subject(NULL, "indoor_hum_subject", &indoor_hum_subject);
    
    // 初始化并注册封面图 Subject
    lv_subject_init_pointer(&cover_img_subject, NULL);
    lv_xml_register_subject(NULL, "cover_img_subject", &cover_img_subject);
    
    // 初始化并注册智能家居开关状态Subject
    lv_subject_init_int(&switch_1_state, 0);
    lv_subject_init_int(&switch_2_state, 0);
    lv_subject_init_int(&switch_3_state, 0);
    lv_subject_init_int(&switch_4_state, 0);
    lv_subject_init_int(&switch_5_state, 0);
    lv_subject_init_int(&switch_6_state, 0);
    
    lv_xml_register_subject(NULL, "switch_1_state", &switch_1_state);
    lv_xml_register_subject(NULL, "switch_2_state", &switch_2_state);
    lv_xml_register_subject(NULL, "switch_3_state", &switch_3_state);
    lv_xml_register_subject(NULL, "switch_4_state", &switch_4_state);
    lv_xml_register_subject(NULL, "switch_5_state", &switch_5_state);
    lv_xml_register_subject(NULL, "switch_6_state", &switch_6_state);
    
    ESP_LOGI(TAG, "主屏幕Subject初始化完成");
}

// XML加载结果结构体
typedef struct {
    char *xml_data;
    bool success;
} xml_load_result_t;

// 从网络加载XML文件函数已移除，改用 http_service.h 中的 http_send_request

// XML加载任务声明
void load_xml_task(void *arg);

// 刷新XML回调函数
void refresh_xml_cb(lv_event_t *e)
{
    ESP_LOGI(TAG, "刷新XML回调函数被调用");
    
    // 检查WiFi连接状态
    if (!g_wifi_connected) {
        ESP_LOGE(TAG, "WiFi未连接，无法刷新XML");
        return;
    }
    
    // 清除当前屏幕上的所有对象
    lv_obj_clean(lv_screen_active());
    
    // 创建XML加载任务，在后台执行网络请求
    xTaskCreate(load_xml_task, "load_xml_task", 8192, NULL, 3, NULL);
    
    ESP_LOGI(TAG, "已启动XML刷新任务");
}

// XML加载完成回调函数
void on_xml_loaded(xml_load_result_t *result)
{
    if (!result->success) {
        ESP_LOGE(TAG, "XML加载失败，显示错误信息");
        
        // 加载失败时显示错误信息
        lv_obj_t *error_label = lv_label_create(lv_scr_act());
        lv_label_set_text(error_label, "XML Load Failed");
        lv_obj_align(error_label, LV_ALIGN_CENTER, 0, 0);
        
        return;
    }
    
    // ESP_LOGI(TAG, "XML内容: %s", result->xml_data); // 移除大日志输出，防止缓冲区溢出
    
    // 增加看门狗重置
    esp_task_wdt_reset();
    vTaskDelay(pdMS_TO_TICKS(1)); // 主动让出 CPU 一下

    // 注册XML组件
    if (lv_xml_register_component_from_data("main", result->xml_data) != LV_RES_OK) {
        ESP_LOGE(TAG, "注册XML组件失败");
        free(result->xml_data);
        
        lv_obj_t *error_label = lv_label_create(lv_scr_act());
        lv_label_set_text(error_label, "XML Register Failed");
        lv_obj_align(error_label, LV_ALIGN_CENTER, 0, 0);
        
        return;
    }
    free(result->xml_data);
    
    // 增加小延迟，让IDLE任务有机会执行
    vTaskDelay(pdMS_TO_TICKS(1));
    
    // 注册XML回调函数
    esp_task_wdt_reset();
    if (lv_xml_register_event_cb(NULL, "refresh_xml_cb", refresh_xml_cb) != LV_RES_OK) {
        ESP_LOGE(TAG, "注册XML回调函数失败");
    } else {
        ESP_LOGI(TAG, "成功注册XML回调函数");
    }
    
    // 注册播放控制回调
    lv_xml_register_event_cb(NULL, "play_prev_cb", play_prev_cb);
    lv_xml_register_event_cb(NULL, "play_pause_cb", play_pause_cb);
    lv_xml_register_event_cb(NULL, "play_next_cb", play_next_cb);
    
    // 注册智能家居开关回调
    lv_xml_register_event_cb(NULL, "switch_1_cb", switch_1_cb);
    lv_xml_register_event_cb(NULL, "switch_2_cb", switch_2_cb);
    lv_xml_register_event_cb(NULL, "switch_3_cb", switch_3_cb);
    lv_xml_register_event_cb(NULL, "switch_4_cb", switch_4_cb);
    lv_xml_register_event_cb(NULL, "switch_5_cb", switch_5_cb);
    lv_xml_register_event_cb(NULL, "switch_6_cb", switch_6_cb);
    
    // 注册天气点击回调
    lv_xml_register_event_cb(NULL, "weather_click_cb", weather_click_cb);
    
    // 增加小延迟，让IDLE任务有机会执行
    vTaskDelay(pdMS_TO_TICKS(5));
    esp_task_wdt_reset();
    
    ESP_LOGI(TAG, "准备创建UI界面");
    
    // 使用XML组件创建主界面
    const char *main_attrs[] = {
        "message", "Hello World",
        NULL, NULL,
    };
    lv_obj_t *main_ui = (lv_obj_t *) lv_xml_create(lv_screen_active(), "main", main_attrs);
    esp_task_wdt_reset();
    vTaskDelay(pdMS_TO_TICKS(5)); // 创建完大 UI 后给一点喘息时间
    if (main_ui == NULL) {
        ESP_LOGE(TAG, "从XML创建主UI失败");
        
        lv_obj_t *error_label = lv_label_create(lv_scr_act());
        lv_label_set_text(error_label, "UI Create Failed");
        lv_obj_align(error_label, LV_ALIGN_CENTER, 0, 0);
        
        return;
    }
    
    // 增加小延迟，让IDLE任务有机会执行
    vTaskDelay(pdMS_TO_TICKS(1));
    
    // 查找音量滑块并添加松开事件回调
    lv_obj_t *volume_slider = lv_obj_find_by_name(main_ui, "song_slider");
    if (volume_slider != NULL) {
        ESP_LOGI(TAG, "找到音量滑块，添加松开事件回调");
        // 添加松开事件回调，只在松开时推送MQTT消息
        lv_obj_add_event_cb(volume_slider, volume_slider_release_cb, LV_EVENT_RELEASED, NULL);
    }
    
    // 手动应用图标偏移 (解决 XML 无法处理 ICON_OFFSET 的问题)
    lv_obj_t *img_obj;
    if ((img_obj = lv_obj_find_by_name(main_ui, "prev_btn"))) {
        ui_set_icon(img_obj, ICON_PREV);
    }
    if ((img_obj = lv_obj_find_by_name(main_ui, "play_btn"))) {
        ui_set_icon(img_obj, ICON_PLAY);
    }
    if ((img_obj = lv_obj_find_by_name(main_ui, "next_btn"))) {
        ui_set_icon(img_obj, ICON_NEXT);
    }
    
    // 手动设置滑块图标
    if ((img_obj = lv_obj_find_by_name(main_ui, "volume_icon"))) {
        ui_set_icon(img_obj, ICON_VOL_SMALL);
    }
    if ((img_obj = lv_obj_find_by_name(main_ui, "brightness_icon"))) {
        ui_set_icon(img_obj, ICON_BRIGHT_SMALL);
    }
    
    // 设置智能家居开关图标
    if ((img_obj = lv_obj_find_by_name(main_ui, "sw_icon_1"))) { ui_set_icon(img_obj, ICON_LIGHT); }
    if ((img_obj = lv_obj_find_by_name(main_ui, "sw_icon_2"))) { ui_set_icon(img_obj, ICON_MONITOR); }
    if ((img_obj = lv_obj_find_by_name(main_ui, "sw_icon_3"))) { ui_set_icon(img_obj, ICON_MONITOR); }
    if ((img_obj = lv_obj_find_by_name(main_ui, "sw_icon_4"))) { ui_set_icon(img_obj, ICON_SPEAKER); }
    if ((img_obj = lv_obj_find_by_name(main_ui, "sw_icon_5"))) { ui_set_icon(img_obj, ICON_MONITOR); }
    if ((img_obj = lv_obj_find_by_name(main_ui, "sw_icon_6"))) { ui_set_icon(img_obj, ICON_SPEAKER); }
    
    ESP_LOGI(TAG, "从XML创建主UI成功");
}

/**
 * @brief 更新功耗显示
 * @param power 功耗瓦数
 */
void update_power_display(float power)
{
    char buf[32];
    sprintf(buf, " #04905E %.0f##5F6777 W#", power);
    lv_subject_snprintf(&power_subject, "%s", buf);
    ESP_LOGI(TAG, "更新功耗显示: %.0fW", power);
}

/**
 * @brief 更新能耗显示
 * @param daily_energy 今日能耗（度）
 * @param monthly_energy 本月能耗（度）
 */
void update_energy_display(float daily_energy, float monthly_energy)
{
    // 计算费用（电价1.2元/度）
    float daily_cost = daily_energy * 1.2;
    float monthly_cost = monthly_energy * 1.2;
    
    // 更新今日能耗显示
    char daily_buf[64];
    sprintf(daily_buf, " #FF0000 %.1f# #5F6777 kW##04905E $%.2f#", daily_energy, daily_cost);
    lv_subject_snprintf(&daily_energy_subject, "%s", daily_buf);
    
    // 更新本月能耗显示
    char monthly_buf[64];
    sprintf(monthly_buf, " #FF0000 %.1f# #5F6777 kW##04905E $%.2f#", monthly_energy, monthly_cost);
    lv_subject_snprintf(&monthly_energy_subject, "%s", monthly_buf);
    
    ESP_LOGI(TAG, "更新能耗显示: 今日%.1f度($%.2f), 本月%.1f度($%.2f)", 
             daily_energy, daily_cost, monthly_energy, monthly_cost);
}

// XML加载任务
void load_xml_task(void *arg)
{
    ESP_LOGI(TAG, "开始执行XML加载任务 (Network)");
    
    // 从网络加载XML文件
    // 注意：请确保你的电脑/服务器IP地址正确，并且文件路径为 /main.xml
    const char *xml_url = "http://192.168.1.218/main.xml";
    
    http_config_t http_cfg = {
        .url = xml_url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = 8000
    };
    char *main_xml = http_send_request(&http_cfg);
    
    // 创建XML加载结果
    xml_load_result_t *result = malloc(sizeof(xml_load_result_t));
    if (result == NULL) {
        ESP_LOGE(TAG, "分配XML加载结果内存失败");
        if (main_xml) free(main_xml);
        vTaskDelete(NULL);
        return;
    }
    
    result->xml_data = main_xml;
    result->success = (main_xml != NULL);
    
    // 发送XML加载完成事件
    event_system_post(EVENT_TYPE_XML_LOADED, result, sizeof(xml_load_result_t));
    
    ESP_LOGI(TAG, "XML加载任务完成");
    vTaskDelete(NULL);
}

// 初始化主屏幕UI
void init_main_screen(void)
{
    // 检查WiFi连接状态
    if (!g_wifi_connected) {
        ESP_LOGE(TAG, "WiFi未连接，无法加载网络XML");
        
        // 显示WiFi连接失败信息
        lv_obj_t *error_label = lv_label_create(lv_scr_act());
        lv_label_set_text(error_label, "WiFi Not Connected");
        lv_obj_align(error_label, LV_ALIGN_CENTER, 0, 0);
        
        return;
    }
    
    // 创建XML加载任务，在后台执行网络请求
    xTaskCreate(load_xml_task, "load_xml_task", 12288, NULL, 3, NULL);
    
    ESP_LOGI(TAG, "已启动XML加载任务");
}

