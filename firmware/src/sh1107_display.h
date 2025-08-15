#ifndef SH1107_DISPLAY_H
#define SH1107_DISPLAY_H

#include <stdint.h>
#include <stdbool.h>
#include "hardware/i2c.h"

#define SH1107_I2C_ADDRESS 0x3D
#define SH1107_WIDTH 128
#define SH1107_HEIGHT 128
#define SH1107_PAGES 16
#define SH1107_BUFFER_SIZE (SH1107_WIDTH * SH1107_PAGES)

#define SH1107_SETCONTRAST 0x81
#define SH1107_DISPLAYALLON_RESUME 0xA4
#define SH1107_DISPLAYALLON 0xA5
#define SH1107_NORMALDISPLAY 0xA6
#define SH1107_INVERTDISPLAY 0xA7
#define SH1107_DISPLAYOFF 0xAE
#define SH1107_DISPLAYON 0xAF
#define SH1107_SETDISPLAYOFFSET 0xD3
#define SH1107_SETCOMPINS 0xDA
#define SH1107_SETVCOMDETECT 0xDB
#define SH1107_SETDISPLAYCLOCKDIV 0xD5
#define SH1107_SETPRECHARGE 0xD9
#define SH1107_SETMULTIPLEX 0xA8
#define SH1107_SETLOWCOLUMN 0x00
#define SH1107_SETHIGHCOLUMN 0x10
#define SH1107_SETSTARTLINE 0x40
#define SH1107_MEMORYMODE 0x20
#define SH1107_COLUMNADDR 0x21
#define SH1107_PAGEADDR 0x22
#define SH1107_COMSCANINC 0xC0
#define SH1107_COMSCANDEC 0xC8
#define SH1107_SEGREMAP 0xA0
#define SH1107_CHARGEPUMP 0x8D
#define SH1107_EXTERNALVCC 0x1
#define SH1107_SWITCHCAPVCC 0x2
#define SH1107_DCDC 0xAD

typedef struct {
    i2c_inst_t *i2c;
    uint8_t buffer[SH1107_BUFFER_SIZE];
    bool initialized;
} sh1107_t;

bool sh1107_init(sh1107_t *display, i2c_inst_t *i2c);
void sh1107_clear(sh1107_t *display);
void sh1107_display(sh1107_t *display);
void sh1107_set_pixel(sh1107_t *display, int16_t x, int16_t y, bool on);
void sh1107_draw_char(sh1107_t *display, int16_t x, int16_t y, char c);
void sh1107_draw_string(sh1107_t *display, int16_t x, int16_t y, const char *str);
void sh1107_set_contrast(sh1107_t *display, uint8_t contrast);

#endif