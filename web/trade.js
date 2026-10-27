// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Kip DeCastro. See ../LICENSE.
//
// The trade ([ LINK ], link.js), drawn at 160 x 144 in four greys and one red, on a canvas the page
// scales up. The settings gather out of the ether into an orb (heaven above, hell below) on the sending screen; the
// link cable, a ringed pipe hanging over it, sucks the orb up; it rides the pipe up, across (the view following) and
// down; it waits at the far mouth until the transfer has settled, drops onto the receiving screen and is absorbed.
// Then the result: Quod's orb spinning, and a text box. If the transfer fails, the orb rides back, and on the next
// screen it shakes and bursts.
const W = 160, H = 144;                          // the screen
const WORLD = 480;                               // the cable's scene: three screens wide
const K = { white: "#ffffff", light: "#a8a8a8", dark: "#545454", black: "#000000", red: "#e83818", deep: "#9c1c0c" };
let g, getSim = () => null;
export function setupTrade(canvas, sim) {       // the canvas, and the sim's screen (a 240 x 240 canvas)
  canvas.width = W;
  canvas.height = H;
  g = canvas.getContext("2d");
  getSim = sim;
}

// ---- drawing
function px(x, y, w, h, c) { g.fillStyle = c; g.fillRect(Math.round(x), Math.round(y), w, h); }
function image(img, x, y, w, h) { if (!img) return; g.imageSmoothingEnabled = true; g.drawImage(img, x, y, w, h); g.imageSmoothingEnabled = false; }
function frameBox(x, y, w, h) {                 // a text box's double edge
  px(x, y, w, h, K.black); px(x + 1, y + 1, w - 2, h - 2, K.white);
  px(x + 2, y + 2, w - 4, h - 4, K.black); px(x + 3, y + 3, w - 6, h - 6, K.white);
}
// a 5 x 7 font: each glyph seven rows of five bits
const FONT = {
  A: [14, 17, 17, 31, 17, 17, 17], B: [30, 17, 17, 30, 17, 17, 30], C: [14, 17, 16, 16, 16, 17, 14], D: [30, 17, 17, 17, 17, 17, 30],
  E: [31, 16, 16, 30, 16, 16, 31], F: [31, 16, 16, 30, 16, 16, 16], G: [14, 17, 16, 23, 17, 17, 15], H: [17, 17, 17, 31, 17, 17, 17],
  I: [14, 4, 4, 4, 4, 4, 14], J: [7, 2, 2, 2, 2, 18, 12], K: [17, 18, 20, 24, 20, 18, 17], L: [16, 16, 16, 16, 16, 16, 31],
  M: [17, 27, 21, 21, 17, 17, 17], N: [17, 17, 25, 21, 19, 17, 17], O: [14, 17, 17, 17, 17, 17, 14], P: [30, 17, 17, 30, 16, 16, 16],
  Q: [14, 17, 17, 17, 21, 18, 13], R: [30, 17, 17, 30, 20, 18, 17], S: [15, 16, 16, 14, 1, 1, 30], T: [31, 4, 4, 4, 4, 4, 4],
  U: [17, 17, 17, 17, 17, 17, 14], V: [17, 17, 17, 17, 17, 10, 4], W: [17, 17, 17, 21, 21, 21, 10], X: [17, 17, 10, 4, 10, 17, 17],
  Y: [17, 17, 17, 10, 4, 4, 4], Z: [31, 1, 2, 4, 8, 16, 31],
  0: [14, 17, 19, 21, 25, 17, 14], 1: [4, 12, 4, 4, 4, 4, 14], 2: [14, 17, 1, 2, 4, 8, 31], 3: [31, 2, 4, 2, 1, 17, 14],
  4: [2, 6, 10, 18, 31, 2, 2], 5: [31, 16, 30, 1, 1, 17, 14], 6: [6, 8, 16, 30, 17, 17, 14], 7: [31, 1, 2, 4, 8, 8, 8],
  8: [14, 17, 17, 14, 17, 17, 14], 9: [14, 17, 17, 15, 1, 2, 12],
  ".": [0, 0, 0, 0, 0, 12, 12], "!": [4, 4, 4, 4, 4, 0, 4], "'": [4, 4, 8, 0, 0, 0, 0], ",": [0, 0, 0, 0, 12, 4, 8],
  "-": [0, 0, 0, 31, 0, 0, 0], ":": [0, 12, 12, 0, 12, 12, 0], "?": [14, 17, 1, 2, 4, 0, 4],
};
function text(s, x, y, c = K.black) {
  for (const ch of s.toUpperCase()) {
    const rows = FONT[ch];
    if (rows) rows.forEach((r, j) => { for (let i = 0; i < 5; i++) if (r >> (4 - i) & 1) px(x + i, y + j, 1, 1, c); });
    x += 6;
  }
}
function say(lines, n = Infinity) {             // the text box along the bottom, typed out (n characters shown)
  frameBox(0, 100, W, 44);
  let left = n;
  lines.forEach((l, j) => { text(l.slice(0, Math.max(0, left)), 10, 112 + j * 14); left -= l.length; });
  if (n >= lines.join("").length && Math.floor(performance.now() / 400) % 2) px(146, 134, 5, 3, K.black);   // ▼
}
const PIC = { x: 48, y: 14, s: 64 };             // where the big orb sits, over the text box
// Quod's orb: its planetoid, small. A dark sphere and its voices circling over its face, each at its own latitude
// and speed with a short trail: angels (above the equator) white, demons (below) red, the root on the equator.
// Lights on the far side are hidden. squash < 1: drawn up thin (sucked into the pipe).
const VOICES = [[0.78, 1.9], [0.55, 1.5], [0.32, 1.25], [0.12, 1.1], [0, 1], [-0.14, 0.9], [-0.36, 0.78], [-0.58, 0.66], [-0.8, 0.55]];
function orb(x, y, r = 6, t = 0, squash = 1) {
  const ry = r / squash, rx = r * squash;
  for (let dy = -Math.ceil(ry); dy <= Math.ceil(ry); dy++) for (let dx = -Math.ceil(rx); dx <= Math.ceil(rx); dx++) {
    const d = (dx / (rx + 0.5)) ** 2 + (dy / (ry + 0.5)) ** 2;
    if (d > 1) continue;
    px(x + dx, y + dy, 1, 1, dy === 0 && (dx + Math.floor(t / 160)) % 2 && d < 0.8 ? K.dark : K.black);   // (the equator, turning)
  }
  if (r < 3) return;
  const dot = Math.max(1, Math.round(r / 7));    // (bigger lights on a bigger orb)
  VOICES.forEach(([lat, speed], i) => {
    const c = lat > 0.05 ? K.white : lat < -0.05 ? K.red : K.light, trail = lat > 0.05 ? K.light : lat < -0.05 ? K.deep : K.dark;
    for (let k = 2; k >= 0; k--) {               // the trail, then the light
      const lon = (t / 900) * speed * 2 * Math.PI + i * 2.1 - k * 0.32;
      if (Math.cos(lon) < 0.05) continue;        // (behind the sphere)
      const lx = rx * 0.92 * Math.cos(Math.asin(Math.max(-1, Math.min(1, lat)))) * Math.sin(lon), ly = -ry * 0.92 * lat;
      px(x + lx - (dot >> 1), y + ly - (dot >> 1), dot, dot, k ? trail : c);
    }
  });
}

