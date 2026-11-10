#!/usr/bin/env python3
"""Finds where each racer's mouth and eyes are on their 3D head and writes content/models/<name>.face, so the game can
open the mouth with their voice and blink the eyes. The spots were picked by eye on a straight-on picture of each head
(320 x 320, see SPOTS: mouth x y width, left eye x y, right eye x y); this script shoots a ray at the head from the front
through each spot to find the point on the face and which way the face points there, and reads the skin colour.
   python3 tools/model3d/face_spots.py <folder with the head pictures and info.json>"""
import json, struct, sys, numpy as np
from PIL import Image
SPOTS = {   # mouth (x, y, width), left eye (x, y), right eye (x, y) - on the 320 x 320 straight-on picture of the head
 'jesus': (160, 178, 40, 128, 128, 190, 128), 'moses': (185, 150, 34, 148, 95, 200, 92), 'david': (175, 150, 36, 145, 108, 202, 103),
 'esther': (163, 240, 34, 120, 190, 200, 190), 'peter': (195, 142, 36, 163, 100, 225, 90), 'mary': (173, 117, 24, 150, 73, 198, 70),
 'daniel': (210, 168, 36, 170, 120, 240, 120), 'samson': (140, 130, 32, 123, 85, 175, 78), 'noah': (185, 160, 36, 145, 105, 210, 105),
 'joseph': (190, 208, 44, 135, 135, 225, 130), 'jonah': (160, 175, 38, 140, 130, 205, 130), 'john': (183, 150, 36, 147, 112, 210, 112),
 'michael': (162, 138, 34, 135, 100, 190, 98), 'gabriel': (180, 182, 40, 145, 128, 210, 125), 'goliath': (160, 225, 44, 125, 175, 185, 175),
 'gideon': (160, 207, 40, 128, 150, 190, 150), 'zacchaeus': (160, 205, 50, 120, 150, 200, 150), 'jacob': (165, 195, 40, 135, 135, 205, 135),
 'isaac': (165, 180, 40, 130, 130, 200, 130), 'abraham': (160, 172, 26, 138, 135, 185, 135), 'pharaoh': (160, 180, 30, 135, 125, 185, 125),
 'nebuchadnezzar': (160, 165, 36, 135, 105, 190, 105)}
def head_tris(name):
    d = open('content/models/%s.kkm' % name, 'rb').read(); n = struct.unpack('>I', d[4:8])[0]; o = 8
    for _ in range(n):
        kind = struct.unpack('>I', d[o:o + 4])[0]; cnt = struct.unpack('>I', d[o + 20:o + 24])[0]; o += 24
        if kind == 6: v = np.frombuffer(d[o:o + 48 * cnt], dtype='>f4').reshape(-1, 12).astype(np.float64); return v[:, :3].reshape(-1, 3, 3)
        o += 48 * cnt
def shoot(T, x, y, o=None, dvec=None):   # the nearest point of the head along a ray (default: from in front, down the -z axis, through (x, y))
    a, b, c = T[:, 0], T[:, 1], T[:, 2]; e1 = b - a; e2 = c - a
    if dvec is None: dvec = np.array([0, 0, -1.0]); o = np.array([x, y, 100.0])
    p = np.cross(dvec, e2); det = (e1 * p).sum(1); ok = np.abs(det) > 1e-12; inv = np.where(ok, 1 / np.where(ok, det, 1), 0)
    s = o - a; u = (s * p).sum(1) * inv; q = np.cross(s, e1); v = (dvec * q).sum(1) * inv; t = (e2 * q).sum(1) * inv
    hit = ok & (u >= 0) & (v >= 0) & (u + v <= 1) & (t > 0)
    if not hit.any(): return None
    i = np.where(hit)[0][np.argmin(t[hit])]; n = np.cross(e1[i], e2[i]); n /= np.linalg.norm(n)
    if n[2] < 0: n = -n
    return o + dvec * t[i], n
