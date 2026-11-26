'use strict';
// Super Family Smash effects pack: original visual effects (drawn in code) and original sound (synthesized with Web Audio).
// Loaded after game.js; it replaces the simple effect and sound functions defined there.

// ================================================================ SOUND
(() => {
  let ctx = null, out = null, verb = null, noiseBuf = null, lastT = {};
  const rnd = (a, b) => a + Math.random() * (b - a);
  function init() {
    if (!ctx) {
      try { ctx = new (window.AudioContext || window.webkitAudioContext)(); } catch (e) { return; }
      const comp = ctx.createDynamicsCompressor(); comp.threshold.value = -14; comp.ratio.value = 6; comp.attack.value = 0.002; comp.release.value = 0.18;
      const master = ctx.createGain(); master.gain.value = 0.85 * (Sfx.volume == null ? 1 : Sfx.volume); Sfx.master = master; comp.connect(master); master.connect(ctx.destination);
      out = ctx.createGain(); out.connect(comp);
      // small-room reverb from a generated impulse
      const len = Math.floor(ctx.sampleRate * 1.3), ir = ctx.createBuffer(2, len, ctx.sampleRate);
      for (let c = 0; c < 2; c++) { const d = ir.getChannelData(c); for (let i = 0; i < len; i++) d[i] = (Math.random() * 2 - 1) * Math.pow(1 - i / len, 3.2); }
      const cv = ctx.createConvolver(); cv.buffer = ir; verb = ctx.createGain(); verb.gain.value = 0.22; verb.connect(cv); cv.connect(comp);
      const n = ctx.sampleRate, b = ctx.createBuffer(1, n, n), d = b.getChannelData(0); for (let i = 0; i < n; i++) d[i] = Math.random() * 2 - 1; noiseBuf = b;
      Sfx.ctx = ctx;
    }
    if (ctx.state === 'suspended') ctx.resume();
  }
  const dest = wet => { const g = ctx.createGain(); g.connect(out); if (wet) { const s = ctx.createGain(); s.gain.value = wet; g.connect(s); s.connect(verb); } return g; };
  function tone(type, f0, f1, dur, vol, o = {}) {
    if (!ctx) return; const t = ctx.currentTime + (o.at || 0), osc = ctx.createOscillator(), g = ctx.createGain();
    osc.type = type; osc.frequency.setValueAtTime(f0, t); if (f1 && f1 !== f0) osc.frequency.exponentialRampToValueAtTime(Math.max(20, f1), t + dur * (o.bend || 1));
    g.gain.setValueAtTime(0.0001, t); g.gain.linearRampToValueAtTime(vol, t + (o.a || 0.003)); g.gain.exponentialRampToValueAtTime(0.0001, t + dur);
    let node = osc;
    if (o.drive) { const ws = ctx.createWaveShaper(), k = o.drive, c = new Float32Array(256); for (let i = 0; i < 256; i++) { const x = i / 128 - 1; c[i] = ((1 + k) * x) / (1 + k * Math.abs(x)); } ws.curve = c; osc.connect(ws); node = ws; }
    node.connect(g); g.connect(dest(o.wet || 0)); osc.start(t); osc.stop(t + dur + 0.03);
  }
  function noise(dur, vol, type, f0, f1, o = {}) {
    if (!ctx) return; const t = ctx.currentTime + (o.at || 0), s = ctx.createBufferSource(); s.buffer = noiseBuf; s.loop = true; s.playbackRate.value = o.rate || 1;
    const f = ctx.createBiquadFilter(); f.type = type; f.Q.value = o.q || 0.8; f.frequency.setValueAtTime(f0, t); if (f1 && f1 !== f0) f.frequency.exponentialRampToValueAtTime(Math.max(30, f1), t + dur);
    const g = ctx.createGain(); g.gain.setValueAtTime(0.0001, t); g.gain.linearRampToValueAtTime(vol, t + (o.a || 0.002)); g.gain.exponentialRampToValueAtTime(0.0001, t + dur);
    s.connect(f); f.connect(g); g.connect(dest(o.wet || 0)); s.start(t, Math.random() * 0.5); s.stop(t + dur + 0.03);
  }
  const gate = (k, ms) => { const n = performance.now(); if (lastT[k] && n - lastT[k] < ms) return false; lastT[k] = n; return true; };

  Object.assign(Sfx, {
    init,
    setVolume(v) { Sfx.volume = v; if (Sfx.master) Sfx.master.gain.value = 0.85 * v; },
    // impacts in three weights: a click, a body thump and a crack, with more low end as damage goes up
    hit(d) {
      if (!ctx || !gate('hit', 28)) return; const k = Math.min(d, 28) / 28, p = rnd(0.92, 1.08);
      noise(0.018, 0.5, 'highpass', 3500);                                              // contact click
      tone('sine', (210 - k * 70) * p, 48, 0.09 + k * 0.16, 0.55 + k * 0.3, { drive: 2 + k * 6 }); // thump
      noise(0.06 + k * 0.12, 0.3 + k * 0.35, 'bandpass', (1900 - k * 900) * p, 500, { q: 0.9, wet: 0.2 + k * 0.3 }); // crack
      if (k > 0.45) { tone('triangle', 95 * p, 34, 0.26 + k * 0.2, 0.5, { drive: 5, wet: 0.3 }); noise(0.22, 0.2, 'lowpass', 900, 120, { wet: 0.4 }); }
    },
    // finishing blow: sharp crack, long sub drop, ringing tail
    bigHit() {
      if (!ctx) return;
      noise(0.03, 0.9, 'highpass', 2500); noise(0.5, 0.55, 'bandpass', 2600, 240, { q: 0.6, wet: 0.6 });
      tone('sine', 160, 26, 0.75, 0.95, { drive: 8, wet: 0.3 }); tone('sawtooth', 320, 60, 0.35, 0.3, { drive: 10, wet: 0.4 });
      tone('sine', 2400, 1900, 0.5, 0.1, { at: 0.02, wet: 0.7 }); tone('sine', 3600, 3000, 0.4, 0.06, { at: 0.03, wet: 0.7 });
    },
    swing(power = 0.4) { if (!ctx || !gate('sw', 40)) return; const p = rnd(0.9, 1.15); noise(0.1 + power * 0.1, 0.1 + power * 0.14, 'bandpass', 900 * p, (2800 + power * 1800) * p, { q: 1.6, a: 0.03 }); },
    blade() { if (!ctx || !gate('bl', 40)) return; const p = rnd(0.95, 1.1); noise(0.14, 0.18, 'bandpass', 2400 * p, 6500 * p, { q: 2.5, a: 0.02, wet: 0.2 }); tone('sine', 3200 * p, 4800 * p, 0.12, 0.03, { wet: 0.4 }); },
    jump() { if (!ctx) return; tone('sine', 280, 620, 0.11, 0.16); noise(0.09, 0.07, 'bandpass', 800, 2400, { q: 1 }); },
    djump() { if (!ctx) return; tone('sine', 480, 1100, 0.13, 0.15, { wet: 0.2 }); tone('triangle', 720, 1500, 0.1, 0.06, { at: 0.03 }); noise(0.12, 0.08, 'bandpass', 1500, 4200, { q: 1.2 }); },
    land(h = 0.3) { if (!ctx || !gate('ld', 50)) return; tone('sine', 120, 55, 0.08 + h * 0.08, 0.25 + h * 0.3); noise(0.07, 0.12 + h * 0.15, 'lowpass', 700, 200); },
    step() { if (!ctx) return; noise(0.035, 0.07, 'bandpass', rnd(500, 900), 300, { q: 1.2 }); },
    dash() { if (!ctx) return; noise(0.16, 0.16, 'bandpass', 500, 2600, { q: 0.9, a: 0.02 }); },
    shield() { if (!ctx) return; tone('sine', 520, 780, 0.12, 0.12, { wet: 0.3 }); tone('sine', 1040, 1560, 0.1, 0.04, { wet: 0.3 }); },
    shieldHit(d = 5) { if (!ctx) return; tone('triangle', 620, 300, 0.12, 0.3, { wet: 0.3 }); noise(0.07, 0.22, 'bandpass', 1400, 600, { q: 2 }); },
    parry() { if (!ctx) return; [1568, 2093, 2637, 3136].forEach((f, i) => tone('sine', f, f * 1.01, 0.5 - i * 0.05, 0.14 - i * 0.02, { at: i * 0.012, wet: 0.8 })); noise(0.03, 0.4, 'highpass', 5000); },
    shieldBreak() { if (!ctx) return; for (let i = 0; i < 14; i++) tone('sine', rnd(1800, 6000), rnd(900, 3000), rnd(0.08, 0.3), 0.07, { at: i * 0.018 + rnd(0, 0.02), wet: 0.6 }); noise(0.5, 0.5, 'highpass', 2200, 800, { wet: 0.5 }); tone('sawtooth', 500, 60, 0.6, 0.3, { drive: 6 }); },
    zap() { if (!ctx || !gate('zp', 40)) return; tone('sawtooth', 1500, 240, 0.14, 0.12, { drive: 3 }); tone('square', 2200, 500, 0.09, 0.05); noise(0.06, 0.1, 'highpass', 4000); },
    charge(k) { if (!ctx || !gate('ch', 70)) return; tone('sawtooth', 180 + k * 520, 200 + k * 560, 0.09, 0.05 + k * 0.05, { drive: 2 }); },
    boom() { if (!ctx || !gate('bm', 60)) return; noise(0.9, 0.7, 'lowpass', 1400, 70, { wet: 0.6 }); tone('sine', 110, 24, 0.9, 0.9, { drive: 6 }); noise(0.05, 0.6, 'highpass', 1800); },
    // ring-out: blast, rising whistle, glitter
    ko() {
      if (!ctx) return;
      noise(1.3, 0.8, 'lowpass', 2400, 60, { wet: 0.8 }); tone('sine', 140, 22, 1.2, 1.0, { drive: 9, wet: 0.3 }); noise(0.05, 0.9, 'highpass', 1500);
      tone('sawtooth', 300, 2600, 0.5, 0.14, { a: 0.1, wet: 0.6 }); noise(0.6, 0.25, 'bandpass', 700, 6000, { q: 2, a: 0.15, wet: 0.5 });
      for (let i = 0; i < 9; i++) tone('sine', rnd(2000, 5200), rnd(1500, 4000), 0.25, 0.05, { at: 0.25 + i * 0.045, wet: 0.9 });
    },
    grab() { if (!ctx) return; tone('square', 240, 150, 0.07, 0.14); noise(0.05, 0.18, 'bandpass', 900, 400, { q: 1.5 }); },
    throw_() { if (!ctx) return; noise(0.2, 0.2, 'bandpass', 600, 3200, { q: 1, a: 0.04 }); },
    blip() { if (!ctx) return; tone('square', 880, 880, 0.045, 0.07); tone('sine', 1320, 1320, 0.06, 0.05, { at: 0.02 }); },
    count() { if (!ctx) return; tone('square', 392, 392, 0.14, 0.14, { wet: 0.3 }); tone('sine', 784, 784, 0.18, 0.12, { wet: 0.3 }); noise(0.03, 0.2, 'highpass', 3000); },
    go() { if (!ctx) return; [523, 659, 784, 1047].forEach((f, i) => tone(i % 2 ? 'square' : 'sawtooth', f, f, 0.5, 0.11, { at: 0.0, wet: 0.5 })); tone('sine', 130, 60, 0.4, 0.5, { drive: 4 }); noise(0.3, 0.3, 'highpass', 2000, 6000, { wet: 0.5 }); },
    game() { if (!ctx) return; [392, 494, 587, 784].forEach((f, i) => tone('sawtooth', f, f, 1.1, 0.12, { at: i * 0.05, wet: 0.7 })); tone('sine', 98, 49, 1.0, 0.7, { drive: 5 }); noise(0.8, 0.4, 'lowpass', 3000, 100, { wet: 0.7 }); },
    od() { if (!ctx) return; tone('sawtooth', 110, 880, 0.7, 0.25, { drive: 6, wet: 0.5, a: 0.05 }); noise(0.8, 0.3, 'bandpass', 500, 7000, { q: 1.2, a: 0.2, wet: 0.6 }); [880, 1320, 1760].forEach((f, i) => tone('sine', f, f, 0.6, 0.08, { at: 0.55 + i * 0.03, wet: 0.9 })); },
    tech() { if (!ctx) return; tone('sine', 900, 1400, 0.08, 0.15); noise(0.05, 0.15, 'highpass', 3000); },
    tone(f0, dur, type = 'square', vol = 0.12, f1 = null) { tone(type, f0, f1 || f0, dur, vol); },
    noise(dur, vol, fc = 1200, q = 0.8, fc1 = null) { noise(dur, vol, 'bandpass', fc, fc1 || fc, { q }); },
  });

  // ---- recorded-quality samples (generated to order; see sfx/). Each sound falls back to the synth above
  // until its sample has decoded, or if decoding is not available.
  const bufs = {}, synth = Object.assign({}, Sfx);
  let loading = false;
  function loadSamples() {
    const src = window.CLASH_SFX; if (!src || loading || !ctx) return; loading = true;
    for (const name in src) {
      try {
        const str = atob(src[name]), u8 = new Uint8Array(str.length); for (let i = 0; i < str.length; i++) u8[i] = str.charCodeAt(i);
        const done = b => { bufs[name] = b; Sfx.samplesReady = Object.keys(bufs).length; };
        const p = ctx.decodeAudioData(u8.buffer, done, () => {}); if (p && p.catch) p.catch(() => {});
      } catch (e) { /* keep the synth for this one */ }
    }
  }
  function play(name, vol = 1, rate = 1, wet = 0.12) {
    const b = bufs[name]; if (!b || !ctx) return false;
    const s = ctx.createBufferSource(); s.buffer = b; s.playbackRate.value = rate;
    const g = ctx.createGain(); g.gain.value = vol; s.connect(g); g.connect(out);
    if (wet) { const w = ctx.createGain(); w.gain.value = wet; g.connect(w); w.connect(verb); }
    s.start(); return true;
  }
  const vary = a => 1 + (Math.random() * 2 - 1) * a;
  // one(name, sample, volume, pitch variation, reverb, minimum gap in ms)
  const one = (key, sample, vol, v = 0.05, wet = 0.12, gap = 0) => function () {
    if (!bufs[sample]) return synth[key].apply(Sfx, arguments);
    if (gap && !gate('s:' + key, gap)) return;
    play(sample, vol, vary(v), wet);
  };
  Object.assign(Sfx, {
    samplesReady: 0,
    init() { init(); loadSamples(); },
    hit(d) {
      const k = Math.min(d, 28) / 28, name = k < 0.3 ? 'hit1' : k < 0.62 ? 'hit2' : 'hit3';
      if (!bufs[name]) return synth.hit(d);
      if (!gate('s:hit', 28)) return;
      play(name, 0.5 + k * 0.5, vary(0.07), 0.1 + k * 0.2);
    },
    bigHit: one('bigHit', 'finisher', 1.0, 0.02, 0.3),
    swing(power = 0.4) { const n = power > 0.6 ? 'swingHeavy' : 'swing'; if (!bufs[n]) return synth.swing(power); if (!gate('s:sw', 60)) return; play(n, 0.3 + power * 0.25, vary(0.1), 0.05); },
    blade: one('blade', 'blade', 0.42, 0.1, 0.1, 60),
    jump: one('jump', 'jump', 0.4, 0.06, 0.05),
    djump: one('djump', 'djump', 0.45, 0.06, 0.15),
    land(h = 0.3) { if (!bufs.land) return synth.land(h); if (!gate('s:ld', 70)) return; play('land', 0.28 + h * 0.4, vary(0.08), 0.05); },
    step: one('step', 'step', 0.2, 0.14, 0, 90),
    dash: one('dash', 'dash', 0.38, 0.06, 0.05, 120),
    shield: one('shield', 'shield', 0.42, 0.04, 0.15, 120),
    shieldHit: one('shieldHit', 'shieldHit', 0.6, 0.06, 0.15, 60),
    parry: one('parry', 'parry', 0.85, 0.02, 0.35),
    shieldBreak: one('shieldBreak', 'shieldBreak', 0.9, 0.02, 0.3),
    zap: one('zap', 'zap', 0.42, 0.08, 0.1, 60),
    charge(k) { if (!bufs.charge) return synth.charge(k); if (!gate('s:ch', 950)) return; play('charge', 0.4, 0.9 + k * 0.3, 0.1); },
    boom() { if (!bufs.hit3) return synth.boom(); if (!gate('s:bm', 80)) return; play('hit3', 0.85, 0.72, 0.3); },
    ko: one('ko', 'ko', 1.0, 0.02, 0.3),
    grab: one('grab', 'grab', 0.5, 0.06, 0.05),
    throw_: one('throw_', 'swingHeavy', 0.5, 0.06, 0.05),
    blip: one('blip', 'blip', 0.4, 0.03, 0, 40),
    count: one('count', 'count', 0.6, 0, 0.2),
    go: one('go', 'go', 0.85, 0, 0.2),
    game: one('game', 'game', 0.95, 0, 0.35),
    od: one('od', 'od', 0.85, 0.02, 0.3),
    tech: one('tech', 'djump', 0.4, 0.02, 0.1),
    // any packed sample by name (items, the Super Orb, finales)
    sample(n, v = 0.7, r = 1) { if (ctx && bufs[n]) play(n, v, r * vary(0.03), 0.15); },
    // character voices: kind is atk1 (light effort), atk2 (big effort), hurt, jump or ko
    voice(f, kind, force) {
      const v = VOICES[f.def.id]; if (!v || !ctx) return;
      const who = f.def.id + (f.port || 0), now = performance.now(), mem = VMEM[who] || (VMEM[who] = { t: 0, last: {} });
      // one voice at a time per fighter, with breathing room so it never chatters
      if (!force && now - mem.t < (kind === 'hurt' ? 650 : 1100)) return;
      const n = VN.filter(i => bufs['v_' + v.g + '_' + kind + '_' + i] && i !== mem.last[kind]);
      if (!n.length) return;
      const pick = n[Math.floor(Math.random() * n.length)];
      mem.last[kind] = pick; mem.t = now;
      const vol = { ko: 0.85, win: 0.9, jump: 0.4, atk1: 0.5, atk2: 0.68, hurt: 0.62 }[kind] || 0.6;
      play('v_' + v.g + '_' + kind + '_' + pick, vol * vary(0.08), v.rate * vary(0.05), kind === 'ko' || kind === 'win' ? 0.45 : 0.12);
    },
  });
  const VMEM = {}, VN = [1, 2, 3];
  // generic generated voices, shared by build; the rate shifts the pitch so family members sharing a set sound different
  const VOICES = { ray: { g: 'boy', rate: 1.0 }, ollie: { g: 'boy', rate: 1.08 }, norah: { g: 'girl', rate: 1.0 }, tyler: { g: 'teen', rate: 0.97 }, jayson: { g: 'teen', rate: 1.05 },
    layne: { g: 'teen', rate: 0.9 }, violet: { g: 'ywoman', rate: 1.0 }, dad: { g: 'man', rate: 1.0 }, mom: { g: 'woman', rate: 1.0 },
    river: { g: 'dog', rate: 1.0 }, rosie: { g: 'cat', rate: 1.0 }, fluff: { g: 'dragon', rate: 1.0 } };
})();

