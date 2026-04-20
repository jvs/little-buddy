#include "system/system.h"

#include "hardware/gpio.h"
#include "hardware/sync.h"
#include "hardware/watchdog.h"
#include "pico/bootrom.h"
#include "pico/time.h"


void system_bootsel(void) {
    reset_usb_boot(0, 0);
}


void system_reboot(void) {
    watchdog_reboot(0, 0, 0);
}


// WS2812 "0" bit at 125 MHz: ~0.35us high + ~0.9us low. NOP counts are
// approximate but well within the LED's tolerance for "0". GPIO 20 is the
// NeoPixel power switch (active high) — power up briefly to clock in the
// off-state, then cut power to be sure.
void system_disable_neopixel(void) {
    const uint power_pin = 20;
    const uint data_pin = 21;

    gpio_init(power_pin);
    gpio_set_dir(power_pin, GPIO_OUT);
    gpio_put(power_pin, 1);

    gpio_init(data_pin);
    gpio_set_dir(data_pin, GPIO_OUT);
    sio_hw->gpio_clr = 1u << data_pin;
    sleep_ms(2);  // Let the LED power up.

    uint32_t mask = 1u << data_pin;
    uint32_t save = save_and_disable_interrupts();

    for (int i = 0; i < 24; i++) {
        sio_hw->gpio_set = mask;
        __asm volatile (".rept 35\n nop\n .endr\n");
        sio_hw->gpio_clr = mask;
        __asm volatile (".rept 95\n nop\n .endr\n");
    }

    restore_interrupts(save);
    sleep_us(80);  // Latch the data.

    gpio_put(power_pin, 0);  // Cut power for good measure.
}
