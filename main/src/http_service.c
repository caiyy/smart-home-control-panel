#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_http_client.h"
#include "http_service.h"

static const char *TAG = "http_service";

// 外部WiFi状态检查函数
extern bool is_wifi_connected(void);

char* http_send_request(const http_config_t *config) {
    if (config == NULL || config->url == NULL) {
        ESP_LOGE(TAG, "Invalid arguments");
        return NULL;
    }

    // 检查WiFi连接状态
    if (!is_wifi_connected()) {
        ESP_LOGW(TAG, "WiFi未连接,跳过HTTP请求: %s", config->url);
        return NULL;
    }

    esp_http_client_config_t client_config = {
        .url = config->url,
        .method = config->method,
        .timeout_ms = config->timeout_ms > 0 ? config->timeout_ms : 3000, // 减少默认超时时间从5秒到3秒
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
        .crt_bundle_attach = NULL, // 明确禁用证书包，防止 HTTPS 干扰
#endif
    };

    esp_http_client_handle_t client = esp_http_client_init(&client_config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        return NULL;
    }

    // 设置请求头
    if (config->headers != NULL) {
        for (int i = 0; config->headers[i] != NULL && config->headers[i+1] != NULL; i += 2) {
            esp_http_client_set_header(client, config->headers[i], config->headers[i+1]);
        }
    }

    // 设置 POST 数据
    if (config->method == HTTP_METHOD_POST && config->post_data != NULL) {
        esp_http_client_set_post_field(client, config->post_data, strlen(config->post_data));
    }

    char *response_buf = NULL;
    int total_read_len = 0;
    
    // 打开连接
    esp_err_t err = esp_http_client_open(client, config->method == HTTP_METHOD_POST ? strlen(config->post_data) : 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open connection: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return NULL;
    }

    // 如果是 POST 且有数据，写入数据
    if (config->method == HTTP_METHOD_POST && config->post_data != NULL) {
        int wlen = esp_http_client_write(client, config->post_data, strlen(config->post_data));
        if (wlen < 0) {
            ESP_LOGE(TAG, "Failed to write post data");
            esp_http_client_cleanup(client);
            return NULL;
        }
    }

    int content_length = esp_http_client_fetch_headers(client);
    int status_code = esp_http_client_get_status_code(client);
    ESP_LOGI(TAG, "HTTP Status = %d, content_length = %d", status_code, content_length);

    if (status_code >= 200 && status_code < 300) {
        // 动态分配缓冲区
        int alloc_size = (content_length > 0) ? (content_length + 1) : 4096; 
        response_buf = malloc(alloc_size);
        if (response_buf) {
            memset(response_buf, 0, alloc_size);
            while (1) {
                // 确保我们至少还有一点空间来读取和存放 null 终止符
                if (total_read_len >= alloc_size - 1) {
                    alloc_size += 4096;
                    char *new_buf = realloc(response_buf, alloc_size);
                    if (new_buf == NULL) {
                        ESP_LOGE(TAG, "Failed to realloc buffer");
                        break;
                    }
                    response_buf = new_buf;
                }

                int read_len = esp_http_client_read(client, response_buf + total_read_len, alloc_size - 1 - total_read_len);
                if (read_len < 0) {
                    ESP_LOGE(TAG, "Error reading from HTTP stream");
                    break;
                }
                if (read_len == 0) break;
                
                total_read_len += read_len;
            }
            response_buf[total_read_len] = '\0';
            ESP_LOGI(TAG, "Read successful, total length: %d", total_read_len);
        } else {
            ESP_LOGE(TAG, "Failed to allocate memory for response");
        }
    } else {
        ESP_LOGE(TAG, "HTTP request failed with status: %d", status_code);
    }

    esp_http_client_cleanup(client);
    return response_buf;
}
