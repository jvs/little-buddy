#include "usb_device.h"
#include "tusb.h"
#include <stdio.h>
#include <stdarg.h>

// Interface numbers (must match usb_descriptors.c)
#define ITF_NUM_HID_KEYBOARD  0  
#define ITF_NUM_HID_MOUSE     1

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

// CDC removed - no CDC callbacks needed

//--------------------------------------------------------------------+
// HID Report Sending Functions
//--------------------------------------------------------------------+

bool usb_device_send_keyboard_report(uint8_t modifier, uint8_t keycode) {
    // Standard HID keyboard report: [modifier, reserved, keycode1, keycode2, keycode3, keycode4, keycode5, keycode6]
    uint8_t report[8] = {0};
    report[0] = modifier;
    report[2] = keycode;  // Put keycode in first slot
    
    if (tud_hid_n_ready(ITF_NUM_HID_KEYBOARD)) {
        return tud_hid_n_report(ITF_NUM_HID_KEYBOARD, 0, report, sizeof(report));
    }
    return false;
}

bool usb_device_send_mouse_report(uint8_t buttons, int8_t delta_x, int8_t delta_y, int8_t scroll) {
    // Standard HID mouse report: [buttons, x, y, wheel]
    uint8_t report[4] = {0};
    report[0] = buttons;
    report[1] = (uint8_t)delta_x;
    report[2] = (uint8_t)delta_y;
    report[3] = (uint8_t)scroll;
    
    if (tud_hid_n_ready(ITF_NUM_HID_MOUSE)) {
        return tud_hid_n_report(ITF_NUM_HID_MOUSE, 0, report, sizeof(report));
    }
    return false;
}

