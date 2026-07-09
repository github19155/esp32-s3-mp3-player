# AI_HANDOFF.md — 项目交接文档（给 Zcode / Codex / GLM）

> 项目：ESP32-S3 MP3 多功能掌机
> 最后更新：2026-07-09
> Git: [github19155/esp32-s3-mp3-player](https://github.com/github19155/esp32-s3-mp3-player)

---

## 1. 什么是这个项目

基于立创·实战派 ESP32-S3 开发板（ESP32-S3-WROOM-1-N16R8，16MB Flash + 8MB Octal PSRAM）做多功能掌机。第一阶段是 **MP3 播放器**（已完成），后续扩展 WiFi、BLE、摄像头、姿态传感器等。

---

## 2. 当前已完成

| 模块 | 状态 | 说明 |
|------|------|------|
| BSP 驱动 | ✅ 完成 | I2C / LCD / 触摸 / 音频 / SD 卡驱动全部就绪，v6.0 新版 I2C API |
| 事件总线 | ✅ 完成 | FreeRTOS 队列，UI / BLE / 按键统一事件分发 |
| SD 卡管理 | ✅ 完成 | 挂载 + opendir/readdir 扫描根目录 MP3/WAV |
| 播放器核心 | ✅ 完成 | 封装 esp-audio-player，纯逻辑，无 UI 依赖 |
| 页面管理器 | ✅ 完成 | 注册表 + 生命周期 + 标题栏/返回按钮 |
| 主菜单 | ✅ 完成 | 6 图标（3×2 网格），仅 MP3 激活，其余灰显占位 |
| MP3 播放 UI | ✅ 完成 | 文件列表 + 播放/暂停/切歌 + 音量滑动条 |
| 编译适配 | ✅ 完成 | ESP-IDF v6.0.1，所有兼容性问题已修复 |
| 中文支持 | ✅ 完成 | font_alipuhui20 矢量字体 |

**未完成**：WiFi、BLE、摄像头、姿态传感器、SD 卡文件浏览。页面枚举和图标已预留，对应图标在主菜单灰显（`.enabled = false`）。

---

## 3. 架构总览

```
main/
├── main.c                  # 入口：链式初始化所有模块（13 步）
│
├── bsp/                    # 板级支持包
│   ├── esp32_s3_szp.h      # 引脚定义 + 全部 BSP API 声明
│   └── esp32_s3_szp.c      # 硬件驱动（v6.0 新版 I2C + LCD + 音频 + SD + IMU）
│
├── event/                  # 事件总线
│   ├── app_event.h         # 事件类型枚举（播放/页面/系统）
│   └── app_event.c         # FreeRTOS 队列 + 分发任务
│
├── pages/                  # 页面层
│   ├── page_manager.h/c    # 页面注册表 + 生命周期（标题栏 + 返回按钮）
│   ├── page_main_menu.h/c  # 主菜单（配置驱动图标网格）
│   └── page_mp3.h/c        # MP3 播放器 UI
│
├── player/                 # 播放器核心（纯逻辑，零 UI 依赖）
│   └── player_core.h/c     # 封装 esp-audio-player
│
├── storage/                # 存储管理
│   └── sd_manager.h/c      # SD 卡挂载 + MP3/WAV 文件扫描
│
├── assets/                 # 图片 + 字体
│   ├── img_*_icon.c        # 6 个应用图标
│   └── font_alipuhui20.c   # 中文矢量字体
│
└── components/             # 修改过的第三方组件（本地化，不会被 idf.py 覆盖）
    ├── chmorgan__esp-audio-player/
    └── espressif__esp_codec_dev/
```

### 页面切换机制

- 主菜单 `main_obj` 始终在屏幕底层
- 每个页面是覆盖在上的容器（`icon_in_obj`）
- 进入页面：`page_manager_open(id)` → 创建容器 + 标题栏 + 返回按钮 + 调用 `on_enter`
- 退出页面：`page_manager_close()` → 调用 `on_exit` + `lv_obj_del(容器)`

### 事件流

```
触摸 → LVGL 事件回调 → page_mp3.c → player_core_*() → audio_player_play()
                                                              ↓
                                                     bsp_i2s_write() → ES8311 → 喇叭
                                                              ↓
                                                     sd_manager_get_path() → SD 卡
```

---

## 4. 硬件摘要

| 组件 | 关键信息 |
|------|----------|
| 主控 | ESP32-S3, 240MHz, 16MB Flash, 8MB Octal PSRAM |
| LCD | 320×240, ST7789, SPI3 (MOSI 40 / CLK 41 / DC 39 / CS PCA9557 BIT0 / 背光 42) |
| 触摸 | FT5x06, I2C 地址 0x38 |
| 音频 DAC | ES8311, I2C 控制口, I2S1 输出 (DOUT 45) |
| 音频 ADC | ES7210, I2C 控制口, I2S1 输入 (SDIN 12) |
| 功放 | NS4150B, 使能脚 PCA9557 BIT1, 由 `pa_en()` 控制 |
| SD 卡 | SDMMC 1-bit (CLK 47 / CMD 48 / D0 21) |
| IMU | QMI8658, I2C 0x6A, 驱动保留但页面未启用 |
| I2C | GPIO1 SDA / GPIO2 SCL, 100kHz |
| BOOT 按键 | GPIO0, 下降沿中断, 内部上拉, 当前未使用 |

> 完整引脚表见 `HARDWARE_REFERENCE.md`

---

## 5. I2C 地址速查

| 设备 | 7-bit 地址 | BSP 代码写法 | 注意 |
|------|------------|-------------|------|
| QMI8658 | `0x6A` | `QMI8658_SENSOR_ADDR 0x6A` | 新 I2C 驱动直接使用 7-bit |
| PCA9557 | `0x19` | `PCA9557_SENSOR_ADDR 0x19` | 新 I2C 驱动直接使用 7-bit |
| 触摸 FT5x06 | `0x38` | `ESP_LCD_TOUCH_IO_I2C_FT5x06_CONFIG()` | — |
| ES8311 DAC | `0x18` | `ES8311_CODEC_DEFAULT_ADDR`（控制字 0x30） | esp_codec_dev 内部右移 |
| ES7210 ADC | `0x41` | 手动传 `.addr = 0x82` | 不是默认值 0x80，不能改 |

---

## 6. 构建环境

| 项 | 值 |
|-----|-----|
| ESP-IDF | v6.0.1（路径：`D:\esp\.espressif\v6.0.1\esp-idf`） |
| 工具链 | `C:\Espressif\tools\`（EIM 离线安装器） |
| Python 虚拟环境 | `C:\Espressif\tools\python\v6.0.1\venv` |
| 编译器 | xtensa-esp32s3-elf-gcc 15.2.0 |
| 项目路径 | `E:\ai\esp32\` |
| 串口 | COM4 |

### 编译命令（PowerShell，不支持 Git Bash）

```powershell
# 加载环境
Remove-Item Env:MSYSTEM -ErrorAction SilentlyContinue
$env:IDF_PATH = 'D:\esp\.espressif\v6.0.1\esp-idf'
$env:IDF_TOOLS_PATH = 'C:\Espressif\tools'
$env:IDF_PYTHON_ENV_PATH = 'C:\Espressif\tools\python\v6.0.1\venv'
$env:ESP_IDF_VERSION = '6.0.1'
$env:ESP_ROM_ELF_DIR = 'C:\Espressif\tools\esp-rom-elfs\20241011'
$env:PATH = "C:\Espressif\tools\python\v6.0.1\venv\Scripts;C:\Espressif\tools\cmake\4.0.3\bin;C:\Espressif\tools\ninja\1.12.1;C:\Espressif\tools\xtensa-esp-elf\esp-15.2.0_20251204\xtensa-esp-elf\bin;" + $env:PATH

# 编译
cd E:\ai\esp32
python D:\esp\.espressif\v6.0.1\esp-idf\tools\idf.py build

# 烧录 + 监控
python D:\esp\.espressif\v6.0.1\esp-idf\tools\idf.py -p COM4 flash monitor
```

### 关键依赖

| 组件 | 版本 |
|------|------|
| lvgl/lvgl | ~8.3.0 |
| espressif/esp_lvgl_port | ~1.4.0 |
| espressif/esp_lcd_touch_ft5x06 | ~1.0.7 |
| chmorgan/esp-audio-player | ~1.0.7 |
| espressif/esp_codec_dev | ~1.3.0 |

### 分区表

| 分区 | 大小 | 说明 |
|------|------|------|
| nvs | 24 KB | WiFi/BLE 配置 |
| phy_init | 4 KB | RF 校准 |
| factory | **8 MB** | 固件 |
| storage | 3 MB | SPIFFS（预留，未使用） |

---

## 7. 已知风险和踩坑记录

### a) ESP-IDF v6.0 不支持 Git Bash

必须用 PowerShell。`idf.py` 检测到 `MSYSTEM` 环境变量会直接拒绝运行。

### b) I2C 驱动必须用新版 API

BSP 的 I2C 层已全部改为 `i2c_master_bus_handle_t`。`esp_codec_dev` 的 `audio_codec_i2c_cfg_t` 结构体有独立的 `.port`（uint8_t）和 `.bus_handle`（void*）字段，**不能把 bus_handle 塞进 port** —— 这曾导致看门狗无限重启。

### c) ESP-IDF v6.0 驱动拆分

`esp_codec_dev` 和 `esp-audio-player` 是旧版组件，默认 `REQUIRES driver`。v6.0 把驱动拆成了 `esp_driver_gpio`、`esp_driver_spi`、`esp_driver_i2c`、`esp_driver_i2s`，必须手动补齐。这些修改过的组件已迁移到 `components/` 本地目录，防止被 `idf.py fullclean` 覆盖。

### d) 音频编解码器 I2C 地址是 8-bit 控制字

`esp_codec_dev` 的控制层在注册设备时会右移 1 位将 8-bit 控制字转为 7-bit 地址。ES8311 传 `0x30`（实际 7-bit `0x18`），ES7210 传 `0x82`（实际 7-bit `0x41`）。**不要**直接把这两个值改成 7-bit，会找不到设备。

### e) 中文字体

`font_alipuhui20.c` 需要 `CONFIG_LV_FONT_FMT_TXT_LARGE=y` 才能编译。已在 `sdkconfig.defaults` 中配置。字体声明用 `LV_FONT_DECLARE(font_alipuhui20)`。

### f) 摄像头

当前 `CAMERA_EN=0`，且 BSP 中 `#if CAMERA_EN` 直接 `#error`。意思是**不能简单把宏改 1**，需要补 BSP 实现 + 添加 `esp32-camera` 依赖。

### g) 功放控制

不是 ES8311 的 `.pa_pin`（当前设为 `GPIO_NUM_NC`），而是播放器回调里的 `pa_en(1)`/`pa_en(0)`，走 PCA9557 BIT1。

---

## 8. 下一步开发建议（按优先级）

| 优先级 | 任务 | 预计工作量 |
|--------|------|-----------|
| **P0** | WiFi 扫描 + 连接页面 | 参考 14-handheld 例程，适配到 page_manager 框架 |
| **P0** | NTP 时间同步（主菜单显示时间） | 依赖 WiFi 连接完成后触发 |
| **P1** | BLE 遥控（通过事件总线控制播放器） | 新建 ble_manager + page_ble |
| **P1** | BOOT 按键 → 事件总线 | ~50 行代码 |
| **P2** | 姿态传感器页面 | 复用 qmi8658 驱动 |
| **P2** | SD 卡文件浏览 | 扩展 sd_manager 支持子目录 |
| **P3** | 摄像头 | 需要补 BSP 实现 + 添加组件依赖 |

### 添加新页面的步骤（通用模板）

1. 在 `page_manager.h` 的 `page_id_t` 枚举里确认 ID 已存在
2. 新建 `pages/page_xxx.c`，实现 `page_xxx_on_enter()` / `page_xxx_on_exit()`
3. 在 `page_xxx.c` 末尾写 `page_xxx_register()`，调用 `page_manager_register()`
4. 在 `main.c` 的注册区加 `page_xxx_register()`
5. 在 `page_main_menu.c` 的 `menu_icons[]` 里把对应行 `.enabled = true`

**核心代码零改动**。

---

## 9. 给 Codex 的交接说明

### 先看这些文件（按顺序）

1. `README.md` — 项目总览
2. `HANDOVER.md` — 交接文档
3. `HARDWARE_REFERENCE.md` — 硬件引脚 + I2C 地址 + 分区表
4. `main/main.c` — 初始化流程（从这里理解模块调用顺序）
5. `main/bsp/esp32_s3_szp.h` — BSP API 声明
6. `main/pages/page_manager.h` — 页面注册机制
7. `main/event/app_event.h` — 事件类型
8. `main/player/player_core.h` — 播放器 API

### 不要改的东西

- **BSP 的 I2C 地址** — ES8311 控制字 `0x30`、ES7210 控制字 `0x82`，这些值已经适配 esp_codec_dev 内部右移逻辑
- **BSP 的 I2C 驱动** — 已适配 v6.0 新版 API，不要退回到旧的 `i2c_param_config()` 写法
- **LCD 引脚** — MOSI 40 / CLK 41 / DC 39 / 背光 42，CS 走 PCA9557
- **音频 I2S** — 当前使用 `I2S_NUM_1` 同时做 TX/RX，不要改成 `I2S_NUM_0`
- **分区表** — factory 8MB / storage 3MB，对齐官方 14-handheld 例程
- **sdkconfig.defaults** — `LV_FONT_FMT_TXT_LARGE=y` 和 `SUPPRESS_DEPRECATE_WARN=y` 不能删
- **components/ 下的 CMakeLists.txt** — 补了 v6.0 所需的 `esp_driver_*` 依赖和 SPI 宏修复

### 可以改的东西

- **pages/ 层** — 页面 UI 逻辑自由修改
- **player_core** — API 已经稳定，如需新功能可扩展
- **event/app_event.h** — 可追加新事件类型
- **主菜单图标配置** — `page_main_menu.c` 的 `menu_icons[]` 数组
- **初始化顺序** — `main.c` 的 `app_main()`，但注意依赖关系（I2C 必须在 LCD/音频之前，NVS 最先）

### 官方例程参考

| 例程 | 路径 |
|------|------|
| 11-mp3_player | `D:\Download\立创·实战派ESP32-S3开发板资料\01-例程\szpi-s3-esp\11-mp3_player\` |
| 14-handheld（最完整） | `D:\Download\立创·实战派ESP32-S3开发板资料\01-例程\szpi-s3-esp\14-handheld\` |

---

## 附录：最近 Git 提交

```
3f02c29 fix: 分区表factory扩至8M + 更新文档
1ba6031 fix: ESP-IDF v6.0 适配 — I2C驱动、中文字体、编译依赖
53e2f51 feat: MP3播放器第一阶段 — 完整可扩展架构
94839ff docs: 添加项目模板文件 + 交接文档
```
