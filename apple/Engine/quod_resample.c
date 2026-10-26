/* SPDX-License-Identifier: MIT */
/* See quod_resample.h. Copyright (c) 2026 Kip DeCastro. See ../../LICENSE. */
#include <math.h>
#include <string.h>
#include "quod_resample.h"

#define HALF (QRS_TAPS / 2)

void quod_rs_init(quod_rs_t *rs, const quod_slot_t *src, double out_rate)
{
    double fc = 0.5 * (out_rate < QUOD_FS ? out_rate / QUOD_FS : 1.0) * 0.91;   /* cutoff, of the input's rate */
    int p, j;
    rs->src = src;
    rs->step = QUOD_FS / out_rate;
    memset(rs->bl, 0, sizeof rs->bl);
    memset(rs->br, 0, sizeof rs->br);
    rs->have = HALF;                            /* (silence before the first frame) */
    rs->x = HALF - 1;
    for (p = 0; p <= QRS_PH; p++) {             /* phase p: the output p / QRS_PH past an input frame */
        double sum = 0, h[QRS_TAPS];
        for (j = 0; j < QRS_TAPS; j++) {
            double d = (double)p / QRS_PH + (HALF - 1) - j;   /* the distance from tap j, input frames */
            double s = d == 0 ? 2 * fc : sin(2 * M_PI * fc * d) / (M_PI * d);
            double w = (d + HALF) / QRS_TAPS;   /* Blackman-Harris over the taps */
            double win = w <= 0 || w >= 1 ? 0 : 0.35875 - 0.48829 * cos(2 * M_PI * w) + 0.14128 * cos(4 * M_PI * w)
                                                - 0.01168 * cos(6 * M_PI * w);
            h[j] = s * win;
            sum += h[j];
        }
        for (j = 0; j < QRS_TAPS; j++) rs->tab[p * QRS_TAPS + j] = (float)(h[j] / sum);   /* (unity at DC) */
    }
}

void quod_rs_render(quod_rs_t *rs, float *l, float *r, uint32_t n)
{
    uint32_t i;
    if (rs->step == 1.0) { rs->src->render(l, r, n); return; }
    for (i = 0; i < n; i++) {
        uint32_t i0 = (uint32_t)rs->x, j, p;
        float fr, sl = 0, sr = 0;
        const float *t0, *t1, *xl, *xr;
        if (i0 + HALF >= rs->have) {            /* more input: first drop what no output needs now */
            uint32_t drop = i0 + 1 - HALF, k = 256;
            memmove(rs->bl, rs->bl + drop, (rs->have - drop) * sizeof *rs->bl);
            memmove(rs->br, rs->br + drop, (rs->have - drop) * sizeof *rs->br);
            rs->have -= drop;
            rs->x -= drop;
            i0 -= drop;
            if (rs->have + k > QRS_BUF) k = QRS_BUF - rs->have;
            rs->src->render(rs->bl + rs->have, rs->br + rs->have, k);
            rs->have += k;
        }
        {
            double f = (rs->x - i0) * QRS_PH;   /* (in double: a float could round up to the next frame) */
            p = (uint32_t)f;
            if (p >= QRS_PH) p = QRS_PH - 1u;
            fr = (float)(f - p);
        }
        t0 = rs->tab + p * QRS_TAPS;
        t1 = t0 + QRS_TAPS;
        xl = rs->bl + i0 + 1 - HALF;
        xr = rs->br + i0 + 1 - HALF;
        for (j = 0; j < QRS_TAPS; j++) {
            float c = t0[j] + (t1[j] - t0[j]) * fr;
            sl += xl[j] * c;
            sr += xr[j] * c;
        }
        l[i] = sl;
        r[i] = sr;
        rs->x += rs->step;
    }
}
