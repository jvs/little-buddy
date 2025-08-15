#include "usb_host.h"
#include "usb_events.h"
#include "hid_parser.h"
#include "tusb.h"
#include "pico/time.h"
#include <stdio.h>

static usb_event_queue_t g_event_queue;
static uint32_t g_report_count = 0;
static hid_descriptor_t g_hid_descriptors[CFG_TUH_HID] = {0};

void usb_host_init(void) {
    usb_event_queue_init(&g_event_queue);
    tuh_init(BOARD_TUH_RHPORT);
}

void usb_host_task(void) {
    tuh_task();
}

usb_event_queue_t* usb_host_get_event_queue(void) {
    return &g_event_queue;
}

uint32_t usb_host_get_report_count(void) {
    return g_report_count;
}

// TinyUSB callbacks
void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len) {
    uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);
    
    usb_event_t event;
    event.type = USB_EVENT_DEVICE_CONNECTED;
    event.timestamp_ms = to_ms_since_boot(get_absolute_time());
    event.data.device.device_address = dev_addr;
    event.data.device.instance = instance;
    
    // For composite devices like TrackPoint, protocol will be NONE
    // but we can still process the reports
    const char* protocol_str[] = { "COMPOSITE", "KEYBOARD", "MOUSE" };
    if (itf_protocol < 3) {
        event.data.device.device_type = protocol_str[itf_protocol];
    } else {
        event.data.device.device_type = "UNKNOWN";
    }
    
    usb_event_queue_push(&g_event_queue, &event);
    
    // Parse HID descriptor if available
    if (desc_report && desc_len > 0 && instance < CFG_TUH_HID) {
        if (hid_parse_descriptor(desc_report, desc_len, &g_hid_descriptors[instance])) {
            // Successfully parsed descriptor
            event.data.device.device_type = "PARSED";
        } else {
            // Failed to parse, fall back to old behavior
            event.data.device.device_type = protocol_str[itf_protocol];
        }
    } else {
        event.data.device.device_type = protocol_str[itf_protocol];
    }
    
    tuh_hid_receive_report(dev_addr, instance);
}

void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance) {
    usb_event_t event;
    event.type = USB_EVENT_DEVICE_DISCONNECTED;
    event.timestamp_ms = to_ms_since_boot(get_absolute_time());
    event.data.device.device_address = dev_addr;
    event.data.device.instance = instance;
    event.data.device.device_type = "UNKNOWN";
    
    usb_event_queue_push(&g_event_queue, &event);
    
    // Debug: device unmounted
}

void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len) {
    g_report_count++;  // Count all USB reports received
    
    uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);
    
    usb_event_t event;
    event.timestamp_ms = to_ms_since_boot(get_absolute_time());
    
    switch (itf_protocol) {
        case HID_ITF_PROTOCOL_MOUSE: {
            hid_mouse_report_t const* mouse_report = (hid_mouse_report_t const*) report;
            
            // Debug: mouse report received
            
            // Only send event if there's actual movement or button press
            if (mouse_report->x != 0 || mouse_report->y != 0 || 
                mouse_report->wheel != 0 || mouse_report->buttons != 0) {
                event.type = USB_EVENT_MOUSE;
                event.data.mouse.delta_x = mouse_report->x;
                event.data.mouse.delta_y = mouse_report->y;
                event.data.mouse.scroll = mouse_report->wheel;
                event.data.mouse.buttons = mouse_report->buttons;
                
                usb_event_queue_push(&g_event_queue, &event);
            }
            break;
        }
        
        case HID_ITF_PROTOCOL_KEYBOARD: {
            hid_keyboard_report_t const* kbd_report = (hid_keyboard_report_t const*) report;
            
            // Debug: keyboard report received
            
            // Send event if any key is pressed or modifier is active
            if (kbd_report->modifier != 0 || kbd_report->keycode[0] != 0) {
                event.type = USB_EVENT_KEYBOARD;
                event.data.keyboard.modifier = kbd_report->modifier;
                event.data.keyboard.keycode = kbd_report->keycode[0]; // Just show first key
                event.data.keyboard.pressed = true;
                usb_event_queue_push(&g_event_queue, &event);
            }
            break;
        }
        
        default:
            // Use HID descriptor parser if available
            if (instance < CFG_TUH_HID && g_hid_descriptors[instance].usage_count > 0) {
                hid_descriptor_t* desc = &g_hid_descriptors[instance];
                
                // Process each usage in the descriptor
                for (int i = 0; i < desc->usage_count; i++) {
                    hid_usage_t* usage = &desc->usages[i];
                    int32_t value;
                    
                    if (hid_extract_value(report, len, usage, &value)) {
                        input_type_t input_type = hid_get_input_type(usage);
                        
                        // Only create events for non-zero values (activity)
                        if (value != 0) {
                            switch (input_type) {
                                case INPUT_TYPE_MOUSE_X:
                                    event.type = USB_EVENT_MOUSE;
                                    event.data.mouse.delta_x = value;
                                    event.data.mouse.delta_y = 0;
                                    event.data.mouse.buttons = 0;
                                    event.data.mouse.scroll = 0;
                                    usb_event_queue_push(&g_event_queue, &event);
                                    break;
                                    
                                case INPUT_TYPE_MOUSE_Y:
                                    event.type = USB_EVENT_MOUSE;
                                    event.data.mouse.delta_x = 0;
                                    event.data.mouse.delta_y = value;
                                    event.data.mouse.buttons = 0;
                                    event.data.mouse.scroll = 0;
                                    usb_event_queue_push(&g_event_queue, &event);
                                    break;
                                    
                                case INPUT_TYPE_MOUSE_BUTTON:
                                    event.type = USB_EVENT_MOUSE;
                                    event.data.mouse.delta_x = 0;
                                    event.data.mouse.delta_y = 0;
                                    event.data.mouse.buttons = value;
                                    event.data.mouse.scroll = 0;
                                    usb_event_queue_push(&g_event_queue, &event);
                                    break;
                                    
                                case INPUT_TYPE_KEYBOARD_KEY:
                                    event.type = USB_EVENT_KEYBOARD;
                                    event.data.keyboard.keycode = value;
                                    event.data.keyboard.modifier = 0;
                                    event.data.keyboard.pressed = true;
                                    usb_event_queue_push(&g_event_queue, &event);
                                    break;
                                    
                                case INPUT_TYPE_KEYBOARD_MODIFIER:
                                    event.type = USB_EVENT_KEYBOARD;
                                    event.data.keyboard.keycode = 0;
                                    event.data.keyboard.modifier = value;
                                    event.data.keyboard.pressed = true;
                                    usb_event_queue_push(&g_event_queue, &event);
                                    break;
                                    
                                default:
                                    break;
                            }
                        }
                    }
                }
            } else {
                // Fallback to raw data display if no descriptor
                if (len > 0) {
                    event.type = USB_EVENT_KEYBOARD;
                    event.data.keyboard.keycode = report[0];
                    event.data.keyboard.modifier = (len > 1) ? report[1] : 0;
                    event.data.keyboard.pressed = true;
                    usb_event_queue_push(&g_event_queue, &event);
                }
            }
            break;
    }
    
    tuh_hid_receive_report(dev_addr, instance);
}