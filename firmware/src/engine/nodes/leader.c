#include "engine/engine.h"


static engine_event_t tmp_event;


void leader_enqueue(engine_event_t event) {
    tmp_event = event;
}


bool leader_dequeue(const engine_event_t *event) {
    if (tmp_event.type == ENGINE_NON_EVENT) {
        return false;
    }

    *event = tmp_event;
    tmp_event.type = ENGINE_NON_EVENT; // Mark as consumed.
    return true;
}

