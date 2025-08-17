#include "usb_device.h"
#include "tusb.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>

// Interface numbers (must match usb_descriptors.c)
// Single combined HID interface for both keyboard and mouse
#define ITF_NUM_HID_COMBINED  0

// Report IDs (must match usb_descriptors.c)
#define REPORT_ID_MOUSE 1
#define REPORT_ID_KEYBOARD 2

// Debug counters for TinyUSB device stack activity
static uint32_t g_mount_calls = 0;
static uint32_t g_suspend_calls = 0;
static uint32_t g_resume_calls = 0;
static uint32_t g_tud_task_calls = 0;

// Debug printf - now just a stub since no CDC
int debug_printf(const char* format, ...) {
    // CDC removed - debug output disabled
    (void)format;
    return 0;
}

// Getter functions for display debugging
uint32_t usb_device_get_mount_calls(void) { return g_mount_calls; }
uint32_t usb_device_get_suspend_calls(void) { return g_suspend_calls; }
uint32_t usb_device_get_tud_task_calls(void) { return g_tud_task_calls; }

void usb_device_init(void) {
    // Don't init here - will be done by unified tusb_init()
}

void usb_device_task(void) {
    g_tud_task_calls++;
    tud_task();
}

//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+

// Invoked when device is mounted
void tud_mount_cb(void)
{
    // Device is mounted and ready
    g_mount_calls++;
    debug_printf("USB Device mounted!\n");
    
    // Reset boot protocol state on mount (like hid-remapper does)
    // This ensures proper state management for HID enumeration
}

// Invoked when device is unmounted
void tud_umount_cb(void)
{
    // Device is unmounted
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en)
{
    (void) remote_wakeup_en;
    g_suspend_calls++;
}

// Invoked when usb bus is resumed
void tud_resume_cb(void)
{
    g_resume_calls++;
}

//--------------------------------------------------------------------+
// USB HID
//--------------------------------------------------------------------+

// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
uint16_t tud_hid_get_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen)
{
    // Handle GET_REPORT requests properly for device enumeration
    debug_printf("tud_hid_get_report_cb itf=%d rid=%d type=%d len=%d\n", itf, report_id, report_type, reqlen);
    
    if (itf == ITF_NUM_HID_COMBINED) {
        // For HID enumeration, we need to handle certain report requests
        if (report_type == HID_REPORT_TYPE_FEATURE) {
            // Handle feature reports (like resolution multiplier)
            if (report_id == 99 && reqlen >= 1) { // REPORT_ID_MULTIPLIER
                buffer[0] = 0x01; // Default resolution multiplier value
                return 1;
            }
        } else if (report_type == HID_REPORT_TYPE_INPUT) {
            // For input reports, return empty/default state
            if (report_id == REPORT_ID_KEYBOARD && reqlen >= 17) {
                memset(buffer, 0, 17); // Clear keyboard report
                return 17;
            } else if (report_id == REPORT_ID_MOUSE && reqlen >= 10) {
                memset(buffer, 0, 10); // Clear mouse report  
                return 10;
            }
        }
    } else {
        // Handle config/vendor interface requests
        if (report_type == HID_REPORT_TYPE_FEATURE && reqlen > 0) {
            memset(buffer, 0, reqlen); // Return zeros for config
            return reqlen;
        }
    }

    return 0; // STALL for unsupported requests
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void tud_hid_set_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize)
{
    // Handle SET_REPORT requests properly for device enumeration
    debug_printf("tud_hid_set_report_cb itf=%d rid=%d type=%d size=%d\n", itf, report_id, report_type, bufsize);
    
    if (itf == ITF_NUM_HID_COMBINED) {
        // Handle report ID extraction if needed (like hid-remapper does)
        if ((report_id == 0) && (report_type == 0) && (bufsize > 0)) {
            report_id = buffer[0];
            buffer++;
            bufsize--;
        }
        
        // Handle specific reports
        if (report_type == HID_REPORT_TYPE_FEATURE) {
            if (report_id == 99 && bufsize >= 1) { // REPORT_ID_MULTIPLIER
                // Store resolution multiplier value (though we don't use it)
                debug_printf("Resolution multiplier set to: %d\n", buffer[0]);
            }
        } else if (report_type == HID_REPORT_TYPE_OUTPUT || (report_id == 98)) { // REPORT_ID_LEDS
            // Handle LED report (caps lock, num lock, etc.)
            if (bufsize >= 1) {
                debug_printf("LED state set to: 0x%02X\n", buffer[0]);
            }
        }
    } else {
        // Handle config interface reports
        if (report_type == HID_REPORT_TYPE_FEATURE) {
            debug_printf("Config report received, size: %d\n", bufsize);
            // Accept config data but don't need to do anything with it
        }
    }
}

