# @kama/window — the specification

What this package **is**, written after each piece lands. Never speculative: what is described here exists and is
tested. What comes next is [ROADMAP.md](ROADMAP.md), why the package departs from kama's design is
[AGENTS.window.md](AGENTS.window.md), and its history is the git log and [CHANGELOG.md](CHANGELOG.md).

**Where it stands** (2026-11-11, kama 0.9.911): a system window natively on Windows (GLFW 3.5.1, MSYS2's static
build), and a headless window on every target, each with a counted native record, so a GPU surface holds its window.
The mouse, touch and a pen are one stream of pointer events, as the W3C's Pointer Events are. The window keeps what is
held, and releases it on a focus change. On the web, opening a window refuses until the web half is built. Not
published.

## Using it

`import { window::Window, window::Event, window::Key, … }`. Natively, Windows links GLFW statically (`libglfw3.a`);
Linux and macOS link a system `glfw`, untried.

## Invariants

- **A `Window` has its own event queue,** so several windows work. A headless window is that queue and a size with
  nothing behind them: `post` fills it, for tests, replays and servers, and it never touches the system.
- **The queue is one fixed ring of 256 records per window, in C** (`csrc/winseam.c`), for both kinds of window, so one
  queue is tested. It never allocates after the window opens.
- **Events come out one at a time, in the order they happened,** so a resize and a click stay in order.
- **Nothing is dropped in silence.** A full ring keeps one slot for a `Dropped(n)` record, so a drop is reported where
  it happened, before anything that arrives once the reader makes room. Only an event that finds no free slot but the
  reserved ones is counted into the drop.
- **The window keeps what is held,** keys and each pointer's buttons, so the events pair up: every up follows its down,
  and every down is followed in time by its up or its pointer's cancel.
  - An up for a key or button the window does not hold is dropped, and so is a second down for a held button.
  - Each pointer holds its own buttons: the mouse's, each touch contact's (its `Left`) and a pen's. Up to 16 pointers
    hold a button at once; a down from a 17th is counted into the drop, so its up is dropped too.
  - A `PointerCancel` ends what its pointer holds, with no ups; one for a pointer holding nothing is dropped.
  - Every pointer event carries the buttons the window holds for its pointer after it, and a mouse's pressure from them:
    0.5 while a button is held, else 0, the W3C's rule for a device that reports none.
  - A key's down is a press (`repeat` false) when the key is not held, and holds it; it is a repeat when the key is
    held. The window says which, whatever the system said.
  - `Key::Unknown`, which names every key the table does not, is never held: its downs and ups pass as the system sends
    them.
- **A focus change, either way, releases everything held, then says `Focus`.**
  - Held pointers end first, in the order they went down: a mouse's buttons go up by number, and a touch or pen contact
    is cancelled, since it never lifted and a tap must not fire. Each is at its pointer's last position.
  - Then held keys go up by ascending HID id, so the eight modifiers go last: the action goes up before the modifier
    that chose it.
  - Each up carries the modifier bits of the keys still held after it, and the lock bits as the last key or button
    event reported them.
  - On a loss while locked, the lock is given back after the ups and before `Focus`.
  - It cannot be turned off: once the focus has gone the system sends the window no more key or button events.
- **No up or cancel is counted into a drop.** The ring keeps a slot for the end of each held key and button. A down
  that finds no room for itself and its up is counted into the drop and not held, so its up is dropped too, and the
  key, if still held, comes back as a press with its next repeat. While the ring overflows, an up or a cancel goes
  before the `Dropped` record, which stays last and goes on counting. `Focus` is an ordinary event: under overflow it may be counted into a drop
  after the ups.
- **What coalesces, and how.** A move, a scroll or a resize joins the newest unread record of its kind:
  - a pointer's move joins only that pointer's move (the same id and kind): it is the newer sample, its position,
    pressure and tilt, with the motion summed;
  - scrolls in the same unit sum;
  - resizes and scale changes keep the last.
- **`pump()` is explicit.** An implicit pump inside `pollEvent` could livelock under a high-rate mouse.
- **The mouse, touch and a pen are one stream of pointer events,** as the W3C's Pointer Events are: each carries its
  `Pointer`, and every button has its own down and up, a second button pressed while one is held included.
- **Keys are physical.** A `Key` is named as the DOM's `KeyboardEvent.code` names it and numbered by its USB HID usage
  id, whatever the layout. Typed text arrives separately, as `Text`.
- **Positions and sizes are logical,** and `scale()` converts them to pixels.
- **A window's native record is counted:** its `Window` holds it, and so does each `NativeWindow` taken from it. The
  record, and the system window behind it, go with the last of them, in either order, so a handle's display and window
  stay valid while it lives.
