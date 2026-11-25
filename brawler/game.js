'use strict';
// Super Family Smash — original platform-fighter prototype. All art is drawn in code; all sound is synthesized.

// ---------------------------------------------------------------- utils
const clamp = (v, a, b) => (v < a ? a : v > b ? b : v);
const lerp = (a, b, t) => a + (b - a) * t;
const sgn = v => (v < 0 ? -1 : v > 0 ? 1 : 0);
const rand = (a, b) => a + Math.random() * (b - a);
const DEG = Math.PI / 180;
const BTNS = ['jump', 'attack', 'special', 'shield', 'grab', 'smash'];
const BUF = 6; // input buffer frames
const PORT_COLORS = ['#ff4d4d', '#4d8dff', '#ffcf3f', '#4dd97a'];

// ---------------------------------------------------------------- audio (synth only)
const Sfx = {
  ctx: null, noiseBuf: null, master: null,
  init() {
    if (!this.ctx) {
      try {
        this.ctx = new (window.AudioContext || window.webkitAudioContext)();
        this.master = this.ctx.createGain(); this.master.gain.value = 0.5; this.master.connect(this.ctx.destination);
        const n = this.ctx.sampleRate * 0.6, b = this.ctx.createBuffer(1, n, this.ctx.sampleRate), d = b.getChannelData(0);
        for (let i = 0; i < n; i++) d[i] = Math.random() * 2 - 1;
        this.noiseBuf = b;
      } catch (e) { this.ctx = null; }
    }
    if (this.ctx && this.ctx.state === 'suspended') this.ctx.resume();
  },
  tone(f0, dur, type = 'square', vol = 0.12, f1 = null) {
    const c = this.ctx; if (!c) return; const t = c.currentTime;
    const o = c.createOscillator(), g = c.createGain(); o.type = type; o.frequency.setValueAtTime(f0, t);
    if (f1) o.frequency.exponentialRampToValueAtTime(Math.max(20, f1), t + dur);
    g.gain.setValueAtTime(vol, t); g.gain.exponentialRampToValueAtTime(0.001, t + dur);
    o.connect(g); g.connect(this.master); o.start(t); o.stop(t + dur + 0.02);
  },
  noise(dur, vol, fc = 1200, q = 0.8, fc1 = null) {
    const c = this.ctx; if (!c) return; const t = c.currentTime;
    const s = c.createBufferSource(); s.buffer = this.noiseBuf;
    const fl = c.createBiquadFilter(); fl.type = 'bandpass'; fl.frequency.setValueAtTime(fc, t); fl.Q.value = q;
    if (fc1) fl.frequency.exponentialRampToValueAtTime(fc1, t + dur);
    const g = c.createGain(); g.gain.setValueAtTime(vol, t); g.gain.exponentialRampToValueAtTime(0.001, t + dur);
    s.connect(fl); fl.connect(g); g.connect(this.master); s.start(t); s.stop(t + dur + 0.02);
  },
  hit(d) { const k = Math.min(d, 28); this.noise(0.07 + k * 0.006, 0.25 + k * 0.012, 700 + k * 50, 0.7); this.tone(160 - k * 2, 0.09 + k * 0.006, 'sine', 0.35, 50); },
  bigHit() { this.noise(0.5, 0.5, 2500, 0.5, 200); this.tone(90, 0.4, 'sawtooth', 0.2, 30); },
  swing() { this.noise(0.09, 0.08, 2600, 1.5, 900); },
  jump() { this.tone(300, 0.08, 'square', 0.05, 520); },
  land() { this.noise(0.05, 0.08, 400, 1); },
  shield() { this.tone(900, 0.07, 'triangle', 0.1, 700); },
  parry() { this.tone(1400, 0.25, 'triangle', 0.18, 2100); this.tone(2100, 0.2, 'sine', 0.1); },
  shieldBreak() { this.tone(600, 0.6, 'sawtooth', 0.15, 80); this.noise(0.4, 0.3, 3000, 0.5, 300); },
  zap() { this.tone(1200, 0.12, 'sawtooth', 0.06, 300); },
  boom() { this.noise(0.9, 0.6, 600, 0.4, 60); this.tone(70, 0.8, 'sine', 0.5, 25); },
  grab() { this.tone(220, 0.06, 'square', 0.08, 180); },
  blip() { this.tone(660, 0.05, 'square', 0.06); },
  go() { this.tone(523, 0.12, 'square', 0.1); setTimeout(() => this.tone(784, 0.3, 'square', 0.1), 110); },
  count() { this.tone(392, 0.15, 'square', 0.08); },
  od() { this.tone(200, 0.7, 'sawtooth', 0.15, 900); this.noise(0.6, 0.2, 1500, 0.6, 5000); },
};

// ---------------------------------------------------------------- stages
// Original family-themed stages. Layout types are genre staples (three platforms, moving platforms, flat, staircase);
// the layouts, names and art are this game's own. The ids are kept from the earlier prototype stages.
const STAGES = {
  skyforge: {
    name: 'Treehouse', theme: 'treehouse', main: { x1: -330, x2: 330, y: 0, depth: 150 },
    plats: [{ x1: -270, x2: -90, y: -150 }, { x1: 90, x2: 270, y: -150 }, { x1: -90, x2: 90, y: -295 }],
    blast: { l: -1150, r: 1150, t: -1000, b: 700 },
    sky: ['#4aa8ff', '#9fd8ff', '#e8f7ff'], ground: '#7a5230', top: '#c8935a', trim: '#f2d19a',
  },
  driftway: {
    name: 'Kitchen Table', theme: 'kitchen', main: { x1: -370, x2: 370, y: 0, depth: 60 },
    plats: [
      { x1: -90, x2: 90, y: -175, move: t => ({ dx: Math.sin(t * 0.011) * 250, dy: 0 }) },
      { x1: -70, x2: 70, y: -330, move: t => ({ dx: Math.sin(t * 0.011 + Math.PI) * 180, dy: Math.cos(t * 0.022) * 30 }) },
    ],
    blast: { l: -1200, r: 1200, t: -1050, b: 700 },
    sky: ['#ffe9c4', '#ffd49a', '#f2b27a'], ground: '#8a5a34', top: '#d9a066', trim: '#fff1d6',
  },
  proving: {
    name: 'Driveway', theme: 'driveway', main: { x1: -400, x2: 400, y: 0, depth: 110 }, plats: [],
    blast: { l: -1150, r: 1150, t: -950, b: 680 },
    sky: ['#ff9a5a', '#ffc78a', '#ffe9c0'], ground: '#4a4d57', top: '#7c808c', trim: '#f4f4f4',
  },
  bunk: {
    name: 'Bunk Beds', theme: 'bedroom', main: { x1: -320, x2: 320, y: 0, depth: 130 },
    plats: [{ x1: -300, x2: -140, y: -125 }, { x1: -70, x2: 110, y: -245 }, { x1: 170, x2: 310, y: -165 }],
    blast: { l: -1150, r: 1150, t: -1000, b: 700 },
    sky: ['#0d1238', '#1d2466', '#3a2f7a'], ground: '#4a3a8a', top: '#8f7bff', trim: '#ffe9a8',
  },
};

// ---------------------------------------------------------------- poses
// [lean, frontArmShoulder, frontArmElbow, backArmShoulder, backArmElbow, frontHip, frontKnee, backHip, backKnee]
// angle 0 = straight down, +PI/2 = forward, PI = up.
const P = (...a) => a;
const POSES = {
  idle: P(0.08, 0.5, 0.9, -0.35, 0.8, 0.22, -0.25, -0.18, -0.15),
  crouch: P(0.45, 0.9, 0.9, 0.2, 0.9, 1.1, -1.9, 0.6, -1.6),
  jumpUp: P(-0.05, 2.4, 0.4, -2.2, -0.3, 0.9, -1.4, -0.1, -0.6),
  fall: P(0.0, 1.9, 0.5, -1.9, -0.5, 0.4, -0.7, -0.2, -0.3),
  jab: P(0.15, 1.57, 0.0, 0.2, 1.6, 0.35, -0.3, -0.3, -0.1),
  jab2: P(0.2, 0.6, 1.6, 1.57, 0.0, 0.35, -0.3, -0.3, -0.1),
  kick: P(-0.35, 0.3, 1.2, -0.9, 0.8, 1.55, 0.0, -0.15, -0.1),
  upper: P(-0.15, 2.95, 0.15, -0.4, 0.8, 0.3, -0.3, -0.25, -0.2),
  sweep: P(0.55, 1.2, 0.5, -0.4, 0.6, 1.6, 0.0, 0.5, -1.8),
  dashkick: P(-0.5, 0.5, 1.0, -1.2, 0.4, 1.3, 0.0, -0.6, -0.6),
  windF: P(-0.25, -1.2, -0.6, -1.4, -0.4, 0.5, -0.6, -0.5, -0.2),
  smashF: P(0.45, 1.6, 0.0, 1.45, 0.15, 0.7, -0.4, -0.8, 0.0),
  windU: P(0.3, 0.3, 1.5, 0.0, 1.4, 0.9, -1.5, 0.4, -1.2),
  smashU: P(-0.25, 3.1, 0.0, 2.95, 0.1, 0.2, -0.2, -0.3, -0.1),
  windD: P(0.4, 1.0, 1.2, -0.5, 1.0, 1.0, -1.8, 0.6, -1.6),
  smashD: P(0.5, 1.9, 0.3, -1.9, -0.3, 1.65, 0.0, -1.65, 0.0),
  nair: P(0, 1.6, 0, -1.6, 0, 1.3, 0, -1.3, 0),
  windFair: P(-0.3, 3.0, 0.2, -0.6, 0.6, 0.6, -1.0, -0.2, -0.6),
  fair: P(0.35, 1.2, 0.0, -1.0, 0.5, 0.7, -0.8, -0.4, -0.5),
  bair: P(0.45, 0.8, 1.0, 0.3, 1.0, 0.2, -0.6, -1.65, 0.0),
  uair: P(-0.7, 2.6, 0.0, -2.4, 0.0, 2.8, 0.0, 0.3, -0.9),
  windDair: P(0.0, 2.0, 0.6, -2.0, -0.6, 1.1, -1.8, 0.8, -1.8),
  dair: P(0.0, 2.6, 0.3, -2.6, -0.3, 0.12, 0.0, -0.12, 0.0),
  grab: P(0.3, 1.55, 0.0, 1.45, 0.1, 0.45, -0.4, -0.35, -0.1),
  throw: P(-0.3, 2.8, 0.1, 2.6, 0.2, 0.3, -0.3, -0.3, -0.1),
  cast: P(0.1, 1.57, 0.0, 1.5, 0.05, 0.3, -0.3, -0.3, -0.1),
  counter: P(-0.2, 0.9, 1.6, 0.8, 1.5, 0.3, -0.5, -0.4, -0.3),
  shield: P(0.25, 0.9, 1.8, 0.7, 1.8, 0.6, -1.0, -0.2, -0.8),
  hurt: P(-0.5, 2.6, 0.5, -2.6, -0.5, 0.6, -0.4, -0.3, -0.6),
  ledge: P(0.0, 3.0, 0.0, 2.9, 0.0, 0.15, -0.3, -0.1, -0.2),
  helpless: P(0.1, 2.8, 0.2, -2.8, -0.2, 0.3, -0.9, -0.3, -0.5),
  tuck: P(0.6, 1.3, 1.6, 1.0, 1.6, 1.6, -2.4, 1.4, -2.4),
  spin: P(-0.1, 3.05, 0, 1.0, 1.2, 0.3, -0.6, -0.2, -0.3),
  dive: P(0.6, -0.5, 0.4, -0.9, 0.4, 0.9, 0.0, 0.3, -1.2),
};
const lerpPose = (a, b, t) => a.map((v, i) => v + (b[i] - v) * t);

// ---------------------------------------------------------------- moves
// H(startFrame, endFrame, x, y, radius, damage, angle, baseKB, KBgrowth, extra)
const H = (s, e, x, y, r, d, a, b, g, ex) => Object.assign({ f: [s, e], x, y, r, d, a, b, g }, ex || {});
const M = o => Object.assign({ len: 30, hb: [], pose: 'jab', air: false, ll: 0 }, o);

function baseMoves() {
  return {
    jab1: M({ len: 16, pose: 'jab', next: 'jab2', win: 6, hb: [H(3, 4, 26, -46, 12, 2.5, 361, 10, 30)] }),
    jab2: M({ len: 16, pose: 'jab2', next: 'jab3', win: 6, hb: [H(3, 4, 28, -44, 12, 2.5, 361, 10, 30)] }),
    jab3: M({ len: 30, pose: 'kick', hb: [H(5, 7, 32, -36, 15, 5, 40, 45, 100)] }),
    ftilt: M({ len: 28, pose: 'kick', hb: [H(6, 8, 36, -34, 15, 9, 361, 22, 95)] }),
    utilt: M({ len: 30, pose: 'upper', hb: [H(5, 10, 8, -80, 17, 7, 88, 40, 115)] }),
    dtilt: M({ len: 22, pose: 'sweep', crouch: true, hb: [H(5, 7, 36, -8, 13, 6, 75, 45, 40)] }),
    dash: M({
      len: 38, pose: 'dashkick', hb: [H(6, 11, 30, -30, 17, 10, 55, 50, 70)],
      onFrame: (f, t) => { if (t < 12) f.vx = f.facing * f.def.run * 0.95; else f.vx *= 0.85; },
    }),
    fsmash: M({
      len: 52, pose: 'smashF', wind: 'windF', charge: 9, smash: true, hb: [H(13, 15, 44, -40, 19, 16, 361, 32, 100)],
      onFrame: (f, t) => { if (t === 12) f.vx = f.facing * 5; },
    }),
    usmash: M({ len: 46, pose: 'smashU', wind: 'windU', charge: 7, smash: true, hb: [H(11, 16, 2, -86, 22, 15, 88, 34, 98)] }),
    dsmash: M({
      len: 48, pose: 'smashD', wind: 'windD', charge: 5, smash: true, crouch: true,
      hb: [H(10, 12, 36, -8, 17, 14, 32, 30, 95), H(10, 12, -36, -8, 17, 14, 148, 30, 95)],
    }),
    nair: M({ len: 34, air: true, ll: 6, pose: 'nair', hb: [H(3, 7, 0, -38, 28, 9, 361, 20, 90), H(8, 20, 0, -38, 26, 5, 361, 10, 80)] }),
    fair: M({ len: 40, air: true, ll: 12, pose: 'fair', wind: 'windFair', hb: [H(10, 13, 34, -38, 17, 13, 361, 30, 98)] }),
    bair: M({ len: 32, air: true, ll: 9, pose: 'bair', hb: [H(7, 10, -36, -34, 17, 12, 140, 22, 100)] }),
    uair: M({ len: 30, air: true, ll: 7, pose: 'uair', hb: [H(5, 9, 10, -82, 19, 8, 82, 30, 110)] }),
    dair: M({ len: 44, air: true, ll: 16, pose: 'dair', wind: 'windDair', hb: [H(14, 17, 0, 6, 19, 13, 270, 20, 90)] }),
    grab: M({ len: 34, pose: 'grab', hb: [H(6, 7, 32, -38, 20, 0, 0, 0, 0, { grab: true })] }),
    dashgrab: M({ len: 40, pose: 'grab', hb: [H(8, 9, 40, -38, 22, 0, 0, 0, 0, { grab: true })], onFrame: f => { f.vx *= 0.9; } }),
    ledgeAtk: M({ len: 40, pose: 'sweep', hb: [H(18, 21, 36, -12, 20, 9, 45, 50, 40)], intang: 18 }),
    getupAtk: M({ len: 36, pose: 'smashD', hb: [H(12, 15, 36, -10, 18, 7, 361, 50, 40), H(12, 15, -36, -10, 18, 7, 140, 50, 40)], intang: 14 }),
    counterStrike: M({ len: 32, pose: 'smashF', counterStrike: true }),
    throw: M({ len: 18, pose: 'throw' }),
  };
}