// the orb bursting, e ms in: a ring going out, the sphere's dark chunks and its lights flying, falling, gone
const DEBRIS = Array.from({ length: 46 }, (_, i) => ({ a: i * 2.399 + (i % 3) * 0.3, v: 0.035 + ((i * 37) % 23) / 400,
  s: i % 4 ? 2 : 3, c: [K.black, K.black, K.red, K.dark, K.deep, K.black][i % 6] }));
function explode(x, y, e) {
  if (e < 260) {                                 // the burst: a ring, every other pixel, growing
    const r = 18 + e / 6;
    for (let k = 0; k < 64; k++) { const a = (k / 64) * Math.PI * 2; if (k % 2) px(x + Math.cos(a) * r, y + Math.sin(a) * r, 2, 2, e < 120 ? K.red : K.dark); }
    if (e < 90) for (let k = 0; k < 16; k++) { const a = (k / 16) * Math.PI * 2; px(x + Math.cos(a) * 10, y + Math.sin(a) * 10, 2, 2, K.black); }
  }
  for (const d of DEBRIS) {
    const fx = x + Math.cos(d.a) * d.v * e, fy = y + Math.sin(d.a) * d.v * e + 0.00006 * e * e;   // (and falling)
    if (fy > 100 || e > 1400) continue;          // (gone behind the text box, or gone)
    if (e > 1000 && (Math.floor(e / 60) + d.s) % 2) continue;   // (flickering out)
    px(fx, fy, d.s, d.s, d.c);
  }
}

