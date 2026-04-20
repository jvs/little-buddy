#include "system/system.h"

#include "hardware/watchdog.h"
#include "pico/bootrom.h"


void system_bootsel(void) {
    reset_usb_boot(0, 0);
}


void system_reboot(void) {
    watchdog_reboot(0, 0, 0);
}
