#ifndef USB_OUTPUT_H
#define USB_OUTPUT_H

#include "usb_event_types.h"

void usb_output_init(void);
bool usb_output_enqueue(const usb_output_event_t *event);
bool usb_output_dequeue(usb_output_event_t *event);
uint32_t usb_output_count(void);

#endif
