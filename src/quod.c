/* SPDX-License-Identifier: MIT */
/* Quod: a monophonic drone, made for the M-VAVE FM-1's panel and screen. Copyright (c) 2026 Kip DeCastro; see
 * ../LICENSE. The host it runs on supplies what quod_port.h lists (fm1/firmware/src/quod.c: the FM-1, under the GPL).
 *
 * A key summons a cluster of phasing oscillators around its note. The cluster is a mirror: above the root it is
 * angelic (sines softening to triangles, a vowel choir, wide, into a shimmer reverb), below it is hellish
 * (triangles folding harder with depth, undertones pulled toward dissonance, narrow, into drive). Keys recentre
 * the drone, they never retrigger it: every oscillator is a particle in syrup that flows to its new pitch at its
 * own rate, with momentum that sloshes (angels light and quick, demons heavy; climbing is harder for the heavy).
 * Stirring it (fast note changes) is agitation, and agitation drags the drone toward hell; left alone it purifies.
 *
 * Built to the spec's DECIDED items; its PROPOSED items are here as constants near the top, easy to change. Two
 * departures: integers only (this platform links no float support, see docs/RULES.md), and one core (the
 * physics runs in the 1 ms timer interrupt, the sound in the audio interrupt, the screen in the main loop).
 *
 *   keys        the root (last key); OCT- OCT+ shift them
 *   SELECT      Saturn  viscosity (water .. cold honey)       ALGORITHM Mercury  slosh (damping)
 *   KNOB 1      Sun     heaven: brightness, shimmer           KNOB 2    Mars     hell: drive, fold, dissonance
 *   KNOB 3      gate    a Buchla low-pass gate: 0 shut, each note a pluck .. 100 open, the drone
 *   KNOB 4      Venus   drift: the twins' depth then their speed, and the voices wandering out of tune (a
 *                       Lyra's few cents to 80, then wild: ~5 semitones deep at 100)   PRESETS   Jupiter  spread
 *   SEL held    shift:  Venus -> the delay
 *               (the dials across the top show what the knobs are now: the planets, shifted, or a step's locks)
 *   FX          the FX page (Temperance): the delay, its time and feedback, the shimmer, the drive; tap to
 *               stay, hold to peek; HOME back
 *   EDIT held   settle: viscosity rises, calming the drone without stopping it
 *   SEQ         step mode: the 16 white keys are the 16 steps (TR / Elektron style). Tap a step: its trig on / off.
 *               Hold a step and play a key: its root (or hold it 0.6 s and let go: it waits for a key, its own
 *               too; SEQ cancels). Hold a step and turn
 *               a planet: a lock, that planet's value while the step plays (FX while holding a step clears its
 *               locks).
 *   PLAY        start / stop      REC  live record: the keys you play go to the step playing
 *   SEL + SELECT / ALGORITHM / Moon / PRESETS: tempo, step length (a bar .. 1/16, or WAIT: the next step when
 *               the drone has settled), how settled, the pattern's length
 *   SAVE        keep everything           HOME  redraw; held: the audio's worst time, the frames a second
 *   HOME + REC held 1 s: the showcase (~95 s from the splash; any key, button or knob stops it)
 *   USB-MIDI    notes set the root; CC 20..26 the seven planets
 * Integers only; see docs/RULES.md. */
#include "quod_port.h"
#include "qlib.h"
#include "qgfx.h"

static int held_btn(uint32_t b) { return (int)(q_in.buttons >> b & 1u); }

/* ---------------------------------------------------------------- the controls --- */
enum { P_SATURN, P_MERCURY, P_SUN, P_MARS, P_MOON, P_VENUS, P_JUPITER, P_GATE, P_DELAY,
       P_BPM, P_DIV, P_THRESH, P_LEN, P_CHAR, P_DTIME, P_DFB, P_VERB, P_DRIVE, P_COUNT };
#define P_NONE P_COUNT                          /* (an encoder with nothing on it) */
typedef struct { const char *name, *tag; int16_t lo, hi, def; } param_t;
static const param_t PARAM[P_COUNT] = {
    [P_SATURN] = {"VISCOSITY", "", 0, 100, 71}, [P_MERCURY] = {"SLOSH", "", 0, 100, 40},
    [P_SUN] = {"HEAVEN", "", 0, 100, 50}, [P_MARS] = {"HELL", "", 0, 100, 30},
    [P_MOON] = {"TILT", "", 0, 100, 50}, [P_VENUS] = {"DRIFT", "", 0, 100, 40},
    [P_JUPITER] = {"SPREAD", "", 0, 100, 60},
    [P_GATE] = {"GATE", "", 0, 100, 100},       /* the low-pass gate's rest: 0 shut (a note plucks), 100 open (drone) */
    [P_DELAY] = {"DELAY", "DLY", 0, 100, 25},
    [P_BPM] = {"TEMPO", "BPM", 10, 240, 60}, [P_DIV] = {"STEP", "DIV", 0, 5, 3},
    [P_THRESH] = {"SETTLED", "THR", 0, 100, 20}, [P_LEN] = {"LENGTH", "LEN", 1, 16, 16},
    [P_CHAR] = {"HEAVEN / HELL", "", 0, 100, 50},   /* bipolar: 50 plain, left hell (Mars), right heaven (Sun) */
    /* the FX page's own (their defaults as it always was) */
    [P_DTIME] = {"DELAY TIME", "", 0, 100, 76}, /* 50 .. 430 ms */
    [P_DFB] = {"FEEDBACK", "", 0, 100, 51},     /* 0 .. 0.9 */
    [P_VERB] = {"SHIMMER", "", 0, 100, 50},     /* the shimmer reverb: its level 0 .. x6, its climb, its tail */
    [P_DRIVE] = {"DRIVE", "", 0, 100, 50},      /* hell's drive: 0 .. x2 */
};
/* the encoders (FM-1 order: SELECT, ALGORITHM, KNOB 1-4, PRESETS) -> the planets, in the spec's order; with SEL */
static const uint8_t ENC_PARAM[7] = {P_SATURN, P_MERCURY, P_CHAR, P_MOON, P_GATE, P_VENUS, P_JUPITER};
/* (Sun and Mars are no longer knobs of their own: KNOB 1 sets them, heaven to the right, hell to the left) */
/* the FX page (Temperance): delay, its time, its feedback, the shimmer, the drive */
static const uint8_t FX_PARAM[7] = {P_DELAY, P_DTIME, P_DFB, P_VERB, P_DRIVE, P_NONE, P_NONE};
static const uint8_t SHIFT_PARAM[7] = {P_BPM, P_DIV, P_NONE, P_NONE, P_THRESH, P_DELAY, P_LEN};
static const char *const DIV_NAME[6] = {"WAIT", "1 BAR", "1/2", "1/4", "1/8", "1/16"};
static const uint8_t DIV_16THS[6] = {0, 16, 8, 4, 2, 1};

#define STEPS 16
#define NOLOCK (-1)
#define STORE_VER 8u                            /* (an older store: the defaults) */
static struct {
    uint32_t ver;
    int16_t p[P_COUNT];
    uint16_t trig;                              /* the steps with a trig */
    uint8_t note[STEPS];                        /* each step's root (MIDI) */
    int8_t lock[STEPS][7];                      /* each step's planets, NOLOCK or 0..100 */
    int8_t oct;
} st;
/* (a backup, engine/quod_engine.h, is these bytes on the FM-1 and on every computer: a change here is a new
 * STORE_VER) */
_Static_assert(sizeof st == 172u, "the state's layout changed: raise STORE_VER");
static volatile int16_t pe[P_COUNT];            /* the controls in effect: the knobs, with the playing step's locks */
/* The probe (tools/fm1_probe.py drives the FM-1 over USB-MIDI, channel 16): stages can be bypassed to find
 * what makes a sound, and TONE replaces everything with a pure 441 Hz sine to test the path out. */
enum { B_ANGELS = 1, B_HELL = 2, B_ROOT = 4, B_CHOIR = 8, B_REVERB = 16, B_DRIVE = 32, B_DELAY = 64, B_FOLD = 128,
       B_PHYSICS = 256, B_DRIFT = 512, B_SOFT = 1024, B_TONE = 2048, B_DC = 4096, B_PREMASTER = 8192 };
static volatile uint32_t dbg, agit_off = 1;      /* agitation: off (the user's choice; the probe's CC 103 = 6 turns
                                                 * it on, 5 off) */
static uint32_t dirty, full_redraw = 1, splash_until, strip_force, ui_fps;
static volatile uint32_t ready, settle_held;
static volatile int32_t root_note = 60;          /* C4 */

/* ---------------------------------------------------------------- the cluster --- */
/* 13 oscillators: the root (index 6) and 12 voices (the others, in order) that live on a ladder of 24 rungs, 12
 * below the root (the undertones: 1/8 .. 4/5) and 12 above (the overtones, mirrored: 5/4 .. 8/1). The 12 voices
 * hold 12 neighbouring rungs; the high / low bias (KNOB 2, Moon) slides that window: all below .. half and half
 * .. all above, the voices gliding (the syrup) to their new rungs, through the root. A voice's character
 * follows where it is: above the root an angel, below a demon (side15, dep15), morphing as it crosses. Below,
 * Hell pulls the deep ones toward dissonance (a minor second up). */
#define NOSC 13
#define ROOT 6
static const int16_t RUNG[24] = {-3600, -3102, -2786, -2400, -2169, -1902, -1586, -1200, -884, -702, -498, -386,
                                 386, 498, 702, 884, 1200, 1586, 1902, 2169, 2400, 2786, 3102, 3600};
#define SINES_ONLY 1                            /* every oscillator a pure sine (no triangles, no fold) for now */
#define HZ_BLEND 16384                          /* detune in Hz (constant beats) vs cents (proportional): half */
#define GRAVITY 52429                           /* 0.8: heavy particles climb slowly, fall quickly */
#define COUPLE 16384                            /* neighbour coupling, x k */

/* omega0 (rad/s Q16) from viscosity, on a curve: fine steps at the fast end, big ones toward the slow:
 * 300 (water: ~0.02 s) .. 0.053 (cold honey: ~2 minutes), as 300 x 0.000176^((v/100)^2) */
static const uint32_t W0[101] = {
    19660800u, 19643806u, 19592911u, 19508379u, 19390647u, 19240321u, 19058170u, 18845123u,
    18602258u, 18330792u, 18032074u, 17707572u, 17358862u, 16987614u, 16595578u, 16184575u,
    15756475u, 15313193u, 14856665u, 14388840u, 13911666u, 13427074u, 12936968u, 12443213u,
    11947621u, 11451945u, 10957865u, 10466983u, 9980814u, 9500782u, 9028209u, 8564317u,
    8110222u, 7666933u, 7235349u, 6816261u, 6410350u, 6018195u, 5640266u, 5276937u,
    4928480u, 4595080u, 4276830u, 3973743u, 3685755u, 3412731u, 3154471u, 2910717u,
    2681157u, 2465434u, 2263150u, 2073874u, 1897143u, 1732475u, 1579365u, 1437299u,
    1305751u, 1184193u, 1072096u, 968933u, 874184u, 787337u, 707893u, 635365u,
    569282u, 509191u, 454656u, 405261u, 360607u, 320319u, 284041u, 251436u,
    222189u, 196005u, 172608u, 151741u, 133166u, 116663u, 102029u, 89076u,
    77633u, 67544u, 58664u, 50863u, 44023u, 38038u, 32809u, 28250u,
    24283u, 20837u, 17849u, 15263u, 13029u, 11103u, 9445u, 8021u,
    6800u, 5754u, 4861u, 4100u, 3452u,
};
/* how long the root takes to get there (within 5 %, for good), x omega0, ms rad/s: by slosh, the mean of a climb
 * and a fall (gravity: climbing is damped 1.8 x, falling 1 / 1.8); at slosh 40 the slowest viscosity is 2 min */
static const uint16_t TAU[101] = {
    12550u, 12516u, 12471u, 12408u, 12301u, 11066u, 11040u, 11007u, 10967u, 10916u,
    10850u, 10752u, 9516u, 9477u, 9428u, 9368u, 9288u, 9175u, 8218u, 8210u,
    8195u, 8168u, 8104u, 7019u, 7044u, 7070u, 7097u, 7123u, 7150u, 7177u,
    7204u, 7230u, 7256u, 7281u, 7303u, 7323u, 7340u, 7349u, 7347u, 7313u,
    6321u, 6369u, 6417u, 6465u, 6512u, 6558u, 6604u, 6650u, 6695u, 6739u,
    6783u, 6825u, 6867u, 6908u, 6948u, 6987u, 7026u, 7063u, 7099u, 7133u,
    7166u, 7198u, 7228u, 7257u, 7283u, 7305u, 7325u, 7339u, 7346u, 7338u,
    6394u, 6442u, 6489u, 6537u, 6584u, 6631u, 6678u, 6725u, 6772u, 6818u,
    6864u, 6910u, 6956u, 7001u, 7047u, 7092u, 7137u, 7182u, 7227u, 7271u,
    7316u, 7360u, 7403u, 7447u, 7490u, 7533u, 7576u, 7619u, 7660u, 7703u,
    7745u,
};

static int32_t absi(int32_t x) { return x < 0 ? -x : x; }

/* ---------------------------------------------------------------- the physics (1 ms timer interrupt) --- */
static volatile int32_t pos[NOSC];              /* live pitch, cents Q16 */
static volatile int32_t settle15[NOSC];         /* each one's stereo spread (Q15), by where it is in its glide: full
                                                 * at the note it left and at the note it is going to, the least
                                                 * halfway between */
static int32_t from_c[NOSC], to_c[NOSC];        /* each glide's ends, cents */
static int64_t vel[NOSC];                       /* cents/s Q16 (64 bits: at viscosity 0 a leap passes 2^15 cents/s,
                                                 * and 32 bits wrapped: the voice flew off) */
static uint32_t inv_m[NOSC], down_f[NOSC], up_f[NOSC], mass8[NOSC];
static volatile int32_t side15[NOSC], dep15[NOSC];   /* live: 32767 above the root .. 0 below (a 4-semitone blend
                                                      * across it); |its interval| / 3 octaves, Q15 */
static volatile int32_t agit;                   /* agitation, smoothed, Q15 */
static volatile int32_t agit_slow;              /* the same over ~2 s: how far it drags the tilt toward hell (a swell,
                                                 * not a jump: deep oscillators arriving at once thump) */
static int32_t visc8 = 71 << 8;                 /* viscosity in effect (settle raises it), Q8 */

static int32_t target_c(int32_t i, const int16_t *p)   /* cents */
{
    int32_t spread = 8192 + p[P_JUPITER] * 410;  /* Q15: 0.25 .. 1.5 */
    int32_t off = 0;
    if (i != ROOT) {
        int32_t v = i < ROOT ? i : i - 1, u8, r, f;
        u8 = clampi(p[P_MOON] * 3072 / 100 - (int32_t)(((int64_t)agit_slow * 1536) >> 15) + v * 256, 0, 23 * 256);
        r = u8 >> 8; f = u8 & 255;               /* its rung, and how far toward the next (the window slides) */
        off = RUNG[r] + (r < 23 ? ((RUNG[r + 1] - RUNG[r]) * f >> 8) : 0);
        if (off < 0) {                           /* below: toward dissonance with depth^2, Mars and agitation */
            int32_t d = clampi(-off * 9, 0, 32767), w = (int32_t)(((int64_t)d * d >> 15) * 23170 >> 15);
            w = (int32_t)((int64_t)w * (p[P_MARS] * 327) >> 15);
            w += (w * agit) >> 15;
            off += (100 * clampi(w, 0, 32767)) >> 15;
        }
    }
    return root_note * 100 + 1200 * st.oct + (int32_t)((int64_t)off * spread >> 15);
}

/* a voice's character from where it is: side and depth, then mass (angels light, demons heavy) and gravity */
static void character(int32_t i)
{
    int32_t rel = (pos[i] >> 16) - (pos[ROOT] >> 16), side, d, m, h, w, gw;
    if (i == ROOT) { side = 16384; d = 0; }
    else {
        side = clampi(16384 + rel * 41, 0, 32767);   /* (+-400 cents: the blend) */
        d = clampi(absi(rel) * 9, 0, 32767);    /* / 3600 cents */
    }
    side15[i] = side; dep15[i] = d;
    m = (int32_t)(((int64_t)(65536 - (int32_t)(((int64_t)d * 26214) >> 15)) * side +
                   (int64_t)(65536 + d * 4) * (32767 - side)) >> 15);   /* angels 1 .. 0.6, demons 1 .. 3 */
    mass8[i] = (uint32_t)m >> 8;
    inv_m[i] = 0xFFFFFFFFu / (uint32_t)m;       /* Q16 */
    h = (int32_t)(((int64_t)(2 * side - 32767) * d) >> 15);   /* -1 deep below .. +1 high above */
    w = (32768 - h) * 2;                         /* weight, Q16: 0 at the top .. 1 at the bottom */
    gw = (int32_t)(((int64_t)GRAVITY * w) >> 16);
    up_f[i] = (uint32_t)(65536 + gw);            /* climbing: more drag */
    down_f[i] = 0xFFFFFFFFu / (uint32_t)(65536 + gw);   /* falling: less */
}

static void physics_init(void)
{
    int32_t i;
    for (i = 0; i < NOSC; i++) {
        pos[i] = target_c(i, st.p) << 16;
        vel[i] = 0;
        settle15[i] = 32767;
        from_c[i] = to_c[i] = target_c(i, st.p);
    }
    for (i = 0; i < NOSC; i++) character(i);
}

