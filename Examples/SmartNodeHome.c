// FINAL: ESP32 Smart Safety Node with BMP180 + MQ135 + OLED + PIR + Button + LEDs + ThingSpeak

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "driver/i2c.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_http_client.h"

// ====== CONFIGURATION ======
#define SDA_GPIO        21
#define SCL_GPIO        22
#define I2C_PORT        I2C_NUM_0
#define I2C_FREQ        100000
#define OLED_ADDR       0x3C
#define BMP180_ADDR     0x77

#define PIR_GPIO        14
#define RED_LED_GPIO    25
#define GREEN_LED_GPIO  26
#define BUTTON_GPIO     27
#define MQ_ADC_CHANNEL  ADC1_CHANNEL_6  // GPIO 34
#define GAS_THRESHOLD   800

#define WIFI_SSID       "Yahyas_iphone"
#define WIFI_PASS       "yahya2004"
#define THINGSPEAK_KEY  "KQMUNMMS6AYM2DW4"

// ====== GLOBALS ======
int16_t AC1, AC2, AC3, B1, B2, MB, MC, MD;
uint16_t AC4, AC5, AC6;

#define GRAPH_POINTS 64
uint8_t temp_graph[GRAPH_POINTS] = {0};

void ssd1306_cmd(uint8_t cmd);
esp_err_t i2c_write_cmd(uint8_t addr, uint8_t control, uint8_t data);
void ssd1306_set_cursor(uint8_t col, uint8_t page);

void draw_temp_graph() {
    for (uint8_t page = 5; page <= 7; page++) {
        ssd1306_cmd(0xB0 + page);
        ssd1306_cmd(0x00);
        ssd1306_cmd(0x10);
        for (int i = 0; i < 128; i++) i2c_write_cmd(OLED_ADDR, 0x40, 0x00);
    }
    for (int i = 0; i < GRAPH_POINTS; i++) {
        uint8_t height = temp_graph[i];
        if (height > 24) height = 24;
        for (int y = 0; y < 3; y++) {
            ssd1306_set_cursor(i * 2, 5 + y);
            for (int j = 0; j < 2; j++) {
                uint8_t col = 0x00;
                if (height > (2 - y) * 8) {
                    uint8_t fill = height - (2 - y) * 8;
                    if (fill > 8) fill = 8;
                    col = (1 << fill) - 1;
                    col <<= (8 - fill);
                }
                i2c_write_cmd(OLED_ADDR, 0x40, col);
            }
        }
    }
}

const uint8_t font5x8[][5] = {
    {0x3E,0x51,0x49,0x45,0x3E}, {0x00,0x42,0x7F,0x40,0x00},
    {0x42,0x61,0x51,0x49,0x46}, {0x21,0x41,0x45,0x4B,0x31},
    {0x18,0x14,0x12,0x7F,0x10}, {0x27,0x45,0x45,0x45,0x39},
    {0x3C,0x4A,0x49,0x49,0x30}, {0x01,0x71,0x09,0x05,0x03},
    {0x36,0x49,0x49,0x49,0x36}, {0x06,0x49,0x49,0x29,0x1E}
};

void i2c_master_init() {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = SDA_GPIO,
        .scl_io_num = SCL_GPIO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_FREQ,
    };
    i2c_param_config(I2C_PORT, &conf);
    i2c_driver_install(I2C_PORT, conf.mode, 0, 0, 0);
}

esp_err_t i2c_write_cmd(uint8_t addr, uint8_t control, uint8_t data) {
    uint8_t buf[2] = {control, data};
    return i2c_master_write_to_device(I2C_PORT, addr, buf, 2, 1000 / portTICK_PERIOD_MS);
}

void ssd1306_cmd(uint8_t cmd) { i2c_write_cmd(OLED_ADDR, 0x00, cmd); }

void ssd1306_init() {
    const uint8_t cmds[] = {
        0xAE, 0xD5, 0x80, 0xA8, 0x3F, 0xD3, 0x00,
        0x40, 0x8D, 0x14, 0x20, 0x00, 0xA1, 0xC8,
        0xDA, 0x12, 0x81, 0xCF, 0xD9, 0xF1, 0xDB,
        0x40, 0xA4, 0xA6, 0xAF
    };
    for (int i = 0; i < sizeof(cmds); i++) ssd1306_cmd(cmds[i]);
}

void ssd1306_clear() {
    for (uint8_t page = 0; page < 8; page++) {
        ssd1306_cmd(0xB0 + page);
        ssd1306_cmd(0x00);
        ssd1306_cmd(0x10);
        for (uint8_t col = 0; col < 128; col++) {
            i2c_write_cmd(OLED_ADDR, 0x40, 0x00);
        }
    }
}

void ssd1306_set_cursor(uint8_t col, uint8_t page) {
    ssd1306_cmd(0xB0 + page);
    ssd1306_cmd(0x00 + (col & 0x0F));
    ssd1306_cmd(0x10 + (col >> 4));
}

void ssd1306_print_char(char c) {
    if (c >= '0' && c <= '9') {
        for (int i = 0; i < 5; i++) i2c_write_cmd(OLED_ADDR, 0x40, font5x8[c - '0'][i]);
        i2c_write_cmd(OLED_ADDR, 0x40, 0x00);
    } else {
        for (int i = 0; i < 5; i++) i2c_write_cmd(OLED_ADDR, 0x40, 0x00);
    }
}

void ssd1306_print(const char* str, uint8_t page) {
    ssd1306_set_cursor(0, page);
    while (*str) ssd1306_print_char(*str++);
}

void wifi_init_sta(void) {
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();
    esp_wifi_connect();
}

