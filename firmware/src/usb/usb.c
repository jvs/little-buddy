#include "usb/usb.h"

// device.c
void usb_device_init(void);
void usb_device_task(void);

// host.c
void usb_host_init(void);
void usb_host_task(void);

// input.c
void usb_input_init(void);

// output.c
void usb_output_init(void);


void usb_init(void) {
    usb_input_init();
    usb_output_init();
    usb_device_init();
    usb_host_init();
}


void usb_task(void) {
    usb_device_task();
    usb_host_task();
}
