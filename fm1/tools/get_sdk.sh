#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Download the three files the build takes from JieLi's AC79 SDK (Apache-2.0), at the tested version
# (AC79NN_SDK_V1.2.1_2023-12-13, the SHA-256s in tools/build.py), into DEST/cpu/wl82/tools/.
#   tools/get_sdk.sh [DEST]            (default: ~/ac79-sdk-min)
set -e
DEST="${1:-$HOME/ac79-sdk-min}"
BASE=https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK/raw/AC79NN_SDK_V1.2.1_2023-12-13/cpu/wl82/tools
mkdir -p "$DEST/cpu/wl82/tools/cfg"
cd "$DEST/cpu/wl82/tools"
for f in uboot.boot cfg_tool.bin cfg/eq_cfg_hw.bin; do
    curl -fsSL --retry 3 "$BASE/$f" -o "$f"
done
shasum -a 256 -c <<'EOF'
4e3b4c220dc96641cb5a723f41e68ce41d5261ae9434bb33fbd7f2c59976ded4  uboot.boot
276579954f076886a6a7694f65dc71c034a63a2c204b76749065c0ac7b010d1b  cfg_tool.bin
41167491bffed4651750719c973d2758adeb9021a5670d02d6a53c85ed80ea7d  cfg/eq_cfg_hw.bin
EOF
echo "AC79_SDK=$DEST"
