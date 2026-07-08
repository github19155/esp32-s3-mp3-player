#include "page_mp3.h"
#include "page_manager.h"
#include "player_core.h"
#include "sd_manager.h"
#include "app_event.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "page_mp3";

LV_FONT_DECLARE(font_alipuhui20);

/* ========== 页面级变量 ========== */
static lv_obj_t *s_music_list = NULL;
static lv_obj_t *s_label_play_pause = NULL;
static lv_obj_t *s_btn_play_pause = NULL;
static lv_obj_t *s_volume_slider = NULL;
static int       s_file_count = 0;

/* ========== 样式 ========== */
typedef struct {
    lv_style_t style_bg;
    lv_style_t style_focus_no_outline;
} button_style_t;

static button_style_t g_styles;

static void btn_style_init(void)
{
    lv_style_init(&g_styles.style_focus_no_outline);
    lv_style_set_outline_width(&g_styles.style_focus_no_outline, 0);

    lv_style_init(&g_styles.style_bg);
    lv_style_set_bg_opa(&g_styles.style_bg, LV_OPA_100);
    lv_style_set_bg_color(&g_styles.style_bg, lv_color_make(255, 255, 255));
    lv_style_set_shadow_width(&g_styles.style_bg, 0);
}

/* ========== 播放器回调（状态变化时更新 UI） ========== */
static void on_player_state_changed(player_state_t state, int index)
{
    lvgl_port_lock(0);
    if (s_label_play_pause) {
        if (state == PLAYER_STATE_PLAYING) {
            lv_label_set_text_static(s_label_play_pause, LV_SYMBOL_PAUSE);
        } else {
            lv_label_set_text_static(s_label_play_pause, LV_SYMBOL_PLAY);
        }
    }
    if (s_music_list && index >= 0 && index < s_file_count) {
        lv_dropdown_set_selected(s_music_list, index);
    }
    lvgl_port_unlock();
}

/* ========== 按钮事件处理 ========== */

static void btn_play_pause_cb(lv_event_t *event)
{
    lv_obj_t *btn = lv_event_get_target(event);
    lv_obj_t *lab = (lv_obj_t *)btn->user_data;

    player_state_t state = player_core_get_state();

    if (state == PLAYER_STATE_IDLE) {
        lvgl_port_lock(0);
        lv_label_set_text_static(lab, LV_SYMBOL_PAUSE);
        lvgl_port_unlock();
        int index = lv_dropdown_get_selected(s_music_list);
        player_core_play(index);
    } else if (state == PLAYER_STATE_PAUSED) {
        lvgl_port_lock(0);
        lv_label_set_text_static(lab, LV_SYMBOL_PAUSE);
        lvgl_port_unlock();
        player_core_resume();
    } else if (state == PLAYER_STATE_PLAYING) {
        lvgl_port_lock(0);
        lv_label_set_text_static(lab, LV_SYMBOL_PLAY);
        lvgl_port_unlock();
        player_core_pause();
    }
}

static void btn_prev_next_cb(lv_event_t *event)
{
    bool is_next = (bool)event->user_data;

    if (is_next) {
        ESP_LOGI(TAG, "Next track");
        player_core_next();
    } else {
        ESP_LOGI(TAG, "Prev track");
        player_core_prev();
    }

    int index = player_core_get_current_index();
    lvgl_port_lock(0);
    lv_dropdown_set_selected(s_music_list, index);
    lvgl_port_unlock();
}

static void volume_slider_cb(lv_event_t *event)
{
    lv_obj_t *slider = lv_event_get_target(event);
    int vol = lv_slider_get_value(slider);
    player_core_set_volume(vol);
    ESP_LOGI(TAG, "Volume: %d", vol);
}

static void music_list_cb(lv_event_t *event)
{
    uint16_t index = lv_dropdown_get_selected(s_music_list);
    ESP_LOGI(TAG, "Select track %d", index);

    player_state_t state = player_core_get_state();
    if (state == PLAYER_STATE_PAUSED) {
        player_core_play(index);
        player_core_pause();
    } else if (state == PLAYER_STATE_PLAYING) {
        player_core_play(index);
    }
}

