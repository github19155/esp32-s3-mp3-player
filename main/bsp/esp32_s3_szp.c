#include <stdio.h>
#include "esp32_s3_szp.h"

static const char *TAG = "esp32_s3_szp";

/* ── 新版 I2C 驱动 (ESP-IDF v6.0: driver/i2c_master.h) ── */
#include "driver/i2c_master.h"
static i2c_master_bus_handle_t i2c_bus_handle = NULL;

/* ── 各 I2C 设备句柄 ── */
static i2c_master_dev_handle_t qmi8658_dev_handle = NULL;
static i2c_master_dev_handle_t pca9557_dev_handle = NULL;

/******************************************************************************/
/***************************  I2C ↓ *******************************************/
esp_err_t bsp_i2c_init(void)
{
    ESP_LOGI(TAG, "Starting I2C bus initialization (v6.0 new driver)...");

    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = BSP_I2C_NUM,
        .sda_io_num = BSP_I2C_SDA,
        .scl_io_num = BSP_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    esp_err_t ret = i2c_new_master_bus(&bus_cfg, &i2c_bus_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create I2C master bus: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "I2C bus initialized successfully (port=%d, scl=%d, sda=%d)",
             BSP_I2C_NUM, BSP_I2C_SCL, BSP_I2C_SDA);
    return ESP_OK;
}

/**
 * @brief 获取 I2C 总线句柄（供 esp_codec_dev / esp_lcd_touch 等组件使用）
 */
i2c_master_bus_handle_t bsp_i2c_get_bus_handle(void)
{
    return i2c_bus_handle;
}
/***************************  I2C ↑  *******************************************/
/*******************************************************************************/


/*******************************************************************************/
/***************************  姿态传感器 QMI8658 ↓   ****************************/

/* 为 QMI8658 在 I2C 总线上注册设备 */
static esp_err_t qmi8658_add_device(void)
{
    if (qmi8658_dev_handle != NULL) return ESP_OK;

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = QMI8658_SENSOR_ADDR,
        .scl_speed_hz    = BSP_I2C_FREQ_HZ,
    };
    return i2c_master_bus_add_device(i2c_bus_handle, &dev_cfg, &qmi8658_dev_handle);
}

/* 读取 QMI8658 寄存器 */
esp_err_t qmi8658_register_read(uint8_t reg_addr, uint8_t *data, size_t len)
{
    if (qmi8658_dev_handle == NULL) {
        esp_err_t ret = qmi8658_add_device();
        if (ret != ESP_OK) return ret;
    }
    return i2c_master_transmit_receive(qmi8658_dev_handle, &reg_addr, 1, data, len, 1000 / portTICK_PERIOD_MS);
}

/* 写 QMI8658 寄存器 */
esp_err_t qmi8658_register_write_byte(uint8_t reg_addr, uint8_t data)
{
    if (qmi8658_dev_handle == NULL) {
        esp_err_t ret = qmi8658_add_device();
        if (ret != ESP_OK) return ret;
    }
    uint8_t write_buf[2] = {reg_addr, data};
    return i2c_master_transmit(qmi8658_dev_handle, write_buf, sizeof(write_buf), 1000 / portTICK_PERIOD_MS);
}

