# Third-party notices

Quod's own code is under the MIT licence (`LICENSE`). These materials in this repository are not, and keep their own
terms.

## The FM-1 firmware

- Files: everything in `fm1/`: the FM-1 platform of Dinghy (zvenson), Felucca (Leo Kuroshita, Hügelton Instruments)
  and SLOOP (isod89), and Quod's host on it.
- Licence: GNU General Public License, version 3 only, full text in `fm1/LICENSE`; who made what in
  `fm1/LICENSING.md`.

## Terminus Font

- Files: `art/fonts/ter-u16n.bdf` (from terminus-font 4.49.1); its 8 x 16 glyphs, ASCII 32..126, converted by
  `tools/gen_font.py` into `gen/quod_font.h`. The FM-1 platform's own font, `fm1/firmware/gen/felucca_font.h`, is
  rendered from the same font.
- Copyright (C) 2020 Dimitar Toshkov Zhekov, with Reserved Font Name "Terminus Font".
- Licence: SIL Open Font License 1.1, full text in `art/fonts/Terminus-OFL.txt`.
- Upstream: <https://terminus-font.sourceforge.net/>

## UnifrakturMaguntia

- Files: `art/fonts/UnifrakturMaguntia-Book.ttf`; the splash's title and the pages' titles rendered from it by
  `tools/gen_splash.py` into `gen/quod_splash.h`.
- Copyright (c) 2010, j. 'mach' wust, with Reserved Font Name UnifrakturMaguntia. Copyright (c) 2009, Peter Wiegel.
- Licence: SIL Open Font License 1.1, full text in `art/fonts/UnifrakturMaguntia-OFL.txt`.

## Baphomet, after Eliphas Lévi

- Files: `gen/quod_splash.h`, the two hands cut from a scan of the engraving by `tools/gen_splash.py` (the scan
  itself is not in this repository).
- The engraving is from Eliphas Lévi, *Dogme et Rituel de la Haute Magie* (1856), in the public domain.

## Names

M-VAVE and FM-1 are names of their owners. Quod is made to run on the FM-1's panel and screen; it is not made or
endorsed by M-VAVE. Apple, Audio Unit, Logic and GarageBand are trademarks of Apple Inc.
