#ifndef NEOPIXEL_H
#define NEOPIXEL_H

#include <stdint.h>
#include <stdbool.h>

// NeoPixel initialization and control
bool neopixel_init(void);
void neopixel_set_color(uint8_t r, uint8_t g, uint8_t b);
void neopixel_off(void);
void neopixel_set_brightness(uint8_t brightness); // 0-255

// Status indication helpers
typedef enum {
    NEOPIXEL_STATUS_OFF,
    NEOPIXEL_STATUS_STARTING,
    NEOPIXEL_STATUS_USB_READY,
    NEOPIXEL_STATUS_DEVICE_DETECTED,
    NEOPIXEL_STATUS_DATA_FLOWING,
    NEOPIXEL_STATUS_ERROR,
    NEOPIXEL_STATUS_DEBUG
} neopixel_status_t;

void neopixel_set_status(neopixel_status_t status);
void neopixel_update_blink(void); // Call periodically for blinking patterns

#endif // NEOPIXEL_H