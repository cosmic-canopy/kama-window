# @kama/window roadmap

**The order of work, and nothing else.** One row per item: what it is, and a link to its reasoning, in
[ROADMAP_DETAIL.md](ROADMAP_DETAIL.md), or, for a departure from kama's design, in
[AGENTS.window.md](AGENTS.window.md). What the package is lives in [SPEC.md](SPEC.md), and its history in the git log.

> **Keep this file short.** `tools/check-roadmap.sh` holds the split, as kama's and friendly-fire-department's guards
> do: a line ceiling, a ceiling per row, every detail section scheduled by a row, and every id valid.
>
> **`WIN-<n>` is a permanent id, not a position.** It is assigned once, from the counter below, which the same edit
> bumps; it is never reused or renumbered, and a shipped row's id retires with it.

**Next id: WIN-17**

**The goal is the browser's platform APIs, native at parity** ([AGENTS.window.md](AGENTS.window.md#what-this-is)): what
a page can do with a canvas, a program can do with a window on every platform.

## NOW: the last stopgap that changes the API

| id | item | why |
|---|---|---|
| WIN-2 | **Held keys and buttons are released on a focus change**: the window tracks what is held, and posts each up before `Focus`, either way, on every backend | [AGENTS.window.md, item 5](AGENTS.window.md#where-this-departs-from-kamas-design-s8-input) |

## NEXT: the web half, then gamepads

| id | item | why |
|---|---|---|
| WIN-3 | **The web half**: a window on a canvas, `KeyboardEvent.code` to HID, the device pixel ratio and its changes, pointer lock, and W3C Pointer Events for mouse, touch and pen | [detail](ROADMAP_DETAIL.md#web-half) |
| WIN-4 | **Gamepads**, on the web and natively, released on a focus change by WIN-2's rule | [detail](ROADMAP_DETAIL.md#gamepads) |

## LATER: the rest of the browser's platform APIs, and every platform

| id | item | why |
|---|---|---|
| WIN-5 | **Text input with IME composition** | [detail](ROADMAP_DETAIL.md#ime) |
| WIN-6 | **The clipboard** | [detail](ROADMAP_DETAIL.md#clipboard) |
| WIN-7 | **Cursor shapes** | [detail](ROADMAP_DETAIL.md#cursor-shapes) |
| WIN-8 | **Fullscreen** | [detail](ROADMAP_DETAIL.md#fullscreen) |
| WIN-9 | **Several windows natively, tested**: each window's events in its own queue | [detail](ROADMAP_DETAIL.md#several-windows) |
| WIN-10 | **File drop** | [detail](ROADMAP_DETAIL.md#file-drop) |
| WIN-11 | **Touch and pen natively**, where GLFW gives only a mouse | [detail](ROADMAP_DETAIL.md#native-pointers) |
| WIN-12 | **Linux and macOS**: X11, Wayland and Cocoa handles, run and tested there and on the Steam Deck | [detail](ROADMAP_DETAIL.md#desktop-platforms) |
| WIN-13 | **Android**: the window from `std::app`'s `SurfaceReady` to `SurfaceLost`, its input, and focus from the lifecycle | [AGENTS.window.md, item 7](AGENTS.window.md#where-this-departs-from-kamas-design-s8-input) |
| WIN-14 | **GLFW vendored** in the package, in place of MSYS2's and the system's | [detail](ROADMAP_DETAIL.md#vendored-glfw) |
| WIN-15 | **The first publish**, before `@kama/gpu`'s: `tests/` green on every target the package claims, the web included, the stopgaps gone, GLFW vendored | [detail](ROADMAP_DETAIL.md#publish) |