/* 初始化 qmi8658 */
esp_err_t qmi8658_init(void)
{
    esp_err_t ret = ESP_OK;
    uint8_t id = 0;
    uint8_t count = 0;

    /* 确保设备已注册 */
    ret = qmi8658_add_device();
    if (ret != ESP_OK) return ret;

    qmi8658_register_read(QMI8658_WHO_AM_I, &id, 1);
    while (id != 0x05) {
        vTaskDelay(100 / portTICK_PERIOD_MS);
        qmi8658_register_read(QMI8658_WHO_AM_I, &id, 1);
        count++;
        if (count >= 3) {
            ret = ESP_FAIL;
            return ret;
        }
    }
    ESP_LOGI(TAG, "QMI8658 OK!");

    qmi8658_register_write_byte(QMI8658_RESET, 0xb0);
    vTaskDelay(10 / portTICK_PERIOD_MS);

    /* 配置运动状态检测 */
    qmi8658_register_write_byte(QMI8658_CATL1_L, 1);
    qmi8658_register_write_byte(QMI8658_CATL1_H, 1);
    qmi8658_register_write_byte(QMI8658_CATL2_L, 1);
    qmi8658_register_write_byte(QMI8658_CATL2_H, 1);
    qmi8658_register_write_byte(QMI8658_CATL3_L, 1);
    qmi8658_register_write_byte(QMI8658_CATL3_H, 1);
    qmi8658_register_write_byte(QMI8658_CATL4_L, 0x77);
    qmi8658_register_write_byte(QMI8658_CATL4_H, 0x01);
    qmi8658_register_write_byte(QMI8658_CTRL9, 0x0E);

    qmi8658_register_write_byte(QMI8658_CATL1_L, 1);
    qmi8658_register_write_byte(QMI8658_CATL1_H, 1);
    qmi8658_register_write_byte(QMI8658_CATL2_L, 0xE8);
    qmi8658_register_write_byte(QMI8658_CATL2_H, 0x03);
    qmi8658_register_write_byte(QMI8658_CATL3_L, 0xE8);
    qmi8658_register_write_byte(QMI8658_CATL3_H, 0x03);
    qmi8658_register_write_byte(QMI8658_CATL4_H, 0x02);
    qmi8658_register_write_byte(QMI8658_CTRL9, 0x0E);

    qmi8658_register_write_byte(QMI8658_CTRL1, 0x40);
    qmi8658_register_write_byte(QMI8658_CTRL7, 0x03);
    qmi8658_register_write_byte(QMI8658_CTRL2, 0x95);
    qmi8658_register_write_byte(QMI8658_CTRL3, 0xd5);
    qmi8658_register_write_byte(QMI8658_CTRL8, 0x0E);

    return ret;
}

/* 关闭芯片运行 */
void qmi8658_close(void)
{
    qmi8658_register_write_byte(QMI8658_CTRL1, 0x01);
}

/* 读取加速度和陀螺仪寄存器值 */
void qmi8658_Read_AccAndGry(t_sQMI8658 *p)
{
    uint8_t status, data_ready = 0;
    int16_t buf[6];

    qmi8658_register_read(QMI8658_STATUS0, &status, 1);
    if (status & 0x03)
        data_ready = 1;
    if (data_ready == 1) {
        data_ready = 0;
        qmi8658_register_read(QMI8658_AX_L, (uint8_t *)buf, 12);
        p->acc_x = buf[0];
        p->acc_y = buf[1];
        p->acc_z = buf[2];
        p->gyr_x = buf[3];
        p->gyr_y = buf[4];
        p->gyr_z = buf[5];
    }
}

/* 获取XYZ轴的倾角值 */
void qmi8658_fetch_angleFromAcc(t_sQMI8658 *p)
{
    float temp;
    qmi8658_Read_AccAndGry(p);
    temp = (float)p->acc_x / sqrt(((float)p->acc_y * (float)p->acc_y + (float)p->acc_z * (float)p->acc_z));
    p->AngleX = atan(temp) * 57.29578f;
    temp = (float)p->acc_y / sqrt(((float)p->acc_x * (float)p->acc_x + (float)p->acc_z * (float)p->acc_z));
    p->AngleY = atan(temp) * 57.29578f;
    temp = sqrt(((float)p->acc_x * (float)p->acc_x + (float)p->acc_y * (float)p->acc_y)) / (float)p->acc_z;
    p->AngleZ = atan(temp) * 57.29578f;
}

/* 获取Motion状态 */
uint8_t qmi8658_fetch_motion(void)
{
    uint8_t status = 0;
    qmi8658_register_read(QMI8658_STATUS1, &status, 1);
    return status;
}

/***************************  姿态传感器 QMI8658 ↑  ****************************/
/*******************************************************************************/


/***********************************************************/
/***************    IO扩展芯片 ↓   *************************/

/* 为 PCA9557 在 I2C 总线上注册设备 */
static esp_err_t pca9557_add_device(void)
{
    if (pca9557_dev_handle != NULL) return ESP_OK;

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = PCA9557_SENSOR_ADDR,
        .scl_speed_hz    = BSP_I2C_FREQ_HZ,
    };
    return i2c_master_bus_add_device(i2c_bus_handle, &dev_cfg, &pca9557_dev_handle);
}

