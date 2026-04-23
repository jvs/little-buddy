#include "engine/nodes/vim.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "engine/engine.h"
#include "engine/keycodes.h"
#include "engine/settings.h"
#include "display/display.h"


#define VIM_QUEUE_SIZE 128
#define PENDING_MAX    4


// Modifier masks for tap(). Bit order is arbitrary internal to this file.
#define M_NONE  0u
#define M_CTRL  (1u << 0)
#define M_SHIFT (1u << 1)
#define M_ALT   (1u << 2)
#define M_GUI   (1u << 3)


static vim_mode_t mode = VIM_MODE_OFF;

// Command parsing state — reset after each resolved command.
static char     pending[PENDING_MAX];
static uint8_t  pending_len;
static uint16_t repeat_count;
static char     waiting_trigger;   // 0 if not waiting; 'r','f','F','t','T' otherwise

// Physical modifier bits held by user (tracked from incoming stream, even
// while vim is OFF, so a toggle mid-hold leaves us with accurate state).
static uint8_t phys_mods;

// Output queue — drained by custom.c into engine_output.
static engine_event_t out_queue[VIM_QUEUE_SIZE];
static uint8_t out_head, out_tail, out_count;

// Track a single "held motion" so hjkl can auto-repeat at the OS level.
// When set, we've emitted press(held_output_kc) with held_output_mods, and
// are waiting for the matching input release to emit the teardown.
static uint8_t held_input_kc;
static uint8_t held_output_kc;
static uint8_t held_output_mods;

// Keycode of the raw event currently being dispatched (for hold tracking).
static uint8_t current_input_kc;


// --- Queue ------------------------------------------------------------------

static void enqueue_out(engine_event_t ev) {
    if (out_count >= VIM_QUEUE_SIZE) return;
    out_queue[out_tail] = ev;
    out_tail = (out_tail + 1) % VIM_QUEUE_SIZE;
    out_count++;
}

bool vim_dequeue(engine_event_t *event) {
    if (out_count == 0) return false;
    *event = out_queue[out_head];
    out_head = (out_head + 1) % VIM_QUEUE_SIZE;
    out_count--;
    return true;
}


// --- Low-level event emission ----------------------------------------------

static void emit_press(uint8_t kc) {
    engine_event_t ev = { .type = ENGINE_PRESS_KEY_EVENT };
    ev.data.keycode = kc;
    enqueue_out(ev);
}

static void emit_release(uint8_t kc) {
    engine_event_t ev = { .type = ENGINE_RELEASE_KEY_EVENT };
    ev.data.keycode = kc;
    enqueue_out(ev);
}

static void emit_mods(uint8_t mods, bool press) {
    // Press order: Ctrl, Shift, Alt, Gui. Release in reverse to keep the
    // "outermost" modifier pressed last / released first.
    const uint8_t bits[4]  = { M_CTRL, M_SHIFT, M_ALT, M_GUI };
    const uint8_t keys[4]  = { KEY_LEFT_CONTROL, KEY_LEFT_SHIFT, KEY_LEFT_ALT, KEY_LEFT_GUI };
    if (press) {
        for (int i = 0; i < 4; i++) if (mods & bits[i]) emit_press(keys[i]);
    } else {
        for (int i = 3; i >= 0; i--) if (mods & bits[i]) emit_release(keys[i]);
    }
}

static void release_held(void) {
    if (held_output_kc == 0) return;
    emit_release(held_output_kc);
    emit_mods(held_output_mods, false);
    held_input_kc = 0;
    held_output_kc = 0;
    held_output_mods = 0;
}

// Start an OS-level auto-repeat for `kc` with `mods`, keyed to the physical
// input keycode currently being dispatched. The matching release event will
// tear it down. Only one hold is tracked at a time; a second call releases
// the previous hold first.
static void hold_motion(uint8_t mods, uint8_t kc) {
    release_held();
    held_input_kc = current_input_kc;
    held_output_kc = kc;
    held_output_mods = mods;
    emit_mods(mods, true);
    emit_press(kc);
}

