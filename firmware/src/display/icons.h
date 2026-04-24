#ifndef DISPLAY_ICONS_H
#define DISPLAY_ICONS_H

#include <stdint.h>

typedef struct {
    const char* name;
    const uint8_t *data;
} icon_t;

// Icon declarations (add your icons here)
extern const icon_t icon_apple_logo;

#endif // DISPLAY_ICONS_H