static void app_tick_ms(void)
{
    int32_t i, e[NOSC], w0, z, goal;
    int64_t k, c, kc;                           /* (64-bit: at the fastest omega0^2 passes 2^31) */
    int64_t a_sum = 0;
    uint32_t q;
    const int16_t *p = (const int16_t *)pe;
    if (!ready || dbg & B_PHYSICS) return;
    /* viscosity: the knob, or rising toward the top while SETTLE is held (~3 s), back over ~1 s */
    goal = (settle_held ? 100 : p[P_SATURN]) << 8;
    if (visc8 < goal) visc8 += settle_held ? 9 : 26;
    if (visc8 > goal) visc8 -= 26;
    w0 = (int32_t)W0[clampi(visc8 >> 8, 0, 100)];
    z = 16384 + p[P_MERCURY] * 492;              /* damping ratio, Q16: 0.25 .. 1.0 (slightly under: slosh) */
    k = ((int64_t)w0 * w0) >> 16;                /* omega0^2, Q16 */
    c = ((int64_t)2 * z * w0) >> 16;
    kc = (k * COUPLE) >> 15;
    for (i = 0; i < NOSC; i++) character(i);
    for (i = 0; i < NOSC; i++) {
        int32_t t = target_c(i, p), now = pos[i] >> 16, span, want = 32767;
        e[i] = pos[i] - (t << 16);
        if (t - to_c[i] > 2 || to_c[i] - t > 2) { from_c[i] = now; to_c[i] = t; }   /* a new glide: from here */
        span = to_c[i] - from_c[i];
        if (span > 5 || span < -5) {             /* how far along, 0 .. 1; spread |2f - 1|: 1 at both ends */
            int32_t f = clampi(clampi(now - from_c[i], -60000, 60000) * 32768 / span, 0, 32767);   /* (32-bit) */
            want = f < 16384 ? 32767 - 2 * f : 2 * f - 32767;
        }
        settle15[i] += (clampi(want, 0, 32767) - settle15[i]) >> 4;   /* (smoothed, ~16 ms) */
    }
    for (i = 0; i < NOSC; i++) {
        int32_t el = i ? e[i - 1] : e[i], er = i < NOSC - 1 ? e[i + 1] : e[i], vc;
        int64_t ce;
        int64_t f;
        ce = (c * (vel[i] > 0 ? up_f[i] : down_f[i])) >> 16;   /* gravity */
        f = -(k * e[i] >> 16) - (ce * vel[i] >> 16) + (kc * ((int64_t)el + er - 2 * (int64_t)e[i]) >> 16);
        f = (f * inv_m[i]) >> 16;                /* a = F / m, cents/s^2 Q16 */
        vel[i] += (f * 67109) >> 26;             /* x 1 ms */
        pos[i] += (int32_t)((vel[i] * 67109) >> 26);
        vc = (int32_t)(vel[i] >> 16);            /* cents/s */
        a_sum += (int64_t)mass8[i] * vc * vc >> 8;
    }
    /* agitation: sum m v^2 against a stir of ~300 cents/s on every oscillator, smoothed over ~250 ms */
    q = (uint32_t)(a_sum >> 10);
    i = q >= 1142u ? 32767 : (int32_t)(q * 32767u / 1142u);
    agit += (i - agit) >> 8;
    agit_slow += (agit - agit_slow) >> 11;
    if (agit_off) agit = agit_slow = 0;
}

/* ---------------------------------------------------------------- the sound (audio interrupt) --- */
static uint32_t ph[NOSC], rng_x = 2463534242u;
/* Lyra-8 pairs: every voice has a near-unison twin at the same place, a few tenths of a hertz to a few hertz
 * above it, so each pair beats (swells and dips) at its own rate; the rates overlap and never line up, and
 * each wanders slowly. DRIFT (KNOB 4) is how deep and how fast: from 0 to 30 the twins fade in (0: pure, still),
 * then their beats quicken, 2^((v - 40) / 30) x these: 40 as below, 100 four times as fast. */
static const uint32_t PH_RATE[101] = {   /* Q15 */
    13004, 13308, 13619, 13937, 14263, 14596, 14938, 15287, 15644, 16010, 16384, 16767, 17159, 17560, 17970, 18390,
    18820, 19260, 19710, 20171, 20643, 21125, 21619, 22124, 22641, 23170, 23712, 24266, 24834, 25414, 26008, 26616,
    27238, 27875, 28526, 29193, 29875, 30574, 31288, 32020, 32768, 33534, 34318, 35120, 35941, 36781, 37641, 38520,
    39421, 40342, 41285, 42250, 43238, 44248, 45283, 46341, 47424, 48533, 49667, 50828, 52016, 53232, 54476, 55749,
    57052, 58386, 59751, 61147, 62576, 64039, 65536, 67068, 68635, 70240, 71882, 73562, 75281, 77041, 78841, 80684,
    82570, 84500, 86475, 88497, 90565, 92682, 94848, 97065, 99334, 101656, 104032, 106464, 108952, 111499, 114105,
    116772, 119501, 122295, 125153, 128078, 131072,
};
static const uint16_t TWIN_MHZ[NOSC] = {310, 530, 770, 1130, 1610, 2310, 470, 670, 970, 1390, 1990, 2850, 4100};
static uint32_t tph[NOSC], twob[NOSC];
/* In hell, a voice above the root becomes a screamer (its scream: how far above x Hell): it goes through hell's
 * bus instead of heaven's, and wails like the screamers (quick slides up to ~1.5 semitones; now and then a jump) */
static int32_t vw_g8[NOSC], vw_goal8[NOSC];
static uint32_t vw_t[NOSC];
static int32_t drift16[NOSC], drift_goal[NOSC]; /* drift: where each voice is (cents Q16), where it heads (Q15 of
                                                 * its reach) */
static uint32_t drift_t[NOSC];                  /* blocks until its next goal */
static int32_t fg_now[NOSC], drive_now = 4096;  /* (fold and drive glide across a block) */
/* The low-pass gate (GATE, KNOB 3), a Buchla 292's: a vactrol's light opens a VCA and a low-pass together. GATE
 * is its rest (0 shut .. 100 open: the drone, untouched); each new root (a key, a step with a trig, MIDI) strikes
 * it: the light on for ~2 ms, the vactrol opening in ~1 and closing as one does, quickly while bright, then a dim
 * tail (~25 ms .. ~0.2 s), the low-pass falling ahead of the level (~100 Hz shut): a pluck, back to the rest.
 * Before the shimmer and the delay: a pluck rings out in them. */
#define LPG_PING 6u                             /* blocks the light stays on (2756 a second) */
#define LPG_ATT 9800                            /* Q15 a block: opening, ~1 ms */
#define LPG_DEC_FAST 475                        /* closing from open, ~25 ms */
#define LPG_DEC_SLOW 60                         /* nearly shut, ~0.2 s (the vactrol's memory) */
#define LPG_LP_SHUT 462                         /* the low-pass shut: a one-pole at ~100 Hz, Q15 */
static volatile uint32_t lpg_strikes;           /* new roots (the main loop counts, the sound takes) */
static uint32_t lpg_seen, lpg_ping;
static int32_t lpg_l = 32767 << 15, lpg_y[4][2];   /* the vactrol's light (Q30), the low-pass (two poles a side,
                                                 * heaven's and hell's) */
/* The screamers: hell's own high voices. They live in a fixed range whatever the root: around E6 (1.2..1.5 kHz),
 * the middle of where women's screams sit (mean F0 484..1981 Hz, Pisanski et al. 2020) and the violins' top
 * register, on Psycho's cluster (E, Eb, F, Gb). Four sines a quarter-tone apart (they beat against each other)
 * with, after the research on screams and horror strings:
 *   roughness  their loudness pulses 30..150 times a second (Arnal et al. 2015: what makes a scream a scream),
 *              faster and deeper with Hell and agitation
 *   stabs      a new note sets off a burst of sharp attacks (Psycho's down-bows)
 *   cracks     now and then, more with Hell and agitation, one jumps pitch, grows off-harmonic sidebands for a
 *              moment, or drops a subharmonic (the nonlinear sounds of a voice past its limit; Blumstein 2010)
 *   wails      each slides on its own, quickly, up to about 1.5 semitones
 * They sound with hell's weight x Hell^2: a hint at the defaults, a cry at full hell. */
#define NSCR 4
#define SCR_ROUGH 0                             /* the roughness pulsing: off for now (to hear the rest) */
#define SCR_STABS 0                             /* the stabs on a new note: off for now */
static const int16_t SCR_C[NSCR] = {8750, 8800, 8850, 8900};   /* MIDI cents: Eb6+1/4, E6, E6+1/4, F6 */
static const int16_t SCR_PAN[NSCR] = {-26000, -9000, 9000, 26000};
static uint32_t scr_ph[NSCR], scr_sph[NSCR], scr_t[NSCR], scr_am[NSCR], scr_sub[NSCR], scr_fm[NSCR], scr_crack[NSCR];
static int32_t scr_g8[NSCR], scr_goal8[NSCR], scr_a[NSCR];
static uint8_t scr_kind[NSCR];
static int32_t stab_env, stab_left, stab_t, stab_root = -1;
static int32_t svf[4][2];                       /* the choir's formants: L F1, L F2, R F1, R F2: low, band */
static volatile int32_t ui_lev[NOSC], ui_beat[NOSC];   /* for the screen: each voice's level, its twin's beat (mHz) */
/* Heaven's choir: an angel's sine bends its own phase (sin(x + I sin x)), a soft harmonic series for the formants
 * to make a vowel of, and it sings with a vibrato of its own; both grow with Heaven and with how much an angel
 * the voice is */
#define CHOIR_I 26000                           /* the throat at full Heaven: ~1.2 radians */
#define CHOIR_VIB 115                           /* the vibrato at full Heaven: ~6 cents (Q15 of the pitch) */
static uint32_t vb_ph[NOSC];
/* Heaven's bell: never struck, it rings. A tuned church bell two octaves above the root (gliding with it): hum,
 * prime, the minor-third tierce, quint, nominal, and above, each swinging from side to side a fraction of a
 * hertz (a real bell's doublets: the warble that keeps it ringing). Its level is Heaven^2; into the shimmer. */
#define NBELL 7
#define BELL_OCT 2400
#define BELL_G 2000
static const uint32_t BELL_R[NBELL] = {32768, 65536, 77529, 98697, 131072, 164495, 197263};   /* x 0.5 1 1.183
                                                                      * 1.506 2 2.51 3.01 (Q16) */
static const int16_t BELL_A[NBELL] = {11000, 16000, 20000, 8000, 26000, 10000, 8000};
static const uint16_t BELL_BEAT[NBELL] = {270, 410, 630, 520, 830, 1170, 1430};   /* mHz, a doublet's beat */
static uint32_t bell_ph[NBELL], bell_sw[NBELL];
static int32_t bell_a[NBELL];
/* the shimmer reverb, stereo: a reverb a side, each fed by its own channel, the right one's lines 23 samples
 * longer (Freeverb's stereo spread), so a voice placed left rings out on the left */
#define RV_SPREAD 23u
static int32_t rv_c[2][4][1356 + RV_SPREAD] Q_POOL;   /* (32 bits: the input keeps 2 more bits, 12 dB less grain) */
static int32_t rv_a[2][2][556 + RV_SPREAD] Q_POOL;   /* (32 bits: the combs' sum, half of it, passes 16 bits in
                                                 * full heaven; a 16-bit store clipped the whole tail) */
static const uint16_t RV_CL[4] = {1116, 1188, 1277, 1356}, RV_AL[2] = {556, 441};
static uint32_t rv_ci[2][4], rv_ai[2][2];
static int32_t rv_f[2][4], shim_fb[2], rvL, rvR;
static int32_t rv_loss = 4588;                  /* the combs' loss a pass, Q15: 0.14 (x 0.86), less with SHIMMER */
static int32_t rv_dcx[2], rv_dcy[2];            /* the reverb's out, its DC blocked: the reverb's gain at DC is ~2,
                                                 * ~3.5 with the long tail, and around the shimmer's loop an offset
                                                 * (the rounding's) grew without end; scaled by the wet level, each
                                                 * SHIMMER step was a click */
static int32_t verb_now;                        /* the wet level, gliding to verb across each block */
#define PS_N 2048u
#define PS_W 1024u
static int16_t ps_buf[2][PS_N] Q_POOL;   /* the octave-up shifters (a side) in the
                                                                      * reverb's feedback */
static uint32_t ps_w[2], ps_d[2];
#define DL_N 22050u                             /* the master delay: up to 0.5 s */
static int32_t dl_buf[2][DL_N] Q_POOL;          /* (32 bits, kept to +-2^16: headroom for the feedback's build) */
static uint32_t dl_w, dl_lfo;
static int32_t dc_xl, dc_yl, dc_xr, dc_yr;


static inline __attribute__((always_inline)) int32_t rnd16(void)
{
    rng_x ^= rng_x << 13;
    rng_x ^= rng_x >> 17;
    rng_x ^= rng_x << 5;
    return (int32_t)rng_x >> 16;
}
static inline __attribute__((always_inline)) uint32_t freq_inc(int32_t c)
{
    uint32_t s = 0;
    while (c < 0) { c += 1200; s++; }
    return cents_inc(c) >> s;
}
static int32_t tri(uint32_t ph0)                 /* in phase with the sine */
{
    int32_t t = (int32_t)((ph0 + 0x40000000u) >> 16);
    return t < 32768 ? 2 * t - 32767 : 2 * (65535 - t) - 32767;
}
static Q_AUDIO_FN int32_t svf_bp(int32_t *s, int32_t x, int32_t f)   /* Chamberlin, band out, Q = 5 */
{
    int32_t hi = x - s[0] - ((s[1] * 6554) >> 15);
    s[1] += (f * hi) >> 15;
    s[0] += (f * s[1]) >> 15;
    s[1] = clampi(s[1], -131072, 131072);
    s[0] = clampi(s[0], -131072, 131072);
    return s[1];
}
static Q_AUDIO_FN int32_t reverb1(uint32_t c, int32_t x)   /* one side: Schroeder (Freeverb's lengths), x in, its tail out */
{
    int32_t k, y, o = 0;
    uint32_t sp = c ? RV_SPREAD : 0u;
    x >>= 1;                                     /* (combs under 2^17: the products below stay in 32 bits) */
    for (k = 0; k < 4; k++) {
        y = rv_c[c][k][rv_ci[c][k]];
        rv_f[c][k] = y + (((rv_f[c][k] - y) * 6554) >> 15);
        rv_c[c][k][rv_ci[c][k]] = x + rv_f[c][k] - ((rv_f[c][k] * rv_loss) >> 15);   /* x 0.86 .. 0.92 */
        if (++rv_ci[c][k] >= RV_CL[k] + sp) rv_ci[c][k] = 0;
        o += y;
    }
    o >>= 4;                                     /* (a quarter, of the 4x input: the reverb's gain stays under 1, so the shimmer's
                                                 * loop, its tail an octave up back in at up to a half, dies away;
                                                 * at a half it rang up to the rails in full heaven: distortion) */
    for (k = 0; k < 2; k++) {                    /* two allpasses in series */
        y = rv_a[c][k][rv_ai[c][k]];
        rv_a[c][k][rv_ai[c][k]] = o + (y >> 1);   /* (bounded: a gain of a half) */
        if (++rv_ai[c][k] >= RV_AL[k] + sp) rv_ai[c][k] = 0;
        o = y - (o >> 1);
    }
    return o;
}
static Q_AUDIO_FN void reverb(int32_t xl, int32_t xr)      /* rvL / rvR out */
{
    rvL = reverb1(0, xl);
    rvR = reverb1(1, xr);
}
static Q_AUDIO_FN int32_t shifter(uint32_t c, int32_t x)    /* an octave up: two heads reading twice as fast, crossfaded */
{
    uint32_t d = ps_d[c], w = ps_w[c], d2 = (d + PS_W / 2u) % PS_W;
    int32_t g1 = (int32_t)(d < PS_W / 2u ? d : PS_W - d), g2 = (int32_t)(d2 < PS_W / 2u ? d2 : PS_W - d2);
    int32_t y = (ps_buf[c][(w - d) % PS_N] * g1 + ps_buf[c][(w - d2) % PS_N] * g2) / (int32_t)(PS_W / 2u);   /* (linear:
                                                 * the shimmer is tonal, the heads in step; equal power summed them
                                                 * to 1.15 and the loop ran away) */
    ps_buf[c][w % PS_N] = (int16_t)clampi(x, -32767, 32767);
    ps_w[c] = w + 1u;
    ps_d[c] = d ? d - 1u : PS_W - 1u;           /* the delay shrinks one a sample: twice the speed */
    return y;
}
static Q_AUDIO_FN int32_t lpg(int32_t *y, int32_t x, int32_t a, int32_t g)   /* the low-pass gate, a side: two
                                                 * one-poles (a: their coefficient), then the VCA (g), Q15 */
{
    y[0] += (int32_t)(((int64_t)(x - y[0]) * a) >> 15);
    y[1] += (int32_t)(((int64_t)(y[0] - y[1]) * a) >> 15);
    return (int32_t)(((int64_t)y[1] * g) >> 15);
}
static Q_AUDIO_FN int32_t sat(int32_t x)                    /* hell's drive: 1.5 x / (1 + |x|), smooth however hard it is
                                                 * pushed (a clip's corner is a click), Q15 */
{
    int32_t a = x < 0 ? -x : x, q = 32768 + a;
    while (a > 65535) { x >>= 1; a >>= 1; q >>= 1; }   /* (scaled together: x << 15 stays in 32 bits) */
    return (x << 15) / q * 3 >> 1;
}
static Q_AUDIO_FN int32_t ceiling(int32_t x)                /* the output's: exactly linear to -6 dBFS, then bending
                                                 * smoothly (slope 1 at the knee) toward full scale. (A cubic soft clip
                                                 * bent everything: heaven's pure sines came out with -48 dB of
                                                 * harmonics and intermodulation, 12 dB under it at 2..6 kHz) */
{
    uint32_t a = (uint32_t)(x < 0 ? -x : x), e;
    if (a <= 16384u) return x;
    e = a - 16384u;
    if (e > 131071u) e = 131071u;               /* (e x 16383 stays in 32 bits) */
    a = 16384u + e * 16383u / (16383u + e);
    return x < 0 ? -(int32_t)a : (int32_t)a;
}

