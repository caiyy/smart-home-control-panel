/*
 * SPDX-FileCopyrightText: 2022-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#include "lvgl.h"

// 屏幕类型枚举
typedef enum {
    SCREEN_MAIN,
    SCREEN_MAX
} screen_type_t;

// UI管理器初始化
void ui_manager_init(void);

// 创建指定类型的屏幕
void ui_create_screen(screen_type_t screen_type);

#endif /* UI_MANAGER_H */
