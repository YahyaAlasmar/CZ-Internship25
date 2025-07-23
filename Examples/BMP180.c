#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "bmp180.h"
#include "i2cdev.h"
#include "esp_log.h"
#include "esp_err.h"

#define I2C_MASTER_SCL_IO          19  // Changed from 22
#define I2C_MASTER_SDA_IO          18  // Changed from 21
#define I2C_MASTER_NUM             I2C_NUM_0
#define I2C_MASTER_FREQ_HZ         50000
#define I2C_MASTER_TX_BUF_DISABLE  0
#define I2C_MASTER_RX_BUF_DISABLE  0

static const char *TAG = "BMP180";

void app_main(void)
{
    float temperature;
    uint32_t pressure;
    bmp180_dev_t dev;

    // Clean up any old driver (safe init)
    i2c_driver_delete(I2C_MASTER_NUM);

    // I2C configuration
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
        .clk_flags = 0
    };

    ESP_ERROR_CHECK(i2c_param_config(I2C_MASTER_NUM, &conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_MASTER_NUM, conf.mode,
                                       I2C_MASTER_RX_BUF_DISABLE,
                                       I2C_MASTER_TX_BUF_DISABLE, 0));
    ESP_ERROR_CHECK(i2cdev_init());

    ESP_ERROR_CHECK(bmp180_init_desc(&dev, I2C_MASTER_NUM, I2C_MASTER_SDA_IO, I2C_MASTER_SCL_IO));
    ESP_ERROR_CHECK(bmp180_init(&dev));

    while (1) {
        if (bmp180_measure(&dev, &temperature, &pressure, BMP180_MODE_STANDARD) == ESP_OK) {
            ESP_LOGI(TAG, "Temperature: %.2f °C, Pressure: %.2f hPa", temperature, pressure / 100.0);
        } else {
            ESP_LOGE(TAG, "Failed to read from BMP180 sensor");
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}














