#pragma once

#include "lvgl.h"
#include "esp32_s3_szp.h"
#include "app_event.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 页面管理器
 *
 * 负责页面注册、生命周期管理、标题栏 + 返回按钮。
 * 每个页面模块初始化时调用 page_manager_register() 注册自己。
 */

typedef enum {
    PAGE_NONE = 0,

    PAGE_MP3,           /* MP3 播放器 */
    PAGE_WIFI,          /* WiFi 设置（预留） */
    PAGE_BLE,           /* 蓝牙（预留） */
    PAGE_CAMERA,        /* 摄像头（预留） */
    PAGE_ATTITUDE,      /* 姿态传感器（预留） */
    PAGE_SDCARD,        /* SD 卡文件浏览（预留） */

    PAGE_MAX
} page_id_t;

typedef struct {
    page_id_t    id;             /* 页面唯一 ID */
    const char  *title;          /* 标题栏文字 */
    lv_color_t   title_bg_color; /* 标题栏背景色 */
    const lv_img_dsc_t *icon;    /* 主菜单图标（可为 NULL） */
    const char  *label;          /* 图标下方标签 */
    bool         enabled;        /* 是否已启用（false = 灰显占位） */
    void       (*on_enter)(void);/* 进入页面回调 */
    void       (*on_exit)(void); /* 退出页面回调 */
} page_config_t;

/**
 * @brief 初始化页面管理器
 */
void page_manager_init(void);

/**
 * @brief 注册一个页面配置
 *
 * 每个页面模块在初始化时调用一次。
 */
void page_manager_register(const page_config_t *config);

/**
 * @brief 打开指定页面（创建容器 + 标题栏 + 调用 on_enter）
 */
void page_manager_open(page_id_t id);

/**
 * @brief 关闭当前页面（调用 on_exit + 删除容器）
 */
void page_manager_close(void);

/**
 * @brief 获取当前打开的页面 ID
 */
page_id_t page_manager_get_current(void);

/**
 * @brief 获取当前页面容器对象（供页面模块在里面创建控件）
 */
lv_obj_t *page_manager_get_container(void);

/**
 * @brief 获取注册表中页面数量
 */
int page_manager_get_page_count(void);

/**
 * @brief 按索引获取页面配置
 */
const page_config_t *page_manager_get_page(int index);

/**
 * @brief 设置/获取主菜单对象引用
 */
void page_manager_set_main_menu(lv_obj_t *main_menu);
lv_obj_t *page_manager_get_main_menu(void);

/**
 * @brief 返回按钮点击处理（由 page_manager 内部绑定，用户无需关心）
 */
void page_manager_on_back_clicked(lv_event_t *e);

/**
 * @brief 事件分发入口（由 app_event_task 调用）
 */
void page_manager_on_event(const app_event_t *event);

#ifdef __cplusplus
}
#endif
