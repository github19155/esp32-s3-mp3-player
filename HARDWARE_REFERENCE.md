# 立创实战派 ESP32-S3 硬件参考

> 适用项目：`E:\ai\esp32`
> 开发板：立创·实战派 ESP32-S3，主控 ESP32-S3-WROOM-1-N16R8
> 当前代码基线：ESP-IDF v6.0.1，`main/bsp/esp32_s3_szp.h/c`
> 官方资料入口：https://wiki.lckfb.com/zh-hans/szpi-esp32s3/
> 最后更新：2026-07-09

## 0. 阅读规则

这份文档分三种信息来源：

| 标记 | 含义 |
|------|------|
| 官方资料 | 来自立创实战派官方 wiki、例程、原理图说明 |
| 当前代码 | 当前仓库真实使用的宏、初始化流程、驱动写法 |
| 待确认 | 官方资料和当前代码存在差异，或者需要实物/串口日志确认 |

特别注意 I2C 地址有两种写法：

| 写法 | 说明 | 例子 |
|------|------|------|
| 7-bit 地址 | ESP-IDF 新 `i2c_master` 驱动直接使用 | QMI8658 `0x6A` |
| 8-bit 控制字 | `esp_codec_dev` 的 I2C 控制层参数会右移 1 位再注册设备 | ES8311 传 `0x30`，实际 7-bit 为 `0x18` |

## 1. 当前项目硬件结论

当前工程已经实际用到：

| 模块 | 状态 | 代码入口 |
|------|------|----------|
| LCD 显示 | 已启用 | `bsp_lvgl_start()` |
| 触摸 | 已启用 | `bsp_touch_new()` |
| SD 卡 | 已启用 | `sd_manager_mount()` -> `bsp_sdcard_mount()` |
| 音频播放 | 已启用 | `bsp_codec_init()` + `player_core_init()` |
| 音频采集 | 驱动已初始化，业务暂未用 | `record_dev_handle` |
| I2C 总线 | 已启用 | `bsp_i2c_init()` |
| PCA9557 IO 扩展 | 已启用 | `pca9557_init()` |
| QMI8658 姿态 | 驱动保留，页面未启用 | `qmi8658_init()` |
| 摄像头 DVP | 宏关闭 | `CAMERA_EN 0` |
| WiFi/BLE | ESP32-S3 内置，页面预留 | 未来页面模块 |

当前 `app_main()` 初始化顺序：

```c
nvs_flash_init();
bsp_i2c_init();
pca9557_init();
bsp_lvgl_start();
sd_manager_mount();
bsp_codec_init();
app_event_init(20);
player_core_init();
page_manager_init();
page_mp3_register();
page_main_menu_create();
```

## 2. 总线拓扑

```text
ESP32-S3
├─ I2C0: GPIO1 SDA, GPIO2 SCL, 100 kHz
│  ├─ QMI8658 IMU
│  ├─ PCA9557 IO 扩展
│  ├─ FT6336/FT5x06 触摸控制器
│  ├─ ES8311 音频 DAC 控制口
│  └─ ES7210 音频 ADC 控制口
├─ SPI3: LCD ST7789
│  ├─ MOSI GPIO40
│  ├─ CLK  GPIO41
│  ├─ DC   GPIO39
│  └─ CS   PCA9557 BIT0
├─ I2S1: 当前项目的音频 TX/RX
│  ├─ MCLK GPIO38
│  ├─ BCLK GPIO14
│  ├─ LRCK GPIO13
│  ├─ DOUT GPIO45, ESP32 -> ES8311
│  └─ SDIN GPIO12, ES7210 -> ESP32
└─ SDMMC 1-bit
   ├─ CLK GPIO47
   ├─ CMD GPIO48
   └─ D0  GPIO21
```

## 3. I2C 设备速查

