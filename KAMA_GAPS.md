# Gaps found in kama, from building @kama/window

What this package hits in kama. Each entry is reduced to the smallest program that shows it and **run** on the version
it names. Never edit `../kama` from here: this file is how kama hears about it, relayed by the owner.

**Current compiler:** kama 0.9.928+g94b8025b (kyoten's pin, 2026-11-12).

Ids are `KWIN-<n>` and are permanent. **Next id: KWIN-2**

## Open

None.

## Closed

A closed entry keeps one line here; its whole text and repro are in this file's history.

- **KWIN-1: writing an `extern value`'s `InlineArray` field passed `kama check` and failed in C.** Found 2026-11-12 on
  0.9.911, fixed in 0.9.928 by kama KR-379 (`94b8025b`): an element write, a whole write and a whole read all build
  and run (kyoten's `.scratch/win4-gamepads/gap-extern-array-928/`). The workaround is retired: `Window.pad` assigns
  each array whole, and `Window.postPad` hands the seam a `winseam_pad` (`winseam_pad_sample`), so `winseam_pad_post`
  is gone.
