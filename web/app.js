// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Kip DeCastro. See ../LICENSE.
//
// Quod in a browser: the FM-1's panel (as the Audio Unit's, apple/Extension/QuodPanelView.m: the same layout, in
// the same units) around the engine in worklet.js. Mouse or touch: press a button or a key; drag an encoder up or
// down, or scroll on it; Shift-click a button to latch it. The computer's keys play too, and any MIDI input.
// [ FLASH ] puts Quod on an FM-1 (flash.js, GPL, on fm1/web).

const PANEL_W = 520, PANEL_H = 880;
const BTN_NAME = ["OCT −", "OCT +", "FX", "SHIFT", "", "", "SETTLE", "SHOWCASE", "HOME", "SAVE", "", "STEPS", "PLAY", "REC"];
const B_GLO = 7, B_HOME = 8, B_SAVE = 9, B_REC = 13, B_PLAY = 12;
const BTN_POS = [[0, 0], [0, 1], [0, 4], [0, 2], null, null, [0, 3], [1, 4], [0, 5], [1, 3], null, [1, 0], [1, 1], [1, 2]];
const ROW_N = [6, 5];
const BLACK_KEY = [0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0];   // from F
const SCR_X = 20, SCR_Y = 20, SCR_S = 480, ENC_Y = 540, ENC_DY = 52, ENC_R = 17;
const BTN_Y0 = 646, BTN_H = 30, BTN_GAP = 8, KEY_Y = 740, KEY_H = 120, KEY_BH = 74;
const ENC_POS = [[1, 0], [1, 1], [2, 0], [3, 0], [4, 0], [5, 0], [0, 1]];   // (column, row), as the FM-1 has them
const WHITE_W = SCR_S / 16;
// the computer's keys: a piano's from C4 (key 7) to F5 (key 24); z / x the octaves, space PLAY
const KEYMAP = { a: 7, w: 8, s: 9, e: 10, d: 11, f: 12, t: 13, g: 14, y: 15, h: 16, u: 17, j: 18, k: 19, o: 20, l: 21, p: 22, ";": 23, "'": 24 };
const KEYBTN = { z: 0, x: 1, " ": B_PLAY };

const $ = (id) => document.getElementById(id);
const canvas = $("panel"), g = canvas.getContext("2d");
const screen = document.createElement("canvas");
screen.width = screen.height = 240;
const sg = screen.getContext("2d");

let node = null, ctx = null, master = null;

// ---- the volume: the knob's level (0..1, heard as its square), its mute, and [ LINK ]'s fade, all on one gain
const VOL = (() => { try { return JSON.parse(localStorage.getItem("quod.volume")) || { level: 0.8, muted: false }; } catch (_) { return { level: 0.8, muted: false }; } })();
let linkLevel = 1;
const gainNow = () => (VOL.muted ? 0 : VOL.level * VOL.level) * linkLevel;
function setVolume(level, muted = VOL.muted) {
  VOL.level = Math.max(0, Math.min(1, level));
  VOL.muted = muted;
  try { localStorage.setItem("quod.volume", JSON.stringify(VOL)); } catch (_) {}
  if (master) master.gain.setTargetAtTime(gainNow(), ctx.currentTime, 0.03);
  draw();
}
// [ LINK ]: the sound to `level` (0..1) over `seconds`; resolves when it's there
export function fadeLink(level, seconds) {
  linkLevel = level;
  if (!master) return Promise.resolve();
  const now = ctx.currentTime, g = master.gain;
  g.cancelScheduledValues(now);
  g.setValueAtTime(g.value, now);
  g.linearRampToValueAtTime(gainNow(), now + seconds);
  return new Promise((r) => setTimeout(r, seconds * 1000));
}
let leds = [0, 0], enc = Array(7).fill(null), names = Array(7).fill("");
let latched = 0, showUntil = 0;
const held = new Map();                         // pointer id -> { kind: "button" | "key" | "enc", i, acc, y }
const keysDown = new Set();

