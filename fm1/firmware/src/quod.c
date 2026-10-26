/* SPDX-License-Identifier: GPL-3.0-only */
/* Quod on the FM-1: the platform (this firmware's, GPL) around Quod itself (MIT, the rest of this repository:
 * build.sh copies ../src and ../gen into firmware/shared/quod). Here: what quod's src/quod_port.h asks of a host.
 * Copyright (C) 2026 Kip DeCastro. */
#define APP_NAME "QUOD"
#define LCD_BAUD 1u                             /* the screen's SPI at 30 MHz (60 / (1 + 1)): a whole screen in
                                                 * ~31 ms, not ~77 (the others keep 12 MHz) */
#define AUDIO_IN_RAM 1                          /* the sound's code runs from RAM: the planetoid's drawing
                                                 * evicted it from the XIP cache (+0.4 ms a block, underruns) */
#include "platform.c"

#define q_ms fm1_ms
#define q_in fm1_in
#define q_button_edges fm1_input_edges
#define q_note_edges fm1_input_note_edges
#define q_enc_take fm1_enc_take
#define q_enc_ms fm1_enc_ms
#define q_led fm1_led_key
#define q_midi_get midi_get
#define q_midi_send midi_send
#define q_settings_load settings_load
#define q_settings_save settings_save
#define q_poll platform_poll
#define q_lcd_sync lcd_sync
#define q_lcd_blit lcd_blit
#define Q_POOL __attribute__((section(".pool")))
#define Q_AUDIO_FN AUDIO_FN
#ifdef DINGHY_HOST
static uint32_t q_ticks(void) { return 0; }
#define Q_TICKS_PER_US 24u
#else
#define q_ticks fm1_ticks
#define Q_TICKS_PER_US FM1_TICKS_PER_US
#endif
static void q_premaster(int on) { uac_premaster = (uint32_t)on; }
static void q_usb_stats(uint32_t v[10])
{
#if !defined(DINGHY_HOST) && FELUCCA_UAC
    v[0] = uac.underruns; v[1] = uac.overruns; v[2] = uac.missed; v[3] = uac.adj_up; v[4] = uac.adj_down;
    v[5] = uac.fill_lo; v[6] = uac.fill_hi; v[7] = uac.stalls; v[8] = uac.pkts; v[9] = uac.frames;
#else
    uint32_t k;
    for (k = 0; k < 10u; k++) v[k] = 0;
#endif
}

#include "quod/quod.c"
