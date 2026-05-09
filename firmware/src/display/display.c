#include "display/display.h"

#include <stdio.h>
#include <string.h>

#include "pico/time.h"

#include "display/sh1107.h"
#include "engine/engine.h"


#define IDLE_TIMEOUT_US 60000000
#define MESSAGE_MAX_LEN 20
#define LEGEND_MAX_LEN 160
#define DEBUG_REFRESH_US 100000   // 10 Hz — enough to read, cheap on I2C.
#define LEGEND_REFRESH_US 200000  // 5 Hz — legend is mostly static.


static sh1107_t display;
static bool display_ok = false;

static display_mode_t mode = DISPLAY_MODE_NORMAL;

static char current_message[MESSAGE_MAX_LEN + 1];
static uint64_t last_message_time_us;
static uint64_t last_render_time_us;
static uint64_t last_debug_render_us;

static char legend[LEGEND_MAX_LEN + 1];
static bool legend_active;
static uint64_t last_legend_render_us;

static void render_legend(void);


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
    last_render_time_us = time_us_64();
    sh1107_set_power(&display, true);
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


void display_draw_icon(const uint8_t *icon_data) {
    if (!display_ok || icon_data == NULL) return;
    sh1107_draw_buffer(&display, icon_data);
}


display_mode_t display_mode(void) { return mode; }


static const char *mode_name(display_mode_t m) {
    switch (m) {
        case DISPLAY_MODE_NORMAL: return "NORMAL";
        case DISPLAY_MODE_DEBUG:  return "DEBUG";
        case DISPLAY_MODE_OFF:    return "OFF";
        default:                  return "?";
    }
}


void display_cycle_mode(void) {
    if (!display_ok) return;

    mode = (mode + 1) % DISPLAY_MODE_COUNT;

    // Keep legend visible if active (e.g., cycled from Z-layer). Legend
    // suppresses mode-owned rendering until it's cleared.
    if (legend_active) {
        display_show_message(mode_name(mode));
        return;
    }

    sh1107_set_power(&display, true);
    current_message[0] = 0;
    sh1107_clear(&display);
    sh1107_display(&display);

    if (mode == DISPLAY_MODE_OFF) {
        sh1107_set_power(&display, false);
        return;
    }

    display_show_message(mode_name(mode));
}


