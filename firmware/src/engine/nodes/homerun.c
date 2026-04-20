#include <string.h>

#include "engine/engine.h"
#include "engine/keycodes.h"


#define HR_BUFFER_SIZE 64
#define HR_OUTPUT_QUEUE_SIZE 16
#define HR_MAX_ARMED 16


typedef struct {
    uint8_t trigger;
    uint8_t output_key;
    uint8_t output_mod;  // 0 = no modifier
} hr_layer_entry_t;


// F layer — symbols. Active while F is held.
static const hr_layer_entry_t f_layer[] = {
    { KEY_U,     KEY_LEFT_BRACKET,  KEY_LEFT_SHIFT },  // {
    { KEY_I,     KEY_RIGHT_BRACKET, KEY_LEFT_SHIFT },  // }
    { KEY_J,     KEY_9,             KEY_LEFT_SHIFT },  // (
    { KEY_K,     KEY_0,             KEY_LEFT_SHIFT },  // )
    { KEY_M,     KEY_LEFT_BRACKET,  0 },               // [
    { KEY_COMMA, KEY_RIGHT_BRACKET, 0 },               // ]
    { KEY_P,     KEY_EQUAL,         KEY_LEFT_SHIFT },  // +
    { KEY_H,     KEY_COMMA,         KEY_LEFT_SHIFT },  // <
    { KEY_L,     KEY_PERIOD,        KEY_LEFT_SHIFT },  // >
};

#define F_LAYER_COUNT (sizeof(f_layer) / sizeof(f_layer[0]))


static bool is_homerun_key(uint8_t keycode) {
    return keycode == KEY_F;
}


static const hr_layer_entry_t *find_layer_entry(uint8_t hr_key, uint8_t trigger) {
    if (hr_key == KEY_F) {
        for (uint8_t i = 0; i < F_LAYER_COUNT; i++) {
            if (f_layer[i].trigger == trigger) return &f_layer[i];
        }
    }
    return NULL;
}


// Ambiguous buffer: HR press(es) awaiting resolution. Layer mode only fills slot 0.
static engine_event_t ambiguous[HR_BUFFER_SIZE];
static uint8_t ambiguous_count;

// Output queue: resolved events ready for dequeue.
static engine_event_t out_queue[HR_OUTPUT_QUEUE_SIZE];
static uint8_t out_head;
static uint8_t out_tail;
static uint8_t out_count;

// Which HR key (if any) is currently acting as a layer. 0 = none.
static uint8_t active_layer_hr;

// Trigger keys pressed during the active layer, awaiting release.
static uint8_t armed[HR_MAX_ARMED];
static uint8_t armed_count;

// Releases to silently drop (trigger's press was consumed by a layer that has ended).
static uint8_t pending_drop[HR_MAX_ARMED];
static uint8_t pending_drop_count;


static void list_add(uint8_t *list, uint8_t *count, uint8_t max, uint8_t keycode) {
    if (*count >= max) return;
    list[(*count)++] = keycode;
}


static bool list_remove(uint8_t *list, uint8_t *count, uint8_t keycode) {
    for (uint8_t i = 0; i < *count; i++) {
        if (list[i] == keycode) {
            list[i] = list[*count - 1];
            (*count)--;
            return true;
        }
    }
    return false;
}


static void enqueue_out(const engine_event_t *event) {
    if (out_count >= HR_OUTPUT_QUEUE_SIZE) return;
    out_queue[out_tail] = *event;
    out_tail = (out_tail + 1) % HR_OUTPUT_QUEUE_SIZE;
    out_count++;
}


static void emit_key(engine_event_type_t type, uint8_t keycode, uint64_t timestamp_us) {
    engine_event_t ev = {
        .type = type,
        .data.keycode = keycode,
        .timestamp_us = timestamp_us,
    };
    enqueue_out(&ev);
}


