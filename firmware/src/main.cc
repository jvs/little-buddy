#include <bsp/board_api.h>
#include <pico/time.h>
#include <hardware/i2c.h>
#include <hardware/gpio.h>
#include <stdio.h>

#include "activity_led.h"
#include "sh1107_display.h"
#include "usb_host.h"
#include "usb_device.h"
#include "usb_events.h"


int main() {
    board_init();

    // Initialize I2C for STEMMA QT connector (GPIO 2=SDA, 3=SCL)
    i2c_init(i2c1, 400000);
    gpio_set_function(2, GPIO_FUNC_I2C);
    gpio_set_function(3, GPIO_FUNC_I2C);
    gpio_pull_up(2);
    gpio_pull_up(3);

    // Initialize display
    sh1107_t display;
    bool display_ok = sh1107_init(&display, i2c1);

    // Initialize USB (device only for testing)
    // usb_host_init();  // Temporarily disabled
    usb_device_init(); // Just setup
    tusb_init();       // Unified TinyUSB init

    // Show startup message
    if (display_ok) {
        sh1107_clear(&display);
        sh1107_draw_string(&display, 10, 10, "LB v1.0");
        sh1107_draw_string(&display, 10, 25, "USB HOST READY");
        sh1107_display(&display);
    }

    usb_event_t last_event = {};
    uint32_t last_display_update = 0;

    debug_printf("Little Buddy starting up...\n");
    
    uint32_t last_heartbeat = 0;
    
    while (true) {
        // Service USB device only (host disabled for testing)
        // usb_host_task();  // Temporarily disabled
        usb_device_task();

        // Process USB events (disabled while testing device-only mode)
        /*
        usb_event_queue_t* queue = usb_host_get_event_queue();
        usb_event_t event;

        while (usb_event_queue_pop(queue, &event)) {
            last_event = event;
            activity_led_on();
            
            // Forward events to USB device (computer)
            switch (event.type) {
                case USB_EVENT_KEYBOARD:
                    usb_device_send_keyboard_report(event.data.keyboard.modifier, event.data.keyboard.keycode);
                    debug_printf("FWD KBD: K=%02X M=%02X\n", event.data.keyboard.keycode, event.data.keyboard.modifier);
                    break;
                    
                case USB_EVENT_MOUSE:
                    usb_device_send_mouse_report(event.data.mouse.buttons, event.data.mouse.delta_x, event.data.mouse.delta_y, event.data.mouse.scroll);
                    debug_printf("FWD MSE: B=%02X DX=%d DY=%d\n", event.data.mouse.buttons, event.data.mouse.delta_x, event.data.mouse.delta_y);
                    break;
                    
                default:
                    break;
            }
            
            sleep_ms(50);  // Brief flash
            activity_led_off();
        }
        */

        // Update display every 100ms or when there's a new event
        uint32_t now = to_ms_since_boot(get_absolute_time());
        if (display_ok && (now - last_display_update > 100)) {
            sh1107_clear(&display);
            
            // Show version and current time (minutes since boot)
            char header[32];
            uint32_t minutes = now / 60000;
            snprintf(header, sizeof(header), "LB v1.0 T+%lum", minutes);
            sh1107_draw_string(&display, 0, 0, header);

            char line[32];  // Larger buffer to avoid truncation

            // Show USB device status (simplified for device-only testing)
            sh1107_draw_string(&display, 0, 15, "DEVICE MODE TEST");
            
            // Show device mount status
            snprintf(line, sizeof(line), "MOUNTED: %s", tud_mounted() ? "YES" : "NO");
            sh1107_draw_string(&display, 0, 30, line);
            
            // Show CDC connection status  
            snprintf(line, sizeof(line), "CDC: %s", tud_cdc_connected() ? "CONN" : "DISC");
            sh1107_draw_string(&display, 0, 45, line);
            
            // Show HID interface readiness
            bool kbd_ready = tud_hid_n_ready(2);
            bool mouse_ready = tud_hid_n_ready(3);
            snprintf(line, sizeof(line), "KBD:%s MSE:%s", kbd_ready ? "RDY" : "NO", mouse_ready ? "RDY" : "NO");
            sh1107_draw_string(&display, 0, 60, line);
            
            // Test: Send dummy reports every few seconds
            static uint32_t last_test = 0;
            if (now - last_test > 3000 && kbd_ready) {  // Every 3 seconds
                usb_device_send_keyboard_report(0, 0x04);  // Send 'a' key
                debug_printf("TEST: Sent keyboard 'a'\n");
                last_test = now;
            }

            sh1107_display(&display);
            last_display_update = now;
        }

        // Heartbeat debug every 5 seconds
        uint32_t now_ms = to_ms_since_boot(get_absolute_time());
        if (now_ms - last_heartbeat > 5000) {
            debug_printf("Heartbeat: device connected=%d\n", tud_cdc_connected());
            last_heartbeat = now_ms;
        }

        sleep_ms(10);  // Small delay to prevent busy waiting
    }

    return 0;
}
