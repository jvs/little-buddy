#include "usb/usb.h"

#include <pico/stdlib.h>
#include <pio_usb.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <tusb.h>


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

// Global HID device array
hid_device_info_t hid_devices[MAX_HID_DEVICES];

// HID Descriptor parsing constants
#define HID_ITEM_USAGE_PAGE     0x05
#define HID_ITEM_USAGE          0x09
#define HID_ITEM_REPORT_SIZE    0x75
#define HID_ITEM_REPORT_COUNT   0x95
#define HID_ITEM_INPUT          0x81
#define HID_ITEM_COLLECTION     0xA1
#define HID_ITEM_END_COLLECTION 0xC0

// HID Usage Pages (values, not items)
#define HID_USAGE_PAGE_GENERIC_DESKTOP  0x01
#define HID_USAGE_PAGE_KEYBOARD         0x07
#define HID_USAGE_PAGE_BUTTON           0x09

// HID Usages (values, not items)
#define HID_USAGE_KEYBOARD              0x06
#define HID_USAGE_MOUSE                 0x02
#define HID_USAGE_POINTER               0x01

static uint32_t input_counter = 0;
static uint32_t last_tick_us = 0;
static uint32_t tick_counter = 0;


// Forward declarations
void enqueue_input_event(
    usb_input_type_t type,
    uint8_t device_address,
    uint8_t interface_id,
    void *event_data
);

void usb_host_init(void) {
    // Clear HID devices array
    memset(hid_devices, 0, sizeof(hid_devices));

    // Initialize PIO USB for host mode
    pio_usb_configuration_t pio_cfg = PIO_USB_DEFAULT_CONFIG;
    pio_cfg.pin_dp = PICO_DEFAULT_PIO_USB_DP_PIN;
    tuh_configure(BOARD_TUH_RHPORT, TUH_CFGID_RPI_PIO_USB_CONFIGURATION, &pio_cfg);

    // Initialize TinyUSB host stack
    tuh_init(BOARD_TUH_RHPORT);
}

void usb_host_task(void) {
    // TinyUSB host task
    tuh_task();

    // Check for 1ms tick events
    uint32_t current_time_us = time_us_32();

    if (current_time_us - last_tick_us >= 1000) { // 1000 microseconds = 1ms
        last_tick_us = current_time_us;
        tick_counter++;

        usb_tick_data_t tick_data;
        tick_data.tick_count = tick_counter;
        tick_data.delta_us = current_time_us - last_tick_us;
        enqueue_input_event(USB_INPUT_TICK, 0, 0, &tick_data);
    }
}

void enqueue_input_event(usb_input_type_t type, uint8_t device_address, uint8_t interface_id, void *event_data) {
    usb_input_event_t event;
    event.type = type;
    event.timestamp_ms = time_us_32() / 1000;  // Convert microseconds to milliseconds
    event.sequence_id = ++input_counter;
    event.interface_id = interface_id;

    // Copy event-specific data
    switch (type) {
        case USB_INPUT_MOUSE:
            if (event_data) {
                event.data.mouse = *(usb_mouse_data_t*)event_data;
            }
            break;
        case USB_INPUT_KEYBOARD:
            if (event_data) {
                event.data.keyboard = *(usb_keyboard_data_t*)event_data;
            }
            break;
        case USB_INPUT_DEVICE_CONNECTED:
        case USB_INPUT_DEVICE_DISCONNECTED:
            if (event_data) {
                event.data.device = *(usb_device_data_t*)event_data;
            } else {
                event.data.device.device_address = device_address;
                event.data.device.instance = interface_id;
                event.data.device.device_type = "HID";
            }
            break;
        case USB_INPUT_TICK:
            if (event_data) {
                event.data.tick = *(usb_tick_data_t*)event_data;
            }
            break;
        default:
            break;
    }

    // Try to enqueue the event
    if (!usb_input_enqueue(&event)) {
        // Queue is full
    }
}

