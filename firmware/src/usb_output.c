#include "usb_event_types.h"

#include <string.h>
#include "usb_output.h"

#define USB_OUTPUT_QUEUE_SIZE 32

typedef struct {
    usb_output_event_t events[USB_OUTPUT_QUEUE_SIZE];
    volatile uint32_t head;
    volatile uint32_t tail;
    volatile uint32_t count;
} usb_output_queue_t;

static usb_output_queue_t queue;

void usb_output_init(void) {
    memset(&queue, 0, sizeof(usb_output_queue_t));
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

    return true;
}

uint32_t usb_output_count(void) {
    return queue.count;
}
