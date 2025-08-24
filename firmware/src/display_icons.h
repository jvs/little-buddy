#ifndef DISPLAY_ICONS_H
#define DISPLAY_ICONS_H

#include <stdint.h>

#include "sh1107_display.h"

typedef enum {
    ICON_BOMB = 0
} icon_t;

void display_icons_draw(sh1107_t* display, icon_t icon);

#endif
