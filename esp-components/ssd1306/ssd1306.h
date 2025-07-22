#pragma once

#include "i2cdev.h"

#define SSD1306_I2C_ADDRESS 0x3C

esp_err_t ssd1306_init_i2c(uint8_t addr, i2c_port_t port, gpio_num_t sda, gpio_num_t scl);
esp_err_t ssd1306_init(void);
void ssd1306_clear(void);
void ssd1306_draw_string(uint8_t x, uint8_t y, const char *text, uint8_t font_size, bool invert);
void ssd1306_refresh(void);