static volatile uint32_t audio_us_max;         /* the worst since start (HOME held shows it) */
static volatile uint32_t audio_us_win;         /* the worst this second (sent out: CC 118 / 119, channel 16) */
static volatile int32_t out_goal;              /* the sound's level, Q15: faded in from the start */
#define SPLASH_MS 4000u                         /* the splash: long enough to watch its symbols flow */
static Q_AUDIO_FN void app_audio(int32_t *out, uint32_t n)
{
    static const int32_t VOWEL[2][2] = {{1400, 4059}, {3407, 5084}};   /* "ooh" (300, 870 Hz), "aah" (730, 1090) */
    uint32_t t0 = q_ticks(), b, i, us;
    int16_t p[P_COUNT];
    int32_t ag = agit;
    memcpy(p, (const void *)pe, sizeof p);
    for (b = 0; b < n; b += 16u) {
        uint32_t inc[NOSC], itd[NOSC], tinc[NOSC], b_inc[NBELL];
        int32_t cpi[NOSC], b_g[NBELL][2], bw;
        int32_t gl[NOSC], gr[NOSC], gla[NOSC], gra[NOSC], glh[NOSC], grh[NOSC];
        int32_t fg[NOSC], k, sun = p[P_SUN] * 327, mars = p[P_MARS] * 327;
        int32_t width = p[P_JUPITER] * 327;     /* spread is also the stereo width: 0 mono .. full, the whole field */
        int32_t bright, f1, f2, shim, drive, wet_dl, lpg_g, lpg_a, lpg_open;
        int32_t verb, dl_base = 2205 + p[P_DTIME] * 168, dl_fb = p[P_DFB] * 295;   /* the delay: 50 .. 430 ms; 0 .. 0.9 */
        uint32_t s_inc[NSCR], s_am[NSCR], s_fm[NSCR];
        int32_t s_gl[NSCR], s_gr[NSCR], s_amd, s_fmi[NSCR], s_sub[NSCR];
        int32_t pair = (int32_t)PH_RATE[p[P_VENUS]];   /* the twins' speed (DRIFT), Q15: x0.4 .. 4 */
        int32_t tg = clampi(p[P_VENUS] * 32767 / 30, 0, 32767);   /* their depth: in by 30 */
        int32_t gt = (24576 * tg) >> 15, gm = 24576 + ((10177 * (32767 - tg)) >> 15);   /* the twin's gain and
                                                 * the voice's (alone it is louder: the pair's power kept), Q15 */
        int32_t dft = p[P_VENUS] * 1311 + (p[P_VENUS] > 80 ? (p[P_VENUS] - 80) * (p[P_VENUS] - 80) *
                      (p[P_VENUS] - 80) * 176 : 0);   /* the wander (DRIFT too), Q15: a Lyra's few cents to 80
                                                 * (x1 at 25, x3.2 at 80), then wild: x9 at 90, x46 at 100 */
        bright = clampi(sun - (ag >> 2), 0, 32767);
        f1 = VOWEL[0][0] + (((VOWEL[1][0] - VOWEL[0][0]) * bright) >> 15);
        f2 = VOWEL[0][1] + (((VOWEL[1][1] - VOWEL[0][1]) * bright) >> 15);
        for (k = 0; k < NOSC; k++) {
            int32_t d = dep15[k], sd = side15[k], c = (pos[k] >> 16) + (drift16[k] >> 16), mhz, amp, pan, lim;
            int32_t scream = k == ROOT ? 0 : (int32_t)(((int64_t)clampi(2 * sd - 32767, 0, 32767) * mars) >> 15);
            if (k != ROOT && sun)                /* heaven redeems the demons: below the root, a voice becomes an
                                                 * angel as far as Heaven goes (heaven's bus, the choir, the
                                                 * vibrato, an angel's detune, drift and place) */
                sd += (int32_t)(((int64_t)(32767 - sd) * sun) >> 15);
            if (scream) {                        /* a screaming voice wails */
                if (vw_t[k]-- == 0u) {
                    vw_t[k] = 1100u + ((uint32_t)rnd16() & 4095u) / 2u;
                    vw_goal8[k] = (rnd16() * 150) >> 7;
                    if ((rnd16() & 32767) < (scream >> 2)) vw_g8[k] = vw_goal8[k];   /* a crack: a jump */
                }
                vw_g8[k] += (vw_goal8[k] - vw_g8[k]) >> 7;
                c += (int32_t)(((int64_t)vw_g8[k] * scream) >> 15) >> 8;
            }
            int32_t v = k < ROOT ? k : k - 1;
            uint32_t base;
            /* drift: each voice wanders, gliding (~0.75 s) to a new goal every 1.5 .. 4 s, as far out of tune as
             * DRIFT lets it; demons stagger, angels hold steadier (by where it is now). (It was a step every block:
             * a 10 Hz jitter of a few cents that the ear averaged away, at any DRIFT.) */
            lim = (512 * sd + (512 + ((d * 2560) >> 15)) * (32767 - sd)) >> 15;
            lim = (int32_t)(((int64_t)lim * dft) >> 15);   /* its reach, cents Q8: 2..12 / 2 at 25, x3.2 at 80,
                                                             * x46 at 100 */
            if (!(dbg & B_DRIFT) && drift_t[k]-- == 0u) {
                drift_t[k] = 4134u + ((uint32_t)rnd16() & 8191u) * 7u / 8u;   /* 1.5 .. 4 s (2756 blocks a second) */
                drift_goal[k] = rnd16();
            }
            drift16[k] += ((int32_t)(((int64_t)drift_goal[k] * lim) >> 7) - drift16[k]) >> 11;
            base = freq_inc(c);
            /* detune: golden-ratio steps (never realigning); beats under 2 Hz above, 20..70 Hz roughness deep
             * below; blended between Hz and cents (proportional to pitch, 344.5 Hz = 2^25) */
            {
                int32_t g = (int32_t)(((uint32_t)(k + 1) * 40503u) & 65535u) >> 1;   /* frac(i phi), Q15 */
                if (k == ROOT) mhz = 0;
                else {                           /* angel's slow beats above, demon's roughness below, blended */
                    int32_t ma = 100 + (int32_t)(((int64_t)1600 * d >> 15) * g >> 15);
                    int32_t md = 500 + (int32_t)(((((int64_t)d * d >> 15) * 70000) >> 15) * (16384 + g) / 32768);   /*
                                                 * (64 bits to the end: deep, it passed 2^31 and wrapped) */
                    md += (int32_t)((int64_t)md * ag >> 15);
                    mhz = (int32_t)(((int64_t)ma * sd + (int64_t)md * (32767 - sd)) >> 15);
                }
                mhz = (int32_t)((int64_t)mhz * dft >> 15);   /* (out of tune with the drift) */
                if (k & 1) mhz = -mhz;
                {
                    int32_t hz_inc = mhz * 97 + mhz * 391 / 1000;   /* 2^32 / 44100 per Hz, /1000 */
                    int32_t prop = (int32_t)(((int64_t)hz_inc * base) >> 25);
                    int32_t dv = (int32_t)(((int64_t)hz_inc * HZ_BLEND + (int64_t)prop * (32768 - HZ_BLEND)) >> 15);
                    int32_t cap = (int32_t)(base >> 5);   /* at most ~3 % of its pitch: deep ones stay near theirs */
                    inc[k] = base + (uint32_t)clampi(dv, -cap, cap);
                }
                {                                /* its twin: a beat of TWIN_MHZ x Venus, wandering +-30 % */
                    int32_t bm;
                    twob[k] += 2000u + (uint32_t)k * 377u;   /* (each wanders at its own slow rate) */
                    bm = (int32_t)(((int64_t)TWIN_MHZ[k] * pair) >> 15);
                    bm += (bm * ((sine(twob[k] << 8) * 9830) >> 15)) >> 15;
                    tinc[k] = inc[k] + (uint32_t)(bm * 97 + bm * 391 / 1000);
                    ui_beat[k] = bm;
                }
                if (k != ROOT) {                 /* the choir's vibrato: 4.6 .. 5.8 Hz, each its own */
                    int32_t vd = (int32_t)(((int64_t)sun * sd >> 15) * CHOIR_VIB >> 15), dv;
                    vb_ph[k] += (4600u + 97u * (uint32_t)k) * 1558u;
                    if (vd) {
                        dv = (sine(vb_ph[k]) * vd) >> 15;
                        inc[k] += (uint32_t)(int32_t)(((int64_t)(int32_t)(inc[k] >> 1) * dv) >> 14);
                        tinc[k] += (uint32_t)(int32_t)(((int64_t)(int32_t)(tinc[k] >> 1) * dv) >> 14);
                    }
                }
            }
            /* level and place. Each voice has its own spot (SLOT), neighbours in pitch on opposite sides, stepping
             * outward with distance from the root; angels across the whole field, demons half (narrow, as the spec
             * has hell); the root in the middle. Spread scales it all: 0 mono .. full the whole field. Placed by
             * level (equal power) and by time: the far ear hears a voice up to 0.6 ms later (a phase offset, as
             * every voice is a sine: no delay line) */
            if (k == ROOT) amp = 5000;
            else amp = ((3600 - ((1200 * d) >> 15)) * sd + (3800 - ((800 * d) >> 15)) * (32767 - sd)) >> 15;
            {                                    /* its place: neighbours on opposite sides, outward with distance
                                                  * from the root; angels the whole field, demons half */
                int32_t mag = k == ROOT ? 0 : (d * sd + (d >> 1) * (32767 - sd)) >> 15;
                int32_t pp = (int32_t)(((int64_t)((v & 1) ? mag : -mag) * width >> 15) * settle15[k] >> 15);
                pan = 16384 + (pp >> 1);
                ui_lev[k] = amp;
                gl[k] = (amp * sine((uint32_t)(32768 - pan) * 32768u)) >> 15;
                gr[k] = (amp * sine((uint32_t)pan * 32768u)) >> 15;
                /* the right channel's lead in phase: 26 samples (0.6 ms) a side at full; positive = to the right */
                itd[k] = (uint32_t)(int32_t)(((int64_t)inc[k] * ((pp * 26) >> 7)) >> 8);
                if (pp > -10000 && pp < 10000) itd[k] = 0;   /* (toward the middle, under ~8 samples: none; the
                                                 * time is spent where the ear hears it, far to a side) */
            }
            /* heaven's bus (choir, shimmer) or hell's (drive), by where it is; and whose cross-modulation */
            {
                int32_t sa = k == ROOT ? 32767 : (int32_t)(((int64_t)sd * (32767 - scream)) >> 15);   /* (screamers: hell) */
                if (dbg & B_ANGELS) sa = 0;
                gla[k] = (gl[k] * sa) >> 15; gra[k] = (gr[k] * sa) >> 15;
                glh[k] = dbg & B_HELL ? 0 : gl[k] - gla[k];
                grh[k] = dbg & B_HELL ? 0 : gr[k] - gra[k];
                if (glh[k] < 8 && glh[k] > -8 && grh[k] < 8 && grh[k] > -8) {   /* (a crumb: all heaven's) */
                    gla[k] = gl[k]; gra[k] = gr[k]; glh[k] = grh[k] = 0;
                }
                cpi[k] = (int32_t)((((int64_t)sun * sa) >> 15) * CHOIR_I >> 15);   /* the choir's throat */
            }
            /* character: angels sine -> triangle with the Sun; demons fold harder with depth, Mars, agitation */
            fg[k] = 0;                           /* (sines only: no triangles, no fold) */
        }
        /* the shimmer's octave-up feedback: Heaven^2, scaled by SHIMMER (x0 .. x1 at 50 .. x2), and above 50 some of
         * its own (up to 0.4, heaven or hell); at most a half, less as the tail lengthens above 50 (the combs' loss
         * 0.14 .. 0.08): at 0.5 with the long tail the loop sat on the ceiling */
        shim = clampi((((sun * sun) >> 16) * p[P_VERB] / 50) + (p[P_VERB] > 50 ? (p[P_VERB] - 50) * 262 : 0), 0,
                      p[P_VERB] > 50 ? 16384 - (p[P_VERB] - 50) * 98 : 16384);
        rv_loss = p[P_VERB] > 50 ? 4588 - (p[P_VERB] - 50) * 39 : 4588;
        drive = 4096 + (((mars * 3) >> 2) + (ag >> 2)) * p[P_DRIVE] / 50;   /* Q12: 1 .. ~9 at 50 (DRIVE) */
        {                                        /* the low-pass gate's vactrol, a block */
            int32_t in = (lpg_ping ? 32767 : p[P_GATE] * 32767 / 100) << 15, v;
            uint32_t s = lpg_strikes;
            if (s != lpg_seen) { lpg_seen = s; lpg_ping = LPG_PING; in = 32767 << 15; }
            if (lpg_ping) lpg_ping--;
            if (in > lpg_l) lpg_l += (int32_t)(((int64_t)(in - lpg_l) * LPG_ATT) >> 15);
            else lpg_l -= (int32_t)(((int64_t)(lpg_l - in) * (LPG_DEC_SLOW + (((LPG_DEC_FAST - LPG_DEC_SLOW) *
                                                                           (lpg_l >> 15)) >> 15))) >> 15);
            v = lpg_l >> 15;
            lpg_open = v >= 32767;               /* (open: the sound untouched) */
            lpg_g = v;
            lpg_a = LPG_LP_SHUT + (int32_t)(((int64_t)(32767 - LPG_LP_SHUT) * ((((v * v) >> 15) * v) >> 15)) >> 15);
                                                 /* (v^3: the low-pass closes ahead of the level) */
        }
        {                                        /* the screamers, a block's worth */
            int32_t hellw = (int32_t)(((int64_t)mars * mars) >> 15);   /* Hell^2 (whatever the bias) */
            int32_t rough = (mars + ag) >> 1, j, rate;
            /* stabs: a new note sets off 4..7 sharp attacks, ~8 a second, each decaying in ~50 ms */
            if (root_note != stab_root) {
                if (SCR_STABS && stab_root >= 0) { stab_left = 4 + (rnd16() & 3); stab_t = 0; }
                stab_root = root_note;
            }
            if (stab_left && stab_t-- <= 0) { stab_env = 32767; stab_left--; stab_t = 300 + (rnd16() & 63); }
            stab_env = (stab_env * 32531) >> 15;
            rate = 30 + ((120 * rough) >> 15);  /* roughness: 30 .. 150 pulses a second */
            s_amd = 8192 + ((20000 * rough) >> 15);   /* its depth, Q15 */
            for (j = 0; j < NSCR; j++) {
                int32_t sw, amp, pp, pan, chance;
                if (scr_t[j]-- == 0u) {          /* a new place to wail to, every 0.4 .. 1.2 s */
                    scr_t[j] = 1100u + ((uint32_t)rnd16() & 4095u) / 2u;
                    scr_goal8[j] = (rnd16() * 150) >> 7;   /* +-150 cents, Q8 */
                }
                scr_g8[j] += (scr_goal8[j] - scr_g8[j]) >> 7;
                /* cracks: up to ~2 a second each at full Hell and agitation */
                chance = (int32_t)(((int64_t)hellw * (8192 + rough)) >> 15) >> 6;   /* of 32768, a block */
                if (!scr_crack[j] && (rnd16() & 32767) < chance) {
                    scr_kind[j] = (uint8_t)((rnd16() & 32767) % 3);
                    scr_crack[j] = 400u + ((uint32_t)rnd16() & 511u);   /* 150 .. 330 ms */
                    if (scr_kind[j] == 0) scr_g8[j] = scr_goal8[j] = (rnd16() * 200) >> 7;   /* a jump, not a slide */
                }
                if (scr_crack[j]) scr_crack[j]--;
                s_inc[j] = freq_inc(SCR_C[j] + (scr_g8[j] >> 8));
                s_fmi[j] = scr_crack[j] && scr_kind[j] == 1 ? 24000 : 0;   /* off-harmonic sidebands: PM at 87 Hz+ */
                s_fm[j] = freq_inc(-1000 + j * 130);   /* (87, 94, 102, 110 Hz: none a harmonic of the voice) */
                s_sub[j] = scr_crack[j] && scr_kind[j] == 2 ? 20000 : 0;   /* a subharmonic, half the pitch */
                s_am[j] = (uint32_t)rate * 97391u + (uint32_t)j * 7000000u;   /* (each a little apart) */
                scr_sph[j] += 1600u << 8;        /* swells, each its own */
                sw = (sine(scr_sph[j] + (uint32_t)j * 0x40000000u) + 32768) >> 1;
                amp = (int32_t)(((int64_t)hellw * (8192 + ((sw * 3) >> 2))) >> 15);
                amp = (int32_t)(((int64_t)amp * (32768 + ag)) >> 15);
                amp += (int32_t)(((int64_t)hellw * stab_env) >> 15) * 4;   /* the stabs ride on top, loud */
                scr_a[j] += ((amp * 2200 >> 15) - scr_a[j]) >> 1;   /* (quick: the stabs keep their edge) */
                pp = (SCR_PAN[j] * width) >> 15;
                pan = 16384 + (pp >> 1);
                s_gl[j] = (scr_a[j] * sine((uint32_t)(32768 - pan) * 32768u)) >> 15;
                s_gr[j] = (scr_a[j] * sine((uint32_t)pan * 32768u)) >> 15;
            }
        }
        bw = (int32_t)(((int64_t)sun * sun) >> 15);
        {                                        /* heaven's bell, a block's worth */
            uint32_t i0 = bw || bell_a[4] ? freq_inc((pos[ROOT] >> 16) + BELL_OCT) : 0u;
            int32_t j;
            for (j = 0; j < NBELL && (bw || bell_a[4]); j++) {   /* (silent: skipped) */
                int32_t a, pan, pp;
                b_inc[j] = (uint32_t)(((uint64_t)i0 * BELL_R[j]) >> 16);
                bell_sw[j] += (uint32_t)BELL_BEAT[j] * 1558u;   /* its swing, a block's step */
                pp = (sine(bell_sw[j]) * ((20000 * width) >> 15)) >> 15;
                bell_a[j] += (((int32_t)(((int64_t)bw * BELL_A[j]) >> 15) * BELL_G >> 15) - bell_a[j]) >> 6;
                a = (bell_a[j] * (26214 + (sine(bell_sw[j] * 2u + 0x40000000u) * 6553 >> 15))) >> 15;   /* (and it
                                                                      * swells a little, mono too) */
                pan = 16384 + (pp >> 1);
                b_g[j][0] = (a * sine((uint32_t)(32768 - pan) * 32768u)) >> 15;
                b_g[j][1] = (a * sine((uint32_t)pan * 32768u)) >> 15;
            }
            bw = bell_a[4] > 1;
        }
        wet_dl = p[P_DELAY] * 230;
        verb = (8192 + (sun >> 2)) * (p[P_VERB] <= 50 ? p[P_VERB] : 50 + (p[P_VERB] - 50) * 5) / 50;   /* the
                                                 * shimmer's level: SHIMMER 0 dry, 50 as it was, 100 x6 */
        {
            int32_t dfg[NOSC], ddr = (drive - drive_now) / 16, dverb = (verb - verb_now) / 16;
            for (k = 0; k < ROOT; k++) dfg[k] = (fg[k] - fg_now[k]) / 16;
        for (i = 0; i < 16u; i++) {
            int32_t aL = 0, aR = 0, hL = 0, hR = 0, s, x, rL, rR, dl, dr;
            drive_now += ddr;
            for (k = 0; k < ROOT; k++) fg_now[k] += dfg[k];
            ph[ROOT] += inc[ROOT];
            tph[ROOT] += tinc[ROOT];
            s = dbg & B_ROOT ? 0 : (sine(ph[ROOT]) * gm + (gt ? sine(tph[ROOT]) * gt : 0)) >> 15;   /* (the pair) */
            aL = (s * gl[ROOT]) >> 15; aR = (s * gr[ROOT]) >> 15;   /* (the earth rides with the angels' bus, dry) */
            for (k = 0; k < NOSC; k++) {         /* the voices and their twins, each into heaven's and hell's buses */
                int32_t l, r, tw;
                if (k == ROOT) continue;
                ph[k] += inc[k];
                tph[k] += tinc[k];
                if (!gl[k] && !gr[k]) continue;
                {
                    uint32_t p0 = ph[k], q = 0;
                    int32_t m, mr;
                    if (cpi[k]) q = (uint32_t)(sine(p0) * cpi[k]);   /* the choir's throat: the voice bends
                                                                       * its own phase (and its twin's) */
                    m = sine(p0 + q);
                    mr = itd[k] ? sine(p0 + itd[k] + q) : m;
                    tw = gt ? (sine(tph[k] + q) * gt) >> 15 : 0;   /* the twin, at the same place (level
                                                                      * only) */
                    l = ((m * gm) >> 15) + tw;
                    r = ((mr * gm) >> 15) + tw;
                }
                aL += (l * gla[k]) >> 15; aR += (r * gra[k]) >> 15;
                hL += (l * glh[k]) >> 15; hR += (r * grh[k]) >> 15;
            }
            if (!(dbg & B_HELL)) {
                int32_t j;
                for (j = 0; j < NSCR; j++) {     /* the screamers, into hell's bus (and its drive) */
                    int32_t v, a;
                    if (!s_gl[j] && !s_gr[j]) continue;
                    scr_ph[j] += s_inc[j];
                    if (s_fmi[j]) {              /* a crack: sidebands */
                        scr_fm[j] += s_fm[j];
                        v = sine(scr_ph[j] + (uint32_t)((sine(scr_fm[j]) * s_fmi[j]) >> 15) * 65536u);
                    } else v = sine(scr_ph[j]);
                    if (s_sub[j]) {              /* a crack: the voice breaks to half its pitch */
                        scr_sub[j] += s_inc[j] >> 1;
                        v = (v * (32767 - s_sub[j] / 2) + sine(scr_sub[j]) * s_sub[j]) >> 15;
                    }
                    if (SCR_ROUGH) {             /* roughness: 1 - depth .. 1, 30..150 times a second */
                        scr_am[j] += s_am[j];
                        a = 32767 - ((s_amd * (32767 - sine(scr_am[j]))) >> 16);
                        v = (v * a) >> 15;
                    }
                    hL += (v * s_gl[j]) >> 15;
                    hR += (v * s_gr[j]) >> 15;
                }
            }
            /* heaven: the vowel choir, then the shimmer reverb (its tail octave-up fed back), which hell's bus feeds
             * too (before its drive): the shimmer is heaven's and hell's */
            if (!(dbg & B_CHOIR)) {
                aL = (aL >> 1) + (svf_bp(svf[0], aL, f1) >> 1) + (svf_bp(svf[1], aL, f2) * 3 >> 3);
                aR = (aR >> 1) + (svf_bp(svf[2], aR, f1) >> 1) + (svf_bp(svf[3], aR, f2) * 3 >> 3);
            }
            if (bw) {                            /* the bell, after the choir, into the shimmer */
                int32_t j, a;
                for (j = 0; j < NBELL; j++) {
                    bell_ph[j] += b_inc[j];
                    a = sine(bell_ph[j]);
                    aL += (a * b_g[j][0]) >> 15;
                    aR += (a * b_g[j][1]) >> 15;
                }
            }
            if (lpg_open) {                      /* the low-pass gate (open: untouched, its low-pass following) */
                lpg_y[0][0] = lpg_y[0][1] = aL; lpg_y[1][0] = lpg_y[1][1] = aR;
                lpg_y[2][0] = lpg_y[2][1] = hL; lpg_y[3][0] = lpg_y[3][1] = hR;
            } else {
                aL = lpg(lpg_y[0], aL, lpg_a, lpg_g); aR = lpg(lpg_y[1], aR, lpg_a, lpg_g);
                hL = lpg(lpg_y[2], hL, lpg_a, lpg_g); hR = lpg(lpg_y[3], hR, lpg_a, lpg_g);
            }
            if (!(dbg & B_REVERB)) {
                reverb(aL + hL + shim_fb[0], aR + hR + shim_fb[1]);
                for (uint32_t c = 0; c < 2u; c++) {   /* its DC blocked (see rv_dcy), then the octave up a side,
                                                 * back in */
                    int32_t v = c ? rvR : rvL;
                    rv_dcy[c] = v - rv_dcx[c] + (int32_t)(((int64_t)rv_dcy[c] * 32604) >> 15); rv_dcx[c] = v;
                    shim_fb[c] = (shifter(c, rv_dcy[c] >> 1) * shim) >> 14;   /* (half in, x2 back: the shifter
                                                 * keeps 16 bits, and a long tail passes 2^15) */
                }
                verb_now += dverb;
                rL = aL + (int32_t)(((int64_t)rv_dcy[0] * verb_now) >> 14);   /* (x2: the reverb's own gain
                                                 * halved; 64 bits: a long tail at x6 passed 2^31 and wrapped) */
                rR = aR + (int32_t)(((int64_t)rv_dcy[1] * verb_now) >> 14);
            } else { rL = aL; rR = aR; }
            /* hell: drive */
            if (!(dbg & B_DRIVE) && (hL | hR)) {   /* (hell's bus silent, as in full heaven: nothing to drive) */
                hL = sat((hL * drive_now) >> 12) * 3 >> 2;
                hR = sat((hR * drive_now) >> 12) * 3 >> 2;
            }
            x = rL + hL;
            s = rR + hR;
            /* the master delay, Lyra-style: its time wanders, it feeds back */
            if (!(dbg & B_DELAY)) {
                /* the delay's time, Q16 samples, read between samples (an integer step would grit) */
                uint32_t dt = ((uint32_t)dl_base << 16) + (uint32_t)(sine(dl_lfo) * 1500 * 2);   /* it wanders +-1500
                                                                      * samples */
                uint32_t r0 = (dl_w + DL_N - (dt >> 16)) % DL_N, r1 = r0 ? r0 - 1u : DL_N - 1u, r2, r3;
                int32_t fr = (int32_t)(dt & 0xFFFFu) >> 1;
                r2 = (r0 + 331u) % DL_N; r3 = r2 ? r2 - 1u : DL_N - 1u;
                dl = dl_buf[0][r0] + (int32_t)(((int64_t)(dl_buf[0][r1] - dl_buf[0][r0]) * fr) >> 15);
                dr = dl_buf[1][r2] + (int32_t)(((int64_t)(dl_buf[1][r3] - dl_buf[1][r2]) * fr) >> 15);
                /* fed back across (blurring the image) when narrow, each side to itself when wide */
                int32_t fx = (dl_fb * (32767 - width)) >> 15, fs_ = (dl_fb * width) >> 15;
                dl_buf[0][dl_w] = clampi(x + ((dr * fx + dl * fs_) >> 15), -65535, 65535);
                dl_buf[1][dl_w] = clampi(s + ((dl * fx + dr * fs_) >> 15), -65535, 65535);
                if (++dl_w >= DL_N) dl_w = 0;
                dl_lfo += 19476u;                /* 0.2 Hz */
                x += (dl * wet_dl) >> 15;
                s += (dr * wet_dl) >> 15;
            }
            if (dbg & B_DC) { dc_yl = x; dc_yr = s; }
            else {
                dc_yl = x - dc_xl + (int32_t)(((int64_t)dc_yl * 32604) >> 15); dc_xl = x;   /* (64 bits: */
                dc_yr = s - dc_xr + (int32_t)(((int64_t)dc_yr * 32604) >> 15); dc_xr = s;   /*  loud wets) */
            }
            if (dbg & B_TONE) {                  /* a pure 441 Hz sine, 100 samples a cycle */
                static uint32_t tph;
                tph += 42949673u;
                dc_yl = dc_yr = sine(tph) >> 1;
            }
            {                                    /* the level: silent under the splash, faded in with the screen */
                static int32_t g;                /* (a step a sample toward the main loop's goal: no zipper) */
                int32_t goal = out_goal;
                g += clampi(goal - g, -4, 4);
                dc_yl = (dc_yl * (g >> 3)) >> 12;
                dc_yr = (dc_yr * (g >> 3)) >> 12;
            }
            if (dbg & B_SOFT) {
                out[2u * (b + i)] += clampi(dc_yl, -32767, 32767) >> 1;
                out[2u * (b + i) + 1u] += clampi(dc_yr, -32767, 32767) >> 1;
            } else {
                out[2u * (b + i)] += ceiling((dc_yl * 81) >> 7);   /* (x 0.633: the level the cubic gave quiet) */
                out[2u * (b + i) + 1u] += ceiling((dc_yr * 81) >> 7);
            }
        }
            for (k = 0; k < ROOT; k++) fg_now[k] = fg[k];
            drive_now = drive;
        }
    }
    us = (q_ticks() - t0) / Q_TICKS_PER_US;
    if (us > audio_us_max) audio_us_max = us;
    if (us > audio_us_win) audio_us_win = us;
}

