#!/usr/bin/env bash
# tools/test.sh — @kama/window's gate: the tests program built debug and release and run natively, and the wasm leg
# built and run under node (its headless cases; the web window is not built yet).
#
#   tools/test.sh
#
# Run it from msys2 UCRT64 on Windows (bash -lic, so emcc and node are on PATH for the wasm leg), or any shell
# elsewhere. Natively it opens a real window unless KAMA_WINDOW_NO_DISPLAY is set.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*) PLAT="${MSYSTEM:-$(uname -s)}-$(uname -m)"; EXE=.exe ;;
    *) PLAT="$(uname -s)-$(uname -m)"; EXE= ;;
esac
if [ -n "${KAMA:-}" ]; then :
elif command -v kama >/dev/null 2>&1; then KAMA=kama
elif [ -x "$ROOT/../kama/out/$PLAT/kama$EXE" ]; then KAMA="$ROOT/../kama/out/$PLAT/kama$EXE"
else echo "test: no kama (set KAMA)" >&2; exit 1
fi

failed=0
step() { echo; echo "== $*"; }
mkdir -p tests/out

step "kama check"
"$KAMA" check kama.json || failed=1
"$KAMA" pkg install tests/kama.json > /dev/null
"$KAMA" check tests/kama.json || failed=1

for mode in debug release; do
    step "native, $mode"
    flag=""
    [ "$mode" = release ] && flag="--release"
    out="tests/out/native-$mode/windowtests$EXE"
    if "$KAMA" build tests/kama.json $flag -o "$out" > "tests/out/build-$mode.log" 2>&1; then
        "$out" || failed=1
    else
        tail -20 "tests/out/build-$mode.log"
        failed=1
    fi
done

step "wasm leg, under node"
if command -v emcc >/dev/null 2>&1 && command -v node >/dev/null 2>&1; then
    if "$KAMA" build tests/kama.json --target WASM -o tests/out/wasm/windowtests.js > tests/out/build-wasm.log 2>&1; then
        KAMA_WINDOW_NO_DISPLAY=1 node tests/out/wasm/windowtests.js || failed=1
    else
        tail -20 tests/out/build-wasm.log
        failed=1
    fi
else
    echo "skip no emcc or node on PATH (run from an interactive UCRT64 shell, bash -lic)"
fi

echo
if [ "$failed" = 0 ]; then echo "test: all passed"; else echo "test: FAILED" >&2; fi
exit "$failed"
