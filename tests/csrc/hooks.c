// hooks.c: hooks.h's bodies. Win32 answers natively on Windows; elsewhere every question answers -1.
#include "hooks.h"

#include <signal.h>

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
int32_t windowtests_resize(void *hwnd, int32_t width, int32_t height) {
    SetWindowPos((HWND)hwnd, NULL, 0, 0, width, height, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
    return 1;
}

// SDL's virtual pads, each held open here so its state can be set.
#include <SDL3/SDL.h>
#define VIRTUAL_PADS 8
static struct {
    SDL_JoystickID id;
    SDL_Joystick *joystick;
    int32_t rumbles;                     // how many rumble requests SDL handed the pad, dual and trigger
    int32_t low, high, left, right;      // the last ones' motors, as SDL handed them, 0 to 65535
} g_virtual[VIRTUAL_PADS];

static bool SDLCALL virtual_rumble(void *userdata, Uint16 low, Uint16 high) {
    int i = (int)(intptr_t)userdata;
    g_virtual[i].rumbles += 1; g_virtual[i].low = low; g_virtual[i].high = high;
    return true;
}

static bool SDLCALL virtual_rumble_triggers(void *userdata, Uint16 left, Uint16 right) {
    int i = (int)(intptr_t)userdata;
    g_virtual[i].rumbles += 1; g_virtual[i].left = left; g_virtual[i].right = right;
    return true;
}
static Uint16 g_virtual_product = 0x7000;   // each pad its own product, so SDL keeps each one's name: it keys a pad's mapping, name and all, by vendor and product

static SDL_Joystick *virtual_of(uint32_t id) {
    for (int i = 0; i < VIRTUAL_PADS; i++) if (g_virtual[i].joystick != NULL && g_virtual[i].id == id) return g_virtual[i].joystick;
    return NULL;
}

uint32_t windowtests_virtual_pad(const char *name, int32_t trigger_motors) {
    if (!SDL_WasInit(SDL_INIT_GAMEPAD)) return 0;
    for (int i = 0; i < VIRTUAL_PADS; i++) {
        if (g_virtual[i].joystick != NULL) continue;
        SDL_VirtualJoystickDesc desc;
        SDL_INIT_INTERFACE(&desc);
        desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
        desc.naxes = SDL_GAMEPAD_AXIS_COUNT;
        desc.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
        desc.name = name;
        desc.vendor_id = 0x1209;
        desc.product_id = g_virtual_product++;
        desc.Rumble = virtual_rumble;
        desc.RumbleTriggers = trigger_motors ? virtual_rumble_triggers : NULL;
        desc.userdata = (void *)(intptr_t)i;
        g_virtual[i].rumbles = 0;
        g_virtual[i].low = g_virtual[i].high = g_virtual[i].left = g_virtual[i].right = 0;
        SDL_JoystickID id = SDL_AttachVirtualJoystick(&desc);
        if (id == 0) return 0;
        g_virtual[i].joystick = SDL_OpenJoystick(id);
        if (g_virtual[i].joystick == NULL) { SDL_DetachVirtualJoystick(id); return 0; }
        g_virtual[i].id = id;
        return (uint32_t)id;
    }
    return 0;
}

int32_t windowtests_virtual_button(uint32_t id, int32_t button, int32_t down) {
    SDL_Joystick *j = virtual_of(id);
    return j != NULL && SDL_SetJoystickVirtualButton(j, button, down != 0) ? 1 : 0;
}

int32_t windowtests_virtual_axis(uint32_t id, int32_t axis, int32_t value) {
    SDL_Joystick *j = virtual_of(id);
    return j != NULL && SDL_SetJoystickVirtualAxis(j, axis, (Sint16)value) ? 1 : 0;
}

int32_t windowtests_sdl_running(void) { return SDL_WasInit(SDL_INIT_GAMEPAD) != 0 ? 1 : 0; }

int32_t windowtests_virtual_rumbled(uint32_t id, int32_t *low, int32_t *high, int32_t *left, int32_t *right) {
    for (int i = 0; i < VIRTUAL_PADS; i++) {
        if (g_virtual[i].joystick == NULL || g_virtual[i].id != id) continue;
        *low = g_virtual[i].low; *high = g_virtual[i].high; *left = g_virtual[i].left; *right = g_virtual[i].right;
        return g_virtual[i].rumbles;
    }
    return -1;
}

void windowtests_virtual_detach(uint32_t id) {
    for (int i = 0; i < VIRTUAL_PADS; i++) {
        if (g_virtual[i].joystick == NULL || g_virtual[i].id != id) continue;
        SDL_CloseJoystick(g_virtual[i].joystick);
        g_virtual[i].joystick = NULL;
        SDL_DetachVirtualJoystick(id);
    }
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
int32_t windowtests_resize(void *hwnd, int32_t width, int32_t height) { (void)hwnd; (void)width; (void)height; return -1; }
uint32_t windowtests_virtual_pad(const char *name, int32_t trigger_motors) { (void)name; (void)trigger_motors; return 0; }
int32_t windowtests_virtual_button(uint32_t id, int32_t button, int32_t down) { (void)id; (void)button; (void)down; return 0; }
int32_t windowtests_virtual_axis(uint32_t id, int32_t axis, int32_t value) { (void)id; (void)axis; (void)value; return 0; }
void windowtests_virtual_detach(uint32_t id) { (void)id; }
int32_t windowtests_sdl_running(void) { return -1; }
int32_t windowtests_virtual_rumbled(uint32_t id, int32_t *low, int32_t *high, int32_t *left, int32_t *right) {
    (void)id; (void)low; (void)high; (void)left; (void)right; return -1;
}
#endif

int32_t windowtests_cstr_is(const void *p, const char *s) { return p != NULL && strcmp((const char *)p, s) == 0; }

void *windowtests_sigint_handler(void) {
    void (*now)(int) = signal(SIGINT, SIG_DFL);
    signal(SIGINT, now);
    return (void *)now;
}
