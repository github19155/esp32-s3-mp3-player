#include "page_wifi.h"
#include "page_manager.h"
#include "app_event.h"
#include "esp32_s3_szp.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "page_wifi";

LV_FONT_DECLARE(font_alipuhui20);

#define WIFI_SCAN_MAX    20
#define WIFI_MAX_RETRY   3
#define NVS_NAMESPACE    "wifi_cfg"
#define NVS_MAX_NET      5

/* ── WiFi 全局句柄（跨页面生命周期） ── */
static esp_netif_t               *s_sta_netif      = NULL;
static esp_event_handler_instance_t s_wifi_handle   = NULL;
static esp_event_handler_instance_t s_ip_handle     = NULL;
static bool                        s_wifi_initialized = false;
static bool                        s_auto_connecting  = false;
static char                        s_last_ip[16]      = "";
static char                        s_pending_password[64] = "";

/* ── 扫描/连接状态 ── */
static lv_obj_t   *s_scan_label       = NULL;
static lv_obj_t   *s_wifi_list        = NULL;
static lv_obj_t   *s_conn_label       = NULL;
static bool        s_scanning         = false;
static bool        s_connecting       = false;
static bool        s_connected        = false;
static int         s_ap_count         = 0;
static int         s_retry_count      = 0;
static wifi_ap_record_t s_ap_records[WIFI_SCAN_MAX];
static char        s_connected_ssid[33] = "";

/* ── 密码页面 ── */
static lv_obj_t   *s_pwd_page         = NULL;
static lv_obj_t   *s_pwd_textarea     = NULL;
static lv_obj_t   *s_pwd_ssid_label   = NULL;

/* ── NVS 多网络操作 ── */
static int wifi_nvs_count(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &h) != ESP_OK) return 0;
    uint8_t n = 0;
    nvs_get_u8(h, "net_count", &n);
    nvs_close(h);
    return (int)n;
}

static bool wifi_nvs_load(int idx, char *ssid, size_t sl, char *pwd, size_t pl)
{
    nvs_handle_t h;
    char key[24];
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &h) != ESP_OK) return false;
    snprintf(key, sizeof(key), "net%d_ssid", idx);
    size_t l = sl;
    esp_err_t r = nvs_get_str(h, key, ssid, &l);
    if (r != ESP_OK) { nvs_close(h); return false; }
    snprintf(key, sizeof(key), "net%d_pwd", idx);
    l = pl;
    r = nvs_get_str(h, key, pwd, &l);
    nvs_close(h);
    return (r == ESP_OK);
}

static bool wifi_nvs_find_password(const char *ssid, char *pwd, size_t pl)
{
    int n = wifi_nvs_count();
    char saved_ssid[33], saved_pwd[65];
    for (int i = 0; i < n; i++) {
        if (wifi_nvs_load(i, saved_ssid, sizeof(saved_ssid), saved_pwd, sizeof(saved_pwd)) &&
            strcmp(saved_ssid, ssid) == 0) {
            strncpy(pwd, saved_pwd, pl - 1);
            pwd[pl - 1] = '\0';
            return true;
        }
    }
    return false;
}

static void wifi_nvs_save(const char *ssid, const char *pwd)
{
    char old_pwd[65];
    if (wifi_nvs_find_password(ssid, old_pwd, sizeof(old_pwd))) {
        if (strcmp(old_pwd, pwd) == 0) return;
    }
    int idx = wifi_nvs_count();
    if (idx >= NVS_MAX_NET) idx = 0;
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) != ESP_OK) return;
    char key[24];
    snprintf(key, sizeof(key), "net%d_ssid", idx);
    nvs_set_str(h, key, ssid);
    snprintf(key, sizeof(key), "net%d_pwd", idx);
    nvs_set_str(h, key, pwd);
    if (idx >= wifi_nvs_count()) nvs_set_u8(h, "net_count", (uint8_t)(idx + 1));
    nvs_commit(h);
    nvs_close(h);
    ESP_LOGI(TAG, "NVS saved[%d]: %s", idx, ssid);
}

