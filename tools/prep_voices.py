#!/usr/bin/env python3
"""Turns the recorded shouts in assets/voices/ into the fallback voice set: content/sfx/voice_<event>.wav.

The recordings are named <gender>_<event>.wav (male_win.wav = "Hallelujah", male_start.wav = "let's do this", ...).
They are used exactly as recorded (no pitch shifting), trimmed and evened out, 32 kHz mono. The game plays
voice_<event>.wav for any character who has no spoken line of their own (content/sfx/voice_<name>_<event>.wav,
made by tools/prep_lines.py from assets/voices/lines/).
Run  python3 tools/prep_voices.py  after adding or changing a recording.
"""
import os, subprocess
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "assets", "voices"); DST = os.path.join(ROOT, "content", "sfx")
# which recording is the fallback for each event
EVENTS = {"start": "male_start", "boost": "male_yahoo", "trick": "male_trick", "win": "male_win", "bless": "male_bless", "fire": "male_fire"}

def run(args): subprocess.run(["ffmpeg", "-nostdin", "-y", "-v", "error"] + args, check=True)
def main():
    os.makedirs(DST, exist_ok=True)
    yahoo = os.path.join(SRC, "male_yahoo.wav")
    if not os.path.exists(yahoo) and os.path.exists(os.path.join(SRC, "male_yahoo_raw.wav")):   # the first shout of the ya-hoo recording
        run(["-i", os.path.join(SRC, "male_yahoo_raw.wav"), "-ss", "0.02", "-to", "1.52", yahoo])
    n = 0
    for ev, rec in EVENTS.items():
        src = os.path.join(SRC, rec + ".wav")
        if not os.path.exists(src): continue
        out = os.path.join(DST, f"voice_{ev}.wav")
        if os.path.exists(out) and os.path.getmtime(out) >= os.path.getmtime(src) and os.path.getmtime(out) >= os.path.getmtime(__file__): continue
        trim = "silenceremove=start_periods=1:start_threshold=-40dB:start_silence=0.03,areverse,silenceremove=start_periods=1:start_threshold=-40dB:start_silence=0.08,areverse"
        run(["-i", src, "-af", f"{trim},loudnorm=I=-14:TP=-1,afade=t=out:st=9:d=0.05", "-ac", "1", "-ar", "32000", "-c:a", "pcm_s16le", out])
        n += 1
    print(f"voices: {n} fallback files made")
if __name__ == "__main__": main()
