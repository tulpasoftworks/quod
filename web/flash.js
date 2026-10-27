// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Kip DeCastro
//
// [ FLASH ]: Quod onto an FM-1 (fm1/web/quodflash.js, on Felucca's updater; GPL as they are), one step at a time.
// Only the step being taken is on screen, and Next stays off until it is done:
//   1. the FM-1: what it runs;
//   2. the way back: on Quod, its state saved to a file; elsewhere, M-VAVE's own package (the FM-1 can't send its
//      firmware back over USB-MIDI), checked against its SHA-256: on M-VAVE's firmware the version it runs, on any
//      other tested on the FM-1 itself (its checks, cancelled before anything is written). An install cut off
//      skips this: it is only ever finished;
//   3. install: Quod (or the official firmware back), once the user says they'll keep it plugged in;
//   4. done: the FM-1's identity, checked when it comes back.
// Sending settings between the page and the FM-1 is [ LINK ] (link.js).
import { openMidi, QuodFlasher, checkQuodPackage, checkStockPackage, quodVersion } from "./fm1/quodflash.js";
import { raise, lower } from "./stage.js";

const $ = (id) => document.getElementById(id);
const dlg = $("flash-dialog"), body = $("fl-body");
const STEPS = ["The FM-1", "The way back", "Install", "Done"];
let flasher = null, quod = null;                // (the MIDI access and the page's package, kept across openings)
let S;

const MODE = {
  quod: (id) => `Quod ${quodVersion(id.text) === "dev" ? "(a development build)" : quodVersion(id.text)} (${id.text})`,
  stock: (id) => `the official firmware, V${+id.text.split("_")[1]} (${id.text})`,
  rescue: () => "its USB rescue: nothing is running, and anything can be installed",
  loader: (id) => `its update mode (${id.text}): an install was cut off`,
  "foreign-loader": (id) => `the update mode of another firmware (${id.text})`,
  other: (id) => `another firmware (${id.text})`,
};
const version = (id) => `V${+id.text.split("_")[1]}`;
const today = () => new Date().toISOString().slice(0, 10);

function reset() {
  S = { step: 0, dev: null, busy: false, note: "", kind: "", stateSaved: false, stock: null, tested: false,
        what: "quod", agree: false, progress: 0, writing: "", failed: false, back: "" };
}
const say = (t, kind = "") => { S.note = t; S.kind = kind; };
async function run(fn) {
  S.busy = true;
  draw();
  try { await fn(); } catch (e) { say(e.message, "bad"); } finally { S.busy = false; draw(); }
}
const writing = () => S.step === 2 && S.busy;   // (nothing closes or goes back while it writes)

// the way back this FM-1 has (the second step done)
function wayBack() {
  const d = S.dev;
  if (!d) return false;
  if (d.mode === "quod") return S.stateSaved;
  if (d.mode === "stock") return !!S.stock && S.stock.product === d.id.text;
  return S.tested;
}

// ---- the steps' actions
async function find() {
  say("Looking for the FM-1…");
  draw();
  if (!flasher) flasher = new QuodFlasher(await openMidi());
  const d = await flasher.identify();
  S.dev = d;
  S.stateSaved = S.tested = false;
  S.stock = null;
  if (!d) return say("No FM-1 answered. Is it on, on a USB data cable, and not open in another app?", "bad");
  if (d.mode === "foreign-loader") return say(`The FM-1 runs ${MODE[d.mode](d.id)}. Finish that update with its own updater first.`, "bad");
  say(`The FM-1 runs ${MODE[d.mode](d.id)}.`, "ok");
}

async function saveState() {
  say("Reading Quod's state from the FM-1…");
  draw();
  const rec = await flasher.backupState();
  const a = document.createElement("a");
  a.href = URL.createObjectURL(new Blob([rec], { type: "application/octet-stream" }));
  a.download = `fm1-quod-${today()}.quodstate`;
  a.click();
  setTimeout(() => URL.revokeObjectURL(a.href), 1000);
  S.stateSaved = true;
  say(`Saved ${a.download} to your downloads.`, "ok");
}

async function chooseStock(file) {
  S.stock = null;
  S.tested = false;
  S.what = "quod";                              // (the official one, back, only once this file is tested)
  say("Checking the file…");
  draw();
  const pkg = await checkStockPackage(await file.arrayBuffer());
  const d = S.dev;
  if (d.mode === "stock" && pkg.product !== d.id.text)
    return say(`That's ${pkg.version}, and the FM-1 runs ${version(d.id)}: choose ${version(d.id)}, the version it can go back to.`, "bad");
  S.stock = pkg;
  say(d.mode === "stock" ? `The official ${pkg.version}: checked. It stays in this page, nothing is uploaded.`
                         : `The official ${pkg.version}: checked. Next, test it on the FM-1.`, "ok");
}

