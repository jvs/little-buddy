#include "neopixel.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/sync.h"

// NeoPixel configuration
#define NEOPIXEL_PIN 21  // GPIO 21 on Feather RP2040 with USB Type A Host
#define NEOPIXEL_FREQ 800000                  // 800 kHz
#define IS_RGBW false                         // Standard RGB NeoPixels

// Initialization state
static bool neopixel_initialized = false;

// Brightness control (0-255)
static uint8_t neopixel_brightness = 255;

// Blinking state for status patterns
static uint32_t last_blink_time = 0;
static bool blink_state = false;
static neopixel_status_t current_status = NEOPIXEL_STATUS_OFF;

// Software bit-bang implementation for WS2812
static void send_bit(bool bit) {
    if (bit) {
        // Send '1': ~0.8µs high, ~0.45µs low
        gpio_put(NEOPIXEL_PIN, 1);
        sleep_us(1);  // Approximately 0.8µs (will be slightly longer)
        gpio_put(NEOPIXEL_PIN, 0);
        sleep_us(1);  // Approximately 0.45µs (will be slightly longer)
    } else {
        // Send '0': ~0.4µs high, ~0.85µs low  
        gpio_put(NEOPIXEL_PIN, 1);
        sleep_us(1);  // Approximately 0.4µs (will be slightly longer)
        gpio_put(NEOPIXEL_PIN, 0);
        sleep_us(2);  // Approximately 0.85µs (will be slightly longer)
    }
}

static void send_byte(uint8_t byte) {
    for (int i = 7; i >= 0; i--) {
        send_bit(byte & (1 << i));
    }
}

static void send_reset(void) {
    gpio_put(NEOPIXEL_PIN, 0);
    sleep_us(50); // Reset time > 50µs
}

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

// Send pixel data to WS2812 using software bit-bang
static inline void put_pixel(uint32_t pixel_grb) {
    // Disable interrupts for precise timing
    uint32_t saved_interrupts = save_and_disable_interrupts();
    
    // Send GRB data (24 bits total)
    send_byte((pixel_grb >> 16) & 0xFF); // Green
    send_byte((pixel_grb >> 8) & 0xFF);  // Red  
    send_byte(pixel_grb & 0xFF);         // Blue
    
    // Send reset signal
    send_reset();
    
    // Restore interrupts
    restore_interrupts(saved_interrupts);
}

bool neopixel_init(void) {
    // Simple GPIO setup instead of PIO
    gpio_init(NEOPIXEL_PIN);
    gpio_set_dir(NEOPIXEL_PIN, GPIO_OUT);
    gpio_put(NEOPIXEL_PIN, 0);
    
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