static void wifi_nvs_clear(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) != ESP_OK) return;
    char key[24];
    for (int i = 0; i < NVS_MAX_NET; i++) {
        snprintf(key, sizeof(key), "net%d_ssid", i); nvs_erase_key(h, key);
        snprintf(key, sizeof(key), "net%d_pwd", i); nvs_erase_key(h, key);
    }
    nvs_set_u8(h, "net_count", 0);
    nvs_commit(h); nvs_close(h);
}

static bool wifi_nvs_has_saved(void)
{
    return wifi_nvs_count() > 0;
}

static void wifi_event_cb(void *arg, esp_event_base_t base, int32_t id, void *data);
static void wifi_scan_task(void *pv);

/* ── WiFi 栈初始化（幂等） ── */
static void ensure_wifi_stack(void)
{
    if (s_wifi_initialized) return;
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    s_sta_netif = esp_netif_create_default_wifi_sta();
    assert(s_sta_netif);
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_cb, NULL, &s_wifi_handle));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_cb, NULL, &s_ip_handle));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    s_wifi_initialized = true;
    ESP_LOGI(TAG, "WiFi stack initialized");
}

/* ── WiFi 事件回调 ── */
static void wifi_event_cb(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) return;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *d = (wifi_event_sta_disconnected_t *)data;
        ESP_LOGW(TAG, "WiFi disconnected, reason=%d", d->reason);
        if (s_auto_connecting && s_retry_count < WIFI_MAX_RETRY) {
            esp_wifi_connect(); s_retry_count++;
        } else if (s_auto_connecting) {
            s_auto_connecting = false; s_connected = false;
            s_connected_ssid[0] = '\0'; s_pending_password[0] = '\0';
            lvgl_port_lock(0);
            if (s_conn_label) lv_label_set_text(s_conn_label, "自动连接失败");
            lvgl_port_unlock();
            s_scanning = true;
            xTaskCreatePinnedToCore(wifi_scan_task, "wifi_scan", 4*1024, NULL, 3, NULL, 1);
        } else if (s_connecting && s_retry_count < WIFI_MAX_RETRY) {
            esp_wifi_connect(); s_retry_count++;
        } else if (s_connecting) {
            s_connecting = false;
            s_connected_ssid[0] = '\0'; s_pending_password[0] = '\0';
            lvgl_port_lock(0);
            if (s_conn_label) lv_label_set_text(s_conn_label, "密码错误或连接失败");
            lvgl_port_unlock();
        }
    }
    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *ev = (ip_event_got_ip_t *)data;
        s_connecting = false; s_auto_connecting = false;
        s_connected = true;
        snprintf(s_last_ip, sizeof(s_last_ip), IPSTR, IP2STR(&ev->ip_info.ip));
        if (s_pending_password[0]) {
            wifi_nvs_save(s_connected_ssid, s_pending_password);
            s_pending_password[0] = '\0';
        }
        lvgl_port_lock(0);
        if (s_conn_label) lv_label_set_text_fmt(s_conn_label, "IP: " IPSTR, IP2STR(&ev->ip_info.ip));
        lvgl_port_unlock();
        if (!s_scanning) {
            s_scanning = true;
            xTaskCreatePinnedToCore(wifi_scan_task, "wifi_scan", 4*1024, NULL, 3, NULL, 1);
        }
    }
}

static void list_item_cb(lv_event_t *e);

/* ── 扫描 ── */
static void do_scan(void)
{
    memset(s_ap_records, 0, sizeof(s_ap_records));
    s_ap_count = 0;
    uint16_t num = WIFI_SCAN_MAX;
    if (esp_wifi_scan_start(NULL, true) != ESP_OK) { ESP_LOGE(TAG, "Scan start failed"); return; }
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&num, s_ap_records));
    s_ap_count = (int)num;
}

