#include <bsp/board_api.h>
#include <pico/time.h>

#include "activity_led.h"


int main() {
    board_init();

    while (true) {
        activity_led_on();
        sleep_ms(60000);
        activity_led_off_maybe();
        sleep_ms(10000);
    }

    return 0;
}
