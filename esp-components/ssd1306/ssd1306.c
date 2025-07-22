#include "ssd1306.h"
#include "font8x8_basic.h"
#include <string.h>

static uint8_t buffer[128 * 8];
static i2c_dev_t dev;

esp_err_t ssd1306_init_i2c(uint8_t addr, i2c_port_t port, gpio_num_t sda, gpio_num_t scl) {
    memset(&dev, 0, sizeof(i2c_dev_t));
    dev.port = port;
    dev.addr = addr;
    dev.cfg.sda_io_num = sda;
    dev.cfg.scl_io_num = scl;
    dev.cfg.sda_pullup_en = GPIO_PULLUP_ENABLE;
    dev.cfg.scl_pullup_en = GPIO_PULLUP_ENABLE;
    dev.cfg.master.clk_speed = 400000;
    esp_err_t err = i2c_dev_create_mutex(&dev);
    if (err != ESP_OK) return err;
    return ESP_OK;

}

static void write_cmd(uint8_t cmd) {
    i2c_dev_write_reg(&dev, 0x00, &cmd, 1);
}

esp_err_t ssd1306_init(void) {
    write_cmd(0xAE);
    write_cmd(0xA8); write_cmd(0x3F);
    write_cmd(0xD3); write_cmd(0x00);
    write_cmd(0x40);
    write_cmd(0xA1);
    write_cmd(0xC0);
    write_cmd(0xDA); write_cmd(0x12);
    write_cmd(0x81); write_cmd(0x7F);
    write_cmd(0xA4);
    write_cmd(0xA6);
    write_cmd(0xD5); write_cmd(0x80);
    write_cmd(0x8D); write_cmd(0x14);
    write_cmd(0x20); write_cmd(0x00);
    write_cmd(0xAF);
    return ESP_OK;
}

void ssd1306_clear(void) {
    memset(buffer, 0x00, sizeof(buffer));
}

void ssd1306_draw_string(uint8_t x, uint8_t page, const char *text, uint8_t font_size, bool invert)
{
    (void)font_size;

    for (size_t i = 0; i < strlen(text); i++) {
        uint8_t c = text[i];
        if (c > 127) c = '?';

        for (int col = 0; col < 8; col++) {
            // Read 1 column of font data (vertical 8 pixels)
            uint8_t col_data = font8x8_basic[c][col];

            // Reverse the bits in each byte to fix mirroring
            uint8_t reversed = 0;
            for (int b = 0; b < 8; b++) {
                reversed <<= 1;
                reversed |= (col_data >> b) & 1;
            }

            if (invert) reversed = ~reversed;

            size_t index = (page * 128) + x + i * 8 + col;
            if (index < sizeof(buffer))
                buffer[index] = reversed;
        }
    }
}








void ssd1306_refresh(void) {
    for (uint8_t page = 0; page < 8; page++) {
        write_cmd(0xB0 + page);
        write_cmd(0x00);
        write_cmd(0x10);
        i2c_dev_write_reg(&dev, 0x40, &buffer[128 * page], 128);
    }
}