| 设备 | 功能 | 7-bit 地址 | 当前代码写法 | 备注 |
|------|------|------------|--------------|------|
| QMI8658 | 6轴 IMU | `0x6A` | `QMI8658_SENSOR_ADDR 0x6A` | 新 I2C 驱动直接使用 7-bit |
| PCA9557 | IO 扩展 | `0x19` | `PCA9557_SENSOR_ADDR 0x19` | 新 I2C 驱动直接使用 7-bit |
| FT6336/FT5x06 | 触摸 | `0x38` | `ESP_LCD_TOUCH_IO_I2C_FT5x06_CONFIG()` | 官方资料常写 FT6336，当前组件名为 FT5x06 |
| ES8311 | 音频 DAC | `0x18` | `ES8311_CODEC_DEFAULT_ADDR 0x30` | `esp_codec_dev` 内部右移为 `0x18` |
| ES7210 | 音频 ADC | `0x41` 当前代码实际值 | `.addr = 0x82` | `esp_codec_dev` 内部右移为 `0x41` |

待确认：

- `esp_codec_dev` 自带 `ES7210_CODEC_DEFAULT_ADDR` 是 `0x80`，右移后是 `0x40`；当前项目手动传 `0x82`，右移后是 `0x41`。不要在没测 I2C 扫描前随意改回默认值。
- 如果以后写独立 I2C 扫描工具，扫描结果应按 7-bit 地址显示，音频芯片很可能看到 `0x18` 和 `0x41`。

## 4. PCA9557 IO 扩展

| PCA9557 位 | 宏 | 功能 | 上电/初始化状态 |
|------------|----|------|-----------------|
| BIT0 | `LCD_CS_GPIO` | LCD 片选 | `1`，空闲不选中 |
| BIT1 | `PA_EN_GPIO` | NS4150B 功放使能 | `0`，默认关闭 |
| BIT2 | `DVP_PWDN_GPIO` | 摄像头掉电控制 | `1`，默认掉电 |

当前初始化：

```c
pca9557_register_write_byte(PCA9557_OUTPUT_PORT, 0x05);
pca9557_register_write_byte(PCA9557_CONFIGURATION_PORT, 0xf8);
```

排查要点：

- LCD 不亮但背光亮：先看 `lcd_cs(0)` 是否被调用。
- 喇叭没声：当前 ES8311 驱动的 `.pa_pin` 是 `GPIO_NUM_NC`，功放实际由播放器事件回调里的 `pa_en(1)` 控制。
- 摄像头启用前要把 `DVP_PWDN` 拉到正确状态，且当前 `CAMERA_EN` 为 0。

## 5. LCD 和触摸

### LCD

| 项目 | 当前值 |
|------|--------|
| 接口 | SPI3 |
| 驱动 | ST7789 兼容 |
| 分辨率 | 320 x 240 |
| 色深 | RGB565，16 bit |
| 像素时钟 | 80 MHz |
| LVGL 刷新缓冲 | 320 x 20 行，双缓冲 |
| 背光 | GPIO42，LEDC 5 kHz，10 bit |

| 信号 | 引脚 |
|------|------|
| MOSI | GPIO40 |
| CLK | GPIO41 |
| DC | GPIO39 |
| CS | PCA9557 BIT0 |
| RST | NC |
| Backlight | GPIO42 |

当前旋转配置：

```c
esp_lcd_panel_swap_xy(panel_handle, true);
esp_lcd_panel_mirror(panel_handle, true, false);

.rotation = {
    .swap_xy = true,
    .mirror_x = true,
    .mirror_y = false,
}
```

### 触摸

| 项目 | 当前值 |
|------|--------|
| 控制器 | 官方资料常见 FT6336，当前代码使用 `esp_lcd_touch_ft5x06` |
| I2C 地址 | `0x38` |
| RST | NC |
| INT | NC |
| 触摸坐标 | 当前代码按横屏做 swap/mirror |

当前代码：

