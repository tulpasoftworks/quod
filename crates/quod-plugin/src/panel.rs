// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Kip DeCastro. See ../../LICENSE.
//
// The FM-1's panel in the plug-in, as the Audio Unit's (apple/Extension/QuodPanelView.m) and the browser's (web/app.js):
// the same layout in the same units (520 x 880), scaled. Press a button or a key; drag an encoder up or down, or
// scroll on it; Shift-click a button to latch it. Below: a .quodstate file (an FM-1 backup) saved or opened.

use crate::QuodParams;
use nih_plug::prelude::*;
use nih_plug_egui::egui::{self, Align2, Color32, ColorImage, FontId, Pos2, Rect, Sense, Stroke, TextureHandle, TextureOptions, Vec2};
use nih_plug_egui::create_egui_editor;
use quod_core::{Slot, BACKUP_MAX};
use std::sync::Arc;
use std::time::{Duration, Instant};

const PW: f32 = 520.0;
const PH: f32 = 880.0;
const K: f32 = 0.9;                             // the window's scale
const BAR: f32 = 40.0;                          // the state's row, below the panel (window pixels)
pub const WIDTH: u32 = (PW * K) as u32;
pub const HEIGHT: u32 = (PH * K + BAR) as u32;

const BTN_NAME: [&str; 14] = ["OCT \u{2212}", "OCT +", "FX", "SHIFT", "", "", "SETTLE", "SHOWCASE", "HOME", "SAVE", "", "STEPS", "PLAY", "REC"];
const B_GLO: usize = 7;
const B_HOME: u32 = 8;
const B_REC: u32 = 13;
const BTN_POS: [Option<(usize, usize)>; 14] = [Some((0, 0)), Some((0, 1)), Some((0, 4)), Some((0, 2)), None, None, Some((0, 3)),
    Some((1, 4)), Some((0, 5)), Some((1, 3)), None, Some((1, 0)), Some((1, 1)), Some((1, 2))];
const ROW_N: [f32; 2] = [6.0, 5.0];
const BLACK_KEY: [bool; 12] = [false, true, false, true, false, true, false, false, true, false, true, false];   // from F
const SCR_X: f32 = 20.0;
const SCR_Y: f32 = 20.0;
const SCR_S: f32 = 480.0;
const ENC_Y: f32 = 540.0;
const ENC_DY: f32 = 52.0;
const ENC_R: f32 = 17.0;
const BTN_Y0: f32 = 646.0;
const BTN_H: f32 = 30.0;
const BTN_GAP: f32 = 8.0;
const KEY_Y: f32 = 740.0;
const KEY_H: f32 = 120.0;
const KEY_BH: f32 = 74.0;
const ENC_POS: [(f32, f32); 7] = [(1.0, 0.0), (1.0, 1.0), (2.0, 0.0), (3.0, 0.0), (4.0, 0.0), (5.0, 0.0), (0.0, 1.0)];
const WHITE_W: f32 = SCR_S / 16.0;
const LIT: Color32 = Color32::from_rgb(242, 89, 64);

fn grey(v: f32) -> Color32 {
    let c = (v * 255.0) as u8;
    Color32::from_rgb(c, c, c)
}
fn enc_centre(e: usize) -> Pos2 {
    Pos2::new(SCR_X + SCR_S * (ENC_POS[e].0 + 0.5) / 6.0, ENC_Y + ENC_POS[e].1 * ENC_DY)
}
fn btn_rect(b: usize) -> Option<Rect> {
    let (row, col) = BTN_POS[b]?;
    let w = (SCR_S - (ROW_N[row] - 1.0) * BTN_GAP) / ROW_N[row];
    Some(Rect::from_min_size(Pos2::new(SCR_X + col as f32 * (w + BTN_GAP), BTN_Y0 + row as f32 * (BTN_H + BTN_GAP)), Vec2::new(w, BTN_H)))
}
fn white_index(k: usize) -> f32 {
    (0..k).filter(|i| !BLACK_KEY[i % 12]).count() as f32
}
fn key_rect(k: usize) -> Rect {
    if !BLACK_KEY[k % 12] {
        Rect::from_min_size(Pos2::new(SCR_X + white_index(k) * WHITE_W, KEY_Y), Vec2::new(WHITE_W, KEY_H))
    } else {
        Rect::from_min_size(Pos2::new(SCR_X + white_index(k - 1) * WHITE_W + WHITE_W * 0.68, KEY_Y), Vec2::new(WHITE_W * 0.64, KEY_BH))
    }
}
fn key_at(p: Pos2) -> Option<usize> {
    (0..27).find(|&k| BLACK_KEY[k % 12] && key_rect(k).contains(p)).or_else(|| (0..27).find(|&k| !BLACK_KEY[k % 12] && key_rect(k).contains(p)))
}
fn button_at(p: Pos2) -> Option<usize> {
    (0..14).find(|&b| btn_rect(b).is_some_and(|r| r.contains(p)))
}
fn enc_at(p: Pos2) -> Option<usize> {
    (0..7).find(|&e| enc_centre(e).distance(p) <= ENC_R + 8.0)
}
fn title(s: &str) -> String {
    // "HEAVEN / HELL" -> "Heaven/Hell" (it fits under a knob)
    let c: Vec<char> = s.chars().collect();
    let (mut t, mut start) = (String::new(), true);
    for i in 0..c.len() {
        if c[i] == ' ' && (c.get(i + 1) == Some(&'/') || (i > 0 && c[i - 1] == '/')) {
            continue;
        }
        t.push(if start { c[i] } else { c[i].to_ascii_lowercase() });
        start = c[i] == ' ' || c[i] == '/';
    }
    t
}

