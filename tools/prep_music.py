#!/usr/bin/env python3
"""Turns the soundtrack files Sean sends into what the game plays.

Put the originals in assets/music/ named by what they are for:
    music_menu    title screen and menus          music_tomb     Sunrise at the Empty Tomb
    music_beth    Bethlehem Road                  music_heaven   Road to Heaven
    music_gal     Sea of Galilee                  music_elah     David vs Goliath
    music_jeru    Jerusalem Road                  music_results  results screen
    music_finallap (optional) plays on the last lap of every road
Any audio format works (mp3, wav, m4a, ogg...). Run  python3 tools/prep_music.py  and each becomes
content/music/NAME.mp3: silence trimmed off both ends, loudness evened out, 32 kHz stereo 128 kbps, which is
what the Wii U sound core runs at, so the game never has to resample. Files already up to date are skipped.
"""
import os, subprocess, sys, glob

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "assets", "music")
DST = os.path.join(ROOT, "content", "music")
os.makedirs(DST, exist_ok=True)

def convert(src, dst):
    af = ("silenceremove=start_periods=1:start_threshold=-45dB:start_silence=0.05,"
          "areverse,silenceremove=start_periods=1:start_threshold=-45dB:start_silence=0.05,areverse,"
          "loudnorm=I=-15:TP=-1.5:LRA=11")
    cmd = ["ffmpeg", "-y", "-v", "error", "-i", src, "-map", "0:a:0", "-vn", "-af", af, "-ar", "32000", "-ac", "2",
           "-codec:a", "libmp3lame", "-b:a", "128k", "-map_metadata", "-1", "-id3v2_version", "0", "-write_xing", "0", dst]
    subprocess.run(cmd, check=True)

def main():
    files = sorted(glob.glob(os.path.join(SRC, "music_*.*")))
    if not files:
        print("nothing in assets/music/"); return
    for f in files:
        name = os.path.splitext(os.path.basename(f))[0]
        out = os.path.join(DST, name + ".mp3")
        if os.path.exists(out) and os.path.getmtime(out) >= os.path.getmtime(f):
            print(f"  {name}: up to date"); continue
        convert(f, out)
        dur = subprocess.run(["ffprobe", "-v", "error", "-show_entries", "format=duration", "-of", "csv=p=0", out],
                             capture_output=True, text=True).stdout.strip()
        print(f"  {name}: {os.path.getsize(out) // 1024} KB, {float(dur):.0f} s")

if __name__ == "__main__":
    main()
