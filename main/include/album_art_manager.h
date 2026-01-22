#ifndef ALBUM_ART_MANAGER_H
#define ALBUM_ART_MANAGER_H

#include <stdbool.h>

/**
 * @brief 请求专辑封面更新
 * 
 * @param raw_url 原始 URL (例如从 MQTT 接收到的音乐路径)
 * @param width 请求的宽度
 * @param height 请求的高度
 * @param is_background 是否为背景图 (如果是，则不下载 JPEG，而是请求 php 返回 LVGL 原始格式且不缓存预览图)
 */
void request_album_art_update(const char* raw_url, int width, int height, bool is_background);

#endif // ALBUM_ART_MANAGER_H
