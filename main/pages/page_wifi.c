#include "page_wifi.h"
#include "page_manager.h"
#include "app_event.h"
#include "wifi_manager.h"
#include "esp32_s3_szp.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "page_wifi";

/* ── UI 控件 ── */
static lv_obj_t *s_status_label  = NULL;  /* 顶部状态文字 */
static lv_obj_t *s_ip_label      = NULL;  /* IP 地址显示 */
static lv_obj_t *s_wifi_list     = NULL;  /* 扫描列表 */
static lv_obj_t *s_connecting_label = NULL; /* 连接中提示 */

/* ── 密码输入覆盖层 ── */
static lv_obj_t *s_pwd_page      = NULL;
static lv_obj_t *s_pwd_textarea  = NULL;
static lv_obj_t *s_pwd_ssid_label = NULL;
static int        s_pwd_ap_index = -1;

/* ── 轮询定时器 ── */
static lv_timer_t *s_poll_timer = NULL;

/* ========== 辅助函数 ========== */

/** RSSI 转百分比 (0-100) */
static int rssi_to_pct(int8_t rssi)
{
    if (rssi >= -50) return 100;
    if (rssi <= -100) return 0;
    return (rssi + 100) * 2;
}

/** RSSI 转信号强度图标 */
static const char *rssi_to_symbol(int8_t rssi)
{
    int pct = rssi_to_pct(rssi);
    if (pct >= 80) return LV_SYMBOL_WIFI;
    if (pct >= 40) return LV_SYMBOL_WIFI " ";  /* 降一级显示 */
    if (pct >= 15) return LV_SYMBOL_WIFI "  ";
    return LV_SYMBOL_WIFI "   ";
}

/** 刷新扫描列表 */
static void refresh_ap_list(void)
{
    if (!s_wifi_list) return;

    lvgl_port_lock(0);
    lv_obj_clean(s_wifi_list);

    int count = wifi_manager_get_ap_count();
    const char *conn_ssid = wifi_manager_get_ssid();
    bool is_connected = (wifi_manager_get_state() == WIFI_STATE_CONNECTED);

    for (int i = 0; i < count; i++) {
        wifi_ap_info_t ap;
        if (!wifi_manager_get_ap(i, &ap)) continue;

        char info[64];
        int pct = rssi_to_pct(ap.rssi);
        bool is_conn = is_connected && conn_ssid[0] &&
                       strcmp(ap.ssid, conn_ssid) == 0;

        if (is_conn) {
            snprintf(info, sizeof(info), "%s%s  — %s",
                     rssi_to_symbol(ap.rssi), ap.ssid, "已连接");
        } else {
            snprintf(info, sizeof(info), "%s%s  (%d%%)",
                     rssi_to_symbol(ap.rssi), ap.ssid, pct);
        }

        lv_obj_t *btn = lv_list_add_btn(s_wifi_list, NULL, info);
        /* 把 AP 索引存到 user_data */
        lv_obj_add_event_cb(btn, list_item_cb, LV_EVENT_CLICKED,
                            (void *)(uintptr_t)i);
        lv_obj_set_style_text_font(lv_obj_get_child(btn, 0),
                                   &lv_font_montserrat_14, 0);
    }

    if (count == 0 && s_status_label) {
        lv_label_set_text(s_status_label, "没有找到 WiFi 网络");
    }

    lvgl_port_unlock();
}

/* ========== 密码输入界面 ========== */

static void pwd_back_cb(lv_event_t *e) { (void)e; return; }
static void pwd_del_cb(lv_event_t *e)
{
    (void)e;
    lv_textarea_del_char(s_pwd_textarea);
}
static void pwd_connect_cb(lv_event_t *e);

static lv_obj_t *roller_num, *roller_low, *roller_up;

static void roller_ok_cb(lv_event_t *e)
{
    lv_obj_t *roller = lv_event_get_user_data(e);
    char buf[2];
    lv_roller_get_selected_str(roller, buf, sizeof(buf));
    lv_textarea_add_text(s_pwd_textarea, buf);
}

