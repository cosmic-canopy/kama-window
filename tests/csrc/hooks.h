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

#endif
