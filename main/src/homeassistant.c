#include <string.h>
#include <esp_log.h>
#include <cJSON.h>
#include "homeassistant.h"
#include "http_service.h"

static const char *TAG = "homeassistant";

// Home Assistant 配置
#define HA_BASE_URL "http://192.168.1.115:8123/api/states/"
#define HA_TOKEN "Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiIyMzdlOWZhYzgzZTU0NmMzYjYyZDMwODA1MzU5ZWU1ZCIsImlhdCI6MTcxNDQ2NDkxMywiZXhwIjoyMDI5ODI0OTEzfQ.menJwDAD3JbQKM0rpbv44KpTTR0ZGLP-GNWvOQuhbUY"

// 能耗 Entity ID
#define ENTITY_DAILY_ENERGY "sensor.daily_energy_consumption"
#define ENTITY_MONTHLY_ENERGY "sensor.monthly_energy_consumption"

// 辅助函数：获取单个实体的状态
char *get_entity_state(const char *entity_id) {
    ESP_LOGI(TAG, "获取实体状态: %s", entity_id);
    
    char url[256];
    snprintf(url, sizeof(url), "%s%s", HA_BASE_URL, entity_id);

    const char *headers[] = {
        "Authorization", HA_TOKEN,
        "Content-Type", "application/json",
        "User-Agent", "ESP32-S3-LVGL9",
        NULL
    };

    http_config_t http_cfg = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .headers = headers,
        .timeout_ms = 3000
    };

    char *response = http_send_request_with_retry(&http_cfg, 2); // 使用重试机制，最多重试2次
    char *state = NULL;

    if (response) {
        cJSON *root = cJSON_Parse(response);
        if (root != NULL) {
            cJSON *state_json = cJSON_GetObjectItem(root, "state");
            if (cJSON_IsString(state_json)) {
                state = strdup(state_json->valuestring);
                ESP_LOGI(TAG, "解析到状态值: %s", state);
            } else {
                ESP_LOGE(TAG, "未找到有效的state字段");
            }
            cJSON_Delete(root);
        } else {
            ESP_LOGE(TAG, "JSON解析失败");
        }
        free(response);
    }

    ESP_LOGI(TAG, "返回状态值: %s", state ? state : "NULL");
    return state;
}

// 获取每日能耗
char *get_daily_energy(void) {
    return get_entity_state(ENTITY_DAILY_ENERGY);
}

// 获取每月能耗
char *get_monthly_energy(void) {
    return get_entity_state(ENTITY_MONTHLY_ENERGY);
}

// 调用 HomeAssistant 服务
esp_err_t call_ha_service(const char *domain, const char *service, const char *entity_id) {
    ESP_LOGI(TAG, "调用服务: %s.%s 实体: %s", domain, service, entity_id);
    
    char url[256];
    snprintf(url, sizeof(url), "http://192.168.1.115:8123/api/services/%s/%s", domain, service);

    const char *headers[] = {
        "Authorization", HA_TOKEN,
        "Content-Type", "application/json",
        "User-Agent", "ESP32-S3-LVGL9",
        NULL
    };

    char post_data[128];
    snprintf(post_data, sizeof(post_data), "{\"entity_id\": \"%s\"}", entity_id);

    http_config_t http_cfg = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .headers = headers,
        .post_data = post_data,
        .timeout_ms = 3000
    };

    char *response = http_send_request_with_retry(&http_cfg, 2); // 使用重试机制，最多重试2次
    if (response) {
        ESP_LOGI(TAG, "服务调用成功，响应: %s", response);
        free(response);
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "服务调用失败");
        return ESP_FAIL;
    }
}