```c
esp_lcd_touch_config_t tp_cfg = {
    .x_max = BSP_LCD_V_RES,
    .y_max = BSP_LCD_H_RES,
    .rst_gpio_num = GPIO_NUM_NC,
    .int_gpio_num = GPIO_NUM_NC,
    .flags = {
        .swap_xy = 1,
        .mirror_x = 1,
        .mirror_y = 0,
    },
};
```

如果触摸方向不对，优先改 `swap_xy/mirror_x/mirror_y`，不要先改 LVGL 页面坐标。

## 6. 音频

### 硬件组成

| 芯片/模块 | 作用 |
|-----------|------|
| ES8311 | DAC，负责音频输出 |
| NS4150B | 功放，驱动喇叭 |
| ES7210 | ADC，负责麦克风采集 |
| 板载麦克风 | 官方资料描述为双麦克风/多通道采集，ES7210 可支持多路输入 |

### I2S 引脚

| 信号 | 引脚 | 方向 |
|------|------|------|
| MCLK | GPIO38 | ESP32 -> codec |
| BCLK/SCLK | GPIO14 | ESP32 -> codec |
| LRCK/WS | GPIO13 | ESP32 -> codec |
| DOUT | GPIO45 | ESP32 -> ES8311 |
| SDIN | GPIO12 | ES7210 -> ESP32 |

当前项目实际使用 `BSP_I2S_NUM I2S_NUM_1`，并在同一个 I2S 端口上创建 TX/RX：

```c
i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(BSP_I2S_NUM, I2S_ROLE_MASTER);
i2s_new_channel(&chan_cfg, &i2s_tx_chan, &i2s_rx_chan);
```

文档中如果看到“录音 I2S0、播放 I2S1”，要结合当前代码理解。当前工程并没有用 `I2S_NUM 0` 那组示例宏去初始化音频，真正工作的音频口是 `I2S_NUM_1`。

### 当前默认音频参数

| 参数 | 当前值 |
|------|--------|
| 默认采样率 | 16000 Hz |
| 默认位宽 | 16 bit |
| 默认通道 | `CODEC_DEFAULT_CHANNEL 2` |
| I2S slot | 32-bit stereo slot |
| 默认播放音量 | 60 |
| ADC 输入通道数 | `ADC_I2S_CHANNEL 4` |
| ADC 增益 | `CODEC_DEFAULT_ADC_VOLUME 24.0` |

### 功放控制

播放器事件回调负责打开/关闭功放：

```c
case AUDIO_PLAYER_CALLBACK_EVENT_PLAYING:
    pa_en(1);
    break;

case AUDIO_PLAYER_CALLBACK_EVENT_PAUSE:
    pa_en(0);
    break;
```

排查顺序：

1. 耳机有声、喇叭没声：查 `pa_en(1)` 是否触发，PCA9557 是否写成功。
2. 完全没声：查 `bsp_codec_init()`、ES8311 I2C 地址、I2S 引脚。
3. 播放变速或噪声：查 MP3 实际采样率是否触发 `_clk_set_fn()`，以及 `bsp_codec_set_fs()` 返回值。
4. 录音异常：查 ES7210 地址 `0x82` 控制字是否适配实物，查 MIC 通道映射。

## 7. SD 卡

| 项目 | 当前值 |
|------|--------|
| 接口 | SDMMC 1-bit |
| 挂载点 | `/sdcard` |
| 文件系统 | FAT |
| 最大打开文件 | 5 |
| 分配单元 | 8 KB |

| 信号 | 引脚 |
|------|------|
| CLK | GPIO47 |
| CMD | GPIO48 |
| D0 | GPIO21 |
| CD/DET | 未使用 |
| WP | 未使用 |

当前挂载代码：

```c
sdmmc_host_t sdmmc_host = SDMMC_HOST_DEFAULT();
sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
slot_config.width = 1;
slot_config.clk = SD_CLK_IO;
slot_config.cmd = SD_CMD_IO;
slot_config.d0 = SD_DAT0_IO;
slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;
```

