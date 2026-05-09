#ifndef ENGINE_STANDARDIZE_H
#define ENGINE_STANDARDIZE_H

#include <stdbool.h>

#include "engine/types.h"

void standardize_enqueue(engine_event_t event);
bool standardize_dequeue(engine_event_t *event);

#endif
