#!/usr/bin/env python3
"""Turns the recorded shouts in assets/voices/ into one voice per character: content/sfx/voice_<name>_<event>.wav.

The recordings are named <gender>_<event>.wav (male_win.wav = "Hallelujah", female_start.wav = "here we go", ...).
Every character gets the set for their gender, pitched up or down a little so each one sounds like themselves
(Samson deepest, Joseph brightest); the length stays the same. 32 kHz mono, trimmed and evened out.
Run  python3 tools/prep_voices.py  after adding or changing a recording.
"""
import os, subprocess
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "assets", "voices"); DST = os.path.join(ROOT, "content", "sfx")
EVENTS = ["start", "boost", "trick", "win", "bless", "fire"]
# which recording plays for each event, per gender (a recording may serve two events)
MALE = {"start": "male_start", "boost": "male_yahoo", "trick": "male_trick", "win": "male_win", "bless": "male_bless", "fire": "male_fire"}
FEMALE = {"start": "female_start", "boost": "female_whoo", "trick": "female_whoo", "win": "male_win", "bless": "female_bless", "fire": "female_whoo"}
# semitones up (+) or down (-) for each character; the female win is "Hallelujah" lifted a fourth
CHARS = {"jesus": ("m", 0), "moses": ("m", -3), "david": ("m", 2), "esther": ("f", 0), "peter": ("m", -1.5), "mary": ("f", 1.5), "daniel": ("m", 1),
         "samson": ("m", -4), "noah": ("m", -2.5), "joseph": ("m", 3), "jonah": ("m", -0.5), "john": ("m", -2)}
EXTRA = {("f", "win"): 5}        # the male Hallelujah serves the women, lifted

def run(args): subprocess.run(["ffmpeg", "-nostdin", "-y", "-v", "error"] + args, check=True)
def main():
    os.makedirs(DST, exist_ok=True)
    yahoo = os.path.join(SRC, "male_yahoo.wav")
    if not os.path.exists(yahoo) and os.path.exists(os.path.join(SRC, "male_yahoo_raw.wav")):   # the first shout of the ya-hoo recording
        run(["-i", os.path.join(SRC, "male_yahoo_raw.wav"), "-ss", "0.02", "-to", "1.52", yahoo])
    n = 0
    for name, (g, semi) in CHARS.items():
        table = MALE if g == "m" else FEMALE
        for ev in EVENTS:
            src = os.path.join(SRC, table[ev] + ".wav")
            if not os.path.exists(src): continue
            shift = semi + EXTRA.get((g, ev), 0)
            out = os.path.join(DST, f"voice_{name}_{ev}.wav")
            if os.path.exists(out) and os.path.getmtime(out) >= os.path.getmtime(src) and os.path.getmtime(out) >= os.path.getmtime(__file__): continue
            trim = "silenceremove=start_periods=1:start_threshold=-40dB:start_silence=0.03,areverse,silenceremove=start_periods=1:start_threshold=-40dB:start_silence=0.08,areverse"
            pitch = f"rubberband=pitch={2 ** (shift / 12.0):.4f}:pitchq=quality," if shift else ""
            run(["-i", src, "-af", f"{trim},{pitch}loudnorm=I=-14:TP=-1,afade=t=out:st=9:d=0.05", "-ac", "1", "-ar", "32000", "-c:a", "pcm_s16le", out])
            n += 1
    print(f"voices: {n} files made, {len(os.listdir(DST))} in content/sfx")
if __name__ == "__main__": main()
