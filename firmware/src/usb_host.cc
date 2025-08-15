#include "usb_host.h"
#include "usb_events.h"
#include "tusb.h"
#include "pico/time.h"
#include <stdio.h>

static usb_event_queue_t g_event_queue;
static uint32_t g_report_count = 0;

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
    if (desc_report && desc_len > 0) {
        // For now, we'll handle reports dynamically based on their content
        // Future: could parse descriptor to understand report structure
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
            // Handle composite device reports (like TrackPoint keyboard)
            if (len > 0) {
                // Try to detect report type based on content patterns
                uint8_t report_id = report[0];
                
                // TrackPoint keyboards typically use different report IDs:
                // Report ID 1: Keyboard data
                // Report ID 2: Mouse data  
                // Report ID 0: May be keyboard without report ID
                
                // TrackPoint keyboard analysis based on your observations:
                // Mouse buttons appear as: KEY=00 MOD=01/02/04 (mouse buttons in modifier field)
                // Mouse movement appears as: KEY=xx MOD=00 (movement data in keycode field)
                // Keyboard keys appear as: MOUSE DX=keycode DY=0 BTNS=00 (keyboard data in mouse fields)
                
                if (len >= 8) {
                    // 8-byte report - likely keyboard format but contains both keyboard and mouse data
                    uint8_t modifier = report[0];
                    uint8_t keycode = report[2]; // Skip reserved byte
                    
                    // Check if this is mouse data disguised as keyboard data
                    if (keycode == 0 && modifier != 0) {
                        // Mouse buttons: modifier field contains button bits
                        event.type = USB_EVENT_MOUSE;
                        event.data.mouse.buttons = modifier;
                        event.data.mouse.delta_x = 0;
                        event.data.mouse.delta_y = 0;
                        event.data.mouse.scroll = 0;
                        usb_event_queue_push(&g_event_queue, &event);
                    } else if (keycode != 0 && modifier == 0) {
                        // Mouse movement: keycode field contains movement data
                        event.type = USB_EVENT_MOUSE;
                        event.data.mouse.buttons = 0;
                        event.data.mouse.delta_x = (int8_t)keycode; // Movement in keycode field
                        event.data.mouse.delta_y = (len > 3) ? (int8_t)report[3] : 0; // Y movement might be in next byte
                        event.data.mouse.scroll = 0;
                        usb_event_queue_push(&g_event_queue, &event);
                    } else if (keycode != 0 && modifier != 0) {
                        // Real keyboard data with modifier
                        event.type = USB_EVENT_KEYBOARD;
                        event.data.keyboard.modifier = modifier;
                        event.data.keyboard.keycode = keycode;
                        event.data.keyboard.pressed = true;
                        usb_event_queue_push(&g_event_queue, &event);
                    }
                } else if (len >= 3) {
                    // Shorter report - check if this is keyboard data disguised as mouse data
                    // Based on your observation: Shift+A shows as "MOUSE DX=4 DY=0"
                    uint8_t buttons = report[0];
                    int8_t delta_x = (int8_t)report[1];
                    int8_t delta_y = (len > 2) ? (int8_t)report[2] : 0;
                    
                    if (buttons == 0 && delta_y == 0 && delta_x > 0 && delta_x < 50) {
                        // Likely keyboard data: DX contains keycode, DY is 0
                        event.type = USB_EVENT_KEYBOARD;
                        event.data.keyboard.keycode = delta_x;
                        event.data.keyboard.modifier = 0; // Modifier might be in separate report
                        event.data.keyboard.pressed = true;
                        usb_event_queue_push(&g_event_queue, &event);
                    } else if (buttons != 0 || delta_x != 0 || delta_y != 0) {
                        // Real mouse data
                        event.type = USB_EVENT_MOUSE;
                        event.data.mouse.buttons = buttons;
                        event.data.mouse.delta_x = delta_x;
                        event.data.mouse.delta_y = delta_y;
                        event.data.mouse.scroll = 0;
                        usb_event_queue_push(&g_event_queue, &event);
                    }
                } else {
                    // Unknown report type - show raw data for debugging
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