// --- specials
function spec(kind) {
  switch (kind) {
    case 'blaster': return M({
      len: 26, pose: 'cast', special: true, drift: 0.6, landLag: 4,
      onFrame: (f, t) => {
        if (!f.grounded && t < 12) f.vy = Math.min(f.vy, 1.5);
        if (t === 8) { spawnProj(f, { type: 'bolt', x: 30, y: -46, vx: 12.5, r: 8, d: 3.5, a: 361, b: 8, g: 20, life: 55 }); Sfx.zap(); }
      },
    });
    case 'chargeShot': return M({
      len: 30, pose: 'cast', special: true, drift: 0.4, landLag: 4,
      onFrame: (f, t, inp) => {
        if (t === 4 && f.charge < 100 && inp.special) {
          f.charge = Math.min(100, f.charge + 1.25); if (!f.grounded) f.vy = Math.min(f.vy, 1);
          if (G.frame % 4 === 0) fx.spark(f.x + f.facing * 30 * f.sc, f.y - 46 * f.sc, f.def.accent, 2, 3);
          if (f.buf.shield) { f.buf.shield = 0; endMove(f); f.storedCharge = true; } // store the charge
          return 'hold';
        }
        if (t === 8) {
          const c = f.charge / 100;
          spawnProj(f, { type: 'orb', x: 32, y: -46, vx: 8 + c * 3, r: 8 + c * 16, d: 4 + c * 20, a: 361, b: 18 + c * 37, g: 40 + c * 45, life: 100 });
          f.charge = 0; f.storedCharge = false; Sfx.zap(); if (c > 0.8) Sfx.tone(180, 0.4, 'sawtooth', 0.15, 60);
        }
      },
    });
    case 'chargePunch': return M({
      len: 44, pose: 'smashF', wind: 'windF', special: true, drift: 0.4, landLag: 8,
      onFrame: (f, t, inp) => {
        if (t === 6 && f.charge < 90 && inp.special) {
          f.charge++; if (!f.grounded) f.vy = Math.min(f.vy, 1);
          if (G.frame % 3 === 0) fx.spark(f.x - f.facing * 20, f.y - 40 * f.sc, '#ff8a3d', 2, 3);
          return 'hold';
        }
        if (t === 12) {
          const c = f.charge / 90; f.vx = f.facing * (6 + c * 8);
          f.xhb = [H(13, 17, 40, -40, 22 + c * 6, 8 + c * 20, 361, 40 + c * 20, 85 + c * 15)];
          f.charge = 0; Sfx.swing();
        }
        if (t > 17) f.vx *= 0.85;
      },
    });
    case 'waveSlash': return M({
      len: 36, pose: 'smashF', wind: 'windF', special: true, drift: 0.5, landLag: 6,
      onFrame: (f, t) => {
        if (!f.grounded && t < 14) f.vy = Math.min(f.vy, 1.5);
        if (t === 12) { spawnProj(f, { type: 'wave', x: 30, y: -24, vx: 9, r: 18, d: 7, a: 50, b: 45, g: 50, life: 45 }); Sfx.swing(); }
      },
    });
    case 'dashStrike': case 'shoulder': {
      const heavy = kind === 'shoulder';
      return M({
        len: 40, pose: heavy ? 'smashF' : 'dashkick', special: true, landLag: 10, noLandCancel: true,
        hb: [H(6, 19, 22, -36, 20, heavy ? 12 : 9, 40, heavy ? 60 : 45, heavy ? 70 : 65)],
        onStart: f => { f.dashUsedNow = !f.grounded && f.sideB; if (!f.grounded) f.sideB = true; },
        onFrame: (f, t) => {
          if (heavy) f.armor = t >= 4 && t <= 20 ? 1 : 0;
          if (t < 6) { f.vx *= 0.5; if (!f.grounded && !f.dashUsedNow) f.vy = 0; }
          else if (t < 20) { f.vx = f.facing * (heavy ? 10.5 : 13); if (!f.grounded && !f.dashUsedNow) f.vy = 0; if (t % 3 === 0) fx.smoke(f.x, f.y - 20, 1); }
          else f.vx *= 0.8;
        },
      });
    }
    case 'bomb': return M({
      len: 32, pose: 'throw', special: true, drift: 0.6, landLag: 4,
      onFrame: (f, t) => { if (t === 10) { spawnProj(f, { type: 'bomb', x: 20, y: -60, vx: 6, vy: -7, grav: 0.35, r: 11, d: 3, a: 70, b: 20, g: 20, life: 110, explode: { r: 58, d: 11, a: 70, b: 45, g: 70 } }); Sfx.swing(); } },
    });
    case 'rocket': case 'rocketHeavy': case 'spiral': case 'hop': {
      const pw = kind === 'rocketHeavy' ? 14.8 : kind === 'spiral' ? 15.5 : kind === 'hop' ? 12.5 : 16.5;
      const hx = kind === 'spiral' ? 5 : 3.5;
      const multi = kind === 'spiral'
        ? [H(5, 22, 26, -44, 22, 1.8, 85, 35, 15, { rehit: 4 }), H(5, 22, -26, -44, 22, 1.8, 95, 35, 15, { rehit: 4 }), H(23, 26, 0, -70, 30, 5, 70, 60, 90)]
        : [H(5, 22, 10, -44, 24, kind === 'rocketHeavy' ? 2.6 : 2, 85, 35, 15, { rehit: 4 }), H(23, 26, 10, -60, 28, 5, 70, 55, 90)];
      return M({
        len: 44, pose: 'spin', special: true, recovery: true, landLag: 18, hb: multi, ledgeFrom: 22, noGravUntil: 0,
        onStart: f => { f.upBUsed = true; },
        onFrame: (f, t, inp) => {
          if (t < 5) { f.vx *= 0.5; f.vy = 0; if (Math.abs(inp.x) > 0.3) f.facing = sgn(inp.x); }
          if (t === 5) { f.vy = -pw; f.vx = inp.x * hx; f.grounded = false; f.ground = null; Sfx.tone(200, 0.35, 'sawtooth', 0.08, 900); }
          if (t > 5 && t < 28 && t % 2 === 0) fx.smoke(f.x, f.y, 1, f.def.accent);
          if (t > 5) f.vx = clamp(f.vx + inp.x * 0.3, -hx, hx);
        },
        onEnd: f => { if (!f.grounded) setState(f, 'helpless'); },
      });
    }
    case 'teleport': return M({
      len: 42, pose: 'cast', special: true, recovery: true, landLag: 14, ledgeFrom: 23, noGrav: true,
      onStart: f => { f.upBUsed = true; },
      onFrame: (f, t, inp) => {
        if (t < 12) { f.vx *= 0.6; f.vy = 0; if (t % 2 === 0) fx.spark(f.x, f.y - 40, f.def.accent, 2, 4); }
        if (t === 12) {
          let dx = inp.x, dy = inp.y; const m = Math.hypot(dx, dy);
          if (m < 0.3) { dx = 0; dy = -1; } else { dx /= m; dy /= m; }
          f.tp = { dx, dy }; f.hidden = true; f.intang = Math.max(f.intang, 12); Sfx.tone(800, 0.2, 'sine', 0.1, 1800);
          if (dx) f.facing = sgn(dx);
          if (dy < 0) { f.grounded = false; f.ground = null; }
        }
        if (t >= 12 && t < 22) { f.vx = f.tp.dx * 23; f.vy = f.tp.dy * 23; }
        if (t === 22) { f.hidden = false; f.vx = f.tp.dx * 3; f.vy = f.tp.dy * 3; fx.spark(f.x, f.y - 40, f.def.accent, 10, 6); }
        if (t > 22) f.vy = Math.min(f.vy + f.def.grav, f.def.fall);
      },
      onEnd: f => { f.hidden = false; if (!f.grounded) setState(f, 'helpless'); },
    });
    case 'diveKick': return M({
      len: 90, pose: 'dive', special: true, landLag: 12, noGrav: true,
      hb: [H(10, 89, 18, -4, 17, 9, 300, 25, 80)],
      onFrame: (f, t) => {
        if (t === 0 && f.grounded) { f.vy = -9; f.grounded = false; f.ground = null; }
        if (t < 10) { f.vy = f.vy * 0.7; f.vx *= 0.7; }
        else { f.vx = f.facing * 7.5; f.vy = 14; if (t % 3 === 0) fx.smoke(f.x, f.y - 30, 1, f.def.accent); }
      },
      onLand: f => { spawnProj(f, { type: 'blast', x: 0, y: -10, vx: 0, r: 44, d: 5, a: 70, b: 50, g: 40, life: 5, invisible: true }); fx.smoke(f.x, f.y, 8); G.shake = Math.max(G.shake, 5); },
      onEnd: f => { if (!f.grounded) setState(f, 'air'); },
    });
    case 'quake': return M({
      len: 44, pose: 'dair', wind: 'windDair', special: true, landLag: 22, noGrav: false,
      onStart: f => { f.quakeAir = !f.grounded; },
      onFrame: (f, t) => {
        if (f.quakeAir) {
          if (t < 10) { f.vy = Math.min(f.vy, 0) * 0.5; f.vx *= 0.7; }
          else {
            f.vy = 18; f.vx *= 0.9; if (t < 80) f.mtHold = true;
            if (!f.xhb) f.xhb = [H(10, 200, 0, 4, 22, 12, 270, 25, 85)];
          }
        } else if (t === 16) {
          spawnProj(f, { type: 'blast', x: 40, y: -12, vx: 0, r: 30, d: 14, a: 80, b: 50, g: 80, life: 4, invisible: true });
          spawnProj(f, { type: 'blast', x: -40, y: -12, vx: 0, r: 30, d: 14, a: 100, b: 50, g: 80, life: 4, invisible: true });
          fx.smoke(f.x, f.y, 10); G.shake = Math.max(G.shake, 8); Sfx.boom();
        }
      },
      onLand: f => {
        if (!f.quakeAir) return;
        spawnProj(f, { type: 'blast', x: 0, y: -14, vx: 0, r: 62, d: 12, a: 80, b: 60, g: 70, life: 5, invisible: true });
        fx.smoke(f.x, f.y, 12); G.shake = Math.max(G.shake, 10); Sfx.boom();
      },
    });
    case 'clogToss': return M({ // kicks a clog off in an arc
      len: 30, pose: 'kick', special: true, drift: 0.7, landLag: 4,
      onFrame: (f, t) => { if (t === 8) { spawnProj(f, { type: 'clog', x: 26, y: -30, vx: 8.5, vy: -5.5, grav: 0.32, r: 10, d: 6, a: 60, b: 30, g: 55, life: 80 }); Sfx.swing(); } },
    });
    case 'cannonball': return M({ // tucks into a ball and rolls through people
      len: 44, pose: 'tuck', rollSpin: true, special: true, landLag: 8, noLandCancel: true,
      hb: [H(7, 26, 6, -28, 24, 2.2, 70, 25, 20, { rehit: 6 }), H(27, 30, 10, -28, 26, 5, 45, 50, 85)],
      onStart: f => { f.dashUsedNow = !f.grounded && f.sideB; if (!f.grounded) f.sideB = true; },
      onFrame: (f, t) => {
        if (t < 7) { f.vx *= 0.6; if (!f.grounded && !f.dashUsedNow) f.vy = Math.min(f.vy, 0.5); }
        else if (t < 30) { f.vx = f.facing * 10; if (!f.grounded && !f.dashUsedNow) f.vy = Math.min(f.vy, 1.2); if (f.grounded && t % 4 === 0) fx.smoke(f.x, f.y, 1); }
        else f.vx *= 0.8;
      },
    });
    case 'starToss': return M({ // flicks a spinning glitter star
      len: 24, pose: 'cast', special: true, drift: 0.7, landLag: 4,
      onFrame: (f, t) => { if (!f.grounded && t < 10) f.vy = Math.min(f.vy, 1.5); if (t === 7) { spawnProj(f, { type: 'star', x: 28, y: -44, vx: 13.5, r: 10, d: 4, a: 361, b: 10, g: 25, life: 46 }); Sfx.zap(); } },
    });
    case 'sparkleDash': return M({ // a streak of glitter: very fast, goes a long way
      len: 36, pose: 'dashkick', special: true, landLag: 9, noLandCancel: true,
      hb: [H(5, 16, 22, -36, 21, 8, 42, 45, 68)],
      onStart: f => { f.dashUsedNow = !f.grounded && f.sideB; if (!f.grounded) f.sideB = true; },
      onFrame: (f, t) => {
        if (t < 5) { f.vx *= 0.4; if (!f.grounded && !f.dashUsedNow) f.vy = 0; }
        else if (t < 17) { f.vx = f.facing * 16.5; if (!f.grounded && !f.dashUsedNow) f.vy = 0; fx.spark(f.x - f.facing * 10, f.y - rand(10, 60), t % 2 ? '#ff6ad5' : '#a65fe0', 2, 3); }
        else f.vx *= 0.78;
      },
    });
    case 'whirl': return M({ // spins with the sword out; can drift while spinning
      len: 52, pose: 'nair', twirl: true, special: true, drift: 1, landLag: 8, noLandCancel: true,
      hb: [H(6, 38, 0, -34, 34, 1.5, 80, 30, 15, { rehit: 5 }), H(40, 43, 0, -40, 38, 4, 60, 50, 95)],
      onFrame: (f, t, inp) => { if (!f.grounded && t > 4 && t < 40) f.vy = Math.min(f.vy, inp.special ? -0.6 : 1.2); if (f.grounded && t < 40) f.vx = approach(f.vx, inp.x * 5, 0.8); },
    });
    case 'counter': return M({
      len: 50, pose: 'counter', special: true, drift: 0.3, landLag: 6,
      onFrame: (f, t) => { f.counter = t >= 5 && t <= 28; if (!f.grounded && t < 30) f.vy = Math.min(f.vy, 1); },
      onEnd: f => { f.counter = false; },
    });
  }
}

// ---------------------------------------------------------------- fighters
const FIGHTERS = [
  {
    id: 'ray', name: 'RAY', blurb: 'Small and floaty. Five jumps, clog toss, cannonball.', color: '#2f55e8', dark: '#27304a', accent: '#2fc4c0', acc: 'kid',
    weight: 76, walk: 3.6, run: 7.4, air: 4.6, airAcc: 0.5, grav: 0.33, fall: 5.4, ff: 8.6, jump: 11.0, hop: 7.0, djump: 8.6, jumps: 5,
    scale: 0.8, power: 0.9, speed: 0.92, reach: 0.95, sp: { n: 'clogToss', s: 'cannonball', u: 'hop', d: 'quake' },
  },
  {
    id: 'norah', name: 'NORAH', blurb: 'The fastest runner. Sparkle dash, star toss, dive kick.', color: '#f08bb4', dark: '#a65fe0', accent: '#ff6ad5', acc: 'kid', trail: ['#ff6ad5', '#a65fe0'],
    weight: 88, walk: 4.6, run: 11.6, air: 5.2, airAcc: 0.55, grav: 0.48, fall: 7.4, ff: 10.8, jump: 12.4, hop: 7.8, djump: 11.8, jumps: 2,
    scale: 0.93, power: 0.9, speed: 0.88, reach: 1.0, sp: { n: 'starToss', s: 'sparkleDash', u: 'rocket', d: 'diveKick' },
  },
  {
    id: 'ollie', name: 'OLLIE', blurb: 'Lightning-fast foam sword. Five jumps, whirl, counter.', color: '#e3271d', dark: '#5d7fb3', accent: '#ffd23f', acc: 'kid',
    weight: 79, walk: 4.3, run: 9.4, air: 4.9, airAcc: 0.6, grav: 0.5, fall: 7.6, ff: 11, jump: 12.2, hop: 7.8, djump: 8.8, jumps: 5,
    scale: 0.78, power: 0.86, speed: 0.82, reach: 1.2, sp: { n: 'whirl', s: 'dashStrike', u: 'spiral', d: 'counter' },
  },
  {
    id: 'tyler', name: 'TYLER', blurb: 'Strong all-rounder. Charge punch, dash strike, counter.', color: '#4a6fd6', dark: '#2c3f66', accent: '#9fd0ff', acc: 'kid',
    weight: 106, walk: 4.0, run: 8.6, air: 4.7, airAcc: 0.5, grav: 0.46, fall: 7.2, ff: 10.2, jump: 12.6, hop: 8.0, djump: 12.2, jumps: 2,
    scale: 1.1, power: 1.08, speed: 1.0, reach: 1.08, sp: { n: 'chargePunch', s: 'dashStrike', u: 'rocket', d: 'counter' },
  },
  {
    id: 'violet', name: 'VIOLET', blurb: 'Quick and graceful. Blaster, dash, teleport, counter.', color: '#9b6bff', dark: '#4b3d6b', accent: '#e3c8ff', acc: 'kid',
    weight: 92, walk: 4.3, run: 9.0, air: 5.2, airAcc: 0.56, grav: 0.42, fall: 6.8, ff: 9.8, jump: 12.4, hop: 7.8, djump: 12.0, jumps: 2,
    scale: 1.02, power: 0.96, speed: 0.9, reach: 1.02, sp: { n: 'blaster', s: 'dashStrike', u: 'teleport', d: 'counter' },
  },
  {
    id: 'layne', name: 'LAYNE', blurb: 'Heavy bruiser. Charge punch, armoured charge, quake.', color: '#3f5f9e', dark: '#1d2740', accent: '#ffd27a', acc: 'kid',
    weight: 124, walk: 3.5, run: 7.4, air: 4.1, airAcc: 0.42, grav: 0.53, fall: 8.0, ff: 11.2, jump: 12.8, hop: 8.2, djump: 12.4, jumps: 2,
    scale: 1.12, power: 1.14, speed: 1.1, reach: 1.06, sp: { n: 'chargePunch', s: 'shoulder', u: 'rocketHeavy', d: 'quake' },
  },
  {
    id: 'dad', name: 'DAD', blurb: 'The powerhouse. Shockwave, armoured charge, quake.', color: '#5a6072', dark: '#2a3a5c', accent: '#ffb347', acc: 'kid',
    weight: 132, walk: 3.6, run: 7.6, air: 4.1, airAcc: 0.42, grav: 0.54, fall: 8.2, ff: 11.4, jump: 13.0, hop: 8.3, djump: 12.6, jumps: 2,
    scale: 1.16, power: 1.2, speed: 1.12, reach: 1.08, sp: { n: 'waveSlash', s: 'shoulder', u: 'rocketHeavy', d: 'quake' },
  },
  {
    id: 'mom', name: 'MOM', blurb: 'Sharp and technical. Charge shot, bombs, counter.', color: '#b9783f', dark: '#2f4468', accent: '#ffe0a8', acc: 'kid',
    weight: 94, walk: 4.1, run: 8.4, air: 4.9, airAcc: 0.54, grav: 0.43, fall: 6.9, ff: 9.9, jump: 12.2, hop: 7.7, djump: 11.8, jumps: 2,
    scale: 1.01, power: 1.0, speed: 0.94, reach: 1.0, sp: { n: 'chargeShot', s: 'bomb', u: 'rocket', d: 'counter' },
  },
  {
    id: 'jayson', name: 'JAYSON', blurb: 'Fast, agile brawler. Blaster, cannonball, dive kick.', color: '#8fa0b8', dark: '#20242e', accent: '#ff7a6b', acc: 'kid',
    weight: 98, walk: 4.3, run: 9.2, air: 5.0, airAcc: 0.56, grav: 0.46, fall: 7.2, ff: 10.4, jump: 12.6, hop: 8.0, djump: 12.2, jumps: 2,
    scale: 1.06, power: 1.0, speed: 0.92, reach: 1.04, sp: { n: 'blaster', s: 'cannonball', u: 'rocket', d: 'diveKick' },
  },
  {
    id: 'vex', name: 'VEX', blurb: 'Light and fast. Triple jump, dive kick.', color: '#38d6ff', dark: '#0d5874', accent: '#fff36b', acc: 'scarf',
    weight: 82, walk: 4.2, run: 9.6, air: 5.0, airAcc: 0.55, grav: 0.46, fall: 7.2, ff: 10.5, jump: 12.6, hop: 8.0, djump: 12.0, jumps: 3,
    scale: 0.92, power: 0.92, speed: 0.9, reach: 1.0, sp: { n: 'blaster', s: 'dashStrike', u: 'rocket', d: 'diveKick' },
  },
  {
    id: 'brick', name: 'BRICK', blurb: 'Heavy hitter. Armoured charge, quake.', color: '#ff7a3d', dark: '#7a2d0b', accent: '#ffe0b0', acc: 'helm',
    weight: 128, walk: 3.4, run: 7.2, air: 4.0, airAcc: 0.4, grav: 0.55, fall: 8.2, ff: 11.5, jump: 12.8, hop: 8.2, djump: 12.5, jumps: 2,
    scale: 1.18, power: 1.15, speed: 1.12, reach: 1.05, sp: { n: 'chargePunch', s: 'shoulder', u: 'rocketHeavy', d: 'quake' },
  },
  {
    id: 'nova', name: 'NOVA', blurb: 'Floaty zoner. Charge shot, bombs, teleport.', color: '#c56bff', dark: '#4b1675', accent: '#8affea', acc: 'orbs',
    weight: 92, walk: 3.6, run: 7.6, air: 5.2, airAcc: 0.5, grav: 0.34, fall: 5.6, ff: 8.4, jump: 11.2, hop: 7.0, djump: 11.0, jumps: 2,
    scale: 0.98, power: 0.95, speed: 1.0, reach: 1.0, sp: { n: 'chargeShot', s: 'bomb', u: 'teleport', d: 'counter' },
  },
  {
    id: 'kai', name: 'KAI', blurb: 'All-rounder with a long blade. Counter.', color: '#4dd97a', dark: '#145a2c', accent: '#e8f4ff', acc: 'blade',
    weight: 100, walk: 3.9, run: 8.4, air: 4.6, airAcc: 0.5, grav: 0.45, fall: 7.0, ff: 10.0, jump: 12.4, hop: 7.8, djump: 12.0, jumps: 2,
    scale: 1.0, power: 1.0, speed: 1.0, reach: 1.22, sp: { n: 'waveSlash', s: 'dashStrike', u: 'spiral', d: 'counter' },
  },
];

