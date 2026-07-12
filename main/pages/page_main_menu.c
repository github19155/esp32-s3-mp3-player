#include "page_main_menu.h"
#include "page_manager.h"
#include "app_event.h"
#include "esp_log.h"
#include <stdio.h>

static const char *TAG = "main_menu";

/* ========== Touch Test 计数器 ========== */
static int touch_count = 0;
static lv_obj_t *count_label = NULL;

static void touch_test_click_cb(lv_event_t *e)
{
    (void)e;
    touch_count++;
    ESP_LOGI(TAG, "Touch test count: %d", touch_count);

    if (count_label) {
        char buf[32];
        snprintf(buf, sizeof(buf), "Taps: %d", touch_count);
        lv_label_set_text(count_label, buf);
    }
}

/* ========== WiFi 页面入口 ========== */
static void wifi_entry_click_cb(lv_event_t *e)
{
    (void)e;
    app_event_post(APP_EVENT_PAGE_OPEN, (int)PAGE_WIFI);
}

/* ========== 创建核心主菜单 ========== */
void page_main_menu_create(void)
{
    lvgl_port_lock(0);
    touch_count = 0;

    /* 黑色背景 */
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x000000), 0);

    /* 渐变背景面板 */
    static lv_style_t style_bg;
    lv_style_init(&style_bg);
    lv_style_set_radius(&style_bg, 10);
    lv_style_set_bg_opa(&style_bg, LV_OPA_COVER);
    lv_style_set_bg_color(&style_bg, lv_color_hex(0x003366));
    lv_style_set_bg_grad_color(&style_bg, lv_color_hex(0x001a33));
    lv_style_set_bg_grad_dir(&style_bg, LV_GRAD_DIR_VER);
    lv_style_set_border_width(&style_bg, 0);
    lv_style_set_pad_all(&style_bg, 0);
    lv_style_set_width(&style_bg, 320);
    lv_style_set_height(&style_bg, 240);

    lv_obj_t *main_obj = lv_obj_create(lv_scr_act());
    lv_obj_add_style(main_obj, &style_bg, 0);

    /* ── 标题：ESP32-S3 CORE ── */
    lv_obj_t *title_label = lv_label_create(main_obj);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(title_label, lv_color_hex(0xffffff), 0);
    lv_label_set_text(title_label, "ESP32-S3 CORE");
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 20);

    /* ── 副标题：Display + Touch Ready ── */
    lv_obj_t *sub_label = lv_label_create(main_obj);
    lv_obj_set_style_text_font(sub_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(sub_label, lv_color_hex(0x88ccff), 0);
    lv_label_set_text(sub_label, "Display + Touch + WiFi Ready");
    lv_obj_align(sub_label, LV_ALIGN_TOP_MID, 0, 55);

    /* ── 分隔线 ── */
    lv_obj_t *line = lv_obj_create(main_obj);
    lv_obj_set_size(line, 260, 2);
    lv_obj_set_style_border_width(line, 0, 0);
    lv_obj_set_style_bg_color(line, lv_color_hex(0x336699), 0);
    lv_obj_set_style_bg_opa(line, LV_OPA_COVER, 0);
    lv_obj_align(line, LV_ALIGN_TOP_MID, 0, 85);

    /* ── 通用按钮样式 ── */
    static lv_style_t btn_style;
    lv_style_init(&btn_style);
    lv_style_set_radius(&btn_style, 12);
    lv_style_set_bg_color(&btn_style, lv_color_hex(0x0099cc));
    lv_style_set_bg_opa(&btn_style, LV_OPA_COVER);
    lv_style_set_border_width(&btn_style, 0);
    lv_style_set_pad_all(&btn_style, 10);
    lv_style_set_text_color(&btn_style, lv_color_hex(0xffffff));

    /* ── Touch Test 按钮 ── */
    lv_obj_t *btn = lv_btn_create(main_obj);
    lv_obj_add_style(btn, &btn_style, 0);
    lv_obj_set_size(btn, 160, 50);
    lv_obj_align(btn, LV_ALIGN_CENTER, 0, -25);
    lv_obj_add_event_cb(btn, touch_test_click_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, "Touch Test");
    lv_obj_set_style_text_font(btn_label, &lv_font_montserrat_20, 0);
    lv_obj_center(btn_label);

    /* ── 点击计数显示 ── */
    count_label = lv_label_create(main_obj);
    lv_obj_set_style_text_font(count_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(count_label, lv_color_hex(0xcccccc), 0);
    lv_label_set_text(count_label, "Taps: 0");
    lv_obj_align(count_label, LV_ALIGN_CENTER, 0, 35);

    /* ── WiFi 按钮（右下） ── */
    static lv_style_t wifi_btn_style;
    lv_style_init(&wifi_btn_style);
    lv_style_set_radius(&wifi_btn_style, 12);
    lv_style_set_bg_color(&wifi_btn_style, lv_color_hex(0xcd5c5c));
    lv_style_set_bg_opa(&wifi_btn_style, LV_OPA_COVER);
    lv_style_set_border_width(&wifi_btn_style, 0);
    lv_style_set_pad_all(&wifi_btn_style, 10);
    lv_style_set_text_color(&wifi_btn_style, lv_color_hex(0xffffff));

    lv_obj_t *wifi_btn = lv_btn_create(main_obj);
    lv_obj_add_style(wifi_btn, &wifi_btn_style, 0);
    lv_obj_set_size(wifi_btn, 80, 40);
    lv_obj_align(wifi_btn, LV_ALIGN_BOTTOM_RIGHT, -10, -30);
    lv_obj_add_event_cb(wifi_btn, wifi_entry_click_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *wifi_btn_label = lv_label_create(wifi_btn);
    lv_obj_set_style_text_font(wifi_btn_label, &lv_font_montserrat_14, 0);
    lv_label_set_text(wifi_btn_label, "WiFi");
    lv_obj_center(wifi_btn_label);

    /* ── 底部版本信息 ── */
    lv_obj_t *ver_label = lv_label_create(main_obj);
    lv_obj_set_style_text_font(ver_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(ver_label, lv_color_hex(0x668899), 0);
    lv_label_set_text(ver_label, "core-base v1.0 + WiFi");
    lv_obj_align(ver_label, LV_ALIGN_BOTTOM_MID, 0, -10);

    lvgl_port_unlock();

    /* 通知 page_manager */
    page_manager_set_main_menu(main_obj);

    ESP_LOGI(TAG, "Core main menu created (with WiFi entry)");
}
