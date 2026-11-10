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
0.9.911): a system window natively on Windows and a headless one on every target, and a window's native record counted,
so a GPU surface holds it (WIN-1, built). The web half and gamepads are not built yet, and WIN-2, the last stopgap, is
next. `tools/test.sh` passes before every commit.

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
6. **Built on 2026-11-10: a window counts references to its native record (WIN-1)** (kama 0.9.911), replacing the
   stopgap below. Natively on Windows (GLFW 3.5.1) and under node, test-first:
   - **18 new cases:** 12 headless on every leg, and 6 on a system window;
   - **8 of 9 mutations caught.** The ninth, an adopted handle releasing its null record, is equivalent, since the C
     release ignores null;
   - **the stopgap's own behaviour** (the window destroyed at close) turns two cases red: the HWND "was destroyed"
     while its handle held it.

   **Stopgap, replaced: the native record was not counted,** so a GPU surface made on a window had to be dropped
   first, a rule kept only in documentation in a safe API. kama drops fields in reverse declaration order, so a type
   declaring `Surface surface; Window window;` destroyed the HWND under a live surface.

   **Measured first, on 2026-11-10** (a C probe in kyoten's `.scratch/win1-window/`, one process per case: GLFW 3.5.1
   and wgpu-native v29.0.1.1 over Vulkan, on an AMD Radeon 880M):

   | a configured surface, after its window is | wgpu-native |
   |---|---|
   | destroyed, then the surface released | clean |
   | destroyed, then a frame acquired and presented | `SuccessSuboptimal`, and present answers Success: frames go nowhere, and nothing says the window is gone |
   | **destroyed, then configured** (a resize after a close) | **the process aborts** (exit 127): "Error in wgpuSurfaceConfigure: Validation Error. Surface does not support the adapter's queue family" |
   | destroyed while a frame is held, which is then presented | Success |
   | hidden | acquire, present and configure all work as on a shown window |
   | hidden, the surface released, then destroyed (this item's order) | clean |
   | gone with `glfwTerminate` | `glfwTerminate` destroys the HWND, and acquire then answers `SuccessSuboptimal` |

   On the web, read from emdawnwebgpu's `library_webgpu.js` and not run, `wgpuInstanceCreateSurface` finds the canvas
   by its selector during the call, and the `GPUCanvasContext` then holds the canvas itself.

   **Decided by the owner on 2026-11-10 (kyoten's tenth session), each as recommended,** by approving the session's
   plan, after the probe overturned none of them:
   1. **What holds the count: `NativeWindow`, not a `Copyable` `Window`.** A `Window` is its event queue, so its copies
      would split the events between them, and a surface's copy would keep the window open, shown and taking clicks no
      one reads, after the program dropped its own. winit's `Arc<Window>` works because its events come from the loop.
      - `window.nativeHandle()` gives a `NativeWindow` that holds the record, and a copy is another hold;
      - its handles stay valid for as long as it lives, for every consumer: a GPU surface, a GL context, a video
        overlay;
      - the count is kept in the C record, as webgpu.h's `AddRef` and `Release` are, and as kama's design has it ("the
        seam keeping a C ref on the window"). The browser's model is the same: a `GPUCanvasContext` holds its `canvas`.

      `NativeWindow` is a resource answering `kind()`, `display()`, `window()` and `isOpen()`. `NativeWindow.of` is
      `unsafe ctor adopt(kind:, display:, window:)`, handles of a window made elsewhere, which holds nothing and is
      always open, as `@kama/gpu`'s `Device.adopt` does.
   2. **What a closed window does to a surface that still holds it.**
      - **Dropping the `Window` closes the window as the user sees it, at once.** Its callbacks go, so the focus the
        hide takes away reaches no one, and it is hidden.
      - **Its GLFW window and HWND go with the record's last holder,** and GLFW with the last record that had one, so
        a held window stays valid while another window closes.
      - **A holder reads `isOpen()` as false.**
      - **A surface on it (`@kama/gpu`'s GPU-2) never reaches the window again:** `acquire()` and `configure()` refuse
        with a new `Closed`, `capabilities()` is refused, and a frame acquired before the close is dropped, never
        presented.
      - **Not `Lost`:** the package documents it as "configure again", and a configure would succeed on the hidden
        window, so the loop would spin.
      - **Not drawing on into the hidden window,** as a browser draws into a canvas taken out of the page: a closed
        native window never comes back, and Android forbids touching one (4).
   3. **The headless window and the web's canvas: one record and one count on every kind.**
      - **Headless:** `nativeHandle()` holds the record with kind `None`, so every counting rule is tested with no
        display, natively and under node.
      - **The canvas (WIN-3):** its record keeps the selector's string, which `window()` points at, alive while held.
        Closing removes the html5 callbacks and never the canvas, which is the page's. Question 2's refusals hold
        there too.
   4. **Android's `SurfaceLost`: the shape now, built with WIN-13.**
      - **At `SurfaceLost`, which the program hands the window** (item 7), the window closes its current record.
        That is question 2's state, so a surface the program failed to drop never touches the window.
      - **At `SurfaceReady` the `Window` takes a new record.** A `Window` owns its current record, and a record, once
        closed, stays closed.
      - **kama's rule stands:** drop the surfaces in that call.

      Read, not measured: Android's `ANativeWindow` is itself counted, and Vulkan's Android surface acquires it, so a
      late release should be memory-safe. It is measured with WIN-13.

   **Where it lives:**
   - `csrc/winseam.c`: a record's `holds` and `closed`, `winseam_hold`, `winseam_release` and `winseam_closed`, and
     `winseam_close` as close-then-release. Closing natively detaches the callbacks and hides the window; the GLFW
     window and GLFW's count go at the last release;
   - `src/window.kama`: `NativeWindow`;
   - `tests/src/handles.kama`, with `winseam_live_records()` counting the records alive, and `tests/csrc/hooks.c`
     asking Win32 whether an HWND is still a window and shown.

   **Stopgap, in `tests/` alone: the tests open their system windows unfocused** (`tests/src/quiet.kama`, GLFW's
   `GLFW_FOCUSED` and `GLFW_FOCUS_ON_SHOW` hints set before each open). A test run took the keyboard from whoever
   was typing (the owner, 2026-11-10: "it fights for focus"). The probe also exited 127 at process end in 9 of 50
   runs while its windows took the focus, and in none of 50 once they opened unfocused; the cause was not found. The
   hook initialises GLFW outside the package's count, which masked one mutation until it was made faithful. **The
   production answer is an open-time setting** (winit's `with_active`, GLFW's `GLFW_FOCUSED`), focused by default, so
   the tests ask for an unfocused window through the API. It is the owner's to decide.
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
