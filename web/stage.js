// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Kip DeCastro. See ../LICENSE.
//
// Over the sim's screen: how [ FLASH ] (flash.js) and [ LINK ] (link.js) open and close. Raising one fades the sim's
// sound out over a second, brings the panel into view, places the dialog over the screen (as wide as the screen; a
// Game Boy window in its middle, a panel from its top) and fades it in with the page dimmed behind it. Lowering it
// does the same backwards.
import { fadeLink } from "./app.js";

const PANEL_W = 520, SCR = { x: 20, y: 20, s: 480 };   // the sim's screen in the panel (app.js's layout)
const FADE_S = 1, SHOW_MS = 300;
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const placed = new Map();                        // an open dialog -> how it sits ("window" or "panel")

function place(dlg, how) {
  const r = document.getElementById("panel").getBoundingClientRect(), k = r.width / PANEL_W;
  const w = SCR.s * k, top = r.top + SCR.y * k;
  Object.assign(dlg.style, {
    left: `${r.left + SCR.x * k}px`, width: `${w}px`,
    top: `${how === "window" ? top + (w - (w * 9) / 10) / 2 : top}px`,   // (a 10:9 window in the square's middle)
  });
}
const replace = () => { for (const [dlg, how] of placed) place(dlg, how); };   // (it stays over the screen: the
window.addEventListener("resize", replace);                                   //  page resized, or still scrolling)
window.addEventListener("scroll", replace, { passive: true });

export async function raise(dlg, how) {
  const r = document.getElementById("panel").getBoundingClientRect(), k = r.width / PANEL_W;   // the screen, all in
  const top = r.top + SCR.y * k, bottom = top + SCR.s * k;                                       // view: its top
  if (top < 0 || bottom > innerHeight) window.scrollBy({ top: top - 16, behavior: "smooth" });  // just under the edge
  await Promise.all([fadeLink(0, FADE_S), sleep(FADE_S * 1000)]);   // (a whole second, sound or not: the scroll ends)
  place(dlg, how);
  placed.set(dlg, how);
  dlg.showModal();
  requestAnimationFrame(() => dlg.classList.add("shown"));
  await sleep(SHOW_MS);
}

export async function lower(dlg) {
  if (!dlg.open) return;
  dlg.classList.remove("shown");
  await sleep(SHOW_MS);
  dlg.close();
  placed.delete(dlg);
  await fadeLink(1, FADE_S);
}
