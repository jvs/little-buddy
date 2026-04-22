#ifndef ENGINE_SETTINGS_H
#define ENGINE_SETTINGS_H

typedef enum {
    OS_MODE_MAC = 0,
    OS_MODE_WINDOWS,
    OS_MODE_COUNT
} os_mode_t;

typedef enum {
    MOUSE_MODE_SCROLL = 0,
    MOUSE_MODE_PASSTHROUGH,
    MOUSE_MODE_COUNT
} mouse_mode_t;

os_mode_t settings_os(void);
mouse_mode_t settings_mouse(void);

void settings_cycle_os(void);
void settings_cycle_mouse(void);

#endif
