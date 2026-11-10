# @kama/window — the specification

What this package **is**, written after each piece lands. Never speculative: what is described here exists and is
tested. What comes next is [ROADMAP.md](ROADMAP.md), why the package departs from kama's design is
[AGENTS.window.md](AGENTS.window.md), and its history is the git log and [CHANGELOG.md](CHANGELOG.md).

**Where it stands** (2026-11-10, kama 0.9.911): a system window natively on Windows (GLFW 3.5.1, MSYS2's static
build), and a headless window on every target, each with a counted native record, so a GPU surface holds its window.
On the web, opening a window refuses until the web half is built. Not published.

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
  reserved one is counted into the drop.
- **What coalesces, and how.** A mouse move, a scroll or a resize joins the newest unread record of its kind:
  - moves keep the last position and sum the motion;
  - scrolls sum;
  - resizes and scale changes keep the last.
- **`pump()` is explicit.** An implicit pump inside `pollEvent` could livelock under a high-rate mouse.
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
  logical. `WindowError::Refused(words)` gives the system's words when it will not open one. On the web it refuses.
- **`Window.headless(width:, height:)`.**
- **`pump()`** takes the system's pending events into the queue: natively, every open window's. Headless, nothing.
- **`pollEvent() -> Optional<Event>`:** the oldest event, or `None` when the queue is empty.
- **`post(event:)`:** an event into the queue as if the system had sent it, coalesced like one. A posted `Resize` also
  sets the size.
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
| `KeyDown(key, mods, repeat)` | the physical key, the modifier bits, and whether it is the system's repeat |
| `KeyUp(key, mods)` | |
| `Text(c)` | one codepoint of typed text |
| `MouseMove(x, y, dx, dy)` | the position (logical) and the motion; while locked, `x` and `y` mean nothing and `dx`, `dy` are raw |
| `MouseButton(button, down, mods)` | |
| `Scroll(dx, dy)` | |
| `Resize(width, height)` | logical |
| `ScaleChanged(scale)` | pixels per logical unit, when the window's scale changes: another display, or the system's setting |
| `Focus(focused)` | |
| `PointerLock(locked)` | |
| `CloseRequested` | |
| `Dropped(count)` | the queue was full, and this many events were lost |

- **`Mods`** are bits: `Shift`, `Control`, `Alt`, `Super`, `CapsLock` and `NumLock`, as in `(mods & Mods::Shift) != 0`.
- **`MouseButton`** is `Left`, `Right`, `Middle`, `Back`, `Forward`, `Sixth`, `Seventh` or `Eighth`.
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

- **GLFW releases held keys and buttons on focus loss itself,** after the focus callback and never on gain, so natively
  `Focus(false)` arrives before the key ups. This was read in GLFW 3.4's `_glfwInputWindowFocus`, not measured. A
  consumer releases what it holds on `Focus(false)` itself until the window tracks it (AGENTS.window.md, item 5).

## Tests

`tools/test.sh` is the gate. It runs:
- `kama check` on the package and its tests;
- `tests/`, one program, natively, debug and release: 56 cases. They cover:
  - the queue's order, coalescing and drops on a headless window;
  - the plain values;
  - the GLFW key table, swept through the externed translator (118 keys mapped, none twice);
  - the native record's count, read through the seam's `winseam_live_records()`: a handle outliving its window and
    reading it as closed, copies, either drop order, and an adopted handle;
  - a system window, and its handle outliving it, still a window and hidden, through Win32 hooks of the tests' own.
    These are skipped when `KAMA_WINDOW_NO_DISPLAY` is set, and their windows open unfocused (AGENTS.window.md,
    item 6);
- the same program under node: 33 cases, the headless ones, and a check that opening a window refuses.
