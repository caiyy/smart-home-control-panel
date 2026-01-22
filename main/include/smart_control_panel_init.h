#ifndef SMART_CONTROL_PANEL_INIT_H
#define SMART_CONTROL_PANEL_INIT_H

#include "lvgl.h"

// 外部变量声明
extern lv_subject_t brightness_subject;

// 触摸屏初始化
void touch_init(void);

// 背光PWM初始化函数
void backlight_pwm_init(void);

// 从NVS读取亮度值
uint8_t read_brightness_from_nvs(void);

// 将亮度值保存到NVS
void save_brightness_to_nvs(uint8_t brightness);

// 设置背光亮度
void set_backlight_brightness(uint8_t brightness);

// LCD初始化
void lcd_init(void);

// LVGL初始化
void lvgl_init(void);

// 触摸检测任务
void touch_task(void *pvParameters);



#endif // SMART_CONTROL_PANEL_INIT_H