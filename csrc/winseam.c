// winseam.c: the seam's bodies (winseam.h says what it is). The ring and the headless window are the same on every
// leg. Natively a GLFW window fills the ring from its callbacks; on the web a canvas's listeners, in the package's JS
// library (js/winseam.js), queue records that `winseam_pump` pulls into the ring (AGENTS.window.md item 8).
#include "winseam.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef __EMSCRIPTEN__
#include <GLFW/glfw3.h>
#if defined(_WIN32)
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif
#endif

#define RING 256
#define POINTERS 16   // pointers holding a button at once (AGENTS.window.md item 8, decision 2.3)

// A pointer holding a button: what it holds, and the event it was last seen in, which the end the window makes for it
// at a focus change carries.
typedef struct held_pointer {
    winseam_event last;
    int32_t held;   // a bit per button
} held_pointer;

// A pad slot as one window sees it (WIN-4): the latest sample, and what the consumer has seen of it.
typedef struct pad_view {
    winseam_pad raw;    // the latest sample, the system's or a post's
    int32_t known;      // the consumer has seen it connect, and not disconnect: the ring keeps a slot for its disconnect
    uint32_t held;      // a bit per button the consumer has seen go down and not up
    int32_t stale;      // released since its last sample: it reads neutral until the next one
    int32_t rumbling;   // the rumbles this window started on it, stopped when its pads close: 1 dual, 2 trigger
} pad_view;

// A button is pressed at 30 of 255 of its travel or more, on every backend (WIN-4, decision 2a): XInput's trigger
// threshold, and Chromium's for an XInput pad.
#define PAD_PRESSED_AT (30.0f / 255.0f)

struct winseam_window;
static void stop_rumbles(struct winseam_window *w);   // the system's pads, below

struct winseam_window {
    winseam_event ring[RING];
    uint32_t head;    // the oldest unread record
    uint32_t count;
    int32_t width;    // logical
    int32_t height;
    int32_t fb_width; // pixels
    int32_t fb_height;
    float scale;
    int32_t cursor_mode;
    int32_t have_last;
    double last_x;
    double last_y;
    int32_t holds;    // the Window's own, and one for each NativeWindow on it: the record goes with the last
    int32_t closed;   // the Window was dropped: hidden, no more events, while a handle still holds the record
    uint8_t held_keys[32];   // a bit per HID id the consumer has seen go down and not up; Unknown (0) is never held
    held_pointer pointers[POINTERS];   // each pointer holding a button, the same way, in the order they went down
    int32_t pointer_count;
    int32_t held_count;      // keys, each pointer's buttons and each pad's, held, and each pad known: the ring keeps a
                             // slot for each one's end
    int32_t lock_bits;       // CapsLock and NumLock, as the last key or button event reported them
    pad_view pads[WINSEAM_PADS];
    int32_t focused;         // the window has the focus, as its last Focus record said (a headless window starts with it)
    int32_t pads_unfocused;  // pads are read while the window is unfocused
    struct winseam_window *next_open;   // the open system windows, which the system's pads reach, newest first
    char *selector;         // on the web, the canvas's selector, which the native handle points at; NULL otherwise
    int32_t canvas;          // on the web, the JS library's index for the window
#ifndef __EMSCRIPTEN__
    GLFWwindow *glfw;
#endif
};

// Records alive, for the tests: every window's, and each closed one a handle still holds.
static int32_t g_live_records = 0;

// A new record, held once, by its Window.
static winseam_window *new_record(void) {
    winseam_window *w = (winseam_window *)calloc(1, sizeof *w);
    if (w == NULL) return NULL;
    w->holds = 1;
    g_live_records += 1;
    return w;
}

static void free_record(winseam_window *w) {
    free(w);
    g_live_records -= 1;
}

static int32_t copy_text(const char *text, uint8_t *dst, int32_t capacity) {
    if (text == NULL) text = "no reason given";
    size_t n = strlen(text);
    if ((int64_t)n > (int64_t)capacity) n = (size_t)capacity;
    memcpy(dst, text, n);
    return (int32_t)n;
}

// ---- the ring ------------------------------------------------------------------------------------------------------

static winseam_event *newest(winseam_window *w) {
    return w->count > 0 ? &w->ring[(w->head + w->count - 1) % RING] : NULL;
}

// `n` events counted into the drop: into the trailing Dropped record, or a new one in the slot kept for it.
static void count_drop(winseam_window *w, int32_t n) {
    winseam_event *last = newest(w);
    if (last != NULL && last->kind == WINSEAM_DROPPED) { last->a += n; return; }
    winseam_event d;
    memset(&d, 0, sizeof d);
    d.kind = WINSEAM_DROPPED;
    d.a = n;
    w->ring[(w->head + w->count) % RING] = d;
    w->count += 1;
}

// Coalesce into the newest unread record when it is the same kind of continuous change (the same pointer's move, a
// scroll in the same unit); otherwise append.
//
// Slots are kept: one for each up the ring owes once `e` is in (`owed`, the keys and buttons then held), and one for a
// Dropped record. So when the ring fills, what follows is counted into the drop, in order, rather than lost, and an up
// the ring owes always has its slot. An event is counted into a trailing Dropped record only while no other slot is
// free: once the reader has made room, it goes after the drop (counting it in regardless lost a key up that arrived
// then; found by kama's review, 2026-11-08). An owed up (`kept`) that finds only its own kept slot left, which happens
// only while the ring overflows, goes before the trailing Dropped record, which stays last and goes on counting.
//
// 1 when `e` was stored or coalesced, 0 when it was counted into a drop.
static int store(winseam_window *w, const winseam_event *e, int32_t owed, int kept) {
    winseam_event *last = newest(w);
    if (last != NULL && last->kind != WINSEAM_DROPPED && last->kind == e->kind) {
        // One pointer's moves: the newer sample, with the motion summed (AGENTS.window.md item 8, decision 2.4).
        if (e->kind == WINSEAM_POINTER_MOVE && last->pointer == e->pointer && last->pointer_kind == e->pointer_kind) {
            double dx = last->dx + e->dx, dy = last->dy + e->dy;
            *last = *e;
            last->dx = dx; last->dy = dy;
            return 1;
        }
        if (e->kind == WINSEAM_SCROLL && last->a == e->a) { last->dx += e->dx; last->dy += e->dy; return 1; }
        if (e->kind == WINSEAM_RESIZE || e->kind == WINSEAM_SCALE_CHANGED) { *last = *e; return 1; }
    }
    if ((int64_t)w->count + 1 + owed <= RING - 1) {
        w->ring[(w->head + w->count) % RING] = *e;
        w->count += 1;
        return 1;
    }
    if (kept && last != NULL && last->kind == WINSEAM_DROPPED && w->count < RING) {
        w->ring[(w->head + w->count) % RING] = *last;
        *last = *e;
        w->count += 1;
        return 1;
    }
    count_drop(w, 1);
    return 0;
}

// ---- what is held (AGENTS.window.md item 5, WIN-2) -------------------------------------------------------------------

// A key the window can hold: a HID id it can tell apart. Key::Unknown (0) names every key the table does not, so its
// downs and ups pass as the system sends them.
static int holdable(int32_t hid) { return hid > 0 && hid < 256; }

static int key_held(const winseam_window *w, int32_t hid) {
    return holdable(hid) && ((w->held_keys[hid >> 3] >> (hid & 7)) & 1);
}

static void hold_key(winseam_window *w, int32_t hid, int held) {
    if (key_held(w, hid) == held) return;
    w->held_keys[hid >> 3] ^= (uint8_t)(1u << (hid & 7));
    w->held_count += held ? 1 : -1;
}

// The pointer `e` is from, when it holds a button. Pointers are told apart by id and kind together.
static held_pointer *held_pointer_of(winseam_window *w, const winseam_event *e) {
    for (int32_t i = 0; i < w->pointer_count; i++) {
        held_pointer *h = &w->pointers[i];
        if (h->last.pointer == e->pointer && h->last.pointer_kind == e->pointer_kind) return h;
    }
    return NULL;
}

// A pointer that holds nothing now, out of the table, which keeps the order the others went down in.
static void forget_pointer(winseam_window *w, held_pointer *h) {
    int32_t i = (int32_t)(h - w->pointers);
    memmove(&w->pointers[i], &w->pointers[i + 1], (size_t)(w->pointer_count - i - 1) * sizeof *h);
    w->pointer_count -= 1;
}

static int32_t bits_in(int32_t bits) {
    int32_t n = 0;
    for (; bits != 0; bits &= bits - 1) n += 1;
    return n;
}

// What the window says a pointer holds after an event, and a mouse's pressure from it: 0.5 while a button is held, the
// W3C's rule for a device that reports none.
static void set_buttons(winseam_event *e, int32_t held) {
    e->buttons = held;
    if (e->pointer_kind == WINSEAM_POINTER_MOUSE) e->pressure = held != 0 ? 0.5f : 0.0f;
}