/* ---------------------------------------------------------------- the panel and the sequencer --- */
/* TR / Elektron style: 16 steps on the white keys. A step with a trig moves the root to its note when it is
 * reached (the physics does everything between); its locks hold while it is the step playing. */
static uint32_t held_keys, seq_play, seq_i, seq_t, rec_on, shift, step_mode, held_used, held_key;
static int32_t held_step = -1, last_enc = -1;
static uint32_t enc_t, held_t, armed;   /* armed: a step held long and let go, waiting for its key */
static int8_t white_of[27];

static void set_root(int32_t n) { root_note = clampi(n, 12, 108); lpg_strikes++; }   /* (a new root strikes the gate) */

static void effective(void)                     /* (built aside, then each value written once: the interrupts
                                                 * read pe at any moment and must never see a knob flicker) */
{
    int16_t v[P_COUNT];
    uint32_t i;
    for (i = 0; i < P_COUNT; i++) v[i] = st.p[i];
    if (seq_play && st.trig >> seq_i & 1u)
        for (i = 0; i < 7u; i++)
            if (st.lock[seq_i][i] != NOLOCK && ENC_PARAM[i] != P_NONE) v[ENC_PARAM[i]] = st.lock[seq_i][i];
    v[P_SUN] = (int16_t)(v[P_CHAR] > 50 ? (v[P_CHAR] - 50) * 2 : 0);   /* KNOB 1: right, heaven */
    v[P_MARS] = (int16_t)(v[P_CHAR] < 50 ? (50 - v[P_CHAR]) * 2 : 0);  /* left, hell */
    for (i = 0; i < P_COUNT; i++) if (pe[i] != v[i]) pe[i] = v[i];
}

static void fire(uint32_t i)                    /* a step is reached */
{
    if (st.trig >> i & 1u && st.note[i]) set_root(st.note[i]);
}

static uint32_t show_on;                        /* the showcase is playing (below) */
static void show_stop(void);

static void keys(void)
{
    static uint32_t prev;
    uint32_t now = q_in.notes, down = (now & ~prev) | (q_note_edges() & ~prev), up = prev & ~now, k;
    prev = now;
    if (show_on && down) { show_stop(); return; }   /* (a key stops the showcase, and is only that) */
    for (k = 0; k < 27u; k++) {
        int32_t n = 53 + (int32_t)k, w = white_of[k];   /* key 0 = F3 */
        if (!(down >> k & 1u)) continue;
        if (step_mode && held_step >= 0) {      /* holding a step: this key is its root */
            st.note[held_step] = (uint8_t)n;
            st.trig |= (uint16_t)(1u << held_step);
            held_used = 1;
            dirty = 1;
            set_root(n);
            if (armed) { armed = 0; held_step = -1; }   /* (the one it waited for) */
        } else if (step_mode && w >= 0) { held_step = w; held_key = k; held_used = 0; held_t = q_ms; }
        else {
            set_root(n);
            if (rec_on && seq_play) {           /* live record: onto the step playing */
                st.note[seq_i] = (uint8_t)n;
                st.trig |= (uint16_t)(1u << seq_i);
                dirty = 1;
            }
        }
    }
    if (held_step >= 0 && !armed && up >> held_key & 1u) {
        if (!held_used && q_ms - held_t >= 600u) { armed = 1; held_keys = now; return; }   /* held long: it waits
                                                                      * for a key (its own too) */
        if (!held_used) {                       /* a tap: the trig on / off */
            st.trig ^= (uint16_t)(1u << held_step);
            if (st.trig >> held_step & 1u && !st.note[held_step]) st.note[held_step] = (uint8_t)root_note;
            dirty = 1;
        }
        held_step = -1;
    }
    held_keys = now;
}

/* the pages, Elektron-style: HOME the cosmos; a page's button opens it, a tap latching it (tapped again, or HOME:
 * back), held showing it only while held */
enum { PG_HOME, PG_FX };
static uint32_t page, page_t, page_was;
static uint32_t knob_param(uint32_t e)          /* what encoder e turns now (P_NONE: nothing) */
{
    return shift ? SHIFT_PARAM[e] : page == PG_FX ? FX_PARAM[e] : ENC_PARAM[e];
}

/* the knobs' acceleration, Elektron-style: turned slowly a detent is one step (precise), turned fast more, up to
 * ENC_ACC_MAX steps, as fast as it turns (about forty quick detents cross 0..100). The speed has momentum: a pause
 * (a regrip between flicks) fades it, halving every ENC_FADE_MS, so quick flicks keep it and a rest of a second or
 * so loses it; a turn back starts again at one. Not for the short ranges (step length, pattern length), where a
 * detent is a choice. The speed is the detents' own (q_enc_ms: when they came), not this loop's: a pass takes
 * 15 .. 115 ms, the screen's, and its batches of detents would make the same turn 1x on one pass and 3x the next. */
#define ENC_IDLE_MS 120u                        /* longer between detents: a pause, the speed fades */
#define ENC_FADE_MS 250                         /* in a pause the speed halves every this many ms */
#define ENC_SLOW 10                             /* detents a second: up to here, one step each */
#define ENC_FAST 50                             /*                   from here, ENC_ACC_MAX steps each */
#define ENC_ACC_MAX 3
static uint32_t enc_last[7], enc_way[7];        /* the last detent's time, its way (0: not turned yet, 1 -, 2 +) */
static int32_t enc_speed[7], enc_frac[7];       /* detents a second, smoothed; the steps carried over (Q8) */

static int32_t enc_mult(int32_t speed)          /* Q8 steps a detent at this speed */
{
    return 256 + clampi(speed - ENC_SLOW, 0, ENC_FAST - ENC_SLOW) * (ENC_ACC_MAX - 1) * 256 / (ENC_FAST - ENC_SLOW);
}

static void enc_gap(uint32_t e, int32_t gap)    /* a detent, gap ms after the one before (0: unknown) */
{
    int32_t h = gap / ENC_FADE_MS;
    if (gap <= 0) return;
    if (gap > (int32_t)ENC_IDLE_MS)             /* after a pause: what is left (halved each ENC_FADE_MS) */
        enc_speed[e] = h > 15 ? 0 : (enc_speed[e] >> h) * (2 * ENC_FADE_MS - gap % ENC_FADE_MS) / (2 * ENC_FADE_MS);
    else enc_speed[e] += (1000 / gap - enc_speed[e]) / 2;   /* turning: smoothed over the last few detents */
}

static int32_t enc_accel(uint32_t e, int32_t d)  /* the detents taken, one at a time, as if seen as they came */
{
    uint32_t first, t = q_enc_ms(e, &first), way = d > 0 ? 2u : 1u;
    int32_t n = d < 0 ? -d : d, k;
    if (enc_way[e] != way) {                    /* turned back (or the first turn): from one */
        enc_speed[e] = 0;
        enc_frac[e] = 0;
    } else enc_gap(e, (int32_t)(first - enc_last[e]));
    enc_frac[e] += enc_mult(enc_speed[e]);
    for (k = 1; k < n; k++) {                   /* the rest, evenly between the first and the last */
        enc_gap(e, (int32_t)(t - first) / (n - 1));
        enc_frac[e] += enc_mult(enc_speed[e]);
    }
    enc_last[e] = t;
    enc_way[e] = way;
    n = enc_frac[e] >> 8;
    enc_frac[e] &= 255;
    return d < 0 ? -n : n;
}

/* where encoder e's dial is now, as the screen draws it (and the Mac's panel its knob): 0 .. 32767, -1 with nothing
 * on it; *lock: a step's lock it shows, the one held or the one playing (on HOME, not shifted) */
static int32_t knob_level(uint32_t e, int *lock)
{
    uint32_t p = knob_param(e);
    int32_t v;
    *lock = 0;
    if (p == P_NONE) return -1;
    v = st.p[p];
    if (!shift && page == PG_HOME) {
        if (step_mode && held_step >= 0) {
            if (st.lock[held_step][e] != NOLOCK) { v = st.lock[held_step][e]; *lock = 1; }
        } else if (pe[p] != st.p[p]) { v = pe[p]; *lock = 1; }
    }
    return (v - PARAM[p].lo) * 32767 / (PARAM[p].hi - PARAM[p].lo);
}

static void knob(uint32_t e, int32_t d)
{
    uint32_t p = knob_param(e);
    if (p == P_NONE) return;
    if (PARAM[p].hi - PARAM[p].lo > 16) d = enc_accel(e, d);
    if (step_mode && held_step >= 0 && !shift && page == PG_HOME) {   /* a lock on the step held */
        int32_t cur = st.lock[held_step][e] == NOLOCK ? st.p[p] : st.lock[held_step][e];
        st.lock[held_step][e] = (int8_t)clampi(cur + d, 0, 100);
        held_used = 1;
    } else st.p[p] = (int16_t)clampi(st.p[p] + d, PARAM[p].lo, PARAM[p].hi);
    dirty = 1;
    last_enc = (int32_t)p;
    enc_t = q_ms;
}

static void buttons(void)
{
    uint32_t rel, b = q_button_edges(&rel), i;
    if (show_on && b) { show_stop(); return; }  /* (a button stops the showcase, and is only that) */
    if (b & (1u << BTN_OCT_DOWN | 1u << BTN_OCT_UP)) {
        st.oct = (int8_t)clampi(st.oct + (b & 1u << BTN_OCT_UP ? 1 : -1), -3, 3);
        dirty = 1;
    }
    if (b & 1u << BTN_PLAY) {
        seq_play = !seq_play;
        if (seq_play) { seq_i = 0; seq_t = q_ms; fire(0); }
    }
    if (b & 1u << BTN_REC) rec_on = !rec_on;
    if (b & 1u << BTN_SEQ) { step_mode = !step_mode; held_step = -1; armed = 0; }
    if (b & 1u << BTN_FX && held_step >= 0) {  /* holding a step: clear its locks */
        for (i = 0; i < 7u; i++) st.lock[held_step][i] = NOLOCK;
        held_used = 1;
        dirty = 1;
    } else if (b & 1u << BTN_FX) {              /* the FX page */
        page_was = page == PG_FX;
        page = PG_FX; page_t = q_ms; full_redraw = 1;
    }
    if (rel & 1u << BTN_FX && page_t) {         /* let go: held, it was momentary; tapped again, back home */
        if (q_ms - page_t >= 400u || page_was) { page = PG_HOME; full_redraw = 1; }
        page_t = 0;
    }
    if (b & 1u << BTN_SAVE && q_settings_save(&st, sizeof st) == 0) dirty = 0;
    if (b & 1u << BTN_HOME) { full_redraw = 1; page = PG_HOME; }
    settle_held = held_btn(BTN_EDIT) && held_step < 0;   /* (EDIT held: settle) */
    shift = held_btn(BTN_SEL);
}