#[derive(Clone, Copy, PartialEq)]
enum Hit {
    Button(usize),
    Key(usize),
    Enc(usize),
}

struct Panel {
    slot: Option<Slot>,
    tex: Option<TextureHandle>,
    seq: u32,
    down: Option<Hit>,
    drag: f32,
    scroll: [f32; 7],
    latched: u32,
    show_until: Option<Instant>,
    engine: Vec<i32>,                           // each control as the engine last had it
    msg: String,
}

pub fn editor(params: Arc<QuodParams>, slot: Option<Slot>) -> Option<Box<dyn Editor>> {
    let n = params.controls.len();
    let panel = Panel { slot, tex: None, seq: u32::MAX, down: None, drag: 0.0, scroll: [0.0; 7], latched: 0, show_until: None,
                        engine: vec![i32::MIN; n], msg: String::new() };
    // (each opening is a new egui context: the screen's texture is made again in it, not kept from the last one)
    create_egui_editor(params.editor.clone(), panel, |_, p| { p.tex = None; p.seq = u32::MAX; }, move |ctx, setter, p| {
        ctx.request_repaint_after(Duration::from_millis(33));
        if let Some(s) = p.slot {
            p.sync(s, &params, setter);
        }
        egui::CentralPanel::default().frame(egui::Frame::NONE.fill(grey(0.07))).show(ctx, |ui| {
            let (rect, _) = ui.allocate_exact_size(Vec2::new(PW * K, PH * K), Sense::click_and_drag());
            p.input(ctx, rect);
            p.draw(ctx, ui.painter_at(rect), rect);
            ui.horizontal(|ui| {
                ui.add_space(SCR_X * K);
                if ui.add_enabled(p.slot.is_some(), egui::Button::new("Save state\u{2026}")).clicked() {
                    p.save_file();
                }
                if ui.add_enabled(p.slot.is_some(), egui::Button::new("Open state\u{2026}")).clicked() {
                    p.open_file();
                }
                ui.label(egui::RichText::new(&p.msg).color(grey(0.65)).monospace());
                ui.with_layout(egui::Layout::right_to_left(egui::Align::Center), |ui| {
                    ui.add_space(SCR_X * K);
                    ui.label(egui::RichText::new(concat!("QUOD ", env!("CARGO_PKG_VERSION"))).color(grey(0.45)).monospace());
                });
            });
        });
    })
}

impl Panel {
    // the panel turned a control (or MIDI did): tell the host, for its automation
    fn sync(&mut self, s: Slot, params: &QuodParams, setter: &ParamSetter) {
        for (i, prm) in params.controls.iter().enumerate() {
            let v = s.param_get(i as u32);
            if v != self.engine[i] {
                if self.engine[i] != i32::MIN && v != prm.value() {
                    setter.begin_set_parameter(prm);
                    setter.set_parameter(prm, v);
                    setter.end_set_parameter(prm);
                }
                self.engine[i] = v;
            }
        }
        if self.show_until.is_some_and(|t| Instant::now() >= t) {
            s.button(B_HOME, false);
            s.button(B_REC, false);
            self.show_until = None;
        }
    }

