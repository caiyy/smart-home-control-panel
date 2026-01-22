#include "ui_common.h"
#include "lvgl.h"
#include "esp_log.h"

// 声明外部资源
#include "../images/uiIcons.c"

/**
 * @brief 获取图标区域定义
 * @param type 图标类型
 * @param x 返回X坐标
 * @param y 返回Y坐标 (基础Y坐标，不含日夜模式偏移)
 * @param w 返回宽度
 * @param h 返回高度
 */
void ui_get_icon_area(IconType type, int *x, int *y, int *w, int *h) {
    switch (type) {
        case ICON_MODE:    *x = 0;   *y = 0;  *w = 40; *h = 40; break;
        case ICON_WIFI:    *x = 40;  *y = 0;  *w = 40; *h = 31; break;
        case ICON_PREV:    *x = 80;  *y = 0;  *w = 30; *h = 30; break;
        case ICON_PAUSE:   *x = 110; *y = 0;  *w = 30; *h = 30; break;
        case ICON_PLAY:    *x = 140; *y = 0;  *w = 30; *h = 30; break;
        case ICON_NEXT:    *x = 170; *y = 0;  *w = 30; *h = 30; break;
        case ICON_WEATHER: *x = 0;   *y = 40; *w = 16; *h = 16; break;
        case ICON_TEMP:    *x = 16;  *y = 40; *w = 16; *h = 16; break;
        case ICON_HUM:     *x = 32;  *y = 40; *w = 16; *h = 16; break;
        case ICON_LIGHT:   *x = 0;   *y = 56; *w = 65; *h = 65; break;
        case ICON_MONITOR: *x = 65;  *y = 56; *w = 65; *h = 65; break;
        case ICON_SPEAKER: *x = 201; *y = 0;  *w = 65; *h = 65; break;
        case ICON_VOL_SMALL:    *x = 160; *y = 30; *w = 20; *h = 20; break;
        case ICON_BRIGHT_SMALL: *x = 180; *y = 30; *w = 20; *h = 20; break;
        case ICON_ALBUM_COVER:  *x = 0;   *y = 242; *w = 100; *h = 100; break;
        case ICON_BACK:    *x = 130; *y = 59; *w = 40; *h = 40; break;
        default:           *x = 0;   *y = 0;  *w = 0;  *h = 0;  break;
    }
}

/**
 * @brief 设置对象显示指定图标
 * @param obj LVGL图像对象
 * @param type 图标类型
 */
void ui_set_icon(lv_obj_t *obj, IconType type) {
    if (obj == NULL) return;
    
    int x, y, w, h;
    ui_get_icon_area(type, &x, &y, &w, &h);
    
    // 设置图片源
    lv_image_set_src(obj, &uiIcons);
    
    // 关键：设置图片内部对齐为左上角，否则offset是相对于中心的
    lv_image_set_inner_align(obj, LV_IMAGE_ALIGN_TOP_LEFT);
    
    lv_image_set_offset_x(obj, -x);
    lv_image_set_offset_y(obj, -y);
    
    // 设置对象大小以匹配图标大小
    lv_obj_set_size(obj, w, h);
}

/**
 * @brief 创建通用图像按钮控件
 * @param parent 父对象
 * @param icon_type 图标类型
 * @param event_cb 事件回调函数
 * @return 创建的图像按钮对象
 */
lv_obj_t* ui_create_image_button(lv_obj_t *parent, IconType icon_type, lv_event_cb_t event_cb) {
    if (parent == NULL) return NULL;
    
    lv_obj_t *btn = lv_image_create(parent);
    if (btn == NULL) return NULL;
    
    // 设置图标
    ui_set_icon(btn, icon_type);
    
    // 设置默认样式
    lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
    
    // 添加触摸反馈：按下时改变透明度
    lv_obj_set_style_opa(btn, LV_OPA_70, LV_STATE_PRESSED);
    
    // 添加事件回调
    if (event_cb != NULL) {
        lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(btn, event_cb, LV_EVENT_CLICKED, NULL);
    }
    
    return btn;
}