static void wifi_scan_task(void *pv)
{
    (void)pv;
    lvgl_port_lock(0);
    if (s_scan_label) lv_label_set_text(s_scan_label, "正在扫描 WiFi...");
    lvgl_port_unlock();
    do_scan();
    lvgl_port_lock(0);
    if (s_scan_label) lv_label_set_text_fmt(s_scan_label, "%d 个网络", s_ap_count);
    if (s_wifi_list) {
        lv_obj_clean(s_wifi_list);
        for (int i = 0; i < s_ap_count; i++) {
            char info[64];
            bool is_conn = s_connected && s_connected_ssid[0] &&
                           strcmp((const char *)s_ap_records[i].ssid, s_connected_ssid) == 0;
            if (is_conn) {
                snprintf(info, sizeof(info), "%s  — 已连接", (const char *)s_ap_records[i].ssid);
            } else {
                snprintf(info, sizeof(info), "%s  (%d%%)",
                         (const char *)s_ap_records[i].ssid,
                         (int)((s_ap_records[i].rssi + 100) * 2));
            }
            lv_obj_t *btn = lv_list_add_btn(s_wifi_list, LV_SYMBOL_WIFI, info);
            lv_obj_add_event_cb(btn, list_item_cb, LV_EVENT_CLICKED, NULL);
            lv_obj_set_style_text_font(lv_obj_get_child(btn, 0), &lv_font_montserrat_24, 0);
        }
    }
    lvgl_port_unlock();
    s_scanning = false;
    vTaskDelete(NULL);
}

static void forget_btn_cb(lv_event_t *e)
{
    (void)e;
    wifi_nvs_clear();
    if (s_connected) {
        esp_wifi_disconnect();
        s_connected = false; s_connected_ssid[0] = '\0'; s_last_ip[0] = '\0';
    }
    lvgl_port_lock(0);
    if (s_conn_label) {
        lv_label_set_text(s_conn_label, "已清除保存网络");
        lv_obj_set_style_text_color(s_conn_label, lv_color_hex(0xcc0000), 0);
    }
    lvgl_port_unlock();
}

static void refresh_btn_cb(lv_event_t *e)
{
    (void)e;
    if (!s_scanning) {
        s_scanning = true;
        xTaskCreatePinnedToCore(wifi_scan_task, "wifi_scan", 4*1024, NULL, 3, NULL, 1);
    }
}

/* ── 密码输入界面 ── */
static lv_obj_t *roller_num, *roller_low, *roller_up;

static void roller_mask_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *obj = lv_event_get_target(e);
    static int16_t mask_top = -1, mask_bottom = -1;
    if (code == LV_EVENT_COVER_CHECK) {
        lv_event_set_cover_res(e, LV_COVER_RES_MASKED);
    } else if (code == LV_EVENT_DRAW_MAIN_BEGIN) {
        const lv_font_t *font = lv_obj_get_style_text_font(obj, LV_PART_MAIN);
        lv_coord_t line_space = lv_obj_get_style_text_line_space(obj, LV_PART_MAIN);
        lv_coord_t font_h = lv_font_get_line_height(font);
        lv_area_t coords; lv_obj_get_coords(obj, &coords);
        lv_area_t rect = {coords.x1, coords.y1, coords.x2,
                          coords.y1 + (lv_obj_get_height(obj)-font_h-line_space)/2};
        lv_draw_mask_fade_param_t *fade = lv_mem_buf_get(sizeof(*fade));
        lv_draw_mask_fade_init(fade, &rect, LV_OPA_TRANSP, rect.y1, LV_OPA_COVER, rect.y2);
        mask_top = lv_draw_mask_add(fade, NULL);
        rect.y1 = rect.y2 + font_h + line_space - 1; rect.y2 = coords.y2;
        fade = lv_mem_buf_get(sizeof(*fade));
        lv_draw_mask_fade_init(fade, &rect, LV_OPA_COVER, rect.y1, LV_OPA_TRANSP, rect.y2);
        mask_bottom = lv_draw_mask_add(fade, NULL);
    } else if (code == LV_EVENT_DRAW_POST_END) {
        lv_draw_mask_fade_param_t *f;
        f = lv_draw_mask_remove_id(mask_top); lv_draw_mask_free_param(f); lv_mem_buf_release(f);
        f = lv_draw_mask_remove_id(mask_bottom); lv_draw_mask_free_param(f); lv_mem_buf_release(f);
        mask_top = mask_bottom = -1;
    }
}

