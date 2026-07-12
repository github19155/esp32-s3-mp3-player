#include "wifi_manager.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "wifi_mgr";

#define NVS_NAMESPACE "wifi_cfg"

/* ── 内部状态 ── */
static bool                  s_initialized       = false;
static esp_netif_t          *s_sta_netif         = NULL;
static esp_event_handler_instance_t s_wifi_evt   = NULL;
static esp_event_handler_instance_t s_ip_evt     = NULL;

static wifi_manager_state_t  s_state             = WIFI_STATE_IDLE;
static char                  s_connected_ssid[WIFI_MANAGER_SSID_MAX] = "";
static char                  s_ip[16]            = "";

/* ── 扫描状态 ── */
static bool                  s_scanning          = false;
static wifi_ap_info_t        s_ap_list[WIFI_MANAGER_SCAN_MAX];
static int                   s_ap_count          = 0;
static wifi_scan_done_cb_t   s_scan_cb           = NULL;

/* ── 连接状态 ── */
static bool                  s_connecting        = false;
static bool                  s_auto_connecting   = false;
static int                   s_retry_count       = 0;
static wifi_connect_cb_t     s_connect_cb        = NULL;
static char                  s_pending_pwd[WIFI_MANAGER_PWD_MAX] = "";

#define WIFI_MAX_RETRY 3

/* ========== NVS 凭据操作 ========== */

int wifi_manager_cred_count(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &h) != ESP_OK) return 0;
    uint8_t n = 0;
    nvs_get_u8(h, "net_count", &n);
    nvs_close(h);
    return (int)n;
}

bool wifi_manager_has_saved(void)
{
    return wifi_manager_cred_count() > 0;
}

bool wifi_manager_cred_load(int idx, char *ssid_out, size_t ssid_len,
                             char *pwd_out, size_t pwd_len)
{
    nvs_handle_t h;
    char key[24];
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &h) != ESP_OK) return false;

    snprintf(key, sizeof(key), "net%d_ssid", idx);
    size_t l = ssid_len;
    esp_err_t r = nvs_get_str(h, key, ssid_out, &l);
    if (r != ESP_OK) { nvs_close(h); return false; }

    snprintf(key, sizeof(key), "net%d_pwd", idx);
    l = pwd_len;
    r = nvs_get_str(h, key, pwd_out, &l);
    nvs_close(h);
    return (r == ESP_OK);
}

bool wifi_manager_cred_find(const char *ssid, char *pwd_out, size_t pwd_len)
{
    int n = wifi_manager_cred_count();
    char saved_ssid[WIFI_MANAGER_SSID_MAX], saved_pwd[WIFI_MANAGER_PWD_MAX];
    for (int i = 0; i < n; i++) {
        if (wifi_manager_cred_load(i, saved_ssid, sizeof(saved_ssid),
                                   saved_pwd, sizeof(saved_pwd)) &&
            strcmp(saved_ssid, ssid) == 0) {
            strncpy(pwd_out, saved_pwd, pwd_len - 1);
            pwd_out[pwd_len - 1] = '\0';
            return true;
        }
    }
    return false;
}

bool wifi_manager_cred_save(const char *ssid, const char *password)
{
    /* 检查是否已存在且密码相同 */
    char old_pwd[WIFI_MANAGER_PWD_MAX];
    if (wifi_manager_cred_find(ssid, old_pwd, sizeof(old_pwd))) {
        if (strcmp(old_pwd, password) == 0) return true; /* 无需更新 */
    }

    int idx = wifi_manager_cred_count();
    if (idx >= WIFI_MANAGER_NVS_MAX) idx = 0; /* 循环覆盖 */

    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) != ESP_OK) return false;

    char key[24];
    snprintf(key, sizeof(key), "net%d_ssid", idx);
    nvs_set_str(h, key, ssid);
    snprintf(key, sizeof(key), "net%d_pwd", idx);
    nvs_set_str(h, key, password);

    int cur = wifi_manager_cred_count();
    if (idx >= cur) nvs_set_u8(h, "net_count", (uint8_t)(idx + 1));

    nvs_commit(h);
    nvs_close(h);
    ESP_LOGI(TAG, "Credential saved[%d]: %s", idx, ssid);
    return true;
}

void wifi_manager_cred_clear(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) != ESP_OK) return;

    char key[24];
    for (int i = 0; i < WIFI_MANAGER_NVS_MAX; i++) {
        snprintf(key, sizeof(key), "net%d_ssid", i); nvs_erase_key(h, key);
        snprintf(key, sizeof(key), "net%d_pwd", i);  nvs_erase_key(h, key);
    }
    nvs_set_u8(h, "net_count", 0);
    nvs_commit(h);
    nvs_close(h);
    ESP_LOGI(TAG, "All credentials cleared");
}

