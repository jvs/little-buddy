#ifndef EVENT_PROCESSOR_H
#define EVENT_PROCESSOR_H

#include "usb_events.h"

// Event processor initialization and management
void event_processor_init(void);
void event_processor_process(usb_event_queue_t* input_queue, usb_output_queue_t* output_queue);

// Event processor state management
void event_processor_reset(void);

#endif // EVENT_PROCESSOR_H