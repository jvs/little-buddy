#ifndef ENGINE_H
#define ENGINE_H

#include "engine/types.h"

// engine.c
void engine_init(void);
void engine_task(void);

// input.c
bool engine_input_dequeue(engine_event_t *event);
bool engine_input_enqueue(const engine_event_t *event);

// output.c
bool engine_output_dequeue(engine_event_t *event);
bool engine_output_enqueue(const engine_event_t *event);

#endif
