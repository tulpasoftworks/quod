// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Kip DeCastro
//
// Quod on an FM-1 from a browser (Web MIDI with SysEx: Chrome or Edge), on Felucca's updater (fm1ota.js, fm1pkg.js):
// what the FM-1 runs, Quod's state to a file and back, Quod installed, and the way back to the official firmware.
// The same as tools/fm1_install.py (--info, --backup, --restore, PACKAGE), which this follows.
//
// The FM-1 can't send its firmware back over USB-MIDI (the updater only reads from the computer), so the way back
// is the official FM-1 V15 package, the user's own copy, checked here against its SHA-256 before anything is written.
import { Updater, pack7, unpack7 } from "./fm1ota.js";
import { Package, productOf, logicalImage, validateStockPackage, sha256hex } from "./fm1pkg.js";

// Quod's identity: FM-1_Q and its version, X.Y as two digits (FM-1_Q02: Quod 0.2; FM-1_Q00: a development build)
export const QUOD_ID = /^FM-1_Q(\d)(\d)$/;
export function quodVersion(text) {             // "FM-1_Q02" -> "0.2", "FM-1_Q00" -> "dev", else null
  const m = QUOD_ID.exec(text);
  return !m ? null : m[1] === "0" && m[2] === "0" ? "dev" : `${m[1]}.${m[2]}`;
}
const LOADER_MARK = "FELUCCA-LOADER-1";
const QUOD_GET = 0x51, QUOD_STATE = 0x52, QUOD_PUT = 0x53, QUOD_PUT_RC = 0x54;   // firmware/src/quod.c
const QUOD_RC = {
  1: "That isn't a Quod state file.",
  2: "The file is damaged.",
  3: "That state is from another version of Quod.",
  4: "The FM-1's flash failed to save it.",
  5: "Quod isn't running (the FM-1 is in its USB rescue).",
};

function fail(code, msg) { const e = new Error(msg); e.code = code; return e; }

// M-VAVE's official firmware: V15 (fm1pkg.js pins it), and V16 (2026-10-21, as m-vave.com/download serves it:
// https://yms-file-store.oss-cn-hongkong.aliyuncs.com/software/firmware/FM-1.fwsc). V16 carries V15's update loader
// unchanged (the one firmware/src/ota.c knows as official), in another flash layout.
const STOCK = { "FM-1_016": { size: 718196, sha256: "d017fb65e70aca97ee29d944470c3515a74581df5c0576bf70de917c7b2b61d5" } };
export const STOCK_IDS = ["FM-1_015", ...Object.keys(STOCK)];

// what the FM-1 runs, by its identity
export function modeOf(id) {
  if (/^ota-/i.test(id.model)) return /^ota-FM-1_9\d\d$/i.test(id.text) ? "loader" : "foreign-loader";
  if (id.text === "FM-1_000") return "rescue";
  if (QUOD_ID.test(id.text)) return "quod";
  if (STOCK_IDS.includes(id.text)) return "stock";
  return "other";
}

export async function openMidi() {
  if (!navigator.requestMIDIAccess) throw fail("nomidi", "This browser has no Web MIDI: use Chrome or Edge on a computer.");
  try {
    return await navigator.requestMIDIAccess({ sysex: true });
  } catch (_) {
    throw fail("denied", "The browser wasn't allowed to use MIDI with SysEx. Allow it (the icon in the address bar) and try again.");
  }
}

// ---- packages

function crc16(d, crc = 0) {                     // CRC-16/XMODEM, as the package and the FM-1 use
  for (const b of d) {
    crc ^= b << 8;
    for (let i = 0; i < 8; i++) crc = crc & 0x8000 ? ((crc << 1) ^ 0x1021) & 0xFFFF : (crc << 1) & 0xFFFF;
  }
  return crc;
}
function dec(b) {                                // JieLi's header cipher (its own inverse)
  const out = Uint8Array.from(b);
  let key = 0xFFFF;
  for (let i = 0; i < out.length; i++) {
    out[i] ^= key & 0xFF;
    key = ((key << 1) ^ (key & 0x8000 ? 0x1021 : 0)) & 0xFFFF;
  }
  return out;
}
const u16 = (b, o) => b[o] | (b[o + 1] << 8);
const u32 = (b, o) => (b[o] | (b[o + 1] << 8) | (b[o + 2] << 16) | (b[o + 3] << 24)) >>> 0;

// what is wrong with an update image (header, file list, each file's CRC), or null (fm1_install.py package_damage):
// the FM-1 checks only the loader before it rewrites the firmware, so a damaged file is caught here
export function packageDamage(image) {
  if (image.length < 0x40) return "too short";
  const hdr = dec(image.subarray(0, 0x40));
  if (crc16(hdr.subarray(2, 0x40)) !== u16(hdr, 0)) return "header CRC";
  const count = u16(hdr, 8);
  if (count < 1 || count > 8 || image.length < 0x40 + 0x50 * count) return "file list";
  const list = image.subarray(0x40, 0x40 + 0x50 * count);
  if (crc16(list) !== u16(hdr, 2)) return "file list CRC";
  for (let i = 0; i < count; i++) {
    const e = dec(list.subarray(i * 0x50, (i + 1) * 0x50));
    const off = u32(e, 8), size = u32(e, 12);
    const name = String.fromCharCode(...e.subarray(0x40, 0x50)).replace(/\0.*$/, "");
    if (off + size > image.length) return `${name}: cut short`;
    if (crc16(image.subarray(off, off + size)) !== u16(e, 4)) return `${name}: CRC`;
  }
  return null;
}

