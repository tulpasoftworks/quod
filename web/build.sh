#!/bin/sh
# SPDX-License-Identifier: MIT
# The browser's Quod (the sim and [ FLASH ]) as a static site: web/build.sh [OUT] [PACKAGE.fwsc VERSION]
#   OUT       default web/dist (serve it: python3 -m http.server -d web/dist)
#   PACKAGE   the Quod firmware [ FLASH ] installs (default fm1/build/quod.fwsc, as "dev", if it is there)
# Needs Rust's wasm32-unknown-unknown target (rustup target add wasm32-unknown-unknown) and a clang that has
# WebAssembly (on a Mac: brew install llvm; crates/quod-core/build.rs finds it).
set -e
cd "$(dirname "$0")/.."
OUT="${1:-web/dist}"
PKG="${2:-fm1/build/quod.fwsc}"
VER="${3:-dev}"
cargo build -q -p quod-wasm --target wasm32-unknown-unknown --release
rm -rf "$OUT" && mkdir -p "$OUT/fm1" "$OUT/fonts" "$OUT/licences"
cp web/index.html web/app.js web/worklet.js web/flash.js web/link.js web/trade.js web/stage.js web/song.js "$OUT/"
cp target/wasm32-unknown-unknown/release/quod_wasm.wasm "$OUT/quod.wasm"
cp fm1/web/fm1ota.js fm1/web/fm1pkg.js fm1/web/quodflash.js "$OUT/fm1/"
cp art/fonts/UnifrakturMaguntia-Book.ttf "$OUT/fonts/"
cp LICENSE "$OUT/licences/LICENSE-MIT.txt"
cp fm1/LICENSE "$OUT/licences/LICENSE-GPL-3.0.txt"
cp THIRD-PARTY-NOTICES.md art/fonts/UnifrakturMaguntia-OFL.txt art/fonts/Terminus-OFL.txt "$OUT/licences/"
# the version, as the FM-1 shows it while GLO is held: X.Y, or DEV for a development build
perl -pi -e 's/__QUOD_VERSION__/'"$(echo "$VER" | tr a-z A-Z)"'/g' "$OUT/index.html"
# every script and the module, by this build's own name (?v=): a browser that has an older one cached fetches this
STAMP=$(cat "$OUT"/*.js "$OUT"/fm1/*.js "$OUT"/quod.wasm "$OUT"/index.html | shasum | cut -c1-10)
perl -pi -e 's/((?:from |import\()"\.\/[^"?]+\.js)"/$1?v='"$STAMP"'"/g; s/"(worklet\.js|quod\.wasm)"/"$1?v='"$STAMP"'"/g' \
    "$OUT"/index.html "$OUT"/*.js "$OUT"/fm1/*.js
if [ -f "$PKG" ]; then
    mkdir -p "$OUT/firmware"
    NAME="quod-$VER.fwsc"
    cp "$PKG" "$OUT/firmware/$NAME"
    printf '{"version": "%s", "file": "%s"}\n' "$VER" "$NAME" > "$OUT/firmware/quod.json"
fi
echo "web: $OUT ($(du -sh "$OUT" | cut -f1))"
