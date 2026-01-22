#include "ntp_time.h"
#include "event_system.h"
#include "esp_log.h"
#include "esp_sntp.h"
#include "string.h"
#include <inttypes.h>

static const char *TAG = "ntp_time";

static ntp_time_config_t s_ntp_config;
static bool s_ntp_initialized = false;
static bool s_ntp_running = false;

/**
 * @brief 初始化NTP时间模块
 * @param config NTP时间配置
 * @return esp_err_t 错误码
 */
esp_err_t ntp_time_init(ntp_time_config_t *config)
{
    if (s_ntp_initialized) {
        ESP_LOGI(TAG, "NTP时间模块已经初始化");
        return ESP_OK;
    }

    if (config == NULL) {
        ESP_LOGE(TAG, "NTP配置参数不能为空");
        return ESP_ERR_INVALID_ARG;
    }

    // 复制配置参数
    s_ntp_config.ntp_server = strdup(config->ntp_server);
    if (s_ntp_config.ntp_server == NULL) {
        ESP_LOGE(TAG, "内存分配失败");
        return ESP_ERR_NO_MEM;
    }

    s_ntp_config.timezone = config->timezone;
    s_ntp_config.enable_daylight_saving = config->enable_daylight_saving;

    s_ntp_initialized = true;
    ESP_LOGI(TAG, "NTP时间模块初始化成功");
    return ESP_OK;
}

/**
 * @brief 时间同步回调函数
 * @param tv 时间值
 */
void ntp_time_sync_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "NTP时间同步成功");
    
    // 获取当前时间
    struct tm now;
    localtime_r(&tv->tv_sec, &now);
    
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &now);
    ESP_LOGI(TAG, "当前时间: %s", time_str);
    
    // 发送时间更新事件
    event_system_post(EVENT_TYPE_NTP_TIME_UPDATED, NULL, 0);
}

/**
 * @brief 启动NTP时间同步
 * @return esp_err_t 错误码
 */
esp_err_t ntp_time_start(void)
{
    if (!s_ntp_initialized) {
        ESP_LOGE(TAG, "NTP时间模块尚未初始化");
        return ESP_ERR_INVALID_STATE;
    }

    if (s_ntp_running) {
        ESP_LOGI(TAG, "NTP时间同步已经在运行中");
        return ESP_OK;
    }

    // 设置时区 - 中国东八区（UTC+8）
    char timezone_str[64];
    // 中国时区格式：CST-8:00 表示 UTC+8，因为CST是Central Standard Time（中部标准时间），需要减去8小时才是UTC时间
    // 正确的中国时区设置应该是 CST-8:00，这表示本地时间比UTC时间早8小时
    sprintf(timezone_str, "CST-8:00");
    setenv("TZ", timezone_str, 1);
    tzset();
    
    ESP_LOGI(TAG, "设置时区: %s", timezone_str);

    // 初始化SNTP
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, s_ntp_config.ntp_server);
    esp_sntp_set_time_sync_notification_cb(ntp_time_sync_cb);
    esp_sntp_init();
    
    s_ntp_running = true;
    ESP_LOGI(TAG, "NTP时间同步启动成功，服务器: %s", s_ntp_config.ntp_server);
    return ESP_OK;
}

/**
 * @brief 停止NTP时间同步
 * @return esp_err_t 错误码
 */
esp_err_t ntp_time_stop(void)
{
    if (!s_ntp_initialized) {
        ESP_LOGE(TAG, "NTP时间模块尚未初始化");
        return ESP_ERR_INVALID_STATE;
    }

    if (!s_ntp_running) {
        ESP_LOGI(TAG, "NTP时间同步已经停止");
        return ESP_OK;
    }

    esp_sntp_stop();
    s_ntp_running = false;
    ESP_LOGI(TAG, "NTP时间同步停止成功");
    return ESP_OK;
}

/**
 * @brief 获取当前时间
 * @param now 当前时间指针
 * @return esp_err_t 错误码
 */
esp_err_t ntp_time_get_current_time(struct tm *now)
{
    if (now == NULL) {
        ESP_LOGE(TAG, "时间结构体指针不能为空");
        return ESP_ERR_INVALID_ARG;
    }

    time_t now_sec;
    time(&now_sec);
    localtime_r(&now_sec, now);
    
    return ESP_OK;
}

/**
 * @brief 将时间格式化为字符串
 * @param now 时间结构体
 * @param buf 输出缓冲区
 * @param buf_len 缓冲区长度
 * @return esp_err_t 错误码
 */
esp_err_t ntp_time_format_time(struct tm *now, char *buf, size_t buf_len)
{
    if (now == NULL || buf == NULL) {
        ESP_LOGE(TAG, "参数不能为空");
        return ESP_ERR_INVALID_ARG;
    }

    strftime(buf, buf_len, "%Y-%m-%d %H:%M:%S", now);
    return ESP_OK;
}
