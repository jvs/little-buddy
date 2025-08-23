#ifndef USB_HOST_H
#define USB_HOST_H

#include <stdint.h>
#include <stdbool.h>
#include <tusb.h>
#include "usb_events.h"

// HID device tracking
typedef struct {
    uint8_t dev_addr;
    uint8_t instance;
    uint8_t itf_protocol;  // HID_ITF_PROTOCOL_KEYBOARD, HID_ITF_PROTOCOL_MOUSE, etc.
    bool is_connected;
    uint16_t report_desc_len;
    bool has_keyboard;  // Detected keyboard usage in descriptor
    bool has_mouse;     // Detected mouse usage in descriptor
    uint8_t input_report_size;  // Size of input reports
} hid_device_info_t;

#define MAX_HID_DEVICES 4

// Global HID device array access
extern hid_device_info_t hid_devices[MAX_HID_DEVICES];

// USB Host initialization and management
void usb_host_init(void);
void usb_host_task(void);

// Event queue management
void usb_host_set_event_queue(usb_event_queue_t* queue, uint32_t* sequence_counter);

// HID descriptor parsing
void parse_hid_descriptor(uint8_t dev_addr, uint8_t instance, const uint8_t* desc, uint16_t desc_len);

// Debug control
void usb_host_set_debug_raw_reports(bool enable);

#endif // USB_HOST_H