// ---- the engine
let nextId = 1;
const waiting = new Map();
function call(t, extra = {}) {                  // a request with a reply (state, backups)
  return new Promise((resolve) => {
    const id = nextId++;
    waiting.set(id, resolve);
    node.port.postMessage({ t, id, ...extra });
  });
}
function send(m, transfer = []) { if (node) node.port.postMessage(m, transfer); }

export async function switchOn() {
  // (~20 ms of buffer, not the least the browser can do: Quod's main loop, the screen with it, runs on the audio thread
  // a millisecond at a time, and the splash's or a busy screen's heavier milliseconds land in one 2.9 ms quantum.
  // With a longer buffer the browser renders several quanta a callback, and those spikes fit; a glide takes 20 ms at
  // the quickest, so the delay isn't heard)
  ctx = new AudioContext({ sampleRate: 44100, latencyHint: 0.02 });
  const [wasm] = await Promise.all([fetch("quod.wasm").then((r) => r.arrayBuffer()), ctx.audioWorklet.addModule("worklet.js")]);
  const opts = (processorOptions) => ({ numberOfInputs: 0, outputChannelCount: [2], processorOptions });
  try {                                         // compiled here, off the audio thread (bytes where a browser can't
    node = new AudioWorkletNode(ctx, "quod", opts({ module: await WebAssembly.compile(wasm) }));   // send a Module)
  } catch (_) {
    node = new AudioWorkletNode(ctx, "quod", opts({ wasm }));
  }
  master = ctx.createGain();
  master.gain.value = gainNow();
  node.connect(master).connect(ctx.destination);
  node.port.onmessage = (e) => fromEngine(e.data);
  for (let i = 0; i < 3; i++) send({ t: "buf", buf: new ArrayBuffer(240 * 240 * 4) });
  const kept = loadKept();
  if (kept) await call("state-set", { bytes: kept });
  await ctx.resume();
  startMidi();
}

function fromEngine(m) {
  if (m.t === "frame") {
    leds = m.leds;
    enc = m.enc.map((lv) => (lv < 0 ? null : { level: lv & 65535, lock: lv >= 65536 }));
    if (m.names) names = m.names;
    if (m.stats && DEBUG) debugLine(m.stats);
    if (m.rgba) {
      sg.putImageData(new ImageData(new Uint8ClampedArray(m.rgba), 240, 240), 0, 0);
      send({ t: "buf", buf: m.rgba }, [m.rgba]);
    }
    if (showUntil && performance.now() >= showUntil) {
      send({ t: "button", b: B_HOME, down: false });
      send({ t: "button", b: B_REC, down: false });
      showUntil = 0;
    }
    draw();
    return;
  }
  const w = waiting.get(m.id);
  if (w) { waiting.delete(m.id); w(m); }
}

// ?debug: each second, the audio thread's worst quantum (ms; 2.9 is the budget), the quanta that ran 3 ms or more,
// and the dropouts the browser counted (Chrome: AudioContext.playbackStats)
const DEBUG = new URLSearchParams(location.search).has("debug");
let underrunsWere = 0;
function debugLine(s) {
  const p = ctx.playbackStats, under = p ? p.underrunEvents : NaN;
  const line = `audio: worst ${s.worst} ms, slow ${s.slow}, dropouts ${p ? under - underrunsWere : "?"}` +
               ` (${p ? under : "?"} since start), at ${ctx.currentTime.toFixed(1)} s`;
  underrunsWere = under;
  console.log(line);
  const el = document.getElementById("debug");
  if (el) el.textContent = line;
}

// SAVE keeps everything, here in this browser, as the FM-1 keeps it in its flash
const KEEP = "quod.state";
function keep(bytes) { try { localStorage.setItem(KEEP, btoa(String.fromCharCode(...bytes))); } catch (_) {} }
function loadKept() {
  try {
    const s = localStorage.getItem(KEEP);
    return s ? Uint8Array.from(atob(s), (c) => c.charCodeAt(0)) : null;
  } catch (_) { return null; }
}
async function saved() { keep((await call("state-get")).bytes); }

