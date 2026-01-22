#ifndef HOMEASSISTANT_H
#define HOMEASSISTANT_H
#include "esp_err.h"

/**
 * @brief 获取每日能耗数据
 * @return 返回每日能耗的字符串表示，需要调用者手动释放内存
 */
char *get_daily_energy(void);

/**
 * @brief 获取每月能耗数据
 * @return 返回每月能耗的字符串表示，需要调用者手动释放内存
 */
char *get_monthly_energy(void);

/**
 * @brief 获取单个实体的状态
 * @param entity_id 实体的ID
 * @return 返回状态字符串，需要调用者释放内存
 */
char *get_entity_state(const char *entity_id);

/**
 * @brief 调用 HomeAssistant 服务
 * @param domain 服务领域 (如 "switch")
 * @param service 服务名称 (如 "turn_on")
 * @param entity_id 实体ID
 * @return esp_err_t 返回执行结果
 */
esp_err_t call_ha_service(const char *domain, const char *service, const char *entity_id);

#endif // HOMEASSISTANT_H
