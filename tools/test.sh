#!/usr/bin/env bash
# tools/test.sh — @kama/window's gate: the tests program built debug and release and run natively, the wasm leg built
# debug and release and run under node (its headless cases), and the browser leg: the same program built for the web,
# debug and release, with its own page (tests/web/shell.html) and run headless in every browser tools/browsers.mjs finds
# (AGENTS.window.md item 10): under `kama run --target WASM` in Edge and Chrome as installed and in the pinned Firefox,
# skipping what needs the driver, and under tools/webtest.mjs, the driver, with `--driver`, in the pinned Chrome for
# Testing (KAMA_BROWSER names one). A release page's JS is held to Closure's renaming (tools/closure-names.mjs).
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

# Every leg runs a debug build and a release build, natively and on the web (kyoten's rendering.md § Decided,
# 2026-11-17): a wasm release runs Closure over the page's JS since kama 0.9.966 (KR-289), which renames every name it
# cannot prove the browser's, and a game ships that build.
#
# Each run's first line is what its program was built as (`build: debug` or `build: release`), so a leg that lost its
# `--release` fails here rather than passing as the leg it names.
builtAs() {
    local log="$1" mode="$2" leg="$3"
    if tr -d '\r' < "$log" | grep -qx "build: $mode"; then
        echo "ok   $leg ran a $mode build"
    else
        echo "FAIL $leg: its program never said \"build: $mode\", so the leg ran another build"
        failed=1
    fi
}
# A release build's JS that still names emscripten's `UTF8ToString` was never run through Closure: a root manifest's
# `"release": { "closure": false }` would do that, and the leg would test nothing of what a game ships.
closured() {
    local js="$1" leg="$2"
    if grep -q 'UTF8ToString' "$js"; then
        echo "FAIL $leg: $js still names UTF8ToString, so Closure never ran over it"
        failed=1
    else
        echo "ok   $leg: Closure ran over $js"
    fi
}

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
        "$out" 2>&1 | tee "tests/out/native-$mode.log" || failed=1
        builtAs "tests/out/native-$mode.log" "$mode" "native, $mode"
    else
        tail -20 "tests/out/build-$mode.log"
        failed=1
    fi
done

for mode in debug release; do
    step "wasm leg, $mode, under node"
    if command -v emcc >/dev/null 2>&1 && command -v node >/dev/null 2>&1; then
        flag=""
        [ "$mode" = release ] && flag="--release"
        out="tests/out/wasm-$mode/windowtests.js"
        if "$KAMA" build tests/kama.json $flag --target WASM -o "$out" > "tests/out/build-wasm-$mode.log" 2>&1; then
            KAMA_WINDOW_NO_DISPLAY=1 node "$out" 2>&1 | tee "tests/out/wasm-$mode.log" || failed=1
            builtAs "tests/out/wasm-$mode.log" "$mode" "the wasm leg, $mode"
            if [ "$mode" = release ]; then closured "$out" "the wasm leg, release"; fi
        else
            tail -20 "tests/out/build-wasm-$mode.log"
            failed=1
        fi
    else
        echo "skip no emcc or node on PATH (run from an interactive UCRT64 shell, bash -lic)"
    fi
done

# A program that depends on the package and reaches none of it still links (WIN-26): kama compiles and links a
# dependency's C whether or not its kama is reached, so the C names nothing that lives only in the package's kama. Found
# when an `expose fn` the C called was missing from kyoten's frame guard; natively, and for the web under node.
step "a program that reaches none of the package links (tests/fixtures/unreached)"
"$KAMA" pkg install tests/fixtures/unreached/kama.json > /dev/null
unreachedRan() {
    local log="$1" leg="$2"
    if tr -d '\r' < "$log" | grep -qx "unreached: linked"; then echo "ok   $leg: a program reaching none of the package links and runs"
    else echo "FAIL $leg: a program reaching none of the package did not link and run"; tail -5 "$log"; failed=1; fi
}
if "$KAMA" build tests/fixtures/unreached/kama.json -o "tests/out/unreached/unreached$EXE" > tests/out/unreached-build.log 2>&1; then
    "tests/out/unreached/unreached$EXE" > tests/out/unreached-native.log 2>&1 || true
    unreachedRan tests/out/unreached-native.log "natively"
else
    tail -5 tests/out/unreached-build.log
    echo "FAIL natively: a program reaching none of the package did not link"
    failed=1
