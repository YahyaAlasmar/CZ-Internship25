#include <stdio.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <nvs_flash.h>
#include <mqtt_client.h>
#include <dht.h>
#include <bmp180.h>
#include <i2cdev.h>

#define TAG "MAIN"

// Wi-Fi
#define WIFI_SSID "Yahyas_iphone"
#define WIFI_PASS "yahya2004"

// MQTT
#define MQTT_BROKER_URI "mqtt://172.20.10.2"
#define MQTT_TOPIC      "esp32/sensors/gas"

// Sensors
#define DHT11_GPIO 4
#define I2C_SDA 21
#define I2C_SCL 22

bmp180_dev_t bmp180;
esp_mqtt_client_handle_t mqtt_client = NULL;

static void wifi_init()
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Connecting to Wi-Fi...");

    ESP_ERROR_CHECK(esp_wifi_connect());
}

void publish_sensor_data(void *pvParameters)
{
    while (1)
    {
        float humidity = 0, dht_temp = 0;
        float bmp_temp = 0;
        uint32_t pressure_raw = 0;

        esp_err_t dht_result = dht_read_float_data(DHT_TYPE_DHT11, DHT11_GPIO, &humidity, &dht_temp);
        if (dht_result != ESP_OK) {
            ESP_LOGW(TAG, "DHT11 read failed: %s", esp_err_to_name(dht_result));
            humidity = -1;
        }

        esp_err_t bmp_result = bmp180_measure(&bmp180, &bmp_temp, &pressure_raw, BMP180_MODE_STANDARD);
        float pressure_hpa = pressure_raw / 100.0f;
        if (bmp_result != ESP_OK) {
            ESP_LOGW(TAG, "BMP180 read failed: %s", esp_err_to_name(bmp_result));
            bmp_temp = -1;
            pressure_hpa = -1;
        }

        char payload[128];
        snprintf(payload, sizeof(payload),
                 "{\"temperature\": %.2f, \"pressure\": %.2f, \"humidity\": %.1f}",
                 bmp_temp, pressure_hpa, humidity);

        if (mqtt_client != NULL) {
            int msg_id = esp_mqtt_client_publish(mqtt_client, MQTT_TOPIC, payload, 0, 1, 0);
            ESP_LOGI(TAG, "MQTT published: %s", payload);
        }

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event)
{
    switch (event->event_id)
    {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT connected");
            xTaskCreate(publish_sensor_data, "sensor_pub_task", 4096, NULL, 5, NULL);
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "MQTT disconnected");
            break;
        default:
            break;
    }
    return ESP_OK;
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    mqtt_event_handler_cb(event_data);
}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    wifi_init();

    i2cdev_init();
    bmp180_init_desc(&bmp180, I2C_NUM_0, I2C_SDA, I2C_SCL);
    bmp180_init(&bmp180);

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker = {
            .address.uri = MQTT_BROKER_URI,
        }
    };

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(mqtt_client);
}