static void midi_in(void)
{
    uint8_t s, d1, d2;
    while (q_midi_get(&s, &d1, &d2)) {
        if ((s & 0xF0u) == 0x90u && d2) {
            set_root(d1);
            if (rec_on && seq_play) { st.note[seq_i] = d1; st.trig |= (uint16_t)(1u << seq_i); dirty = 1; }
        } else if (s == 0xBFu) {                /* the probe (channel 16) */
            static uint32_t lo;
            if (d1 >= 30u && d1 < 30u + P_COUNT) {   /* CC 30+i: control i, 0..127 across its range */
                uint32_t p = d1 - 30u;
                st.p[p] = (int16_t)(PARAM[p].lo + (PARAM[p].hi - PARAM[p].lo) * d2 / 127);
            } else if (d1 == 100u) lo = d2;     /* CC 100, 101: the bypass mask, low then high 7 bits */
            else if (d1 == 101u) { dbg = lo | (uint32_t)d2 << 7; q_premaster((int)(dbg >> 13 & 1u)); }
            else if (d1 == 103u) {              /* CC 103: 1 play / stop, 2 stop, 3 the defaults, 4 REC */
                if (d2 == 1u) { seq_play = !seq_play; if (seq_play) { seq_i = 0; seq_t = q_ms; fire(0); } }
                else if (d2 == 2u) seq_play = 0;
                else if (d2 == 3u) { uint32_t i; for (i = 0; i < P_COUNT; i++) st.p[i] = PARAM[i].def; }
                else if (d2 == 4u) rec_on = !rec_on;
                else if (d2 == 5u) agit_off = 1;
                else if (d2 == 6u) agit_off = 0;
            } else if (d1 == 105u) {            /* CC 105: the USB audio's counters, 14 bits each on CC 70.. (hi, lo) */
                uint32_t v[10], k;
                q_usb_stats(v);
                for (k = 0; k < 10u; k++) {
                    q_midi_send(0xBF, 70u + 2u * k, v[k] >> 7 & 127u);
                    q_midi_send(0xBF, 71u + 2u * k, v[k] & 127u);
                }
            } else if (d1 == 104u) {            /* CC 104: report: agitation (CC 117), the mask (CC 115, 116) */
                q_midi_send(0xBF, 117, (uint32_t)agit >> 8 & 127u);
                q_midi_send(0xBF, 115, dbg & 127u);
                q_midi_send(0xBF, 116, dbg >> 7 & 127u);
            }
        } else if ((s & 0xF0u) == 0xB0u && d1 >= 20u && d1 <= 26u) {   /* CC 20..26: the planets */
            uint32_t p = ENC_PARAM[d1 - 20u];
            if (p != P_NONE) {
                st.p[p] = (int16_t)(PARAM[p].lo + (PARAM[p].hi - PARAM[p].lo) * d2 / 127);
                last_enc = (int32_t)p; enc_t = q_ms;
            }
        }
    }
}

static uint32_t step_ms(void)                   /* a step's length; 0 = WAIT (until the drone settles) */
{
    return 15000u * DIV_16THS[st.p[P_DIV]] / (uint32_t)st.p[P_BPM];   /* a sixteenth = 60000 / bpm / 4 */
}

static void sequencer(void)
{
    uint32_t len = (uint32_t)st.p[P_LEN], ms = step_ms();
    int adv;
    if (!seq_play) return;
    if (seq_i >= len) seq_i = 0;
    if (!ms) adv = q_ms - seq_t >= 250u && agit < st.p[P_THRESH] * 82;
    else adv = q_ms - seq_t >= ms;
    if (!adv) return;
    seq_t = ms && q_ms - seq_t < 2u * ms ? seq_t + ms : q_ms;   /* (no drift; no catching up after a stall) */
    seq_i = (seq_i + 1u) % len;
    fire(seq_i);
}

/* ---------------------------------------------------------------- the screen: a small cosmos --- */
#define C_EMERALD QRGB(40, 200, 120)
#define C_GOLD QRGB(255, 196, 60)
#define C_WGOLD QRGB(255, 236, 180)
#define C_RED QRGB(170, 16, 24)
#define C_EMBER QRGB(255, 70, 30)

static void qg_circle(int32_t cx, int32_t cy, int32_t r, uint16_t c)
{
    int32_t x = r, y = 0, err = 1 - r;
    while (x >= y) {
        qg_pset(cx + x, cy + y, c); qg_pset(cx - x, cy + y, c); qg_pset(cx + x, cy - y, c); qg_pset(cx - x, cy - y, c);
        qg_pset(cx + y, cy + x, c); qg_pset(cx - y, cy + x, c); qg_pset(cx + y, cy - x, c); qg_pset(cx - y, cy - x, c);
        y++;
        if (err < 0) err += 2 * y + 1;
        else { x--; err += 2 * (y - x) + 1; }
    }
}

/* the seven planets' glyphs, drawn in a ~20 px box around (cx, cy) */
static void planet(uint32_t which, int32_t cx, int32_t cy, uint16_t c)
{
    switch (which) {
    case P_SUN: qg_circle(cx, cy, 8, c); qg_rect(cx - 1, cy - 1, 3, 3, c); break;
    case P_GATE:                                 /* a pluck: struck, then dying away */
        qg_line(cx - 8, cy + 7, cx + 9, cy + 7, c);
        qg_line(cx - 7, cy + 7, cx - 5, cy - 8, c); qg_line(cx - 5, cy - 8, cx - 2, cy - 1, c);
        qg_line(cx - 2, cy - 1, cx + 2, cy + 3, c); qg_line(cx + 2, cy + 3, cx + 6, cy + 5, c);
        qg_line(cx + 6, cy + 5, cx + 9, cy + 6, c);
        break;
    case P_MOON: {                               /* a crescent: the outer circle where the inner one is not */
        int32_t x, y;
        for (y = -8; y <= 8; y++)
            for (x = -8; x <= 8; x++) {
                int32_t o = x * x + y * y, in = (x - 4) * (x - 4) + y * y;
                if (o <= 64 && o >= 42 && in > 49) qg_pset(cx + x, cy + y, c);
                else if (o <= 64 && in > 49 && in <= 64) qg_pset(cx + x, cy + y, c);
            }
        break;
    }
    case P_MERCURY:
        qg_circle(cx, cy - 1, 4, c);
        qg_line(cx - 4, cy - 9, cx - 2, cy - 6, c); qg_line(cx + 4, cy - 9, cx + 2, cy - 6, c);
        qg_line(cx, cy + 3, cx, cy + 10, c); qg_line(cx - 3, cy + 7, cx + 3, cy + 7, c);
        break;
    case P_VENUS:
        qg_circle(cx, cy - 4, 5, c);
        qg_line(cx, cy + 1, cx, cy + 10, c); qg_line(cx - 4, cy + 6, cx + 4, cy + 6, c);
        break;
    case P_MARS:
        qg_circle(cx - 2, cy + 3, 5, c);
        qg_line(cx + 2, cy - 1, cx + 8, cy - 7, c);
        qg_line(cx + 8, cy - 7, cx + 3, cy - 7, c); qg_line(cx + 8, cy - 7, cx + 8, cy - 2, c);
        break;
    case P_JUPITER:
        qg_line(cx - 6, cy - 6, cx - 2, cy - 9, c); qg_line(cx - 2, cy - 9, cx, cy - 6, c);
        qg_line(cx, cy - 6, cx - 6, cy + 4, c); qg_line(cx - 7, cy + 4, cx + 7, cy + 4, c);
        qg_line(cx + 3, cy - 8, cx + 3, cy + 10, c);
        break;
    case P_SATURN:
        qg_line(cx - 3, cy - 10, cx - 3, cy + 8, c); qg_line(cx - 7, cy - 6, cx + 1, cy - 6, c);
        qg_line(cx - 3, cy - 1, cx + 3, cy - 3, c); qg_line(cx + 3, cy - 3, cx + 5, cy + 2, c);
        qg_line(cx + 5, cy + 2, cx + 2, cy + 7, c); qg_line(cx + 2, cy + 7, cx + 5, cy + 10, c);
        break;
    default:                                     /* the shifted and sequencer controls: a small hexagram */
        qg_line(cx, cy - 8, cx + 7, cy + 4, c); qg_line(cx + 7, cy + 4, cx - 7, cy + 4, c); qg_line(cx - 7, cy + 4, cx, cy - 8, c);
        qg_line(cx, cy + 8, cx + 7, cy - 4, c); qg_line(cx + 7, cy - 4, cx - 7, cy - 4, c); qg_line(cx - 7, cy - 4, cx, cy + 8, c);
        break;
    }
}

static void hexagram(int32_t cx, int32_t cy, int32_t r)   /* fire above, water below, interlocked */
{
    int32_t h = r * 7 / 8, hw = r * 3 / 4;
    qg_line(cx, cy - r, cx + hw, cy + h / 2, C_GOLD); qg_line(cx + hw, cy + h / 2, cx - hw, cy + h / 2, C_GOLD);
    qg_line(cx - hw, cy + h / 2, cx, cy - r, C_GOLD);
    qg_line(cx, cy + r, cx + hw, cy - h / 2, C_RED); qg_line(cx + hw, cy - h / 2, cx - hw, cy - h / 2, C_RED);
    qg_line(cx - hw, cy - h / 2, cx, cy + r, C_RED);
}

static void value_text(char *s, uint32_t p, int32_t v)
{
    if (p == P_CHAR) {
        if (v == 50) put(s, "PLAIN");
        else { s = put(s, v > 50 ? "HEAVEN " : "HELL "); put_num(s, (uint32_t)(v > 50 ? v - 50 : 50 - v) * 2u, 1); }
    } else if (p == P_DIV) put(s, DIV_NAME[v]);
    else {
        s = put_num(s, (uint32_t)v, 1);
        if (p == P_SATURN || p == P_MERCURY) {   /* and how long a glide takes */
            uint32_t ms = TAU[p == P_MERCURY ? v : st.p[P_MERCURY]] * 65536u / W0[p == P_SATURN ? v : st.p[P_SATURN]];
            s = put(s, "  ");
            if (ms < 1000u) { s = put_num(s, ms, 1); put(s, " ms"); }
            else if (ms < 10000u) { s = put_num(s, ms / 1000u, 1); s = put(s, "."); s = put_num(s, ms / 100u % 10u, 1); put(s, " s"); }
            else if (ms < 60000u) { s = put_num(s, ms / 1000u, 1); put(s, " s"); }
            else { s = put_num(s, ms / 60000u, 1); s = put(s, ":"); put_num(s, ms / 1000u % 60u, 2); }
        }
    }
}

/* rows 4..70: the seven knobs as dials, a glyph in each and an arc for where it is (270 degrees, from the
 * bottom left). Shifted: the shifted controls, by their tags. Holding a step: its locks in gold, the rest dim. */
static int32_t sph_root_rgb[3];
static uint16_t sph_rgb(int32_t r, int32_t g, int32_t b, int32_t l);
/* the dials sit as the FM-1's knobs do, in six columns:        SELECT  KNOB 1  KNOB 2  KNOB 3  KNOB 4
 *                                                       PRESETS  ALGORITHM                       */
static const uint8_t DIAL_COL[7] = {1, 1, 2, 3, 4, 5, 0}, DIAL_ROW[7] = {0, 1, 0, 0, 0, 0, 1};
static void draw_dials(void)                    /* rows 4..34, and on the left 40..70 */
{
    uint32_t j;
    for (j = 0; j < 7u; j++) {
        int32_t cx = 20 + DIAL_COL[j] * 40, cy = 19 + DIAL_ROW[j] * 36, a, n, lit;
        uint32_t p = shift ? SHIFT_PARAM[j] : ENC_PARAM[j];
        int32_t v, locked = 0;
        if (p == P_NONE) { qg_circle(cx, cy, 14, QRGB(20, 34, 28)); continue; }   /* (nothing on this knob) */
        v = st.p[p];
        uint16_t c = sph_rgb(sph_root_rgb[0], sph_root_rgb[1], sph_root_rgb[2], 32767);   /* (the screen's colour,
                                                                      * by KNOB 1) */
        uint16_t cd = sph_rgb(sph_root_rgb[0], sph_root_rgb[1], sph_root_rgb[2], 7200);
        if (step_mode && held_step >= 0 && !shift) {
            locked = st.lock[held_step][j] != NOLOCK;
            if (locked) { v = st.lock[held_step][j]; c = C_GOLD; cd = QRGB(70, 54, 16); }
            else c = sph_rgb(sph_root_rgb[0], sph_root_rgb[1], sph_root_rgb[2], 18000);
        } else if (!shift && pe[p] != st.p[p]) { v = pe[p]; c = C_GOLD; }   /* a lock playing */
        if (shift) c = C_WGOLD;
        lit = (v - PARAM[p].lo) * 48 / (PARAM[p].hi - PARAM[p].lo);
        {
            int32_t px = 0, py = 0, qx = 0, qy = 0;
            for (n = 0; n <= 48; n++) {         /* 270 degrees in 48 segments, two pixels thick */
                uint32_t ph;
                int32_t x, y, x2, y2;
                a = -135 + n * 270 / 48;
                ph = (uint32_t)(a < 0 ? a + 360 : a) * 11930465u;   /* degrees -> a cycle of 2^32 */
                x = cx + (sine(ph) * 15 >> 15);
                y = cy - (sine(ph + 0x40000000u) * 15 >> 15);
                x2 = cx + (sine(ph) * 13 >> 15);
                y2 = cy - (sine(ph + 0x40000000u) * 13 >> 15);
                if (n) {
                    uint16_t col = n <= lit ? c : cd;
                    qg_line(px, py, x, y, col);
                    qg_line(qx, qy, x2, y2, col);
                }
                if (n == lit) qg_rect(x - 1, y - 1, 3, 3, QC_WHITE);   /* the pointer */
                px = x; py = y; qx = x2; qy = y2;
            }
        }
        if (shift) text_c(cx - 17, cy - 8, 34, &QFONT_S, PARAM[p].tag, c);
        else if (p == P_CHAR) planet(v > 50 ? P_SUN : v < 50 ? P_MARS : P_CHAR, cx, cy, c);   /* sun, Mars, or both */
        else planet(p, cx, cy, c);
        if ((int32_t)p == last_enc && q_ms - enc_t < 1500u) qg_rect(cx - 12, cy + 17, 24, 2, c);   /* the one turned */
    }
}

static void draw_overlay(void)                  /* rows 40..74 right of the low dials, for 1.5 s after a knob moves:
                                                 * its name and value (under it when both do not fit on a line) */
{
    char v[16];
    uint32_t p = (uint32_t)last_enc;
    int32_t val = st.p[p];
    uint16_t c = sph_rgb(sph_root_rgb[0], sph_root_rgb[1], sph_root_rgb[2], 32767);   /* (the screen's colour) */
    if (step_mode && held_step >= 0 && !shift) {
        uint32_t j;
        for (j = 0; j < 7u; j++)
            if (ENC_PARAM[j] == p && st.lock[held_step][j] != NOLOCK) { val = st.lock[held_step][j]; c = C_GOLD; }
    }
    qg_text(88, 41, &QFONT_S, PARAM[p].name, c);
    value_text(v, p, val);
    qg_text(232 - qg_text_w(&QFONT_S, v), 88 + qg_text_w(&QFONT_S, PARAM[p].name) + 8 > 232 - qg_text_w(&QFONT_S, v)
            ? 58 : 41, &QFONT_S, v, QC_WHITE);
}

/* rows 200..216: the 16 steps (gold a trig, white the step playing, emerald the step held, dim past the length) */
static void draw_steps(void)
{
    uint32_t i, len = (uint32_t)st.p[P_LEN];
    for (i = 0; i < STEPS; i++) {
        int32_t x = (int32_t)i * 15;
        int32_t h = i % 4u ? 10 : 12, on = st.trig >> i & 1u;
        int32_t l = on ? 18000 : 4600;           /* in the screen's colour: a trig .. empty, dimmer past the end */
        if (i >= len) l = on ? 7000 : 1800;
        if (seq_play && i == seq_i) l = 32767;   /* the step playing: full */
        qg_rect(x + 1, 202, 13, h, sph_rgb(sph_root_rgb[0], sph_root_rgb[1], sph_root_rgb[2], l));
        if ((int32_t)i == held_step) {           /* the step held: a frame round it */
            uint16_t c = sph_rgb(sph_root_rgb[0], sph_root_rgb[1], sph_root_rgb[2], 32767);
            qg_rect(x, 201, 15, 1, c); qg_rect(x, 202 + h, 15, 1, c);
            qg_rect(x, 201, 1, h + 2, c); qg_rect(x + 14, 201, 1, h + 2, c);
        }
        {                                       /* a step's locks: a dot under it */
            uint32_t j, any = 0;
            for (j = 0; j < 7u; j++) any |= st.lock[i][j] != NOLOCK;
            if (any && st.trig >> i & 1u) qg_rect(x + 6, 215, 3, 2, C_GOLD);
        }
    }
}

static void draw_status(void)                   /* rows 222..239 */
{
    char s[40], *q = s;
    *q = 0;
    if (step_mode && held_step >= 0) {
        q = put(q, "STEP ");
        q = put_num(q, (uint32_t)held_step + 1u, 1);
        q = put(q, "  ");
        if (st.trig >> held_step & 1u) q = put_note(q, st.note[held_step] + 12 * st.oct);
        else q = put(q, "OFF");
    } else {
        q = put(q, seq_play ? "PLAY " : "STOP ");
        q = put_num(q, (uint32_t)st.p[P_BPM], 1);
        q = put(q, " ");
        q = put(q, DIV_NAME[st.p[P_DIV]]);
        if (rec_on) q = put(q, "  REC");
        if (step_mode) q = put(q, "  STEP");
        if (settle_held) q = put(q, "  SETTLE");
    }
    if (held_btn(BTN_HOME)) {
        q = put(s, "AUDIO "); q = put_num(q, audio_us_max, 1); q = put(q, " US  ");
        q = put_num(q, ui_fps, 1); put(q, " FPS");
    }
    if (held_btn(BTN_GLO)) { q = put(s, "QUOD "); put(q, Q_VERSION); }   /* (GLO held: the version) */
    qg_text(6, 222, &QFONT_S, s, rec_on && !step_mode ? QRGB(255, 59, 48)
                                : sph_rgb(sph_root_rgb[0], sph_root_rgb[1], sph_root_rgb[2], 32767));   /* (the screen's colour) */
    if (dirty) qg_text(228, 222, &QFONT_S, "*", QC_GREY);
}

/* rows 60..198: the cosmos, a planetoid. Each voice is a point of light circling a sphere at the latitude of its
 * live pitch (above the root north, below it south, the root on the equator), white in heaven, red in hell, with
 * a fading trail; together the trails draw the sphere, as big as the spread. It turns on a tilted axis, each
 * voice at its pitch's ratio to the root, all of it as fast as the phasing; the far side is dim. */
#define SPH_X 120
#define SPH_Y 129
#define SPH_R (sph_r8 >> 8)                     /* its radius: SPREAD, 0 .. 100 -> 16 .. 66 px (smoothed) */
static int32_t sph_r8 = 46 << 8;
/* A new root makes a new sphere: higher to the right, lower to the left, and it swallows the old one like a
 * black hole. The view widens to hold both. The voices go one by one, the lightest first, across the glide's time
 * (as the readout has it, at least 0.8 s), each in three stages: it sinks to the old one's equator, it is drawn
 * off along the equator's plane in a stream that spirals in ever faster to the new one's equator, then it rises
 * to its latitude there. Once all have arrived the new one slides to the middle. World x, Q8 px: */
#define NTRAV (NOSC + NSCR + NBELL)             /* each light crosses on its own: the voices, the screamers, the
                                                 * bell's partials */
