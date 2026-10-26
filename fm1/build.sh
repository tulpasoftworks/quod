#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
# Build Quod for the FM-1 (on Dinghy's platform; see README.md).
#   ./build.sh [--release X.Y]
#   JIELI_TOOLCHAIN  JieLi Linux toolchain (default: ~/.jieli/toolchain; tools/get_toolchain.sh)
#   AC79_SDK         the JieLi AC79 SDK's three files (default: ~/ac79-sdk-min; tools/get_sdk.sh)
set -e
cd "$(dirname "$0")"
export JIELI_TOOLCHAIN="${JIELI_TOOLCHAIN:-$HOME/.jieli/toolchain}"
export AC79_SDK="${AC79_SDK:-$HOME/ac79-sdk-min}"
PY="${PYTHON:-.venv/bin/python}"
[ -x "$PY" ] || PY=python3

"$PY" -c 'import PIL' 2>/dev/null || { echo "build.sh: $PY has no Pillow (pip3 install Pillow)"; exit 1; }
[ -x "$JIELI_TOOLCHAIN/pi32v2/bin/clang" ] || { echo "build.sh: no toolchain in $JIELI_TOOLCHAIN (run tools/get_toolchain.sh)"; exit 1; }
[ -f "$AC79_SDK/cpu/wl82/tools/uboot.boot" ] || { echo "build.sh: no JieLi AC79 SDK in $AC79_SDK (run tools/get_sdk.sh)"; exit 1; }
if [ "$(uname -s)" != Linux ] || [ "$(uname -m)" != x86_64 ]; then
    docker info >/dev/null 2>&1 || { echo "build.sh: Docker is not running"; exit 1; }
fi
# Quod itself (MIT) is the rest of this repository: its sources and generated headers, copied side by side
# (Docker sees only this folder).
rm -rf firmware/shared && mkdir -p firmware/shared/quod && cp ../src/* ../gen/* firmware/shared/quod/
exec "$PY" tools/build.py "$@"
