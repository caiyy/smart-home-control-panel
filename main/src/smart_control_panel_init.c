#include <stdio.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_heap_caps.h"

// 重定义默认字体为ht16，必须在包含lvgl.h之前定义
#undef LV_FONT_DEFAULT
#define LV_FONT_DEFAULT &ht16

#include "lvgl.h"

// 声明ht16字体外部变量
extern const lv_font_t ht16;
// 声明time_100字体外部变量
extern const lv_font_t time_100;
#include "st7701s.h"
#include <Vernon_GT911.h>
#include "nvs_flash.h"
#include "smart_control_panel_config.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_timer.h"

// 外部变量声明
extern esp_lcd_panel_handle_t panel_handle;
extern Vernon_GT911 gt911;
extern lv_disp_t *lv_disp;
extern lv_indev_t *lv_indev;
extern touch_data_t g_touch_data;
extern SemaphoreHandle_t g_touch_mutex;
extern const char *TAG;



// 函数声明
uint8_t read_brightness_from_nvs(void);
void save_brightness_to_nvs(uint8_t brightness);
void set_backlight_brightness(uint8_t brightness);

// 触摸屏初始化
void touch_init(void)
{
    ESP_LOGI(TAG, "初始化GT911触摸屏");
    // 初始化GT911触摸屏，使用I2C_NUM_0，地址0x5D，分辨率480x480
    GT911_init(&gt911, TOUCH_SDA, TOUCH_SCL, TOUCH_INT, TOUCH_RST, I2C_NUM_0, GT911_ADDR1, ST7701S_LCD_H_RES, ST7701S_LCD_V_RES);
    // 设置触摸屏方向为正常方向
    GT911_setRotation(&gt911, ROTATION_NORMAL);
    ESP_LOGI(TAG, "GT911触摸屏初始化完成");
}

// 背光PWM初始化函数
void backlight_pwm_init(void)
{
    ESP_LOGI(TAG, "初始化背光PWM");
    
    // 从NVS读取亮度值
    uint8_t brightness = read_brightness_from_nvs();
    
    // 配置PWM定时器
    ledc_timer_config_t timer_config = {
        .duty_resolution = PWM_RESOLUTION_LED,
        .freq_hz = PWM_FREQ_LED,
        .speed_mode = PWM_SPEED_MODE,
        .timer_num = PWM_TIMER_LED,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer_config));
    
    // 配置PWM通道
    ledc_channel_config_t channel_config = {
        .gpio_num = ST7701S_PIN_NUM_BK_LIGHT,
        .speed_mode = PWM_SPEED_MODE,
        .channel = PWM_CHANNEL_LED,
        .timer_sel = PWM_TIMER_LED,
        .duty = brightness,
        .hpoint = 0,
        .flags.output_invert = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&channel_config));
    
    ESP_LOGI(TAG, "背光PWM初始化完成，占空比：%d", brightness);
}

// 从NVS读取亮度值
uint8_t read_brightness_from_nvs(void)
{
    uint8_t brightness = PWM_DUTY_DEFAULT;
    nvs_handle_t nvs_handle;
    
    // 打开NVS命名空间
    esp_err_t err = nvs_open("smart_panel", NVS_READWRITE, &nvs_handle);
    if (err == ESP_OK) {
        // 读取亮度值
        err = nvs_get_u8(nvs_handle, "brightness", &brightness);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "从NVS读取亮度值失败，使用默认值：%d", brightness);
        } else {
            ESP_LOGI(TAG, "从NVS读取亮度值：%d", brightness);
        }
        // 关闭NVS
        nvs_close(nvs_handle);
    } else {
        ESP_LOGE(TAG, "打开NVS失败：%s", esp_err_to_name(err));
    }
    
    // 确保亮度值在70-255范围内
    if (brightness < 50) {
        brightness = 50;
    }
    
    return brightness;
}

// 将亮度值保存到NVS
void save_brightness_to_nvs(uint8_t brightness)
{
    nvs_handle_t nvs_handle;
    
    // 打开NVS命名空间
    esp_err_t err = nvs_open("smart_panel", NVS_READWRITE, &nvs_handle);
    if (err == ESP_OK) {
        // 保存亮度值
        err = nvs_set_u8(nvs_handle, "brightness", brightness);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "保存亮度值到NVS失败：%s", esp_err_to_name(err));
        } else {
            // 提交写入
            err = nvs_commit(nvs_handle);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "提交亮度值到NVS失败：%s", esp_err_to_name(err));
            } else {
                ESP_LOGI(TAG, "保存亮度值到NVS成功：%d", brightness);
            }
        }
        // 关闭NVS
        nvs_close(nvs_handle);
    } else {
        ESP_LOGE(TAG, "打开NVS失败：%s", esp_err_to_name(err));
    }
}

