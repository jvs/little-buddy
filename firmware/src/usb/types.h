#ifndef USB_TYPES_H
#define USB_TYPES_H

#include <stdbool.h>
#include <stdint.h>


typedef enum {
    USB_INPUT_NONE = 0,
    USB_INPUT_MOUSE,
    USB_INPUT_KEYBOARD,
    USB_INPUT_DEVICE_CONNECTED,
    USB_INPUT_DEVICE_DISCONNECTED,
    USB_INPUT_TICK
} usb_input_type_t;


typedef enum {
    USB_OUTPUT_NONE = 0,
    USB_OUTPUT_MOUSE,
    USB_OUTPUT_KEYBOARD
} usb_output_type_t;


typedef struct {
    int8_t delta_x;
    int8_t delta_y;
    int8_t scroll;
    uint8_t buttons;
} usb_mouse_data_t;


typedef struct {
    uint8_t modifier;
    uint8_t keycodes[6];
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
    usb_input_type_t type;
    uint32_t timestamp_ms;
    uint32_t sequence_id;      // Event sequence number
    uint8_t interface_id;      // Which HID interface (0, 1, etc.)
    union {
        usb_mouse_data_t mouse;
        usb_keyboard_data_t keyboard;
        usb_device_data_t device;
        usb_tick_data_t tick;
    } data;
} usb_input_event_t;


// Output event structure (no timestamp/sequence needed)
typedef struct {
    usb_output_type_t type;
    union {
        usb_mouse_data_t mouse;
        usb_keyboard_data_t keyboard;
    } data;
} usb_output_event_t;


// Timing utilities (handles uint32_t wraparound correctly)
uint32_t time_delta_ms(uint32_t start_ms, uint32_t end_ms);

typedef void (*usb_input_event_callback_t)(usb_input_event_t *event);
typedef void (*usb_output_event_callback_t)(usb_output_event_t *event);

#endif
