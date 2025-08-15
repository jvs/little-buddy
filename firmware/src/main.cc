#include <bsp/board_api.h>
#include <pico/time.h>

#include "activity_led.h"


int main() {
    board_init();

    while (true) {
        activity_led_on();
        sleep_ms(2 * 1000);
        activity_led_off();
        sleep_ms(1 * 1000);
    }

    return 0;
}
