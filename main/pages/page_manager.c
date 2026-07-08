#include "page_manager.h"
#include "app_event.h"
#include "esp32_s3_szp.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "page_mgr";

/* ========== 注册表 ========== */
#define MAX_PAGES 10

static page_config_t s_registry[MAX_PAGES];
static int s_registry_count = 0;

/* ========== 当前页面状态 ========== */
static page_id_t   s_current_page = PAGE_NONE;
static lv_obj_t   *s_current_obj = NULL;

/* ========== 主菜单引用（page_main_menu 设置） ========== */
static lv_obj_t *s_main_menu_obj = NULL;

/* ========== 内部辅助：创建页面通用容器 ========== */
static lv_obj_t *create_page_container(lv_color_t bg_color)
{
    static lv_style_t style;
    lv_style_init(&style);
    lv_style_set_radius(&style, 10);
    lv_style_set_bg_opa(&style, LV_OPA_COVER);
    lv_style_set_bg_color(&style, bg_color);
    lv_style_set_border_width(&style, 0);
    lv_style_set_pad_all(&style, 0);
    lv_style_set_width(&style, 320);
    lv_style_set_height(&style, 240);

    lv_obj_t *obj = lv_obj_create(lv_scr_act());
    lv_obj_add_style(obj, &style, 0);
    return obj;
}

/* ========== 页面管理 API ========== */

void page_manager_register(const page_config_t *config)
{
    if (s_registry_count >= MAX_PAGES) {
        ESP_LOGE(TAG, "Registry full, cannot register page %d", config->id);
        return;
    }
    if (config->id <= PAGE_NONE || config->id >= PAGE_MAX) {
        ESP_LOGE(TAG, "Invalid page id: %d", config->id);
        return;
    }

    /* 检查重复 */
    for (int i = 0; i < s_registry_count; i++) {
        if (s_registry[i].id == config->id) {
            ESP_LOGW(TAG, "Page %d already registered, updating", config->id);
            s_registry[i] = *config;
            return;
        }
    }

    s_registry[s_registry_count++] = *config;
    ESP_LOGI(TAG, "Registered page %d: \"%s\" (enabled=%d)", config->id, config->title, config->enabled);
}

static const page_config_t *find_config(page_id_t id)
{
    for (int i = 0; i < s_registry_count; i++) {
        if (s_registry[i].id == id) return &s_registry[i];
    }
    return NULL;
}

int page_manager_get_page_count(void)
{
    return s_registry_count;
}

const page_config_t *page_manager_get_page(int index)
{
    if (index < 0 || index >= s_registry_count) return NULL;
    return &s_registry[index];
}

void page_manager_open(page_id_t id)
{
    if (s_current_page != PAGE_NONE) {
        ESP_LOGW(TAG, "Page %d already open, close it first", s_current_page);
        page_manager_close();
    }

    const page_config_t *cfg = find_config(id);
    if (cfg == NULL) {
        ESP_LOGE(TAG, "Page %d not registered", id);
        return;
    }
    if (!cfg->enabled) {
        ESP_LOGW(TAG, "Page %d is disabled", id);
        return;
    }

    ESP_LOGI(TAG, "Opening page %d: \"%s\"", id, cfg->title);

    /* 创建容器 */
    lvgl_port_lock(0);
    s_current_obj = create_page_container(cfg->title_bg_color);

    /* 创建标题栏 */
    lv_obj_t *title_bar = lv_obj_create(s_current_obj);
    lv_obj_set_size(title_bar, 320, 40);
    lv_obj_set_style_pad_all(title_bar, 0, 0);
    lv_obj_align(title_bar, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(title_bar, cfg->title_bg_color, 0);

    lv_obj_t *title_label = lv_label_create(title_bar);
    lv_label_set_text(title_label, cfg->title);
    lv_obj_set_style_text_color(title_label, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_20, 0);
    lv_obj_align(title_label, LV_ALIGN_CENTER, 0, 0);

    /* 创建返回按钮 */
    lv_obj_t *btn_back = lv_btn_create(title_bar);
    lv_obj_align(btn_back, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_size(btn_back, 60, 30);
    lv_obj_set_style_border_width(btn_back, 0, 0);
    lv_obj_set_style_pad_all(btn_back, 0, 0);
    lv_obj_set_style_bg_opa(btn_back, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(btn_back, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_add_event_cb(btn_back, page_manager_on_back_clicked, LV_EVENT_CLICKED, NULL);

    lv_obj_t *back_label = lv_label_create(btn_back);
    lv_label_set_text(back_label, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_font(back_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(back_label, lv_color_hex(0xffffff), 0);
    lv_obj_align(back_label, LV_ALIGN_CENTER, -10, 0);

    lvgl_port_unlock();

    s_current_page = id;

    /* 调用页面自定义 on_enter */
    if (cfg->on_enter) {
        cfg->on_enter();
    }
}

void page_manager_close(void)
{
    if (s_current_page == PAGE_NONE) return;

    const page_config_t *cfg = find_config(s_current_page);

    /* 调用页面自定义 on_exit */
    if (cfg && cfg->on_exit) {
        cfg->on_exit();
    }

    lvgl_port_lock(0);
    if (s_current_obj) {
        lv_obj_del(s_current_obj);
        s_current_obj = NULL;
    }
    lvgl_port_unlock();

    ESP_LOGI(TAG, "Closed page %d", s_current_page);
    s_current_page = PAGE_NONE;
}

page_id_t page_manager_get_current(void)
{
    return s_current_page;
}

lv_obj_t *page_manager_get_container(void)
{
    return s_current_obj;
}

void page_manager_set_main_menu(lv_obj_t *main_menu)
{
    s_main_menu_obj = main_menu;
}

lv_obj_t *page_manager_get_main_menu(void)
{
    return s_main_menu_obj;
}

/* ========== 事件回调 ========== */

void page_manager_on_back_clicked(lv_event_t *e)
{
    (void)e;
    app_event_post(APP_EVENT_PAGE_CLOSE, s_current_page);
}

void page_manager_on_event(const app_event_t *event)
{
    switch (event->type) {
    case APP_EVENT_PAGE_OPEN:
        page_manager_open((page_id_t)event->payload);
        break;
    case APP_EVENT_PAGE_CLOSE:
        page_manager_close();
        break;
    default:
        break;
    }
}

void page_manager_init(void)
{
    s_current_page = PAGE_NONE;
    s_current_obj = NULL;
    ESP_LOGI(TAG, "Page manager initialized (%d pages registered)", s_registry_count);
}
