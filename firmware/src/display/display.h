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

typedef enum {
    DISPLAY_MODE_NORMAL = 0,
    DISPLAY_MODE_DEBUG,
    DISPLAY_MODE_OFF,
    DISPLAY_MODE_COUNT
} display_mode_t;

display_mode_t display_mode(void);
void display_cycle_mode(void);

// Legend: persistent overlay that shows regardless of display mode (powers on
// the panel if needed). Text may contain '\n' to split into lines. Cleared
// explicitly; survives mode changes until then.
void display_show_legend(const char *text);
void display_clear_legend(void);

// Activity mode: show short messages in response to user actions. Messages
// persist on screen for up to 60 seconds of idle time before clearing.
void display_show_message(const char *text);
void display_clear_if_showing(const char *text);
void display_clear_activity(void);
void display_tick(void);

#endif