// The modifier bits of the keys held now, with the lock bits last reported: what an up the window makes carries.
static int32_t held_mods(const winseam_window *w) {
    int32_t m = w->lock_bits;
    if (key_held(w, 0xE1) || key_held(w, 0xE5)) m |= 1;   // ShiftLeft, ShiftRight
    if (key_held(w, 0xE0) || key_held(w, 0xE4)) m |= 2;   // ControlLeft, ControlRight
    if (key_held(w, 0xE2) || key_held(w, 0xE6)) m |= 4;   // AltLeft, AltRight
    if (key_held(w, 0xE3) || key_held(w, 0xE7)) m |= 8;   // MetaLeft, MetaRight: Super
    return m;
}

// ---- gamepads (WIN-4) ------------------------------------------------------------------------------------------------

static int slot_ok(int32_t slot) { return slot >= 0 && slot < WINSEAM_PADS; }

// Whether a pad's buttons and sticks reach the consumer now: while the window has the focus, or always when the program
// asked for pads in the background (decision 5).
static int pads_open(const winseam_window *w) { return w->focused || w->pads_unfocused; }

static uint32_t pressed_of(const winseam_pad *p) {
    uint32_t bits = 0;
    for (int32_t b = 0; b < WINSEAM_PAD_BUTTONS; b++) {
        if (p->buttons[b] >= PAD_PRESSED_AT) bits |= 1u << b;
    }
    return bits;
}

static void pad_record(winseam_event *e, int32_t kind, int32_t slot, int32_t button) {
    memset(e, 0, sizeof *e);
    e->kind = kind; e->a = slot; e->b = button;
}

// An up for each button the pad in `slot` holds, by number; the ring owes each. The pad then reads neutral until its
// next sample.
static void release_pad(winseam_window *w, int32_t slot) {
    pad_view *v = &w->pads[slot];
    winseam_event e;
    for (int32_t b = 0; b < WINSEAM_PAD_BUTTONS; b++) {
        uint32_t bit = 1u << b;
        if ((v->held & bit) == 0) continue;
        v->held &= ~bit;
        w->held_count -= 1;
        pad_record(&e, WINSEAM_PAD_UP, slot, b);
        store(w, &e, w->held_count, 1);
    }
    v->stale = 1;
}

static void release_pads(winseam_window *w) {
    for (int32_t slot = 0; slot < WINSEAM_PADS; slot++) {
        if (w->pads[slot].known) release_pad(w, slot);
    }
}

// A sample of the pad in `slot`, compared with what the consumer has seen (decision 2):
// - a pad not seen before connects; one gone releases what it held, then disconnects;
// - then each button no longer pressed goes up, then each newly pressed goes down, each by number;
// - while the window is closed to pads (decision 5) no button is pressed;
// - a connect or a down counted into a drop was never seen, so the next sample makes it again.
static void pad_take(winseam_window *w, int32_t slot, const winseam_pad *s) {
    if (!slot_ok(slot)) return;
    pad_view *v = &w->pads[slot];
    winseam_event e;
    if (!s->connected) {
        memset(&v->raw, 0, sizeof v->raw);
        v->stale = 0;
        if (!v->known) return;
        release_pad(w, slot);
        v->stale = 0;
        v->known = 0;
        v->rumbling = 0;
        w->held_count -= 1;
        pad_record(&e, WINSEAM_PAD_DISCONNECTED, slot, 0);
        store(w, &e, w->held_count, 1);
        return;
    }
    v->raw = *s;
    v->stale = 0;
    if (!v->known) {
        pad_record(&e, WINSEAM_PAD_CONNECTED, slot, 0);
        if (!store(w, &e, w->held_count + 1, 0)) return;
        v->known = 1;
        w->held_count += 1;
    }
    uint32_t want = pads_open(w) ? pressed_of(s) : 0;
    for (int32_t b = 0; b < WINSEAM_PAD_BUTTONS; b++) {
        uint32_t bit = 1u << b;
        if ((v->held & bit) == 0 || (want & bit) != 0) continue;
        v->held &= ~bit;
        w->held_count -= 1;
        pad_record(&e, WINSEAM_PAD_UP, slot, b);
        store(w, &e, w->held_count, 1);
    }
    for (int32_t b = 0; b < WINSEAM_PAD_BUTTONS; b++) {
        uint32_t bit = 1u << b;
        if ((v->held & bit) != 0 || (want & bit) == 0) continue;
        pad_record(&e, WINSEAM_PAD_DOWN, slot, b);
        if (!store(w, &e, w->held_count + 1, 0)) continue;
        v->held |= bit;
        w->held_count += 1;
    }
}

void winseam_pad_sample(winseam_window *w, int32_t slot, const winseam_pad *sample) { pad_take(w, slot, sample); }

void winseam_pad_post(winseam_window *w, int32_t slot, int32_t connected, const float *buttons, const float *axes) {
    winseam_pad s;
    memset(&s, 0, sizeof s);
    s.connected = connected ? 1 : 0;
    if (s.connected) {
        memcpy(s.buttons, buttons, sizeof s.buttons);
        memcpy(s.axes, axes, sizeof s.axes);
    }
    pad_take(w, slot, &s);
}

void winseam_pad_read(winseam_window *w, int32_t slot, winseam_pad *into) {
    memset(into, 0, sizeof *into);
    if (!slot_ok(slot)) return;
    const pad_view *v = &w->pads[slot];
    if (!v->known) return;
    into->connected = 1;
    if (!pads_open(w) || v->stale) return;
    memcpy(into->buttons, v->raw.buttons, sizeof into->buttons);
    memcpy(into->axes, v->raw.axes, sizeof into->axes);
}

void winseam_set_pads_when_unfocused(winseam_window *w, int32_t on) {
    w->pads_unfocused = on ? 1 : 0;
    if (!pads_open(w)) { stop_rumbles(w); release_pads(w); }
}

// A posted pad event moves the slot's pad as the system would: a down presses its button fully, an up lets it go, a
// connect brings a pad at rest, a disconnect takes it away.
static void post_pad(winseam_window *w, const winseam_event *e) {
    if (!slot_ok(e->a)) return;
    pad_view *v = &w->pads[e->a];
    winseam_pad s = v->raw;
    int pressing = e->kind == WINSEAM_PAD_DOWN;
    if (e->kind == WINSEAM_PAD_DISCONNECTED) { memset(&s, 0, sizeof s); }
    else if (e->kind == WINSEAM_PAD_CONNECTED) { if (!s.connected) { memset(&s, 0, sizeof s); s.connected = 1; } }
    else {
        if (e->b < 0 || e->b >= WINSEAM_PAD_BUTTONS) return;
        if (!pressing && !s.connected) return;
        s.connected = 1;
        s.buttons[e->b] = pressing ? 1.0f : 0.0f;
    }
    pad_take(w, e->a, &s);
}

// The end of every pointer holding a button, in the order they went down: a mouse's buttons go up, by number, and a
// touch or pen contact is cancelled, since it never lifted and a tap must not fire (AGENTS.window.md item 8, decision
// 2.3). Each is at the pointer's last position. Then, when `pads`, an up for each button a pad holds, by slot and number
// (WIN-4, decision 6). Then an up for every held key by ascending HID id, so the modifiers (224 to 231) go last: the
// action goes up before the modifier that chose it. Each up carries the modifiers still held after it. The ring owes
// each of them, so none is counted into a drop.
static void release_held(winseam_window *w, int pads) {
    winseam_event e;
    while (w->pointer_count > 0) {
        held_pointer *h = &w->pointers[0];
        if (h->last.pointer_kind == WINSEAM_POINTER_MOUSE) {
            for (int32_t b = 0; b < WINSEAM_BUTTONS; b++) {
                if (((h->held >> b) & 1) == 0) continue;
                h->held &= ~(1 << b);
                w->held_count -= 1;
                e = h->last;
                e.kind = WINSEAM_POINTER_UP; e.a = b; e.b = held_mods(w); e.c = 0; e.dx = 0; e.dy = 0;
                set_buttons(&e, h->held);
                store(w, &e, w->held_count, 1);
            }
        } else {
            w->held_count -= bits_in(h->held);
            e = h->last;
            e.kind = WINSEAM_POINTER_CANCEL; e.a = 0; e.b = 0; e.c = 0; e.dx = 0; e.dy = 0;
            set_buttons(&e, 0);
            e.pressure = 0.0f;
            store(w, &e, w->held_count, 1);
        }
        forget_pointer(w, h);
    }
    if (pads) release_pads(w);
    for (int32_t hid = 1; hid < 256; hid++) {
        if (!key_held(w, hid)) continue;
        hold_key(w, hid, 0);
        memset(&e, 0, sizeof e);
        e.kind = WINSEAM_KEY_UP; e.a = hid; e.b = held_mods(w);
        e.x = w->last_x; e.y = w->last_y;
        store(w, &e, w->held_count, 1);
    }
}

void winseam_release_held(winseam_window *w) { release_held(w, 1); }

