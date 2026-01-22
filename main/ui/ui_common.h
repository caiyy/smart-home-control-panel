#ifndef UI_COMMON_H
#define UI_COMMON_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

// 统一的图标类型枚举
typedef enum {
    ICON_MODE,
    ICON_WIFI,
    ICON_PREV,
    ICON_PAUSE,
    ICON_PLAY,
    ICON_NEXT,
    ICON_WEATHER,
    ICON_TEMP,
    ICON_HUM,
    ICON_LIGHT,
    ICON_MONITOR,
    ICON_SPEAKER,
    ICON_VOL_SMALL,
    ICON_BRIGHT_SMALL,
    ICON_ALBUM_COVER,
    ICON_BACK
} IconType;

/**
 * @brief 获取图标区域定义
 * @param type 图标类型
 * @param x 返回X坐标
 * @param y 返回Y坐标 (基础Y坐标，不含日夜模式偏移)
 * @param w 返回宽度
 * @param h 返回高度
 */
void ui_get_icon_area(IconType type, int *x, int *y, int *w, int *h);

/**
 * @brief 设置对象显示指定图标
 * @param obj LVGL图像对象
 * @param type 图标类型
 */
void ui_set_icon(lv_obj_t *obj, IconType type);

/**
 * @brief 创建通用图像按钮控件
 * @param parent 父对象
 * @param icon_type 图标类型
 * @param event_cb 事件回调函数
 * @return 创建的图像按钮对象
 */
lv_obj_t* ui_create_image_button(lv_obj_t *parent, IconType icon_type, lv_event_cb_t event_cb);

#ifdef __cplusplus
}
#endif

#endif // UI_COMMON_H
