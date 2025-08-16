#include <bsp/board_api.h>
#include <pico/time.h>
#include <hardware/i2c.h>
#include <hardware/gpio.h>
#include <stdio.h>
#include <tusb.h>
#include <pio_usb.h>
#include <pico/time.h>

#include "activity_led.h"
#include "sh1107_display.h"
#include "usb_host.h"
#include "usb_device.h"
#include "usb_events.h"

static repeating_timer_t sof_timer;

// Manual SOF timer for PIO USB - critical for host operation
static bool __no_inline_not_in_flash_func(manual_sof)(repeating_timer_t* rt) {
    pio_usb_host_frame();
    return true;
}

void configure_pio_usb() {
    // Configure PIO USB for host mode - matching hid-remapper
    pio_usb_configuration_t pio_cfg = PIO_USB_DEFAULT_CONFIG;
    pio_cfg.pin_dp = PICO_DEFAULT_PIO_USB_DP_PIN;
    pio_cfg.skip_alarm_pool = true;
    tuh_configure(BOARD_TUH_RHPORT, TUH_CFGID_RPI_PIO_USB_CONFIGURATION, &pio_cfg);
    
    // Add manual SOF timer - essential for PIO USB host operation
    add_repeating_timer_us(-1000, manual_sof, NULL, &sof_timer);
}

int main() {
    // Initialize I2C for STEMMA QT connector (GPIO 2=SDA, 3=SCL)
    i2c_init(i2c1, 400000);
    gpio_set_function(2, GPIO_FUNC_I2C);
    gpio_set_function(3, GPIO_FUNC_I2C);
    gpio_pull_up(2);
    gpio_pull_up(3);

    // Initialize display
    sh1107_t display;
    bool display_ok = sh1107_init(&display, i2c1);

    // Initialize USB host/device setup
    usb_host_init();
    usb_device_init();
    
    // Match hid-remapper sequence exactly: board_init() THEN PIO USB THEN tusb_init()
    board_init();
    configure_pio_usb();  // This matches hid-remapper's extra_init() placement
    tusb_init();

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
    uint32_t last_test_movement = 0;
    
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

            // USB DEVICE DEBUG INFO
            
            // Device status
            bool mounted = tud_mounted();
            bool suspended = tud_suspended();
            snprintf(line, sizeof(line), "DEV: %s %s", 
                    mounted ? "MOUNT" : "NOMNT",
                    suspended ? "SUSP" : "ACTV");
            sh1107_draw_string(&display, 0, 15, line);
            
            // HID interface status  
            bool hid0_ready = tud_hid_n_ready(0);
            bool hid1_ready = tud_hid_n_ready(1);
            snprintf(line, sizeof(line), "HID: IF0=%s IF1=%s", 
                    hid0_ready ? "RDY" : "WAIT",
                    hid1_ready ? "RDY" : "WAIT");
            sh1107_draw_string(&display, 0, 30, line);
            
            // Combined forwarding status
            snprintf(line, sizeof(line), "FWD: %s", hid0_ready ? "READY" : "WAIT");
            sh1107_draw_string(&display, 0, 45, line);
            
            // Host event summary (one line)
            if (last_event.type == USB_EVENT_MOUSE) {
                snprintf(line, sizeof(line), "HOST: MSE DX=%d DY=%d B=%d",
                        last_event.data.mouse.delta_x, last_event.data.mouse.delta_y, 
                        last_event.data.mouse.buttons);
            } else if (last_event.type == USB_EVENT_KEYBOARD) {
                snprintf(line, sizeof(line), "HOST: KBD K=%02X M=%02X", 
                        last_event.data.keyboard.keycode, last_event.data.keyboard.modifier);
            } else {
                snprintf(line, sizeof(line), "HOST: No events");
            }
            sh1107_draw_string(&display, 0, 60, line);
            
            // Test status
            uint32_t test_time_left = 3000 - (now_ms - last_test_movement);
            if (test_time_left > 3000) test_time_left = 0; // Handle wraparound
            snprintf(line, sizeof(line), "TEST: %lums to next move", test_time_left);
            sh1107_draw_string(&display, 0, 75, line);

            sh1107_display(&display);
            last_display_update = now;
        }

        // Test mouse movement every 3 seconds (only if device is mounted)
        uint32_t now_ms = to_ms_since_boot(get_absolute_time());
        if (tud_mounted() && (now_ms - last_test_movement > 3000)) {
            usb_device_send_test_mouse_movement();
            last_test_movement = now_ms;
        }

        // Heartbeat debug every 5 seconds
        if (now_ms - last_heartbeat > 5000) {
            debug_printf("Heartbeat: device mounted=%d\n", tud_mounted());
            last_heartbeat = now_ms;
        }

        sleep_ms(10);  // Small delay to prevent busy waiting
    }

    return 0;
}