// A pointer's event: its buttons held per pointer, as keys are (AGENTS.window.md item 8, decision 2).
// - An up for a button the pointer does not hold is dropped, and so is a second down for one it does, or for a button
//   no table names.
// - A down from a pointer holding nothing, while 16 others hold something, is counted into the drop.
// - A cancel ends what the pointer holds, with no ups; one for a pointer holding nothing is dropped.
// - Every event carries what the pointer holds after it.
static void take_pointer(winseam_window *w, const winseam_event *e) {
    winseam_event p = *e;
    held_pointer *h = held_pointer_of(w, e);
    int32_t held = h != NULL ? h->held : 0;
    if (p.kind == WINSEAM_POINTER_DOWN || p.kind == WINSEAM_POINTER_UP) {
        if (p.a < 0 || p.a >= WINSEAM_BUTTONS) return;
        int32_t bit = 1 << p.a;
        if (p.kind == WINSEAM_POINTER_DOWN) {
            if ((held & bit) != 0) return;
            if (h == NULL && w->pointer_count == POINTERS) { count_drop(w, 1); return; }
            set_buttons(&p, held | bit);
            if (!store(w, &p, w->held_count + 1, 0)) return;   // counted into the drop: not held
            if (h == NULL) {
                h = &w->pointers[w->pointer_count];
                w->pointer_count += 1;
                h->held = 0;
            }
            h->held |= bit;
            h->last = p;
            w->held_count += 1;
            return;
        }
        if ((held & bit) == 0) return;
        h->held &= ~bit;
        w->held_count -= 1;
        set_buttons(&p, h->held);
        h->last = p;
        if (h->held == 0) forget_pointer(w, h);
        store(w, &p, w->held_count, 1);
        return;
    }
    if (p.kind == WINSEAM_POINTER_CANCEL) {
        if (h == NULL) return;
        w->held_count -= bits_in(h->held);
        forget_pointer(w, h);
        set_buttons(&p, 0);
        store(w, &p, w->held_count, 1);
        return;
    }
    set_buttons(&p, held);   // a move, an enter or a leave
    if (h != NULL) h->last = p;
    store(w, &p, w->held_count, 0);
}

// Every event the window takes, from the system's callbacks and from `post`, comes through here: the held set's rules,
// then the ring.
// - An up for a key or button not held is dropped.
// - A key's down is a press when the key is not held, and holds it; a repeat when it is, whatever the system said. A
//   down that is counted into a drop is not held: the consumer never saw it.
// - A pointer's buttons, as `take_pointer` says.
// - A focus change, either way, releases everything held, then gives back a lock a loss undoes, then says Focus.
static void take(winseam_window *w, const winseam_event *e) {
    if (e->kind == WINSEAM_KEY_DOWN || e->kind == WINSEAM_KEY_UP || e->kind == WINSEAM_POINTER_DOWN ||
        e->kind == WINSEAM_POINTER_UP) {
        w->lock_bits = e->b & (16 | 32);   // CapsLock, NumLock
    }
    if ((e->kind == WINSEAM_KEY_DOWN || e->kind == WINSEAM_KEY_UP) && holdable(e->a)) {
        int32_t hid = e->a;
        winseam_event k = *e;
        if (e->kind == WINSEAM_KEY_UP) {
            if (!key_held(w, hid)) return;
            hold_key(w, hid, 0);
            store(w, &k, w->held_count, 1);
            return;
        }
        if (key_held(w, hid)) { k.c = 1; store(w, &k, w->held_count, 0); return; }
        k.c = 0;
        if (store(w, &k, w->held_count + 1, 0)) hold_key(w, hid, 1);
        return;
    }
    if (e->kind >= WINSEAM_POINTER_MOVE && e->kind <= WINSEAM_POINTER_LEAVE) {
        take_pointer(w, e);
        return;
    }
    if (e->kind == WINSEAM_FOCUS) {
        w->focused = e->a ? 1 : 0;
        if (!w->pads_unfocused && !e->a) stop_rumbles(w);
        release_held(w, !w->pads_unfocused);
        // Losing the focus while locked gives the pointer back, as a browser does on blur; clicking in locks it again.
        if (!e->a && w->cursor_mode == 2) winseam_set_cursor(w, 0);
    }
    store(w, e, w->held_count, 0);
}

static void push(winseam_window *w, const winseam_event *e) { take(w, e); }

static void push_simple(winseam_window *w, int32_t kind, int32_t a, int32_t b, int32_t c) {
    winseam_event e;
    memset(&e, 0, sizeof e);
    e.kind = kind; e.a = a; e.b = b; e.c = c;
    e.x = w->last_x; e.y = w->last_y;
    push(w, &e);
}

int32_t winseam_next(winseam_window *w, winseam_event *out) {
    if (w->count == 0) return 0;
    *out = w->ring[w->head];
    w->head = (w->head + 1) % RING;
    w->count -= 1;
    return 1;
}

void winseam_post(winseam_window *w, const winseam_event *e) {
    if (e->kind >= WINSEAM_PAD_CONNECTED && e->kind <= WINSEAM_PAD_UP) { post_pad(w, e); return; }
    if (e->kind == WINSEAM_RESIZE) { w->width = e->a; w->height = e->b; w->fb_width = e->a; w->fb_height = e->b; }
    push(w, e);
}

winseam_window *winseam_headless(int32_t width, int32_t height) {
    winseam_window *w = new_record();
    if (w == NULL) return NULL;
    w->focused = 1;
    w->width = width; w->height = height;
    w->fb_width = width; w->fb_height = height;
    w->scale = 1.0f;
    return w;
}

int32_t winseam_headless_p(winseam_window *w) {
#ifndef __EMSCRIPTEN__
    return w->glfw == NULL;
#else
    return w->selector == NULL;
#endif
}

void winseam_size(winseam_window *w, int32_t *width, int32_t *height) { *width = w->width; *height = w->height; }
void winseam_framebuffer_size(winseam_window *w, int32_t *width, int32_t *height) {
    *width = w->fb_width; *height = w->fb_height;
}
float winseam_scale(winseam_window *w) { return w->scale; }

// ---- the system's pads (WIN-4, decision 1), on every leg ---------------------------------------------------------------

// The system's pads, one process-wide table: each pump of a system window reads the system into it and hands every
// slot's sample to every open system window. What the system says of each pad comes with it.
typedef struct system_pad {
    winseam_pad sample;
    char name[128];
    char serial[64];
    char id[128];
    int32_t vendor;
    int32_t product;
    int32_t caps;          // WINSEAM_PAD_RUMBLE, WINSEAM_PAD_TRIGGER_RUMBLE
    int32_t mapped;        // a mapping knows it: its standard view is read; 0, read only raw
    int32_t raw_counts[3]; // buttons, axes, hats, as the system numbers them
    uint8_t raw_buttons[WINSEAM_PAD_RAW_BUTTONS];
    float raw_axes[WINSEAM_PAD_RAW_AXES];
    uint8_t raw_hats[WINSEAM_PAD_RAW_HATS];
    float rumble[4];       // a test's table: the last rumble asked for, strong, weak, left, right
    int32_t rumble_ms;
    int32_t rumbles;       // a test's table: how many were asked for since the slot was set
} system_pad;

static system_pad g_system[WINSEAM_PADS];
static int32_t g_from_system = 1;          // the system fills the table; 0 while a test or a replay does (decision 8)
static winseam_window *g_open = NULL;      // the open system windows

static void open_add(winseam_window *w) { w->next_open = g_open; g_open = w; }

static void open_remove(winseam_window *w) {
    for (winseam_window **p = &g_open; *p != NULL; p = &(*p)->next_open) {
        if (*p == w) { *p = w->next_open; w->next_open = NULL; return; }
    }
}

static void copy_string(char *dst, size_t capacity, const char *src) {
    if (src == NULL) src = "";
    size_t n = strlen(src);
    if (n >= capacity) n = capacity - 1;
    memcpy(dst, src, n);
    dst[n] = 0;
}

static void system_slot(int32_t slot, const winseam_pad *sample, const char *name, int32_t vendor, int32_t product,
                        const char *serial, const char *id, int32_t caps) {
    system_pad *p = &g_system[slot];
    memset(p, 0, sizeof *p);
    p->caps = caps;
    p->sample = *sample;
    copy_string(p->name, sizeof p->name, name);
    copy_string(p->serial, sizeof p->serial, serial);
    copy_string(p->id, sizeof p->id, id);
    p->vendor = vendor;
    p->product = product;
}

// Every slot's sample to every open system window.
static void deliver_system_pads(void) {
    for (winseam_window *w = g_open; w != NULL; w = w->next_open) {
        for (int32_t slot = 0; slot < WINSEAM_PADS; slot++) pad_take(w, slot, &g_system[slot].sample);
    }
}

void winseam_pads_from_system(int32_t on) {
    g_from_system = on ? 1 : 0;
    if (on) memset(g_system, 0, sizeof g_system);
}

void winseam_system_pad(int32_t slot, int32_t connected, const float *buttons, const float *axes, const char *name,
                        int32_t vendor, int32_t product, const char *serial, const char *id, int32_t caps) {
    if (!slot_ok(slot)) return;
    winseam_pad s;
    memset(&s, 0, sizeof s);
    if (connected) {
        s.connected = 1;
        memcpy(s.buttons, buttons, sizeof s.buttons);
        memcpy(s.axes, axes, sizeof s.axes);
        system_slot(slot, &s, name, vendor, product, serial, id, caps);
        g_system[slot].mapped = 1;
    } else {
        system_slot(slot, &s, "", 0, 0, "", "", 0);
    }
}

