# 立创实战派 ESP32-S3 硬件配置速查表

> 来源：官方例程 `esp32_s3_szp.h` / `main.c` / 原理图（IDF 5.x）
> 最后更新：2026-07-08

---

## 一、整体架构速览

```
                    ESP32-S3
                       │
          ┌────────────┼────────────┐
          │            │            │
        I2C-0       I2S-0/1     SPI-3 (LCD)
     (GPIO1/2)    (GPIO13/14/38)  (GPIO40/41/39)
          │            │            │
     ┌────┴────┐   ┌───┴───┐   ┌──┴──────┐
     │         │   │       │   │         │
   QMI8658  PCA9557 ES7210 ES8311  2.4" LCD
   (0x6A)   (0x19)  (0x41) (0x18) (320×240)
     │         │
     │    ┌────┼────┐
     │    │    │    │
     │   CS   PA   DVP
   姿态   LCD 功放  摄像头
```

---

## 二、I2C 总线 (I2C-0)

```
SDA:  GPIO 1
SCL:  GPIO 2
频率: 100 kHz
```

| I2C 从设备 | 地址 | 功能 |
|------------|------|------|
| **QMI8658** | `0x6A` | 6轴姿态传感器（加速度+陀螺仪） |
| **PCA9557** | `0x19` | IO扩展芯片（LCD_CS / PA_EN / DVP_PWDN） |
| **ES7210** | `0x41` | 音频ADC，麦克风采集 |
| **ES8311** | `0x18` | 音频DAC，耳机输出 |
| **FT5x06** | — | 触摸控制器（部分例程） |

---

## 三、按键

| 引脚 | 功能 | 触发方式 | 说明 |
|------|------|---------|------|
| **GPIO 0** | BOOT 按键 | 下降沿中断 | 内部上拉，按下为低电平 |

> 代码示例（`01-boot_key/main/main.c`）

```c
gpio_config_t io0_conf = {
    .intr_type = GPIO_INTR_NEGEDGE,  // 下降沿中断
    .mode = GPIO_MODE_INPUT,
    .pin_bit_mask = 1 << GPIO_NUM_0,
    .pull_down_en = 0,
    .pull_up_en = 1                  // 使能内部上拉
};
gpio_config(&io0_conf);
gpio_isr_handler_add(GPIO_NUM_0, gpio_isr_handler, (void*) GPIO_NUM_0);
```

---

## 四、姿态传感器 QMI8658

| 参数 | 值 |
|------|-----|
| **I2C 地址** | `0x6A` |
| **类型** | 6轴 IMU（3轴加速度 + 3轴陀螺仪） |

**关键寄存器：**

| 寄存器 | 地址 | 说明 |
|--------|------|------|
| WHO_AM_I | 0x00 | 芯片ID |
| CTRL1~CTRL9 | 0x02~0x0A | 控制寄存器 |
| AX_L/H | 0x35/0x36 | 加速度 X 轴 |
| AY_L/H | 0x37/0x38 | 加速度 Y 轴 |
| AZ_L/H | 0x39/0x3A | 加速度 Z 轴 |
| GX_L/H | 0x3B/0x3C | 陀螺仪 X 轴 |
| GY_L/H | 0x3D/0x3E | 陀螺仪 Y 轴 |
| GZ_L/H | 0x3F/0x40 | 陀螺仪 Z 轴 |
| RESET | 0x60 | 软件复位 |

**API：**
```c
void qmi8658_init(void);                           // 初始化
void qmi8658_fetch_angleFromAcc(t_sQMI8658 *p);    // 获取倾角
uint8_t qmi8658_fetch_motion(void);                 // 获取运动状态（手持版）
```

---

## 五、IO 扩展芯片 PCA9557

| 参数 | 值 |
|------|-----|
| **I2C 地址** | `0x19` |

**扩展引脚分配：**

| PCA9557 引脚 | 宏定义 | 功能 |
|-------------|--------|------|
| GPIO 0 | `LCD_CS_GPIO` (BIT0) | LCD 片选信号 |
| GPIO 1 | `PA_EN_GPIO` (BIT1) | 功放使能 |
| GPIO 2 | `DVP_PWDN_GPIO` (BIT2) | 摄像头掉电控制 |

**API：**
```c
void pca9557_init(void);
void lcd_cs(uint8_t level);      // 控制 LCD 片选
void pa_en(uint8_t level);       // 控制功放使能
void dvp_pwdn(uint8_t level);    // 控制摄像头电源
```

---

## 六、LCD 显示屏

### 基本参数

| 参数 | 值 |
|------|-----|
| **接口** | SPI（SPI3_HOST） |
| **分辨率** | 320 × 240 |
| **色深** | 16 bit (RGB565) |
| **驱动芯片** | ST7789（兼容） |
| **触摸芯片** | FT5x06（I2C） |
| **像素时钟** | 80 MHz |
| **LVGL 缓冲高度** | 20 行 |

### 引脚定义

