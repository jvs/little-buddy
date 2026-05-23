#include <stdio.h>
#include <string.h>

#include "engine/engine.h"
#include "engine/keycodes.h"
#include "display/display.h"
#include "system/system.h"


// Forward decls for Z-layer actions.
void settings_cycle_os(void);
void settings_cycle_mouse(void);
void settings_cycle_standardize(void);
void display_cycle_mode(void);
void vim_toggle(void);

static void enter_config_mode(void);
static void enter_bootsel(void);


#define HR_BUFFER_SIZE 64
#define HR_OUTPUT_QUEUE_SIZE 128
#define HR_MAX_ARMED 16

#define FULL_OVERLAP_LIMIT_US    150000
#define PARTIAL_OVERLAP_LIMIT_US 250000


typedef void (*hr_action_fn)(void);

typedef struct {
    uint8_t trigger;
    uint8_t output_key;     // ignored when `action` is non-NULL
    uint8_t output_mod;     // 0 = no modifier
    hr_action_fn action;    // NULL → key+mod combo; otherwise invoked instead
} hr_layer_entry_t;


// F layer — navigation. Active while F is held as a modifier.
static const hr_layer_entry_t f_layer[] = {
    { KEY_H, KEY_LEFT,      0 },
    { KEY_J, KEY_DOWN,      0 },
    { KEY_K, KEY_UP,        0 },
    { KEY_L, KEY_RIGHT,     0 },
    { KEY_U, KEY_PAGE_UP,   0 },
    { KEY_M, KEY_PAGE_DOWN, 0 },
};

#define F_LAYER_COUNT (sizeof(f_layer) / sizeof(f_layer[0]))


