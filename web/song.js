// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Kip DeCastro. See ../LICENSE.
// [ LINK ]'s song while a trade runs: a little 8-bit loop (two pulse waves, a triangle bass, a noise hat), and a
// jingle at the end, up if the trade went through, down if it didn't. Its own AudioContext, so the sim's sound can
// fade out under it.

const NOTE = { C: 0, D: 2, E: 4, F: 5, G: 7, A: 9, B: 11 };
const hz = (n) => {                              // "C#5" -> Hz
  const m = /^([A-G])(#|b)?(-?\d)$/.exec(n);
  const midi = 12 * (+m[3] + 1) + NOTE[m[1]] + (m[2] === "#" ? 1 : m[2] === "b" ? -1 : 0);
  return 440 * 2 ** ((midi - 69) / 12);
};
// "E5 G5 C6:2 -:2" -> [[Hz or 0, steps]]: a step is the tune's `step` of a beat; ":n" holds it n steps, "-" rests,
// "x" is a hit (the hat's)
const seq = (s) => s.trim().split(/\s+/).map((t) => { const [n, d] = t.split(":"); return [n === "-" ? 0 : n === "x" ? 1 : hz(n), +(d || 1)]; });
const rep = (s, n) => Array(n).fill(s).join(" ");

// the tune: bpm, the step (of a beat), its voices; a loop of 8 bars, then `win` and `lose`
export const TUNES = {
  // D major, quick, with a sparkling arpeggio: D Bm G A, twice
  sparkle: {
    name: "Sparkle", bpm: 168, step: 0.5,
    lead: "F#5 A5 D6 A5 F#5 A5 B5 A5 | F#5:2 D5 F#5 B5:2 A5:2 | G5 B5 D6 B5 A5 G5 F#5 E5 | E5:2 A5:2 C#6:2 E6:2 |" +
          "D6 C#6 D6 A5 F#5:2 A5:2 | B5 A5 F#5 D5 F#5:2 B4:2 | G5 F#5 G5 B5 A5:2 G5 E5 | C#5 E5 A5 C#6 E6:2 -:2",
    harm: rep("D5 F#5 A5 F#5", 4) + " " + rep("B4 D5 F#5 D5", 4) + " " + rep("G4 B4 D5 B4", 4) + " " + rep("A4 C#5 E5 C#5", 4) + " " +
          rep("D5 F#5 A5 F#5", 4) + " " + rep("B4 D5 F#5 D5", 4) + " " + rep("G4 B4 D5 B4", 4) + " " + rep("A4 C#5 E5 C#5", 4),
    harmStep: 0.25,
    bass: "D3:2 D3 A2 D3:2 F#3:2 | B2:2 B2 F#2 B2:2 D3:2 | G2:2 G2 D3 G2:2 B2:2 | A2:2 A2 E3 A2:2 C#3:2 |" +
          "D3:2 D3 A2 D3:2 F#3:2 | B2:2 B2 F#2 B2:2 D3:2 | G2:2 G2 D3 G2:2 B2:2 | A2:2 A2 E3 A2:2 C#3:2",
    hat: rep("x x", 32),
    win: "A5 D6 F#6 A6 F#6 A6 D7:6", lose: "A5:2 G#5:2 G5:2 F#5:2 D5:2 A4:6",
  },
};

// a pulse wave of duty d (0..0.5): 1/8, 1/4, 1/2, as 8-bit consoles had
function pulse(ctx, d) {
  const n = 32, re = new Float32Array(n), im = new Float32Array(n);
  for (let k = 1; k < n; k++) re[k] = (2 / (k * Math.PI)) * Math.sin(k * Math.PI * d);
  return ctx.createPeriodicWave(re, im);
}

let ctx = null, waves = null, noise = null;
function setup() {
  if (ctx) return;
  ctx = new AudioContext();
  waves = { lead: pulse(ctx, 0.25), harm: pulse(ctx, 0.125) };
  noise = ctx.createBuffer(1, ctx.sampleRate, ctx.sampleRate);   // (held for 8 samples: an 8-bit hiss)
  const d = noise.getChannelData(0);
  for (let i = 0, v = 0; i < d.length; i++) { if (i % 8 === 0) v = Math.random() * 2 - 1; d[i] = v; }
}

function tone(out, voice, f, t, len) {
  const o = ctx.createOscillator(), g = ctx.createGain();
  if (voice === "bass") o.type = "triangle"; else o.setPeriodicWave(waves[voice]);
  o.frequency.value = f;
  const peak = { lead: 0.32, harm: 0.13, bass: 0.5 }[voice];
  g.gain.setValueAtTime(0, t);
  g.gain.linearRampToValueAtTime(peak, t + 0.004);
  g.gain.setTargetAtTime(peak * 0.6, t + 0.01, 0.08);
  g.gain.setTargetAtTime(0, t + len * 0.85, 0.012);
  o.connect(g).connect(out);
  o.start(t);
  o.stop(t + len + 0.08);
}
function hit(out, t) {
  const s = ctx.createBufferSource(), g = ctx.createGain(), hp = ctx.createBiquadFilter();
  s.buffer = noise;
  hp.type = "highpass";
  hp.frequency.value = 6000;
  g.gain.setValueAtTime(0.16, t);
  g.gain.exponentialRampToValueAtTime(0.001, t + 0.05);
  s.connect(hp).connect(g).connect(out);
  s.start(t, Math.random() * 0.5);
  s.stop(t + 0.06);
}

// the tune `name`, looping, at `volume` (0..1) -> { end(ok): its jingle, then silence (resolves when it's over),
// stop(): silence now }
export function play(name, volume = 0.64) {
  setup();
  ctx.resume();
  const tune = TUNES[name], beat = 60 / tune.bpm;
  const out = ctx.createGain();
  out.gain.value = volume;
  out.connect(ctx.destination);
  const voices = ["lead", "harm", "bass", "hat"].map((v) => {
    const notes = seq(tune[v].replace(/\|/g, " ")), step = beat * (v === "harm" && tune.harmStep || tune.step);
    return { v, notes, step, i: 0, at: 0 };
  });
  const loop = Math.max(...voices.map((x) => x.notes.reduce((a, [, d]) => a + d * x.step, 0)));
  let t0 = ctx.currentTime + 0.08, ending = false;
  for (const x of voices) x.at = t0;
  function pump() {                              // (schedules what falls in the next 0.25 s)
    const until = ctx.currentTime + 0.25;
    for (const x of voices) {
      while (!ending && x.at < until) {
        const [f, d] = x.notes[x.i], len = d * x.step;
        if (f) x.v === "hat" ? hit(out, x.at) : tone(out, x.v, f, x.at, len);
        x.at += len;
        if (++x.i === x.notes.length) x.i = 0;
      }
    }
  }
  pump();
  const timer = setInterval(pump, 60);
  const hush = (when) => {
    out.gain.setTargetAtTime(0, when, 0.02);
    setTimeout(() => out.disconnect(), (when - ctx.currentTime + 0.3) * 1000);
  };
  return {
    stop() { ending = true; clearInterval(timer); hush(ctx.currentTime); },
    end(ok) {
      ending = true;
      clearInterval(timer);
      // the loop's notes already scheduled ring out under a quick fade; the jingle starts on the next beat
      const now = ctx.currentTime, next = now + beat - ((now - t0) % beat);
      const fade = ctx.createGain();
      out.disconnect();
      out.connect(fade).connect(ctx.destination);
      fade.gain.setValueAtTime(1, now);
      fade.gain.linearRampToValueAtTime(0, next);
      const jo = ctx.createGain();
      jo.gain.value = volume;
      jo.connect(ctx.destination);
      let t = next;
      for (const [f, d] of seq(ok ? tune.win : tune.lose)) {
        const len = d * beat * 0.25;
        if (f) { tone(jo, "lead", f, t, len); tone(jo, "bass", f / 4, t, len); }
        t += len;
      }
      setTimeout(() => { fade.disconnect(); jo.disconnect(); }, (t - now + 0.4) * 1000);
      return new Promise((r) => setTimeout(r, (t - now) * 1000));
    },
  };
}
