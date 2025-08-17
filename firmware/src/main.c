#include <bsp/board_api.h>
#include <hardware/i2c.h>
#include <hardware/gpio.h>
#include <stdbool.h>

#include "sh1107_display.h"


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


    // Show startup message
    if (display_ok) {
        sh1107_clear(&display);
        sh1107_draw_string(&display, 10, 10, "HELLO, WORLD");
        sh1107_display(&display);
    }

    return 0;
}
