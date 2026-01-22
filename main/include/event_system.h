/*
 * SPDX-FileCopyrightText: 2022-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#ifndef EVENT_SYSTEM_H
#define EVENT_SYSTEM_H

#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_event.h"

// UI更新类型枚举
typedef enum {
    UI_UPDATE_TYPE_LYRICS,              // 更新歌词
    UI_UPDATE_TYPE_SONG_NAME,           // 更新歌曲名称
    UI_UPDATE_TYPE_ARTIST,              // 更新歌手
    UI_UPDATE_TYPE_PLAY_PROGRESS,       // 更新播放进度
    UI_UPDATE_TYPE_VOLUME,              // 更新音量
    UI_UPDATE_TYPE_SONG_TIME,           // 更新歌曲时间
    UI_UPDATE_TYPE_POWER,               // 更新功耗
    UI_UPDATE_TYPE_ENERGY,              // 更新能耗
    UI_UPDATE_TYPE_DAILY_ENERGY,        // 更新每日能耗
    UI_UPDATE_TYPE_MONTHLY_ENERGY,      // 更新每月能耗
    UI_UPDATE_TYPE_WEATHER_DESC,        // 更新天气描述
    UI_UPDATE_TYPE_WEATHER_TEMP,        // 更新室外温度
    UI_UPDATE_TYPE_WEATHER_HUM,         // 更新室外湿度
    UI_UPDATE_TYPE_INDOOR_TEMP,         // 更新室内温度
    UI_UPDATE_TYPE_INDOOR_HUM,          // 更新室内湿度
    UI_UPDATE_TYPE_ALBUM_ART,           // 更新专辑封面
    UI_UPDATE_TYPE_PLAY_STATE,          // 更新播放状态
    UI_UPDATE_TYPE_SWITCH_STATE,        // 更新开关状态
    UI_UPDATE_TYPE_MAX                  // UI更新类型最大值
} ui_update_type_t;

// UI更新数据结构
typedef struct {
    ui_update_type_t type;             // 更新类型
    union {
        char str_value[256];           // 字符串值
        int int_value;                 // 整数值
        void *ptr_value;               // 指针值 (用于图片等)
    } value;                           // 更新值
} ui_update_t;

// 事件类型枚举
typedef enum {
    EVENT_TYPE_WIFI_CONNECTED,         // WiFi连接成功事件
    EVENT_TYPE_WIFI_DISCONNECTED,      // WiFi断开事件
    EVENT_TYPE_BUTTON_CLICK,           // 按钮点击事件
    EVENT_TYPE_TOUCH_EVENT,            // 触摸事件
    EVENT_TYPE_LOAD_XML,               // 加载XML事件
    EVENT_TYPE_XML_LOADED,             // XML加载完成事件
    EVENT_TYPE_REFRESH_XML,            // 刷新XML事件
    EVENT_TYPE_NTP_TIME_UPDATED,       // NTP时间更新事件
    EVENT_TYPE_MQTT_CONNECTED,         // MQTT连接成功事件
    EVENT_TYPE_MQTT_DISCONNECTED,      // MQTT断开连接事件
    EVENT_TYPE_MQTT_MESSAGE_RECEIVED,  // MQTT消息接收事件
    EVENT_TYPE_UI_UPDATE,              // UI更新事件
    EVENT_TYPE_MAX                     // 事件类型最大值
} event_type_t;

// 事件队列句柄，外部可见
extern QueueHandle_t g_system_event_queue;    // 系统事件队列
extern QueueHandle_t g_ui_event_queue;        // UI事件队列

// 事件数据结构
typedef struct {
    event_type_t type;             // 事件类型
    void *data;                    // 事件数据
    size_t data_len;               // 事件数据长度
    TickType_t send_time;          // 事件发送时间戳（单位：tick）
} event_t;

// 事件队列初始化
esp_err_t event_system_init(void);

// 发送事件
esp_err_t event_system_post(event_type_t type, void *data, size_t data_len);

// 注册事件处理函数
esp_err_t event_system_register_handler(event_type_t type, TaskHandle_t handler_task);

// 事件处理任务
void event_system_task(void *arg);

// HA 监控任务
void ha_monitor_task(void *arg);

// 24小时天气数据结构
typedef struct {
    float temperature;              // 温度(摄氏度)
    float apparent_temperature;     // 体感温度(摄氏度)
    int weather_code;               // 天气代码
    int humidity;                   // 湿度(百分比)
} hourly_weather_t;

// 获取24小时天气数据
const hourly_weather_t* get_24h_weather_data(int *count);

#endif /* EVENT_SYSTEM_H */