//--------------------------------------------------------------------+
// HOST MODE CALLBACKS
//--------------------------------------------------------------------+

void tuh_mount_cb(uint8_t dev_addr) {
    // Device attached
}

void tuh_umount_cb(uint8_t dev_addr) {
    // Device removed
}

void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len) {
    // Find empty slot to store device info
    int device_slot = -1;
    for (int i = 0; i < MAX_HID_DEVICES; i++) {
        if (!hid_devices[i].is_connected) {
            hid_devices[i].dev_addr = dev_addr;
            hid_devices[i].instance = instance;
            hid_devices[i].itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);
            hid_devices[i].is_connected = true;
            hid_devices[i].report_desc_len = desc_len;
            hid_devices[i].has_keyboard = false;
            hid_devices[i].has_mouse = false;
            hid_devices[i].input_report_size = 0;
            device_slot = i;
            break;
        }
    }

    // Simple heuristic approach: assume devices can do both keyboard and mouse
    if (device_slot >= 0) {
        uint8_t itf_protocol = hid_devices[device_slot].itf_protocol;

        if (itf_protocol == HID_ITF_PROTOCOL_KEYBOARD) {
            // Standard keyboard
            hid_devices[device_slot].has_keyboard = true;
        } else if (itf_protocol == HID_ITF_PROTOCOL_MOUSE) {
            // Standard mouse
            hid_devices[device_slot].has_mouse = true;
        } else {
            // Unknown protocol - assume it's a composite device like trackpoint
            // Most trackpoint keyboards can do both
            hid_devices[device_slot].has_keyboard = true;
            hid_devices[device_slot].has_mouse = true;
        }
    }

    // Request to receive report
    tuh_hid_receive_report(dev_addr, instance);

    // Enqueue device connected event
    usb_device_data_t device_data;
    device_data.device_address = dev_addr;
    device_data.instance = instance;
    device_data.device_type = "HID";
    enqueue_input_event(USB_INPUT_DEVICE_CONNECTED, dev_addr, instance, &device_data);
}

void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance) {
    // Enqueue device disconnected event before clearing device info
    usb_device_data_t device_data;
    device_data.device_address = dev_addr;
    device_data.instance = instance;
    device_data.device_type = "HID";
    enqueue_input_event(USB_INPUT_DEVICE_DISCONNECTED, dev_addr, instance, &device_data);

    // Clear device info quietly
    for (int i = 0; i < MAX_HID_DEVICES; i++) {
        if (hid_devices[i].is_connected &&
            hid_devices[i].dev_addr == dev_addr &&
            hid_devices[i].instance == instance) {
            hid_devices[i].is_connected = false;
            break;
        }
    }
}

void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len) {
    // Find the device info
    for (int i = 0; i < MAX_HID_DEVICES; i++) {
        if (hid_devices[i].is_connected &&
            hid_devices[i].dev_addr == dev_addr &&
            hid_devices[i].instance == instance) {

            // Handle keyboard and mouse reports
            if (len == 8) {
                // Trackpoint keyboard report: [modifier, reserved, key1, key2, key3, key4, key5, key6]
                usb_keyboard_data_t keyboard_data;
                keyboard_data.modifier = report[0];
                memcpy(&keyboard_data.keycodes, report + 2, 6);
                enqueue_input_event(USB_INPUT_KEYBOARD, dev_addr, instance, &keyboard_data);
            } else if (len == 6 && report[0] == 0x01) {
                // Trackpoint mouse report: [0x01, buttons, x, y, wheel, ?]
                usb_mouse_data_t mouse_data;
                mouse_data.buttons = report[1];
                mouse_data.delta_x = (int8_t)report[2];
                mouse_data.delta_y = (int8_t)report[3];
                mouse_data.scroll = (int8_t)report[4];
                enqueue_input_event(USB_INPUT_MOUSE, dev_addr, instance, &mouse_data);
            }
            break;
        }
    }

    // Continue to request next report
    tuh_hid_receive_report(dev_addr, instance);
}
