#ifndef HTTP_SERVICE_H
#define HTTP_SERVICE_H

#include "esp_http_client.h"

/**
 * @brief HTTP 请求配置结构体
 */
typedef struct {
    const char *url;            // 请求 URL
    esp_http_client_method_t method; // HTTP 方法 (GET, POST 等)
    const char *post_data;      // POST 数据 (如果是 POST 请求)
    const char **headers;       // 请求头，格式为 [key1, val1, key2, val2, ..., NULL]
    int timeout_ms;             // 超时时间
} http_config_t;

/**
 * @brief 发送 HTTP 请求并获取响应内容
 * 
 * @param config 请求配置
 * @return char* 响应内容字符串（需手动 free），失败返回 NULL
 */
char* http_send_request(const http_config_t *config);

#endif // HTTP_SERVICE_H
