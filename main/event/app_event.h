#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 应用事件类型
 *
 * UI 触摸、BLE 遥控、按键等所有输入统一走此事件通道。
 * 后续加功能只需在此枚举末尾追加新类型。
 */
typedef enum {
    APP_EVENT_NONE = 0,

    /* 播放控制事件 */
    APP_EVENT_PLAY,
    APP_EVENT_PAUSE,
    APP_EVENT_RESUME,
    APP_EVENT_NEXT,
    APP_EVENT_PREV,
    APP_EVENT_VOLUME_UP,
    APP_EVENT_VOLUME_DOWN,
    APP_EVENT_VOLUME_SET,       /* payload = 0..100 */

    /* 页面导航事件 */
    APP_EVENT_PAGE_OPEN,        /* payload = page_id_t */
    APP_EVENT_PAGE_CLOSE,

    /* 系统事件 */
    APP_EVENT_SD_MOUNTED,
    APP_EVENT_SD_UNMOUNTED,
    APP_EVENT_BTN_BOOT_PRESS,   /* GPIO 0 按键 */

    APP_EVENT_MAX
} app_event_type_t;

typedef struct {
    app_event_type_t type;
    int payload;                /* 页面ID / 音量值 / 索引 等 */
} app_event_t;

/**
 * @brief 初始化事件队列
 * @param queue_len 队列最大深度
 */
void app_event_init(int queue_len);

/**
 * @brief 投递事件（非阻塞，满则丢弃）
 */
void app_event_post(app_event_type_t type, int payload);

/**
 * @brief 投递事件（阻塞等待，直到入队）
 */
void app_event_post_blocking(app_event_type_t type, int payload);

/**
 * @brief 事件处理任务 — 从队列取事件并分发到各模块
 */
void app_event_task(void *pvParameters);

#ifdef __cplusplus
}
#endif
