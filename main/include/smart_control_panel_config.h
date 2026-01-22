#ifndef SMART_CONTROL_PANEL_CONFIG_H
#define SMART_CONTROL_PANEL_CONFIG_H

#include <stdint.h>
#include <stdbool.h>
#include "driver/ledc.h"

// 触摸数据结构体，用于在任务间共享
typedef struct {
    bool touched;
    uint16_t x;
    uint16_t y;
} touch_data_t;

// SPI引脚配置
#define SPI_PORT  SPI3_HOST  // SPI端口号
#define SPI_SDA   40         // SDA
#define SPI_SCL   39         // SCL
#define SPI_CS    38         // CS

// RGB屏幕引脚配置
#define ST7701S_LCD_BK_LIGHT_ON_LEVEL  1
#define ST7701S_LCD_BK_LIGHT_OFF_LEVEL !ST7701S_LCD_BK_LIGHT_ON_LEVEL
#define ST7701S_PIN_NUM_BK_LIGHT       41       // 背光

// 背光PWM配置
#define PWM_CHANNEL_LED LEDC_CHANNEL_0
#define PWM_TIMER_LED LEDC_TIMER_0
#define PWM_FREQ_LED 2000  // 2000Hz
#define PWM_RESOLUTION_LED LEDC_TIMER_8_BIT  // 8位分辨率 (0-255)
#define PWM_DUTY_DEFAULT 128  // 默认亮度50%
#define PWM_SPEED_MODE LEDC_LOW_SPEED_MODE
#define ST7701S_PIN_NUM_PCLK           45       // PCLK
#define ST7701S_PIN_NUM_DE             48       // DE
#define ST7701S_PIN_NUM_VSYNC          47       // VSYNC
#define ST7701S_PIN_NUM_HSYNC          21       // HSYNC
#define ST7701S_PIN_NUM_DATA0          14       // B0
#define ST7701S_PIN_NUM_DATA1          13        // B1
#define ST7701S_PIN_NUM_DATA2          12        // B2
#define ST7701S_PIN_NUM_DATA3          11        // B3
#define ST7701S_PIN_NUM_DATA4          10        // B4
#define ST7701S_PIN_NUM_DATA5          9        // G0
#define ST7701S_PIN_NUM_DATA6          46        // G1
#define ST7701S_PIN_NUM_DATA7          3        // G2
#define ST7701S_PIN_NUM_DATA8          20        // G3
#define ST7701S_PIN_NUM_DATA9          19        // G4
#define ST7701S_PIN_NUM_DATA10         8        // G5
#define ST7701S_PIN_NUM_DATA11         18       // R0
#define ST7701S_PIN_NUM_DATA12         17       // R1
#define ST7701S_PIN_NUM_DATA13         16       // R2
#define ST7701S_PIN_NUM_DATA14         15       // R3
#define ST7701S_PIN_NUM_DATA15         7       // R4
#define ST7701S_PIN_NUM_DISP_EN        -1

// 触摸屏引脚定义
#define TOUCH_SDA 5
#define TOUCH_SCL 4
#define TOUCH_INT 6
#define TOUCH_RST -1   // 复位引脚 (使用未被RGB面板占用的GPIO)

// 屏幕分辨率参数
#define ST7701S_LCD_H_RES              480      // 水平方向
#define ST7701S_LCD_V_RES              480      // 垂直方向

// RGB通信时序相关参数
#define ST7701S_LCD_PIXEL_CLOCK_HZ     (12 * 1000 * 1000)  // clk 12MHz
#define ST7701S_HSYNC_PULSE_WIDTH       8                  // hpw 8 
#define ST7701S_HSYNC_BACK_PORCH        50                  // hbp 50 
#define ST7701S_HSYNC_FRONT_PORCH       10                 // hfp 10 
#define ST7701S_VSYNC_PULSE_WIDTH       8                  // vpw 8 
#define ST7701S_VSYNC_BACK_PORCH        12                  // vbp 12 
#define ST7701S_VSYNC_FRONT_PORCH       15                 // vfp 15 

#endif // SMART_CONTROL_PANEL_CONFIG_H