#include "engine/engine.h"
#include "usb/usb.h"
#include "engine/debugger.h"


static void process_tick_event(const usb_tick_data_t *tick_data);
// static void process_device_connected(const usb_device_data_t *device_data);
// static void process_device_disconnected(const usb_device_data_t *device_data);


void engine_init(void) {
    debugger_show_inputs();
}


void engine_task(void) {
    usb_input_event_t input_event;
    usb_output_event_t output_event;

    // Process all events in the input queue
    while (usb_input_dequeue(&input_event)) {
        switch (input_event.type) {
            case USB_INPUT_MOUSE:
                output_event.type = USB_OUTPUT_MOUSE;
                output_event.data.mouse = input_event.data.mouse;

                (void)usb_output_enqueue(&output_event);
                break;

            case USB_INPUT_KEYBOARD:
                output_event.type = USB_OUTPUT_KEYBOARD;
                output_event.data.keyboard = input_event.data.keyboard;

                (void)usb_output_enqueue(&output_event);
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