// a .quodstate file (an FM-1 backup: engine/quod_engine.h) to and from the sim
export async function simBackup() { return (await call("backup-get")).bytes; }
const BACKUP_RC = { 1: "That isn't a Quod state file.", 2: "The file is damaged.", 3: "That state is from another version of Quod." };
export async function simRestore(bytes) {
  const { rc } = await call("backup-set", { bytes: new Uint8Array(bytes) });
  if (rc) throw new Error(BACKUP_RC[rc] || `Error ${rc}.`);
}
export function running() { return !!node; }
export function simScreen() { return screen; }   // (the sim's screen, 240 x 240: [ LINK ] draws it)

// ---- the panel's inputs
function button(b, down) {
  send({ t: "button", b, down });
  if (b === B_SAVE && down) setTimeout(saved, 50);
}
function key(k, down) { send({ t: "key", k, down }); }

// ---- MIDI in (notes, CC 20..26 the knobs): every input, as the app does
async function startMidi() {
  if (!navigator.requestMIDIAccess) return;
  try {
    const access = await navigator.requestMIDIAccess();
    const hook = () => {
      for (const input of access.inputs.values())
        input.onmidimessage = (e) => {
          const [st, d1 = 0, d2 = 0] = e.data;
          if (st < 0xF0) send({ t: "midi", st, d1, d2 });
        };
      $("midi-ins").textContent = [...access.inputs.values()].map((i) => i.name).join(", ") || "none";
    };
    access.onstatechange = hook;
    hook();
  } catch (_) {}
}

// ---- layout (panel units)
function encCentre(e) { return [SCR_X + SCR_S * (ENC_POS[e][0] + 0.5) / 6, ENC_Y + ENC_POS[e][1] * ENC_DY]; }
function btnRect(b) {
  const p = BTN_POS[b];
  if (!p) return null;
  const w = (SCR_S - (ROW_N[p[0]] - 1) * BTN_GAP) / ROW_N[p[0]];
  return [SCR_X + p[1] * (w + BTN_GAP), BTN_Y0 + p[0] * (BTN_H + BTN_GAP), w, BTN_H];
}
function whiteIndex(k) {
  if (BLACK_KEY[k % 12]) return -1;
  let w = 0;
  for (let i = 0; i < k; i++) w += !BLACK_KEY[i % 12];
  return w;
}
function keyRect(k) {
  if (!BLACK_KEY[k % 12]) return [SCR_X + whiteIndex(k) * WHITE_W, KEY_Y, WHITE_W, KEY_H];
  return [SCR_X + whiteIndex(k - 1) * WHITE_W + WHITE_W * 0.68, KEY_Y, WHITE_W * 0.64, KEY_BH];
}
const inRect = (x, y, r) => r && x >= r[0] && x < r[0] + r[2] && y >= r[1] && y < r[1] + r[3];
function keyAt(x, y) {
  for (let k = 0; k < 27; k++) if (BLACK_KEY[k % 12] && inRect(x, y, keyRect(k))) return k;
  for (let k = 0; k < 27; k++) if (!BLACK_KEY[k % 12] && inRect(x, y, keyRect(k))) return k;
  return -1;
}
function buttonAt(x, y) { for (let b = 0; b < 14; b++) if (inRect(x, y, btnRect(b))) return b; return -1; }
const VOL_AT = [SCR_X + SCR_S * 0.5 / 6, ENC_Y];   // the volume knob: the encoders' empty top-left place
const volAt = (x, y) => Math.hypot(x - VOL_AT[0], y - VOL_AT[1]) <= ENC_R + 8;
function encAt(x, y) {
  for (let e = 0; e < 7; e++) { const [cx, cy] = encCentre(e); if (Math.hypot(x - cx, y - cy) <= ENC_R + 8) return e; }
  return -1;
}

