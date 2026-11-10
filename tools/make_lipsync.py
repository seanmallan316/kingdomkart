#!/usr/bin/env python3
"""Mouth movements for every voice line: content/sfx/<voice>.lip, one letter per 1/30 second -
'0' closed .. '7' wide open (an 'ah' or 'ee' mouth), 'a'..'h' the same openings with round lips (an 'oh' or 'oo').
Worked out from the recording itself: how loud it is (how open) and how bright it sounds (low and dark = round lips).
   python3 tools/make_lipsync.py"""
import glob, subprocess, numpy as np, os
FPS = 30; SR = 16000
for path in sorted(glob.glob('content/sfx/voice_*.wav')):
    raw = subprocess.run(['ffmpeg', '-v', 'error', '-i', path, '-ac', '1', '-ar', str(SR), '-f', 's16le', '-'], capture_output=True).stdout
    x = np.frombuffer(raw, dtype='<i2').astype(np.float64) / 32768
    if len(x) < SR // 10: continue
    hop = SR // FPS; win = int(SR * 0.04); n = len(x) // hop
    rms = np.zeros(n); cen = np.zeros(n)
    for i in range(n):
        seg = x[i * hop:i * hop + win]
        if len(seg) < 16: continue
        rms[i] = np.sqrt((seg ** 2).mean() + 1e-12)
        sp = np.abs(np.fft.rfft(seg * np.hanning(len(seg)))); f = np.fft.rfftfreq(len(seg), 1 / SR); band = (f > 80) & (f < 4000)
        cen[i] = (sp[band] * f[band]).sum() / (sp[band].sum() + 1e-9)
    db = 20 * np.log10(rms + 1e-9); top = np.percentile(db, 95)
    op = np.clip((db - (top - 32)) / 32, 0, 1) ** 1.4
    loc = np.array([db[max(0, i - 6):i + 7].max() for i in range(n)]); op *= np.clip(1 - (loc - db) / 14, 0.35, 1)   # dips between syllables close the mouth a little
    op = np.convolve(op, [0.25, 0.5, 0.25], mode='same')                       # no jitter
    op[op < 0.12] = 0                                                        # quiet: closed
    voiced = op > 0.2; cmed = np.median(cen[voiced]) if voiced.any() else 1000
    out = []
    for i in range(n):
        lvl = int(round(op[i] * 7)); rnd = voiced[i] and cen[i] < cmed * 0.8 and lvl > 0
        out.append(chr((ord('a') if rnd else ord('0')) + lvl))
    open(path[:-4] + '.lip', 'w').write(''.join(out)); print(os.path.basename(path), ''.join(out)[:70])
