# @kama/window roadmap, the detail

Each row's reasoning: what it builds, where it starts, what is open, and what done means. A row's questions are asked
when it comes up, and their answers are recorded here or, for a departure from kama's design, in
[AGENTS.window.md](AGENTS.window.md). When a row ships, its section goes: what was built is the [SPEC](SPEC.md), and
how it was decided is the git log. `tools/check-roadmap.sh` holds every section to a row.

**The browser is the reference.** Each row is designed against the browser's own API first, and native implements the
same `Event`s and methods. Where a platform cannot, the SPEC says so for that platform.

<a id="firefox"></a>
## WIN-18: the browser leg in Firefox

The browser leg drives Chromium's engine over the DevTools protocol, which Firefox no longer speaks (AGENTS.window.md
item 8, decision 10). Firefox takes WebDriver BiDi: a client for it in `tools/`, with no npm
packages, beside the DevTools one, and Firefox installed where the gate runs. It is due before the first publish,
which needs every claimed target green; Safari waits for a Mac (WIN-12). What differs is measured, not assumed:
Firefox reports a mouse wheel in lines, and its pointer lock and focus may order their events otherwise.

**Measured first, on 2026-11-12** (kyoten's sixteenth session, `.scratch/win18-kamarun/`; kama 0.9.928, whose
`kama run --target WASM` runs a page in any browser, KR-377). This package's browser leg and `@kama/gpu`'s, under
`kama run … --target WASM --headless`, against the stopgap (`tools/webtest.mjs`) on the same builds:

| | Edge 157 | Chrome for Testing 157 | Firefox 160 |
|---|---|---|---|
| `@kama/window`'s tests (stopgap: 264, 0 failed) | 253 cases, 8 failed | 253 cases, 8 failed (see below) | 253 cases, 8 failed |
| `@kama/gpu`'s tests (stopgap: 74, 0 failed) | 74, 1 failed | 74, 1 failed | no WebGPU adapter, then an abort |
| `@kama/gpu`'s clear, 60 frames | 60 frames, 0 errors | 60 frames, 0 errors | no WebGPU adapter, an abort |

- **Firefox runs this package's cases as Chromium does:** the same 8 failures in all three, and no other.
- **The 8 are two causes, neither Firefox's:**
  - 7 cases are written for a device scale of 1.5, "so pixel sizes are not CSS sizes", which the stopgap sets
    (`--force-device-scale-factor`). `kama run` opens at the display's own scale, 1 headless, where pixels are CSS
    pixels; it takes no browser flags and no scale.
  - The trusted-input steps (14 requests, `tests/src/web.kama`) wait out 3,600 frames for a driver, and the 11 cases
    after the first never run. Trusted input is kama's KR-378, filed and not built.
- **`@kama/gpu`'s one failure is the scale too:** its too-wide canvas is 6000 CSS pixels, past 8192 only at 1.5.
- **WebGPU needs no flag under `kama run`:** headless Edge and Chrome for Testing give an adapter without
  `--enable-unsafe-webgpu`.
- **Headless Firefox 160 gives no WebGPU adapter here:** `navigator.gpu` is there and `requestAdapter()` answers null,
  with the default prefs and with four sets that force WebGPU, its blocklist off, the GPU process and hardware
  compositing. A visible Firefox was not tried: its window would take the focus.
- **Chrome for Testing unzipped into `~/browsers` never loads the page under `kama run`:** the folder lacks the read
  access Chrome's installer grants its sandbox, so its sandboxed network service crashes at start-up, and the first
  navigation, the page `kama run` names on the command line, hangs. With `--disable-features=NetworkServiceSandbox` it
  runs as Edge does. The stopgap never saw this: it opens `about:blank` and navigates later, over the DevTools protocol.
- **Time:** about the same. `@kama/gpu`'s tests take 7.4 to 8.4 s under `kama run` with its build, and 6.8 to 7.1 s
  under the stopgap without one. This package's take 65 to 224 s under `kama run`, all but a few waiting out the
  trusted-input steps, against 8.5 s.

**Decided on 2026-11-12** (AGENTS.window.md item 10): no BiDi client. The leg runs the program under `kama run` in
Edge, Chrome as installed and the pinned Firefox, skipping, by name, the cases that wait on trusted input (kama's
KR-378) or a device scale (kyoten's KY-52); and under `tools/webtest.mjs`, with `--driver`, in the pinned Chrome for
Testing alone, every case run. Done when both runs are green in `tools/test.sh`.

<a id="pointer-samples"></a>
## WIN-19: every pointer sample

The ring keeps a move's last position and sums its motion (AGENTS.window.md item 8, decision 2.6), which a game and a
UI need. A drawing program needs every sample of a 240 Hz pen: the web's `getCoalescedEvents()` and
`getPredictedEvents()`, and natively each platform's high-rate input. How they reach a program without growing every
move's record is this row's question.

<a id="pad-sensors"></a>
## WIN-22: a pad's motion sensors, touchpad and lights

Decided on 2026-11-12 to follow the gamepads (AGENTS.window.md item 9, decision 7). Natively SDL3 gives a pad's
gyroscope and accelerometer, its touchpads, an RGB light bar and player lights: the 8BitDo, coming up briefly as a
Switch Pro pad through SDL's own driver, reported a gyroscope, an accelerometer, a player light and a Home light (item
9's measurements). The buttons past the standard 17 (a Share or Capture button, paddles, a touchpad's click) are this
row's too. The web's
Gamepad API gives none of them; only WebHID reaches them, in Chromium, per vendor protocol and behind a permission.
This row's questions: what the web half is, and whether sensors come as events, as state or both.

