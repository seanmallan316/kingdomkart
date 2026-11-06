#!/usr/bin/env python3
"""Turns assets/video/NAME.mp4 into frames the game can play (content/video/NAME_000.jpg ...) plus NAME.txt
(frame count, frames per second, width, height). The sound track goes to assets/music/music_NAME.wav so
prep_music.py packs it. The Wii U has no video player we can use, so the game shows the frames itself."""
import os, subprocess, glob, shutil
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC, DST = os.path.join(ROOT, "assets", "video"), os.path.join(ROOT, "content", "video")
FPS, W = 24, 1280   # the full picture at the source frame rate; the game decodes frames ahead on two spare CPU cores
os.makedirs(DST, exist_ok=True)
for f in sorted(glob.glob(os.path.join(SRC, "*.mp4"))):
    name = os.path.splitext(os.path.basename(f))[0]
    info = os.path.join(DST, name + ".txt")
    if os.path.exists(info) and os.path.getmtime(info) >= os.path.getmtime(f): print(f"  {name}: up to date"); continue
    for old in glob.glob(os.path.join(DST, name + "_*.jpg")): os.remove(old)
    subprocess.run(["ffmpeg", "-nostdin", "-y", "-v", "error", "-i", f, "-vf", f"fps={FPS},scale={W}:-2", "-q:v", "2",
                    os.path.join(DST, name + "_%03d.jpg")], check=True)
    frames = sorted(glob.glob(os.path.join(DST, name + "_*.jpg")))
    for i, fr in enumerate(frames):   # ffmpeg counts from 1; the game from 0
        shutil.move(fr, os.path.join(DST, f"{name}_{i:03d}.jpg"))
    out = subprocess.run(["ffprobe", "-v", "error", "-select_streams", "v:0", "-show_entries", "stream=width,height", "-of", "csv=p=0",
                          os.path.join(DST, f"{name}_000.jpg")], capture_output=True, text=True).stdout.strip().split(",")
    with open(info, "w") as t: t.write(f"{len(frames)} {FPS} {out[0]} {out[1]}\n")
    subprocess.run(["ffmpeg", "-nostdin", "-y", "-v", "error", "-i", f, "-vn", "-ac", "2", os.path.join(ROOT, "assets", "music", f"music_{name}.wav")], check=True)
    total = sum(os.path.getsize(x) for x in glob.glob(os.path.join(DST, name + "_*.jpg")))
    print(f"  {name}: {len(frames)} frames at {FPS} fps, {out[0]}x{out[1]}, {total // 1024} KB")
