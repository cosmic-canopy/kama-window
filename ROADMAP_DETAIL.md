# @kama/window roadmap, the detail

Each row's reasoning: what it builds, where it starts, what is open, and what done means. A row's questions are asked
when it comes up, and their answers are recorded here or, for a departure from kama's design, in
[AGENTS.window.md](AGENTS.window.md). When a row ships, its section goes: what was built is the [SPEC](SPEC.md), and
how it was decided is the git log. `tools/check-roadmap.sh` holds every section to a row.

**The browser is the reference.** Each row is designed against the browser's own API first, and native implements the
same `Event`s and methods. Where a platform cannot, the SPEC says so for that platform.

<a id="web-half"></a>
## WIN-3: the web half

Today `Window.open` refuses on the web. This row makes a window on a canvas.

- **It starts from friendly-fire-department's web platform header** (`gfx/src/platform/ffd_platform.h`): emscripten's
  html5 callbacks on `#canvas` filling the same C ring as GLFW's callbacks do natively, so the queue's rules hold on
  every backend unchanged.
- **What it builds:**
  - **keys:** `KeyboardEvent.code` to the HID id, one table, with its own test, as the GLFW table has
    (`tests/src/keys_web.kama` is its empty place today);
  - **text:** typed text as `Text`, apart from the keys, as natively; which DOM events it is read from is measured
    against an IME's (WIN-5);
  - **sizes:** the canvas's CSS size, its pixel size from the device pixel ratio, and a change of either as `Resize` or
    `ScaleChanged`;
  - **pointer lock:** requested on a gesture, `PointerLock` from `pointerlockchange`, and released on blur, as natively;
  - **focus, and the close request:** from the page's focus and visibility;
  - **`nativeHandle()`:** `NativeKind::Canvas` with the canvas's selector, which `@kama/gpu` makes a surface from.
- **Pointer Events** (AGENTS.window.md, item 7): one stream for mouse, touch and pen, each event with a pointer id, its
  type, buttons, pressure and tilt, and down, move, up and cancel. The mouse's events become pointer events of type
  mouse.
  - **To ask when the row comes up:** how `Event` carries them, since replacing `MouseMove` and `MouseButton` changes
    the API every consumer matches on, and the package is unpublished.
- **Done:** `tests/` runs in a browser, the system-window case included. The web leg is a browser, not node, and
  `@kama/gpu`'s GPU-3 can start.

<a id="gamepads"></a>
## WIN-4: gamepads

- **The model is the W3C Gamepad API's:** a pad connected or disconnected, its buttons and axes, the standard mapping.
- **Natively,** GLFW's gamepad API: `glfwGetGamepadState`, with SDL's mapping database.
- **To ask when the row comes up:**
  - whether pads come as events, in the one queue, or as state polled each frame, which is how both APIs give them;
  - how a pad is identified across a disconnect;
  - **whether a program reads pads while its window is unfocused** (the owner, 2026-11-10, deciding WIN-2). Unlike
    keys, a pad reports in the background, so this is a real choice: a setting, where the release of keys and buttons
    is not. Unity's input package resets by default only the devices that cannot run in the background. Unity also
    says a Windows program must have the focus to receive a controller's input at all, which is measured here first.
- **WIN-2's rule extends to pads:** on a focus change each held button and deflected axis is released. A pad's state is
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

The owner, 2026-11-10, deciding WIN-2: kyoten will handle locale and localization, so the package does too. Two parts:
- **The keys of other keyboards, named.** Japanese, Korean and Brazilian keyboards have keys `Key` does not name yet:
  Henkan, Muhenkan, Kana, Yen, the ABNT2 key, Lang1 and Lang2 (USB HID International1 to 5 and Lang1 to 9; the DOM's
  `Convert`, `NonConvert`, `KanaMode`, `IntlYen`, `IntlRo`, `Lang1`, `Lang2`). Today each is `Key::Unknown`. GLFW has
  no token for them and reports each as `GLFW_KEY_UNKNOWN` with its scancode (measured, WIN-2's probe), so natively
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
- WIN-2, which changes the API;
- WIN-3, so the web is a claimed target that passes;
- WIN-14.

It is published before `@kama/gpu`, which then depends on its version, so a consumer names one dependency. The README's
status and the manifest's description, which names gamepads, are made true for the registry, which cannot be changed
after.
