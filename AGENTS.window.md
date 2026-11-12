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

**[SPEC.md](SPEC.md) is what the package is, and [ROADMAP.md](ROADMAP.md) what comes next.** In short (2026-11-11, kama
0.9.911): a system window natively on Windows, the page's canvas on the web, and a headless one on every target, and a
window's native record counted, so a GPU surface holds it (WIN-1, built), a window opens without taking the focus when
asked (WIN-16, built), the window keeps what is held and releases it on a focus change (WIN-2, built), and the web half
with Pointer Events on every backend (WIN-3, built), and a window's pixel size through its handle (WIN-20, built, for
`@kama/gpu`'s GPU-3), and under `sizedByPage` `open` refuses a canvas the page sizes on neither axis (WIN-21, built).
Gamepads are built (2026-11-12, item 9): on the web through the Gamepad API, natively through SDL3, vendored, on
Windows so far. `tests/` runs natively, under node and in every headless browser present (a Chrome for Testing pinned
under `~/browsers`, Edge). The browser leg in Firefox (WIN-18) is next. `tools/test.sh` passes before every commit.

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
5. **Kept: losing the focus while locked unlocks the pointer,** as a browser does on blur. **Built on 2026-11-10: held
   keys and buttons released on a focus change (WIN-2)** (kama 0.9.911), replacing the stopgap below; how it was built
   is at this item's end.

   **Stopgap, replaced: held keys were not released on focus loss;** `Focus(false)` was the only signal, so every
   consumer had to release them itself. **The production answer:** the window tracks what is held. On a focus change, either way, it posts a `KeyUp` for
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

   **Measured first, on 2026-11-10** (a C probe in kyoten's `.scratch/win2-focus/`, one process per case, over MSYS2's
   static GLFW 3.5.1, the build this package links, whose source is not on this machine). Win32 messages were sent to a
   hidden window's HWND, so GLFW's own window procedure ran its paths while the window never had or took the focus:
   every case left it hidden and the foreground unchanged.

   | sent to the hidden window | what GLFW 3.5.1 reports |
   |---|---|
   | W down, then `WM_KILLFOCUS` | a press; the focus callback (0); **then** its own release of W, mods 0 |
   | W down, then `WM_SETFOCUS` | a press; the focus callback (1); nothing released |
   | W up, never down | nothing: GLFW drops an up for a key it holds as released |
   | W down twice, then up | a press, a **repeat**, a release |
   | Shift and W down, then `WM_KILLFOCUS` | the focus callback, then W's release, then Shift's, both mods 0 (GLFW's token order) |
   | Henkan down twice, Muhenkan down, `WM_KILLFOCUS`, Henkan up | each down a **press** of `GLFW_KEY_UNKNOWN` (−1), none released at the loss, and the up passed on |
   | the left button down, then `WM_KILLFOCUS` | a press, the focus callback, a release; **GLFW's mouse capture is set on the window, and its focus release leaves it set** |
   | the left button up with no down, then down twice, then up | all four passed on: GLFW filters no button |

   Each matches what GLFW 3.4's `src/input.c` and `src/window.c` say (read in friendly-fire-department's vendored copy).
   A mod bit cannot be sent this way: GLFW reads them from the thread's key state, which a sent message does not move,
   so a sent Shift press reports mods 0.

   **Decided by the owner on 2026-11-10 (kyoten's eleventh session), each as recommended,** after the probe overturned
   none of them:
   1. **One pairing rule, kept by the ring in C, so every backend has it.**
      - An up for a key or button the window does not hold is dropped. A browser delivers the `keyup` of a key pressed
        before the page had the focus (read, not run; WIN-3 measures it), and GLFW passes on a button's (measured).
      - **The window says what a repeat is, not the backend.** A `KeyDown` for a key not held is a press
        (`repeat: false`) and holds it, and one for a held key is a repeat. This is GLFW's own rule for keys
        (`_glfwInputKey`), on every backend. A key held across a focus gain therefore comes back as a press, though a
        browser may mark the system's repeat `repeat: true` (read, not run; WIN-3 measures it), so a consumer that
        ignores repeats still sees it.
      - A second down for a held button, with no up between, is dropped.
      - So every up the consumer sees follows its down, and every down it sees is followed in time by its up.
   2. **`Key::Unknown` is never held.** It names every key the table does not, so the window cannot pair its ups and
      downs; GLFW reports each down of one as a press and releases none (measured). Its events pass through as the
      backend sends them. The owner, on how one arises: kyoten will handle locale and localization, so the keys of
      keyboards beyond the US layout are to be named (WIN-17).
   3. **The release: held buttons first, then keys by ascending HID id, so the eight modifiers (224 to 231) go last;
      then what the focus change undoes (`PointerLock(false)` on a loss while locked); then `Focus`.** The action goes
      up before the modifier that chose it: an editor's Alt+drag ends as the orbit it began as, and S goes up before
      Ctrl. **Each up carries the modifier bits of the keys still held after it,** as a browser's `keyup` reads had the
      user let go in that order, and the lock bits (CapsLock, NumLock) as the last key or button event reported them,
      so a release never flips a lock. GLFW's own releases carry 0, which would contradict the next event: W's up
      would say Shift is not held, one event before Shift's own up. Asked on the way, and decided:
      - **Several keyboards and mice on one machine are not pursued** (the owner: "we probably shouldn't worry about
        multiple keyboards and mice if that isn't a thing the web could handle anyway. Controllers are the natural
        split screen solve"). Every system merges them into one keyboard and one cursor; a browser cannot tell them
        apart, and natively it would take each platform's raw device input.
      - **Focus handling is not configurable.** The `Focus` events always arrive, and what a program does with them (a
        pause screen, muting, running on for a networked game) is its own. The release of keys and buttons cannot be
        turned off: once the focus is lost the system sends the window no more of their events, so keeping them held
        could only leave them stuck. Unity's input package has that option (`BackgroundBehavior.IgnoreFocus`), and its
        own issue tracker says keys held at the focus loss then keep their pressed state. Whether a program reads pads
        while unfocused is a real choice, since a pad reports in the background: it is WIN-4's question.
   4. **The ring keeps room for an up of everything held.** An event other than such an up goes in only while a slot
      is left beyond one for each held key and button and the one for `Dropped`; otherwise it is counted into the drop.
      An up of a held key or button is never dropped, whether the system sent it or the window made it. A down that
      finds no room is counted into the drop and not held, so its later up is dropped too, and the key, if still held,
      comes back as a press with its next repeat. `Focus` stays an ordinary event: under overflow it may be counted
      into a `Dropped` after the ups, with the held state still right. `queueDrops` and `queueDropsPartial` fill the ring
      with `Text`, which no rule touches, so their counts stand; a new case holds the reserve.
   5. **A `post`ed event moves the held set,** as if the system had sent it, and a posted `Focus` releases first. So
      the rules are tested headless on every leg, and a replayed stream is idempotent: its recorded ups release their
      keys, and its `Focus` then finds nothing held.
   6. **GLFW's own releases are dropped by rule 1,** in either order (measured above: after the callback). A case in
      `tests/` holds it on a hidden system window through the same Win32 messages, keys only: a button down sets
      GLFW's mouse capture on the window (measured), so buttons are held by the headless cases.
   7. **`window.releaseHeld()`** posts the same ups as a focus change, in the same order with the same bits, and no
      `Focus`: a program's own release, as the-mallow-man's remote-play guard makes when a tick comes long after the
      last (`game/src/inputguard.kama`). One path in C serves both, headless too.

   The owner also asked whether a window's made-up ups could leave a weird state. **They cannot leave a key stuck
   down:** once the window releases a key, the system's own up for it is dropped, and its next down is a fresh press.
   **What can happen is the reverse, briefly:** a key still physically held after a focus change reads as up until the
   system repeats it or it is pressed again, and a mouse button, which never repeats, until it is pressed again. Asking
   the system which keys are down at a focus gain is not done: a browser cannot, and the release on gain exists for a
   remote desktop that drops key-ups, where the remote system itself still believes the key is down.

   **Built (WIN-2), test-first, natively on Windows and under node:**
   - **36 new cases** (97 natively, 69 under node): 33 headless on every leg in `tests/src/held.kama`, and 3 on a hidden
     system window, whose GLFW window procedure is handed `WM_KEYDOWN`, `WM_KILLFOCUS` and `WM_SETFOCUS` through the
     tests' Win32 hooks (`windowtests_send_key`, `windowtests_send_focus`). Of the first 31 written, 23 failed on the
     old code, the native two among them in GLFW's order (`Focus false`, then its `KeyUp`); the other 8 pass on it too
     and hold a rule a change could break. The 3 `releaseHeld` cases did not compile until it existed. 2 more were added
     at the boundaries, each for a mutation: a down with room for itself but not its up, and a button down into a full
     ring.
   - **31 of 31 mutations caught,** none refused by the compiler. One, a button down counted into the drop yet held,
     was missed at first: no case filled the ring and then pressed a button. The case was added and the mutation
     caught.
   - `queueDrops` and `queueDropsPartial` fill the ring with `Text`, which no held-set rule touches, and their counts
     stand (decision 4). Their late event, the regression from kama's review, is `Text` too: a held key's up now always
     has a slot.
   - **One detail decided in the building, within decision 4:** while the ring overflows, an up it owes goes before the
     trailing `Dropped` record, which stays last and goes on counting, so the ring needs only one slot for drops.

   **Where it lives:**
   - `csrc/winseam.c`: `take()`, which every event goes through, GLFW's callbacks and `winseam_post` alike: the held
     bitmap (a bit per HID id and per button), `held_count`, the lock bits, `release_held()` and the room `store()`
     keeps. `on_focus` is only the `Focus` record now; the unlock moved into `take()`, so a posted `Focus` gives a lock
     back too. `winseam_release_held` is the public call;
   - `src/window.kama`: `Window.releaseHeld()`, through `src/native/seam.kama`'s `releaseHeldInput`;
   - `tests/src/held.kama`, and the two hooks in `tests/csrc/hooks.c`.
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

   **Stopgap, replaced on 2026-11-10 (WIN-16): the tests set GLFW's focus hints themselves before each open**
   (`GLFW_FOCUSED` and `GLFW_FOCUS_ON_SHOW`). A test run took the keyboard from whoever was typing (the owner, 2026-11-10: "it fights for focus"). The probe also exited 127 at process end in 9 of 50
   runs while its windows took the focus, and in none of 50 once they opened unfocused; the cause was not found. The
   hook initialises GLFW outside the package's count, which masked one mutation until it was made faithful. **The
   production answer is an open-time setting** (winit's `with_active`, GLFW's `GLFW_FOCUSED`), focused by default, so
   the tests ask for an unfocused window through the API. **Decided** (the owner, 2026-11-10: "yes b sounds better"):
   an options value, `WindowOptions`, over a separate `Window.openUnfocused`.

   **Built (WIN-16):**
   - **The shape is kama's own** (its SPEC § Construction, KR-367; `std::net`'s `ListenOptions` and `connectWith`):
     `Window.openWith(title:, width:, height:, options:)` beside an unchanged `Window.open`, and `WindowOptions` a
     `type value` made by `make()`, its fields public with their defaults. Its one field so far is `focused`, true.
   - **Natively every open sets both hints from it, both ways.** GLFW keeps a hint until it terminates, so setting them
     only for an unfocused open would leave the next window unfocused too. At creation GLFW applies `GLFW_FOCUSED` to a
     shown window; `GLFW_FOCUS_ON_SHOW` is the window's attribute for a later show (GLFW 3.4's source as FFD vendors
     it, read).
   - **The tests never take the focus, even when the package is wrong.** The cases that need a window that would take
     the focus open it hidden and read its `FOCUS_ON_SHOW` from GLFW's own window property (`L"GLFW"`, in MSYS2's 3.5.1
     library). A window is shown only after they pass, unfocused, and checked not to be the foreground; once one is,
     the program shows no other. The suite reproduced the bug first with no window shown, and all 5 mutations were
     caught the same way. A sixth, dropping only `GLFW_FOCUSED`, is not run: catching it means a window takes the
     foreground once, and the foreground case holds it.
   - `@kama/gpu`'s tests open through it too, and its `examples/clear` leaves the focus where it is under `--frames`.
   - On the web, `focused` is WIN-3's: a canvas takes the focus only when the page gives it.
