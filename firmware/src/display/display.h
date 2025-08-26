#ifndef DISPLAY_H
#define DISPLAY_H

#include <hardware/i2c.h>
#include <stdbool.h>
#include <stdint.h>

bool display_init(i2c_inst_t *i2c);
void display_clear_buffer(void);
void display_show_buffer(void);

void display_set_pixel(int16_t x, int16_t y, bool on);
void display_copy_pixels(const uint8_t *pixel_data);
void display_draw_char(int16_t x, int16_t y, char c);
void display_draw_string(int16_t x, int16_t y, const char *str);

#endif
