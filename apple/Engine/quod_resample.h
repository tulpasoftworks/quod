/* SPDX-License-Identifier: MIT */
/* From Quod's 44.1 kHz to the host's rate: a windowed-sinc interpolator (32 taps, 256 phases, linear between them),
 * pulling from an engine slot as it needs. At 44.1 kHz it is a straight copy.
 * Copyright (c) 2026 Kip DeCastro. See ../../LICENSE. */
#pragma once
#include "quod_engine.h"

#define QRS_TAPS 32
#define QRS_PH 256
#define QRS_BUF 4096

typedef struct {
    const quod_slot_t *src;
    double step;                                /* input frames per output frame */
    double x;                                   /* the next output's place in buf (input frames) */
    uint32_t have;                              /* frames in buf */
    float bl[QRS_BUF], br[QRS_BUF];
    float tab[(QRS_PH + 1) * QRS_TAPS];
} quod_rs_t;

void quod_rs_init(quod_rs_t *rs, const quod_slot_t *src, double out_rate);
void quod_rs_render(quod_rs_t *rs, float *l, float *r, uint32_t n);
