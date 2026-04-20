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
// approximate but well within the LED's tolerance for "0".
void system_disable_neopixel(void) {
    const uint pin = 21;
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_OUT);
    sio_hw->gpio_clr = 1u << pin;
    sleep_us(80);  // Reset latch (>50us low)

    uint32_t mask = 1u << pin;
    uint32_t save = save_and_disable_interrupts();

    for (int i = 0; i < 24; i++) {
        sio_hw->gpio_set = mask;
        __asm volatile (".rept 35\n nop\n .endr\n");
        sio_hw->gpio_clr = mask;
        __asm volatile (".rept 95\n nop\n .endr\n");
    }

    restore_interrupts(save);
    sleep_us(80);
}
