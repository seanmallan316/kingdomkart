#!/usr/bin/env python3
"""Turns the spoken lines in assets/voices/lines/ (<character>_<event>.mp3 or .wav, one line each, made with ElevenLabs)
into what the game plays: content/sfx/voice_<character>_<event>.wav, 32 kHz mono, trimmed and evened out.

Events: start boost trick win ouch bless fire.  A line here replaces the pitched shared shout tools/prep_voices.py
would otherwise make for that character and event, so run this AFTER prep_voices.py.
Files already up to date are skipped.
"""
import os, subprocess, glob
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "assets", "voices", "lines"); DST = os.path.join(ROOT, "content", "sfx")

def run(args): subprocess.run(["ffmpeg", "-nostdin", "-y", "-v", "error"] + args, check=True)
def main():
    os.makedirs(DST, exist_ok=True)
    files = sorted(glob.glob(os.path.join(SRC, "*.mp3")) + glob.glob(os.path.join(SRC, "*.wav")))
    n = 0
    for f in files:
        name = os.path.splitext(os.path.basename(f))[0]
        out = os.path.join(DST, f"voice_{name}.wav")
        if os.path.exists(out) and os.path.getmtime(out) >= os.path.getmtime(f) and os.path.getmtime(out) >= os.path.getmtime(__file__): continue
        trim = "silenceremove=start_periods=1:start_threshold=-45dB:start_silence=0.03,areverse,silenceremove=start_periods=1:start_threshold=-45dB:start_silence=0.12,areverse"
        run(["-i", f, "-af", f"{trim},loudnorm=I=-15:TP=-1,afade=t=out:st=9:d=0.05", "-ac", "1", "-ar", "32000", "-c:a", "pcm_s16le", out])
        n += 1
    print(f"lines: {n} files made from {len(files)} recordings")
if __name__ == "__main__": main()
