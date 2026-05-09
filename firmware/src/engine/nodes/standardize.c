#include "engine/nodes/standardize.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "engine/engine.h"
#include "engine/keycodes.h"
#include "engine/settings.h"


#define MOD_BIT(kc) (1u << ((kc) - 0xE0))
#define MOD_LCTRL   MOD_BIT(KEY_LEFT_CONTROL)
#define MOD_LSHIFT  MOD_BIT(KEY_LEFT_SHIFT)
#define MOD_LALT    MOD_BIT(KEY_LEFT_ALT)
#define MOD_LGUI    MOD_BIT(KEY_LEFT_GUI)
#define MOD_RSHIFT  MOD_BIT(KEY_RIGHT_SHIFT)

#define STD_QUEUE_SIZE 64


// Keys where Alt should behave like Ctrl on Windows.
static const uint8_t WIN_ALT_TO_CTRL[] = {
    KEY_N, KEY_T, KEY_Z, KEY_X, KEY_C, KEY_V,
    KEY_W, KEY_A, KEY_S, KEY_F, KEY_R,
};

// Keys where Command should behave like Alt on MacOS.
static const uint8_t MAC_GUI_TO_ALT[] = {
    KEY_H, KEY_J, KEY_K, KEY_L, KEY_SEMICOLON,
    KEY_U, KEY_O, KEY_T, KEY_P,
};


// Physical modifier bits currently held by the user (tracked from events
// flowing through; modifier events still pass through unchanged to the host).
static uint8_t phys_mods;


// Record of keys whose press we rewrote, so the matching release can unwind
// the same synthetic modifier dance.
typedef struct {
    uint8_t orig_kc;      // keycode from input event (non-zero = slot used)
    uint8_t emit_kc;      // keycode actually emitted to host
    uint8_t synth_mods;   // mods we synthetically pressed
    uint8_t displaced;    // mods we synthetically released
} swap_t;

#define MAX_SWAPS 8
static swap_t swaps[MAX_SWAPS];


// Output queue — drained by custom.c and fed into homerun.
static engine_event_t out_queue[STD_QUEUE_SIZE];
static uint8_t out_head;
static uint8_t out_tail;
static uint8_t out_count;


static void enqueue_out(engine_event_t event) {
    if (out_count >= STD_QUEUE_SIZE) return;
    out_queue[out_tail] = event;
    out_tail = (out_tail + 1) % STD_QUEUE_SIZE;
    out_count++;
}


bool standardize_dequeue(engine_event_t *event) {
    if (out_count == 0) return false;
    *event = out_queue[out_head];
    out_head = (out_head + 1) % STD_QUEUE_SIZE;
    out_count--;
    return true;
}


static bool in_list(const uint8_t *list, size_t n, uint8_t kc) {
    for (size_t i = 0; i < n; i++) if (list[i] == kc) return true;
    return false;
}


static swap_t *alloc_swap(uint8_t orig_kc) {
    for (size_t i = 0; i < MAX_SWAPS; i++) {
        if (swaps[i].orig_kc == 0) {
            swaps[i].orig_kc = orig_kc;
            return &swaps[i];
        }
    }
    return NULL;
}


static swap_t *find_swap(uint8_t orig_kc) {
    for (size_t i = 0; i < MAX_SWAPS; i++) {
        if (swaps[i].orig_kc == orig_kc) return &swaps[i];
    }
    return NULL;
}


static void emit_mod(uint8_t bit_index, bool press, uint64_t ts) {
    engine_event_t ev = {
        .type = press ? ENGINE_PRESS_KEY_EVENT : ENGINE_RELEASE_KEY_EVENT,
        .timestamp_us = ts,
    };
    ev.data.keycode = 0xE0 + bit_index;
    enqueue_out(ev);
}


static void emit_mod_mask(uint8_t mask, bool press, uint64_t ts) {
    for (uint8_t bit = 0; bit < 8; bit++) {
        if (mask & (1u << bit)) emit_mod(bit, press, ts);
    }
}


void standardize_enqueue(engine_event_t event) {
    bool is_press   = event.type == ENGINE_PRESS_KEY_EVENT;
    bool is_release = event.type == ENGINE_RELEASE_KEY_EVENT;
    bool is_mod_evt = (is_press || is_release)
                      && event.data.keycode >= 0xE0 && event.data.keycode <= 0xE7;

    if (is_mod_evt) {
        uint8_t bit = 1u << (event.data.keycode - 0xE0);
        if (is_press) phys_mods |= bit;
        else          phys_mods &= ~bit;
        enqueue_out(event);
        return;
    }

    if (!settings_standardize() || !(is_press || is_release)) {
        enqueue_out(event);
        return;
    }

    uint64_t ts = event.timestamp_us;

    if (is_press) {
        uint8_t kc = event.data.keycode;
        uint8_t emit_kc = kc;
        uint8_t synth = 0, displaced = 0;

        if (settings_os() == OS_MODE_WINDOWS) {
            bool alt_held = phys_mods & MOD_LALT;
            uint8_t shift_bits = phys_mods & (MOD_LSHIFT | MOD_RSHIFT);
            if (alt_held && shift_bits && kc == KEY_LEFT_BRACKET) {
                // Alt+Shift+[ → Ctrl+Tab
                displaced = MOD_LALT | shift_bits;
                synth = MOD_LCTRL;
                emit_kc = KEY_TAB;
            } else if (alt_held && shift_bits && kc == KEY_RIGHT_BRACKET) {
                // Alt+Shift+] → Ctrl+Shift+Tab (keep shift)
                displaced = MOD_LALT;
                synth = MOD_LCTRL;
                emit_kc = KEY_TAB;
            } else if (alt_held && in_list(WIN_ALT_TO_CTRL, sizeof(WIN_ALT_TO_CTRL), kc)) {
                displaced = MOD_LALT;
                synth = MOD_LCTRL;
            }
        } else if (settings_os() == OS_MODE_MAC) {
            // Physical Alt arrives here as GUI (remapper swaps them on Mac).
            bool gui_held = phys_mods & MOD_LGUI;
            if (gui_held && in_list(MAC_GUI_TO_ALT, sizeof(MAC_GUI_TO_ALT), kc)) {
                displaced = MOD_LGUI;
                synth = MOD_LALT;
            }
        }

        if (displaced || synth) {
            swap_t *s = alloc_swap(kc);
            if (s) {
                s->emit_kc = emit_kc;
                s->synth_mods = synth;
                s->displaced = displaced;
                emit_mod_mask(displaced, false, ts);
                emit_mod_mask(synth, true, ts);
                event.data.keycode = emit_kc;
            }
        }
        enqueue_out(event);
        return;
    }

    // Release
    swap_t *s = find_swap(event.data.keycode);
    if (s) {
        uint8_t synth = s->synth_mods;
        uint8_t displaced = s->displaced;
        event.data.keycode = s->emit_kc;
        enqueue_out(event);
        emit_mod_mask(synth, false, ts);
        // Re-press displaced mods only if still physically held.
        emit_mod_mask(displaced & phys_mods, true, ts);
        memset(s, 0, sizeof(*s));
        return;
    }
    enqueue_out(event);
}
