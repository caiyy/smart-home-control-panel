/*
 * SPDX-FileCopyrightText: 2022-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include "ui_manager.h"
#include "../screens/main/main_screen.h"
#include "esp_log.h"

static const char *TAG = "ui_manager";

// 当前屏幕类型
static screen_type_t g_current_screen = SCREEN_MAX;

// UI管理器初始化
void ui_manager_init(void)
{
    ESP_LOGI(TAG, "初始化UI管理器");
    // 这里可以添加UI管理器的初始化代码
}

// 创建指定类型的屏幕
void ui_create_screen(screen_type_t screen_type)
{
    if (screen_type >= SCREEN_MAX) {
        ESP_LOGE(TAG, "无效的屏幕类型: %d", screen_type);
        return;
    }
    
    ESP_LOGI(TAG, "创建屏幕: %d", screen_type);
    
    // 清除当前屏幕上的所有对象
    lv_obj_clean(lv_screen_active());
    
    // 根据屏幕类型调用对应的创建函数
    switch (screen_type) {
        case SCREEN_MAIN:
            init_main_screen();
            break;
        default:
            ESP_LOGE(TAG, "未实现的屏幕类型: %d", screen_type);
            break;
    }
    
    // 更新当前屏幕类型
    g_current_screen = screen_type;
}


