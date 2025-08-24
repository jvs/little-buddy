#include "event_processor.h"

//--------------------------------------------------------------------+
// FORWARD DECLARATIONS
//--------------------------------------------------------------------+

static void process_tick_event(const usb_tick_data_t* tick_data);
static void process_device_connected(const usb_device_data_t* device_data);
static void process_device_disconnected(const usb_device_data_t* device_data);

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

void event_processor_process(usb_event_queue_t* input_queue, usb_output_queue_t* output_queue) {
    usb_event_t input_event;
    
    // Process all events in the input queue
    while (usb_event_queue_dequeue(input_queue, &input_event)) {
        switch (input_event.type) {
            case USB_EVENT_MOUSE:
                // Pass mouse events through unchanged (for now)
                usb_output_enqueue_mouse(
                    output_queue,
                    input_event.data.mouse.delta_x,
                    input_event.data.mouse.delta_y,
                    input_event.data.mouse.buttons,
                    input_event.data.mouse.scroll
                );
                break;
                
            case USB_EVENT_KEYBOARD:
                // Pass keyboard events through unchanged (for now)
                usb_output_enqueue_keyboard(
                    output_queue,
                    input_event.data.keyboard.keycode,
                    input_event.data.keyboard.modifier,
                    input_event.data.keyboard.pressed
                );
                break;
                
            case USB_EVENT_TICK:
                // Process tick events (currently ignored)
                process_tick_event(&input_event.data.tick);
                break;
                
            case USB_EVENT_DEVICE_CONNECTED:
                // Handle device connection
                process_device_connected(&input_event.data.device);
                break;
                
            case USB_EVENT_DEVICE_DISCONNECTED:
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

static void process_tick_event(const usb_tick_data_t* tick_data) {
    // Handle tick events here
    // Examples: timeouts, periodic actions, state machines
    (void)tick_data; // Suppress unused parameter warning for now
}

static void process_device_connected(const usb_device_data_t* device_data) {
    // Handle device connection
    // Examples: initialize device-specific settings, update state
    (void)device_data; // Suppress unused parameter warning for now
}

static void process_device_disconnected(const usb_device_data_t* device_data) {
    // Handle device disconnection  
    // Examples: cleanup device state, reset mappings
    (void)device_data; // Suppress unused parameter warning for now
}