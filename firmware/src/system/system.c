#include "system/system.h"

#include "hardware/gpio.h"
#include "hardware/sync.h"
#include "hardware/watchdog.h"
#include "pico/bootrom.h"
#include "pico/platform.h"
#include "pico/time.h"


void system_bootsel(void) {
    reset_usb_boot(0, 0);
}


void system_reboot(void) {
    watchdog_reboot(0, 0, 0);
}


// WS2812 "0" bit at 125 MHz: ~0.35us high + ~0.9us low. NOP counts are
// approximate but well within the LED's tolerance for "0". Runs from RAM
// (not_in_flash) for deterministic timing; XIP stalls would stretch the
// high pulse and make "0"s look like "1"s (bright white glow). noinline
// keeps the caller's loop body in range of M0+ conditional branches.
static void __not_in_flash_func(ws2812_zero_bit)(uint32_t mask) __attribute__((noinline));
static void __not_in_flash_func(ws2812_zero_bit)(uint32_t mask) {
    sio_hw->gpio_set = mask;
    __asm volatile (".rept 35\n nop\n .endr\n");
    sio_hw->gpio_clr = mask;
    __asm volatile (".rept 95\n nop\n .endr\n");
}

// Try to kill the NeoPixel by every mechanism we can think of: clock in
// all-zeros on the documented data pin (21), then drive every nearby
// reserved pin low on the theory that one of them is the actual power
// switch on this board. Skips USB host pins (16, 18) and I2C pins (2, 3).
void __not_in_flash_func(system_disable_neopixel)(void) {
    const uint data_pin = 21;

    // Documented power pin (20): hold high while clocking in zeros.
    gpio_init(20);
    gpio_set_dir(20, GPIO_OUT);
    gpio_put(20, 1);

    gpio_init(data_pin);
    gpio_set_dir(data_pin, GPIO_OUT);
    sio_hw->gpio_clr = 1u << data_pin;
    sleep_ms(2);  // Let the LED power up.

    uint32_t mask = 1u << data_pin;
    uint32_t save = save_and_disable_interrupts();

    for (int i = 0; i < 24; i++) {
        ws2812_zero_bit(mask);
    }

    restore_interrupts(save);
    sleep_us(80);  // Latch the data.

    // Now drive every candidate pin low. If any of these is the real
    // NEOPIXEL_POWER (active high), the LED loses power regardless.
    const uint kill_pins[] = {17, 19, 20, 21, 22, 23};
    for (uint i = 0; i < sizeof(kill_pins) / sizeof(kill_pins[0]); i++) {
        gpio_init(kill_pins[i]);
        gpio_set_dir(kill_pins[i], GPIO_OUT);
        gpio_put(kill_pins[i], 0);
    }
}