// ================================================================ VISUALS
// Everything bright is drawn on its own canvas that blends as light over the 3D scene, using soft sprites and
// smooth ribbons, then bloomed. Nothing has a hard edge.
(() => {
  const TAU = Math.PI * 2;
  const easeOut = t => 1 - Math.pow(1 - t, 3), easeOut5 = t => 1 - Math.pow(1 - t, 5);
  // ---- glow canvas
  const fxc = document.createElement('canvas'); fxc.id = 'fxc';
  fxc.style.cssText = 'position:fixed;inset:0;width:100%;height:100%;display:block;pointer-events:none;mix-blend-mode:screen;opacity:0.78';
  const main = document.getElementById('c'); main.parentNode.insertBefore(fxc, main);
  const X = fxc.getContext('2d'), b1 = document.createElement('canvas'), B1 = b1.getContext('2d'), b2 = document.createElement('canvas'), B2 = b2.getContext('2d');
  let FW = 0, FH = 0;
  function size() { FW = fxc.width = Math.max(2, innerWidth); FH = fxc.height = Math.max(2, innerHeight); b1.width = Math.ceil(FW / 4); b1.height = Math.ceil(FH / 4); b2.width = Math.ceil(FW / 10); b2.height = Math.ceil(FH / 10); }
  addEventListener('resize', size); size();

  // ---- soft sprites
  const mk = (w, h, d) => { const c = document.createElement('canvas'); c.width = w; c.height = h; d(c.getContext('2d'), w, h); return c; };
  const S_GLOW = mk(128, 128, g => { const r = g.createRadialGradient(64, 64, 0, 64, 64, 64); r.addColorStop(0, 'rgba(255,255,255,1)'); r.addColorStop(0.22, 'rgba(255,255,255,0.6)'); r.addColorStop(0.55, 'rgba(255,255,255,0.14)'); r.addColorStop(1, 'rgba(255,255,255,0)'); g.fillStyle = r; g.fillRect(0, 0, 128, 128); });
  const S_STREAK = mk(256, 64, g => { g.translate(128, 32); g.save(); g.scale(2, 0.16); g.drawImage(S_GLOW, -64, -64); g.restore(); g.globalAlpha = 0.45; g.scale(1.5, 0.42); g.drawImage(S_GLOW, -64, -64); });
  const S_COMET = mk(256, 64, g => { const r = g.createLinearGradient(0, 0, 256, 0); r.addColorStop(0, 'rgba(255,255,255,0)'); r.addColorStop(0.75, 'rgba(255,255,255,0.7)'); r.addColorStop(1, 'rgba(255,255,255,1)'); g.fillStyle = r; g.beginPath(); g.moveTo(0, 32); g.quadraticCurveTo(170, 6, 250, 16); g.quadraticCurveTo(262, 32, 250, 48); g.quadraticCurveTo(170, 58, 0, 32); g.fill(); g.globalCompositeOperation = 'destination-in'; g.translate(128, 32); g.scale(2, 0.5); g.drawImage(S_GLOW, -64, -64); });
  const S_RING = mk(256, 256, g => { const r = g.createRadialGradient(128, 128, 40, 128, 128, 128); r.addColorStop(0, 'rgba(255,255,255,0)'); r.addColorStop(0.62, 'rgba(255,255,255,0.03)'); r.addColorStop(0.84, 'rgba(255,255,255,0.95)'); r.addColorStop(0.92, 'rgba(255,255,255,0.3)'); r.addColorStop(1, 'rgba(255,255,255,0)'); g.fillStyle = r; g.fillRect(0, 0, 256, 256); });
  const S_WISP = [0, 1, 2, 3].map(i => mk(128, 128, g => { let s = 7 + i * 31; const r = () => (s = (s * 16807) % 2147483647) / 2147483647; for (let k = 0; k < 16; k++) { const a = r() * TAU, d = r() * 30, rad = 14 + r() * 26; g.globalAlpha = 0.16; g.drawImage(S_GLOW, 64 + Math.cos(a) * d - rad, 64 + Math.sin(a) * d - rad, rad * 2, rad * 2); } }));
  const tints = new Map();
  function tint(spr, color) {
    if (!color || color === '#ffffff' || color === '#fff') return spr;
    let m = tints.get(spr); if (!m) tints.set(spr, (m = new Map()));
    let c = m.get(color); if (!c) { c = mk(spr.width, spr.height, g => { g.drawImage(spr, 0, 0); g.globalCompositeOperation = 'source-in'; g.fillStyle = color.length > 7 ? color.slice(0, 7) : color; g.fillRect(0, 0, spr.width, spr.height); }); m.set(color, c); }
    return c;
  }
  function spr(img, x, y, w, h, rot, alpha) { if (alpha <= 0.003) return; X.globalAlpha = Math.min(1, alpha); if (rot) { X.save(); X.translate(x, y); X.rotate(rot); X.drawImage(img, -w / 2, -h / 2, w, h); X.restore(); } else X.drawImage(img, x - w / 2, y - h / 2, w, h); }

  // ---- particles (own list; G.parts keeps only confetti and the KO beam)
  let ps = [], lastFrame = 0;
  const add = p => { if (ps.length < 1500) ps.push(p); };
  const Pn = (k, x, y, life, o) => add(Object.assign({ k, x, y, life, t: 0, vx: 0, vy: 0, drag: 0.9, grav: 0, color: '#ffffff', size: 10 }, o));

  Object.assign(fx, {
    add(p) { if (G.parts.length < 300) G.parts.push(p); },
    spark(x, y, color, n = 4, sp = 5) { for (let i = 0; i < n; i++) { const a = rand(0, TAU), s = rand(sp * 0.3, sp); Pn('glow', x, y, rand(14, 28), { vx: Math.cos(a) * s, vy: Math.sin(a) * s, color, size: rand(5, 11), drag: 0.92, grav: 0.05 }); } },
    hit(x, y, d, color, lx, ly, spd) {
      const k = Math.min(d, 28) / 28, ang = lx != null ? Math.atan2(ly, lx) : rand(0, TAU);
      // kept see-through and short so the fighters stay visible through a hit
      Pn('bloom', x, y, 7 + k * 4, { size: 50 + k * 110, color: '#ffffff', amp: 0.6 });
      Pn('bloom', x, y, 12 + k * 7, { size: 90 + k * 190, color, amp: 0.45 });
      Pn('flare', x, y, 10 + k * 6, { size: 180 + k * 420, color, rot: rand(-0.12, 0.12), amp: 0.6 });
      Pn('ring', x, y, 16 + k * 10, { size: 60 + k * 190, color });
      const petals = 7 + Math.round(k * 9), off = rand(0, TAU);
      for (let i = 0; i < petals; i++) { const a = off + (i / petals) * TAU + rand(-0.12, 0.12), s = rand(9, 15) + k * 16; Pn('glow', x, y, rand(11, 18) + k * 7, { vx: Math.cos(a) * s, vy: Math.sin(a) * s, color: i % 2 ? '#ffffff' : color, size: 9 + k * 12, drag: 0.8, stretch: 5 }); }
      const cone = 6 + Math.round(k * 22);
      for (let i = 0; i < cone; i++) { const a = ang + rand(-0.45, 0.45) * (1 - k * 0.3), s = rand(10, 22) + k * rand(8, 30); Pn('glow', x, y, rand(14, 26) + k * 10, { vx: Math.cos(a) * s, vy: Math.sin(a) * s, color: i % 3 ? color : '#ffffff', size: 6 + k * 9, drag: 0.86, stretch: 6 }); }
      for (let i = 0; i < 2 + k * 6; i++) { const a = ang + rand(-1.2, 1.2), s = rand(1, 4 + k * 5); Pn('wisp', x, y, rand(22, 38), { vx: Math.cos(a) * s, vy: Math.sin(a) * s, color, size: 34 + k * 56, rot: rand(0, TAU), vr: rand(-0.05, 0.05), alpha: 0.3, drag: 0.93, v: i % 4 }); }
      for (let i = 0; i < 2 + k * 6; i++) Pn('twinkle', x + rand(-50, 50) * (0.4 + k), y + rand(-50, 50) * (0.4 + k), rand(14, 30), { size: rand(16, 40) * (0.5 + k), color: i % 2 ? color : '#ffffff', delay: rand(0, 8) });
    },
    smoke(x, y, n = 3, color) { for (let i = 0; i < n; i++) Pn('wisp', x + rand(-10, 10), y - rand(0, 8), rand(26, 44), { vx: rand(-1.4, 1.4), vy: rand(-1.4, -0.3), color: color && color.length <= 7 ? color : '#cfd4e6', size: rand(26, 48), rot: rand(0, TAU), vr: rand(-0.04, 0.04), alpha: 0.3, drag: 0.95, v: i % 4 }); },
    dust(x, y, dir = 0, n = 6) { for (let i = 0; i < n; i++) { const d = dir || (i % 2 ? 1 : -1); Pn('wisp', x + rand(-6, 6), y - rand(2, 8), rand(24, 40), { vx: -d * rand(1.5, 5), vy: rand(-1.1, -0.2), color: '#d8d2c8', size: rand(26, 46), rot: rand(0, TAU), vr: rand(-0.05, 0.05), alpha: 0.26, drag: 0.9, v: i % 4 }); } },
    ring(x, y, color, size = 40) { Pn('ring', x, y, 20, { size: size * 2.4, color }); },
    groundRing(x, y, color, size = 60) { Pn('ring', x, y, 22, { size: size * 2.6, color, flat: 0.26 }); },
    boom(x, y, r) {
      Pn('bloom', x, y, 14, { size: r * 4, color: '#ffffff' }); Pn('bloom', x, y, 24, { size: r * 6, color: '#ff9a3d' }); Pn('ring', x, y, 22, { size: r * 4.4, color: '#ffd27a' });
      for (let i = 0; i < 16; i++) { const a = rand(0, TAU), s = rand(6, 20); Pn('glow', x, y, rand(14, 26), { vx: Math.cos(a) * s, vy: Math.sin(a) * s, color: i % 2 ? '#ffffff' : '#ffb347', size: 12, drag: 0.84, stretch: 5 }); }
      for (let i = 0; i < 8; i++) Pn('wisp', x, y, rand(30, 50), { vx: rand(-3, 3), vy: rand(-3.5, 0.5), color: i % 2 ? '#ff8a3d' : '#8a8494', size: r * rand(1, 1.8), rot: rand(0, TAU), vr: rand(-0.05, 0.05), alpha: 0.5, drag: 0.93, v: i % 4 });
    },
    koBlast(x, y, color) {
      G.parts.push({ k: 'ko', x, y, life: 70, t: 0, color, ang: Math.atan2(-150 - y, -x) });
      for (let i = 0; i < 50; i++) { const a = Math.atan2(-150 - y, -x) + rand(-0.9, 0.9), s = rand(8, 34); G.parts.push({ k: 'confetti', x, y, life: rand(30, 70), t: 0, vx: Math.cos(a) * s, vy: Math.sin(a) * s, color: [color, '#ffffff', '#ffd23f', '#ff6ad5', '#6cf0ff'][i % 5], size: rand(4, 9), rot: rand(0, TAU), vr: rand(-0.4, 0.4), grav: 0.25, drag: 0.95 }); }
      Sfx.ko && Sfx.ko();
    },
    finisher(x, y, lx, ly, color, frames) {
      const ang = Math.atan2(ly, lx);
      G.fin = { t: 0, max: frames, x, y, color, ang, seed: Math.random() * 1000 };
      for (let i = 0; i < 7; i++) Pn('arc', x, y, rand(frames * 0.6, frames + 8), { color: i % 2 ? '#ffffff' : color, ang: rand(0, TAU), len: rand(150, 330), seed: rand(0, 99), size: rand(3, 6) });
      for (let i = 0; i < 34; i++) { const a = ang + rand(-0.5, 0.5), s = rand(18, 60); Pn('glow', x, y, rand(18, 34), { vx: Math.cos(a) * s, vy: Math.sin(a) * s, color: i % 2 ? '#ffffff' : color, size: 14, drag: 0.86, stretch: 7 }); }
      Pn('bloom', x, y, frames, { size: 240, color, hold: true, amp: 0.35 }); Pn('bloom', x, y, 10, { size: 150, color: '#ffffff', amp: 0.6 }); Pn('flare', x, y, frames, { size: 1300, color, hold: true });
      for (let i = 0; i < 3; i++) Pn('ring', x, y, 22 + i * 9, { size: 300 + i * 220, color: i ? color : '#ffffff', delay: i * 3 });
    },
  });

  // ---- sim-rate hooks: sounds, dust, trails
  const mem = new WeakMap();
  const LIMBS = ['LeftHand', 'RightHand', 'LeftFoot', 'RightFoot', 'Blade'];
  const baseStep = step;
  step = function () {
    baseStep();
    if (G.frame < lastFrame) ps = []; lastFrame = G.frame;
    if (G.fin && ++G.fin.t > G.fin.max + 14) G.fin = null;
    for (const f of G.fighters) {
      let m = mem.get(f); if (!m) { m = { st: f.state, gr: f.grounded, trail: [], jumps: f.airJumps, limb: {}, prev: {} }; mem.set(f, m); }
      const sp = Math.hypot(f.lvx, f.lvy), gone = f.state === 'dead' || f.state === 'out' || f.hidden;
      // launch ribbon
      if (f.state === 'hitstun' && sp > 6 && !gone) m.trail.push({ x: f.x, y: f.y - f.h * 0.5, w: Math.min(30, 8 + sp * 0.9) }); else if (m.trail.length) m.trail.splice(0, 2);
      if (m.trail.length > 22) m.trail.shift();
      if (f.state === 'hitstun' && sp > 12 && f.hitstop <= 0) { Pn('wisp', f.x + rand(-8, 8), f.y - f.h * 0.5 + rand(-10, 10), rand(22, 36), { vx: rand(-0.6, 0.6), vy: rand(-0.6, 0.6), color: PORT_COLORS[f.port], size: rand(34, 58), rot: rand(0, TAU), vr: rand(-0.06, 0.06), alpha: 0.4, drag: 0.94, v: G.frame % 4 }); if (G.frame % 2 === 0) Pn('glow', f.x + rand(-12, 12), f.y - rand(8, f.h), rand(16, 28), { vx: -f.lvx * 0.1 + rand(-1, 1), vy: -f.lvy * 0.1 + rand(-1, 1), color: '#ffffff', size: rand(5, 10), drag: 0.92 }); }
      // limb and sword trails during attacks (follows the real bones of the scanned models)
      const lp = !gone && typeof R3 !== 'undefined' && R3.limbPoints ? R3.limbPoints(f) : null;
      const swinging = f.state === 'attack' && f.hitstop <= 0;
      for (const n of LIMBS) {
        const tr = m.limb[n] || (m.limb[n] = []), p = lp && lp[n], pv = m.prev[n];
        if (p && pv) {
          const rv = Math.hypot(p.x - pv.x - (f.vx + f.lvx), p.y - pv.y - (f.vy + f.lvy));
          const min = n === 'Blade' ? 5 : 7;
          if (swinging && rv > min) tr.push({ x: p.x, y: p.y, w: Math.min(n === 'Blade' ? 26 : 20, 4 + rv * 0.9) * f.sc });
          else if (tr.length) tr.splice(0, f.hitstop > 0 ? 0 : 2);
        } else if (tr.length && f.hitstop <= 0) tr.splice(0, 2);
        if (tr.length > (n === 'Blade' ? 13 : 9)) tr.shift();
        if (p) m.prev[n] = { x: p.x, y: p.y }; else delete m.prev[n];
      }
      // footsteps, dust, state-change sounds
      if (f.state === 'run' && f.grounded && f.hitstop <= 0 && f.anim % 11 === 0) { Sfx.step(); fx.dust(f.x - f.facing * 8, f.y, f.facing, 1); }
      if (f.state === 'run' && m.st !== 'run') { Sfx.dash(); fx.dust(f.x, f.y, f.facing, 6); }
      if (f.grounded && !m.gr && f.state !== 'respawn' && !gone) { const hard = f.state === 'down' || m.st === 'hitstun'; fx.dust(f.x, f.y, 0, hard ? 10 : 4); fx.groundRing(f.x, f.y, '#ffffff', hard ? 60 : 26); Sfx.land(hard ? 1 : 0.3); if (hard && sp > 9) Pn('bloom', f.x, f.y, 10, { size: 160, color: '#ffffff' }); }
      if (!f.grounded && f.airJumps < m.jumps && f.state === 'air') { Sfx.djump(); fx.groundRing(f.x, f.y, PORT_COLORS[f.port], 34); for (let i = 0; i < 6; i++) Pn('glow', f.x + rand(-14, 14), f.y, rand(12, 20), { vx: rand(-2, 2), vy: rand(1, 4), color: PORT_COLORS[f.port], size: 8, drag: 0.9 }); }
      if (f.state === 'attack' && f.move && f.mtHold && f.move.charge) Sfx.charge(Math.min(1, f.charge / 60));
      if (f.state === 'attack' && m.st !== 'attack' && f.move && f.def.id === 'ollie' && !f.move.special) Sfx.blade();
      if (f.state === 'tech' && m.st !== 'tech') Sfx.tech();
      if (f.state === 'shieldstun' && m.st !== 'shieldstun') { Sfx.shieldHit(); Pn('ring', f.x, f.y - f.h * 0.5, 16, { size: 170 * f.sc, color: PORT_COLORS[f.port] }); }
      if (f.state === 'holding' && m.st !== 'holding') Sfx.grab();
      if (f.od > 0) Pn('wisp', f.x + rand(-14, 14) * f.sc, f.y - rand(0, f.h * 0.6), rand(18, 30), { vx: rand(-0.5, 0.5), vy: rand(-4, -2), color: f.def.accent, size: rand(22, 40), rot: rand(0, TAU), vr: rand(-0.08, 0.08), alpha: 0.5, drag: 0.97, v: G.frame % 4 });
      if (f.state === 'attack' && f.charge > 0 && f.mtHold) { const a = rand(0, TAU), r = f.h * rand(0.9, 1.3); Pn('glow', f.x + Math.cos(a) * r, f.y - f.h / 2 + Math.sin(a) * r, 12, { vx: -Math.cos(a) * r / 11, vy: -Math.sin(a) * r / 11, color: f.def.accent, size: 9, drag: 1, stretch: 4 }); }
      // voices
      if (f.state === 'attack' && m.st !== 'attack' && f.move) {
        const n = f.move.name, big = f.move.smash || f.move.special || n === 'throw', light = n === 'jab1' || n === 'jab2' || n === 'grab' || n === 'dashgrab';
        if (big) { if (Math.random() < 0.8) Sfx.voice(f, 'atk2'); } else if (Math.random() < (light ? 0.1 : 0.3)) Sfx.voice(f, 'atk1');
      }
      if (f.state === 'hitstun' && m.st !== 'hitstun' && f.hitstun > 14 && Math.random() < 0.6) Sfx.voice(f, 'hurt');
      if (f.state === 'air' && m.st === 'jumpsquat' && Math.random() < 0.18) Sfx.voice(f, 'jump');
      if ((f.state === 'dead' || f.state === 'out') && m.st !== 'dead' && m.st !== 'out') Sfx.voice(f, 'ko', true);
      m.st = f.state; m.gr = f.grounded; m.jumps = f.airJumps;
    }
    for (const p of G.projs) { if (p.invisible) continue; const tr = p.fxTrail || (p.fxTrail = []); tr.push({ x: p.x, y: p.y, w: p.r * 1.5 }); if (tr.length > 10) tr.shift(); }
    if (G.ending === 1 && !G.gameSnd) { G.gameSnd = true; Sfx.game(); }
    if (!G.ending) G.gameSnd = false;
  };
  if (window.__clash) window.__clash.step = step;

  const baseCam = updateCam;
  updateCam = function () {
    baseCam();
    const f = G.fin; if (!f || G.camLock) return;
    const k = Math.max(0, 1 - f.t / (f.max + 6));
    G.cam.x = lerp(G.cam.x, f.x, 0.2 * k); G.cam.y = lerp(G.cam.y, f.y, 0.2 * k); G.cam.z = Math.min(1.9, G.cam.z * (1 + 0.03 * k));
  };

  updateParts = function () {
    for (const p of ps) {
      if (p.delay > 0) { p.delay--; continue; }
      p.t++; p.x += p.vx; p.y += p.vy; p.vx *= p.drag; p.vy = p.vy * p.drag + p.grav; if (p.vr) p.rot += p.vr;
    }
    ps = ps.filter(p => p.t < p.life);
    for (const p of G.parts) { p.t++; if (p.vx == null) continue; p.x += p.vx; p.y += p.vy; const d = p.drag || 0.94; p.vx *= d; p.vy = p.vy * d + (p.grav || 0); if (p.vr) p.rot += p.vr; }
    G.parts = G.parts.filter(p => p.t < p.life);
  };

  // ---- smooth tapered ribbon through a list of points (tail first, head last)
  function ribbon(pts, color, alpha, wMul) {
    const n = pts.length; if (n < 3) return;
    const sm = [];
    for (let i = 0; i < n - 1; i++) { // Catmull-Rom subdivision
      const p0 = pts[Math.max(0, i - 1)], p1 = pts[i], p2 = pts[i + 1], p3 = pts[Math.min(n - 1, i + 2)];
      for (let s = 0; s < 4; s++) { const t = s / 4, t2 = t * t, t3 = t2 * t;
        sm.push({ x: 0.5 * (2 * p1.x + (p2.x - p0.x) * t + (2 * p0.x - 5 * p1.x + 4 * p2.x - p3.x) * t2 + (3 * p1.x - p0.x - 3 * p2.x + p3.x) * t3),
          y: 0.5 * (2 * p1.y + (p2.y - p0.y) * t + (2 * p0.y - 5 * p1.y + 4 * p2.y - p3.y) * t2 + (3 * p1.y - p0.y - 3 * p2.y + p3.y) * t3), w: p1.w + (p2.w - p1.w) * t }); }
    }
    sm.push(pts[n - 1]);
    const m = sm.length, L = [], R = [];
    for (let i = 0; i < m; i++) {
      const a = sm[Math.max(0, i - 1)], b = sm[Math.min(m - 1, i + 1)]; let nx = -(b.y - a.y), ny = b.x - a.x; const d = Math.hypot(nx, ny) || 1; nx /= d; ny /= d;
      const u = i / (m - 1), w = sm[i].w * wMul * Math.pow(u, 0.8) * (u > 0.9 ? 1 - (u - 0.9) * 6 : 1) * 0.5;
      L.push([sm[i].x + nx * w, sm[i].y + ny * w]); R.push([sm[i].x - nx * w, sm[i].y - ny * w]);
    }
    const g = X.createLinearGradient(sm[0].x, sm[0].y, sm[m - 1].x, sm[m - 1].y); const c = color.length > 7 ? color.slice(0, 7) : color;
    g.addColorStop(0, c + '00'); g.addColorStop(0.5, c + '99'); g.addColorStop(1, c + 'ff');
    X.globalAlpha = alpha; X.fillStyle = g; X.beginPath(); X.moveTo(L[0][0], L[0][1]);
    for (let i = 1; i < m; i++) X.lineTo(L[i][0], L[i][1]); for (let i = m - 1; i >= 0; i--) X.lineTo(R[i][0], R[i][1]); X.closePath(); X.fill();
  }
  const hex6 = c => (c.length === 4 ? '#' + c[1] + c[1] + c[2] + c[2] + c[3] + c[3] : c);

  function drawParticle(p) {
    if (p.delay > 0) return;
    const u = p.t / p.life, k = 1 - u;
    switch (p.k) {
      case 'glow': {
        const v = Math.hypot(p.vx, p.vy), a = Math.min(1, k * 1.6), s = p.size * (0.5 + k * 0.7);
        if (p.stretch && v > 1.2) { const len = s + v * p.stretch; spr(tint(S_COMET, p.color), p.x - p.vx * p.stretch * 0.35, p.y - p.vy * p.stretch * 0.35, len, s * 1.5, Math.atan2(p.vy, p.vx), a); spr(S_GLOW, p.x, p.y, s * 1.1, s * 1.1, 0, a); }
        else { spr(tint(S_GLOW, p.color), p.x, p.y, s * 2.4, s * 2.4, 0, a); spr(S_GLOW, p.x, p.y, s, s, 0, a); }
        break;
      }
      case 'wisp': { const s = p.size * (0.55 + easeOut(u) * 1.1); spr(tint(S_WISP[p.v || 0], p.color), p.x, p.y, s, s, p.rot, (p.alpha || 0.4) * Math.sin(Math.min(1, u * 5) * Math.PI / 2) * k); break; }
      case 'bloom': { const s = p.size * (p.hold ? 0.8 + 0.2 * Math.sin(u * 40) : 0.45 + easeOut5(u) * 0.75); spr(tint(S_GLOW, p.color), p.x, p.y, s, s, 0, (p.hold ? Math.min(1, k * 3) : k * k) * (p.amp || 1)); break; }
      case 'flare': { const s = p.size * (p.hold ? 1 : 0.4 + easeOut5(u) * 0.8), a = (p.hold ? Math.min(1, k * 3) * 0.6 : k * k) * (p.amp || 1); spr(tint(S_STREAK, p.color), p.x, p.y, s, s * 0.11, p.rot || 0, a); spr(S_STREAK, p.x, p.y, s * 0.55, s * 0.05, p.rot || 0, a); spr(tint(S_STREAK, p.color), p.x, p.y, s * 0.3, s * 0.045, (p.rot || 0) + Math.PI / 2, a * 0.7); break; }
      case 'ring': { const s = p.size * (0.12 + easeOut5(u) * 0.95); X.globalAlpha = 1; spr(tint(S_RING, p.color), p.x, p.y, s, s * (p.flat || 1), 0, k * k * 0.95); break; }
      case 'twinkle': { const s = p.size * Math.sin(u * Math.PI), a = Math.sin(u * Math.PI); spr(tint(S_STREAK, p.color), p.x, p.y, s * 2, s * 0.32, 0, a); spr(tint(S_STREAK, p.color), p.x, p.y, s * 2, s * 0.32, Math.PI / 2, a); spr(S_GLOW, p.x, p.y, s * 0.6, s * 0.6, 0, a); break; }
      case 'arc': { // flowing energy arc: a smooth curve that writhes and tapers
        const a = Math.min(1, k * 2.5), tt = G.frame * 0.35 + p.seed, N = 22; X.lineCap = 'round'; X.lineJoin = 'round';
        for (let pass = 0; pass < 2; pass++) {
          X.globalAlpha = a * (pass ? 0.95 : 0.35); X.strokeStyle = pass ? '#ffffff' : hex6(p.color); X.lineWidth = p.size * (pass ? 0.55 : 2.6) * (0.4 + k);
          X.beginPath();
          for (let i = 0; i <= N; i++) { const s = i / N, d = p.len * s * (0.5 + easeOut(Math.min(1, u * 3)) * 0.5), env = Math.sin(s * Math.PI) * (0.5 + s), off = (Math.sin(s * 7 + tt) * 26 + Math.sin(s * 13 - tt * 1.7 + p.seed) * 12) * env;
            const px = p.x + Math.cos(p.ang) * d - Math.sin(p.ang) * off, py = p.y + Math.sin(p.ang) * d + Math.cos(p.ang) * off; if (i) X.lineTo(px, py); else X.moveTo(px, py); }
          X.stroke();
        }
        break;
      }
    }
  }

  // ---- the glow pass: called from the game's render with the world transform already on the main canvas
  drawParts = function () {
    // confetti stays on the normal canvas
    for (const p of G.parts) if (p.k === 'confetti') { const k = 1 - p.t / p.life; ctx.globalAlpha = Math.min(1, k * 2); ctx.fillStyle = p.color; ctx.save(); ctx.translate(p.x, p.y); ctx.rotate(p.rot); ctx.beginPath(); ctx.ellipse(0, 0, p.size * 0.6, p.size * 0.3 * (0.3 + Math.abs(Math.cos(p.t * 0.3))), 0, 0, TAU); ctx.fill(); ctx.restore(); }
    ctx.globalAlpha = 1;
    const T = ctx.getTransform(), q = FW / (cv.width || 1);
    X.setTransform(1, 0, 0, 1, 0, 0); X.globalCompositeOperation = 'source-over'; X.globalAlpha = 1; X.fillStyle = '#000'; X.fillRect(0, 0, FW, FH);
    X.setTransform(T.a * q, T.b * q, T.c * q, T.d * q, T.e * q, T.f * q); X.globalCompositeOperation = 'lighter';
    for (const f of G.fighters) {
      const m = mem.get(f); if (!m) continue; const pc = PORT_COLORS[f.port];
      if (m.trail.length > 2) { ribbon(m.trail, pc, 0.55, 1.0); ribbon(m.trail, '#ffffff', 0.8, 0.3); }
      for (const n of LIMBS) { const tr = m.limb[n]; if (tr && tr.length > 2) { ribbon(tr, hex6(f.def.accent), 0.7, n === 'Blade' ? 1.7 : 1.3); ribbon(tr, '#ffffff', 0.95, n === 'Blade' ? 0.6 : 0.42); const h = tr[tr.length - 1]; spr(tint(S_GLOW, f.def.accent), h.x, h.y, h.w * 3, h.w * 3, 0, 0.7); } }
      if (f.state === 'attack' && f.move && !f.hidden) for (const h of activeHBsForDraw(f)) { const w = hbWorld(f, h); spr(tint(S_GLOW, f.def.accent), w.x, w.y, w.r * 3.4, w.r * 3.4, 0, 0.5); spr(S_GLOW, w.x, w.y, w.r * 1.4, w.r * 1.4, 0, 0.55); }
      if (f.charge > 0 && f.state === 'attack' && !f.hidden) { const c = Math.min(1, f.charge / 60), s = f.h * (1.2 + c * 1.4) * (1 + 0.08 * Math.sin(G.frame * 0.6)); spr(tint(S_GLOW, f.def.accent), f.x, f.y - f.h / 2, s, s, 0, 0.35 + c * 0.45); for (let k2 = 0; k2 < 2; k2++) { const uu = (G.frame * 0.04 + k2 * 0.5) % 1, rs = f.h * (2.6 - uu * 2.1); spr(tint(S_RING, f.def.accent), f.x, f.y - f.h / 2, rs, rs, 0, uu * (0.3 + c * 0.5)); } }
      if (f.od > 0 && !f.hidden && f.state !== 'dead') spr(tint(S_GLOW, f.def.accent), f.x, f.y - f.h / 2, f.h * 2.2, f.h * 2.6, 0, 0.3 + 0.1 * Math.sin(G.frame * 0.3));
      if (f.counter) { const s = f.h * (1.5 + 0.1 * Math.sin(G.frame)); spr(S_RING, f.x, f.y - f.h / 2, s, s, 0, 0.8); }
    }
    for (const p of G.projs) { if (p.invisible) continue; const c = hex6(p.owner.def.accent); if (p.fxTrail && p.fxTrail.length > 2) { ribbon(p.fxTrail, c, 0.7, 1.4); ribbon(p.fxTrail, '#ffffff', 0.8, 0.5); } spr(tint(S_GLOW, c), p.x, p.y, p.r * 5, p.r * 5, 0, 0.8); spr(S_GLOW, p.x, p.y, p.r * 2, p.r * 2, 0, 0.9); }
    for (const p of ps) drawParticle(p);
    // screen-space: finisher speed lines and the KO beam
    X.setTransform(1, 0, 0, 1, 0, 0);
    const q2 = FW / VW, fin = G.fin;
    if (fin) {
      const k = fin.t < fin.max ? Math.min(1, fin.t / 3) : Math.max(0, 1 - (fin.t - fin.max) / 14), c = w2s(fin.x, fin.y), cx = c.x * q2, cy = c.y * q2, R = Math.hypot(FW, FH);
      let s = fin.seed; const rr = () => (s = (s * 9301 + 49297) % 233280) / 233280;
      for (let i = 0; i < 70; i++) {
        const a = rr() * TAU, sp = 26 + rr() * 40, range = R * 0.9, d = 110 + ((rr() * range + G.frame * sp) % range), len = 180 + rr() * 520 + d * 0.5, wd = 5 + rr() * 16 + d * 0.012;
        const al = k * Math.min(1, (d - 100) / 220) * (0.25 + rr() * 0.6);
        spr(i % 3 === 0 ? tint(S_STREAK, hex6(fin.color)) : S_STREAK, cx + Math.cos(a) * (d + len / 2), cy + Math.sin(a) * (d + len / 2), len, wd, a, al);
      }
    }
    for (const p of G.parts) {
      if (p.k !== 'ko') continue;
      const u = p.t / p.life, k = 1 - u, sp = w2s(p.x, p.y), ex = clamp(sp.x, 0, VW) * q2, ey = clamp(sp.y, 0, VH) * q2, L = Math.max(FW, FH) * (0.7 + easeOut5(u) * 1.1), wd = (260 * k * k + 14) * (1 + 0.06 * Math.sin(p.t * 1.3)), col = hex6(p.color);
      X.save(); X.translate(ex, ey); X.rotate(p.ang);
      spr(tint(S_STREAK, col), L * 0.42, 0, L * 1.7, wd * 2.6, 0, k * 0.9); spr(tint(S_STREAK, col), L * 0.42, 0, L * 1.5, wd * 1.2, 0, k); spr(S_STREAK, L * 0.4, 0, L * 1.4, wd * 0.5, 0, k);
      for (let i = 0; i < 16; i++) { const ph = ((p.t * 0.05 + i / 16) % 1), d = ph * L, off = Math.sin(ph * 9 + i * 2.1 + p.t * 0.2) * wd * 0.5 * (1 - ph * 0.4), sz = (40 + (i % 5) * 22) * (0.5 + k); spr(i % 2 ? S_GLOW : tint(S_WISP[i % 4], col), d, off, sz * 1.6, sz, 0, k * (1 - ph) * 0.9); }
      spr(S_GLOW, 0, 0, 520 * k + 60, 520 * k + 60, 0, k); spr(tint(S_GLOW, col), 0, 0, 900 * k + 100, 900 * k + 100, 0, k * 0.8);
      for (let i = 0; i < 3; i++) { const uu = Math.min(1, u * 1.6 + i * 0.12), rs = 140 + easeOut5(uu) * (700 + i * 320); spr(tint(S_RING, i ? col : '#ffffff'), 0, 0, rs * 0.55, rs, 0, (1 - uu) * 0.9); }
      X.restore();
    }
    if (G.fxLow) { X.globalAlpha = 1; X.globalCompositeOperation = 'source-over'; return; }
    // bloom: blur by shrinking twice and adding the soft copies back
    X.setTransform(1, 0, 0, 1, 0, 0); X.globalAlpha = 1;
    B1.globalCompositeOperation = 'copy'; B1.drawImage(fxc, 0, 0, b1.width, b1.height); B2.globalCompositeOperation = 'copy'; B2.drawImage(b1, 0, 0, b2.width, b2.height);
    X.globalCompositeOperation = 'lighter'; X.globalAlpha = 0.55; X.drawImage(b1, 0, 0, FW, FH); X.globalAlpha = 0.6; X.drawImage(b2, 0, 0, FW, FH);
    X.globalAlpha = 1; X.globalCompositeOperation = 'source-over';
  };

  // finisher vignette (soft, on the normal canvas) - the beam itself is on the glow canvas
  drawKOBeams = function () {
    const f = G.fin; if (!f) return;
    const k = f.t < f.max ? Math.min(1, f.t / 3) : Math.max(0, 1 - (f.t - f.max) / 14), c = w2s(f.x, f.y), R = Math.hypot(VW, VH);
    const g = ctx.createRadialGradient(c.x, c.y, 60, c.x, c.y, R * 0.75); g.addColorStop(0, 'rgba(8,5,22,' + 0.1 * k + ')'); g.addColorStop(0.35, 'rgba(8,5,22,' + 0.5 * k + ')'); g.addColorStop(1, 'rgba(4,2,12,' + 0.86 * k + ')');
    ctx.fillStyle = g; ctx.fillRect(0, 0, VW, VH);
  };

  const baseMenu = drawMenuBg;
  drawMenuBg = function () { baseMenu(); if (fxc.dataset.clear !== '1') { X.setTransform(1, 0, 0, 1, 0, 0); X.globalCompositeOperation = 'source-over'; X.fillStyle = '#000'; X.fillRect(0, 0, FW, FH); fxc.dataset.clear = '1'; ps = []; } };
  const baseStart = startMatch;
  startMatch = function (c) { fxc.dataset.clear = '0'; ps = []; G.fin = null; return baseStart(c); };
  if (window.__clash) window.__clash.startMatch = startMatch;

  // ---- per-fighter overlay on the normal canvas: shield bubble, dizzy stars, player tag
  drawFighterFX = function (f) {
    if (f.state === 'dead' || f.state === 'out' || f.hidden) return;
    const sc = f.sc, pc = PORT_COLORS[f.port];
    if (f.state === 'shield' || f.state === 'shieldstun') {
      const r = 46 * sc * (0.35 + 0.65 * f.shieldHP / 50), cx = f.x, cy = f.y - f.h * 0.5;
      const g = ctx.createRadialGradient(cx - r * 0.3, cy - r * 0.35, r * 0.1, cx, cy, r); g.addColorStop(0, 'rgba(255,255,255,0.5)'); g.addColorStop(0.5, pc + '44'); g.addColorStop(0.92, pc + 'aa'); g.addColorStop(1, pc + '00');
      ctx.fillStyle = g; ctx.beginPath(); ctx.arc(cx, cy, r * 1.04, 0, TAU); ctx.fill();
      ctx.strokeStyle = 'rgba(255,255,255,.8)'; ctx.lineWidth = 1.5; ctx.lineCap = 'round'; const a = G.frame * 0.07; ctx.beginPath(); ctx.arc(cx, cy, r * 0.8, a, a + 1.1); ctx.stroke(); ctx.beginPath(); ctx.arc(cx, cy, r * 0.62, a + 2.6, a + 3.4); ctx.stroke();
    }
    if (f.state === 'dizzy') { ctx.fillStyle = '#ffe14d'; for (let k = 0; k < 4; k++) { const aa = G.frame * 0.11 + k * 1.57; ctx.beginPath(); ctx.arc(f.x + Math.cos(aa) * 20, f.y - f.h - 8 + Math.sin(aa) * 5, 4, 0, TAU); ctx.fill(); } }
    ctx.fillStyle = pc; ctx.font = 'bold 15px Trebuchet MS, sans-serif'; ctx.textAlign = 'center';
    const ty = f.y - f.h - 26, tag = f.cpu ? 'CPU' : 'P' + (f.port + 1);
    ctx.lineWidth = 3; ctx.strokeStyle = 'rgba(0,0,0,.6)'; ctx.strokeText(tag, f.x, ty); ctx.fillText(tag, f.x, ty);
    ctx.beginPath(); ctx.moveTo(f.x - 6, ty + 4); ctx.lineTo(f.x + 6, ty + 4); ctx.lineTo(f.x, ty + 11); ctx.fill();
  };
})();