int32_t winseam_system_pad_rumbled(int32_t slot, float *motors, int32_t *milliseconds) {
    if (!slot_ok(slot)) return 0;
    const system_pad *p = &g_system[slot];
    memcpy(motors, p->rumble, sizeof p->rumble);
    *milliseconds = p->rumble_ms;
    return p->rumbles;
}

// The system's word on the pad in `slot`, when `w` is a system window that knows a pad there.
static const system_pad *info_of(winseam_window *w, int32_t slot) {
    if (!slot_ok(slot) || winseam_headless_p(w) || !w->pads[slot].known) return NULL;
    return &g_system[slot];
}

int32_t winseam_pad_text(winseam_window *w, int32_t slot, int32_t which, uint8_t *into, int32_t capacity) {
    const system_pad *p = info_of(w, slot);
    if (p == NULL) return -1;
    const char *s = which == 0 ? p->name : which == 1 ? p->serial : p->id;
    size_t n = strlen(s);
    if ((int64_t)n > (int64_t)capacity) n = (size_t)capacity;
    memcpy(into, s, n);
    return (int32_t)n;
}

int32_t winseam_pad_number(winseam_window *w, int32_t slot, int32_t which) {
    const system_pad *p = info_of(w, slot);
    if (p == NULL) return -1;
    return which == 0 ? p->vendor : which == 1 ? p->product : which == 2 ? p->caps : p->mapped;
}

int32_t winseam_pad_raw(winseam_window *w, int32_t slot, int32_t *counts, uint8_t *buttons, float *axes, uint8_t *hats) {
    memset(counts, 0, 3 * sizeof *counts);
    memset(buttons, 0, WINSEAM_PAD_RAW_BUTTONS);
    memset(axes, 0, WINSEAM_PAD_RAW_AXES * sizeof *axes);
    memset(hats, 0, WINSEAM_PAD_RAW_HATS);
    const system_pad *p = info_of(w, slot);
    if (p == NULL) return 0;
    memcpy(counts, p->raw_counts, sizeof p->raw_counts);
    if (!pads_open(w) || w->pads[slot].stale) return 1;
    memcpy(buttons, p->raw_buttons, sizeof p->raw_buttons);
    memcpy(axes, p->raw_axes, sizeof p->raw_axes);
    memcpy(hats, p->raw_hats, sizeof p->raw_hats);
    return 1;
}

static float unit(float v) { return v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v; }

static int system_rumble(int32_t slot, float a, float b, int32_t milliseconds, int triggers);   // natively or on the web

// A rumble into the system's slot: a test's table records it; the system plays it. Refused for a motor the pad has not.
static int rumble_slot(int32_t slot, float a, float b, int32_t milliseconds, int triggers) {
    system_pad *p = &g_system[slot];
    if ((p->caps & (triggers ? WINSEAM_PAD_TRIGGER_RUMBLE : WINSEAM_PAD_RUMBLE)) == 0) return 0;
    if (!g_from_system) {
        p->rumble[triggers ? 2 : 0] = a;
        p->rumble[triggers ? 3 : 1] = b;
        p->rumble_ms = milliseconds;
        p->rumbles += 1;
        return 1;
    }
    return system_rumble(slot, a, b, milliseconds, triggers);
}

static int32_t pad_rumble(winseam_window *w, int32_t slot, float a, float b, int32_t milliseconds, int triggers) {
    if (!slot_ok(slot) || winseam_headless_p(w) || !w->pads[slot].known || !pads_open(w)) return 0;
    if (milliseconds < 0) milliseconds = 0;
    a = unit(a); b = unit(b);
    if (!rumble_slot(slot, a, b, milliseconds, triggers)) return 0;
    int32_t bit = triggers ? 2 : 1;
    if ((a > 0.0f || b > 0.0f) && milliseconds > 0) w->pads[slot].rumbling |= bit;
    else w->pads[slot].rumbling &= ~bit;
    return 1;
}

int32_t winseam_pad_rumble(winseam_window *w, int32_t slot, float strong, float weak, int32_t milliseconds) {
    return pad_rumble(w, slot, strong, weak, milliseconds, 0);
}

int32_t winseam_pad_rumble_triggers(winseam_window *w, int32_t slot, float left, float right, int32_t milliseconds) {
    return pad_rumble(w, slot, left, right, milliseconds, 1);
}

// Each rumble the window started, stopped: its pads have closed (decision 6).
static void stop_rumbles(winseam_window *w) {
    for (int32_t slot = 0; slot < WINSEAM_PADS; slot++) {
        pad_view *v = &w->pads[slot];
        if (v->rumbling & 1) rumble_slot(slot, 0.0f, 0.0f, 0, 0);
        if (v->rumbling & 2) rumble_slot(slot, 0.0f, 0.0f, 0, 1);
        v->rumbling = 0;
    }
}

#ifndef __EMSCRIPTEN__
// ---- natively: GLFW ------------------------------------------------------------------------------------------------

// glfwInit and glfwTerminate are process-wide, and terminate destroys every window: counted, so the last window's
// close is the one that terminates.
static int32_t g_glfw_users = 0;

#if defined(_WIN32)
// ---- natively, the system's pads: SDL3's gamepad subsystem, alone, beside GLFW's windows (WIN-4, decision 1) ----------
//
// SDL starts with the first system window and stops with the last, as GLFW does. No SDL window exists, so SDL's own
// background gate is inert, and the window decides what an unfocused pad reads (decision 5). On Windows SDL reads
// GameInput where Microsoft's runtime is present, and XInput where it is not (decision 9).
#include <SDL3/SDL.h>

extern int winseam_gameinput_usable(void);   // csrc/winseam_gameinput.cpp: SDL's own test

static int g_sdl = 0;
// Each slot's pad as SDL opened it: a gamepad, or, for a pad no mapping knows, its joystick alone (decision 3).
static SDL_Gamepad *g_sdl_pads[WINSEAM_PADS];
static SDL_Joystick *g_sdl_raw[WINSEAM_PADS];
static SDL_JoystickID g_sdl_ids[WINSEAM_PADS];

// SDL_GameControllerDB's Windows lines (third_party/sdl-db/gamecontrollerdb.c, written by tools/vendor-sdl.sh).
extern const char winseam_gamecontrollerdb[];
extern const size_t winseam_gamecontrollerdb_size;

// The mappings a program added, kept so each start of SDL applies them again.
static char *g_added = NULL;
static size_t g_added_size = 0;

static int32_t sdl_add_mappings(const char *text, size_t size) {
    SDL_IOStream *io = SDL_IOFromConstMem(text, size);
    if (io == NULL) return 0;
    int n = SDL_AddGamepadMappingsFromIO(io, true);
    return n < 0 ? 0 : (int32_t)n;
}

static void sdl_start(void) {
    if (g_sdl) return;
    SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");              // a pad library never takes the program's Ctrl+C
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1"); // the window decides what an unfocused pad reads
    if (winseam_gameinput_usable()) {
        SDL_SetHint(SDL_HINT_JOYSTICK_GAMEINPUT, "1");
        // Stopgap (WIN-23): with GameInput and XInput both on, SDL 3.4 adds a replugged pad twice, so XInput is turned
        // off while GameInput is usable, as SDL's main branch does inside SDL.
        SDL_SetHint(SDL_HINT_XINPUT_ENABLED, "0");
    }
    g_sdl = SDL_Init(SDL_INIT_GAMEPAD) ? 1 : 0;
    if (!g_sdl) return;
    sdl_add_mappings(winseam_gamecontrollerdb, winseam_gamecontrollerdb_size);
    if (g_added != NULL) sdl_add_mappings(g_added, g_added_size);
}

int32_t winseam_add_pad_mappings(const char *text, int32_t length) {
    if (length <= 0) return 0;
    int32_t took = g_sdl ? sdl_add_mappings(text, (size_t)length) : 0;
    if (g_sdl && took == 0) return 0;
    char *grown = (char *)realloc(g_added, g_added_size + (size_t)length + 1);
    if (grown == NULL) return took;
    memcpy(grown + g_added_size, text, (size_t)length);
    grown[g_added_size + (size_t)length] = '\n';
    g_added = grown;
    g_added_size += (size_t)length + 1;
    return took;
}

static SDL_Joystick *sdl_joystick(int32_t slot) {
    return g_sdl_pads[slot] != NULL ? SDL_GetGamepadJoystick(g_sdl_pads[slot]) : g_sdl_raw[slot];
}

static void sdl_close(int32_t slot) {
    if (g_sdl_pads[slot] != NULL) SDL_CloseGamepad(g_sdl_pads[slot]);
    if (g_sdl_raw[slot] != NULL) SDL_CloseJoystick(g_sdl_raw[slot]);
    g_sdl_pads[slot] = NULL;
    g_sdl_raw[slot] = NULL;
    g_sdl_ids[slot] = 0;
}

static void sdl_stop(void) {
    if (!g_sdl) return;
    for (int32_t slot = 0; slot < WINSEAM_PADS; slot++) sdl_close(slot);
    if (g_from_system) memset(g_system, 0, sizeof g_system);
    SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
    g_sdl = 0;
}