| 功能 | 引脚 | 说明 |
|------|------|------|
| **MOSI** | GPIO 40 | SPI 数据 |
| **CLK** | GPIO 41 | SPI 时钟 |
| **DC** | GPIO 39 | 数据/命令选择 |
| **CS** | PCA9557 BIT0 | 片选（通过 IO 扩展） |
| **RST** | NC | 未连接 |
| **背光** | GPIO 42 | LEDC_CHANNEL_0 PWM 调光 |

### API
```c
esp_err_t bsp_lcd_init(void);                         // LCD 初始化
void bsp_lvgl_start(void);                            // LVGL 初始化
esp_err_t bsp_display_brightness_set(int percent);     // 背光亮度 0~100
esp_err_t bsp_display_backlight_off(void);            // 关闭背光
esp_err_t bsp_display_backlight_on(void);             // 打开背光
void lcd_set_color(uint16_t color);                   // 全屏填充颜色
void lcd_draw_pictrue(x1, y1, x2, y2, data);         // 绘制图片
```

---

## 七、摄像头 (DVP)

| 参数 | 值 |
|------|-----|
| **接口** | DVP 并口（8位数据） |
| **XCLK** | 24 MHz |

### 引脚定义

| 功能 | 引脚 | 功能 | 引脚 |
|------|------|------|------|
| **XCLK** | GPIO 5 | **VSYNC** | GPIO 3 |
| **SIOD** | GPIO 1 (I2C SDA) | **HREF** | GPIO 46 |
| **SIOC** | GPIO 2 (I2C SCL) | **PCLK** | GPIO 7 |
| **D0** | GPIO 16 | **D4** | GPIO 15 |
| **D1** | GPIO 18 | **D5** | GPIO 6 |
| **D2** | GPIO 8 | **D6** | GPIO 4 |
| **D3** | GPIO 17 | **D7** | GPIO 9 |
| **PWDN** | PCA9557 BIT2 | **RESET** | -1（NC） |

> 注意：DVP 和 LCD 共享部分 SPI/I2C 资源，分时复用。
> 摄像头通过宏 `CAMERA_EN` (0/1) 控制是否编译。

---

## 八、I2S 音频总线

开发板支持 **2 路 I2S**：

| I2S 端口 | 用途 |
|----------|------|
| **I2S-0** | 录音（ES7210 麦克风） |
| **I2S-1** | 播放（ES8311 耳机 / NS4150B 功放） |

### 引脚定义（简化版录音 I2S-0）

| 功能 | 引脚 |
|------|------|
| **MCLK** | GPIO 38 |
| **BCLK** | GPIO 14 |
| **WS (LRCK)** | GPIO 13 |
| **DIN** | GPIO 12 |

### 引脚定义（完整音频 I2S-1）

| 功能 | 引脚 |
|------|------|
| **MCLK** | GPIO 38 |
| **SCLK (BCLK)** | GPIO 14 |
| **LRCK (WS)** | GPIO 13 |
| **SDIN** | GPIO 12（ADC 输入） |
| **DOUT** | GPIO 45（DAC 输出） |
| **PWR_CTRL** | NC |

> **关键配置：** ES8311 音频编解码器用 `I2S_NUM_1` 进行播放

---

## 九、音频编解码器 ES7210（麦克风/录音）

| 参数 | 值 |
|------|-----|
| **I2C 地址** | `0x41` |
| **接口** | I2S TDM 模式 |
| **采样率** | 48kHz (录音例程) / 16kHz (语音识别) |
| **位宽** | 16 bit |
| **通道数** | 2 |
| **MIC 增益** | 30dB |
| **MIC 偏置** | 2.87V |

---

## 十、音频编解码器 ES8311（播放/耳机）

| 参数 | 值 |
|------|-----|
| **I2C 地址** | `0x18` |
| **接口** | I2S 标准模式 |
| **默认采样率** | 16kHz~48kHz |
| **位宽** | 16 bit |
| **默认音量** | 60%（范围 0~100） |
| **功放芯片** | NS4150B |
| **功放使能** | PCA9557 BIT1 (`PA_EN`) |

### 音频播放 API
```c
esp_err_t bsp_codec_init(void);                  // 初始化音频编解码器
esp_err_t bsp_i2s_write(buf, len, &written, ms); // 写音频数据播放
esp_err_t bsp_codec_set_fs(rate, bits, ch);      // 设置采样率/位宽/通道
esp_err_t bsp_codec_mute_set(bool enable);       // 静音控制
esp_err_t bsp_codec_volume_set(vol, &set);       // 音量 0~100
esp_err_t bsp_speaker_set_fs(rate, bits, ch);    // 扬声器参数（手持版）
int bsp_get_feed_channel(void);                  // 获取音频输入通道数
esp_err_t bsp_get_feed_data(is_raw, buf, len);   // 获取麦克风数据（手持版）
```

---

## 十一、Micro SD 卡 (SDMMC)

| 参数 | 值 |
|------|-----|
| **接口** | SDMMC 1线模式 (SDMODE) |
| **挂载点** | `/sdcard` |

| 功能 | 引脚 |
|------|------|
| **CLK** | GPIO 47 |
| **CMD** | GPIO 48 |
| **D0** | GPIO 21 |
| **CD/DET** | 未使用 |
| **WP** | 未使用 |

