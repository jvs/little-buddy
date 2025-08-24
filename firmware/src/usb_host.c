#include "usb_host.h"
#include <pico/stdlib.h>
#include <pio_usb.h>
#include <string.h>
#include <stdio.h>

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

// Event queue management
static usb_event_queue_t* event_queue = NULL;
static uint32_t* sequence_counter = NULL;


// Tick event timing
static uint32_t last_tick_us = 0;
static uint32_t tick_counter = 0;


// Forward declarations
void enqueue_usb_event(usb_event_type_t type, uint8_t device_address, uint8_t interface_id, void* event_data);

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
        enqueue_usb_event(USB_EVENT_TICK, 0, 0, &tick_data);
    }
}

void usb_host_set_event_queue(usb_event_queue_t* queue, uint32_t* seq_counter) {
    event_queue = queue;
    sequence_counter = seq_counter;
}

void enqueue_usb_event(usb_event_type_t type, uint8_t device_address, uint8_t interface_id, void* event_data) {
    if (!event_queue || !sequence_counter) return;

    usb_event_t event;
    event.type = type;
    event.timestamp_ms = time_us_32() / 1000;  // Convert microseconds to milliseconds
    event.sequence_id = ++(*sequence_counter);
    event.interface_id = interface_id;

    // Copy event-specific data
    switch (type) {
        case USB_EVENT_MOUSE:
            if (event_data) {
                event.data.mouse = *(usb_mouse_data_t*)event_data;
            }
            break;
        case USB_EVENT_KEYBOARD:
            if (event_data) {
                event.data.keyboard = *(usb_keyboard_data_t*)event_data;
            }
            break;
        case USB_EVENT_DEVICE_CONNECTED:
        case USB_EVENT_DEVICE_DISCONNECTED:
            if (event_data) {
                event.data.device = *(usb_device_data_t*)event_data;
            } else {
                event.data.device.device_address = device_address;
                event.data.device.instance = interface_id;
                event.data.device.device_type = "HID";
            }
            break;
        case USB_EVENT_TICK:
            if (event_data) {
                event.data.tick = *(usb_tick_data_t*)event_data;
            }
            break;
        default:
            break;
    }

    // Try to enqueue the event
    if (!usb_event_queue_enqueue(event_queue, &event)) {
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
    enqueue_usb_event(USB_EVENT_DEVICE_CONNECTED, dev_addr, instance, &device_data);
}

void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance) {
    // Enqueue device disconnected event before clearing device info
    usb_device_data_t device_data;
    device_data.device_address = dev_addr;
    device_data.instance = instance;
    device_data.device_type = "HID";
    enqueue_usb_event(USB_EVENT_DEVICE_DISCONNECTED, dev_addr, instance, &device_data);

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
                usb_keyboard_data_t kbd_data;
                memcpy(&kbd_data.keycodes, report + 2, 6);
                enqueue_usb_event(USB_EVENT_KEYBOARD, dev_addr, instance, &kbd_data);
            } else if (len == 6 && report[0] == 0x01) {
                // Trackpoint mouse report: [0x01, buttons, x, y, wheel, ?]
                usb_mouse_data_t mouse_data;
                mouse_data.buttons = report[1];
                mouse_data.delta_x = (int8_t)report[2];
                mouse_data.delta_y = (int8_t)report[3];
                mouse_data.scroll = (int8_t)report[4];
                enqueue_usb_event(USB_EVENT_MOUSE, dev_addr, instance, &mouse_data);
            }
            break;
        }
    }

    // Continue to request next report
    tuh_hid_receive_report(dev_addr, instance);
}

//--------------------------------------------------------------------+
// HID DESCRIPTOR PARSER
//--------------------------------------------------------------------+

void parse_hid_descriptor(uint8_t dev_addr, uint8_t instance, const uint8_t* desc, uint16_t desc_len) {

    // Find the device slot
    int device_slot = -1;
    for (int i = 0; i < MAX_HID_DEVICES; i++) {
        if (hid_devices[i].is_connected &&
            hid_devices[i].dev_addr == dev_addr &&
            hid_devices[i].instance == instance) {
            device_slot = i;
            break;
        }
    }

    if (device_slot == -1) return;

    uint16_t pos = 0;
    uint8_t current_usage_page = 0;
    uint8_t current_usage = 0;
    uint8_t report_size = 0;
    uint8_t report_count = 0;

    while (pos < desc_len) {
        uint8_t item = desc[pos];
        uint8_t size = item & 0x03;
        uint8_t type = (item >> 2) & 0x03;
        uint8_t tag = (item >> 4) & 0x0F;

        pos++; // Move past the item byte

        // Extract data based on size
        uint32_t data = 0;
        for (int i = 0; i < size; i++) {
            if (pos + i < desc_len) {
                data |= (desc[pos + i] << (i * 8));
            }
        }
        pos += size;

        // Parse based on tag
        switch (item & 0xFC) { // Mask out size bits
            case HID_ITEM_USAGE_PAGE:
                current_usage_page = data;
                break;

            case HID_ITEM_USAGE:
                current_usage = data;

                // Check for keyboard or mouse usage
                if (current_usage_page == HID_USAGE_PAGE_GENERIC_DESKTOP) {
                    if (current_usage == HID_USAGE_KEYBOARD) {
                        hid_devices[device_slot].has_keyboard = true;
                    } else if (current_usage == HID_USAGE_MOUSE || current_usage == HID_USAGE_POINTER) {
                        hid_devices[device_slot].has_mouse = true;
                    }
                } else if (current_usage_page == HID_USAGE_PAGE_KEYBOARD) {
                    hid_devices[device_slot].has_keyboard = true;
                }
                break;

            case HID_ITEM_REPORT_SIZE:
                report_size = data;
                break;

            case HID_ITEM_REPORT_COUNT:
                report_count = data;
                break;

            case HID_ITEM_INPUT:
                // This defines an input report field
                if (report_size > 0 && report_count > 0) {
                    uint8_t field_size = (report_size * report_count + 7) / 8; // Convert bits to bytes
                    if (field_size > hid_devices[device_slot].input_report_size) {
                        hid_devices[device_slot].input_report_size += field_size;
                    }
                }
                break;
        }
    }

    // Descriptor parsed
}
