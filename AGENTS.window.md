# @kama/window — what is true in THIS repo

The two generated files carry the general rules: `AGENTS.md` is the language, and `AGENTS.package.md` is the half a
publishable library needs. `kama agents install` writes both, and a re-install rewrites them, so they are **not**
hand-edited. This file is the project-specific third, and it is the one to edit.

## What this is

**A window and its events, a first-party package** (kama KR-26; it was decided on 2026-10-16 that it is not std).
`Window` comes several at a time, with a headless mode and `post`. `Event` arrives in one coalescing queue that reports
`Dropped(n)`. Keys are physical, with `Text` beside them, `pump()` is explicit, and gamepads are in.

**As complete as the browser's platform APIs, native at parity** (the owner, 2026-11-10; kyoten's
`../kyoten/docs/design/architecture.md` § Decided, how the libraries grow). Web first means the browser's APIs are the
checklist, beyond what kyoten's hello cube needs (a window or canvas, physical keys, a gamepad, a headless window):
- pointer and touch;
- text input with IME composition;
- the clipboard;
- cursor shapes;
- fullscreen and pointer lock;
- DPI changes;
- several windows, natively;
- file drop.

Each document keeps to one purpose (the owner, 2026-11-10): [SPEC.md](SPEC.md) is what the package is,
[ROADMAP.md](ROADMAP.md) the order of work and [ROADMAP_DETAIL.md](ROADMAP_DETAIL.md) each row's reasoning, and this
file the agent's brief and the departures below, the record kama reads.

## Where things stand

**[SPEC.md](SPEC.md) is what the package is, and [ROADMAP.md](ROADMAP.md) what comes next.** In short (2026-11-10, kama
0.9.911): a system window natively on Windows and a headless one on every target, the web half and gamepads not built
yet, and WIN-1 and WIN-2, the stopgaps, next. `tools/test.sh` passes before every commit.

## Where this departs from kama's design (s8-input)

**The package is standalone and held to production grade, and its first consumer drives what that means** (the
owner, 2026-11-08: "The bar is production grade general solutions"). kama's design is where it started. Each departure
is recorded here with its reason, and the owner relays them to kama. **Kept** marks the production answer. A
**stopgap** is fixed before the first publish, and says what replaces it. Not departures but not built yet: the web
half, gamepads, vendored GLFW (FFD vendors 3.4 in `gfx/third_party/glfw`), and native handles beyond Win32 (X11,
Wayland, Cocoa). kama reviewed this list on 2026-11-08 (`../kama-notes-for-kyoten.md`); its points are folded in below.

1. **Kept: the ring is C for both kinds of window.** A headless window is the ring with no GLFW window behind it, so one
   queue is tested and a headless test never initialises GLFW (no display needed). GLFW 3.4's null platform was not
   used: it is chosen once per `glfwInit`, for the whole process.
2. **Kept: what coalesces, and how.** Moves keep the last position and sum the motion; scrolls sum; resizes and scale
   changes keep the last. A full ring keeps one slot for a `Dropped(n)` record, so the drop is reported where it
   happened, before anything that arrives once the reader drains it. **Fixed 2026-11-08** (kama's review, reproduced
   by a test first): an event was counted into a trailing `Dropped` record even after the reader had made room, so a
   key up arriving then was lost. It now goes after the drop, and only an event that finds no free slot but the
   reserved one is counted in.
3. **Kept: `Key` is named by the DOM's `KeyboardEvent.code`** (`KeyA`, `Digit1`, `ArrowUp`, `ShiftLeft`) and
   numbered by the HID usage id, as the design says.
4. **Kept: `nativeHandle()` returns a `NativeWindow` value** (a kind, a display or module, a window), the shape a GPU
   surface needs on each system; `size()` and `framebufferSize()` return a `Size`.
5. **Kept: losing the focus while locked unlocks the pointer,** as a browser does on blur. **Stopgap, WIN-2: held keys are
   not released on focus loss;** `Focus(false)` is the only signal, so every consumer must release them itself.
   **The production answer:** the window tracks what is held. On a focus change, either way, it posts a `KeyUp` for
   each held key and a button-up for each held mouse button, then the `Focus` event, on every platform; once gamepads
   land, each held button and deflected axis is released too. Alt-tab or a blurred tab never delivers the key-up, so
   without this a consumer is left with a stuck key. Releasing on focus gain as well covers a remote desktop, which
   drops key-ups around focus changes, and a key really still held comes back with its next repeat. Both games do
   this in their platform layers: FFD's `ffd__on_blur` (`gfx/src/platform/ffd_platform.h`) and the-mallow-man's
   `release_all_synth` (`gfx/src/platform/tmm_platform.c`).
   - **Today the backends already differ** (kama's review): GLFW releases held keys and buttons on focus loss itself,
     after the focus callback and never on gain (GLFW 3.4 `_glfwInputWindowFocus`, read in FFD's vendored
     `src/window.c`), so natively `Focus(false)` arrives before the key ups. A browser sends no `keyup` after `blur`.
   - **So the ring keeps the held set itself,** releases before posting `Focus`, and drops an up for a key it does not
     hold. GLFW's own releases then find nothing held, and every backend behaves the same way.
6. **Stopgap, WIN-1: a window's native record is not reference-counted,** so a GPU surface made on it must be dropped first.
   **The production answer:** the record counts references, and a surface holds one (`@kama/gpu`'s item 9), so the
   two drop safely in either order. On Android the OS takes the window at `SurfaceLost` whatever is counted;
   `std::app` says so first, and every surface made on it is dropped in that call.
7. **Missing: pointers other than a mouse (WIN-3, WIN-11), and Android (WIN-13)** (kama's review).
   - **Pointer events.** A phone, a tablet and a pen need touch and pen input as well as the mouse. **The production
     answer is W3C Pointer Events,** which the web half receives anyway: one stream for mouse, touch and pen, each
     event carrying a pointer id, its type, buttons, pressure and tilt, with down, move, up and cancel. The mouse
     events become pointer events of type mouse. They are designed with the web half (W2), since the browser's API is
     the design's reference.
   - **Android.** `NativeKind` gains `Android` (an `ANativeWindow*`). The window wraps `std::app::android::window()`
     from `SurfaceReady` to `SurfaceLost`, reads input through `std::app::android::setInputHandler`, and turns the
     program's `FocusGained` and `FocusLost` into `Focus`. The program hands it those lifecycle events: `std::app` does
     not route them to packages behind its back. Rotation is not a lifecycle event; the size is read from the window.
     An undrawn Android window gets no touches (`NO_INPUT_CHANNEL`, measured by kama on the emulator).

## Where the truth is

- **This package's own documents,** above: the SPEC, the ROADMAP and its detail, and the CHANGELOG.
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
- **Test-driven** (the owner, 2026-11-08). A change starts with a test that fails without it, and a bug is fixed only
  after a test reproduces it. `tools/test.sh` passes before every commit.
- **Dependencies are injected** (the owner, 2026-11-08). Code takes what it depends on as a parameter or a contract,
  so a test hands it a fake; the headless window is that fake for a window's consumers.
- **Commit on the branch, and push only when the owner asks.**

## Build

    kama check kama.json
    tools/test.sh                                   # the gate: the roadmap, kama check, the tests natively and under node
    tools/check-roadmap.sh                          # ROADMAP.md stays the order, its detail stays scheduled

On Windows, drive kama from msys2 UCRT64 (kama's `docs/platforms/windows.md`). Where `kama` is not on the `PATH`, use
`../kama/out/UCRT64-x86_64/kama.exe`.
