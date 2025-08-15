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

    // Initialize USB host and device
    usb_host_init();
    usb_device_init();

    // Show startup message
    if (display_ok) {
        sh1107_clear(&display);
        sh1107_draw_string(&display, 10, 10, "LITTLE BUDDY");
        sh1107_draw_string(&display, 10, 25, "USB HOST READY");
        sh1107_display(&display);
    }

    usb_event_t last_event = {};
    uint32_t last_display_update = 0;

    debug_printf("Little Buddy starting up...\n");
    
    while (true) {
        // Service USB host and device
        usb_host_task();
        usb_device_task();

        // Process USB events
        usb_event_queue_t* queue = usb_host_get_event_queue();
        usb_event_t event;

        while (usb_event_queue_pop(queue, &event)) {
            last_event = event;
            activity_led_on();
            sleep_ms(50);  // Brief flash
            activity_led_off();
        }

        // Update display every 100ms or when there's a new event
        uint32_t now = to_ms_since_boot(get_absolute_time());
        if (display_ok && (now - last_display_update > 100)) {
            sh1107_clear(&display);
            sh1107_draw_string(&display, 0, 0, "LITTLE BUDDY");

            char line[32];  // Larger buffer to avoid truncation

            // Show USB report and mount counts for debugging
            uint32_t report_count = usb_host_get_report_count();
            uint32_t mount_count = usb_host_get_mount_count();
            snprintf(line, sizeof(line), "REPORTS: %lu", report_count);
            sh1107_draw_string(&display, 0, 60, line);
            snprintf(line, sizeof(line), "MOUNTS: %lu", mount_count);
            sh1107_draw_string(&display, 0, 75, line);

            // Show interface information
            snprintf(line, sizeof(line), "IF0: %s", usb_host_get_interface_info(0));
            sh1107_draw_string(&display, 0, 15, line);
            snprintf(line, sizeof(line), "IF1: %s", usb_host_get_interface_info(1));
            sh1107_draw_string(&display, 0, 30, line);
            
            // Show last event with interface info
            if (last_event.type != USB_EVENT_NONE) {
                switch (last_event.type) {
                    case USB_EVENT_MOUSE:
                        snprintf(line, sizeof(line), "IF%d MOUSE DX=%d DY=%d",
                                last_event.interface_id, last_event.data.mouse.delta_x, last_event.data.mouse.delta_y);
                        sh1107_draw_string(&display, 0, 45, line);
                        break;

                    case USB_EVENT_KEYBOARD:
                        snprintf(line, sizeof(line), "IF%d KEY=%02X MOD=%02X", 
                                last_event.interface_id, last_event.data.keyboard.keycode, last_event.data.keyboard.modifier);
                        sh1107_draw_string(&display, 0, 45, line);
                        break;

                    default:
                        break;
                }
            }

            sh1107_display(&display);
            last_display_update = now;
        }

        sleep_ms(10);  // Small delay to prevent busy waiting
    }

    return 0;
}
