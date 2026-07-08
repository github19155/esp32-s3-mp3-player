# 项目交接文档

> 从 Claude Code 迁移到 zcode，这份文档保证你能无缝继续。

---

## 环境检查清单

- [x] ESP-IDF v5.4 已安装
- [x] 项目位于 `e:\ai\esp32\`
- [x] 官方资料位于 `D:\Download\立创·实战派ESP32-S3开发板资料\`
- [x] Git 已初始化，代码已提交

---

## 项目状态（2026-07-08）

当前是 **空代码框架**，按架构设计逐步填充。

```
e:\ai\esp32\
├── .git/
├── .gitignore
├── README.md                 ← 项目总览 + 目标 + 架构
├── HARDWARE_REFERENCE.md     ← 硬件引脚速查表
├── HANDOVER.md               ← 这份交接文档
├── CMakeLists.txt             ← ESP-IDF 项目入口
├── sdkconfig.defaults         ← 芯片配置（S3/PSRAM/16MB Flash/LVGL）
├── partitions.csv             ← 分区表（nvs/factory/storage）
├── main/
│   ├── CMakeLists.txt         ← 主组件编译
│   ├── idf_component.yml      ← 组件依赖
│   ├── bsp/
│   │   ├── esp32_s3_szp.h     ← 硬件驱动头文件
│   │   └── esp32_s3_szp.c     ← 硬件驱动实现 (I2C/PCA9557/LCD/音频/SD)
│   ├── pages/                 ← 页面（待实现）
│   ├── player/                ← 播放器（待实现）
│   └── storage/               ← 存储（待实现）
```

---

## 打开项目后要做什么

### 1. 设目标芯片
```bash
idf.py set-target esp32s3
```

### 2. 装依赖
```bash
idf.py clean fullclean
idf.py build
```
第一次 build 会自动下载 `idf_component.yml` 里声明的依赖。

### 3. 编译 & 烧录
```bash
idf.py build flash monitor
```

---

## 核心依赖清单

| 组件 | 版本 | 用途 |
|------|------|------|
| `lvgl/lvgl` | ~8.3.0 | 图形库 |
| `espressif/esp_lvgl_port` | ~1.4.0 | LVGL 与 ESP 的桥接 |
| `espressif/esp_lcd_touch_ft5x06` | ~1.0.7 | 触摸驱动 |
| `chmorgan/esp-audio-player` | ~1.0.7 | MP3/WAV 解码播放 |
| `chmorgan/esp-file-iterator` | 1.0.0 | 文件列表遍历 |
| `espressif/esp_codec_dev` | ~1.3.0 | 音频编解码器驱动（ES8311/ES7210） |

---

## 开发顺序

按 README.md 的阶段规划，第一阶段只做 MP3 播放器：

1. **复制硬件驱动层**
   - 从 `D:\Download\立创·实战派ESP32-S3开发板资料\01-例程\szpi-s3-esp\14-handheld\main\` 把 `esp32_s3_szp.h` 和 `esp32_s3_szp.c` 拷到 `main/bsp/`
   - ⚠️ 注意：SD 卡部分的函数声明在 14-handheld 版本才完整

2. **写 `main/main.c`**
   - NVS 初始化
   - bsp 层初始化（I2C → PCA9557 → LCD/LVGL → 音频 → SD卡）
   - 启动主菜单页面

3. **写 `main/pages/page_main_menu.c`**
   - 参考官方 14-handheld 的 `lv_main_page()`
   - 先只放 1 个图标：MP3 播放器
   - 其他图标灰显占位

4. **写 `main/pages/page_mp3.c`**
   - 参考官方 11-mp3_player + 14-handheld 的 `music_event_handler()`
   - 改为从 SD 卡扫描文件（用 `opendir/readdir` 替代 file_iterator）

5. **写 `main/storage/sd_manager.c`**
   - SD 卡挂载/卸载/文件扫描
   - 参考官方 03-micro_sd + 14-handheld 的 `bsp_sdcard_mount()`

6. **写 `main/player/player_core.c`**
   - 封装 `esp-audio-player`，统一播放/暂停/切歌/音量接口

---

## 关键参考资料路径

| 资料 | 路径 |
|------|------|
| 官方 MP3 例程 | `D:\Download\立创·实战派ESP32-S3开发板资料\01-例程\szpi-s3-esp\11-mp3_player\` |
| 手持设备综合例程 | `D:\Download\立创·实战派ESP32-S3开发板资料\01-例程\szpi-s3-esp\14-handheld\` |
| 原理图 | `D:\Download\立创·实战派ESP32-S3开发板资料\02-文档\立创实战派ESP32-S3开发板原理图.pdf` |
| 芯片手册 | `D:\Download\立创·实战派ESP32-S3开发板资料\02-文档\02-芯片手册\` |

---

## 硬件引脚速查

> 完整版在 [HARDWARE_REFERENCE.md](HARDWARE_REFERENCE.md)

**最常用的几个：**

| 功能 | 引脚 |
|------|------|
| I2C SDA/SCL | GPIO 1 / GPIO 2 |
| LCD MOSI/CLK/DC/背光 | GPIO 40/41/39/42 |
| I2S MCLK/SCLK/LRCK/DOUT/SDIN | GPIO 38/14/13/45/12 |
| SD 卡 CLK/CMD/D0 | GPIO 47/48/21 |
| IO 扩展 I2C 地址 | 0x19 |
| 音频 DAC I2C 地址 | 0x18 |
| 音频 ADC I2C 地址 | 0x41（左声道）/ 0x82（右声道）|

---

## 踩坑提醒

1. **GPIO 0 被 BOOT 按键占用**，配置为输入上拉，不要用作输出
2. **LCD 的 CS 不是直连 ESP32**，是通过 PCA9557 IO 扩展芯片控制
3. **功放使能 PA_EN 也通过 PCA9557 控制**，播放前要拉高
4. **SD 卡用 1-bit SDMMC 模式**（只用 D0），不是 SPI 模式
5. **PSRAM 是 8MB Octal 模式**，sdkconfig 里要配对
6. **LVGL 用 PSRAM 做帧缓冲**，DMA 不能同时开启
7. **esp32_s3_szp.c 用的是新版 I2C 驱动** (`driver/i2c_master.h`)，不是旧版 `driver/i2c.h`
