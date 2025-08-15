#ifndef USB_DEVICE_H
#define USB_DEVICE_H

void usb_device_init(void);
void usb_device_task(void);
int debug_printf(const char* format, ...);

#endif