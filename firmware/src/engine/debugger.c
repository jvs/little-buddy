#include "engine/debugger.h"

#include <stddef.h>
#include <stdio.h>

#include "usb/usb.h"
#include "display/display.h"


static void show_input_event(usb_input_event_t *event);
static void show_input_keyboard(usb_keyboard_data_t *keyboard);
static void show_input_mouse_event(usb_mouse_data_t *mouse);


void debugger_show_inputs(void) {
    set_input_callback(&show_input_event);
}


void debugger_hide_inputs(void) {
    set_input_callback(NULL);
}


static void show_input_event(usb_input_event_t *event) {
    display_clear_buffer();

    if (event->type == USB_INPUT_MOUSE) {
        show_input_mouse_event(&(event->data.mouse));
    } else if (event->type == USB_INPUT_KEYBOARD) {
        show_input_keyboard_event(&(event->data.keyboard));
    }
}


static void show_input_keyboard(usb_keyboard_data_t *keyboard) {
    if (
        keyboard->modifier == 0
        && keyboard->keycodes[0] == 0
        && keyboard->keycodes[1] == 0
        && keyboard->keycodes[2] == 0
        && keyboard->keycodes[3] == 0
        && keyboard->keycodes[4] == 0
        && keyboard->keycodes[5] == 0
    ) {
        return;
    }

    int16_t x = 1;
    int16_t y = 1;

    display_draw_string(x, y, "KEYBOARD");
    y += 10;

    char buffer[20];

    snprintf(buffer, sizeof(buffer), "MOD: %X", keyboard->modifier);
    display_draw_string(x, y, buffer);
    y += 10;

    for (uint8_t i = 0; i < 6; i++) {
        snprintf(buffer, sizeof(buffer), "K%d: %X", i, keyboard->keycodes[i]);
        display_draw_string(x, y, buffer);
        y += 10;
    }

    display_show_buffer();
}


static void show_input_mouse_event(usb_mouse_data_t *mouse) {
    if (
        mouse->delta_x == 0
        && mouse->delta_y == 0
        && mouse->scroll == 0
        && mouse->buttons == 0
    ) {
        return;
    }

    int16_t x = 1;
    int16_t y = 1;

    display_draw_string(x, y, "MOUSE");
    y += 10;

    char buffer[20];

    snprintf(buffer, sizeof(buffer), "DX: %d", mouse->delta_x);
    display_draw_string(x, y, buffer);
    y += 10;

    snprintf(buffer, sizeof(buffer), "DY: %d", mouse->delta_y);
    display_draw_string(x, y, buffer);
    y += 10;

    snprintf(buffer, sizeof(buffer), "SCR: %d", mouse->scroll);
    display_draw_string(x, y, buffer);
    y += 10;

    snprintf(buffer, sizeof(buffer), "BTN: %X", mouse->buttons);
    display_draw_string(x, y, buffer);
    y += 10;
}