### 挂载示例
```c
sdmmc_host_t host = SDMMC_HOST_DEFAULT();
sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
slot.width = 1;    // 1线SD模式
slot.clk  = 47;
slot.cmd  = 48;
slot.d0   = 21;
slot.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot, &mount_config, &card);
```

---

## 十二、WiFi

| 参数 | 说明 |
|------|------|
| **模式** | Station（STA） |
| **加密** | WPA2_PSK / WPA3_SAE |
| **最大重连** | 3 次 |

WiFi 使用 ESP32-S3 内置 WiFi，无额外引脚配置。**使用前必须初始化 NVS：**
```c
nvs_flash_init();
```

---

## 十三、BLE HID 设备

BLE 使用 ESP32-S3 内置蓝牙，无额外引脚配置。例程实现了一个蓝牙键盘设备。

---

## 十四、SPIFFS 文件系统

| 参数 | 值 |
|------|-----|
| **挂载点** | `/spiffs` |

用于存储 MP3 音频文件等静态资源。

---

## 十五、引脚功能汇总表（按 GPIO 号排序）

| GPIO | 功能 | 所属模块 |
|------|------|---------|
| **0** | BOOT 按键（输入，内部上拉） | 按键 |
| **1** | I2C SDA / 摄像头 SIOD | I2C / 摄像头 |
| **2** | I2C SCL / 摄像头 SIOC | I2C / 摄像头 |
| **3** | 摄像头 VSYNC | 摄像头 |
| **4** | 摄像头 D6 | 摄像头 |
| **5** | 摄像头 XCLK | 摄像头 |
| **6** | 摄像头 D5 | 摄像头 |
| **7** | 摄像头 PCLK | 摄像头 |
| **8** | 摄像头 D2 | 摄像头 |
| **9** | 摄像头 D7 | 摄像头 |
| **12** | I2S SDIN (麦克风数据输入) | 音频 |
| **13** | I2S WS / LRCK | 音频 |
| **14** | I2S BCLK / SCLK | 音频 |
| **15** | 摄像头 D4 | 摄像头 |
| **16** | 摄像头 D0 | 摄像头 |
| **17** | 摄像头 D3 | 摄像头 |
| **18** | 摄像头 D1 | 摄像头 |
| **21** | SD 卡 D0 | SD卡 |
| **38** | I2S MCLK | 音频 |
| **39** | LCD DC（数据/命令） | LCD |
| **40** | LCD MOSI（SPI 数据） | LCD |
| **41** | LCD CLK（SPI 时钟） | LCD |
| **42** | LCD 背光（PWM） | LCD |
| **45** | I2S DOUT（DAC 输出） | 音频 |
| **46** | 摄像头 HREF | 摄像头 |
| **47** | SD 卡 CLK | SD卡 |
| **48** | SD 卡 CMD | SD卡 |

---

## 十六、各例程入口速查

| 例程 | 需要初始化的模块 |
|------|------------------|
| **01-boot_key** | GPIO 中断 + 队列 |
| **02-attitude** | `bsp_i2c_init()` → `qmi8658_init()` |
| **03-micro_sd** | SDMMC 挂载 `/sdcard` |
| **04-audio_es7210** | I2C + I2S-0 + ES7210(0x41) + SD卡 |
| **05-audio_es8311** | I2C + I2S + ES8311(0x18) |
| **06-lcd** | I2C + PCA9557 + SPI3 + LCD |
| **07-lcd_camera** | I2C + PCA9557 + LCD + DVP摄像头 |
| **08-lcd_lvgl** | I2C + PCA9557 + LCD + LVGL |
| **09-wifi_scan_connect** | NVS + I2C + PCA9557 + LCD/LVGL + WiFi |
| **10-ble_hid_device** | BLE 协议栈 |
| **11-mp3_player** | NVS + I2C + PCA9557 + LCD + SPIFFS + 音频 + SD卡 |
| **12-speech_recognition** | NVS + I2C + PCA9557 + LCD + SPIFFS + 音频 |
| **13-human_face_detection** | I2C + PCA9557 + LCD + 摄像头 + ESP-DL |
| **14-handheld** | NVS + I2C + PCA9557 + LCD/LVGL + SPIFFS + 音频 + SD卡 + 姿态 |

---

## 十七、典型初始化顺序（通用模板）

```c
void app_main(void)
{
    // 1. NVS（WiFi/BLE 需要）
    nvs_flash_init();

    // 2. I2C 总线
    bsp_i2c_init();        // GPIO1=SDA, GPIO2=SCL, 100kHz

    // 3. IO 扩展芯片
    pca9557_init();        // LCD_CS, PA_EN, DVP_PWDN

    // 4. LCD + LVGL
    bsp_lvgl_start();      // 包含 bsp_lcd_init() + lvgl

    // 5. 文件系统
    bsp_spiffs_mount();    // /spiffs (如需要)

    // 6. 音频
    bsp_codec_init();      // ES8311 + ES7210

    // 7. SD 卡（如需要）
    bsp_sdcard_mount();

    // 8. 姿态传感器（如需要）
    qmi8658_init();
}
```
