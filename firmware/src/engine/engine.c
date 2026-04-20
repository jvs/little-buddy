#include "engine/engine.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "usb/usb.h"
#include "display/display.h"

static void receive_usb_inputs(void);
static void send_usb_outputs(void);
static void reset_engine(void);

static void process_keyboard(const usb_keyboard_data_t *keyboard, uint64_t timestamp_us);
static void process_mouse(const usb_mouse_data_t *mouse, uint64_t timestamp_us);

// input.c
void engine_input_init(void);

// output.c
void engine_output_init(void);

// custom.c
void engine_custom_init(void);
void engine_custom_task(void);

// Input state — previous USB reports for diffing
static usb_keyboard_data_t prev_keyboard;
static uint8_t prev_mouse_buttons;

// Output state — current held keyboard and mouse button state
static uint8_t output_modifier;
static uint8_t output_keycodes[6];
static uint8_t output_buttons;


void engine_init(void) {
    engine_input_init();
    engine_output_init();
    engine_custom_init();
}


void engine_task(void) {
    receive_usb_inputs();
    engine_custom_task();
    send_usb_outputs();
    display_tick();
}


static void process_keyboard(const usb_keyboard_data_t *keyboard, uint64_t timestamp_us) {
    engine_event_t event;
    event.timestamp_us = timestamp_us;

    // Diff modifier bits; each bit maps to keycode 0xE0 + bit_index.
    uint8_t modifier_changed = keyboard->modifier ^ prev_keyboard.modifier;
    for (uint8_t bit = 0; bit < 8; bit++) {
        if (!(modifier_changed & (1 << bit))) continue;
        event.type = (keyboard->modifier & (1 << bit)) ? ENGINE_PRESS_KEY_EVENT : ENGINE_RELEASE_KEY_EVENT;
        event.data.keycode = 0xE0 + bit;
        engine_input_enqueue(&event);
    }

    // Keys in prev but not in new → release.
    for (uint8_t i = 0; i < 6; i++) {
        uint8_t kc = prev_keyboard.keycodes[i];
        if (kc == 0) continue;
        bool still_held = false;
        for (uint8_t j = 0; j < 6; j++) {
            if (keyboard->keycodes[j] == kc) { still_held = true; break; }
        }
        if (!still_held) {
            event.type = ENGINE_RELEASE_KEY_EVENT;
            event.data.keycode = kc;
            engine_input_enqueue(&event);
        }
    }

    // Keys in new but not in prev → press.
    for (uint8_t i = 0; i < 6; i++) {
        uint8_t kc = keyboard->keycodes[i];
        if (kc == 0) continue;
        bool was_held = false;
        for (uint8_t j = 0; j < 6; j++) {
            if (prev_keyboard.keycodes[j] == kc) { was_held = true; break; }
        }
        if (!was_held) {
            event.type = ENGINE_PRESS_KEY_EVENT;
            event.data.keycode = kc;
            engine_input_enqueue(&event);
        }
    }

    prev_keyboard = *keyboard;
}


static void process_mouse(const usb_mouse_data_t *mouse, uint64_t timestamp_us) {
    engine_event_t event;
    event.timestamp_us = timestamp_us;

    if (mouse->delta_x != 0 || mouse->delta_y != 0) {
        event.type = ENGINE_MOVE_EVENT;
        event.data.move.delta_x = mouse->delta_x;
        event.data.move.delta_y = mouse->delta_y;
        engine_input_enqueue(&event);
    }

    if (mouse->scroll != 0) {
        event.type = ENGINE_SCROLL_EVENT;
        event.data.scroll = mouse->scroll;
        engine_input_enqueue(&event);
    }

    // Diff button bits; button number is 1-based.
    uint8_t buttons_changed = mouse->buttons ^ prev_mouse_buttons;
    for (uint8_t bit = 0; bit < 8; bit++) {
        if (!(buttons_changed & (1 << bit))) continue;
        event.type = (mouse->buttons & (1 << bit)) ? ENGINE_PRESS_BUTTON_EVENT : ENGINE_RELEASE_BUTTON_EVENT;
        event.data.button = bit + 1;
        engine_input_enqueue(&event);
    }

    prev_mouse_buttons = mouse->buttons;
}


