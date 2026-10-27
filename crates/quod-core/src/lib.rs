// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Kip DeCastro. See ../../LICENSE.
//
//! Quod's engine for Rust hosts: the C engine (`engine/quod_engine.h`) compiled in, one [`Slot`] an instrument.
//! Quod keeps its state in its own statics, so each slot is the engine compiled once more: on the web there is one,
//! elsewhere eight ([`Slot::take`] finds a free one, as the Audio Unit's instances do).
#![no_std]

use core::ffi::{c_char, c_int, CStr};

pub const FS: u32 = 44_100;
pub const W: usize = 240;
pub const H: usize = 240;
pub const BACKUP_MAX: usize = 268;
pub const STATE_MAX: usize = 512;

#[repr(C)]
pub struct RawSlot {
    pub init: extern "C" fn(),
    pub render: extern "C" fn(*mut f32, *mut f32, u32),
    pub midi: extern "C" fn(u8, u8, u8),
    pub param_count: extern "C" fn() -> u32,
    pub param_name: extern "C" fn(u32) -> *const c_char,
    pub param_range: extern "C" fn(u32, *mut i32, *mut i32, *mut i32),
    pub param_get: extern "C" fn(u32) -> i32,
    pub param_set: extern "C" fn(u32, i32),
    pub state_get: extern "C" fn(*mut u8, u32) -> u32,
    pub state_set: extern "C" fn(*const u8, u32) -> c_int,
    pub button: extern "C" fn(u32, c_int),
    pub key: extern "C" fn(u32, c_int),
    pub enc: extern "C" fn(u32, i32),
    pub screen: extern "C" fn(*mut u32) -> *const u16,
    pub leds: extern "C" fn() -> u64,
    pub enc_name: extern "C" fn(u32) -> *const c_char,
    pub enc_level: extern "C" fn(u32, *mut c_int) -> i32,
    pub backup_get: extern "C" fn(*mut u8, u32) -> u32,
    pub backup_set: extern "C" fn(*const u8, u32) -> c_int,
}

