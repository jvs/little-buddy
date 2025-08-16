#include "usb_host.h"
#include "usb_events.h"
#include "hid_parser.h"
#include "usb_device.h"
#include "tusb.h"
#include "pico/time.h"
#include <stdio.h>

static usb_event_queue_t* g_event_queue = nullptr;
static uint32_t g_report_count = 0;
static uint32_t g_mount_count = 0;
static uint32_t g_sequence_counter = 0;
static hid_descriptor_t g_hid_descriptors[CFG_TUH_HID] = {0};
static uint8_t g_interface_protocols[CFG_TUH_HID] = {0}; // Track each interface's protocol
static last_report_t g_last_reports[CFG_TUH_HID] = {0}; // Track last report from each interface

void usb_host_init(usb_event_queue_t* queue) {
    g_event_queue = queue;
}

void usb_host_task(void) {
    tuh_task();
}

usb_event_queue_t* usb_host_get_event_queue(void) {
    return g_event_queue;
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

const last_report_t* usb_host_get_last_report(uint8_t instance) {
    if (instance >= CFG_TUH_HID) return NULL;
    return &g_last_reports[instance];
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
    
    if (g_event_queue) usb_event_queue_push(g_event_queue, &event);
    
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
        if (g_event_queue) usb_event_queue_push(g_event_queue, &debug_event);
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
    
    if (g_event_queue) usb_event_queue_push(g_event_queue, &event);
    
    // Debug: device unmounted
}

void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len) {
    g_report_count++;  // Count all USB reports received
    
    // Store raw report data for display debugging
    if (instance < CFG_TUH_HID) {
        g_last_reports[instance].length = (len > 16) ? 16 : len;
        g_last_reports[instance].timestamp_ms = to_ms_since_boot(get_absolute_time());
        for (int i = 0; i < g_last_reports[instance].length; i++) {
            g_last_reports[instance].data[i] = report[i];
        }
    }
    
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
    
    // Parse based on interface number since TrackPoint shows as COMPOSITE
    if (instance == 0 && len == 8) {
        // IF0: Keyboard (8 bytes) - Standard HID format
        // Byte 0: modifier, Byte 1: reserved, Byte 2: keycode
        uint8_t modifier = report[0];
        uint8_t keycode = report[2];
        
        // Send event if any key is pressed or modifier is active
        if (modifier != 0 || keycode != 0) {
            event.type = USB_EVENT_KEYBOARD;
            event.data.keyboard.modifier = modifier;
            event.data.keyboard.keycode = keycode;
            event.data.keyboard.pressed = true;
            if (g_event_queue) usb_event_queue_push(g_event_queue, &event);
        }
    } else if (instance == 1 && len == 6) {
        // IF1: Mouse (6 bytes) - Custom format with report ID
        // Byte 0: report ID (always 01), Byte 1: buttons, Bytes 2-5: movement
        uint8_t buttons = report[1];
        int8_t delta_x = (int8_t)report[2];
        int8_t delta_y = (int8_t)report[3];
        
        // Send event if there's movement or button activity
        // Note: idle state is "01000000" not all zeros
        if (buttons != 0 || delta_x != 0 || delta_y != 0) {
            event.type = USB_EVENT_MOUSE;
            event.data.mouse.buttons = buttons;
            event.data.mouse.delta_x = delta_x;
            event.data.mouse.delta_y = delta_y;
            event.data.mouse.scroll = 0;  // Might be in bytes 4-5 if needed
            if (g_event_queue) usb_event_queue_push(g_event_queue, &event);
        }
    }
    
    tuh_hid_receive_report(dev_addr, instance);
}