7. **Missing: pointers other than a mouse (WIN-3, WIN-11), and Android (WIN-13)** (kama's review).
   - **Pointer events.** A phone, a tablet and a pen need touch and pen input as well as the mouse. **The production
     answer is W3C Pointer Events,** which the web half receives anyway: one stream for mouse, touch and pen, each
     event carrying a pointer id, its type, buttons, pressure and tilt, with down, move, up and cancel. The mouse
     events become pointer events of type mouse. **Decided with the web half on 2026-11-11** (item 8, decision 2).
   - **Android.** `NativeKind` gains `Android` (an `ANativeWindow*`). The window wraps `std::app::android::window()`
     from `SurfaceReady` to `SurfaceLost`, reads input through `std::app::android::setInputHandler`, and turns the
     program's `FocusGained` and `FocusLost` into `Focus`. The program hands it those lifecycle events: `std::app` does
     not route them to packages behind its back. Rotation is not a lifecycle event; the size is read from the window.
     An undrawn Android window gets no touches (`NO_INPUT_CHANNEL`, measured by kama on the emulator).
8. **Built on 2026-11-11: the web half (WIN-3), a window on a canvas, and Pointer Events on every backend** (kama
   0.9.911). How it was decided is below, and how it was built at this item's end.

   **Measured first, on 2026-11-11** (kyoten's `.scratch/win3-web/`): headless Edge 157 driven over the DevTools
   protocol from node 24, with no npm packages, and the browser's own trusted input. A plain page was measured, and so
   was a kama program built for the web (kama 0.9.911, emsdk 6.0.13) whose path dependency ships a JS library.
   - **The harness.** `--dump-dom` returns at the page's load, before emscripten's `main` has run, so it cannot run a
     test program. A DevTools-protocol driver can: it reads the console as it comes, sees an abort, and sends input
     between ticks.
   - **emsdk 6.0.13's `html5.h` has no Pointer Events, no ResizeObserver and no device-pixel-ratio change** (read). A
     dependency's `jsLibraries` file links into the program, and a synthetic `dispatchEvent` reaches its listeners
     synchronously.
   - **A listener that calls into C after the program has quit aborts the page:** "native function called after
     runtime exit". A queue in JS that C drains never calls into the program.
   - **Keys go to the focused element.** With a text field focused the canvas gets none. A tab switch gives the canvas
     `blur` before the window's, and no `keyup`; coming back gives the canvas its focus again. A click outside the
     canvas blurs it, Tab leaves it unless its `keydown` is prevented, and preventing `pointerdown` stops a click from
     focusing it.
   - **Pointers.** The mouse is pointer 1, a pen 2, and each touch contact a new id, never reused. A second button
     pressed while one is held arrives as a `pointermove` with `button` set (W3C's chorded buttons), not a
     `pointerdown`. A pen's pressure, tilt and twist arrive as sent, and its barrel is button 2. Touch has implicit
     capture and a contact size, and only the first contact is primary. Without `touch-action: none` a drag over a
     scrollable page is taken by the browser: `pointercancel` after the first move.
   - **Sizes.** At a scale of 1.5, `devicePixelContentBoxSize` gives exact device pixels (480 × 300 for 320 × 200). An
     inline style beats the page's stylesheet. A canvas with no CSS size is laid out at its pixel size, so setting its
     pixels to CSS size × scale grows it each time. A runtime change of the pixel ratio cannot be faithfully made
     headless.
   - **Pointer lock.** Refused outside a user gesture, granted inside a `pointerdown`. Escape releases it and the page
     sees no Escape key. Asked again at once, it is refused (`SecurityError`); 1.5 s later it is granted. Edge accepts
     `unadjustedMovement`. A tab switch releases it before the blur.

   **Decided by the owner on 2026-11-11 (kyoten's twelfth session), each as recommended,** after the probe overturned
   none of them:
   1. **One JS library for every listener, pulled at `pump()`.** Every listener queues a record in JS, and `pump()`
      drains the queue into the ring through `take()`, as GLFW's callbacks run inside `glfwPollEvents`. So `pump()`
      takes the system's pending events on every backend, nothing calls into the program from outside (it cannot abort
      after `quit`), and the held set's rules are the C ring's, one implementation. It is kama's own `std::net::web`
      model ("no callbacks into wasm"); the owner: "Isn't this similar to how networking works?". The JS queue keeps
      WIN-2's guarantees: moves coalesce in it as in the ring, an up is never dropped, and past a cap a drop is counted
      and reported as `Dropped` where it happened.
   2. **Pointer Events replace `MouseMove` and `MouseButton`, on every backend.**
      1. **The cases:** `PointerMove(pointer, dx, dy)`, `PointerDown(pointer, button, mods)`, `PointerUp(pointer,
         button, mods)`, `PointerCancel(pointer)`, `PointerEnter(pointer)` and `PointerLeave(pointer)`. A `Pointer` is
         a plain value: `id`, `kind` (`Mouse`, `Touch` or `Pen`), `primary`, `x`, `y`, `buttons`, `pressure`, `tiltX`,
         `tiltY`, `twist`, `width` and `height`. `Scroll` stays its own event, as a wheel is not a pointer event in the
         W3C spec. Natively GLFW gives a mouse alone: kind `Mouse`, id 1 (the browser's mouse id), enter and leave from
         its cursor-enter callback.
      2. **Every button gets its own down and up.** The browser's chorded `pointermove` is translated, so a right press
         while the left is held is a `PointerDown(button: Right)`, as GLFW and winit report it, and WIN-2's pairing
         holds per button.
      3. **Touch and pen contacts are held like buttons, and a focus change cancels them.** A held contact ends with
         `PointerCancel`, not an up: the finger never lifted, so a tap must not fire, and a browser does the same when
         it takes a pointer. Mouse buttons keep WIN-2's ups. The ring keeps room for each held contact's end, for up to
         16 pointers held at once; a down beyond that is counted into the drop.
      4. **Moves coalesce per pointer:** a move joins the newest unread record only when it is the same pointer's move.
      5. **`PointerButton` replaces `MouseButton`:** `Left`, `Middle`, `Right`, `Back`, `Forward` and `Eraser`
         numbered as the W3C's `button` is (0 to 5), then `Sixth`, `Seventh` and `Eighth`. A touch contact and a pen
         tip report `Left`, a pen's barrel `Right`. GLFW's order (left, right, middle) is mapped at the seam.
      6. **Not now: every high-rate sample** (`getCoalescedEvents`, a 240 Hz pen). The ring keeps the last position;
         a drawing program that needs every sample has its own row (WIN-19).
   3. **Which canvas.**
      1. **The canvas is the page's.** Nothing matching the selector, an element that is not a canvas, or a canvas
         already a window: `open` refuses, in those words. Several canvases are several windows, each with its own
         queue. The window never creates or removes a canvas, and closing it detaches its listeners and gives back
         what it set on the element. (winit creates a canvas when none is given, which puts the window in charge of
         the page's layout.)
      2. **`title` is ignored on the web.** The page owns its `<title>`, and a canvas may be one part of a page; FFD
         ignores it too.

      How a program names its canvas was found on the way and asked as question 11 (below).
   4. **Focus is the canvas's, not the page's.** Keys are heard on the canvas, so typing in a page's chat box beside the
      game never moves the player; `Focus` follows the canvas's focus and blur, and a tab switch reaches the canvas too
      (measured), so WIN-2's release runs. `focused: true` (the default) focuses the canvas at open, as a native window
      takes the keys at launch; `false` leaves the focus where it is until the user clicks the canvas. A pointer down
      focuses the canvas, done by the window since it prevents the default (5). A canvas with no `tabindex` gets 0, so
      Tab can reach it; a page's own `tabindex` is kept. Clicking outside the canvas takes the keys away until the
      user clicks back, as clicking another window does natively.
   5. **While the canvas has the focus, the window takes:**
      - **every key it names** (`Key::Unknown` passes): its default is prevented, Tab, F1 to F24, Space and the arrows
        included, so Space does not scroll the page, Tab does not leave the game, F5 does not reload and Ctrl+S is the
        game's. The browser keeps the few chords no page can take (Ctrl+W, Ctrl+T). With the default prevented, `Text`
        is read from the `keydown`'s `key` (one printable character), as winit does; IME composition is WIN-5's;
      - **`pointerdown`'s default,** so no text is selected and the canvas image is not dragged; the window focuses
        the canvas itself;
      - **the context menu and the wheel** over the canvas, so the right button is a game button and the page does not
        scroll;
      - **`touch-action: none`** on the canvas, so a touch neither pans nor zooms the page over it, given back at close.

      No setting turns this off until a consumer needs one (winit has one). The cost: keyboard page zoom (Ctrl+plus)
      does not work while the game has the focus; the browser menu's still does.
   6. **`open` sets the canvas's CSS size inline to `width` × `height`,** as a native window opens at its size. A
      `WindowOptions` field, `sizedByPage` (false by default), leaves the CSS size to the page, and `width` and
      `height` go unused (Godot's web export asks the same, as its canvas resize policy). Either way the window sets
      the canvas's pixel size from `devicePixelContentBoxSize`, or CSS size × scale where a browser lacks it (Safari).
      `size()` is the CSS size, `framebufferSize()` the pixels, `scale()` the device pixel ratio, and a change arrives
      as `Resize` (ResizeObserver) or `ScaleChanged` (`matchMedia`).

      **Under `sizedByPage`, `open` refuses a canvas the page sizes on neither axis (WIN-21, built on 2026-11-11).**
      - **Found** by `@kama/gpu`'s web leg (GPU-3): a canvas the page gives no CSS size is laid out at its pixel size,
        so at a pixel ratio of 1.5 each report set the pixels to the layout × 1.5 and the layout followed. 450 × 225
        became 5130 × 2570 in 6 ticks. At a ratio of 1 it holds still, so it hides on many machines.
      - **Investigated** (kyoten's `.scratch/gpu3-web/FINDINGS.md`, its last section):
        - **Runs away, or grows and stops:** a canvas with no CSS size, or with only `width`/`height` attributes, ran
          away to 33,554,432 pixels in 29 writes; one in a parent sized by its children grew, then stopped.
        - **Held:** a fixed CSS size, a percentage of a sized parent, a flex or grid item, and one axis with the
          other auto.
        - **None of eight engines and libraries detects it or refuses:** winit, Bevy, Godot, Unity, three.js,
          Babylon.js, PlayCanvas and emscripten. Those that pin the CSS size cannot loop, and those that let the page
          size the canvas document a CSS rule. wgpu with winit has the loop (learn-wgpu's 26.0.1 note), and Unity's
          tracker has the attribute-only case.
      - **Decided by the owner, as recommended:** "Is your recommendation for WIN-21 the production answer? if so yes".
        - It refuses at the mistake, on every screen, which is the package's rule elsewhere.
        - Pinning the CSS size would override the page's later CSS, and a CSS rule alone hides at a ratio of 1.
        - **What it costs:** CSS that arrives after `open`, and `max-width` alone, are refused though they held. A
          page that removes the size after `open` is not caught: a row of its own if it ever matters.
      - **Measured before building** (kyoten's `.scratch/win21-sizing/FINDINGS.md`):
        - Edge 157 matches Chrome for Testing 157 on every shape, and a canvas sized by its height alone (width auto)
          holds as the width-only one does.
        - **The `aspect-ratio` canvas the investigation saw rewritten 15 times in 30 frames is not a bug.** Alone it
          holds, 1 write in 60 frames, in 48 runs: both browsers, ratios 1.25 to 2, fractional offsets, `16 / 9` and
          `auto 16 / 9`, and 19 million pixels down the page. On the investigation's page the runaway above it moved
          it every frame.
      - **Built:** `js/winseam.js`'s `follows` changes the pixels once by +37 × +23, which also changes their aspect,
        reads `getBoundingClientRect()`, and puts them back. `winseam_web_open` returns -5 when both axes followed,
        before anything of the canvas is touched, and `winseam_open` gives the words (SPEC § Window).
        - `tests/`: 9 new cases in each browser, 189 in all. 8 are in-process: the three shapes refused in the words,
          their pixels and styles left as they were, a refused canvas still a window without `sizedByPage`, and the
          two one-axis shapes opened at their layout. The ninth holds a one-axis window still over the trusted steps,
          about 150 frames.
        - **The mutations: 9 of 9 caught** in Chrome for Testing:
          - the check removed;
          - one axis refused (`&&` made `||`);
          - the check made without `sizedByPage`;
          - the pixels, or the height alone, not put back;
          - the height compared as the width;
          - a step of 0, or none on the height;
          - -5 said as "already a window".
   7. **Pointer lock waits for a click.** On the web a program calls `setCursor(Locked)` from a tick, never in a
      gesture, so asking at once always fails. `setCursor(Locked)` records the wish, and the window asks for the lock
      in the next pointer down on the canvas, with raw motion (`unadjustedMovement`) where offered: the web's "click to
      play". `PointerLock(true)` arrives only when the lock takes. When the browser releases it (Escape, a tab switch),
      `PointerLock(false)` arrives and the mode falls back to `Normal`, so the program asks again, as WIN-2 kept for a
      focus loss natively. A refusal posts nothing, and the wish stands. On the web alone, Escape while locked is the
      browser's.
   8. **The web never posts `CloseRequested`.** A page's close or a navigation never waits for the program, and
      `beforeunload` can only ask the browser to show its own prompt. Visibility stays `std::app`'s `Suspended`, and the
      focus loss that comes with it releases what is held. A "leave site?" prompt for unsaved work would be its own
      setting, later.
   9. **`Scroll` carries its unit,** `ScrollUnit::Lines` or `ScrollUnit::Pixels`, as winit's `LineDelta` and
      `PixelDelta` do. Nothing is guessed: GLFW's notches are lines, a browser's `deltaMode` says which, and a page's
      worth becomes pixels of the canvas's height. FFD's division of pixels by 100 differs by browser and system.
   10. **The gate runs `tests/` in a browser.**
       1. A node driver in `tools/`, with no npm packages, serves the built test page, runs headless Edge with a
          throwaway profile, and reads the console to the last line, an abort or a time limit. In-process cases dispatch
          DOM events from the test program, the web's Win32 messages; the cases that need the browser's own behaviour
          print a request, and the driver performs it as trusted input. The node leg stays, for the headless cases.
       2. **Firefox is a row of its own before the first publish (WIN-18):** it dropped the DevTools protocol, so it
          needs a WebDriver BiDi client and Firefox installed. Safari waits for a Mac (WIN-12).
   11. **A canvas is named by a ctor of its own, `Window.openIn(canvas:, title:, width:, height:, options:)`** (asked on
       the way, decided by the owner on 2026-11-11 as recommended: "I think I'll take your recommendation for q11").
       - **Found:** the draft was `options.canvas = "#game"`, but a `type value` cannot hold a `string` (kama 0.9.911:
         "a `value` owns nothing, but … transitively owns a resource — declare it `type resource`"), and a resource's
         fields are private, which would reverse WIN-16's `o.focused = false`.
       - **So the selector is a parameter,** as kama's `std::net` spells an address: "always spelled, never defaulted:
         it is what kind of socket this is, not a policy". Which canvas is what the window is. `open` and `openWith`
         take `#canvas`, emscripten's own name for it, and `WindowOptions` stays a value of policy flags (`focused`,
         `sizedByPage`).
       - **Natively the selector is ignored, as the web ignores `title`:** a browser editor's four viewports, each
         `openIn` its own canvas, are four system windows natively with the same code.
       - The alternative, `WindowOptions` as a resource with setters (kama's `std::process` `Command` builder is the
         precedent), was not taken.

   **Built (WIN-3), test-first, in three parts, each committed with every leg green and its mutations caught:**
   1. **Pointer Events on both backends** (decision 2, and decision 9's `ScrollUnit`). The ring record grew to carry a
      pointer (88 bytes), and the held set became a table of up to 16 pointers in the order they went down. 36 cases
      were written first, not compiling on the old API (133 natively, 97 under node). 33 of 33 mutations were caught;
      four of those cases were added on reading the code for what no case would catch (the order after a lift, a
      cancel's pressure, a pointer's lock bits, `primary` posted). `@kama/gpu`'s suite passed on it unchanged.
   2. **The browser leg** (decision 10): `tools/webtest.mjs` and `tests/web/shell.html`, proven on the headless cases
      before the web window existed (97 in Edge 157). Each red path was run on a page of its own (a failure, an
      exception, an abort, no summary), and a trusted click and key arrived as requested.
   3. **The web window** (decisions 1 and 3 to 8, then 11): `js/winseam.js` and `winseam.c`'s web half. Its cases went
      red first (the window refused to open), then green: 175 in Edge, 100 under node, 134 natively. 48 of 50
      mutations of the JS library and the C web half were caught. Of the other two, one removed code the browser made
      redundant (the JS held a locked pointer's position still, which the browser already does: the Pointer Lock spec,
      and measured), and one is equivalent: C setting the mode to `Normal` when the browser gives the lock back, since
      the JS holds the wish and the browser the lock. A first pass left C's rule that a size report posts `Resize` only
      for a new CSS size uncaught, because no case changed the scale; the size path then got its in-process cases, and
      the rule was caught.

   **Measured in the building** (headless Edge 157 at a scale of 1.5, trusted input over the DevTools protocol):
   - a click on an unfocused canvas gives `PointerEnter`, `Focus true`, then its down and up: the focus first, as a
     native window's activation does;
   - **item 5's first reading:** a key pressed in the page's field and let go on the canvas gives the canvas a `keyup`
     it never saw go down, and the ring drops it. The second, a held key's repeat after a focus gain, cannot be made
     headless (the protocol's `autoRepeat` is whatever the driver says), and decision 1 of item 5 makes it moot;
   - a drag released outside the canvas ends in its up, through pointer capture (the mutation without it loses the up);
   - a touch drag over the canvas gives its moves and its up, and no cancel, with `touch-action: none`;
   - pointer lock takes at the next click; a locked move of 30 CSS pixels is `dx` 30 with the position held still;
     Escape gives the lock back with no Escape key; after that, the next click does not lock;
   - a trusted position at a scale of 1.5 comes back within a millionth of a pixel of the CSS one;
   - **a change of the device pixel ratio cannot be made headless:** the protocol's `Emulation.setDeviceMetricsOverride`
     fired a plain page's `matchMedia` change (at 119 ms, to 2.0000000596), but never reached the test page's, waiting
     half a second, in three runs. What follows the browser's report is tested in-process instead
     (`windowtests_report_sizes`), and the browser's own report is read, not run.

   **The harness, found in the building:** focusing a text field scrolled the small headless viewport, so the driver
   opens a 1280 × 1024 window and the tests focus with `preventScroll`; input beyond the viewport is not delivered as
   page input; a drag's `mouseMoved` must name its held button, or the pointer is not in its active buttons state and
   capture does not hold; the browser's helper processes hold the throwaway profile a moment after it exits; and a
   mutation driver that builds for the web must itself be started from a UCRT64 login shell, so `emcc` is on its
   `PATH`.

   **Decided in the building, within the decisions:**
   - the page's queue is capped at 1024 records, and an up, a cancel, a focus change, a lock and the sizes go on to
     1536, so a key is never left held (decision 1); its drops are counted into the ring's `Dropped`;
   - a pointer down on the canvas captures the pointer until its up (`setPointerCapture`), as a native window captures
     the mouse, so a drag out of the canvas still ends;
   - positions are the canvas's content box's, inside its border and padding;
   - `Text` is read from a keydown's `key` when it is one printable character typed with neither Control nor Meta,
     unless AltGraph made it (AltGr+E types €), so Ctrl+S types nothing;
   - the wheel's deltas are negated, positive being left and up as GLFW and winit report (FFD negated only `deltaY`);
   - opening posts no `Focus`, natively or on the web: the canvas is focused before the listeners are attached;
   - `pump()` on any canvas window takes every canvas window's queue, as `glfwPollEvents` takes every GLFW window's.

   **Where it lives:**
   - `js/winseam.js`: the listeners, the page's queue, the DOM `code` table, sizes, pointer lock;
   - `csrc/winseam.c`: `take_pointer()` and the pointer table on every leg, and the web half (`winseam_web_*` calls,
     `web_take`, the layout asserted against the JS);
   - `src/window.kama`: `Pointer`, `PointerKind`, `PointerButton`, `ScrollUnit`, the pointer events, `openIn`,
     `WindowOptions.sizedByPage`;
   - `tests/src/pointers.kama`, `tests/src/web.kama`, `tests/src/keys_web.kama`, `tests/js/windowtests.js`,
     `tests/web/shell.html`, and `tools/webtest.mjs`.

9. **Built on 2026-11-12: gamepads (WIN-4)** (kama 0.9.911), on the web through the browser's Gamepad API and natively
   through SDL3, vendored. How it was decided is first, as the row's detail held it; how it was built is at this item's
   end.

   - **The model is the W3C Gamepad API's:** a pad connected or disconnected, its buttons and axes, the standard mapping.
   - **Split screen is pads** (the owner, 2026-11-10: "Controllers are the natural split screen solve"). Several keyboards
     and mice on one machine are not pursued, since a browser cannot tell them apart.

   **Measured first, on 2026-11-11 and 2026-11-12** (kyoten's `.scratch/win4-gamepads/FINDINGS.md`), on Windows 11 with
   the owner's pads, each wired: an Xbox 360 pad, an Xbox Series pad and an 8BitDo Ultimate. C probes over MSYS2's GLFW
   3.5.1 and SDL3 3.4.18, every press made with another window focused unless stated; the probe page in visible Edge 157
   and Firefox 160.
   - **GLFW adds a replugged pad a second time, through DirectInput, with its right trigger reading fully pulled:** 9 of
     20 replugs, both Xbox pads. Read in its source: DirectInput skips a pad only once Windows' raw-input list names it,
     which races its arrival. GLFW also has no rumble, no Guide button on XInput pads, and no model for an XInput pad.
   - **A pad reports to a program in the background** through XInput, GLFW and SDL alike. Unity's "a Windows program
     needs the focus" is its Windows.Gaming.Input backend.
   - **SDL3, initialised as `SDL_INIT_GAMEPAD` alone beside a GLFW window** (Godot 4.5's arrangement), never duplicated a
     pad on one backend (12 replugs), and names each pad's type and labels.
   - **Windows' pad backends, through SDL:**
     - XInput, SDL's default: reads in the background, plays dual rumble, has no trigger rumble, and reports an Xbox One
       or Series pad as a generic one.
     - Windows.Gaming.Input: **gives no input at all while the window is unfocused**, and does not take the 360 pad.
     - GameInput: needs Microsoft's runtime 1.1 or newer (the inbox `GameInput.dll` here is 0.2309, API 0; with the
       redistributable 3.5.283 installed, SDL takes it). It reads all three pads in the background, plays dual rumble,
       trigger rumble and each motor alone (the owner felt every one, each distinct), names each pad's maker and model
       (8BitDo's 2dc8/3106 too), and gives each pad a device id that held across every replug and run.
     - **With GameInput and XInput both on, SDL 3.4.18 adds a replugged 360 pad twice** (2 of 2): XInput sees it about
       220 ms before GameInput reports it. SDL's main branch turns XInput and DirectInput off while GameInput handles Xbox
       pads; 3.4.20 (2026-11-03), the newest release, does not.
   - **The browsers:**

     | | Edge 157 | Firefox 160 |
     |---|---|---|
     | exposed | every pad on one press | each pad on its own activity, once while unfocused |
     | `id` | "Xbox 360 Controller (XInput STANDARD GAMEPAD)" for both Xbox pads | "xinput" |
     | rumble | `dual-rumble` on both, `trigger-rumble` on the Series pad, felt | no `vibrationActuator` |
     | visible, unfocused | no input | input |
     | minimized | no input | no input |
     | a replug | the same index back, exposed at once, no duplicate | the same |

   - **Chromium applies no dead zone** to an XInput pad's sticks and calls a trigger pressed at 30 of 255 (read in
     `xinput_data_fetcher_win.cc`). The 360 pad's right stick rests 0.11 off centre.
   - **Licences:** SDL is zlib; Microsoft's GameInput header and loader are MIT; its runtime installer is under
     Microsoft's redistributable licence.

   **Decided by the owner on 2026-11-12, each as recommended** ("All of this sounds great"):
   1. **Natively, SDL3's gamepad subsystem, alone** (`SDL_INIT_GAMEPAD`, no SDL window), beside GLFW's windows. **On the
      web, the browser's Gamepad API from `js/winseam.js`, and nothing else:** no SDL and no mapping database in a web
      build (the owner: as lean as possible on the web).
   2. **Both events and state.** A pad's connection and each button's down and up are events in the one queue, under the
      keys' pairing rule (item 5, decision 1). The whole pad, each button's analog value and the axes, is state read after
      each `pump()`. Axis motion is not queued.
      - **2a. Dead zones, drift and response curves are the consumer's,** in its bindings. The package reports values as
        the device gives them, and decides only when an analog trigger is pressed: at 30 of 255, XInput's threshold and
        Chromium's, computed by the package from the value on every backend.
   3. **The W3C standard layout everywhere:** 17 buttons with analog values and 4 axes, the triggers 0 to 1 natively too.
      Face buttons by position (`South`, `East`, `West`, `North`), as `Key` names positions; each pad's own labels (A,
      Cross, B) for prompts go with WIN-17's. A raw view for a pad nothing maps. Natively, SDL's mapping database, shipped
      and updatable at run time; none on the web, where the browser maps.
   4. **Slots, the lowest free, reused,** as the browser and SDL's player index do. Each connection is a new one and
      carries what is known: the name, the vendor and product where given, a serial where there is one, and natively
      GameInput's device id. Which player a returning pad belongs to is the consumer's.
   5. **Reading pads while unfocused is off by default,** a setting a program changes at any time. Off, a pad reads
      neutral while the window is unfocused, on every backend. On, it reads in the background natively, and on the web as
      far as the browser gives (Firefox while visible, Chromium not).
   6. **Item 5's release extends to pads.** On a focus change, with 5 off, a button-up for each held pad button, and the
      state neutral until the focus returns. The order: mouse buttons, pad buttons slot by slot, keys by HID id,
      `PointerLock(false)`, `Focus`. A button still held after a gain comes back as a down on the next read. With 5 on,
      pads are not released.
   7. **Rumble in this row:** dual rumble, trigger rumble and each motor alone where a backend can, and a query of what a
      pad supports, since Firefox and a pad without motors have none. Gyro, accelerometer, touchpad and lights are WIN-22.
   8. **Tests:** the seam reads pads through one function per backend (SDL natively, `navigator.getGamepads()` on the
      web), which the tests replace, a C hook natively and a `getGamepads` shim with a dispatched `gamepadconnected` in the
      browser, so the path from a sample to events, state and releases is the real one. A headless window takes a pad
      state a program posts, as `post()` takes events.
   9. **On Windows, GameInput where Microsoft's runtime is present, XInput otherwise;** SDL falls back by itself (measured).
      Shipping Microsoft's GameInput installer is each program's choice, under Microsoft's licence. **Stopgap:** with both
      on, SDL 3.4.18 duplicates a replugged pad, so the package turns XInput off when GameInput is usable, as SDL's main
      branch does, until an SDL release does it (WIN-23).

   **Applied from `AGENTS.package.md`, not asked:** SDL3 is vendored, not linked, since MSYS2's is built without GameInput
   and the rule is a vendored C library. `tools/vendor-sdl.sh` pins an SDL 3.4 release and Microsoft's GameInput header by
   hash, and kama compiles the gamepad subset on every native target, with SDL's zlib notice and the header's MIT notice
   beside it.

   **Measured after the building, with the owner's pads** (kyoten's sixteenth session, 2026-11-12; its
   `.scratch/win4-gamepads/FINDINGS.md`, runs 8 and 9):
   - **GameInput's device id follows a pad to another USB port:** the 360 pad, moved between two USB-A ports, kept the id
     it has had in every run. Two pads of the same model were not tried.
   - **A pad's labels follow the mode its firmware chose, not what is printed on it:** the 8BitDo Ultimate (Switch
     edition) is read by place correctly in Xbox mode, but SDL labels its top button Y and its right button B, where the
     pad's print says X and A. Only its 2.6 s as a Switch Pro pad reported Nintendo's labels. Decision 3's labels for
     prompts (with WIN-17's) cannot come from SDL alone for such a pad; its vendor and product are the only hint.
   - **The guide button may never arrive:** with Steam running, Steam took the Home press and came forward, and no run
     logged a guide press. A program cannot count on receiving it.

   **Built, in five commits, test-first, each part's mutations caught or explained:**
   - **The ring** (part A): each window keeps its own view of each of 16 slots; a sample, the system's or a post's, is
     compared with what the consumer has seen, so a pad's connect and disconnect and each button's down and up are
     events, and the whole pad is state (`Window.pad`, a `Gamepad`). The ring keeps a slot for each held pad button's up
     and each known pad's disconnect; a connect or a down counted into a drop is made again by the next sample.
   - **SDL natively** (part B1): SDL's gamepad subsystem starts with the first system window and stops with the last;
     each pump of a system window reads SDL once into one process-wide table and hands every slot to every open system
     window; `Window.padInfo` gives what the system says of a pad.
   - **Rumble** (part B2): `Window.rumble` and `rumbleTriggers`, with `PadInfo.rumble` and `triggerRumble`.
   - **The mapping** (part B3): SDL_GameControllerDB's Windows lines shipped, `Window::addPadMappings`, and a pad no
     mapping knows read raw (`Window.padRaw`, `PadInfo.mapped`), mapped in place when a mapping arrives.
   - **The web** (part C): each pump reads `navigator.getGamepads()` into the same table; rumble through the
     actuator's effects.

   **Decided in the building, within the decisions:**
   - **Every button is pressed at 30 of 255 of its travel, computed by the package from its value** (decision 2a's
     rule for the triggers, which leaves a digital button unchanged): the browser's own `pressed` is not read.
   - **A rumble follows the pads' focus rule:** refused while the window's pads read neutral, and each rumble the
     window started stopped when its pads close at a focus loss, as its held buttons are released (decisions 5 and 6;
     Unity's input system pauses haptics on a focus loss, from memory). The owner was told, and did not object.
   - **A trigger rumble is refused for a pad with no trigger motors,** on every backend, so a program is never told one
     played: SDL's GameInput driver sends it regardless, and the owner felt the 360 pad and the 8BitDo buzz for it.
   - **SDL never takes the program's Ctrl+C** (`SDL_HINT_NO_SIGNAL_HANDLERS`), and its own background gate is off: the
     window decides what an unfocused pad reads.
   - **A pad keeps the slot it took; a new one takes the lowest free,** natively as the browser does (its index).
   - **The web gives a pad no serial and no device id,** and no hats; a pad's vendor and product are read from its id,
     as Chromium and Firefox each write them.
   - **The tests read no real pad but where a case asks for one** (decision 8): the run replaces the system's read with
     an empty table, so a run never depends on the pads plugged into the machine, and never rumbles one. SDL's own path
     is tested through SDL's virtual pads, found by their names; the web's through a `navigator.getGamepads` of the
     tests' own (`tests/js/windowtests.js`), whose `pressed` is the browser's half-travel rule, so the package's 30 of 255
     is seen to win.
   - **KWIN-1** (KAMA_GAPS.md): writing an `extern value`'s `InlineArray` field passed `kama check` and failed in C, so a
     posted pad crossed the seam by its parts. Fixed in kama 0.9.928 (KR-379); since then a posted pad crosses as a
     whole `winseam_pad` (`winseam_pad_sample`), and `Window.pad` assigns each array whole.

   **Where it lives:**
   - `csrc/winseam.c`: the ring's pads (`pad_take`, the release, `stop_rumbles`), the system's table, the SDL backend
     (Windows) and the web's; `csrc/winseam_gameinput.cpp`: SDL's own test of GameInput, for the stopgap (WIN-23);
   - `js/winseam.js`: `winseam_web_pads_read`, `winseam_web_pad`, `winseam_web_rumble`;
   - `src/window.kama`: `Gamepad`, `PadButton`, `PadAxis`, `PadInfo`, `PadRaw`, the four events, and the window's pad
     methods;
   - `third_party/` and `tools/vendor-sdl.sh`: SDL 3.4.18, its Windows configuration, SDL_GameControllerDB's Windows
     lines and Microsoft's GameInput header, pinned by hash (`third_party/VENDORED.md`);
   - `tests/src/pads.kama` (headless), `tests/src/systempads.kama` (natively), `tests/src/webpads.kama` (the web), and
     `tests/csrc/hooks.c`'s virtual pads.

10. **Decided on 2026-11-12, before it was built: the browser legs under `kama run` (WIN-18).** kama 0.9.928's
    `kama run --target WASM` (KR-377) runs a page in any browser with no automation protocol, so the planned WebDriver
    BiDi client was measured against it first: this package's browser leg and `@kama/gpu`'s under `kama run` in Edge
    157, Chrome for Testing 157 and Firefox 160 (ROADMAP_DETAIL.md § WIN-18 has the table; kyoten's
    `.scratch/win18-kamarun/`). Firefox runs every case Chromium runs; what fails in all three is trusted input (kama's
    KR-378) and a device scale `kama run` cannot set. An unzipped Chrome for Testing never loads a page under
    `kama run`, even with its folder given the read access an installed Chrome's has (`~/browsers/README.md`).

    **Decided by the owner on 2026-11-12 (kyoten's sixteenth session), each as recommended,** asked in the chat after
    the measurement ("Sounds good as recommended we can wait on kama for that"; then "I take your recommendation" for
    the driver's browser, asked when Chrome for Testing would not run):
    1. **`kama run` for every case it can run, Firefox among the browsers, and no BiDi client of the package's own.**
       The browser leg is two runs of the same program:
       - under `kama run --target WASM --headless`, in Edge and Chrome as installed and in the pinned Firefox. The
         cases that need trusted input or a device scale of 1.5 are skipped there, each saying what it waits on;
       - under `tools/webtest.mjs`, the DevTools-protocol driver, which passes `--driver` and runs the whole program at a
         device scale of 1.5, performing its trusted input.
       **Stopgap:** Firefox's own input (a wheel in lines, the order of its pointer lock and focus events) is untested
       until KR-378 lands, and the driver stays until then. It is the reason KR-378 is raised with kama.
    2. **The driver runs in the pinned Chrome for Testing alone,** until that browser runs under `kama run`. The
       pinned Chromium so keeps every case, trusted input and the scale's included, and a red run in it can be
       reproduced; `kama run` takes the browsers that run it. Chrome for Testing's silent hang is kyoten's KY-53.
    3. **A device scale under `kama run` is kama's** (kyoten's KY-52, filed): until it lands, the scale cases run in
       the driver's leg alone.


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