// a Quod package (.fwsc) -> { product, image }; throws with what is wrong
export function checkQuodPackage(bytes) {
  const raw = new Uint8Array(bytes);
  let product;
  try { product = productOf(raw); new Package(raw); } catch (e) { throw fail("badpkg", `Not an FM-1 package (${e.message}).`); }
  if (!QUOD_ID.test(product)) throw fail("badpkg", `That package is ${product}, not Quod (FM-1_Q and its version).`);
  if (!new TextDecoder("latin1").decode(raw).includes(LOADER_MARK)) throw fail("badpkg", "That package has no Felucca update loader.");
  const image = logicalImage(raw);
  const bad = packageDamage(image);
  if (bad) throw fail("badpkg", `The package is damaged (${bad}); nothing was written. Download it again.`);
  return { product, image };
}

// the user's official FM-1 package (V15 or V16) -> { product, image, version }: its exact size and SHA-256
export async function checkStockPackage(bytes) {
  const raw = new Uint8Array(bytes);
  let product = "";
  try { product = productOf(raw); } catch (_) {}
  const pin = STOCK[product];
  if (pin) {
    if (raw.length !== pin.size || (await sha256hex(raw)) !== pin.sha256)
      throw fail("badstock", `That ${product} isn't M-VAVE's file unchanged.`);
    new Package(raw);                           // (its header, file list and flash image, each by its CRC)
    return { product, image: logicalImage(raw), version: `V${+product.split("_")[1]}` };
  }
  try {
    return { ...(await validateStockPackage(raw)), version: "V15" };
  } catch (e) {
    throw fail("badstock", e.message);
  }
}

// ---- Quod's own commands, in the updater's framing (firmware/src/ota.c)

export function appMsg(cmd, body) {
  const n = body.length;
  let s = 0;
  for (const b of body) s += b;
  return [0xF0, ...pack7([0x00, 0x59, cmd, n & 0xFF, (n >> 8) & 0xFF, 0, ...body, ~s & 0xFF]), 0xF7];
}
export function parseApp(pkt, cmd) {
  if (pkt[0] !== 0xF0 || pkt[pkt.length - 1] !== 0xF7) return null;
  const u = unpack7(pkt.slice(1, -1));
  if (u.length < 7 || u[0] !== 0 || u[1] !== 0x59 || u[2] !== cmd) return null;
  const n = u[3] | (u[4] << 8) | (u[5] << 16);
  if (u.length < n + 7) return null;
  let s = 0;
  for (let i = 6; i < 6 + n; i++) s += u[i];
  return (~s & 0xFF) === u[6 + n] ? new Uint8Array(u.slice(6, 6 + n)) : null;
}

// ---- the way back, tested: the FM-1's own checks of an official package, cancelled before anything is written.
// The updater (firmware/src/ota.c) reads the package, stages its loader, then asks the computer to confirm
// (0xE0000000): nothing is committed without "success", and anything else unstages it. A refusal on the way shows in
// what it asked last: its start-up area (the first 16 KiB of flash.bin), which only a package with the same one may
// rewrite, is the common one.
const UPGRADE = [0xF0, 0x22, 0x24, 0x35, 0x7F, 0xF7];
const CONFIRM = 0xE0000000, HEAD = 0x4000;
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
function response(addr, data, fl = 0) {         // (fm1ota.js's, which it doesn't export)
  const len = data.length;
  const body = [0x00, 0x59, 0x30, (len + 8) & 0xFF, ((len + 8) >> 8) & 0xFF, 0, fl,
    addr & 0xFF, (addr >>> 8) & 0xFF, (addr >>> 16) & 0xFF, (addr >>> 24) & 0xFF, len & 0xFF, (len >> 8) & 0xFF, 0, ...data];
  let s = 0;
  for (let i = 6; i < body.length; i++) s += body[i];
  body.push(~s & 0xFF);
  return [0xF0, ...pack7(body), 0xF7];
}
function parseRequest(pkt) {
  if (pkt[0] !== 0xF0 || pkt[pkt.length - 1] !== 0xF7) return null;
  const u = unpack7(pkt.slice(1, -1));
  if (u.length !== 15 || u[0] !== 0 || u[1] !== 0x59 || u[2] !== 0x30) return null;
  let s = 0;
  for (let i = 6; i < 14; i++) s += u[i];
  if ((~s & 0xFF) !== u[14]) return null;
  return { fl: u[6], addr: u32(u, 7), len: u[11] | (u[12] << 8) | (u[13] << 16) };
}
function flashBinAt(image) {                    // where flash.bin (the firmware, its start-up area first) is, or -1
  const hdr = dec(image.subarray(0, 0x40)), count = u16(hdr, 8);
  for (let i = 0; i < count; i++) {
    const e = dec(image.subarray(0x40 + i * 0x50, 0x40 + (i + 1) * 0x50));
    if (u16(e, 0) === 0) return u32(e, 8);
  }
  return -1;
}