// ---- drawing
function title(s) {                             // "HEAVEN / HELL" -> "Heaven/Hell" (it fits under a knob)
  let t = "", start = true;
  for (let i = 0; i < s.length; i++) {
    const c = s[i];
    if (c === " " && (s[i + 1] === "/" || s[i - 1] === "/")) continue;
    t += start ? c : c.toLowerCase();
    start = c === " " || c === "/";
  }
  return t;
}
function label(s, x, y, w, h, colour, size) {
  g.fillStyle = colour;
  g.font = `500 ${size}px ui-monospace, SFMono-Regular, Menlo, Consolas, monospace`;
  g.textAlign = "center";
  g.textBaseline = "middle";
  g.fillText(s, x + w / 2, y + h / 2);
}
const grey = (v) => `rgb(${v * 255 | 0},${v * 255 | 0},${v * 255 | 0})`;
const isHeld = (kind, i) => [...held.values()].some((h) => h.kind === kind && h.i === i);

function draw() {
  const k = canvas.width / PANEL_W;
  g.setTransform(1, 0, 0, 1, 0, 0);
  g.fillStyle = grey(0.07);
  g.fillRect(0, 0, canvas.width, canvas.height);
  g.setTransform(k, 0, 0, k, 0, 0);
  g.fillStyle = "#000";
  g.fillRect(SCR_X - 4, SCR_Y - 4, SCR_S + 8, SCR_S + 8);
  g.imageSmoothingEnabled = false;
  g.drawImage(screen, SCR_X, SCR_Y, SCR_S, SCR_S);

  for (let e = 0; e < 7; e++) {                 // the encoders
    const [cx, cy] = encCentre(e);
    g.beginPath();
    g.arc(cx, cy, ENC_R, 0, 2 * Math.PI);
    g.fillStyle = grey(isHeld("enc", e) ? 0.32 : 0.2);
    g.fill();
    g.lineWidth = 1.5;
    g.strokeStyle = grey(0.45);
    g.stroke();
    const d = enc[e];                           // its dial, as the screen draws it: 270 degrees from the bottom left,
    if (d) {                                    // lit as far as it is (gold: a step's lock)
      const r = ENC_R - 4, at = -135 + 270 * d.level / 32767, rad = (a) => (a - 90) * Math.PI / 180;
      g.lineWidth = 2;
      g.beginPath(); g.arc(cx, cy, r, rad(-135), rad(135)); g.strokeStyle = grey(0.32); g.stroke();
      g.beginPath(); g.arc(cx, cy, r, rad(-135), rad(at)); g.strokeStyle = d.lock ? "rgb(242,191,64)" : grey(0.85); g.stroke();
      g.fillStyle = "#fff";
      g.fillRect(cx + r * Math.cos(rad(at)) - 1.5, cy + r * Math.sin(rad(at)) - 1.5, 3, 3);
    }
    label(title(names[e] || ""), cx - 36, cy - ENC_R - 15, 72, 12, grey(names[e] ? 0.65 : 0.3), 9);
  }
  {                                             // the volume knob: its level lit, grey when muted
    const [cx, cy] = VOL_AT, r = ENC_R - 4, rad = (a) => (a - 90) * Math.PI / 180, at = -135 + 270 * VOL.level;
    g.beginPath(); g.arc(cx, cy, ENC_R, 0, 2 * Math.PI);
    g.fillStyle = grey(isHeld("vol", 0) ? 0.32 : 0.2); g.fill();
    g.lineWidth = 1.5; g.strokeStyle = grey(0.45); g.stroke();
    g.lineWidth = 2;
    g.beginPath(); g.arc(cx, cy, r, rad(-135), rad(135)); g.strokeStyle = grey(0.32); g.stroke();
    g.beginPath(); g.arc(cx, cy, r, rad(-135), rad(at)); g.strokeStyle = VOL.muted ? grey(0.4) : grey(0.85); g.stroke();
    g.fillStyle = VOL.muted ? grey(0.5) : "#fff";
    g.fillRect(cx + r * Math.cos(rad(at)) - 1.5, cy + r * Math.sin(rad(at)) - 1.5, 3, 3);
    label(VOL.muted ? "Muted" : "Volume", cx - 36, cy - ENC_R - 15, 72, 12, VOL.muted ? "rgb(242,89,64)" : grey(0.65), 9);
  }
  for (let b = 0; b < 14; b++) {                // the buttons, lit by Quod's LEDs
    const r = btnRect(b);
    if (!r) continue;
    const lit = (b < 32 ? leds[0] >>> b : 0) & 1;
    const down = isHeld("button", b) || (latched >> b) & 1 || (b === B_GLO && showUntil);
    g.beginPath();
    g.roundRect(r[0], r[1], r[2], r[3], 5);
    g.fillStyle = grey(down ? 0.42 : 0.17);
    g.fill();
    if (lit) { g.lineWidth = 2; g.strokeStyle = "rgb(242,89,64)"; g.stroke(); }
    label(BTN_NAME[b], r[0], r[1], r[2], r[3], lit ? "#fff" : grey(0.75), 11);
  }
  for (let pass = 0; pass < 2; pass++)          // the keys: the white ones, then the black over them
    for (let kk = 0; kk < 27; kk++) {
      const black = BLACK_KEY[kk % 12];
      if (black !== pass) continue;
      const r = keyRect(kk), id = 14 + kk;
      const down = isHeld("key", kk) || [...keysDown].some((c) => KEYMAP[c] === kk);
      const lit = (id < 32 ? leds[0] >>> id : leds[1] >>> (id - 32)) & 1;
      g.fillStyle = black ? grey(down ? 0.35 : 0.1) : grey(down ? 0.72 : 0.9);
      g.fillRect(r[0] + 0.5, r[1], r[2] - 1, r[3]);
      if (lit) { g.fillStyle = "rgb(242,89,64)"; g.fillRect(r[0] + r[2] / 2 - 4, r[1] + r[3] - 14, 8, 8); }
    }
}

