#include "neopixel.h"
#include "ws2812.pio.h"
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"

// NeoPixel configuration
#define NEOPIXEL_PIN PICO_DEFAULT_WS2812_PIN  // GPIO 16 on Feather RP2040
#define NEOPIXEL_FREQ 800000                  // 800 kHz
#define IS_RGBW false                         // Standard RGB NeoPixels

// PIO state machine configuration
static PIO neopixel_pio = pio0;
static uint neopixel_sm = 0;
static uint neopixel_offset;
static bool neopixel_initialized = false;

// Brightness control (0-255)
static uint8_t neopixel_brightness = 255;

// Blinking state for status patterns
static uint32_t last_blink_time = 0;
static bool blink_state = false;
static neopixel_status_t current_status = NEOPIXEL_STATUS_OFF;

// Color conversion: RGB to GRB format for WS2812
static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b) {
    // Apply brightness scaling
    r = (r * neopixel_brightness) / 255;
    g = (g * neopixel_brightness) / 255;
    b = (b * neopixel_brightness) / 255;
    
    // WS2812 expects GRB format
    return ((uint32_t)(r) << 8) |
           ((uint32_t)(g) << 16) |
           (uint32_t)(b);
}

// Send pixel data to WS2812
static inline void put_pixel(uint32_t pixel_grb) {
    pio_sm_put_blocking(neopixel_pio, neopixel_sm, pixel_grb << 8u);
}

bool neopixel_init(void) {
    // Try to find an available state machine
    neopixel_sm = pio_claim_unused_sm(neopixel_pio, false);
    if (neopixel_sm == -1) {
        // Try PIO1 if PIO0 is full
        neopixel_pio = pio1;
        neopixel_sm = pio_claim_unused_sm(neopixel_pio, false);
        if (neopixel_sm == -1) {
            return false;
        }
    }
    
    // Check if we can add the program
    if (!pio_can_add_program(neopixel_pio, &ws2812_program)) {
        return false;
    }
    
    // Add the program to PIO
    neopixel_offset = pio_add_program(neopixel_pio, &ws2812_program);
    
    // Initialize the state machine
    ws2812_program_init(neopixel_pio, neopixel_sm, neopixel_offset, 
                       NEOPIXEL_PIN, NEOPIXEL_FREQ, IS_RGBW);
    
    neopixel_initialized = true;
    
    // Start with the pixel off
    neopixel_off();
    
    return true;
}

void neopixel_set_color(uint8_t r, uint8_t g, uint8_t b) {
    if (!neopixel_initialized) return;
    
    uint32_t pixel = urgb_u32(r, g, b);
    put_pixel(pixel);
}

void neopixel_off(void) {
    neopixel_set_color(0, 0, 0);
}

void neopixel_set_brightness(uint8_t brightness) {
    neopixel_brightness = brightness;
}

void neopixel_set_status(neopixel_status_t status) {
    current_status = status;
    
    // Set immediate color based on status
    switch (status) {
        case NEOPIXEL_STATUS_OFF:
            neopixel_off();
            break;
            
        case NEOPIXEL_STATUS_STARTING:
            neopixel_set_color(255, 255, 0); // Yellow
            break;
            
        case NEOPIXEL_STATUS_USB_READY:
            neopixel_set_color(0, 0, 255);   // Blue
            break;
            
        case NEOPIXEL_STATUS_DEVICE_DETECTED:
            neopixel_set_color(0, 255, 0);   // Green
            break;
            
        case NEOPIXEL_STATUS_DATA_FLOWING:
            neopixel_set_color(0, 255, 255); // Cyan
            break;
            
        case NEOPIXEL_STATUS_ERROR:
            neopixel_set_color(255, 0, 0);   // Red
            break;
            
        case NEOPIXEL_STATUS_DEBUG:
            neopixel_set_color(255, 0, 255); // Purple
            break;
    }
}

void neopixel_update_blink(void) {
    if (!neopixel_initialized) return;
    
    uint32_t now = time_us_32();
    uint32_t blink_interval = 0;
    
    // Determine blink pattern based on status
    switch (current_status) {
        case NEOPIXEL_STATUS_STARTING:
            blink_interval = 200000; // Fast blink (200ms)
            break;
            
        case NEOPIXEL_STATUS_USB_READY:
            blink_interval = 1000000; // Slow blink (1s)
            break;
            
        case NEOPIXEL_STATUS_ERROR:
            blink_interval = 100000; // Very fast blink (100ms)
            break;
            
        default:
            // No blinking for other states
            return;
    }
    
    // Handle blinking
    if (now - last_blink_time >= blink_interval) {
        last_blink_time = now;
        blink_state = !blink_state;
        
        if (blink_state) {
            neopixel_set_status(current_status); // Turn on with status color
        } else {
            neopixel_off(); // Turn off
        }
    }
}