#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#define LED_GPIO_PIN 2 // GPIO pin for the LED
#define BUTTON_GPIO_PIN 0 // GPIO pin for the button


void app_main(void)
{
    gpio_set_direction(LED_GPIO_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(BUTTON_GPIO_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BUTTON_GPIO_PIN, GPIO_PULLDOWN_ONLY); // Enable pull-down resistor on button pin
    while(1){
        if(gpio_get_level(BUTTON_GPIO_PIN)==1) { // Check if the button is pressed
            vTaskDelay(pdMS_TO_TICKS(100)); // Debounce delay
            gpio_set_level(LED_GPIO_PIN, 1); // Turn on the LED
        } else{ // If the button is not pressed
            vTaskDelay(pdMS_TO_TICKS(100)); // Debounce delay
            gpio_set_level(LED_GPIO_PIN, 0); // Turn off the LED
        }
        
    }


}