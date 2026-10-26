/* SPDX-License-Identifier: MIT */
/* Quod's small library: a sine, pitch in cents -> phase step, numbers and notes as text, a hash. Integers only.
 * Copyright (c) 2026 Kip DeCastro. See ../LICENSE. */
#include <stdint.h>

#define Q_INLINE static inline __attribute__((always_inline))

Q_INLINE int32_t clampi(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }

/* ---- the sine: a cycle in 1024 steps (SINT, RAM), read between them. q_sine_init() fills it, before any sound.
 * Built from the series sin x = x - x^3/3! + x^5/5! - ... to x^13 on a quarter cycle, Q30 (error under 1e-9), the
 * other three quarters by symmetry. */
static int16_t SINT[1025];
static void q_sine_init(void)
{
    static const int64_t INV[6] = {178956971, 53687091, 25565282, 14913081, 9761289, 6882960};   /* 1 / (2k (2k+1))
                                                 * for k = 1..6, Q30: x^2 / 6, / 20, / 42, / 72, / 110, / 156 */
    uint32_t i;
    for (i = 0; i <= 256u; i++) {
        int64_t x = ((int64_t)i * 1686629713) >> 8;   /* (pi / 2) i / 256, Q30 */
        int64_t x2 = (x * x) >> 30, s = 1 << 30;
        int32_t k, v;
        for (k = 5; k >= 0; k--)                /* Horner: 1 - x^2/6 (1 - x^2/20 (1 - ...)) */
            s = (1 << 30) - ((((x2 * INV[k]) >> 30) * s) >> 30);
        s = (x * s) >> 30;
        v = (int32_t)((s * 32767 + (1 << 29)) >> 30);
        SINT[i] = (int16_t)v;                   /* the first quarter, mirrored into the second */
        SINT[512u - i] = (int16_t)v;
        SINT[512u + i] = (int16_t)-v;           /* and the second half the first's negative */
        SINT[1024u - i] = (int16_t)-v;
    }
}
/* a full cycle over 2^32 -> -32767..32767 */
Q_INLINE int32_t sine(uint32_t ph)
{
    uint32_t i = ph >> 22;
    int32_t a = SINT[i];
    return a + (((SINT[i + 1u] - a) * (int32_t)(ph >> 7 & 0x7FFFu)) >> 15);
}

/* ---- pitch: MIDI note x 100 (cents) -> a phase step at 44.1 kHz (2^32 a cycle) */
static const uint32_t INC_C9[12] = {           /* MIDI 120..131: round(f / 44100 x 2^32); lower octaves shift right */
    815363807u, 863847862u, 915214929u, 969636441u, 1027294024u, 1088380105u,
    1153098554u, 1221665363u, 1294309365u, 1371273005u, 1452813141u, 1539201906u};
static const uint32_t CENT[100] = {             /* round(2^(i / 1200) x 2^30) */
    1073741824u, 1074362221u, 1074982976u, 1075604090u, 1076225563u, 1076847394u, 1077469586u, 1078092136u, 1078715047u, 1079338317u,
    1079961947u, 1080585938u, 1081210289u, 1081835001u, 1082460074u, 1083085508u, 1083711303u, 1084337460u, 1084963979u, 1085590860u,
    1086218103u, 1086845708u, 1087473676u, 1088102007u, 1088730701u, 1089359758u, 1089989179u, 1090618963u, 1091249112u, 1091879624u,
    1092510500u, 1093141742u, 1093773347u, 1094405318u, 1095037654u, 1095670355u, 1096303422u, 1096936855u, 1097570653u, 1098204818u,
    1098839349u, 1099474247u, 1100109512u, 1100745144u, 1101381143u, 1102017509u, 1102654243u, 1103291345u, 1103928816u, 1104566654u,
    1105204861u, 1105843437u, 1106482382u, 1107121695u, 1107761379u, 1108401432u, 1109041854u, 1109682647u, 1110323810u, 1110965344u,
    1111607248u, 1112249523u, 1112892169u, 1113535186u, 1114178575u, 1114822336u, 1115466468u, 1116110973u, 1116755850u, 1117401100u,
    1118046723u, 1118692719u, 1119339088u, 1119985830u, 1120632946u, 1121280436u, 1121928300u, 1122576538u, 1123225151u, 1123874139u,
    1124523502u, 1125173240u, 1125823353u, 1126473842u, 1127124707u, 1127775947u, 1128427564u, 1129079558u, 1129731928u, 1130384676u,
    1131037800u, 1131691302u, 1132345181u, 1132999438u, 1133654074u, 1134309087u, 1134964479u, 1135620249u, 1136276399u, 1136932927u
};
#define CENTS_LO (-851)                         /* 5 Hz */
#define CENTS_HI 13506                          /* 20 kHz */
Q_INLINE uint32_t cents_inc(int32_t c)
{
    uint32_t u = (uint32_t)(clampi(c, CENTS_LO, CENTS_HI) + 2400), oct = u / 1200u, r = u % 1200u;
    uint32_t inc = (uint32_t)(((uint64_t)INC_C9[r / 100u] * CENT[r % 100u]) >> 30);
    return oct <= 12u ? inc >> (12u - oct) : inc << (oct - 12u);   /* (octave 12: MIDI 120..131) */
}

/* ---- text: each writes at p, ends the string, returns its end */
static char *put(char *p, const char *s)
{
    while ((*p = *s++) != 0) p++;
    return p;
}
static char *put_num(char *p, uint32_t v, uint32_t digits)   /* at least that many digits */
{
    char rev[12];
    uint32_t n = 0;
    while (v || n < digits || n == 0) { rev[n++] = (char)('0' + v % 10u); v /= 10u; }
    while (n) *p++ = rev[--n];
    *p = 0;
    return p;
}
static char *put_note(char *p, int32_t n)        /* MIDI note: C4 = 60 */
{
    static const char NOTE[12][3] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    int32_t oct = (n + 120) / 12 - 11;           /* (floored, for notes below C-1 too) */
    p = put(p, NOTE[(n + 120) % 12]);
    if (oct < 0) { *p++ = '-'; return put_num(p, (uint32_t)-oct, 1); }
    return put_num(p, (uint32_t)oct, 1);
}

/* ---- FNV-1a, 32 bits */
static uint32_t hash(uint32_t h, const void *data, uint32_t n)
{
    const uint8_t *b = (const uint8_t *)data;
    uint32_t i;
    for (i = 0; i < n; i++) h = (h ^ b[i]) * 16777619u;
    return h;
}
static uint32_t hash_u(uint32_t h, uint32_t v) { return hash(h, &v, sizeof v); }
