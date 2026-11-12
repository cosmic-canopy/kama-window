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

<a id="pointer-samples"></a>
## WIN-19: every pointer sample

The ring keeps a move's last position and sums its motion (AGENTS.window.md item 8, decision 2.6), which a game and a
UI need. A drawing program needs every sample of a 240 Hz pen: the web's `getCoalescedEvents()` and
`getPredictedEvents()`, and natively each platform's high-rate input. How they reach a program without growing every
move's record is this row's question.

<a id="gamepads"></a>
## WIN-4: gamepads

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
beside it. Whether GameInput's device id follows a pad to another USB port is measured in the building.

<a id="pad-sensors"></a>
## WIN-22: a pad's motion sensors, touchpad and lights

Decided on 2026-11-12 to follow WIN-4 (its decision 7). Natively SDL3 gives a pad's gyroscope and accelerometer, its
touchpads, an RGB light bar and player lights: the 8BitDo, coming up briefly as a Switch Pro pad through SDL's own
driver, reported a gyroscope, an accelerometer, a player light and a Home light (WIN-4's measurements). The web's
Gamepad API gives none of them; only WebHID reaches them, in Chromium, per vendor protocol and behind a permission.
This row's questions: what the web half is, and whether sensors come as events, as state or both.

<a id="sdl-gameinput-fix"></a>
## WIN-23: SDL's own switch from XInput to GameInput

WIN-4's stopgap (its decision 9): with GameInput and XInput both on, SDL 3.4.18 adds a replugged pad twice, so the
package turns XInput off itself while GameInput is usable. SDL's main branch does that inside SDL (XInput's and
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
