#include "sd_manager.h"
#include "esp32_s3_szp.h"
#include "esp_log.h"
#include <dirent.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <sys/stat.h>

static const char *TAG = "sd_manager";

/* 音乐文件存放目录 */
#define SD_MUSIC_PATH   "/sdcard/music"

/* 文件列表存储 */
static char s_file_paths[SD_MAX_FILES][128];   /* 完整路径 */
static char s_file_names[SD_MAX_FILES][64];    /* 仅文件名 */
static int  s_file_count = 0;
static bool s_mounted = false;

esp_err_t sd_manager_mount(void)
{
    if (s_mounted) {
        ESP_LOGW(TAG, "SD card already mounted");
        return ESP_OK;
    }

    esp_err_t ret = bsp_sdcard_mount();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount SD card: %s", esp_err_to_name(ret));
        return ret;
    }

    s_mounted = true;
    ESP_LOGI(TAG, "SD card mounted at %s", SD_MOUNT_POINT);
    return ESP_OK;
}

esp_err_t sd_manager_unmount(void)
{
    if (!s_mounted) {
        return ESP_OK;
    }

    s_file_count = 0;
    s_mounted = false;

    return bsp_sdcard_unmount();
}

int sd_manager_scan(const char *extension)
{
    if (!s_mounted) {
        ESP_LOGE(TAG, "SD card not mounted, cannot scan");
        return -1;
    }

    DIR *dir = opendir(SD_MUSIC_PATH);
    if (dir == NULL) {
        ESP_LOGE(TAG, "Failed to open directory: %s", SD_MUSIC_PATH);
        return -1;
    }

    s_file_count = 0;
    struct dirent *ent;

    while ((ent = readdir(dir)) != NULL && s_file_count < SD_MAX_FILES) {
        /* 只处理常规文件 */
        if (ent->d_type != DT_REG) continue;

        /* 扩展名过滤 */
        if (extension != NULL) {
            const char *ext = strrchr(ent->d_name, '.');
            if (ext == NULL) continue;             /* 无扩展名 */
            ext++;                                  /* 跳过 '.' */
            if (strcasecmp(ext, extension) != 0) continue;
        }

        /* 存储路径和文件名 */
        int n = snprintf(s_file_paths[s_file_count], sizeof(s_file_paths[0]),
                         "%s/%s", SD_MUSIC_PATH, ent->d_name);
        if (n < 0 || n >= sizeof(s_file_paths[0])) {
            ESP_LOGW(TAG, "Path too long: %s/%s", SD_MUSIC_PATH, ent->d_name);
            continue;
        }

        strncpy(s_file_names[s_file_count], ent->d_name, sizeof(s_file_names[0]) - 1);
        s_file_names[s_file_count][sizeof(s_file_names[0]) - 1] = '\0';

        ESP_LOGI(TAG, "  [%d] %s", s_file_count, s_file_names[s_file_count]);
        s_file_count++;
    }

    closedir(dir);
    ESP_LOGI(TAG, "Scan complete: %d file(s) found", s_file_count);
    return s_file_count;
}

int sd_manager_get_file_count(void)
{
    return s_file_count;
}

esp_err_t sd_manager_get_path(int index, char *path, size_t max_len)
{
    if (index < 0 || index >= s_file_count) {
        return ESP_FAIL;
    }
    strncpy(path, s_file_paths[index], max_len - 1);
    path[max_len - 1] = '\0';
    return ESP_OK;
}

const char *sd_manager_get_name(int index)
{
    if (index < 0 || index >= s_file_count) {
        return NULL;
    }
    return s_file_names[index];
}
