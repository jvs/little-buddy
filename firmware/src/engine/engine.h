#ifndef ENGINE_H
#define ENGINE_H

#include "engine/types.h"
#include "usb/types.h"

// engine.c
void engine_init(void);
void engine_task(void);

// Last-seen USB reports, for debug display. NULL if none seen yet.
const usb_mouse_data_t    *engine_last_input_mouse(void);
const usb_keyboard_data_t *engine_last_input_keyboard(void);
const usb_mouse_data_t    *engine_last_output_mouse(void);
const usb_keyboard_data_t *engine_last_output_keyboard(void);

// input.c
bool engine_input_dequeue(engine_event_t *event);
bool engine_input_enqueue(const engine_event_t *event);

// output.c
bool engine_output_dequeue(engine_event_t *event);
bool engine_output_enqueue(const engine_event_t *event);

#endif
