#ifndef ENGINE_H
#define ENGINE_H

#include "engine/types.h"

// engine.c
void engine_init(void);
void engine_task(void);

// input.c
bool engine_input_dequeue(usb_input_event_t *event);
bool engine_input_enqueue(const usb_input_event_t *event);
uint32_t engine_input_count();

#endif
