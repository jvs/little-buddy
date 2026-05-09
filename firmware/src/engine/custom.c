#include "engine/engine.h"
#include "engine/keycodes.h"
#include "engine/nodes/standardize.h"
#include "engine/nodes/vim.h"

// nodes/remapper.c
void remapper_apply(engine_event_t *event);

// nodes/homerun.c
void homerun_init(void);
void homerun_enqueue(engine_event_t event);
bool homerun_dequeue(engine_event_t *event);

// nodes/leader.c
void leader_enqueue(engine_event_t event);
bool leader_dequeue(engine_event_t *event);

// nodes/follower.c
void follower_enqueue(engine_event_t event);
bool follower_dequeue(engine_event_t *event);


void engine_custom_init(void) {
    homerun_init();
}


void engine_custom_task(void) {
    engine_event_t event;

    while (engine_input_dequeue(&event)) {
        remapper_apply(&event);
        standardize_enqueue(event);
    }

    while (standardize_dequeue(&event)) {
        homerun_enqueue(event);
    }

    while (homerun_dequeue(&event)) {
        vim_enqueue(event);
    }

    while (vim_dequeue(&event)) {
        engine_output_enqueue(&event);
    }
}
