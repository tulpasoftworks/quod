// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Kip DeCastro. See ../../LICENSE.
//
//! Quod in the browser. web/worklet.js instantiates this module in an AudioWorklet (at 44.1 kHz: the browser
//! resamples) and calls these exports; there is no other glue. Buffers live in this module's memory: the worklet
//! reads them through the pointers below. Everything runs on the audio thread, a millisecond at a time, as the
//! Audio Unit's engine does: the physics, the main loop (input, sequencer, screen), then the sound.
#![cfg_attr(target_arch = "wasm32", no_std)]

use quod_core::{Slot, BACKUP_MAX, STATE_MAX, W, H};

#[cfg(target_arch = "wasm32")]
#[panic_handler]
fn panic(_: &core::panic::PanicInfo) -> ! {
    core::arch::wasm32::unreachable()
}

const MAX_FRAMES: usize = 1024;
struct Mem {
    l: [f32; MAX_FRAMES],
    r: [f32; MAX_FRAMES],
    buf: [u8; STATE_MAX],                       // state and backups, both ways
    rgba: [u8; W * H * 4],
    clock_us: u64,
    seq: u32,
}
struct Cell(core::cell::UnsafeCell<Mem>);
unsafe impl Sync for Cell {}                    // (one thread: the worklet's)
static MEM: Cell = Cell(core::cell::UnsafeCell::new(Mem {
    l: [0.0; MAX_FRAMES], r: [0.0; MAX_FRAMES], buf: [0; STATE_MAX], rgba: [0; W * H * 4], clock_us: 0, seq: 0,
}));
fn mem() -> &'static mut Mem {
    unsafe { &mut *MEM.0.get() }
}

/// The engine's clock (engine/quod_engine.h): the worklet sets it before each call (its audio clock).
#[no_mangle]
pub extern "C" fn quod_clock_us() -> u64 {
    mem().clock_us
}
#[no_mangle]
pub extern "C" fn quod_set_clock_us(t: f64) {
    mem().clock_us = t as u64
}

#[no_mangle]
pub extern "C" fn quod_init() {
    Slot::first().init()
}

/// Renders `n` frames (at most 1024) into the buffers quod_left / quod_right point at.
#[no_mangle]
pub extern "C" fn quod_render(n: u32) {
    let n = (n as usize).min(MAX_FRAMES);
    let m = mem();
    Slot::first().render(&mut m.l[..n], &mut m.r[..n])
}
#[no_mangle]
pub extern "C" fn quod_left() -> *const f32 {
    mem().l.as_ptr()
}
#[no_mangle]
pub extern "C" fn quod_right() -> *const f32 {
    mem().r.as_ptr()
}

#[no_mangle]
pub extern "C" fn quod_midi(st: u32, d1: u32, d2: u32) {
    Slot::first().midi(st as u8, d1 as u8, d2 as u8)
}
#[no_mangle]
pub extern "C" fn quod_button(b: u32, down: u32) {
    Slot::first().button(b, down != 0)
}
#[no_mangle]
pub extern "C" fn quod_key(k: u32, down: u32) {
    Slot::first().key(k, down != 0)
}
#[no_mangle]
pub extern "C" fn quod_enc(e: u32, d: i32) {
    Slot::first().enc(e, d)
}

/// The screen as RGBA, when it changed since the last call (else 0): quod_rgba points at it.
#[no_mangle]
pub extern "C" fn quod_screen_changed() -> u32 {
    let (fb, seq) = Slot::first().screen();
    let m = mem();
    if seq == m.seq {
        return 0;
    }
    m.seq = seq;
    for (i, &px) in fb.iter().enumerate() {
        let c = quod_core::rgb(px);
        m.rgba[4 * i..4 * i + 4].copy_from_slice(&[c[0], c[1], c[2], 255]);
    }
    1
}
#[no_mangle]
pub extern "C" fn quod_rgba() -> *const u8 {
    mem().rgba.as_ptr()
}
#[no_mangle]
pub extern "C" fn quod_leds_lo() -> u32 {
    Slot::first().leds() as u32
}
#[no_mangle]
pub extern "C" fn quod_leds_hi() -> u32 {
    (Slot::first().leds() >> 32) as u32
}
/// What encoder e turns now, a NUL-terminated string ("": nothing).
#[no_mangle]
pub extern "C" fn quod_enc_name(e: u32) -> *const u8 {
    Slot::first().enc_name(e).as_ptr()
}
#[no_mangle]
pub extern "C" fn quod_enc_name_len(e: u32) -> u32 {
    Slot::first().enc_name(e).len() as u32
}
/// Its dial: 0..32767, + 65536 for a step's lock; -1: nothing on it.
#[no_mangle]
pub extern "C" fn quod_enc_level(e: u32) -> i32 {
    match Slot::first().enc_level(e) {
        Some((v, lock)) => v + if lock { 65536 } else { 0 },
        None => -1,
    }
}

/// The shared buffer (STATE_MAX bytes) for the calls below.
#[no_mangle]
pub extern "C" fn quod_buf() -> *mut u8 {
    mem().buf.as_mut_ptr()
}
#[no_mangle]
pub extern "C" fn quod_state_get() -> u32 {
    Slot::first().state_get(&mut mem().buf) as u32
}
#[no_mangle]
pub extern "C" fn quod_state_set(n: u32) -> u32 {
    Slot::first().state_set(&mem().buf[..(n as usize).min(STATE_MAX)]) as u32
}
#[no_mangle]
pub extern "C" fn quod_backup_get() -> u32 {
    Slot::first().backup_get(&mut mem().buf[..BACKUP_MAX]) as u32
}
/// 0 taken; 1 not a backup, 2 damaged, 3 another version.
#[no_mangle]
pub extern "C" fn quod_backup_set(n: u32) -> u32 {
    use quod_core::BackupError::*;
    match Slot::first().backup_set(&mem().buf[..(n as usize).min(STATE_MAX)]) {
        Ok(()) => 0,
        Err(NotABackup) => 1,
        Err(Damaged) => 2,
        Err(OtherVersion) => 3,
    }
}
