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

// USB descriptor debug functions
extern uint32_t usb_get_device_desc_calls(void);
extern uint32_t usb_get_config_desc_calls(void);
extern uint32_t usb_get_hid_desc_calls(void);

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
            
            char line[20];  // Shorter lines for small display

            // Device mount status - this is the key diagnostic
            bool mounted = tud_mounted();
            snprintf(line, sizeof(line), "DEV: %s", mounted ? "MOUNT" : "NOMNT");
            sh1107_draw_string(&display, 0, 0, line);
            
            // HID readiness  
            bool hid0_ready = tud_hid_n_ready(0);
            snprintf(line, sizeof(line), "HID0: %s", hid0_ready ? "RDY" : "WAIT");
            sh1107_draw_string(&display, 0, 15, line);
            
            // Host events
            if (last_event.type == USB_EVENT_MOUSE) {
                snprintf(line, sizeof(line), "HOST: MSE");
            } else if (last_event.type == USB_EVENT_KEYBOARD) {
                snprintf(line, sizeof(line), "HOST: KBD");
            } else {
                snprintf(line, sizeof(line), "HOST: NONE");
            }
            sh1107_draw_string(&display, 0, 30, line);
            
            // USB descriptor callback counts - this will tell us what's happening
            uint32_t dev_calls = usb_get_device_desc_calls();
            uint32_t cfg_calls = usb_get_config_desc_calls();
            uint32_t hid_calls = usb_get_hid_desc_calls();
            uint32_t mnt_calls = usb_device_get_mount_calls();
            
            snprintf(line, sizeof(line), "D:%ld C:%ld H:%ld", dev_calls, cfg_calls, hid_calls);
            sh1107_draw_string(&display, 0, 45, line);
            
            snprintf(line, sizeof(line), "MNT:%ld", mnt_calls);
            sh1107_draw_string(&display, 0, 60, line);

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
