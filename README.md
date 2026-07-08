# ESP32-S3 多功能掌机

> 基于立创·实战派 ESP32-S3 开发板
> 开发中 · 当前阶段：MP3 播放器

---

## 项目目标

做一个**带触摸屏的多功能掌机**，逐步迭代：

| 阶段 | 功能 | 状态 |
|------|------|------|
| **MP3 播放器** | SD 卡读取 MP3，触摸屏控制播放/暂停/切歌/音量 | 🔨 开发中 |
| WiFi 联网 | 配网、NTP 时间同步、天气显示 | ⏸ 计划中 |
| BLE 遥控 | 手机蓝牙遥控切歌、调音量 | ⏸ 计划中 |
| 摄像头 | 拍照、存 SD 卡 | ⏸ 计划中 |
| 姿态传感器 | 摇一摇切歌、运动检测 | ⏸ 计划中 |
| 语音控制 | 离线语音命令识别 | ⏸ 计划中 |

## 硬件资源

| 硬件 | 型号 | 用途 |
|------|------|------|
| 主控 | ESP32-S3 (双核 240MHz, 8MB PSRAM) | — |
| 屏幕 | 2.4" LCD 320×240 + FT5x06 触摸 | 显示、交互 |
| 音频输出 | ES8311 DAC + NS4150B 功放 | 播放音乐 |
| 音频输入 | ES7210 ADC × 2 通道 | 麦克风 |
| 存储 | Micro SD 卡 (SDMMC 1-bit) | 存放 MP3 |
| 传感器 | QMI8658 (6轴 IMU) | 姿态检测 |
| 摄像头 | DVP 并口摄像头 | 拍照 |
| 无线 | WiFi + BLE | 联网、遥控 |

> 完整硬件引脚定义见 [HARDWARE_REFERENCE.md](HARDWARE_REFERENCE.md)

## 架构设计

```
main/
├── main.c                  # 入口：初始化 + 启动主菜单
│
├── bsp/                    # 板级支持包（硬件初始化）
│   ├── bsp_board.h         # 引脚定义
│   └── bsp_board.c         # I2C/PCA9557/LCD/音频/SD 初始化
│
├── pages/                  # 页面层
│   ├── page_manager.h/c    # 页面管理器（枚举 + 注册）
│   ├── page_main_menu.c    # 主菜单（图标网格）
│   └── page_mp3.c          # MP3 播放页面
│
├── player/                 # 播放器核心
│   └── player_core.c       # 封装 audio_player 库
│
└── storage/                # 存储管理
    └── sd_manager.c        # SD 卡挂载 + 文件扫描
```

### 页面切换机制

采用官方例程的简单模式：

- 主菜单 `main_obj` 始终存在于屏幕底层
- 每个页面是覆盖在上的 `icon_in_obj` 容器
- 进入页面：创建容器 + 子控件
- 退出页面：`lv_obj_del(容器)` → 主菜单自然可见

### 事件流

```
触摸屏点击 → LVGL 事件回调 → 页面内处理
                              ↓
                          player_core（播放控制）
                              ↓
                          sd_manager（文件读取）
                              ↓
                          bsp_board（I2S 输出）
```

后续 BLE 遥控阶段，在 `page_manager` 层加入事件队列，统一分发。

## 开发环境

- **ESP-IDF**: v5.4
- **LVGL**: 通过 `esp_lvgl_port` 组件
- **编译器**: xtensa-esp32s3-elf-gcc

## 资料链接

- [硬件配置速查表](HARDWARE_REFERENCE.md)
- 官方例程：`D:\Download\立创·实战派ESP32-S3开发板资料\01-例程\`
- 原理图：`D:\Download\立创·实战派ESP32-S3开发板资料\02-文档\立创实战派ESP32-S3开发板原理图.pdf`
