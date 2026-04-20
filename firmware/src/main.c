#include <bsp/board_api.h>
#include <hardware/i2c.h>
#include <hardware/gpio.h>
#include <pico/stdlib.h>
#include <stdbool.h>
#include <tusb.h>
#include <pio_usb.h>

#include "display/display.h"
#include "engine/engine.h"
#include "usb/usb.h"


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
    bool display_ok = display_init(i2c1);

    // Show initial startup message
    if (display_ok) {
        display_clear_buffer();
        display_draw_string(1, 10, "STARTING...");
        display_show_buffer();
    }

    sleep_ms(500);

    usb_init();
    engine_init();

    sleep_ms(100);

    // Show USB ready message
    if (display_ok) {
        display_clear_buffer();
        display_draw_string(1, 10, "READY...");
        display_show_buffer();
    }

    while (1) {
        usb_task();
        engine_task();
    }

    return 0;
}
