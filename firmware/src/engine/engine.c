#include "engine/engine.h"

#include <stdbool.h>
#include <stdint.h>

#include "usb/usb.h"
#include "engine/debugger.h"

static void receive_usb_inputs(void);
static void run_engine(void);
static void send_usb_outputs(void);
static void reset_engine(void);

// input.c
void usb_input_init(void);

// output.c
void engine_output_init(void);


void engine_init(void) {
    engine_input_init();
    engine_output_init();
    debugger_show_inputs();
}


void engine_task(void) {
    receive_usb_inputs();
    run_engine();
    send_usb_outputs();
}

static void receive_usb_inputs(void) {
    usb_input_event_t input_event;

    // Process all events in the input queue.
    // TODO: Create engine_event_t values and enqueue them with engine_input_enqueue.
    while (usb_input_dequeue(&input_event)) {
        switch (input_event.type) {
            case USB_INPUT_MOUSE:
                break;

            case USB_INPUT_KEYBOARD:
                break;

            case USB_INPUT_TICK:
                break;

            case USB_INPUT_DEVICE_CONNECTED:
                reset_engine();
                break;

            case USB_INPUT_DEVICE_DISCONNECTED:
                reset_engine();
                break;

            default:
                break;
        }
    }
}

static void run_engine(void) {
    // For now, just copy from the input queue to the output queue.
    engine_event_t input_event;

    // Process all events in the input queue.
    while (engine_input_dequeue(&input_event)) {
        engine_output_enqueue(&input_event);
    }
}

static void send_usb_outputs(void) {
    engine_event_t output_event;

    while (engine_output_dequeue(&output_event)) {
        // TODO: Create a usb_output_event_t and call usb_output_enqueue.
    }
}

static void reset_engine(void) {
    // Do thing for now. Eventually, maybe reset the queues and internal state.
}
