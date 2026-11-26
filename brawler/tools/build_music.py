#!/usr/bin/env python3
"""Packs music/*.mp3 into musicjs.js (window.CLASH_MUSIC.<name>) so the songs play from file://."""
import base64, glob, os
here = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
parts = ["  %s: 'data:audio/mpeg;base64,%s'," % (os.path.splitext(os.path.basename(f))[0], base64.b64encode(open(f, 'rb').read()).decode()) for f in sorted(glob.glob(os.path.join(here, 'music', '*.mp3')))]
open(os.path.join(here, 'musicjs.js'), 'w').write("// generated from music/*.mp3 (the family's own songs, made with Suno) - do not edit\nwindow.CLASH_MUSIC = {\n" + "\n".join(parts) + "\n};\n")
