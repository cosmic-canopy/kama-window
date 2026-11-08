# @kama/window — what is true in THIS repo

The two generated files carry the general rules: `AGENTS.md` is the language, and `AGENTS.package.md` is the half a
publishable library needs. `kama agents install` writes both, and a re-install rewrites them, so they are **not**
hand-edited. This file is the project-specific third, and it is the one to edit.

## What this is

**A window and its events, a first-party package** (kama KR-26; it was decided on 2026-10-16 that it is not std).
`Window` comes several at a time, with a headless mode and `post`. `Event` arrives in one coalescing queue that reports
`Dropped(n)`. Keys are physical, with `Text` beside them, `pump()` is explicit, and gamepads are in.

## Where things stand (2026-11-07, kama 0.9.874)

**The native window and the headless one work; the web window and gamepads do not exist yet.** Built (KR-26's commit 1):

- `csrc/winseam.{h,c}`: the event ring (fixed, coalescing, `Dropped(n)` in order through a reserved slot) for every
  window, and natively a GLFW window per ring through the window user pointer, with `glfwInit` counted.
- `src/window.kama`: `Window` (`open`, `headless`, `pump`, `pollEvent`, `post`, `size`, `framebufferSize`, `scale`,
  `setCursor`, `nativeHandle`), `Event`, `Key` (DOM `code` names, HID ids), `MouseButton`, `Mods`, `CursorMode`.
- `tests/`: the queue's order, coalescing and drops on a headless window, the GLFW key table swept through the
  externed translator (118 keys mapped, none twice), and a system window (33 cases natively, debug and release; 17
  under node, where opening a window refuses until the web half exists). `tools/test.sh` runs it all.

Next: the web half (html5 callbacks on `#canvas`, `KeyboardEvent.code` to HID, the device pixel ratio, pointer lock),
then gamepads. Before publishing, GLFW is vendored (AGENTS.package.md): today Windows links MSYS2's static
`libglfw3.a` (3.5.1) and Linux and macOS a system `glfw`, untried, and `nativeHandle()` answers on Win32 only.

## Amendments to relay to kama's design (s8-input)

What the build needed or chose that the design does not say, for kama to rule; nothing here is decided from this
repository.

1. **The ring is C for both kinds of window.** A headless window is the ring with no GLFW window behind it, so one
   queue is tested and a headless test never initialises GLFW (no display needed). GLFW 3.4's null platform was not
   used: it is chosen once per `glfwInit`, for the whole process.
2. **What coalesces, and how:** moves keep the last position and sum the motion; scrolls sum; resizes and scale
   changes keep the last. A full ring keeps one slot for a `Dropped(n)` record, so the drop is reported where it
   happened, before anything that arrives once the reader drains it.
3. **`Key` is named by the DOM's `KeyboardEvent.code`** (`KeyA`, `Digit1`, `ArrowUp`, `ShiftLeft`) and numbered by
   the HID usage id, as the design says.
4. **`nativeHandle()` returns a `NativeWindow` value** (a kind, a display or module, a window), the shape a GPU
   surface needs on each system; `size()` and `framebufferSize()` return a `Size`.
5. **Losing the focus while locked unlocks the pointer,** as a browser does on blur. Held keys are not released by
   the window on focus loss (the-mallow-man does that); `Focus(false)` is the signal, and the consumer releases.

## Where the truth is

- **The design is kama's:** `../kama/docs/ROADMAP_DETAIL.md` § 8, at the anchor `s8-input`. Read it before writing
  code. A change the build needs goes back to kama as an amendment of that section; it is not decided here.
- **kyoten builds it, as its first consumer** (ruled by kama on 2026-11-02; kyoten's KYR-51). kyoten's sessions do the
  work, and the owner relays to kama.
- **The starting code** is the platform layers it replaces, in the owner's games. What each assumes is in
  `../kyoten/docs/research/harvest.md` § 2 (the input event vocabulary and the fold) and § 3 (the platform header:
  window, surface, keys, mouse and pads).

## Rules

- **Never edit `../kama` from here.** A compiler or std gap goes in [KAMA_GAPS.md](KAMA_GAPS.md), with a minimal repro
  and the kama version.
- **Name no engine.** Nothing here names kyoten or a game: a tool, an editor and a game use it alike.
- **Verify, don't trust.** Compile the snippet, run it, read the emitted C. A doc is not evidence, including this one.
- **Commit on the branch, and push only when the owner asks.**

## Build

    kama check kama.json

On Windows, drive kama from msys2 UCRT64 (kama's `docs/platforms/windows.md`). Where `kama` is not on the `PATH`, use
`../kama/out/UCRT64-x86_64/kama.exe`.
