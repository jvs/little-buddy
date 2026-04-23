#include "engine/settings.h"
#include "display/display.h"


static os_mode_t os_mode = OS_MODE_MAC;
static mouse_mode_t mouse_mode = MOUSE_MODE_SCROLL;
static bool standardize_on = false;


static const char *os_name(os_mode_t m) {
    switch (m) {
        case OS_MODE_MAC:     return "MAC";
        case OS_MODE_WINDOWS: return "WINDOWS";
        default:              return "?";
    }
}


static const char *mouse_name(mouse_mode_t m) {
    switch (m) {
        case MOUSE_MODE_SCROLL:      return "SCROLL";
        case MOUSE_MODE_PASSTHROUGH: return "MOUSE";
        default:                     return "?";
    }
}


os_mode_t settings_os(void) { return os_mode; }
mouse_mode_t settings_mouse(void) { return mouse_mode; }
bool settings_standardize(void) { return standardize_on; }


void settings_cycle_os(void) {
    os_mode = (os_mode + 1) % OS_MODE_COUNT;
    display_show_message(os_name(os_mode));
}


void settings_cycle_mouse(void) {
    mouse_mode = (mouse_mode + 1) % MOUSE_MODE_COUNT;
    display_show_message(mouse_name(mouse_mode));
}


void settings_cycle_standardize(void) {
    standardize_on = !standardize_on;
    display_show_message(standardize_on ? "STANDARD ON" : "STANDARD OFF");
}
