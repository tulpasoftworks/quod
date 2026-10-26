/* SPDX-License-Identifier: MIT */
/* Quod as an Audio Unit's engine. Quod keeps its state in its own statics, so the engine is compiled once per slot
 * (quod_slots.c): each slot is an instrument of its own, and an AU instance takes a free one.
 * Copyright (c) 2026 Kip DeCastro. See ../../LICENSE. */
#pragma once
#include <stdint.h>

#define QUOD_FS 44100u                          /* the instrument's own rate (the AU resamples to the host's) */
#define QUOD_SLOTS 8
#define QUOD_W 240
#define QUOD_H 240

typedef struct {
    void (*init)(void);                         /* (again on reuse: the controls back to their defaults) */
    void (*render)(float *l, float *r, uint32_t n);   /* n frames at QUOD_FS */
    void (*midi)(uint8_t st, uint8_t d1, uint8_t d2);
    uint32_t (*param_count)(void);
    const char *(*param_name)(uint32_t i);
    void (*param_range)(uint32_t i, int32_t *lo, int32_t *hi, int32_t *def);
    int32_t (*param_get)(uint32_t i);
    void (*param_set)(uint32_t i, int32_t v);
    uint32_t (*state_get)(void *buf, uint32_t cap);   /* bytes written (0: too small) */
    int (*state_set)(const void *buf, uint32_t n);    /* 0: taken */
    void (*button)(uint32_t b, int down);       /* the panel, from the UI's thread */
    void (*key)(uint32_t k, int down);          /* 0 = F3 .. 26 = G5 */
    void (*enc)(uint32_t e, int32_t detents);
    const uint16_t *(*screen)(uint32_t *seq);   /* RGB565, byte-swapped (as the FM-1's panel takes it); seq counts blits */
    uint64_t (*leds)(void);                     /* bit id: buttons 0..13, keys 14..40 */
    const char *(*enc_name)(uint32_t e);        /* what encoder e turns now ("": nothing) */
    int32_t (*enc_level)(uint32_t e, int *lock);   /* where its dial is: 0 .. 32767 (-1: nothing); a step's lock or
                                                 * not (the screen's dial, for the panel's knob) */
} quod_slot_t;

extern const quod_slot_t *const QUOD_SLOT[QUOD_SLOTS];
