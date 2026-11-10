// winseam.h: @kama/window's C seam. A window is a ring of events plus, natively, a GLFW window whose callbacks fill it
// (reached through GLFW's window user pointer, so several windows work); a headless window is the ring alone, filled by
// `post`. kama reads the ring one record at a time and turns each into an `Event`. (kama's design: ROADMAP_DETAIL.md
// § 8, s8-input.)
//
// The ring is fixed (no heap after open) and coalesces: a mouse move, a scroll or a resize joins the newest unread
// record of its kind. When it is full a new record is dropped and counted, and the reader gets one `Dropped(n)` record
// after the ones it holds, so a drop is never silent.
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
#define WINSEAM_MOUSE_MOVE 4       // x, y (logical), dx, dy
#define WINSEAM_MOUSE_BUTTON 5     // a: button, b: mods, c: down
#define WINSEAM_SCROLL 6           // dx, dy
#define WINSEAM_RESIZE 7           // a: width, b: height (logical)
#define WINSEAM_SCALE_CHANGED 8    // x: the new scale
#define WINSEAM_FOCUS 9            // a: focused
#define WINSEAM_POINTER_LOCK 10    // a: locked
#define WINSEAM_CLOSE_REQUESTED 11
#define WINSEAM_DROPPED 12         // a: how many

typedef struct winseam_event {
    int32_t kind;
    int32_t a;
    int32_t b;
    int32_t c;
    double x;
    double y;
    double dx;
    double dy;
} winseam_event;

typedef struct winseam_window winseam_window;

// A window with a GLFW window behind it, `width` × `height` logical. NULL on failure, with the reason copied into `why`
// (at most `capacity` bytes, UTF-8, not terminated) and its length in `*why_length`.
winseam_window *winseam_open(const char *title, int32_t width, int32_t height, uint8_t *why, int32_t capacity,
                             int32_t *why_length);
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

// Natively, take the system's pending events into every open window's ring. Headless or on the web, nothing.
void winseam_pump(winseam_window *window);
// The next record into `*into`: 1, or 0 when the ring is empty.
int32_t winseam_next(winseam_window *window, winseam_event *into);
// A record into the ring as if the system had sent it (a test, a replay, a server).
void winseam_post(winseam_window *window, const winseam_event *event);

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

// The key table, exposed so a test can check it: GLFW's key token to a HID usage id, 0 when unmapped.
int32_t winseam_glfw_key_to_hid(int32_t glfw_key);

#endif
