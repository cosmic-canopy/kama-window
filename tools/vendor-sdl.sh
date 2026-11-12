#!/bin/sh
# vendor-sdl.sh — SDL3's gamepad backend and Microsoft's GameInput header, vendored (AGENTS.package.md, "Vendoring a C
# library"; ROADMAP_DETAIL.md § WIN-4, its decisions 1 and 9 and the rule applied after them).
#
# It owns third_party/sdl/, third_party/sdl-config/windows/, third_party/sdl-db/, third_party/gameinput/, and the
# `third_party/sdl/` and `third_party/sdl-db/` entries of kama.json's `csources` array (fenced by its own `"csources": [` line and the `],` that closes it; every other entry
# in it is kept as written). Nothing it writes is hand-edited: upgrading is a change to the constants below, a run, a
# read of the diff, and a version bump of the package.
#
# SDL is configured as WIN-4 measured it: static; audio, video, camera, dialogs and the tray off; GameInput on. SDL's
# CMake looks for GameInput's header by compiling it as C, which the header refuses (`#error GameInput requires C++`),
# so the check's answer is given (SDL's Visual Studio build uses `__has_include`). The disabled subsystems' stubs still
# compile, since SDL's dynamic API refers to every public function. The sources copied are exactly the ones that
# configuration compiles, and the headers exactly the ones they include (ninja's record of the compiler's own
# dependency lists), byte for byte; the configuration's generated `SDL_build_config.h` is kept for Windows, with clang,
# the compiler kama drives there. Linux and macOS come with WIN-12, each configured on its own system.
#
# Runs in msys2 UCRT64: curl, sha256sum, tar, unzip, cmake, ninja, clang, cygpath.
set -eu

SDL_VERSION=3.4.18
SDL_SHA256=9c75cf16330322c217dedd2e0609f1124f1b54b8633e763467b4684d0f4334a3
SDL_URL="https://github.com/libsdl-org/SDL/releases/download/release-$SDL_VERSION/SDL3-$SDL_VERSION.tar.gz"
GAMEINPUT_VERSION=3.5.283
GAMEINPUT_SHA256=b5988cb8ff9d7009b6ddf6ad4e3ff87e91b00cc17ffcd5d628dabc21c208f100
GAMEINPUT_URL="https://api.nuget.org/v3-flatcontainer/microsoft.gameinput/$GAMEINPUT_VERSION/microsoft.gameinput.$GAMEINPUT_VERSION.nupkg"
# SDL_GameControllerDB, the pads' mappings (WIN-4, decision 3): its Windows lines, shipped and loaded at each start of SDL.
GCDB_COMMIT=c0f11d40b91f57853ce9714c703e119d8243f91c
GCDB_SHA256=a2299ef257b68c62b9fcce39fc59b5fb3a5a7068214f766a96c21595b55a11c3
GCDB_LICENSE_SHA256=97f35b302b361680ec1e891e95d2d52097bb95abff361434916d99dc1305f127
GCDB_URL="https://raw.githubusercontent.com/mdqinc/SDL_GameControllerDB/$GCDB_COMMIT"

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
fail() { echo "vendor-sdl: $1" >&2; exit 1; }

fetch() {   # url, sha256, file
  curl -fsSL -o "$WORK/$3" "$1" || fail "could not download $1"
  echo "$2  $WORK/$3" | sha256sum -c - >/dev/null 2>&1 || fail "$3 is not the file pinned: its SHA-256 is not $2"
}
fetch "$SDL_URL" "$SDL_SHA256" sdl.tar.gz
fetch "$GAMEINPUT_URL" "$GAMEINPUT_SHA256" gameinput.nupkg
fetch "$GCDB_URL/gamecontrollerdb.txt" "$GCDB_SHA256" gamecontrollerdb.txt
fetch "$GCDB_URL/LICENSE" "$GCDB_LICENSE_SHA256" gamecontrollerdb-LICENSE
tar -xzf "$WORK/sdl.tar.gz" -C "$WORK"
SRC="$WORK/SDL3-$SDL_VERSION"
mkdir -p "$WORK/gameinput"
unzip -q "$WORK/gameinput.nupkg" 'native/include/GameInput.h' -d "$WORK/gameinput"
GAMEINPUT_H="$WORK/gameinput/native/include"

# Configure and build once, to learn what the configuration compiles and what that includes.
BUILD="$WORK/build"
cmake -S "$SRC" -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DSDL_SHARED=OFF -DSDL_STATIC=ON -DSDL_TEST_LIBRARY=OFF -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF \
  -DSDL_AUDIO=OFF -DSDL_VIDEO=OFF -DSDL_CAMERA=OFF -DSDL_DIALOG=OFF -DSDL_TRAY=OFF \
  -DHAVE_GAMEINPUT_H=1 "-DCMAKE_CXX_FLAGS=-I$(cygpath -m "$GAMEINPUT_H")" > "$WORK/configure.log" 2>&1 \
  || { tail -20 "$WORK/configure.log" >&2; fail "SDL's configure failed"; }
grep -q 'Joystick drivers:.*gameinput' "$WORK/configure.log" || fail "SDL configured without its GameInput driver"
ninja -C "$BUILD" SDL3-static > "$WORK/build.log" 2>&1 || { tail -20 "$WORK/build.log" >&2; fail "SDL's build failed"; }

MSRC=$(cygpath -m "$SRC")
ninja -C "$BUILD" -t commands SDL3-static | grep -oE "$MSRC/src/[A-Za-z0-9_/.+-]+\.(c|cpp)" | sed "s|^$MSRC/||" \
  | sort -u > "$WORK/sources"
