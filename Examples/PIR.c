#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#define PIR_Sensor_Pin 4
void app_main(void)
{
    gpio_config_t config = {
        .pin_bit_mask = (1ull << PIR_Sensor_Pin),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    gpio_config(&config);

    printf("PIR sensor example: \n");
    
    while(1){
        int motion = gpio_get_level(PIR_Sensor_Pin);
        if(motion) {
            printf("Motion detected! \n");
        }else {
            printf("No motion detected. \n");

        }
        }
        vTaskDelay(pdMS_TO_TICKS(1000)); // Check every 1 second

    }


   