// Fighters are drawn and simulated larger than the first prototype so they read clearly on screen.
const FIGHTER_SIZE = 1.9;
for (const d of FIGHTERS) d.scale *= FIGHTER_SIZE;

function buildMoves(def) {
  const m = baseMoves();
  m.nspec = spec(def.sp.n); m.sspec = spec(def.sp.s); m.uspec = spec(def.sp.u); m.dspec = spec(def.sp.d);
  for (const k in m) {
    const mv = m[k]; mv.name = k;
    const sp = mv.special ? 1 : def.speed;
    mv.len = Math.round(mv.len * sp);
    if (mv.charge) mv.charge = Math.round(mv.charge * sp);
    mv.hb = mv.hb.map(h => Object.assign({}, h, { f: [Math.round(h.f[0] * sp), Math.round(h.f[1] * sp)], d: h.d * def.power }));
  }
  return m;
}

// ---------------------------------------------------------------- game state
const G = {
  mode: 'title', paused: false, frame: 0, fighters: [], projs: [], parts: [], stage: null, plats: [],
  cam: { x: 0, y: -150, z: 0.9 }, shake: 0, flash: 0, countdown: 0, ending: 0, cfg: null, ledgeOcc: {}, banner: null,
};

function newFighter(def, port, ctrl, cpu, lvl) {
  return {
    def, port, ctrl, cpu, lvl, moves: buildMoves(def), sc: def.scale,
    w: 34 * def.scale, h: 80 * def.scale,
    x: 0, y: 0, vx: 0, vy: 0, lvx: 0, lvy: 0, facing: 1, grounded: false, ground: null,
    airJumps: def.jumps - 1, state: 'air', t: 0, lag: 0, move: null, mt: 0, mtHold: false, hitIds: new Map(), xhb: null,
    hitstop: 0, hitstun: 0, tumble: false, intang: 0, invinc: 0, shieldHP: 50, percent: 0, stocks: 3,
    meter: 0, od: 0, ff: false, dropT: 0, airdodged: false, sideB: false, upBUsed: false,
    ledge: null, ledgeCD: 0, ledgeIntUsed: false, charge: 0, counter: false, armor: 0,
    held: null, holder: null, holdT: 0, holdMax: 0, pummelT: 0,
    prevRaw: {}, inp: {}, buf: {}, holdDirT: 0, lastTapT: -99, lastTapX: 0, dirFreshT: -99, dirFreshYT: -99,
    techBuf: 0, shake: 0, flipT: 0, anim: 0, hidden: false, rot: 0, pctShake: 0,
    stats: { kos: 0, falls: 0, sds: 0, dealt: 0 }, lastHitBy: null, lastHitT: -999, place: 0, outT: 0, ai: {},
  };
}

const alive = f => f.stocks > 0;
function setState(f, s, lag = 0) { f.state = s; f.t = 0; f.lag = lag; if (s !== 'attack') { f.move = null; f.xhb = null; f.counter = false; f.armor = 0; f.mtHold = false; } }
function consume(f, b) { if (f.buf[b] > 0) { f.buf[b] = 0; return true; } return false; }

function startMove(f, name) {
  const mv = f.moves[name]; if (!mv) return;
  f.state = 'attack'; f.t = 0; f.move = mv; f.mt = 0; f.hitIds = new Map(); f.xhb = null; f.charge = f.storedCharge ? f.charge : 0;
  f.mtHold = false; f.armor = 0; f.counter = false;
  if (mv.intang) f.intang = Math.max(f.intang, mv.intang);
  if (mv.onStart) mv.onStart(f);
  if (!mv.special && !mv.counterStrike && name !== 'throw') Sfx.swing();
}
function endMove(f) {
  const mv = f.move;
  if (mv && mv.onEnd) mv.onEnd(f);
  if (f.state === 'attack') { if (f.grounded) setState(f, 'idle'); else setState(f, 'air'); }
}

// ---------------------------------------------------------------- input
const Keys = {};
addEventListener('keydown', e => {
  Keys[e.code] = true; Sfx.init();
  if (['Space', 'ArrowUp', 'ArrowDown', 'ArrowLeft', 'ArrowRight'].includes(e.code)) e.preventDefault();
  if ((e.code === 'Escape' || e.code === 'KeyP') && G.mode === 'fight') togglePause();
});
addEventListener('keyup', e => { Keys[e.code] = false; });
addEventListener('blur', () => { for (const k in Keys) Keys[k] = false; });

const KEYMAPS = [
  { l: ['KeyA'], r: ['KeyD'], u: ['KeyW'], d: ['KeyS'], jump: ['Space'], attack: ['KeyJ'], special: ['KeyK'], shield: ['KeyL', 'ShiftLeft'], smash: ['KeyI'], grab: ['KeyU'] },
  { l: ['ArrowLeft'], r: ['ArrowRight'], u: ['ArrowUp'], d: ['ArrowDown'], jump: ['Numpad0', 'Slash'], attack: ['Numpad1', 'Comma'], special: ['Numpad2', 'Period'], shield: ['Numpad3', 'ShiftRight'], smash: ['Numpad5', 'Semicolon'], grab: ['Numpad4', 'Quote'] },
];
const any = arr => arr.some(k => Keys[k]);
function emptyIn() { return { x: 0, y: 0, jump: false, attack: false, special: false, shield: false, grab: false, smash: false, sx: 0, sy: 0, analog: false }; }

function keyPoll(mapIdx) {
  const m = KEYMAPS[mapIdx], o = emptyIn(); if (!m) return o;
  o.x = (any(m.r) ? 1 : 0) - (any(m.l) ? 1 : 0); o.y = (any(m.d) ? 1 : 0) - (any(m.u) ? 1 : 0);
  for (const b of BTNS) o[b] = any(m[b]);
  if (o.smash) { o.sx = o.x; o.sy = o.y; }
  return o;
}
function padPoll(i) {
  const o = emptyIn(); const pads = navigator.getGamepads ? navigator.getGamepads() : []; const p = pads && pads[i]; if (!p) return o;
  const dz = v => (Math.abs(v) < 0.22 ? 0 : v); const b = n => !!(p.buttons[n] && p.buttons[n].pressed);
  o.x = dz(p.axes[0] || 0); o.y = dz(p.axes[1] || 0);
  if (b(14)) o.x = -1; if (b(15)) o.x = 1; if (b(12)) o.y = -1; if (b(13)) o.y = 1;
  o.analog = !(b(12) || b(13) || b(14) || b(15));
  o.attack = b(0); o.special = b(1); o.jump = b(2) || b(3); o.shield = b(4) || b(5) || b(6); o.grab = b(7);
  const rx = dz(p.axes[2] || 0), ry = dz(p.axes[3] || 0);
  if (Math.hypot(rx, ry) > 0.6) { o.smash = true; o.sx = Math.abs(rx) > Math.abs(ry) ? sgn(rx) : 0; o.sy = Math.abs(ry) >= Math.abs(rx) ? sgn(ry) : 0; }
  if (b(9) && !padPoll.prevStart[i] && G.mode === 'fight') togglePause();
  padPoll.prevStart[i] = b(9);
  return o;
}
padPoll.prevStart = {};

const Touch = { st: emptyIn(), stickId: null, on: false };
function touchPoll() { return Object.assign({}, Touch.st, { sx: Touch.st.smash ? Touch.st.x : 0, sy: Touch.st.smash ? Touch.st.y : 0, analog: true }); }
function mergeIn(list) {
  const o = emptyIn();
  for (const s of list) {
    if (Math.abs(s.x) > Math.abs(o.x)) { o.x = s.x; o.analog = s.analog; }
    if (Math.abs(s.y) > Math.abs(o.y)) o.y = s.y;
    for (const b of BTNS) o[b] = o[b] || s[b];
    if (s.smash) { o.sx = s.sx; o.sy = s.sy; }
  }
  return o;
}
function humanCtrl(port) {
  return { poll: () => mergeIn([keyPoll(port), padPoll(port), port === 0 ? touchPoll() : emptyIn()]) };
}

function readInput(f) {
  const raw = f.ctrl.poll(f), p = f.prevRaw;
  const inp = Object.assign({}, raw);
  for (const b of BTNS) { if (raw[b] && !p[b]) f.buf[b] = BUF; else if (f.buf[b] > 0) f.buf[b]--; }
  const ax = Math.abs(raw.x) > 0.5 ? sgn(raw.x) : 0, pax = Math.abs(p.x || 0) > 0.5 ? sgn(p.x) : 0;
  if (ax && ax !== pax) {
    if (G.frame - f.lastTapT < 14 && f.lastTapX === ax) inp.doubleTap = true;
    f.lastTapT = G.frame; f.lastTapX = ax; f.dirFreshT = G.frame;
    if (raw.analog && Math.abs(p.x || 0) < 0.3 && Math.abs(raw.x) > 0.85) inp.flickX = true;
  }
  const ay = Math.abs(raw.y) > 0.5 ? sgn(raw.y) : 0, pay = Math.abs(p.y || 0) > 0.5 ? sgn(p.y) : 0;
  if (ay && ay !== pay) { f.dirFreshYT = G.frame; if (raw.analog && Math.abs(p.y || 0) < 0.3 && Math.abs(raw.y) > 0.85) inp.flickY = true; }
  inp.downTap = raw.y > 0.5 && !((p.y || 0) > 0.5);
  inp.freshX = G.frame - f.dirFreshT <= 3; inp.freshY = G.frame - f.dirFreshYT <= 3;
  if (inp.flickX || inp.flickY) f.flickT = G.frame;
  inp.flick = G.frame - (f.flickT || -99) <= 3;
  if (ax) f.holdDirT++; else f.holdDirT = 0;
  if (raw.shield) f.techBuf = 14; else if (f.techBuf > 0) f.techBuf--;
  f.prevRaw = raw; f.inp = inp;
}

// ---------------------------------------------------------------- actions
function trySuper(f) {
  if (f.meter >= 100 && f.od <= 0 && f.buf.special > 0 && (f.buf.shield > 0 || f.inp.shield)) {
    f.buf.special = 0; f.buf.shield = 0; f.meter = 0; f.od = 360; f.invinc = Math.max(f.invinc, 30);
    G.flash = 10; G.shake = 10; Sfx.od(); fx.ring(f.x, f.y - 40, f.def.accent);
    for (let i = 0; i < 30; i++) fx.spark(f.x, f.y - 40, f.def.accent, 1, 12);
    G.banner = { text: f.def.name + ' OVERDRIVE!', t: 90, color: f.def.accent };
    return true;
  }
  return false;
}
function turnTo(f, x) { if (Math.abs(x) > 0.3) f.facing = sgn(x); }
function smashDir(f) {
  const i = f.inp; if (i.smash || f.buf.smash) return { x: i.sx, y: i.sy };
  return { x: i.x, y: i.y };
}
function doSpecial(f) {
  const i = f.inp;
  if (i.y < -0.5) { if (!f.grounded && f.upBUsed) return false; startMove(f, 'uspec'); }
  else if (i.y > 0.5) startMove(f, 'dspec');
  else if (Math.abs(i.x) > 0.4) { turnTo(f, i.x); startMove(f, 'sspec'); }
  else startMove(f, 'nspec');
  return true;
}
function doAerial(f, x, y) {
  if (y < -0.5) startMove(f, 'uair');
  else if (y > 0.5) startMove(f, 'dair');
  else if (Math.abs(x) > 0.4) startMove(f, sgn(x) === f.facing ? 'fair' : 'bair');
  else startMove(f, 'nair');
}
function doSmash(f, d) {
  if (d.y < -0.5) startMove(f, 'usmash');
  else if (d.y > 0.5) startMove(f, 'dsmash');
  else { turnTo(f, d.x); startMove(f, 'fsmash'); }
}

function groundActions(f) {
  const i = f.inp;
  if (trySuper(f)) return true;
  if (consume(f, 'jump')) { setState(f, 'jumpsquat'); return true; }
  if (consume(f, 'special')) return doSpecial(f);
  if (consume(f, 'smash')) { doSmash(f, smashDir(f)); return true; }
  if (consume(f, 'attack')) {
    if (i.flick) doSmash(f, { x: i.x, y: i.y });
    else if (f.state === 'run') startMove(f, 'dash');
    else if (i.y < -0.5) startMove(f, 'utilt');
    else if (i.y > 0.5) startMove(f, 'dtilt');
    else if (Math.abs(i.x) > 0.4) { turnTo(f, i.x); startMove(f, 'ftilt'); }
    else startMove(f, 'jab1');
    return true;
  }
  if (consume(f, 'grab')) { startMove(f, f.state === 'run' ? 'dashgrab' : 'grab'); return true; }
  if (i.shield || f.buf.shield > 0) {
    f.buf.shield = 0;
    if (Math.abs(i.x) > 0.5 && i.freshX) { startRoll(f, sgn(i.x)); return true; }
    if (i.y > 0.5 && i.freshY) { setState(f, 'spotdodge'); return true; }
    setState(f, 'shield'); Sfx.shield(); return true;
  }
  if (i.downTap && f.ground && f.ground.pass) { f.grounded = false; f.ground = null; f.dropT = 12; f.y += 3; setState(f, 'air'); return true; }
  return false;
}
function startRoll(f, dir) { setState(f, 'roll'); f.rollDir = dir; }

