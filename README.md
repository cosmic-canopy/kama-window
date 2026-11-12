# @kama/window

A window and its events, for [kama](https://kama-lang.org): several windows and a headless mode, one coalescing event
queue, physical keys and text, an explicit `pump()`, and gamepads.

**Status: the native window (Windows), the web window and the headless one work, with gamepads, not yet published.**
[SPEC.md](SPEC.md) is what it is, and [ROADMAP.md](ROADMAP.md) what comes next. Natively on Windows it compiles SDL3's
gamepad subsystem, vendored (zlib; `third_party/VENDORED.md`). The design is kama's (its `docs/ROADMAP_DETAIL.md` § 8, KR-26), and kyoten builds it
as its first consumer.

Licensed under either of [MIT](LICENSE-MIT) or [Apache-2.0](LICENSE-APACHE), at your option.
