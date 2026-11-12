# Gaps found in kama, from building @kama/window

What this package hits in kama. Each entry is reduced to the smallest program that shows it and **run** on the version
it names. Never edit `../kama` from here: this file is how kama hears about it, relayed by the owner.

**Current compiler:** kama 0.9.911+g238198b3 (kyoten's pin, 2026-11-12).

Ids are `KWIN-<n>` and are permanent. **Next id: KWIN-2**

## Open

### KWIN-1: writing an `extern value`'s `InlineArray` field passes `kama check` and fails in C

Found building WIN-4 (gamepads), 2026-11-12, on kama 0.9.911+g238198b3. A C struct with an array field
(`float v[4]`) is declared to kama as `type extern value` with an `InlineArray<float32>#(4)` field, the seam
documented in kama's ENGINE_READINESS ("InlineArray bridges"). **Reading an element compiles** (the lowering casts
`&p.v` to kama's array struct). **Writing one, or assigning the whole array, passes `kama check` and is refused by
clang:**

```c
// csrc/gap.h
#include <stdint.h>
typedef struct gap_pad { int32_t n; float v[4]; } gap_pad;
```

```kama
// src/main.kama, in an executable whose kama.json has "cincludes": ["csrc"]
import { core::println };
extern "gap.h";

@generate(zero)
type extern value gap_pad {
    public int32 n;
    public InlineArray<float32>#(4) v;
}

fn uint8 main() {
    gap_pad p = gap_pad.zero();
    float32 read = p.v[1];   // compiles
    p.v[0] = 5.0;            // clang: incompatible pointer types passing 'float (*)[4]' to parameter of type
                             // 'InlineArray__kama_of__float32__kama_and__4__kama_end *'
    println(s: "${read}");
    return 0;
}
```

Replacing the element write with `InlineArray<float32>#(4) a = [1.0, 2.0, 3.0, 4.0]; p.v = a;` gives clang's "array
type 'float[4]' is not assignable". `kama check kama.json` is OK for both (`1 unit analyzed`).

**Expected:** the write lowers with the same cast the read uses (or a `memcpy` for a whole array), or `kama check`
refuses it by name, so the C compiler is never the first to say.

**Worked around** in `src/window.kama`: `Window.pad` reads element by element, and `Window.postPad` passes the
addresses of its own `Gamepad`'s arrays (`addr(of: pad.buttons[0])`) to `winseam_pad_post`, which builds the C struct.
The repro is kyoten's `.scratch/win4-gamepads/gap-extern-array/`.