function groundMove(f) {
  const i = f.inp, d = f.def; const sp = f.od > 0 ? 1.15 : 1;
  if (i.y > 0.5 && Math.abs(i.x) < 0.5) { if (f.state !== 'crouch') setState(f, 'crouch'); f.vx = approach(f.vx, 0, 0.8); return; }
  if (Math.abs(i.x) > 0.2) {
    const dir = sgn(i.x);
    const wantRun = f.state === 'run' && dir === f.facing || i.doubleTap || (i.analog && Math.abs(i.x) > 0.85 && i.freshX) || f.holdDirT > 18;
    if (wantRun) {
      if (f.state !== 'run') { setState(f, 'run'); fx.smoke(f.x - dir * 10, f.y, 2); f.vx = dir * d.run * 0.7 * sp; }
      f.facing = dir; f.vx = approach(f.vx, dir * d.run * sp, 1.1);
      if (d.trail && G.frame % 2 === 0) fx.spark(f.x - dir * 14, f.y - rand(4, 50) * f.sc, d.trail[(G.frame >> 1) % 2], 1, 2);
    } else {
      if (f.state !== 'walk') setState(f, 'walk');
      f.facing = dir; f.vx = approach(f.vx, i.x * d.walk * sp * (i.analog ? 1 : 1), 0.8);
    }
  } else {
    if (f.state !== 'idle') setState(f, 'idle');
    f.vx = approach(f.vx, 0, 0.7);
  }
}
const approach = (v, t, a) => (v < t ? Math.min(t, v + a) : Math.max(t, v - a));

function airDrift(f, mult = 1) {
  const i = f.inp, d = f.def; const sp = (f.od > 0 ? 1.15 : 1) * mult;
  if (Math.abs(i.x) > 0.2) { const tgt = i.x * d.air * sp; if (Math.abs(f.vx) < Math.abs(tgt) || sgn(f.vx) !== sgn(tgt)) f.vx = approach(f.vx, tgt, d.airAcc * mult); }
  else f.vx = approach(f.vx, 0, 0.06);
}
function fastFallCheck(f) { if (f.inp.downTap && f.vy > -2 && !f.grounded) { f.ff = true; f.vy = f.def.ff; } }

function airActions(f) {
  const i = f.inp;
  if (trySuper(f)) return true;
  if (f.buf.jump > 0 && f.airJumps > 0) {
    f.buf.jump = 0; f.airJumps--; f.vy = -f.def.djump; f.vx = i.x * f.def.air * 0.9; f.ff = false; f.flipT = 24;
    fx.ring(f.x, f.y, '#ffffff80', 24); Sfx.jump(); return false;
  }
  if (consume(f, 'special')) return doSpecial(f);
  if (consume(f, 'smash')) { const d = smashDir(f); doAerial(f, d.x, d.y); return true; }
  if (consume(f, 'attack')) { doAerial(f, i.x, i.y); return true; }
  if (consume(f, 'grab')) { startMove(f, 'nair'); return true; }
  if (f.buf.shield > 0 && !f.airdodged) {
    f.buf.shield = 0; f.airdodged = true; setState(f, 'airdodge');
    const m = Math.hypot(i.x, i.y);
    f.adDir = m > 0.3 ? { x: i.x / m, y: i.y / m } : null;
    if (f.adDir) { f.vx = f.adDir.x * 9.5; f.vy = f.adDir.y * 9.5; }
    return true;
  }
  return false;
}

// ---------------------------------------------------------------- update
function update(f) {
  const i = f.inp, d = f.def;
  f.t++; f.anim++;
  if (f.intang > 0) f.intang--; if (f.invinc > 0) f.invinc--; if (f.dropT > 0) f.dropT--; if (f.ledgeCD > 0) f.ledgeCD--;
  if (f.flipT > 0) f.flipT--; if (f.pctShake > 0) f.pctShake--;
  if (f.od > 0) { f.od--; if (G.frame % 3 === 0) fx.spark(f.x + rand(-20, 20), f.y - rand(10, 70) * f.sc, d.accent, 1, 2); }
  if (f.state !== 'shield' && f.state !== 'shieldstun') f.shieldHP = Math.min(50, f.shieldHP + 0.08);
  let physics = true;

  switch (f.state) {
    case 'idle': case 'walk': case 'run': case 'crouch':
      if (!groundActions(f)) groundMove(f);
      break;
    case 'land':
      f.vx = approach(f.vx, 0, 0.8);
      if (f.t >= f.lag) { setState(f, 'idle'); groundActions(f); }
      break;
    case 'jumpsquat':
      if (f.t >= 3) {
        const full = i.jump;
        f.vy = -(full ? d.jump : d.hop); f.vx = f.vx * 0.85 + i.x * 1.5; f.grounded = false; f.ground = null; f.ff = false;
        setState(f, 'air'); fx.smoke(f.x, f.y, 3); Sfx.jump();
        if (f.buf.attack > 0 || f.buf.smash > 0) airActions(f);
      }
      break;
    case 'air':
      if (!airActions(f)) { airDrift(f); fastFallCheck(f); }
      break;
    case 'attack':
      stepMove(f);
      break;
    case 'helpless':
      airDrift(f, 0.7); fastFallCheck(f);
      break;
    case 'hitstun':
      f.hitstun--;
      if (f.grounded) f.vx = approach(f.vx, 0, 0.5);
      if (f.hitstun <= 0) setState(f, f.grounded ? 'idle' : 'air');
      break;
    case 'shield':
      f.vx = approach(f.vx, 0, 0.8);
      f.shieldHP -= 0.14;
      if (f.shieldHP <= 0) { shieldBreak(f); break; }
      if (trySuper(f)) break;
      if (consume(f, 'jump')) { setState(f, 'jumpsquat'); break; }
      if (consume(f, 'attack') || consume(f, 'grab')) { startMove(f, 'grab'); break; }
      if (f.buf.special > 0 && i.y < -0.5) { f.buf.special = 0; startMove(f, 'uspec'); break; }
      if (consume(f, 'smash')) { const sd = smashDir(f); if (sd.y < -0.5) { startMove(f, 'usmash'); break; } }
      if (Math.abs(i.x) > 0.5 && i.freshX) { startRoll(f, sgn(i.x)); break; }
      if (i.y > 0.5 && i.freshY) { setState(f, 'spotdodge'); break; }
      if (!i.shield) setState(f, 'shieldDrop');
      break;
    case 'shieldstun':
      f.vx = approach(f.vx, 0, 0.6);
      if (f.t >= f.lag) setState(f, i.shield ? 'shield' : 'shieldDrop');
      break;
    case 'shieldDrop':
      if (f.t >= 7) setState(f, 'idle');
      break;
    case 'dizzy':
      f.vx = approach(f.vx, 0, 0.5);
      if (f.grounded && f.t >= f.lag) setState(f, 'idle');
      break;
    case 'roll':
      if (f.t === 4) f.intang = Math.max(f.intang, 15);
      if (f.t >= 4 && f.t < 22) f.vx = f.rollDir * 7.2; else f.vx = approach(f.vx, 0, 1.2);
      if (f.t >= 30) { f.facing = -f.rollDir; setState(f, 'idle'); }
      break;
    case 'spotdodge':
      f.vx = 0; if (f.t === 3) f.intang = Math.max(f.intang, 15);
      if (f.t >= 26) setState(f, 'idle');
      break;
    case 'airdodge':
      if (f.t === 3) f.intang = Math.max(f.intang, 24);
      if (f.adDir) { f.vx *= 0.9; f.vy *= 0.9; } else { airDrift(f, 0.6); }
      if (f.t >= 30) setState(f, 'air');
      break;
    case 'tech':
      if (f.t === 1) f.intang = Math.max(f.intang, 20);
      f.vx = f.techDir * 5; if (f.t >= 24) { f.vx = 0; setState(f, 'idle'); }
      break;
    case 'down':
      f.vx = approach(f.vx, 0, 0.6);
      if ((f.t > 10 && (Math.abs(i.x) > 0.5 || i.jump || i.shield || f.buf.attack)) || f.t > 70) {
        if (consume(f, 'attack')) { setState(f, 'idle'); startMove(f, 'getupAtk'); }
        else if (Math.abs(i.x) > 0.5) { f.intang = 18; startRoll(f, sgn(i.x)); }
        else { setState(f, 'getup'); f.intang = 20; }
      }
      break;
    case 'getup':
      if (f.t >= 22) setState(f, 'idle');
      break;
    case 'ledge': physics = false; ledgeUpdate(f); break;
    case 'ledgeclimb':
      physics = false;
      if (f.t >= 22) {
        const L = f.climbTo; f.x = L.x; f.y = L.y; f.grounded = true; f.ground = G.mainSurf; f.vx = f.vy = 0; setState(f, 'idle');
      }
      break;
    case 'holding': physics = true; holdUpdate(f); break;
    case 'grabbed': physics = false; break;
    case 'respawn':
      physics = false;
      if (f.t > 30 && (Math.abs(i.x) > 0.5 || Math.abs(i.y) > 0.5 || BTNS.some(b => f.buf[b] > 0)) || f.t > 180) {
        setState(f, 'air'); f.invinc = 120; f.vy = 0; f.airJumps = d.jumps - 1;
      }
      break;
    case 'dead': case 'out':
      physics = false;
      if (f.state === 'dead' && f.t >= 70) respawn(f);
      break;
  }
  if (physics) doPhysics(f);
}

function stepMove(f) {
  const mv = f.move, i = f.inp;
  f.mtHold = false;
  if (mv.charge && f.mt === mv.charge && f.charge < 60) {
    const held = i.smash || i.attack || (f.inp.smash);
    if (held) { f.charge++; f.mtHold = true; if (G.frame % 4 === 0) fx.spark(f.x + rand(-24, 24), f.y - rand(10, 70), '#ffffff', 1, 3); f.shake = 1; }
  }
  if (mv.onFrame) { const r = mv.onFrame(f, f.mt, i); if (r === 'hold') f.mtHold = true; if (f.state !== 'attack') return; }
  // jab chain
  if (mv.next && f.mt >= mv.win && f.buf.attack > 0 && Math.abs(i.x) < 0.4 && Math.abs(i.y) < 0.5) {
    f.buf.attack = 0; startMove(f, mv.next); return;
  }
  if (!f.grounded) {
    if (mv.air || mv.special) airDrift(f, mv.drift != null ? mv.drift : 1);
    if (mv.air) fastFallCheck(f);
  } else if (!mv.onFrame || mv.name === 'fsmash') {
    f.vx = approach(f.vx, 0, 0.7);
  }
  // counter strike damage box
  if (mv.counterStrike && f.mt === 5) f.xhb = [H(5, 9, 40, -40, 30, f.counterDmg, 361, 60, 80)];
}

function shieldBreak(f) {
  f.shieldHP = 20; setState(f, 'dizzy', 240); f.grounded = false; f.ground = null; f.vy = -13;
  Sfx.shieldBreak(); G.shake = 14; fx.ring(f.x, f.y - 40, '#7fd8ff', 60);
  for (let k = 0; k < 20; k++) fx.spark(f.x, f.y - 40, '#9fe6ff', 1, 10);
}

// ---------------------------------------------------------------- grab & throw
function holdUpdate(f) {
  const v = f.held, i = f.inp;
  f.vx = approach(f.vx, 0, 1);
  if (!v || v.state !== 'grabbed') { setState(f, 'idle'); f.held = null; return; }
  v.x = f.x + f.facing * (f.w / 2 + v.w / 2 + 2); v.y = f.y; v.facing = -f.facing; v.vx = v.vy = 0;
  f.holdT++;
  if (BTNS.some(b => v.buf[b] === BUF) || (Math.abs(v.inp.x) > 0.5 && v.inp.freshX)) f.holdT += 4; // mash out
  if (f.pummelT > 0) f.pummelT--;
  if (f.t > 6) {
    let dir = null;
    if (Math.abs(i.x) > 0.6) dir = sgn(i.x) === f.facing ? 'f' : 'b';
    else if (i.y < -0.6) dir = 'u'; else if (i.y > 0.6) dir = 'd';
    if (consume(f, 'smash')) { const sd = smashDir(f); dir = sd.y < -0.5 ? 'u' : sd.y > 0.5 ? 'd' : sgn(sd.x) === -f.facing ? 'b' : 'f'; }
    if (dir) { doThrow(f, v, dir); return; }
    if (consume(f, 'attack') && f.pummelT <= 0) {
      f.pummelT = 14; v.percent += 1.3 * f.def.power; v.pctShake = 8; v.shake = 4; f.hitstop = 3; v.hitstop = 3;
      fx.spark((f.x + v.x) / 2, f.y - 45, '#fff', 4, 4); Sfx.hit(2); addMeter(f, 1.3);
    }
  }
  if (f.holdT > f.holdMax) releaseGrab(f, v);
}
function releaseGrab(f, v) {
  f.held = null; setState(f, 'idle'); f.vx = -f.facing * 4;
  if (v && v.state === 'grabbed') { v.holder = null; setState(v, 'air'); v.grounded = false; v.ground = null; v.vy = -7; v.vx = f.facing * 4; }
}
function doThrow(f, v, dir) {
  const pw = f.def.power;
  const T = { f: { a: 45, d: 8, b: 65, g: 60 }, b: { a: 135, d: 10, b: 65, g: 70 }, u: { a: 90, d: 7, b: 70, g: 60 }, d: { a: 78, d: 6, b: 60, g: 38 } }[dir];
  startMove(f, 'throw'); f.held = null;
  v.holder = null; setState(v, 'air');
  if (dir === 'b') { v.x = f.x - f.facing * (f.w / 2 + v.w / 2); }
  if (dir === 'd') { fx.smoke(v.x, v.y, 5); }
  applyHit(f, v, { d: T.d * pw, a: T.a, b: T.b, g: T.g }, f.facing, { throw: true, px: v.x, py: v.y - 40 });
}

// ---------------------------------------------------------------- ledges
function ledgeList() {
  const m = G.stage.main;
  return [{ id: 'L', x: m.x1, y: m.y, side: -1 }, { id: 'R', x: m.x2, y: m.y, side: 1 }];
}
function tryLedge(f) {
  if (f.grounded || f.ledgeCD > 0) return;
  const okState = f.state === 'air' || f.state === 'helpless' || (f.state === 'attack' && f.move && f.move.ledgeFrom != null && f.mt >= f.move.ledgeFrom);
  if (!okState) return;
  if (f.vy < 0 && f.state !== 'helpless') return;
  if (f.inp.y > 0.5) return; // holding down = don't grab
  for (const L of ledgeList()) {
    if (G.ledgeOcc[L.id] && G.ledgeOcc[L.id] !== f && G.ledgeOcc[L.id].state === 'ledge') continue;
    const outX = L.x + L.side * (f.w / 2 + 2);
    const dx = (f.x - outX) * L.side; // >0 means further out
    if (dx > -26 && dx < 44 && f.y > L.y + 8 && f.y < L.y + f.h * 1.25) {
      if (f.state === 'attack' && f.move && f.move.onEnd) f.move.onEnd(f);
      setState(f, 'ledge'); f.ledge = L; G.ledgeOcc[L.id] = f;
      f.x = outX; f.y = L.y + f.h * 0.78; f.vx = f.vy = f.lvx = f.lvy = 0; f.facing = -L.side; f.ff = false;
      f.airJumps = f.def.jumps - 1; f.airdodged = false; f.sideB = false; f.upBUsed = false;
      f.intang = Math.max(f.intang, f.ledgeIntUsed ? 22 : 60); f.ledgeIntUsed = true; f.hidden = false;
      Sfx.grab(); return;
    }
  }
}
function ledgeUpdate(f) {
  const L = f.ledge, i = f.inp; f.vx = f.vy = 0;
  const leave = () => { G.ledgeOcc[L.id] = null; f.ledge = null; };
  const onStage = { x: L.x - L.side * (f.w / 2 + 6), y: L.y };
  if (f.t < 8) return;
  if (consume(f, 'jump')) { leave(); setState(f, 'air'); f.y = L.y - 4; f.x = L.x + L.side * 6; f.vy = -f.def.jump * 0.92; f.vx = -L.side * 2; Sfx.jump(); return; }
  if (consume(f, 'attack') || consume(f, 'smash')) { leave(); f.x = onStage.x; f.y = onStage.y; f.grounded = true; f.ground = G.mainSurf; setState(f, 'idle'); startMove(f, 'ledgeAtk'); return; }
  if (consume(f, 'shield')) { leave(); f.x = onStage.x; f.y = onStage.y; f.grounded = true; f.ground = G.mainSurf; f.intang = 20; startRoll(f, -L.side); return; }
  if (i.y < -0.5 || i.x * -L.side > 0.5) { leave(); f.intang = Math.max(f.intang, 22); setState(f, 'ledgeclimb'); f.climbTo = onStage; return; }
  if (i.y > 0.5 || i.x * L.side > 0.5 || f.t > 300) { leave(); setState(f, 'air'); f.ledgeCD = 30; f.x += L.side * 6; f.vy = 1; return; }
}