<a id="sdl-gameinput-fix"></a>
## WIN-23: SDL's own switch from XInput to GameInput

The gamepads' stopgap (AGENTS.window.md item 9, decision 9): with GameInput and XInput both on, SDL 3.4.18 adds a
replugged pad twice, so the package turns XInput off itself while GameInput is usable (`csrc/winseam_gameinput.cpp`). SDL's main branch does that inside SDL (XInput's and
DirectInput's init check `SDL_UsingGameInputForXInputControllers()`); no release has it yet (3.4.20, 2026-11-03).
When one does, the vendored SDL moves to it and the package's switch goes, with the duplicate's test kept.

<a id="ime"></a>
## WIN-5: text input with IME composition

On the web, `compositionstart`, `compositionupdate` and `compositionend`, the composed text coming out as `Text`. A
program says where the caret is, so the IME's window sits beside it. Natively, what GLFW gives for an IME's
composition is to be measured before the design, and the platform's own API used where it gives none.

<a id="international-keys"></a>
## WIN-17: keyboards beyond the US layout

The owner, 2026-11-10, deciding the release of held keys (AGENTS.window.md item 5): kyoten will handle locale and
localization, so the package does too. Two parts:
- **The keys of other keyboards, named.** Japanese, Korean and Brazilian keyboards have keys `Key` does not name yet:
  Henkan, Muhenkan, Kana, Yen, the ABNT2 key, Lang1 and Lang2 (USB HID International1 to 5 and Lang1 to 9; the DOM's
  `Convert`, `NonConvert`, `KanaMode`, `IntlYen`, `IntlRo`, `Lang1`, `Lang2`). Today each is `Key::Unknown`. GLFW has
  no token for them and reports each as `GLFW_KEY_UNKNOWN` with its scancode (measured, AGENTS.window.md item 5), so natively
  they are mapped by scancode, on each system. Media and launch keys sit on HID's consumer page, and whether `Key`
  names them is this row's question.
- **Each key's label in the player's layout,** for prompts and rebinding screens. `Key` names a position, so the key
  in W's place is `KeyW` everywhere, and on AZERTY it reads "Z". The web's `navigator.keyboard.getLayoutMap()` gives
  the label, in Chromium alone; GLFW's `glfwGetKeyName` gives it natively. A browser without the layout map, and a
  label that changes with the layout, are this row's questions.

Translating a program's own text is not this package's: in kyoten it is the engine's.

<a id="clipboard"></a>
## WIN-6: the clipboard

The web's asynchronous Clipboard API, which a browser grants only with permission or on a gesture, so a read is
polled, as `@kama/gpu`'s requests are. Natively, GLFW's clipboard string. Text first; images and other types are this
row's question.

<a id="cursor-shapes"></a>
## WIN-7: cursor shapes

The CSS cursor keywords on the web, and GLFW's standard cursors natively, named once for both. A cursor from an image
is this row's question.

<a id="fullscreen"></a>
## WIN-8: fullscreen

The web's Fullscreen API, which a browser grants only on a gesture, its change reported as an event. Natively, GLFW's
fullscreen on a monitor, and a borderless window the size of the display.

<a id="several-windows"></a>
## WIN-9: several windows natively, tested

Each window is its own ring, filled through GLFW's user pointer, and `pump()` takes every open window's events, since
GLFW polls for the whole process. No test opens two windows yet. Done: two windows open at once, each event in the
queue of the window it happened to, and closing one leaving the other working.

<a id="file-drop"></a>
## WIN-10: file drop

The web's drag and drop hands over files' contents, never their paths; GLFW's drop callback hands over paths. How one
`Event` serves both, and what a program reads a dropped file through, is this row's question.

<a id="native-pointers"></a>
## WIN-11: touch and pen natively

GLFW reports a mouse alone. Pointer Events' model (AGENTS.window.md item 8) natively needs each platform's own input:
- Windows' pointer messages;
- touch on Wayland and X11;
- macOS' trackpad and pen.

Whether that is a patch to the vendored GLFW (WIN-14) or a backend of the package's own is this row's question.

<a id="desktop-platforms"></a>
## WIN-12: Linux and macOS

Linux and macOS link a system GLFW today, untried, and `nativeHandle()` answers on Win32 alone. This row:
- answers X11's, Wayland's and Cocoa's handles through GLFW's native access;
- runs `tests/` on each, the Steam Deck among them;
- holds each in the gate where a machine can run it.

<a id="vendored-glfw"></a>
## WIN-14: GLFW vendored

`AGENTS.package.md` asks a package to vendor the C library it needs, and Windows links MSYS2's static GLFW 3.5.1
today. friendly-fire-department vendors GLFW 3.4 (`gfx/third_party/glfw`). This row brings the source into the package,
built by kama on every native target, with GLFW's zlib notice beside it.

<a id="publish"></a>
## WIN-15: the first publish

kama's floor for a first publish is `tests/` green on every target the package claims, and `AGENTS.package.md` has the
rest. Before it:
- WIN-18, so the web is green in Firefox as well as Chromium (it is in Chromium: AGENTS.window.md item 8);
- WIN-14.

It is published before `@kama/gpu`, which then depends on its version, so a consumer names one dependency. The README's
status and the manifest's description, which names gamepads, are made true for the registry, which cannot be changed
after.