static int32_t sph_wnew8, sph_wold8, sph_cam8, sph_zoom = 32767, sph_a8[NTRAV], sph_moving;
static uint8_t sph_ph[NTRAV];                   /* 0 on its old one, 1 crossing, 2 home */
static uint8_t sph_cap[NTRAV], sph_in[NTRAV];   /* crossing: drawn off yet, captured yet */
static uint32_t sph_go[NTRAV], sph_trav;        /* when each begins to go; how long a crossing takes */
static int32_t sph_v0x[NTRAV], sph_v0z[NTRAV], sph_rho0[NTRAV];   /* where it was drawn off, from the new one's
                                                 * centre, px Q4, and how far */
static uint32_t sph_rng = 2891336453u;          /* the screen's own dice (the sound's are the audio's alone) */
static int32_t sph_rnd(void) { sph_rng = sph_rng * 1664525u + 1013904223u; return (int32_t)(sph_rng >> 16); }
#define SPH_SWIRL 0x80000000u                   /* the stream swings half round the new one as it falls in */
static int32_t sph_cx, sph_rad;                 /* the sphere being drawn: its centre x, its radius on screen */
static int32_t sph_root_rgb[3] = {242, 141, 130};   /* (the screen's colour: by KNOB 1, see sph_points) */
#define SPH_TRAIL 20                            /* points a voice's trail, 6 degrees apart */
#define SPH_N (NTRAV * (2 * SPH_TRAIL + 12))   /* the most there can be: crossing, each its afterimage, its tail,
                                                 * its twin's */
static uint32_t sph_lon[NTRAV], sph_tw[NOSC], sph_bell, sph_init;   /* (each light's longitude; the bell's ring) */
static const int32_t SPH_TC = 30790, SPH_TS = 11207;   /* the axis tilted 20 degrees toward us: cos, sin (Q15) */
static int32_t isqrt(uint32_t v)
{
    uint32_t r = 0, b = 1u << 30;
    while (b > v) b >>= 2;
    while (b) { if (v >= r + b) { v -= r + b; r = (r >> 1) + b; } else r >>= 1; b >>= 2; }
    return (int32_t)r;
}
static uint32_t atan2p(int32_t y, int32_t x)    /* the angle of (x, y), a turn = 2^32 (CORDIC) */
{
    static const uint32_t AT[16] = {536870912u, 316933406u, 167458907u, 85004756u, 42667331u, 21354465u,
                                    10679838u, 5340245u, 2670163u, 1335087u, 667544u, 333772u, 166886u, 83443u,
                                    41722u, 20861u};
    uint32_t a = 0, i;
    if (x < 0) { x = -x; y = -y; a = 0x80000000u; }
    x <<= 8; y <<= 8;                           /* (callers' values are small) */
    for (i = 0; i < 16u; i++) {
        int32_t nx;
        if (y > 0) { nx = x + (y >> i); y -= x >> i; x = nx; a += AT[i]; }
        else { nx = x - (y >> i); y += x >> i; x = nx; a -= AT[i]; }
    }
    return a;
}
typedef struct { int16_t x, y; uint16_t c; uint8_t head, front; } sph_pt;
static sph_pt sph_p[SPH_N];
static uint32_t sph_nb, sph_nf;                 /* the far side's from the start, the near side's from the end (so
                                                 * they are drawn far first, in one pass) */
static sph_pt *sph_slot(int front) { return front ? &sph_p[SPH_N - 1u - sph_nf++] : &sph_p[sph_nb++]; }
static uint16_t sph_rgb(int32_t r, int32_t g, int32_t b, int32_t l)   /* l: Q15 */
{
    return QRGB((uint32_t)((r * l) >> 15), (uint32_t)((g * l) >> 15), (uint32_t)((b * l) >> 15));
}
/* a light and its trail: at latitude lat (degrees), longitude lon, colour rgb at brightness lev (Q15) */
static void sph_trail(int32_t lat, uint32_t lon0, int32_t r, int32_t g, int32_t bl, int32_t lev, int32_t n)
{
    int32_t sl = sine((uint32_t)lat * 11930465u), cl = sine((uint32_t)lat * 11930465u + 0x40000000u), i;
    if (lev < 600) return;                      /* (silent: not there) */
    for (i = 0; i < n && sph_nb + sph_nf < SPH_N; i++) {
        uint32_t lon = lon0 - (uint32_t)i * 71582788u;   /* 6 degrees back each */
        int32_t x = (cl * sine(lon + 0x40000000u)) >> 15, z = (cl * sine(lon)) >> 15;
        int32_t y2 = (sl * SPH_TC - z * SPH_TS) >> 15, z2 = (sl * SPH_TS + z * SPH_TC) >> 15;
        int32_t l = (32767 - i * (32767 / n)) * (19660 + ((z2 + 32767) * 13107 >> 16)) >> 15;
        sph_pt *q = sph_slot(z2 >= 0);
        if (i) l = (l * 3) >> 2;                /* (the trail a little dimmer than the light) */
        l = (l * lev) >> 15;
        q->x = (int16_t)(sph_cx + ((x * sph_rad) >> 15));
        q->y = (int16_t)(SPH_Y - ((y2 * sph_rad) >> 15));
        q->c = sph_rgb(r, g, bl, z2 >= 0 ? l : l >> 1);
        q->head = i == 0;
        q->front = z2 >= 0;
    }
}
/* a voice in the stream, e ms into its crossing: on the new one's equatorial plane, px Q4 from its centre */
static void sph_stream_at(int32_t k, int32_t sf, int32_t *x, int32_t *z);
static int32_t sph_fall(uint32_t e)             /* how far it has fallen, e ms into its crossing, Q15 */
{
    uint32_t t1 = sph_trav / 5u, t2 = sph_trav * 3u / 5u;
    int32_t w = e <= t1 ? 0 : e >= t1 + t2 ? 32767 : (int32_t)((e - t1) * 32767u / (t2 | 1u));
    return (w * w) >> 15;                        /* it falls ever faster */
}
static void sph_stream(int32_t k, uint32_t e, int32_t *x, int32_t *z) { sph_stream_at(k, sph_fall(e), x, z); }
static void sph_stream_at(int32_t k, int32_t sf, int32_t *x, int32_t *z)
{
    int32_t sp = (sf * sf) >> 15;                /* nearly straight in, spinning up only near the new one */
    int32_t r16 = SPH_R * 16, rho0 = sph_rho0[k];
    int32_t mag = rho0 + (int32_t)(((int64_t)(r16 - rho0) * sf) >> 15);
    int32_t sc = mag * 4096 / rho0;             /* (Q12: the one divide) */
    uint32_t th = (uint32_t)(((uint64_t)SPH_SWIRL * (uint32_t)sp) >> 15);
    int32_t c = sine(th + 0x40000000u), sn = sine(th);
    *x = (((sph_v0x[k] * c - sph_v0z[k] * sn) >> 15) * sc) >> 12;
    *z = (((sph_v0x[k] * sn + sph_v0z[k] * c) >> 15) * sc) >> 12;
}
static void sph_dot(int32_t x, int32_t z, int32_t r, int32_t g, int32_t bl, int32_t l, int head)   /* on the plane */
{
    int32_t y2 = (-z * SPH_TS) >> 15, z2 = (z * SPH_TC) >> 15, r16 = SPH_R * 16;
    int32_t d = clampi(z2 * 32767 / r16, -32767, 32767);
    sph_pt *q;
    if (sph_nb + sph_nf >= SPH_N) return;
    q = sph_slot(z2 >= 0);
    l = (l * (19660 + ((d + 32767) * 13107 >> 16))) >> 15;
    q->x = (int16_t)(sph_cx + ((((int64_t)x * sph_zoom) >> 15) >> 4));
    q->y = (int16_t)(SPH_Y - ((((int64_t)y2 * sph_zoom) >> 15) >> 4));
    q->c = sph_rgb(r, g, bl, z2 >= 0 ? l : l >> 1);
    q->head = (uint8_t)head;
    q->front = z2 >= 0;
}
/* a voice's colour: heaven white .. hell red, by where it is (a screamer: red) */
static void sph_voice_rgb(int32_t k, int32_t mars, int32_t *r, int32_t *g, int32_t *bl)
{
    int32_t side = side15[k];
    side += (int32_t)(((int64_t)(32767 - side) * (pe[P_SUN] * 327)) >> 15);   /* (heaven redeems: white) */
    side = (int32_t)(((int64_t)side * (32767 - (int32_t)(((int64_t)clampi(2 * side - 32767, 0, 32767) * mars) >> 15)))
                     >> 15);
    *r = 230 + ((25 * side) >> 15); *g = 30 + ((222 * side) >> 15); *bl = 20 + ((220 * side) >> 15);
}
static int32_t sph_screen_x(int32_t w8) { return SPH_X + (int32_t)(((int64_t)(w8 - sph_cam8) * sph_zoom) >> 23); }
/* where a light is drawn, crossing or not: its centre (sph_cx) and its latitude now; 0 if the stream drew it */
static int sph_place(int32_t t, int32_t *lat, int32_t r, int32_t g, int32_t bl, int32_t lev, int32_t n,
                     uint32_t now, uint32_t t1, uint32_t t2, uint32_t t3)
{
    uint32_t e = now - sph_go[t];
    if (sph_ph[t] != 1) { sph_cx = sph_screen_x(sph_ph[t] == 0 ? sph_a8[t] : sph_wnew8); return 1; }
    if (e < t1) {                                /* sinking to the old one's equator */
        *lat = *lat * (int32_t)(t1 - e) / (int32_t)(t1 | 1u);
        sph_cx = sph_screen_x(sph_a8[t]);
        return 1;
    }
    if (e < t1 + t2) {                           /* drawn off: the stream */
        int32_t i, x, z;
        if (!sph_cap[t]) {                       /* (where it leaves: its place on the old equator) */
            sph_v0x[t] = ((sph_a8[t] - sph_wnew8) >> 4) + ((SPH_R * 16 * sine(sph_lon[t] + 0x40000000u)) >> 15);
            sph_v0z[t] = (SPH_R * 16 * sine(sph_lon[t])) >> 15;
            sph_rho0[t] = clampi(isqrt((uint32_t)(sph_v0x[t] * sph_v0x[t] + sph_v0z[t] * sph_v0z[t])), 64, 1 << 20);
            sph_cap[t] = 1;
        }
        sph_cx = sph_screen_x(sph_wnew8);
        if (lev < 600) return 0;
        {                                        /* the light, and behind it where it has been: a comet's tail
                                                 * spaced along the fall (not in time: a slow one would pile up);
                                                 * its twin a little behind it */
            int32_t sf = sph_fall(e), ds = 655;  /* (2 % of the fall a point) */
            sph_cx = sph_screen_x(sph_a8[t]);    /* the orbit it left, an afterimage fading as it falls away */
            sph_trail(0, sph_lon[t], r, g, bl, (int32_t)(((int64_t)lev * (32767 - sf)) >> 16), n);
            sph_cx = sph_screen_x(sph_wnew8);
            int32_t tl = t < NOSC ? (int32_t)(((int64_t)((lev * 3) >> 2) * clampi(pe[P_VENUS] * 32767 / 30, 0, 32767)) >> 15) : 0;
            for (i = 0; i < SPH_TRAIL && sf - i * ds >= 0; i++) {
                sph_stream_at(t, sf - i * ds, &x, &z);
                sph_dot(x, z, r, g, bl, ((32767 - i * (32767 / SPH_TRAIL)) * lev >> 15) * (i ? 3 : 4) >> 2, i == 0);
            }
            for (i = 0; tl >= 600 && i < 12 && sf - (i + 3) * ds >= 0; i++) {
                sph_stream_at(t, sf - (i + 3) * ds, &x, &z);
                sph_dot(x, z, r, g, bl, ((32767 - i * (32767 / 12)) * tl >> 15) * (i ? 3 : 4) >> 2, i == 0);
            }
        }
        return 0;
    }
    if (!sph_in[t]) {                            /* captured: rising to its latitude on the new one */
        int32_t x, z;
        sph_stream(t, t1 + t2, &x, &z);
        sph_lon[t] = atan2p(z, x);
        sph_in[t] = 1;
    }
    *lat = *lat * (int32_t)(e - t1 - t2) / (int32_t)(t3 | 1u);
    sph_cx = sph_screen_x(sph_wnew8);
    return 1;
}
static void sph_points(uint32_t dt)
{
    int32_t k, j, base = pos[ROOT], mars = pe[P_MARS] * 327, venus = pe[P_VENUS] * 655;
    uint32_t inc0 = freq_inc(0);
    static int32_t last_to, have_to;
    int32_t home = 0, r = 16 + pe[P_JUPITER] / 2, rd = r < 52 ? r : 52, zgoal;
    uint32_t now = q_ms, t1 = sph_trav / 5u, t2 = sph_trav * 3u / 5u, t3 = sph_trav - t1 - t2;
    sph_nb = sph_nf = 0;
    sph_r8 += ((r << 8) - sph_r8) >> 2;
    if (!sph_init) {                            /* spread round at the start: the golden angle */
        for (k = 0; k < NOSC; k++) { sph_lon[k] = (uint32_t)k * 0x9E3779B9u; sph_tw[k] = 0x80000000u; }
        for (j = 0; j < NSCR; j++) sph_lon[NOSC + j] = (uint32_t)j * 0x40000000u + 0x20000000u;
        for (j = 0; j < NBELL; j++) sph_lon[NOSC + NSCR + j] = (uint32_t)j * 613566757u;   /* (360 / 7 apart) */
        for (k = 0; k < NTRAV; k++) sph_ph[k] = 2;
        sph_init = 1;
    }
    if (!have_to) { last_to = to_c[ROOT]; have_to = 1; }
    if (to_c[ROOT] != last_to) {                 /* a new root: a new sphere beside the one it is now */
        int32_t d = (2 * rd + 14) << 8, j2, wprev = sph_wnew8;
        uint32_t T = TAU[pe[P_MERCURY]] * 65536u / W0[clampi(visc8 >> 8, 0, 100)];
        if (T < 800u) T = 800u;
        sph_wold8 = sph_wnew8;
        sph_wnew8 += to_c[ROOT] > last_to ? d : -d;
        last_to = to_c[ROOT];
        sph_moving = 1;
        sph_trav = T / 2u;
        t1 = sph_trav / 5u; t2 = sph_trav * 3u / 5u; t3 = sph_trav - t1 - t2;
        for (k = 0; k < NTRAV; k++) {
            if (sph_ph[k] == 1 && sph_cap[k] && !sph_in[k]) {   /* in the stream: it turns toward the new one */
                int32_t x, z;
                sph_stream(k, now - sph_go[k], &x, &z);
                sph_v0x[k] = x + ((wprev - sph_wnew8) >> 4);
                sph_v0z[k] = z;
                sph_rho0[k] = clampi(isqrt((uint32_t)(sph_v0x[k] * sph_v0x[k] + sph_v0z[k] * sph_v0z[k])), 64, 1 << 20);
                sph_go[k] = now - t1;            /* (in the stream already) */
                continue;
            }
            if (sph_ph[k] == 1 && !sph_cap[k]) continue;   /* (still sinking to the equator: it goes on) */
            if (sph_ph[k] != 0) sph_a8[k] = wprev;   /* home (or just arrived): that one is now its old one */
            sph_ph[k] = 0;
        }
        for (k = 0; k < NTRAV; k++)
            if (sph_ph[k] == 0) {                /* its turn, one at a time: the bell's partials and the screamers
                                                 * (the highest) first, then the voices, lightest first */
                int32_t rank = 0;
                if (k >= NOSC) rank = NTRAV - 1 - k;
                else {
                    rank = NSCR + NBELL;
                    for (j2 = 0; j2 < NOSC; j2++) rank += mass8[j2] < mass8[k] || (mass8[j2] == mass8[k] && j2 < k);
                }
                sph_go[k] = now + (uint32_t)rank * (T / 2u) / (NTRAV - 1);
            }
    }
    for (k = 0; k < NTRAV; k++) {
        if (sph_ph[k] == 0 && (int32_t)(now - sph_go[k]) >= 0) { sph_ph[k] = 1; sph_cap[k] = sph_in[k] = 0; }
        if (sph_ph[k] == 1 && now - sph_go[k] >= sph_trav) { sph_ph[k] = 2; sph_a8[k] = sph_wnew8; }
        home += sph_ph[k] == 2;
    }
    if (sph_moving && home == NTRAV) sph_moving = 0;
    if (!sph_moving) {                           /* (settled: the new one is the origin again) */
        int32_t o = sph_wnew8;
        sph_wnew8 = 0; sph_wold8 -= o; sph_cam8 -= o;
        for (k = 0; k < NTRAV; k++) sph_a8[k] -= o;
    }
    sph_cam8 += ((sph_moving ? (sph_wold8 + sph_wnew8) / 2 : sph_wnew8) - sph_cam8) >> 3;
    zgoal = sph_moving ? rd * 32767 / r : 32767;
    sph_zoom += (zgoal - sph_zoom) >> 3;
    sph_rad = (SPH_R * sph_zoom) >> 15;
    {                                            /* the screen's colour (the root, the dials, the note, the steps,
                                                 * the status): where KNOB 1 is, hell red .. plain rose .. heaven
                                                 * white (as the voices go) */
        int32_t t = pe[P_CHAR] * 32767 / 100;
        sph_root_rgb[0] = 230 + ((25 * t) >> 15); sph_root_rgb[1] = 30 + ((222 * t) >> 15);
        sph_root_rgb[2] = 20 + ((220 * t) >> 15);
    }
    for (k = 0; k < NOSC; k++) {
        int32_t rel = (pos[k] - base) >> 16, u = absi(clampi(rel, -3600, 3600)) * 32767 / 3600;
        int32_t lat = (rel < 0 ? -1 : 1) * ((isqrt((uint32_t)u << 15) * 80) >> 15);   /* degrees: 80 x sqrt (near
                                                                      * intervals open out) */
        int32_t r, g, bl, lev = clampi(ui_lev[k] * 9, 0, 32767);   /* (a voice ~3600: full) */
        /* it turns at its pitch's ratio to the root's (the harmony: simple ratios fall into step), the root once
         * in 8 s at DRIFT 50 (Venus: 0 still .. 100 twice that) */
        sph_lon[k] += (uint32_t)(((uint64_t)dt * 536871u * (uint32_t)clampi(
                          (int32_t)(((uint64_t)freq_inc(rel) << 12) / inc0), 0, 8 << 12) >> 12) * venus >> 15);
        /* its twin laps it once every 8 of their beats: the faster they beat, the faster it chases */
        sph_tw[k] += dt * (uint32_t)clampi(ui_beat[k], 0, 40000) * 537u;
        if (k == ROOT) { r = sph_root_rgb[0]; g = sph_root_rgb[1]; bl = sph_root_rgb[2]; }
        else sph_voice_rgb(k, mars, &r, &g, &bl);
        if (!sph_place(k, &lat, r, g, bl, lev, SPH_TRAIL, now, t1, t2, t3)) continue;
        sph_trail(lat, sph_lon[k], r, g, bl, lev, SPH_TRAIL);
        sph_trail(lat, sph_lon[k] + sph_tw[k], r, g, bl,   /* the twin, as deep as the phasing */
                  (int32_t)(((int64_t)((lev * 3) >> 2) * clampi(pe[P_VENUS] * 32767 / 30, 0, 32767)) >> 15), 12);
    }
    /* the screamers: embers high in the north, quick, each wailing (its latitude with its pitch), flaring when
     * it cracks */
    for (j = 0; j < NSCR; j++) {
        int32_t t = NOSC + j, lat = 56 + j * 4 + (scr_g8[j] >> 12), lev = clampi(scr_a[j] * 15, 0, 32767);   /* (2200:
                                                                      * full) */
        int crack = scr_crack[j] != 0;
        sph_lon[t] += (uint32_t)(((uint64_t)dt * (900000u + (uint32_t)j * 70000u) * (uint32_t)venus) >> 15);
        if (crack) lat += (sph_rnd() & 7) - 3;
        if (sph_place(t, &lat, 255, crack ? 120 : 50, crack ? 40 : 30, crack ? 32767 : lev, 10, now, t1, t2, t3))
            sph_trail(lat, sph_lon[t], 255, crack ? 120 : 50, crack ? 40 : 30, crack ? 32767 : lev, 10);
    }
    /* heaven's bell: a ring of pale gold round the north pole, a light a partial, turning slowly together (each
     * finds its place in the ring again after a crossing) */
    sph_bell += (uint32_t)(((uint64_t)dt * 150000u * (uint32_t)venus) >> 15);
    for (j = 0; j < NBELL; j++) {
        int32_t t = NOSC + NSCR + j, lat = 72, lev = BELL_A[j] * BELL_G >> 15;
        lev = lev ? clampi(bell_a[j] * 32767 / lev, 0, 32767) : 0;
        sph_lon[t] += (uint32_t)(((uint64_t)dt * 150000u * (uint32_t)venus) >> 15);
        if (sph_ph[t] == 2) sph_lon[t] += (uint32_t)((int32_t)(sph_bell + (uint32_t)j * 613566757u - sph_lon[t]) >> 5);
        if (sph_place(t, &lat, 255, 226, 150, lev, 6, now, t1, t2, t3)) sph_trail(lat, sph_lon[t], 255, 226, 150, lev, 6);
    }
}
static void draw_cosmos(void)                   /* the whole screen, in five strips of 48 rows */
{
    static uint32_t last_t;
    int32_t strip;
    uint32_t t = q_ms, j;
    char name[8];
    sph_points(last_t ? t - last_t : 0u);
    last_t = t;
    put_note(name, root_note + 12 * st.oct);
    for (strip = 0; strip < 5; strip++) {       /* (each part drawn only in the strips it is in) */
        int32_t y0 = strip * 48;
        qg_begin(240, 48, QC_BLACK);
        qg_oy = -y0;
        if (strip <= 1) draw_dials();                                   /* rows 4..72 */
        if (strip <= 1 && last_enc >= 0 && q_ms - enc_t < 1500u) draw_overlay();   /* 40..74 */
        if (strip >= 3)                                                 /* 180..196 */
            text_c(184, 180, 56, &QFONT_S, name, sph_rgb(sph_root_rgb[0], sph_root_rgb[1], sph_root_rgb[2], 32767));   /* (the
                                                                      * root's colour) */
        for (j = 0; j < SPH_N; j++) {            /* the far side, then the near */
            const sph_pt *q;
            if (j == sph_nb) j = SPH_N - sph_nf;
            if (j >= SPH_N) break;
            q = &sph_p[j];
            if (q->y < y0 - 2 || q->y > y0 + 49) continue;
            if (q->head && q->front) qg_rect(q->x - 1, q->y - 1, 3, 3, q->c);
            else if (q->head) qg_rect(q->x, q->y, 2, 2, q->c);
            else qg_pset(q->x, q->y, q->c);
        }
        if (strip == 4) { draw_steps(); draw_status(); }                /* 200..239 */
        if (q_ms - splash_until < 800u) {      /* after the splash: it fades in over 0.8 s */
            uint32_t k, lv = (q_ms - splash_until) * 256u / 800u, n = qg_w * qg_h;
            for (k = 0; k < n; k++) {
                uint32_t c = qg_swap16(qg_px[k]);
                c = ((((c >> 11) * lv) >> 8) << 11) | (((((c >> 5) & 63u) * lv) >> 8) << 5) | (((c & 31u) * lv) >> 8);
                qg_px[k] = qg_swap16(c);
            }
        }
        {                                        /* unchanged since the last frame: not sent again */
            static uint32_t sum[5];
            uint32_t h = 2166136261u, i, n = qg_w * qg_h;
            for (i = 0; i < n; i++) h = (h ^ qg_px[i]) * 16777619u;   /* (FNV-1a) */
            if (h == sum[strip] && !strip_force) { qg_oy = 0; continue; }
            sum[strip] = h;
        }
        qg_oy = 0;
        qg_blit(0, (uint32_t)strip * 48u);
    }
}

