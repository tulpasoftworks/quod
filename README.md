# Quod

*Quod est superius est sicut quod inferius*: that which is above is like that which is below (the Emerald Tablet).

Quod is a drone instrument built on that line. You play one note, the **root**, and a cluster of voices
gathers around it: above the root they are **angels**, below it **demons**, and the cluster is a mirror between
**heaven** and **hell**. Playing another note doesn't start a new sound; the whole cluster *flows* there, each voice
a weight moving through syrup. You don't play notes on it so much as steer it.

It runs on the M-VAVE FM-1 (a small keyboard with seven encoders and a 240 x 240 screen, which it was made for), on
a Mac as an Audio Unit instrument with its own app, as a VST3 and CLAP plug-in (Windows), and in a browser, which can
also install it on an FM-1.

## How it works

### The cluster

Thirteen sine voices: the root, and twelve voices on a **ladder of 24 just intervals**. Above the root the ladder is
the overtone series (5/4, 4/3, 3/2, 5/3, 2/1, 5/2, 3/1, 7/2, 4/1, 5/1, 6/1, 8/1); below, the same ratios mirrored
into undertones (4/5 down to 1/8). The twelve voices always stand on twelve neighbouring rungs.

- **Tilt** slides that window of twelve along the ladder: all below the root (a choir of demons), half and half, or all
  above (all angels). The voices glide to their new rungs, crossing the root as they go.
