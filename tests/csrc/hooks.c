// hooks.c: hooks.h's bodies. Win32 answers natively on Windows; elsewhere every question answers -1.
#include "hooks.h"

#if defined(_WIN32)
#include <windows.h>
#include <GLFW/glfw3.h>

int32_t windowtests_is_window(void *hwnd) { return IsWindow((HWND)hwnd) ? 1 : 0; }
int32_t windowtests_is_visible(void *hwnd) { return IsWindowVisible((HWND)hwnd) ? 1 : 0; }
int32_t windowtests_win32(void) { return 1; }
int32_t windowtests_is_foreground(void *hwnd) { return GetForegroundWindow() == (HWND)hwnd ? 1 : 0; }
int32_t windowtests_focus_on_show(void *hwnd) {
    GLFWwindow *g = (GLFWwindow *)GetPropW((HWND)hwnd, L"GLFW");
    if (g == NULL) return -1;
    return glfwGetWindowAttrib(g, GLFW_FOCUS_ON_SHOW) ? 1 : 0;
}
void windowtests_shown_next(int32_t shown) {
    if (!glfwInit()) return;
    glfwWindowHint(GLFW_VISIBLE, shown ? GLFW_TRUE : GLFW_FALSE);
}
#else
int32_t windowtests_is_window(void *hwnd) { (void)hwnd; return -1; }
int32_t windowtests_is_visible(void *hwnd) { (void)hwnd; return -1; }
int32_t windowtests_win32(void) { return 0; }
int32_t windowtests_is_foreground(void *hwnd) { (void)hwnd; return -1; }
int32_t windowtests_focus_on_show(void *hwnd) { (void)hwnd; return -1; }
void windowtests_shown_next(int32_t shown) { (void)shown; }
#endif
