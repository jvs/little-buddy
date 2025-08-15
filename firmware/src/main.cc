#include <bsp/board_api.h>
#include <pico/time.h>
#include <hardware/i2c.h>
#include <hardware/gpio.h>
#include <stdio.h>

#include "activity_led.h"
#include "sh1107_display.h"
#include "usb_host.h"
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

    // Initialize USB host
    usb_host_init();

    // Show startup message
    if (display_ok) {
        sh1107_clear(&display);
        sh1107_draw_string(&display, 10, 10, "LITTLE BUDDY");
        sh1107_draw_string(&display, 10, 25, "USB HOST READY");
        sh1107_display(&display);
    }

    usb_event_t last_event = {};
    uint32_t last_display_update = 0;

    while (true) {
        // Service USB host
        usb_host_task();

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

            char line[21];  // 20 chars + null terminator

            // Show USB report count for debugging
            uint32_t report_count = usb_host_get_report_count();
            snprintf(line, sizeof(line), "REPORTS: %lu", report_count);
            sh1107_draw_string(&display, 0, 60, line);

            if (last_event.type == USB_EVENT_NONE) {
                snprintf(line, sizeof(line), "NO USB EVENTS");
                sh1107_draw_string(&display, 0, 15, line);
            } else {
                snprintf(line, sizeof(line), "LAST: %lu MS AGO", now - last_event.timestamp_ms);
                sh1107_draw_string(&display, 0, 15, line);

                switch (last_event.type) {
                    case USB_EVENT_MOUSE:
                        snprintf(line, sizeof(line), "MOUSE: DX=%d DY=%d",
                                last_event.data.mouse.delta_x, last_event.data.mouse.delta_y);
                        sh1107_draw_string(&display, 0, 30, line);
                        snprintf(line, sizeof(line), "BTNS: 0x%02X", last_event.data.mouse.buttons);
                        sh1107_draw_string(&display, 0, 45, line);
                        break;

                    case USB_EVENT_KEYBOARD:
                        snprintf(line, sizeof(line), "KBD: KEY=%02X", last_event.data.keyboard.keycode);
                        sh1107_draw_string(&display, 0, 30, line);
                        snprintf(line, sizeof(line), "MOD: %02X", last_event.data.keyboard.modifier);
                        sh1107_draw_string(&display, 0, 45, line);
                        break;

                    case USB_EVENT_DEVICE_CONNECTED:
                        snprintf(line, sizeof(line), "CONNECTED:");
                        sh1107_draw_string(&display, 0, 30, line);
                        snprintf(line, sizeof(line), "%s", last_event.data.device.device_type);
                        sh1107_draw_string(&display, 0, 45, line);
                        break;

                    case USB_EVENT_DEVICE_DISCONNECTED:
                        snprintf(line, sizeof(line), "DISCONNECTED");
                        sh1107_draw_string(&display, 0, 30, line);
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