/* 读取PCA9557寄存器 */
esp_err_t pca9557_register_read(uint8_t reg_addr, uint8_t *data, size_t len)
{
    if (pca9557_dev_handle == NULL) {
        esp_err_t ret = pca9557_add_device();
        if (ret != ESP_OK) return ret;
    }
    return i2c_master_transmit_receive(pca9557_dev_handle, &reg_addr, 1, data, len, 1000 / portTICK_PERIOD_MS);
}

/* 写PCA9557寄存器 */
esp_err_t pca9557_register_write_byte(uint8_t reg_addr, uint8_t data)
{
    if (pca9557_dev_handle == NULL) {
        esp_err_t ret = pca9557_add_device();
        if (ret != ESP_OK) return ret;
    }
    uint8_t write_buf[2] = {reg_addr, data};
    return i2c_master_transmit(pca9557_dev_handle, write_buf, sizeof(write_buf), 1000 / portTICK_PERIOD_MS);
}

/* 初始化PCA9557 */
void pca9557_init(void)
{
    pca9557_add_device();
    /* DVP_PWDN=1  PA_EN=0  LCD_CS=1 */
    pca9557_register_write_byte(PCA9557_OUTPUT_PORT, 0x05);
    /* IO0 IO1 IO2 设置为输出，其他保持输入 */
    pca9557_register_write_byte(PCA9557_CONFIGURATION_PORT, 0xf8);
}

/* 设置PCA9557某个IO引脚输出高低电平 */
esp_err_t pca9557_set_output_state(uint8_t gpio_bit, uint8_t level)
{
    uint8_t data;
    pca9557_register_read(PCA9557_OUTPUT_PORT, &data, 1);
    return pca9557_register_write_byte(PCA9557_OUTPUT_PORT, SET_BITS(data, gpio_bit, level));
}

void lcd_cs(uint8_t level)   { pca9557_set_output_state(LCD_CS_GPIO, level); }
void pa_en(uint8_t level)    { pca9557_set_output_state(PA_EN_GPIO, level); }
void dvp_pwdn(uint8_t level) { pca9557_set_output_state(DVP_PWDN_GPIO, level); }

/***************    IO扩展芯片 ↑   *************************/
/***********************************************************/


/***********************************************************/
/****************    LCD显示屏 ↓   *************************/

/* 背光PWM初始化 */
esp_err_t bsp_display_brightness_init(void)
{
    const ledc_channel_config_t LCD_backlight_channel = {
        .gpio_num = BSP_LCD_BACKLIGHT,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LCD_LEDC_CH,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = 0,
        .duty = 0,
        .hpoint = 0,
        .flags.output_invert = true
    };
    const ledc_timer_config_t LCD_backlight_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = 0,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK
    };

    ESP_ERROR_CHECK(ledc_timer_config(&LCD_backlight_timer));
    ESP_ERROR_CHECK(ledc_channel_config(&LCD_backlight_channel));
    return ESP_OK;
}

/* 背光亮度设置 */
esp_err_t bsp_display_brightness_set(int brightness_percent)
{
    if (brightness_percent > 100) brightness_percent = 100;
    else if (brightness_percent < 0) brightness_percent = 0;

    ESP_LOGI(TAG, "Setting LCD backlight: %d%%", brightness_percent);
    uint32_t duty_cycle = (1023 * brightness_percent) / 100;
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LCD_LEDC_CH, duty_cycle));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LCD_LEDC_CH));
    return ESP_OK;
}

esp_err_t bsp_display_backlight_off(void) { return bsp_display_brightness_set(0); }
esp_err_t bsp_display_backlight_on(void)  { return bsp_display_brightness_set(100); }

/* 液晶屏句柄 */
static esp_lcd_panel_handle_t panel_handle = NULL;
esp_lcd_panel_io_handle_t io_handle = NULL;
static esp_lcd_touch_handle_t tp;
static lv_disp_t *disp;
static lv_indev_t *disp_indev = NULL;

