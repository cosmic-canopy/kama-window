// winseam.h: @kama/window's C seam. A window is a ring of events plus, natively, a GLFW window whose callbacks fill it
// (reached through GLFW's window user pointer, so several windows work); a headless window is the ring alone, filled by
// `post`. kama reads the ring one record at a time and turns each into an `Event`. (kama's design: ROADMAP_DETAIL.md
// § 8, s8-input.)
//
// The ring is fixed (no heap after open) and coalesces: a pointer's move, a scroll or a resize joins the newest unread
// record of its kind (the same pointer's; the same unit's). When it is full a new record is dropped and counted, and the reader gets one `Dropped(n)` record
// after the ones it holds, so a drop is never silent.
//
// The window keeps what is held, keys and each pointer's buttons (AGENTS.window.md items 5 and 8): an up for what is not
// held is dropped, a key's down is a press or a repeat as the window holds it, and on a focus change, either way, every
// held mouse button goes up and every held touch or pen contact is cancelled, then every held key goes up, before the
// Focus record. The ring keeps a slot for each held one's end, so none is ever counted into a drop. GLFW's callbacks and
// `winseam_post` go through the same rules.
//
// Keys arrive as USB HID usage ids (the keyboard page), the DOM's `KeyboardEvent.code` numbering: GLFW's US-named,
// layout-independent tokens translate through one table, anything unmapped to 0, Unknown.
//
// This header reaches every C file of a consumer's build (kama emits each extern header into one shared header), so it
// declares and never defines.
#ifndef WINSEAM_H
#define WINSEAM_H

#include <stdint.h>

// Record kinds, the `Event` cases in order.
#define WINSEAM_KEY_DOWN 1         // a: HID key, b: mods, c: repeat
#define WINSEAM_KEY_UP 2           // a: HID key, b: mods
#define WINSEAM_TEXT 3             // a: codepoint
#define WINSEAM_POINTER_MOVE 4     // the pointer; dx, dy
#define WINSEAM_POINTER_DOWN 5     // the pointer; a: button (W3C's numbering), b: mods
#define WINSEAM_POINTER_UP 6       // the pointer; a: button, b: mods
#define WINSEAM_POINTER_CANCEL 7   // the pointer
#define WINSEAM_POINTER_ENTER 8    // the pointer
#define WINSEAM_POINTER_LEAVE 9    // the pointer
#define WINSEAM_SCROLL 10          // dx, dy; a: the unit, 0 lines or 1 pixels
#define WINSEAM_RESIZE 11          // a: width, b: height (logical)
#define WINSEAM_SCALE_CHANGED 12   // x: the new scale
#define WINSEAM_FOCUS 13           // a: focused
#define WINSEAM_POINTER_LOCK 14    // a: locked
#define WINSEAM_CLOSE_REQUESTED 15
#define WINSEAM_DROPPED 16         // a: how many

// Pointer kinds, and the buttons a pointer can hold: 0 to 5 as the W3C's `button` numbers them (left, middle, right,
// back, forward, a pen's eraser), then a mouse's sixth to eighth.
#define WINSEAM_POINTER_MOUSE 0
#define WINSEAM_POINTER_TOUCH 1
#define WINSEAM_POINTER_PEN 2
#define WINSEAM_BUTTONS 9

// One event. A pointer event carries its pointer in `x`, `y` and the fields after `dy`, a W3C PointerEvent's; the
// window sets `buttons` to what it holds for the pointer after the event, and a mouse's `pressure` from it (0.5 while
// a button is held, the W3C's rule for a device with none).
typedef struct winseam_event {
    int32_t kind;
    int32_t a;
    int32_t b;
    int32_t c;
    double x;               // a pointer's position (logical); a new scale
    double y;
    double dx;              // a move's motion; a scroll's amount
    double dy;
    int32_t pointer;        // the pointer's id
    int32_t pointer_kind;   // WINSEAM_POINTER_MOUSE, _TOUCH or _PEN
    int32_t primary;
    int32_t buttons;        // a bit per button held: 1 << button
    float pressure;         // 0 to 1
    float tilt_x;           // degrees, -90 to 90
    float tilt_y;
    float twist;            // degrees, 0 to 359
    float width;            // the contact's size, logical
    float height;
} winseam_event;