static void emit_combo(const hr_layer_entry_t *entry, uint64_t timestamp_us) {
    if (entry->output_mod != 0) {
        emit_key(ENGINE_PRESS_KEY_EVENT, entry->output_mod, timestamp_us);
    }
    emit_key(ENGINE_PRESS_KEY_EVENT, entry->output_key, timestamp_us);
    emit_key(ENGINE_RELEASE_KEY_EVENT, entry->output_key, timestamp_us);
    if (entry->output_mod != 0) {
        emit_key(ENGINE_RELEASE_KEY_EVENT, entry->output_mod, timestamp_us);
    }
}


void homerun_init(void) {
    memset(ambiguous, 0, sizeof(ambiguous));
    ambiguous_count = 0;
    memset(out_queue, 0, sizeof(out_queue));
    out_head = 0;
    out_tail = 0;
    out_count = 0;
    active_layer_hr = 0;
    armed_count = 0;
    pending_drop_count = 0;
}


void homerun_enqueue(engine_event_t event) {
    // 1. Orphan release of a trigger whose layer has already ended — drop silently.
    if (event.type == ENGINE_RELEASE_KEY_EVENT &&
        list_remove(pending_drop, &pending_drop_count, event.data.keycode)) {
        return;
    }

    // 2. A layer is currently active.
    if (active_layer_hr != 0) {
        // Release of the HR key → end the layer.
        if (event.type == ENGINE_RELEASE_KEY_EVENT && event.data.keycode == active_layer_hr) {
            // Armed triggers never got their combos emitted; drop their releases too.
            for (uint8_t i = 0; i < armed_count; i++) {
                list_add(pending_drop, &pending_drop_count, HR_MAX_ARMED, armed[i]);
            }
            active_layer_hr = 0;
            armed_count = 0;
            return;
        }

        // Press of a layer-valid trigger → arm it, emit nothing yet.
        if (event.type == ENGINE_PRESS_KEY_EVENT &&
            find_layer_entry(active_layer_hr, event.data.keycode) != NULL) {
            list_add(armed, &armed_count, HR_MAX_ARMED, event.data.keycode);
            return;
        }

        // Release of an armed trigger → emit combo.
        if (event.type == ENGINE_RELEASE_KEY_EVENT &&
            list_remove(armed, &armed_count, event.data.keycode)) {
            const hr_layer_entry_t *entry = find_layer_entry(active_layer_hr, event.data.keycode);
            if (entry != NULL) emit_combo(entry, event.timestamp_us);
            return;
        }

        // Everything else (non-trigger keys, ticks, mouse) passes through.
        enqueue_out(&event);
        return;
    }

    // 3. An HR press is in the ambiguous buffer; try to resolve with this event.
    if (ambiguous_count > 0) {
        uint8_t hr = ambiguous[0].data.keycode;

        // Release of the pending HR key → normal tap.
        if (event.type == ENGINE_RELEASE_KEY_EVENT && event.data.keycode == hr) {
            enqueue_out(&ambiguous[0]);
            enqueue_out(&event);
            ambiguous_count = 0;
            return;
        }

        // A key press while ambiguous decides the HR's fate.
        if (event.type == ENGINE_PRESS_KEY_EVENT) {
            if (find_layer_entry(hr, event.data.keycode) != NULL) {
                // Layer confirmed. The HR press itself produces no output.
                active_layer_hr = hr;
                ambiguous_count = 0;
                list_add(armed, &armed_count, HR_MAX_ARMED, event.data.keycode);
                return;
            }
            // Non-layer key → HR resolves as a tap, then the new event passes through.
            enqueue_out(&ambiguous[0]);
            ambiguous_count = 0;
            enqueue_out(&event);
            return;
        }

        // Ticks, mouse, etc. pass through without affecting resolution.
        enqueue_out(&event);
        return;
    }

    // 4. Idle. Start tracking an HR key on its press; otherwise pass through.
    if (event.type == ENGINE_PRESS_KEY_EVENT && is_homerun_key(event.data.keycode)) {
        ambiguous[0] = event;
        ambiguous_count = 1;
        return;
    }

    enqueue_out(&event);
}


bool homerun_dequeue(engine_event_t *event) {
    if (out_count == 0) return false;
    *event = out_queue[out_head];
    out_head = (out_head + 1) % HR_OUTPUT_QUEUE_SIZE;
    out_count--;
    return true;
}