/* 液晶屏初始化 */
esp_err_t bsp_display_new(void)
{
    esp_err_t ret = ESP_OK;
    ESP_RETURN_ON_ERROR(bsp_display_brightness_init(), TAG, "Brightness init failed");

    /* 初始化SPI总线 */
    ESP_LOGD(TAG, "Initialize SPI bus");
    const spi_bus_config_t buscfg = {
        .sclk_io_num = BSP_LCD_SPI_CLK,
        .mosi_io_num = BSP_LCD_SPI_MOSI,
        .miso_io_num = GPIO_NUM_NC,
        .quadwp_io_num = GPIO_NUM_NC,
        .quadhd_io_num = GPIO_NUM_NC,
        .max_transfer_sz = BSP_LCD_H_RES * 40 * sizeof(uint16_t),
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(BSP_LCD_SPI_NUM, &buscfg, SPI_DMA_CH_AUTO), TAG, "SPI init failed");

    /* 液晶屏控制IO初始化 */
    ESP_LOGD(TAG, "Install panel IO");
    const esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = BSP_LCD_DC,
        .cs_gpio_num = BSP_LCD_SPI_CS,
        .pclk_hz = BSP_LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits = LCD_CMD_BITS,
        .lcd_param_bits = LCD_PARAM_BITS,
        .spi_mode = 2,
        .trans_queue_depth = 10,
    };
    ESP_GOTO_ON_ERROR(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)BSP_LCD_SPI_NUM, &io_config, &io_handle),
                      err, TAG, "New panel IO failed");

    /* 初始化液晶屏驱动芯片ST7789 */
    ESP_LOGD(TAG, "Install LCD driver");
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = BSP_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = BSP_LCD_BITS_PER_PIXEL,
    };
    ESP_GOTO_ON_ERROR(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle), err, TAG, "New panel failed");

    esp_lcd_panel_reset(panel_handle);
    lcd_cs(0);
    esp_lcd_panel_init(panel_handle);
    esp_lcd_panel_invert_color(panel_handle, true);
    esp_lcd_panel_swap_xy(panel_handle, true);
    esp_lcd_panel_mirror(panel_handle, true, false);

    return ret;

err:
    if (panel_handle) esp_lcd_panel_del(panel_handle);
    if (io_handle)    esp_lcd_panel_io_del(io_handle);
    spi_bus_free(BSP_LCD_SPI_NUM);
    return ret;
}

/* LCD显示初始化 */
esp_err_t bsp_lcd_init(void)
{
    esp_err_t ret = bsp_display_new();
    lcd_set_color(0x0000);
    ret = esp_lcd_panel_disp_on_off(panel_handle, true);
    ret = bsp_display_backlight_on();
    return ret;
}

/* 液晶屏初始化+添加LVGL接口 */
static lv_disp_t *bsp_display_lcd_init(void)
{
    bsp_display_new();
    lcd_set_color(0xffff);
    esp_lcd_panel_disp_on_off(panel_handle, true);

    ESP_LOGD(TAG, "Add LCD screen");
    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io_handle,
        .panel_handle = panel_handle,
        .buffer_size = BSP_LCD_H_RES * BSP_LCD_DRAW_BUF_HEIGHT,
        .double_buffer = true,
        .hres = BSP_LCD_H_RES,
        .vres = BSP_LCD_V_RES,
        .monochrome = false,
        .rotation = {
            .swap_xy = true,
            .mirror_x = true,
            .mirror_y = false,
        },
        .flags = {
            .buff_dma = true,
            .buff_spiram = false,
        }
    };
    return lvgl_port_add_disp(&disp_cfg);
}

/* 触摸屏初始化 (ESP-IDF v6.0: 使用 i2c_master_bus_handle_t) */
esp_err_t bsp_touch_new(esp_lcd_touch_handle_t *ret_touch)
{
    ESP_LOGI(TAG, "Starting touch screen initialization...");

    esp_lcd_touch_config_t tp_cfg = {
        .x_max = BSP_LCD_V_RES,
        .y_max = BSP_LCD_H_RES,
        .rst_gpio_num = GPIO_NUM_NC,
        .int_gpio_num = GPIO_NUM_NC,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = 1,
            .mirror_x = 1,
            .mirror_y = 0,
        },
    };
    esp_lcd_panel_io_handle_t tp_io_handle = NULL;
    esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_FT5x06_CONFIG();
    tp_io_config.scl_speed_hz = 400000;

    ESP_LOGI(TAG, "Creating touch screen I2C IO with bus handle");
    /* v6.0 API: esp_lcd_new_panel_io_i2c 接受 i2c_master_bus_handle_t */
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i2c(i2c_bus_handle, &tp_io_config, &tp_io_handle), TAG, "");

    ESP_LOGI(TAG, "Creating FT5x06 touch screen driver");
    ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_ft5x06(tp_io_handle, &tp_cfg, ret_touch));

    ESP_LOGI(TAG, "Touch screen initialized successfully");
    return ESP_OK;
}

