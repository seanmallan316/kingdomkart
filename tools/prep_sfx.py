#!/usr/bin/env python3
"""Turns recorded sound effects in assets/sfx/ into what the game plays (content/sfx/NAME.wav, 32 kHz 16-bit).

File names the game looks for:
  kart_engine_idle, kart_engine_acceleration, kart_engine_top_speed, kart_engine_deceleration   (loops: blended by speed)
  kart_engine_start, kart_gear_shift, kart_exhaust_pop, kart_braking, kart_hard_braking            (one-shots)
  sting_countdown (3-2-1-GO), sting_unlock (something new unlocked)                                 (stereo one-shots)
  sfx_<name> replaces one of the game's own synthesized effects, e.g. sfx_coin.wav, sfx_lap.wav
Loops get their last 80 ms blended into their first 80 ms so they repeat without a click. Everything is level-matched.
Any input format works. Files already up to date are skipped.
"""
import os, subprocess, glob, wave, struct
import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "assets", "sfx")
DST = os.path.join(ROOT, "content", "sfx")
os.makedirs(DST, exist_ok=True)
SR = 32000
LOOPS = {"kart_engine_idle", "kart_engine_acceleration", "kart_engine_top_speed", "kart_engine_deceleration"}

def decode(path, channels):
    raw = subprocess.run(["ffmpeg", "-nostdin", "-v", "error", "-i", path, "-map", "0:a:0", "-vn", "-ac", str(channels),
                          "-ar", str(SR), "-f", "f32le", "-"], capture_output=True, check=True).stdout
    x = np.frombuffer(raw, np.float32).reshape(-1, channels)
    return x.copy()

def trim(x, thresh=0.004):
    a = np.abs(x).max(axis=1)
    idx = np.where(a > thresh)[0]
    if len(idx) == 0: return x
    s, e = max(0, idx[0] - 160), min(len(x), idx[-1] + 800)
    return x[s:e]

def make_loop(x, ms=80):
    n = int(SR * ms / 1000)
    if len(x) < 4 * n: return x
    head, tail = x[:n], x[-n:]
    w = np.linspace(0, 1, n, dtype=np.float32)[:, None]
    x = x[:-n].copy()
    x[:n] = head * w + tail * (1 - w)
    return x

def write(path, x):
    x = np.clip(x, -1, 1)
    with wave.open(path, "wb") as w:
        w.setnchannels(x.shape[1]); w.setsampwidth(2); w.setframerate(SR)
        w.writeframes((x * 32767).astype("<i2").tobytes())

def main():
    files = sorted(glob.glob(os.path.join(SRC, "*.*")))
    if not files: print("nothing in assets/sfx/"); return
    for f in files:
        name = os.path.splitext(os.path.basename(f))[0]
        out = os.path.join(DST, name + ".wav")
        if os.path.exists(out) and os.path.getmtime(out) >= os.path.getmtime(f):
            print(f"  {name}: up to date"); continue
        stereo = name.startswith("sting_")
        x = decode(f, 2 if stereo else 1)
        if name in LOOPS:
            x = make_loop(x)
        else:
            x = trim(x)
        peak = np.abs(x).max()
        if peak > 0: x *= 0.9 / peak
        write(out, x)
        print(f"  {name}: {len(x) / SR:.2f} s, {'stereo' if stereo else 'mono'}{', loop' if name in LOOPS else ''}, {os.path.getsize(out) // 1024} KB")

if __name__ == "__main__":
    main()
