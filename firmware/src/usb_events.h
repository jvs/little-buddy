#ifndef USB_EVENTS_H
#define USB_EVENTS_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    INPUT_NONE = 0,
    INPUT_MOUSE,
    INPUT_KEYBOARD,
    INPUT_DEVICE_CONNECTED,
    INPUT_DEVICE_DISCONNECTED,
    INPUT_TICK
} input_type_t;

typedef enum {
    OUTPUT_NONE = 0,
    OUTPUT_MOUSE,
    OUTPUT_KEYBOARD
} output_type_t;

typedef struct {
    int8_t delta_x;
    int8_t delta_y;
    int8_t scroll;
    uint8_t buttons;
} mouse_data_t;

typedef struct {
    uint8_t modifier;
    uint8_t keycodes[6];
} keyboard_data_t;

typedef struct {
    uint8_t device_address;
    uint8_t instance;
    const char* device_type;
} device_data_t;

typedef struct {
    uint32_t tick_count;       // Running tick counter
    uint32_t delta_us;         // Microseconds since last tick
} tick_data_t;

typedef struct {
    input_type_t type;
    uint32_t timestamp_ms;
    uint32_t sequence_id;      // Event sequence number
    uint8_t interface_id;      // Which HID interface (0, 1, etc.)
    union {
        mouse_data_t mouse;
        keyboard_data_t keyboard;
        device_data_t device;
        tick_data_t tick;
    } data;
} input_event_t;

// Output event structure (no timestamp/sequence needed)
typedef struct {
    output_type_t type;
    union {
        mouse_data_t mouse;
        keyboard_data_t keyboard;
    } data;
} output_event_t;

#define INPUT_QUEUE_SIZE 32
#define OUTPUT_QUEUE_SIZE 32

typedef struct {
    input_event_t events[INPUT_QUEUE_SIZE];
    volatile uint32_t head;
    volatile uint32_t tail;
    volatile uint32_t count;
} input_queue_t;

typedef struct {
    output_event_t events[OUTPUT_QUEUE_SIZE];
    volatile uint32_t head;
    volatile uint32_t tail;
    volatile uint32_t count;
} output_queue_t;

// Input queue functions
void input_queue_init(input_queue_t *input_queue);
bool input_queue_enqueue(input_queue_t *input_queue, const input_event_t *input_event);
bool input_queue_dequeue(input_queue_t *input_queue, input_event_t *input_event);
uint32_t input_queue_count(const input_queue_t *queue);

// Timing utilities (handles uint32_t wraparound correctly)
uint32_t time_delta_ms(uint32_t start_ms, uint32_t end_ms);

// Output queue functions
void output_queue_init(output_queue_t *queue);
bool output_queue_enqueue(output_queue_t *queue, const output_event_t *event);
bool output_queue_dequeue(output_queue_t *queue, output_event_t *event);
uint32_t output_queue_count(const output_queue_t *queue);

#endif