// ---------------------------------------------------------------- physics
function surfaces() { return G.surfs; }
function doPhysics(f) {
  const d = f.def;
  const mv = f.state === 'attack' ? f.move : null;
  const noGrav = (mv && mv.noGrav) || f.state === 'airdodge' && f.adDir && f.t < 20;
  if (!f.grounded && !noGrav) {
    const max = f.ff ? d.ff : d.fall;
    f.vy = Math.min(f.vy + d.grav, Math.max(max, f.vy > max ? f.vy : max));
    if (f.vy > max) f.vy = Math.max(max, f.vy - 0.5);
    if (f.ff) f.vy = Math.max(f.vy, d.ff * 0.9);
  }
  // launch decay
  const lm = Math.hypot(f.lvx, f.lvy);
  if (lm > 0) { const nm = Math.max(0, lm - 0.23); f.lvx *= nm / lm; f.lvy *= nm / lm; }
  if (f.grounded) { f.lvx *= 0.82; f.lvy = 0; if (Math.abs(f.lvx) < 0.1) f.lvx = 0; }
  // tumble trail
  if (f.state === 'hitstun' && lm > 9 && G.frame % 2 === 0) fx.smoke(f.x, f.y - f.h / 2, 1, lm > 22 ? '#ffffffaa' : null);

  const px = f.x, py = f.y;
  const tvx = f.vx + f.lvx, tvy = f.vy + f.lvy;
  f.x += tvx; f.y += tvy;

  if (f.grounded) {
    const g = f.ground;
    if (tvy < -0.01) { f.grounded = false; f.ground = null; }
    else if (f.x < g.x1 || f.x > g.x2) {
      const canFall = f.state === 'walk' || f.state === 'run' || f.state === 'hitstun' || (f.state === 'attack' && f.move && f.move.special) || f.state === 'down' || f.state === 'dizzy';
      if (canFall) { f.grounded = false; f.ground = null; if (f.state === 'walk' || f.state === 'run') setState(f, 'air'); }
      else { f.x = clamp(f.x, g.x1, g.x2); f.vx = 0; f.lvx = 0; }
    } else f.y = g.y;
  }
  if (!f.grounded && tvy >= 0) {
    for (const s of surfaces()) {
      if (s.pass && (f.dropT > 0 || (f.inp.y > 0.7 && f.state !== 'hitstun') || f.state === 'airdodge' && f.adDir && f.adDir.y > 0.5)) continue;
      if (py <= s.y + 0.5 + (s.dy || 0) && f.y >= s.y && f.x >= s.x1 && f.x <= s.x2) { land(f, s); break; }
    }
  }
  // main stage body as a solid box
  const m = G.stage.main;
  const bx1 = f.x - f.w / 2, bx2 = f.x + f.w / 2, by1 = f.y - f.h, by2 = f.y;
  if (!(f.grounded && f.ground === G.mainSurf) && bx2 > m.x1 && bx1 < m.x2 && by2 > m.y + 1 && by1 < m.y + m.depth) {
    const pby1 = py - f.h;
    if (pby1 >= m.y + m.depth - 1) { f.y = m.y + m.depth + f.h; f.vy = Math.max(0, f.vy); f.lvy = Math.abs(f.lvy) * 0.5; }
    else {
      const pushL = bx2 - m.x1, pushR = m.x2 - bx1;
      if (pushL < pushR) { f.x -= pushL; if (f.lvx > 0) f.lvx = -f.lvx * 0.6; if (f.vx > 0) f.vx = 0; }
      else { f.x += pushR; if (f.lvx < 0) f.lvx = -f.lvx * 0.6; if (f.vx < 0) f.vx = 0; }
    }
  }
  tryLedge(f);
}

function land(f, s) {
  const wasLv = Math.hypot(f.lvx, f.lvy);
  f.grounded = true; f.ground = s; f.y = s.y; f.vy = 0;
  f.airJumps = f.def.jumps - 1; f.airdodged = false; f.sideB = false; f.upBUsed = false; f.ledgeIntUsed = false; f.ff = false;
  const st = f.state;
  if (st === 'hitstun') {
    if (f.tumble) {
      if (f.techBuf > 0) { f.lvx = f.lvy = 0; setState(f, 'tech'); f.techDir = sgn(f.inp.x); fx.spark(f.x, f.y, '#fff', 6, 5); Sfx.blip(); return; }
      if (f.lvy > 6 && wasLv > 9) { f.grounded = false; f.ground = null; f.lvy = -f.lvy * 0.55; f.y -= 1; fx.smoke(f.x, f.y, 6); Sfx.land(); return; }
      f.lvy = 0; setState(f, 'down'); fx.smoke(f.x, f.y, 5); Sfx.land(); return;
    }
    f.lvy = 0; return;
  }
  f.lvy = 0;
  fx.smoke(f.x, f.y, 2);
  if (st === 'attack') {
    const mv = f.move;
    if (mv.onLand) mv.onLand(f);
    if (mv.air) { setState(f, 'land', mv.ll); return; }
    if (mv.special && !(mv.noLandCancel && f.mt < 20)) { if (mv.onEnd && mv.recovery) { /* no helpless on land */ } setState(f, 'land', mv.landLag || 6); f.hidden = false; return; }
    return;
  }
  if (st === 'helpless') { setState(f, 'land', 18); return; }
  if (st === 'airdodge') { setState(f, 'land', f.adDir ? 10 : 3); return; }
  if (st === 'dizzy') return;
  if (st === 'air' || st === 'jumpsquat') { setState(f, 'land', 3); Sfx.land(); }
}

// ---------------------------------------------------------------- hits
function hurtRect(f) {
  const crouch = f.state === 'crouch' || (f.state === 'attack' && f.move.crouch) || f.state === 'down';
  const h = f.h * (crouch ? 0.6 : 1) * (f.state === 'down' ? 0.5 : 1);
  return { x1: f.x - f.w / 2, x2: f.x + f.w / 2, y1: f.y - h, y2: f.y };
}
function circRect(cx, cy, r, R) { const nx = clamp(cx, R.x1, R.x2), ny = clamp(cy, R.y1, R.y2); return (cx - nx) ** 2 + (cy - ny) ** 2 <= r * r; }
function kbFormula(p, d, w, b, g) { return ((p / 10 + (p * d) / 20) * (200 / (w + 100)) * 1.4 + 18) * (g / 100) + b; }
function hbWorld(f, h) { const s = f.sc; return { x: f.x + h.x * f.facing * s * f.def.reach, y: f.y + h.y * s, r: h.r * s * (h.x !== 0 ? Math.sqrt(f.def.reach) : 1) }; }
function activeHBs(f) {
  if (f.state !== 'attack' || !f.move || f.hitstop > 0) return [];
  const list = f.move.hb.concat(f.xhb || []);
  return list.filter(h => f.mt >= h.f[0] && f.mt <= h.f[1]);
}
function canBeHit(v) {
  return alive(v) && v.state !== 'dead' && v.state !== 'out' && v.state !== 'respawn' && v.intang <= 0 && v.invinc <= 0 && !v.hidden;
}
function addMeter(f, d) { if (f.od <= 0) f.meter = Math.min(100, f.meter + d); }

function applyHit(att, vic, hb, dir, o = {}) {
  let dmg = hb.d * (att && att.od > 0 ? 1.3 : 1);
  // counter
  if (vic.counter && !o.throw) {
    vic.counter = false; vic.counterDmg = Math.max(8, dmg * 1.25);
    vic.facing = att ? sgn(att.x - vic.x) || vic.facing : vic.facing;
    vic.intang = 30; startMove(vic, 'counterStrike');
    if (att && !o.projectile) { att.hitstop = 22; att.shake = 10; }
    G.flash = 6; Sfx.parry(); fx.ring(vic.x, vic.y - 40, '#ffffff', 50);
    return true;
  }
  // shield
  if ((vic.state === 'shield' || vic.state === 'shieldstun') && !o.throw) {
    const facingAtt = true;
    if (vic.state === 'shield' && vic.t <= 5 && facingAtt) { // parry
      Sfx.parry(); G.flash = 4; fx.ring(vic.x, vic.y - 40, '#ffffff', 46);
      if (att && !o.projectile) { att.hitstop = 22; att.shake = 6; }
      vic.hitstop = 6; setState(vic, 'idle'); vic.intang = 10;
      return true;
    }
    vic.shieldHP -= dmg * 1.19;
    const stun = Math.floor(dmg * 0.55) + 3;
    setState(vic, 'shieldstun', stun);
    vic.lvx = dir * (dmg * 0.22 + 1);
    const hl = Math.floor(dmg * 0.35 + 3);
    vic.hitstop = hl; if (att && !o.projectile) att.hitstop = hl;
    Sfx.shield(); fx.spark(o.px, o.py, '#9fe6ff', 5, 5);
    if (vic.shieldHP <= 0) shieldBreak(vic);
    return true;
  }
  // armor
  if (vic.armor && dmg < 13) {
    vic.percent = Math.min(999, vic.percent + dmg); vic.pctShake = 10; vic.hitstop = 5; if (att) att.hitstop = 5;
    Sfx.hit(dmg); fx.spark(o.px, o.py, '#ffd27a', 6, 6); return true;
  }
  // real hit
  if (vic.state === 'holding' && vic.held) releaseGrab(vic, vic.held);
  if (vic.state === 'grabbed' && vic.holder && vic.holder !== att) { const h = vic.holder; h.held = null; setState(h, 'idle'); }
  if (vic.state === 'ledge' && vic.ledge) { G.ledgeOcc[vic.ledge.id] = null; vic.ledge = null; }
  if (vic.state === 'attack' && vic.move && vic.move.onEnd) vic.move.onEnd(vic);
  vic.holder = null;
  vic.hidden = false;
  vic.percent = Math.min(999, vic.percent + dmg);
  vic.pctShake = 12;
  let kb = kbFormula(vic.percent, dmg, vic.def.weight, hb.b, hb.g);
  if (att && att.od > 0) kb *= 1.12;
  let ang = hb.a;
  if (ang === 361) ang = vic.grounded && kb < 60 ? 0 : 40;
  let lx = Math.cos(ang * DEG) * dir, ly = -Math.sin(ang * DEG);
  // directional influence
  const ix = vic.inp.x || 0, iy = vic.inp.y || 0;
  if (Math.hypot(ix, iy) > 0.3 && kb > 40) {
    const perp = ix * -ly + iy * lx; // cross
    const rot = clamp(perp, -1, 1) * 15 * DEG;
    const c = Math.cos(rot), s = Math.sin(rot); const nlx = lx * c - ly * s, nly = lx * s + ly * c; lx = nlx; ly = nly;
  }
  const spd = kb * 0.135;
  vic.vx = 0; vic.vy = 0; vic.lvx = lx * spd; vic.lvy = ly * spd;
  if (vic.grounded && vic.lvy > 0 && kb > 50) { vic.lvy = -vic.lvy * 0.7; } // spiked on ground: bounce up
  if (vic.lvy < -0.5) { vic.grounded = false; vic.ground = null; }
  vic.hitstun = Math.floor(kb * 0.4); vic.tumble = kb > 80;
  setState(vic, 'hitstun'); vic.ff = false; vic.airdodged = false; vic.sideB = false; vic.upBUsed = false; vic.ledgeIntUsed = false;
  vic.facing = -dir || vic.facing;
  const hl = Math.min(20, Math.floor(dmg * 0.45 + 4)) + (o.charge || 0);
  vic.hitstop = hl; vic.shake = hl; if (att && !o.projectile) att.hitstop = hl;
  if (att) { vic.lastHitBy = att; vic.lastHitT = G.frame; att.stats.dealt += dmg; addMeter(att, dmg); }
  addMeter(vic, dmg * 0.5);
  G.shake = Math.max(G.shake, Math.min(22, kb * 0.06));
  fx.hit(o.px, o.py, dmg, att ? att.def.accent : '#fff', lx, ly, spd);
  Sfx.hit(dmg);
  if (spd > 25) { // strong enough to finish: long freeze with the dramatic backdrop
    G.flash = 6; vic.hitstop += 16; vic.shake = vic.hitstop; if (att && !o.projectile) att.hitstop += 16; Sfx.bigHit();
    if (fx.finisher) fx.finisher(o.px, o.py, lx, ly, att ? att.def.accent : '#fff', vic.hitstop);
  }
  return true;
}

function checkHits() {
  const fs = G.fighters;
  for (const a of fs) {
    const hbs = activeHBs(a); if (!hbs.length) continue;
    for (const h of hbs) {
      const w = hbWorld(a, h);
      for (const v of fs) {
        if (v === a || !canBeHit(v) || (a.team && a.team === v.team)) continue;
        if (v.state === 'grabbed' && v.holder === a) continue;
        const last = a.hitIds.get(v);
        if (last != null && !(h.rehit && a.mt - last >= h.rehit)) continue;
        if (!circRect(w.x, w.y, w.r, hurtRect(v))) continue;
        if (h.grab) {
          if (v.state === 'shield' || v.state === 'shieldstun' || v.grounded || v.y > -20 + (v.ground ? v.ground.y : 0)) {
            if (v.state === 'holding' && v.held) releaseGrab(v, v.held);
            if (v.state === 'ledge' && v.ledge) { G.ledgeOcc[v.ledge.id] = null; v.ledge = null; }
            setState(a, 'holding'); a.held = v; a.holdT = 0; a.holdMax = 70 + v.percent * 0.9; a.pummelT = 0;
            setState(v, 'grabbed'); v.holder = a; v.grounded = true; v.ground = a.ground; v.lvx = v.lvy = 0;
            Sfx.grab(); a.hitIds.set(v, a.mt);
            break;
          }
          continue;
        }
        a.hitIds.set(v, a.mt);
        // mark all of this move's boxes as having hit this victim (one hit per move unless rehit)
        const dir = h.a === 361 || (h.a > 90 && h.a < 270) ? a.facing : a.facing;
        const charge = a.move && a.move.smash ? Math.floor(a.charge / 10) : 0;
        const ch = a.move && a.move.smash ? 1 + (a.charge / 60) * 0.4 : 1;
        applyHit(a, v, Object.assign({}, h, { d: h.d * ch }), dir, { px: (w.x + v.x) / 2, py: clamp(w.y, v.y - v.h, v.y), charge });
      }
      if (a.state !== 'attack') break;
    }
  }
  // projectiles
  for (const p of G.projs) {
    if (p.dead) continue;
    for (const v of fs) {
      if (v === p.owner || !canBeHit(v) || p.hit.has(v) || (p.owner.team && p.owner.team === v.team)) continue;
      if (!circRect(p.x, p.y, p.r, hurtRect(v))) continue;
      p.hit.add(v);
      applyHit(p.owner, v, p, sgn(p.vx) || p.owner.facing, { projectile: true, px: p.x, py: p.y });
      if (p.explode) explodeProj(p); else if (p.type !== 'blast') p.dead = true;
      break;
    }
  }
  // projectile vs projectile
  for (let i = 0; i < G.projs.length; i++) for (let j = i + 1; j < G.projs.length; j++) {
    const a = G.projs[i], b = G.projs[j];
    if (a.dead || b.dead || a.owner === b.owner || a.type === 'blast' || b.type === 'blast') continue;
    if (Math.hypot(a.x - b.x, a.y - b.y) < a.r + b.r) {
      if (a.d > b.d * 1.5) b.dead = true; else if (b.d > a.d * 1.5) a.dead = true; else { a.dead = b.dead = true; }
      fx.spark((a.x + b.x) / 2, (a.y + b.y) / 2, '#fff', 6, 5);
    }
  }
}

// ---------------------------------------------------------------- projectiles
function spawnProj(f, o) {
  const p = Object.assign({ vy: 0, grav: 0, hit: new Set(), owner: f, t: 0 }, o);
  p.x = f.x + o.x * f.facing * f.sc; p.y = f.y + o.y * f.sc; p.vx = o.vx * f.facing;
  p.d = o.d * f.def.power; p.f = [0, 999];
  G.projs.push(p); return p;
}
function explodeProj(p) {
  if (p.dead) return; p.dead = true;
  const e = p.explode;
  const b = { type: 'blast', x: p.x, y: p.y, vx: 0, vy: 0, grav: 0, r: e.r, d: e.d * p.owner.def.power, a: e.a, b: e.b, g: e.g, life: 5, hit: p.hit, owner: p.owner, t: 0, invisible: true };
  G.projs.push(b);
  fx.boom(p.x, p.y, e.r); Sfx.boom(); G.shake = Math.max(G.shake, 8);
}
function updateProjs() {
  for (const p of G.projs) {
    if (p.dead) continue;
    p.t++; p.vy += p.grav; p.x += p.vx; p.y += p.vy;
    if (p.type === 'wave') {
      // hug the ground; die off ledges
      const s = G.surfs.find(s => p.x >= s.x1 && p.x <= s.x2 && Math.abs(p.y + 20 - s.y) < 40);
      if (!s) p.vy = 4; else { p.y = s.y - 20; p.vy = 0; }
      if (G.frame % 2 === 0) fx.spark(p.x, p.y + 14, p.owner.def.accent, 1, 2);
    }
    if (p.type === 'bomb') {
      for (const s of G.surfs) if (p.vy > 0 && p.y >= s.y - p.r && p.y - p.vy < s.y - p.r + 1 && p.x >= s.x1 && p.x <= s.x2) { explodeProj(p); break; }
    }
    if (p.type === 'orb' && G.frame % 2 === 0) fx.spark(p.x - p.vx, p.y, p.owner.def.accent, 1, 1.5);
    if (p.t >= p.life) { if (p.explode) explodeProj(p); p.dead = true; }
    const bl = G.stage.blast; if (p.x < bl.l || p.x > bl.r || p.y > bl.b) p.dead = true;
  }
  G.projs = G.projs.filter(p => !p.dead);
}

