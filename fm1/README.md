# Quod on the M-VAVE FM-1

The firmware that runs Quod on the FM-1: Quod itself (`../src`, `../gen`, MIT) inside the FM-1's platform
(this folder, GPL-3.0-only). What Quod does and how to play it is in the [top README](../README.md).

Built on [Dinghy](https://github.com/zvenson/dinghy) (zvenson), which carries the
[Felucca](https://github.com/hugelton/Felucca) platform (Leo Kuroshita, Hügelton Instruments) as
[SLOOP](https://github.com/isod89/sloop-fm1) (isod89) has it: updates, USB rescue, USB-MIDI, USB audio, panel,
screen, flash. GPL-3.0, as they are: see `LICENSE` and `LICENSING.md`.

Package identity `FM-1_Q` and the version, X.Y as two digits (`FM-1_Q02` is Quod 0.2; `FM-1_Q00` a development
build); USB `1209:0003`; MIDI port name "Felucca".

## Build, try, install

```sh
tools/get_toolchain.sh                        # JieLi's Linux toolchain -> ~/.jieli/toolchain
tools/get_sdk.sh                              # the three AC79 SDK files the package needs -> ~/ac79-sdk-min
./build.sh                                    # -> build/quod.fwsc
tests/run_host.sh tests/quod.txt              # the same code on a computer: build/host/quod.wav + screen PNGs
python3 tools/fm1_install.py build/quod.fwsc  # install over USB-MIDI
python3 tools/fm1_install.py --backup quod.quodstate   # Quod's state to a file (--restore: back)
```

Or from a browser: [ FLASH ] in the browser's Quod (`../web`), on Felucca's Web MIDI updater (`web/fm1ota.js`,
`web/fm1pkg.js`, unchanged) and `web/quodflash.js`.

Needs Python 3 with Pillow (and mido and python-rtmidi to install); `.venv/bin/python` is used if there is one.
The toolchain is Linux x86-64 only: on Linux x86-64 the build runs it directly, elsewhere in Docker (on a Mac:
Docker Desktop, or colima with `colima start --vm-type vz --vz-rosetta`). `tests/run_host.sh` compiles in Docker
too on a Mac (`gcc:14`). `./build.sh --release X.Y` builds `build/quod-X.Y.fwsc` with that version.

## Getting back

- **USB rescue**: hold **OCT-** while switching on; then install any build (or another firmware).
- **Stock firmware**: M-VAVE's updater, or the [FM-1 Firmware Hub](https://thefirmwarehub.com/).

## Where things are

| | |
| --- | --- |
| `firmware/src/quod.c` | the host: what Quod's `src/quod_port.h` asks for, from the platform |
| `firmware/src/platform.c` | Dinghy's platform (with khronic's changes: a song area in flash, the 1 ms tick hook, the USB PID) |
| `firmware/hal/`, `firmware/loader/` | the chip, and the update loader |
| `tools/build.py` | the build: compiles, checks the chip's rules, packages |
| `tools/fm1_install.py` | installs a package over USB-MIDI (`--info` shows what the FM-1 runs) |
| `web/` | the same from a browser: Felucca's updater, and Quod's install and state backup on it |
| `tests/host.c`, `tests/quod*.txt` | a fake FM-1 on a computer, and scripts that play Quod on it |
| `docs/` | Dinghy's API and the chip's rules (written for Dinghy's own app, `dinghy.c`) |