// D layer — symbols. Active while D is held as a modifier.
static const hr_layer_entry_t d_layer[] = {
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

#define D_LAYER_COUNT (sizeof(d_layer) / sizeof(d_layer[0]))


// Z layer — device actions. Triggers fire callbacks instead of emitting keys.
static const hr_layer_entry_t z_layer[] = {
    { KEY_B, 0, 0, enter_bootsel },
    { KEY_D, 0, 0, display_cycle_mode },
    { KEY_H, 0, 0, enter_config_mode },
    { KEY_M, 0, 0, settings_cycle_mouse },
    { KEY_O, 0, 0, settings_cycle_os },
    { KEY_R, 0, 0, system_reboot },
    { KEY_S, 0, 0, settings_cycle_standardize },
    { KEY_V, 0, 0, vim_toggle },
};


static const char *const Z_LEGEND =
    "Z-LAYER\n"
    " \n"
    "B: BOOTSEL\n"
    "D: DISPLAY\n"
    "H: CONFIG\n"
    "M: MOUSE\n"
    "O: OS\n"
    "R: REBOOT\n"
    "S: STANDARD\n"
    "V: VIM";

#define Z_LAYER_COUNT (sizeof(z_layer) / sizeof(z_layer[0]))


// Per-HR-key enable state. Users can toggle these in the Z-layer's config mode.
// Disabled HR keys behave as ordinary keys (no layer, no tap-latch).
static bool hr_enabled_f = true;
static bool hr_enabled_d = true;
static bool hr_enabled_z = true;
static bool hr_enabled_v = true;
static bool hr_enabled_n = true;

static bool config_mode;


static bool *hr_enabled_slot(uint8_t keycode) {
    if (keycode == KEY_F) return &hr_enabled_f;
    if (keycode == KEY_D) return &hr_enabled_d;
    if (keycode == KEY_Z) return &hr_enabled_z;
    if (keycode == KEY_V) return &hr_enabled_v;
    if (keycode == KEY_N) return &hr_enabled_n;
    return NULL;
}


static bool is_homerun_key(uint8_t keycode) {
    bool *slot = hr_enabled_slot(keycode);
    return slot != NULL && *slot;
}


static void show_config_legend(void) {
    char buf[160];
    snprintf(buf, sizeof(buf),
        "CONFIG MODE\n"
        " \n"
        "TAP KEY\n"
        "TO TOGGLE\n"
        " \n"
        "F: %s\n"
        "D: %s\n"
        "Z: %s\n"
        "V: %s\n"
        "N: %s",
        hr_enabled_f ? "ON" : "OFF",
        hr_enabled_d ? "ON" : "OFF",
        hr_enabled_z ? "ON" : "OFF",
        hr_enabled_v ? "ON" : "OFF",
        hr_enabled_n ? "ON" : "OFF");
    display_show_legend(buf);
}


static void enter_config_mode(void) {
    config_mode = true;
    show_config_legend();
}


static void enter_bootsel(void) {
    display_clear_legend();
    display_clear_activity();
    system_bootsel();
}


static const hr_layer_entry_t *find_layer_entry(uint8_t hr_key, uint8_t trigger) {
    if (hr_key == KEY_F) {
        for (uint8_t i = 0; i < F_LAYER_COUNT; i++) {
            if (f_layer[i].trigger == trigger) return &f_layer[i];
        }
    } else if (hr_key == KEY_D) {
        for (uint8_t i = 0; i < D_LAYER_COUNT; i++) {
            if (d_layer[i].trigger == trigger) return &d_layer[i];
        }
    } else if (hr_key == KEY_Z) {
        for (uint8_t i = 0; i < Z_LAYER_COUNT; i++) {
            if (z_layer[i].trigger == trigger) return &z_layer[i];
        }
    }
    return NULL;
}


// Ambiguous buffer: events starting with the HR press, held until resolution.
// Tick events drive time checks but are not buffered (they have no meaning on replay).
static engine_event_t buffer[HR_BUFFER_SIZE];
static uint8_t buffer_count;

// Output queue: resolved events ready for dequeue.
static engine_event_t out_queue[HR_OUTPUT_QUEUE_SIZE];
static uint8_t out_head;
static uint8_t out_tail;
static uint8_t out_count;

// Which HR key (if any) is currently acting as a layer modifier. 0 = none.
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


// Emit press/release separately so the OS sees the key actually held, which
// enables auto-repeat (e.g., hold D+J to get repeated DOWN arrows).
static void emit_combo_press(const hr_layer_entry_t *entry, uint64_t timestamp_us) {
    if (entry->action != NULL) {
        entry->action();
        return;
    }
    if (entry->output_mod != 0) {
        emit_key(ENGINE_PRESS_KEY_EVENT, entry->output_mod, timestamp_us);
    }
    emit_key(ENGINE_PRESS_KEY_EVENT, entry->output_key, timestamp_us);
}


static void emit_combo_release(const hr_layer_entry_t *entry, uint64_t timestamp_us) {
    if (entry->action != NULL) return;
    emit_key(ENGINE_RELEASE_KEY_EVENT, entry->output_key, timestamp_us);
    if (entry->output_mod != 0) {
        emit_key(ENGINE_RELEASE_KEY_EVENT, entry->output_mod, timestamp_us);
    }
}


// Layer-state event handler. Used for live events and for replaying buffered
// events when we promote from AMBIGUOUS to LAYER.
static void apply_in_layer(const engine_event_t *event) {
    // Release of HR → end layer. Any still-armed triggers never had their
    // release come in yet — emit combos now, drop their eventual releases.
    if (event->type == ENGINE_RELEASE_KEY_EVENT && event->data.keycode == active_layer_hr) {
        for (uint8_t i = 0; i < armed_count; i++) {
            const hr_layer_entry_t *entry = find_layer_entry(active_layer_hr, armed[i]);
            if (entry != NULL) emit_combo_release(entry, event->timestamp_us);
            list_add(pending_drop, &pending_drop_count, HR_MAX_ARMED, armed[i]);
        }
        if (active_layer_hr == KEY_Z) {
            display_clear_legend();
            config_mode = false;
        } else {
            char letter[2] = { 'A' + (active_layer_hr - KEY_A), 0 };
            display_clear_if_showing(letter);
        }
        active_layer_hr = 0;
        armed_count = 0;
        return;
    }

    // Release of an armed trigger → emit the combo's release edge. Runs even
    // in config mode so the H key (whose action opened config mode) gets
    // untracked on release and we don't leave a stale pending_drop behind.
    if (event->type == ENGINE_RELEASE_KEY_EVENT &&
        list_remove(armed, &armed_count, event->data.keycode)) {
        const hr_layer_entry_t *entry = find_layer_entry(active_layer_hr, event->data.keycode);
        if (entry != NULL) emit_combo_release(entry, event->timestamp_us);
        return;
    }

    // Config mode: presses toggle HR-key enabled state and get swallowed;
    // non-armed releases are silently dropped.
    if (config_mode) {
        if (event->type == ENGINE_PRESS_KEY_EVENT) {
            bool *slot = hr_enabled_slot(event->data.keycode);
            if (slot != NULL) {
                *slot = !*slot;
                char msg[24];
                snprintf(msg, sizeof(msg), "%s HR %c",
                    *slot ? "ENABLED" : "DISABLED",
                    'A' + (event->data.keycode - KEY_A));
                show_config_legend();
                display_show_message(msg);
            } else {
                display_show_message("NOT HR KEY");
            }
        }
        return;
    }

    // V/N layers — tap a prefix key (F13/F14), then pass the key through unchanged.
    if (event->type == ENGINE_PRESS_KEY_EVENT &&
        (active_layer_hr == KEY_V || active_layer_hr == KEY_N)) {
        uint8_t prefix = (active_layer_hr == KEY_V) ? KEY_F18 : KEY_F19;
        emit_key(ENGINE_PRESS_KEY_EVENT,   prefix, event->timestamp_us);
        emit_key(ENGINE_RELEASE_KEY_EVENT, prefix, event->timestamp_us);
        enqueue_out(event);
        return;
    }

    // Press of a layer-valid trigger → arm and emit the combo's press edge now
    // so the OS can auto-repeat while the trigger is held.
    if (event->type == ENGINE_PRESS_KEY_EVENT) {
        const hr_layer_entry_t *entry = find_layer_entry(active_layer_hr, event->data.keycode);
        if (entry != NULL) {
            list_add(armed, &armed_count, HR_MAX_ARMED, event->data.keycode);
            emit_combo_press(entry, event->timestamp_us);
            return;
        }
    }

    // Everything else (non-trigger keys, ticks, mouse) passes through.
    enqueue_out(event);
}


// Promote from AMBIGUOUS to LAYER: HR press becomes an invisible modifier and
// the buffered events replay through the layer logic.
static void promote_to_layer(void) {
    active_layer_hr = buffer[0].data.keycode;
    if (active_layer_hr == KEY_Z) {
        config_mode = false;
        display_show_legend(Z_LEGEND);
    } else {
        char letter[2] = { 'A' + (active_layer_hr - KEY_A), 0 };
        display_show_message(letter);
    }
    for (uint8_t i = 1; i < buffer_count; i++) {
        apply_in_layer(&buffer[i]);
    }
    buffer_count = 0;
}


// Does the buffer contain a completed press+release pair for any non-HR key?
static bool buffer_has_completed_pair(void) {
    for (uint8_t i = 1; i < buffer_count; i++) {
        if (buffer[i].type != ENGINE_PRESS_KEY_EVENT) continue;
        uint8_t kc = buffer[i].data.keycode;
        for (uint8_t j = i + 1; j < buffer_count; j++) {
            if (buffer[j].type == ENGINE_RELEASE_KEY_EVENT && buffer[j].data.keycode == kc) {
                return true;
            }
        }
    }
    return false;
}


void homerun_init(void) {
    memset(buffer, 0, sizeof(buffer));
    buffer_count = 0;
    memset(out_queue, 0, sizeof(out_queue));
    out_head = 0;
    out_tail = 0;
    out_count = 0;
    active_layer_hr = 0;
    armed_count = 0;
    pending_drop_count = 0;
}


void homerun_enqueue(engine_event_t event) {
    // Orphan release of a trigger whose layer already ended — drop silently.
    if (event.type == ENGINE_RELEASE_KEY_EVENT &&
        list_remove(pending_drop, &pending_drop_count, event.data.keycode)) {
        return;
    }

    // A layer is active — run through layer logic.
    if (active_layer_hr != 0) {
        apply_in_layer(&event);
        return;
    }

    // Ambiguous: we're holding an HR press pending resolution.
    if (buffer_count > 0) {
        uint64_t hr_press_time = buffer[0].timestamp_us;
        uint8_t hr = buffer[0].data.keycode;

        // HR release → case 1 (tap). Flush buffer verbatim + the HR release.
        if (event.type == ENGINE_RELEASE_KEY_EVENT && event.data.keycode == hr) {
            for (uint8_t i = 0; i < buffer_count; i++) {
                enqueue_out(&buffer[i]);
            }
            enqueue_out(&event);
            buffer_count = 0;
            return;
        }

        // Non-tick events get buffered so case 1 / promotion can replay them.
        if (event.type != ENGINE_TICK_EVENT) {
            if (buffer_count < HR_BUFFER_SIZE) {
                buffer[buffer_count++] = event;
            }
        }

        // Check for resolution based on elapsed time since the HR press.
        uint64_t elapsed = event.timestamp_us - hr_press_time;

        if (elapsed > PARTIAL_OVERLAP_LIMIT_US) {
            // Case 2: held longer than the partial-overlap limit → modifier.
            promote_to_layer();
        } else if (elapsed > FULL_OVERLAP_LIMIT_US && buffer_has_completed_pair()) {
            // Case 3: another key fully press+released past the full-overlap limit → modifier.
            promote_to_layer();
        }
        return;
    }

    // Idle. Start tracking an HR key on its press; otherwise pass through.
    if (event.type == ENGINE_PRESS_KEY_EVENT && is_homerun_key(event.data.keycode)) {
        buffer[0] = event;
        buffer_count = 1;
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
