/*
 * SPDX-FileCopyrightText: 2022-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#ifndef MAIN_SCREEN_H
#define MAIN_SCREEN_H

#include "lvgl.h"
#include "event_system.h"

// XML加载结果结构体
typedef struct {
    char *xml_data;
    bool success;
} xml_load_result_t;

// 时间相关的Subject和缓冲区
extern lv_subject_t date_subject;
extern char date_buf[64];
extern char prev_date_buf[64];

extern lv_subject_t time_subject;
extern char time_buf[32];
extern char prev_time_buf[32];

extern lv_subject_t weekday_subject;
extern char weekday_buf[32];
extern char prev_weekday_buf[32];

// 歌曲相关的Subject
extern lv_subject_t song_lyrics_subject;
extern lv_subject_t song_name_subject;
extern lv_subject_t song_artist_subject;
extern lv_subject_t song_time_subject;
extern lv_subject_t play_progress_subject;

// 控制相关的Subject
extern lv_subject_t brightness_subject_value;
extern lv_subject_t volume_subject_value;

// 能耗相关的Subject
extern lv_subject_t power_subject;
extern lv_subject_t daily_energy_subject;
extern lv_subject_t monthly_energy_subject;

// 刷新XML回调函数
extern void refresh_xml_cb(lv_event_t *e);

// 更新功耗显示
extern void update_power_display(float power);

// 更新能耗显示
extern void update_energy_display(float daily_energy, float monthly_energy);

// 初始化主屏幕UI
extern void init_main_screen(void);

// 初始化主屏幕Subject
extern void init_main_screen_subjects(void);

// XML加载完成回调函数
extern void on_xml_loaded(xml_load_result_t *result);

#endif /* MAIN_SCREEN_H */
