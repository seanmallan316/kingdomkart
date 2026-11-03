#!/usr/bin/env python3
"""Splits one ElevenLabs take that holds several lines separated by pauses into one file per line.
usage: split_take.py take.mp3 name_event1 name_event2 ...   -> assets/voices/lines/<name_event>.mp3 for each
Fails loudly if the number of spoken parts found is not the number of names given."""
import sys, os, subprocess, re
import numpy as np
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DST = os.path.join(ROOT, "assets", "voices", "lines")
SR = 32000
def main():
    take, names = sys.argv[1], sys.argv[2:]
    raw = subprocess.run(["ffmpeg", "-nostdin", "-v", "error", "-i", take, "-ac", "1", "-ar", str(SR), "-f", "f32le", "-"], capture_output=True, check=True).stdout
    x = np.frombuffer(raw, np.float32)
    win = SR // 50                                   # 20 ms windows
    env = np.array([np.abs(x[i:i + win]).max() for i in range(0, len(x), win)])
    loud = env > 0.02
    # a gap of >= 1.5 s of quiet separates lines
    parts, i, n = [], 0, len(loud)
    while i < n:
        if not loud[i]: i += 1; continue
        j = i
        while j < n:
            k = j
            while k < n and not loud[k]: k += 1
            if k - j >= 75 or k >= n: break           # 75 windows = 1.5 s quiet (the breaks are 2 s)
            j = k + 1
            while j < n and loud[j]: j += 1
        parts.append((i, j)); i = j
    if len(parts) != len(names):
        print(f"found {len(parts)} parts, expected {len(names)}: {[(round(a*win/SR,2), round(b*win/SR,2)) for a,b in parts]}"); sys.exit(1)
    os.makedirs(DST, exist_ok=True)
    for (a, b), name in zip(parts, names):
        s, e = max(0, a * win / SR - 0.05), min(len(x) / SR, b * win / SR + 0.15)
        subprocess.run(["ffmpeg", "-nostdin", "-y", "-v", "error", "-i", take, "-ss", f"{s:.3f}", "-to", f"{e:.3f}", os.path.join(DST, name + ".mp3")], check=True)
        print(f"  {name}: {e - s:.2f} s")
if __name__ == "__main__": main()
