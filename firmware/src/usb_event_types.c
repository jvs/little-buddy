#include "usb_event_types.h"

uint32_t time_delta_ms(uint32_t start_ms, uint32_t end_ms) {
    // This works correctly even with uint32_t wraparound
    // due to unsigned integer arithmetic properties
    return (end_ms - start_ms);
}