// ---- the two ends, as Kip drew them (the sprite editor; world pixels): 0 nothing, 1 light, 2 dark, 3 black, 4 red,
// 5 deep red, 6 the screen (blue). The scene's measures come from them: each screen is where its 6s are, and the
// pipe drops over its middle.
const COMPUTER = { x0: 33, y0: 66, rows: [
  "0000000000000000003333333333333333333333333333333333333333333333333333333333000000000000000000",
  "0000000000000000031111111111111111111111111111111111111111111111111111111111300000000000000000",
  "0000000000000000031111111111111111111111111111111111111111111111111111111111300000000000000000",
  "0000000000000000031111111111111111111111111111111111111111111111111111111111300000000000000000",
  "0000000000000000031111112222222222222222222222222222222222222222222222111111300000000000000000",
  "0000000000000000031111112233333333333333333333333333333333333333333322111111300000000000000000",
  "0000000000000000031111112333333333333333333333333333333333333333333332111111300000000000000000",
  "0000000000000000031111112336666666666666666666666666666666666666666332111111300000000000000000",
  "0000000000000000031111112336666666666666666666666666666666666666666332111111300000000000000000",
  "0000000000000000031111112336600066666666666666666666666666666666666332111111300000000000000000",
  "0000000000000000031111112336606666666666666666666666666666666666666332111111300000000000000000",
  "0000000000000000031111112336606666666666666666666666666666666666666332111111300000000000000000",
  "0000000000000000031111112336666666666666666666666666666666666666666332111111300000000000000000",
  "0000000000000000031111112336666666666666666666666666666666666666666332111111300000000000000000",
  "0000000000000000031111112336666666666666666666666666666666666666666332111111300000000000000000",
  "0000000000000000031111112336666666666666666666666666666666666666666332111111300000000000000000",
  "0000000000000000031111112336666666666666666666666666666666666666666332111111300000000000000000",
  "0000000000000000031111112336666666666666666666666666666666666666666332111111300000000000000000",
  "0000000000000000031111112336666666666666666666666666666666666666666332111111300000000000000000",
  "0000000000000000031111112336666666666666666666666666666666666666666332111111300000000000000000",
  "0000000000000000031111112336666666666666666666666666666666666666666332111111300000000000000000",
  "0000000000000000031111112336666666666666666666666666666666666666666332111111300000000000000000",
  "0000000000000000031111112336666666666666666666666666666666666666666332111111300000000000000000",
  "0000000000000000031111112336666666666666666666666666666666666666666332111111300000000000000000",
  "0000000000000000031111112336666666666666666666666666666666666666666332111111300000000000000000",
  "0000000000000000031111112336666666666666666666666666666666666666666332111111300000000000000000",
  "0000000000000000031111112336666666666666666666666666666666666666666332111111300000000000000000",
  "0000000000000000031111112336666666666666666666666666666666666666666332111111300000000000000000",
  "0000000000000000031111112336666666666666666666666666666666666666666332111111300000000000000000",
  "0000000000000000031111112336666666666666666666666666666666666666666332111111300000000000000000",
  "0000000000000000031111112336666666666666666666666666666666666666666332111111300000000000000000",
  "0000000000000000031111112336666666666666666666666666666666666666666332111111300000000000000000",
  "0000000000000000031111112336666666666666666666666666666666666666666332111111300000000000000000",
  "0000000000000000031111112336666666666666666666666666666666666666666332111111300000000000000000",
  "0000000000000000031111112336666666666666666666666666666666666666666332111111300000000000000000",
  "0000000000000000031111112333333333333333333333333333333333333333333332111111300000000000000000",
  "0000000000000000031111112233333333333333333333333333333333333333333322111111300000000000000000",
  "0000000000000000031111112222222222222222222222222222222222222222222222111111300000000000000000",
  "0000000000000000031111111111111111111111111111111111111111111111111111111111300000000000000000",
  "0000000000000000031111111111111111111111111111111111111111111111111111111111300000000000000000",
  "0000000000000000031111111111111111111111111111111111111333331133333111111111300000000000000000",
  "0000000000000000031111111111111111111111111111111111111300031130003111111111300000000000000000",
  "0000000000000000031111111111111111111111111111111111111300031130003111441111300000000000000000",
  "0000000000000000031111111111111111111111111111111111111300031130003111441111300000000000000000",
  "0000000000000000031111111111111111111111111111111111111333331133333111111111300000000000000000",
  "0000000000000000031111111111111111111111111111111111111111111111111111111111300000000000000000",
  "0000000000000000031111111111111111111111111111111111111111111111111111111111300000000000000000",
  "0000000000000000031111111111111111111111111111111111111111111111111111111111300000000000000000",
  "0000000000000000031111111111111111111111111111111111111111111111111111111111300000000000000000",
  "0000000000000000003333333333333333333333333333333333333333333333333333333333000000000000000000",
  "0000000000000000000000000000000000000333333333333333333330000000000000000000000000000000000000",
  "0000000000000000000000000000000000000333333333333333333330000000000000000000000000000000000000",
  "0000000000000000000000000000000000000333333333333333333330000000000000000000000000000000000000",
  "0000000000000000000000000000000003333333333333333333333333333000000000000000000000000000000000",
  "0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000",
  "0003333333333333333333333333333333333333333333333333333333333333333333333333333333333333333000",
  "0031111111111111111111111111111111111111111111111111111111111111111111111111111111111111111300",
  "0031000003000003100000300000300000300000300000300000300000300000300000300000300000310000031300",
  "0311000003000003100000300000300000300000300000300000300000300000300000300000300000310000031130",
  "0311333333333333133333333333333333333333333333333333333333333333333333333333333333313333331130",
  "0311111111111111111111111111111111111111111111111111111111111111111111111111111111111111111130",
  "0310000030000031000003000003000003000003100000300000300000310000030000030000030000031000003130",
  "3110000030000031000003000003000003000003100000300000300000310000030000030000030000031000003113",
  "3113333333333331333333333333333333333333133333333333333333313333333333333333333333331333333113",
  "3111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111113",
  "3100000300000310000030000031000003000003100000300000300000310000030000031000003000003100000313",
  "3100000300000310000030000031000003000003100000300000300000310000030000031000003000003100000313",
  "3133333333333313333333333331333333333333133333333333333333313333333333331333333333333133333313",
  "3111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111113",
  "3111111111111111111111111300000000000000000000000000000000000000000031111111111111111111111113",
  "3111111111111111111111111300000000000000000000000000000000000000000031111111111111111111111113",
  "3111111111111111111111111333333333333333333333333333333333333333333331111111111111111111111113",
  "3111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111113",
  "3222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222223",
  "3333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333",
] };
const FM1 = { x0: 340, y0: 79, rows: [
  "033333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333330",
  "300000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000003",
  "300000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000003",
  "300000000000000000000000000003333333333333333333333333333333300000000000000000000000000000000000000000000000003",
  "300000333000000000333000000003333333333333333333333333333333300000000000000000000000000000000000000000000000003",
  "300033313330000033313330000003366666666666666666666666666663300000000333000000003330000000033300000000333000003",
  "300031111130000031311130000003366666666666666666666666666663300000033313330000333133300003331333000033313330003",
  "300331111133000331111133000003366666666666666666666666666663300000031111130000311311300003111113000031111130003",
  "300311111113000311111113000003366666666666666666666666666663300000331111133003311111330033111133300331111133003",
  "300333111133000331111133000003366666666666666666666666666663300000313111113003111111130031111111300311111113003",
  "300031111130000031111130000003366666666666666666666666666663300000331111133003311111330033111113300331113133003",
  "300033313330000033313330000003366666666666666666666666666663300000031111130000311111300003111113000031111130003",
  "300000333000000000333000000003366666666666666666666666666663300000033313330000333133300003331333000033313330003",
  "300000000000000000000000000003366666666666666666666666666663300000000333000000003330000000033300000000333000003",
  "300000000000000000000000000003366666666666666666666666666663300000000000000000000000000000000000000000000000003",
  "300000333000000000333000000003366666666666666666666666666663300000000000000000000000000000000000000000000000003",
  "300033313330000033313330000003366666666666666666666666666663300000000000000000000000000000000000000000000000003",
  "300031111130000031111130000003366666666666666666666666666663300000000000000000000000000000000000000000000000003",
  "300331111133000331111133000003366666666666666666666666666663300000333333333003333333330033333333300333333333003",
  "300311111313000311111113000003366666666666666666666666666663300000344444443003111111130031111111300311111113003",
  "300331111133000331111133000003366666666666666666666666666663300000344444443003111111130031111111300311111113003",
  "300031111130000031131130000003366666666666666666666666666663300000344444443003111111130031111111300311111113003",
  "300033313330000033313330000003366666666666666666666666666663300000333333333003333333330033333333300333333333003",
  "300000333000000000333000000003333333333333333333333333333333300000000000000000000000000000000000000000000000003",
  "300000000000000000000000000003333333333333333333333333333333300000000000000000000000000000000000000000000000003",
  "300000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000003",
  "300333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333003",
  "300300000333330003333300000300000333330003333300033333000003000003333300033333000003000003333300033333000003003",
  "300300000333330003333300000300000333330003333300033333000003000003333300033333000003000003333300033333000003003",
  "300300000333330003333300000300000333330003333300033333000003000003333300033333000003000003333300033333000003003",
  "300300000333330003333300000300000333330003333300033333000003000003333300033333000003000003333300033333000003003",
  "300300000333330003333300000300000333330003333300033333000003000003333300033333000003000003333300033333000003003",
  "300300000333330003333300000300000333330003333300033333000003000003333300033333000003000003333300033333000003003",
  "300300000333330003333300000300000333330003333300033333000003000003333300033333000003000003333300033333000003003",
  "300300000333330003333300000300000333330003333300033333000003000003333300033333000003000003333300033333000003003",
  "300300000333330003333300000300000333330003333300033333000003000003333300033333000003000003333300033333000003003",
  "300300000333330003333300000300000333330003333300033333000003000003333300033333000003000003333300033333000003003",
  "300300000333330003333300000300000333330003333300033333000003000003333300033333000003000003333300033333000003003",
  "300300000003000000030000000300000003000000030000000300000003000000030000000300000003000000030000000300000003003",
  "300300000003000000030000000300000003000000030000000300000003000000030000000300000003000000030000000300000003003",
  "300300000003000000030000000300000003000000030000000300000003000000030000000300000003000000030000000300000003003",
  "300300000003000000030000000300000003000000030000000300000003000000030000000300000003000000030000000300000003003",
  "300300000003000000030000000300000003000000030000000300000003000000030000000300000003000000030000000300000003003",
  "300300000003000000030000000300000003000000030000000300000003000000030000000300000003000000030000000300000003003",
  "300300000003000000030000000300000003000000030000000300000003000000030000000300000003000000030000000300000003003",
  "300300000003000000030000000300000003000000030000000300000003000000030000000300000003000000030000000300000003003",
  "300333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333003",
  "300000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000003",
  "300000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000003",
  "033333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333330",
] };
const SPRITE_PAL = [null, "#a8a8a8", "#545454", "#000000", "#e83818", "#9c1c0c", "#3a6df0"];
function screenOf(sp) {
  let x0 = 1e9, y0 = 1e9, x1 = -1, y1 = -1;
  sp.rows.forEach((r, y) => [...r].forEach((ch, x) => {
    if (ch === "6") { x0 = Math.min(x0, x); x1 = Math.max(x1, x); y0 = Math.min(y0, y); y1 = Math.max(y1, y); }
  }));
  return { x: sp.x0 + x0, y: sp.y0 + y0, w: x1 - x0 + 1, h: y1 - y0 + 1 };
}
const SIM_SCR = screenOf(COMPUTER), FM1_SCR = screenOf(FM1);