void display_show_message(const char *text) {
    if (!display_ok) return;
    if (mode == DISPLAY_MODE_OFF && !legend_active) return;

    strncpy(current_message, text, MESSAGE_MAX_LEN);
    current_message[MESSAGE_MAX_LEN] = 0;
    last_message_time_us = time_us_64();

    // Legend mode redraws in its own pass (next tick picks up the message).
    if (legend_active) {
        last_legend_render_us = 0;
        render_legend();
        return;
    }

    // Roughly center the message horizontally; 6 px per char in the 5x7 font.
    int16_t len = (int16_t)strlen(current_message);
    int16_t x = (128 - len * 6) / 2;
    if (x < 0) x = 0;
    int16_t y = 60;

    sh1107_set_power(&display, true);
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


static void render_legend(void) {
    sh1107_clear(&display);

    int16_t y = 2;
    const char *p = legend;
    char line[22];
    while (*p && y < 128) {
        size_t i = 0;
        while (*p && *p != '\n' && i < sizeof(line) - 1) {
            line[i++] = *p++;
        }
        line[i] = 0;
        if (*p == '\n') p++;
        sh1107_draw_string(&display, 0, y, line);
        y += 9;
    }

    // Activity messages keep showing under the legend until they time out.
    if (current_message[0] != 0) {
        int16_t len = (int16_t)strlen(current_message);
        int16_t x = (128 - len * 6) / 2;
        if (x < 0) x = 0;
        sh1107_draw_string(&display, x, 118, current_message);
    }

    sh1107_display(&display);
}


static void render_debug(void) {
    char buf[24];
    sh1107_clear(&display);

    const usb_keyboard_data_t *ikb = engine_last_input_keyboard();
    const usb_mouse_data_t    *ims = engine_last_input_mouse();
    const usb_keyboard_data_t *okb = engine_last_output_keyboard();
    const usb_mouse_data_t    *oms = engine_last_output_mouse();

    snprintf(buf, sizeof(buf), "IN  KB mod:%02X", ikb ? ikb->modifier : 0);
    sh1107_draw_string(&display, 0, 0, buf);
    if (ikb) {
        snprintf(buf, sizeof(buf), "%02X %02X %02X %02X %02X %02X",
            ikb->keycodes[0], ikb->keycodes[1], ikb->keycodes[2],
            ikb->keycodes[3], ikb->keycodes[4], ikb->keycodes[5]);
        sh1107_draw_string(&display, 0, 9, buf);
    }

    snprintf(buf, sizeof(buf), "IN  MS btn:%02X", ims ? ims->buttons : 0);
    sh1107_draw_string(&display, 0, 18, buf);
    if (ims) {
        snprintf(buf, sizeof(buf), "dx%+4d dy%+4d s%+4d",
            ims->delta_x, ims->delta_y, ims->scroll);
        sh1107_draw_string(&display, 0, 27, buf);
    }

    snprintf(buf, sizeof(buf), "OUT KB mod:%02X", okb ? okb->modifier : 0);
    sh1107_draw_string(&display, 0, 40, buf);
    if (okb) {
        snprintf(buf, sizeof(buf), "%02X %02X %02X %02X %02X %02X",
            okb->keycodes[0], okb->keycodes[1], okb->keycodes[2],
            okb->keycodes[3], okb->keycodes[4], okb->keycodes[5]);
        sh1107_draw_string(&display, 0, 49, buf);
    }

    snprintf(buf, sizeof(buf), "OUT MS btn:%02X", oms ? oms->buttons : 0);
    sh1107_draw_string(&display, 0, 58, buf);
    if (oms) {
        snprintf(buf, sizeof(buf), "dx%+4d dy%+4d s%+4d",
            oms->delta_x, oms->delta_y, oms->scroll);
        sh1107_draw_string(&display, 0, 67, buf);
    }

    sh1107_display(&display);
}


void display_show_legend(const char *text) {
    if (!display_ok) return;
    strncpy(legend, text, LEGEND_MAX_LEN);
    legend[LEGEND_MAX_LEN] = 0;
    legend_active = true;
    last_legend_render_us = 0;  // force immediate redraw on next tick
    sh1107_set_power(&display, true);
    render_legend();
}


void display_clear_legend(void) {
    if (!display_ok) return;
    legend_active = false;
    legend[0] = 0;

    if (mode == DISPLAY_MODE_OFF) {
        sh1107_set_power(&display, false);
        return;
    }

    sh1107_clear(&display);
    // Preserve any active activity message in its normal position.
    if (current_message[0] != 0) {
        int16_t len = (int16_t)strlen(current_message);
        int16_t x = (128 - len * 6) / 2;
        if (x < 0) x = 0;
        sh1107_draw_string(&display, x, 60, current_message);
    }
    sh1107_display(&display);
    last_debug_render_us = 0;  // let debug mode redraw immediately.
}


void display_tick(void) {
    if (!display_ok) return;

    uint64_t now = time_us_64();

    if (current_message[0] != 0 && now - last_message_time_us >= IDLE_TIMEOUT_US) {
        current_message[0] = 0;
        last_render_time_us = 0;
        if (!legend_active && mode != DISPLAY_MODE_DEBUG) {
            sh1107_clear(&display);
            sh1107_display(&display);
        }
    }

    if (legend_active) {
        if (now - last_legend_render_us < LEGEND_REFRESH_US) return;
        last_legend_render_us = now;
        render_legend();
        return;
    }

    if (mode == DISPLAY_MODE_OFF) return;

    if (current_message[0] != 0) return;  // message stays until timeout

    if (mode == DISPLAY_MODE_DEBUG) {
        if (now - last_debug_render_us < DEBUG_REFRESH_US) return;
        last_debug_render_us = now;
        render_debug();
        return;
    }

    // Blank and power off after 60s of no display updates (e.g. idle logo).
    if (last_render_time_us != 0 && now - last_render_time_us >= IDLE_TIMEOUT_US) {
        last_render_time_us = 0;
        sh1107_clear(&display);
        sh1107_display(&display);  // flush blank to GDRAM before power-off to avoid stale flicker on wake
        sh1107_set_power(&display, false);
    }
}
