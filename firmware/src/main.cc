#include <bsp/board_api.h>
#include <pico/time.h>
#include <hardware/i2c.h>
#include <hardware/gpio.h>

#include "activity_led.h"
#include "sh1107_display.h"


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
    
    // Show startup message
    if (display_ok) {
        sh1107_clear(&display);
        sh1107_draw_string(&display, 10, 10, "Little Buddy");
        sh1107_draw_string(&display, 10, 25, "Starting...");
        sh1107_display(&display);
    }

    int counter = 0;
    while (true) {
        activity_led_on();
        
        // Update display with counter if working
        if (display_ok) {
            sh1107_clear(&display);
            sh1107_draw_string(&display, 10, 10, "Little Buddy");
            
            char count_str[16];
            snprintf(count_str, sizeof(count_str), "Count: %d", counter);
            sh1107_draw_string(&display, 10, 30, count_str);
            
            sh1107_display(&display);
            counter++;
        }
        
        sleep_ms(2 * 1000);
        activity_led_off();
        sleep_ms(1 * 1000);
    }

    return 0;
}