// ---- the scene: the sim's computer under the pipe's left mouth, the FM-1 under its right one
const XA = SIM_SCR.x + (SIM_SCR.w >> 1), XB = FM1_SCR.x + (FM1_SCR.w >> 1);   // the pipe's two drops: over the screens
const YT = 20, R = 14, HW = 8;                   // its top run, bend radius, half width
const MOUTH_A = COMPUTER.y0 - 8, MOUTH_B = FM1.y0 - 12;   // where its mouths open, over each device
// the pipe's centreline, by length: down from the top run to each mouth
const SEG = [MOUTH_A - (YT + R), (Math.PI * R) / 2, XB - XA - 2 * R, (Math.PI * R) / 2, MOUTH_B - (YT + R)];
const LEN = SEG.reduce((a, b) => a + b);
function along(s) {                             // a point on the centreline, s from the left mouth
  s = Math.max(0, Math.min(LEN, s));
  if (s <= SEG[0]) return [XA, MOUTH_A - s];
  s -= SEG[0];
  if (s <= SEG[1]) { const a = Math.PI + (s / SEG[1]) * (Math.PI / 2); return [XA + R + R * Math.cos(a), YT + R + R * Math.sin(a)]; }
  s -= SEG[1];
  if (s <= SEG[2]) return [XA + R + s, YT];
  s -= SEG[2];
  if (s <= SEG[3]) { const a = -Math.PI / 2 + (s / SEG[3]) * (Math.PI / 2); return [XB - R + R * Math.cos(a), YT + R + R * Math.sin(a)]; }
  return [XB, YT + R + (s - SEG[3])];
}
// the pipe, drawn once: black walls, white inside, rings every eight pixels along it, shade down one side, lipped mouths
const PIPE = (() => {
  const c = document.createElement("canvas");
  c.width = WORLD; c.height = H;
  const q = c.getContext("2d");
  const put = (x, y, col) => { q.fillStyle = col; q.fillRect(x, y, 1, 1); };
  for (let y = 0; y < 90; y++) for (let x = 0; x < WORLD; x++) {
    let off, s;                                  // across the pipe (+: the outside of its bends), and along it
    if (x <= XA + R && y >= YT + R) { off = XA - x; s = MOUTH_A - y; if (y > MOUTH_A) continue; }
    else if (x >= XB - R && y >= YT + R) { off = x - XB; s = LEN - (MOUTH_B - y); if (y > MOUTH_B) continue; }
    else if (x < XA + R) { const dx = x - (XA + R), dy = y - (YT + R); off = Math.hypot(dx, dy) - R; s = SEG[0] + (Math.atan2(dy, dx) + Math.PI) / (Math.PI / 2) * SEG[1]; }
    else if (x > XB - R) { const dx = x - (XB - R), dy = y - (YT + R); off = Math.hypot(dx, dy) - R; s = SEG[0] + SEG[1] + SEG[2] + (Math.atan2(dy, dx) + Math.PI / 2) / (Math.PI / 2) * SEG[3]; }
    else { off = YT - y; s = SEG[0] + SEG[1] + (x - XA - R); }
    const a = Math.abs(off);
    if (a > HW) continue;
    if (a > HW - 1.2) put(x, y, K.black);
    else if (Math.floor(s) % 8 === 0) put(x, y, off > 0 ? K.dark : K.light);   // a ring
    else if (off > HW - 3.5) put(x, y, K.light);                                // the shaded side
    else put(x, y, K.white);
  }
  for (const [x, y] of [[XA, MOUTH_A], [XB, MOUTH_B]]) {                       // the mouths: a lip
    q.fillStyle = K.black; q.fillRect(x - HW - 2, y - 3, 2 * HW + 5, 5);
    q.fillStyle = K.dark; q.fillRect(x - HW - 1, y - 2, 2 * HW + 3, 3);
    q.fillStyle = K.black; q.fillRect(x - HW + 1, y + 1, 2 * HW - 1, 1);
  }
  return c;
})();
function sprite(sp, flash) {                    // drawn pixel for pixel; flash: its screen lit white (taking the orb in)
  sp.rows.forEach((r, y) => {
    for (let x = 0; x < r.length; x++) {
      const v = +r[x];
      if (v) px(sp.x0 + x, sp.y0 + y, 1, 1, v === 6 && flash ? K.white : SPRITE_PAL[v]);
    }
  });
}
function monitor(img, flash) {
  void img;
  sprite(COMPUTER, flash);
}
function fm1(img, flash) {
  void img;
  sprite(FM1, flash);
}
function scene(cam, s) {                         // s: what's on the screens this frame, and the orb or specks
  px(0, 0, W, H, K.white);
  g.save();
  g.translate(-Math.round(cam), 0);
  g.drawImage(PIPE, 0, 0);
  monitor(s.sim, s.simFlash);
  fm1(s.fm, s.fmFlash);
  if (s.specks) for (const p of s.specks) px(p[0], p[1], 1, 1, p[2]);
  if (s.orb) orb(...s.orb);
  g.restore();
}

