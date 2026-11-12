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
#define WINSEAM_PAD_CONNECTED 17   // a: the slot
#define WINSEAM_PAD_DISCONNECTED 18 // a: the slot
#define WINSEAM_PAD_DOWN 19        // a: the slot, b: the button (the W3C's standard numbering)
#define WINSEAM_PAD_UP 20          // a: the slot, b: the button

// Gamepads (WIN-4, ROADMAP_DETAIL.md § WIN-4). Each window keeps its own view of each slot: the latest sample, the
// system's or a post's, compared with what the consumer has seen, makes a slot's connect and disconnect and each button's
// down and up, paired as a key's are. A button is pressed at 30 of 255 or more of its travel, on every backend (decision
// 2a). A focus change releases each held pad button after the pointers' and before the keys', and while the window is
// unfocused a pad reads neutral, unless the program asked for pads in the background (decisions 5 and 6).
#define WINSEAM_PADS 16
#define WINSEAM_PAD_BUTTONS 17     // the W3C's standard gamepad: faces, shoulders, triggers, back, start, sticks, d-pad, guide
#define WINSEAM_PAD_AXES 4         // left x, left y, right x, right y: -1 to 1, right and down positive

// A pad as one read of it. `connected` 0 is a slot with no pad, whose values are 0.
typedef struct winseam_pad {
    int32_t connected;
    float buttons[WINSEAM_PAD_BUTTONS];   // 0 to 1
    float axes[WINSEAM_PAD_AXES];         // -1 to 1
} winseam_pad;

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
// The end of every held pointer (a mouse's buttons up, a contact cancelled), then an up for every held pad button by slot
// and number, then an up for every held key by HID id, as a focus change makes them, with no Focus record. A pad then
// reads neutral until its next sample.
void winseam_release_held(winseam_window *window);

// A pad's sample into slot `slot` (0 to WINSEAM_PADS - 1, any other ignored) as if the system had read it: compared with
// what the consumer has seen, it makes the slot's events.
void winseam_pad_sample(winseam_window *window, int32_t slot, const winseam_pad *sample);
// The same, from its parts: WINSEAM_PAD_BUTTONS values at `buttons` and WINSEAM_PAD_AXES at `axes` (kama's arrays, which
// cannot yet write into a winseam_pad's: KAMA_GAPS.md).
void winseam_pad_post(winseam_window *window, int32_t slot, int32_t connected, const float *buttons, const float *axes);
// The pad in `slot` as the consumer reads it: connected once its connect is queued; neutral while the window is unfocused
// (unless pads are read in the background) and after a release until its next sample; all 0 for an empty slot.
void winseam_pad_read(winseam_window *window, int32_t slot, winseam_pad *into);
// Pads read while the window is unfocused (1) or neutral then (0, the default). Turned off while unfocused, what the pads
// hold is released at once.
void winseam_set_pads_when_unfocused(winseam_window *window, int32_t on);

// The system's pads (WIN-4, decision 1). Every pump of a system window reads them once (natively SDL3, on Windows so far;
// on the web the Gamepad API) into one process-wide table, and hands each slot's sample to every open system window,
// as `glfwPollEvents` takes every GLFW window's events. A new pad takes the lowest free slot and keeps it while it stays.
// A headless window has none.
//
// A test or a replay replaces the system's read (decision 8): `winseam_pads_from_system(0)` stops reading the system, and
// `winseam_system_pad` sets a slot as the system would (WINSEAM_PAD_BUTTONS values at `buttons`, WINSEAM_PAD_AXES at
// `axes`; `vendor` and `product` 0 when unknown, `serial` and `id` "" when there are none). `winseam_pads_from_system(1)`
// reads the system again, the table emptied first.
void winseam_pads_from_system(int32_t on);
void winseam_system_pad(int32_t slot, int32_t connected, const float *buttons, const float *axes, const char *name,
                        int32_t vendor, int32_t product, const char *serial, const char *id, int32_t caps);
// While a test replaces the system's read: the last rumble the system's `slot` was asked for, its strong, weak, left and
// right motors into `motors[4]` (a dual rumble sets the first two, a trigger rumble the last two) and its milliseconds;
// the number of requests since the slot was set, 0 when none.
int32_t winseam_system_pad_rumbled(int32_t slot, float *motors, int32_t *milliseconds);
// What the system says of the pad in `slot`, as `window` knows it: its name (`which` 0), its serial (1) or its id (2), the
// system's own name for the device, copied into `into` (at most `capacity` bytes, UTF-8, not terminated), the length
// returned; its USB vendor (`which` 0) or product (1), 0 when the system gives none, what it can do (2): bits
// WINSEAM_PAD_RUMBLE and WINSEAM_PAD_TRIGGER_RUMBLE, or whether a mapping knows it (3): 1, or 0 for a pad read only raw.
// -1 when the window knows no pad there, or is headless.
int32_t winseam_pad_text(winseam_window *window, int32_t slot, int32_t which, uint8_t *into, int32_t capacity);
int32_t winseam_pad_number(winseam_window *window, int32_t slot, int32_t which);
#define WINSEAM_PAD_RUMBLE 1            // a strong (low-frequency) and a weak (high-frequency) motor
#define WINSEAM_PAD_TRIGGER_RUMBLE 2    // a motor in each trigger

// Rumble (WIN-4, decision 7) on the pad in `slot`, as `window` knows it: `strong` the low-frequency motor and `weak` the
// high-frequency one, each clamped to 0 to 1, for `milliseconds`; all 0 stops it. The trigger rumble the same with each
// trigger's motor. 1 when the system took it; 0 when the window knows no pad there or is headless, when its pads read
// neutral (unfocused, pads not read in the background: decision 5), or when the pad has no such motor. A focus loss
// that closes the window's pads stops each rumble the window started, as it releases each held button (decision 6).
int32_t winseam_pad_rumble(winseam_window *window, int32_t slot, float strong, float weak, int32_t milliseconds);
int32_t winseam_pad_rumble_triggers(winseam_window *window, int32_t slot, float left, float right, int32_t milliseconds);

// The pad in `slot` as the system numbers its controls, before any mapping (WIN-4, decision 3): how many buttons, axes
// and hats it has into `counts[3]` (at most 32, 8 and 4), each button 0 or 1 into `buttons[32]`, each axis -1 to 1 into
// `axes[8]`, each hat into `hats[4]` (bits: 1 up, 2 right, 4 down, 8 left). 1 when `window` knows a pad there, 0 when not
// or when it is headless. The values read 0 while its pads read neutral, as the standard view does.
int32_t winseam_pad_raw(winseam_window *window, int32_t slot, int32_t *counts, uint8_t *buttons, float *axes,
                        uint8_t *hats);
#define WINSEAM_PAD_RAW_BUTTONS 32
#define WINSEAM_PAD_RAW_AXES 8
#define WINSEAM_PAD_RAW_HATS 4

// Mappings for pads, `length` bytes of SDL's text format, a mapping a line (SDL_GameControllerDB's): kept, and applied
// now and at each start of SDL, so a pad they name is mapped in its slot from then on. How many SDL took now; 0 when no
// system window is open (they apply when one opens), and on the web, where the browser maps. Text SDL takes nothing
// from is not kept.
int32_t winseam_add_pad_mappings(const char *text, int32_t length);

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
