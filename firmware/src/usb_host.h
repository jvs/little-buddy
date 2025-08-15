#ifndef USB_HOST_H
#define USB_HOST_H

#include "usb_events.h"

void usb_host_init(void);
void usb_host_task(void);
usb_event_queue_t* usb_host_get_event_queue(void);
uint32_t usb_host_get_report_count(void);
uint32_t usb_host_get_mount_count(void);

#endif