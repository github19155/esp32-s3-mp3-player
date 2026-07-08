#include "app_event.h"
#include "esp_log.h"

static const char *TAG = "app_event";

static QueueHandle_t s_event_queue = NULL;

/* ========== 转发声明 — 事件分发目标 ========== */
extern void page_manager_on_event(const app_event_t *event);
extern void player_core_on_event(const app_event_t *event);

void app_event_init(int queue_len)
{
    if (s_event_queue != NULL) {
        ESP_LOGW(TAG, "Event queue already initialized");
        return;
    }
    s_event_queue = xQueueCreate(queue_len, sizeof(app_event_t));
    if (s_event_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create event queue");
    }
    ESP_LOGI(TAG, "Event queue created, depth=%d", queue_len);
}

void app_event_post(app_event_type_t type, int payload)
{
    if (s_event_queue == NULL) {
        ESP_LOGW(TAG, "Event queue not initialized, dropping event %d", type);
        return;
    }
    app_event_t event = { .type = type, .payload = payload };
    if (xQueueSend(s_event_queue, &event, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Event queue full, dropped event %d", type);
    }
}

void app_event_post_blocking(app_event_type_t type, int payload)
{
    if (s_event_queue == NULL) {
        ESP_LOGW(TAG, "Event queue not initialized");
        return;
    }
    app_event_t event = { .type = type, .payload = payload };
    xQueueSend(s_event_queue, &event, portMAX_DELAY);
}

void app_event_task(void *pvParameters)
{
    app_event_t event;

    ESP_LOGI(TAG, "Event dispatch task started");

    while (1) {
        if (xQueueReceive(s_event_queue, &event, portMAX_DELAY) == pdTRUE) {
            ESP_LOGD(TAG, "Dispatch event type=%d payload=%d", event.type, event.payload);

            /* 分发到各模块 — 按事件类型路由 */

            /* 播放器相关 → player_core */
            switch (event.type) {
            case APP_EVENT_PLAY:
            case APP_EVENT_PAUSE:
            case APP_EVENT_RESUME:
            case APP_EVENT_NEXT:
            case APP_EVENT_PREV:
            case APP_EVENT_VOLUME_UP:
            case APP_EVENT_VOLUME_DOWN:
            case APP_EVENT_VOLUME_SET:
                player_core_on_event(&event);
                break;
            default:
                break;
            }

            /* 页面导航 → page_manager */
            switch (event.type) {
            case APP_EVENT_PAGE_OPEN:
            case APP_EVENT_PAGE_CLOSE:
                page_manager_on_event(&event);
                break;
            default:
                break;
            }
        }
    }
}