// Emit `count` presses of (mods+kc). Modifiers pressed once, key tapped N
// times, modifiers released once.
static void tap(uint8_t mods, uint8_t kc, uint16_t count) {
    if (count == 0) count = 1;
    emit_mods(mods, true);
    for (uint16_t i = 0; i < count; i++) {
        emit_press(kc);
        emit_release(kc);
    }
    emit_mods(mods, false);
}


// --- OS-aware semantic actions ---------------------------------------------

static bool is_mac(void) { return settings_os() == OS_MODE_MAC; }

// Movement
static void move_left(uint16_t c)  { tap(M_NONE, KEY_LEFT,  c); }
static void move_right(uint16_t c) { tap(M_NONE, KEY_RIGHT, c); }
static void move_up(uint16_t c)    { tap(M_NONE, KEY_UP,    c); }
static void move_down(uint16_t c)  { tap(M_NONE, KEY_DOWN,  c); }

static void move_word_forward(uint16_t c) {
    tap(is_mac() ? M_ALT : M_CTRL, KEY_RIGHT, c);
}
static void move_word_backward(uint16_t c) {
    tap(is_mac() ? M_ALT : M_CTRL, KEY_LEFT, c);
}
// No clean cross-OS "word end" primitive — approximate as word forward.
static void move_word_end(uint16_t c) { move_word_forward(c); }

static void move_line_start(void) {
    if (is_mac()) tap(M_GUI, KEY_LEFT, 1);
    else          tap(M_NONE, KEY_HOME, 1);
}
static void move_line_end(void) {
    if (is_mac()) tap(M_GUI, KEY_RIGHT, 1);
    else          tap(M_NONE, KEY_END, 1);
}
static void move_doc_start(void) {
    if (is_mac()) tap(M_GUI, KEY_UP, 1);
    else          tap(M_CTRL, KEY_HOME, 1);
}
static void move_doc_end(void) {
    if (is_mac()) tap(M_GUI, KEY_DOWN, 1);
    else          tap(M_CTRL, KEY_END, 1);
}

// Editing
static void delete_char(uint16_t c)      { tap(M_NONE, KEY_DELETE,    c); }
static void delete_char_back(uint16_t c) { tap(M_NONE, KEY_BACKSPACE, c); }

static void delete_word(uint16_t c) {
    tap(is_mac() ? M_ALT : M_CTRL, KEY_DELETE, c);
}
static void delete_word_back(uint16_t c) {
    tap(is_mac() ? M_ALT : M_CTRL, KEY_BACKSPACE, c);
}

static void delete_to_line_start(void) {
    if (is_mac()) tap(M_GUI | M_SHIFT, KEY_LEFT, 1);
    else          tap(M_SHIFT,         KEY_HOME, 1);
    tap(M_NONE, KEY_DELETE, 1);
}
static void delete_to_line_end(void) {
    if (is_mac()) tap(M_GUI | M_SHIFT, KEY_RIGHT, 1);
    else          tap(M_SHIFT,         KEY_END, 1);
    tap(M_NONE, KEY_DELETE, 1);
}

// dd — go to line start, select down c lines, delete.
static void delete_lines(uint16_t c) {
    move_line_start();
    tap(M_SHIFT, KEY_DOWN, c);
    tap(M_NONE, KEY_DELETE, 1);
}

// cc / S — empty the current line's contents and leave cursor there.
static void delete_line_contents(void) {
    move_line_start();
    delete_to_line_end();
}

static void copy_sel(void)  { tap(is_mac() ? M_GUI : M_CTRL, KEY_C, 1); }
static void cut_sel(void)   { tap(is_mac() ? M_GUI : M_CTRL, KEY_X, 1); }
static void paste(uint16_t c) { tap(is_mac() ? M_GUI : M_CTRL, KEY_V, c); }
static void undo(uint16_t c)  { tap(is_mac() ? M_GUI : M_CTRL, KEY_Z, c); }