// ---------------------------------------------------------------- KO / respawn
function checkBlast(f) {
  if (f.state === 'dead' || f.state === 'out' || f.state === 'respawn') return;
  const b = G.stage.blast;
  // top blast zone only kills while tumbling
  const outTop = f.y - f.h < b.t && (f.state === 'hitstun' || f.y < b.t - 200);
  if (f.x < b.l || f.x > b.r || f.y > b.b || outTop) ko(f);
}
function ko(f) {
  f.stocks--; f.stats.falls++;
  if (f.lastHitBy && G.frame - f.lastHitT < 600 && f.lastHitBy !== f) f.lastHitBy.stats.kos++; else f.stats.sds++;
  if (f.held) releaseGrab(f, f.held);
  if (f.ledge) { G.ledgeOcc[f.ledge.id] = null; f.ledge = null; }
  const b = G.stage.blast;
  const ex = clamp(f.x, b.l, b.r), ey = clamp(f.y - f.h / 2, b.t, b.b);
  fx.koBlast(ex, ey, f.def.color, f.port);
  G.shake = 24; G.flash = 8; Sfx.boom();
  f.lvx = f.lvy = f.vx = f.vy = 0;
  setState(f, f.stocks > 0 ? 'dead' : 'out');
  if (f.stocks <= 0) { f.outT = G.frame; }
}
function respawn(f) {
  f.x = f.spawnX; f.y = -330; f.percent = 0; f.vx = f.vy = f.lvx = f.lvy = 0; f.grounded = false; f.ground = null;
  f.facing = f.x > 0 ? -1 : 1; f.shieldHP = 50; f.charge = 0; f.storedCharge = false; f.hidden = false;
  setState(f, 'respawn');
}

// ---------------------------------------------------------------- CPU
function aiCtrl() { return { poll: f => aiPoll(f) }; }
function aiPoll(f) {
  const a = f.ai, o = emptyIn(), lvl = f.lvl; a.t = (a.t || 0) + 1;
  if (G.countdown > 0) return o;
  if (a.holdShield > 0) { a.holdShield--; o.shield = true; }
  if (a.holdJump > 0) { a.holdJump--; o.jump = true; }
  if (a.holdSpecial > 0) { a.holdSpecial--; o.special = true; }
  if (a.holdX) { o.x = a.holdX; }
  const m = G.stage.main;
  let tgt = null, best = 1e9;
  for (const g of G.fighters) if (g !== f && alive(g) && g.state !== 'dead' && g.state !== 'respawn' && !(f.team && g.team === f.team)) {
    const dd = Math.hypot(g.x - f.x, g.y - f.y) + (g.percent > 100 ? -80 : 0); if (dd < best) { best = dd; tgt = g; }
  }
  const S = f.state;
  if (S === 'respawn') { if (f.t > 40 + rand(0, 60)) o.x = f.x > 0 ? -1 : 1; return o; }
  if (S === 'grabbed') { if (a.t % Math.max(3, 14 - lvl) === 0) o.attack = true; o.x = a.t % 10 < 5 ? 1 : -1; return o; }
  if (S === 'holding') {
    if (f.t > 12 + (9 - lvl) * 2) {
      const edgeDir = f.x > 0 ? 1 : -1;
      const r = (a.throwPick ??= Math.random());
      if (f.held && f.held.percent > 110 && r < 0.6) o.x = edgeDir;
      else if (r < 0.3) o.y = 1; else if (r < 0.55) o.y = -1; else o.x = edgeDir;
    } else a.throwPick = undefined;
    return o;
  }
  if (S === 'ledge') {
    if (f.t > 10 + (10 - lvl) * 3 + (a.ledgeWait ??= rand(0, 40))) {
      a.ledgeWait = undefined; const r = Math.random();
      if (r < 0.35) o.x = f.ledge ? -f.ledge.side : 0; else if (r < 0.6) o.jump = true; else if (r < 0.8) o.attack = true; else o.shield = true;
    }
    return o;
  }
  if (S === 'hitstun') {
    // DI toward stage / survival DI
    o.x = f.x > 0 ? -1 : 1; o.y = f.lvy < 0 ? 0 : -1;
    if (lvl >= 4 && f.y > -40 && f.lvy > 0) o.shield = a.t % 4 === 0; // try to tech
    return o;
  }
  const offstage = !f.grounded && (f.x < m.x1 - 5 || f.x > m.x2 + 5 || f.y > m.y + 4);
  if (offstage || (!f.grounded && f.y > -40 && (f.x < m.x1 + 30 || f.x > m.x2 - 30) && f.vy > 0)) {
    const toward = f.x < 0 ? 1 : -1; const ledgeX = f.x < 0 ? m.x1 : m.x2;
    o.x = toward;
    if (S === 'helpless' || S === 'attack') { if (S === 'attack' && f.move && f.move.name === 'uspec') o.x = toward; return o; }
    const below = f.y - m.y, dx = Math.abs(f.x - ledgeX);
    if (f.airJumps > 0 && f.vy > -1 && (below > -80 || dx > 260)) { if (!a.jumpedT || a.t - a.jumpedT > 8) { o.jump = true; a.jumpedT = a.t; } return o; }
    if (f.airJumps === 0 && !f.upBUsed && f.vy > -2 && (below > -30 || dx > 230)) {
      o.special = true;
      if (f.def.sp.u === 'teleport') { const ang = Math.atan2(m.y - 20 - f.y, ledgeX - toward * 40 - f.x); o.x = Math.cos(ang); o.y = Math.sin(ang); if (o.y > -0.5) o.y = -0.6; }
      else o.y = -1;
      return o;
    }
    if (['dashStrike', 'sparkleDash'].includes(f.def.sp.s) && !f.sideB && dx > 200 && below < 0 && Math.random() < 0.05) { o.special = true; o.x = toward; return o; }
    return o;
  }
  if (!tgt) return o;
  const dx = tgt.x - f.x, dy = tgt.y - f.y, adx = Math.abs(dx);
  const range = 50 * f.def.reach * f.sc + 26;
  if (a.cool > 0) { a.cool--; o.x = a.mx || 0; if (S === 'shield' && !o.shield && a.holdShield > 0) o.shield = true; return edgeSafe(f, o); }
  const react = Math.max(3, 24 - lvl * 2.3);
  // defense
  if (tgt.state === 'attack' && adx < 120 && Math.abs(dy) < 100 && f.grounded && Math.random() < lvl * 0.02) {
    const r = Math.random();
    if (r < 0.55) { a.holdShield = 12 + rand(0, 18); o.shield = true; }
    else if (r < 0.75) { o.shield = true; o.x = -sgn(dx); }
    else { o.shield = true; o.y = 1; }
    a.cool = Math.floor(react); return o;
  }
  if (S === 'shield') {
    if (adx < 60 && Math.random() < 0.08 * lvl / 5) { o.grab = true; o.shield = true; }
    else if (tgt.state !== 'attack' || Math.random() < 0.02) { o.shield = false; a.holdShield = 0; }
    return o;
  }
  // projectile at range
  const hasProj = ['blaster', 'chargeShot', 'waveSlash', 'starToss', 'clogToss'].includes(f.def.sp.n);
  if (hasProj && adx > 260 && Math.abs(dy) < 120 && Math.random() < 0.02 + lvl * 0.004) {
    if (f.facing !== sgn(dx)) { o.x = sgn(dx); return o; }
    o.special = true; if (f.def.sp.n === 'chargeShot') a.holdSpecial = Math.floor(rand(5, 60));
    a.cool = Math.floor(react) + 10; return o;
  }
  if (f.def.sp.s === 'bomb' && adx > 160 && adx < 360 && Math.random() < 0.01 + lvl * 0.002) {
    if (f.facing !== sgn(dx)) { o.x = sgn(dx); return o; }
    o.special = true; o.x = sgn(dx); a.cool = Math.floor(react) + 10; return o;
  }
  const tgtOff = !tgt.grounded && (tgt.x < m.x1 || tgt.x > m.x2);
  if (adx > range || Math.abs(dy) > 90) {
    if (tgtOff && f.grounded) {
      // edge-guard: walk to the edge and wait
      const ex = tgt.x < 0 ? m.x1 + 40 : m.x2 - 40;
      o.x = Math.abs(f.x - ex) > 20 ? sgn(ex - f.x) : 0;
      if (tgt.state === 'ledge' && Math.abs(f.x - ex) < 60 && Math.random() < 0.08) { o.attack = true; o.y = 1; a.cool = react; }
      return o;
    }
    o.x = sgn(dx);
    if (dy < -110 && f.grounded && Math.random() < 0.04 + lvl * 0.005) { a.holdJump = Math.random() < 0.5 ? 6 : 2; o.jump = true; }
    if (!f.grounded && dy < -60 && f.airJumps > 0 && f.vy > 0 && Math.random() < 0.05) o.jump = true;
    if (f.grounded && adx > 140 && adx < 220 && Math.random() < 0.015 && ['dashStrike', 'sparkleDash'].includes(f.def.sp.s)) { o.special = true; a.cool = react; }
    a.mx = o.x;
    return edgeSafe(f, o);
  }
  // in range
  if (f.facing !== sgn(dx) && adx > 10 && f.grounded && Math.random() < 0.5) { o.x = sgn(dx); return o; }
  const r = Math.random();
  if (f.grounded) {
    const killPct = 95 - (tgt.def.weight - 100) * 0.6;
    if (dy < -55) { if (r < 0.45) { o.smash = true; o.sy = -1; } else { o.attack = true; o.y = -1; } }
    else if (tgt.state === 'shield' && r < 0.6) o.grab = true;
    else if (tgt.percent > killPct + rand(-20, 30) && r < 0.55) { o.smash = true; o.sx = sgn(dx); o.x = 0; }
    else if (r < 0.18) o.grab = true;
    else if (r < 0.45) { o.attack = true; o.x = sgn(dx) * 0.6; }
    else if (r < 0.58) { o.attack = true; o.y = 1; }
    else if (r < 0.66 && f.def.sp.d === 'quake') { o.special = true; o.y = 1; }
    else o.attack = true;
  } else {
    o.attack = true;
    if (dy < -40) o.y = -1; else if (dy > 40) o.y = 1; else o.x = sgn(dx) || f.facing;
  }
  a.cool = Math.floor(react + rand(0, 14 - lvl));
  a.mx = 0;
  return o;
}
function edgeSafe(f, o) {
  const m = G.stage.main;
  if (f.grounded && f.ground === G.mainSurf && ((f.x < m.x1 + 30 && o.x < 0) || (f.x > m.x2 - 30 && o.x > 0))) o.x = 0;
  return o;
}

// ---------------------------------------------------------------- effects
const fx = {
  add(p) { if (G.parts.length < 900) G.parts.push(p); },
  spark(x, y, color, n = 4, sp = 5) {
    for (let i = 0; i < n; i++) { const a = rand(0, Math.PI * 2), s = rand(sp * 0.3, sp); this.add({ k: 'spark', x, y, vx: Math.cos(a) * s, vy: Math.sin(a) * s, life: rand(10, 22), t: 0, color, size: rand(2, 4) }); }
  },
  hit(x, y, d, color) {
    const n = 6 + Math.min(14, d | 0);
    for (let i = 0; i < n; i++) { const a = rand(0, Math.PI * 2), s = rand(4, 8 + d * 0.5); this.add({ k: 'line', x, y, vx: Math.cos(a) * s, vy: Math.sin(a) * s, life: rand(8, 16), t: 0, color: i % 2 ? '#ffffff' : color, size: 2 + d * 0.12 }); }
    this.add({ k: 'flash', x, y, life: 7, t: 0, size: 26 + d * 2.4, color: '#ffffff' });
  },
  smoke(x, y, n = 3, color) {
    for (let i = 0; i < n; i++) this.add({ k: 'smoke', x: x + rand(-10, 10), y: y - rand(0, 8), vx: rand(-1.5, 1.5), vy: rand(-1.4, -0.2), life: rand(18, 34), t: 0, size: rand(6, 13), color: color || '#d9dcea' });
  },
  ring(x, y, color, size = 40) { this.add({ k: 'ring', x, y, life: 18, t: 0, size, color }); },
  boom(x, y, r) { this.add({ k: 'flash', x, y, life: 10, t: 0, size: r * 1.5, color: '#ffe08a' }); this.smoke(x, y, 10); this.spark(x, y, '#ffb347', 16, 9); },
  koBlast(x, y, color, port) {
    this.add({ k: 'ko', x, y, life: 50, t: 0, color, ang: Math.atan2(-150 - y, -x) });
    this.spark(x, y, color, 40, 22); this.spark(x, y, '#ffffff', 20, 16);
  },
};
function updateParts() {
  for (const p of G.parts) { p.t++; if (p.vx != null) { p.x += p.vx; p.y += p.vy; if (p.k === 'spark' || p.k === 'line') { p.vx *= 0.9; p.vy = p.vy * 0.9 + 0.15; } } }
  G.parts = G.parts.filter(p => p.t < p.life);
}

// ---------------------------------------------------------------- match flow
function startMatch(cfg) {
  G.cfg = cfg; G.frame = 0; G.projs = []; G.parts = []; G.ledgeOcc = {}; G.ending = 0; G.banner = null; G.paused = false;
  const st = STAGES[cfg.stage]; G.stage = st;
  G.mainSurf = { x1: st.main.x1, x2: st.main.x2, y: st.main.y, pass: false, main: true };
  G.plats = st.plats.map(p => ({ x1: p.x1, x2: p.x2, y: p.y, bx1: p.x1, bx2: p.x2, by: p.y, move: p.move, pass: true, dx: 0, dy: 0 }));
  G.surfs = [G.mainSurf, ...G.plats];
  const players = cfg.slots.map((s, i) => ({ s, i })).filter(o => o.s.type !== 'off');
  const n = players.length; const span = (st.main.x2 - st.main.x1) * 0.62;
  G.fighters = players.map((o, k) => {
    const def = o.s.def || FIGHTERS[o.s.fighter];
    const f = newFighter(def, o.i, o.s.type === 'cpu' ? aiCtrl() : humanCtrl(o.i), o.s.type === 'cpu', o.s.level);
    f.stocks = o.s.stocks || cfg.stocks; f.team = o.s.team || 0; f.spawnX = n === 1 ? 0 : -span / 2 + (span * k) / (n - 1);
    f.x = f.spawnX; f.y = 0; f.grounded = true; f.ground = G.mainSurf; f.facing = f.x > 0 ? -1 : 1; setState(f, 'idle');
    return f;
  });
  R3.buildStage(st, G.plats); R3.resetRigs();
  G.countdown = 200; G.cam = { x: 0, y: -60, z: 1.3 };
  G.mode = 'fight'; showScreen(null);
  Touch.on && document.getElementById('touch').classList.add('on');
}

function step() {
  G.frame++;
  if (G.countdown > 0) {
    G.countdown--;
    if (G.countdown === 180 || G.countdown === 120 || G.countdown === 60) Sfx.count();
    if (G.countdown === 0) Sfx.go();
  }
  // platforms
  for (const p of G.plats) {
    if (!p.move) continue;
    const m = p.move(G.frame); const nx1 = p.bx1 + m.dx, ny = p.by + m.dy;
    p.dx = nx1 - p.x1; p.dy = ny - p.y; p.x1 = nx1; p.x2 = p.bx2 + m.dx; p.y = ny;
    for (const f of G.fighters) if (f.grounded && f.ground === p) { f.x += p.dx; f.y = p.y; }
  }
  for (const f of G.fighters) readInput(f);
  for (const f of G.fighters) {
    if (G.countdown > 0) { if (f.state !== 'idle') setState(f, 'idle'); continue; }
    if (f.hitstop > 0) { f.hitstop--; if (f.shake > 0) f.shake--; continue; }
    f.shake = 0;
    update(f);
  }
  // soft push between grounded fighters standing on the same surface
  for (let i = 0; i < G.fighters.length; i++) for (let j = i + 1; j < G.fighters.length; j++) {
    const a = G.fighters[i], b = G.fighters[j];
    if (!a.grounded || !b.grounded || a.ground !== b.ground || a.state === 'grabbed' || b.state === 'grabbed' || a.state === 'holding' || b.state === 'holding') continue;
    const ov = (a.w + b.w) / 2 - Math.abs(a.x - b.x);
    if (ov > 0 && !a.hidden && !b.hidden) { const d = (a.x === b.x ? (i % 2 ? 1 : -1) : sgn(a.x - b.x)) * Math.min(ov, 3) * 0.5; a.x += d; b.x -= d; }
  }
  updateProjs();
  if (G.countdown <= 0) checkHits();
  // advance move timers
  for (const f of G.fighters) {
    if (f.state !== 'attack' || f.hitstop > 0) continue;
    if (!f.mtHold) f.mt++;
    if (f.state === 'attack' && f.mt >= f.move.len) endMove(f);
  }
  for (const f of G.fighters) checkBlast(f);
  updateParts();
  if (G.shake > 0) G.shake *= 0.86; if (G.shake < 0.3) G.shake = 0;
  if (G.flash > 0) G.flash--;
  if (G.banner && --G.banner.t <= 0) G.banner = null;
  // end
  if (!G.ending) {
    const left = G.fighters.filter(alive), sides = new Set(left.map(f => f.team || 'solo' + f.port));
    if (G.cfg.training ? false : G.fighters.length > 1 ? sides.size <= 1 : left.length === 0) { G.ending = 1; G.banner = { text: 'GAME!', t: 999, color: '#ffcf3f', big: true }; Sfx.boom(); }
  } else if (++G.ending > 150) showResults();
}

