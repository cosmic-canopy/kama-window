// hooks.h: what the tests ask the system about a window, from outside the package. Declares and never defines (kama emits
// each extern header into one shared header).
#ifndef WINDOWTESTS_HOOKS_H
#define WINDOWTESTS_HOOKS_H

#include <stdint.h>

// 1 when `hwnd` is still a window, 0 when it is not; -1 off Windows, where the question is not answered.
int32_t windowtests_is_window(void *hwnd);
// 1 when `hwnd` is shown, 0 when it is hidden; -1 off Windows.
int32_t windowtests_is_visible(void *hwnd);

#endif
