#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_http_client.h"
#include "nvs_flash.h"

#include "bmp180.h"
#include "ssd1306.h"
#include "i2cdev.h"

// === CONFIG ===
#define SDA_GPIO        21
#define SCL_GPIO        22    
#define I2C_PORT        I2C_NUM_0

#define PIR_GPIO        14
#define RED_LED_GPIO    25
#define GREEN_LED_GPIO  26
#define BUTTON_GPIO     27
#define MQ_ADC_CHANNEL  ADC1_CHANNEL_6 // GPIO34

#define WIFI_SSID       "Yahyas_iphone"
#define WIFI_PASS       "yahya2004"
#define THINGSPEAK_KEY  "CCXDKK2LLFZ13JHK"

#define CALIBRATION_SAMPLES 100
#define GAS_DELTA           300

static const char *TAG = "SMART_NODE";

int gas_baseline = 0;

void calibrate_mq135() {
    printf("Calibrating MQ135...\n");
    int total = 0;
    for (int i = 0; i < CALIBRATION_SAMPLES; i++) {
        total += adc1_get_raw(MQ_ADC_CHANNEL);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    gas_baseline = total / CALIBRATION_SAMPLES;
    printf("MQ135 baseline: %d\n", gas_baseline);
}

void wifi_init_sta(void) {
    static bool wifi_initialized = false;
    if (wifi_initialized) return;

    ESP_ERROR_CHECK(esp_netif_init());

    esp_err_t err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
        ESP_ERROR_CHECK(err);

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_connect());

    wifi_initialized = true;
}

void send_to_thingspeak(float temp, float pressure, int gas, int motion) {
    char url[256];
    snprintf(url, sizeof(url),
             "http://api.thingspeak.com/update?api_key=%s&field1=%.2f&field2=%.2f&field3=%d&field4=%d",
             THINGSPEAK_KEY, temp, pressure / 100.0, gas, motion);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (esp_http_client_perform(client) == ESP_OK)
        printf("ThingSpeak updated.\n");
    else
        printf("ThingSpeak upload failed.\n");
    esp_http_client_cleanup(client);
}

void app_main(void) {
    // Initialize NVS and WiFi
    ESP_ERROR_CHECK(nvs_flash_init());
    wifi_init_sta();

    // Initialize I2C and devices
    ESP_ERROR_CHECK(i2cdev_init());

    // OLED INIT
    ESP_ERROR_CHECK(ssd1306_init_i2c(SSD1306_I2C_ADDRESS, I2C_PORT, SDA_GPIO, SCL_GPIO));
    ESP_ERROR_CHECK(ssd1306_init());
    ssd1306_clear();

    // BMP180 INIT
    bmp180_dev_t bmp;
    memset(&bmp, 0, sizeof(bmp));
    ESP_ERROR_CHECK(bmp180_init_desc(&bmp, I2C_PORT, SDA_GPIO, SCL_GPIO));
    ESP_ERROR_CHECK(bmp180_init(&bmp));

    // Setup other hardware
    gpio_set_direction(PIR_GPIO, GPIO_MODE_INPUT);
    gpio_set_direction(BUTTON_GPIO, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BUTTON_GPIO, GPIO_PULLUP_ONLY);
    gpio_set_direction(RED_LED_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_direction(GREEN_LED_GPIO, GPIO_MODE_OUTPUT);
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(MQ_ADC_CHANNEL, ADC_ATTEN_DB_11);

    calibrate_mq135();

    int alert = 0;
    int loop_count = 0;

    while (1) {
        float temp = 0;
        uint32_t pressure = 0;
        ESP_ERROR_CHECK(bmp180_measure(&bmp, &temp, &pressure, BMP180_MODE_STANDARD));

        int gas = adc1_get_raw(MQ_ADC_CHANNEL);
        int motion = gpio_get_level(PIR_GPIO);
        int button = (gpio_get_level(BUTTON_GPIO) == 0);

        if (gas > gas_baseline + GAS_DELTA && motion && !alert) {
            gpio_set_level(RED_LED_GPIO, 1);
            gpio_set_level(GREEN_LED_GPIO, 0);
            alert = 1;
        } else if (button && alert) {
            gpio_set_level(RED_LED_GPIO, 0);
            gpio_set_level(GREEN_LED_GPIO, 1);
            alert = 0;
        } else if (!alert) {
            gpio_set_level(RED_LED_GPIO, 0);
            gpio_set_level(GREEN_LED_GPIO, 1);
        }

        printf("Temp: %.1f C | Pressure: %.0f hPa | Gas: %d | Motion: %s | Status: %s\n",
               temp, pressure / 100.0, gas, motion ? "YES" : "NO", alert ? "DANGER" : "SAFE");

        char line1[32], line2[32], line3[32], line4[32];
        snprintf(line1, sizeof(line1), "T:%.1fC", temp);
        snprintf(line2, sizeof(line2), "P:%.0fhPa", pressure / 100.0);
        snprintf(line3, sizeof(line3), "G:%d", gas);
        snprintf(line4, sizeof(line4), "M:%s[%s]", motion ? "Y" : "N", alert ? "DNG" : "SAFE");

        ssd1306_clear();
        ssd1306_draw_string(0, 0, line1, 1, false);
        ssd1306_draw_string(0, 1, line2, 1, false);
        ssd1306_draw_string(0, 2, line3, 1, false);
        ssd1306_draw_string(0, 3, line4, 1, false);
        ssd1306_refresh();

        if (++loop_count % 8 == 0) {
            send_to_thingspeak(temp, pressure, gas, motion);
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}