function fit() {
  const w = canvas.clientWidth, dpr = window.devicePixelRatio || 1;
  canvas.width = Math.round(w * dpr);
  canvas.height = Math.round(w * PANEL_H / PANEL_W * dpr);
  draw();
}

// ---- pointer, wheel, keys
function at(e) {
  const r = canvas.getBoundingClientRect();
  return [(e.clientX - r.left) * PANEL_W / r.width, (e.clientY - r.top) * PANEL_W / r.width];
}
canvas.addEventListener("pointerdown", (e) => {
  const [x, y] = at(e);
  let b, k, en;
  if (volAt(x, y)) {                            // the volume: dragged, or clicked (mute)
    canvas.setPointerCapture(e.pointerId);
    held.set(e.pointerId, { kind: "vol", i: 0, y, moved: 0 });
    e.preventDefault();
    return draw();
  }
  if (!node) return;
  canvas.setPointerCapture(e.pointerId);
  if ((b = buttonAt(x, y)) === B_GLO) {        // SHOWCASE: HOME + REC, held 1.2 s
    send({ t: "button", b: B_HOME, down: true });
    send({ t: "button", b: B_REC, down: true });
    showUntil = performance.now() + 1200;
  } else if (b >= 0) {
    if (e.shiftKey) { latched ^= 1 << b; button(b, !!((latched >> b) & 1)); }
    else if ((latched >> b) & 1) { latched &= ~(1 << b); button(b, false); }
    else { held.set(e.pointerId, { kind: "button", i: b }); button(b, true); }
  } else if ((k = keyAt(x, y)) >= 0) {
    held.set(e.pointerId, { kind: "key", i: k });
    key(k, true);
  } else if ((en = encAt(x, y)) >= 0) {
    held.set(e.pointerId, { kind: "enc", i: en, acc: 0, y });
    e.preventDefault();
  }
  draw();
});
canvas.addEventListener("pointermove", (e) => {
  const h = held.get(e.pointerId);
  if (!h) return;
  const [x, y] = at(e);
  if (h.kind === "vol") {                       // up = louder: 150 units, the whole range
    h.moved += Math.abs(h.y - y);
    setVolume(VOL.level + (h.y - y) / 150);
    h.y = y;
  } else if (h.kind === "enc") {                // up = clockwise, a detent every 6 units
    h.acc += h.y - y;
    h.y = y;
    while (h.acc >= 6) { send({ t: "enc", e: h.i, d: 1 }); h.acc -= 6; }
    while (h.acc <= -6) { send({ t: "enc", e: h.i, d: -1 }); h.acc += 6; }
  } else if (h.kind === "key") {                // a glissando across the keys
    const k = keyAt(x, y);
    if (k >= 0 && k !== h.i) { key(h.i, false); h.i = k; key(k, true); draw(); }
  }
});
function release(e) {
  const h = held.get(e.pointerId);
  if (!h) return;
  held.delete(e.pointerId);
  if (h.kind === "button") button(h.i, false);
  if (h.kind === "key") key(h.i, false);
  if (h.kind === "vol" && h.moved < 3) setVolume(VOL.level, !VOL.muted);   // (a click, not a drag: mute, unmute)
  draw();
}
canvas.addEventListener("pointerup", release);
canvas.addEventListener("pointercancel", release);
const scrollAcc = Array(7).fill(0);
canvas.addEventListener("wheel", (e) => {
  const [x, y] = at(e), en = encAt(x, y);
  if (volAt(x, y)) { e.preventDefault(); setVolume(VOL.level - (e.deltaMode ? e.deltaY * 0.05 : e.deltaY / 600)); return; }
  if (!node || en < 0) return;
  e.preventDefault();
  scrollAcc[en] -= e.deltaMode ? e.deltaY : e.deltaY / 30;
  while (scrollAcc[en] >= 1) { send({ t: "enc", e: en, d: 1 }); scrollAcc[en] -= 1; }
  while (scrollAcc[en] <= -1) { send({ t: "enc", e: en, d: -1 }); scrollAcc[en] += 1; }
}, { passive: false });

