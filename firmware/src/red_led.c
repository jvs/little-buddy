#include "red_led.h"

#include <bsp/board_api.h>


static bool led_state = false;

void red_led_on() {
    led_state = true;
    board_led_write(true);
}

void red_led_off() {
    led_state = false;
    board_led_write(false);
}

bool red_led_check() {
    return led_state;
}
