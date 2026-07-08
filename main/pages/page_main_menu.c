#include "page_main_menu.h"
#include "page_manager.h"
#include "app_event.h"
#include "esp_log.h"

static const char *TAG = "main_menu";

/* ========== 图标网格配置（配置驱动） ========== */
typedef struct {
    page_id_t    page_id;
    const char  *label;
    lv_color_t   color;
    bool         enabled;
    const lv_img_dsc_t *icon;
} menu_icon_cfg_t;

/* --- 灰显占位图标（enabled=false 时使用）--- */
LV_IMG_DECLARE(img_att_icon);
LV_IMG_DECLARE(img_music_icon);
LV_IMG_DECLARE(img_sd_icon);
LV_IMG_DECLARE(img_camera_icon);
LV_IMG_DECLARE(img_wifiset_icon);
LV_IMG_DECLARE(img_btset_icon);

static const menu_icon_cfg_t menu_icons[] = {
    { .page_id = PAGE_MP3,      .label = "MP3",    .color = LV_COLOR_MAKE(0xf8, 0x7c, 0x30), .enabled = true,  .icon = &img_music_icon   },
    { .page_id = PAGE_WIFI,     .label = "WiFi",   .color = LV_COLOR_MAKE(0xcd, 0x5c, 0x5c), .enabled = false, .icon = &img_wifiset_icon },
    { .page_id = PAGE_BLE,      .label = "BLE",    .color = LV_COLOR_MAKE(0xb8, 0x7f, 0xa8), .enabled = false, .icon = &img_btset_icon   },
    { .page_id = PAGE_CAMERA,   .label = "摄像",   .color = LV_COLOR_MAKE(0xd8, 0xb0, 0x10), .enabled = false, .icon = &img_camera_icon  },
    { .page_id = PAGE_ATTITUDE, .label = "姿态",   .color = LV_COLOR_MAKE(0x30, 0xa8, 0x30), .enabled = false, .icon = &img_att_icon     },
    { .page_id = PAGE_SDCARD,   .label = "SD卡",   .color = LV_COLOR_MAKE(0x00, 0x8b, 0x8b), .enabled = false, .icon = &img_sd_icon      },
};

#define ICON_COLS  3
#define ICON_ROWS  2
#define ICON_GAP_X 15
#define ICON_GAP_Y 15
#define ICON_W     80
#define ICON_H     80
#define START_X    ((320 - (ICON_COLS * ICON_W + (ICON_COLS - 1) * ICON_GAP_X)) / 2)
#define START_Y    50

/* ========== 图标点击事件 ========== */
static void icon_click_cb(lv_event_t *e)
{
    page_id_t page_id = (page_id_t)(uintptr_t)lv_event_get_user_data(e);
    ESP_LOGI(TAG, "Icon clicked: page_id=%d", page_id);
    app_event_post(APP_EVENT_PAGE_OPEN, (int)page_id);
}

/* ========== 创建主菜单 ========== */
void page_main_menu_create(void)
{
    lvgl_port_lock(0);

    /* 黑色背景 */
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x000000), 0);

    /* 渐变背景面板 */
    static lv_style_t style_bg;
    lv_style_init(&style_bg);
    lv_style_set_radius(&style_bg, 10);
    lv_style_set_bg_opa(&style_bg, LV_OPA_COVER);
    lv_style_set_bg_color(&style_bg, lv_color_hex(0x00BFFF));
    lv_style_set_bg_grad_color(&style_bg, lv_color_hex(0x00BF00));
    lv_style_set_bg_grad_dir(&style_bg, LV_GRAD_DIR_VER);
    lv_style_set_border_width(&style_bg, 0);
    lv_style_set_pad_all(&style_bg, 0);
    lv_style_set_width(&style_bg, 320);
    lv_style_set_height(&style_bg, 240);

    lv_obj_t *main_obj = lv_obj_create(lv_scr_act());
    lv_obj_add_style(main_obj, &style_bg, 0);

    /* 左上角欢迎语 */
    lv_obj_t *welcome_label = lv_label_create(main_obj);
    lv_obj_set_style_text_font(welcome_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(welcome_label, lv_color_hex(0xffffff), 0);
    lv_label_set_long_mode(welcome_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(welcome_label, 280);
    lv_label_set_text(welcome_label, "欢迎使用立创实战派开发板");
    lv_obj_align(welcome_label, LV_ALIGN_TOP_LEFT, 8, 5);

    /* 图标样式 */
    static lv_style_t icon_style;
    lv_style_init(&icon_style);
    lv_style_set_radius(&icon_style, 16);
    lv_style_set_bg_opa(&icon_style, LV_OPA_COVER);
    lv_style_set_text_color(&icon_style, lv_color_hex(0xffffff));
    lv_style_set_border_width(&icon_style, 0);
    lv_style_set_pad_all(&icon_style, 5);
    lv_style_set_width(&icon_style, ICON_W);
    lv_style_set_height(&icon_style, ICON_H);

    int count = sizeof(menu_icons) / sizeof(menu_icons[0]);

    for (int i = 0; i < count; i++) {
        int row = i / ICON_COLS;
        int col = i % ICON_COLS;
        int x = START_X + col * (ICON_W + ICON_GAP_X);
        int y = START_Y + row * (ICON_H + ICON_GAP_Y);

        lv_obj_t *btn = lv_btn_create(main_obj);
        lv_obj_add_style(btn, &icon_style, 0);
        lv_obj_set_pos(btn, x, y);

        if (menu_icons[i].enabled) {
            lv_obj_set_style_bg_color(btn, menu_icons[i].color, 0);
            lv_obj_add_event_cb(btn, icon_click_cb, LV_EVENT_CLICKED,
                               (void *)(uintptr_t)menu_icons[i].page_id);
        } else {
            /* 灰显 */
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x888888), 0);
            lv_obj_add_state(btn, LV_STATE_DISABLED);
        }

        /* 图标图片 */
        if (menu_icons[i].icon) {
            lv_obj_t *img = lv_img_create(btn);
            lv_img_set_src(img, menu_icons[i].icon);

            /* 灰显时降低透明度 */
            if (!menu_icons[i].enabled) {
                lv_obj_set_style_img_opa(img, LV_OPA_50, 0);
            }

            /* 有标签时图标上移一点 */
            lv_obj_align(img, LV_ALIGN_CENTER, 0, -8);
        }

        /* 标签文字 */
        lv_obj_t *label = lv_label_create(btn);
        lv_label_set_text(label, menu_icons[i].label);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
        if (!menu_icons[i].enabled) {
            lv_obj_set_style_text_color(label, lv_color_hex(0xcccccc), 0);
        }
        lv_obj_align(label, LV_ALIGN_BOTTOM_MID, 0, -4);
    }

    lvgl_port_unlock();

    /* 通知 page_manager */
    page_manager_set_main_menu(main_obj);

    ESP_LOGI(TAG, "Main menu created (%d icons, %d enabled)", count,
             menu_icons[0].enabled + menu_icons[1].enabled + menu_icons[2].enabled +
             menu_icons[3].enabled + menu_icons[4].enabled + menu_icons[5].enabled);
}
