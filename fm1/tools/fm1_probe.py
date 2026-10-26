#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Drive quod on the FM-1 over USB-MIDI and listen to it over USB audio: find what makes discontinuities.

  fm1_probe.py [--seconds S] [--stir] [--out DIR] RUN...
  RUN: name=MASK[,cc=val...]  e.g. all=0  tone=TONE  noreverb=REVERB  only-root=ANGELS+HELL
  MASK: 0 or bypass names joined by + (see quod.c: ANGELS HELL ROOT CHOIR REVERB DRIVE DELAY FOLD PHYSICS DRIFT
        SOFT TONE DC); cc=val sets control CC 30+i style values (e.g. 33=127 sets HELL to max).

Each run: the defaults (CC 103 = 3), the mask, the controls, a C3 drone (or --stir: a new note every 250 ms),
then S seconds recorded from "Felucca" at its native 44.1 kHz. It prints jumps a second (a sample-to-sample step
far above the local slope) and the peak. Needs sounddevice, soundfile, numpy, mido + python-rtmidi."""
import argparse, os, subprocess, sys, threading, time
import numpy as np
import mido, sounddevice as sd, soundfile as sf

BITS = dict(ANGELS=1, HELL=2, ROOT=4, CHOIR=8, REVERB=16, DRIVE=32, DELAY=64, FOLD=128, PHYSICS=256, DRIFT=512,
            SOFT=1024, TONE=2048, DC=4096, PREMASTER=8192)
FS = 44100


def set_rate(rate):
    """macOS: put the FM-1's USB audio at its native rate (no 44.1 -> 48 kHz resampling on the device)."""
    here = os.path.dirname(os.path.abspath(__file__))
    code = r'''
import ctypes, ctypes.util, struct, sys
CA = ctypes.cdll.LoadLibrary(ctypes.util.find_library('CoreAudio'))
CF = ctypes.cdll.LoadLibrary(ctypes.util.find_library('CoreFoundation'))
def cc(s): return struct.unpack('>I', s.encode())[0]
class A(ctypes.Structure): _fields_ = [('s', ctypes.c_uint32), ('c', ctypes.c_uint32), ('e', ctypes.c_uint32)]
a = A(cc('dev#'), cc('glob'), 0); sz = ctypes.c_uint32(0)
CA.AudioObjectGetPropertyDataSize(1, ctypes.byref(a), 0, None, ctypes.byref(sz))
buf = ctypes.create_string_buffer(sz.value); CA.AudioObjectGetPropertyData(1, ctypes.byref(a), 0, None, ctypes.byref(sz), buf)
CF.CFStringGetCString.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_long, ctypes.c_uint32]
for d in struct.unpack('%dI' % (len(buf.raw) // 4), buf.raw):
    n = A(cc('lnam'), cc('glob'), 0); ref = ctypes.c_void_p(); s8 = ctypes.c_uint32(8)
    CA.AudioObjectGetPropertyData(d, ctypes.byref(n), 0, None, ctypes.byref(s8), ctypes.byref(ref))
    b = ctypes.create_string_buffer(256); CF.CFStringGetCString(ref, b, 256, 0x08000100)
    if b.value == b'Felucca':
        r = A(cc('nsrt'), cc('glob'), 0); v = ctypes.c_double(float(sys.argv[1]))
        CA.AudioObjectSetPropertyData(d, ctypes.byref(r), 0, None, 8, ctypes.byref(v))
'''
    subprocess.run([sys.executable, '-I', '-c', code, str(rate)], check=True)


def jumps(x):
    """sample steps far above the local slope, a second; and the peak"""
    d1 = np.abs(np.diff(x))
    loc = np.convolve(d1, np.ones(441) / 441, mode='same')
    st = np.where(d1 > 6 * loc + 400)[0]
    return len(st) / (len(x) / FS), int(np.abs(x).max()), st


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('runs', nargs='+')
    ap.add_argument('--seconds', type=float, default=6)
    ap.add_argument('--stir', action='store_true')
    ap.add_argument('--out', default='build/listen')
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    set_rate(FS)
    name = next(n for n in mido.get_output_names() if 'Felucca' in n)
    out = mido.open_output(name)
    cc = lambda c, v: out.send(mido.Message('control_change', channel=15, control=c, value=v))
    for run in a.runs:
        label, _, spec = run.partition('=')
        parts = spec.split(',') if spec else ['0']
        mask = 0
        for b in parts[0].split('+'):
            if b and b != '0': mask |= BITS[b.upper()]
        cc(103, 3)                                   # the defaults
        cc(103, 2)                                   # sequencer stopped
        mask |= BITS['PREMASTER']                    # (the USB audio before the MASTER knob)
        cc(100, mask & 127); cc(101, mask >> 7)
        for kv in parts[1:]:
            k, v = kv.split('='); cc(int(k), int(v))
        out.send(mido.Message('note_on', note=60, velocity=100)); out.send(mido.Message("note_off", note=60))
        time.sleep(2.5)                              # let it settle into the new state
        stop = threading.Event()
        def stir():
            notes = [60, 67, 63, 55, 72, 48, 65, 58]; i = 0
            while not stop.is_set():
                out.send(mido.Message('note_on', note=notes[i % len(notes)], velocity=100)); i += 1
                time.sleep(0.25)
        th = threading.Thread(target=stir) if a.stir else None
        if th: th.start()
        x = sd.rec(int(a.seconds * FS), samplerate=FS, channels=2, device='Felucca', dtype='int16'); sd.wait()
        stop.set()
        if th: th.join()
        sf.write(os.path.join(a.out, f'probe-{label}.wav'), x, FS)
        rate, peak, _ = jumps(x[:, 0].astype(float))
        print(f'{label:16s} mask {mask & 8191:5d}  jumps/s {rate:7.1f}  peak {peak}', flush=True)
    cc(100, 0); cc(101, 0)


if __name__ == '__main__':
    main()