typedef struct winseam_window winseam_window;

// A system window, `width` × `height` logical, taking the focus as it opens when `focused` is 1 and leaving it where it
// is when 0. Natively a GLFW window titled `title`. On the web the page's canvas `canvas` (a selector), whose CSS size
// the window sets to `width` × `height` unless `sized_by_page` is 1; `title` is the page's to set. NULL on failure, with
// the reason copied into `why` (at most `capacity` bytes, UTF-8, not terminated) and its length in `*why_length`.
winseam_window *winseam_open(const char *title, int32_t width, int32_t height, int32_t focused, const char *canvas,
                             int32_t sized_by_page, uint8_t *why, int32_t capacity, int32_t *why_length);
// A window with no system window behind it: its ring, its size and `post`. Never touches GLFW. NULL only when out of
// memory.
winseam_window *winseam_headless(int32_t width, int32_t height);

// A record is counted: its Window holds it once, from open or headless, and each NativeWindow on it once more. It goes
// with its last holder, in either order, so a handle's display and window stay valid while it lives.
//
// The Window's drop: the window closes as the user sees it at once (natively hidden, its callbacks gone), then the
// Window's hold is released. The GLFW window, and GLFW with the last record that had one, go at the last release.
void winseam_close(winseam_window *window);
void winseam_hold(winseam_window *window);
void winseam_release(winseam_window *window);
// 1 once the window is closed: its Window dropped.
int32_t winseam_closed(winseam_window *window);
// Records alive in the process, for the tests.
int32_t winseam_live_records(void);

// Take the system's pending events into every open window's ring: natively GLFW's, on the web the queues the package's
// JS library fills. Headless, nothing.
void winseam_pump(winseam_window *window);
// The next record into `*into`: 1, or 0 when the ring is empty.
int32_t winseam_next(winseam_window *window, winseam_event *into);
// A record into the ring as if the system had sent it (a test, a replay, a server): the held set's rules apply to it.
void winseam_post(winseam_window *window, const winseam_event *event);
// The end of every held pointer (a mouse's buttons up, a contact cancelled), then an up for every held key by HID id, as
// a focus change makes them, with no Focus record.
void winseam_release_held(winseam_window *window);

void winseam_size(winseam_window *window, int32_t *width, int32_t *height);
void winseam_framebuffer_size(winseam_window *window, int32_t *width, int32_t *height);
float winseam_scale(winseam_window *window);
// 0 normal, 1 hidden, 2 locked (hidden, unbounded raw motion; a PointerLock record says when it takes hold).
void winseam_set_cursor(winseam_window *window, int32_t mode);
int32_t winseam_headless_p(winseam_window *window);

// What a GPU surface needs: the kind (0 none, 1 Win32, 2 X11, 3 Wayland, 4 a Cocoa NSWindow, 5 a canvas selector),
// the display or module (HINSTANCE, Display*, wl_display*), and the window (HWND, X11 Window, wl_surface*, NSWindow*,
// the selector's C string).
int32_t winseam_native_kind(winseam_window *window);
void *winseam_native_display(winseam_window *window);
void *winseam_native_window(winseam_window *window);

// The tables, exposed so a test can check them: GLFW's key token to a HID usage id, 0 when unmapped; GLFW's mouse button
// to the W3C's numbering, -1 when unmapped.
int32_t winseam_glfw_key_to_hid(int32_t glfw_key);
int32_t winseam_glfw_button_to_pointer(int32_t glfw_button);
// On the web, the DOM's `KeyboardEvent.code` to a HID usage id (0 when unmapped), and how many codes the table names: the
// package's JS library's (js/winseam.js). Not defined natively.
int32_t winseam_web_code_to_hid(const char *code);
int32_t winseam_web_code_count(void);

#endif