ninja -C "$BUILD" -t deps | grep -oE "$MSRC/[A-Za-z0-9_/.+-]+" | sed "s|^$MSRC/||" | sort -u > "$WORK/closure"
cat "$WORK/sources" >> "$WORK/closure"
sort -u "$WORK/closure" -o "$WORK/closure"
[ -s "$WORK/sources" ] || fail "no sources found in the build's commands"

# The trees, pristine.
rm -rf "$ROOT/third_party/sdl" "$ROOT/third_party/sdl-config" "$ROOT/third_party/sdl-db" "$ROOT/third_party/gameinput"
mkdir -p "$ROOT/third_party/sdl" "$ROOT/third_party/sdl-config/windows" "$ROOT/third_party/sdl-db" "$ROOT/third_party/gameinput"
while IFS= read -r f; do
  mkdir -p "$ROOT/third_party/sdl/$(dirname "$f")"
  cp "$SRC/$f" "$ROOT/third_party/sdl/$f"
done < "$WORK/closure"
cp "$SRC/LICENSE.txt" "$ROOT/third_party/sdl/LICENSE.txt"
cp "$BUILD/include-config-release/build_config/SDL_build_config.h" "$ROOT/third_party/sdl-config/windows/SDL_build_config.h"
# Under the name SDL includes it by (`<gameinput.h>`), so a case-sensitive file system finds it too.
cp "$GAMEINPUT_H/GameInput.h" "$ROOT/third_party/gameinput/gameinput.h"
# The database's Windows lines, as one C string. Each line becomes a literal of its own; none may hold a quote or a
# backslash, which would need escaping.
grep 'platform:Windows' "$WORK/gamecontrollerdb.txt" | tr -d '\r' > "$WORK/windows.txt"
if grep -q '["\\]' "$WORK/windows.txt"; then fail "a mapping line holds a quote or a backslash"; fi
GCDB_LINES=$(wc -l < "$WORK/windows.txt" | tr -d ' ')
GCDB_ALL=$(wc -l < "$WORK/gamecontrollerdb.txt" | tr -d ' ')
{
  echo "// Written by tools/vendor-sdl.sh: SDL_GameControllerDB $GCDB_COMMIT's Windows lines ($GCDB_LINES of $GCDB_ALL),"
  echo "// zlib license (LICENSE.txt beside it). Do not edit: run the script."
  echo "#include <stddef.h>"
  echo ""
  echo "const char winseam_gamecontrollerdb[] ="
  sed 's/.*/    "&\\n"/' "$WORK/windows.txt"
  echo "    ;"
  echo "const size_t winseam_gamecontrollerdb_size = sizeof winseam_gamecontrollerdb - 1;"
} > "$ROOT/third_party/sdl-db/gamecontrollerdb.c"
cp "$WORK/gamecontrollerdb-LICENSE" "$ROOT/third_party/sdl-db/LICENSE.txt"
cat > "$ROOT/third_party/VENDORED.md" <<EOF
# Vendored by tools/vendor-sdl.sh. Do not edit: run the script.

- \`sdl/\`: SDL $SDL_VERSION ($SDL_URL, SHA-256 $SDL_SHA256), zlib license (\`sdl/LICENSE.txt\`): the
  $(wc -l < "$WORK/sources" | tr -d ' ') sources a Windows build with only the joystick, haptic, HIDAPI, power and sensor
  subsystems compiles, and the $(($(wc -l < "$WORK/closure") - $(wc -l < "$WORK/sources"))) files they include.
- \`sdl-config/windows/\`: that configuration's generated \`SDL_build_config.h\` (clang, msys2 UCRT64).
- \`sdl-db/\`: SDL_GameControllerDB $GCDB_COMMIT ($GCDB_URL/gamecontrollerdb.txt, SHA-256 $GCDB_SHA256), zlib
  license (\`sdl-db/LICENSE.txt\`): its $GCDB_LINES Windows lines of $GCDB_ALL, as one C string, loaded at each start of SDL.
- \`gameinput/\`: Microsoft's \`GameInput.h\`, as \`gameinput.h\` (the name SDL includes), from the NuGet package Microsoft.GameInput $GAMEINPUT_VERSION ($GAMEINPUT_URL,
  SHA-256 $GAMEINPUT_SHA256), MIT license (in the header). Microsoft's GameInput runtime itself is not here: it is a
  program's to install, under Microsoft's redistributable license.
EOF

# kama.json's csources: the SDL and database entries regenerated, every other entry kept.
KJ="$ROOT/kama.json"
grep -q '^  "csources": \[$' "$KJ" || fail 'kama.json has no `  "csources": [` line of its own'
{ sed 's|^|third_party/sdl/|' "$WORK/sources"; echo "third_party/sdl-db/gamecontrollerdb.c"; } > "$WORK/entries"
awk -v list="$WORK/entries" '
  /^  "csources": \[$/ { print; inside = 1; next }
  inside && /^  \],?$/ {
    n = 0; while ((getline f < list) > 0) entries[++n] = f
    for (i = 1; i <= n; i++) {
      printf "%s    { \"path\": \"%s\", \"compileFor\": [\"OS_WINDOWS\"] }", (kept || i > 1 ? ",\n" : ""), entries[i]
    }
    printf "\n"; print; inside = 0; next
  }
  inside && /third_party\/sdl(-db)?\// { next }
  inside { sub(/,$/, ""); printf "%s%s", (kept ? ",\n" : ""), $0; kept = 1; next }
  { print }
' "$KJ" > "$WORK/kama.json"
mv "$WORK/kama.json" "$KJ"
echo "vendor-sdl: SDL $SDL_VERSION, $(wc -l < "$WORK/sources" | tr -d ' ') sources and $(wc -l < "$WORK/closure" | tr -d ' ') files; GameInput $GAMEINPUT_VERSION's header"
