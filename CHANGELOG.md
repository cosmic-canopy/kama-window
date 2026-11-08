# Changelog

All notable changes to `@kama/window`. The format is [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this
package follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

- The native window and the headless one (2026-11-07, kama 0.9.874): `Window` (`open`, `headless`, `pump`,
  `pollEvent`, `post`, `size`, `framebufferSize`, `scale`, `setCursor`, `nativeHandle`), `Event`, `Key` (USB HID ids,
  DOM `code` names), `MouseButton`, `Mods` and `CursorMode`, over a C seam (`csrc/winseam.c`): one fixed event ring
  per window that coalesces moves, scrolls and resizes and reports `Dropped(n)` in order, filled natively by a GLFW
  window's callbacks through its user pointer and by `post` on a headless one. `tests/` passes natively (33 cases,
  debug and release, a real window included) and under node (17, the headless ones). On the web, opening a window
  refuses until the html5 half is built. Windows links MSYS2's static GLFW 3.5.1; vendoring GLFW comes before
  publishing.
- Seeded with `kama seed` on kama 0.9.831 (2026-11-03).
