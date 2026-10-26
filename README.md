# Quod

*Quod est superius est sicut quod inferius*: that which is above is like that which is below (the Emerald Tablet).

Quod is a drone instrument built on that line. You play one note, the **root**, and a cluster of voices
gathers around it: above the root they are **angels**, below it **demons**, and the cluster is a mirror between
**heaven** and **hell**. Playing another note doesn't start a new sound; the whole cluster *flows* there, each voice
a weight moving through syrup. You don't play notes on it so much as steer it.

It runs on the M-VAVE FM-1 (a small keyboard with seven encoders and a 240 x 240 screen, which it was made for) and on
a Mac as an Audio Unit instrument with its own app.

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

To get back: hold OCT- while switching on (USB rescue), then install any firmware. More in `fm1/README.md`.

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
- **Up to eight instances** at once, each its own instrument. The controls are automatable parameters; the project
  keeps the controls, steps, notes and locks.
- **`apple/Tools/quod_film.m`** films the Audio Unit offline (its sound at a host's rate and its panel at 30 fps),
  pressing SHOWCASE at 5 s. Its build line is at its top.

## Releases

`tools/release.sh X.Y` builds a release on a Mac and drafts it on GitHub: the FM-1 firmware (`quod-X.Y.fwsc`) and
the app with its Audio Unit, signed with a Developer ID and notarised by Apple. The credentials stay in the Mac's
keychain; the script's head lists what it needs once. The draft is published from its GitHub page.

## Running it on something else

Quod is integer C in one translation unit, with no dependencies. A host defines what `src/quod_port.h` lists (time,
keys and knobs, MIDI, settings, a screen to blit strips to), then includes `src/quod.c` with `src/` and `gen/` on the
include path. Quod gives back `app_audio` (44.1 kHz, stereo), `app_tick_ms` (the physics, once a millisecond), and
`quod_init` / `quod_step` (the main loop). `apple/Engine` is a complete example.

| Path | What |
| --- | --- |
| `src/quod.c` | the instrument: sound, physics, sequencer, pages, screen |
| `src/quod_port.h` | what a host provides |
| `src/qlib.h`, `src/qgfx.h` | sine, pitch, text; the strip canvas, lines, rectangles, text |
| `gen/quod_font.h`, `tools/gen_font.py` | the text font, from `art/fonts/ter-u16n.bdf` |
| `gen/quod_splash.h`, `tools/gen_splash.py` | the splash and the page titles: the hands from a scan of the engraving (not included), the titles from `art/fonts` (Pillow) |
| `apple/` | the Audio Unit and its app (`Engine/`: the port and the resampler; `Extension/`: the AU and its panel) |
| `fm1/` | the FM-1 firmware: the platform, the host (`firmware/src/quod.c`), the build and the installer (GPL) |

## Licence

MIT, see `LICENSE`, except `fm1/`: the FM-1 firmware's platform and host are GPL-3.0-only (`fm1/LICENSE`,
`fm1/LICENSING.md`), and a firmware built from them with Quod is a whole under the GPL. Quod's own sources stay MIT;
nothing outside `fm1/` includes anything in it. The fonts in `art/fonts` (SIL Open Font License), the engraving the
splash's hands come from (public domain), and what is generated from them in `gen/`, keep their own terms: see
`THIRD-PARTY-NOTICES.md`. The app carries these in its bundle (`Contents/Resources/Licenses`), and each release has
them in `Quod-X.Y-licenses.zip`.