static void show_password_input(int ap_index)
{
    wifi_ap_info_t ap;
    if (!wifi_manager_get_ap(ap_index, &ap)) return;

    lvgl_port_lock(0);

    s_pwd_page = lv_obj_create(lv_scr_act());
    lv_obj_set_size(s_pwd_page, 320, 240);
    lv_obj_set_style_border_width(s_pwd_page, 0, 0);
    lv_obj_set_style_pad_all(s_pwd_page, 0, 0);
    lv_obj_set_style_radius(s_pwd_page, 0, 0);
    lv_obj_set_style_bg_color(s_pwd_page, lv_color_hex(0x1a1a2e), 0);

    /* 返回按钮 */
    lv_obj_t *btn_back = lv_btn_create(s_pwd_page);
    lv_obj_align(btn_back, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_size(btn_back, 60, 40);
    lv_obj_set_style_bg_opa(btn_back, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(btn_back, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_add_event_cb(btn_back, pwd_back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lab_back = lv_label_create(btn_back);
    lv_label_set_text(lab_back, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(lab_back, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_text_font(lab_back, &lv_font_montserrat_20, 0);
    lv_obj_center(lab_back);

    /* SSID 标题 */
    s_pwd_ssid_label = lv_label_create(s_pwd_page);
    lv_obj_set_style_text_font(s_pwd_ssid_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_pwd_ssid_label, lv_color_hex(0xffffff), 0);
    lv_label_set_text(s_pwd_ssid_label, ap.ssid);
    lv_obj_align(s_pwd_ssid_label, LV_ALIGN_TOP_MID, 0, 10);

    /* 密码输入框 */
    s_pwd_textarea = lv_textarea_create(s_pwd_page);
    lv_textarea_set_one_line(s_pwd_textarea, true);
    lv_textarea_set_placeholder_text(s_pwd_textarea, "Password");
    lv_obj_set_width(s_pwd_textarea, 150);
    lv_obj_align(s_pwd_textarea, LV_ALIGN_TOP_LEFT, 10, 45);
    lv_obj_add_state(s_pwd_textarea, LV_STATE_FOCUSED);
    lv_obj_set_style_text_font(s_pwd_textarea, &lv_font_montserrat_14, 0);

    /* 自动填入已保存密码 */
    char saved_pwd[WIFI_MANAGER_PWD_MAX] = "";
    if (wifi_manager_cred_find(ap.ssid, saved_pwd, sizeof(saved_pwd))) {
        lv_textarea_set_text(s_pwd_textarea, saved_pwd);
    }

    /* 连接按钮 */
    lv_obj_t *btn_ok = lv_btn_create(s_pwd_page);
    lv_obj_align(btn_ok, LV_ALIGN_TOP_LEFT, 170, 45);
    lv_obj_set_size(btn_ok, 60, 36);
    lv_obj_set_style_text_font(btn_ok, &lv_font_montserrat_14, 0);
    lv_obj_add_event_cb(btn_ok, pwd_connect_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lab_ok = lv_label_create(btn_ok);
    lv_label_set_text(lab_ok, "OK");
    lv_obj_set_style_text_font(lab_ok, &lv_font_montserrat_14, 0);
    lv_obj_center(lab_ok);

    /* 删除按钮 */
    lv_obj_t *btn_del = lv_btn_create(s_pwd_page);
    lv_obj_align(btn_del, LV_ALIGN_TOP_LEFT, 240, 45);
    lv_obj_set_size(btn_del, 60, 36);
    lv_obj_add_event_cb(btn_del, pwd_del_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lab_del = lv_label_create(btn_del);
    lv_label_set_text(lab_del, LV_SYMBOL_BACKSPACE);
    lv_obj_set_style_text_font(lab_del, &lv_font_montserrat_14, 0);
    lv_obj_center(lab_del);

    /* ── 三个滚轮选择器（数字/小写/大写） ── */
    static lv_style_t roller_style;
    lv_style_init(&roller_style);
    lv_style_set_bg_color(&roller_style, lv_color_hex(0x16213e));
    lv_style_set_text_color(&roller_style, lv_color_hex(0xffffff));
    lv_style_set_border_width(&roller_style, 0);
    lv_style_set_pad_all(&roller_style, 0);

    /* 数字滚轮 */
    roller_num = lv_roller_create(s_pwd_page);
    lv_obj_add_style(roller_num, &roller_style, 0);
    lv_roller_set_options(roller_num, "0\n1\n2\n3\n4\n5\n6\n7\n8\n9",
                          LV_ROLLER_MODE_INFINITE);
    lv_roller_set_visible_row_count(roller_num, 3);
    lv_obj_set_width(roller_num, 90);
    lv_obj_align(roller_num, LV_ALIGN_BOTTOM_LEFT, 15, -53);
    lv_obj_t *bn = lv_btn_create(s_pwd_page);
    lv_obj_align(bn, LV_ALIGN_BOTTOM_LEFT, 15, -10);
    lv_obj_set_size(bn, 90, 30);
    lv_obj_add_event_cb(bn, roller_ok_cb, LV_EVENT_CLICKED, roller_num);
    lv_obj_t *ln = lv_label_create(bn);
    lv_label_set_text(ln, LV_SYMBOL_OK); lv_obj_center(ln);

    /* 小写字母滚轮 */
    roller_low = lv_roller_create(s_pwd_page);
    lv_obj_add_style(roller_low, &roller_style, 0);
    lv_roller_set_options(roller_low,
        "a\nb\nc\nd\ne\nf\ng\nh\ni\nj\nk\nl\nm\n"
        "n\no\np\nq\nr\ns\nt\nu\nv\nw\nx\ny\nz",
        LV_ROLLER_MODE_INFINITE);
    lv_roller_set_visible_row_count(roller_low, 3);
    lv_obj_set_width(roller_low, 90);
    lv_obj_align(roller_low, LV_ALIGN_BOTTOM_LEFT, 115, -53);
    bn = lv_btn_create(s_pwd_page);
    lv_obj_align(bn, LV_ALIGN_BOTTOM_LEFT, 115, -10);
    lv_obj_set_size(bn, 90, 30);
    lv_obj_add_event_cb(bn, roller_ok_cb, LV_EVENT_CLICKED, roller_low);
    ln = lv_label_create(bn); lv_label_set_text(ln, LV_SYMBOL_OK); lv_obj_center(ln);

    /* 大写字母滚轮 */
    roller_up = lv_roller_create(s_pwd_page);
    lv_obj_add_style(roller_up, &roller_style, 0);
    lv_roller_set_options(roller_up,
        "A\nB\nC\nD\nE\nF\nG\nH\nI\nJ\nK\nL\nM\n"
        "N\nO\nP\nQ\nR\nS\nT\nU\nV\nW\nX\nY\nZ",
        LV_ROLLER_MODE_INFINITE);
    lv_roller_set_visible_row_count(roller_up, 3);
    lv_obj_set_width(roller_up, 90);
    lv_obj_align(roller_up, LV_ALIGN_BOTTOM_LEFT, 215, -53);
    bn = lv_btn_create(s_pwd_page);
    lv_obj_align(bn, LV_ALIGN_BOTTOM_LEFT, 215, -10);
    lv_obj_set_size(bn, 90, 30);
    lv_obj_add_event_cb(bn, roller_ok_cb, LV_EVENT_CLICKED, roller_up);
    ln = lv_label_create(bn); lv_label_set_text(ln, LV_SYMBOL_OK); lv_obj_center(ln);

    s_pwd_ap_index = ap_index;

    lvgl_port_unlock();
}

static void close_password_input(void)
{
    if (s_pwd_page) {
        lvgl_port_lock(0);
        lv_obj_del(s_pwd_page);
        s_pwd_page = NULL;
        s_pwd_textarea = NULL;
        s_pwd_ssid_label = NULL;
        lvgl_port_unlock();
    }
}

/* ========== 回调 ========== */

/** 扫描列表项点击 */
static void list_item_cb(lv_event_t *e)
{
    int ap_index = (int)(uintptr_t)lv_event_get_user_data(e);
    if (ap_index < 0 || ap_index >= wifi_manager_get_ap_count()) return;
    show_password_input(ap_index);
}

/** 密码页连接按钮 */
static void pwd_connect_cb(lv_event_t *e)
{
    (void)e;
    const char *ssid = lv_label_get_text(s_pwd_ssid_label);
    const char *pwd  = lv_textarea_get_text(s_pwd_textarea);

    wifi_manager_connect(ssid, pwd, NULL);

    close_password_input();

    lvgl_port_lock(0);
    if (s_connecting_label) {
        lv_label_set_text_fmt(s_connecting_label, "连接中: %s...", ssid);
    }
    lvgl_port_unlock();
}

/** 删除按钮（密码页外层的独立返回处理） */
static void pwd_back_cb(lv_event_t *e)
{
    (void)e;
    close_password_input();
}

/** 刷新按钮 */
static void refresh_btn_cb(lv_event_t *e)
{
    (void)e;
    lvgl_port_lock(0);
    if (s_status_label) lv_label_set_text(s_status_label, "正在扫描 WiFi...");
    lvgl_port_unlock();

    wifi_manager_scan_start(on_scan_done);
}

/** 忘记网络按钮 */
static void forget_btn_cb(lv_event_t *e)
{
    (void)e;
    wifi_manager_disconnect();
    wifi_manager_cred_clear();
    wifi_manager_deinit();

    /* 重新初始化 */
    wifi_manager_init();

    lvgl_port_lock(0);
    if (s_ip_label) lv_label_set_text(s_ip_label, "");
    if (s_status_label) lv_label_set_text(s_status_label, "已清除保存网络");
    if (s_connecting_label) lv_label_set_text(s_connecting_label, "");
    if (s_wifi_list) lv_obj_clean(s_wifi_list);
    lvgl_port_unlock();
}

/* ── WiFi manager 回调 ── */

static void on_scan_done(int ap_count, const wifi_ap_info_t *ap_list)
{
    (void)ap_list;
    lvgl_port_lock(0);
    if (s_status_label) {
        if (ap_count > 0) {
            lv_label_set_text_fmt(s_status_label, "发现 %d 个 WiFi 网络", ap_count);
        } else {
            lv_label_set_text(s_status_label, "没有找到 WiFi 网络");
        }
    }
    lvgl_port_unlock();
    refresh_ap_list();
}

/* ========== 轮询更新 ========== */

static void poll_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    wifi_manager_state_t state = wifi_manager_get_state();

    lvgl_port_lock(0);

    /* 更新 IP 显示 */
    if (s_ip_label) {
        const char *ip = wifi_manager_get_ip();
        if (ip[0]) {
            lv_label_set_text_fmt(s_ip_label, "IP: %s", ip);
        }
    }

    /* 更新连接状态提示 */
    if (s_connecting_label) {
        switch (state) {
        case WIFI_STATE_CONNECTING:
            if (lv_label_get_text(s_connecting_label)[0] == '\0') {
                lv_label_set_text(s_connecting_label, "连接中...");
            }
            break;
        case WIFI_STATE_CONNECTED:
            lv_label_set_text_fmt(s_connecting_label, "已连接: %s",
                                  wifi_manager_get_ssid());
            break;
        case WIFI_STATE_DISCONNECTED:
            lv_label_set_text(s_connecting_label, "连接失败");
            break;
        default:
            break;
        }
    }

    /* 更新扫描列表（连接状态变化时刷新信号标记） */
    if (state == WIFI_STATE_CONNECTED || state == WIFI_STATE_DISCONNECTED) {
        if (s_wifi_list && wifi_manager_get_ap_count() > 0) {
            refresh_ap_list();
        }
    }

    lvgl_port_unlock();
}

/* ========== 页面生命周期 ========== */

static void page_wifi_on_enter(void)
{
    lv_obj_t *container = page_manager_get_container();
    if (!container) return;

    /* 初始化 WiFi 栈 */
    wifi_manager_init();

    lvgl_port_lock(0);

    /* ── 顶部工具栏 ── */
    /* 忘记按钮（左上角） */
    lv_obj_t *btn_forget = lv_btn_create(container);
    lv_obj_align(btn_forget, LV_ALIGN_TOP_LEFT, 5, 45);
    lv_obj_set_size(btn_forget, 70, 28);
    lv_obj_set_style_border_width(btn_forget, 0, 0);
    lv_obj_add_event_cb(btn_forget, forget_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lab_forget = lv_label_create(btn_forget);
    lv_label_set_text(lab_forget, "忘记");
    lv_obj_set_style_text_font(lab_forget, &lv_font_montserrat_14, 0);
    lv_obj_center(lab_forget);

    /* 状态标签（顶部中） */
    s_status_label = lv_label_create(container);
    lv_obj_set_style_text_color(s_status_label, lv_color_hex(0xcccccc), 0);
    lv_obj_set_style_text_font(s_status_label, &lv_font_montserrat_14, 0);
    lv_label_set_text(s_status_label, "");
    lv_obj_align(s_status_label, LV_ALIGN_TOP_MID, 0, 50);

    /* 刷新按钮（右上角） */
    lv_obj_t *btn_refresh = lv_btn_create(container);
    lv_obj_align(btn_refresh, LV_ALIGN_TOP_RIGHT, -5, 45);
    lv_obj_set_size(btn_refresh, 55, 28);
    lv_obj_set_style_border_width(btn_refresh, 0, 0);
    lv_obj_add_event_cb(btn_refresh, refresh_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lab_refresh = lv_label_create(btn_refresh);
    lv_label_set_text(lab_refresh, LV_SYMBOL_REFRESH);
    lv_obj_set_style_text_font(lab_refresh, &lv_font_montserrat_20, 0);
    lv_obj_center(lab_refresh);

    /* IP 地址显示 */
    s_ip_label = lv_label_create(container);
    lv_obj_set_style_text_color(s_ip_label, lv_color_hex(0x00cc66), 0);
    lv_obj_set_style_text_font(s_ip_label, &lv_font_montserrat_14, 0);
    if (wifi_manager_get_state() == WIFI_STATE_CONNECTED) {
        lv_label_set_text_fmt(s_ip_label, "IP: %s", wifi_manager_get_ip());
    } else {
        lv_label_set_text(s_ip_label, "");
    }
    lv_obj_align(s_ip_label, LV_ALIGN_TOP_LEFT, 5, 80);

    /* 连接中提示 */
    s_connecting_label = lv_label_create(container);
    lv_obj_set_style_text_color(s_connecting_label, lv_color_hex(0xffcc00), 0);
    lv_obj_set_style_text_font(s_connecting_label, &lv_font_montserrat_14, 0);
    lv_label_set_text(s_connecting_label, "");
    lv_obj_align(s_connecting_label, LV_ALIGN_TOP_MID, 0, 80);

    /* ── AP 列表 ── */
    s_wifi_list = lv_list_create(container);
    lv_obj_set_size(s_wifi_list, 310, 130);
    lv_obj_align(s_wifi_list, LV_ALIGN_TOP_LEFT, 5, 100);
    lv_obj_set_style_border_width(s_wifi_list, 0, 0);
    lv_obj_set_style_text_font(s_wifi_list, &lv_font_montserrat_14, 0);
    lv_obj_set_scrollbar_mode(s_wifi_list, LV_SCROLLBAR_MODE_OFF);

    lvgl_port_unlock();

    /* 启动轮询定时器（每 500ms 更新一次状态） */
    s_poll_timer = lv_timer_create(poll_timer_cb, 500, NULL);

    /* 已连接：刷新扫描列表 */
    if (wifi_manager_get_state() == WIFI_STATE_CONNECTED) {
        wifi_manager_scan_start(on_scan_done);
    }
    /* 有保存凭据：尝试自动连接 */
    else if (wifi_manager_has_saved()) {
        lvgl_port_lock(0);
        if (s_status_label) lv_label_set_text(s_status_label, "正在连接已保存网络...");
        lvgl_port_unlock();
        wifi_manager_auto_connect(NULL);
        /* 自动连接后也触发扫描 */
        wifi_manager_scan_start(on_scan_done);
    }
    /* 无凭据：直接扫描 */
    else {
        wifi_manager_scan_start(on_scan_done);
    }
}

static void page_wifi_on_exit(void)
{
    /* 停止轮询定时器 */
    if (s_poll_timer) {
        lv_timer_del(s_poll_timer);
        s_poll_timer = NULL;
    }

    /* 关闭密码输入覆盖层 */
    close_password_input();

    /* 取消扫描 */
    wifi_manager_scan_stop();

    /* 未连接时释放 WiFi 资源 */
    if (wifi_manager_get_state() != WIFI_STATE_CONNECTED) {
        wifi_manager_deinit();
    }

    /* 清空 UI 引用 */
    s_status_label = NULL;
    s_ip_label = NULL;
    s_wifi_list = NULL;
    s_connecting_label = NULL;

    ESP_LOGI(TAG, "WiFi page exited");
}

/* ========== 注册入口 ========== */

void page_wifi_register(void)
{
    page_config_t cfg = {
        .id             = PAGE_WIFI,
        .title          = "WiFi",
        .label          = "WiFi",
        .title_bg_color = LV_COLOR_MAKE(0xcd, 0x5c, 0x5c),
        .enabled        = true,
        .icon           = NULL,
        .on_enter       = page_wifi_on_enter,
        .on_exit        = page_wifi_on_exit,
    };
    page_manager_register(&cfg);
    ESP_LOGI(TAG, "WiFi page registered");
}