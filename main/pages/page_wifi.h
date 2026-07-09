#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 注册 WiFi 扫描页面
 *
 * 调用时机：在 page_manager_init() 之后，page_main_menu_create() 之前。
 */
void page_wifi_register(void);

#ifdef __cplusplus
}
#endif