// ---- the menu: the whole choice in the window. The computer above, the FM-1 below, both full size; between them an
// arrow down (send) and an arrow up (bring). Pointing at one turns it red; clicking (or Enter) picks it.
let FULL = null;
function snapshot(draw, x0, y0, w, h) {          // a device drawn on its own, cropped to it
  const c = document.createElement("canvas");
  c.width = w; c.height = h;
  const keep = g;
  g = c.getContext("2d");
  g.translate(-x0, -y0);
  draw();
  g = keep;
  return c;
}
function devices() {                            // (each sprite, cropped to itself already)
  const of = (sp, draw) => snapshot(draw, sp.x0, sp.y0, sp.rows[0].length, sp.rows.length);
  return (FULL ||= { sim: of(COMPUTER, () => monitor(null, false)), fm: of(FM1, () => fm1(null, false)) });
}
function arrow(cx, y, down, c) {                 // 9 wide, 9 tall
  px(cx - 1, down ? y : y + 5, 3, 4, c);
  for (let k = 0; k < 5; k++) px(cx - 4 + k, down ? y + 4 + k : y + 4 - k, 9 - 2 * k, 1, c);
}
const PICKS = [{ to: "fm1", cx: 70, down: true }, { to: "sim", cx: 90, down: false }];   // (down on the left)
const SEND = PICKS.findIndex((p) => p.down), BRING = 1 - SEND;
function menu(hover) {
  const d = devices(), top = 1, bottom = H - 1 - d.fm.height;
  px(0, 0, W, H, K.white);
  g.drawImage(d.sim, (W - d.sim.width) >> 1, top);
  g.drawImage(d.fm, (W - d.fm.width) >> 1, bottom);
  const gap = top + d.sim.height, y = gap + ((bottom - gap - 9) >> 1);   // (the arrows, in the middle of the gap)
  PICKS.forEach((p, i) => arrow(p.cx, y, p.down, i === hover ? K.red : K.black));
}
let turn = 0;                                    // whose frames the window shows: the latest screen's
export function rest() { turn++; menu(SEND); }
// the choice: resolves "fm1" (send) or "sim" (bring) when an arrow is clicked
export function choose() {
  turn++;
  const cv = g.canvas;
  let hover = SEND;
  menu(hover);
  return new Promise((resolve) => {
    const side = (e) => { const r = cv.getBoundingClientRect(); return (e.clientX - r.left) / r.width * W < W / 2 ? 0 : 1; };
    const move = (e) => { const h = side(e); if (h !== hover) { hover = h; menu(hover); } };
    const pick = (i) => { cv.removeEventListener("pointermove", move); cv.removeEventListener("click", click); cv.removeEventListener("keydown", key); resolve(PICKS[i].to); };
    const click = (e) => pick(side(e));
    const key = (e) => {
      if (["ArrowLeft", "ArrowRight", "ArrowUp", "ArrowDown"].includes(e.key)) {
        e.preventDefault();
        hover = e.key === "ArrowDown" ? SEND : e.key === "ArrowUp" ? BRING : 1 - hover;
        menu(hover);
      }
      if (e.key === "Enter" || e.key === " ") { e.preventDefault(); pick(hover); }
    };
    cv.addEventListener("pointermove", move);
    cv.addEventListener("click", click);
    cv.addEventListener("keydown", key);
  });
}
// after a trade: on with a click (or Enter), as a handheld waits for its button
export function next() {
  const cv = g.canvas;
  return new Promise((resolve) => {
    const done = (e) => {
      if (e.type === "keydown" && e.key !== "Enter" && e.key !== " ") return;
      e.preventDefault();
      cv.removeEventListener("click", done); cv.removeEventListener("keydown", done);
      resolve();
    };
    cv.addEventListener("click", done);
    cv.addEventListener("keydown", done);
  });
}
// a word before any trade (no FM-1, the sim off): the orb, and two lines in the text box
export function notice(lines) {
  turn++;
  px(0, 0, W, H, K.white);
  orb(PIC.x + 32, PIC.y + 32, 22, performance.now());
  say(lines);
}

