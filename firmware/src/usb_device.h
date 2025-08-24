#ifndef USB_DEVICE_H
#define USB_DEVICE_H

#include <stdint.h>
#include <stdbool.h>
#include <tusb.h>
#include "usb_events.h"

// USB Device initialization and management
void usb_device_init(void);
void usb_device_task(void);

void usb_device_flush_output_queue(output_queue_t *queue);

// Interface numbers for device configuration
enum {
    ITF_NUM_HID_KEYBOARD = 0,
    ITF_NUM_HID_MOUSE,
    ITF_NUM_TOTAL
};

// Configuration constants
#define CONFIG_TOTAL_LEN  (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN + TUD_HID_DESC_LEN)
#define EPNUM_HID_KEYBOARD 0x81
#define EPNUM_HID_MOUSE   0x82

#endif // USB_DEVICE_H