static void roller_ok_cb(lv_event_t *e)
{
    lv_obj_t *roller = lv_event_get_user_data(e);
    char buf[2];
    lv_roller_get_selected_str(roller, buf, sizeof(buf));
    lv_textarea_add_text(s_pwd_textarea, buf);
}

static void pwd_del_cb(lv_event_t *e) { (void)e; lv_textarea_del_char(s_pwd_textarea); }

static void pwd_connect_cb(lv_event_t *e)
{
    (void)e;
    const char *ssid = lv_label_get_text(s_pwd_ssid_label);
    const char *pwd  = lv_textarea_get_text(s_pwd_textarea);
    wifi_config_t cfg = { .sta = { .threshold.authmode = WIFI_AUTH_WPA2_PSK }};
    strncpy((char *)cfg.sta.ssid, ssid, sizeof(cfg.sta.ssid)-1);
    strncpy((char *)cfg.sta.password, pwd, sizeof(cfg.sta.password)-1);
    if (s_connected) { esp_wifi_disconnect(); s_connected = false; }
    esp_wifi_set_config(WIFI_IF_STA, &cfg);
    esp_wifi_connect();
    s_connecting = true; s_connected = false; s_retry_count = 0;
    strncpy(s_connected_ssid, ssid, sizeof(s_connected_ssid)-1);
    strncpy(s_pending_password, pwd, sizeof(s_pending_password)-1);
    lv_obj_del(s_pwd_page); s_pwd_page = NULL;
    lvgl_port_lock(0);
    if (s_conn_label) lv_label_set_text(s_conn_label, "连接中...");
    lvgl_port_unlock();
}

static void pwd_back_cb(lv_event_t *e) { (void)e; lv_obj_del(s_pwd_page); s_pwd_page = NULL; }

