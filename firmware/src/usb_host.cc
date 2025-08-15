#include "usb_host.h"
#include "usb_events.h"
#include "hid_parser.h"
#include "usb_device.h"
#include "tusb.h"
#include "pico/time.h"
#include <stdio.h>

static usb_event_queue_t g_event_queue;
static uint32_t g_report_count = 0;
static uint32_t g_mount_count = 0;
static uint32_t g_sequence_counter = 0;
static hid_descriptor_t g_hid_descriptors[CFG_TUH_HID] = {0};
static uint8_t g_interface_protocols[CFG_TUH_HID] = {0}; // Track each interface's protocol

void usb_host_init(void) {
    usb_event_queue_init(&g_event_queue);
    // Don't init here - will be done by unified tusb_init()
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

uint32_t usb_host_get_mount_count(void) {
    return g_mount_count;
}

const char* usb_host_get_interface_info(uint8_t instance) {
    if (instance >= CFG_TUH_HID) return "INVALID";
    
    static const char* protocol_names[] = { "COMPOSITE", "KEYBOARD", "MOUSE" };
    uint8_t protocol = g_interface_protocols[instance];
    
    if (protocol < 3) {
        return protocol_names[protocol];
    }
    return "UNKNOWN";
}

// TinyUSB callbacks
void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len) {
    g_mount_count++;  // Track mount calls
    uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);
    
    debug_printf("=== HID MOUNT ===\n");
    debug_printf("Device: addr=%d, instance=%d\n", dev_addr, instance);
    debug_printf("Protocol: %d (0=none, 1=kbd, 2=mouse)\n", itf_protocol);
    debug_printf("Descriptor: len=%d, ptr=%p\n", desc_len, desc_report);
    
    // Store protocol for this interface
    if (instance < CFG_TUH_HID) {
        g_interface_protocols[instance] = itf_protocol;
    }
    
    usb_event_t event;
    event.type = USB_EVENT_DEVICE_CONNECTED;
    event.timestamp_ms = to_ms_since_boot(get_absolute_time());
    event.sequence_id = g_sequence_counter++;
    event.interface_id = instance;
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
    
    // Parse HID descriptor if available first, before pushing event
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
    
    usb_event_queue_push(&g_event_queue, &event);
    
    // Request first report
    if (!tuh_hid_receive_report(dev_addr, instance)) {
        // If this fails, we won't get any reports
        // Create a debug event to show the failure
        usb_event_t debug_event;
        debug_event.type = USB_EVENT_KEYBOARD;
        debug_event.timestamp_ms = to_ms_since_boot(get_absolute_time());
        debug_event.sequence_id = g_sequence_counter++;
        debug_event.interface_id = instance;
        debug_event.data.keyboard.keycode = 0xFF; // Special code to indicate error
        debug_event.data.keyboard.modifier = 0xFF;
        debug_event.data.keyboard.pressed = true;
        usb_event_queue_push(&g_event_queue, &debug_event);
    }
}

void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance) {
    usb_event_t event;
    event.type = USB_EVENT_DEVICE_DISCONNECTED;
    event.timestamp_ms = to_ms_since_boot(get_absolute_time());
    event.sequence_id = g_sequence_counter++;
    event.interface_id = instance;
    event.data.device.device_address = dev_addr;
    event.data.device.instance = instance;
    event.data.device.device_type = "UNKNOWN";
    
    usb_event_queue_push(&g_event_queue, &event);
    
    // Debug: device unmounted
}

void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len) {
    g_report_count++;  // Count all USB reports received
    
    // Use stored protocol for this interface
    uint8_t const itf_protocol = (instance < CFG_TUH_HID) ? g_interface_protocols[instance] : HID_ITF_PROTOCOL_NONE;
    
    // Debug: Show raw report data
    debug_printf("REPORT IF%d: len=%d, bytes=[", instance, len);
    for (int i = 0; i < len && i < 8; i++) {
        debug_printf("%02X ", report[i]);
    }
    debug_printf("]\n");
    
    usb_event_t event;
    event.timestamp_ms = to_ms_since_boot(get_absolute_time());
    event.sequence_id = g_sequence_counter++;
    event.interface_id = instance;
    
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
            // TrackPoint keyboard sends composite reports - need pattern detection
            if (len >= 8) {
                // 8-byte report: likely keyboard format
                // Based on your observations: mouse buttons in modifier field, movement in keycode
                uint8_t modifier = report[0];
                uint8_t keycode = report[2]; // Skip reserved byte
                
                if (keycode == 0 && modifier != 0) {
                    // Mouse buttons: modifier contains button bits
                    event.type = USB_EVENT_MOUSE;
                    event.data.mouse.buttons = modifier;
                    event.data.mouse.delta_x = 0;
                    event.data.mouse.delta_y = 0;
                    event.data.mouse.scroll = 0;
                    usb_event_queue_push(&g_event_queue, &event);
                } else if (keycode != 0 && modifier == 0) {
                    // Mouse movement: keycode contains X movement
                    event.type = USB_EVENT_MOUSE;
                    event.data.mouse.buttons = 0;
                    event.data.mouse.delta_x = (int8_t)keycode;
                    event.data.mouse.delta_y = (len > 3) ? (int8_t)report[3] : 0;
                    event.data.mouse.scroll = 0;
                    usb_event_queue_push(&g_event_queue, &event);
                } else if (keycode != 0) {
                    // Real keyboard data
                    event.type = USB_EVENT_KEYBOARD;
                    event.data.keyboard.modifier = modifier;
                    event.data.keyboard.keycode = keycode;
                    event.data.keyboard.pressed = true;
                    usb_event_queue_push(&g_event_queue, &event);
                }
            } else if (len >= 3 && len <= 5) {
                // 3-5 byte report: might be keyboard data disguised as mouse
                // Based on observation: Shift+A shows as "MOUSE DX=4 DY=0"
                uint8_t buttons = report[0];
                int8_t delta_x = (int8_t)report[1];
                int8_t delta_y = (len > 2) ? (int8_t)report[2] : 0;
                
                if (buttons == 0 && delta_y == 0 && delta_x > 0 && delta_x < 50) {
                    // Keyboard data in mouse format: DX=keycode
                    event.type = USB_EVENT_KEYBOARD;
                    event.data.keyboard.keycode = delta_x;
                    event.data.keyboard.modifier = 0;
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
            }
            break;
    }
    
    tuh_hid_receive_report(dev_addr, instance);
}