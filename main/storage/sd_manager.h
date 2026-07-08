#pragma once

#include "esp_err.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief SD 卡管理模块
 *
 * 封装 SDMMC 1-bit 模式挂载/卸载 + MP3/WAV 文件扫描。
 * 数据源：/sdcard 根目录
 * 遍历方式：POSIX opendir/readdir
 */

#define SD_MOUNT_POINT          "/sdcard"
#define SD_MAX_FILES            128         /* 最大可管理文件数 */

/**
 * @brief 挂载 SD 卡
 */
esp_err_t sd_manager_mount(void);

/**
 * @brief 卸载 SD 卡
 */
esp_err_t sd_manager_unmount(void);

/**
 * @brief 扫描 SD 卡根目录下的 MP3/WAV 文件
 * @param extensions  文件扩展名过滤器，如 "mp3" 或 "wav"，传 NULL 则不过滤
 * @return 扫描到的文件数量，失败返回 -1
 */
int sd_manager_scan(const char *extension);

/**
 * @brief 获取文件数量
 */
int sd_manager_get_file_count(void);

/**
 * @brief 按索引获取文件完整路径
 * @param index  文件索引 (0-based)
 * @param path   输出缓冲区
 * @param max_len 缓冲区大小
 * @return 成功返回 ESP_OK，索引无效返回 ESP_FAIL
 */
esp_err_t sd_manager_get_path(int index, char *path, size_t max_len);

/**
 * @brief 按索引获取文件名（不含路径）
 * @param index  文件索引 (0-based)
 * @return 文件名指针（内部静态缓冲区），索引无效返回 NULL
 */
const char *sd_manager_get_name(int index);

#ifdef __cplusplus
}
#endif