/* ========== 事件回调 ========== */

static void wifi_event_cb(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;

    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        s_state = WIFI_STATE_IDLE;
        return;
    }

    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *d = (wifi_event_sta_disconnected_t *)data;
        ESP_LOGW(TAG, "Disconnected, reason=%d", d->reason);

        if (s_auto_connecting && s_retry_count < WIFI_MAX_RETRY) {
            esp_wifi_connect();
            s_retry_count++;
            return;
        }
        if (s_auto_connecting) {
            s_auto_connecting = false;
            s_connected_ssid[0] = '\0';
            s_pending_pwd[0] = '\0';
            s_state = WIFI_STATE_DISCONNECTED;
            if (s_connect_cb) {
                s_connect_cb(false, s_connected_ssid, "");
                s_connect_cb = NULL;
            }
            return;
        }

        if (s_connecting && s_retry_count < WIFI_MAX_RETRY) {
            esp_wifi_connect();
            s_retry_count++;
            return;
        }
        if (s_connecting) {
            s_connecting = false;
            s_connected_ssid[0] = '\0';
            s_pending_pwd[0] = '\0';
            s_state = WIFI_STATE_DISCONNECTED;
            if (s_connect_cb) {
                s_connect_cb(false, s_connected_ssid, "");
                s_connect_cb = NULL;
            }
            return;
        }
    }

    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *ev = (ip_event_got_ip_t *)data;
        s_connecting = false;
        s_auto_connecting = false;
        s_state = WIFI_STATE_CONNECTED;
        snprintf(s_ip, sizeof(s_ip), IPSTR, IP2STR(&ev->ip_info.ip));
        ESP_LOGI(TAG, "Got IP: %s", s_ip);

        /* 保存凭据 */
        if (s_pending_pwd[0]) {
            wifi_manager_cred_save(s_connected_ssid, s_pending_pwd);
            s_pending_pwd[0] = '\0';
        }

        if (s_connect_cb) {
            s_connect_cb(true, s_connected_ssid, s_ip);
            s_connect_cb = NULL;
        }
    }
}

/* ========== 初始化 / 去初始化 ========== */

void wifi_manager_init(void)
{
    if (s_initialized) return;

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    s_sta_netif = esp_netif_create_default_wifi_sta();
    assert(s_sta_netif);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_cb, NULL, &s_wifi_evt));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_cb, NULL, &s_ip_evt));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    s_initialized = true;
    s_state = WIFI_STATE_IDLE;
    ESP_LOGI(TAG, "WiFi manager initialized");
}

void wifi_manager_deinit(void)
{
    if (!s_initialized) return;

    esp_wifi_stop();
    esp_wifi_deinit();
    esp_netif_destroy(s_sta_netif);
    s_sta_netif = NULL;
    esp_event_loop_delete_default();
    s_wifi_evt = NULL;
    s_ip_evt = NULL;

    s_initialized = false;
    s_state = WIFI_STATE_IDLE;
    s_connected_ssid[0] = '\0';
    s_ip[0] = '\0';
    ESP_LOGI(TAG, "WiFi manager deinitialized");
}

/* ========== 扫描 ========== */

static void scan_task(void *pv)
{
    (void)pv;
    s_state = WIFI_STATE_SCANNING;

    /* 执行阻塞扫描 */
    wifi_scan_config_t scan_cfg = { 0 };
    if (esp_wifi_scan_start(&scan_cfg, true) != ESP_OK) {
        ESP_LOGE(TAG, "Scan start failed");
        s_state = WIFI_STATE_ERROR;
        s_scanning = false;
        if (s_scan_cb) { s_scan_cb(0, NULL); s_scan_cb = NULL; }
        vTaskDelete(NULL);
        return;
    }

    uint16_t num = WIFI_MANAGER_SCAN_MAX;
    wifi_ap_record_t raw[WIFI_MANAGER_SCAN_MAX];
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&num, raw));

    s_ap_count = (int)num;
    for (int i = 0; i < s_ap_count; i++) {
        strncpy(s_ap_list[i].ssid, (const char *)raw[i].ssid,
                WIFI_MANAGER_SSID_MAX - 1);
        s_ap_list[i].ssid[WIFI_MANAGER_SSID_MAX - 1] = '\0';
        s_ap_list[i].rssi     = raw[i].rssi;
        s_ap_list[i].authmode = raw[i].authmode;
    }

    /* 按 RSSI 降序排序 */
    for (int i = 0; i < s_ap_count - 1; i++) {
        for (int j = i + 1; j < s_ap_count; j++) {
            if (s_ap_list[j].rssi > s_ap_list[i].rssi) {
                wifi_ap_info_t tmp = s_ap_list[i];
                s_ap_list[i] = s_ap_list[j];
                s_ap_list[j] = tmp;
            }
        }
    }

    if (s_state != WIFI_STATE_CONNECTED) {
        s_state = WIFI_STATE_IDLE;
    }

    if (s_scan_cb) {
        s_scan_cb(s_ap_count, s_ap_list);
        s_scan_cb = NULL;
    }

    s_scanning = false;
    ESP_LOGI(TAG, "Scan done: %d APs", s_ap_count);
    vTaskDelete(NULL);
}

