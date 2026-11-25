'use strict';
// Clash Core effects pack: original visual effects (drawn in code) and original sound (synthesized with Web Audio).
// Loaded after game.js; it replaces the simple effect and sound functions defined there.

// ================================================================ SOUND
(() => {
  let ctx = null, out = null, verb = null, noiseBuf = null, lastT = {};
  const rnd = (a, b) => a + Math.random() * (b - a);
  function init() {
    if (!ctx) {
      try { ctx = new (window.AudioContext || window.webkitAudioContext)(); } catch (e) { return; }
      const comp = ctx.createDynamicsCompressor(); comp.threshold.value = -14; comp.ratio.value = 6; comp.attack.value = 0.002; comp.release.value = 0.18;
      const master = ctx.createGain(); master.gain.value = 0.85; comp.connect(master); master.connect(ctx.destination);
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
})();

// ================================================================ VISUALS
(() => {
  const TAU = Math.PI * 2;
  const add = p => { if (G.parts.length < 1400) G.parts.push(p); };
  const P = (k, x, y, life, o) => add(Object.assign({ k, x, y, life, t: 0 }, o));
  const dirSpread = (a, s) => a + rand(-s, s);

  Object.assign(fx, {
    add,
    spark(x, y, color, n = 4, sp = 5) {
      for (let i = 0; i < n; i++) { const a = rand(0, TAU), s = rand(sp * 0.3, sp); P('ember', x, y, rand(12, 26), { vx: Math.cos(a) * s, vy: Math.sin(a) * s, color, size: rand(1.5, 3.5), grav: 0.12 }); }
    },
    // impact: white core, coloured starburst, streaks thrown along the launch direction, shock ring
    hit(x, y, d, color, lx, ly, spd) {
      const k = Math.min(d, 28) / 28, ang = lx != null ? Math.atan2(ly, lx) : null;
      P('core', x, y, 7 + k * 5, { size: 20 + k * 46, color: '#ffffff' });
      P('burst', x, y, 9 + k * 7, { size: 30 + k * 70, color, rot: rand(0, TAU), points: 6 + Math.round(k * 4) });
      P('shock', x, y, 14 + k * 8, { size: 22 + k * 60, color, w: 3 + k * 5 });
      const n = 6 + Math.round(k * 16);
      for (let i = 0; i < n; i++) {
        const a = ang != null && i % 3 ? dirSpread(ang, 0.7) : rand(0, TAU), s = rand(5, 11 + k * 12);
        P('streak', x, y, rand(8, 16 + k * 8), { vx: Math.cos(a) * s, vy: Math.sin(a) * s, color: i % 2 ? '#ffffff' : color, size: 1.6 + k * 2.6, len: 2.2 + k * 1.6 });
      }
      for (let i = 0; i < 3 + k * 8; i++) { const a = rand(0, TAU), s = rand(2, 6 + k * 5); P('ember', x, y, rand(18, 36), { vx: Math.cos(a) * s, vy: Math.sin(a) * s - 1.5, color, size: rand(2, 4), grav: 0.18 }); }
      if (k > 0.4) for (let i = 0; i < 3; i++) P('star4', x + rand(-30, 30) * k, y + rand(-30, 30) * k, rand(10, 18), { size: rand(10, 22) * (0.6 + k), color: '#ffffff', rot: rand(0, 1) });
    },
    smoke(x, y, n = 3, color) {
      for (let i = 0; i < n; i++) P('puff', x + rand(-10, 10), y - rand(0, 8), rand(20, 38), { vx: rand(-1.6, 1.6), vy: rand(-1.3, -0.2), size: rand(7, 15), color: color || '#e6e8f2' });
    },
    dust(x, y, dir = 0, n = 6) { // low puffs that skid along the ground
      for (let i = 0; i < n; i++) { const d = dir || (i % 2 ? 1 : -1); P('puff', x + rand(-6, 6), y - rand(0, 4), rand(18, 30), { vx: -d * rand(1.2, 4.2), vy: rand(-0.9, -0.1), size: rand(6, 12), color: '#e9e4da', drag: 0.9 }); }
    },
    ring(x, y, color, size = 40) { P('shock', x, y, 18, { size, color, w: 4 }); },
    groundRing(x, y, color, size = 60) { P('shock', x, y, 20, { size, color, w: 5, flat: 0.28 }); },
    boom(x, y, r) {
      P('core', x, y, 12, { size: r * 1.3, color: '#fff4c8' }); P('burst', x, y, 14, { size: r * 2, color: '#ffb347', rot: rand(0, TAU), points: 9 });
      P('shock', x, y, 20, { size: r * 1.5, color: '#ffd27a', w: 6 });
      for (let i = 0; i < 18; i++) { const a = rand(0, TAU), s = rand(4, 12); P('streak', x, y, rand(10, 20), { vx: Math.cos(a) * s, vy: Math.sin(a) * s, color: i % 2 ? '#fff' : '#ff9a3d', size: 3, len: 2.5 }); }
      this.smoke(x, y, 10, '#7a7480');
    },
    // ring-out: a column of light firing back into the screen, confetti and rings
    koBlast(x, y, color) {
      P('ko', x, y, 62, { color, ang: Math.atan2(-150 - y, -x) });
      for (let i = 0; i < 70; i++) { const a = Math.atan2(-150 - y, -x) + rand(-0.9, 0.9), s = rand(8, 34); P('confetti', x, y, rand(30, 70), { vx: Math.cos(a) * s, vy: Math.sin(a) * s, color: [color, '#ffffff', '#ffd23f', '#ff6ad5', '#6cf0ff'][i % 5], size: rand(4, 9), rot: rand(0, TAU), vr: rand(-0.4, 0.4), grav: 0.25, drag: 0.95 }); }
      for (let i = 0; i < 3; i++) P('shock', x, y, 26 + i * 8, { size: 160 + i * 110, color: i ? color : '#ffffff', w: 9 - i * 2 });
      Sfx.ko && Sfx.ko();
    },
    // dramatic freeze on a blow strong enough to finish: dark tint, speed lines, lightning, camera punch-in
    finisher(x, y, lx, ly, color, frames) {
      G.fin = { t: 0, max: frames, x, y, color, ang: Math.atan2(ly, lx), seed: Math.random() * 1000 };
      for (let i = 0; i < 5; i++) P('bolt', x, y, rand(8, 16), { color: i % 2 ? '#ffffff' : color, ang: rand(0, TAU), len: rand(120, 260), seed: Math.random() * 99 });
      for (let i = 0; i < 26; i++) { const a = dirSpread(Math.atan2(ly, lx), 0.5), s = rand(14, 40); P('streak', x, y, rand(14, 26), { vx: Math.cos(a) * s, vy: Math.sin(a) * s, color: i % 2 ? '#fff' : color, size: 4, len: 3 }); }
      P('burst', x, y, 20, { size: 190, color, rot: rand(0, TAU), points: 12 }); P('core', x, y, 14, { size: 110, color: '#fff' });
    },
    slashArc(x, y, r, a0, a1, color, w) { P('slash', x, y, 9, { r, a0, a1, color, w }); },
  });

  // ---- per-frame hooks: footsteps, trails, state-change puffs and sounds
  const mem = new WeakMap();
  const baseStep = step;
  step = function () {
    baseStep();
    if (G.fin && ++G.fin.t > G.fin.max + 10) G.fin = null;
    for (const f of G.fighters) {
      let m = mem.get(f); if (!m) { m = { st: f.state, gr: f.grounded, trail: [], jumps: f.airJumps, hs: 0 }; mem.set(f, m); }
      const sp = Math.hypot(f.lvx, f.lvy);
      // launch ribbon
      if (f.state === 'hitstun' && sp > 7) m.trail.push({ x: f.x, y: f.y - f.h * 0.5, w: Math.min(26, 6 + sp * 0.7) }); else if (m.trail.length) m.trail.shift();
      if (m.trail.length > 16) m.trail.shift();
      if (f.state === 'hitstun' && sp > 14 && G.frame % 2 === 0 && f.hitstop <= 0) P('ember', f.x + rand(-10, 10), f.y - rand(10, f.h), rand(14, 26), { vx: rand(-1, 1), vy: rand(-1, 1), color: PORT_COLORS[f.port], size: rand(2, 5), grav: 0 });
      // footsteps and run dust
      if (f.state === 'run' && f.grounded && f.hitstop <= 0) { if (f.anim % 11 === 0) { Sfx.step(); fx.dust(f.x - f.facing * 8, f.y, f.facing, 1); } }
      if (f.state === 'run' && m.st !== 'run') { Sfx.dash(); fx.dust(f.x, f.y, f.facing, 7); }
      if (f.grounded && !m.gr && f.state !== 'respawn') { const hard = f.state === 'down' || m.st === 'hitstun'; fx.dust(f.x, f.y, 0, hard ? 12 : 5); fx.groundRing(f.x, f.y, '#ffffff', hard ? 70 : 34); Sfx.land(hard ? 1 : 0.3); }
      if (!f.grounded && f.airJumps < m.jumps && f.state === 'air') { Sfx.djump(); fx.groundRing(f.x, f.y, PORT_COLORS[f.port], 40); }
      if (f.state === 'attack' && f.move && f.mtHold && f.move.charge) Sfx.charge(Math.min(1, f.charge / 60));
      if (f.state === 'attack' && m.st !== 'attack' && f.move && f.def.id === 'ollie' && !f.move.special) Sfx.blade();
      if (f.state === 'tech' && m.st !== 'tech') Sfx.tech();
      if (f.state === 'shieldstun' && m.st !== 'shieldstun') { Sfx.shieldHit(); P('shock', f.x, f.y - f.h * 0.5, 14, { size: 60 * f.sc, color: PORT_COLORS[f.port], w: 4 }); }
      if (f.state === 'holding' && m.st !== 'holding') Sfx.grab();
      if (f.od > 0 && G.frame % 2 === 0) P('flame', f.x + rand(-16, 16) * f.sc, f.y - rand(0, 20), rand(16, 28), { vx: rand(-0.4, 0.4), vy: rand(-3.4, -1.6), color: f.def.accent, size: rand(6, 12) });
      // spike into the ground or a hard ground bounce
      if (f.state === 'hitstun' && f.grounded && !m.gr && sp > 9) { fx.groundRing(f.x, f.y, '#ffffff', 110); for (let i = 0; i < 8; i++) P('streak', f.x, f.y, rand(8, 14), { vx: rand(-9, 9), vy: rand(-9, -2), color: '#fff', size: 2.5, len: 2 }); }
      m.st = f.state; m.gr = f.grounded; m.jumps = f.airJumps; m.hs = f.hitstop;
    }
    if (G.ending === 1 && !G.gameSnd) { G.gameSnd = true; Sfx.game(); }
    if (!G.ending) G.gameSnd = false;
  };
  if (window.__clash) window.__clash.step = step;

  // camera punch-in during a finisher
  const baseCam = updateCam;
  updateCam = function () {
    baseCam();
    const f = G.fin; if (!f || G.camLock) return;
    const k = Math.max(0, 1 - f.t / (f.max + 6));
    G.cam.x = lerp(G.cam.x, f.x, 0.22 * k); G.cam.y = lerp(G.cam.y, f.y, 0.22 * k); G.cam.z = Math.min(1.9, G.cam.z * (1 + 0.035 * k));
  };

  // ---- drawing
  const glow = (c, b) => { ctx.shadowColor = c; ctx.shadowBlur = b; };
  function star(x, y, r, inner, points, rot) { ctx.beginPath(); for (let i = 0; i < points * 2; i++) { const a = rot + (i * Math.PI) / points, rr = i % 2 ? r * inner : r; ctx.lineTo(x + Math.cos(a) * rr, y + Math.sin(a) * rr); } ctx.closePath(); }

  drawParts = function () {
    // soft particles first
    for (const p of G.parts) {
      const k = 1 - p.t / p.life;
      if (p.k === 'puff' || p.k === 'smoke') { ctx.globalAlpha = k * 0.42; ctx.fillStyle = p.color; ctx.beginPath(); ctx.arc(p.x, p.y, p.size * (1.5 - k * 0.7), 0, TAU); ctx.fill(); }
    }
    ctx.globalAlpha = 1; ctx.globalCompositeOperation = 'lighter';
    for (const p of G.parts) {
      const k = 1 - p.t / p.life, e = 1 - k;
      switch (p.k) {
        case 'ember': case 'spark': ctx.globalAlpha = k; ctx.fillStyle = p.color; glow(p.color, 8); ctx.beginPath(); ctx.arc(p.x, p.y, p.size * (0.5 + k * 0.6), 0, TAU); ctx.fill(); break;
        case 'streak': case 'line': { ctx.globalAlpha = k; ctx.strokeStyle = p.color; ctx.lineWidth = p.size * (0.4 + k); ctx.lineCap = 'round'; glow(p.color, 10); const L = p.len || 2.2; ctx.beginPath(); ctx.moveTo(p.x, p.y); ctx.lineTo(p.x - p.vx * L, p.y - p.vy * L); ctx.stroke(); break; }
        case 'core': case 'flash': { const r = p.size * (0.5 + e * 0.7); const g = ctx.createRadialGradient(p.x, p.y, 0, p.x, p.y, r); g.addColorStop(0, 'rgba(255,255,255,' + k + ')'); g.addColorStop(0.5, 'rgba(255,255,255,' + k * 0.5 + ')'); g.addColorStop(1, 'rgba(255,255,255,0)'); ctx.globalAlpha = 1; ctx.shadowBlur = 0; ctx.fillStyle = g; ctx.beginPath(); ctx.arc(p.x, p.y, r, 0, TAU); ctx.fill(); break; }
        case 'burst': ctx.globalAlpha = k * 0.9; ctx.fillStyle = p.color; glow(p.color, 18); star(p.x, p.y, p.size * (0.35 + e * 0.8), 0.28, p.points, p.rot + e * 0.4); ctx.fill(); break;
        case 'star4': ctx.globalAlpha = k; ctx.fillStyle = p.color; glow('#ffffff', 12); star(p.x, p.y, p.size * (0.4 + k * 0.6), 0.16, 4, p.rot * TAU + e); ctx.fill(); break;
        case 'shock': case 'ring': { ctx.globalAlpha = k * 0.9; ctx.strokeStyle = p.color; ctx.lineWidth = (p.w || 4) * k + 0.8; glow(p.color, 12); const r = p.size * (0.25 + (1 - k * k) * 1.1); ctx.beginPath(); ctx.ellipse(p.x, p.y, r, r * (p.flat || 1), 0, 0, TAU); ctx.stroke(); break; }
        case 'flame': ctx.globalAlpha = k * 0.75; ctx.fillStyle = p.color; glow(p.color, 14); ctx.beginPath(); ctx.ellipse(p.x, p.y, p.size * k * 0.6, p.size * k * 1.3, 0, 0, TAU); ctx.fill(); break;
        case 'slash': { ctx.globalAlpha = k * 0.85; glow(p.color, 16); const steps = 14; for (let i = 0; i < steps; i++) { const u0 = i / steps, u1 = (i + 1) / steps; ctx.strokeStyle = i > steps * 0.6 ? '#ffffff' : p.color; ctx.lineWidth = (p.w || 8) * u1 * (0.4 + k * 0.6); ctx.beginPath(); ctx.arc(p.x, p.y, p.r, p.a0 + (p.a1 - p.a0) * u0, p.a0 + (p.a1 - p.a0) * u1, p.a1 < p.a0); ctx.stroke(); } break; }
        case 'bolt': { ctx.globalAlpha = k; ctx.strokeStyle = p.color; ctx.lineWidth = 2 + k * 4; ctx.lineJoin = 'miter'; glow(p.color, 16); ctx.beginPath(); ctx.moveTo(p.x, p.y); let s = p.seed; const rr = () => ((s = (s * 9301 + 49297) % 233280) / 233280 - 0.5); for (let i = 1; i <= 7; i++) { const d = (p.len * i) / 7, off = rr() * 46; ctx.lineTo(p.x + Math.cos(p.ang) * d - Math.sin(p.ang) * off, p.y + Math.sin(p.ang) * d + Math.cos(p.ang) * off); } ctx.stroke(); break; }
      }
    }
    ctx.globalCompositeOperation = 'source-over'; ctx.shadowBlur = 0;
    for (const p of G.parts) if (p.k === 'confetti') { const k = 1 - p.t / p.life; ctx.globalAlpha = Math.min(1, k * 2); ctx.fillStyle = p.color; ctx.save(); ctx.translate(p.x, p.y); ctx.rotate(p.rot); ctx.fillRect(-p.size / 2, -p.size / 4, p.size, p.size / 2 * Math.abs(Math.cos(p.t * 0.3)) + 1); ctx.restore(); }
    ctx.globalAlpha = 1;
  };

  updateParts = function () {
    for (const p of G.parts) {
      p.t++; if (p.vx == null) continue;
      p.x += p.vx; p.y += p.vy;
      if (p.k === 'streak' || p.k === 'line') { p.vx *= 0.86; p.vy *= 0.86; }
      else if (p.k === 'ember' || p.k === 'spark' || p.k === 'confetti') { const d = p.drag || 0.93; p.vx *= d; p.vy = p.vy * d + (p.grav == null ? 0.15 : p.grav); if (p.vr) p.rot += p.vr; }
      else if (p.k === 'puff') { const d = p.drag || 0.96; p.vx *= d; p.vy *= 0.97; }
    }
    G.parts = G.parts.filter(p => p.t < p.life);
  };

  drawKOBeams = function () {
    // finisher backdrop (screen space, over the 3D scene)
    const f = G.fin;
    if (f) {
      const k = f.t < f.max ? 1 : Math.max(0, 1 - (f.t - f.max) / 10), c = w2s(f.x, f.y);
      ctx.fillStyle = 'rgba(6,4,18,' + 0.5 * k + ')'; ctx.fillRect(0, 0, VW, VH);
      ctx.save(); ctx.translate(c.x, c.y); ctx.globalCompositeOperation = 'lighter';
      let s = f.seed; const rr = () => (s = (s * 9301 + 49297) % 233280) / 233280;
      const R = Math.hypot(VW, VH);
      for (let i = 0; i < 46; i++) {
        const a = rr() * TAU + G.frame * 0.002, r0 = 120 + rr() * 260 + ((G.frame * 30 + i * 53) % 200), wdt = 0.006 + rr() * 0.02;
        ctx.fillStyle = i % 4 === 0 ? f.color : '#ffffff'; ctx.globalAlpha = k * (0.25 + rr() * 0.5);
        ctx.beginPath(); ctx.moveTo(Math.cos(a) * r0, Math.sin(a) * r0); ctx.lineTo(Math.cos(a - wdt) * R, Math.sin(a - wdt) * R); ctx.lineTo(Math.cos(a + wdt) * R, Math.sin(a + wdt) * R); ctx.fill();
      }
      ctx.restore(); ctx.globalAlpha = 1; ctx.globalCompositeOperation = 'source-over';
    }
    for (const p of G.parts) {
      if (p.k !== 'ko') continue;
      const k = 1 - p.t / p.life, sp = w2s(p.x, p.y), ex = clamp(sp.x, 0, VW), ey = clamp(sp.y, 0, VH), len = Math.max(VW, VH) * (0.5 + (1 - k) * 0.9);
      ctx.save(); ctx.translate(ex, ey); ctx.rotate(p.ang); ctx.globalCompositeOperation = 'lighter';
      for (let L = 0; L < 3; L++) {
        const wdt = (150 - L * 50) * k + 8, g = ctx.createLinearGradient(0, 0, len, 0);
        g.addColorStop(0, L === 2 ? '#ffffff' : p.color); g.addColorStop(1, 'rgba(255,255,255,0)');
        ctx.globalAlpha = k * (0.55 + L * 0.2); ctx.fillStyle = g;
        ctx.beginPath(); ctx.moveTo(0, -wdt * 0.25); ctx.lineTo(len, -wdt * (1.2 - L * 0.3)); ctx.lineTo(len, wdt * (1.2 - L * 0.3)); ctx.lineTo(0, wdt * 0.25); ctx.fill();
      }
      // rays fanning out of the blast
      for (let i = 0; i < 9; i++) { const a = (i - 4) * 0.16 + Math.sin(p.t * 0.4 + i) * 0.03; ctx.globalAlpha = k * 0.35; ctx.fillStyle = i % 2 ? '#ffffff' : p.color; ctx.beginPath(); ctx.moveTo(0, 0); ctx.lineTo(Math.cos(a - 0.02) * len * 1.2, Math.sin(a - 0.02) * len * 1.2); ctx.lineTo(Math.cos(a + 0.02) * len * 1.2, Math.sin(a + 0.02) * len * 1.2); ctx.fill(); }
      ctx.restore(); ctx.globalAlpha = 1; ctx.globalCompositeOperation = 'source-over';
    }
  };

  // ---- per-fighter overlay: launch ribbon, swing arcs, shield, rings, tag
  const swing = new WeakMap();
  drawFighterFX = function (f) {
    if (f.state === 'dead' || f.state === 'out' || f.hidden) return;
    const d = f.def, sc = f.sc, m = mem.get(f), pc = PORT_COLORS[f.port];
    // launch ribbon
    if (m && m.trail.length > 2) {
      ctx.globalCompositeOperation = 'lighter'; ctx.lineCap = 'round'; ctx.lineJoin = 'round';
      for (let pass = 0; pass < 2; pass++) for (let i = 1; i < m.trail.length; i++) {
        const a = m.trail[i - 1], b = m.trail[i], u = i / m.trail.length;
        ctx.globalAlpha = u * (pass ? 0.9 : 0.5); ctx.strokeStyle = pass ? '#ffffff' : pc; ctx.lineWidth = b.w * u * (pass ? 0.35 : 1.2);
        ctx.beginPath(); ctx.moveTo(a.x, a.y); ctx.lineTo(b.x, b.y); ctx.stroke();
      }
      ctx.globalCompositeOperation = 'source-over'; ctx.globalAlpha = 1;
    }
    // swing arcs: a tapered ribbon that follows each active hitbox
    let sw = swing.get(f); if (!sw) { sw = { key: null, pts: [] }; swing.set(f, sw); }
    const hbs = activeHBsForDraw(f);
    const key = f.state === 'attack' && f.move ? f.move.name + ':' + (f.hitIds ? 1 : 0) : null;
    if (key !== sw.key) { sw.key = key; sw.pts = []; }
    if (hbs.length) { const w = hbWorld(f, hbs[0]); if (f.hitstop <= 0) { sw.pts.push({ x: w.x, y: w.y, r: w.r, a: Math.atan2(w.y - (f.y - f.h * 0.5), w.x - f.x) }); if (sw.pts.length > 7) sw.pts.shift(); } }
    else if (sw.pts.length) sw.pts.shift();
    if (sw.pts.length) {
      ctx.globalCompositeOperation = 'lighter';
      const last = sw.pts[sw.pts.length - 1], sweep = 0.9 + sw.pts.length * 0.16, dir = f.facing;
      for (let i = 0; i < 12; i++) {
        const u0 = i / 12, u1 = (i + 1) / 12, a0 = last.a - dir * sweep * (1 - u0), a1 = last.a - dir * sweep * (1 - u1);
        const R = Math.hypot(last.x - f.x, last.y - (f.y - f.h * 0.5)) + last.r * 0.5;
        ctx.globalAlpha = u1 * 0.8; ctx.strokeStyle = i > 8 ? '#ffffff' : d.accent; ctx.lineWidth = last.r * 0.9 * u1; ctx.shadowColor = d.accent; ctx.shadowBlur = 14;
        ctx.beginPath(); ctx.arc(f.x, f.y - f.h * 0.5, Math.max(R, 14), Math.min(a0, a1), Math.max(a0, a1)); ctx.stroke();
      }
      const g = ctx.createRadialGradient(last.x, last.y, 0, last.x, last.y, last.r * 1.2); g.addColorStop(0, 'rgba(255,255,255,0.75)'); g.addColorStop(1, 'rgba(255,255,255,0)');
      ctx.globalAlpha = 1; ctx.shadowBlur = 0; ctx.fillStyle = g; ctx.beginPath(); ctx.arc(last.x, last.y, last.r * 1.2, 0, TAU); ctx.fill();
      ctx.globalCompositeOperation = 'source-over';
    }
    // shield: glassy bubble with a moving highlight
    if (f.state === 'shield' || f.state === 'shieldstun') {
      const r = 46 * sc * (0.35 + 0.65 * f.shieldHP / 50), cx = f.x, cy = f.y - f.h * 0.5;
      const g = ctx.createRadialGradient(cx - r * 0.3, cy - r * 0.35, r * 0.1, cx, cy, r); g.addColorStop(0, 'rgba(255,255,255,0.55)'); g.addColorStop(0.45, pc + '55'); g.addColorStop(1, pc + 'aa');
      ctx.fillStyle = g; ctx.beginPath(); ctx.arc(cx, cy, r, 0, TAU); ctx.fill();
      ctx.strokeStyle = '#ffffff'; ctx.globalAlpha = 0.85; ctx.lineWidth = 2.5; ctx.shadowColor = pc; ctx.shadowBlur = 16; ctx.stroke();
      ctx.globalAlpha = 0.5; ctx.lineWidth = 1.5; const a = G.frame * 0.07; ctx.beginPath(); ctx.arc(cx, cy, r * 0.8, a, a + 1.1); ctx.stroke(); ctx.beginPath(); ctx.arc(cx, cy, r * 0.62, a + 2.6, a + 3.4); ctx.stroke();
      ctx.globalAlpha = 1; ctx.shadowBlur = 0;
    }
    if (f.state === 'dizzy') { ctx.fillStyle = '#ffe14d'; for (let k = 0; k < 4; k++) { const aa = G.frame * 0.11 + k * 1.57; star(f.x + Math.cos(aa) * 20, f.y - f.h - 8 + Math.sin(aa) * 5, 6, 0.4, 5, aa); ctx.fill(); } }
    if (f.counter) { ctx.strokeStyle = '#ffffff'; ctx.shadowColor = '#fff'; ctx.shadowBlur = 14; ctx.globalAlpha = 0.6 + 0.4 * Math.sin(G.frame); ctx.lineWidth = 3; ctx.beginPath(); ctx.arc(f.x, f.y - f.h / 2, f.h * 0.6, 0, TAU); ctx.stroke(); ctx.globalAlpha = 1; ctx.shadowBlur = 0; }
    if (f.charge > 0 && f.state === 'attack') { // gathering energy: rings closing in
      const c = Math.min(1, f.charge / 60); ctx.globalCompositeOperation = 'lighter'; ctx.strokeStyle = d.accent; ctx.shadowColor = d.accent; ctx.shadowBlur = 12;
      for (let k = 0; k < 3; k++) { const u = ((G.frame * 0.05 + k / 3) % 1); ctx.globalAlpha = u * (0.4 + c * 0.5); ctx.lineWidth = 2 + c * 3; ctx.beginPath(); ctx.arc(f.x, f.y - f.h / 2, f.h * (1.1 - u * 0.8), 0, TAU); ctx.stroke(); }
      ctx.globalCompositeOperation = 'source-over'; ctx.globalAlpha = 1; ctx.shadowBlur = 0;
      if (G.frame % 3 === 0) { const a = rand(0, TAU), r = f.h * 0.9; P('streak', f.x + Math.cos(a) * r, f.y - f.h / 2 + Math.sin(a) * r, 9, { vx: -Math.cos(a) * 8, vy: -Math.sin(a) * 8, color: d.accent, size: 2, len: 1.5 }); }
    }
    if (f.invinc > 0 && f.state !== 'respawn') { ctx.globalCompositeOperation = 'lighter'; ctx.globalAlpha = 0.25 + 0.2 * Math.sin(G.frame * 0.5); ctx.strokeStyle = '#ffffff'; ctx.lineWidth = 3; ctx.beginPath(); ctx.ellipse(f.x, f.y - f.h * 0.5, f.w * 0.9, f.h * 0.62, 0, 0, TAU); ctx.stroke(); ctx.globalCompositeOperation = 'source-over'; ctx.globalAlpha = 1; }
    // player tag
    ctx.fillStyle = pc; ctx.font = 'bold 15px Trebuchet MS, sans-serif'; ctx.textAlign = 'center';
    const ty = f.y - f.h - 26, tag = f.cpu ? 'CPU' : 'P' + (f.port + 1);
    ctx.lineWidth = 3; ctx.strokeStyle = 'rgba(0,0,0,.6)'; ctx.strokeText(tag, f.x, ty); ctx.fillText(tag, f.x, ty);
    ctx.beginPath(); ctx.moveTo(f.x - 6, ty + 4); ctx.lineTo(f.x + 6, ty + 4); ctx.lineTo(f.x, ty + 11); ctx.fill();
  };

  // projectiles get a glow trail
  const baseProj = drawProj;
  drawProj = function (p) {
    if (!p.invisible && G.frame % 2 === 0 && !G.paused) P('ember', p.x, p.y, 12, { vx: -p.vx * 0.1 + rand(-0.6, 0.6), vy: rand(-0.6, 0.6), color: p.owner.def.accent, size: Math.max(2, p.r * 0.3), grav: 0 });
    ctx.globalCompositeOperation = 'lighter'; baseProj(p); ctx.globalCompositeOperation = 'source-over'; baseProj(p);
  };
})();
