// hooks.c: hooks.h's bodies. Win32 answers natively on Windows; elsewhere every question answers -1.
#include "hooks.h"

#if defined(_WIN32)
#include <windows.h>

int32_t windowtests_is_window(void *hwnd) { return IsWindow((HWND)hwnd) ? 1 : 0; }
int32_t windowtests_is_visible(void *hwnd) { return IsWindowVisible((HWND)hwnd) ? 1 : 0; }
#else
int32_t windowtests_is_window(void *hwnd) { (void)hwnd; return -1; }
int32_t windowtests_is_visible(void *hwnd) { (void)hwnd; return -1; }
#endif
