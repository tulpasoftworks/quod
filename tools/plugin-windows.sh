#!/bin/sh
# SPDX-License-Identifier: MIT
# The plug-in for Windows, built on this Mac: tools/plugin-windows.sh [VERSION]  -> target/Quod-VERSION-Windows.zip
# (Quod.vst3 and Quod.clap, 64-bit, the C runtime linked in; with the licences: the VST3 is a whole under the GPL-3.0,
# see crates/quod-plugin/Cargo.toml).
# Needs, once: cargo install cargo-xwin (it fetches Microsoft's C runtime and Windows SDK libraries, under Microsoft's
# licence), rustup target add x86_64-pc-windows-msvc, and LLVM's clang-cl and lld-link (on a Mac: brew install llvm).
set -e
cd "$(dirname "$0")/.."
V="${1:-dev}"
[ -d /opt/homebrew/opt/llvm/bin ] && PATH="/opt/homebrew/opt/llvm/bin:$PATH"
cargo xwin build -q --release -p quod-plugin --target x86_64-pc-windows-msvc
DLL=target/x86_64-pc-windows-msvc/release/quod_plugin.dll
OUT=target/Quod-$V-Windows
rm -rf "$OUT" "$OUT.zip" && mkdir -p "$OUT/Quod.vst3/Contents/x86_64-win"
cp "$DLL" "$OUT/Quod.vst3/Contents/x86_64-win/Quod.vst3"     # (the VST3 bundle layout)
cp "$DLL" "$OUT/Quod.clap"
cp LICENSE "$OUT/LICENSE-MIT.txt"
cp fm1/LICENSE "$OUT/LICENSE-GPL-3.0.txt"
(cd target && zip -qr "Quod-$V-Windows.zip" "Quod-$V-Windows")
echo "plug-in for Windows: $OUT.zip ($(du -h "$OUT.zip" | cut -f1))"
