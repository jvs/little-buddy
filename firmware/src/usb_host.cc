#include "usb_host.h"
#include "usb_events.h"
#include "tusb.h"
#include "pico/time.h"
#include <stdio.h>

static usb_event_queue_t g_event_queue;

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

// TinyUSB callbacks
void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len) {
    uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);
    
    usb_event_t event;
    event.type = USB_EVENT_DEVICE_CONNECTED;
    event.timestamp_ms = to_ms_since_boot(get_absolute_time());
    event.data.device.device_address = dev_addr;
    event.data.device.instance = instance;
    
    const char* protocol_str[] = { "Unknown", "Keyboard", "Mouse" };
    if (itf_protocol < 3) {
        event.data.device.device_type = protocol_str[itf_protocol];
    } else {
        event.data.device.device_type = "Unknown";
    }
    
    usb_event_queue_push(&g_event_queue, &event);
    
    printf("HID device mounted: addr=%d, instance=%d, protocol=%s\r\n", 
           dev_addr, instance, event.data.device.device_type);
    
    if (!tuh_hid_receive_report(dev_addr, instance)) {
        printf("Error: cannot request to receive report\r\n");
    }
}

void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance) {
    usb_event_t event;
    event.type = USB_EVENT_DEVICE_DISCONNECTED;
    event.timestamp_ms = to_ms_since_boot(get_absolute_time());
    event.data.device.device_address = dev_addr;
    event.data.device.instance = instance;
    event.data.device.device_type = "Unknown";
    
    usb_event_queue_push(&g_event_queue, &event);
    
    printf("HID device unmounted: addr=%d, instance=%d\r\n", dev_addr, instance);
}

void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len) {
    uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);
    
    usb_event_t event;
    event.timestamp_ms = to_ms_since_boot(get_absolute_time());
    
    switch (itf_protocol) {
        case HID_ITF_PROTOCOL_MOUSE: {
            hid_mouse_report_t const* mouse_report = (hid_mouse_report_t const*) report;
            
            event.type = USB_EVENT_MOUSE;
            event.data.mouse.delta_x = mouse_report->x;
            event.data.mouse.delta_y = mouse_report->y;
            event.data.mouse.scroll = mouse_report->wheel;
            event.data.mouse.buttons = mouse_report->buttons;
            
            usb_event_queue_push(&g_event_queue, &event);
            break;
        }
        
        case HID_ITF_PROTOCOL_KEYBOARD: {
            hid_keyboard_report_t const* kbd_report = (hid_keyboard_report_t const*) report;
            
            event.type = USB_EVENT_KEYBOARD;
            event.data.keyboard.modifier = kbd_report->modifier;
            
            for (int i = 0; i < 6; i++) {
                if (kbd_report->keycode[i] != 0) {
                    event.data.keyboard.keycode = kbd_report->keycode[i];
                    event.data.keyboard.pressed = true;
                    usb_event_queue_push(&g_event_queue, &event);
                }
            }
            break;
        }
        
        default:
            break;
    }
    
    if (!tuh_hid_receive_report(dev_addr, instance)) {
        printf("Error: cannot request next report\r\n");
    }
}