#include "neopixel.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"

// Stub implementation - NeoPixel disabled for now
bool neopixel_init(void) {
    return false; // Always return false = not initialized
}

void neopixel_set_color(uint8_t r, uint8_t g, uint8_t b) {
    // Do nothing
}

void neopixel_off(void) {
    // Do nothing
}

void neopixel_set_brightness(uint8_t brightness) {
    // Do nothing
}

void neopixel_set_status(neopixel_status_t status) {
    // Do nothing
}

void neopixel_update_blink(void) {
    // Do nothing
}