    fn input(&mut self, ctx: &egui::Context, rect: Rect) {
        let Some(s) = self.slot else { return };
        let to_panel = |p: Pos2| Pos2::new((p.x - rect.min.x) / K, (p.y - rect.min.y) / K);
        let (pressed, released, pos, shift, delta, scroll) = ctx.input(|i| {
            (i.pointer.primary_pressed(), i.pointer.primary_released(), i.pointer.interact_pos().or(i.pointer.hover_pos()),
             i.modifiers.shift, i.pointer.delta(), i.smooth_scroll_delta.y)
        });
        let Some(pos) = pos.filter(|p| rect.contains(*p)).map(to_panel) else {
            if released {
                self.release(s);
            }
            return;
        };
        if pressed {
            if let Some(b) = button_at(pos) {
                if b == B_GLO {                     // SHOWCASE: HOME + REC, held 1.2 s
                    s.button(B_HOME, true);
                    s.button(B_REC, true);
                    self.show_until = Some(Instant::now() + Duration::from_millis(1200));
                } else if shift {
                    self.latched ^= 1 << b;
                    s.button(b as u32, self.latched >> b & 1 != 0);
                } else if self.latched >> b & 1 != 0 {
                    self.latched &= !(1 << b);
                    s.button(b as u32, false);
                } else {
                    self.down = Some(Hit::Button(b));
                    s.button(b as u32, true);
                }
            } else if let Some(k) = key_at(pos) {
                self.down = Some(Hit::Key(k));
                s.key(k as u32, true);
            } else if let Some(e) = enc_at(pos) {
                self.down = Some(Hit::Enc(e));
                self.drag = 0.0;
            }
        } else if let Some(Hit::Enc(e)) = self.down {   // up = clockwise, a detent every 6 units
            self.drag -= delta.y / K;
            while self.drag >= 6.0 { s.enc(e as u32, 1); self.drag -= 6.0; }
            while self.drag <= -6.0 { s.enc(e as u32, -1); self.drag += 6.0; }
        } else if let Some(Hit::Key(k)) = self.down {   // a glissando across the keys
            if let Some(k2) = key_at(pos).filter(|&k2| k2 != k) {
                s.key(k as u32, false);
                s.key(k2 as u32, true);
                self.down = Some(Hit::Key(k2));
            }
        }
        if released {
            self.release(s);
        }
        if scroll != 0.0 {
            if let Some(e) = enc_at(pos) {
                self.scroll[e] += scroll / 12.0;
                while self.scroll[e] >= 1.0 { s.enc(e as u32, 1); self.scroll[e] -= 1.0; }
                while self.scroll[e] <= -1.0 { s.enc(e as u32, -1); self.scroll[e] += 1.0; }
            }
        }
    }

    fn release(&mut self, s: Slot) {
        match self.down.take() {
            Some(Hit::Button(b)) => s.button(b as u32, false),
            Some(Hit::Key(k)) => s.key(k as u32, false),
            _ => {}
        }
    }