static void show_password_input(int ap_index)
{
    lvgl_port_lock(0);
    s_pwd_page = lv_obj_create(lv_scr_act());
    lv_obj_set_size(s_pwd_page, 320, 240);
    lv_obj_set_style_border_width(s_pwd_page, 0, 0);
    lv_obj_set_style_pad_all(s_pwd_page, 0, 0);
    lv_obj_set_style_radius(s_pwd_page, 0, 0);

    lv_obj_t *btn_back = lv_btn_create(s_pwd_page);
    lv_obj_align(btn_back, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_size(btn_back, 60, 40);
    lv_obj_set_style_bg_opa(btn_back, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_add_event_cb(btn_back, pwd_back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lab_back = lv_label_create(btn_back);
    lv_label_set_text(lab_back, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(lab_back, lv_color_hex(0x000000), 0);
    lv_obj_center(lab_back);

    s_pwd_ssid_label = lv_label_create(s_pwd_page);
    lv_obj_set_style_text_font(s_pwd_ssid_label, &lv_font_montserrat_20, 0);
    lv_label_set_text(s_pwd_ssid_label, (const char *)s_ap_records[ap_index].ssid);
    lv_obj_align(s_pwd_ssid_label, LV_ALIGN_TOP_MID, 0, 10);

    s_pwd_textarea = lv_textarea_create(s_pwd_page);
    lv_textarea_set_one_line(s_pwd_textarea, true);
    lv_textarea_set_placeholder_text(s_pwd_textarea, "Password");
    lv_obj_set_width(s_pwd_textarea, 150);
    lv_obj_align(s_pwd_textarea, LV_ALIGN_TOP_LEFT, 10, 40);
    lv_obj_add_state(s_pwd_textarea, LV_STATE_FOCUSED);

    /* 自动填入已保存密码 */
    char saved_pwd[65] = "";
    if (wifi_nvs_find_password((const char *)s_ap_records[ap_index].ssid,
                                saved_pwd, sizeof(saved_pwd))) {
        lv_textarea_set_text(s_pwd_textarea, saved_pwd);
    }

    lv_obj_t *btn_ok = lv_btn_create(s_pwd_page);
    lv_obj_align(btn_ok, LV_ALIGN_TOP_LEFT, 170, 40);
    lv_obj_set_width(btn_ok, 60);
    lv_obj_add_event_cb(btn_ok, pwd_connect_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lab_ok = lv_label_create(btn_ok);
    lv_label_set_text(lab_ok, "OK"); lv_obj_center(lab_ok);

    lv_obj_t *btn_del = lv_btn_create(s_pwd_page);
    lv_obj_align(btn_del, LV_ALIGN_TOP_LEFT, 240, 40);
    lv_obj_set_width(btn_del, 60);
    lv_obj_add_event_cb(btn_del, pwd_del_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lab_del = lv_label_create(btn_del);
    lv_label_set_text(lab_del, LV_SYMBOL_BACKSPACE); lv_obj_center(lab_del);

    static lv_style_t roller_style;
    lv_style_init(&roller_style);
    lv_style_set_bg_color(&roller_style, lv_color_black());
    lv_style_set_text_color(&roller_style, lv_color_white());
    lv_style_set_border_width(&roller_style, 0);
    lv_style_set_pad_all(&roller_style, 0);

    roller_num = lv_roller_create(s_pwd_page);
    lv_obj_add_style(roller_num, &roller_style, 0);
    lv_roller_set_options(roller_num, "0\n1\n2\n3\n4\n5\n6\n7\n8\n9", LV_ROLLER_MODE_INFINITE);
    lv_roller_set_visible_row_count(roller_num, 3);
    lv_obj_set_width(roller_num, 90);
    lv_obj_align(roller_num, LV_ALIGN_BOTTOM_LEFT, 15, -53);
    lv_obj_add_event_cb(roller_num, roller_mask_cb, LV_EVENT_ALL, NULL);
    lv_obj_t *bn = lv_btn_create(s_pwd_page);
    lv_obj_align(bn, LV_ALIGN_BOTTOM_LEFT, 15, -10); lv_obj_set_width(bn, 90);
    lv_obj_add_event_cb(bn, roller_ok_cb, LV_EVENT_CLICKED, roller_num);
    lv_obj_t *ln = lv_label_create(bn); lv_label_set_text(ln, LV_SYMBOL_OK); lv_obj_center(ln);

    roller_low = lv_roller_create(s_pwd_page);
    lv_obj_add_style(roller_low, &roller_style, 0);
    lv_roller_set_options(roller_low,
        "a\nb\nc\nd\ne\nf\ng\nh\ni\nj\nk\nl\nm\nn\no\np\nq\nr\ns\nt\nu\nv\nw\nx\ny\nz", LV_ROLLER_MODE_INFINITE);
    lv_roller_set_visible_row_count(roller_low, 3);
    lv_obj_set_width(roller_low, 90);
    lv_obj_align(roller_low, LV_ALIGN_BOTTOM_LEFT, 115, -53);
    lv_obj_add_event_cb(roller_low, roller_mask_cb, LV_EVENT_ALL, NULL);
    bn = lv_btn_create(s_pwd_page);
    lv_obj_align(bn, LV_ALIGN_BOTTOM_LEFT, 115, -10); lv_obj_set_width(bn, 90);
    lv_obj_add_event_cb(bn, roller_ok_cb, LV_EVENT_CLICKED, roller_low);
    ln = lv_label_create(bn); lv_label_set_text(ln, LV_SYMBOL_OK); lv_obj_center(ln);

    roller_up = lv_roller_create(s_pwd_page);
    lv_obj_add_style(roller_up, &roller_style, 0);
    lv_roller_set_options(roller_up,
        "A\nB\nC\nD\nE\nF\nG\nH\nI\nJ\nK\nL\nM\nN\nO\nP\nQ\nR\nS\nT\nU\nV\nW\nX\nY\nZ", LV_ROLLER_MODE_INFINITE);
    lv_roller_set_visible_row_count(roller_up, 3);
    lv_obj_set_width(roller_up, 90);
    lv_obj_align(roller_up, LV_ALIGN_BOTTOM_LEFT, 215, -53);
    lv_obj_add_event_cb(roller_up, roller_mask_cb, LV_EVENT_ALL, NULL);
    bn = lv_btn_create(s_pwd_page);
    lv_obj_align(bn, LV_ALIGN_BOTTOM_LEFT, 215, -10); lv_obj_set_width(bn, 90);
    lv_obj_add_event_cb(bn, roller_ok_cb, LV_EVENT_CLICKED, roller_up);
    ln = lv_label_create(bn); lv_label_set_text(ln, LV_SYMBOL_OK); lv_obj_center(ln);

    lvgl_port_unlock();
}

/* ── 点击扫描列表项 ── */
static void list_item_cb(lv_event_t *e)
{
    const char *text = lv_list_get_btn_text(s_wifi_list, lv_event_get_target(e));
    if (!text) return;
    for (int i = 0; i < s_ap_count; i++) {
        char info[64];
        snprintf(info, sizeof(info), "%s  (%d%%)",
                 (const char *)s_ap_records[i].ssid,
                 (int)((s_ap_records[i].rssi + 100) * 2));
        if (strcmp(text, info) == 0) { show_password_input(i); return; }
        snprintf(info, sizeof(info), "%s  — 已连接", (const char *)s_ap_records[i].ssid);
        if (strcmp(text, info) == 0) { show_password_input(i); return; }
    }
}

/* ── 自动连接任务 ── */
static void auto_connect_task(void *pv)
{
    (void)pv;
    char ssid[33] = "", pwd[65] = "";
    if (!wifi_nvs_load(0, ssid, sizeof(ssid), pwd, sizeof(pwd))) {
        ESP_LOGI(TAG, "No saved WiFi credentials");
        s_auto_connecting = false;
        if (!s_scanning) {
            s_scanning = true;
            xTaskCreatePinnedToCore(wifi_scan_task, "wifi_scan", 4*1024, NULL, 3, NULL, 1);
        }
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "Auto-connecting to: %s", ssid);
    lvgl_port_lock(0);
    if (s_conn_label) lv_label_set_text(s_conn_label, "正在连接已保存网络...");
    lvgl_port_unlock();
    wifi_config_t cfg = { .sta = { .threshold.authmode = WIFI_AUTH_WPA2_PSK }};
    strncpy((char *)cfg.sta.ssid, ssid, sizeof(cfg.sta.ssid)-1);
    strncpy((char *)cfg.sta.password, pwd, sizeof(cfg.sta.password)-1);
    esp_wifi_set_config(WIFI_IF_STA, &cfg);
    esp_wifi_connect();
    s_auto_connecting = true; s_retry_count = 0;
    strncpy(s_connected_ssid, ssid, sizeof(s_connected_ssid)-1);
    strncpy(s_pending_password, pwd, sizeof(s_pending_password)-1);
    vTaskDelete(NULL);
}

/* ── 页面 on_enter / on_exit ── */
static void page_wifi_on_enter(void)
{
    lv_obj_t *container = page_manager_get_container();
    if (!container) return;
    ensure_wifi_stack();
    lvgl_port_lock(0);
    s_scan_label = lv_label_create(container);
    lv_obj_set_style_text_font(s_scan_label, &font_alipuhui20, 0);
    lv_obj_set_style_text_color(s_scan_label, lv_color_hex(0x000000), 0);
    lv_obj_align(s_scan_label, LV_ALIGN_TOP_MID, 0, 42);
    lv_obj_t *btn = lv_btn_create(container);
    lv_obj_align(btn, LV_ALIGN_TOP_RIGHT, -5, 42);
    lv_obj_set_size(btn, 50, 30);
    lv_obj_add_event_cb(btn, refresh_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lab = lv_label_create(btn);
    lv_label_set_text(lab, LV_SYMBOL_REFRESH);
    lv_obj_set_style_text_font(lab, &lv_font_montserrat_20, 0); lv_obj_center(lab);
    lv_obj_t *btn_forget = lv_btn_create(container);
    lv_obj_align(btn_forget, LV_ALIGN_TOP_LEFT, 5, 42);
    lv_obj_set_size(btn_forget, 80, 30);
    lv_obj_add_event_cb(btn_forget, forget_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lab_forget = lv_label_create(btn_forget);
    lv_label_set_text(lab_forget, "忘记");
    lv_obj_set_style_text_font(lab_forget, &font_alipuhui20, 0); lv_obj_center(lab_forget);
    s_conn_label = lv_label_create(container);
    lv_obj_set_style_text_font(s_conn_label, &font_alipuhui20, 0);
    lv_obj_set_style_text_color(s_conn_label, lv_color_hex(0x006600), 0);
    lv_obj_align(s_conn_label, LV_ALIGN_TOP_LEFT, 5, 60);
    s_wifi_list = lv_list_create(container);
    lv_obj_set_size(s_wifi_list, 310, 140);
    lv_obj_align(s_wifi_list, LV_ALIGN_TOP_LEFT, 5, 80);
    lv_obj_set_style_border_width(s_wifi_list, 0, 0);
    lv_obj_set_style_text_font(s_wifi_list, &font_alipuhui20, 0);
    lv_obj_set_scrollbar_mode(s_wifi_list, LV_SCROLLBAR_MODE_OFF);
    if (s_connected && s_last_ip[0])
        lv_label_set_text_fmt(s_conn_label, "IP: %s", s_last_ip);
    lvgl_port_unlock();
    if (s_connected) {
        s_scanning = true;
        xTaskCreatePinnedToCore(wifi_scan_task, "wifi_scan", 4*1024, NULL, 3, NULL, 1);
    } else if (wifi_nvs_has_saved()) {
        xTaskCreatePinnedToCore(auto_connect_task, "auto_conn", 4*1024, NULL, 3, NULL, 1);
    } else {
        s_scanning = true;
        xTaskCreatePinnedToCore(wifi_scan_task, "wifi_scan", 4*1024, NULL, 3, NULL, 1);
    }
}

static void page_wifi_on_exit(void)
{
    int t = 50;
    while ((s_scanning || s_connecting || s_auto_connecting) && t-- > 0)
        vTaskDelay(pdMS_TO_TICKS(100));
    if (s_pwd_page) { lv_obj_del(s_pwd_page); s_pwd_page = NULL; }
    if (s_connected) {
        s_scan_label = NULL; s_wifi_list = NULL; s_conn_label = NULL;
        return;
    }
    esp_wifi_stop(); esp_wifi_deinit();
    esp_netif_destroy(s_sta_netif); s_sta_netif = NULL;
    esp_event_loop_delete_default(); s_wifi_initialized = false;
    s_scan_label = NULL; s_wifi_list = NULL; s_conn_label = NULL;
    s_connected_ssid[0] = '\0';
}

void page_wifi_register(void)
{
    page_config_t c = {
        .id = PAGE_WIFI, .title = "WiFi", .label = "WiFi",
        .title_bg_color = LV_COLOR_MAKE(0xcd, 0x5c, 0x5c),
        .enabled = true, .icon = NULL,
        .on_enter = page_wifi_on_enter, .on_exit = page_wifi_on_exit,
    };
    page_manager_register(&c);
}
