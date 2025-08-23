#include "usb_events.h"
#include <string.h>

void usb_event_queue_init(usb_event_queue_t* queue) {
    memset(queue, 0, sizeof(usb_event_queue_t));
}

bool usb_event_queue_enqueue(usb_event_queue_t* queue, const usb_event_t* event) {
    if (queue->count >= USB_EVENT_QUEUE_SIZE) {
        return false;
    }
    
    queue->events[queue->tail] = *event;
    queue->tail = (queue->tail + 1) % USB_EVENT_QUEUE_SIZE;
    queue->count++;
    
    return true;
}

bool usb_event_queue_dequeue(usb_event_queue_t* queue, usb_event_t* event) {
    if (queue->count == 0) {
        return false;
    }
    
    *event = queue->events[queue->head];
    queue->head = (queue->head + 1) % USB_EVENT_QUEUE_SIZE;
    queue->count--;
    
    return true;
}

bool usb_event_queue_is_empty(const usb_event_queue_t* queue) {
    return queue->count == 0;
}

uint32_t usb_event_queue_count(const usb_event_queue_t* queue) {
    return queue->count;
}
