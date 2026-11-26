#!/usr/bin/env python3
"""Builds one self-contained playable file: every script (and any supplied logo / intro / title video) is inlined into a
single HTML page. Usage: python3 tools/build_single.py [output.html]   (default: SuperFamilySmash90s.html next to index.html)
The game is Super Family Smash (90's Version): same name, logo, intro and title video as the original."""
import base64, os, re, sys
OUT_NAME = 'SuperFamilySmash90s.html'
here = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(here, OUT_NAME)
html = open(os.path.join(here, 'index.html'), encoding='utf-8').read()
def data(name, mime):
    return 'data:%s;base64,%s' % (mime, base64.b64encode(open(os.path.join(here, name), 'rb').read()).decode())
# optional supplied media: inlined only if the file exists and the page references it
for name, mime in (('logo.webp', 'image/webp'), ('intro.webm', 'video/webm'), ('titlebg.mp4', 'video/mp4'), ('titlebg.webm', 'video/webm')):
    if os.path.exists(os.path.join(here, name)) and ('src="%s"' % name) in html:
        html = html.replace('src="%s"' % name, 'src="%s"' % data(name, mime))
def inline(m):
    js = open(os.path.join(here, m.group(1)), encoding='utf-8').read().replace('</script', '<\\/script')
    return '<script>\n%s\n</script>' % js
html = re.sub(r'<script src="([^"]+)"></script>', inline, html)
open(out, 'w', encoding='utf-8').write(html)
print(out, round(os.path.getsize(out) / 1e6, 1), 'MB')