// Yank: select, copy, press Left to collapse selection to start.
static void yank_lines(uint16_t c) {
    move_line_start();
    tap(M_SHIFT, KEY_DOWN, c);
    copy_sel();
    move_left(1);
}
static void yank_word(uint16_t c) {
    tap((is_mac() ? M_ALT : M_CTRL) | M_SHIFT, KEY_RIGHT, c);
    copy_sel();
    move_left(1);
}
static void yank_to_line_start(void) {
    if (is_mac()) tap(M_GUI | M_SHIFT, KEY_LEFT, 1);
    else          tap(M_SHIFT,         KEY_HOME, 1);
    copy_sel();
    move_right(1);
}
static void yank_to_line_end(void) {
    if (is_mac()) tap(M_GUI | M_SHIFT, KEY_RIGHT, 1);
    else          tap(M_SHIFT,         KEY_END, 1);
    copy_sel();
    move_left(1);
}

static void open_line_below(void) {
    move_line_end();
    tap(M_SHIFT, KEY_ENTER, 1);
}
static void open_line_above(void) {
    move_line_start();
    tap(M_SHIFT, KEY_ENTER, 1);
    move_up(1);
}


// --- Mode transitions ------------------------------------------------------

static void show_mode(void) {
    switch (mode) {
        case VIM_MODE_OFF:    display_show_message("VIM OFF");    break;
        case VIM_MODE_NORMAL: display_show_message("VIM NORMAL"); break;
        case VIM_MODE_INSERT: display_show_message("VIM INSERT"); break;
        case VIM_MODE_VISUAL: display_show_message("VIM VISUAL"); break;
    }
}

static void reset_pending(void) {
    pending_len = 0;
    pending[0] = 0;
    repeat_count = 0;
    waiting_trigger = 0;
}

static void enter_insert(void) {
    release_held();
    mode = VIM_MODE_INSERT;
    show_mode();
}

static void enter_visual(void) {
    release_held();
    mode = VIM_MODE_VISUAL;
    emit_press(KEY_LEFT_SHIFT);
    show_mode();
}

static void exit_visual_to_normal(void) {
    release_held();
    // Release the held Shift before dropping back to NORMAL.
    emit_release(KEY_LEFT_SHIFT);
    mode = VIM_MODE_NORMAL;
}


// --- Input -> char mapping --------------------------------------------------

static char keycode_to_char(uint8_t kc, bool shift) {
    if (kc >= KEY_A && kc <= KEY_Z) {
        char lower = 'a' + (kc - KEY_A);
        return shift ? (char)(lower - 32) : lower;
    }
    if (kc >= KEY_1 && kc <= KEY_9) {
        if (!shift) return '1' + (kc - KEY_1);
    }
    if (kc == KEY_0 && !shift) return '0';
    if (shift) {
        switch (kc) {
            case KEY_4: return '$';
            case KEY_6: return '^';
        }
    }
    return 0;
}


// --- Command handlers ------------------------------------------------------

static void handle_char_arg(engine_event_t event) {
    char trig = waiting_trigger;
    waiting_trigger = 0;
    reset_pending();

    if (trig == 'r') {
        // Replace current char: delete it, then synthesize the typed char
        // using its raw keycode + current physical shift state.
        delete_char(1);
        bool shift = phys_mods & ((1u << 1) | (1u << 5));
        if (shift) emit_press(KEY_LEFT_SHIFT);
        emit_press(event.data.keycode);
        emit_release(event.data.keycode);
        if (shift) emit_release(KEY_LEFT_SHIFT);
        return;
    }
    // f/F/t/T are stubbed — no reliable cross-app primitive for "move to
    // next occurrence of char". Noop for MVP.
}

static void try_finalize_normal(uint16_t count);
static void try_finalize_visual(uint16_t count);

static void handle_normal(char c) {
    // Count accumulation — digits at the start of a command.
    if (pending_len == 0) {
        if (c >= '1' && c <= '9') {
            repeat_count = repeat_count * 10 + (c - '0');
            if (repeat_count > 1000) repeat_count = 1000;
            return;
        }
        if (c == '0' && repeat_count > 0) {
            repeat_count *= 10;
            if (repeat_count > 1000) repeat_count = 1000;
            return;
        }
    }

    if (pending_len >= PENDING_MAX - 1) { reset_pending(); return; }
    pending[pending_len++] = c;
    pending[pending_len] = 0;

    uint16_t count = repeat_count > 0 ? repeat_count : 1;
    try_finalize_normal(count);
}