- **Dropping a `Window` closes it at once, as the user sees it:** natively it is hidden, and takes no more events.
  Its GLFW window is destroyed with the record's last holder.
- **A window is not `Sendable`, and neither is a `NativeWindow`:** they live on the thread that opened the window,
  which GLFW requires. `glfwInit` and `glfwTerminate` are counted across records, so GLFW is terminated with the last
  record that has a GLFW window, and a closed window still held keeps it up.

## Window

- **`Window.open(title:, width:, height:) -> Result<Window, WindowError>`:** a system window, `width` × `height`
  logical, opened with the default options. `WindowError::Refused(words)` gives the system's words when it will not
  open one. On the web it refuses.
- **`Window.openWith(title:, width:, height:, options:)`** opens one as a `WindowOptions` says, named as kama names a
  build plus its settings (`connectWith`). `WindowOptions.make()` gives the defaults, and its fields are public:
  - `focused`, true by default: the window takes the keyboard focus as it opens, as a game should at launch. False
    leaves the focus where it is, as a tool's or a test's window should; natively both of GLFW's focus hints are set
    from it on every open, so one open's choice never leaks into the next.
- **`Window.headless(width:, height:)`.**
- **`pump()`** takes the system's pending events into the queue: natively, every open window's. Headless, nothing.
- **`pollEvent() -> Optional<Event>`:** the oldest event, or `None` when the queue is empty.
- **`post(event:)`:** an event into the queue as if the system had sent it: coalesced like one, and moving what is held
  like one, so a posted `Focus` releases what is held first. A posted `Resize` also sets the size. A replayed stream
  releases nothing twice: its recorded ups release their keys, and its `Focus` then finds nothing held.
- **`releaseHeld()`:** an up for everything held, as a focus change makes them, with no `Focus` after: a program's own
  release, for a reason only it knows (a remote desktop that drops key ups, a long stall).
- **Sizes:**
  - `size() -> Size`, logical;
  - `framebufferSize() -> Size`, in pixels;
  - `scale() -> float32`, pixels per logical unit.
- **`setCursor(mode:)`:** `CursorMode::Normal`, `Hidden` or `Locked` (hidden, with raw and unbounded motion). A
  `PointerLock` event says when a lock takes hold. **Losing the focus while locked gives the pointer back,** as a
  browser does on blur.
- **`nativeHandle() -> NativeWindow`:** what a GPU surface is made from, holding the window's record. A headless
  window answers `NativeKind::None`. Natively only Win32's handles are answered so far.
- **Dropping a window closes it:** hidden at once, natively, with no more events. A `NativeWindow` taken from it, or a
  GPU surface holding one, keeps its system window valid until the last of them is dropped, and reads it as closed.

## Event

| Event | Carries |
|---|---|
| `KeyDown(key, mods, repeat)` | the physical key, the modifier bits, and whether it is a repeat: the key was already held |
| `KeyUp(key, mods)` | |
| `Text(c)` | one codepoint of typed text |
| `PointerMove(pointer, dx, dy)` | the pointer, its position logical, and the motion; while locked, the position means nothing and `dx`, `dy` are raw |
| `PointerDown(pointer, button, mods)` | a button pressed: a mouse's, a touch contact (`Left`), a pen's tip (`Left`), barrel (`Right`) or eraser |
| `PointerUp(pointer, button, mods)` | |
| `PointerCancel(pointer)` | the pointer's buttons ended with no up: the system took it, or a focus change ended a touch or pen contact |
| `PointerEnter(pointer)`, `PointerLeave(pointer)` | the pointer came over the window, or left it |
| `Scroll(dx, dy, unit)` | positive is left and up, as GLFW and winit report; `unit` is `Lines` or `Pixels`, as the system said |
| `Resize(width, height)` | logical |
| `ScaleChanged(scale)` | pixels per logical unit, when the window's scale changes: another display, or the system's setting |
| `Focus(focused)` | |
| `PointerLock(locked)` | |
| `CloseRequested` | |
| `Dropped(count)` | the queue was full, and this many events were lost |

