# @kama/window roadmap, the detail

Each row's reasoning: what it builds, where it starts, what is open, and what done means. A row's questions are asked
when it comes up, and their answers are recorded here or, for a departure from kama's design, in
[AGENTS.window.md](AGENTS.window.md). When a row ships, its section goes: what was built is the [SPEC](SPEC.md), and
how it was decided is the git log. `tools/check-roadmap.sh` holds every section to a row.

**The browser is the reference.** Each row is designed against the browser's own API first, and native implements the
same `Event`s and methods. Where a platform cannot, the SPEC says so for that platform.

<a id="page-sized-canvas"></a>
## WIN-21: `sizedByPage` on a canvas the page does not size

Found by `@kama/gpu`'s web leg on 2026-11-11 (kyoten's `.scratch/gpu3-web/`, headless Edge 157 at a pixel ratio of
1.5): a canvas the page gives no CSS size is laid out at its pixel size, so under `WindowOptions.sizedByPage` each
report sets the pixels to the layout × 1.5 and the layout follows. With no surface on it, 450 × 225 became 5130 × 2570
in 6 ticks; with one, 51912 × 25973 in 10. At a pixel ratio of 1 it holds still, so it hides on many machines. The
owner decided on 2026-11-11 that it is investigated first (GPU-3's question 3):
- which ways a page sizes a canvas loop, and which hold: a fixed CSS size, a percentage of a sized parent, a flex or
  grid item, `aspect-ratio`, one axis only, a stylesheet that arrives after `open`;
- whether `open` can tell them apart reliably and refuse with words naming the fix, or the loop must be broken where
  a report is taken;
- how winit, Bevy, Godot's web export, Unity's web template, three.js, Babylon.js, PlayCanvas and emscripten handle it.

**Investigated on 2026-11-11** (kyoten's `.scratch/gpu3-web/FINDINGS.md`, its last section): each canvas driven by the
window's rule at ratios of 1.5 and 1. A canvas with no CSS size, or with only `width`/`height` attributes, ran away to
33,554,432 pixels in 29 writes; one in a parent sized by its children grew to 1240 × 618 and stopped; a fixed CSS size,
a percentage of a sized parent, a flex or grid item, and `width: 100%` with `height: auto` (the pixels keep their
aspect) each held. At a ratio of 1 nothing moved. Of eight engines and libraries surveyed (winit, Bevy, Godot, Unity,
three.js, Babylon.js, PlayCanvas, emscripten), none detects the loop or refuses: those that pin the CSS size cannot
loop, and those that let the page size the canvas document a CSS rule. wgpu with winit has the loop (learn-wgpu's
26.0.1 note), and Unity's tracker has the attribute-only case.

**Decided by the owner on 2026-11-11, as recommended** ("Is your recommendation for WIN-21 the production answer? if so
yes"): **under `sizedByPage`, `open` refuses a canvas the page sizes on neither axis.** It changes the canvas's pixels
once, reads the layout, and puts them back; when both width and height followed, it refuses with words naming the
fix, as `WindowError`: the page gives the canvas no CSS size, so give it a width (and a height or an aspect ratio) in
CSS, or leave `sizedByPage` off. One axis following is allowed, since a width with `height: auto` keeps the pixels'
aspect (measured stable). Refusing at the mistake, on every screen, is the package's rule elsewhere; pinning the CSS
size would override the page's later CSS, and a CSS rule alone hides at a ratio of 1. **What it costs:** a canvas whose
CSS arrives after `open`, and `max-width` alone, are refused though they held; the page sizes the canvas before the
program opens it. A page that removes the canvas's size after `open` is not caught by a check at `open`; a row of its
own if it ever matters. Built test-first, with cases at a pixel ratio other than 1. The `aspect-ratio` canvas seen
rewritten every other frame in the investigation is measured alone before it is called a bug.

<a id="firefox"></a>
## WIN-18: the browser leg in Firefox

The browser leg drives Chromium's engine over the DevTools protocol, which Firefox no longer speaks (AGENTS.window.md
item 8, decision 10). Firefox takes WebDriver BiDi: a client for it in `tools/`, with no npm
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
