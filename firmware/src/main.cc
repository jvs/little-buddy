#include <bsp/board_api.h>
#include <pico/time.h>

#include "activity_led.h"


int main() {
    board_init();

    while (true) {
        activity_led_on();
        sleep_ms(500);
        activity_led_off_maybe();
    }

    return 0;
}
