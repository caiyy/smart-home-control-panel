/*
 * SPDX-FileCopyrightText: 2022-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "esp32_mqtt_client.h"
#include "event_system.h"

static const char *TAG = "mqtt_client";

// 外部WiFi状态检查函数
extern bool is_wifi_connected(void);

// MQTT客户端句柄
static esp_mqtt_client_handle_t g_mqtt_client = NULL;

// MQTT服务器配置
#define MQTT_BROKER_URL "mqtt://192.168.1.115:1883" // 修正IP地址，192.158应为192.168
#define MQTT_USERNAME "caiyy"
#define MQTT_PASSWORD "yangyong1229"
#define MQTT_CLIENT_ID "esp32_jt"
#define MQTT_KEEPALIVE 30

// 订阅主题
#define MQTT_SUBSCRIBE_TOPIC_LYRICS "homeassistant/sensor/esp32_music_player/lyrics/state"
#define MQTT_SUBSCRIBE_TOPIC_SONG "homeassistant/sensor/esp32_music_player/song/state"
#define MQTT_SUBSCRIBE_TOPIC_ARTIST "homeassistant/sensor/esp32_music_player/artist/state"
#define MQTT_SUBSCRIBE_TOPIC_POSITION "homeassistant/number/esp32_music_player/position/state"
#define MQTT_SUBSCRIBE_TOPIC_VOLUME "homeassistant/number/esp32_music_player/volume/state"
#define MQTT_SUBSCRIBE_TOPIC_PROGRESS "homeassistant/sensor/esp32_music_player/progress/state"
// 能耗相关主题 - Tasmota设备
#define MQTT_SUBSCRIBE_TOPIC_TASMOTA_ENERGY "tele/tasmota_A0DA50/SENSOR"
#define MQTT_SUBSCRIBE_TOPIC_URL "homeassistant/sensor/esp32_music_player/url/state"
#define MQTT_SUBSCRIBE_TOPIC_PLAY_STATE "homeassistant/switch/esp32_music_player/play/state"
#define MQTT_SUBSCRIBE_QOS 0

// MQTT事件处理回调函数
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "事件ID: %d", event_id);
    esp_mqtt_event_handle_t event = event_data;
    
    switch (event_id) {
        case MQTT_EVENT_CONNECTED:
            // ESP_LOGI(TAG, "MQTT连接成功");
            // 订阅所有主题
            esp_mqtt_client_subscribe(g_mqtt_client, MQTT_SUBSCRIBE_TOPIC_LYRICS, MQTT_SUBSCRIBE_QOS);
            esp_mqtt_client_subscribe(g_mqtt_client, MQTT_SUBSCRIBE_TOPIC_SONG, MQTT_SUBSCRIBE_QOS);
            esp_mqtt_client_subscribe(g_mqtt_client, MQTT_SUBSCRIBE_TOPIC_ARTIST, MQTT_SUBSCRIBE_QOS);
            esp_mqtt_client_subscribe(g_mqtt_client, MQTT_SUBSCRIBE_TOPIC_POSITION, MQTT_SUBSCRIBE_QOS);
            esp_mqtt_client_subscribe(g_mqtt_client, MQTT_SUBSCRIBE_TOPIC_VOLUME, MQTT_SUBSCRIBE_QOS);
            esp_mqtt_client_subscribe(g_mqtt_client, MQTT_SUBSCRIBE_TOPIC_PROGRESS, MQTT_SUBSCRIBE_QOS);
            // 订阅Tasmota能耗主题
            esp_mqtt_client_subscribe(g_mqtt_client, MQTT_SUBSCRIBE_TOPIC_TASMOTA_ENERGY, MQTT_SUBSCRIBE_QOS);
            // 订阅封面 URL 主题
            esp_mqtt_client_subscribe(g_mqtt_client, MQTT_SUBSCRIBE_TOPIC_URL, MQTT_SUBSCRIBE_QOS);
            // 订阅播放状态主题
            esp_mqtt_client_subscribe(g_mqtt_client, MQTT_SUBSCRIBE_TOPIC_PLAY_STATE, MQTT_SUBSCRIBE_QOS);
            
            // ESP_LOGI(TAG, "已订阅所有主题");
            // 发送MQTT连接成功事件
            event_system_post(EVENT_TYPE_MQTT_CONNECTED, NULL, 0);
            break;
        case MQTT_EVENT_DISCONNECTED:
            // 检查WiFi连接状态
            if (!is_wifi_connected()) {
                ESP_LOGI(TAG, "MQTT断开连接(WiFi已断开)");
            } else {
                ESP_LOGW(TAG, "MQTT断开连接(WiFi正常)");
            }
            // 发送MQTT断开连接事件
            event_system_post(EVENT_TYPE_MQTT_DISCONNECTED, NULL, 0);
            break;
        case MQTT_EVENT_SUBSCRIBED:
            // ESP_LOGI(TAG, "订阅成功, 消息ID: %d", event->msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            // ESP_LOGI(TAG, "取消订阅成功, 消息ID: %d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            // ESP_LOGI(TAG, "发布成功, 消息ID: %d", event->msg_id);
            break;
        case MQTT_EVENT_DATA:
            // ESP_LOGI(TAG, "收到MQTT消息");
            // ESP_LOGI(TAG, "主题: %.*s", event->topic_len, event->topic);
            // ESP_LOGI(TAG, "数据: %.*s", event->data_len, event->data);
            
            // 分配内存存储MQTT消息
            mqtt_message_t *mqtt_msg = malloc(sizeof(mqtt_message_t));
            if (mqtt_msg == NULL) {
                // ESP_LOGE(TAG, "分配MQTT消息内存失败");
                break;
            }
            
            // 复制主题
            mqtt_msg->topic = malloc(event->topic_len + 1);
            if (mqtt_msg->topic == NULL) {
                // ESP_LOGE(TAG, "分配主题内存失败");
                free(mqtt_msg);
                break;
            }
            memcpy(mqtt_msg->topic, event->topic, event->topic_len);
            mqtt_msg->topic[event->topic_len] = '\0';
            
            // 复制数据
            mqtt_msg->data = malloc(event->data_len + 1);
            if (mqtt_msg->data == NULL) {
                // ESP_LOGE(TAG, "分配数据内存失败");
                free(mqtt_msg->topic);
                free(mqtt_msg);
                break;
            }
            memcpy(mqtt_msg->data, event->data, event->data_len);
            mqtt_msg->data[event->data_len] = '\0';
            
            mqtt_msg->data_len = event->data_len;
            mqtt_msg->qos = event->qos;
            mqtt_msg->retain = event->retain;
            
            // 发送MQTT消息接收事件
            event_system_post(EVENT_TYPE_MQTT_MESSAGE_RECEIVED, mqtt_msg, sizeof(mqtt_message_t));
            break;
        case MQTT_EVENT_ERROR:
            // ESP_LOGE(TAG, "MQTT错误");
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                // ESP_LOGE(TAG, "TCP传输错误: %s", strerror(event->error_handle->esp_tls_last_esp_err));
                // 打印更多错误信息
                // ESP_LOGE(TAG, "连接返回码: %d", event->error_handle->connect_return_code);
                // ESP_LOGE(TAG, "系统错误码: %d", event->error_handle->esp_tls_last_esp_err);
            }
            break;
        case MQTT_EVENT_BEFORE_CONNECT:
            // ESP_LOGI(TAG, "MQTT准备连接");
            break;
        default:
            // ESP_LOGW(TAG, "未知MQTT事件: %d", event_id);
            break;
    }
}

// MQTT客户端初始化
esp_err_t mqtt_client_init(void)
{
    // ESP_LOGI(TAG, "初始化MQTT客户端");
    
    // 配置MQTT客户端
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER_URL,
        .credentials.username = MQTT_USERNAME,
        .credentials.authentication.password = MQTT_PASSWORD,
        .credentials.client_id = MQTT_CLIENT_ID,
        .session.keepalive = MQTT_KEEPALIVE,
        .network.disable_auto_reconnect = false,
    };
    
    // 创建MQTT客户端
    g_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (g_mqtt_client == NULL) {
        // ESP_LOGE(TAG, "创建MQTT客户端失败");
        return ESP_FAIL;
    }
    
    // 注册MQTT事件处理回调
    esp_mqtt_client_register_event(g_mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    
    // ESP_LOGI(TAG, "MQTT客户端初始化成功");
    return ESP_OK;
}

// 启动MQTT客户端连接
esp_err_t mqtt_client_start(void)
{
    if (g_mqtt_client == NULL) {
        // ESP_LOGE(TAG, "MQTT客户端未初始化");
        return ESP_ERR_INVALID_STATE;
    }
    
    // ESP_LOGI(TAG, "启动MQTT客户端连接");
    esp_err_t err = esp_mqtt_client_start(g_mqtt_client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "启动MQTT客户端失败: %s", esp_err_to_name(err));
        return err;
    }
    
    // ESP_LOGI(TAG, "MQTT客户端启动成功");
    return ESP_OK;
}

// 停止MQTT客户端连接
esp_err_t mqtt_client_stop(void)
{
    if (g_mqtt_client == NULL) {
        // ESP_LOGE(TAG, "MQTT客户端未初始化");
        return ESP_ERR_INVALID_STATE;
    }
    
    // ESP_LOGI(TAG, "停止MQTT客户端连接");
    esp_err_t err = esp_mqtt_client_stop(g_mqtt_client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "停止MQTT客户端失败: %s", esp_err_to_name(err));
        return err;
    }
    
    // ESP_LOGI(TAG, "MQTT客户端停止成功");
    return ESP_OK;
}

// 订阅MQTT主题
esp_err_t mqtt_client_subscribe(const char *topic, int qos)
{
    if (g_mqtt_client == NULL) {
        // ESP_LOGE(TAG, "MQTT客户端未初始化");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (topic == NULL) {
        // ESP_LOGE(TAG, "主题为空");
        return ESP_ERR_INVALID_ARG;
    }
    
    // ESP_LOGI(TAG, "订阅主题: %s, QoS: %d", topic, qos);
    int msg_id = esp_mqtt_client_subscribe(g_mqtt_client, topic, qos);
    if (msg_id < 0) {
        // ESP_LOGE(TAG, "订阅主题失败");
        return ESP_FAIL;
    }
    
    // ESP_LOGI(TAG, "订阅请求发送成功, 消息ID: %d", msg_id);
    return ESP_OK;
}

// 发布MQTT消息
esp_err_t mqtt_client_publish(const char *topic, const char *data, int qos, bool retain)
{
    if (g_mqtt_client == NULL) {
        // ESP_LOGE(TAG, "MQTT客户端未初始化");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (topic == NULL || data == NULL) {
        // ESP_LOGE(TAG, "主题或数据为空");
        return ESP_ERR_INVALID_ARG;
    }
    
    // ESP_LOGI(TAG, "发布消息到主题: %s, QoS: %d, retain: %d", topic, qos, retain);
    int msg_id = esp_mqtt_client_publish(g_mqtt_client, topic, data, strlen(data), qos, retain);
    if (msg_id < 0) {
        // ESP_LOGE(TAG, "发布消息失败");
        return ESP_FAIL;
    }
    
    // ESP_LOGI(TAG, "发布请求发送成功, 消息ID: %d", msg_id);
    return ESP_OK;
}

// 获取MQTT客户端句柄
esp_mqtt_client_handle_t mqtt_client_get_handle(void)
{
    return g_mqtt_client;
}
