#ifndef USB_DEVICE_H
#define USB_DEVICE_H

#include <stdint.h>
#include <stdbool.h>
#include <tusb.h>
#include "usb_events.h"

// USB Device initialization and management
void usb_device_init(void);
void usb_device_task(void);

// HID device helper functions
void send_keyboard_report(uint8_t modifier, uint8_t keycode);
void send_mouse_report(int8_t delta_x, int8_t delta_y, uint8_t buttons);

// Output queue processing
void usb_device_process_output_queue(usb_output_queue_t* queue);

// Interface numbers for device configuration
enum {
    ITF_NUM_CDC = 0,
    ITF_NUM_CDC_DATA,
    ITF_NUM_HID_KEYBOARD,
    ITF_NUM_HID_MOUSE,
    ITF_NUM_TOTAL
};

// Configuration constants
#define CONFIG_TOTAL_LEN  (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN + TUD_HID_DESC_LEN + TUD_HID_DESC_LEN)
#define EPNUM_CDC_NOTIF   0x81
#define EPNUM_CDC_OUT     0x02
#define EPNUM_CDC_IN      0x82
#define EPNUM_HID_KEYBOARD 0x83
#define EPNUM_HID_MOUSE   0x84

#endif // USB_DEVICE_H