void wifi_manager_scan_start(wifi_scan_done_cb_t callback)
{
    if (!s_initialized) return;
    if (s_scanning) return;

    s_scan_cb = callback;
    s_scanning = true;
    xTaskCreatePinnedToCore(scan_task, "wifi_scan", 4 * 1024,
                            NULL, 3, NULL, 1);
}

void wifi_manager_scan_stop(void)
{
    s_scanning = false;
    s_scan_cb = NULL;
}

/* ========== 连接 ========== */

void wifi_manager_connect(const char *ssid, const char *password,
                          wifi_connect_cb_t connect_cb)
{
    if (!s_initialized) return;

    if (s_state == WIFI_STATE_CONNECTED) {
        esp_wifi_disconnect();
    }

    wifi_config_t cfg = { 0 };
    cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    strncpy((char *)cfg.sta.ssid, ssid, sizeof(cfg.sta.ssid) - 1);
    strncpy((char *)cfg.sta.password, password, sizeof(cfg.sta.password) - 1);

    esp_wifi_set_config(WIFI_IF_STA, &cfg);
    esp_wifi_connect();

    s_connecting = true;
    s_auto_connecting = false;
    s_retry_count = 0;
    s_connect_cb = connect_cb;
    s_state = WIFI_STATE_CONNECTING;
    strncpy(s_connected_ssid, ssid, WIFI_MANAGER_SSID_MAX - 1);
    strncpy(s_pending_pwd, password, WIFI_MANAGER_PWD_MAX - 1);

    ESP_LOGI(TAG, "Connecting to: %s", ssid);
}

void wifi_manager_disconnect(void)
{
    if (!s_initialized) return;
    if (s_state != WIFI_STATE_CONNECTED) return;

    esp_wifi_disconnect();
    s_state = WIFI_STATE_DISCONNECTED;
    s_connected_ssid[0] = '\0';
    s_ip[0] = '\0';
    ESP_LOGI(TAG, "Disconnected");
}

/* ========== 自动连接 ========== */

static void auto_connect_task(void *pv)
{
    (void)pv;
    char ssid[WIFI_MANAGER_SSID_MAX] = "";
    char pwd[WIFI_MANAGER_PWD_MAX] = "";

    if (!wifi_manager_cred_load(0, ssid, sizeof(ssid), pwd, sizeof(pwd))) {
        ESP_LOGI(TAG, "No saved credentials for auto-connect");
        s_auto_connecting = false;
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "Auto-connecting to: %s", ssid);

    wifi_config_t cfg = { 0 };
    cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    strncpy((char *)cfg.sta.ssid, ssid, sizeof(cfg.sta.ssid) - 1);
    strncpy((char *)cfg.sta.password, pwd, sizeof(cfg.sta.password) - 1);

    esp_wifi_set_config(WIFI_IF_STA, &cfg);
    esp_wifi_connect();

    s_auto_connecting = true;
    s_retry_count = 0;
    s_state = WIFI_STATE_CONNECTING;
    strncpy(s_connected_ssid, ssid, WIFI_MANAGER_SSID_MAX - 1);
    strncpy(s_pending_pwd, pwd, WIFI_MANAGER_PWD_MAX - 1);

    vTaskDelete(NULL);
}

void wifi_manager_auto_connect(wifi_connect_cb_t connect_cb)
{
    if (!s_initialized) return;
    if (s_connecting || s_auto_connecting) return;

    s_connect_cb = connect_cb;
    xTaskCreatePinnedToCore(auto_connect_task, "auto_conn", 4 * 1024,
                            NULL, 3, NULL, 1);
}

/* ========== 状态查询 ========== */

wifi_manager_state_t wifi_manager_get_state(void)
{
    return s_state;
}

const char *wifi_manager_get_ssid(void)
{
    return s_connected_ssid;
}

const char *wifi_manager_get_ip(void)
{
    return s_ip;
}

int wifi_manager_get_ap_count(void)
{
    return s_ap_count;
}

bool wifi_manager_get_ap(int index, wifi_ap_info_t *out)
{
    if (!out || index < 0 || index >= s_ap_count) return false;
    *out = s_ap_list[index];
    return true;
}