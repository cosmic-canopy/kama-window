# @kama/window roadmap, the detail

Each row's reasoning: what it builds, where it starts, what is open, and what done means. A row's questions are asked
when it comes up, and their answers are recorded here or, for a departure from kama's design, in
[AGENTS.window.md](AGENTS.window.md). When a row ships, its section goes: what was built is the [SPEC](SPEC.md), and
how it was decided is the git log. `tools/check-roadmap.sh` holds every section to a row.

**The browser is the reference.** Each row is designed against the browser's own API first, and native implements the
same `Event`s and methods. Where a platform cannot, the SPEC says so for that platform.

<a id="web-half"></a>
## WIN-3: the web half

Today `Window.open` refuses on the web. This row makes a window on a canvas, and Pointer Events on every backend. Its
questions were decided on 2026-11-11 ([AGENTS.window.md](AGENTS.window.md#where-this-departs-from-kamas-design-s8-input)
item 8), after a probe in headless Edge; question 11, how a program names its canvas, is open.

- **It starts from friendly-fire-department's web platform header** (`gfx/src/platform/ffd_platform.h`), but not its
  html5 callbacks: emsdk's `html5.h` has no Pointer Events, ResizeObserver or pixel-ratio change. The package ships a
  JS library (`jsLibraries`) whose listeners queue records that `pump()` drains into the C ring (decision 1), so the
  ring's rules hold on every backend unchanged.
- **What it builds, in order, each part test-first with a mutation pass:**
  1. **Pointer Events on both backends** (decision 2): `PointerMove`, `PointerDown`, `PointerUp`, `PointerCancel`,
     `PointerEnter` and `PointerLeave` carrying a `Pointer`; `PointerButton` for `MouseButton`; the held set keyed by
     pointer, a focus change cancelling a held contact; moves coalescing per pointer; `Scroll` with its unit
     (decision 9). GLFW's mouse becomes pointer 1, and the tests are ported.
  2. **The browser leg** (decision 10): a node driver and a static server in `tools/`, a test shell page, and
     `tools/test.sh` running `tests/` in headless Edge, proven on the headless cases before the web window exists.
  3. **The web window:** `winseam.c`'s web half and the JS library. On a canvas named by question 11's answer:
     - the refusals and the canvas left as found (decision 3);
     - keys from `KeyboardEvent.code` through one table, with its own test (`tests/src/keys_web.kama`), `Text` from
       the `keydown`'s `key`, and what the window prevents (decision 5);
     - focus from the canvas's own focus and blur (decision 4), so a blur releases what is held. Two readings item 5
       left open are measured here: the `keyup` of a key pressed before the canvas had the focus, and a repeat after a
       focus gain;
     - the CSS size, the pixel size and the scale, and their changes (decision 6);
     - pointer lock on the next click (decision 7);
     - pointer capture on a down, so a drag released outside the canvas still ends in its up, as a native window
       captures the mouse;
     - `nativeHandle()`: `NativeKind::Canvas` with the canvas's selector, which `@kama/gpu` makes a surface from.
- **Done:** `tests/` runs in a browser, the canvas window's cases included, in-process and with trusted input. The
  web leg is a browser as well as node, and `@kama/gpu`'s GPU-3 can start.

<a id="firefox"></a>
## WIN-18: the browser leg in Firefox

The browser leg drives Chromium's engine over the DevTools protocol, which Firefox no longer speaks (decided with
WIN-3, AGENTS.window.md item 8, decision 10). Firefox takes WebDriver BiDi: a client for it in `tools/`, with no npm
packages, beside the DevTools one, and Firefox installed where the gate runs. It is due before the first publish,
which needs every claimed target green; Safari waits for a Mac (WIN-12). What differs is measured, not assumed:
Firefox reports a mouse wheel in lines, and its pointer lock and focus may order their events otherwise.

<a id="pointer-samples"></a>
## WIN-19: every pointer sample

The ring keeps a move's last position and sums its motion (AGENTS.window.md item 8, decision 2.6), which a game and a
UI need. A drawing program needs every sample of a 240 Hz pen: the web's `getCoalescedEvents()` and
`getPredictedEvents()`, and natively each platform's high-rate input. How they reach a program without growing every
move's record is this row's question.

<a id="gamepads"></a>
## WIN-4: gamepads

- **The model is the W3C Gamepad API's:** a pad connected or disconnected, its buttons and axes, the standard mapping.
- **Natively,** GLFW's gamepad API: `glfwGetGamepadState`, with SDL's mapping database.
- **To ask when the row comes up:**
  - whether pads come as events, in the one queue, or as state polled each frame, which is how both APIs give them;
  - how a pad is identified across a disconnect;
  - **whether a program reads pads while its window is unfocused** (the owner, 2026-11-10, deciding the release of
    held keys, AGENTS.window.md item 5). Unlike
    keys, a pad reports in the background, so this is a real choice: a setting, where the release of keys and buttons
    is not. Unity's input package resets by default only the devices that cannot run in the background. Unity also
    says a Windows program must have the focus to receive a controller's input at all, which is measured here first.
- **The focus rule extends to pads** (AGENTS.window.md item 5): on a focus change each held button and deflected axis is
  released. A pad's state is
  read, not only sent, so a button still held after a focus gain comes back with the next read, not a repeat.
- **Split screen is pads** (the owner, 2026-11-10: "Controllers are the natural split screen solve"). Several keyboards
  and mice on one machine are not pursued, since a browser cannot tell them apart.

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

GLFW reports a mouse alone. Pointer Events' model (WIN-3) natively needs each platform's own input:
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
- WIN-3, so the web is a claimed target that passes;
- WIN-18, so the web is green in Firefox as well as Chromium;
- WIN-14.

It is published before `@kama/gpu`, which then depends on its version, so a consumer names one dependency. The README's
status and the manifest's description, which names gamepads, are made true for the registry, which cannot be changed
after.