static int32_t sdl_slot_of(SDL_JoystickID id) {
    for (int32_t slot = 0; slot < WINSEAM_PADS; slot++) {
        if ((g_sdl_pads[slot] != NULL || g_sdl_raw[slot] != NULL) && g_sdl_ids[slot] == id) return slot;
    }
    return -1;
}

// A new pad takes the lowest free slot.
static int32_t sdl_free_slot(void) {
    for (int32_t slot = 0; slot < WINSEAM_PADS; slot++) {
        if (g_sdl_pads[slot] == NULL && g_sdl_raw[slot] == NULL) return slot;
    }
    return -1;
}

// What SDL says of the slot's pad, into the table: through its mapping when it has one, else its joystick's own word.
static void sdl_info(int32_t slot) {
    SDL_Joystick *j = sdl_joystick(slot);
    SDL_PropertiesID props = SDL_GetJoystickProperties(j);
    int32_t caps = (SDL_GetBooleanProperty(props, SDL_PROP_JOYSTICK_CAP_RUMBLE_BOOLEAN, false) ? WINSEAM_PAD_RUMBLE : 0) |
                   (SDL_GetBooleanProperty(props, SDL_PROP_JOYSTICK_CAP_TRIGGER_RUMBLE_BOOLEAN, false) ? WINSEAM_PAD_TRIGGER_RUMBLE : 0);
    winseam_pad s;
    memset(&s, 0, sizeof s);
    s.connected = 1;
    SDL_Gamepad *g = g_sdl_pads[slot];
    system_slot(slot, &s, g != NULL ? SDL_GetGamepadName(g) : SDL_GetJoystickName(j), SDL_GetJoystickVendor(j),
                SDL_GetJoystickProduct(j), SDL_GetJoystickSerial(j), SDL_GetJoystickPath(j), caps);
    g_system[slot].mapped = g != NULL ? 1 : 0;
}

// A pad SDL maps: a new one takes the lowest free slot; one read raw until now is mapped in its slot.
static void sdl_add(SDL_JoystickID id) {
    int32_t slot = sdl_slot_of(id);
    if (slot >= 0 && g_sdl_pads[slot] != NULL) return;
    SDL_Gamepad *g = SDL_OpenGamepad(id);
    if (g == NULL) return;
    if (slot >= 0) {
        SDL_CloseJoystick(g_sdl_raw[slot]);
        g_sdl_raw[slot] = NULL;
    } else {
        slot = sdl_free_slot();
        if (slot < 0) { SDL_CloseGamepad(g); return; }
    }
    g_sdl_pads[slot] = g;
    g_sdl_ids[slot] = id;
    sdl_info(slot);
}

// A pad no mapping knows: read raw, in the lowest free slot.
static void sdl_add_raw(SDL_JoystickID id) {
    if (SDL_IsGamepad(id) || sdl_slot_of(id) >= 0) return;
    int32_t slot = sdl_free_slot();
    if (slot < 0) return;
    SDL_Joystick *j = SDL_OpenJoystick(id);
    if (j == NULL) return;
    g_sdl_raw[slot] = j;
    g_sdl_ids[slot] = id;
    sdl_info(slot);
}

static void sdl_remove(SDL_JoystickID id) {
    int32_t slot = sdl_slot_of(id);
    if (slot < 0) return;
    sdl_close(slot);
    memset(&g_system[slot], 0, sizeof g_system[slot]);
}

// SDL's -32768 to 32767, as -1 to 1.
static float sdl_axis(Sint16 v) {
    float f = (float)v / 32767.0f;
    return f < -1.0f ? -1.0f : f;
}

// The W3C's standard button order, as SDL names them; the triggers are SDL's axes.
static const SDL_GamepadButton k_sdl_buttons[WINSEAM_PAD_BUTTONS] = {
    SDL_GAMEPAD_BUTTON_SOUTH, SDL_GAMEPAD_BUTTON_EAST, SDL_GAMEPAD_BUTTON_WEST, SDL_GAMEPAD_BUTTON_NORTH,
    SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, SDL_GAMEPAD_BUTTON_INVALID,
    SDL_GAMEPAD_BUTTON_INVALID, SDL_GAMEPAD_BUTTON_BACK, SDL_GAMEPAD_BUTTON_START, SDL_GAMEPAD_BUTTON_LEFT_STICK,
    SDL_GAMEPAD_BUTTON_RIGHT_STICK, SDL_GAMEPAD_BUTTON_DPAD_UP, SDL_GAMEPAD_BUTTON_DPAD_DOWN,
    SDL_GAMEPAD_BUTTON_DPAD_LEFT, SDL_GAMEPAD_BUTTON_DPAD_RIGHT, SDL_GAMEPAD_BUTTON_GUIDE,
};

static void sdl_sample(SDL_Gamepad *g, winseam_pad *s) {
    memset(s, 0, sizeof *s);
    s->connected = 1;
    if (g == NULL) return;   // read only raw: the standard view stays at rest
    for (int32_t b = 0; b < WINSEAM_PAD_BUTTONS; b++) {
        if (k_sdl_buttons[b] == SDL_GAMEPAD_BUTTON_INVALID) continue;
        s->buttons[b] = SDL_GetGamepadButton(g, k_sdl_buttons[b]) ? 1.0f : 0.0f;
    }
    float lt = sdl_axis(SDL_GetGamepadAxis(g, SDL_GAMEPAD_AXIS_LEFT_TRIGGER));
    float rt = sdl_axis(SDL_GetGamepadAxis(g, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER));
    s->buttons[6] = lt < 0.0f ? 0.0f : lt;
    s->buttons[7] = rt < 0.0f ? 0.0f : rt;
    s->axes[0] = sdl_axis(SDL_GetGamepadAxis(g, SDL_GAMEPAD_AXIS_LEFTX));
    s->axes[1] = sdl_axis(SDL_GetGamepadAxis(g, SDL_GAMEPAD_AXIS_LEFTY));
    s->axes[2] = sdl_axis(SDL_GetGamepadAxis(g, SDL_GAMEPAD_AXIS_RIGHTX));
    s->axes[3] = sdl_axis(SDL_GetGamepadAxis(g, SDL_GAMEPAD_AXIS_RIGHTY));
}

// The joystick's own controls, as SDL numbers them.
static void sdl_sample_raw(SDL_Joystick *j, system_pad *p) {
    int32_t nb = SDL_GetNumJoystickButtons(j), na = SDL_GetNumJoystickAxes(j), nh = SDL_GetNumJoystickHats(j);
    if (nb < 0) nb = 0;
    if (na < 0) na = 0;
    if (nh < 0) nh = 0;
    if (nb > WINSEAM_PAD_RAW_BUTTONS) nb = WINSEAM_PAD_RAW_BUTTONS;
    if (na > WINSEAM_PAD_RAW_AXES) na = WINSEAM_PAD_RAW_AXES;
    if (nh > WINSEAM_PAD_RAW_HATS) nh = WINSEAM_PAD_RAW_HATS;
    p->raw_counts[0] = nb; p->raw_counts[1] = na; p->raw_counts[2] = nh;
    for (int32_t b = 0; b < nb; b++) p->raw_buttons[b] = SDL_GetJoystickButton(j, b) ? 1 : 0;
    for (int32_t a = 0; a < na; a++) p->raw_axes[a] = sdl_axis(SDL_GetJoystickAxis(j, a));
    for (int32_t h = 0; h < nh; h++) p->raw_hats[h] = SDL_GetJoystickHat(j, h);
}

// SDL's 0 to 65535, from 0 to 1.
static Uint16 sdl_motor(float v) { return (Uint16)(v * 65535.0f + 0.5f); }

static int system_rumble(int32_t slot, float a, float b, int32_t milliseconds, int triggers) {
    if (g_sdl_pads[slot] == NULL && g_sdl_raw[slot] == NULL) return 0;
    SDL_Joystick *j = sdl_joystick(slot);
    Uint32 ms = (Uint32)milliseconds;
    return triggers ? SDL_RumbleJoystickTriggers(j, sdl_motor(a), sdl_motor(b), ms) : SDL_RumbleJoystick(j, sdl_motor(a), sdl_motor(b), ms);
}

// SDL's arrivals, removals and remappings, then each pad's state, into the table.
static void read_system_pads(void) {
    if (!g_sdl || !g_from_system) return;
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_EVENT_GAMEPAD_ADDED) sdl_add(e.gdevice.which);
        else if (e.type == SDL_EVENT_JOYSTICK_ADDED) sdl_add_raw(e.jdevice.which);
        else if (e.type == SDL_EVENT_GAMEPAD_REMOVED || e.type == SDL_EVENT_JOYSTICK_REMOVED) sdl_remove(e.jdevice.which);
        else if (e.type == SDL_EVENT_GAMEPAD_REMAPPED) {
            int32_t slot = sdl_slot_of(e.gdevice.which);
            if (slot >= 0) sdl_info(slot);
        }
    }
    for (int32_t slot = 0; slot < WINSEAM_PADS; slot++) {
        if (g_sdl_pads[slot] == NULL && g_sdl_raw[slot] == NULL) continue;
        sdl_sample(g_sdl_pads[slot], &g_system[slot].sample);
        sdl_sample_raw(sdl_joystick(slot), &g_system[slot]);
    }
}
#else
// Linux and macOS: their SDL comes with WIN-12, each configured on its own system. Until then, no system pads.
static void sdl_start(void) { }
static void sdl_stop(void) { }
static void read_system_pads(void) { }
static int system_rumble(int32_t slot, float a, float b, int32_t milliseconds, int triggers) {
    (void)slot; (void)a; (void)b; (void)milliseconds; (void)triggers;
    return 0;
}
int32_t winseam_add_pad_mappings(const char *text, int32_t length) { (void)text; (void)length; return 0; }
#endif

