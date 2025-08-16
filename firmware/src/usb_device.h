#ifndef USB_DEVICE_H
#define USB_DEVICE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void usb_device_init(void);
void usb_device_task(void);
int debug_printf(const char* format, ...);

// HID report sending functions
bool usb_device_send_keyboard_report(uint8_t modifier, uint8_t keycode);
bool usb_device_send_mouse_report(uint8_t buttons, int8_t delta_x, int8_t delta_y, int8_t scroll);

// Test functions
bool usb_device_send_test_mouse_movement(void);

// Debug counters (from usb_descriptors.c)
uint32_t usb_get_device_desc_calls(void);
uint32_t usb_get_config_desc_calls(void);
uint32_t usb_get_hid_desc_calls(void);

// Debug counters (from usb_device.cc)
uint32_t usb_device_get_mount_calls(void);

#ifdef __cplusplus
}
#endif

#endif