def spot(T, x, y, r):   # the point under (x, y), and the direction the face points there: a plane fitted through hits around it
    c = shoot(T, x, y)
    if c is None: return None
    pts = [c[0]]
    for dx in (-r, 0, r):
        for dy in (-r, 0, r):
            h = shoot(T, x + dx, y + dy)
            if h is not None and abs(h[0][2] - c[0][2]) < r * 3: pts.append(h[0])
    P = np.array(pts); P0 = P.mean(0); n = np.linalg.svd(P - P0)[2][2]
    if n[2] < 0: n = -n
    n = n * 0.7 + np.array([0, 0, 1.0]) * 0.3; n /= np.linalg.norm(n)   # (lean toward straight ahead: a face is roughly flat-on)
    return c[0], n
folder = sys.argv[1]; info = json.load(open(folder + '/info.json'))
for name, sp in SPOTS.items():
    T = head_tris(name); inf = info[name]; size = inf['size']; img = np.asarray(Image.open('%s/%s.png' % (folder, name)).convert('RGB')).astype(float) / 255
    def to3(px, py): return inf['cx'] + (px / 320 - 0.5) * size, inf['cy'] + (0.5 - py / 320) * size
    def avg(px, py, r=5): y0, x0 = int(py), int(px); return img[max(0, y0 - r):y0 + r, max(0, x0 - r):x0 + r].reshape(-1, 3).mean(0)
    out = []
    def patch(kind, c, n, W, H, nx, ny):   # a little sheet lying on the face (for the mouth or an eyelid picture), following its curve
        r = np.cross([0, 1.0, 0], n); r /= np.linalg.norm(r); u = np.cross(n, r); lines = ['%s %d %d' % (kind, nx, ny)]
        for j in range(ny):
            for i in range(nx):
                fu, fv = i / (nx - 1), j / (ny - 1); q = c + r * (fu - 0.5) * W + u * (0.5 - fv) * H
                h = shoot(T, 0, 0, q + n * W, -n); pt = h[0] if h is not None and np.linalg.norm(h[0] - q) < W else q
                pt = pt + n * W * 0.035
                lines.append('%.5f %.5f %.5f %.3f %.3f %.3f %.4f %.4f' % (*pt, *n, fu, fv))
        return lines
    mx, my = to3(sp[0], sp[1]); m = spot(T, mx, my, sp[2] / 320 * size * 0.5)
    if m is None: print('no mouth hit for', name); continue
    out.append('mouth %.4f %.4f %.4f %.3f %.3f %.3f %.4f' % (*m[0], *m[1], sp[2] / 320 * size)); mw = sp[2] / 320 * size
    out += patch('mouthpatch', m[0], m[1], mw * 1.5, mw * 1.1, 9, 7)
    for k in range(2):
        ex, ey = to3(sp[3 + k * 2], sp[4 + k * 2]); e = spot(T, ex, ey, sp[2] / 320 * size * 0.35)
        if e is None: print('no eye hit for', name); continue
        es = (sp[2] * 0.75 if sp[2] > 30 else 26) / 320 * size
        out.append('eye %.4f %.4f %.4f %.3f %.3f %.3f %.4f' % (*e[0], *e[1], es)); out += patch('eyepatch', e[0], e[1], es * 1.5, es * 1.5, 7, 7)
    cheek = avg(sp[3], sp[4] + 26, 4) * 0.5 + avg(sp[5], sp[6] + 26, 4) * 0.5   # the skin just under the eyes: the eyelids' colour
    out.append('skin %.3f %.3f %.3f' % tuple(cheek))
    open('content/models/%s.face' % name, 'w').write('\n'.join(out) + '\n'); print(name, out[0], out[-1])