static winseam_window *of(GLFWwindow *g) { return (winseam_window *)glfwGetWindowUserPointer(g); }

static int32_t mods_of(int mods) { return (int32_t)(mods & 0x3F); }   // GLFW's bits: shift, ctrl, alt, super, caps, num

// GLFW's key tokens name US-layout positions, the same physical keys the HID usage ids (and the DOM's `code`) name.
int32_t winseam_glfw_key_to_hid(int32_t k) {
    if (k >= GLFW_KEY_A && k <= GLFW_KEY_Z) return 0x04 + (k - GLFW_KEY_A);
    if (k >= GLFW_KEY_1 && k <= GLFW_KEY_9) return 0x1E + (k - GLFW_KEY_1);
    if (k >= GLFW_KEY_F1 && k <= GLFW_KEY_F12) return 0x3A + (k - GLFW_KEY_F1);
    if (k >= GLFW_KEY_F13 && k <= GLFW_KEY_F24) return 0x68 + (k - GLFW_KEY_F13);
    if (k >= GLFW_KEY_KP_1 && k <= GLFW_KEY_KP_9) return 0x59 + (k - GLFW_KEY_KP_1);
    switch (k) {
        case GLFW_KEY_0: return 0x27;
        case GLFW_KEY_ENTER: return 0x28;
        case GLFW_KEY_ESCAPE: return 0x29;
        case GLFW_KEY_BACKSPACE: return 0x2A;
        case GLFW_KEY_TAB: return 0x2B;
        case GLFW_KEY_SPACE: return 0x2C;
        case GLFW_KEY_MINUS: return 0x2D;
        case GLFW_KEY_EQUAL: return 0x2E;
        case GLFW_KEY_LEFT_BRACKET: return 0x2F;
        case GLFW_KEY_RIGHT_BRACKET: return 0x30;
        case GLFW_KEY_BACKSLASH: return 0x31;
        case GLFW_KEY_SEMICOLON: return 0x33;
        case GLFW_KEY_APOSTROPHE: return 0x34;
        case GLFW_KEY_GRAVE_ACCENT: return 0x35;
        case GLFW_KEY_COMMA: return 0x36;
        case GLFW_KEY_PERIOD: return 0x37;
        case GLFW_KEY_SLASH: return 0x38;
        case GLFW_KEY_CAPS_LOCK: return 0x39;
        case GLFW_KEY_PRINT_SCREEN: return 0x46;
        case GLFW_KEY_SCROLL_LOCK: return 0x47;
        case GLFW_KEY_PAUSE: return 0x48;
        case GLFW_KEY_INSERT: return 0x49;
        case GLFW_KEY_HOME: return 0x4A;
        case GLFW_KEY_PAGE_UP: return 0x4B;
        case GLFW_KEY_DELETE: return 0x4C;
        case GLFW_KEY_END: return 0x4D;
        case GLFW_KEY_PAGE_DOWN: return 0x4E;
        case GLFW_KEY_RIGHT: return 0x4F;
        case GLFW_KEY_LEFT: return 0x50;
        case GLFW_KEY_DOWN: return 0x51;
        case GLFW_KEY_UP: return 0x52;
        case GLFW_KEY_NUM_LOCK: return 0x53;
        case GLFW_KEY_KP_DIVIDE: return 0x54;
        case GLFW_KEY_KP_MULTIPLY: return 0x55;
        case GLFW_KEY_KP_SUBTRACT: return 0x56;
        case GLFW_KEY_KP_ADD: return 0x57;
        case GLFW_KEY_KP_ENTER: return 0x58;
        case GLFW_KEY_KP_0: return 0x62;
        case GLFW_KEY_KP_DECIMAL: return 0x63;
        case GLFW_KEY_WORLD_2: return 0x64;   // the ISO key beside left shift (DOM IntlBackslash)
        case GLFW_KEY_MENU: return 0x65;
        case GLFW_KEY_KP_EQUAL: return 0x67;
        case GLFW_KEY_LEFT_CONTROL: return 0xE0;
        case GLFW_KEY_LEFT_SHIFT: return 0xE1;
        case GLFW_KEY_LEFT_ALT: return 0xE2;
        case GLFW_KEY_LEFT_SUPER: return 0xE3;
        case GLFW_KEY_RIGHT_CONTROL: return 0xE4;
        case GLFW_KEY_RIGHT_SHIFT: return 0xE5;
        case GLFW_KEY_RIGHT_ALT: return 0xE6;
        case GLFW_KEY_RIGHT_SUPER: return 0xE7;
        default: return 0;
    }
}

static void on_key(GLFWwindow *g, int key, int scancode, int action, int mods) {
    (void)scancode;
    winseam_window *w = of(g);
    int32_t hid = winseam_glfw_key_to_hid((int32_t)key);
    if (action == GLFW_PRESS) push_simple(w, WINSEAM_KEY_DOWN, hid, mods_of(mods), 0);
    else if (action == GLFW_REPEAT) push_simple(w, WINSEAM_KEY_DOWN, hid, mods_of(mods), 1);
    else if (action == GLFW_RELEASE) push_simple(w, WINSEAM_KEY_UP, hid, mods_of(mods), 0);
}

static void on_char(GLFWwindow *g, unsigned int codepoint) { push_simple(of(g), WINSEAM_TEXT, (int32_t)codepoint, 0, 0); }

// GLFW numbers a mouse's buttons left, right, middle; the W3C's `button`, left, middle, right, then back, forward and
// (5) a pen's eraser. GLFW's sixth to eighth follow at 6.
int32_t winseam_glfw_button_to_pointer(int32_t b) {
    switch (b) {
        case GLFW_MOUSE_BUTTON_LEFT: return 0;
        case GLFW_MOUSE_BUTTON_MIDDLE: return 1;
        case GLFW_MOUSE_BUTTON_RIGHT: return 2;
        case GLFW_MOUSE_BUTTON_4: return 3;
        case GLFW_MOUSE_BUTTON_5: return 4;
        case GLFW_MOUSE_BUTTON_6: return 6;
        case GLFW_MOUSE_BUTTON_7: return 7;
        case GLFW_MOUSE_BUTTON_8: return 8;
        default: return -1;
    }
}

// The mouse as a pointer, as a browser reports it: pointer 1, primary, one unit across, at its last position.
static void mouse_record(winseam_window *w, winseam_event *e, int32_t kind) {
    memset(e, 0, sizeof *e);
    e->kind = kind;
    e->x = w->last_x; e->y = w->last_y;
    e->pointer = 1;
    e->pointer_kind = WINSEAM_POINTER_MOUSE;
    e->primary = 1;
    e->width = 1.0f; e->height = 1.0f;
}

static void on_mouse_button(GLFWwindow *g, int button, int action, int mods) {
    winseam_window *w = of(g);
    int32_t b = winseam_glfw_button_to_pointer((int32_t)button);
    if (b < 0) return;
    winseam_event e;
    mouse_record(w, &e, action == GLFW_PRESS ? WINSEAM_POINTER_DOWN : WINSEAM_POINTER_UP);
    e.a = b;
    e.b = mods_of(mods);
    push(w, &e);
}

// The mouse entering or leaving the window, where GLFW says the cursor is.
static void on_cursor_enter(GLFWwindow *g, int entered) {
    winseam_window *w = of(g);
    winseam_event e;
    mouse_record(w, &e, entered ? WINSEAM_POINTER_ENTER : WINSEAM_POINTER_LEAVE);
    glfwGetCursorPos(g, &e.x, &e.y);
    push(w, &e);
}

// Positions are GLFW's screen coordinates, the logical ones. Locked, the position means nothing and the delta is the
// raw motion; the first sample after a lock (or ever) only seeds it.
static void on_cursor(GLFWwindow *g, double x, double y) {
    winseam_window *w = of(g);
    winseam_event e;
    mouse_record(w, &e, WINSEAM_POINTER_MOVE);
    e.x = x; e.y = y;
    if (w->have_last) { e.dx = x - w->last_x; e.dy = y - w->last_y; }
    w->have_last = 1;
    w->last_x = x; w->last_y = y;
    push(w, &e);
}

static void on_scroll(GLFWwindow *g, double dx, double dy) {
    winseam_window *w = of(g);
    winseam_event e;
    memset(&e, 0, sizeof e);
    e.kind = WINSEAM_SCROLL;
    e.a = 0;   // lines: GLFW's offsets are a wheel's notches
    e.dx = dx; e.dy = dy;
    push(w, &e);
}

