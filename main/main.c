#include <stdio.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp32_s3_szp.h"
#include "app_event.h"
#include "sd_manager.h"
#include "player_core.h"
#include "page_manager.h"
#include "page_main_menu.h"
#include "page_mp3.h"

static const char *TAG = "main";

/* ========== 内存监控（每 10 秒输出一次） ========== */
static void memory_monitor_task(void *pvParameters)
{
    while (1) {
        size_t free_dram  = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        size_t free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        ESP_LOGI(TAG, "Memory — DRAM free: %u, PSRAM free: %u", free_dram, free_psram);
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

/* ========== 程序入口 ========== */
void app_main(void)
{
    /* ── 1. NVS 初始化 ── */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "NVS initialized");

    /* ── 2. I2C 总线 ── */
    ESP_ERROR_CHECK(bsp_i2c_init());
    ESP_LOGI(TAG, "I2C bus initialized");

    /* ── 3. IO 扩展芯片（LCD_CS / PA_EN / DVP_PWDN） ── */
    pca9557_init();
    ESP_LOGI(TAG, "PCA9557 initialized");

    /* ── 4. LCD + LVGL + 触摸 ── */
    bsp_lvgl_start();
    ESP_LOGI(TAG, "LCD + LVGL + Touch initialized");

    /* ── 5. SD 卡 ── */
    ret = sd_manager_mount();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "SD card mount failed, MP3 playback unavailable");
    }

    /* ── 6. 音频编解码器 ── */
    ESP_ERROR_CHECK(bsp_codec_init());
    ESP_LOGI(TAG, "Audio codec initialized");

    /* ── 7. 事件总线 ── */
    app_event_init(20);

    /* ── 8. 播放器核心 ── */
    player_core_init();

    /* ── 9. 页面管理器 ── */
    page_manager_init();

    /* ── 10. 注册各页面 ── */
    /* ═══════════════════════════════════════════════════════════
     *  扩展指南：添加新功能时，在此处加一行 xxx_register()
     *  然后去 page_main_menu.c 把对应 icon 的 enabled 改为 true
     * ═══════════════════════════════════════════════════════════ */
    page_mp3_register();
    // page_wifi_register();      /* 未来：取消注释 */
    // page_ble_register();       /* 未来：取消注释 */
    // page_camera_register();    /* 未来：取消注释 */
    // page_attitude_register();  /* 未来：取消注释 */
    // page_sdcard_register();    /* 未来：取消注释 */

    /* ── 11. 创建主菜单 ── */
    page_main_menu_create();

    /* ── 12. 启动事件分发任务 ── */
    xTaskCreatePinnedToCore(app_event_task, "event_task", 4 * 1024, NULL, 5, NULL, 0);

    /* ── 13. 内存监控（可选） ── */
    xTaskCreatePinnedToCore(memory_monitor_task, "mem_mon", 2 * 1024, NULL, 1, NULL, 1);

    ESP_LOGI(TAG, "System ready — MP3 player running");

    /* 主循环空闲 */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
