#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 注册 MP3 播放器页面到 page_manager
 *
 * 调用时机：在 page_manager_init() 之后，page_main_menu_create() 之前。
 * 内部完成：SD 卡扫描、播放器配置、注册页面回调。
 */
void page_mp3_register(void);

#ifdef __cplusplus
}
#endif
