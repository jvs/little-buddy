#include "usb_device.h"
#include "tusb.h"
#include <stdio.h>
#include <stdarg.h>

// Interface numbers (must match usb_descriptors.c)
#define ITF_NUM_HID_KEYBOARD  0  
#define ITF_NUM_HID_MOUSE     1

// Report IDs (must match usb_descriptors.c)
#define REPORT_ID_MOUSE 1
#define REPORT_ID_KEYBOARD 2

// Debug printf - now just a stub since no CDC
int debug_printf(const char* format, ...) {
    // CDC removed - debug output disabled
    (void)format;
    return 0;
}

void usb_device_init(void) {
    // Don't init here - will be done by unified tusb_init()
}

void usb_device_task(void) {
    tud_task();
}

//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+

// Invoked when device is mounted
void tud_mount_cb(void)
{
    // Device is mounted and ready
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
}

// Invoked when usb bus is resumed
void tud_resume_cb(void)
{
}

//--------------------------------------------------------------------+
// USB HID
//--------------------------------------------------------------------+

// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
uint16_t tud_hid_get_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen)
{
    // TODO implement if needed
    (void) itf;
    (void) report_id;
    (void) report_type;
    (void) buffer;
    (void) reqlen;

    return 0;
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void tud_hid_set_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize)
{
    // TODO implement if needed for LEDs, etc.
    (void) itf;
    (void) report_id;
    (void) report_type;
    (void) buffer;
    (void) bufsize;
}

// Missing HID callback that hid-remapper has
void tud_hid_set_protocol_cb(uint8_t instance, uint8_t protocol) {
    // Handle HID boot protocol negotiation (required for proper enumeration)
    debug_printf("tud_hid_set_protocol_cb %d %d\n", instance, protocol);
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
    
    if (tud_hid_n_ready(ITF_NUM_HID_KEYBOARD)) {
        return tud_hid_n_report(ITF_NUM_HID_KEYBOARD, REPORT_ID_KEYBOARD, report, sizeof(report));
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
    
    if (tud_hid_n_ready(ITF_NUM_HID_KEYBOARD)) {  // Send through keyboard interface (contains all HID reports)
        return tud_hid_n_report(ITF_NUM_HID_KEYBOARD, REPORT_ID_MOUSE, report, sizeof(report));
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

