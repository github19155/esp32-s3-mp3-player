#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 注册 WiFi 页面到 page_manager
 *
 * 调用时机：page_manager_init() 之后，page_main_menu_create() 之前。
 * 页面进入时自动初始化 WiFi 栈、扫描并尝试自动连接。
 */
void page_wifi_register(void);

#ifdef __cplusplus
}
#endif
