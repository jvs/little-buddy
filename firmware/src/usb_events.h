#ifndef USB_EVENTS_H
#define USB_EVENTS_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    USB_EVENT_NONE = 0,
    USB_EVENT_MOUSE,
    USB_EVENT_KEYBOARD,
    USB_EVENT_DEVICE_CONNECTED,
    USB_EVENT_DEVICE_DISCONNECTED,
    USB_EVENT_TICK
} usb_event_type_t;

typedef struct {
    int8_t delta_x;
    int8_t delta_y;
    int8_t scroll;
    uint8_t buttons;
} usb_mouse_data_t;

typedef struct {
    uint8_t keycode;
    uint8_t modifier;
    bool pressed;
} usb_keyboard_data_t;

typedef struct {
    uint8_t device_address;
    uint8_t instance;
    const char* device_type;
} usb_device_data_t;

typedef struct {
    uint32_t tick_count;       // Running tick counter
    uint32_t delta_us;         // Microseconds since last tick
} usb_tick_data_t;

typedef struct {
    usb_event_type_t type;
    uint32_t timestamp_ms;
    uint32_t sequence_id;      // Event sequence number
    uint8_t interface_id;      // Which HID interface (0, 1, etc.)
    union {
        usb_mouse_data_t mouse;
        usb_keyboard_data_t keyboard;
        usb_device_data_t device;
        usb_tick_data_t tick;
    } data;
} usb_event_t;

#define USB_EVENT_QUEUE_SIZE 32

typedef struct {
    usb_event_t events[USB_EVENT_QUEUE_SIZE];
    volatile uint32_t head;
    volatile uint32_t tail;
    volatile uint32_t count;
} usb_event_queue_t;

void usb_event_queue_init(usb_event_queue_t* queue);
bool usb_event_queue_enqueue(usb_event_queue_t* queue, const usb_event_t* event);
bool usb_event_queue_dequeue(usb_event_queue_t* queue, usb_event_t* event);
bool usb_event_queue_is_empty(const usb_event_queue_t* queue);
uint32_t usb_event_queue_count(const usb_event_queue_t* queue);

// Timing utilities (handles uint32_t wraparound correctly)
uint32_t time_delta_ms(uint32_t start_ms, uint32_t end_ms);

//--------------------------------------------------------------------+
// OUTPUT QUEUE (for events to send to computer)
//--------------------------------------------------------------------+

typedef enum {
    USB_OUTPUT_NONE = 0,
    USB_OUTPUT_MOUSE,
    USB_OUTPUT_KEYBOARD
} usb_output_type_t;

// Output event structure (no timestamp/sequence needed)
typedef struct {
    usb_output_type_t type;
    union {
        usb_mouse_data_t mouse;
        usb_keyboard_data_t keyboard;
    } data;
} usb_output_event_t;

#define USB_OUTPUT_QUEUE_SIZE 16

typedef struct {
    usb_output_event_t events[USB_OUTPUT_QUEUE_SIZE];
    volatile uint32_t head;
    volatile uint32_t tail;
    volatile uint32_t count;
} usb_output_queue_t;

// Output queue functions
void usb_output_queue_init(usb_output_queue_t* queue);
bool usb_output_queue_enqueue(usb_output_queue_t* queue, const usb_output_event_t* event);
bool usb_output_queue_dequeue(usb_output_queue_t* queue, usb_output_event_t* event);
bool usb_output_queue_is_empty(const usb_output_queue_t* queue);
uint32_t usb_output_queue_count(const usb_output_queue_t* queue);

// Helper functions for enqueuing output events
bool usb_output_enqueue_mouse(usb_output_queue_t* queue, int8_t delta_x, int8_t delta_y, uint8_t buttons, int8_t scroll);
bool usb_output_enqueue_keyboard(usb_output_queue_t* queue, uint8_t keycode, uint8_t modifier, bool pressed);

#endif