static void receive_usb_inputs(void) {
    usb_input_event_t input_event;

    while (usb_input_dequeue(&input_event)) {
        switch (input_event.type) {
            case USB_INPUT_MOUSE:
                process_mouse(&input_event.data.mouse, input_event.timestamp_us);
                break;

            case USB_INPUT_KEYBOARD:
                process_keyboard(&input_event.data.keyboard, input_event.timestamp_us);
                break;

            case USB_INPUT_TICK: {
                engine_event_t event = {
                    .type = ENGINE_TICK_EVENT,
                    .timestamp_us = input_event.timestamp_us
                };
                engine_input_enqueue(&event);
                break;
            }

            case USB_INPUT_DEVICE_CONNECTED:
            case USB_INPUT_DEVICE_DISCONNECTED:
                reset_engine();
                break;

            default:
                break;
        }
    }
}


static void enqueue_keyboard_report(void) {
    usb_output_event_t usb_event;
    usb_event.type = USB_OUTPUT_KEYBOARD;
    usb_event.data.keyboard.modifier = output_modifier;
    memcpy(usb_event.data.keyboard.keycodes, output_keycodes, 6);
    usb_output_enqueue(&usb_event);
}

static void send_usb_outputs(void) {
    engine_event_t event;
    bool mouse_changed = false;
    int8_t delta_x = 0;
    int8_t delta_y = 0;
    int8_t scroll = 0;

    // Keyboard events each produce their own USB report — otherwise a
    // press+release pair drained in the same tick coalesces to no-op and
    // nothing reaches the host. Mouse movement/scroll still coalesces.
    while (engine_output_dequeue(&event)) {
        switch (event.type) {
            case ENGINE_PRESS_KEY_EVENT: {
                uint8_t kc = event.data.keycode;
                if (kc >= 0xE0 && kc <= 0xE7) {
                    output_modifier |= (1 << (kc - 0xE0));
                } else {
                    for (uint8_t i = 0; i < 6; i++) {
                        if (output_keycodes[i] == 0) { output_keycodes[i] = kc; break; }
                    }
                }
                enqueue_keyboard_report();
                break;
            }

            case ENGINE_RELEASE_KEY_EVENT: {
                uint8_t kc = event.data.keycode;
                if (kc >= 0xE0 && kc <= 0xE7) {
                    output_modifier &= ~(1 << (kc - 0xE0));
                } else {
                    for (uint8_t i = 0; i < 6; i++) {
                        if (output_keycodes[i] == kc) { output_keycodes[i] = 0; break; }
                    }
                }
                enqueue_keyboard_report();
                break;
            }

            case ENGINE_PRESS_BUTTON_EVENT:
                output_buttons |= (1 << (event.data.button - 1));
                mouse_changed = true;
                break;

            case ENGINE_RELEASE_BUTTON_EVENT:
                output_buttons &= ~(1 << (event.data.button - 1));
                mouse_changed = true;
                break;

            case ENGINE_MOVE_EVENT:
                delta_x += event.data.move.delta_x;
                delta_y += event.data.move.delta_y;
                mouse_changed = true;
                break;

            case ENGINE_SCROLL_EVENT:
                scroll += event.data.scroll;
                mouse_changed = true;
                break;

            default:
                break;
        }
    }

    if (mouse_changed) {
        usb_output_event_t usb_event;
        usb_event.type = USB_OUTPUT_MOUSE;
        usb_event.data.mouse.delta_x = delta_x;
        usb_event.data.mouse.delta_y = delta_y;
        usb_event.data.mouse.scroll = scroll;
        usb_event.data.mouse.buttons = output_buttons;
        usb_output_enqueue(&usb_event);
    }
}


static void reset_engine(void) {
    memset(&prev_keyboard, 0, sizeof(prev_keyboard));
    prev_mouse_buttons = 0;
    output_modifier = 0;
    memset(output_keycodes, 0, sizeof(output_keycodes));
    output_buttons = 0;
}