void send_to_thingspeak(float temp, float pressure, int gas, int motion) {
    char url[256];
    snprintf(url, sizeof(url),
        "http://api.thingspeak.com/update?api_key=%s&field1=%.2f&field2=%.2f&field3=%d&field4=%d",
        THINGSPEAK_KEY, temp, pressure / 100.0, gas, motion);
    esp_http_client_config_t config = {.url = url, .method = HTTP_METHOD_GET};
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (esp_http_client_perform(client) == ESP_OK)
        printf("ThingSpeak updated.\n");
    else
        printf("ThingSpeak upload failed.\n");
    esp_http_client_cleanup(client);
}

esp_err_t bmp180_read(uint8_t reg, uint8_t *data, size_t len) {
    return i2c_master_write_read_device(I2C_PORT, BMP180_ADDR, &reg, 1, data, len, 1000 / portTICK_PERIOD_MS);
}

esp_err_t bmp180_write(uint8_t reg, uint8_t val) {
    uint8_t data[2] = {reg, val};
    return i2c_master_write_to_device(I2C_PORT, BMP180_ADDR, data, 2, 1000 / portTICK_PERIOD_MS);
}

void bmp180_read_calibration() {
    uint8_t buf[22];
    bmp180_read(0xAA, buf, 22);
    AC1 = (buf[0] << 8) | buf[1];
    AC2 = (buf[2] << 8) | buf[3];
    AC3 = (buf[4] << 8) | buf[5];
    AC4 = (buf[6] << 8) | buf[7];
    AC5 = (buf[8] << 8) | buf[9];
    AC6 = (buf[10] << 8) | buf[11];
    B1  = (buf[12] << 8) | buf[13];
    B2  = (buf[14] << 8) | buf[15];
    MB  = (buf[16] << 8) | buf[17];
    MC  = (buf[18] << 8) | buf[19];
    MD  = (buf[20] << 8) | buf[21];
}

void bmp180_read_temp_pressure(float *temp_c, float *pressure_pa) {
    uint8_t buf[2];
    int32_t UT, UP, X1, X2, B5, B6, X3, B3, p;
    uint32_t B4, B7;

    bmp180_write(0xF4, 0x2E); vTaskDelay(pdMS_TO_TICKS(5));
    bmp180_read(0xF6, buf, 2);
    UT = (buf[0] << 8) | buf[1];
    X1 = ((UT - AC6) * AC5) >> 15;
    X2 = (MC << 11) / (X1 + MD);
    B5 = X1 + X2;
    *temp_c = ((B5 + 8) >> 4) / 10.0;

    bmp180_write(0xF4, 0x34); vTaskDelay(pdMS_TO_TICKS(8));
    bmp180_read(0xF6, buf, 2);
    UP = (buf[0] << 8) | buf[1];

    B6 = B5 - 4000;
    X1 = (B2 * (B6 * B6 >> 12)) >> 11;
    X2 = (AC2 * B6) >> 11;
    X3 = X1 + X2;
    B3 = (((AC1 * 4 + X3) << 0) + 2) >> 2;
    X1 = (AC3 * B6) >> 13;
    X2 = (B1 * (B6 * B6 >> 12)) >> 16;
    X3 = ((X1 + X2) + 2) >> 2;
    B4 = (AC4 * (uint32_t)(X3 + 32768)) >> 15;
    B7 = ((uint32_t)UP - B3) * 50000;
    p = (B7 < 0x80000000) ? (B7 << 1) / B4 : (B7 / B4) << 1;
    X1 = (p >> 8) * (p >> 8);
    X1 = (X1 * 3038) >> 16;
    X2 = (-7357 * p) >> 16;
    *pressure_pa = p + ((X1 + X2 + 3791) >> 4);
}

void app_main() {
    nvs_flash_init();
    wifi_init_sta();
    i2c_master_init();
    ssd1306_init();
    ssd1306_clear();
    bmp180_read_calibration();

    gpio_set_direction(PIR_GPIO, GPIO_MODE_INPUT);
    gpio_set_direction(BUTTON_GPIO, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BUTTON_GPIO, GPIO_PULLUP_ONLY);
    gpio_set_direction(RED_LED_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_direction(GREEN_LED_GPIO, GPIO_MODE_OUTPUT);
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(MQ_ADC_CHANNEL, ADC_ATTEN_DB_11);

    int alert = 0;
    int loop_count = 0;

    while (1) {
        float temp = 0, pressure = 0;
        bmp180_read_temp_pressure(&temp, &pressure);

        uint8_t graph_val = (uint8_t)((temp - 15.0f) * (24.0f / 30.0f));
        if (graph_val > 24) graph_val = 24;
        for (int i = 0; i < GRAPH_POINTS - 1; i++) temp_graph[i] = temp_graph[i + 1];
        temp_graph[GRAPH_POINTS - 1] = graph_val;

        int gas = adc1_get_raw(MQ_ADC_CHANNEL);
        int motion = gpio_get_level(PIR_GPIO);
        int button = (gpio_get_level(BUTTON_GPIO) == 0);

        if (gas > GAS_THRESHOLD && motion && !alert) {
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
        snprintf(line2, sizeof(line2), "P:%.0fhPa", pressure / 100);
        snprintf(line3, sizeof(line3), "G:%d", gas);
        snprintf(line4, sizeof(line4), "M:%s[%s]", motion ? "Y" : "N", alert ? "DNG" : "SAFE");

        ssd1306_clear();
        ssd1306_print(line1, 0);
        ssd1306_print(line2, 1);
        ssd1306_print(line3, 2);
        ssd1306_print(line4, 3);
        draw_temp_graph();

        if (++loop_count % 8 == 0) {
            send_to_thingspeak(temp, pressure, gas, motion);
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}