/* 触摸屏初始化+添加LVGL接口 */
static lv_indev_t *bsp_display_indev_init(lv_disp_t *disp)
{
    ESP_ERROR_CHECK(bsp_touch_new(&tp));
    assert(tp);

    const lvgl_port_touch_cfg_t touch_cfg = {
        .disp = disp,
        .handle = tp,
    };
    return lvgl_port_add_touch(&touch_cfg);
}

/* 开发板显示初始化 */
void bsp_lvgl_start(void)
{
    lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    lvgl_port_init(&lvgl_cfg);

    disp = bsp_display_lcd_init();
    disp_indev = bsp_display_indev_init(disp);
    bsp_display_backlight_on();
}

/* 显示图片 */
void lcd_draw_pictrue(int x_start, int y_start, int x_end, int y_end, const unsigned char *gImage)
{
    size_t pixels_byte_size = (x_end - x_start) * (y_end - y_start) * 2;
    uint16_t *pixels = (uint16_t *)heap_caps_malloc(pixels_byte_size, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
    if (NULL == pixels) {
        ESP_LOGE(TAG, "Memory for bitmap is not enough");
        return;
    }
    memcpy(pixels, gImage, pixels_byte_size);
    esp_lcd_panel_draw_bitmap(panel_handle, x_start, y_start, x_end, y_end, (uint16_t *)pixels);
    heap_caps_free(pixels);
}

/* 设置液晶屏颜色 */
void lcd_set_color(uint16_t color)
{
    uint16_t *buffer = (uint16_t *)heap_caps_malloc(BSP_LCD_H_RES * sizeof(uint16_t), MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
    if (NULL == buffer) {
        ESP_LOGE(TAG, "Memory for bitmap is not enough");
    } else {
        for (size_t i = 0; i < BSP_LCD_H_RES; i++) buffer[i] = color;
        for (int y = 0; y < 240; y++)
            esp_lcd_panel_draw_bitmap(panel_handle, 0, y, 320, y + 1, buffer);
        free(buffer);
    }
}
/***************    LCD显示屏 ↑   *************************/
/***********************************************************/


/***********************************************************/
/****************    摄像头 ↓   ****************************/
/* CAMERA_EN=0，摄像头代码不编译 */
#if CAMERA_EN
#error "Camera not supported in this build (CAMERA_EN=0)"
#endif
/********************    摄像头 ↑   *************************/
/***********************************************************/


/***********************************************************/
/***************    SPIFFS文件系统 ↓   *********************/

esp_err_t bsp_spiffs_mount(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = SPIFFS_BASE,
        .partition_label = "storage",
        .max_files = 5,
        .format_if_mount_failed = false,
    };

    esp_err_t ret_val = esp_vfs_spiffs_register(&conf);
    ESP_ERROR_CHECK(ret_val);

    size_t total = 0, used = 0;
    ret_val = esp_spiffs_info(conf.partition_label, &total, &used);
    if (ret_val != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret_val));
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }
    return ret_val;
}

/***************    SPIFFS文件系统 ↑  *********************/
/**********************************************************/


/***********************************************************/
/*********************    SD卡  ↓   *********************/
sdmmc_card_t *sdmmc_card = NULL;

esp_err_t bsp_sdcard_mount(void)
{
    ESP_LOGI(TAG, "Mounting SD card");
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 8 * 1024
    };

    sdmmc_host_t sdmmc_host = SDMMC_HOST_DEFAULT();
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.width = 1;
    slot_config.clk = SD_CLK_IO;
    slot_config.cmd = SD_CMD_IO;
    slot_config.d0 = SD_DAT0_IO;
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    ESP_LOGI(TAG, "Mounting filesystem Starting");
    return esp_vfs_fat_sdmmc_mount(SD_MOUNT_POINT, &sdmmc_host, &slot_config, &mount_config, &sdmmc_card);
}

esp_err_t bsp_sdcard_unmount(void)
{
    return esp_vfs_fat_sdcard_unmount(SD_MOUNT_POINT, sdmmc_card);
}
/**********************    SD卡 ↑  ************************/
/**********************************************************/