static void try_finalize_normal(uint16_t count) {
    if (pending_len == 1) {
        bool no_count = repeat_count == 0;
        switch (pending[0]) {
            case 'h': if (no_count) hold_motion(M_NONE, KEY_LEFT);  else move_left(count);  break;
            case 'j': if (no_count) hold_motion(M_NONE, KEY_DOWN);  else move_down(count);  break;
            case 'k': if (no_count) hold_motion(M_NONE, KEY_UP);    else move_up(count);    break;
            case 'l': if (no_count) hold_motion(M_NONE, KEY_RIGHT); else move_right(count); break;
            case 'w': case 'W': move_word_forward(count);  break;
            case 'b': case 'B': move_word_backward(count); break;
            case 'e': case 'E': move_word_end(count);      break;
            case '0': move_line_start();         break;
            case '^': move_line_start();         break;
            case '$': move_line_end();           break;
            case 'G':
                if (repeat_count > 0) {
                    // Nearest OS equivalent to "go to line N": just treat as doc end.
                    move_doc_end();
                } else {
                    move_doc_end();
                }
                break;

            case 'x': delete_char(count);        break;
            case 'X': delete_char_back(count);   break;
            case 'D': delete_to_line_end();      break;

            case 'p': paste(count);              break;
            case 'u': undo(count);               break;

            case 'i': enter_insert();            break;
            case 'a': move_right(1); enter_insert();       break;
            case 'I': move_line_start(); enter_insert();   break;
            case 'A': move_line_end(); enter_insert();     break;
            case 'o': open_line_below(); enter_insert();   break;
            case 'O': open_line_above(); enter_insert();   break;
            case 's': delete_char(count); enter_insert();  break;
            case 'S': delete_line_contents(); enter_insert(); break;
            case 'C': delete_to_line_end(); enter_insert();   break;

            case 'v': enter_visual();            break;

            case 'r': waiting_trigger = 'r'; return;
            case 'f': waiting_trigger = 'f'; return;
            case 'F': waiting_trigger = 'F'; return;
            case 't': waiting_trigger = 't'; return;
            case 'T': waiting_trigger = 'T'; return;

            case 'd': case 'c': case 'y': case 'g':
                return;  // wait for second char

            default: break;
        }
        reset_pending();
        return;
    }

    if (pending_len == 2) {
        char a = pending[0], b = pending[1];
        if (a == 'd') {
            switch (b) {
                case 'd': delete_lines(count);       break;
                case 'w': delete_word(count);        break;
                case 'e': delete_word(count);        break;  // approx
                case 'b': delete_word_back(count);   break;
                case '0': delete_to_line_start();    break;
                case '$': delete_to_line_end();      break;
                default: break;
            }
            reset_pending();
            return;
        }
        if (a == 'c') {
            switch (b) {
                case 'c': delete_line_contents(); enter_insert(); break;
                case 'w': delete_word(count); enter_insert();     break;
                case 'e': delete_word(count); enter_insert();     break;
                case 'b': delete_word_back(count); enter_insert(); break;
                case '0': delete_to_line_start(); enter_insert(); break;
                default: break;
            }
            reset_pending();
            return;
        }
        if (a == 'y') {
            switch (b) {
                case 'y': yank_lines(count);       break;
                case 'w': yank_word(count);        break;
                case 'e': yank_word(count);        break;
                case '0': yank_to_line_start();    break;
                case '$': yank_to_line_end();      break;
                default: break;
            }
            reset_pending();
            return;
        }
        if (a == 'g' && b == 'g') { move_doc_start(); reset_pending(); return; }
        reset_pending();
        return;
    }

    reset_pending();
}

static void handle_visual(char c) {
    if (pending_len == 0 && c >= '1' && c <= '9') {
        repeat_count = repeat_count * 10 + (c - '0');
        if (repeat_count > 1000) repeat_count = 1000;
        return;
    }
    if (pending_len == 0 && c == '0' && repeat_count > 0) {
        repeat_count *= 10;
        return;
    }

    if (pending_len >= PENDING_MAX - 1) { reset_pending(); return; }
    pending[pending_len++] = c;
    pending[pending_len] = 0;

    uint16_t count = repeat_count > 0 ? repeat_count : 1;
    try_finalize_visual(count);
}