    fn draw(&mut self, ctx: &egui::Context, g: egui::Painter, rect: Rect) {
        let at = |p: Pos2| Pos2::new(rect.min.x + p.x * K, rect.min.y + p.y * K);
        let r = |r: Rect| Rect::from_min_max(at(r.min), at(r.max));
        let mono = |size: f32| FontId::monospace(size * K);
        g.rect_filled(rect, 0.0, grey(0.07));
        g.rect_filled(r(Rect::from_min_size(Pos2::new(SCR_X - 4.0, SCR_Y - 4.0), Vec2::splat(SCR_S + 8.0))), 0.0, Color32::BLACK);
        let Some(s) = self.slot else {
            g.text(at(Pos2::new(PW / 2.0, SCR_Y + 240.0)), Align2::CENTER_CENTER, "EVERY QUOD SLOT IS IN USE", mono(14.0), grey(0.5));
            return;
        };
        let (fb, seq) = s.screen();
        if seq != self.seq || self.tex.is_none() {
            self.seq = seq;
            let mut rgba = vec![0u8; 240 * 240 * 4];
            for (i, &px) in fb.iter().enumerate() {
                let c = quod_core::rgb(px);
                rgba[4 * i..4 * i + 4].copy_from_slice(&[c[0], c[1], c[2], 255]);
            }
            let img = ColorImage::from_rgba_unmultiplied([240, 240], &rgba);
            match &mut self.tex {
                Some(t) => t.set(img, TextureOptions::NEAREST),
                None => self.tex = Some(ctx.load_texture("quod-screen", img, TextureOptions::NEAREST)),
            }
        }
        if let Some(t) = &self.tex {
            let uv = Rect::from_min_max(Pos2::ZERO, Pos2::new(1.0, 1.0));
            g.image(t.id(), r(Rect::from_min_size(Pos2::new(SCR_X, SCR_Y), Vec2::splat(SCR_S))), uv, Color32::WHITE);
        }
        let leds = s.leds();
        for e in 0..7 {                             // the encoders
            let c = enc_centre(e);
            g.circle_filled(at(c), ENC_R * K, grey(if self.down == Some(Hit::Enc(e)) { 0.32 } else { 0.2 }));
            g.circle_stroke(at(c), ENC_R * K, Stroke::new(1.5 * K, grey(0.45)));
            if let Some((lv, lock)) = s.enc_level(e as u32) {   // its dial, as the screen draws it (gold: a lock)
                let rr = ENC_R - 4.0;
                let pt = |d: f32| at(Pos2::new(c.x + rr * d.to_radians().sin(), c.y - rr * d.to_radians().cos()));
                let to = -135.0 + 270.0 * lv as f32 / 32767.0;
                let all: Vec<Pos2> = (0..=48).map(|n| pt(-135.0 + n as f32 * 270.0 / 48.0)).collect();
                let lit: Vec<Pos2> = (0..=48).map(|n| -135.0 + n as f32 * 270.0 / 48.0).filter(|&d| d <= to).map(pt).collect();
                g.add(egui::Shape::line(all, Stroke::new(2.0 * K, grey(0.32))));
                g.add(egui::Shape::line(lit, Stroke::new(2.0 * K, if lock { Color32::from_rgb(242, 191, 64) } else { grey(0.85) })));
                g.rect_filled(Rect::from_center_size(pt(to), Vec2::splat(3.0 * K)), 0.0, Color32::WHITE);
            }
            let name = s.enc_name(e as u32);
            g.text(at(Pos2::new(c.x, c.y - ENC_R - 9.0)), Align2::CENTER_CENTER, title(name), mono(9.0),
                   grey(if name.is_empty() { 0.3 } else { 0.65 }));
        }
        for b in 0..14 {                            // the buttons, lit by Quod's LEDs
            let Some(br) = btn_rect(b) else { continue };
            let lit = leds >> b & 1 != 0;
            let held = self.down == Some(Hit::Button(b)) || self.latched >> b & 1 != 0 || (b == B_GLO && self.show_until.is_some());
            g.rect_filled(r(br), 5.0 * K, grey(if held { 0.42 } else { 0.17 }));
            if lit {
                g.rect_stroke(r(br), 5.0 * K, Stroke::new(2.0 * K, LIT), egui::StrokeKind::Inside);
            }
            g.text(at(br.center()), Align2::CENTER_CENTER, BTN_NAME[b], mono(11.0), if lit { Color32::WHITE } else { grey(0.75) });
        }
        for pass in [false, true] {                 // the keys: the white ones, then the black over them
            for k in (0..27).filter(|k| BLACK_KEY[k % 12] == pass) {
                let kr = key_rect(k);
                let down = self.down == Some(Hit::Key(k));
                let c = if pass { grey(if down { 0.35 } else { 0.1 }) } else { grey(if down { 0.72 } else { 0.9 }) };
                g.rect_filled(r(kr.shrink2(Vec2::new(0.5, 0.0))), 0.0, c);
                if leds >> (14 + k) & 1 != 0 {
                    g.rect_filled(r(Rect::from_min_size(Pos2::new(kr.center().x - 4.0, kr.max.y - 14.0), Vec2::splat(8.0))), 0.0, LIT);
                }
            }
        }
    }

    // a .quodstate file: the same as the FM-1's backup, the sim's and the Mac app's
    fn save_file(&mut self) {
        let Some(s) = self.slot else { return };
        let mut buf = [0u8; BACKUP_MAX];
        let n = s.backup_get(&mut buf);
        if let Some(path) = rfd::FileDialog::new().add_filter("Quod state", &["quodstate"]).set_file_name("quod.quodstate").save_file() {
            self.msg = match std::fs::write(&path, &buf[..n]) {
                Ok(()) => "Saved.".into(),
                Err(e) => format!("Not saved: {e}"),
            };
        }
    }
    fn open_file(&mut self) {
        let Some(s) = self.slot else { return };
        let Some(path) = rfd::FileDialog::new().add_filter("Quod state", &["quodstate"]).pick_file() else { return };
        self.msg = match std::fs::read(&path).map_err(|e| e.to_string()).and_then(|b| s.backup_set(&b).map_err(|e| match e {
            quod_core::BackupError::NotABackup => "That isn't a Quod state file.".into(),
            quod_core::BackupError::Damaged => "The file is damaged.".into(),
            quod_core::BackupError::OtherVersion => "That state is from another version of Quod.".into(),
        })) {
            Ok(()) => "Playing it.".into(),
            Err(e) => e,
        };
    }
}
