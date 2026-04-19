#include "engine/engine.h"

void engine_custom_init(void) {
    (void)0;
}

void engine_custom_task(void) {
    // For now, just copy from the input queue to the output queue.
    engine_event_t event;

    while (engine_input_dequeue(&event)) {
        engine_output_enqueue(&event);
    }
}