/***********************************************************/
/*********************    音频 ↓   *************************/
static esp_codec_dev_handle_t play_dev_handle;
static esp_codec_dev_handle_t record_dev_handle;
static i2s_chan_handle_t i2s_tx_chan = NULL;
static i2s_chan_handle_t i2s_rx_chan = NULL;
static const audio_codec_data_if_t *i2s_data_if = NULL;

/* I2S总线初始化 */
esp_err_t bsp_audio_init(void)
{
    if (i2s_tx_chan && i2s_rx_chan) return ESP_OK;

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(BSP_I2S_NUM, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true;
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &i2s_tx_chan, &i2s_rx_chan));

    const i2s_std_config_t std_cfg_default = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(16000),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(32, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = GPIO_I2S_MCLK,
            .bclk = GPIO_I2S_SCLK,
            .ws   = GPIO_I2S_LRCK,
            .dout = GPIO_I2S_DOUT,
            .din  = GPIO_I2S_SDIN,
        },
    };

    if (i2s_tx_chan != NULL) {
        ESP_ERROR_CHECK(i2s_channel_init_std_mode(i2s_tx_chan, &std_cfg_default));
        ESP_ERROR_CHECK(i2s_channel_enable(i2s_tx_chan));
    }
    if (i2s_rx_chan != NULL) {
        ESP_ERROR_CHECK(i2s_channel_init_std_mode(i2s_rx_chan, &std_cfg_default));
        ESP_ERROR_CHECK(i2s_channel_enable(i2s_rx_chan));
    }

    audio_codec_i2s_cfg_t i2s_cfg = {
        .port = BSP_I2S_NUM,
        .rx_handle = i2s_rx_chan,
        .tx_handle = i2s_tx_chan,
    };
    i2s_data_if = audio_codec_new_i2s_data(&i2s_cfg);
    assert(i2s_data_if);

    return ESP_OK;
}

/* 初始化音频输出芯片 */
esp_codec_dev_handle_t bsp_audio_codec_speaker_init(void)
{
    ESP_LOGI(TAG, "Starting audio codec speaker initialization...");

    if (i2s_data_if == NULL) {
        ESP_LOGI(TAG, "Initializing I2S peripheral...");
        ESP_ERROR_CHECK(bsp_audio_init());
    }
    assert(i2s_data_if);

    const audio_codec_gpio_if_t *gpio_if = audio_codec_new_gpio();

    ESP_LOGI(TAG, "Creating I2C control interface for ES8311 codec");
    audio_codec_i2c_cfg_t i2c_cfg = {
        .port = BSP_I2C_NUM,
        .bus_handle = i2c_bus_handle,
        .addr = ES8311_CODEC_DEFAULT_ADDR,
    };
    const audio_codec_ctrl_if_t *i2c_ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);
    assert(i2c_ctrl_if);
    ESP_LOGI(TAG, "I2C control interface created successfully");

    esp_codec_dev_hw_gain_t gain = {
        .pa_voltage = 5.0,
        .codec_dac_voltage = 3.3,
    };

    es8311_codec_cfg_t es8311_cfg = {
        .ctrl_if = i2c_ctrl_if,
        .gpio_if = gpio_if,
        .codec_mode = ESP_CODEC_DEV_WORK_MODE_DAC,
        .pa_pin = GPIO_PWR_CTRL,
        .pa_reverted = false,
        .master_mode = false,
        .use_mclk = true,
        .digital_mic = false,
        .invert_mclk = false,
        .invert_sclk = false,
        .hw_gain = gain,
    };
    const audio_codec_if_t *es8311_dev = es8311_codec_new(&es8311_cfg);
    assert(es8311_dev);

    esp_codec_dev_cfg_t codec_dev_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_OUT,
        .codec_if = es8311_dev,
        .data_if = i2s_data_if,
    };
    return esp_codec_dev_new(&codec_dev_cfg);
}