当前业务只扫描 `/sdcard` 根目录下的常规文件，MP3 文件名过长会被跳过或截断。

## 8. QMI8658 姿态传感器

| 项目 | 当前值 |
|------|--------|
| I2C 地址 | `0x6A` |
| WHO_AM_I 预期 | `0x05` |
| 功能 | 3轴加速度 + 3轴陀螺仪 |
| 当前状态 | 驱动保留，页面未启用 |

已提供 API：

```c
esp_err_t qmi8658_init(void);
void qmi8658_close(void);
void qmi8658_fetch_angleFromAcc(t_sQMI8658 *p);
uint8_t qmi8658_fetch_motion(void);
```

添加姿态页面时，建议：

1. 在 `app_main()` 里注册 `page_attitude_register()`。
2. 在页面进入时调用 `qmi8658_init()`，退出时按需 `qmi8658_close()`。
3. 用定时器或任务低频读取，不要在 LVGL 绘制回调里直接阻塞 I2C。

## 9. 摄像头 DVP

当前 `CAMERA_EN` 为 0，摄像头相关代码不会编译。

| 信号 | 引脚 | 信号 | 引脚 |
|------|------|------|------|
| XCLK | GPIO5 | VSYNC | GPIO3 |
| SIOD | GPIO1 | HREF | GPIO46 |
| SIOC | GPIO2 | PCLK | GPIO7 |
| D0 | GPIO16 | D4 | GPIO15 |
| D1 | GPIO18 | D5 | GPIO6 |
| D2 | GPIO8 | D6 | GPIO4 |
| D3 | GPIO17 | D7 | GPIO9 |
| PWDN | PCA9557 BIT2 | RESET | NC |

启用摄像头前要确认：

- `CAMERA_EN` 改为 1 后，是否加入 `esp32-camera` 依赖。
- 摄像头和 LCD 同屏显示需要较多 PSRAM，确认 `sdkconfig` 中 PSRAM 已启用。
- GPIO1/2 同时作为主 I2C 和摄像头 SCCB，初始化顺序要避免重复安装总线。
- 当前 `esp32_s3_szp.c` 里 `#if CAMERA_EN` 直接 `#error`，这意味着摄像头不是简单打开宏就能工作，还需要补 BSP 实现。

## 10. WiFi 与 BLE

WiFi 和 BLE 均使用 ESP32-S3 内置射频，无额外 GPIO。

使用前要求：

```c
nvs_flash_init();
```

当前分区表中 `nvs` 为 24 KB，可用于 WiFi/BLE 配置信息。后续做联网功能时，建议不要在 BSP 文件里塞业务逻辑，按当前项目结构新增页面和服务模块。

## 11. SPIFFS

| 项目 | 当前值 |
|------|--------|
| 分区名 | `storage` |
| 挂载点 | `/spiffs` |
| 当前状态 | BSP 有挂载函数，主流程未调用 |

分区表：

| 分区 | 大小 | 说明 |
|------|------|------|
| `nvs` | 24 KB | WiFi/BLE/NVS |
| `phy_init` | 4 KB | RF 校准 |
| `factory` | 8 MB | 固件 |
| `storage` | 3 MB | SPIFFS |

## 12. GPIO 总表

