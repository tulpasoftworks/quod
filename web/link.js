// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Kip DeCastro
//
// [ LINK ]: Quod's settings (the controls, the steps, their notes and locks: a .quodstate) between the sim and an FM-1
// running Quod, over USB-MIDI (fm1/web/quodflash.js; GPL with it), drawn as a trade (trade.js). Each of the two
// buttons fades the sim's sound out over a second, then the trade's window fades in over the sim's screen with the
// page dimmed behind it; a click on the result fades it out and brings the sound back.
import { openMidi, QuodFlasher } from "./fm1/quodflash.js";
import { simBackup, simRestore, running, simScreen } from "./app.js";
import { raise, lower } from "./stage.js";
import { canFlash } from "./flash.js";
import { setupTrade, next, notice, trade } from "./trade.js";

const $ = (id) => document.getElementById(id);
const dlg = $("link-dialog"), cv = $("ln-canvas");
let flasher = null, busy = false;
setupTrade(cv, simScreen);

const say = (t, kind = "") => { $("ln-msg").textContent = t; $("ln-msg").dataset.kind = kind; };
const WINDOW = {                                 // (quodflash.js's errors, in the window's two lines)
  nomidi: ["THIS BROWSER HAS", "NO WEB MIDI."],
  denied: ["ALLOW MIDI IN THE", "ADDRESS BAR."],
  old: ["THE FM-1'S QUOD IS", "TOO OLD: FLASH IT."],
};
function short(e, lines) { e.lines = lines; return e; }   // (the window's two lines for an error before a trade)

async function quodFM1() {
  if (!running()) throw short(new Error("Switch the sim on first: its settings live in the sim's engine."), ["SWITCH THE SIM", "ON FIRST."]);
  notice(["LOOKING FOR", "THE FM-1..."]);
  if (!flasher) flasher = new QuodFlasher(await openMidi());
  const d = await flasher.identify();
  if (!d) throw short(new Error("No FM-1 answered. Is it on, on a USB data cable, and not open in another app?"), ["NO FM-1", "ANSWERED."]);
  if (d.mode !== "quod") throw short(new Error(`The FM-1 runs ${d.id.text}, not Quod. Install Quod with [ FLASH ] first.`), ["THE FM-1 ISN'T", "RUNNING QUOD."]);
}
async function move(to) {
  try {
    try { await quodFM1(); } catch (e) { notice(e.lines || WINDOW[e.code] || ["THAT DIDN'T", "WORK."]); throw e; }
    if (to === "fm1") {
      await trade("fm1", null, simBackup().then((b) => flasher.restoreState(b)));
      say("The FM-1 plays the sim's settings now, and has saved them.", "ok");
    } else {
      await trade("sim", null, flasher.backupState().then((b) => simRestore(b)));
      say("The sim plays the FM-1's settings now. Press SAVE to keep them in this browser.", "ok");
    }
  } catch (e) {
    say(e.message, "bad");
  }
}

// one move, start to finish: the sound out, the window in, the trade, a click, the window out, the sound back
async function link(to) {
  if (busy) return;
  busy = true;
  for (const id of ["ln-send", "ln-bring"]) $(id).disabled = true;
  say("");
  notice(["", ""]);                              // (a blank window to fade in on)
  await raise(dlg, "window");
  cv.focus({ focusVisible: false });             // (Enter works; no ring for a mouse user)
  await move(to);
  await next();
  await lower(dlg);
  for (const id of ["ln-send", "ln-bring"]) $(id).disabled = false;
  busy = false;
}

export function setupLink() {
  if (!canFlash()) {
    for (const id of ["ln-send", "ln-bring"]) $(id).disabled = true;
    $("link-note").textContent = "[ LINK ] needs Chrome or Edge: it reaches the FM-1 over USB with Web MIDI, which this browser doesn't have.";
    return;
  }
  $("ln-send").addEventListener("click", () => link("fm1"));
  $("ln-bring").addEventListener("click", () => link("sim"));
  dlg.addEventListener("cancel", (e) => e.preventDefault());   // (a click on the window ends it, once it's done)
}
