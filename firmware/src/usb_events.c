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

uint32_t time_delta_ms(uint32_t start_ms, uint32_t end_ms) {
    // This works correctly even with uint32_t wraparound
    // due to unsigned integer arithmetic properties
    return (end_ms - start_ms);
}

//--------------------------------------------------------------------+
// OUTPUT QUEUE IMPLEMENTATION
//--------------------------------------------------------------------+

void usb_output_queue_init(usb_output_queue_t* queue) {
    memset(queue, 0, sizeof(usb_output_queue_t));
}

bool usb_output_queue_enqueue(usb_output_queue_t* queue, const usb_output_event_t* event) {
    if (queue->count >= USB_OUTPUT_QUEUE_SIZE) {
        return false;  // Queue full
    }
    
    queue->events[queue->tail] = *event;
    queue->tail = (queue->tail + 1) % USB_OUTPUT_QUEUE_SIZE;
    queue->count++;
    
    return true;
}

bool usb_output_queue_dequeue(usb_output_queue_t* queue, usb_output_event_t* event) {
    if (queue->count == 0) {
        return false;  // Queue empty
    }
    
    *event = queue->events[queue->head];
    queue->head = (queue->head + 1) % USB_OUTPUT_QUEUE_SIZE;
    queue->count--;
    
    return true;
}

bool usb_output_queue_is_empty(const usb_output_queue_t* queue) {
    return queue->count == 0;
}

uint32_t usb_output_queue_count(const usb_output_queue_t* queue) {
    return queue->count;
}

bool usb_output_enqueue_mouse(usb_output_queue_t* queue, int8_t delta_x, int8_t delta_y, uint8_t buttons, int8_t scroll) {
    usb_output_event_t event;
    event.type = USB_OUTPUT_MOUSE;
    event.data.mouse.delta_x = delta_x;
    event.data.mouse.delta_y = delta_y;
    event.data.mouse.buttons = buttons;
    event.data.mouse.scroll = scroll;
    
    return usb_output_queue_enqueue(queue, &event);
}

bool usb_output_enqueue_keyboard(usb_output_queue_t* queue, uint8_t keycode, uint8_t modifier, bool pressed) {
    usb_output_event_t event;
    event.type = USB_OUTPUT_KEYBOARD;
    event.data.keyboard.keycode = keycode;
    event.data.keyboard.modifier = modifier;
    event.data.keyboard.pressed = pressed;
    
    return usb_output_queue_enqueue(queue, &event);
}
