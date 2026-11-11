// hooks.c: hooks.h's bodies. Win32 answers natively on Windows; elsewhere every question answers -1.
#include "hooks.h"

#include <string.h>

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
int32_t windowtests_send_key(void *hwnd, int32_t vk, int32_t up) {
    UINT scancode = MapVirtualKeyW((UINT)vk, MAPVK_VK_TO_VSC);
    LPARAM l = 1 | ((LPARAM)(scancode & 0xff) << 16);   // a repeat count of 1, and the scan code
    if (up) l |= ((LPARAM)1 << 30) | ((LPARAM)1 << 31);   // was down, and is going up
    SendMessageW((HWND)hwnd, up ? WM_KEYUP : WM_KEYDOWN, (WPARAM)vk, l);
    return 1;
}
int32_t windowtests_send_focus(void *hwnd, int32_t focused) {
    SendMessageW((HWND)hwnd, focused ? WM_SETFOCUS : WM_KILLFOCUS, 0, 0);
    return 1;
}
#else
int32_t windowtests_is_window(void *hwnd) { (void)hwnd; return -1; }
int32_t windowtests_is_visible(void *hwnd) { (void)hwnd; return -1; }
int32_t windowtests_win32(void) { return 0; }
int32_t windowtests_is_foreground(void *hwnd) { (void)hwnd; return -1; }
int32_t windowtests_focus_on_show(void *hwnd) { (void)hwnd; return -1; }
void windowtests_shown_next(int32_t shown) { (void)shown; }
int32_t windowtests_send_key(void *hwnd, int32_t vk, int32_t up) { (void)hwnd; (void)vk; (void)up; return -1; }
int32_t windowtests_send_focus(void *hwnd, int32_t focused) { (void)hwnd; (void)focused; return -1; }
#endif

int32_t windowtests_cstr_is(const void *p, const char *s) { return p != NULL && strcmp((const char *)p, s) == 0; }