/* the splash: the hands of Levi's Baphomet (1856), the raised one (SOLVE) in gold, the lowered (COAGULA) in red
 * (his ink lines as light): as above, so below; the title in blackletter, white. 4-bit grey (tools/gen_splash.py) */
#include "quod_splash.h"
static void draw_grey(const uint8_t *px, int32_t w, int32_t h, int32_t x0, int32_t y0, int32_t r, int32_t g,
                      int32_t b, int solid)   /* solid: 1 .. 15 black .. full (nothing behind shows); else 1 .. 15 */
{
    int32_t x, y, bw = (w + 1) / 2, n = solid ? 14 : 15, o = solid ? 1 : 0;
    for (y = 0; y < h; y++) {
        if (y0 + y + qg_oy < 0 || y0 + y + qg_oy >= (int32_t)qg_h) continue;   /* (only this strip's rows) */
        for (x = 0; x < w; x++) {
            int32_t v = px[y * bw + x / 2];
            v = x & 1 ? v & 15 : v >> 4;
            if (v) {
                v -= o;
                qg_pset(x0 + x, y0 + y, QRGB((uint32_t)(r * v / n), (uint32_t)(g * v / n), (uint32_t)(b * v / n)));
            }
        }
    }
}
/* Behind: the dials' symbols, an ocean we see a window into. The world is 384 px square and wraps round (no edges:
 * what leaves one side comes in at the other), the screen a 240 px window in it; 441 symbols fill it. Each is a
 * disc of its own, drawn round and down into a whirlpool in the middle of the window, fading as it goes (it
 * takes less room as it nears the drain, so they crowd in) and gone at the centre (and born again far out, past the window's corners), stirred a little by broad currents
 * drifting through (the world's own size, so they wrap too), pushed apart from its neighbours (a little bounce) and dragging on them (viscosity: they flow as one).
 * They start tiled (a brick bond), and pass behind the hands and the title. Q8 px, a step a 16 ms; the neighbours
 * found by a grid of 32 px cells. */
#define FL_W 384                                /* the world, px (a whole number of cells) */
#define FL_C 32                                 /* a cell, px */
#define FL_G (FL_W / FL_C)
#define FL_N 441                                /* (21 x 21: ~18 px apart, touching: a light pressure) */
#define FL_R 9                                  /* a symbol's radius, px */
#define FL_OX ((FL_W - 240) / 2)                /* where the window is */
#ifndef FL_SPEED
#define FL_SPEED 512                            /* the whirlpool's speed, Q8: 256 as described below (x2) */
#endif
static int32_t fl_x[FL_N], fl_y[FL_N], fl_vx[FL_N], fl_vy[FL_N];
static uint8_t fl_sym[FL_N];
static int16_t fl_head[FL_G * FL_G], fl_next[FL_N], fl_rr[FL_N];
static uint32_t fl_n, fl_t, fl_ph[4];
static int32_t fl_wrap(int32_t d)                /* a difference across the world, the short way round (Q8) */
{
    if (d > (FL_W / 2) << 8) d -= FL_W << 8;
    if (d < -((FL_W / 2) << 8)) d += FL_W << 8;
    return d;
}
static const uint8_t FL_SYM[9] = {P_SATURN, P_MERCURY, P_SUN, P_MARS, P_MOON, P_VENUS, P_JUPITER, P_GATE, P_COUNT};
static void fluid_init(void)
{
    int32_t row, col;
    fl_n = 0;
    for (row = 0; row < 21; row++)
        for (col = 0; col < 21; col++) {
            fl_x[fl_n] = ((col * 2 + (row & 1)) * FL_W << 8) / 42; fl_y[fl_n] = ((row * 2 + 1) * FL_W << 8) / 42;
            fl_vx[fl_n] = fl_vy[fl_n] = 0;
            fl_sym[fl_n++] = (uint8_t)((uint32_t)(row * 4 + col) % 9u);   /* (which of FL_SYM) */
        }
    fl_t = q_ms;
}
static void fluid_step(void)
{
    uint32_t i;
    int32_t c;
    /* the currents: broad swirls drifting through, each at its own pace. They only stir: each is a stream
     * function psi = sin(a x + b y + phase), its flow (b, -a) cos(...), so nothing gathers or empties (a fluid
     * is not squeezed); a, b whole turns across the world, so they wrap too. */
    {
        static const int8_t MA[4] = {1, 1, 2, 0}, MB[4] = {1, -2, 1, 1};
        static const int16_t AMP[4] = {9, 5, 5, 7};              /* Q8 px a step: a faint stir, under the swirl */
        static const uint32_t PH[4] = {4000000u, 2700000u, 3200000u, 2300000u};
        uint32_t m;
        for (m = 0; m < 4u; m++) fl_ph[m] += PH[m];
        for (i = 0; i < fl_n; i++) {
            uint32_t px = (uint32_t)(fl_x[i] >> 8) * (0xFFFFFFFFu / FL_W), py = (uint32_t)(fl_y[i] >> 8) * (0xFFFFFFFFu / FL_W);
            int32_t u = 0, v = 0, sh;
            for (m = 0; m < 4u; m++) {
                int32_t c = sine((uint32_t)MA[m] * px + (uint32_t)MB[m] * py + fl_ph[m] + 0x40000000u) * AMP[m];
                u += (c * MB[m]) >> 15;
                v -= (c * MA[m]) >> 15;
            }
            {                                    /* the whirlpool round the middle of the window, anticlockwise.
                                                 * Out past 70 px it draws in and turns as 1 / r (24 px a second
                                                 * in, 30 round, at 70); inside, both slow as sqrt r toward the
                                                 * drain, so the symbols crowd in as they shrink (shrinking as
                                                 * r / 70: to stay as full, they must come as 1 / r^2 closer) */
                int32_t dx = (fl_x[i] >> 8) - FL_W / 2, dy = (fl_y[i] >> 8) - FL_W / 2, r = isqrt((uint32_t)(dx * dx + dy * dy));
                if (r < 1) r = 1;
                if (r >= 70) {
                    u += (-dx * (6860 * FL_SPEED >> 8) + dy * (8600 * FL_SPEED >> 8)) / (r * r);   /* (screen y is
                                                 * down: + dy turns it anticlockwise) */
                    v += (-dy * (6860 * FL_SPEED >> 8) - dx * (8600 * FL_SPEED >> 8)) / (r * r);
                } else {
                    int32_t sq = isqrt((uint32_t)(r * 256)), sr = sq * (188 * FL_SPEED >> 8) >> 8;   /* Q8 a step */
                    int32_t st = sq * (235 * FL_SPEED >> 8) >> 8;
                    u += (-dx * sr + dy * st) / r;
                    v += (-dy * sr - dx * st) / r;
                }
                sh = 3;                          /* (each goes nearly as the water goes: no flinging out of the
                                                 * turn, no thinning where it quickens) */
            }
            fl_vx[i] += (u - fl_vx[i]) >> sh;
            fl_vy[i] += (v - fl_vy[i]) >> sh;
        }
    }
    for (i = 0; i < fl_n; i++) {                 /* each one's size, Q4 px: shrinking down the drain (so they can
                                                 * crowd into it) */
        int32_t dx = (fl_x[i] >> 8) - FL_W / 2, dy = (fl_y[i] >> 8) - FL_W / 2, r2 = dx * dx + dy * dy;
        fl_rr[i] = (int16_t)(r2 >= 70 * 70 ? FL_R * 16 : FL_R * 16 * isqrt((uint32_t)r2) / 70);
    }
    for (c = 0; c < FL_G * FL_G; c++) fl_head[c] = -1;
    for (i = 0; i < fl_n; i++) {
        c = (fl_y[i] >> 8) / FL_C * FL_G + (fl_x[i] >> 8) / FL_C;
        fl_next[i] = fl_head[c]; fl_head[c] = (int16_t)i;
    }
    for (i = 0; i < fl_n; i++) {
        int32_t cx = (fl_x[i] >> 8) / FL_C, cy = (fl_y[i] >> 8) / FL_C, ox, oy;
        for (oy = -1; oy <= 1; oy++)
            for (ox = -1; ox <= 1; ox++) {
                int32_t j = fl_head[((cy + oy + FL_G) % FL_G) * FL_G + (cx + ox + FL_G) % FL_G];
                for (; j >= 0; j = fl_next[j]) {
                    int32_t dx, dy, d2, d, o, nx, ny, vn, cd;
                    if ((uint32_t)j <= i) continue;          /* (each pair once) */
                    dx = fl_wrap(fl_x[j] - fl_x[i]) >> 4; dy = fl_wrap(fl_y[j] - fl_y[i]) >> 4;   /* Q4 */
                    d2 = dx * dx + dy * dy;
                    cd = fl_rr[i] + fl_rr[j];            /* (touching, Q4) */
                    if (4 * d2 >= 9 * cd * cd) continue;
                    fl_vx[i] += (fl_vx[j] - fl_vx[i]) >> 6; fl_vx[j] -= (fl_vx[j] - fl_vx[i]) >> 6;   /* viscosity */
                    fl_vy[i] += (fl_vy[j] - fl_vy[i]) >> 6; fl_vy[j] -= (fl_vy[j] - fl_vy[i]) >> 6;
                    if (d2 >= cd * cd) {                 /* near, not touching: a gentle pull together (cohesion:
                                                 * the holes the swirl tears open close again, as in a liquid) */
                        if (5 * d2 < 8 * cd * cd) {     /* (within ~1.26 x touching) */
                            d = isqrt((uint32_t)d2);
                            o = (d - cd) << 4;           /* Q8 px apart past touching: each comes 1/32 of it */
                            nx = (dx << 8) / d; ny = (dy << 8) / d;
                            fl_x[i] += (nx * o) >> 13; fl_y[i] += (ny * o) >> 13;
                            fl_x[j] -= (nx * o) >> 13; fl_y[j] -= (ny * o) >> 13;
                        }
                        continue;
                    }
                    d = isqrt((uint32_t)d2);
                    if (d == 0) { dx = 16; d = 16; }
                    nx = (dx << 8) / d; ny = (dy << 8) / d;      /* the normal, Q8 */
                    o = (cd - d) << 4;                           /* the overlap, Q8 px: each moves half of it */
                    fl_x[i] -= (nx * o) >> 9; fl_y[i] -= (ny * o) >> 9;
                    fl_x[j] += (nx * o) >> 9; fl_y[j] += (ny * o) >> 9;
                    vn = ((fl_vx[j] - fl_vx[i]) * nx + (fl_vy[j] - fl_vy[i]) * ny) >> 8;
                    if (vn < 0) {                                /* closing: a soft bounce (e ~ 0.3) */
                        int32_t k = (vn * 166) >> 8;             /* (1 + e) / 2 */
                        fl_vx[i] += (k * nx) >> 8; fl_vy[i] += (k * ny) >> 8;
                        fl_vx[j] -= (k * nx) >> 8; fl_vy[j] -= (k * ny) >> 8;
                    }
                }
            }
    }
    for (i = 0; i < fl_n; i++) {                 /* on, and round the world */
        int32_t dx, dy;
        fl_x[i] = (fl_x[i] + fl_vx[i] + (FL_W << 8)) % (FL_W << 8);
        fl_y[i] = (fl_y[i] + fl_vy[i] + (FL_W << 8)) % (FL_W << 8);
        dx = (fl_x[i] >> 8) - FL_W / 2; dy = (fl_y[i] >> 8) - FL_W / 2;
        if (dx * dx + dy * dy < 10 * 10) {       /* down the middle: gone; it comes again from far out (past the
                                                 * window's corners), already turning */
            uint32_t a = (uint32_t)rnd16() << 16;
            int32_t r = 178 + (rnd16() & 7);
            fl_x[i] = (FL_W / 2 + ((sine(a + 0x40000000u) * r) >> 15)) << 8;
            fl_y[i] = (FL_W / 2 + ((sine(a) * r) >> 15)) << 8;
            fl_vx[i] = (sine(a) * 12) >> 15; fl_vy[i] = -(sine(a + 0x40000000u) * 12) >> 15;
        }
    }
}
static void draw_splash(void)
{
    int32_t strip, lv = (int32_t)(splash_until - q_ms);   /* (its last 0.8 s, all of it fades out) */
    uint32_t i;
    lv = lv >= 800 ? 255 : lv <= 0 ? 0 : lv * 255 / 800;
    if (!fl_n) fluid_init();
    for (i = 0; i < 6u && q_ms - fl_t >= 16u; i++) { fluid_step(); fl_t += 16u; }   /* (in time; a stall skips) */
    if (q_ms - fl_t >= 16u) fl_t = q_ms;
    for (strip = 0; strip < 5; strip++) {
        qg_begin(240, 48, QC_BLACK);
        qg_oy = -strip * 48;
        for (i = 0; i < fl_n; i++) {             /* the window: those in it, and those half in it */
            int32_t x = (fl_x[i] >> 8) - FL_OX, y = (fl_y[i] >> 8) - FL_OX;   /* (-72 .. 311: the world once) */
            int32_t r2 = (x - 120) * (x - 120) + (y - 120) * (y - 120);
            if (x < -12 || x > 252 || y + 12 + qg_oy < 0 || y - 12 + qg_oy >= 48) continue;
            {
                int32_t f = r2 >= 70 * 70 ? lv : lv * isqrt((uint32_t)r2) / 70;   /* (fading down the drain) */
                if (f >= 8)
                    planet(FL_SYM[fl_sym[i]], x, y, QRGB((uint32_t)(52 * f / 255), (uint32_t)(44 * f / 255), (uint32_t)(34 * f / 255)));
            }
        }
        draw_grey(HAND_UP, HAND_UP_W, HAND_UP_H, HAND_UP_X, HAND_UP_Y, lv, 228 * lv / 255, 160 * lv / 255, 1);   /* above */
        draw_grey(HAND_DOWN, HAND_DOWN_W, HAND_DOWN_H, HAND_DOWN_X, HAND_DOWN_Y, lv, 72 * lv / 255, 48 * lv / 255, 1);
        draw_grey(TITLE, TITLE_W, TITLE_H, TITLE_X, TITLE_Y, lv, lv, lv, 0);
        qg_oy = 0;
        qg_blit(0, (uint32_t)strip * 48u);
    }
}

/* ---------------------------------------------------------------- the FX page: XIV, Temperance --- */
/* The tarot's Temperance pours between two cups: the alchemy of mixing, the wet into the dry. A card's frame; its
 * title; the seven knobs as dials (as on HOME, in the screen's colour), each with its alchemical sign; between the
 * cups a stream running as the delay runs; the knob last turned, written out. */
