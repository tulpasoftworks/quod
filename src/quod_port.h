/* SPDX-License-Identifier: MIT */
/* What Quod asks of its host. Quod is one translation unit: a host defines what is below, then includes quod.c.
 * Copyright (c) 2026 Kip DeCastro. See ../LICENSE.
 *
 * The panel (an M-VAVE FM-1's): 27 keys (bit 0 = F3 .. 26 = G5), 14 buttons and 7 encoders, numbered
 *   enum { BTN_OCT_DOWN = 0, BTN_OCT_UP = 1, BTN_FX = 2, BTN_SEL = 3, BTN_ENV = 4, BTN_LFO = 5, BTN_EDIT = 6,
 *          BTN_GLO = 7, BTN_HOME = 8, BTN_SAVE = 9, BTN_ARP = 10, BTN_SEQ = 11, BTN_PLAY = 12, BTN_REC = 13 };
 *   KEY_LED(k)                                 a key's LED id (for q_led)
 *
 * Time, input, output (the host's names may differ: #define them):
 *   volatile uint32_t q_ms                     milliseconds since start
 *   struct { uint32_t notes, buttons; } q_in   keys and buttons held now
 *   uint32_t q_button_edges(uint32_t *released)   buttons pressed since the last call (and, out, released)
 *   uint32_t q_note_edges(void)                keys pressed since the last call (a tap shorter than a poll)
 *   int32_t q_enc_take(uint32_t e)             encoder e's detents since the last call (+ clockwise)
 *   uint32_t q_enc_ms(uint32_t e, uint32_t *first)   when the last of them came, and the first (ms, any clock:
 *                                              only the gaps count; first = last when the host can't tell)
 *   void q_led(uint32_t id, int on)
 *   int q_midi_get(uint8_t *st, uint8_t *d1, uint8_t *d2)   1: a MIDI message in (status, data)
 *   void q_midi_send(uint32_t st, uint32_t d1, uint32_t d2)
 *   int q_settings_load(void *p, uint32_t n)  0: n bytes read back      int q_settings_save(const void *, uint32_t)
 *   void q_poll(void)                          the host's own main-loop duties (called every pass)
 *   void q_lcd_sync(void)                      wait until the last q_lcd_blit has read its pixels
 *   void q_lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *px)   onto the 240 x 240 screen
 *   uint32_t q_ticks(void), Q_TICKS_PER_US     a free-running counter (the audio's time is measured with it)
 *   void q_premaster(int on)                   probes: USB audio before the master volume (or nothing)
 *   void q_usb_stats(uint32_t v[10])           probes: the USB audio's glitch counters (or zeros)
 *   memcpy, memset
 *
 * Optional, with these defaults:
 *   Q_VERSION    the firmware's version, shown while GLO is held        ("DEV")
 *   Q_POOL       where the big buffers go (a section attribute)        (nothing)
 *   Q_AUDIO_FN   on the functions the audio calls (e.g. run from RAM)  (noinline)
 *
 * Quod gives the host:
 *   void app_audio(int32_t *out, uint32_t n)   n stereo frames at 44.1 kHz, added into out (L R L R, Q15)
 *   void app_tick_ms(void)                     once a millisecond: the physics (an interrupt, or between blocks)
 *   void quod_init(void)                       once, before the others
 *   void quod_step(void)                       a pass of the main loop: input, the sequencer, the screen
 *   void app_main(void)                        quod_init, then q_poll and quod_step for ever
 */
#ifndef Q_VERSION
#define Q_VERSION "DEV"
#endif
#ifndef Q_POOL
#define Q_POOL
#endif
#ifndef Q_AUDIO_FN
#define Q_AUDIO_FN __attribute__((noinline))
#endif