export class QuodFlasher {
  constructor(access) {
    this.up = new Updater(access);
  }

  // -> { id, mode, name } of the FM-1 that answers, or null
  async identify() {
    const dev = await this.up.find();
    if (!dev) return null;
    dev.link.close();
    return { id: dev.id, mode: modeOf(dev.id), name: dev.name };
  }

  async ask(cmd, body, want) {
    const dev = await this.up.find((id) => QUOD_ID.test(id.text));
    if (!dev) throw fail("notquod", "No FM-1 running Quod answered.");
    try {
      dev.link.drain();
      if (!dev.link.send(appMsg(cmd, body))) throw fail("lost", "The FM-1 was disconnected.");
      const end = Date.now() + 3000;
      while (Date.now() < end) {
        const p = await dev.link.read(end - Date.now());
        if (!p) break;
        const r = parseApp(p, want);
        if (r) return r;
      }
      throw fail("old", "The FM-1 didn't answer: its Quod is older than the state backup. Install this one.");
    } finally {
      dev.link.close();
    }
  }

  // Quod's state on the FM-1 (what it plays now, saved or not) -> a .quodstate file's bytes
  async backupState() {
    const r = await this.ask(QUOD_GET, [], QUOD_STATE);
    if (r[0]) throw fail("state", QUOD_RC[r[0]] || `Error ${r[0]}.`);
    return r.slice(1);
  }

  // a .quodstate file -> the FM-1: it plays it, and saves it (the sound stops for a moment while it saves)
  async restoreState(rec) {
    const r = await this.ask(QUOD_PUT, Array.from(rec), QUOD_PUT_RC);
    if (r[0]) throw fail("state", QUOD_RC[r[0]] || `Error ${r[0]}.`);
  }

  // can the official package { product, image, version } take this FM-1 back? Its checks, run and cancelled: true,
  // or it throws why not. Not on M-VAVE's firmware (its updater isn't ota.c). onStep(requests)
  async testWayBack(pkg, onStep = () => {}) {
    const dev = await this.up.find((id) => !/^ota-/i.test(id.model));
    if (!dev) throw fail("notfound", "FM-1 not found. Is it on, on a USB data cable, and not open in another app?");
    const link = dev.link, head = flashBinAt(pkg.image);
    let asked = 0, last = -1, confirms = 0, quiet = Date.now();
    try {
      link.drain();
      link.send(UPGRADE);
      await sleep(2000);
      for (;;) {
        const pkt = await link.read(1000);
        if (!pkt) {
          if (link.lost) throw fail("lost", "The FM-1 was disconnected during the test. Nothing was written.");
          if (Date.now() - quiet > 8000) break;
          continue;
        }
        const r = parseRequest(pkt);
        if (!r) continue;
        quiet = Date.now();
        if (r.addr === CONFIRM) {               // its checks passed: no (it asks four times)
          link.send(response(r.addr, [...new TextEncoder().encode("cancel"), 0, 0], r.fl));
          if (++confirms >= 4) break;
          continue;
        }
        if (r.len > 512 || r.addr + r.len > pkg.image.length) throw fail("badreq", "The FM-1 asked for a part outside the package. Nothing was written.");
        await sleep(10);
        link.send(response(r.addr, Array.from(pkg.image.subarray(r.addr, r.addr + r.len)), r.fl));
        last = r.addr;
        onStep(++asked);
      }
    } finally {
      link.close();
    }
    if (confirms) return true;
    if (head >= 0 && last >= head && last < head + HEAD)
      throw fail("head", `${pkg.version} can't take this FM-1 back: its start-up area isn't ${pkg.version}'s (it came from another version). Nothing was written.`);
    throw fail("refused", `The FM-1 refused ${pkg.version} after ${asked} requests. Nothing was written.`);
  }

  // install { product, image } (checkQuodPackage or checkStockPackage); an FM-1 left in update mode is finished
  // first (Felucca's loader with any, M-VAVE's with an official package). onStep(step, detail): start, verify,
  // loader, write, reboot, done
  async install(pkg, onStep) {
    const dev = await this.identify();
    if (!dev) throw fail("notfound", "FM-1 not found. Is it on, on a USB data cable, and not open in another app?");
    if (dev.mode === "foreign-loader" && !STOCK_IDS.includes(pkg.product))
      throw fail("foreign", `The FM-1 is in the update mode of another firmware (${dev.id.text}): finish that update with its own updater first.`);
    if (dev.mode === "loader" || dev.mode === "foreign-loader") {
      if (!(await this.up.resume(pkg.image, onStep, pkg.product)))
        throw fail("stopped", "The write didn't finish. Replug the USB cable and press Install again: the FM-1 stays in update mode until it is done.");
      return pkg.product;
    }
    return this.up.install(pkg.image, pkg.product, onStep);
  }
}
