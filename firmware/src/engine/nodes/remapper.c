#include "engine/engine.h"
#include "engine/keycodes.h"
#include "engine/settings.h"


static uint8_t remap_keys(uint8_t keycode) {
    switch (keycode) {
        case KEY_PAGE_UP:    return KEY_LEFT;
        case KEY_PAGE_DOWN:  return KEY_RIGHT;
        case KEY_CAPS_LOCK:  return KEY_ESCAPE;
    }

    if (settings_os() == OS_MODE_MAC) {
        // Swap Alt and GUI on MacOS since the physical Alt key is mapped to GUI in MacOS.
        switch (keycode) {
            case KEY_LEFT_ALT:   return KEY_LEFT_GUI;
            case KEY_LEFT_GUI:   return KEY_LEFT_ALT;
            case KEY_RIGHT_ALT:  return KEY_RIGHT_GUI;
            case KEY_RIGHT_GUI:  return KEY_RIGHT_ALT;
        }
    }

    return keycode;
}


void remapper_apply(engine_event_t *event) {
    if (event->type == ENGINE_PRESS_KEY_EVENT || event->type == ENGINE_RELEASE_KEY_EVENT) {
        event->data.keycode = remap_keys(event->data.keycode);
    }

    if (event->type == ENGINE_MOVE_EVENT && settings_mouse() == MOUSE_MODE_SCROLL) {
        // Transform mouse (trackpoint) movement into scroll events. Accumulate
        // sub-tick motion so a hard push still scrolls faster than a light one,
        // rather than clamping every event to a single tick. Sign is inverted
        // so pushing the trackpoint up scrolls content up (natural scroll).
        static const int16_t SCROLL_DIVISOR = 8;
        static int16_t accum_y = 0;
        accum_y -= event->data.move.delta_y;
        int8_t ticks = (int8_t)(accum_y / SCROLL_DIVISOR);
        accum_y -= (int16_t)ticks * SCROLL_DIVISOR;
        if (ticks != 0) {
            event->type = ENGINE_SCROLL_EVENT;
            event->data.scroll = ticks;
        } else {
            event->type = ENGINE_NON_EVENT;
        }
    }
}
