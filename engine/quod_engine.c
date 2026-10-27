/* SPDX-License-Identifier: MIT */
/* One slot of the engine: Quod's port (quod_port.h) for an Audio Unit, then Quod itself. Compiled once per slot with
 * QE_SLOT set (quod_slots.c). Everything runs on the render thread, a millisecond at a time as on the FM-1: the
 * physics, the main loop (input, sequencer, the screen into a frame buffer), and each 256-frame block when due.
 * The UI's thread only sets inputs and reads the screen and the LEDs.
 * Copyright (c) 2026 Kip DeCastro. See ../LICENSE. */
#include <stdint.h>
#if defined(__wasm__) && !defined(__wasi__)     /* (no libc: the compiler's own, which the Rust crate's runtime backs) */
#define memcpy __builtin_memcpy
#define memset __builtin_memset
#else
#include <string.h>
#endif
#include "quod_engine.h"

#ifndef QE_SLOT
#error "QE_SLOT: compile through quod_slots.c"
#endif

/* ---- the port */
enum { BTN_OCT_DOWN = 0, BTN_OCT_UP = 1, BTN_FX = 2, BTN_SEL = 3, BTN_ENV = 4, BTN_LFO = 5, BTN_EDIT = 6,
       BTN_GLO = 7, BTN_HOME = 8, BTN_SAVE = 9, BTN_ARP = 10, BTN_SEQ = 11, BTN_PLAY = 12, BTN_REC = 13 };
#define KEY_LED(k) (14u + (k))

static volatile uint32_t q_ms;
static struct { volatile uint32_t notes, buttons; } q_in;
static uint32_t ui_edges, ui_released, ui_note_edges;   /* (atomics: set by the UI, taken by the main loop) */
static int32_t ui_enc[7];
static uint32_t ui_enc_ms[7], ui_enc_ms0[7];    /* when the UI turned an encoder last, and first since a take (ms) */
static uint32_t ui_enc_taken[7], ui_enc_taken0[7];   /* the two at the last take */
static uint64_t ui_leds;

static uint32_t q_button_edges(uint32_t *released)
{
    *released = __atomic_exchange_n(&ui_released, 0u, __ATOMIC_ACQ_REL);
    return __atomic_exchange_n(&ui_edges, 0u, __ATOMIC_ACQ_REL);
}
static uint32_t q_note_edges(void) { return __atomic_exchange_n(&ui_note_edges, 0u, __ATOMIC_ACQ_REL); }
static int32_t q_enc_take(uint32_t e)
{
    ui_enc_taken[e] = __atomic_load_n(&ui_enc_ms[e], __ATOMIC_ACQUIRE);
    ui_enc_taken0[e] = __atomic_load_n(&ui_enc_ms0[e], __ATOMIC_ACQUIRE);
    return __atomic_exchange_n(&ui_enc[e], 0, __ATOMIC_ACQ_REL);
}
static uint32_t q_enc_ms(uint32_t e, uint32_t *first) { *first = ui_enc_taken0[e]; return ui_enc_taken[e]; }
static void q_led(uint32_t id, int on)
{
    uint64_t bit = (uint64_t)1 << id;
    if (on) __atomic_fetch_or(&ui_leds, bit, __ATOMIC_RELAXED);
    else __atomic_fetch_and(&ui_leds, ~bit, __ATOMIC_RELAXED);
}

static uint8_t mq[256][3];                      /* MIDI in: the host's events, then the main loop (one thread) */
static uint32_t mq_w, mq_r;
static int q_midi_get(uint8_t *st, uint8_t *d1, uint8_t *d2)
{
    if (mq_r == mq_w) return 0;
    *st = mq[mq_r & 255u][0]; *d1 = mq[mq_r & 255u][1]; *d2 = mq[mq_r & 255u][2];
    mq_r++;
    return 1;
}
static void q_midi_send(uint32_t st, uint32_t d1, uint32_t d2) { (void)st; (void)d1; (void)d2; }

static uint8_t saved[512];                      /* SAVE: kept here (the AU's own state is the host's project) */
static uint32_t saved_n;
static int q_settings_load(void *p, uint32_t n)
{
    if (!saved_n || n > saved_n) return -1;
    memcpy(p, saved, n);
    return 0;
}
static int q_settings_save(const void *p, uint32_t n)
{
    if (n > sizeof saved) return -1;
    memcpy(saved, p, n);
    saved_n = n;
    return 0;
}