// 设置背光亮度
void set_backlight_brightness(uint8_t brightness)
{
    // 确保亮度值在60-255范围内
    if (brightness < 65) {
        brightness = 65;
    }
    
    // 设置PWM占空比
    ledc_set_duty(PWM_SPEED_MODE, PWM_CHANNEL_LED, brightness);
    ledc_update_duty(PWM_SPEED_MODE, PWM_CHANNEL_LED);
    
    // 保存亮度值到NVS
    save_brightness_to_nvs(brightness);
    
    ESP_LOGI(TAG, "设置背光亮度：%d", brightness);
}

// LCD初始化
void lcd_init(void)
{
    vernon_st7701s_handle vernon_st7701s = st7701s_new_object(SPI_SDA, SPI_SCL, SPI_CS, SPI_PORT, SPI_METHOD);
    st7701s_screen_init(vernon_st7701s, 9);

    ESP_LOGI(TAG, "安装RGB LCD面板驱动");
    esp_lcd_rgb_panel_config_t panel_config = {
        .data_width = 16, // RGB565 in parallel mode, thus 16bit in width
        .psram_trans_align = 64,
        .num_fbs = 1,
        .clk_src = LCD_CLK_SRC_PLL240M,
        .disp_gpio_num = ST7701S_PIN_NUM_DISP_EN,
        .pclk_gpio_num = ST7701S_PIN_NUM_PCLK,
        .vsync_gpio_num = ST7701S_PIN_NUM_VSYNC,
        .hsync_gpio_num = ST7701S_PIN_NUM_HSYNC,
        .de_gpio_num = ST7701S_PIN_NUM_DE,
        .data_gpio_nums = {
            ST7701S_PIN_NUM_DATA0,
            ST7701S_PIN_NUM_DATA1,
            ST7701S_PIN_NUM_DATA2,
            ST7701S_PIN_NUM_DATA3,
            ST7701S_PIN_NUM_DATA4,
            ST7701S_PIN_NUM_DATA5,
            ST7701S_PIN_NUM_DATA6,
            ST7701S_PIN_NUM_DATA7,
            ST7701S_PIN_NUM_DATA8,
            ST7701S_PIN_NUM_DATA9,
            ST7701S_PIN_NUM_DATA10,
            ST7701S_PIN_NUM_DATA11,
            ST7701S_PIN_NUM_DATA12,
            ST7701S_PIN_NUM_DATA13,
            ST7701S_PIN_NUM_DATA14,
            ST7701S_PIN_NUM_DATA15,
        },
        .timings = {
            .pclk_hz = ST7701S_LCD_PIXEL_CLOCK_HZ,
            .h_res = ST7701S_LCD_H_RES,
            .v_res = ST7701S_LCD_V_RES,
            .hsync_pulse_width = ST7701S_HSYNC_PULSE_WIDTH,    
            .hsync_back_porch = ST7701S_HSYNC_BACK_PORCH,      
            .hsync_front_porch = ST7701S_HSYNC_FRONT_PORCH,    
            .vsync_pulse_width = ST7701S_VSYNC_PULSE_WIDTH,    
            .vsync_back_porch = ST7701S_VSYNC_BACK_PORCH,      
            .vsync_front_porch = ST7701S_VSYNC_FRONT_PORCH,    
            .flags = 
            {
                .hsync_idle_low = 0,    // HSYNC 信号空闲时的电平，0：高电平，1：低电平
                .vsync_idle_low = 0,    // VSYNC 信号空闲时的电平，0 表示高电平，1：低电平
                .de_idle_high = 0,      // DE 信号空闲时的电平，0：高电平，1：低电平
                .pclk_active_neg = 0,   // 时钟信号的有效边沿，0：上升沿有效，1：下降沿有效
                .pclk_idle_high = 0,    // PCLK 信号空闲时的电平，0：高电平，1：低电平
            },
        },
        .flags.fb_in_psram = true, // allocate frame buffer in PSRAM
    };
    ESP_ERROR_CHECK(esp_lcd_new_rgb_panel(&panel_config, &panel_handle));

    ESP_LOGI(TAG, "初始化RGB LCD面板");
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    
    // 初始化背光PWM
    backlight_pwm_init();
}

