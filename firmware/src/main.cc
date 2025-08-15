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

    // Initialize USB (both host and device)
    usb_host_init();  // Just init event queue
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
            
            // Show version and current time (minutes since boot)
            char header[32];
            uint32_t minutes = now / 60000;
            snprintf(header, sizeof(header), "LB v1.0 T+%lum", minutes);
            sh1107_draw_string(&display, 0, 0, header);

            char line[32];  // Larger buffer to avoid truncation

            // Show USB report and mount counts for debugging
            uint32_t report_count = usb_host_get_report_count();
            uint32_t mount_count = usb_host_get_mount_count();
            snprintf(line, sizeof(line), "RPT:%lu MNT:%lu", report_count, mount_count);
            sh1107_draw_string(&display, 0, 15, line);

            // Show interface information and raw reports side by side
            const last_report_t* report0 = usb_host_get_last_report(0);
            const last_report_t* report1 = usb_host_get_last_report(1);
            
            // Interface 0 header
            snprintf(line, sizeof(line), "IF0:%s", usb_host_get_interface_info(0));
            sh1107_draw_string(&display, 0, 30, line);
            
            // Interface 0 raw data (first 4 bytes)
            if (report0 && report0->length > 0) {
                snprintf(line, sizeof(line), "%02X%02X%02X%02X L%d", 
                        report0->data[0], 
                        report0->length > 1 ? report0->data[1] : 0,
                        report0->length > 2 ? report0->data[2] : 0,
                        report0->length > 3 ? report0->data[3] : 0,
                        report0->length);
                sh1107_draw_string(&display, 0, 45, line);
            } else {
                sh1107_draw_string(&display, 0, 45, "---- L0");
            }
            
            // Interface 1 header  
            snprintf(line, sizeof(line), "IF1:%s", usb_host_get_interface_info(1));
            sh1107_draw_string(&display, 0, 60, line);
            
            // Interface 1 raw data (first 4 bytes)
            if (report1 && report1->length > 0) {
                snprintf(line, sizeof(line), "%02X%02X%02X%02X L%d", 
                        report1->data[0], 
                        report1->length > 1 ? report1->data[1] : 0,
                        report1->length > 2 ? report1->data[2] : 0,
                        report1->length > 3 ? report1->data[3] : 0,
                        report1->length);
                sh1107_draw_string(&display, 0, 75, line);
            } else {
                sh1107_draw_string(&display, 0, 75, "---- L0");
            }
            
            // Show last parsed event on a separate line
            if (last_event.type != USB_EVENT_NONE) {
                switch (last_event.type) {
                    case USB_EVENT_MOUSE:
                        snprintf(line, sizeof(line), "MSE: DX=%d DY=%d B=%d",
                                last_event.data.mouse.delta_x, last_event.data.mouse.delta_y, 
                                last_event.data.mouse.buttons);
                        sh1107_draw_string(&display, 0, 90, line);
                        break;

                    case USB_EVENT_KEYBOARD:
                        snprintf(line, sizeof(line), "KBD: K=%02X M=%02X", 
                                last_event.data.keyboard.keycode, last_event.data.keyboard.modifier);
                        sh1107_draw_string(&display, 0, 90, line);
                        break;

                    default:
                        break;
                }
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