static void q_poll(void) {}
static uint16_t fb[QUOD_W * QUOD_H];
static volatile uint32_t fb_seq;
static void q_lcd_sync(void) {}
static void q_lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *px)
{
    uint32_t j;
    for (j = 0; j < h && y + j < QUOD_H; j++)
        memcpy(fb + (y + j) * QUOD_W + x, px + j * w, (x + w <= QUOD_W ? w : QUOD_W - x) * sizeof *px);
    fb_seq++;
}
static uint32_t q_ticks(void) { return (uint32_t)quod_clock_us(); }
#define Q_TICKS_PER_US 1u
static void q_premaster(int on) { (void)on; }
static void q_usb_stats(uint32_t v[10]) { memset(v, 0, 10 * sizeof *v); }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#pragma clang diagnostic ignored "-Wunused-variable"
#pragma clang diagnostic ignored "-Wshorten-64-to-32"
#pragma clang diagnostic ignored "-Wconversion"
#pragma clang diagnostic ignored "-Wsign-conversion"
#pragma clang diagnostic ignored "-Wcomma"
#include "quod.c"
#pragma clang diagnostic pop

/* ---- the engine: a millisecond at a time, the blocks into a ring */
#define BLK 256u                                /* (the FM-1's: Quod's block-rate parts count on it) */
#define RING 2048u
static float ring_l[RING], ring_r[RING];
static uint32_t ring_w, ring_r_, owed;

static void one_ms(void)
{
    q_ms++;
    app_tick_ms();
    quod_step();
    owed += QUOD_FS;
    while (owed >= 1000u * BLK) {
        int32_t blk[2u * BLK];
        uint32_t i;
        owed -= 1000u * BLK;
        memset(blk, 0, sizeof blk);
        app_audio(blk, BLK);
        for (i = 0; i < BLK; i++, ring_w++) {
            ring_l[ring_w & (RING - 1u)] = (float)blk[2u * i] * (1.0f / 32768.0f);
            ring_r[ring_w & (RING - 1u)] = (float)blk[2u * i + 1u] * (1.0f / 32768.0f);
        }
    }
}

