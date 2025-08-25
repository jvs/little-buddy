#ifndef USB_H
#define USB_H

#include "usb/types.h"

// input.c
bool usb_input_dequeue(usb_input_event_t *event);
bool usb_input_enqueue(const usb_input_event_t *event);
uint32_t usb_input_count();

// output.c
bool usb_output_dequeue(usb_output_event_t *event);
bool usb_output_enqueue(const usb_output_event_t *event);
uint32_t usb_output_count(void);

// usb.c
void usb_init(void);
void usb_task(void);

#endif
