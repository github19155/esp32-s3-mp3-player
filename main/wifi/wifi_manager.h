#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief WiFi 管理器 — 纯逻辑模块，不依赖 LVGL
 *
 * 职责：
 * - WiFi 栈初始化/去初始化
 * - 扫描 AP 列表
 * - 连接/断开
 * - NVS 凭据保存/加载（最多 5 组）
 * - 自动重连（已保存网络）
 * - 状态查询（只读，供 UI 轮询或回调）
 *
 * page_wifi 通过回调获取状态更新，本模块不直接操作 UI。
 */

#define WIFI_MANAGER_SCAN_MAX   20
#define WIFI_MANAGER_SSID_MAX   33
#define WIFI_MANAGER_PWD_MAX    65
#define WIFI_MANAGER_NVS_MAX    5

/** WiFi 连接状态 */
typedef enum {
    WIFI_STATE_IDLE = 0,
    WIFI_STATE_SCANNING,
    WIFI_STATE_CONNECTING,
    WIFI_STATE_CONNECTED,
    WIFI_STATE_DISCONNECTED,
    WIFI_STATE_ERROR,
} wifi_manager_state_t;

/** 单个 AP 扫描结果 */
typedef struct {
    char     ssid[WIFI_MANAGER_SSID_MAX];
    int8_t   rssi;
    uint8_t  authmode;
} wifi_ap_info_t;

/** 扫描完成回调 — 在 WiFi 任务上下文中调用，UI 需自行加锁 */
typedef void (*wifi_scan_done_cb_t)(int ap_count, const wifi_ap_info_t *ap_list);

/** 状态变化回调 */
typedef void (*wifi_state_change_cb_t)(wifi_manager_state_t state);

/** 连接结果回调 — connected=true 时 ip 为有效 IP 字符串 */
typedef void (*wifi_connect_cb_t)(bool connected, const char *ssid, const char *ip);

/* ========== 生命周期 ========== */

/** 初始化 WiFi 管理器（幂等），注册事件循环 */
void wifi_manager_init(void);

/** 去初始化（关闭 WiFi、释放资源）。未连接时调用以省电。 */
void wifi_manager_deinit(void);

/* ========== 扫描 ========== */

/** 开始异步扫描。完成后调用回调（回调在 WiFi 任务上下文，UI 需加锁）。 */
void wifi_manager_scan_start(wifi_scan_done_cb_t callback);

/** 取消扫描 */
void wifi_manager_scan_stop(void);

/* ========== 连接 ========== */

/** 连接到指定 AP。异步，结果通过 connect_cb 通知。 */
void wifi_manager_connect(const char *ssid, const char *password,
                          wifi_connect_cb_t connect_cb);

/** 断开当前连接 */
void wifi_manager_disconnect(void);

/* ========== 自动连接 ========== */

/** 尝试连接 NVS 中第一个已保存网络。回调在 WiFi 事件上下文调用。 */
void wifi_manager_auto_connect(wifi_connect_cb_t connect_cb);

/* ========== NVS 凭据 ========== */

/** 保存 SSID+密码到 NVS（若已存在则更新，最多 5 组，循环覆盖） */
bool wifi_manager_cred_save(const char *ssid, const char *password);

/** 按索引加载凭据 */
bool wifi_manager_cred_load(int index, char *ssid_out, size_t ssid_len,
                             char *pwd_out, size_t pwd_len);

/** 查找 SSID 对应密码 */
bool wifi_manager_cred_find(const char *ssid, char *pwd_out, size_t pwd_len);

/** 清除所有保存的凭据 */
void wifi_manager_cred_clear(void);

/** 是否有已保存凭据 */
bool wifi_manager_has_saved(void);

/** 已保存凭据数量 */
int  wifi_manager_cred_count(void);

/* ========== 状态查询 ========== */

/** 获取当前状态 */
wifi_manager_state_t wifi_manager_get_state(void);

/** 获取当前连接的 SSID（未连接时返回空串） */
const char *wifi_manager_get_ssid(void);

/** 获取当前 IP 地址字符串（未获取时返回空串） */
const char *wifi_manager_get_ip(void);

/** 获取最后一次扫描的 AP 数量 */
int wifi_manager_get_ap_count(void);

/** 获取最后一次扫描结果中第 index 个 AP 的信息 */
bool wifi_manager_get_ap(int index, wifi_ap_info_t *out);

#ifdef __cplusplus
}
#endif