// ---- the trade: to = "fm1" (the sim's settings to the FM-1) or "sim"; src: (unused, kept for callers); job: the transfer
const ease = (k) => (k < 0.5 ? 2 * k * k : 1 - (-2 * k + 2) ** 2 / 2);
const TYPE = 28;                                 // ms a character, typing
export function trade(to, src, job) {
  const forward = to === "fm1";
  const got = forward ? ["FM-1 RECEIVED", "QUOD'S SETTINGS!"] : ["THE SIM RECEIVED", "THE FM-1'S SETTINGS!"];
  const lost = ["THE TRADE DIDN'T", "GO THROUGH."];
  let settled = null;
  job.then(() => { settled = "ok"; }, (e) => { settled = e; });
  if (matchMedia("(prefers-reduced-motion: reduce)").matches)
    return job.then(() => { px(0, 0, W, H, K.white); orb(PIC.x + 32, PIC.y + 32, 22, 0); say(got); },
                    (e) => { rest(); say(lost); throw e; });
  // each end: its screen's centre, its mouth (as a place along the pipe), and the view that shows it
  const A = { at: [SIM_SCR.x + SIM_SCR.w / 2, SIM_SCR.y + SIM_SCR.h / 2], s: 0, cam: 0 };
  const B = { at: [FM1_SCR.x + FM1_SCR.w / 2, FM1_SCR.y + FM1_SCR.h / 2], s: LEN, cam: WORLD - W };
  const [from, dest] = forward ? [A, B] : [B, A];
  const specks = Array.from({ length: 28 }, (_, i) => ({ a: i * 2.4, r: 26 + (i * 7) % 22, c: [K.red, K.black, K.dark][i % 3] }));
  return new Promise((resolve, reject) => {
    const mine = ++turn;                         // (a later screen stops this one's frames)
    let done = false;
    let phase = "ether", p0 = performance.now(), cam = from.cam, s = from.s, back = 0;
    const set = (ph) => { phase = ph; p0 = performance.now(); };
    const screens = (extra = {}) => ({ sim: forward ? src : getSim(), fm: null, ...extra });
    const mouth = (end) => along(end.s);
    function tick(t) {
      if (mine !== turn) return;
      const k = t - p0;
      if (phase === "ether") {                   // specks gather out of nothing onto the screen, and become the orb
        const q = Math.min(1, k / 1100), [cx, cy] = from.at;
        const sp = specks.map((p) => { const r = p.r * (1 - ease(q)), a = p.a + q * 4; return [cx + Math.cos(a) * r, cy + Math.sin(a) * r * 0.7, p.c]; });
        scene(cam, screens({ specks: q < 0.85 ? sp : null, orb: q > 0.7 ? [cx, cy, Math.round(2 + 4 * (q - 0.7) / 0.3), t] : null }));
        if (q >= 1) set("suck");
      } else if (phase === "suck") {             // the pipe draws it up into its mouth
        const q = Math.min(1, k / 650), a = q * q, [mx, my] = mouth(from), [cx, cy] = from.at;
        const x = cx + (mx - cx) * a + Math.sin(q * 20) * (1 - q), y = cy + (my - cy) * a;
        scene(cam, screens({ orb: [x, y, 6, t, 1 - 0.35 * Math.sin(q * Math.PI)] }));
        if (q >= 1) set("ride");
      } else if (phase === "ride" || phase === "back") {   // through the pipe, the view following; back, if it failed
        const q = Math.min(1, k / (phase === "ride" ? 2600 : 1400));
        const u = phase === "ride" ? ease(q) : back * (1 - ease(q));
        s = from.s + (dest.s - from.s) * u;
        const [x, y] = along(s);
        cam += (Math.max(0, Math.min(WORLD - W, x - W / 2)) - cam) * 0.14;
        scene(cam, screens({ orb: [x, y, 6, t] }));
        if (phase === "ride" && settled && settled !== "ok" && q > 0.15) { back = u; set("back"); }
        else if (q >= 1) set(phase === "ride" ? "wait" : "fall");
      } else if (phase === "wait") {             // at the far mouth until the transfer has settled
        const [x, y] = along(dest.s + (forward ? -4 : 4));
        cam += (dest.cam - cam) * 0.14;
        scene(cam, screens({ orb: [x, y + (Math.floor(k / 220) % 2), 6, t] }));
        if (settled === "ok" && k > 300) set("drop");
        else if (settled && settled !== "ok") { back = 1; set("back"); }
      } else if (phase === "drop" || phase === "fall") {   // out of the mouth, onto the screen below
        const end = phase === "drop" ? dest : from, [mx, my] = mouth(end), [cx, cy] = end.at;
        const q = Math.min(1, k / 600), y = my + (cy - my) * q * q;   // (falling: faster as it goes)
        cam += (end.cam - cam) * 0.14;
        scene(cam, screens({ orb: [mx + (cx - mx) * q, y, 6, t] }));
        if (q >= 1) set(phase === "drop" ? "absorb" : "lost");
      } else if (phase === "absorb") {           // the screen takes it in: it shrinks, the screen flashes, then shows it
        const q = Math.min(1, k / 800), [cx, cy] = dest.at;
        const flash = q > 0.35 && q < 0.75 && Math.floor(q * 16) % 2 === 0;
        const ex = forward ? { fm: q > 0.5 ? src : null, fmFlash: flash } : { simFlash: flash };
        scene(cam, screens({ ...ex, orb: q < 0.4 ? [cx, cy, Math.max(1, Math.round(6 * (1 - q / 0.4))), t] : null }));
        if (q >= 1) set("got");
      } else if (phase === "got") {              // the result: Quod's orb, spinning, and the words (it keeps spinning
        px(0, 0, W, H, K.white);                 // until the next screen takes the window)
        orb(PIC.x + 32, PIC.y + 32, 22, t);
        say(got, k / TYPE);
        if (k > got.join("").length * TYPE + 300 && !done) { done = true; resolve(true); }
      } else if (phase === "lost") {             // it didn't go through: the orb, shaking, then bursting
        const [cx, cy] = [PIC.x + 32, PIC.y + 32];
        px(0, 0, W, H, K.white);
        if (k < 600) {
          const j = Math.round((k / 600) * 3);   // (shaking harder)
          orb(cx + (Math.floor(k / 40) % 2 ? j : -j), cy + (Math.floor(k / 60) % 2 ? 1 : 0), 22, t);
        } else explode(cx, cy, k - 600);
        say(lost, (k - 300) / TYPE);
        if (k > 600 + 1400 && k > 300 + lost.join("").length * TYPE + 300) return reject(settled);
      }
      requestAnimationFrame(tick);
    }
    requestAnimationFrame(tick);
  });
}
