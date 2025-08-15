#include "activity_led.h"

#include <bsp/board_api.h>


static bool led_state = false;

void activity_led_on() {
    led_state = true;
    board_led_write(true);
}

void activity_led_off_maybe() {
    led_state = false;
    board_led_write(false);
}