extern "C" {
    static quod_slot_0: RawSlot;
}
#[cfg(not(target_arch = "wasm32"))]
extern "C" {
    static QUOD_SLOT: [&'static RawSlot; 8];
}

/// Why a backup wasn't taken (`engine/quod_engine.h`).
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum BackupError {
    NotABackup,
    Damaged,
    OtherVersion,
}

/// One instrument.
#[derive(Clone, Copy)]
pub struct Slot {
    raw: &'static RawSlot,
    index: usize,
}

#[cfg(not(target_arch = "wasm32"))]
static TAKEN: [core::sync::atomic::AtomicBool; 8] = [const { core::sync::atomic::AtomicBool::new(false) }; 8];

impl Slot {
    /// The first slot (the web's only one). Not counted as taken.
    pub fn first() -> Slot {
        Slot { raw: unsafe { &quod_slot_0 }, index: 0 }
    }

    /// A free slot, initialised; `None` when all eight are in use. Give it back with [`Slot::give_back`].
    #[cfg(not(target_arch = "wasm32"))]
    pub fn take() -> Option<Slot> {
        use core::sync::atomic::Ordering;
        let i = TAKEN.iter().position(|t| t.compare_exchange(false, true, Ordering::AcqRel, Ordering::Acquire).is_ok())?;
        let s = Slot { raw: unsafe { QUOD_SLOT[i] }, index: i };
        s.init();
        Some(s)
    }

    #[cfg(not(target_arch = "wasm32"))]
    pub fn give_back(self) {
        TAKEN[self.index].store(false, core::sync::atomic::Ordering::Release);
    }

    pub fn raw(&self) -> &'static RawSlot {
        self.raw
    }
    pub fn index(&self) -> usize {
        self.index
    }

    /// (Again on reuse: the controls back to their defaults.)
    pub fn init(&self) {
        (self.raw.init)()
    }
    /// Frames at 44.1 kHz (`l` and `r` the same length).
    pub fn render(&self, l: &mut [f32], r: &mut [f32]) {
        let n = l.len().min(r.len());
        (self.raw.render)(l.as_mut_ptr(), r.as_mut_ptr(), n as u32)
    }
    pub fn midi(&self, st: u8, d1: u8, d2: u8) {
        (self.raw.midi)(st, d1, d2)
    }
    pub fn param_count(&self) -> u32 {
        (self.raw.param_count)()
    }
    pub fn param_name(&self, i: u32) -> &'static str {
        unsafe { CStr::from_ptr((self.raw.param_name)(i)) }.to_str().unwrap_or("")
    }
    /// (lo, hi, default)
    pub fn param_range(&self, i: u32) -> (i32, i32, i32) {
        let (mut lo, mut hi, mut def) = (0, 0, 0);
        (self.raw.param_range)(i, &mut lo, &mut hi, &mut def);
        (lo, hi, def)
    }
    pub fn param_get(&self, i: u32) -> i32 {
        (self.raw.param_get)(i)
    }
    pub fn param_set(&self, i: u32, v: i32) {
        (self.raw.param_set)(i, v)
    }
    /// The project's state (the controls, steps, notes, locks): bytes written, 0 if `buf` is too small.
    pub fn state_get(&self, buf: &mut [u8]) -> usize {
        (self.raw.state_get)(buf.as_mut_ptr(), buf.len() as u32) as usize
    }
    pub fn state_set(&self, buf: &[u8]) -> bool {
        (self.raw.state_set)(buf.as_ptr(), buf.len() as u32) == 0
    }
    /// The state as an FM-1 backup (a .quodstate file).
    pub fn backup_get(&self, buf: &mut [u8]) -> usize {
        (self.raw.backup_get)(buf.as_mut_ptr(), buf.len() as u32) as usize
    }
    pub fn backup_set(&self, buf: &[u8]) -> Result<(), BackupError> {
        match (self.raw.backup_set)(buf.as_ptr(), buf.len() as u32) {
            0 => Ok(()),
            -2 => Err(BackupError::Damaged),
            -3 => Err(BackupError::OtherVersion),
            _ => Err(BackupError::NotABackup),
        }
    }
    /// Button 0..13 (`src/quod_port.h`).
    pub fn button(&self, b: u32, down: bool) {
        (self.raw.button)(b, down as c_int)
    }
    /// Key 0 (F3) ..= 26 (G5).
    pub fn key(&self, k: u32, down: bool) {
        (self.raw.key)(k, down as c_int)
    }
    pub fn enc(&self, e: u32, detents: i32) {
        (self.raw.enc)(e, detents)
    }
    /// The screen (RGB565, byte-swapped as the FM-1's panel takes it) and how many blits it has had.
    pub fn screen(&self) -> (&'static [u16; W * H], u32) {
        let mut seq = 0;
        let p = (self.raw.screen)(&mut seq);
        (unsafe { &*(p as *const [u16; W * H]) }, seq)
    }
    /// Bit id: buttons 0..13, keys 14..40.
    pub fn leds(&self) -> u64 {
        (self.raw.leds)()
    }
    /// What encoder `e` turns now ("": nothing).
    pub fn enc_name(&self, e: u32) -> &'static str {
        unsafe { CStr::from_ptr((self.raw.enc_name)(e)) }.to_str().unwrap_or("")
    }
    /// Where its dial is (0..=32767) and whether that's a step's lock; `None`: nothing on it.
    pub fn enc_level(&self, e: u32) -> Option<(i32, bool)> {
        let mut lock = 0;
        let v = (self.raw.enc_level)(e, &mut lock);
        (v >= 0).then_some((v, lock != 0))
    }
}

/// RGB565 byte-swapped (the screen's) -> 8-bit RGB.
pub fn rgb(px: u16) -> [u8; 3] {
    let v = px.swap_bytes() as u32;
    [((v >> 11) * 255 / 31) as u8, ((v >> 5 & 63) * 255 / 63) as u8, ((v & 31) * 255 / 31) as u8]
}

/// From the engine's 44.1 kHz to the host's rate (`engine/quod_resample.h`): a straight copy at 44.1 kHz.
#[cfg(not(target_arch = "wasm32"))]
pub struct Resampler(*mut core::ffi::c_void);

#[cfg(not(target_arch = "wasm32"))]
extern "C" {
    fn quod_rs_new(src: *const RawSlot, out_rate: f64) -> *mut core::ffi::c_void;
    fn quod_rs_free(rs: *mut core::ffi::c_void);
    fn quod_rs_render(rs: *mut core::ffi::c_void, l: *mut f32, r: *mut f32, n: u32);
}

#[cfg(not(target_arch = "wasm32"))]
impl Resampler {
    pub fn new(slot: Slot, out_rate: f64) -> Option<Resampler> {
        let p = unsafe { quod_rs_new(slot.raw, out_rate) };
        (!p.is_null()).then_some(Resampler(p))
    }
    pub fn render(&mut self, l: &mut [f32], r: &mut [f32]) {
        let n = l.len().min(r.len());
        unsafe { quod_rs_render(self.0, l.as_mut_ptr(), r.as_mut_ptr(), n as u32) }
    }
}
#[cfg(not(target_arch = "wasm32"))]
impl Drop for Resampler {
    fn drop(&mut self) {
        unsafe { quod_rs_free(self.0) }
    }
}
#[cfg(not(target_arch = "wasm32"))]
unsafe impl Send for Resampler {}
