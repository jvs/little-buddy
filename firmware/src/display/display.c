#include "display/display.h"

#include <string.h>

#include "pico/time.h"

#include "display/sh1107.h"


#define IDLE_TIMEOUT_US 60000000
#define MESSAGE_MAX_LEN 15


static sh1107_t display;
static bool display_ok = false;
static bool display_on = true;

static char current_message[MESSAGE_MAX_LEN + 1];
static uint64_t last_message_time_us;


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


void display_toggle(void) {
    if (!display_ok) return;

    display_on = !display_on;
    sh1107_set_power(&display, display_on);
}


void display_show_message(const char *text) {
    if (!display_ok) return;

    strncpy(current_message, text, MESSAGE_MAX_LEN);
    current_message[MESSAGE_MAX_LEN] = 0;
    last_message_time_us = time_us_64();

    // Roughly center the message horizontally; 6 px per char in the 5x7 font.
    int16_t len = (int16_t)strlen(current_message);
    int16_t x = (128 - len * 6) / 2;
    if (x < 0) x = 0;
    int16_t y = 60;

    sh1107_clear(&display);
    sh1107_draw_string(&display, x, y, current_message);
    sh1107_display(&display);
}


void display_clear_if_showing(const char *text) {
    if (!display_ok) return;
    if (strcmp(current_message, text) != 0) return;
    display_clear_activity();
}


void display_clear_activity(void) {
    if (!display_ok) return;

    current_message[0] = 0;
    last_message_time_us = 0;
    sh1107_clear(&display);
    sh1107_display(&display);
}


void display_tick(void) {
    if (!display_ok) return;
    if (current_message[0] == 0) return;
    if (time_us_64() - last_message_time_us < IDLE_TIMEOUT_US) return;
    display_clear_activity();
}
