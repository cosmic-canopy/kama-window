# Changelog

All notable changes to `@kama/window`. The format is [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this
package follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

- **Under `WindowOptions.sizedByPage`, `open` refuses a canvas the page sizes on neither axis** (WIN-21, 2026-11-11,
  kama 0.9.911). Such a canvas is laid out at its pixel size, and the window sets the pixels from the layout × the
  device pixel ratio, so above a ratio of 1 it grew each frame. Before touching anything of the canvas, `open` changes
  its pixels once, reads its layout, and puts them back, and refuses with `WindowError::Refused` in words naming the
  fix when both axes followed. A canvas the page sizes on one axis still opens: its pixels' aspect gives the other,
  and it holds. **Refused now, though it held before:** CSS that sizes the canvas only after `open`, and `max-width`
  alone. `tests/` gains 9 cases in each browser, 189 in all.
- **`NativeWindow.framebufferSize()`** (WIN-20, 2026-11-11, kama 0.9.911): the window's size in pixels through its
  handle, as `Window.framebufferSize()` gives it, so a GPU surface holding the handle configures at the window's size
  and the window stays the one writer of a canvas's pixels (`@kama/gpu`'s GPU-3, decision 2). 0 × 0 once the window is
  closed, and for an adopted handle. `tests/` gains 6 cases natively, 3 under node and 5 in a browser.
- The browser leg runs in every headless Chromium present: a Chrome for Testing pinned under `~/browsers`, Edge and
  Chrome. `KAMA_BROWSER` names one, in place of `KAMA_WINDOW_BROWSER`, and `tools/webtest.mjs --list` says which.

- **A window on the web: the page's `#canvas`** (WIN-3, 2026-11-11, kama 0.9.911; AGENTS.window.md item 8).
  - The package links a JS library (`js/winseam.js`) whose listeners queue records that `pump()` pulls into the ring,
    so nothing calls into the program from outside.
  - Keys come from `KeyboardEvent.code` through one table, `Text` from a keydown's `key`, and the window takes the
    defaults of the keys it names, a pointer down, the context menu and the wheel.
  - Focus is the canvas's, and `focused` focuses it at open; the window gives the canvas a `tabindex` and
    `touch-action: none`, and captures a pointer until its up.
  - The canvas's CSS size is set from `width` and `height`, unless `WindowOptions.sizedByPage`; its backing store
    follows its exact device pixels, and a change is a `Resize` or `ScaleChanged`.
  - `setCursor(Locked)` asks for the lock at the next click; the browser giving it back sets the mode to `Normal`.
  - `nativeHandle()` answers `NativeKind::Canvas` with the selector.
  - `Window.openIn(canvas:, title:, width:, height:, options:)` opens on the canvas a selector names, several canvases
    being several windows; natively the selector goes unused. `open` and `openWith` take `#canvas`.
  - `winseam_open` takes the canvas's selector and `sized_by_page`.
  - `tests/` runs in headless Edge: 175 cases there, 100 under node, 134 natively.

- **Breaking: the mouse, touch and a pen are one stream of pointer events,** as the W3C's Pointer Events are (WIN-3,
  2026-11-11, kama 0.9.911; AGENTS.window.md item 8, decision 2).
  - `PointerMove`, `PointerDown`, `PointerUp`, `PointerCancel`, `PointerEnter` and `PointerLeave` replace `MouseMove`
    and `MouseButton`, each carrying a `Pointer` (`id`, `kind`, `primary`, `x`, `y`, `buttons`, `pressure`, `tiltX`,
    `tiltY`, `twist`, `width`, `height`).
  - `PointerButton` replaces `MouseButton`, numbered as the W3C's `button` is: `Middle` is 1 and `Right` 2, and `Eraser`
    is 5.
  - The held set is per pointer, for up to 16 at once; a focus change cancels a held touch or pen contact rather than
    lifting it.
  - A move coalesces only with its own pointer's.
  - `Scroll(dx, dy, unit)` carries a `ScrollUnit`, `Lines` or `Pixels`; natively it is lines.
  - Natively the mouse is pointer 1, and GLFW's cursor-enter callback gives `PointerEnter` and `PointerLeave`.
  - `tests/` gains 36 cases.

- **Breaking: the window keeps what is held, and releases it on a focus change** (WIN-2, 2026-11-10, kama 0.9.911;
  AGENTS.window.md item 5).
  - An up for a key or button not held is dropped, as is a second down for a held button.
  - A key's `repeat` is the window's: a down for a key not held is a press, and one for a held key a repeat.
    `Key::Unknown` is never held.
  - A focus change, either way, releases every held button, then every held key by HID id, before `Focus`. Each up
    carries the modifiers still held after it. GLFW's own releases are dropped.
  - The ring keeps a slot for each held key's or button's up, so no up is counted into a drop.
  - `post` follows the same rules, and `Window.releaseHeld()` releases on a program's request.
  - `tests/` gains 36 cases.
- **A window can open without taking the focus** (WIN-16, 2026-11-10, kama 0.9.911; AGENTS.window.md item 6):
  `Window.openWith(title:, width:, height:, options:)` takes a `WindowOptions` (`WindowOptions.make()`, its fields
  public), whose `focused` is true by default. `Window.open` is unchanged and takes the defaults. `tests/` opens every
  window through it, in place of GLFW hints of its own, and gains 5 cases.
- **Breaking: a window's native record is counted, and `NativeWindow` holds it** (WIN-1, 2026-11-10, kama 0.9.911;
  AGENTS.window.md item 6).
  - `NativeWindow` is a `Copyable` resource: `kind()`, `display()`, `window()` and `isOpen()` replace its public
    fields, and `NativeWindow.adopt(kind:, display:, window:)` replaces `NativeWindow.of`.
  - A handle keeps its window's system window valid after the `Window` is dropped, which now hides the window at once
    and destroys it with the last handle, so a GPU surface holding one drops safely in either order.
  - `tests/` gains 18 cases.
- **Breaking:** the two plain values are made memberwise, by kama's rule (2026-11-10, kama 0.9.911, its SPEC
  § Construction, KR-367): `Size.make(width:, height:)` is `Size.of(…)`, and `NativeWindow.make(kind:, display:,
  window:)` is `NativeWindow.of(…)`, each written by `@generate(of)`. `Window.open` and `Window.headless` keep their
  names. Entries below keep the names of their day.
- Fixed (2026-11-08, found by kama's review): the event ring counted an event into a trailing `Dropped` record
  even after the reader had made room, so a key up arriving then was lost. It now goes after the drop; only an event
  that finds no free slot but the reserved one is counted in. Two tests hold it: a partial drain, then a key up that
  must arrive, and the boundary where only the reserved slot is free.

- The native window and the headless one (2026-11-07, kama 0.9.874): `Window` (`open`, `headless`, `pump`,
  `pollEvent`, `post`, `size`, `framebufferSize`, `scale`, `setCursor`, `nativeHandle`), `Event`, `Key` (USB HID ids,
  DOM `code` names), `MouseButton`, `Mods` and `CursorMode`, over a C seam (`csrc/winseam.c`): one fixed event ring
  per window that coalesces moves, scrolls and resizes and reports `Dropped(n)` in order, filled natively by a GLFW
  window's callbacks through its user pointer and by `post` on a headless one. `tests/` passes natively (33 cases,
  debug and release, a real window included) and under node (17, the headless ones). On the web, opening a window
  refuses until the html5 half is built. Windows links MSYS2's static GLFW 3.5.1; vendoring GLFW comes before
  publishing.
- Seeded with `kama seed` on kama 0.9.831 (2026-11-03).
