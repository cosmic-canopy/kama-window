# Changelog

All notable changes to `@kama/window`. The format is [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this
package follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

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
