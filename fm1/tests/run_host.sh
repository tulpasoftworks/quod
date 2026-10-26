#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Run Quod's firmware (firmware/src/quod.c) on this computer (tests/host.c): build/host/quod.wav and the screens as PNG.
#   tests/run_host.sh [SCRIPT]   (SCRIPT: tests/*.txt, see host.c; default: 2.6 s of the app untouched)
# On macOS the compile runs in a Linux container (the platform uses ELF section names).
set -e
cd "$(dirname "$0")/.."
mkdir -p build/host
# Quod itself (MIT) is the rest of this repository, copied side by side (Docker sees only this folder).
rm -rf firmware/shared && mkdir -p firmware/shared/quod && cp ../src/* ../gen/* firmware/shared/quod/
APP=quod
NAME=$APP
[ -n "$1" ] && NAME=$(basename "$1" .txt)
rm -f build/host/$NAME-*.png
CMD="cc -O2 -Wall -Wno-unused-function -DDINGHY_HOST -DAPP_VER='\"${APP_VER:-dev}\"' -Ifirmware/src -Ifirmware/gen -Ifirmware/shared -o build/host/$APP firmware/src/$APP.c && build/host/$APP build/host/$NAME $1"
if [ "$(uname -s)" = Linux ]; then sh -c "$CMD"; else docker run --rm -v "$PWD":/w -w /w gcc:14 sh -c "$CMD"; fi
PY="${PYTHON:-.venv/bin/python}"
[ -x "$PY" ] || PY=python3
"$PY" - "$NAME" <<'PY'
import sys
from pathlib import Path
from PIL import Image
for p in sorted(Path("build/host").glob(sys.argv[1] + "-*.ppm")):
    Image.open(p).resize((480, 480), Image.NEAREST).save(p.with_suffix(".png"))
    p.unlink()
    print("screen ->", p.with_suffix(".png"))
PY
