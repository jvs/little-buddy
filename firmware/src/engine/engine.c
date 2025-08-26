#include "engine/engine.h"

#include <stdbool.h>
#include <stdint.h>

#include "usb/usb.h"
#include "engine/debugger.h"


static bool is_stretch_layer_active = false;

static void process_keyboard_event(const usb_keyboard_data_t *keyboard);
static void process_mouse_event(const usb_mouse_data_t *mouse);
static void process_tick_event(const usb_tick_data_t *tick_data);
// static void process_device_connected(const usb_device_data_t *device_data);
// static void process_device_disconnected(const usb_device_data_t *device_data);


void engine_init(void) {
    debugger_show_inputs();
}


void engine_task(void) {
    usb_input_event_t input_event;
    // usb_output_event_t output_event;

    // Process all events in the input queue
    while (usb_input_dequeue(&input_event)) {
        switch (input_event.type) {
            case USB_INPUT_MOUSE:
                process_mouse_event(&input_event.data.mouse);
                // output_event.type = USB_OUTPUT_MOUSE;
                // output_event.data.mouse = input_event.data.mouse;
                // (void)usb_output_enqueue(&output_event);
                break;

            case USB_INPUT_KEYBOARD:
                process_keyboard_event(&input_event.data.keyboard);
                // output_event.type = USB_OUTPUT_KEYBOARD;
                // output_event.data.keyboard = input_event.data.keyboard;
                // (void)usb_output_enqueue(&output_event);
                break;

            case USB_INPUT_TICK:
                process_tick_event(&input_event.data.tick);
                break;

            case USB_INPUT_DEVICE_CONNECTED:
                // process_device_connected(&input_event.data.device);
                break;

            case USB_INPUT_DEVICE_DISCONNECTED:
                // process_device_disconnected(&input_event.data.device);
                break;

            default:
                // Unknown event type, ignore
                break;
        }
    }
}


static void process_keyboard_event(const usb_keyboard_data_t *keyboard) {
    usb_output_event_t output_event;
    output_event.type = USB_OUTPUT_KEYBOARD;
    output_event.data.keyboard = *keyboard;
    uint8_t *keycodes = output_event.data.keyboard.keycodes;

    if (is_stretch_layer_active) {
        for (uint8_t i = 0; i < 6; i++) {
            uint8_t keycode = keycodes[i];
            switch (keycode) {
                case 0x0B: keycodes[i] = 0x50; break;
                case 0x0D: keycodes[i] = 0x51; break;
                case 0x0E: keycodes[i] = 0x52; break;
                case 0x0F: keycodes[i] = 0x4F; break;
            }

        }
    }

    (void) usb_output_enqueue(&output_event);
}


static void process_mouse_event(const usb_mouse_data_t *mouse) {
    is_stretch_layer_active = (mouse->buttons == 1 || mouse->buttons == 2);
}


static void process_tick_event(const usb_tick_data_t *tick_data) {
    (void)tick_data; // Suppress unused parameter warning for now
}

// static void process_device_connected(const usb_device_data_t *device_data) {
//     (void)device_data; // Suppress unused parameter warning for now
// }
//
// static void process_device_disconnected(const usb_device_data_t *device_data) {
//     (void)device_data; // Suppress unused parameter warning for now
// }
