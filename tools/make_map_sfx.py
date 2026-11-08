#!/usr/bin/env python3
"""Makes the Bible Journeys map sounds (content/sfx/map_*.wav, 32 kHz mono) from scratch: paper unfolding, the land
rising, pins popping up, picking a stop, the toy kart driving off, a padlock opening, the journey line reaching a stop.
   python3 tools/make_map_sfx.py"""
import numpy as np, wave
SR = 32000; rng = np.random.default_rng(7)
def t_(d): return np.arange(int(SR * d)) / SR
def env(n, a, d, sustain=0.0):   # attack / decay in seconds
    e = np.ones(n); na = max(1, int(a * SR)); e[:na] = np.linspace(0, 1, na)
    k = np.arange(n - na) / SR; e[na:] = sustain + (1 - sustain) * np.exp(-k / max(d, 1e-3)); return e
def lp(x, a):   # one-pole low-pass (a: 0..1, smaller = darker)
    y = np.zeros_like(x); s = 0.0
    for i, v in enumerate(x): s += a * (v - s); y[i] = s
    return y
def hp(x, a): return x - lp(x, a)
def bp(x, lo, hi): return lp(hp(x, lo), hi)
def save(name, x, gain=0.85):
    x = x / (np.max(np.abs(x)) + 1e-9) * gain; x = np.clip(x, -1, 1)
    with wave.open('content/sfx/%s.wav' % name, 'wb') as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR); w.writeframes((x * 32767).astype('<i2').tobytes())
def paper(d, density, seed):   # crinkly paper: lots of tiny filtered noise crackles with a soft swish under them
    r = np.random.default_rng(seed); n = int(SR * d); out = np.zeros(n)
    sw = bp(r.standard_normal(n), 0.08, 0.5) * 0.25 * np.sin(np.linspace(0, np.pi, n)) ** 1.5
    for _ in range(int(density * d)):
        at = int(r.uniform(0, n - 600)); L = int(r.uniform(80, 500)); g = r.uniform(0.2, 1.0) * np.sin(np.pi * at / n) ** 0.7
        c = hp(r.standard_normal(L), r.uniform(0.15, 0.5)) * np.exp(-np.arange(L) / (L * 0.25)) * g; out[at:at + L] += c
    return out + sw
# 1. the map unfolding: a crinkly rustle and a soft flap as the panel lands
u = paper(0.9, 140, 1); n = len(u); flap = lp(rng.standard_normal(n), 0.06) * env(n, 0.002, 0.05); fl = np.zeros(n); k = int(0.68 * SR); fl[k:] = flap[:n - k] * 1.6
save('map_unfold', u + fl)
# 2. the land rising out of the paper: a deep warm swell with a shimmer climbing over it
d = 1.6; tt = t_(d); n = len(tt); sw = np.sin(np.linspace(0, np.pi, n)) ** 1.2
rum = lp(rng.standard_normal(n), 0.01) * 3.0 + 0.4 * np.sin(2 * np.pi * (45 + 25 * tt) * tt)
sh = sum(np.sin(2 * np.pi * f * (1 + 0.5 * tt) * tt) * (0.25 / (i + 1)) for i, f in enumerate([660, 880, 990, 1320]))
save('map_rise', (rum + sh * np.clip(tt / d, 0, 1)) * sw)
# 3. a pin popping up: a short wooden pop
tt = t_(0.14); n = len(tt); f = 520 * np.exp(-tt * 18) + 300
save('map_pin', np.sin(2 * np.pi * np.cumsum(f) / SR) * env(n, 0.001, 0.035) + 0.3 * hp(rng.standard_normal(n), 0.4) * env(n, 0.0005, 0.006), 0.6)
# 4. picking a stop: a bright wooden tick with a tiny bell
tt = t_(0.35); n = len(tt)
tick = hp(rng.standard_normal(n), 0.3) * env(n, 0.0005, 0.004)
bell = (np.sin(2 * np.pi * 1568 * tt) + 0.5 * np.sin(2 * np.pi * 2352 * tt) + 0.25 * np.sin(2 * np.pi * 3136 * tt)) * env(n, 0.002, 0.09) * 0.35
save('map_select', tick + bell, 0.6)
# 5. the toy kart driving off: a little putt-putt engine that revs up and pulls away
d = 1.1; tt = t_(d); n = len(tt); rate = 14 + 22 * np.clip(tt / 0.5, 0, 1); ph = np.cumsum(rate) / SR
putt = (np.sin(2 * np.pi * ph) > 0.6).astype(float); putt = lp(putt * (0.6 + 0.4 * rng.standard_normal(n)), 0.25)
tone = np.sin(2 * np.pi * np.cumsum(70 + 90 * np.clip(tt / 0.6, 0, 1)) / SR) * 0.5
fade = np.clip(1.3 - tt / d * 1.3, 0, 1) * np.clip(tt / 0.03, 0, 1)
save('map_drive', (putt + tone) * fade, 0.55)
# 6. a stop opening: a padlock click, then a rising chime
tt = t_(1.3); n = len(tt); out = np.zeros(n)
for at, g in [(0.0, 1.0), (0.07, 0.7)]:
    k = int(at * SR); L = 400; out[k:k + L] += hp(rng.standard_normal(L), 0.5) * np.exp(-np.arange(L) / 50) * g
for i, f in enumerate([784, 988, 1175, 1568]):
    k = int((0.18 + i * 0.09) * SR); m = n - k; ts = np.arange(m) / SR
    out[k:] += (np.sin(2 * np.pi * f * ts) + 0.3 * np.sin(2 * np.pi * 2 * f * ts)) * np.exp(-ts / 0.35) * 0.3
save('map_unlock', out, 0.7)
# 7. the journey line reaching a stop: a soft magic chime with a shimmer
tt = t_(1.4); n = len(tt); out = np.zeros(n)
for i, f in enumerate([1047, 1319, 1568, 2093]): out += np.sin(2 * np.pi * f * tt + i) * np.exp(-tt / (0.5 + 0.1 * i)) * (0.4 / (1 + i * 0.4))
glit = hp(rng.standard_normal(n), 0.6) * np.exp(-tt / 0.25) * 0.08 * (0.5 + 0.5 * np.sin(2 * np.pi * 23 * tt))
save('map_stop', (out + glit) * np.clip(tt / 0.005, 0, 1), 0.6)
# 8. the line drawing itself between stops: a quiet quill-on-parchment scratch (loops while it draws)
save('map_draw', paper(0.8, 60, 9) * 0.6 + 0.4 * bp(rng.standard_normal(int(0.8 * SR)), 0.2, 0.6) * (0.6 + 0.4 * np.sin(2 * np.pi * 6 * t_(0.8))), 0.35)
print('made the map sounds')
