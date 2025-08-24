#include <bsp/board_api.h>
#include <hardware/i2c.h>
#include <hardware/gpio.h>
#include <pico/stdlib.h>
#include <stdbool.h>
#include <tusb.h>
#include <pio_usb.h>

#include "display_icons.h"
#include "event_processor.h"
#include "sh1107_display.h"
#include "usb_device.h"
#include "usb_host.h"
#include "usb_input.h"
#include "usb_output.h"


int main() {
    // Give everything time to power up properly.
    sleep_ms(2000);

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

    // Initialize USB event queues
    usb_input_init();
    usb_output_init();

    // Initialize USB device and host
    usb_device_init();
    usb_host_init();

    // Wait a bit for USB to initialize
    sleep_ms(100);

    // Show USB ready message
    if (display_ok) {
        sh1107_clear(&display);
        // display_icons_draw(&display, ICON_BOMB);
        sh1107_draw_string(&display, 1, 10, "READY!");
        sh1107_display(&display);
    }

    while (1) {
        usb_device_task();
        usb_host_task();
        event_processor_process();
    }

    return 0;
}