| GPIO | 当前/预留功能 |
|------|---------------|
| 0 | BOOT 按键 |
| 1 | I2C SDA，摄像头 SIOD |
| 2 | I2C SCL，摄像头 SIOC |
| 3 | 摄像头 VSYNC |
| 4 | 摄像头 D6 |
| 5 | 摄像头 XCLK |
| 6 | 摄像头 D5 |
| 7 | 摄像头 PCLK |
| 8 | 摄像头 D2 |
| 9 | 摄像头 D7 |
| 12 | I2S SDIN，ES7210 -> ESP32 |
| 13 | I2S LRCK/WS |
| 14 | I2S BCLK/SCLK |
| 15 | 摄像头 D4 |
| 16 | 摄像头 D0 |
| 17 | 摄像头 D3 |
| 18 | 摄像头 D1 |
| 21 | SDMMC D0 |
| 38 | I2S MCLK |
| 39 | LCD DC |
| 40 | LCD MOSI |
| 41 | LCD CLK |
| 42 | LCD 背光 PWM |
| 45 | I2S DOUT，ESP32 -> ES8311 |
| 46 | 摄像头 HREF |
| 47 | SDMMC CLK |
| 48 | SDMMC CMD |

GPIO19/20 通常与 ESP32-S3 USB D-/D+ 相关，当前工程没有业务使用，开发时不要随意占用。

## 13. 添加新功能时的硬件初始化建议

| 功能 | 建议初始化点 | 注意事项 |
|------|--------------|----------|
| WiFi | 独立 `wifi_manager` 或页面进入时 | 依赖 NVS，避免阻塞 UI |
| BLE | 独立 `ble_manager` | 注意和 WiFi 共存内存 |
| 姿态页面 | 页面进入时 `qmi8658_init()` | 低频读数，避免卡 LVGL |
| 录音/语音识别 | 音频服务层 | 复用现有 `record_dev_handle`，确认通道映射 |
| 摄像头 | 新 BSP/服务模块 | 需要先移除当前 `#error` 并补实现 |
| SPIFFS 资源 | 启动时或按需挂载 | 需要烧录 SPIFFS 镜像 |

## 14. 给 AI 协作的硬件约束

让 Zcode、Codex 或其他模型改代码时，可以直接引用这些约束：

1. 不要改 LCD 引脚：MOSI GPIO40、CLK GPIO41、DC GPIO39、背光 GPIO42、CS 走 PCA9557 BIT0。
2. 不要把 ES8311 地址从 `0x30` 改成 `0x18`，因为当前 `esp_codec_dev` 控制层需要 8-bit 控制字。
3. 不要把 ES7210 当前 `.addr = 0x82` 改回默认值，除非先做 I2C 扫描并确认实物地址。
4. 当前音频实际使用 `I2S_NUM_1`，不是旧示例宏 `I2S_NUM 0`。
5. 功放使能走 `pa_en()`，不是 ES8311 `.pa_pin`。
6. 摄像头当前不可直接启用，`CAMERA_EN=1` 会触发 BSP 中的 `#error`。
7. UI 代码调用 LVGL 时保持使用 `lvgl_port_lock()` / `lvgl_port_unlock()`。

## 15. 官方资料与本项目差异记录

| 项目 | 官方资料常见描述 | 当前项目事实 | 处理建议 |
|------|------------------|--------------|----------|
| 触摸芯片 | FT6336 | 使用 `esp_lcd_touch_ft5x06` 组件，地址 `0x38` | 先保持当前可用驱动 |
| 音频 I2S | 可能按例程分 I2S0/I2S1 | 当前 `I2S_NUM_1` 同时 TX/RX | 以后重构音频前先测播放和录音 |
| ES7210 地址 | 资料可能写 `0x41` 或组件默认控制字 `0x80` | 当前代码传 `0x82`，实际 7-bit `0x41` | 保持当前写法，补 I2C scan 工具确认 |
| 摄像头 | 官方例程支持 | 当前项目关闭且 BSP 阻止编译 | 后续单独做摄像头分支 |

## 16. 建议后续补充

为了让硬件资料更可靠，建议后续加一个 `tools/i2c_scan` 或临时页面输出：

```text
I2C scan:
0x18 ES8311
0x19 PCA9557
0x38 touch
0x41 ES7210
0x6A QMI8658
```

拿到真实串口日志后，把扫描结果贴回本文件。这样以后 AI 或人工调试都能以实物为准。
