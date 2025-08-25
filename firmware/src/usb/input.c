#include "usb/usb.h"

#include <string.h>


#define USB_INPUT_QUEUE_SIZE 32

typedef struct {
    usb_input_event_t events[USB_INPUT_QUEUE_SIZE];
    volatile uint32_t head;
    volatile uint32_t tail;
    volatile uint32_t count;
} usb_input_queue_t;

static usb_input_queue_t queue;

void usb_input_init(void) {
    memset(&queue, 0, sizeof(usb_input_queue_t));
}

bool usb_input_enqueue(const usb_input_event_t *event) {
    if (queue.count >= USB_INPUT_QUEUE_SIZE) {
        return false;
    }

    queue.events[queue.tail] = *event;
    queue.tail = (queue.tail + 1) % USB_INPUT_QUEUE_SIZE;
    queue.count++;

    return true;
}

bool usb_input_dequeue(usb_input_event_t *event) {
    if (queue.count == 0) {
        return false;
    }

    *event = queue.events[queue.head];
    queue.head = (queue.head + 1) % USB_INPUT_QUEUE_SIZE;
    queue.count--;

    return true;
}

uint32_t usb_input_count(void) {
    return queue.count;
}
