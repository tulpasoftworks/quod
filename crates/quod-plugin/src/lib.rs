// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Kip DeCastro. See ../../LICENSE.
//
//! Quod as a VST3 and CLAP instrument, as the Audio Unit is on a Mac (apple/Extension): an engine slot each
//! instance (eight at once), resampled to the host's rate; the controls as automatable parameters; the project keeps
//! the controls, steps, notes and locks; and the FM-1's panel (panel.rs). A .vst3 is GPL-3.0 (nih-plug's VST3
//! bindings); see Cargo.toml.

use nih_plug::prelude::*;
use nih_plug_egui::EguiState;
use quod_core::{Resampler, Slot, STATE_MAX};
use std::collections::BTreeMap;
use std::sync::{Arc, Mutex};

mod panel;

pub struct QuodParams {
    /// The engine's controls, in its order ("p0".."p17", as the Audio Unit's addresses).
    pub controls: Vec<IntParam>,
    pub editor: Arc<EguiState>,
    /// Quod's state (engine/quod_engine.h's state_get), kept current by the audio thread for the host's project.
    pub state: Mutex<Vec<u8>>,
}

fn title_case(s: &str) -> String {
    // "HEAVEN / HELL" -> "Heaven / Hell"
    let mut t = String::new();
    let mut start = true;
    for c in s.chars() {
        t.push(if start { c } else { c.to_ascii_lowercase() });
        start = c == ' ' || c == '/';
    }
    t
}

impl QuodParams {
    fn new() -> Self {
        let s = Slot::first();
        let controls = (0..s.param_count())
            .map(|i| {
                let (lo, hi, def) = s.param_range(i);
                IntParam::new(title_case(s.param_name(i)), def, IntRange::Linear { min: lo, max: hi })
            })
            .collect();
        QuodParams {
            controls,
            editor: EguiState::from_size(panel::WIDTH, panel::HEIGHT),
            state: Mutex::new(Vec::with_capacity(STATE_MAX)),
        }
    }
}

unsafe impl Params for QuodParams {
    fn param_map(&self) -> Vec<(String, ParamPtr, String)> {
        self.controls.iter().enumerate().map(|(i, p)| (format!("p{i}"), p.as_ptr(), String::new())).collect()
    }
    fn serialize_fields(&self) -> BTreeMap<String, String> {
        let st = self.state.lock().unwrap();
        let hex: String = st.iter().map(|b| format!("{b:02x}")).collect();
        BTreeMap::from([("quod".to_string(), hex)])
    }
    fn deserialize_fields(&self, serialized: &BTreeMap<String, String>) {
        if let Some(hex) = serialized.get("quod") {
            let bytes: Vec<u8> = (0..hex.len() / 2).filter_map(|i| u8::from_str_radix(&hex[2 * i..2 * i + 2], 16).ok()).collect();
            *self.state.lock().unwrap() = bytes;
        }
    }
}

pub struct Quod {
    params: Arc<QuodParams>,
    slot: Option<Slot>,
    rs: Option<Resampler>,
    applied: Vec<i32>,                          // each control as last given to the engine
    since_kept: f32,                            // seconds since the state was copied for the project
}

impl Default for Quod {
    fn default() -> Self {
        let params = Arc::new(QuodParams::new());
        let n = params.controls.len();
        Quod { params, slot: Slot::take(), rs: None, applied: vec![i32::MIN; n], since_kept: 0.0 }
    }
}

impl Drop for Quod {
    fn drop(&mut self) {
        if let Some(s) = self.slot.take() {
            s.give_back();
        }
    }
}

impl Plugin for Quod {
    const NAME: &'static str = "Quod";
    const VENDOR: &'static str = "Tulpa SoftWorks";
    const URL: &'static str = "https://github.com/tulpasoftworks/quod";
    const EMAIL: &'static str = "";
    const VERSION: &'static str = env!("CARGO_PKG_VERSION");

