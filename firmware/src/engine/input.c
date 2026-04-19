#include "engine/engine.h"

#include <string.h>


#define ENGINE_INPUT_QUEUE_SIZE 128


typedef struct {
    engine_event_t events[ENGINE_INPUT_QUEUE_SIZE];
    volatile uint32_t head;
    volatile uint32_t tail;
    volatile uint32_t count;
} engine_input_queue_t;


static engine_input_queue_t queue;


void engine_input_init(void) {
    memset(&queue, 0, sizeof(engine_input_queue_t));
}


bool engine_input_enqueue(const engine_event_t *event) {
    if (queue.count >= ENGINE_INPUT_QUEUE_SIZE) {
        return false;
    }

    queue.events[queue.tail] = *event;
    queue.tail = (queue.tail + 1) % ENGINE_INPUT_QUEUE_SIZE;
    queue.count++;

    return true;
}


bool engine_input_dequeue(engine_event_t *event) {
    if (queue.count == 0) {
        return false;
    }

    *event = queue.events[queue.head];
    queue.head = (queue.head + 1) % ENGINE_INPUT_QUEUE_SIZE;
    queue.count--;

    return true;
}