// ---------------------------------------------------------------- render
const cv = document.getElementById('c'); let ctx = cv.getContext('2d'); const mainCtx = ctx;
let VW = 0, VH = 0, DPR = 1;
function resize() { DPR = Math.min(2, window.devicePixelRatio || 1); VW = innerWidth; VH = innerHeight; cv.width = VW * DPR; cv.height = VH * DPR; }
addEventListener('resize', resize); resize();

function updateCam() {
  if (G.camLock) { Object.assign(G.cam, G.camLock); return; } // debug / photo mode
  const st = G.stage, b = st.blast;
  let x1 = 1e9, x2 = -1e9, y1 = 1e9, y2 = -1e9, n = 0;
  for (const f of G.fighters) {
    if (f.state === 'dead' || f.state === 'out') continue;
    const fx_ = clamp(f.x, b.l * 0.75, b.r * 0.75), fy = clamp(f.y, b.t * 0.7, b.b * 0.8);
    x1 = Math.min(x1, fx_); x2 = Math.max(x2, fx_); y1 = Math.min(y1, fy - f.h); y2 = Math.max(y2, fy); n++;
  }
  // frame the fighters closely; only widen when they spread out
  if (!n) { x1 = st.main.x1 * 0.5; x2 = st.main.x2 * 0.5; y1 = -200; y2 = 0; }
  const padX = 70, padY = 65;
  const w = Math.max(340, x2 - x1 + padX * 2), h = Math.max(260, y2 - y1 + padY * 2);
  const base = VH / 720;
  const z = clamp(Math.min(VW / (w * base), VH / (h * base)), 0.5, 3.2);
  const c = G.cam;
  c.x = lerp(c.x, (x1 + x2) / 2, 0.1); c.y = lerp(c.y, (y1 + y2) / 2 - 12, 0.1); c.z = lerp(c.z, z, z < c.z ? 0.12 : 0.05);
}
function camScale() { return G.cam.z * (VH / 720); }
function w2s(x, y) { const s = camScale(); return { x: (x - G.cam.x) * s + VW / 2, y: (y - G.cam.y) * s + VH / 2 }; }

function render() {
  ctx.setTransform(DPR, 0, 0, DPR, 0, 0);
  if (G.mode !== 'fight' || !G.stage) { drawMenuBg(); if (!R3.menuRender()) R3.show(false); return; }
  updateCam();
  const sh = G.shake, shx = rand(-sh, sh), shy = rand(-sh, sh);
  R3.show(true);
  ctx.clearRect(0, 0, VW, VH);
  if (!R3.render(shx, shy)) { ctx.fillStyle = '#141a33'; ctx.fillRect(0, 0, VW, VH); bigText('3D needs WebGL', '#fff', 40); return; }
  const s = camScale();
  ctx.save();
  ctx.translate(VW / 2 + shx, VH / 2 + shy);
  ctx.scale(s, s); ctx.translate(-G.cam.x, -G.cam.y);
  for (const f of G.fighters) if (f.state === 'respawn') drawRespawnPlat(f);
  for (const p of G.projs) drawProj(p);
  for (const f of G.fighters) drawFighterFX(f);
  drawParts();
  ctx.restore();
  drawKOBeams();
  drawOffscreen();
  drawHUD();
  if (G.flash > 0) { ctx.fillStyle = `rgba(255,255,255,${G.flash * 0.05})`; ctx.fillRect(0, 0, VW, VH); }
  drawBanner();
}

function drawMenuBg() {
  // the menus sit over a live 3D scene, so the overlay canvas only needs clearing
  ctx.clearRect(0, 0, VW, VH);
}
function drawRespawnPlat(f) {
  ctx.save(); ctx.translate(f.x, f.y);
  ctx.fillStyle = f.def.color; ctx.globalAlpha = 0.85;
  ctx.beginPath(); ctx.ellipse(0, 4, 46, 9, 0, 0, Math.PI * 2); ctx.fill();
  ctx.globalAlpha = 0.25; ctx.beginPath(); ctx.moveTo(-40, 4); ctx.lineTo(40, 4); ctx.lineTo(10, 80); ctx.lineTo(-10, 80); ctx.fill();
  ctx.restore(); ctx.globalAlpha = 1;
}
function drawProj(p) {
  if (p.invisible) return;
  const c = p.owner.def.accent;
  ctx.save(); ctx.translate(p.x, p.y);
  if (p.type === 'bolt') { ctx.fillStyle = c; ctx.shadowColor = c; ctx.shadowBlur = 14; ctx.fillRect(-14, -3, 28, 6); }
  else if (p.type === 'orb') {
    ctx.shadowColor = c; ctx.shadowBlur = 20; ctx.fillStyle = c; ctx.beginPath(); ctx.arc(0, 0, p.r, 0, Math.PI * 2); ctx.fill();
    ctx.fillStyle = '#fff'; ctx.beginPath(); ctx.arc(0, 0, p.r * 0.5, 0, Math.PI * 2); ctx.fill();
  } else if (p.type === 'wave') {
    ctx.strokeStyle = c; ctx.lineWidth = 5; ctx.shadowColor = c; ctx.shadowBlur = 12;
    ctx.beginPath(); ctx.arc(-sgn(p.vx) * 10, 0, p.r, sgn(p.vx) > 0 ? -1.2 : Math.PI - 1.2 + 0.4, sgn(p.vx) > 0 ? 1.2 : Math.PI + 1.2 - 0.4); ctx.stroke();
  } else if (p.type === 'star') {
    ctx.rotate(p.t * 0.5); ctx.shadowColor = '#a65fe0'; ctx.shadowBlur = 16; ctx.fillStyle = '#ff6ad5';
    ctx.beginPath(); for (let i = 0; i < 10; i++) { const r = i % 2 ? p.r * 0.45 : p.r * 1.2, a = i * Math.PI / 5; ctx.lineTo(Math.cos(a) * r, Math.sin(a) * r); } ctx.fill();
    ctx.fillStyle = '#fff'; ctx.beginPath(); ctx.arc(0, 0, p.r * 0.3, 0, Math.PI * 2); ctx.fill();
  } else if (p.type === 'clog') {
    ctx.rotate(p.t * 0.4 * sgn(p.vx)); ctx.fillStyle = '#0e5f5e'; ctx.beginPath(); ctx.ellipse(0, 0, p.r * 1.25, p.r * 0.72, 0, 0, Math.PI * 2); ctx.fill();
    ctx.fillStyle = '#1f9c9a'; ctx.beginPath(); ctx.ellipse(0, -1, p.r * 1.1, p.r * 0.58, 0, 0, Math.PI * 2); ctx.fill();
    ctx.fillStyle = '#0e5f5e'; for (let k = -1; k <= 1; k++) { ctx.beginPath(); ctx.arc(k * 4 + 3, -2, 1.3, 0, Math.PI * 2); ctx.fill(); }
  } else if (p.type === 'bomb') {
    ctx.fillStyle = '#2a2140'; ctx.beginPath(); ctx.arc(0, 0, p.r, 0, Math.PI * 2); ctx.fill();
    ctx.fillStyle = G.frame % 10 < 5 ? c : '#ff5050'; ctx.beginPath(); ctx.arc(0, -p.r * 0.2, p.r * 0.4, 0, Math.PI * 2); ctx.fill();
  }
  ctx.restore();
}
function drawParts() {
  for (const p of G.parts) {
    const k = 1 - p.t / p.life;
    if (p.k === 'spark') { ctx.fillStyle = p.color; ctx.globalAlpha = k; ctx.fillRect(p.x - p.size / 2, p.y - p.size / 2, p.size, p.size); }
    else if (p.k === 'line') { ctx.strokeStyle = p.color; ctx.globalAlpha = k; ctx.lineWidth = p.size; ctx.beginPath(); ctx.moveTo(p.x, p.y); ctx.lineTo(p.x - p.vx * 2.2, p.y - p.vy * 2.2); ctx.stroke(); }
    else if (p.k === 'smoke') { ctx.fillStyle = p.color; ctx.globalAlpha = k * 0.5; ctx.beginPath(); ctx.arc(p.x, p.y, p.size * (1.4 - k * 0.6), 0, Math.PI * 2); ctx.fill(); }
    else if (p.k === 'flash') { ctx.fillStyle = p.color; ctx.globalAlpha = k * 0.9; ctx.beginPath(); ctx.arc(p.x, p.y, p.size * (0.6 + (1 - k) * 0.6), 0, Math.PI * 2); ctx.fill(); }
    else if (p.k === 'ring') { ctx.strokeStyle = p.color; ctx.globalAlpha = k; ctx.lineWidth = 4 * k + 1; ctx.beginPath(); ctx.arc(p.x, p.y, p.size * (0.4 + (1 - k) * 1.2), 0, Math.PI * 2); ctx.stroke(); }
  }
  ctx.globalAlpha = 1;
}
function drawKOBeams() {
  for (const p of G.parts) {
    if (p.k !== 'ko') continue;
    const k = 1 - p.t / p.life; const sp = w2s(p.x, p.y);
    const ex = clamp(sp.x, 0, VW), ey = clamp(sp.y, 0, VH);
    const len = Math.max(VW, VH) * 0.9 * (0.4 + (1 - k) * 0.8);
    ctx.save(); ctx.translate(ex, ey); ctx.rotate(p.ang);
    const g = ctx.createLinearGradient(0, 0, len, 0); g.addColorStop(0, p.color); g.addColorStop(1, 'rgba(255,255,255,0)');
    ctx.globalAlpha = k; ctx.fillStyle = g;
    const wdt = 90 * k + 10;
    ctx.beginPath(); ctx.moveTo(0, -wdt * 0.2); ctx.lineTo(len, -wdt); ctx.lineTo(len, wdt); ctx.lineTo(0, wdt * 0.2); ctx.fill();
    ctx.fillStyle = '#fff'; ctx.globalAlpha = k * 0.8; ctx.beginPath(); ctx.moveTo(0, -wdt * 0.06); ctx.lineTo(len * 0.8, -wdt * 0.25); ctx.lineTo(len * 0.8, wdt * 0.25); ctx.lineTo(0, wdt * 0.06); ctx.fill();
    ctx.restore();
  }
  ctx.globalAlpha = 1;
}

// --- fighter drawing
function computePose(f) {
  const S = f.state, a = f.anim;
  let pose = POSES.idle, rot = 0;
  const swing = (amp, sp) => {
    const ph = a * sp, s = Math.sin(ph), c = Math.cos(ph);
    return P(0.12 + amp * 0.18, 0.5 - s * amp, 0.9, -0.3 + s * amp, 0.8, 0.3 + s * amp * 0.9, -0.4 - Math.max(0, -c) * amp * 1.3, 0.3 - s * amp * 0.9, -0.4 - Math.max(0, c) * amp * 1.3);
  };
  switch (S) {
    case 'idle': case 'shieldDrop': case 'getup': pose = lerpPose(POSES.idle, POSES.crouch, 0.08 + 0.06 * Math.sin(a * 0.08)); break;
    case 'walk': pose = swing(0.6, 0.22); break;
    case 'run': pose = swing(1.05, 0.34); break;
    case 'crouch': case 'jumpsquat': case 'land': pose = POSES.crouch; break;
    case 'air': pose = f.vy < 0 ? POSES.jumpUp : POSES.fall; if (f.flipT > 0) { rot = (1 - f.flipT / 24) * Math.PI * 2 * f.facing; pose = POSES.tuck; } break;
    case 'helpless': pose = POSES.helpless; break;
    case 'attack': {
      const mv = f.move; const strike = POSES[mv.pose] || POSES.jab;
      const hbs = mv.hb.concat(f.xhb || []);
      let s0 = hbs.length ? Math.min(...hbs.map(h => h.f[0])) : Math.min(8, mv.len / 3);
      let s1 = hbs.length ? Math.max(...hbs.map(h => Math.min(h.f[1], mv.len - 4))) : mv.len * 0.6;
      if (mv.name === 'nspec' || mv.name === 'sspec' || mv.name === 'dspec') { s0 = Math.min(s0, 8); }
      const base = f.grounded ? POSES.idle : POSES.fall;
      const wind = mv.wind ? POSES[mv.wind] : base;
      const t = f.mt;
      if (t < s0) pose = lerpPose(base, wind, Math.min(1, t / Math.max(1, s0 * 0.7)));
      if (t < s0 && t >= s0 * 0.7) pose = lerpPose(wind, strike, (t - s0 * 0.7) / Math.max(1, s0 * 0.3));
      if (t >= s0 && t <= s1) pose = strike;
      if (t > s1) pose = lerpPose(strike, base, Math.min(1, (t - s1) / Math.max(1, mv.len - s1)));
      if (mv.name === 'nair' && t >= 3 && t < 20) rot = (t / 20) * Math.PI * 2 * f.facing * 0.5;
      if (mv.pose === 'spin') rot = f.mt * 0.6 * f.facing;
      if (mv.rollSpin && f.mt >= 7 && f.mt < 30) rot = f.mt * 0.55 * f.facing;
      if (f.mtHold && mv.charge) pose = wind;
      break;
    }
    case 'hitstun': pose = POSES.hurt; if (f.tumble) rot = f.anim * 0.35 * -f.facing; break;
    case 'shield': case 'shieldstun': pose = POSES.shield; break;
    case 'dizzy': pose = lerpPose(POSES.idle, POSES.hurt, 0.5 + 0.3 * Math.sin(a * 0.1)); rot = Math.sin(a * 0.07) * 0.2; break;
    case 'roll': case 'tech': pose = POSES.tuck; rot = (f.t / 30) * Math.PI * 2 * (f.rollDir || f.techDir || 1); break;
    case 'spotdodge': pose = POSES.crouch; break;
    case 'airdodge': pose = POSES.tuck; break;
    case 'down': pose = POSES.hurt; rot = -Math.PI / 2 * f.facing; break;
    case 'ledge': pose = POSES.ledge; break;
    case 'ledgeclimb': pose = lerpPose(POSES.ledge, POSES.crouch, f.t / 22); break;
    case 'holding': pose = POSES.grab; break;
    case 'grabbed': pose = POSES.hurt; break;
    case 'respawn': pose = POSES.idle; break;
  }
  return { pose, rot };
}

// where the 3D body is drawn from: feet position (with ledge-climb slide and hit shake) and hip height
function fighterAnchor(f, pose) {
  const sc = f.sc, L = 18 * sc;
  const fdy = (h, k) => L * (Math.cos(h) + Math.cos(h + k));
  let hipH = (f.grounded || f.state === 'ledgeclimb') ? clamp(Math.max(fdy(pose[5], pose[6]), fdy(pose[7], pose[8])), 14 * sc, 36 * sc) : 36 * sc;
  if (f.state === 'down') hipH = 12 * sc;
  if (f.state === 'ledgeclimb') hipH = lerp(36 * sc, 20 * sc, f.t / 22);
  let ox = f.x, oy = f.y;
  if (f.state === 'ledgeclimb') { ox = lerp(f.x, f.climbTo.x, f.t / 22); oy = lerp(f.y, f.climbTo.y, Math.min(1, f.t / 14)); }
  if (f.shake > 0) ox += rand(-3, 3);
  return { ox, oy, hipH };
}
// 2D overlay effects on top of the 3D fighter: attack swoosh, shield bubble, rings, player tag
function drawFighterFX(f) {
  if (f.state === 'dead' || f.state === 'out' || f.hidden) return;
  const d = f.def, sc = f.sc;
  for (const h of activeHBsForDraw(f)) {
    const w = hbWorld(f, h);
    ctx.fillStyle = d.accent; ctx.globalAlpha = 0.28; ctx.beginPath(); ctx.arc(w.x, w.y, w.r, 0, Math.PI * 2); ctx.fill();
    ctx.globalAlpha = 0.6; ctx.strokeStyle = '#ffffff'; ctx.lineWidth = 3; ctx.beginPath(); ctx.arc(w.x, w.y, w.r * 0.8, -1 + f.mt * 0.5, 1 + f.mt * 0.5); ctx.stroke(); ctx.globalAlpha = 1;
  }
  if (f.state === 'shield' || f.state === 'shieldstun') {
    const r = 46 * sc * (0.35 + 0.65 * f.shieldHP / 50);
    ctx.fillStyle = PORT_COLORS[f.port]; ctx.globalAlpha = 0.35; ctx.beginPath(); ctx.arc(f.x, f.y - f.h * 0.5, r, 0, Math.PI * 2); ctx.fill();
    ctx.globalAlpha = 0.8; ctx.strokeStyle = '#ffffff'; ctx.lineWidth = 2; ctx.stroke(); ctx.globalAlpha = 1;
  }
  if (f.state === 'dizzy') { ctx.fillStyle = '#ffe14d'; for (let k = 0; k < 3; k++) { const aa = G.frame * 0.1 + k * 2.1; ctx.beginPath(); ctx.arc(f.x + Math.cos(aa) * 18, f.y - f.h - 8 + Math.sin(aa) * 4, 3.5, 0, Math.PI * 2); ctx.fill(); } }
  if (f.counter) { ctx.strokeStyle = '#ffffff'; ctx.globalAlpha = 0.6 + 0.4 * Math.sin(G.frame); ctx.lineWidth = 3; ctx.beginPath(); ctx.arc(f.x, f.y - f.h / 2, f.h * 0.6, 0, Math.PI * 2); ctx.stroke(); ctx.globalAlpha = 1; }
  if (f.charge > 0 && f.state === 'attack') { ctx.strokeStyle = d.accent; ctx.lineWidth = 3; ctx.globalAlpha = 0.7; ctx.beginPath(); ctx.arc(f.x, f.y - f.h / 2, f.h * 0.4 + Math.sin(G.frame * 0.5) * 4, 0, Math.PI * 2); ctx.stroke(); ctx.globalAlpha = 1; }
  ctx.fillStyle = PORT_COLORS[f.port]; ctx.font = 'bold 15px Trebuchet MS, sans-serif'; ctx.textAlign = 'center';
  const ty = f.y - f.h - 26;
  ctx.lineWidth = 3; ctx.strokeStyle = 'rgba(0,0,0,.6)'; ctx.strokeText(f.cpu ? 'CPU' : 'P' + (f.port + 1), f.x, ty);
  ctx.fillText(f.cpu ? 'CPU' : 'P' + (f.port + 1), f.x, ty);
  ctx.beginPath(); ctx.moveTo(f.x - 6, ty + 4); ctx.lineTo(f.x + 6, ty + 4); ctx.lineTo(f.x, ty + 11); ctx.fill();
}
function activeHBsForDraw(f) { if (f.state !== 'attack' || !f.move) return []; return f.move.hb.concat(f.xhb || []).filter(h => !h.grab && f.mt >= h.f[0] && f.mt <= h.f[1]); }