fi
if command -v emcc >/dev/null 2>&1 && command -v node >/dev/null 2>&1; then
    if "$KAMA" build tests/fixtures/unreached/kama.json --target WASM -o tests/out/unreached-wasm/unreached.js \
            > tests/out/unreached-wasm-build.log 2>&1; then
        node tests/out/unreached-wasm/unreached.js > tests/out/unreached-wasm.log 2>&1 || true
        unreachedRan tests/out/unreached-wasm.log "on wasm"
    else
        tail -5 tests/out/unreached-wasm-build.log
        echo "FAIL on wasm: a program reaching none of the package did not link"
        failed=1
    fi
else
    echo "skip no emcc or node on PATH (run from an interactive UCRT64 shell, bash -lic)"
fi

step "browser leg, headless"
if ! command -v emcc >/dev/null 2>&1 || ! command -v node >/dev/null 2>&1; then
    echo "skip no emcc or node on PATH (run from an interactive UCRT64 shell, bash -lic)"
elif ! "$KAMA" build tests/kama.json --target WASM -o tests/out/web-debug/windowtests.html > tests/out/build-web-debug.log 2>&1; then
    tail -20 tests/out/build-web-debug.log
    failed=1
elif ! "$KAMA" build tests/kama.json --release --target WASM -o tests/out/web-release/windowtests.html \
        > tests/out/build-web-release.log 2>&1; then
    tail -20 tests/out/build-web-release.log
    failed=1
else
    # The release page's JS: run through Closure, and every name js/winseam.js hands the browser kept, since most of the
    # paths that read them (a pad's rumble, pointer lock's raw motion) no headless case reaches. The tests' own JS too:
    # its fake pads stand in for the browser's, and the driver sets its acknowledgement from outside the program.
    closured tests/out/web-release/windowtests.js "the release page"
    node tools/closure-names.mjs js/winseam.js tests/out/web-release/windowtests.js || failed=1
    node tools/closure-names.mjs tests/js/windowtests.js tests/out/web-release/windowtests.js || failed=1
    # Under `kama run`, Firefox among the browsers, at a device scale of 1.5: the cases that need trusted input are
    # skipped, each saying so (kama's KR-378). The scale cases run, so a run that skips one has failed to set the scale.
    if browsers="$(node tools/browsers.mjs run 2>/dev/null)"; then
        while IFS= read -r browser; do
            for mode in debug release; do
                flag=""
                [ "$mode" = release ] && flag="--release"
                echo "-- kama run, $browser, $mode, at a device scale of 1.5"
                log="tests/out/kama-run-$mode.log"
                KAMA_BROWSER="$browser" bounded "$KAMA" run tests/kama.json $flag --target WASM --headless --scale 1.5 \
                    --window 1280x1024 > "$log" 2>&1 || failed=1
                cat "$log"
                builtAs "$log" "$mode" "kama run, $browser, $mode"
                if grep -q "needs a device scale of 1.5" "$log"; then
                    echo "FAIL the run at a device scale of 1.5 skipped a scale case: the page was not at that scale"
                    failed=1
                fi
            done
        done <<< "$browsers"
    else
        echo "skip no browser for kama run (Edge or Chrome installed, or Firefox pinned under ~/browsers; or set KAMA_BROWSER)"
    fi
    # The driver, every case run: trusted input and a device scale of 1.5, in the pinned Chrome for Testing. Its run skips
    # nothing that waits on the driver, or the program has failed to see `--driver` and passed by skipping.
    if browsers="$(node tools/browsers.mjs driver 2>/dev/null)"; then
        while IFS= read -r browser; do
            for mode in debug release; do
                echo "-- the driver, $browser, $mode"
                log="tests/out/driver-run-$mode.log"
                node tools/webtest.mjs "tests/out/web-$mode" "windowtests.html?arg=--driver" --browser "$browser" > "$log" 2>&1 || failed=1
                cat "$log"
                builtAs "$log" "$mode" "the driver, $browser, $mode"
                if grep -q "needs a device scale of 1.5\|(kama's KR-378)" "$log"; then
                    echo "FAIL the driver's run skipped a case: the program did not see --driver, or the page its scale"
                    failed=1
                fi
            done
        done <<< "$browsers"
    else
        echo "skip no Chrome for Testing pinned under ~/browsers, so no driver run"
    fi
fi

echo
if [ "$failed" = 0 ]; then echo "test: all passed"; else echo "test: FAILED" >&2; fi
exit "$failed"