static void on_size(GLFWwindow *g, int width, int height) {
    winseam_window *w = of(g);
    w->width = width; w->height = height;
    push_simple(w, WINSEAM_RESIZE, (int32_t)width, (int32_t)height, 0);
}

static void on_framebuffer_size(GLFWwindow *g, int width, int height) {
    winseam_window *w = of(g);
    w->fb_width = width; w->fb_height = height;
}

static void on_scale(GLFWwindow *g, float sx, float sy) {
    (void)sy;
    winseam_window *w = of(g);
    w->scale = sx;
    winseam_event e;
    memset(&e, 0, sizeof e);
    e.kind = WINSEAM_SCALE_CHANGED;
    e.x = (double)sx;
    push(w, &e);
}

// A focus change releases what is held, and a loss gives back a lock (`take`). GLFW then releases the keys and buttons
// it holds itself, after this callback and only on a loss (measured on 3.5.1, AGENTS.window.md item 5): the window holds
// none of them by then, so each of those ups is dropped.
static void on_focus(GLFWwindow *g, int focused) { push_simple(of(g), WINSEAM_FOCUS, focused ? 1 : 0, 0, 0); }

static void on_close(GLFWwindow *g) {
    glfwSetWindowShouldClose(g, GLFW_FALSE);   // the program decides; the request is only an event
    push_simple(of(g), WINSEAM_CLOSE_REQUESTED, 0, 0, 0);
}