static void e_init(void)
{
    mq_r = mq_w = 0;
    quod_init();
}
static void e_render(float *l, float *r, uint32_t n)
{
    uint32_t i;
    while (ring_w - ring_r_ < n) one_ms();
    for (i = 0; i < n; i++, ring_r_++) {
        l[i] = ring_l[ring_r_ & (RING - 1u)];
        r[i] = ring_r[ring_r_ & (RING - 1u)];
    }
}
static void e_midi(uint8_t s, uint8_t d1, uint8_t d2)
{
    if (mq_w - mq_r >= 256u) return;
    mq[mq_w & 255u][0] = s; mq[mq_w & 255u][1] = d1; mq[mq_w & 255u][2] = d2;
    mq_w++;
}
static uint32_t e_param_count(void) { return P_COUNT; }
static const char *e_param_name(uint32_t i) { return i < P_COUNT ? PARAM[i].name : ""; }
static void e_param_range(uint32_t i, int32_t *lo, int32_t *hi, int32_t *def)
{
    if (i >= P_COUNT) { *lo = *hi = *def = 0; return; }
    *lo = PARAM[i].lo; *hi = PARAM[i].hi; *def = PARAM[i].def;
}
static int32_t e_param_get(uint32_t i) { return i < P_COUNT ? st.p[i] : 0; }
static void e_param_set(uint32_t i, int32_t v)
{
    if (i < P_COUNT) st.p[i] = (int16_t)clampi(v, PARAM[i].lo, PARAM[i].hi);
}
static uint32_t e_state_get(void *buf, uint32_t cap)
{
    if (cap < sizeof st) return 0;
    memcpy(buf, &st, sizeof st);
    return sizeof st;
}
static int e_state_set(const void *buf, uint32_t n)
{
    if (n != sizeof st || ((const uint32_t *)buf)[0] != STORE_VER) return -1;
    memcpy(&st, buf, sizeof st);
    return 0;
}
static void e_button(uint32_t b, int down)
{
    uint32_t bit = 1u << b;
    if (b >= 14u) return;
    if (down) {
        __atomic_fetch_or(&q_in.buttons, bit, __ATOMIC_ACQ_REL);
        __atomic_fetch_or(&ui_edges, bit, __ATOMIC_ACQ_REL);
    } else {
        __atomic_fetch_and(&q_in.buttons, ~bit, __ATOMIC_ACQ_REL);
        __atomic_fetch_or(&ui_released, bit, __ATOMIC_ACQ_REL);
    }
}
static void e_key(uint32_t k, int down)
{
    uint32_t bit = 1u << k;
    if (k >= 27u) return;
    if (down) {
        __atomic_fetch_or(&q_in.notes, bit, __ATOMIC_ACQ_REL);
        __atomic_fetch_or(&ui_note_edges, bit, __ATOMIC_ACQ_REL);
    } else __atomic_fetch_and(&q_in.notes, ~bit, __ATOMIC_ACQ_REL);
}
static void e_enc(uint32_t e, int32_t d)        /* (the times first: a take that sees the turn sees its times) */
{
    uint32_t ms;
    if (e >= 7u) return;
    ms = (uint32_t)(quod_clock_us() / 1000u);
    if (!__atomic_load_n(&ui_enc[e], __ATOMIC_ACQUIRE)) __atomic_store_n(&ui_enc_ms0[e], ms, __ATOMIC_RELEASE);
    __atomic_store_n(&ui_enc_ms[e], ms, __ATOMIC_RELEASE);
    __atomic_fetch_add(&ui_enc[e], d, __ATOMIC_ACQ_REL);
}
static const uint16_t *e_screen(uint32_t *seq) { *seq = fb_seq; return fb; }
static uint64_t e_leds(void) { return __atomic_load_n(&ui_leds, __ATOMIC_RELAXED); }
static const char *e_enc_name(uint32_t e)
{
    uint32_t p = e < 7u ? knob_param(e) : P_NONE;
    return p < P_COUNT ? PARAM[p].name : "";
}
static int32_t e_enc_level(uint32_t e, int *lock)
{
    if (e >= 7u) { *lock = 0; return -1; }
    return knob_level(e, lock);
}

#define BK_MAGIC 0x59474E44u                    /* "DNGY", as the FM-1's settings sector has it */
static uint32_t bk_crc32(const uint8_t *p, uint32_t n)
{
    uint32_t c = 0xFFFFFFFFu, k;
    while (n--) {
        c ^= *p++;
        for (k = 0; k < 8u; k++) c = c & 1u ? (c >> 1) ^ 0xEDB88320u : c >> 1;
    }
    return ~c;
}
static void bk_put32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }
static uint32_t bk_get32(const uint8_t *p) { return p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static uint32_t e_backup_get(void *buf, uint32_t cap)
{
    uint8_t *b = buf;
    if (cap < 12u + sizeof st) return 0;
    memcpy(b + 12, &st, sizeof st);
    bk_put32(b, BK_MAGIC);
    bk_put32(b + 4, sizeof st);
    bk_put32(b + 8, bk_crc32(b + 12, sizeof st));
    return 12u + sizeof st;
}
static int e_backup_set(const void *buf, uint32_t n)
{
    const uint8_t *b = buf;
    if (n < 16u || bk_get32(b) != BK_MAGIC || bk_get32(b + 4) != n - 12u) return -1;
    if (bk_crc32(b + 12, n - 12u) != bk_get32(b + 8)) return -2;
    if (n - 12u != sizeof st || bk_get32(b + 12) != STORE_VER) return -3;
    memcpy(&st, b + 12, sizeof st);
    return 0;
}
_Static_assert(12u + sizeof st <= QUOD_BACKUP_MAX, "QUOD_BACKUP_MAX");

#define QE_CAT2(a, b) a##b
#define QE_CAT(a, b) QE_CAT2(a, b)
const quod_slot_t QE_CAT(quod_slot_, QE_SLOT) = {
    e_init, e_render, e_midi, e_param_count, e_param_name, e_param_range, e_param_get, e_param_set,
    e_state_get, e_state_set, e_button, e_key, e_enc, e_screen, e_leds, e_enc_name, e_enc_level, e_backup_get, e_backup_set};
