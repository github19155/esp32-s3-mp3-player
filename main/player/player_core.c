#include "player_core.h"
#include "app_event.h"
#include "sd_manager.h"
#include "esp32_s3_szp.h"
#include "audio_player.h"
#include "esp_log.h"
#include <stdio.h>

static const char *TAG = "player_core";

static audio_player_config_t s_player_config = {0};
static uint8_t   s_volume = 60;            /* 默认音量，与 BSP VOLUME_DEFAULT 一致 */
static int       s_file_count = 0;
static int       s_current_index = 0;
static player_state_t s_state = PLAYER_STATE_IDLE;
static player_callback_t s_callback = NULL;

/* ========== esp-audio-player 回调函数 ========== */

static esp_err_t _mute_fn(AUDIO_PLAYER_MUTE_SETTING setting)
{
    bsp_codec_mute_set(setting == AUDIO_PLAYER_MUTE ? true : false);
    if (setting == AUDIO_PLAYER_UNMUTE) {
        bsp_codec_volume_set(s_volume, NULL);
    }
    return ESP_OK;
}

static esp_err_t _write_fn(void *audio_buffer, size_t len, size_t *bytes_written, uint32_t timeout_ms)
{
    return bsp_i2s_write(audio_buffer, len, bytes_written, timeout_ms);
}

static esp_err_t _clk_set_fn(uint32_t rate, uint32_t bits_cfg, i2s_slot_mode_t ch)
{
    return bsp_codec_set_fs(rate, bits_cfg, ch);
}

static void _audio_callback(audio_player_cb_ctx_t *ctx)
{
    ESP_LOGI(TAG, "audio_event = %d", ctx->audio_event);

    switch (ctx->audio_event) {
    case AUDIO_PLAYER_CALLBACK_EVENT_IDLE:
        /* 播放完一首 → 自动切下一首 */
        ESP_LOGI(TAG, "Track finished, auto next");
        s_state = PLAYER_STATE_IDLE;
        player_core_next();
        break;

    case AUDIO_PLAYER_CALLBACK_EVENT_PLAYING:
        s_state = PLAYER_STATE_PLAYING;
        pa_en(1);  /* 打开功放 */
        if (s_callback) s_callback(s_state, s_current_index);
        break;

    case AUDIO_PLAYER_CALLBACK_EVENT_PAUSE:
        s_state = PLAYER_STATE_PAUSED;
        pa_en(0);  /* 关闭功放 */
        if (s_callback) s_callback(s_state, s_current_index);
        break;

    default:
        break;
    }
}

/* ========== 公共 API ========== */

void player_core_init(void)
{
    s_player_config.mute_fn    = _mute_fn;
    s_player_config.write_fn   = _write_fn;
    s_player_config.clk_set_fn = _clk_set_fn;
    s_player_config.priority   = 6;
    s_player_config.coreID     = 1;

    ESP_ERROR_CHECK(audio_player_new(s_player_config));
    ESP_ERROR_CHECK(audio_player_callback_register(_audio_callback, NULL));

    s_state = PLAYER_STATE_IDLE;
    ESP_LOGI(TAG, "Player core initialized");
}

void player_core_set_file_count(int count)
{
    s_file_count = count;
}

void player_core_play(int index)
{
    if (s_file_count == 0) {
        ESP_LOGW(TAG, "No files to play");
        return;
    }
    if (index < 0) index = 0;
    if (index >= s_file_count) index = s_file_count - 1;

    s_current_index = index;

    char path[128];
    if (sd_manager_get_path(index, path, sizeof(path)) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get path for index %d", index);
        return;
    }

    FILE *fp = fopen(path, "rb");
    if (fp) {
        ESP_LOGI(TAG, "Playing [%d/%d] %s", index, s_file_count, path);
        audio_player_play(fp);
    } else {
        ESP_LOGE(TAG, "Failed to open: %s", path);
    }
}

void player_core_pause(void)
{
    if (s_state == PLAYER_STATE_PLAYING) {
        audio_player_pause();
    }
}

void player_core_resume(void)
{
    if (s_state == PLAYER_STATE_PAUSED) {
        audio_player_resume();
    }
}

void player_core_next(void)
{
    if (s_file_count == 0) return;
    int next = (s_current_index + 1) % s_file_count;
    player_core_play(next);
}

void player_core_prev(void)
{
    if (s_file_count == 0) return;
    int prev = (s_current_index - 1 + s_file_count) % s_file_count;
    player_core_play(prev);
}

void player_core_set_volume(int vol)
{
    if (vol < 0)   vol = 0;
    if (vol > 100) vol = 100;
    s_volume = vol;
    bsp_codec_volume_set(vol, NULL);
    ESP_LOGI(TAG, "Volume set to %d", vol);
}

int player_core_get_volume(void)
{
    return s_volume;
}

player_state_t player_core_get_state(void)
{
    return s_state;
}

int player_core_get_current_index(void)
{
    return s_current_index;
}

void player_core_register_callback(player_callback_t cb)
{
    s_callback = cb;
}

void player_core_deinit(void)
{
    audio_player_delete();
    s_state = PLAYER_STATE_IDLE;
    s_callback = NULL;
}

/* ========== 事件分发 ========== */

void player_core_on_event(const void *event_ptr)
{
    const app_event_t *event = (const app_event_t *)event_ptr;

    switch (event->type) {
    case APP_EVENT_PLAY:
        if (s_state == PLAYER_STATE_IDLE) {
            player_core_play(s_current_index);
        }
        break;
    case APP_EVENT_PAUSE:
        player_core_pause();
        break;
    case APP_EVENT_RESUME:
        player_core_resume();
        break;
    case APP_EVENT_NEXT:
        player_core_next();
        break;
    case APP_EVENT_PREV:
        player_core_prev();
        break;
    case APP_EVENT_VOLUME_UP:
        player_core_set_volume(s_volume + 5);
        break;
    case APP_EVENT_VOLUME_DOWN:
        player_core_set_volume(s_volume - 5);
        break;
    case APP_EVENT_VOLUME_SET:
        player_core_set_volume(event->payload);
        break;
    default:
        break;
    }
}
