#include "engine/engine.h"
#include "engine/keycodes.h"


typedef enum {
    HOST_OS_MAC,
    HOST_OS_WINDOWS,
} host_os_t;


#define HOST_OS HOST_OS_MAC


static uint8_t remap_keys(uint8_t keycode) {
    switch (keycode) {
        case KEY_PAGE_UP:    return KEY_LEFT;
        case KEY_PAGE_DOWN:  return KEY_RIGHT;
        case KEY_CAPS_LOCK:  return KEY_ESCAPE;
    }

    if (HOST_OS == HOST_OS_MAC) {
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
}
