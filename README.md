# ESP32-S3 多功能掌机

> 基于立创·实战派 ESP32-S3 开发板（ESP32-S3-WROOM-1-N16R8）
> ESP-IDF v6.0.1 · LVGL 8.3 · 16MB Flash · 8MB Octal PSRAM

---

## 当前状态

**核心基线（master）已完成编译和实机验证**（2026-07-12）。

| 分支 | 描述 | 状态 |
|------|------|------|
| `master` | 核心基线 — 后续所有功能分支的母版 | ✅ 编译及实机验证通过 |
| `feature/mp3` | 原 MP3 v0.1 稳定版本 | ✅ 已保留 |
| `codex/core-base` | 核心切换前的同版本备份分支 | 📦 临时保留 |

### 核心基线包含

| 功能 | 状态 |
|------|------|
| NVS 非易失性存储 | ✅ |
| I2C 总线 + PCA9557 | ✅ |
| LCD 显示 (ST77922) + LVGL 8.3 + 触摸 (FT5x06) | ✅ |
| 事件总线 (FreeRTOS) | ✅ |
| 页面管理器 (注册表 + 生命周期) | ✅ |
| 最小主页 (ESP32-S3 CORE + Touch Test) | ✅ |
| MP3 播放器 | ⏸ 源码保留，待 feature/mp3 分支恢复 |
| SD 卡 | ⏸ 源码保留，待功能分支恢复 |
| WiFi | ⏸ 待 feature/wifi 分支 |
| BLE | ⏸ 待 feature/ble 分支 |
| 摄像头 | ⏸ 待 feature/camera 分支 |
| 姿态传感器 | ⏸ 待 feature/attitude 分支 |

---

## 快速开始

```bash
# 1. 切换到核心基线分支
git checkout master

# 2. 设置芯片
idf.py set-target esp32s3

# 3. 编译
idf.py build

# 4. 烧录 + 监控
idf.py -p COM4 flash monitor
```

> **注意**：ESP-IDF v6.0 不支持 Git Bash，需在 PowerShell 或 CMD 中运行。

---

## 架构

```
main/
├── main.c                  # 入口：核心初始化（NVS → I2C → PCA9557 → LCD/Touch → EventBus → PageMgr）
│
├── bsp/                    # 板级支持包（v6.0 新版 I2C 驱动）
│   ├── esp32_s3_szp.h      # 引脚定义 + BSP API 声明
│   └── esp32_s3_szp.c      # 硬件驱动（I2C/LCD/音频/SD/IMU）
│
├── event/                  # 事件总线
│   ├── app_event.h         # 事件类型枚举
│   └── app_event.c         # FreeRTOS 队列分发
│
├── pages/                  # 页面层
│   ├── page_manager.h/c    # 页面注册 + 生命周期管理
│   └── page_main_menu.h/c  # 核心主页（英文轻量，Touch Test）
│
├── player/                 # 播放器核心（源码保留，核心基线不编译）
│   └── player_core.h/c     # 封装 esp-audio-player
│
├── storage/                # 存储管理（源码保留，核心基线不编译）
│   └── sd_manager.h/c      # SD 卡挂载 + 扫描
│
├── assets/                 # 图片 + 字体资源（源码保留，核心基线不编译）
│   ├── img_*_icon.c        # 6 个应用图标
│   └── font_alipuhui20.c   # 中文矢量字体
│
└── components/             # 修改过的第三方组件（本地化，源码保留）
    ├── chmorgan__esp-audio-player/
    └── espressif__esp_codec_dev/
```

### 扩展机制

从 `master` 创建功能分支后：

1. 写 `pages/page_wifi.c`，实现注册函数
2. 在 `main.c` 中加初始化调用
3. 在 `main/CMakeLists.txt` 加入新源文件

**核心基线不包含 SD/音频/MP3/WiFi，这些模块各自从 feature 分支恢复。**

---

## 分区表

| 分区 | 大小 | 说明 |
|------|------|------|
| nvs | 24k | WiFi/BLE 配网信息 |
| phy_init | 4k | 射频校准 |
| factory | **8M** | 固件（核心基线 524KB，约占 6%） |
| storage | 3M | SPIFFS（预留） |

---

## 关键依赖

### 核心基线依赖

| 组件 | 版本 | 用途 |
|------|------|------|
| lvgl/lvgl | ~8.3.0 | 图形库 |
| espressif/esp_lvgl_port | ~1.4.0 | LVGL-ESP 桥接 |
| espressif/esp_lcd_touch_ft5x06 | ~1.0.7 | 触摸驱动 |

### MP3 分支附加依赖（核心基线已移除）

| 组件 | 版本 | 用途 |
|------|------|------|
| chmorgan/esp-audio-player | ~1.0.7 | MP3/WAV 解码播放 |
| espressif/esp_codec_dev | ~1.3.0 | ES8311/ES7210 音频编解码器 |

> BSP 的 LCD 与音频函数目前位于同一个源文件，因此 `esp_codec_dev` 会被动进入链接；核心启动流程不会初始化或调用音频功能。播放器、MP3 解码器和音频页面均未链接。

---

## 硬件引脚速查

| 功能 | 引脚 |
|------|------|
| I2C SDA/SCL | GPIO 1 / 2 |
| LCD MOSI/CLK/DC/背光 | GPIO 40/41/39/42 |
| I2S MCLK/SCLK/LRCK/DOUT/SDIN | GPIO 38/14/13/45/12 |
| SD 卡 CLK/CMD/D0 | GPIO 47/48/21 |
| LCD CS / PA_EN / DVP_PWDN | PCA9557 IO扩展 (I2C 0x19) |
| 音频 DAC ES8311 | I2C 0x18 |
| 音频 ADC ES7210 | I2C 0x82 |

> 完整定义见 [HARDWARE_REFERENCE.md](HARDWARE_REFERENCE.md)

---

## ESP-IDF v6.0 适配记录

本项目基于 ESP-IDF v6.0.1，做了以下适配：

- I2C 驱动从 v5.x 旧 API 迁移到 `driver/i2c_master.h` 新版
- `esp_codec_dev` 和 `esp-audio-player` 补充 `esp_driver_*` 依赖
- 修复 `HSPI_HOST` 宏兼容性（→ `SPI3_HOST`）
- 分区表 factory 从 3MB 扩到 8MB（参考官方 14-handheld 例程）
- 添加 `LV_FONT_FMT_TXT_LARGE=y` 支持中文大字库
- `CAMERA_EN=0` 关闭摄像头编译（第一阶段不需要）

---

## 资料链接

- [硬件配置速查表](HARDWARE_REFERENCE.md)
- [长期开发计划](LONG_TERM_PLAN.md)
- [AI 协作中枢](AI_HANDOFF.md)
- 官方例程：`D:\Download\立创·实战派ESP32-S3开发板资料\01-例程\`
- 原理图：`D:\Download\立创·实战派ESP32-S3开发板资料\02-文档\立创实战派ESP32-S3开发板原理图.pdf`
