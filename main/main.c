#include <stdio.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp32_s3_szp.h"
#include "app_event.h"
#include "page_manager.h"
#include "page_main_menu.h"
#include "page_wifi.h"

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

    /* ── 5. 事件总线 ── */
    app_event_init(20);
    ESP_LOGI(TAG, "Event bus initialized");

    /* ── 6. 页面管理器 ── */
    page_manager_init();

    /* ── 7. 注册各页面 ── */
    page_wifi_register();

    /* ── 8. 创建主菜单 ── */
    page_main_menu_create();

    /* ── 9. 启动事件分发任务 ── */
    xTaskCreatePinnedToCore(app_event_task, "event_task", 4 * 1024, NULL, 5, NULL, 0);

    /* ── 10. 内存监控（可选） ── */
    xTaskCreatePinnedToCore(memory_monitor_task, "mem_mon", 2 * 1024, NULL, 1, NULL, 1);

    ESP_LOGI(TAG, "Core system ready");

    /* 主循环空闲 */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}