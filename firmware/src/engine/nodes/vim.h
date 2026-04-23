#ifndef ENGINE_NODES_VIM_H
#define ENGINE_NODES_VIM_H

#include <stdbool.h>

#include "engine/types.h"

typedef enum {
    VIM_MODE_OFF = 0,
    VIM_MODE_NORMAL,
    VIM_MODE_INSERT,
    VIM_MODE_VISUAL,
} vim_mode_t;

void vim_enqueue(engine_event_t event);
bool vim_dequeue(engine_event_t *event);

// Z-layer action: toggles vim on/off (OFF <-> NORMAL).
void vim_toggle(void);

vim_mode_t vim_mode(void);

#endif
