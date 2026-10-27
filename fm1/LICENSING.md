# Licensing

This folder is free software under the GNU General Public License, version 3 only (`GPL-3.0-only`, full text in
`LICENSE`). You may use, study, change and share it. If you give someone a firmware built from it, give them its
complete source under the same licence too: this repository is that source.

The rest of the repository (Quod itself: `../src`, `../gen`, `../engine`, `../crates`, `../tools`, `../apple`,
`../web`) is under the MIT licence (`../LICENSE`), except the browser's [ FLASH ], `../web/flash.js`, which runs on
`web/` here and is GPL-3.0-only with it. The firmware built from both is a whole under the GPL; Quod's own sources
stay MIT. Nothing else outside this folder includes anything in it.

## Who made what

| | |
| --- | --- |
| The platform: `firmware/hal/`, `firmware/loader/`, `crt0.S`, `app.ld`, `firmware/src/` (`usb.c`, `ota.c`, `recovery.c`, `lcd.c`, `gfx.c`, `libc.c`, `bootguard.h`, `uac_fir.h`), `tools/`, `web/fm1ota.js`, `web/fm1pkg.js` (unchanged from Felucca's `web/`) | Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments ([Felucca](https://github.com/hugelton/Felucca)), with the changes of [SLOOP](https://github.com/isod89/sloop-fm1) (isod89) |
| The wiring `platform.c`, the host mode `tests/host.c`, `tests/run_host.sh`, the docs | zvenson ([Dinghy](https://github.com/zvenson/dinghy)) |
| The Quod host `firmware/src/quod.c` (with its state backup), `web/quodflash.js`, the test scripts `tests/quod*.txt`, `tools/get_sdk.sh`, and the changes to the platform for Quod and khronic (the app's SysEx hook in `ota.c`; `--backup`, `--restore` in `tools/fm1_install.py`) | Kip DeCastro |

## Third-party material

| What | Licence | Where |
| --- | --- | --- |
| Terminus font 8x16 (ter-u16n), rendered into `firmware/gen/felucca_font.h` | SIL OFL 1.1 | `../art/fonts/Terminus-OFL.txt` |
| JieLi AC79 SDK: `uboot.boot`, `cfg_tool.bin`, `eq_cfg_hw.bin` are read from your SDK checkout at build time (`tools/get_sdk.sh`) and placed in the package; no SDK files are in this tree | Apache-2.0 | <https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK> |

## Trademarks

"Felucca" and "Hügelton Instruments" are names of Hügelton Instruments; "M-VAVE" and "FM-1" belong to their
owners. This firmware is not made or endorsed by them.
