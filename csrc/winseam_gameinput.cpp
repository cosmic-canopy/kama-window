// winseam_gameinput.cpp: WIN-4's stopgap (WIN-23). Whether SDL can use GameInput on this machine, by SDL's own test
// (`SDL_InitGameInput`: Microsoft's runtime present, version 1.1 or newer, no driver SDL knows to break it), so the seam
// turns XInput off before SDL starts. SDL 3.4 with both on adds a replugged pad twice (ROADMAP_DETAIL.md § WIN-4's
// measurements); SDL's main branch makes the same switch inside SDL, and this file goes when a release does.
//
// C++, because SDL's GameInput layer is, and its header with it. Windows only (kama.json's gate).
#include "SDL_internal.h"
#include "core/windows/SDL_gameinput.h"

extern "C" int winseam_gameinput_usable(void) {
#ifdef HAVE_GAMEINPUT_H
    if (!SDL_InitGameInput(NULL)) {
        SDL_ClearError();
        return 0;
    }
    SDL_QuitGameInput();
    return 1;
#else
    return 0;
#endif
}
