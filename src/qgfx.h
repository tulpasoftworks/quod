/* SPDX-License-Identifier: MIT */
/* Quod's drawing: one strip of the screen at a time (a canvas of up to 240 x 48), points, lines, filled rectangles
 * and text drawn into it, then handed to the host (q_lcd_blit). Pixels are RGB565 stored byte-swapped, as an
 * ST7789 takes them over SPI; a host that wants them plain swaps them back (qg_swap16).
 * Copyright (c) 2026 Kip DeCastro. See ../LICENSE. */

#define QRGB(r, g, b) ((uint16_t)((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3)))
#define QC_BLACK 0x0000u
#define QC_WHITE 0xFFFFu
#define QC_GREY QRGB(110, 110, 120)

#define QG_MAX (240u * 48u)
static uint16_t qg_px[QG_MAX] Q_POOL;
static uint32_t qg_w, qg_h;                     /* the canvas now */
static int32_t qg_oy;                           /* added to every y: draw in screen rows, the canvas a strip of them */

Q_INLINE uint16_t qg_swap16(uint32_t c) { return (uint16_t)((c & 0xFFu) << 8 | (c >> 8 & 0xFFu)); }

static void qg_begin(uint32_t w, uint32_t h, uint16_t bg)
{
    uint32_t i, n;
    uint16_t v = qg_swap16(bg);
    if (h > QG_MAX / w) h = QG_MAX / w;
    q_lcd_sync();                               /* (the host may still be sending the last strip from qg_px) */
    qg_w = w;
    qg_h = h;
    for (i = 0, n = w * h; i < n; i++) qg_px[i] = v;
}

static void qg_blit(uint32_t x, uint32_t y) { q_lcd_blit(x, y, qg_w, qg_h, qg_px); }

Q_INLINE void qg_pset(int32_t x, int32_t y, uint16_t c)
{
    uint32_t ux = (uint32_t)x, uy = (uint32_t)(y + qg_oy);
    if (ux < qg_w && uy < qg_h) qg_px[uy * qg_w + ux] = qg_swap16(c);
}

static void qg_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c)
{
    int32_t x0 = x < 0 ? 0 : x, x1 = x + w > (int32_t)qg_w ? (int32_t)qg_w : x + w;
    int32_t y0 = y + qg_oy, y1 = y0 + h, i, j;
    uint16_t v = qg_swap16(c);
    if (y0 < 0) y0 = 0;
    if (y1 > (int32_t)qg_h) y1 = (int32_t)qg_h;
    for (j = y0; j < y1; j++) {
        uint16_t *row = qg_px + (uint32_t)j * qg_w;
        for (i = x0; i < x1; i++) row[i] = v;
    }
}

/* a line, ends included: step along the longer axis, the shorter one when its error passes half a pixel */
static void qg_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t c)
{
    int32_t ax = x1 - x0, ay = y1 - y0, sx = ax < 0 ? -1 : 1, sy = ay < 0 ? -1 : 1, n, k, e;
    if (ax < 0) ax = -ax;
    if (ay < 0) ay = -ay;
    if (ax > 4000 || ay > 4000) return;         /* (nothing sane is that long: a bad coordinate) */
    if (ax >= ay) {
        n = ax; e = ax / 2;
        for (k = 0; k <= n; k++) {
            qg_pset(x0, y0, c);
            x0 += sx;
            if ((e -= ay) < 0) { e += ax; y0 += sy; }
        }
    } else {
        n = ay; e = ay / 2;
        for (k = 0; k <= n; k++) {
            qg_pset(x0, y0, c);
            y0 += sy;
            if ((e -= ax) < 0) { e += ay; x0 += sx; }
        }
    }
}

/* ---- text: a monospaced bitmap font, a byte a row (tools/gen_font.py) */
typedef struct {
    uint8_t h, adv, first, last;
    const uint8_t *rows;                        /* h bytes a glyph, from first to last */
} qfont_t;
#include "quod_font.h"

static const uint8_t *qg_glyph(const qfont_t *f, uint32_t ch)
{
    if (ch >= 'a' && ch <= 'z' && f->last < 'a') ch -= 32u;
    if (ch < f->first || ch > f->last) ch = '?';
    return f->rows + (ch - f->first) * f->h;
}

static int32_t qg_text(int32_t x, int32_t y, const qfont_t *f, const char *s, uint16_t c)   /* returns the end x */
{
    for (; *s; s++, x += f->adv) {
        const uint8_t *g = qg_glyph(f, (uint8_t)*s);
        uint32_t r, b;
        for (r = 0; r < f->h; r++)
            if (g[r])
                for (b = 0; b < 8u; b++)
                    if (g[r] & 0x80u >> b) qg_pset(x + (int32_t)b, y + (int32_t)r, c);
    }
    return x;
}

static int32_t qg_text_w(const qfont_t *f, const char *s)
{
    int32_t n = 0;
    while (s[n]) n++;
    return n * f->adv;
}

static void text_c(int32_t x, int32_t y, int32_t w, const qfont_t *f, const char *s, uint16_t c)   /* centred in w */
{
    qg_text(x + (w - qg_text_w(f, s)) / 2, y, f, s, c);
}
