#include "page_wifi.h"
#include "page_manager.h"
#include "app_event.h"
#include "esp32_s3_szp.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "page_wifi";

LV_FONT_DECLARE(font_alipuhui20);

/* ── 最大扫描数量 ── */
#define WIFI_SCAN_MAX    20

/* ── WiFi 句柄（页面级） ── */
static esp_netif_t         *s_sta_netif   = NULL;
static esp_event_handler_instance_t s_wifi_any_handle = NULL;
static bool                 s_scanning     = false;
static lv_obj_t            *s_scan_label   = NULL;
static lv_obj_t            *s_wifi_list    = NULL;
static int                  s_ap_count     = 0;

/* ── 扫描结果 ── */
static wifi_ap_record_t s_ap_records[WIFI_SCAN_MAX];

/* ── WiFi 事件处理（空，扫描不需要） ── */
static void wifi_event_handler(void *arg, esp_event_base_t base,
                               int32_t id, void *data)
{
    (void)arg; (void)base; (void)id; (void)data;
}

/* ── 扫描并刷新列表 ── */
static void do_scan(void)
{
    memset(s_ap_records, 0, sizeof(s_ap_records));
    s_ap_count = 0;

    uint16_t num = WIFI_SCAN_MAX;
    esp_err_t ret = esp_wifi_scan_start(NULL, true);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi scan start failed: %s", esp_err_to_name(ret));
        return;
    }

    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&num, s_ap_records));
    s_ap_count = (int)num;
    ESP_LOGI(TAG, "WiFi scan: %d AP(s) found", s_ap_count);
}

/* ── 扫描任务（避免阻塞 UI） ── */
static void wifi_scan_task(void *pv)
{
    (void)pv;

    lvgl_port_lock(0);
    if (s_scan_label) {
        lv_label_set_text(s_scan_label, "正在扫描 WiFi...");
    }
    lvgl_port_unlock();

    do_scan();

    lvgl_port_lock(0);

    /* 更新标题 */
    if (s_scan_label) {
        lv_label_set_text_fmt(s_scan_label, "%d 个网络", s_ap_count);
    }

    /* 填充列表 */
    if (s_wifi_list) {
        lv_obj_clean(s_wifi_list);
        for (int i = 0; i < s_ap_count; i++) {
            char info[64];
            snprintf(info, sizeof(info), "%s  (%d%%)",
                     (const char *)s_ap_records[i].ssid,
                     (int)((s_ap_records[i].rssi + 100) * 2)); /* RSSI 近似百分比 */
            lv_obj_t *btn = lv_list_add_btn(s_wifi_list, LV_SYMBOL_WIFI, info);
            lv_obj_set_style_text_font(lv_obj_get_child(btn, 0),
                                       &lv_font_montserrat_24, 0);
        }
    }

    lvgl_port_unlock();
    s_scanning = false;
    vTaskDelete(NULL);
}

/* ── 点击列表项 ── */
static void list_item_cb(lv_event_t *e)
{
    const char *name = lv_list_get_btn_text(s_wifi_list, lv_event_get_target(e));
    if (name) {
        ESP_LOGI(TAG, "Selected: %s", name);
    }
    /* 第一阶段不做连接 */
}

/* ── 刷新按钮 ── */
static void refresh_btn_cb(lv_event_t *e)
{
    (void)e;
    if (!s_scanning) {
        s_scanning = true;
        xTaskCreatePinnedToCore(wifi_scan_task, "wifi_scan", 4 * 1024,
                                NULL, 3, NULL, 1);
    }
}

/* ── 页面 on_enter ── */
static void page_wifi_on_enter(void)
{
    lv_obj_t *container = page_manager_get_container();
    if (container == NULL) return;

    /* 初始化 WiFi 栈 */
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    s_sta_netif = esp_netif_create_default_wifi_sta();
    assert(s_sta_netif);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                     &wifi_event_handler, NULL, &s_wifi_any_handle));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    lvgl_port_lock(0);

    /* 扫描提示 */
    s_scan_label = lv_label_create(container);
    lv_obj_set_style_text_font(s_scan_label, &font_alipuhui20, 0);
    lv_obj_set_style_text_color(s_scan_label, lv_color_hex(0x000000), 0);
    lv_label_set_text(s_scan_label, "正在扫描 WiFi...");
    lv_obj_align(s_scan_label, LV_ALIGN_TOP_MID, 0, 42);

    /* 刷新按钮 */
    lv_obj_t *btn_refresh = lv_btn_create(container);
    lv_obj_align(btn_refresh, LV_ALIGN_TOP_RIGHT, -5, 42);
    lv_obj_set_size(btn_refresh, 50, 30);
    lv_obj_add_event_cb(btn_refresh, refresh_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *refresh_label = lv_label_create(btn_refresh);
    lv_label_set_text(refresh_label, LV_SYMBOL_REFRESH);
    lv_obj_set_style_text_font(refresh_label, &lv_font_montserrat_20, 0);
    lv_obj_center(refresh_label);

    /* WiFi 列表 */
    s_wifi_list = lv_list_create(container);
    lv_obj_set_size(s_wifi_list, 310, 150);
    lv_obj_align(s_wifi_list, LV_ALIGN_TOP_LEFT, 5, 75);
    lv_obj_set_style_border_width(s_wifi_list, 0, 0);
    lv_obj_set_style_text_font(s_wifi_list, &font_alipuhui20, 0);
    lv_obj_set_scrollbar_mode(s_wifi_list, LV_SCROLLBAR_MODE_OFF);

    lvgl_port_unlock();

    /* 启动后台扫描 */
    s_scanning = true;
    xTaskCreatePinnedToCore(wifi_scan_task, "wifi_scan", 4 * 1024,
                            NULL, 3, NULL, 1);
}

/* ── 页面 on_exit ── */
static void page_wifi_on_exit(void)
{
    /* 等待扫描任务结束 */
    int timeout = 50;
    while (s_scanning && timeout-- > 0) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    /* 释放 WiFi 资源 */
    esp_wifi_stop();
    esp_wifi_deinit();
    esp_netif_destroy(s_sta_netif);
    s_sta_netif = NULL;
    esp_event_loop_delete_default();

    s_scan_label = NULL;
    s_wifi_list  = NULL;
}

/* ── 注册 ── */
void page_wifi_register(void)
{
    page_config_t cfg = {
        .id             = PAGE_WIFI,
        .title          = "WiFi 扫描",
        .title_bg_color = LV_COLOR_MAKE(0xcd, 0x5c, 0x5c),
        .icon           = NULL,
        .label          = "WiFi",
        .enabled        = true,
        .on_enter       = page_wifi_on_enter,
        .on_exit        = page_wifi_on_exit,
    };
    page_manager_register(&cfg);
}
