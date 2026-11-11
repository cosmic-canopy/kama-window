// winseam.c: the seam's bodies (winseam.h says what it is). The ring and the headless window are the same on every
// leg; natively a GLFW window fills the ring from its callbacks. The web's html5 half is not built yet: on the web
// `winseam_open` fails, saying so, and a headless window works.
#include "winseam.h"

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
    uint8_t held_buttons;    // a bit per mouse button, the same way
    int32_t held_count;      // keys and buttons held: the ring keeps a slot for each one's up
    int32_t lock_bits;       // CapsLock and NumLock, as the last key or button event reported them
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

// Coalesce into the newest unread record when it is the same kind of continuous change; otherwise append.
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
    winseam_event *last = w->count > 0 ? &w->ring[(w->head + w->count - 1) % RING] : NULL;
    if (last != NULL && last->kind != WINSEAM_DROPPED && last->kind == e->kind) {
        if (e->kind == WINSEAM_MOUSE_MOVE) {
            last->x = e->x; last->y = e->y; last->dx += e->dx; last->dy += e->dy;
            return 1;
        }
        if (e->kind == WINSEAM_SCROLL) { last->dx += e->dx; last->dy += e->dy; return 1; }
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
    if (last != NULL && last->kind == WINSEAM_DROPPED) { last->a += 1; return 0; }
    winseam_event d;
    memset(&d, 0, sizeof d);
    d.kind = WINSEAM_DROPPED;
    d.a = 1;
    w->ring[(w->head + w->count) % RING] = d;
    w->count += 1;
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

static int button_held(const winseam_window *w, int32_t button) { return (w->held_buttons >> button) & 1; }

static void hold_button(winseam_window *w, int32_t button, int held) {
    if (button_held(w, button) == held) return;
    w->held_buttons ^= (uint8_t)(1u << button);
    w->held_count += held ? 1 : -1;
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

// An up for every held button, then every held key by ascending HID id, so the modifiers (224 to 231) go last: the
// action goes up before the modifier that chose it. Each carries the modifiers still held after it. The ring owes each
// of them, so none is counted into a drop.
static void release_held(winseam_window *w) {
    winseam_event e;
    for (int32_t b = 0; b < 8; b++) {
        if (!button_held(w, b)) continue;
        hold_button(w, b, 0);
        memset(&e, 0, sizeof e);
        e.kind = WINSEAM_MOUSE_BUTTON; e.a = b; e.b = held_mods(w); e.c = 0;
        e.x = w->last_x; e.y = w->last_y;
        store(w, &e, w->held_count, 1);
    }
    for (int32_t hid = 1; hid < 256; hid++) {
        if (!key_held(w, hid)) continue;
        hold_key(w, hid, 0);
        memset(&e, 0, sizeof e);
        e.kind = WINSEAM_KEY_UP; e.a = hid; e.b = held_mods(w);
        e.x = w->last_x; e.y = w->last_y;
        store(w, &e, w->held_count, 1);
    }
}

void winseam_release_held(winseam_window *w) { release_held(w); }

// Every event the window takes, from the system's callbacks and from `post`, comes through here: the held set's rules,
// then the ring.
// - An up for a key or button not held is dropped.
// - A key's down is a press when the key is not held, and holds it; a repeat when it is, whatever the system said. A
//   down that is counted into a drop is not held: the consumer never saw it.
// - A second down for a held button is dropped.
// - A focus change, either way, releases everything held, then gives back a lock a loss undoes, then says Focus.
static void take(winseam_window *w, const winseam_event *e) {
    if (e->kind == WINSEAM_KEY_DOWN || e->kind == WINSEAM_KEY_UP || e->kind == WINSEAM_MOUSE_BUTTON) {
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
    if (e->kind == WINSEAM_MOUSE_BUTTON && e->a >= 0 && e->a < 8) {
        int32_t b = e->a;
        if (e->c) {
            if (button_held(w, b)) return;
            if (store(w, e, w->held_count + 1, 0)) hold_button(w, b, 1);
            return;
        }
        if (!button_held(w, b)) return;
        hold_button(w, b, 0);
        store(w, e, w->held_count, 1);
        return;
    }
    if (e->kind == WINSEAM_FOCUS) {
        release_held(w);
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
    if (e->kind == WINSEAM_RESIZE) { w->width = e->a; w->height = e->b; w->fb_width = e->a; w->fb_height = e->b; }
    push(w, e);
}

winseam_window *winseam_headless(int32_t width, int32_t height) {
    winseam_window *w = new_record();
    if (w == NULL) return NULL;
    w->width = width; w->height = height;
    w->fb_width = width; w->fb_height = height;
    w->scale = 1.0f;
    return w;
}

int32_t winseam_headless_p(winseam_window *w) {
#ifndef __EMSCRIPTEN__
    return w->glfw == NULL;
#else
    (void)w;
    return 1;
#endif
}

void winseam_size(winseam_window *w, int32_t *width, int32_t *height) { *width = w->width; *height = w->height; }
void winseam_framebuffer_size(winseam_window *w, int32_t *width, int32_t *height) {
    *width = w->fb_width; *height = w->fb_height;
}
float winseam_scale(winseam_window *w) { return w->scale; }

#ifndef __EMSCRIPTEN__
// ---- natively: GLFW ------------------------------------------------------------------------------------------------

// glfwInit and glfwTerminate are process-wide, and terminate destroys every window: counted, so the last window's
// close is the one that terminates.
static int32_t g_glfw_users = 0;

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

static void on_mouse_button(GLFWwindow *g, int button, int action, int mods) {
    push_simple(of(g), WINSEAM_MOUSE_BUTTON, (int32_t)button, mods_of(mods), action == GLFW_PRESS ? 1 : 0);
}

// Positions are GLFW's screen coordinates, the logical ones. Locked, the position means nothing and the delta is the
// raw motion; the first sample after a lock (or ever) only seeds it.
static void on_cursor(GLFWwindow *g, double x, double y) {
    winseam_window *w = of(g);
    winseam_event e;
    memset(&e, 0, sizeof e);
    e.kind = WINSEAM_MOUSE_MOVE;
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

winseam_window *winseam_open(const char *title, int32_t width, int32_t height, int32_t focused, uint8_t *why,
                             int32_t capacity, int32_t *why_length) {
    *why_length = 0;
    if (g_glfw_users == 0 && !glfwInit()) {
        const char *description = NULL;
        glfwGetError(&description);
        *why_length = copy_text(description, why, capacity);
        return NULL;
    }
    g_glfw_users += 1;
    winseam_window *w = new_record();
    if (w == NULL) {
        *why_length = copy_text("out of memory", why, capacity);
        g_glfw_users -= 1;
        if (g_glfw_users == 0) glfwTerminate();
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
        if (g_glfw_users == 0) glfwTerminate();
        return NULL;
    }
    glfwSetWindowUserPointer(w->glfw, w);
    glfwSetKeyCallback(w->glfw, on_key);
    glfwSetCharCallback(w->glfw, on_char);
    glfwSetMouseButtonCallback(w->glfw, on_mouse_button);
    glfwSetCursorPosCallback(w->glfw, on_cursor);
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
    return w;
}

// Closed as the user sees it: no callback fills the ring again, and the window is hidden. Its GLFW window and HWND stay
// valid for whoever still holds the record (a GPU surface on it: a configure on a destroyed window aborts wgpu-native).
// The callbacks go first, so the focus the hide takes away reaches no one.
static void system_close(winseam_window *w) {
    if (w->glfw == NULL) return;
    glfwSetKeyCallback(w->glfw, NULL);
    glfwSetCharCallback(w->glfw, NULL);
    glfwSetMouseButtonCallback(w->glfw, NULL);
    glfwSetCursorPosCallback(w->glfw, NULL);
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
    if (g_glfw_users == 0) glfwTerminate();
}

void winseam_pump(winseam_window *w) {
    if (w->glfw != NULL) glfwPollEvents();
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
// ---- the web: not built yet (KR-26's commit 2, html5 callbacks on #canvas) -------------------------------------------

int32_t winseam_glfw_key_to_hid(int32_t k) { (void)k; return 0; }

winseam_window *winseam_open(const char *title, int32_t width, int32_t height, int32_t focused, uint8_t *why,
                             int32_t capacity, int32_t *why_length) {
    (void)title; (void)width; (void)height; (void)focused;
    *why_length = copy_text("@kama/window has no web window yet: only a headless one", why, capacity);
    return NULL;
}

static void system_close(winseam_window *w) { (void)w; }
static void system_destroy(winseam_window *w) { (void)w; }
void winseam_pump(winseam_window *w) { (void)w; }

void winseam_set_cursor(winseam_window *w, int32_t mode) {
    int32_t was = w->cursor_mode;
    w->cursor_mode = mode;
    if ((mode == 2) != (was == 2)) push_simple(w, WINSEAM_POINTER_LOCK, mode == 2 ? 1 : 0, 0, 0);
}

int32_t winseam_native_kind(winseam_window *w) { (void)w; return 0; }
void *winseam_native_display(winseam_window *w) { (void)w; return NULL; }
void *winseam_native_window(winseam_window *w) { (void)w; return NULL; }
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
