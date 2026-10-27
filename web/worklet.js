// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Kip DeCastro. See ../LICENSE.
//
// Quod's engine (quod.wasm: crates/quod-wasm) on the audio thread, at 44.1 kHz. The page sends the panel and MIDI
// in; out come the sound, and about 30 times a second the screen (when it changed), the LEDs and the encoders.
// The screen's buffers go back and forth (transferred), so nothing is allocated here as it plays.

class Quod extends AudioWorkletProcessor {
  constructor(options) {
    super();
    const o = options.processorOptions;         // (compiled by the page, or bytes where a Module can't be sent here)
    this.q = new WebAssembly.Instance(o.module || new WebAssembly.Module(o.wasm), {}).exports;
    this.q.quod_init();
    this.pool = [];
    this.lastPost = 0;
    this.names = [];
    this.nameAt = [];
    this.msg = { t: "frame", leds: [0, 0], enc: [] };
    this.statsAt = 0;
    this.port.onmessage = (e) => this.message(e.data);
  }

  bytesIn(bytes) {                              // into the module's shared buffer
    const n = Math.min(bytes.length, 512);
    new Uint8Array(this.q.memory.buffer, this.q.quod_buf(), n).set(bytes.subarray(0, n));
    return n;
  }
  bytesOut(n) { return new Uint8Array(this.q.memory.buffer, this.q.quod_buf(), n).slice(); }

  message(m) {
    const q = this.q;
    q.quod_set_clock_us(currentTime * 1e6);
    switch (m.t) {
      case "button": q.quod_button(m.b, m.down ? 1 : 0); break;
      case "key": q.quod_key(m.k, m.down ? 1 : 0); break;
      case "enc": q.quod_enc(m.e, m.d); break;
      case "midi": q.quod_midi(m.st, m.d1, m.d2); break;
      case "buf": this.pool.push(m.buf); break;
      case "state-get": this.port.postMessage({ t: "state", id: m.id, bytes: this.bytesOut(q.quod_state_get()) }); break;
      case "state-set": this.port.postMessage({ t: "state-set", id: m.id, ok: !!q.quod_state_set(this.bytesIn(m.bytes)) }); break;
      case "backup-get": this.port.postMessage({ t: "backup", id: m.id, bytes: this.bytesOut(q.quod_backup_get()) }); break;
      case "backup-set": this.port.postMessage({ t: "backup-set", id: m.id, rc: q.quod_backup_set(this.bytesIn(m.bytes)) }); break;
    }
  }

  name(e) {
    const q = this.q, n = q.quod_enc_name_len(e);
    return String.fromCharCode(...new Uint8Array(q.memory.buffer, q.quod_enc_name(e), n));
  }

  post() {                                      // (the message's parts reused; a name read only when it changed:
    const q = this.q, m = this.msg;             //  each control's name is a string of its own, so its address)
    m.leds[0] = q.quod_leds_lo(); m.leds[1] = q.quod_leds_hi();
    delete m.names; delete m.rgba; delete m.stats;
    for (let e = 0; e < 7; e++) {
      m.enc[e] = q.quod_enc_level(e);
      const at = q.quod_enc_name(e);
      if (at !== this.nameAt[e]) { this.nameAt[e] = at; this.names[e] = this.name(e); m.names = this.names; }
    }
    if (currentTime - this.statsAt >= 1) {
      m.stats = { worst: this.worst, slow: this.slow };
      this.statsAt = currentTime; this.worst = this.slow = 0;
    }
    const transfer = [];
    if (this.pool.length && q.quod_screen_changed()) {
      const buf = this.pool.pop();
      new Uint8Array(buf).set(new Uint8Array(q.memory.buffer, q.quod_rgba(), 240 * 240 * 4));
      m.rgba = buf;
      transfer.push(buf);
    }
    this.port.postMessage(m, transfer);
  }

  process(_in, outputs) {
    const q = this.q, out = outputs[0], n = out[0].length, t0 = Date.now();
    if (this.left?.length !== n || this.left.buffer !== q.memory.buffer) {   // (the views made once, not each quantum:
      this.left = new Float32Array(q.memory.buffer, q.quod_left(), n);      // nothing allocated as it plays, so no
      this.right = new Float32Array(q.memory.buffer, q.quod_right(), n);    // collections on this thread)
    }
    q.quod_set_clock_us(currentTime * 1e6);
    q.quod_render(n);
    out[0].set(this.left);
    if (out[1]) out[1].set(this.right);
    if (currentTime - this.lastPost >= 1 / 30) { this.lastPost = currentTime; this.post(); }
    const dt = Date.now() - t0;                 // (?debug: the page shows these each second; Date.now's milliseconds
    this.worst = Math.max(this.worst || 0, dt); //  are enough to see a quantum, 2.9 ms, run late)
    this.slow = (this.slow || 0) + (dt >= 3);
    return true;
  }
}

registerProcessor("quod", Quod);
