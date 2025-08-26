#include "usb/usb.h"

#include <string.h>


#define USB_OUTPUT_QUEUE_SIZE 32

typedef struct {
    usb_output_event_t events[USB_OUTPUT_QUEUE_SIZE];
    volatile uint32_t head;
    volatile uint32_t tail;
    volatile uint32_t count;
} usb_output_queue_t;

static usb_output_queue_t queue;
static usb_output_event_callback_t output_callback;

void usb_output_init(void) {
    memset(&queue, 0, sizeof(usb_output_queue_t));
}

void usb_set_output_callback(usb_output_event_callback_t cb) {
    output_callback = cb;
}


bool usb_output_enqueue(const usb_output_event_t *event) {
    if (queue.count >= USB_OUTPUT_QUEUE_SIZE) {
        return false;
    }

    queue.events[queue.tail] = *event;
    queue.tail = (queue.tail + 1) % USB_OUTPUT_QUEUE_SIZE;
    queue.count++;

    return true;
}

bool usb_output_dequeue(usb_output_event_t *event) {
    if (queue.count == 0) {
        return false;
    }

    *event = queue.events[queue.head];
    queue.head = (queue.head + 1) % USB_OUTPUT_QUEUE_SIZE;
    queue.count--;

    if (output_callback != NULL) {
        output_callback(event);
    }

    return true;
}

uint32_t usb_output_count(void) {
    return queue.count;
}