// the computer's keyboard, drawn: what each key plays (KEYMAP, KEYBTN), lit while held
const NOTE = ["C", "C\u266f", "D", "D\u266f", "E", "F", "F\u266f", "G", "G\u266f", "A", "A\u266f", "B"];
const KEYBTN_NAME = { 0: "OCT\u2212", 1: "OCT+", [B_PLAY]: "PLAY" };
const keyEls = new Map();
export function drawKeys(el) {
  const rows = [["qwertyuiop", 0.5], ["asdfghjkl;'", 0.75], ["zxcvbnm", 1.25], [" ", 3]];
  for (const [keys, indent] of rows) {
    const r = document.createElement("div");
    r.className = "r";
    r.style.paddingLeft = `calc(100% / 12 * ${indent})`;
    for (const c of keys) {
      const k = document.createElement("div"), midi = 53 + KEYMAP[c];   // (key 0 is F3, MIDI 53)
      const what = c in KEYMAP ? NOTE[midi % 12] + (Math.floor(midi / 12) - 1) : KEYBTN_NAME[KEYBTN[c]];
      k.className = "k" + (what ? " on" : "") + (c === " " ? " wide" : "");
      k.textContent = c === " " ? "space" : c.toUpperCase();
      if (what) { const b = document.createElement("b"); b.textContent = what; k.append(b); }
      keyEls.set(c, k);
      r.append(k);
    }
    el.append(r);
  }
}
function lightKey(c, on) { keyEls.get(c)?.classList.toggle("down", on); }

function typing(e) { return e.target.closest("input, select, textarea, dialog[open]") || e.metaKey || e.ctrlKey || e.altKey; }
window.addEventListener("keydown", (e) => {
  const c = e.key.toLowerCase();
  if (!node || typing(e) || e.repeat || !(c in KEYMAP || c in KEYBTN)) return;
  e.preventDefault();
  keysDown.add(c);
  lightKey(c, true);
  if (c in KEYMAP) key(KEYMAP[c], true); else button(KEYBTN[c], true);
  draw();
});
window.addEventListener("keyup", (e) => {
  const c = e.key.toLowerCase();
  if (!keysDown.delete(c)) return;
  lightKey(c, false);
  if (c in KEYMAP) key(KEYMAP[c], false); else button(KEYBTN[c], false);
  draw();
});
window.addEventListener("blur", () => {         // (a key let go while the window was away)
  for (const c of keysDown) { lightKey(c, false); if (c in KEYMAP) key(KEYMAP[c], false); else button(KEYBTN[c], false); }
  keysDown.clear();
  draw();
});

new ResizeObserver(fit).observe(canvas);
fit();
