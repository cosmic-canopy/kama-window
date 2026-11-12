#!/usr/bin/env bash
# tools/test.sh — @kama/window's gate: the tests program built debug and release and run natively, the wasm leg built
# and run under node (its headless cases), and the browser leg: the same program built for the web with its own page
# (tests/web/shell.html) and run headless in every browser tools/browsers.mjs finds (AGENTS.window.md item 10): under
# `kama run --target WASM` in Edge and Chrome as installed and in the pinned Firefox, skipping what needs the driver, and
# under tools/webtest.mjs, the driver, with `--driver`, in the pinned Chrome for Testing (KAMA_BROWSER names one).
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
# A browser run, bounded where `timeout` exists: `kama run` has no time limit, and a browser that never asks for the page
# waits for ever (kyoten's KY-53).
bounded() { if command -v timeout >/dev/null 2>&1; then timeout 300 "$@"; else "$@"; fi; }
mkdir -p tests/out

step "the roadmap"
tools/check-roadmap.sh || failed=1

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

step "browser leg, headless"
if ! command -v emcc >/dev/null 2>&1 || ! command -v node >/dev/null 2>&1; then
    echo "skip no emcc or node on PATH (run from an interactive UCRT64 shell, bash -lic)"
elif ! "$KAMA" build tests/kama.json --target WASM -o tests/out/web/windowtests.html > tests/out/build-web.log 2>&1; then
    tail -20 tests/out/build-web.log
    failed=1
else
    # Under `kama run`, Firefox among the browsers: the cases that need the driver are skipped, each saying so.
    if browsers="$(node tools/browsers.mjs run 2>/dev/null)"; then
        while IFS= read -r browser; do
            echo "-- kama run, $browser"
            KAMA_BROWSER="$browser" bounded "$KAMA" run tests/kama.json --target WASM --headless || failed=1
        done <<< "$browsers"
    else
        echo "skip no browser for kama run (Edge or Chrome installed, or Firefox pinned under ~/browsers; or set KAMA_BROWSER)"
    fi
    # The driver, every case run: trusted input and a device scale of 1.5, in the pinned Chrome for Testing. Its run skips
    # nothing that waits on the driver, or the program has failed to see `--driver` and passed by skipping.
    if browsers="$(node tools/browsers.mjs driver 2>/dev/null)"; then
        while IFS= read -r browser; do
            echo "-- the driver, $browser"
            log="tests/out/driver-run.log"
            node tools/webtest.mjs tests/out/web "windowtests.html?arg=--driver" --browser "$browser" > "$log" 2>&1 || failed=1
            cat "$log"
            if grep -q "(kyoten's KY-52)\|(kama's KR-378)" "$log"; then
                echo "FAIL the driver's run skipped a case that waits on the driver: the program did not see --driver"
                failed=1
            fi
        done <<< "$browsers"
    else
        echo "skip no Chrome for Testing pinned under ~/browsers, so no driver run"
    fi
fi

echo
if [ "$failed" = 0 ]; then echo "test: all passed"; else echo "test: FAILED" >&2; fi
exit "$failed"
