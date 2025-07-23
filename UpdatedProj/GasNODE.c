#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "driver/adc.h"
#include "mqtt_client.h"
#include "esp_netif.h"

#define TAG "GAS_MONITOR"

// Wi-Fi Credentials
#define WIFI_SSID      "Yahyas_iphone"
#define WIFI_PASS      "yahya2004"

// MQTT
#define MQTT_BROKER_URI "mqtt://172.20.10.2"
#define MQTT_TOPIC      "esp32/sensors/gas"

// Sensor constants
#define RL_VALUE    10000.0
#define V_REF       1100.0
#define V_SUPPLY    5000.0
#define ADC_MAX     4095.0

// MQ-135 (CO2) on GPIO34
#define MQ135_CHANNEL ADC1_CHANNEL_6
float R0_MQ135 = 10.0;

// MQ-5 (CH4) on GPIO35
#define MQ5_CHANNEL ADC1_CHANNEL_7
float R0_MQ5 = 10.0;

// MQ-9 (CO) on GPIO32
#define MQ9_CHANNEL ADC1_CHANNEL_4
float R0_MQ9 = 10.0;

esp_mqtt_client_handle_t mqtt_client = NULL;

// ---------------------------- Utility Functions ----------------------------
float read_voltage(adc1_channel_t channel) {
    int raw = adc1_get_raw(channel);
    return ((float)raw / ADC_MAX) * V_REF;
}

float calculate_rs(float vout_mv) {
    return ((V_SUPPLY - vout_mv) / vout_mv) * RL_VALUE;
}

float calculate_ppm_mq135(float rs, float r0) {
    float ratio = rs / r0;
    float ppm_log = -0.42 * log10(ratio) + 1.92;
    return pow(10, ppm_log);
}

float calculate_ppm_mq5(float rs, float r0) {
    float ratio = rs / r0;
    float ppm_log = -2.632 * log10(ratio) + 2.5;
    return pow(10, ppm_log);
}

float calculate_ppm_mq9(float rs, float r0) {
    float ratio = rs / r0;
    float ppm_log = -0.48 * log10(ratio) + 1.58;
    return pow(10, ppm_log);
}

float calibrate_r0(adc1_channel_t channel, float clean_air_ratio, const char *label) {
    ESP_LOGI(TAG, "Calibrating R0 for %s...", label);
    float sum_rs = 0;
    for (int i = 0; i < 100; i++) {
        float vout = read_voltage(channel);
        float rs = calculate_rs(vout);
        sum_rs += rs;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    float avg_rs = sum_rs / 100.0;
    float r0 = avg_rs / clean_air_ratio;
    ESP_LOGI(TAG, "%s Calibration complete. R0 = %.2f Ω", label, r0);
    return r0;
}

// ---------------------------- Wi-Fi + MQTT ----------------------------
static void wifi_init(void) {
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

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;
    if (event_id == MQTT_EVENT_CONNECTED) {
        ESP_LOGI(TAG, "MQTT Connected");
    } else if (event_id == MQTT_EVENT_DISCONNECTED) {
        ESP_LOGI(TAG, "MQTT Disconnected");
    }
}

static void mqtt_init(void) {
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker = {
            .address.uri = MQTT_BROKER_URI,
        },
    };

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(mqtt_client);
}


// ---------------------------- Main Application ----------------------------
void app_main() {
    // Init
    ESP_LOGI(TAG, "Starting...");
    esp_log_level_set(TAG, ESP_LOG_INFO);
    nvs_flash_init();
    wifi_init();
    mqtt_init();

    // ADC config
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(MQ135_CHANNEL, ADC_ATTEN_DB_11);
    adc1_config_channel_atten(MQ5_CHANNEL, ADC_ATTEN_DB_11);
    adc1_config_channel_atten(MQ9_CHANNEL, ADC_ATTEN_DB_11);

    // Calibrate sensors
    R0_MQ135 = calibrate_r0(MQ135_CHANNEL, 3.6, "MQ-135 (CO2)");
    R0_MQ5   = calibrate_r0(MQ5_CHANNEL,   6.5, "MQ-5 (CH4)");
    R0_MQ9   = calibrate_r0(MQ9_CHANNEL,   9.8, "MQ-9 (CO)");

    while (1) {
        // MQ-135
        float v_mq135 = read_voltage(MQ135_CHANNEL);
        float rs_mq135 = calculate_rs(v_mq135);
        float ppm_co2 = calculate_ppm_mq135(rs_mq135, R0_MQ135);

        // MQ-5
        float v_mq5 = read_voltage(MQ5_CHANNEL);
        float rs_mq5 = calculate_rs(v_mq5);
        float ppm_ch4 = calculate_ppm_mq5(rs_mq5, R0_MQ5);

        // MQ-9
        float v_mq9 = read_voltage(MQ9_CHANNEL);
        float rs_mq9 = calculate_rs(v_mq9);
        float ppm_co = calculate_ppm_mq9(rs_mq9, R0_MQ9);

        // Log locally
        ESP_LOGI(TAG, "[MQ-135] V=%.2f mV | CO₂ = %.2f ppm", v_mq135, ppm_co2);
        ESP_LOGI(TAG, "[MQ-5  ] V=%.2f mV | CH₄ = %.2f ppm", v_mq5, ppm_ch4);
        ESP_LOGI(TAG, "[MQ-9  ] V=%.2f mV | CO  = %.2f ppm", v_mq9, ppm_co);

        // Send over MQTT
        char payload[128];
        snprintf(payload, sizeof(payload),
                 "{\"co2\": %.2f, \"ch4\": %.2f, \"co\": %.2f}",
                 ppm_co2, ppm_ch4, ppm_co);

        esp_mqtt_client_publish(mqtt_client, MQTT_TOPIC, payload, 0, 1, 0);
        ESP_LOGI(TAG, "MQTT Published: %s", payload);

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
