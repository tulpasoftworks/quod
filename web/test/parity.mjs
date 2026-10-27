// SPDX-License-Identifier: MIT
// The same script as crates/quod-core/examples/render.rs, in the WebAssembly build: the hashes must agree.
//   node web/test/parity.mjs [quod_wasm.wasm]
import { readFileSync } from "node:fs";
const path = process.argv[2] || new URL("../../target/wasm32-unknown-unknown/release/quod_wasm.wasm", import.meta.url);
const { instance } = await WebAssembly.instantiate(readFileSync(path));
const q = instance.exports;
q.quod_init();
let h = 0x811C9DC5 >>> 0, peak = 0;
const bytes = new Uint8Array(4), f = new Float32Array(bytes.buffer);
const t0 = performance.now();
for (let blk = 0; blk < 1000; blk++) {
  if (blk === 0) q.quod_midi(0x90, 48, 100);
  if (blk === 150) q.quod_midi(0xB0, 22, 127);
  if (blk === 400) q.quod_midi(0x90, 55, 100);
  if (blk === 600) q.quod_midi(0xB0, 22, 0);
  if (blk === 800) q.quod_midi(0xB0, 25, 127);
  q.quod_render(441);
  const L = new Float32Array(q.memory.buffer, q.quod_left(), 441), R = new Float32Array(q.memory.buffer, q.quod_right(), 441);
  for (let i = 0; i < 441; i++) for (const v of [L[i], R[i]]) {
    f[0] = v; peak = Math.max(peak, Math.abs(v));
    for (const b of bytes) h = Math.imul(h ^ b, 0x01000193) >>> 0;
  }
}
console.log(`hash ${h.toString(16).padStart(8, "0")} peak ${peak.toFixed(4)}  (${((performance.now() - t0) / 1000).toFixed(2)} s for 10 s)`);
