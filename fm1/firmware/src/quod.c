/* SPDX-License-Identifier: GPL-3.0-only */
/* Quod on the FM-1: the platform (this firmware's, GPL) around Quod itself (MIT, the rest of this repository:
 * build.sh copies ../src and ../gen into firmware/shared/quod). Here: what quod's src/quod_port.h asks of a host.
 * Copyright (C) 2026 Kip DeCastro. */
#define APP_NAME "QUOD"
#define LCD_BAUD 1u                             /* the screen's SPI at 30 MHz (60 / (1 + 1)): a whole screen in
                                                 * ~31 ms, not ~77 (the others keep 12 MHz) */
#define AUDIO_IN_RAM 1                          /* the sound's code runs from RAM: the planetoid's drawing
                                                 * evicted it from the XIP cache (+0.4 ms a block, underruns) */
#ifndef DINGHY_HOST                             /* (tests/host.c has no updater) */
#include <stdint.h>
static void quod_sysex(uint32_t cmd, const uint8_t *body, uint32_t n);
#define OTA_APP_CMD quod_sysex                  /* the state backup (below), on the updater's SysEx path */
#endif
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
#define Q_VERSION APP_VER                       /* (GLO held shows it: "0.2", or "DEV" for a development build) */
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

/* ---- the state backup, over the updater's SysEx framing (ota.c: F0 pack7(00 59 cmd len24 body ~sum) F7), outside
 * an update. fm1_install.py --backup / --restore and the browser flasher use it:
 *   0x51 (no body)        -> 0x52 [rc] [record]: rc 0 and the state playing now (saved or not), as the settings
 *                            sector keeps it ("DNGY", length, CRC-32, Quod's state: a .quodstate file); rc 5 not
 *                            running (the USB rescue)
 *   0x53 [record]         -> 0x54 [rc]: rc 0 it plays and is saved; 1 not a backup, 2 damaged, 3 another version of
 *                            Quod's state, 4 the flash failed, 5 not running
 * The save erases the settings sector: the sound stops for ~50 ms. */
#ifndef DINGHY_HOST
static uint8_t qsx_msg[8 + 1 + 12 + sizeof st], qsx_wire[2 + (sizeof qsx_msg * 8 + 6) / 7];
static void quod_sx_send(uint32_t cmd, const uint8_t *body, uint32_t n)
{
    uint32_t i, s = 0, w;
    qsx_msg[0] = 0; qsx_msg[1] = 0x59; qsx_msg[2] = (uint8_t)cmd;
    qsx_msg[3] = (uint8_t)n; qsx_msg[4] = (uint8_t)(n >> 8); qsx_msg[5] = 0;
    for (i = 0; i < n; i++) { qsx_msg[6 + i] = body[i]; s += body[i]; }
    qsx_msg[6 + n] = (uint8_t)~s;
    qsx_wire[0] = 0xF0;
    w = 1u + ota_pack7(qsx_msg, 7u + n, qsx_wire + 1);
    qsx_wire[w++] = 0xF7;
    ota_wire_send(qsx_wire, w);
}
#include <stdint.h>
static void quod_sysex(uint32_t cmd, const uint8_t *b, uint32_t n)
{
    static uint8_t r[1 + 12 + sizeof st];
    static __typeof__(st) in;
    uint32_t h[3];
    if (cmd != 0x51u && cmd != 0x53u) return;
    if (!ready || recovery_active) { r[0] = 5; quod_sx_send(cmd + 1u, r, 1); return; }
    if (cmd == 0x51u) {
        h[0] = SETTINGS_MAGIC; h[1] = sizeof st; h[2] = crc32((const uint8_t *)&st, sizeof st);
        r[0] = 0;
        memcpy(r + 1, h, sizeof h);
        memcpy(r + 13, &st, sizeof st);
        quod_sx_send(0x52, r, sizeof r);
        return;
    }
    memcpy(h, b, n >= 12u ? 12u : 0u);
    if (n < 16u || h[0] != SETTINGS_MAGIC || h[1] != n - 12u) r[0] = 1;
    else if (crc32(b + 12, n - 12u) != h[2]) r[0] = 2;
    else if (n - 12u != sizeof st || (memcpy(&in, b + 12, sizeof in), in.ver != STORE_VER)) r[0] = 3;
    else {
        st = in;
        dirty = 0; full_redraw = 1;
        r[0] = settings_save(&st, sizeof st) ? 4 : 0;
    }
    quod_sx_send(0x54, r, 1);
}
#endif