winseam_window *winseam_open(const char *title, int32_t width, int32_t height, int32_t focused, const char *canvas,
                             int32_t sized_by_page, uint8_t *why, int32_t capacity, int32_t *why_length) {
    (void)canvas; (void)sized_by_page;   // the web's
    *why_length = 0;
    if (g_glfw_users == 0 && !glfwInit()) {
        const char *description = NULL;
        glfwGetError(&description);
        *why_length = copy_text(description, why, capacity);
        return NULL;
    }
    if (g_glfw_users == 0) sdl_start();
    g_glfw_users += 1;
    winseam_window *w = new_record();
    if (w == NULL) {
        *why_length = copy_text("out of memory", why, capacity);
        g_glfw_users -= 1;
        if (g_glfw_users == 0) { sdl_stop(); glfwTerminate(); }
        return NULL;
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    // Both, both ways, on every open: GLFW keeps a hint until it terminates, so an unfocused open would otherwise leave
    // the next one unfocused too. FOCUSED is what a window does as it opens; FOCUS_ON_SHOW, what it does when shown.
    int focus = focused ? GLFW_TRUE : GLFW_FALSE;
    glfwWindowHint(GLFW_FOCUSED, focus);
    glfwWindowHint(GLFW_FOCUS_ON_SHOW, focus);
    w->glfw = glfwCreateWindow(width, height, title, NULL, NULL);
    if (w->glfw == NULL) {
        const char *description = NULL;
        glfwGetError(&description);
        *why_length = copy_text(description, why, capacity);
        free_record(w);
        g_glfw_users -= 1;
        if (g_glfw_users == 0) { sdl_stop(); glfwTerminate(); }
        return NULL;
    }
    glfwSetWindowUserPointer(w->glfw, w);
    glfwSetKeyCallback(w->glfw, on_key);
    glfwSetCharCallback(w->glfw, on_char);
    glfwSetMouseButtonCallback(w->glfw, on_mouse_button);
    glfwSetCursorPosCallback(w->glfw, on_cursor);
    glfwSetCursorEnterCallback(w->glfw, on_cursor_enter);
    glfwSetScrollCallback(w->glfw, on_scroll);
    glfwSetWindowSizeCallback(w->glfw, on_size);
    glfwSetFramebufferSizeCallback(w->glfw, on_framebuffer_size);
    glfwSetWindowContentScaleCallback(w->glfw, on_scale);
    glfwSetWindowFocusCallback(w->glfw, on_focus);
    glfwSetWindowCloseCallback(w->glfw, on_close);
    int ww = 0, wh = 0, fw = 0, fh = 0;
    float sx = 1.0f, sy = 1.0f;
    glfwGetWindowSize(w->glfw, &ww, &wh);
    glfwGetFramebufferSize(w->glfw, &fw, &fh);
    glfwGetWindowContentScale(w->glfw, &sx, &sy);
    w->width = ww; w->height = wh;
    w->fb_width = fw; w->fb_height = fh;
    w->scale = sx;
    w->focused = glfwGetWindowAttrib(w->glfw, GLFW_FOCUSED) ? 1 : 0;
    open_add(w);
    return w;
}

// Closed as the user sees it: no callback fills the ring again, and the window is hidden. Its GLFW window and HWND stay
// valid for whoever still holds the record (a GPU surface on it: a configure on a destroyed window aborts wgpu-native).
// The callbacks go first, so the focus the hide takes away reaches no one.
static void system_close(winseam_window *w) {
    open_remove(w);
    if (w->glfw == NULL) return;
    glfwSetKeyCallback(w->glfw, NULL);
    glfwSetCharCallback(w->glfw, NULL);
    glfwSetMouseButtonCallback(w->glfw, NULL);
    glfwSetCursorPosCallback(w->glfw, NULL);
    glfwSetCursorEnterCallback(w->glfw, NULL);
    glfwSetScrollCallback(w->glfw, NULL);
    glfwSetWindowSizeCallback(w->glfw, NULL);
    glfwSetFramebufferSizeCallback(w->glfw, NULL);
    glfwSetWindowContentScaleCallback(w->glfw, NULL);
    glfwSetWindowFocusCallback(w->glfw, NULL);
    glfwSetWindowCloseCallback(w->glfw, NULL);
    glfwHideWindow(w->glfw);
}

// The record's last holder is gone: the GLFW window goes, and GLFW with the last record that had one, so a closed
// window still held keeps GLFW up under it.
static void system_destroy(winseam_window *w) {
    if (w->glfw == NULL) return;
    glfwDestroyWindow(w->glfw);
    g_glfw_users -= 1;
    if (g_glfw_users == 0) { sdl_stop(); glfwTerminate(); }
}

void winseam_pump(winseam_window *w) {
    if (w->glfw == NULL) return;
    glfwPollEvents();
    read_system_pads();
    deliver_system_pads();
}

void winseam_set_cursor(winseam_window *w, int32_t mode) {
    int32_t was = w->cursor_mode;
    w->cursor_mode = mode;
    if (w->glfw != NULL) {
        int raw = glfwRawMouseMotionSupported();
        if (mode == 2) {
            glfwSetInputMode(w->glfw, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            if (raw) glfwSetInputMode(w->glfw, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
        } else {
            if (raw) glfwSetInputMode(w->glfw, GLFW_RAW_MOUSE_MOTION, GLFW_FALSE);
            glfwSetInputMode(w->glfw, GLFW_CURSOR, mode == 1 ? GLFW_CURSOR_HIDDEN : GLFW_CURSOR_NORMAL);
        }
    }
    if ((mode == 2) != (was == 2)) {
        w->have_last = 0;   // the first sample in the new mode only seeds the delta
        push_simple(w, WINSEAM_POINTER_LOCK, mode == 2 ? 1 : 0, 0, 0);
    }
}

int32_t winseam_native_kind(winseam_window *w) {
    if (w->glfw == NULL) return 0;
#if defined(_WIN32)
    return 1;
#else
    return 0;   // X11, Wayland and Cocoa come with their legs (not built or verified yet)
#endif
}

void *winseam_native_display(winseam_window *w) {
    if (w->glfw == NULL) return NULL;
#if defined(_WIN32)
    return (void *)GetModuleHandleW(NULL);
#else
    return NULL;
#endif
}

void *winseam_native_window(winseam_window *w) {
    if (w->glfw == NULL) return NULL;
#if defined(_WIN32)
    return (void *)glfwGetWin32Window(w->glfw);
#else
    return NULL;
#endif
}

#else
// ---- the web: a window on the page's canvas (AGENTS.window.md item 8) ------------------------------------------------

// js/winseam.js writes records in this layout, at these offsets.
_Static_assert(offsetof(winseam_event, x) == 16, "js/winseam.js writes winseam_event's layout");
_Static_assert(offsetof(winseam_event, pointer) == 48, "js/winseam.js writes winseam_event's layout");
_Static_assert(offsetof(winseam_event, pressure) == 64, "js/winseam.js writes winseam_event's layout");
_Static_assert(offsetof(winseam_event, height) == 84, "js/winseam.js writes winseam_event's layout");
_Static_assert(sizeof(winseam_event) == 88, "js/winseam.js writes winseam_event's layout");

// js/winseam.js's own record, never in the ring: the canvas's sizes, which become Resize and ScaleChanged when they
// change. a: CSS width, b: CSS height (rounded), x: the device pixel ratio, dx and dy: the pixels.
#define WEB_SIZES 100

// js/winseam.js's.
extern int32_t winseam_web_open(winseam_window *record, const char *selector, int32_t width, int32_t height,
                                int32_t focused, int32_t sized_by_page, int32_t *sizes, float *scale);
extern void winseam_web_close(int32_t index);
extern winseam_window *winseam_web_pull(winseam_event *into);
extern void winseam_web_set_cursor(int32_t index, int32_t mode);
// The page's pads (WIN-4, decision 1): one read of `navigator.getGamepads()` a pump, then each slot from it. A slot is
// the pad's browser index. 1 and its parts written (meta: vendor, product, rumble bits, mapped, raw buttons, raw axes),
// or 0 for an empty index.
extern void winseam_web_pads_read(void);
extern int32_t winseam_web_pad(int32_t slot, winseam_pad *sample, char *name, int32_t name_capacity, int32_t *meta,
                               uint8_t *raw_buttons, float *raw_axes);
extern int32_t winseam_web_rumble(int32_t slot, float a, float b, int32_t milliseconds, int32_t triggers);

// js/winseam.js writes a pad's sample in this layout, at these offsets.
_Static_assert(offsetof(winseam_pad, buttons) == 4, "js/winseam.js writes winseam_pad's layout");
_Static_assert(offsetof(winseam_pad, axes) == 72, "js/winseam.js writes winseam_pad's layout");
_Static_assert(sizeof(winseam_pad) == 88, "js/winseam.js writes winseam_pad's layout");

// The page's pads into the table, while the system's are read.
static void read_system_pads(void) {
    if (!g_from_system) return;
    winseam_web_pads_read();
    for (int32_t slot = 0; slot < WINSEAM_PADS; slot++) {
        system_pad *p = &g_system[slot];
        winseam_pad s;
        char name[sizeof p->name];
        int32_t meta[6] = { 0, 0, 0, 0, 0, 0 };
        uint8_t raw_buttons[WINSEAM_PAD_RAW_BUTTONS];
        float raw_axes[WINSEAM_PAD_RAW_AXES];
        memset(&s, 0, sizeof s);
        memset(name, 0, sizeof name);
        memset(raw_buttons, 0, sizeof raw_buttons);
        memset(raw_axes, 0, sizeof raw_axes);
        if (!winseam_web_pad(slot, &s, name, (int32_t)sizeof name, meta, raw_buttons, raw_axes)) {
            memset(p, 0, sizeof *p);
            continue;
        }
        s.connected = 1;
        p->sample = s;
        copy_string(p->name, sizeof p->name, name);
        p->serial[0] = 0;   // the web gives a pad no serial and no id of its own (anti-fingerprinting)
        p->id[0] = 0;
        p->vendor = meta[0];
        p->product = meta[1];
        p->caps = meta[2];
        p->mapped = meta[3];
        p->raw_counts[0] = meta[4] > WINSEAM_PAD_RAW_BUTTONS ? WINSEAM_PAD_RAW_BUTTONS : meta[4];
        p->raw_counts[1] = meta[5] > WINSEAM_PAD_RAW_AXES ? WINSEAM_PAD_RAW_AXES : meta[5];
        p->raw_counts[2] = 0;   // the Gamepad API has no hats
        memcpy(p->raw_buttons, raw_buttons, sizeof raw_buttons);
        memcpy(p->raw_axes, raw_axes, sizeof raw_axes);
    }
}

int32_t winseam_glfw_key_to_hid(int32_t k) { (void)k; return 0; }
int32_t winseam_glfw_button_to_pointer(int32_t b) { (void)b; return -1; }

// The title is the page's: a canvas may be one part of a page (decision 3.2).
winseam_window *winseam_open(const char *title, int32_t width, int32_t height, int32_t focused, const char *canvas,
                             int32_t sized_by_page, uint8_t *why, int32_t capacity, int32_t *why_length) {
    (void)title;
    *why_length = 0;
    winseam_window *w = new_record();
    size_t n = strlen(canvas);
    char *selector = (char *)malloc(n + 1);
    if (w == NULL || selector == NULL) {
        free(selector);
        if (w != NULL) free_record(w);
        *why_length = copy_text("out of memory", why, capacity);
        return NULL;
    }
    memcpy(selector, canvas, n + 1);
    int32_t sizes[4] = { 0, 0, 0, 0 };
    float scale = 1.0f;
    int32_t index = winseam_web_open(w, canvas, width, height, focused, sized_by_page, sizes, &scale);
    if (index < 0) {
        char words[512];
        if (index == -1) snprintf(words, sizeof words, "no page: a window opens on a canvas, and this program is not running in a browser");
        else if (index == -2) snprintf(words, sizeof words, "no element on the page matches `%s`", canvas);
        else if (index == -3) snprintf(words, sizeof words, "`%s` is not a canvas", canvas);
        else if (index == -4) snprintf(words, sizeof words, "`%s` is already a window", canvas);
        else snprintf(words, sizeof words, "the page does not size `%s`: its layout follows its pixels on both axes, so "
                      "under sizedByPage it would grow each frame; give it a CSS width, and a height or an aspect-ratio, "
                      "or leave sizedByPage off", canvas);   // -5, WIN-21
        *why_length = copy_text(words, why, capacity);
        free(selector);
        free_record(w);
        return NULL;
    }
    w->selector = selector;
    w->canvas = index;
    w->focused = focused ? 1 : 0;   // the canvas was focused as it opened, or left until the user clicks it
    open_add(w);
    w->width = sizes[0]; w->height = sizes[1];
    w->fb_width = sizes[2]; w->fb_height = sizes[3];
    w->scale = scale;
    return w;
}

// Closed as the user sees it: the canvas's listeners go, and what the window set on it is given back. The selector the
// native handle points at stays until the record's last holder goes.
static void system_close(winseam_window *w) {
    open_remove(w);
    if (w->selector != NULL) winseam_web_close(w->canvas);
}

static void system_destroy(winseam_window *w) {
    free(w->selector);
    w->selector = NULL;
}

// A record from js/winseam.js: the sizes become the events of what changed; the queue's own drop is counted into the
// ring's; the browser giving the lock back sets the mode to Normal (decision 7); the rest is what any system sends.
static void web_take(winseam_window *w, const winseam_event *e) {
    if (e->kind == WEB_SIZES) {
        float scale = (float)e->x;
        w->fb_width = (int32_t)e->dx;
        w->fb_height = (int32_t)e->dy;
        if (scale != w->scale) {
            w->scale = scale;
            winseam_event s;
            memset(&s, 0, sizeof s);
            s.kind = WINSEAM_SCALE_CHANGED;
            s.x = (double)scale;
            push(w, &s);
        }
        if (e->a != w->width || e->b != w->height) {
            w->width = e->a;
            w->height = e->b;
            push_simple(w, WINSEAM_RESIZE, e->a, e->b, 0);
        }
        return;
    }
    if (e->kind == WINSEAM_DROPPED) { count_drop(w, e->a); return; }
    if (e->kind == WINSEAM_POINTER_LOCK && !e->a) w->cursor_mode = 0;
    take(w, e);
}

// Every canvas window's queue, as `glfwPollEvents` takes every GLFW window's.
void winseam_pump(winseam_window *w) {
    if (w->selector == NULL) return;
    winseam_event e;
    winseam_window *target;
    while ((target = winseam_web_pull(&e)) != NULL) web_take(target, &e);
    read_system_pads();
    deliver_system_pads();
}

// On a canvas, the browser says when a lock takes and when it goes (decision 7): a lock is asked for at the next
// pointer down, and PointerLock comes from the browser. Headless, at once.
void winseam_set_cursor(winseam_window *w, int32_t mode) {
    int32_t was = w->cursor_mode;
    w->cursor_mode = mode;
    if (w->selector != NULL) {
        winseam_web_set_cursor(w->canvas, mode);
        return;
    }
    if ((mode == 2) != (was == 2)) push_simple(w, WINSEAM_POINTER_LOCK, mode == 2 ? 1 : 0, 0, 0);
}

// The browser maps the page's pads.
int32_t winseam_add_pad_mappings(const char *text, int32_t length) { (void)text; (void)length; return 0; }

// A rumble on the page's pad: its actuator's dual-rumble or trigger-rumble effect, all 0 its reset.
static int system_rumble(int32_t slot, float a, float b, int32_t milliseconds, int triggers) {
    return winseam_web_rumble(slot, a, b, milliseconds, triggers) != 0;
}

int32_t winseam_native_kind(winseam_window *w) { return w->selector != NULL ? 5 : 0; }
void *winseam_native_display(winseam_window *w) { (void)w; return NULL; }
void *winseam_native_window(winseam_window *w) { return w->selector; }
#endif

// ---- the record's holders, on every leg -------------------------------------------------------------------------------

void winseam_hold(winseam_window *w) {
    if (w != NULL) w->holds += 1;
}

void winseam_release(winseam_window *w) {
    if (w == NULL) return;
    w->holds -= 1;
    if (w->holds > 0) return;
    system_destroy(w);
    free_record(w);
}

void winseam_close(winseam_window *w) {
    if (w == NULL) return;
    if (!w->closed) {
        w->closed = 1;
        system_close(w);
    }
    winseam_release(w);
}

int32_t winseam_closed(winseam_window *w) { return w->closed; }

int32_t winseam_live_records(void) { return g_live_records; }
