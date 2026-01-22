/*
 * SPDX-FileCopyrightText: 2022-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#ifndef ESP32_MQTT_CLIENT_H
#define ESP32_MQTT_CLIENT_H

#include <stdint.h>
#include "esp_err.h"
#include "mqtt_client.h"

// MQTT消息结构体
typedef struct {
    char *topic;       // 消息主题
    char *data;        // 消息数据
    int data_len;      // 消息数据长度
    int qos;           // 消息QoS等级
    int retain;        // 消息保留标志
} mqtt_message_t;

// MQTT客户端配置结构体
typedef struct {
    const char *broker_url;        // MQTT服务器URL
    const char *username;          // MQTT用户名
    const char *password;          // MQTT密码
    const char *client_id;         // MQTT客户端ID
    int keepalive;                 // 心跳间隔（秒）
    bool disable_auto_reconnect;   // 是否禁用自动重连
} mqtt_client_config_t;

// MQTT客户端初始化
esp_err_t mqtt_client_init(void);

// 启动MQTT客户端连接
esp_err_t mqtt_client_start(void);

// 停止MQTT客户端连接
esp_err_t mqtt_client_stop(void);

// 订阅MQTT主题
esp_err_t mqtt_client_subscribe(const char *topic, int qos);

// 发布MQTT消息
esp_err_t mqtt_client_publish(const char *topic, const char *data, int qos, bool retain);

// 获取MQTT客户端句柄
esp_mqtt_client_handle_t mqtt_client_get_handle(void);

#endif /* ESP32_MQTT_CLIENT_H */