    const AUDIO_IO_LAYOUTS: &'static [AudioIOLayout] = &[AudioIOLayout {
        main_input_channels: None,
        main_output_channels: NonZeroU32::new(2),
        ..AudioIOLayout::const_default()
    }];
    const MIDI_INPUT: MidiConfig = MidiConfig::MidiCCs;
    const SAMPLE_ACCURATE_AUTOMATION: bool = false;

    type SysExMessage = ();
    type BackgroundTask = ();

    fn params(&self) -> Arc<dyn Params> {
        self.params.clone()
    }

    fn editor(&mut self, _async_executor: AsyncExecutor<Self>) -> Option<Box<dyn Editor>> {
        panel::editor(self.params.clone(), self.slot)
    }

    fn initialize(&mut self, _layout: &AudioIOLayout, config: &BufferConfig, _ctx: &mut impl InitContext<Self>) -> bool {
        let Some(slot) = self.slot else { return true };   // (every slot in use: silence, and the panel says so)
        let kept = self.params.state.lock().unwrap();
        if !kept.is_empty() {
            slot.state_set(&kept);
        }
        drop(kept);
        self.applied.fill(i32::MIN);
        self.rs = Resampler::new(slot, config.sample_rate as f64);
        self.rs.is_some()
    }

    fn process(&mut self, buffer: &mut Buffer, _aux: &mut AuxiliaryBuffers, ctx: &mut impl ProcessContext<Self>) -> ProcessStatus {
        let (Some(slot), Some(rs)) = (self.slot, self.rs.as_mut()) else {
            for ch in buffer.as_slice() {
                ch.fill(0.0);
            }
            return ProcessStatus::Normal;
        };
        for (i, p) in self.params.controls.iter().enumerate() {   // the host's controls (automation) -> the engine
            let v = p.value();
            if v != self.applied[i] {
                self.applied[i] = v;
                slot.param_set(i as u32, v);
            }
        }
        while let Some(ev) = ctx.next_event() {
            let byte = |v: f32| (v * 127.0).round().clamp(0.0, 127.0) as u8;
            match ev {
                NoteEvent::NoteOn { channel, note, velocity, .. } => slot.midi(0x90 | channel, note, byte(velocity).max(1)),
                NoteEvent::NoteOff { channel, note, .. } => slot.midi(0x80 | channel, note, 0),
                NoteEvent::MidiCC { channel, cc, value, .. } => slot.midi(0xB0 | channel, cc, byte(value)),
                _ => {}
            }
        }
        let n = buffer.samples();
        if let [l, r, ..] = buffer.as_slice() {      // (stereo: the only layout)
            rs.render(l, r);
        }
        self.since_kept += n as f32 / ctx.transport().sample_rate;
        if self.since_kept >= 0.5 {                 // the state, for the project (without waiting on the editor)
            if let Ok(mut kept) = self.params.state.try_lock() {
                self.since_kept = 0.0;
                kept.resize(STATE_MAX, 0);          // (within its capacity: no allocation)
                let got = slot.state_get(&mut kept);
                kept.truncate(got);
            }
        }
        ProcessStatus::Normal
    }
}

impl ClapPlugin for Quod {
    const CLAP_ID: &'static str = "com.tulpasoftworks.quod";
    const CLAP_DESCRIPTION: Option<&'static str> = Some("A drone between heaven and hell");
    const CLAP_MANUAL_URL: Option<&'static str> = Some(Self::URL);
    const CLAP_SUPPORT_URL: Option<&'static str> = None;
    const CLAP_FEATURES: &'static [ClapFeature] = &[ClapFeature::Instrument, ClapFeature::Synthesizer, ClapFeature::Stereo];
}

impl Vst3Plugin for Quod {
    const VST3_CLASS_ID: [u8; 16] = *b"TlpaQuodDroneVs3";
    const VST3_SUBCATEGORIES: &'static [Vst3SubCategory] = &[Vst3SubCategory::Instrument, Vst3SubCategory::Synth];
}

nih_export_clap!(Quod);
nih_export_vst3!(Quod);
