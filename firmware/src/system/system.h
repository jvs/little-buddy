#ifndef SYSTEM_H
#define SYSTEM_H

// Enter the RP2040 USB bootloader (device appears as a mass-storage drive).
void system_bootsel(void);

// Software reset via the watchdog.
void system_reboot(void);

#endif