async function testStock() {
  const v = S.stock.version;
  say(`Testing ${v} on the FM-1 (its checks, stopped before anything is written)…`);
  draw();
  try {
    await flasher.testWayBack(S.stock, (n) => { say(`Testing ${v} on the FM-1… (${n} reads)`); draw(); });
  } catch (e) {
    S.stock = null;
    throw e;
  }
  S.tested = true;
  say(`${v} can take this FM-1 back: it passed the FM-1's own checks, and nothing was written.`, "ok");
}

// Felucca's updater's errors, in words: what happened, and whether anything was written
function explain(e, name) {
  const before = /nothing was written/.test(e.message), n = (/after (\d+) requests/.exec(e.message) || [])[1];
  switch (e.code) {
    case "stopped": return before ? `The FM-1 refused ${name}${n ? ` after ${n} requests` : ""}. Nothing was written: it runs what it ran before.`
      : "The write stopped. Replug the USB cable and press Install again: the FM-1 stays in update mode until it's finished.";
    case "lost": return before ? "The FM-1 was disconnected. Nothing was written."
      : "The FM-1 was disconnected while writing. Plug it back in and press Install again: it finishes the write.";
    case "noloader": return "The FM-1 didn't come back in update mode. Replug the USB cable and press Install again: it stays in update mode until it's finished.";
    case "noreturn": return "The FM-1 didn't come back after writing. Switch it off and on.";
    case "mismatch": return `Written, but the FM-1 now reports ${e.detail}.`;
    default: return e.message;
  }
}

const STEP = {
  start: (a) => `Starting the update on ${a}…`,
  verify: () => "The FM-1 is checking the package…",
  loader: () => "Switching to update mode…",
  write: () => "Writing… Don't unplug the FM-1.",
  reboot: () => "Restarting…",
  done: (a) => `Done: the FM-1 runs ${a}.`,
};
async function install() {
  const pkg = S.what === "official" ? S.stock : quod, name = pkg === quod ? `Quod ${quod.version}` : pkg.version;
  S.failed = false;
  S.progress = 1;
  S.writing = "Don't unplug the FM-1 while it writes.";
  say("");
  draw();
  try {
    S.back = await flasher.install(pkg, (step, a) => {
      S.writing = (STEP[step] || (() => step))(a);
      if (step === "write") S.progress = Math.min(99, Math.max(S.progress, ((a * 512) / pkg.image.length) * 100));
      draw();
    });
  } catch (e) {
    S.failed = true;
    S.writing = explain(e, name);
    return;
  }
  S.progress = 100;
  S.dev = await flasher.identify();
  S.step = 3;
}

// ---- drawing: the rail, the step on its own, Back / Next
function stepText() {
  const d = S.dev;
  switch (S.step) {
    case 0: return ["Connect the FM-1", "Plug it into this computer with a USB data cable and switch it on."];
    case 1:
      if (d.mode === "quod") return ["Save Quod's state",
        "The FM-1 runs Quod. Save what it plays now (the controls, the steps, their notes and locks) before installing over it. " +
        "To put the official firmware back instead, also choose M-VAVE's FM-1.fwsc and test it on the FM-1."];
      if (d.mode === "stock") return [`Your copy of ${version(d.id)}`,
        `The FM-1 can't send its firmware back over USB, so the way back is M-VAVE's own file. It runs ${version(d.id)}: choose that FM-1.fwsc.`];
      return ["A way back, tested", "The FM-1 can't send its firmware back over USB, so the way back is M-VAVE's own file. " +
        "Choose it, then test it on the FM-1: it runs its checks and stops before writing anything."];
    case 2: return d.mode === "loader"
      ? ["Finish the install", "An install was cut off, and the FM-1 is waiting in update mode. Finish it now."]
      : [quod ? `Install Quod ${quod.version}` : "Install Quod", "Takes about a minute. The FM-1 restarts on its own when it's done."];
    case 3: return ["Done", `The FM-1 runs ${S.dev ? MODE[S.dev.mode](S.dev.id) : S.back}.`];
  }
}

const button = (label, act, off, cls = "plain") => `<button class="${cls}" data-act="${act}" ${off ? "disabled" : ""}>${label}</button>`;

function actions() {
  const d = S.dev, h = [];
  if (S.step === 0) h.push(button("Find the FM-1", "find", S.busy));
  if (S.step === 1 && d.mode === "quod") h.push(button("Save Quod's state", "saveState", S.busy || S.stateSaved));
  if (S.step === 1) {                           // (on Quod, the official package is optional: only to go back)
    h.push(button(S.stock ? "Choose another file" : "Choose FM-1.fwsc", "pick", S.busy));
    if (d.mode !== "stock") h.push(button("Test it on the FM-1", "test", S.busy || !S.stock || S.tested));
  }
  if (S.step === 2 && (!S.progress || S.failed)) {
    if (S.stock && S.tested && d.mode !== "loader" && d.mode !== "stock") h.push(`<div class="what">
      <label><input type="radio" name="fl-what" value="quod" ${S.what === "quod" ? "checked" : ""}> Quod ${quod ? quod.version : ""}</label>
      <label><input type="radio" name="fl-what" value="official" ${S.what === "official" ? "checked" : ""}> the official ${S.stock.version}, back</label></div>`);
    h.push(`<label class="agree"><input type="checkbox" id="fl-agree" ${S.agree ? "checked" : ""} ${S.busy ? "disabled" : ""}>
      <span>I'll keep the FM-1 plugged in until this says Done.</span></label>`);
    const what = S.what === "official" ? `the official ${S.stock.version}` : "Quod";
    const label = S.failed ? "Install again" : d.mode === "loader" ? "Finish the install" : `Install ${what}`;
    h.push(button(label, "install", !S.agree || S.busy || (S.what === "quod" && !quod), "fm1btn"));
  }
  if (S.step === 3 && S.dev && S.dev.mode === "quod")
    h.push(`<p class="small">To send settings between this page and the FM-1, use [ LINK ].</p>`);
  return h.join("");
}

