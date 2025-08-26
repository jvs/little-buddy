#include "display/display.h"

#include <hardware/i2c.h>
#include "display/sh1107.h"


static sh1107_t display;
static bool display_ok = false;


bool display_init(i2c_inst_t *i2c) {
    display_ok = sh1107_init(&display, i2c1);
    return display_ok;
}

void display_clear_buffer(void) {
    if (!display_ok) return;

    sh1107_clear(&display);
}

void display_show_buffer(void) {
    if (!display_ok) return;

    sh1107_display(&display);
}

void display_copy_pixels(const uint8_t *pixel_data) {
    if (!display_ok) return;

    sh1107_draw_buffer(&display, pixel_data);
}

void display_draw_char(int16_t x, int16_t y, char c) {
    if (!display_ok) return;

    sh1107_draw_char(&display, x, y, c);
}

void display_draw_string(int16_t x, int16_t y, const char *str) {
    if (!display_ok) return;

    sh1107_draw_string(&display, x, y, str);
}
