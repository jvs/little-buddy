#include "usb_events.h"
#include <string.h>

void input_queue_init(input_queue_t *queue) {
    memset(queue, 0, sizeof(input_queue_t));
}

bool input_queue_enqueue(input_queue_t *queue, const input_event_t *event) {
    if (queue->count >= INPUT_QUEUE_SIZE) {
        return false;
    }

    queue->events[queue->tail] = *event;
    queue->tail = (queue->tail + 1) % INPUT_QUEUE_SIZE;
    queue->count++;

    return true;
}

bool input_queue_dequeue(input_queue_t *queue, input_event_t *event) {
    if (queue->count == 0) {
        return false;
    }

    *event = queue->events[queue->head];
    queue->head = (queue->head + 1) % INPUT_QUEUE_SIZE;
    queue->count--;

    return true;
}

uint32_t input_queue_count(const input_queue_t *queue) {
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

void output_queue_init(output_queue_t *queue) {
    memset(queue, 0, sizeof(output_queue_t));
}

bool output_queue_enqueue(output_queue_t *queue, const output_event_t *event) {
    if (queue->count >= OUTPUT_QUEUE_SIZE) {
        return false;  // Queue full
    }

    queue->events[queue->tail] = *event;
    queue->tail = (queue->tail + 1) % OUTPUT_QUEUE_SIZE;
    queue->count++;

    return true;
}

bool output_queue_dequeue(output_queue_t *queue, output_event_t *event) {
    if (queue->count == 0) {
        return false;  // Queue empty
    }

    *event = queue->events[queue->head];
    queue->head = (queue->head + 1) % OUTPUT_QUEUE_SIZE;
    queue->count--;

    return true;
}

uint32_t output_queue_count(const output_queue_t *queue) {
    return queue->count;
}
