#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Quod's splash, as 4-bit grey images (two pixels a byte, the left one high, rows padded to a byte; 0 = nothing):

  the hands  of Eliphas Levi's Baphomet (Dogme et Rituel de la Haute Magie, 1856; public domain), the raised one
             (SOLVE) and the lowered one (COAGULA): as above, so below. Each is cut out of the engraving
             (a scan of it, 1274 x 1890, not in this repository: QUOD_ENGRAVING names it, default
             art/baphomet-levi-1856.jpg; the crops below are in that scan. The white of its skin flooded from a point
             in the palm and in each finger
             the engraver outlined apart, the parts too shaded to flood drawn round, the creases filled, grown to
             take in the outline) and inverted: his ink lines are the light. The wrists fade out.
  the title  "As Above, So Below" in UnifrakturMaguntia (art/fonts, SIL Open Font License); and the pages' titles
             (the tarot's cards) in the same.

  gen_splash.py [gen/quod_splash.h] [preview.png]"""
import os
import sys
from PIL import Image, ImageDraw, ImageFilter, ImageChops, ImageFont

OUT = sys.argv[1] if len(sys.argv) > 1 else 'gen/quod_splash.h'
PREVIEW = sys.argv[2] if len(sys.argv) > 2 else None
ART = os.environ.get('QUOD_ENGRAVING', 'art/baphomet-levi-1856.jpg')   # (public domain; not committed)
FONT = 'art/fonts/UnifrakturMaguntia-Book.ttf'
SCALE = 0.85
# name, crop box in the engraving, a point in each part outlined on its own (palm, fingers; in the crop), the parts
# too shaded to flood (drawn round, in the crop), the wrist's fade ((x, y) in the crop -> 0..1), where on the screen
HANDS = [
    ('UP', (210, 600, 320, 800), [(70, 150), (58, 50), (32, 100), (45, 100)],
     [[(27, 118), (45, 118), (58, 125), (60, 200), (38, 200), (33, 175), (30, 150), (27, 130)]],   # the palm's
     lambda x, y, w, h: min(1.0, max(0.0, (h - y) / 55.0)), (12, 2)),                           # shaded side
    ('DOWN', (990, 1110, 1165, 1300), [(50, 50), (78, 105), (112, 135)],   # (palm, a curled finger, the second
     [[(92, 110), (104, 104), (108, 112), (102, 122), (95, 120)]],         # pointing finger; the knuckle)
     lambda x, y, w, h: min(1.0, max(0.0, (x + y - 25) / 60.0)), (106, 34)),
]
TITLE = [('As Above, So Below', 26, 190)]                           # text, size, top
PAGES = [('FX_TITLE', 'XIV \u00b7 Temperance', 22)]               # the pages' titles: name, text, size


def cut(src, box, seeds, polys):
    g = src.crop(box)
    b = g.point(lambda v: 255 if v > 150 else 0)
    for seed in seeds:
        if b.getpixel(seed) == 255:
            ImageDraw.floodfill(b, seed, 128)
        else:
            print('seed on ink:', seed)
    mask = b.point(lambda v: 255 if v == 128 else 0)
    d = ImageDraw.Draw(mask)
    for poly in polys:
        d.polygon(poly, fill=255)
    w, h = mask.size
    out = Image.new('L', (w + 2, h + 2), 0)
    out.paste(mask, (1, 1))
    ImageDraw.floodfill(out, (0, 0), 100)            # what the border reaches is outside; the rest (creases) in
    mask = out.crop((1, 1, w + 1, h + 1)).point(lambda v: 0 if v == 100 else 255).filter(ImageFilter.MaxFilter(7))
    ink = g.point(lambda v: min(255, int((max(0, 215 - v) / 215) ** 0.55 * 255)))   # the ink, as light
    return ImageChops.multiply(ink, mask), mask


def hand(src, box, seeds, polys, fade):
    """the ink as light, and where the hand is (solid: what is behind it does not show through)"""
    im, mask = cut(src, box, seeds, polys)
    w, h = im.size
    px, mx = im.load(), mask.load()
    for y in range(h):
        for x in range(w):
            f = fade(x, y, w, h)
            px[x, y] = int(px[x, y] * f)
            mx[x, y] = int(mx[x, y] * f)
    size = (round(w * SCALE), round(h * SCALE))
    return im.resize(size, Image.LANCZOS).point(lambda v: min(255, v * 3 // 2)), mask.resize(size, Image.LANCZOS)


def title(lines=None):
    im = Image.new('L', (240, 240))
    d = ImageDraw.Draw(im)
    for text, size, y in lines or TITLE:
        f = ImageFont.truetype(FONT, size)
        d.text(((240 - d.textlength(text, font=f)) / 2, y), text, font=f, fill=255)
    box = im.getbbox()
    return im.crop(box), box[:2]


def emit(lines, name, im, at, mask=None):
    """0 nothing; with a mask (solid): 1 .. 15 black .. full, without: 1 .. 15 of the colour"""
    w, h = im.size
    data = []
    for y in range(h):
        if mask:
            row = [0 if mask.getpixel((x, y)) < 128 else 1 + (im.getpixel((x, y)) * 14 + 127) // 255 for x in range(w)]
        else:
            row = [min(15, (im.getpixel((x, y)) + 8) // 17) for x in range(w)]
        if w & 1:
            row.append(0)
        data += [(row[i] << 4) | row[i + 1] for i in range(0, len(row), 2)]
    lines += [f'#define {name}_W {w}', f'#define {name}_H {h}', f'#define {name}_X {at[0]}', f'#define {name}_Y {at[1]}',
              f'static const uint8_t {name}[{len(data)}] = {{']
    for i in range(0, len(data), 24):
        lines.append('    ' + ', '.join(f'0x{v:02X}' for v in data[i:i + 24]) + ',')
    lines.append('};')


def main():
    if not os.path.exists(ART):
        sys.exit(f"gen_splash.py: no engraving at {ART}: a 1274 x 1890 scan of Levi's Baphomet (1856, public domain); "
                 "set QUOD_ENGRAVING")
    src = Image.open(ART).convert('L')
    lines = ['/* generated by tools/gen_splash.py: the hands of Levi\'s Baphomet (1856, public domain), the title in',
             ' * UnifrakturMaguntia (SIL Open Font License); 4-bit grey, two pixels a byte (the left one high): 0 nothing; the',
             ' * hands are solid, 1 .. 15 black .. full; the title 1 .. 15 of its colour */']
    parts = []
    for name, box, seeds, polys, fade, at in HANDS:
        im, mask = hand(src, box, seeds, polys, fade)
        emit(lines, 'HAND_' + name, im, at, mask)
        parts.append((im, at))
    im, at = title()
    emit(lines, 'TITLE', im, at)
    parts.append((im, at))
    for name, text, size in PAGES:                   # (centred; the page places it in y)
        pim, pat = title([(text, size, 10)])
        emit(lines, name, pim, (pat[0], 0))
    open(OUT, 'w').write('\n'.join(lines) + '\n')
    print(OUT, [p[0].size for p in parts])
    if PREVIEW:
        p = Image.new('RGB', (240, 240))
        for (im, at), col in zip(parts, [(255, 228, 160), (255, 72, 48), (255, 255, 255)]):
            p.paste(Image.merge('RGB', [im.point(lambda v, c=c: v * c // 255) for c in col]), at, im)
        p.resize((480, 480), Image.NEAREST).save(PREVIEW)


if __name__ == '__main__':
    main()
