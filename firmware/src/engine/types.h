#ifndef ENGINE_TYPES_H
#define ENGINE_TYPES_H

#include <stdbool.h>
#include <stdint.h>


typedef enum {
    ENGINE_NON_EVENT = 0,
    ENGINE_MOVE_EVENT,
    ENGINE_PRESS_BUTTON_EVENT,
    ENGINE_PRESS_KEY_EVENT,
    ENGINE_RELEASE_BUTTON_EVENT,
    ENGINE_RELEASE_KEY_EVENT,
    ENGINE_SCROLL_EVENT,
    ENGINE_TICK_EVENT
} engine_event_type_t;


typedef struct {
    int8_t delta_x;
    int8_t delta_y;
} engine_move_t;


typedef struct {
    engine_event_type_t type;
    union {
        uint8_t button;
        uint8_t keycode;
        int8_t scroll;
        engine_move_t move;
    } data;
    uint64_t timestamp_us;
} engine_event_t;

#endif
