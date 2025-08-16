#ifndef USB_HOST_H
#define USB_HOST_H

#include "usb_events.h"

typedef struct {
    uint8_t data[16];
    uint16_t length;
    uint32_t timestamp_ms;
} last_report_t;

void usb_host_init(usb_event_queue_t* queue);
void usb_host_task(void);
usb_event_queue_t* usb_host_get_event_queue(void);
uint32_t usb_host_get_report_count(void);
uint32_t usb_host_get_mount_count(void);
const char* usb_host_get_interface_info(uint8_t instance);
const last_report_t* usb_host_get_last_report(uint8_t instance);

#endif