/* 初始化音频输入芯片 */
esp_codec_dev_handle_t bsp_audio_codec_microphone_init(void)
{
    ESP_LOGI(TAG, "Starting audio codec microphone initialization...");

    if (i2s_data_if == NULL) {
        ESP_LOGI(TAG, "Initializing I2S peripheral...");
        ESP_ERROR_CHECK(bsp_audio_init());
    }
    assert(i2s_data_if);

    ESP_LOGI(TAG, "Creating I2C control interface for ES7210 codec");
    audio_codec_i2c_cfg_t i2c_cfg = {
        .port = BSP_I2C_NUM,
        .bus_handle = i2c_bus_handle,
        .addr = 0x82,
    };
    const audio_codec_ctrl_if_t *i2c_ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);
    assert(i2c_ctrl_if);
    ESP_LOGI(TAG, "I2C control interface created successfully");

    es7210_codec_cfg_t es7210_cfg = {
        .ctrl_if = i2c_ctrl_if,
        .mic_selected = ES7120_SEL_MIC1 | ES7120_SEL_MIC2 | ES7120_SEL_MIC3 | ES7120_SEL_MIC4,
    };
    const audio_codec_if_t *es7210_dev = es7210_codec_new(&es7210_cfg);
    assert(es7210_dev);

    esp_codec_dev_cfg_t codec_es7210_dev_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_IN,
        .codec_if = es7210_dev,
        .data_if = i2s_data_if,
    };
    return esp_codec_dev_new(&codec_es7210_dev_cfg);
}

/* 设置采样率 */
esp_err_t bsp_codec_set_fs(uint32_t rate, uint32_t bits_cfg, i2s_slot_mode_t ch)
{
    esp_err_t ret = ESP_OK;

    esp_codec_dev_sample_info_t fs = {
        .sample_rate = rate,
        .channel = ch,
        .bits_per_sample = bits_cfg,
    };

    if (play_dev_handle)   ret = esp_codec_dev_close(play_dev_handle);
    if (record_dev_handle) {
        ret |= esp_codec_dev_close(record_dev_handle);
        ret |= esp_codec_dev_set_in_gain(record_dev_handle, CODEC_DEFAULT_ADC_VOLUME);
    }
    if (play_dev_handle)   ret |= esp_codec_dev_open(play_dev_handle, &fs);
    if (record_dev_handle) ret |= esp_codec_dev_open(record_dev_handle, &fs);
    return ret;
}

/* 音频芯片初始化 */
esp_err_t bsp_codec_init(void)
{
    play_dev_handle = bsp_audio_codec_speaker_init();
    assert((play_dev_handle) && "play_dev_handle not initialized");

    record_dev_handle = bsp_audio_codec_microphone_init();
    assert((record_dev_handle) && "record_dev_handle not initialized");

    bsp_codec_set_fs(CODEC_DEFAULT_SAMPLE_RATE, CODEC_DEFAULT_BIT_WIDTH, CODEC_DEFAULT_CHANNEL);
    esp_codec_dev_set_out_vol(play_dev_handle, VOLUME_DEFAULT);

    return ESP_OK;
}

/* 播放音乐 */
esp_err_t bsp_i2s_write(void *audio_buffer, size_t len, size_t *bytes_written, uint32_t timeout_ms)
{
    (void)timeout_ms;
    esp_err_t ret = esp_codec_dev_write(play_dev_handle, audio_buffer, len);
    *bytes_written = len;
    return ret;
}

/* 设置静音 */
esp_err_t bsp_codec_mute_set(bool enable)
{
    return esp_codec_dev_set_out_mute(play_dev_handle, enable);
}

/* 设置喇叭音量 */
esp_err_t bsp_codec_volume_set(int volume, int *volume_set)
{
    (void)volume_set;
    return esp_codec_dev_set_out_vol(play_dev_handle, volume);
}

/* 获取MIC通道数 */
int bsp_get_feed_channel(void)
{
    return ADC_I2S_CHANNEL;
}

/* 获取MIC数据 */
esp_err_t bsp_get_feed_data(bool is_get_raw_channel, int16_t *buffer, int buffer_len)
{
    int audio_chunksize = buffer_len / (sizeof(int16_t) * ADC_I2S_CHANNEL);
    esp_err_t ret = esp_codec_dev_read(record_dev_handle, (void *)buffer, buffer_len);

    if (!is_get_raw_channel) {
        for (int i = 0; i < audio_chunksize; i++) {
            int16_t ref = buffer[4 * i + 0];
            buffer[3 * i + 0] = buffer[4 * i + 1];
            buffer[3 * i + 1] = buffer[4 * i + 3];
            buffer[3 * i + 2] = ref;
        }
    }
    return ret;
}

/*********************    音频 ↑   *************************/
/***********************************************************/
