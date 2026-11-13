# @kama/window roadmap

**The order of work, and nothing else.** One row per item: what it is, and a link to its reasoning, in
[ROADMAP_DETAIL.md](ROADMAP_DETAIL.md), or, for a departure from kama's design, in
[AGENTS.window.md](AGENTS.window.md). What the package is lives in [SPEC.md](SPEC.md), and its history in the git log.

> **Keep this file short.** `tools/check-roadmap.sh` holds the split, as kama's and friendly-fire-department's guards
> do: a line ceiling, a ceiling per row, every detail section scheduled by a row, and every id valid.
>
> **`WIN-<n>` is a permanent id, not a position.** It is assigned once, from the counter below, which the same edit
> bumps; it is never reused or renumbered, and a shipped row's id retires with it.

**Next id: WIN-25**

**The goal is the browser's platform APIs, native at parity** ([AGENTS.window.md](AGENTS.window.md#what-this-is)): what
a page can do with a canvas, a program can do with a window on every platform.

## NOW: waiting on kama; kyoten's order goes on in `@kama/gpu` (GPU-4)

| id | item | why |
|---|---|---|
| WIN-24 | **The driver retired**: trusted input and the pinned Chrome for Testing under `kama run`, once kama gives them | [detail](ROADMAP_DETAIL.md#retire-driver) |

## LATER: the rest of the browser's platform APIs, and every platform

| id | item | why |
|---|---|---|
| WIN-23 | **SDL's own switch from XInput to GameInput**: when an SDL release has it, the vendored SDL moves to it and the gamepads' stopgap goes | [detail](ROADMAP_DETAIL.md#sdl-gameinput-fix) |
| WIN-22 | **A pad's motion sensors, touchpad and lights**: natively through SDL3, on the web only through WebHID | [detail](ROADMAP_DETAIL.md#pad-sensors) |
| WIN-5 | **Text input with IME composition** | [detail](ROADMAP_DETAIL.md#ime) |
| WIN-17 | **Keyboards beyond the US layout**: their extra keys named, and each key's label in the player's layout | [detail](ROADMAP_DETAIL.md#international-keys) |
| WIN-6 | **The clipboard** | [detail](ROADMAP_DETAIL.md#clipboard) |
| WIN-7 | **Cursor shapes** | [detail](ROADMAP_DETAIL.md#cursor-shapes) |
| WIN-8 | **Fullscreen** | [detail](ROADMAP_DETAIL.md#fullscreen) |
| WIN-9 | **Several windows natively, tested**: each window's events in its own queue | [detail](ROADMAP_DETAIL.md#several-windows) |
| WIN-10 | **File drop** | [detail](ROADMAP_DETAIL.md#file-drop) |
| WIN-11 | **Touch and pen natively**, where GLFW gives only a mouse | [detail](ROADMAP_DETAIL.md#native-pointers) |
| WIN-12 | **Linux and macOS**: X11, Wayland and Cocoa handles, run and tested there and on the Steam Deck | [detail](ROADMAP_DETAIL.md#desktop-platforms) |
| WIN-13 | **Android**: the window from `std::app`'s `SurfaceReady` to `SurfaceLost`, its input, and focus from the lifecycle | [AGENTS.window.md, item 7](AGENTS.window.md#where-this-departs-from-kamas-design-s8-input) |
| WIN-19 | **Every pointer sample**, for a drawing program: coalesced and predicted events, and high-rate input natively | [detail](ROADMAP_DETAIL.md#pointer-samples) |
| WIN-14 | **GLFW vendored** in the package, in place of MSYS2's and the system's | [detail](ROADMAP_DETAIL.md#vendored-glfw) |
| WIN-15 | **The first publish**, before `@kama/gpu`'s: `tests/` green on every target the package claims, the web in Chromium and Firefox, the stopgaps gone, GLFW vendored | [detail](ROADMAP_DETAIL.md#publish) |