function draw() {
  const [title, text] = stepText(), d = S.dev;
  const canNext = !S.busy && ((S.step === 0 && d && d.mode !== "foreign-loader") || (S.step === 1 && wayBack()));
  const canBack = !S.busy && (S.step === 1 || (S.step === 2 && (!S.progress || S.failed)));
  body.innerHTML = `
    <div class="rail">${STEPS.map((t, k) => `<div class="${k < S.step ? "done" : k === S.step ? "now" : ""}"><i></i><span>${t}</span></div>`).join("")}</div>
    <h3>${title}</h3>
    <p class="lead">${text}</p>
    <div class="row">${actions()}</div>
    <p class="msg" data-kind="${S.kind}" aria-live="polite">${S.note}</p>
    ${S.step === 2 && S.progress ? `<p><progress max="100" value="${S.progress}"></progress></p>
      <p class="msg" data-kind="${S.failed ? "bad" : ""}" aria-live="polite">${S.writing}</p>` : ""}
    ${S.step === 2 && !quod ? `<p class="msg" data-kind="bad">This page carries no Quod package.</p>` : ""}
    ${S.failed ? `<p class="small">If the FM-1 doesn't start up: hold OCT\u2212 while switching it on (its USB rescue), then install again.</p>` : ""}
    <div class="foot">
      ${canBack ? button("Back", "back", false, "link") : "<span></span>"}
      ${S.step < 2 ? button("Next", "next", !canNext, "fm1btn") : S.step === 3 ? button("Close", "close", false, "fm1btn") : ""}
    </div>`;
  $("fl-close").disabled = writing();
}

const act = {
  find: () => run(find),
  saveState: () => run(saveState),
  pick: () => $("fl-file").click(),
  test: () => run(testStock),
  install: () => run(install),
  next: () => { S.step = S.dev.mode === "loader" ? 2 : S.step + 1; say(""); draw(); },
  back: () => { S.step = S.step === 2 && S.dev.mode === "loader" ? 0 : S.step - 1; S.agree = false; say(""); draw(); },
  close: () => lower(dlg),
};

// [ FLASH ] talks to the FM-1 with Web MIDI and SysEx: Chrome and Edge (Chromium) have both
export function canFlash() {
  return !!navigator.requestMIDIAccess && !!navigator.userAgentData?.brands?.some((b) => /Chromium/.test(b.brand));
}

export function setupFlash() {
  if (!canFlash()) {
    $("flash").disabled = true;
    $("flash-note").textContent = "[ FLASH ] needs Chrome or Edge: it reaches the FM-1 over USB with Web MIDI, which this browser doesn't have.";
    return;
  }
  reset();
  $("flash").addEventListener("click", () => {
    if (dlg.open) return;
    if (!S.busy) reset();
    draw();
    raise(dlg, "panel");                         // (over the sim's screen, as [ LINK ]: stage.js)
  });
  $("fl-close").addEventListener("click", () => { if (!writing()) lower(dlg); });
  dlg.addEventListener("cancel", (e) => { e.preventDefault(); if (!writing()) lower(dlg); });
  body.addEventListener("click", (e) => {
    const b = e.target.closest("[data-act]");
    if (b && !b.disabled) act[b.dataset.act]();
  });
  body.addEventListener("change", (e) => {
    if (e.target.id === "fl-agree") { S.agree = e.target.checked; draw(); }
    if (e.target.name === "fl-what") { S.what = e.target.value; draw(); }
  });
  $("fl-file").addEventListener("change", (e) => {
    const f = e.target.files[0];
    e.target.value = "";                        // (the same file chosen again is a change too)
    if (f) run(() => chooseStock(f));
  });
  // the package this page carries (firmware/quod.json)
  fetch("firmware/quod.json", { cache: "no-store" }).then((r) => (r.ok ? r.json() : Promise.reject(new Error())))
    .then((m) => fetch(`firmware/${m.file}`).then((r) => r.arrayBuffer()).then((b) => { quod = { ...checkQuodPackage(b), version: m.version }; }))
    .catch(() => { quod = null; })
    .finally(() => dlg.open && draw());
}
