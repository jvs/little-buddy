#include <bsp/board_api.h>
#include <hardware/i2c.h>
#include <hardware/gpio.h>
#include <pico/stdlib.h>
#include <stdbool.h>
#include <tusb.h>
#include <pio_usb.h>

#include "sh1107_display.h"
#include "usb_events.h"
#include "usb_host.h"
#include "usb_device.h"

// Global USB event queue
static usb_event_queue_t usb_event_queue;
static uint32_t event_sequence_counter = 0;

// Non-blocking keyboard test state
uint32_t keyboard_test_deadline = 0;
bool keyboard_test_pending = false;

// Global variables for display updates
char last_event[32] = "None";
uint8_t last_key = 0;

void neopixel_force_gpio_mode() {
    // Don't touch PIO state machines at all
    // Just force the GPIO pins to SIO mode

    // This removes the pins from PIO control without
    // affecting the PIO state machines
    gpio_set_function(20, GPIO_FUNC_SIO);
    gpio_set_function(21, GPIO_FUNC_SIO);

    // Now set them as outputs
    gpio_init(20);
    gpio_init(21);
    gpio_set_dir(20, GPIO_OUT);
    gpio_set_dir(21, GPIO_OUT);

    // Turn off NeoPixel
    gpio_put(20, 0);  // Power
    gpio_put(21, 0);  // Data

    // Make sure they stay low
    gpio_set_slew_rate(20, GPIO_SLEW_RATE_SLOW);
    gpio_set_slew_rate(21, GPIO_SLEW_RATE_SLOW);
    gpio_set_drive_strength(20, GPIO_DRIVE_STRENGTH_2MA);
    gpio_set_drive_strength(21, GPIO_DRIVE_STRENGTH_2MA);
}

int main() {
    // Give everything time to power up properly.
    sleep_ms(2000);

    // Turn off NeoPixel before anything else.
    neopixel_force_gpio_mode();

    // Initialize board
    board_init();

    // Initialize I2C for STEMMA QT connector (GPIO 2=SDA, 3=SCL)
    sleep_ms(500);
    i2c_init(i2c1, 400000);
    gpio_set_function(2, GPIO_FUNC_I2C);
    gpio_set_function(3, GPIO_FUNC_I2C);
    gpio_pull_up(2);
    gpio_pull_up(3);

    // Initialize display (optional - may not be present)
    sh1107_t display;
    bool display_ok = sh1107_init(&display, i2c1);

    // Show initial startup message
    if (display_ok) {
        sh1107_clear(&display);
        sh1107_draw_string(&display, 1, 10, "STARTING...");
        sh1107_display(&display);
        sleep_ms(500);
    }

    // Initialize USB event queue
    usb_event_queue_init(&usb_event_queue);

    // Initialize Boot button (GPIO 23 on RP2040)
    gpio_init(23);
    gpio_set_dir(23, GPIO_IN);
    gpio_pull_up(23);  // Boot button pulls to ground when pressed

    // Initialize USB device and host
    usb_device_init();
    usb_host_init();
    usb_host_set_event_queue(&usb_event_queue, &event_sequence_counter);

    // Wait a bit for USB to initialize
    sleep_ms(100);

    // Show USB ready message
    if (display_ok) {
        sh1107_clear(&display);
        sh1107_draw_string(&display, 1, 10, "USB DUAL MODE");
        sh1107_draw_string(&display, 1, 25, "DEVICE + HOST");
        sh1107_display(&display);
    }

    static uint32_t byte_count = 0;
    char status_buf[32];

    while (1) {
        // USB tasks
        usb_device_task();
        usb_host_task();
        
        // CDC task for handling serial communication
        byte_count += cdc_task();

        // Check for pending keyboard test
        if (keyboard_test_pending && time_us_32() >= keyboard_test_deadline) {
            tud_cdc_write_str("PRESSING 'B'...\r\n");
            tud_cdc_write_flush();
            send_keyboard_report(0, HID_KEY_B);  // Press 'b'
            sleep_ms(100);  // Longer press duration
            tud_cdc_write_str("RELEASING 'B'...\r\n");
            tud_cdc_write_flush();
            send_keyboard_report(0, 0);          // Release 'b'
            sleep_ms(100);  // Ensure release is processed
            tud_cdc_write_str("KEY 'B' COMPLETE\r\n");
            tud_cdc_write_flush();
            keyboard_test_pending = false;
        }

        // Check Boot button (GPIO 23, active low)
        static bool boot_button_last = true;
        bool boot_button_now = gpio_get(23);
        if (boot_button_last && !boot_button_now) {  // Button press (falling edge)
            tud_cdc_write_str("BOOT BUTTON PRESSED!\r\n");
            tud_cdc_write_flush();
        }
        boot_button_last = boot_button_now;

        // Update display periodically
        static uint32_t last_display_update = 0;
        if (time_us_32() - last_display_update > 500000) { // Update every 500ms
            last_display_update = time_us_32();

            if (display_ok) {
                sh1107_clear(&display);

                // Count connected devices
                int device_count = 0;
                int kbd_count = 0;
                int mouse_count = 0;
                for (int i = 0; i < MAX_HID_DEVICES; i++) {
                    if (hid_devices[i].is_connected) {
                        device_count++;
                        if (hid_devices[i].has_keyboard) kbd_count++;
                        if (hid_devices[i].has_mouse) mouse_count++;
                    }
                }

                // Line 1: Device counts
                snprintf(status_buf, sizeof(status_buf), "HID: %d (K:%d M:%d)", device_count, kbd_count, mouse_count);
                sh1107_draw_string(&display, 1, 10, status_buf);

                // Line 2: Last event
                sh1107_draw_string(&display, 1, 25, last_event);

                // Line 3: CDC bytes for testing
                snprintf(status_buf, sizeof(status_buf), "CDC: %lu B", byte_count);
                sh1107_draw_string(&display, 1, 40, status_buf);

                sh1107_display(&display);
            }
        }
    }

    return 0;
}