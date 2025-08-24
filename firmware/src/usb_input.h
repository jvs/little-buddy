#ifndef USB_INPUT_H
#define USB_INPUT_H

void usb_input_init(void);
bool usb_input_enqueue(const usb_input_event_t *event);
bool usb_input_dequeue(usb_input_event_t *event);
uint32_t usb_input_count();

#endif // USB_INPUT_H