function drawOffscreen() {
  for (const f of G.fighters) {
    if (f.state === 'dead' || f.state === 'out') continue;
    const p = w2s(f.x, f.y - f.h / 2);
    const m = 34;
    if (p.x > -10 && p.x < VW + 10 && p.y > -10 && p.y < VH - 90) continue;
    const cx = clamp(p.x, m, VW - m), cy = clamp(p.y, m, VH - 130);
    ctx.fillStyle = 'rgba(10,12,24,.8)'; ctx.strokeStyle = PORT_COLORS[f.port]; ctx.lineWidth = 3;
    ctx.beginPath(); ctx.arc(cx, cy, 24, 0, Math.PI * 2); ctx.fill(); ctx.stroke();
    ctx.fillStyle = f.def.color; ctx.beginPath(); ctx.arc(cx, cy, 11, 0, Math.PI * 2); ctx.fill();
    const a = Math.atan2(p.y - cy, p.x - cx);
    ctx.fillStyle = PORT_COLORS[f.port]; ctx.beginPath(); ctx.moveTo(cx + Math.cos(a) * 34, cy + Math.sin(a) * 34); ctx.lineTo(cx + Math.cos(a + 0.4) * 24, cy + Math.sin(a + 0.4) * 24); ctx.lineTo(cx + Math.cos(a - 0.4) * 24, cy + Math.sin(a - 0.4) * 24); ctx.fill();
  }
}
function pctColor(p) { const k = Math.min(p, 160) / 160; return `hsl(${50 - k * 50}, 100%, ${100 - k * 52}%)`; }
function drawHUD() {
  // on short screens (phones held sideways) the HUD shrinks so it does not cover the fight
  const hs = clamp(VH / 600, 0.62, 1); ctx.save(); ctx.translate(VW / 2, VH); ctx.scale(hs, hs); ctx.translate(-VW / 2, -VH);
  const fs = G.fighters, n = fs.length;
  const cw = Math.min(210, (VW - 40) / n - 10), chh = 74;
  const total = n * cw + (n - 1) * 12; let x0 = (VW - total) / 2;
  const y0 = VH - chh - 12;
  for (const f of fs) {
    const x = x0; x0 += cw + 12;
    ctx.fillStyle = 'rgba(10,12,24,.72)'; ctx.fillRect(x, y0, cw, chh);
    ctx.fillStyle = PORT_COLORS[f.port]; ctx.fillRect(x, y0, 5, chh);
    // portrait
    // narrow cards (phones held upright, 4 players): smaller portrait, shorter name, text shrunk to fit the card
    const narrow = cw < 175, pr = narrow ? 13 : 19, px = narrow ? x + 22 : x + 32, tx = narrow ? x + 40 : x + 58;
    ctx.fillStyle = f.def.color; ctx.beginPath(); ctx.arc(px, y0 + 32, pr, 0, Math.PI * 2); ctx.fill();
    ctx.fillStyle = '#f4f7ff'; ctx.fillRect(px, y0 + 32 - pr * 0.26, pr * 0.74, pr * 0.32);
    ctx.textAlign = 'left'; ctx.fillStyle = '#fff';
    const label = f.def.name + (f.cpu && !narrow ? ' · CPU ' + f.lvl : ''), room = x + cw - 6 - tx;
    let fz = 12; ctx.font = `bold ${fz}px Trebuchet MS, sans-serif`; while (fz > 7 && ctx.measureText(label).width > room) { fz--; ctx.font = `bold ${fz}px Trebuchet MS, sans-serif`; }
    ctx.fillText(label, tx, y0 + 16);
    if (alive(f)) {
      const sh = f.pctShake > 0 ? rand(-3, 3) : 0;
      ctx.font = `italic 900 ${Math.min(36, Math.max(20, (x + cw - tx) * 0.36))}px Trebuchet MS, sans-serif`;
      ctx.fillStyle = '#000'; ctx.fillText(f.percent.toFixed(0) + '%', tx + 2 + sh + 2, y0 + 50 + sh + 2);
      ctx.fillStyle = pctColor(f.percent); ctx.fillText(f.percent.toFixed(0) + '%', tx + 2 + sh, y0 + 50 + sh);
    } else { ctx.fillStyle = '#8b95b8'; ctx.font = 'bold 18px Trebuchet MS'; ctx.fillText('OUT', tx + 2, y0 + 48); }
    // stocks
    for (let k = 0; k < Math.min(f.stocks, 6); k++) { ctx.fillStyle = f.def.color; ctx.beginPath(); ctx.arc(x + (narrow ? 11 : 14) + k * (narrow ? 8 : 12), y0 + 60, narrow ? 3.2 : 4.5, 0, Math.PI * 2); ctx.fill(); }
    // meter
    const mx = narrow ? tx - 4 : x + 60, mw = x + cw - 8 - mx, my = y0 + 60;
    ctx.fillStyle = '#222842'; ctx.fillRect(mx, my, mw, 7);
    const full = f.meter >= 100 && f.od <= 0;
    ctx.fillStyle = f.od > 0 ? f.def.accent : full ? (G.frame % 20 < 10 ? '#ffffff' : f.def.accent) : '#6c7bd8';
    ctx.fillRect(mx, my, mw * (f.od > 0 ? f.od / 360 : f.meter / 100), 7);
    if (full) { ctx.fillStyle = '#fff'; ctx.font = 'bold 9px Trebuchet MS'; ctx.fillText(mw > 100 ? 'OVERDRIVE READY' : 'READY!', mx, my - 2); }
  }
  ctx.restore();
  // countdown
  if (G.countdown > 0) {
    const n2 = Math.ceil(G.countdown / 60) - 0;
    const txt = G.countdown > 20 ? String(Math.min(3, Math.ceil((G.countdown - 20) / 60))) : '';
    bigText(txt || 'GO!', '#ffcf3f', 120);
  } else if (G.frame < 230 && G.countdown === 0 && G.frame > 200) bigText('GO!', '#ffcf3f', 120);
}
function bigText(t, color, size) {
  ctx.textAlign = 'center'; ctx.font = `italic 900 ${size}px Trebuchet MS, sans-serif`;
  ctx.lineWidth = 10; ctx.strokeStyle = '#000'; ctx.strokeText(t, VW / 2, VH / 2);
  ctx.fillStyle = color; ctx.fillText(t, VW / 2, VH / 2);
}
function drawBanner() {
  const b = G.banner; if (!b) return;
  if (b.big) { bigText(b.text, b.color, 140); return; }
  ctx.textAlign = 'center'; ctx.font = 'italic 900 40px Trebuchet MS, sans-serif';
  ctx.lineWidth = 7; ctx.strokeStyle = '#000'; ctx.strokeText(b.text, VW / 2, 90); ctx.fillStyle = b.color; ctx.fillText(b.text, VW / 2, 90);
}

// ---------------------------------------------------------------- menus
const $ = id => document.getElementById(id);
function showScreen(id) { for (const s of document.querySelectorAll('.screen')) s.classList.toggle('on', s.id === id); if (id) $('touch').classList.remove('on'); }
const cfg = {
  stage: 'skyforge', stocks: 3,
  slots: [{ type: 'human', fighter: 0, level: 5 }, { type: 'cpu', fighter: 6, level: 5 }, { type: 'cpu', fighter: 7, level: 5 }, { type: 'off', fighter: 3, level: 5 }],
};
function buildSelect() {
  const wrap = $('slots'); wrap.innerHTML = '';
  cfg.slots.forEach((s, i) => {
    const d = FIGHTERS[s.fighter];
    const el = document.createElement('div'); el.className = 'slot' + (s.type === 'off' ? ' off' : '');
    el.innerHTML = `<div class="tag" style="background:${PORT_COLORS[i]}">P${i + 1}</div>
      <div class="stats">${[['SPEED', d.run / 11.6], ['POWER', (d.power - 0.8) / 0.45], ['WEIGHT', (d.weight - 70) / 65], ['AIR', d.jumps / 5]].map(([n, v]) => `<span>${n}</span><i><b style="width:${Math.round(clamp(v, 0.1, 1) * 100)}%;background:${d.color}"></b></i>`).join('')}</div>
      <div class="row"><button data-a="prev">◀</button><div class="nm" style="color:${d.color};min-width:90px">${d.name}</div><button data-a="next">▶</button></div>
      <div class="ds">${d.blurb}</div>
      <div class="row">${['human', 'cpu', 'off'].map(t => `<button data-t="${t}" class="${s.type === t ? 'on' : ''}">${t === 'human' ? 'PLAYER' : t.toUpperCase()}</button>`).join('')}</div>
      <div class="row" ${s.type === 'cpu' ? '' : 'style="visibility:hidden"'}><button data-a="ldn">−</button><span>LV ${s.level}</span><button data-a="lup">+</button></div>`;
    el.onclick = e => {
      const b = e.target.closest('button'); if (!b) return; Sfx.init(); Sfx.blip();
      if (b.dataset.t) s.type = b.dataset.t;
      if (b.dataset.a === 'prev') s.fighter = (s.fighter + FIGHTERS.length - 1) % FIGHTERS.length;
      if (b.dataset.a === 'next') s.fighter = (s.fighter + 1) % FIGHTERS.length;
      if (b.dataset.a === 'lup') s.level = Math.min(9, s.level + 1);
      if (b.dataset.a === 'ldn') s.level = Math.max(1, s.level - 1);
      buildSelect();
    };
    wrap.appendChild(el);
  });
  $('optStage').innerHTML = Object.entries(STAGES).map(([k, v]) => `<button data-s="${k}" class="${cfg.stage === k ? 'on' : ''}">${v.name}</button>`).join('');
  $('optStock').innerHTML = [1, 2, 3, 4, 5].map(n => `<button data-n="${n}" class="${cfg.stocks === n ? 'on' : ''}">${n}</button>`).join('');
}
$('optStage').onclick = e => { const b = e.target.closest('button'); if (b) { cfg.stage = b.dataset.s; Sfx.blip(); buildSelect(); } };
$('optStock').onclick = e => { const b = e.target.closest('button'); if (b) { cfg.stocks = +b.dataset.n; Sfx.blip(); buildSelect(); } };
function drawPortrait(c, def) { R3.portrait(c, def); }

$('bPlay').onclick = () => { Sfx.init(); Sfx.blip(); showScreen('hub'); };
function startDemo() {
  Sfx.init(); startMatch({ stage: ['skyforge', 'driftway', 'proving'][Math.floor(Math.random() * 3)], stocks: 3,
    slots: [0, 1, 2, 3].map(i => ({ type: 'cpu', fighter: Math.floor(Math.random() * 9), level: 6 + (i % 3) })) });
}
$('bBack').onclick = () => { Sfx.blip(); showScreen('hub'); G.mode = 'title'; };
$('bFight').onclick = () => {
  const n = cfg.slots.filter(s => s.type !== 'off').length;
  if (n < 2) { const b = $('bFight'); b.textContent = 'TURN ON 2+'; setTimeout(() => { b.textContent = 'FIGHT!'; }, 1400); return; }
  Sfx.init(); startMatch(JSON.parse(JSON.stringify(cfg)));
};
function togglePause() {
  if (G.ending) return;
  G.paused = !G.paused; showScreen(G.paused ? 'pause' : null);
  if (!G.paused && Touch.on) $('touch').classList.add('on');
}
$('bResume').onclick = togglePause;
$('bQuit').onclick = () => { G.paused = false; G.mode = 'title'; showScreen('hub'); };
$('bAgain').onclick = () => startMatch(G.cfg);
$('bMenu').onclick = () => { G.mode = 'title'; buildSelect(); showScreen('select'); };

function showResults() {
  G.mode = 'results';
  const order = [...G.fighters].sort((a, b) => (b.stocks - a.stocks) || (b.outT - a.outT) || (a.percent - b.percent));
  const winner = order[0];
  $('resTitle').textContent = winner ? winner.def.name + ' WINS!' : 'RESULTS';
  $('resTitle').style.filter = 'none';
  $('resTable').innerHTML = `<tr><th>PLACE</th><th>FIGHTER</th><th>KOs</th><th>FALLS</th><th>SDs</th><th>DAMAGE</th></tr>` +
    order.map((f, i) => `<tr><td class="place" style="color:${PORT_COLORS[f.port]}">${i + 1}</td><td style="color:${f.def.color};font-weight:900">${f.def.name} <span style="color:#8b95b8;font-weight:400">${f.cpu ? 'CPU' : 'P' + (f.port + 1)}</span></td><td>${f.stats.kos}</td><td>${f.stats.falls}</td><td>${f.stats.sds}</td><td>${f.stats.dealt.toFixed(0)}%</td></tr>`).join('');
  showScreen('results');
}

// ---------------------------------------------------------------- touch controls
(function setupTouch() {
  const coarse = window.matchMedia && matchMedia('(pointer: coarse)').matches;
  Touch.on = coarse;
  const stick = $('stick'), knob = $('knob');
  const setStick = (cx, cy) => {
    const r = stick.getBoundingClientRect(); const ox = r.left + r.width / 2, oy = r.top + r.height / 2;
    let dx = (cx - ox) / (r.width / 2), dy = (cy - oy) / (r.height / 2); const m = Math.hypot(dx, dy); if (m > 1) { dx /= m; dy /= m; }
    Touch.st.x = Math.abs(dx) < 0.2 ? 0 : dx; Touch.st.y = Math.abs(dy) < 0.25 ? 0 : dy;
    knob.style.left = 45 + dx * 45 + 'px'; knob.style.top = 45 + dy * 45 + 'px';
  };
  stick.addEventListener('pointerdown', e => { Sfx.init(); Touch.stickId = e.pointerId; stick.setPointerCapture(e.pointerId); setStick(e.clientX, e.clientY); });
  stick.addEventListener('pointermove', e => { if (e.pointerId === Touch.stickId) setStick(e.clientX, e.clientY); });
  const endStick = e => { if (e.pointerId !== Touch.stickId) return; Touch.stickId = null; Touch.st.x = Touch.st.y = 0; knob.style.left = knob.style.top = '45px'; };
  stick.addEventListener('pointerup', endStick); stick.addEventListener('pointercancel', endStick);
  for (const b of document.querySelectorAll('#tbtns button')) {
    const k = b.dataset.b;
    b.addEventListener('pointerdown', e => { Sfx.init(); Touch.st[k] = true; b.classList.add('down'); b.setPointerCapture(e.pointerId); e.preventDefault(); });
    const up = () => { Touch.st[k] = false; b.classList.remove('down'); };
    b.addEventListener('pointerup', up); b.addEventListener('pointercancel', up);
  }
  $('tpause').addEventListener('click', () => { if (G.mode === 'fight') togglePause(); });
})();

// ---------------------------------------------------------------- main loop
let last = performance.now(), acc = 0;
function loop(now) {
  requestAnimationFrame(loop);
  const dt = Math.min(100, now - last); last = now;
  // the game runs at G.speed (0.8 by default, a bit calmer than full speed); fighters are drawn between steps so it stays smooth
  const stepMs = 1000 / (60 * (G.speed || 0.8));
  if (G.mode === 'fight' && !G.paused) { acc += dt; let n = 0; while (acc >= stepMs && n < 4) { for (const f of G.fighters) { f.px = f.x; f.py = f.y; } step(); acc -= stepMs; n++; } if (n === 4) acc = 0; G.alpha = acc / stepMs; } else G.alpha = 1;
  render();
}
requestAnimationFrame(loop);
// scanned family models load in the background; fighters use the simple shapes until they are ready
if (typeof R3 !== 'undefined') R3.loadModels(() => { G.modelsReady = true; if (typeof refreshMenuScene === 'function') refreshMenuScene(); if (document.getElementById('select').classList.contains('on')) buildSelect(); if (G.mode === 'fight') R3.resetRigs(); });

// test hooks
window.__clash = { G, startMatch, step, FIGHTERS, STAGES, cfg };