// LVGL刷新回调函数
static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    if (panel_handle) {
        // 将LVGL渲染的图像刷新到LCD屏幕
        esp_lcd_panel_draw_bitmap(panel_handle, area->x1, area->y1, 
                                 area->x2 + 1, area->y2 + 1, px_map);
    }
    
    // 通知LVGL刷新完成
    lv_display_flush_ready(disp);
}

// LVGL输入设备读取回调函数
static void lvgl_indev_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    bool is_touched = false;
    uint16_t x = 0;
    uint16_t y = 0;
    
    // 从共享数据中读取触摸状态
    if (g_touch_mutex) {
        if (xSemaphoreTake(g_touch_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            is_touched = g_touch_data.touched;
            x = g_touch_data.x;
            y = g_touch_data.y;
            xSemaphoreGive(g_touch_mutex);
        }
    }
    
    data->state = is_touched ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    data->point.x = x;
    data->point.y = y;
}

// LVGL定时器回调函数
static void lvgl_tick_cb(void *arg)
{
    lv_tick_inc(1); // 1ms ticks
}

// 主题回调函数已移至主题管理器

// LVGL初始化
void lvgl_init(void)
{
    ESP_LOGI(TAG, "初始化LVGL库");
    
    // 初始化LVGL
    lv_init();
    
    // 主题初始化将由主题管理器处理
    
    // 注册ht16字体到XML系统
    lv_xml_register_font(NULL, "ht16", &ht16);
    ESP_LOGI(TAG, "成功注册ht16字体到XML系统");
    
    // 注册time_100字体到XML系统
    lv_xml_register_font(NULL, "time_100", &time_100);
    ESP_LOGI(TAG, "成功注册time_100字体到XML系统");
    
    // 创建LVGL显示设备
    lv_disp = lv_display_create(ST7701S_LCD_H_RES, ST7701S_LCD_V_RES);
    if (!lv_disp) {
        ESP_LOGE(TAG, "创建LVGL显示设备失败");
        return;
    }
    
    // 设置LVGL显示的刷新回调
    lv_display_set_flush_cb(lv_disp, lvgl_flush_cb);
    
    // 分配LVGL绘制缓冲区
    size_t draw_buf_size = ST7701S_LCD_H_RES * 100 * 2; // 100行缓冲区
    void *draw_buf = heap_caps_malloc(draw_buf_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!draw_buf) {
        ESP_LOGE(TAG, "分配LVGL绘制缓冲区失败");
        return;
    }
    
    // 设置LVGL绘制缓冲区
    lv_display_set_buffers(lv_disp, draw_buf, NULL, draw_buf_size, LV_DISPLAY_RENDER_MODE_PARTIAL);
    
    // 创建LVGL输入设备（触摸屏）
    lv_indev = lv_indev_create();
    if (lv_indev) {
        lv_indev_set_type(lv_indev, LV_INDEV_TYPE_POINTER);
        lv_indev_set_read_cb(lv_indev, lvgl_indev_read_cb);
        lv_indev_set_display(lv_indev, lv_disp);
        ESP_LOGI(TAG, "创建LVGL触摸输入设备成功");
    }
    
    // 创建ESP定时器，用于LVGL的tick
    esp_timer_handle_t lvgl_tick_timer = NULL;
    esp_timer_create_args_t tick_timer_args = {
        .callback = &lvgl_tick_cb,
        .name = "lvgl_tick"
    };
    ESP_ERROR_CHECK(esp_timer_create(&tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, 1000)); // 1ms ticks
    
    ESP_LOGI(TAG, "LVGL初始化成功");
}

// 触摸检测任务
void touch_task(void *pvParameters)
{
    uint16_t x = 0;
    uint16_t y = 0;
    
    while (1) {
        // 检测触摸
        bool touched = GT911_touched(&gt911);
        
        if (touched) {
            // 获取第一个触摸点坐标
            GT911_read_pos(&gt911, &x, &y, 0);
        }
        
        // 保护共享数据访问
        if (g_touch_mutex) {
            if (xSemaphoreTake(g_touch_mutex, portMAX_DELAY) == pdTRUE) {
                g_touch_data.touched = touched;
                if (touched) {
                    g_touch_data.x = x;
                    g_touch_data.y = y;
                    ESP_LOGI(TAG, "检测到触摸，坐标：x=%d, y=%d", x, y);
                }
                xSemaphoreGive(g_touch_mutex);
            }
        }
        
        // 每100ms检测一次触摸
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