# the pictures the sheets show: content/img/face_mouth.png (8 openings across; top row wide, bottom row round) and
# content/img/face_lid.png (8 steps of an eyelid closing; white, tinted to each racer's skin in the game)
from PIL import ImageDraw
def mouth_atlas():
    S = 4; cw, ch = 64 * S, 64 * S; im = Image.new('RGBA', (8 * cw, 2 * ch), (0, 0, 0, 0)); d = ImageDraw.Draw(im)
    for row in range(2):
        for k in range(1, 8):
            o = k / 7; x0 = k * cw + cw / 2; y0 = row * ch + ch * 0.5
            w = (0.42 + 0.22 * o) * cw if row == 0 else (0.2 + 0.16 * o) * cw; h = (0.1 + 0.5 * o) * ch if row == 0 else (0.16 + 0.44 * o) * ch
            d.ellipse([x0 - w / 2 - 3 * S, y0 - h / 2 - 3 * S, x0 + w / 2 + 3 * S, y0 + h / 2 + 3 * S], fill=(150, 62, 58, 255))   # the lips
            d.ellipse([x0 - w / 2, y0 - h / 2, x0 + w / 2, y0 + h / 2], fill=(52, 14, 18, 255))                                  # inside the mouth
            if o > 0.3: d.chord([x0 - w / 2 + 2 * S, y0 - h / 2 + 1 * S, x0 + w / 2 - 2 * S, y0 + h * 0.1], 180, 360, fill=(245, 240, 230, 255))   # the top teeth
            if o > 0.45: d.chord([x0 - w * 0.32, y0 + h * 0.05, x0 + w * 0.32, y0 + h / 2 - 1 * S], 0, 180, fill=(196, 84, 96, 255))           # the tongue
    im = im.resize((8 * 64, 2 * 64), Image.LANCZOS); im.save('content/img/face_mouth.png')
def lid_atlas():
    S = 4; cw = 64 * S; im = Image.new('RGBA', (8 * cw, cw), (0, 0, 0, 0))
    for k in range(1, 8):
        c = k / 7; cell = Image.new('RGBA', (cw, cw), (0, 0, 0, 0)); d = ImageDraw.Draw(cell)
        ew, eh = cw * 0.62, cw * 0.5; x0, y0 = cw / 2, cw / 2; top = y0 - eh / 2; edge = top + eh * c
        mask = Image.new('L', (cw, cw), 0); ImageDraw.Draw(mask).ellipse([x0 - ew / 2, top, x0 + ew / 2, y0 + eh / 2], fill=255)
        lid = Image.new('L', (cw, cw), 0); ImageDraw.Draw(lid).rectangle([0, 0, cw, edge], fill=255)
        ImageDraw.Draw(lid).ellipse([x0 - ew / 2, edge - eh * 0.18, x0 + ew / 2, edge + eh * 0.18], fill=255)   # a rounded lid edge
        a = Image.fromarray((np.asarray(mask, float) * np.asarray(lid, float) / 255).astype(np.uint8))
        cell.paste(Image.new('RGBA', (cw, cw), (255, 255, 255, 255)), (0, 0), a)
        dd = ImageDraw.Draw(cell)
        if c < 0.8: dd.arc([x0 - ew / 2, edge - eh * 0.18, x0 + ew / 2, edge + eh * 0.18], 0, 180, fill=(40, 26, 22, 255), width=3 * S)   # the lashes
        else: dd.arc([x0 - ew / 2, top, x0 + ew / 2, y0 + eh / 2 - 2 * S], 15, 165, fill=(40, 26, 22, 255), width=3 * S)            # shut: the lashes along the bottom
        cell.putalpha(Image.fromarray(np.minimum(np.asarray(cell.getchannel('A')), np.asarray(mask.resize((cw, cw))) ).astype(np.uint8)))
        im.paste(cell, (k * cw, 0))
    im = im.resize((8 * 64, 64), Image.LANCZOS); im.save('content/img/face_lid.png')
mouth_atlas(); lid_atlas(); print('made the mouth and eyelid pictures')
