#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 创建主菜单页面
 *
 * 根据 page_manager 注册表中 enabled=true 的页面自动生成图标网格。
 * 调用一次，内部创建 lv_obj 作为底层 main_obj。
 */
void page_main_menu_create(void);

#ifdef __cplusplus
}
#endif
