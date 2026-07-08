# 项目交接文档

> ESP32-S3 MP3 播放器 · ESP-IDF v6.0.1
> 最后更新：2026-07-08

---

## 环境

| 项 | 值 |
|-----|-----|
| ESP-IDF | v6.0.1（路径：`D:\esp\.espressif\v6.0.1\esp-idf`） |
| 工具链 | `C:\Espressif\tools\`（EIM 离线安装器） |
| 项目路径 | `E:\ai\esp32\` |
| 芯片 | ESP32-S3 (WROOM-1-N16R8) |
| 串口 | COM4 |
| GitHub | [github19155/esp32-s3-mp3-player](https://github.com/github19155/esp32-s3-mp3-player) |

---

## 项目状态（2026-07-08）

MP3 播放器第一阶段 **已实现**，架构支持扩展。

```
e:\ai\esp32\
├── CMakeLists.txt             # 项目入口 + 全局编译选项
├── sdkconfig.defaults         # 芯片/PSRAM/LVGL/LV_FONT_FMT_TXT_LARGE
├── partitions.csv             # 分区表（factory 8M + storage 3M）
├── README.md                  # 项目总览
├── HARDWARE_REFERENCE.md      # 硬件引脚速查表
├── HANDOVER.md                # 这份交接文档
├── components/                # 修改过的第三方组件（本地化）
│   ├── chmorgan__esp-audio-player/
│   └── espressif__esp_codec_dev/
├── main/
│   ├── CMakeLists.txt         # 主组件编译
│   ├── idf_component.yml      # 组件依赖
│   ├── main.c                 # 入口：链式初始化
│   ├── assets/                # 图标 + 中文字体
│   ├── bsp/                   # 板级支持包（v6.0 新版 I2C）
│   ├── event/                 # 事件总线
│   ├── pages/                 # 页面层
│   ├── player/                # 播放器核心
│   └── storage/               # SD 卡管理
```

---

## 编译 & 烧录

```powershell
# 1. 加载 ESP-IDF 环境（PowerShell）
& 'C:\Espressif\tools\Microsoft.v6.0.1.PowerShell_profile.ps1'

# 2. 编译
cd E:\ai\esp32
idf.py build

# 3. 烧录 + 监控
idf.py -p COM4 flash monitor
```

> **重要**：ESP-IDF v6.0 不支持 Git Bash，必须在 PowerShell 或 CMD 中执行。
> 如果 Git Bash 中执行会报 `MSys/Mingw is no longer supported`。

---

## 扩展指南

### 加新功能页面

1. 写 `main/pages/page_xxx.c`，实现 `page_xxx_register()` 调用 `page_manager_register()`
2. 在 `main/main.c` 的注册区加一行 `page_xxx_register()`
3. 在 `main/pages/page_main_menu.c` 的 `menu_icons[]` 数组里把对应行 `.enabled = true`

### BLE 遥控

BLE 回调直接调用 `player_core_next()` / `player_core_prev()` 等 API，和触摸 UI 走同一条通道。也可通过 `app_event_post()` 投递事件。

---

## v6.0 适配记录

| 问题 | 修复 |
|------|------|
| I2C 旧 API 弃用 | 重写为 `i2c_master_bus_handle_t` 新版 |
| `audio_codec_i2c_cfg_t` 传参 | `port` 和 `bus_handle` 分开设置 |
| `esp_codec_dev` 缺编译依赖 | 补充 `esp_driver_gpio/spi/i2c/i2s` |
| `esp-audio-player` 缺编译依赖 | 补充 `esp_driver_i2s` |
| `HSPI_HOST` 未定义 | → `SPI3_HOST` |
| C++ `-Wignored-qualifiers` 错误 | 全局 `-Wno-ignored-qualifiers` |
| factory 分区 3MB 不够 | → 8MB（参考官方 14-handheld） |
| 中文字体太大 | 启用 `LV_FONT_FMT_TXT_LARGE` |
| `esp_camera.h` 缺失 | `CAMERA_EN=0` 关闭摄像头编译 |
| `managed_components` 修改后被覆盖 | 迁移到 `components/` 本地管理 |

---

## 参考资料

| 资料 | 路径 |
|------|------|
| 官方 MP3 例程 | `D:\Download\立创·实战派ESP32-S3开发板资料\01-例程\szpi-s3-esp\11-mp3_player\` |
| 手持设备综合例程 | `D:\Download\立创·实战派ESP32-S3开发板资料\01-例程\szpi-s3-esp\14-handheld\` |
| 原理图 | `D:\Download\立创·实战派ESP32-S3开发板资料\02-文档\立创实战派ESP32-S3开发板原理图.pdf` |
| 芯片手册 | `D:\Download\立创·实战派ESP32-S3开发板资料\02-文档\02-芯片手册\` |