// Missing HID callback that hid-remapper has
void tud_hid_set_protocol_cb(uint8_t instance, uint8_t protocol) {
    // Handle HID boot protocol negotiation (required for proper enumeration)
    debug_printf("tud_hid_set_protocol_cb instance=%d protocol=%d\n", instance, protocol);
    
    // Track boot protocol state like hid-remapper does
    static bool boot_protocol_keyboard = false;
    boot_protocol_keyboard = (protocol == HID_PROTOCOL_BOOT);
    
    // This callback is critical for proper HID enumeration
    // macOS and other systems may require this for device acceptance
}

//--------------------------------------------------------------------+
// HID Report Sending Functions
//--------------------------------------------------------------------+

bool usb_device_send_keyboard_report(uint8_t modifier, uint8_t keycode) {
    // Custom keyboard report with Report ID (matching hid-remapper format)
    // Based on hid-remapper descriptor: 1 byte modifier + 112 bits (14 bytes) for keys + 5 bits + 2 bits + 1 bit padding
    uint8_t report[17] = {0};  // 1 + 1 + 14 + 1 = 17 bytes total
    report[0] = REPORT_ID_KEYBOARD;
    report[1] = modifier;  // Modifier byte
    
    // Set keycode bit in the 112-bit array (14 bytes)
    if (keycode >= 0x04 && keycode <= 0x73) {
        uint8_t bit_index = keycode - 0x04;
        uint8_t byte_index = 2 + (bit_index / 8);
        uint8_t bit_offset = bit_index % 8;
        report[byte_index] |= (1 << bit_offset);
    }
    
    if (tud_hid_n_ready(ITF_NUM_HID_COMBINED)) {
        return tud_hid_n_report(ITF_NUM_HID_COMBINED, REPORT_ID_KEYBOARD, report, sizeof(report));
    }
    return false;
}

bool usb_device_send_mouse_report(uint8_t buttons, int8_t delta_x, int8_t delta_y, int8_t scroll) {
    // Custom mouse report with Report ID (matching hid-remapper format)
    // Report format: [Report ID, buttons, x_low, x_high, y_low, y_high, wheel_low, wheel_high, pan_low, pan_high]
    uint8_t report[10] = {0};
    report[0] = 1;  // REPORT_ID_MOUSE
    report[1] = buttons;
    
    // Convert 8-bit deltas to 16-bit (little endian)
    int16_t x16 = (int16_t)delta_x;
    int16_t y16 = (int16_t)delta_y;
    int16_t scroll16 = (int16_t)scroll;
    
    report[2] = x16 & 0xFF;        // x low byte
    report[3] = (x16 >> 8) & 0xFF; // x high byte
    report[4] = y16 & 0xFF;        // y low byte
    report[5] = (y16 >> 8) & 0xFF; // y high byte
    report[6] = scroll16 & 0xFF;        // wheel low byte
    report[7] = (scroll16 >> 8) & 0xFF; // wheel high byte
    // report[8], report[9] = pan (leave as 0)
    
    if (tud_hid_n_ready(ITF_NUM_HID_COMBINED)) {  // Send through combined interface
        return tud_hid_n_report(ITF_NUM_HID_COMBINED, REPORT_ID_MOUSE, report, sizeof(report));
    }
    return false;
}

//--------------------------------------------------------------------+
// Test Functions
//--------------------------------------------------------------------+

bool usb_device_send_test_mouse_movement(void) {
    // Send a small mouse movement to test device functionality
    return usb_device_send_mouse_report(0, 1, 1, 0);  // Move 1 pixel right and down
}