- **Spread** scales every interval, from a tight knot (a quarter of the ladder's width) to wider than it (1.5x), and
  widens the stereo image with it.

A voice is an angel or a demon by its live pitch, not by which voice it is: above the root an angel, below a demon,
blending across the four semitones either side of the root, so a voice gliding through it changes gradually. Heaven
and hell then pull on that: heaven redeems the voices below the root, and hell makes the ones above it scream (below).

### Heaven and hell

One knob, **Heaven / Hell**, runs from full hell (left) through plain (middle) to full heaven (right).

Toward **heaven**, the angels sing:

- **the choir**: each angel's sine bends its own phase, a "throat" that grows harmonics, and two vowel formants
  shape them from "ooh" to "aah" as heaven brightens; each sings with its own vibrato;
- **the bell**: a church bell two octaves above the root, never struck, always ringing, louder the higher heaven goes
  (its hum, prime, minor-third tierce, quint, nominal and two partials above, each swaying slowly side to side like a
  real bell's doublets);
- **the shimmer**: a reverb whose tail is shifted up an octave and fed back into itself, so it climbs, the more the
  higher heaven goes. Hell's voices ring in the reverb too; **Shimmer** (on the FX page) sets its level (0 dry, up to
  6x), and above 50 lengthens its tail and makes it climb by itself, heaven or hell;
- **redemption**: as heaven rises, the demons below the root are redeemed and become angels too.

Toward **hell**, the demons take over:

- the deep voices are pulled out of tune toward dissonance (up to a minor second), the deeper the more;
- **the drive**: hell's voices go through a saturating drive;
- **the screamers**: four voices of hell's own, high around E6 a quarter-tone apart (the range of a human scream and
  of *Psycho*'s violins), wailing in quick slides and now and then cracking: a jump, sidebands, a voice breaking
  to its subharmonic;
- voices above the root, the would-be angels, are dragged into hell's bus and scream too.

### The syrup

Every voice is a mass on a spring, simulated a thousand times a second. A new root moves the springs' anchors; the
voices follow.

- **Viscosity**: how thick the syrup is, from water (a glide of a fiftieth of a second) to cold honey (about two
  minutes). Turning it or Slosh, the screen shows how long the root will take to arrive.
- **Slosh**: the damping. Low, and the voices overshoot and swing before they settle; high, and they ease in.
- **Mass and gravity**: angels are light and quick (down to 0.6 of the root's mass, the highest), demons heavy (up to
  three times the root's, the deepest), and the deep ones climb slowly and fall quickly.
- **Coupling**: each voice pulls on its neighbours, so the cluster moves as a body.
- **Settle** (held): the syrup thickens over a few seconds, calming the drone without stopping it.

(The design also has **agitation**: stirring the drone with quick note changes would drag it toward hell, and left
alone it would purify. It's switched off for now.)

### The gate

**Gate** is a Buchla low-pass gate on the whole cluster: a vactrol whose light opens a VCA and a low-pass filter
together. Every new root (a key, a sequencer step with a trig, a MIDI note) strikes it: it opens in about a
millisecond and closes the way a vactrol does, quickly while bright (~25 ms) and then a dim tail (~0.2 s), the
filter darkening ahead of the level. Gate sets where it rests: at 0 it rests shut, so every note is a pluck; at 100
it rests open, the drone, untouched; between, a dimmer, darker drone that each note plucks brighter. It comes before
the shimmer and the delay, so a pluck rings out in them.

### Twins and drift

- Every voice has a **twin** a fraction of a hertz to a few hertz away, each pair beating at its own slowly wandering
  rate (about 0.3 to 4 beats a second at the default), Lyra-8 style.
- Every voice is also detuned a little from its rung, never in step with the others: slow beats among the angels,
  roughness among the deep demons.
- And each wanders out of tune on its own, gliding to a new pitch every few seconds. Demons stagger, angels hold
  steadier.
- **Drift** turns all three: from 0 to 30 it fades the twins in, and from there speeds their beating up (0.8 times
  the default's at 30, 4 times at 100); and all along it scales the detune and the wander: none at 0 (pure, still),
  a few cents, about what an analog Lyra-8 strays (to 80), then quickly wilder, to about five semitones deep (100).

### The sequencer

Sixteen steps, and each step is a **root**, not a note: when a step plays, the cluster flows to it through the syrup.

- **Steps** turns the keyboard's white keys into the sixteen steps. Tap one to switch it on or off; hold one and play a
  key to set its root; hold one and turn a knob for a **lock**, that knob's value while the step plays.
- Step length runs from a bar to a sixteenth, or **WAIT**: the next step comes when the drone has settled (as settled
  as **Settled** asks). Then the music moves at the speed of the syrup.
- **Rec** records the keys you play onto the step that's playing.

### The screen

- **Home** is the cosmos: a planetoid where every voice is a point of light circling a sphere, trailing light. Its
  latitude is its pitch: the root on the equator, higher voices further north, lower ones further south. Its colour is
  its nature: white for an angel, red for a demon, so heaven turns the sphere white and hell turns it red. Each
  voice's twin circles beside it, as bright as the twins are deep; the screamers are embers high in the north and
  the bell a crown above them. The sphere spins at the twins' speed, each voice at its ratio to the root, so simple
  ratios turn in step. Its size is the spread.
- A **new root** makes a new sphere, and it swallows the old one like a black hole: the lights sink, stream off in a
  spiral, and rise to their places on the new one.
- The knobs are **planets**, drawn as dials at the top, laid out as the FM-1's knobs are: Saturn the viscosity,
  Mercury the slosh, the Sun and Mars heaven and hell, the Moon the tilt, Venus the drift, Jupiter the spread (and
  the gate, the one that isn't). The screen's colour follows heaven and hell, from red to white.
- Other pages are **tarot cards**: the FX page is *XIV Temperance*, with alchemical signs on its dials.
- At power-on, the hands of Lévi's Baphomet (one raised, one lowered: as above, so below) over a whirlpool of
  planetary and alchemical signs.

### The showcase

A 95-second tour of all of the above, from the splash through heaven, a new root, the spread, the tilt, hell and
the drift. On the FM-1, hold HOME + REC for a second; on the Mac, click SHOWCASE. Any key, button or knob stops it,
and everything returns to how you left it.

## Controls

| Control (FM-1 / Mac) | What it does |
| --- | --- |
| keys | the root (the last key played); OCT - / OCT + move the keyboard by octaves |
| SELECT | viscosity |
| ALGORITHM | slosh |
| KNOB 1 | heaven / hell |
| KNOB 2 | tilt |
| KNOB 3 | gate (0 a pluck a note .. 100 the drone) |
| KNOB 4 | drift (the twins, and the voices wandering out of tune) |
| PRESETS | spread |
| SEL / SHIFT, held | the knobs become tempo, step length, settled, delay, pattern length |
| FX | the FX page: delay level, time and feedback, shimmer, drive (tap to stay, hold to peek) |
| EDIT / SETTLE, held | settle |
| SEQ / STEPS | step mode |
| PLAY, REC | start / stop the sequencer; record |
| SAVE | keep everything |
| HOME | back to the cosmos |
| HOME + REC held 1 s / SHOWCASE | the showcase |
| GLO, held (the FM-1) | the firmware's version on the status row: QUOD 0.2 |
| MIDI | notes set the root; CC 20..26 turn the seven knobs |

The knobs accelerate, as on an Elektron: turned slowly, a detent is one step; turned quickly, up to three, so about
forty quick detents cross a whole range. Step length and pattern length always go one at a time.

## The FM-1 firmware

`fm1/` is the firmware that runs Quod on the FM-1: Quod inside the FM-1 platform of
[Dinghy](https://github.com/zvenson/dinghy), [Felucca](https://github.com/hugelton/Felucca) and
[SLOOP](https://github.com/isod89/sloop-fm1). It builds with JieLi's toolchain (Linux x86-64, or Docker elsewhere)
and installs over USB-MIDI:

    cd fm1
    tools/get_toolchain.sh && tools/get_sdk.sh  # once
    ./build.sh                                  # -> build/quod.fwsc
    python3 tools/fm1_install.py build/quod.fwsc

Or from the browser's Quod, without Python: [ FLASH ] (below). To get back: hold OCT- while switching on (USB
rescue), then install any firmware. More in `fm1/README.md`.

The FM-1 knows its firmware by an identity: Quod's is `FM-1_Q` and its version X.Y as two digits (`FM-1_Q02` is
Quod 0.2; `FM-1_Q00` a development build), so it can't be taken for Felucca's or Dinghy's (`FM-1_9XY`). Felucca's
web installer and M-VAVE's updater read only digits there: to leave Quod with them, start the FM-1 in its USB rescue
first (it is `FM-1_000` there). Whether M-VAVE's own firmware installs an `FM-1_Q` package hasn't been tried yet.

Quod's state (the controls, the steps, their notes and locks) goes to a file and back over the same USB-MIDI:
`fm1_install.py --backup FILE.quodstate`, `--restore FILE.quodstate`. A `.quodstate` file is the same on the FM-1,
in the browser, in the Mac app (File > Open / Save State) and in the plug-in.

## The Audio Unit (macOS 13 or later)

`apple/` is an app carrying an Audio Unit v3 instrument (type `aumu`, subtype `Quod`, manufacturer `Tlpa`: Tulpa
SoftWorks), on Apple's frameworks only. The app plays Quod on its own, from every MIDI source to the speakers; opening
it once registers the Audio Unit for Logic, GarageBand and other hosts.

    cd apple
    xcodegen                                    # (only after changing project.yml; Quod.xcodeproj is committed)
    xcodebuild -scheme Quod -configuration Release -derivedDataPath build build
    ditto build/Build/Products/Release/Quod.app ~/Applications/Quod.app && open ~/Applications/Quod.app
    auval -v aumu Quod Tlpa

Signing: the build signs with an Apple Development certificate of the team in `apple/Local.xcconfig`, which isn't
committed; make it with one line, `DEVELOPMENT_TEAM = <your team ID>` (or sign to run locally). A release outside the
App Store wants a Developer ID certificate and notarisation.

- **The panel** is the FM-1's screen, encoders and keys, with the buttons Quod uses, named for what they do. The
  encoders are labelled with what each turns now. Drag an encoder up or down, or scroll on it; Shift-click latches a
  button.
- **The sound** is computed at 44.1 kHz, a millisecond at a time as on the FM-1, and resampled to the host's rate.
- **The app's menus**: About Quod (its version), and File > Open / Save State for a `.quodstate` file.
- **Up to eight instances** at once, each its own instrument. The controls are automatable parameters; the project
  keeps the controls, steps, notes and locks.
- **`apple/Tools/quod_film.m`** films the Audio Unit offline (its sound at a host's rate and its panel at 30 fps),
  pressing SHOWCASE at 5 s. Its build line is at its top.

## In a browser

`web/` is Quod in a browser: the engine (`engine/`, with Quod in it) compiled to WebAssembly by a Rust crate
(`crates/quod-wasm`) and played in an AudioWorklet at 44.1 kHz, sample for sample the native build's
(`web/test/parity.mjs` checks it), around the FM-1's panel. The computer's keys and any MIDI input play it.

    rustup target add wasm32-unknown-unknown   # once; on a Mac also brew install llvm (Apple's clang has no WebAssembly)
    web/build.sh                               # -> web/dist, with fm1/build/quod.fwsc if it is there
    python3 -m http.server -d web/dist

The panel is the Mac app's, with a volume knob in the encoders' empty top-left place (drag it, or click it to mute).
SAVE keeps everything in this browser; the page shows its version under [ source ].

Both of these need Web MIDI with SysEx (Chrome or Edge), and both open over the sim's screen, its sound faded out and
the page dimmed:

- **[ FLASH ]** (Install Quod on FM-1) installs Quod on an FM-1, one step at a time, on Felucca's updater (`fm1/web`,
  GPL). The FM-1 can't send its firmware back over USB-MIDI, so before it installs anything it wants a way back: on
  an FM-1 running Quod, Quod's state saved to a file; on M-VAVE's firmware, M-VAVE's own package for the version it
  runs (V15 or V16, checked against its SHA-256 in the page, never uploaded); on any other, that package tested on the
  FM-1 itself (its own checks, stopped before anything is written). Then it checks the FM-1 comes back as Quod. It
  installs the official firmware back the same way.
- **[ LINK ]** sends Quod's settings (a `.quodstate`) from the sim to an FM-1 running Quod, or from the FM-1 to the
  sim, drawn as a trade in a small 8-bit window (`web/trade.js`, its two machines as pixel art), with an 8-bit song
  while it runs (`web/song.js`).

Every release carries this site with its firmware as `quod-site.zip`, ready for any static host; the latest is at
https://github.com/tulpasoftworks/quod/releases/latest/download/quod-site.zip. CI builds the same for every push
(the `quod-site` artifact).

## The plug-in (VST3, CLAP)

`crates/quod-plugin` is Quod as a VST3 and CLAP instrument, on [nih-plug](https://github.com/robbert-vdh/nih-plug):
as the Audio Unit is, eight instances at once, resampled to the host's rate, the controls automatable, the project
keeping the controls, steps, notes and locks, and the FM-1's panel (its version in the corner). CI builds it for
Windows on every push and runs it through pluginval (the VST3) and clap-validator (the CLAP) there; it hasn't been
played in a Windows host yet.

Installing it on Windows: unzip `Quod-X.Y-Windows.zip` (from a release), copy the `Quod.vst3` folder into
`C:\Program Files\Common Files\VST3` (Windows asks for an administrator there), and rescan the plug-ins in the
host: Quod is an instrument by Tulpa SoftWorks. For a CLAP host, `Quod.clap` goes in
`C:\Program Files\Common Files\CLAP`. The builds aren't signed; a host loads them all the same.

Building it:

    cargo xtask bundle quod-plugin --release   # -> target/bundled/Quod.vst3, Quod.clap (for this computer)
    tools/plugin-windows.sh 0.2                # -> target/Quod-0.2-Windows.zip, cross-built on a Mac (cargo-xwin)

On Windows itself it builds with `CC=clang-cl` (MSVC's own cl has no `__atomic` builtins). The CLAP passes
clap-validator but for its state tests, which fail in nih-plug's CLAP wrapper (it loads a state without asking the
host to rescan the parameters); the VST3 has its own wrapper.

## Releases

`tools/release.sh X.Y` builds a release on a Mac and drafts it on GitHub: the FM-1 firmware (`quod-X.Y.fwsc`), the
app with its Audio Unit, signed with a Developer ID and notarised by Apple, the plug-in for Windows
(`Quod-X.Y-Windows.zip`, cross-built with `tools/plugin-windows.sh`), and the browser's Quod with that firmware
(`quod-site.zip`). The credentials stay in the Mac's
keychain; the script's head lists what it needs once. The draft is published from its GitHub page.

## Running it on something else

Quod is integer C in one translation unit, with no dependencies. A host defines what `src/quod_port.h` lists (time,
keys and knobs, MIDI, settings, a screen to blit strips to), then includes `src/quod.c` with `src/` and `gen/` on the
include path. Quod gives back `app_audio` (44.1 kHz, stereo), `app_tick_ms` (the physics, once a millisecond), and
`quod_init` / `quod_step` (the main loop). `engine/` is a complete example, and the Audio Unit's, the browser's and
the plug-in's engine: Quod compiled once per instrument, a millisecond at a time, with a resampler and a backup
format (`engine/quod_engine.h`); `crates/quod-core` is it for Rust.

| Path | What |
| --- | --- |
| `src/quod.c` | the instrument: sound, physics, sequencer, pages, screen |
| `src/quod_port.h` | what a host provides |
| `src/qlib.h`, `src/qgfx.h` | sine, pitch, text; the strip canvas, lines, rectangles, text |
| `gen/quod_font.h`, `tools/gen_font.py` | the text font, from `art/fonts/ter-u16n.bdf` |
| `gen/quod_splash.h`, `tools/gen_splash.py` | the splash and the page titles: the hands from a scan of the engraving (not included), the titles from `art/fonts` (Pillow) |
| `engine/` | the engine the Audio Unit, the browser and the plug-in share: the port, the slots, the resampler, the clock |
| `apple/` | the Audio Unit and its app (`Extension/`: the AU and its panel) |
| `crates/` | the engine for Rust (`quod-core`), in the browser (`quod-wasm`), as a plug-in (`quod-plugin`) |
| `web/` | the browser's Quod: the panel, the worklet, [ FLASH ] (`flash.js`, GPL) |
| `fm1/` | the FM-1 firmware: the platform, the host (`firmware/src/quod.c`), the build and the installers (`tools/`, `web/`) (GPL) |

## Licence

MIT, see `LICENSE`, except `fm1/`: the FM-1 firmware's platform and host are GPL-3.0-only (`fm1/LICENSE`,
`fm1/LICENSING.md`), and a firmware built from them with Quod is a whole under the GPL; and `web/flash.js`, the
browser's [ FLASH ], GPL-3.0-only as the updater it runs on (`fm1/web`, Felucca's). A VST3 built from
`crates/quod-plugin` links nih-plug's VST3 bindings (GPL-3.0), so it too is a whole under the GPL; the CLAP is not.
Quod's own sources stay MIT; nothing else outside `fm1/` includes anything in it. The fonts in `art/fonts` (SIL Open Font License), the engraving the
splash's hands come from (public domain), and what is generated from them in `gen/`, keep their own terms: see
`THIRD-PARTY-NOTICES.md`. The app carries these in its bundle (`Contents/Resources/Licenses`), and each release has
them in `Quod-X.Y-licenses.zip`.