static void try_finalize_visual(uint16_t count) {
    if (pending_len == 1) {
        bool no_count = repeat_count == 0;
        switch (pending[0]) {
            case 'h': if (no_count) hold_motion(M_NONE, KEY_LEFT);  else move_left(count);  break;
            case 'j': if (no_count) hold_motion(M_NONE, KEY_DOWN);  else move_down(count);  break;
            case 'k': if (no_count) hold_motion(M_NONE, KEY_UP);    else move_up(count);    break;
            case 'l': if (no_count) hold_motion(M_NONE, KEY_RIGHT); else move_right(count); break;
            case 'w': case 'W': move_word_forward(count);  break;
            case 'b': case 'B': move_word_backward(count); break;
            case 'e': case 'E': move_word_end(count);      break;
            case '0': move_line_start();          break;
            case '^': move_line_start();          break;
            case '$': move_line_end();            break;
            case 'G': move_doc_end();             break;

            case 'y':
                emit_release(KEY_LEFT_SHIFT);
                copy_sel();
                mode = VIM_MODE_NORMAL;
                show_mode();
                break;
            case 'd': case 'x':
                emit_release(KEY_LEFT_SHIFT);
                cut_sel();
                mode = VIM_MODE_NORMAL;
                show_mode();
                break;

            case 'g': return;  // wait for 'gg'

            default: break;
        }
        reset_pending();
        return;
    }

    if (pending_len == 2 && pending[0] == 'g' && pending[1] == 'g') {
        move_doc_start();
        reset_pending();
        return;
    }

    reset_pending();
}


// --- Mode / activation -----------------------------------------------------

vim_mode_t vim_mode(void) { return mode; }

void vim_toggle(void) {
    if (mode == VIM_MODE_OFF) {
        mode = VIM_MODE_NORMAL;
    } else {
        // Tear down whatever state we're in: release held shift if visual,
        // release any held motion, drop parsing state, mark off.
        release_held();
        if (mode == VIM_MODE_VISUAL) emit_release(KEY_LEFT_SHIFT);
        mode = VIM_MODE_OFF;
    }
    reset_pending();
    show_mode();
}


// --- Main entry point ------------------------------------------------------

void vim_enqueue(engine_event_t event) {
    bool is_press   = event.type == ENGINE_PRESS_KEY_EVENT;
    bool is_release = event.type == ENGINE_RELEASE_KEY_EVENT;
    bool is_mod     = (is_press || is_release)
                      && event.data.keycode >= 0xE0
                      && event.data.keycode <= 0xE7;

    // Track physical modifier state always, even when off.
    if (is_mod) {
        uint8_t bit = 1u << (event.data.keycode - 0xE0);
        if (is_press) phys_mods |= bit;
        else          phys_mods &= ~bit;
    }

    // Held-motion release: if this event is the release of the physical key
    // that started an OS-level auto-repeat, tear it down regardless of mode.
    if (is_release && held_input_kc != 0 && event.data.keycode == held_input_kc) {
        release_held();
        return;
    }

    if (mode == VIM_MODE_OFF) {
        enqueue_out(event);
        return;
    }

    if (mode == VIM_MODE_INSERT) {
        if (is_press && event.data.keycode == KEY_ESCAPE) {
            mode = VIM_MODE_NORMAL;
            reset_pending();
            show_mode();
            return;
        }
        enqueue_out(event);
        return;
    }

    // NORMAL or VISUAL: swallow releases outright.
    if (!is_press) return;

    if (event.data.keycode == KEY_ESCAPE) {
        if (mode == VIM_MODE_VISUAL) exit_visual_to_normal();
        reset_pending();
        show_mode();
        return;
    }

    // Modifier-only press: track (done above) and drop.
    if (is_mod) return;

    // Character-waiting commands consume any typeable press.
    if (waiting_trigger) {
        handle_char_arg(event);
        return;
    }

    bool shift = phys_mods & ((1u << 1) | (1u << 5));
    char c = keycode_to_char(event.data.keycode, shift);
    if (c == 0) return;  // unmapped: drop in NORMAL/VISUAL

    current_input_kc = event.data.keycode;
    if (mode == VIM_MODE_NORMAL) handle_normal(c);
    else                          handle_visual(c);
}