static void alch(uint32_t p, int32_t cx, int32_t cy, uint16_t c)   /* a parameter's alchemical sign */
{
    switch (p) {
    case P_DELAY:                                /* air: the triangle up, barred (echoes through the air) */
        qg_line(cx, cy - 8, cx + 8, cy + 6, c); qg_line(cx + 8, cy + 6, cx - 8, cy + 6, c); qg_line(cx - 8, cy + 6, cx, cy - 8, c);
        qg_line(cx - 7, cy + 1, cx + 7, cy + 1, c);
        break;
    case P_DTIME:                                /* the hourglass */
        qg_line(cx - 6, cy - 8, cx + 6, cy - 8, c); qg_line(cx - 6, cy + 8, cx + 6, cy + 8, c);
        qg_line(cx - 6, cy - 8, cx + 6, cy + 8, c); qg_line(cx + 6, cy - 8, cx - 6, cy + 8, c);
        break;
    case P_DFB: {                                /* the ouroboros: a circle eating its tail */
        int32_t n, px = 0, py = 0;
        for (n = 0; n <= 28; n++) {
            uint32_t ph = (uint32_t)n * 0x08888888u + 0x10000000u;   /* (most of a turn: a gap at the mouth) */
            int32_t x = cx + (sine(ph + 0x40000000u) * 8 >> 15), y = cy - (sine(ph) * 8 >> 15);
            if (n) qg_line(px, py, x, y, c);
            px = x; py = y;
        }
        qg_line(px, py, px + 3, py - 2, c); qg_line(px, py, px + 3, py + 2, c);   /* (its head) */
        break;
    }
    case P_VERB:                                 /* water: the triangle down */
        qg_line(cx - 8, cy - 6, cx + 8, cy - 6, c); qg_line(cx + 8, cy - 6, cx, cy + 8, c); qg_line(cx, cy + 8, cx - 8, cy - 6, c);
        break;
    case P_DRIVE:                                /* fire: the triangle up */
        qg_line(cx, cy - 8, cx + 8, cy + 6, c); qg_line(cx + 8, cy + 6, cx - 8, cy + 6, c); qg_line(cx - 8, cy + 6, cx, cy - 8, c);
        break;
    default:
        break;
    }
}
static void fx_value(char *s, uint32_t p, int32_t v)
{
    if (p == P_DTIME) { s = put_num(s, (uint32_t)((2205 + v * 168) * 1000 / 44100), 1); put(s, " ms"); }
    else if (p == P_DFB) { s = put_num(s, (uint32_t)(v * 90 / 100), 1); put(s, " %"); }
    else put_num(s, (uint32_t)v, 1);
}
static void draw_fx(void)
{
    static uint32_t t0;
    int32_t strip, j;
    uint16_t c = sph_rgb(sph_root_rgb[0], sph_root_rgb[1], sph_root_rgb[2], 32767);
    uint16_t cd = sph_rgb(sph_root_rgb[0], sph_root_rgb[1], sph_root_rgb[2], 7200);
    uint16_t cm = sph_rgb(sph_root_rgb[0], sph_root_rgb[1], sph_root_rgb[2], 15000);
    {                                            /* (the colour, as HOME has it) */
        int32_t t = pe[P_CHAR] * 32767 / 100;
        sph_root_rgb[0] = 230 + ((25 * t) >> 15); sph_root_rgb[1] = 30 + ((222 * t) >> 15);
        sph_root_rgb[2] = 20 + ((220 * t) >> 15);
    }
    if (!t0) t0 = q_ms;
    for (strip = 0; strip < 5; strip++) {
        int32_t y0 = strip * 48;
        qg_begin(240, 48, QC_BLACK);
        qg_oy = -y0;
        /* the card: a double frame, a diamond at each corner */
        qg_rect(2, 2, 236, 1, cm); qg_rect(2, 237, 236, 1, cm); qg_rect(2, 2, 1, 236, cm); qg_rect(237, 2, 1, 236, cm);
        qg_rect(6, 6, 228, 1, cd); qg_rect(6, 233, 228, 1, cd); qg_rect(6, 6, 1, 228, cd); qg_rect(233, 6, 1, 228, cd);
        {
            static const int16_t CX[4] = {4, 235, 4, 235}, CY[4] = {4, 4, 235, 235};
            for (j = 0; j < 4; j++) {
                qg_line(CX[j] - 4, CY[j], CX[j], CY[j] - 4, c); qg_line(CX[j], CY[j] - 4, CX[j] + 4, CY[j], c);
                qg_line(CX[j] + 4, CY[j], CX[j], CY[j] + 4, c); qg_line(CX[j], CY[j] + 4, CX[j] - 4, CY[j], c);
            }
        }
        if (strip == 0) draw_grey(FX_TITLE, FX_TITLE_W, FX_TITLE_H, FX_TITLE_X, 12, sph_root_rgb[0], sph_root_rgb[1], sph_root_rgb[2], 0);
        qg_rect(40, 38, 160, 1, cd);             /* a rule under the title */
        /* the seven knobs, as dials where they sit (DIAL_COL, DIAL_ROW) */
        for (j = 0; j < 7; j++) {
            int32_t cx = 24 + DIAL_COL[j] * 38, cy = 64 + DIAL_ROW[j] * 36, n, lit, v;
            uint32_t p = FX_PARAM[j];
            int32_t px = 0, py = 0, qx = 0, qy = 0;
            if (cy + 18 + qg_oy < 0 || cy - 18 + qg_oy >= 48) continue;
            if (p == P_NONE) { qg_circle(cx, cy, 14, cd); continue; }   /* (nothing on this knob) */
            v = st.p[p];
            lit = (v - PARAM[p].lo) * 48 / (PARAM[p].hi - PARAM[p].lo);
            for (n = 0; n <= 48; n++) {
                int32_t a = -135 + n * 270 / 48, x, y, x2, y2;
                uint32_t ph = (uint32_t)(a < 0 ? a + 360 : a) * 11930465u;
                x = cx + (sine(ph) * 15 >> 15); y = cy - (sine(ph + 0x40000000u) * 15 >> 15);
                x2 = cx + (sine(ph) * 13 >> 15); y2 = cy - (sine(ph + 0x40000000u) * 13 >> 15);
                if (n) { qg_line(px, py, x, y, n <= lit ? c : cd); qg_line(qx, qy, x2, y2, n <= lit ? c : cd); }
                if (n == lit) qg_rect(x - 1, y - 1, 3, 3, QC_WHITE);
                px = x; py = y; qx = x2; qy = y2;
            }
            alch(p, cx, cy, c);
            if ((int32_t)p == last_enc && q_ms - enc_t < 1500u) qg_rect(cx - 12, cy + 18, 24, 2, c);
        }
        /* Temperance's two cups, the stream between them running as the delay does (its time, its feedback) */
        {
            int32_t k;
            uint32_t run = (q_ms - t0) * (uint32_t)(40 + (100 - st.p[P_DTIME]) * 2);
            /* the cups: left above, right below (moved 36 right and 8 down, clear of the low dials) */
            qg_line(96, 112, 120, 112, c); qg_line(96, 112, 100, 130, c); qg_line(120, 112, 116, 130, c); qg_line(100, 130, 116, 130, c);
            qg_line(108, 130, 108, 136, c); qg_line(102, 136, 114, 136, c);
            qg_line(192, 158, 216, 158, c); qg_line(192, 158, 196, 176, c); qg_line(216, 158, 212, 176, c); qg_line(196, 176, 212, 176, c);
            qg_line(204, 176, 204, 182, c); qg_line(198, 182, 210, 182, c);
            for (k = 0; k < 24; k++) {           /* the stream: off the upper cup's lip, falling (a parabola) into
                                                 * the lower cup's mouth */
                int32_t u = (int32_t)((((uint32_t)k << 11) + (run >> 4)) & 0xFFFFu);   /* along it, 0 .. 65535 */
                int32_t x = 120 + (u * 84 >> 16), y = 110 + (int32_t)((((int64_t)u * u) >> 16) * 46 >> 16);
                int32_t l = 6000 + st.p[P_DFB] * 260;   /* (brighter, the more it feeds back) */
                qg_rect(x, y, 2, 2, sph_rgb(sph_root_rgb[0], sph_root_rgb[1], sph_root_rgb[2], l));
            }
        }
        /* the knob last turned (else the delay), written out */
        {
            char v[16];
            uint32_t p = (last_enc >= 0 && q_ms - enc_t < 4000u) ? (uint32_t)last_enc : P_DELAY;
            for (j = 0; j < 7; j++) if (FX_PARAM[j] == p) break;
            if (j == 7) p = P_DELAY;
            qg_text(16, 196, &QFONT_S, PARAM[p].name, c);
            fx_value(v, p, st.p[p]);
            qg_text(224 - qg_text_w(&QFONT_S, v), 196, &QFONT_S, v, QC_WHITE);
            qg_rect(16, 216, 208, 1, cd);
            qg_rect(16, 216, 208 * (st.p[p] - PARAM[p].lo) / (PARAM[p].hi - PARAM[p].lo), 1, c);
        }
        qg_oy = 0;
        qg_blit(0, (uint32_t)strip * 48u);
    }
}

static void draw(void)
{
    static uint32_t frame_t;
    {                                            /* the sound fades in from the start, under the splash, full once the
                                                 * screen is (its 4 s and the screen's 0.8): squared, even to the ear */
        uint32_t e = q_ms - (splash_until - SPLASH_MS);
        int32_t u = e >= SPLASH_MS + 800u ? 32767 : (int32_t)(e * 32767u / (SPLASH_MS + 800u));
        out_goal = (int32_t)(((int64_t)u * u) >> 15);
    }
    if (q_ms < splash_until) {                 /* the splash: its symbols flow, ~30 frames a second */
        if (q_ms - frame_t >= 33u || full_redraw) { full_redraw = 0; frame_t = q_ms; draw_splash(); }
        if (q_ms + 40u >= splash_until) full_redraw = 1;   /* (then the cosmos, all of it) */
        return;
    }
    if (q_ms - frame_t < 33u && !full_redraw) return;   /* up to ~30 frames a second */
    frame_t = q_ms;
    strip_force = full_redraw;
    full_redraw = 0;
    {                                            /* how many a second (HOME held shows it) */
        static uint32_t n, t0;
        n++;
        if (q_ms - t0 >= 1000u) { ui_fps = n * 1000u / (q_ms - t0); n = 0; t0 = q_ms; }
    }
    if (page == PG_FX) draw_fx(); else draw_cosmos();
}

static void leds(void)
{
    uint32_t k, blink = q_ms >> 7 & 1u;
    for (k = 0; k < 27u; k++) {
        int32_t w = white_of[k];
        int on;
        if (step_mode && w >= 0) {              /* the steps: trigs, the playhead, the one held */
            on = st.trig >> w & 1u;
            if (seq_play && (uint32_t)w == seq_i) on = !on;
            if (w == held_step) on = blink;
        } else on = (held_keys >> k & 1u) || (53 + (int32_t)k == root_note);
        q_led(KEY_LED(k), on);
    }
    q_led(BTN_PLAY, seq_play && (q_ms - seq_t < 120u || (!step_ms() && blink)));
    q_led(BTN_REC, (int)rec_on);
    q_led(BTN_SEQ, (int)step_mode);
    q_led(BTN_HOME, page == PG_HOME);           /* the page showing: HOME's or FX's button lit */
    q_led(BTN_FX, page == PG_FX);
    q_led(BTN_EDIT, (int)settle_held);
    q_led(BTN_SEL, (int)shift);
    q_led(BTN_SAVE, (int)dirty);
    q_led(BTN_OCT_DOWN, st.oct < 0);
    q_led(BTN_OCT_UP, st.oct > 0);
}

static int key_black(uint32_t k) { return (0x54Au >> ((53u + k) % 12u)) & 1u; }

/* ---- the showcase: HOME + REC held 1 s. A tour of the drone from the splash on (~95 s): heaven, a new root, the
 * spread, the tilt, hell, phasing, drift. Any key, button or knob stops it; then (or at its end) the
 * controls, the root and the sequencer are as they were. */
typedef struct { uint32_t t0, t1; uint8_t cc, v0, v1; } show_ev_t;   /* cc 30 + i: control i from v0 to v1 (0..127
                                                 * across its range) over t0 .. t1 ms; cc 0: the root becomes v0 */
static const show_ev_t SHOW[] = {
    {14, 14, 0, 48, 0},           {14, 14, 30 + P_CHAR, 64, 64},          /* C3, between heaven and hell */
    {8000, 14000, 30 + P_CHAR, 64, 127},                                  /* into heaven */
    {14500, 14500, 30 + P_SATURN, 97, 97}, {15000, 15000, 0, 55, 0},      /* thicker; G3 */
    {22000, 25500, 30 + P_JUPITER, 76, 127}, {25500, 29000, 30 + P_JUPITER, 127, 38},
    {29000, 30500, 30 + P_JUPITER, 38, 75},                               /* the spread out, in, back */
    {31000, 34500, 30 + P_MOON, 64, 0}, {34500, 38000, 30 + P_MOON, 0, 127},
    {38000, 39500, 30 + P_MOON, 127, 64},                                 /* the tilt down, up, back */
    {40000, 40000, 30 + P_SATURN, 111, 111}, {40500, 40500, 0, 36, 0},    /* honey; C2 */
    {41000, 55000, 30 + P_CHAR, 127, 1},                                  /* down into hell */
    {56000, 56000, 30 + P_VENUS, 0, 0}, {59000, 64000, 30 + P_VENUS, 0, 38},   /* the twins come in: a slow beat */
    {67000, 69000, 30 + P_VENUS, 38, 64}, {71000, 75000, 30 + P_VENUS, 64, 102},   /* faster, to a Lyra's most */
    {78000, 81000, 30 + P_VENUS, 102, 51},                                /* back to the default */
    {82000, 85000, 30 + P_VENUS, 51, 125},                                /* then wild: out of tune */
    {86000, 86000, 30 + P_SATURN, 90, 90}, {86500, 86500, 0, 53, 0},      /* F3 */
};
#define SHOW_N (sizeof SHOW / sizeof SHOW[0])
#define SHOW_MS 95000u
#define SHOW_BTNS (1u << BTN_HOME | 1u << BTN_REC)
static uint32_t show_t, show_done[(SHOW_N + 31u) / 32u], show_seq, show_rec;
static int16_t show_p[P_COUNT];
static int32_t show_root;

static void show_start(void)
{
    uint32_t i;
    memcpy(show_p, st.p, sizeof show_p);
    show_root = root_note; show_seq = seq_play;
    show_rec = !rec_on;                          /* (REC was pressed for it: what it was before) */
    for (i = 0; i < P_COUNT; i++) st.p[i] = PARAM[i].def;
    memset(show_done, 0, sizeof show_done);
    seq_play = 0; rec_on = 0; step_mode = 0; held_step = -1; armed = 0;
    page = PG_HOME; page_t = 0;
    show_t = q_ms; show_on = 1;
    splash_until = q_ms + SPLASH_MS; fl_n = 0;  /* (from the splash, the sound fading in again) */
    full_redraw = 1;
}

static void show_stop(void)
{
    memcpy(st.p, show_p, sizeof show_p);
    set_root(show_root);
    rec_on = show_rec;
    seq_play = show_seq; seq_t = q_ms;
    if (q_ms < splash_until) splash_until = q_ms;   /* (stopped in the splash: straight to the cosmos) */
    show_on = 0;
    full_redraw = 1;
}

static void show_poll(void)
{
    static uint32_t held_t;
    uint32_t t, k;
    if ((q_in.buttons & SHOW_BTNS) == SHOW_BTNS) {   /* both held: a second, then it starts (once a hold) */
        if (!held_t) held_t = q_ms | 1u;
        else if (held_t != 2u && q_ms - held_t >= 1000u && !show_on) { show_start(); held_t = 2u; }
    } else held_t = 0;
    if (!show_on) return;
    t = q_ms - show_t;                          /* (after a start: read before it, its first pass ran the tour up to
                                                 * the time since boot, and it began wherever that was) */
    if (t >= SHOW_MS) { show_stop(); return; }
    for (k = 0; k < SHOW_N; k++) {
        const show_ev_t *e = &SHOW[k];
        uint32_t done = show_done[k / 32u] >> (k % 32u) & 1u, p = e->cc - 30u;
        int32_t v;
        if (done || t < e->t0) continue;
        if (!e->cc) { set_root(e->v0); show_done[k / 32u] |= 1u << (k % 32u); continue; }
        if (t >= e->t1) { v = e->v1; show_done[k / 32u] |= 1u << (k % 32u); }
        else v = e->v0 + (e->v1 - e->v0) * (int32_t)(t - e->t0) / (int32_t)(e->t1 - e->t0);
        st.p[p] = (int16_t)(PARAM[p].lo + (PARAM[p].hi - PARAM[p].lo) * v / 127);
        if (e->t1 > e->t0) { last_enc = (int32_t)p; enc_t = q_ms; }   /* (its name and value, as if turned) */
    }
}

static void quod_init(void)
{
    uint32_t i, w = 0;
    q_sine_init();                              /* (first: everything after may use the sine) */
    for (i = 0; i < 27u; i++) white_of[i] = key_black(i) ? -1 : (int8_t)w++;
    if (q_settings_load(&st, sizeof st) != 0) st.ver = 0;   /* (none, or an older one: the defaults) */
    if (st.ver != STORE_VER) {
        memset(&st, 0, sizeof st);
        st.ver = STORE_VER;
        for (i = 0; i < P_COUNT; i++) st.p[i] = PARAM[i].def;
        memset(st.lock, NOLOCK, sizeof st.lock);
        st.trig = 1u | 1u << 4 | 1u << 8 | 1u << 12;   /* C4, G4, Eb4, G3 on the beats */
        st.note[0] = 60; st.note[4] = 67; st.note[8] = 63; st.note[12] = 55;
    }
    effective();
    visc8 = st.p[P_SATURN] << 8;
    physics_init();
    splash_until = q_ms + SPLASH_MS;
    ready = 1;
}

static void quod_step(void)                     /* a pass of the main loop */
{
    uint32_t e;
    buttons();
    keys();
    midi_in();
    for (e = 0; e < 7u; e++) {
        int32_t d = q_enc_take(e);
        if (d && show_on) show_stop();          /* (a knob stops the showcase) */
        else if (d) knob(e, d);
    }
    show_poll();
    sequencer();
    effective();
    {                                           /* the audio's worst time this second, to the computer */
        static uint32_t rep_t;
        if (q_ms - rep_t >= 1000u) {
            uint32_t us = audio_us_win;
            rep_t = q_ms;
            audio_us_win = 0;
            q_midi_send(0xBF, 118, us >> 7 & 127u);
            q_midi_send(0xBF, 119, us & 127u);
        }
    }
    leds();
    draw();
}

static void app_main(void)
{
    quod_init();
    for (;;) {
        q_poll();
        quod_step();
    }
}
