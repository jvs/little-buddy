#include "event_processor.h"

//--------------------------------------------------------------------+
// FORWARD DECLARATIONS
//--------------------------------------------------------------------+

static void process_tick_event(const tick_data_t* tick_data);
static void process_device_connected(const device_data_t* device_data);
static void process_device_disconnected(const device_data_t* device_data);

//--------------------------------------------------------------------+
// EVENT PROCESSOR STATE
//--------------------------------------------------------------------+

// Add state variables here as the processor grows
// Examples: key mapping tables, mouse sensitivity, macro state, etc.

//--------------------------------------------------------------------+
// EVENT PROCESSOR IMPLEMENTATION
//--------------------------------------------------------------------+

void event_processor_init(void) {
    // Initialize processor state
    // This will be called once at startup
}

void event_processor_reset(void) {
    // Reset processor state
    // Useful for switching modes or recovering from errors
}

void event_processor_process(input_queue_t *input_queue, output_queue_t *output_queue) {
    input_event_t input_event;
    output_event_t output_event;

    // Process all events in the input queue
    while (input_queue_dequeue(input_queue, &input_event)) {
        switch (input_event.type) {
            case INPUT_MOUSE:
                // Pass mouse events through unchanged (for now)
                output_event.type = OUTPUT_MOUSE;
                output_event.data.mouse = input_event.data.mouse;

                (void)usb_output_queue_enqueue(output_queue, &output_event);
                break;

            case INPUT_KEYBOARD:
                // Pass keyboard events through unchanged (for now)
                output_event.type = OUTPUT_KEYBOARD;
                output_event.data.keyboard = input_event.data.keyboard;

                (void)usb_output_queue_enqueue(output_queue, &output_event);
                break;

            case INPUT_TICK:
                // Process tick events (currently ignored)
                process_tick_event(&input_event.data.tick);
                break;

            case INPUT_DEVICE_CONNECTED:
                // Handle device connection
                process_device_connected(&input_event.data.device);
                break;

            case INPUT_DEVICE_DISCONNECTED:
                // Handle device disconnection
                process_device_disconnected(&input_event.data.device);
                break;

            default:
                // Unknown event type, ignore
                break;
        }
    }
}

//--------------------------------------------------------------------+
// EVENT PROCESSING HELPERS
//--------------------------------------------------------------------+

static void process_tick_event(const tick_data_t* tick_data) {
    // Handle tick events here
    // Examples: timeouts, periodic actions, state machines
    (void)tick_data; // Suppress unused parameter warning for now
}

static void process_device_connected(const device_data_t* device_data) {
    // Handle device connection
    // Examples: initialize device-specific settings, update state
    (void)device_data; // Suppress unused parameter warning for now
}

static void process_device_disconnected(const device_data_t* device_data) {
    // Handle device disconnection
    // Examples: cleanup device state, reset mappings
    (void)device_data; // Suppress unused parameter warning for now
}
