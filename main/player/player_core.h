#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 播放器核心模块（纯逻辑，零 UI 依赖）
 *
 * 封装 esp-audio-player 组件，提供统一播放控制接口。
 * UI 和 BLE 遥控通过同一组 API 控制播放器。
 */

typedef enum {
    PLAYER_STATE_IDLE = 0,
    PLAYER_STATE_PLAYING,
    PLAYER_STATE_PAUSED,
} player_state_t;

/**
 * @brief 播放器状态变化回调
 * @param state          新状态
 * @param current_index  当前播放文件索引
 */
typedef void (*player_callback_t)(player_state_t state, int current_index);

/**
 * @brief 初始化播放器
 *
 * 必须在 bsp_codec_init() 之后调用。
 * 注册 _audio_player_mute_fn / write_fn / clk_set_fn 回调。
 */
void player_core_init(void);

/**
 * @brief 设置文件列表（由 storage 层注入）
 * @param count  文件数量
 *
 * 设置后，player_core 通过 sd_manager_get_path() 按索引打开文件。
 */
void player_core_set_file_count(int count);

/**
 * @brief 播放指定索引的文件
 */
void player_core_play(int index);

/**
 * @brief 暂停
 */
void player_core_pause(void);

/**
 * @brief 继续播放
 */
void player_core_resume(void);

/**
 * @brief 下一首
 */
void player_core_next(void);

/**
 * @brief 上一首
 */
void player_core_prev(void);

/**
 * @brief 设置音量
 * @param vol  0 ~ 100
 */
void player_core_set_volume(int vol);

/**
 * @brief 获取当前音量
 */
int player_core_get_volume(void);

/**
 * @brief 获取播放状态
 */
player_state_t player_core_get_state(void);

/**
 * @brief 获取当前播放文件索引
 */
int player_core_get_current_index(void);

/**
 * @brief 注册状态变化回调（UI 层注册以更新界面）
 */
void player_core_register_callback(player_callback_t cb);

/**
 * @brief 销毁播放器
 */
void player_core_deinit(void);

/**
 * @brief 事件分发入口（由 app_event_task 调用）
 */
void player_core_on_event(const void *event);

#ifdef __cplusplus
}
#endif
