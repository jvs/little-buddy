#include "engine/engine.h"

#include <string.h>


#define ENGINE_OUTPUT_QUEUE_SIZE 32

typedef struct {
    engine_event_t events[ENGINE_OUTPUT_QUEUE_SIZE];
    volatile uint32_t head;
    volatile uint32_t tail;
    volatile uint32_t count;
} engine_output_queue_t;

static engine_output_queue_t queue;

void engine_output_init(void) {
    memset(&queue, 0, sizeof(engine_output_queue_t));
}

bool engine_output_enqueue(const engine_event_t *event) {
    if (queue.count >= ENGINE_OUTPUT_QUEUE_SIZE) {
        return false;
    }

    queue.events[queue.tail] = *event;
    queue.tail = (queue.tail + 1) % ENGINE_OUTPUT_QUEUE_SIZE;
    queue.count++;

    return true;
}

bool engine_output_dequeue(engine_event_t *event) {
    while (true) {
        if (queue.count == 0) {
            return false;
        }
        *event = queue.events[queue.head];
        queue.head = (queue.head + 1) % ENGINE_OUTPUT_QUEUE_SIZE;
        queue.count--;

        // Ignore non-events and tick events.
        if (event->type != ENGINE_NON_EVENT && event->type != ENGINE_TICK_EVENT) {
            return true;
        }
    };
}
