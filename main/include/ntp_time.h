#ifndef NTP_TIME_H
#define NTP_TIME_H

#include "esp_err.h"
#include "esp_sntp.h"
#include "time.h"

// NTP时间配置结构体
typedef struct {
    char *ntp_server;
    int32_t timezone;
    bool enable_daylight_saving;
} ntp_time_config_t;

/**
 * @brief 初始化NTP时间模块
 * @param config NTP时间配置
 * @return esp_err_t 错误码
 */
esp_err_t ntp_time_init(ntp_time_config_t *config);

/**
 * @brief 启动NTP时间同步
 * @return esp_err_t 错误码
 */
esp_err_t ntp_time_start(void);

/**
 * @brief 停止NTP时间同步
 * @return esp_err_t 错误码
 */
esp_err_t ntp_time_stop(void);

/**
 * @brief 获取当前时间
 * @param now 当前时间指针
 * @return esp_err_t 错误码
 */
esp_err_t ntp_time_get_current_time(struct tm *now);

/**
 * @brief 将时间格式化为字符串
 * @param now 时间结构体
 * @param buf 输出缓冲区
 * @param buf_len 缓冲区长度
 * @return esp_err_t 错误码
 */
esp_err_t ntp_time_format_time(struct tm *now, char *buf, size_t buf_len);

/**
 * @brief 时间同步回调函数
 * @param tv 时间值
 */
void ntp_time_sync_cb(struct timeval *tv);

#endif // NTP_TIME_H
