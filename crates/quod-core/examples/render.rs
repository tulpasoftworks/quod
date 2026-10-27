// SPDX-License-Identifier: MIT
// The same script as web/test/parity.mjs, natively: 10 s of Quod, a hash of every sample (the two must agree).
//   cargo run --release -p quod-core --example render [OUT.f32]
use quod_core::Slot;

fn main() {
    let s = Slot::take().expect("a slot");
    let (mut l, mut r) = (vec![0f32; 441], vec![0f32; 441]);
    let mut h: u32 = 0x811C9DC5;
    let mut all = Vec::new();
    for blk in 0..1000u32 {                     // 1000 x 10 ms
        match blk {
            0 => s.midi(0x90, 48, 100),
            150 => s.midi(0xB0, 22, 127),       // heaven / hell: full heaven
            400 => s.midi(0x90, 55, 100),
            600 => s.midi(0xB0, 22, 0),         // full hell
            800 => s.midi(0xB0, 25, 127),       // drift
            _ => {}
        }
        s.render(&mut l, &mut r);
        for i in 0..441 {
            for v in [l[i], r[i]] {
                for b in v.to_bits().to_le_bytes() { h = (h ^ b as u32).wrapping_mul(0x01000193); }
                all.push(v);
            }
        }
    }
    let peak = all.iter().fold(0f32, |m, v| m.max(v.abs()));
    println!("hash {h:08x} peak {peak:.4}");
    if let Some(out) = std::env::args().nth(1) {
        std::fs::write(out, all.iter().flat_map(|v| v.to_le_bytes()).collect::<Vec<_>>()).unwrap();
    }
}
