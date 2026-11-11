// hooks.h: what the tests ask the system about a window, from outside the package. Declares and never defines (kama emits
// each extern header into one shared header).
#ifndef WINDOWTESTS_HOOKS_H
#define WINDOWTESTS_HOOKS_H

#include <stdint.h>

// 1 when `hwnd` is still a window, 0 when it is not; -1 off Windows, where the question is not answered.
int32_t windowtests_is_window(void *hwnd);
// 1 when `hwnd` is shown, 0 when it is hidden; -1 off Windows.
int32_t windowtests_is_visible(void *hwnd);
// 1 on Windows, where the questions here are answered; 0 elsewhere.
int32_t windowtests_win32(void);
// 1 when `hwnd` is the foreground window, the one taking the keyboard; 0 when not; -1 off Windows.
int32_t windowtests_is_foreground(void *hwnd);
// GLFW's FOCUS_ON_SHOW on the GLFW window behind `hwnd`, found through the window property GLFW sets on each: 1 when
// showing it would take the focus, 0 when not; -1 off Windows, or when no GLFW window is behind it.
int32_t windowtests_focus_on_show(void *hwnd);
// Whether the next windows opened are shown (GLFW_VISIBLE): GLFW keeps the hint until it terminates. It brings GLFW up
// if no window has; nothing off Windows.
void windowtests_shown_next(int32_t shown);
// Sends `hwnd` a key message as the system would: WM_KEYDOWN, or WM_KEYUP when `up`, for the virtual key `vk` with its
// scan code. GLFW's window procedure handles it at once, whether or not the window has the focus. 1 when sent; -1 off
// Windows.
int32_t windowtests_send_key(void *hwnd, int32_t vk, int32_t up);
// Sends `hwnd` WM_SETFOCUS when `focused`, else WM_KILLFOCUS: GLFW's window procedure runs its focus path, while the
// window neither takes nor gives up the real focus. 1 when sent; -1 off Windows.
int32_t windowtests_send_focus(void *hwnd, int32_t focused);

// 1 when the C string at `p` is `s`.
int32_t windowtests_cstr_is(const void *p, const char *s);

// On the web, the tests' own JS library (tests/js/windowtests.js): DOM events dispatched at the element a selector names,
// answering 1 when the event's default was left to the page and 0 when a listener prevented it, and what the window
// did to the element. Not defined natively.
int32_t windowtests_in_browser(void);
int32_t windowtests_key(const char *sel, const char *type, const char *code, const char *key, int32_t mods,
                        int32_t repeat);
int32_t windowtests_pointer(const char *sel, const char *type, int32_t id, int32_t kind, double x, double y,
                            int32_t button, int32_t buttons, float pressure, float tilt_x, float tilt_y, float twist,
                            float width, float height, int32_t primary);
int32_t windowtests_wheel(const char *sel, double dx, double dy, int32_t mode);
int32_t windowtests_context_menu(const char *sel);
void windowtests_focus(const char *sel);
int32_t windowtests_active(const char *sel);
int32_t windowtests_style_is(const char *sel, const char *prop, const char *value);
int32_t windowtests_attr_is(const char *sel, const char *name, const char *value);
void windowtests_set_style(const char *sel, const char *prop, const char *value);
void windowtests_backing(const char *sel, int32_t *width, int32_t *height);
int32_t windowtests_acked(void);
int32_t windowtests_locked(const char *sel);
int32_t windowtests_report_sizes(const char *sel, double width, double height, int32_t fw, int32_t fh, double scale);

#endif
