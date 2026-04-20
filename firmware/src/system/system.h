#ifndef SYSTEM_H
#define SYSTEM_H

// Enter the RP2040 USB bootloader (device appears as a mass-storage drive).
void system_bootsel(void);

// Software reset via the watchdog.
void system_reboot(void);

// Send all-zeros to the on-board WS2812 NeoPixel (GPIO 21) to turn it off.
void system_disable_neopixel(void);

#endif
