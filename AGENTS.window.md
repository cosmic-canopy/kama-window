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

## Where this departs from kama's design (s8-input)

**The package is standalone and held to production grade, and its first consumer drives what that means** (the
owner, 2026-11-08: "The bar is production grade general solutions"). kama's design is where it started. Each departure
is recorded here with its reason, and the owner relays them to kama. **Kept** marks the production answer. A
**stopgap** is fixed before the first publish, and says what replaces it. Not departures but not built yet: the web
half, gamepads, vendored GLFW, and native handles beyond Win32 (X11, Wayland, Cocoa).

1. **Kept: the ring is C for both kinds of window.** A headless window is the ring with no GLFW window behind it, so one
   queue is tested and a headless test never initialises GLFW (no display needed). GLFW 3.4's null platform was not
   used: it is chosen once per `glfwInit`, for the whole process.
2. **Kept: what coalesces, and how.** Moves keep the last position and sum the motion; scrolls sum; resizes and scale
   changes keep the last. A full ring keeps one slot for a `Dropped(n)` record, so the drop is reported where it
   happened, before anything that arrives once the reader drains it.
3. **Kept: `Key` is named by the DOM's `KeyboardEvent.code`** (`KeyA`, `Digit1`, `ArrowUp`, `ShiftLeft`) and
   numbered by the HID usage id, as the design says.
4. **Kept: `nativeHandle()` returns a `NativeWindow` value** (a kind, a display or module, a window), the shape a GPU
   surface needs on each system; `size()` and `framebufferSize()` return a `Size`.
5. **Kept: losing the focus while locked unlocks the pointer,** as a browser does on blur. **Stopgap: held keys are
   not released on focus loss;** `Focus(false)` is the only signal, so every consumer must release them itself.
   **The production answer:** the window tracks what is held. On a focus change, either way, it posts a `KeyUp` for
   each held key and a button-up for each held mouse button, then the `Focus` event, on every platform; once gamepads
   land, each held button and deflected axis is released too. Alt-tab or a blurred tab never delivers the key-up, so
   without this a consumer is left with a stuck key. Releasing on focus gain as well covers a remote desktop, which
   drops key-ups around focus changes, and a key really still held comes back with its next repeat. Both games do
   this in their platform layers: FFD's `ffd__on_blur` (`gfx/src/platform/ffd_platform.h`) and the-mallow-man's
   `release_all_synth` (`gfx/src/platform/tmm_platform.c`).
6. **Stopgap: a window's native record is not reference-counted,** so a GPU surface made on it must be dropped first.
   **The production answer:** the record counts references, and a surface holds one (`@kama/gpu`'s item 9), so the
   two drop safely in either order.

## Where the truth is

- **The design started as kama's:** `../kama/docs/ROADMAP_DETAIL.md` § 8, at the anchor `s8-input`. Read it before
  writing code. A departure the production bar needs is made here and recorded above with its reason.
- **kyoten builds it, as its first consumer** (ruled by kama on 2026-11-02; kyoten's KYR-51), and drives what
  production grade means for it (the owner, 2026-11-08). kyoten's sessions do the work, and the owner relays to kama.
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