/* ========== 构建文件列表 ========== */
static void build_file_list(lv_obj_t *dropdown)
{
    lvgl_port_lock(0);
    lv_dropdown_clear_options(dropdown);
    lvgl_port_unlock();

    for (int i = 0; i < s_file_count; i++) {
        const char *name = sd_manager_get_name(i);
        if (name) {
            lvgl_port_lock(0);
            lv_dropdown_add_option(dropdown, name, i);
            lvgl_port_unlock();
        }
    }

    lvgl_port_lock(0);
    if (s_file_count > 0) {
        lv_dropdown_set_selected(dropdown, 0);
    } else {
        lv_dropdown_set_options_static(dropdown, "无音乐文件");
    }
    lvgl_port_unlock();
}

/* ========== 页面生命周期 ========== */

static void page_mp3_on_enter(void)
{
    lv_obj_t *container = page_manager_get_container();
    if (container == NULL) return;

    lvgl_port_lock(0);

    btn_style_init();

    /* --- 播放/暂停按钮 --- */
    s_btn_play_pause = lv_btn_create(container);
    lv_obj_align(s_btn_play_pause, LV_ALIGN_CENTER, 0, 40);
    lv_obj_set_size(s_btn_play_pause, 50, 50);
    lv_obj_set_style_radius(s_btn_play_pause, 25, LV_STATE_DEFAULT);
    lv_obj_add_flag(s_btn_play_pause, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_add_style(s_btn_play_pause, &g_styles.style_focus_no_outline, LV_STATE_FOCUS_KEY);
    lv_obj_add_style(s_btn_play_pause, &g_styles.style_focus_no_outline, LV_STATE_FOCUSED);

    s_label_play_pause = lv_label_create(s_btn_play_pause);
    lv_label_set_text_static(s_label_play_pause, LV_SYMBOL_PLAY);
    lv_obj_center(s_label_play_pause);
    lv_obj_set_user_data(s_btn_play_pause, (void*)s_label_play_pause);
    lv_obj_add_event_cb(s_btn_play_pause, btn_play_pause_cb, LV_EVENT_VALUE_CHANGED, NULL);

    /* --- 上一首按钮 --- */
    lv_obj_t *btn_prev = lv_btn_create(container);
    lv_obj_set_size(btn_prev, 50, 50);
    lv_obj_set_style_radius(btn_prev, 25, LV_STATE_DEFAULT);
    lv_obj_clear_flag(btn_prev, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_align_to(btn_prev, s_btn_play_pause, LV_ALIGN_OUT_LEFT_MID, -40, 0);
    lv_obj_add_style(btn_prev, &g_styles.style_focus_no_outline, LV_STATE_FOCUS_KEY);
    lv_obj_add_style(btn_prev, &g_styles.style_focus_no_outline, LV_STATE_FOCUSED);
    lv_obj_add_style(btn_prev, &g_styles.style_bg, LV_STATE_FOCUS_KEY);
    lv_obj_add_style(btn_prev, &g_styles.style_bg, LV_STATE_FOCUSED);
    lv_obj_add_style(btn_prev, &g_styles.style_bg, LV_STATE_DEFAULT);

    lv_obj_t *lab_prev = lv_label_create(btn_prev);
    lv_label_set_text_static(lab_prev, LV_SYMBOL_PREV);
    lv_obj_set_style_text_font(lab_prev, &lv_font_montserrat_24, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(lab_prev, lv_color_make(0, 0, 0), LV_STATE_DEFAULT);
    lv_obj_center(lab_prev);
    lv_obj_add_event_cb(btn_prev, btn_prev_next_cb, LV_EVENT_CLICKED, (void*)false);

    /* --- 下一首按钮 --- */
    lv_obj_t *btn_next = lv_btn_create(container);
    lv_obj_set_size(btn_next, 50, 50);
    lv_obj_set_style_radius(btn_next, 25, LV_STATE_DEFAULT);
    lv_obj_clear_flag(btn_next, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_align_to(btn_next, s_btn_play_pause, LV_ALIGN_OUT_RIGHT_MID, 40, 0);
    lv_obj_add_style(btn_next, &g_styles.style_focus_no_outline, LV_STATE_FOCUS_KEY);
    lv_obj_add_style(btn_next, &g_styles.style_focus_no_outline, LV_STATE_FOCUSED);
    lv_obj_add_style(btn_next, &g_styles.style_bg, LV_STATE_FOCUS_KEY);
    lv_obj_add_style(btn_next, &g_styles.style_bg, LV_STATE_FOCUSED);
    lv_obj_add_style(btn_next, &g_styles.style_bg, LV_STATE_DEFAULT);

    lv_obj_t *lab_next = lv_label_create(btn_next);
    lv_label_set_text_static(lab_next, LV_SYMBOL_NEXT);
    lv_obj_set_style_text_font(lab_next, &lv_font_montserrat_24, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(lab_next, lv_color_make(0, 0, 0), LV_STATE_DEFAULT);
    lv_obj_center(lab_next);
    lv_obj_add_event_cb(btn_next, btn_prev_next_cb, LV_EVENT_CLICKED, (void*)true);

    /* --- 音量调节滑动条 --- */
    s_volume_slider = lv_slider_create(container);
    lv_obj_set_size(s_volume_slider, 200, 10);
    lv_obj_set_ext_click_area(s_volume_slider, 15);
    lv_obj_align(s_volume_slider, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_slider_set_range(s_volume_slider, 0, 100);
    lv_slider_set_value(s_volume_slider, player_core_get_volume(), LV_ANIM_ON);
    lv_obj_add_event_cb(s_volume_slider, volume_slider_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *lab_vol_min = lv_label_create(container);
    lv_label_set_text_static(lab_vol_min, LV_SYMBOL_VOLUME_MID);
    lv_obj_set_style_text_font(lab_vol_min, &lv_font_montserrat_20, LV_STATE_DEFAULT);
    lv_obj_align_to(lab_vol_min, s_volume_slider, LV_ALIGN_OUT_LEFT_MID, -10, 0);

    lv_obj_t *lab_vol_max = lv_label_create(container);
    lv_label_set_text_static(lab_vol_max, LV_SYMBOL_VOLUME_MAX);
    lv_obj_set_style_text_font(lab_vol_max, &lv_font_montserrat_20, LV_STATE_DEFAULT);
    lv_obj_align_to(lab_vol_max, s_volume_slider, LV_ALIGN_OUT_RIGHT_MID, 10, 0);

    /* --- 文件下拉列表 --- */
    s_music_list = lv_dropdown_create(container);
    lv_dropdown_clear_options(s_music_list);
    lv_dropdown_set_options_static(s_music_list, "扫描中...");
    lv_obj_set_style_text_font(s_music_list, &font_alipuhui20, LV_STATE_ANY);
    lv_obj_set_width(s_music_list, 200);
    lv_obj_align(s_music_list, LV_ALIGN_TOP_MID, 0, 40);
    lv_obj_add_event_cb(s_music_list, music_list_cb, LV_EVENT_VALUE_CHANGED, NULL);

    build_file_list(s_music_list);

    lvgl_port_unlock();
}

static void page_mp3_on_exit(void)
{
    /* 停止播放，但不销毁播放器（player_core 在 main.c 初始化一次） */
    if (player_core_get_state() == PLAYER_STATE_PLAYING ||
        player_core_get_state() == PLAYER_STATE_PAUSED) {
        player_core_pause();
    }
    s_music_list = NULL;
    s_label_play_pause = NULL;
    s_btn_play_pause = NULL;
    s_volume_slider = NULL;
}

/* ========== 页面注册 ========== */

void page_mp3_register(void)
{
    /* 先扫描 SD 卡 */
    s_file_count = sd_manager_scan("mp3");
    if (s_file_count <= 0) {
        /* 再尝试 wav */
        s_file_count = sd_manager_scan("wav");
    }

    if (s_file_count <= 0) {
        ESP_LOGW(TAG, "No MP3/WAV files found on SD card");
    }

    /* 设置播放器文件数 */
    player_core_set_file_count(s_file_count > 0 ? s_file_count : 0);

    /* 注册播放器回调 */
    player_core_register_callback(on_player_state_changed);

    /* 注册页面配置 */
    page_config_t cfg = {
        .id             = PAGE_MP3,
        .title          = "音乐播放器",
        .title_bg_color = LV_COLOR_MAKE(0xf8, 0x7c, 0x30),
        .icon           = NULL,   /* 图标由 page_main_menu 管理 */
        .label          = "MP3",
        .enabled        = true,
        .on_enter       = page_mp3_on_enter,
        .on_exit        = page_mp3_on_exit,
    };
    page_manager_register(&cfg);
}