- **`Mods`** are bits: `Shift`, `Control`, `Alt`, `Super`, `CapsLock` and `NumLock`, as in `(mods & Mods::Shift) != 0`.
- **`Pointer`** is a plain value, a W3C PointerEvent's fields: `id` (the mouse is 1; each touch contact has its own,
  never reused), `kind` (`PointerKind::Mouse`, `Touch` or `Pen`), `primary`, `x`, `y`, `buttons` (a bit per
  `PointerButton`, what the window holds), `pressure` (0 to 1), `tiltX`, `tiltY` (degrees), `twist` (degrees), `width`
  and `height` (the contact's size). `Pointer.at(id:, kind:, x:, y:)` is a primary pointer of one unit, at rest, for
  `post`; `pressed(button:)` reads `buttons`.
- **`PointerButton`** is numbered as the W3C's `button` is: `Left` 0, `Middle` 1, `Right` 2, `Back` 3, `Forward` 4,
  `Eraser` 5, then `Sixth`, `Seventh` and `Eighth`. GLFW's mouse buttons translate through one table.
- **`ScrollUnit`** is `Lines` (a wheel's notches natively) or `Pixels`. Nothing converts one into the other.
- **`Key`** has 119 keys, each the DOM's `code` name with its HID id: letters, digits, the editing and navigation keys,
  F1 to F24, the keypad, the modifiers left and right, and the international keys. GLFW's key tokens translate through
  one table; a token the table does not map is `Key::Unknown`, and no two map to one `Key`.

## NativeWindow

A window's system handles, holding its native record. It is a resource, and `Copyable`: a copy is another hold.
- **`kind() -> NativeKind`:** `None`, `Win32`, `X11`, `Wayland`, `Cocoa` or `Canvas`, saying whose handles
  `display()` and `window()` are (the module or display, and the window: an HINSTANCE and an HWND on Win32).
- **`isOpen()`:** false once the window's `Window` is dropped. The handles stay valid while the `NativeWindow` lives,
  but nothing should present to the window again.
- **`NativeWindow.adopt(kind:, display:, window:)`:** the handles of a window made elsewhere. It holds nothing, so its
  maker keeps the window alive, and it is always open.

## Plain values

- **`Size`:** `width` and `height`, made memberwise: `Size.of(width:, height:)`.

## What each backend does that the others do not

- **Pointers, natively: a mouse alone.** GLFW reports no touch or pen (WIN-11): the mouse is pointer 1, primary, one
  unit across, its enter and leave from GLFW's cursor-enter callback.
- **Held keys and buttons: nothing.** Every backend gives the same events. GLFW releases the keys and buttons it holds
  itself, after the focus callback and only on a loss, each with mods 0; the window has released them by then, so each
  of those ups is dropped (measured on GLFW 3.5.1, AGENTS.window.md item 5). A browser sends no `keyup` after a blur.

## Tests

`tools/test.sh` is the gate. It runs:
- `kama check` on the package and its tests;
- `tests/`, one program, natively, debug and release: 133 cases. They cover:
  - the queue's order, coalescing and drops on a headless window;
  - what is held, on a headless window (`tests/src/held.kama`): the pairing of ups and downs, `Key::Unknown`, the
    release's order and bits, the room kept for ups in a full ring, a replayed stream, and `releaseHeld`;
  - pointer events, on a headless window (`tests/src/pointers.kama`): coalescing per pointer, the buttons and a mouse's
    pressure the window says a pointer holds, a pen's fields, pairing per pointer and button, cancels, a focus change
    cancelling contacts in the order they went down, 16 pointers held at once, the room kept for a cancel, and
    scrolls by unit;
  - GLFW's own focus path on a hidden system window, driven by Win32 messages sent to it, so it never takes the focus:
    GLFW's ups after the focus callback dropped, a key released once, either way;
  - the plain values;
  - the GLFW key table, swept through the externed translator (118 keys mapped, none twice), and its button table;
  - the native record's count, read through the seam's `winseam_live_records()`: a handle outliving its window and
    reading it as closed, copies, either drop order, and an adopted handle;
  - a system window, and its handle outliving it, still a window and hidden, through Win32 hooks of the tests' own;
  - the options: an unfocused window shown without taking the foreground, and, on hidden windows read through GLFW's
    `FOCUS_ON_SHOW`, an unfocused open asking for no focus and a plain open after it asking for the focus again.

  The system windows are skipped when `KAMA_WINDOW_NO_DISPLAY` is set. **A test run never takes the keyboard:** every
  window it shows opens with `focused: false` (`tests/src/quiet.kama`), only after the hidden cases have passed, and
  the program shows no more once one has taken the foreground;
- the same program under node: 97 cases, the headless ones, and checks that opening a window refuses.
- the same program in a headless Chromium (Edge or Chrome; `KAMA_WINDOW_BROWSER` names another), built for the web with
  its own page, `tests/web/shell.html`: 97 cases. `tools/webtest.mjs` (node, no npm packages) serves it, runs the
  browser over the DevTools protocol with a throwaway profile at a device scale of 1.5, and reads the console to the
  summary line, an abort, an exception or a time limit. A case that needs the browser's own behaviour asks the driver
  for trusted input (a click, a key, a touch) by printing a request. Skipped, and said so, with no browser, `emcc` or
  `node`.
