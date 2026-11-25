'use strict';
// Super Family Smash: item boxes, weapons, power-ups, the Super Orb and the Family Finale cinematic attacks.
// Loaded after game.js and fx.js; it wraps a few game.js functions (step, startMatch, groundActions, airActions,
// applyHit, ko, aiPoll, updateCam, render) instead of changing them, and draws its 3D models through an R3 hook.
(() => {
  const TAU = Math.PI * 2;
  const snd = (n, v, r) => { if (Sfx.sample) Sfx.sample(n, v, r); };

  // ------------------------------------------------------------ what can come out of a box
  const ITEMS = {
    bat:      { name: 'Slugger Bat', kind: 'weapon', w: 3, r: 18 },
    blaster:  { name: 'Foam Blaster', kind: 'weapon', w: 3, r: 18, ammo: 16 },
    balloon:  { name: 'Water Balloon', kind: 'throw', w: 3, r: 16 },
    mallet:   { name: 'Mega Mallet', kind: 'mallet', w: 1.2, r: 20, dur: 480 },
    candy:    { name: 'Star Candy', kind: 'power', w: 1.5, r: 18, dur: 480 },
    juice:    { name: 'Grow Juice', kind: 'power', w: 1.5, r: 16, dur: 600 },
    sneakers: { name: 'Zoom Sneakers', kind: 'power', w: 2, r: 16, dur: 600 },
    cookie:   { name: 'Heart Cookie', kind: 'power', w: 2, r: 16 },
  };
  const BOXES = { crate: { hp: 18, r: 28 }, capsule: { hp: 8, r: 20 } };
  const FINALE = { ray: 'storm', ollie: 'storm', mom: 'storm', norah: 'rush', tyler: 'rush', layne: 'rush', dad: 'beam', violet: 'beam', jayson: 'beam' };
  const pickType = () => { const keys = Object.keys(ITEMS); let s = 0; for (const k of keys) s += ITEMS[k].w; let r = Math.random() * s; for (const k of keys) { r -= ITEMS[k].w; if (r <= 0) return k; } return 'bat'; };
  let uid = 0;
  const on = () => G.cfg && G.cfg.items !== false;

  // ------------------------------------------------------------ setup per match
  function reset() {
    G.items = []; G.orb = null; G.cine = null; G.notes = []; G.pulses = [];
    G.boxT = rand(300, 480); G.orbT = rand(1100, 1600);
    for (const f of G.fighters) {
      f.baseSc = f.sc; f.baseDef = f.def; f.item = null; f.final = false; f.starT = 0; f.giantT = 0; f.speedT = 0; f.malletT = 0; f.malletCD = new Map(); f.hold = {}; f.pcd = { attack: 0, special: 0, smash: 0 };
      Object.assign(f.moves, itemMoves());
    }
  }
  function itemMoves() {
    return {
      batSwing: M({ len: 30, pose: 'smashF', hb: [H(7, 11, 52, -50, 28, 11, 361, 32, 92)] }),
      batHomer: M({ len: 58, pose: 'smashF', wind: 'windF', charge: 10, smash: true, hb: [H(14, 17, 56, -48, 32, 22, 40, 48, 125, { homer: true })] }),
      batAir: M({ len: 34, air: true, ll: 8, pose: 'nair', hb: [H(5, 12, 42, -48, 32, 10, 361, 26, 95)] }),
      shoot: M({ len: 18, pose: 'jab', special: true, drift: 1, landLag: 2, onFrame: (f, t) => { if (t === 5) fire(f); } }),
      toss: M({ len: 20, pose: 'throw', special: true, drift: 1 }),
      finale: M({ len: 99999, pose: 'smashF', special: true, drift: 0 }),
      pulseA: M({ len: 34, pose: 'smashD', special: true, drift: 0.5, onFrame: (f, t) => { if (t === 7) pulse(f, 'attack'); } }),
      pulseB: M({ len: 36, pose: 'smashU', special: true, drift: 0.5, onFrame: (f, t) => { if (t === 7) pulse(f, 'special'); } }),
      pulseS: M({ len: 40, pose: 'smashF', special: true, drift: 0.5, onFrame: (f, t) => { if (t === 9) pulse(f, 'smash'); } }),
      finaleBeam: M({ len: 99999, pose: 'jab', special: true, drift: 0 }),
      finaleRush: M({ len: 99999, pose: 'dashkick', special: true, drift: 0 }),
      finaleStorm: M({ len: 99999, pose: 'nair', special: true, drift: 0 }),
    };
  }

  // ------------------------------------------------------------ spawning
  function spawnBox() {
    const st = G.stage, surf = Math.random() < 0.7 || !G.plats.length ? G.mainSurf : G.plats[Math.floor(Math.random() * G.plats.length)];
    const type = Math.random() < 0.6 ? 'crate' : 'capsule';
    const it = { id: ++uid, type, box: true, hp: BOXES[type].hp, r: BOXES[type].r, x: rand(surf.x1 + 50, surf.x2 - 50), y: surf.y - 900, vx: 0, vy: 0, t: 0, grounded: false, hits: new WeakMap() };
    G.items.push(it); note(it.x, surf.y - 120, type === 'crate' ? 'ITEM CRATE!' : 'CAPSULE!', '#ffe08a', 0);
  }
  function spawnItem(type, x, y, vx = 0, vy = 0) {
    const d = ITEMS[type]; const it = { id: ++uid, type, r: d.r, x, y, vx, vy, t: 0, grounded: false, ammo: d.ammo || 0, hits: new WeakMap() };
    G.items.push(it); return it;
  }
  function openBox(it, byF) {
    it.dead = true; snd('itemBreak', 0.85); G.shake = Math.max(G.shake, 4);
    fx.boom && fx.boom(it.x, it.y - it.r, 40);
    for (let i = 0; i < 18; i++) fx.spark(it.x, it.y - it.r, i % 2 ? '#ffe08a' : '#ffffff', 1, 9);
    const n = it.type === 'crate' && Math.random() < 0.45 ? 2 : 1;
    for (let k = 0; k < n; k++) spawnItem(pickType(), it.x + (k - (n - 1) / 2) * 30, it.y - 10, (k - (n - 1) / 2) * 3 + rand(-1, 1), -8);
  }

  // ------------------------------------------------------------ physics for loose items
  function fall(it) {
    const py = it.y; it.vy = Math.min(it.vy + 0.55, 16); it.x += it.vx; it.y += it.vy; const was = it.grounded; it.grounded = false;
    if (it.vy >= 0) for (const s of G.surfs) if (it.x >= s.x1 - 4 && it.x <= s.x2 + 4 && py <= s.y + 1 && it.y >= s.y) {
      it.y = s.y; it.ground = s;
      if (it.thrown) { landThrown(it); if (it.dead) return; }
      if (it.vy > 5) { it.vy = -it.vy * 0.3; if (!was) { fx.smoke && fx.smoke(it.x, it.y, 2); Sfx.land(0.4); } } else { it.vy = 0; it.grounded = true; }
      it.vx *= 0.7; break;
    }
    if (it.grounded && it.ground && it.ground.dx) it.x += it.ground.dx;
    const b = G.stage.blast; if (it.y > b.b || it.x < b.l || it.x > b.r) it.dead = true;
  }
  function landThrown(it) {
    if (it.type === 'balloon') return splash(it);
    if (it.type === 'capsule') return openBox(it);
    it.thrown = null;
  }
  function splash(it) {
    it.dead = true; snd('itemBreak', 0.6, 1.4); Sfx.boom && Sfx.boom();
    for (let i = 0; i < 26; i++) fx.spark(it.x, it.y - 14, i % 2 ? '#7fd0ff' : '#ffffff', 1, 10);
    fx.ring(it.x, it.y - 14, '#7fd0ff', 70);
    const owner = it.thrown;
    for (const v of G.fighters) if (v !== owner && canBeHit(v) && Math.hypot(v.x - it.x, v.y - v.h / 2 - (it.y - 14)) < 95) applyHit(owner || null, v, { d: 13, a: 55, b: 45, g: 95 }, sgn(v.x - it.x) || 1, { projectile: true, px: it.x, py: it.y - 14 });
  }

  // ------------------------------------------------------------ picking up, using, throwing
  const canUse = f => ['idle', 'walk', 'run', 'crouch', 'air', 'land'].includes(f.state);
  function nearItem(f) {
    let best = null, bd = 1e9;
    for (const it of G.items) {
      if (it.dead || it.held || it.thrown || it.type === 'crate') continue;
      const dx = Math.abs(it.x - f.x), dy = Math.abs(it.y - f.y);
      if (dx < f.w / 2 + it.r + 26 && dy < f.h * 0.9 && dx + dy < bd) { best = it; bd = dx + dy; }
    }
    return best;
  }
  function pickUp(f, it) {
    const d = ITEMS[it.type]; Sfx.grab(); snd('itemGet', 0.7);
    if (it.type === 'capsule') { it.held = f; f.item = it; return; }
    note(f.x, f.y - f.h - 30, d.name.toUpperCase() + '!', '#ffffff', 1);
    if (d.kind === 'weapon' || d.kind === 'throw') { it.held = f; f.item = it; return; }
    it.dead = true;
    if (it.type === 'cookie') { f.percent = Math.max(0, f.percent - 45); for (let i = 0; i < 14; i++) fx.spark(f.x, f.y - f.h / 2, '#ff7ab0', 1, 7); }
    else if (it.type === 'candy') { f.starT = d.dur; f.invinc = Math.max(f.invinc, d.dur); }
    else if (it.type === 'juice') { f.giantT = d.dur; grow(f, 1.45); }
    else if (it.type === 'sneakers') { f.speedT = d.dur; const b = f.baseDef; f.def = Object.assign({}, b, { walk: b.walk * 1.4, run: b.run * 1.5, air: b.air * 1.35, jump: b.jump * 1.1, djump: b.djump * 1.1 }); }
    else if (it.type === 'mallet') { const m = spawnItem('mallet', f.x, f.y); m.held = f; f.item = m; f.malletT = d.dur; }
    fx.ring(f.x, f.y - f.h / 2, '#ffffff', 60);
  }
  function grow(f, k) { f.sc = f.baseSc * k; f.w = 34 * f.sc; f.h = 80 * f.sc; }
  function release(f, dirX, dirY) {
    const it = f.item; if (!it) return; f.item = null; it.held = null;
    if (it.type === 'mallet') { it.dead = true; return; }
    it.x = f.x + f.facing * 26 * f.sc; it.y = f.y - f.h * 0.55; it.grounded = false;
    if (dirY > 0.5) { it.vx = 0; it.vy = 2; return; } // drop
    it.thrown = f; it.vx = dirY < -0.5 ? 0 : (dirX || f.facing) * 17; it.vy = dirY < -0.5 ? -17 : -4;
    Sfx.throw_ && Sfx.throw_();
  }
  function fire(f) {
    const it = f.item; if (!it) return;
    if (it.ammo <= 0) { Sfx.blip && Sfx.blip(); return; }
    it.ammo--; Sfx.zap(); spawnProj(f, { type: 'bolt', x: 46, y: -62, vx: 16, r: 9, d: 3.2, a: 361, b: 12, g: 18, life: 60 });
    fx.spark(f.x + f.facing * 50 * f.sc, f.y - 62 * f.sc, '#ffb347', 3, 4);
  }
  // runs before the normal ground/air actions; returns true when it handled the input
  function itemActions(f) {
    const i = f.inp;
    if (G.cine) return G.cine.f === f;
    if (f.final && consume(f, 'special')) { startFinale(f); return true; }
    if (f.malletT > 0) { for (const b of ['attack', 'smash', 'special', 'grab', 'shield']) f.buf[b] = 0; f.inp.shield = false; return false; }
    const it = f.item;
    if (it) {
      if (consume(f, 'grab')) { startMove(f, 'toss'); release(f, Math.abs(i.x) > 0.3 ? sgn(i.x) : f.facing, i.y); return true; }
      const smash = consume(f, 'smash'), atk = smash || consume(f, 'attack');
      if (atk) {
        if (Math.abs(i.x) > 0.3) f.facing = sgn(i.x);
        if (it.type === 'bat') startMove(f, !f.grounded ? 'batAir' : (smash || i.flick) ? 'batHomer' : 'batSwing');
        else if (it.type === 'blaster') { if (it.ammo > 0) startMove(f, 'shoot'); else { startMove(f, 'toss'); release(f, f.facing, 0); } }
        else { startMove(f, 'toss'); release(f, f.facing, i.y < -0.5 ? -1 : 0); }
        return true;
      }
      return false;
    }
    if (f.buf.attack > 0 || f.buf.grab > 0) { const n = nearItem(f); if (n) { f.buf.attack = 0; f.buf.grab = 0; pickUp(f, n); return true; } }
    return false;
  }
  const baseGA = groundActions, baseAA = airActions;
  groundActions = function (f) { return itemActions(f) || baseGA(f); };
  airActions = function (f) { return itemActions(f) || baseAA(f); };

  // ------------------------------------------------------------ hits: giants hit harder and are heavier; the bat's home run
  const baseHit = applyHit;
  applyHit = function (att, vic, hb, dir, o) {
    let h = hb;
    if ((att && att.giantT > 0) || vic.giantT > 0) { h = Object.assign({}, hb); if (att && att.giantT > 0) h.d *= 1.35; if (vic.giantT > 0) { h.b *= 0.7; h.g *= 0.75; } }
    const r = baseHit(att, vic, h, dir, o || {});
    if (hb.homer && vic.state === 'hitstun') { snd('homeRun', 0.9); G.flash = Math.max(G.flash, 8); note(vic.x, vic.y - vic.h - 30, 'HOME RUN!', '#ffd23f', 1); }
    return r;
  };
  const baseKo = ko;
  ko = function (f) {
    if (f.item) { f.item.dead = true; f.item = null; }
    if (f.final) f.final = false;
    if (f.giantT > 0) { f.giantT = 0; grow(f, 1); }
    f.starT = 0; f.malletT = 0; if (f.speedT > 0) { f.speedT = 0; f.def = f.baseDef; }
    return baseKo(f);
  };

  // ------------------------------------------------------------ the Super Orb: it floats around; break it to earn a Family Finale
  function spawnOrb() {
    const m = G.stage.main, side = Math.random() < 0.5 ? -1 : 1;
    G.orb = { x: side * ((m.x2 - m.x1) / 2 + 400), y: m.y - 420, vx: 0, vy: 0, hp: 30, t: 0, tx: 0, ty: m.y - 300, life: 1500, hits: new WeakMap(), flash: 0 };
    G.banner = { text: 'A SUPER ORB APPEARS!', t: 100, color: '#fff3a8' }; snd('itemGet', 0.9, 0.7);
  }
  function orbStep() {
    const o = G.orb, m = G.stage.main; o.t++;
    if (o.flash > 0) o.flash--;
    if (o.t >= o.life) { o.vy -= 0.4; o.y += o.vy; if (o.y < G.stage.blast.t - 300) { G.orb = null; G.orbT = rand(1500, 2400); } return; }
    if (o.t % 100 === 1) { o.tx = rand(m.x1 + 80, m.x2 - 80); o.ty = m.y - rand(140, 520); }
    o.vx = (o.vx + (o.tx - o.x) * 0.0016) * 0.965; o.vy = (o.vy + (o.ty - o.y) * 0.0016) * 0.965;
    const sp = Math.hypot(o.vx, o.vy), mx = 4.2; if (sp > mx) { o.vx *= mx / sp; o.vy *= mx / sp; }
    o.x += o.vx + Math.sin(o.t * 0.05) * 0.8; o.y += o.vy + Math.cos(o.t * 0.07) * 0.6;
    if (G.frame % 3 === 0) fx.spark(o.x + rand(-20, 20), o.y + rand(-20, 20), `hsl(${(o.t * 4) % 360},100%,70%)`, 1, 2);
  }
  function hitOrb(f, d, dir) {
    const o = G.orb; o.hp -= d; o.flash = 8; o.vx += dir * (3 + d * 0.3); o.vy -= 1.5;
    Sfx.hit(d); fx.spark(o.x, o.y, '#ffffff', 8, 7);
    if (o.hp <= 0) {
      G.orb = null; f.final = true; G.orbT = rand(1800, 2700);
      snd('orbBreak', 1); G.flash = 12; G.shake = Math.max(G.shake, 10);
      for (let k = 0; k < 3; k++) fx.ring(o.x, o.y, ['#ff6ad5', '#ffd23f', '#6cf0ff'][k], 60 + k * 40);
      for (let i = 0; i < 40; i++) fx.spark(o.x, o.y, `hsl(${i * 9},100%,65%)`, 1, 14);
      G.banner = { text: f.def.name.toUpperCase() + ' HAS THE SUPER ORB!', t: 120, color: f.def.accent };
      if (!f.cpu) note(f.x, f.y - f.h - 40, 'PRESS SPECIAL!', '#ffffff', 1, 150);
    }
  }

  // ------------------------------------------------------------ hit detection for boxes, items and the orb
  function hitTargets() {
    const targets = G.items.filter(it => !it.dead && !it.held && it.box);
    for (const a of G.fighters) {
      const hbs = activeHBs(a); if (!hbs.length) continue;
      for (const h of hbs) {
        if (h.grab) continue; const w = hbWorld(a, h);
        for (const it of targets) {
          if (it.dead || it.hits.get(a) === a.hitIds) continue;
          if (Math.hypot(w.x - it.x, w.y - (it.y - it.r)) > w.r + it.r) continue;
          it.hits.set(a, a.hitIds); it.hp -= h.d; it.vx += a.facing * 2; Sfx.hit(h.d * 0.6); fx.spark(it.x, it.y - it.r, '#ffe08a', 4, 5);
          if (it.hp <= 0) openBox(it, a);
        }
        const o = G.orb;
        if (o && o.t < o.life && o.hits.get(a) !== a.hitIds && Math.hypot(w.x - o.x, w.y - o.y) < w.r + 30) { o.hits.set(a, a.hitIds); hitOrb(a, h.d, a.facing); }
      }
    }
    for (const p of G.projs) {
      if (p.dead) continue;
      for (const it of targets) if (!it.dead && Math.hypot(p.x - it.x, p.y - (it.y - it.r)) < p.r + it.r) { it.hp -= p.d; if (p.type !== 'blast') p.dead = true; if (it.hp <= 0) openBox(it, p.owner); }
      const o = G.orb; if (o && !p.dead && p.owner && Math.hypot(p.x - o.x, p.y - o.y) < p.r + 30) { if (p.type !== 'blast') p.dead = true; hitOrb(p.owner, p.d, sgn(p.vx) || 1); }
    }
    // thrown items hit fighters
    for (const it of G.items) {
      if (it.dead || !it.thrown || Math.hypot(it.vx, it.vy) < 4) continue;
      for (const v of G.fighters) {
        if (v === it.thrown || !canBeHit(v)) continue;
        if (!circRect(it.x, it.y - it.r, it.r + 6, hurtRect(v))) continue;
        if (it.type === 'balloon') { splash(it); break; }
        applyHit(it.thrown, v, { d: it.type === 'bat' ? 12 : it.box ? 14 : 8, a: 45, b: 35, g: 80 }, sgn(it.vx) || 1, { projectile: true, px: it.x, py: it.y - it.r });
        if (it.type === 'capsule') { openBox(it); break; }
        it.vx *= -0.3; it.vy = -5; it.thrown = null; break;
      }
    }
  }

  // ------------------------------------------------------------ the Family Finale (cinematic super attack)
  function startFinale(f) {
    f.final = false; if (f.item) release(f, f.facing, 1);
    const type = FINALE[f.def.id] || 'beam';
    G.cine = { f, t: 0, type, phase: 'intro', caught: [], hit: new Set(), px: f.x, py: f.y, bt: 0 };
    startMove(f, 'finale'); f.vx = f.vy = 0;
    snd('finaleCharge', 1); Sfx.od && Sfx.od(); G.flash = 10; G.banner = null;
    if (Sfx.voice) Sfx.voice(f, 'atk2', true);
  }
  function endFinale() {
    const c = G.cine, f = c.f; G.cine = null;
    for (const v of c.caught) if (v.hitstop > 60) v.hitstop = 0;
    f.intang = 40; if (f.state === 'attack') endMove(f);
  }
  function beamRect(f) { const y = f.y - f.h * 0.55, x0 = f.x + f.facing * 30 * f.sc; return { x1: Math.min(x0, x0 + f.facing * 2600), x2: Math.max(x0, x0 + f.facing * 2600), y1: y - 70 * f.sc, y2: y + 70 * f.sc, y }; }
  function inRect(v, r) { return v.x + v.w / 2 > r.x1 && v.x - v.w / 2 < r.x2 && v.y > r.y1 && v.y - v.h < r.y2; }
  const foes = f => G.fighters.filter(v => v !== f && alive(v) && v.state !== 'dead' && v.state !== 'out' && v.state !== 'respawn' && !(f.team && f.team === v.team));
  function small(f, v, dir) { const ii = v.intang, iv = v.invinc; v.intang = 0; v.invinc = 0; applyHit(f, v, { d: 2.2, a: 0, b: 0, g: 0 }, dir, { px: v.x, py: v.y - v.h / 2 }); v.intang = ii; v.invinc = iv; }
  function launch(f, v, dir, ang = 40) { v.hitstop = 0; v.intang = 0; v.invinc = 0; applyHit(f, v, { d: 18, a: ang, b: 95, g: 125 }, dir, { px: v.x, py: v.y - v.h / 2 }); }
  function cinePre() {
    const c = G.cine; if (!c) return; const f = c.f;
    if (c.phase === 'intro' || c.phase === 'barrage') for (const v of G.fighters) if (v !== f && !c.caught.includes(v)) v.hitstop = Math.max(v.hitstop, 2);
  }
  function cinePost() {
    const c = G.cine; if (!c) return; const f = c.f; c.t++;
    if (f.state === 'dead' || f.state === 'out') return endFinale();
    if (c.phase === 'intro') {
      f.x = c.px; f.y = c.py; f.vx = f.vy = 0;
      if (c.t >= 78) {
        c.phase = 'act'; c.t = 0;
        startMove(f, c.type === 'beam' ? 'finaleBeam' : c.type === 'rush' ? 'finaleRush' : 'finaleStorm');
        snd(c.type === 'beam' ? 'finaleBeam' : 'finaleCharge', 1, c.type === 'beam' ? 1 : 1.3); G.shake = 14; G.flash = 8;
      }
      return;
    }
    if (c.phase === 'act' && c.type === 'beam') {
      f.x = c.px; f.y = c.py; f.vx = f.vy = 0; const r = beamRect(f);
      if (c.t % 6 === 0) for (const v of foes(f)) if (inRect(v, r)) { c.hit.add(v); small(f, v, f.facing); v.lvx = f.facing * 1.2; v.lvy = -0.2; }
      G.shake = Math.max(G.shake, 6);
      if (c.t >= 80) { for (const v of c.hit) launch(f, v, f.facing, 30); return endFinale(); }
      return;
    }
    if (c.phase === 'act' && c.type === 'rush') {
      const m = G.stage.main, nx = f.x + f.facing * 26;
      f.vy = 0; f.y = c.py;
      if (c.t < 44 && nx > m.x1 - 200 && nx < m.x2 + 200) f.x = nx;
      if (G.frame % 2 === 0) fx.spark(f.x - f.facing * 30, f.y - f.h / 2, f.def.accent, 2, 5);
      for (const v of foes(f)) if (!c.caught.includes(v) && Math.abs(v.x - f.x) < 80 * f.sc && Math.abs((v.y - v.h / 2) - (f.y - f.h / 2)) < 100 * f.sc) { c.caught.push(v); v.hitstop = 9999; Sfx.hit(12); G.flash = 6; }
      c.caught.forEach((v, k) => { v.x = f.x + f.facing * (55 + k * 30) * f.sc; v.y = f.y; });
      if (c.t >= 46) { if (c.caught.length) { c.phase = 'barrage'; c.t = 0; snd('finaleCharge', 0.9, 1.5); } else return endFinale(); }
      return;
    }
    if (c.phase === 'barrage') {
      f.y = c.py;
      if (c.t % 5 === 0) for (const v of c.caught) { small(f, v, f.facing); v.hitstop = 9999; G.shake = 8; }
      if (c.t >= 72) { for (const v of c.caught) launch(f, v, f.facing, 45); G.flash = 12; snd('homeRun', 1, 0.8); return endFinale(); }
      return;
    }
    if (c.phase === 'act' && c.type === 'storm') {
      f.x = c.px; f.vx = f.vy = 0; f.y = c.py - Math.min(60, c.t * 3);
      const R = 360 * Math.max(1, f.sc / 1.4);
      for (const v of foes(f)) {
        const dx = f.x - v.x, dy = (f.y - f.h / 2) - (v.y - v.h / 2), d = Math.hypot(dx, dy);
        if (d < R) { v.x += dx * 0.05; v.y += dy * 0.05; v.lvx = v.lvy = 0; if (c.t % 6 === 0) { c.hit.add(v); small(f, v, sgn(dx) || 1); } }
      }
      G.shake = Math.max(G.shake, 5);
      if (c.t >= 84) { for (const v of c.hit) launch(f, v, sgn(v.x - f.x) || 1, 50); G.flash = 12; Sfx.boom && Sfx.boom(); return endFinale(); }
    }
  }

  // ------------------------------------------------------------ the frame step
  function itemsStep() {
    if (!G.items) reset();
    if (G.countdown > 0 || G.ending) { cinePost(); return; }
    cinePost();
    if (on() && !G.cine) {
      const loose = G.items.filter(i => !i.dead && !i.held).length;
      if (--G.boxT <= 0) { if (loose < 4) spawnBox(); G.boxT = rand(480, 780); }
      if (!G.orb && !G.fighters.some(f => f.final) && --G.orbT <= 0) spawnOrb();
    }
    if (G.orb && !G.cine) orbStep();
    for (const it of G.items) {
      if (it.dead) continue; it.t++;
      if (it.held) { it.x = it.held.x; it.y = it.held.y; continue; }
      fall(it);
      if (!it.box && it.grounded && !it.thrown && it.t > 1100) it.dead = true; // unclaimed items vanish after a while
    }
    hitTargets();
    pulseInput(); pulseStep();
    for (const f of G.fighters) {
      if (f.starT > 0) { f.starT--; if (G.frame % 2 === 0) fx.spark(f.x + rand(-24, 24) * f.sc, f.y - rand(0, f.h), `hsl(${(G.frame * 12) % 360},100%,65%)`, 1, 3); }
      if (f.giantT > 0 && --f.giantT === 0) { grow(f, 1); fx.ring(f.x, f.y - f.h / 2, '#ffffff', 50); }
      if (f.speedT > 0) { if (--f.speedT === 0) f.def = f.baseDef; else if (G.frame % 4 === 0 && Math.abs(f.vx) > 3) fx.spark(f.x - sgn(f.vx) * 20, f.y - 10, '#ffffff', 1, 2); }
      if (f.malletT > 0) {
        if (--f.malletT === 0) { if (f.item && f.item.type === 'mallet') { f.item.dead = true; f.item = null; } }
        else malletHits(f);
      }
      if (f.final && G.frame % 2 === 0) fx.spark(f.x + rand(-30, 30) * f.sc, f.y - rand(0, f.h), `hsl(${(G.frame * 9) % 360},100%,70%)`, 1, 3);
    }
    G.items = G.items.filter(i => !i.dead);
    for (const n of G.notes) n.t++; G.notes = G.notes.filter(n => n.t < n.life);
  }
  function malletHits(f) {
    const ph = Math.sin(G.frame * 0.35), cx = f.x + f.facing * (40 + ph * 14) * f.sc, cy = f.y - (60 + ph * 20) * f.sc, r = 44 * f.sc;
    if (G.frame % 12 === 0) Sfx.swing && Sfx.swing();
    for (const v of G.fighters) {
      if (v === f || !canBeHit(v) || (f.team && f.team === v.team)) continue;
      const last = f.malletCD.get(v) || -99; if (G.frame - last < 24) continue;
      if (!circRect(cx, cy, r, hurtRect(v))) continue;
      f.malletCD.set(v, G.frame); applyHit(f, v, { d: 15, a: 50, b: 60, g: 110 }, f.facing, { px: cx, py: cy });
    }
    for (const it of G.items) if (it.box && !it.dead && !it.held && Math.hypot(cx - it.x, cy - it.y) < r + it.r) openBox(it, f);
    const o = G.orb; if (o && G.frame % 20 === 0 && Math.hypot(cx - o.x, cy - o.y) < r + 30) hitOrb(f, 10, f.facing);
  }
  // ------------------------------------------------------------ pulse attacks: hold Attack, Special or Smash to release a shockwave (10 s recharge each)
  const PULSE = {
    attack: { move: 'pulseA', R: 200, d: 9, a: 45, b: 55, g: 72, col: '#ffffff', name: 'POWER PULSE' },
    special: { move: 'pulseB', R: 290, d: 7, a: 80, b: 62, g: 62, col: null, name: 'ENERGY PULSE' },
    smash: { move: 'pulseS', R: 230, d: 15, a: 38, b: 66, g: 105, col: '#ffd23f', name: 'SMASH PULSE' },
  };
  const HOLD = 28, pulseCD = () => Math.round(600 * (G.speed || 0.8));
  const PULSE_MOVES = new Set(['pulseA', 'pulseB', 'pulseS', 'finale', 'finaleBeam', 'finaleRush', 'finaleStorm']);
  function canPulse(f) {
    if (G.cine || f.malletT > 0) return false;
    if (['idle', 'walk', 'run', 'crouch', 'air', 'land', 'jumpsquat', 'shield'].includes(f.state)) return true;
    return f.state === 'attack' && f.move && !PULSE_MOVES.has(Object.keys(f.moves).find(k => f.moves[k] === f.move));
  }
  function pulseInput() {
    for (const f of G.fighters) {
      if (!f.pcd) continue; const raw = f.prevRaw || {};
      for (const b in PULSE) {
        if (f.pcd[b] > 0) f.pcd[b]--;
        f.hold[b] = raw[b] ? (f.hold[b] || 0) + 1 : 0;
        if (f.hold[b] === HOLD && f.pcd[b] <= 0 && canPulse(f)) {
          f.pcd[b] = pulseCD(); f.hold[b] = HOLD + 1; if (f.state === 'attack') endMove(f);
          startMove(f, PULSE[b].move); f.buf[b] = 0; f.charge = 0;
          snd('finaleCharge', 0.5, 1.8); Sfx.voice && Sfx.voice(f, 'atk2');
        }
      }
    }
  }
  function pulse(f, b) {
    const P = PULSE[b], k = f.sc / 1.9;
    G.pulses.push({ f, b, x: f.x, y: f.y - f.h * 0.5, r: 0, R: P.R * Math.max(0.8, k), t: 0, hit: new Set(), col: P.col || f.def.accent });
    G.shake = Math.max(G.shake, b === 'smash' ? 10 : 6); G.flash = Math.max(G.flash, 4);
    snd(b === 'smash' ? 'orbBreak' : 'itemBreak', b === 'smash' ? 0.55 : 0.7, b === 'special' ? 0.8 : 1.15);
    for (let i = 0; i < 24; i++) fx.spark(f.x, f.y - f.h * 0.5, i % 2 ? '#ffffff' : (P.col || f.def.accent), 1, 14);
    note(f.x, f.y - f.h - 30, P.name + '!', P.col || f.def.accent, 1, 60);
  }
  function pulseStep() {
    for (const p of G.pulses) {
      p.t++; p.r = p.R * (1 - Math.pow(1 - Math.min(1, p.t / 16), 3)); const f = p.f, P = PULSE[p.b];
      for (const v of G.fighters) {
        if (v === f || p.hit.has(v) || !canBeHit(v) || (f.team && f.team === v.team)) continue;
        const d = Math.hypot(v.x - p.x, (v.y - v.h / 2) - p.y);
        if (d > p.r + v.w / 2) continue;
        p.hit.add(v); applyHit(f, v, { d: P.d * f.def.power, a: P.a, b: P.b, g: P.g }, sgn(v.x - p.x) || f.facing, { projectile: true, px: v.x, py: v.y - v.h / 2 });
      }
      for (const it of G.items) if (it.box && !it.dead && !it.held && !p.hit.has(it) && Math.hypot(it.x - p.x, it.y - it.r - p.y) < p.r + it.r) { p.hit.add(it); openBox(it, f); }
      const o = G.orb; if (o && !p.hit.has(o) && Math.hypot(o.x - p.x, o.y - p.y) < p.r + 30) { p.hit.add(o); hitOrb(f, P.d, sgn(o.x - p.x) || 1); }
    }
    G.pulses = G.pulses.filter(p => p.t < 24);
  }
  function note(x, y, text, color, follow, life = 90) { if (G.notes) G.notes.push({ x, y, text, color, t: 0, life }); }

  // the fighter doing a finale can't be hit (without the invincibility blink)
  const baseCanBeHit = canBeHit;
  canBeHit = function (v) { return !(G.cine && G.cine.f === v) && baseCanBeHit(v); };
  const baseStep = step;
  step = function () { cinePre(); baseStep(); if (G.mode === 'fight') itemsStep(); };
  if (window.__clash) window.__clash.step = step;
  const baseStart = startMatch;
  startMatch = function (cfg) { baseStart(cfg); reset(); };
  if (window.__clash) window.__clash.startMatch = startMatch;

  // ------------------------------------------------------------ CPUs: chase the orb, grab items, use the finale
  const baseAi = aiPoll;
  aiPoll = function (f) {
    const r = baseAi(f);
    try {
      const press = G.frame % 2 === 0, lvl = f.lvl || 5, foesL = foes(f);
      if (G.cine) return r;
      if (f.final && foesL.length) {
        const t = foesL.reduce((a, b) => Math.abs(b.x - f.x) < Math.abs(a.x - f.x) ? b : a);
        const reach = (FINALE[f.def.id] || 'beam') === 'beam' ? 1400 : 320;
        r.x = sgn(t.x - f.x) * (Math.abs(t.x - f.x) > reach * 0.6 ? 1 : 0.35);
        if (Math.abs(t.x - f.x) < reach && Math.abs(t.y - f.y) < 160 && press) { r.special = true; r.x = sgn(t.x - f.x) * 0.2; }
        return r;
      }
      const o = G.orb;
      if (o && o.t < o.life && Math.random() < 0.4 + lvl * 0.06) {
        const dx = o.x - f.x, dy = o.y - (f.y - f.h * 0.6);
        if (Math.abs(dx) > 40) r.x = sgn(dx);
        r.jump = dy < -60 && Math.abs(dx) < 260 && press;
        r.attack = Math.hypot(dx, dy) < 110 && press; r.smash = false; r.special = false; r.shield = false;
        if (r.attack) r.y = dy < -50 ? -1 : 0;
        return r;
      }
      const near = foesL.some(v => Math.abs(v.x - f.x) < 160 && Math.abs(v.y - f.y) < 120);
      if (!f.item && !near && f.malletT <= 0) {
        let best = null, bd = 420;
        for (const it of G.items) { if (it.dead || it.held || it.thrown) continue; const d = Math.abs(it.x - f.x) + Math.abs(it.y - f.y) * 0.5; if (d < bd) { bd = d; best = it; } }
        if (best) {
          r.x = Math.abs(best.x - f.x) > 20 ? sgn(best.x - f.x) : 0; r.smash = false; r.special = false;
          r.attack = (best.box || Math.abs(best.x - f.x) < 40) && Math.abs(best.y - f.y) < 80 && Math.abs(best.x - f.x) < 70 && press;
        }
      }
      if (f.item && f.item.type === 'balloon' && near && press) r.attack = true;
      const a = f.ai || (f.ai = {});
      if (a.pulseB) { r[a.pulseB] = true; r.x = 0; if (--a.pulseN <= 0) a.pulseB = null; return r; }
      if (near && f.pcd && Math.random() < 0.004 * lvl) { const ready = Object.keys(PULSE).filter(b => f.pcd[b] <= 0); if (ready.length) { a.pulseB = ready[Math.floor(Math.random() * ready.length)]; a.pulseN = HOLD + 4; } }
    } catch (e) { /* the base AI keeps working */ }
    return r;
  };

  // ------------------------------------------------------------ camera close-ups during the finale
  const baseCam = updateCam;
  updateCam = function () {
    const prev = { x: G.cam.x, y: G.cam.y, z: G.cam.z };
    baseCam(); const c = G.cine; if (!c || G.camLock) return;
    let tx, ty, tz, k = 0.22;
    if (c.phase === 'intro') { const f = c.f; tx = f.x + f.facing * 8; ty = f.y - f.h * 0.62; tz = 3.3 / Math.max(1, f.sc / 1.6); }
    else if (c.phase === 'barrage' && c.caught[0]) { const v = c.caught[0]; tx = (v.x + c.f.x) / 2; ty = v.y - v.h * 0.6; tz = 2.6; }
    else if (c.phase === 'act' && c.type === 'beam') { const f = c.f; tx = f.x + f.facing * 420; ty = f.y - f.h * 0.5; tz = Math.min(G.cam.z, 1.0); k = 0.08; }
    else return;
    G.cam.x = lerp(prev.x, tx, k); G.cam.y = lerp(prev.y, ty, k); G.cam.z = lerp(prev.z, tz, k);
  };

  // ------------------------------------------------------------ 2D overlay: finale effects, notes, orb holder halo
  const baseRender = render;
  render = function () {
    baseRender();
    if (G.mode !== 'fight' || !G.stage || !G.items) return;
    const g = mainCtx, s = camScale(), c = G.cine;
    g.save(); g.setTransform(DPR, 0, 0, DPR, 0, 0);
    // world-space effects
    g.save(); g.translate(VW / 2, VH / 2); g.scale(s, s); g.translate(-G.cam.x, -G.cam.y);
    for (const f of G.fighters) if (f.final && f.state !== 'dead' && f.state !== 'out') {
      const p = 0.5 + 0.5 * Math.sin(G.frame * 0.2); g.globalCompositeOperation = 'lighter';
      const gr = g.createRadialGradient(f.x, f.y - f.h / 2, 10, f.x, f.y - f.h / 2, f.h * (0.8 + p * 0.15));
      gr.addColorStop(0, 'rgba(255,255,255,0)'); gr.addColorStop(0.7, `hsla(${(G.frame * 6) % 360},100%,60%,${0.25 + p * 0.15})`); gr.addColorStop(1, 'rgba(0,0,0,0)');
      g.fillStyle = gr; g.beginPath(); g.arc(f.x, f.y - f.h / 2, f.h, 0, TAU); g.fill(); g.globalCompositeOperation = 'source-over';
    }
    if (c && c.phase === 'act' && c.type === 'beam') {
      const f = c.f, r = beamRect(f), grow = Math.min(1, c.t / 8) * (c.t > 70 ? Math.max(0, 1 - (c.t - 70) / 10) : 1), hh = 34 * f.sc * grow * (0.9 + Math.random() * 0.15);
      g.globalCompositeOperation = 'lighter';
      const gr = g.createLinearGradient(0, r.y - hh * 1.5, 0, r.y + hh * 1.5); gr.addColorStop(0, 'rgba(0,0,0,0)'); gr.addColorStop(0.25, f.def.accent); gr.addColorStop(0.5, '#ffffff'); gr.addColorStop(0.75, f.def.accent); gr.addColorStop(1, 'rgba(0,0,0,0)');
      g.globalAlpha = 0.85; g.fillStyle = gr; g.fillRect(r.x1, r.y - hh * 1.5, r.x2 - r.x1, hh * 3); g.globalAlpha = 1;
      for (let k = 0; k < 6; k++) { const yy = r.y + rand(-hh, hh); g.strokeStyle = 'rgba(255,255,255,.7)'; g.lineWidth = rand(1, 3); g.beginPath(); g.moveTo(r.x1, yy); g.lineTo(r.x2, yy + rand(-6, 6)); g.stroke(); }
      g.fillStyle = 'rgba(255,255,255,.95)'; g.fillRect(r.x1, r.y - hh * 0.18, r.x2 - r.x1, hh * 0.36);
      const ox = f.x + f.facing * 30 * f.sc; const og = g.createRadialGradient(ox, r.y, 0, ox, r.y, hh * 1.8); og.addColorStop(0, '#ffffff'); og.addColorStop(0.4, f.def.accent); og.addColorStop(1, 'rgba(0,0,0,0)'); g.fillStyle = og; g.beginPath(); g.arc(ox, r.y, hh * 1.8, 0, TAU); g.fill();
      g.globalCompositeOperation = 'source-over';
    }
    if (c && c.phase === 'act' && c.type === 'storm') {
      const f = c.f, cx = f.x, cy = f.y - f.h / 2, R = 360 * Math.max(1, f.sc / 1.4); g.globalCompositeOperation = 'lighter';
      for (let k = 0; k < 7; k++) { g.strokeStyle = k % 2 ? f.def.accent : '#ffffff'; g.globalAlpha = 0.5; g.lineWidth = 10 - k; const a0 = c.t * (0.18 + k * 0.03) + k; g.beginPath(); g.arc(cx, cy, R * (0.3 + k * 0.11), a0, a0 + 2.2); g.stroke(); }
      g.globalAlpha = 1; g.globalCompositeOperation = 'source-over';
    }
    if (c && c.phase === 'act' && c.type === 'rush') {
      const f = c.f; g.globalCompositeOperation = 'lighter'; g.strokeStyle = f.def.accent; g.lineWidth = 6;
      for (let k = 0; k < 8; k++) { const y = f.y - rand(10, f.h); g.globalAlpha = 0.6; g.beginPath(); g.moveTo(f.x - f.facing * rand(40, 80), y); g.lineTo(f.x - f.facing * rand(200, 420), y); g.stroke(); }
      g.globalAlpha = 1; g.globalCompositeOperation = 'source-over';
    }
    for (const p of G.pulses || []) {
      const a = 1 - p.t / 24; g.globalCompositeOperation = 'lighter';
      for (let k = 0; k < 3; k++) { g.strokeStyle = k === 1 ? '#ffffff' : p.col; g.globalAlpha = a * (k === 1 ? 0.9 : 0.55); g.lineWidth = (k === 1 ? 6 : 16 - k * 3) / Math.max(0.5, s) * 1.5; g.beginPath(); g.arc(p.x, p.y, Math.max(1, p.r - k * 10), 0, TAU); g.stroke(); }
      const rg = g.createRadialGradient(p.x, p.y, 0, p.x, p.y, Math.max(1, p.r)); rg.addColorStop(0, 'rgba(255,255,255,0)'); rg.addColorStop(0.85, 'rgba(255,255,255,0)'); rg.addColorStop(1, p.col);
      g.globalAlpha = a * 0.35; g.fillStyle = rg; g.beginPath(); g.arc(p.x, p.y, Math.max(1, p.r), 0, TAU); g.fill();
      g.globalAlpha = 1; g.globalCompositeOperation = 'source-over';
    }
    for (const f of G.fighters) {
      if (!f.hold || f.state === 'dead' || f.state === 'out') continue;
      for (const b in PULSE) {
        const h = f.hold[b] || 0; if (h < 8 || h > HOLD || f.pcd[b] > 0) continue;
        const u = (h - 8) / (HOLD - 8), cy = f.y - f.h / 2, col = PULSE[b].col || f.def.accent;
        g.globalCompositeOperation = 'lighter'; g.strokeStyle = col; g.globalAlpha = 0.4 + u * 0.5; g.lineWidth = 4 / Math.max(0.5, s) * 1.5;
        g.beginPath(); g.arc(f.x, cy, f.h * (0.9 - u * 0.35), -Math.PI / 2, -Math.PI / 2 + TAU * u); g.stroke();
        g.globalAlpha = 1; g.globalCompositeOperation = 'source-over';
      }
    }
    for (const n of G.notes) {
      const a = Math.min(1, (n.life - n.t) / 20), y = n.y - n.t * 0.6;
      g.globalAlpha = a; g.font = `italic 900 ${22 / Math.max(0.6, s) * 1.2}px Trebuchet MS, sans-serif`; g.textAlign = 'center';
      g.lineWidth = 5 / s; g.strokeStyle = '#000'; g.strokeText(n.text, n.x, y); g.fillStyle = n.color; g.fillText(n.text, n.x, y);
    }
    g.globalAlpha = 1; g.restore();
    // pulse recharge dials on each HUD card (same layout as drawHUD)
    {
      const hs = clamp(VH / 600, 0.62, 1), fs = G.fighters, lay = hudLayout(fs.length);
      g.save(); g.translate(VW / 2, VH); g.scale(hs, hs); g.translate(-VW / 2, -VH);
      for (let fi = 0; fi < fs.length; fi++) {
        const f = fs[fi], x = lay[fi].x, cw = lay[fi].cw, y0 = lay[fi].y; if (!f.pcd) continue;
        ['attack', 'special', 'smash'].forEach((b, k) => {
          const cx = x + cw - 14 - (2 - k) * 17, cy = y0 - 9, cd = f.pcd[b], full = pulseCD(), u = cd > 0 ? 1 - cd / full : 1;
          g.fillStyle = 'rgba(10,12,24,.8)'; g.beginPath(); g.arc(cx, cy, 7.5, 0, TAU); g.fill();
          g.fillStyle = cd > 0 ? 'rgba(255,255,255,.35)' : (PULSE[b].col || f.def.accent); g.beginPath(); g.moveTo(cx, cy); g.arc(cx, cy, 6, -Math.PI / 2, -Math.PI / 2 + TAU * u); g.closePath(); g.fill();
          g.fillStyle = cd > 0 ? '#ffffff' : '#000'; g.font = 'bold 8px Trebuchet MS'; g.textAlign = 'center'; g.fillText('ABS'[k], cx, cy + 3);
        });
      }
      g.restore();
    }
    // cinematic framing
    if (c && (c.phase === 'intro' || c.phase === 'barrage')) {
      const k = Math.min(1, c.t / 10), bar = VH * 0.12 * k, f = c.f;
      g.fillStyle = 'rgba(0,0,0,.35)'; g.fillRect(0, 0, VW, VH);
      g.globalCompositeOperation = 'lighter'; g.strokeStyle = f.def.accent; g.globalAlpha = 0.35;
      for (let i = 0; i < 40; i++) { const a = (i / 40) * TAU + G.frame * 0.02, r1 = Math.max(VW, VH) * rand(0.25, 0.4); g.lineWidth = rand(2, 7); g.beginPath(); g.moveTo(VW / 2 + Math.cos(a) * r1, VH / 2 + Math.sin(a) * r1); g.lineTo(VW / 2 + Math.cos(a) * VW, VH / 2 + Math.sin(a) * VW); g.stroke(); }
      g.globalAlpha = 1; g.globalCompositeOperation = 'source-over';
      g.fillStyle = '#000'; g.fillRect(0, 0, VW, bar); g.fillRect(0, VH - bar, VW, bar);
      if (c.phase === 'intro') {
        const slide = Math.min(1, c.t / 14), fs = Math.min(VW * 0.09, 64);
        g.textAlign = 'left'; g.font = `italic 900 ${fs}px Trebuchet MS, sans-serif`;
        const x = -VW * 0.5 * (1 - slide) + VW * 0.06, y = VH - bar - fs * 0.5;
        g.lineWidth = 8; g.strokeStyle = '#000'; g.strokeText(f.def.name.toUpperCase(), x, y - fs);
        const gr = g.createLinearGradient(0, y - fs * 2, 0, y); gr.addColorStop(0, '#ffffff'); gr.addColorStop(1, f.def.accent); g.fillStyle = gr; g.fillText(f.def.name.toUpperCase(), x, y - fs);
        g.font = `italic 900 ${fs * 0.62}px Trebuchet MS, sans-serif`; g.strokeText('FAMILY FINALE!', x + fs * 0.2, y); g.fillStyle = '#ffd23f'; g.fillText('FAMILY FINALE!', x + fs * 0.2, y);
      }
    }
    g.restore();
  };

  // ------------------------------------------------------------ 3D models for items and the orb
  const T3 = window.THREE; let group = null, orbMesh = null, glowTex = null; const meshes = new Map();
  function canvasTex(w, h, draw) { const cv = document.createElement('canvas'); cv.width = w; cv.height = h; draw(cv.getContext('2d'), w, h); const t = new T3.CanvasTexture(cv); t.colorSpace = T3.SRGBColorSpace; return t; }
  const std = (c, o) => new T3.MeshStandardMaterial(Object.assign({ color: c, roughness: 0.55, metalness: 0 }, o || {}));
  function starShape(r1, r2, n = 5) { const s = new T3.Shape(); for (let i = 0; i <= n * 2; i++) { const r = i % 2 ? r2 : r1, a = (i / (n * 2)) * TAU + Math.PI / 2; const x = Math.cos(a) * r, y = Math.sin(a) * r; if (!i) s.moveTo(x, y); else s.lineTo(x, y); } return s; }
  function heartShape(k) { const s = new T3.Shape(); s.moveTo(0, -k); s.bezierCurveTo(-k * 1.4, -k * 0.2, -k * 1.1, k * 1.0, 0, k * 0.45); s.bezierCurveTo(k * 1.1, k * 1.0, k * 1.4, -k * 0.2, 0, -k); return s; }
  const ext = (shape, depth, bevel) => { const g = new T3.ExtrudeGeometry(shape, { depth, bevelEnabled: true, bevelSize: bevel, bevelThickness: bevel, bevelSegments: 3, curveSegments: 16 }); g.center(); return g; };
  // each model is built with its grip (or base) at the origin, pointing up +y
  function build(type) {
    const g = new T3.Group(), add = (geo, mat, x = 0, y = 0, z = 0) => { const m = new T3.Mesh(geo, mat); m.position.set(x, y, z); m.castShadow = true; g.add(m); return m; };
    if (type === 'crate') {
      const tex = canvasTex(256, 256, (c, w, h) => {
        c.fillStyle = '#c98b4b'; c.fillRect(0, 0, w, h); for (let k = 0; k < 5; k++) { c.fillStyle = k % 2 ? 'rgba(0,0,0,.08)' : 'rgba(255,255,255,.06)'; c.fillRect(0, k * 51, w, 51); c.fillStyle = 'rgba(60,30,10,.5)'; c.fillRect(0, k * 51, w, 3); }
        c.strokeStyle = '#7a4a22'; c.lineWidth = 26; c.strokeRect(13, 13, w - 26, h - 26); c.beginPath(); c.moveTo(26, 26); c.lineTo(w - 26, h - 26); c.stroke();
        c.fillStyle = '#ffd23f'; c.strokeStyle = '#7a4a22'; c.lineWidth = 6; c.beginPath(); for (let i = 0; i <= 10; i++) { const r = i % 2 ? 26 : 58, a = i / 10 * TAU - Math.PI / 2; c.lineTo(w / 2 + Math.cos(a) * r, h / 2 + Math.sin(a) * r); } c.fill(); c.stroke();
      });
      add(new T3.BoxGeometry(54, 54, 54), std('#ffffff', { map: tex, roughness: 0.8 }), 0, 27, 0);
    } else if (type === 'capsule') {
      const cap = new T3.CapsuleGeometry(17, 20, 8, 20), cols = [], p = cap.attributes.position, a = new T3.Color('#ff4d6d'), b = new T3.Color('#fff6ee');
      for (let i = 0; i < p.count; i++) { const c = p.getY(i) > 0 ? a : b; cols.push(c.r, c.g, c.b); } cap.setAttribute('color', new T3.Float32BufferAttribute(cols, 3));
      const m = add(cap, std('#ffffff', { vertexColors: true, roughness: 0.25, metalness: 0.1 }), 0, 20, 0); m.rotation.z = Math.PI / 2; m.position.y = 18;
      const band = add(new T3.TorusGeometry(17.5, 2.2, 8, 28), std('#ffd23f', { metalness: 0.6, roughness: 0.3 }), 0, 18, 0); band.rotation.y = Math.PI / 2;
    } else if (type === 'bat') {
      const pts = [[4.6, -12], [4.8, -10], [2.8, -8], [2.6, 18], [3.2, 30], [5.2, 52], [6.6, 66], [6.6, 72], [4.5, 75], [0, 76]].map(q => new T3.Vector2(q[0], q[1]));
      pts.unshift(new T3.Vector2(0, -12)); add(new T3.LatheGeometry(pts, 24), std('#d9a66b', { roughness: 0.45 }));
      add(new T3.CylinderGeometry(3.1, 3.1, 22, 16), std('#1b1b24', { roughness: 0.9 }), 0, 4, 0);
    } else if (type === 'blaster') {
      add(new T3.BoxGeometry(12, 30, 18), std('#ff8a1e'), 0, 18, 6);
      add(new T3.CylinderGeometry(5, 5, 34, 16), std('#2f7dff'), 0, 44, 6); add(new T3.CylinderGeometry(6.5, 6.5, 6, 16), std('#ffd23f'), 0, 62, 6);
      add(new T3.BoxGeometry(10, 18, 9), std('#2f7dff'), 0, 2, -2).rotation.x = 0.3; add(new T3.BoxGeometry(14, 8, 22), std('#ffd23f'), 0, 26, 10);
    } else if (type === 'mallet') {
      add(new T3.CylinderGeometry(2.8, 3.2, 76, 12), std('#c98b4b'), 0, 30, 0);
      const head = add(new T3.CylinderGeometry(21, 21, 50, 24), std('#ff3b3b', { roughness: 0.4 }), 0, 74, 0); head.rotation.z = Math.PI / 2;
      for (const sx of [-1, 1]) { const cap = add(new T3.CylinderGeometry(22.5, 22.5, 7, 24), std('#ffd23f', { roughness: 0.35 }), sx * 26, 74, 0); cap.rotation.z = Math.PI / 2; }
    } else if (type === 'candy') {
      const m = add(ext(starShape(24, 11), 7, 3), std('#ffd23f', { emissive: '#ffb000', emissiveIntensity: 0.6, roughness: 0.25 }), 0, 26, 0); m.userData.rainbow = true;
    } else if (type === 'juice') {
      const lbl = canvasTex(128, 192, (c, w, h) => { const gr = c.createLinearGradient(0, 0, 0, h); gr.addColorStop(0, '#b06aff'); gr.addColorStop(1, '#6a2fd6'); c.fillStyle = gr; c.fillRect(0, 0, w, h); c.fillStyle = '#ffd23f'; c.font = 'bold 30px sans-serif'; c.textAlign = 'center'; c.fillText('GROW', w / 2, 80); c.fillStyle = '#ff6ad5'; c.beginPath(); c.arc(w / 2, 130, 26, 0, TAU); c.fill(); });
      add(new T3.BoxGeometry(24, 38, 16), std('#ffffff', { map: lbl }), 0, 19, 0); add(new T3.CylinderGeometry(1.5, 1.5, 22, 8), std('#ffffff'), 5, 46, 0).rotation.z = -0.3;
    } else if (type === 'cookie') {
      add(ext(heartShape(17), 6, 2.5), std('#c98a4a', { roughness: 0.9 }), 0, 22, 0);
      add(ext(heartShape(13), 2, 1.5), std('#ff7ab0', { roughness: 0.4 }), 0, 23, 5.5);
    } else if (type === 'sneakers') {
      add(new T3.BoxGeometry(38, 7, 16), std('#ffffff'), 0, 4, 0); const up = add(new T3.CapsuleGeometry(8, 22, 6, 14), std('#ff3b3b'), -2, 15, 0); up.rotation.z = Math.PI / 2;
      add(new T3.BoxGeometry(10, 18, 15), std('#ff3b3b'), -12, 20, 0); for (const sz of [-1, 1]) { const w = add(new T3.PlaneGeometry(20, 12), std('#ffffff', { side: T3.DoubleSide }), -16, 28, sz * 9); w.rotation.z = 0.5; }
    } else if (type === 'balloon') {
      const m = add(new T3.SphereGeometry(16, 24, 18), std('#4fb6ff', { roughness: 0.12, transparent: true, opacity: 0.88 }), 0, 20, 0); m.scale.y = 1.15;
      add(new T3.ConeGeometry(3, 6, 10), std('#2f8fe0'), 0, 2, 0).rotation.x = Math.PI;
    }
    return g;
  }
  // how each item sits on the ground (lying down for long things)
  const LIE = { bat: 1.35, mallet: 1.35, blaster: 1.45 };
  function glow() {
    if (glowTex) return glowTex;
    glowTex = canvasTex(128, 128, (c, w, h) => { const gr = c.createRadialGradient(64, 64, 0, 64, 64, 64); gr.addColorStop(0, 'rgba(255,255,255,1)'); gr.addColorStop(0.3, 'rgba(255,240,200,.6)'); gr.addColorStop(1, 'rgba(255,255,255,0)'); c.fillStyle = gr; c.fillRect(0, 0, w, h); });
    return glowTex;
  }
  function makeOrb() {
    const g = new T3.Group();
    const mat = new T3.ShaderMaterial({ uniforms: { t: { value: 0 }, flash: { value: 0 } },
      vertexShader: 'varying vec3 vN; varying vec3 vP; void main(){ vN = normalize(normalMatrix * normal); vP = position; gl_Position = projectionMatrix * modelViewMatrix * vec4(position,1.0); }',
      fragmentShader: 'uniform float t; uniform float flash; varying vec3 vN; varying vec3 vP; void main(){ vec3 p = normalize(vP); float a = atan(p.z, p.x); float h = t*0.25 + p.y*0.6 + a*0.318; vec3 col = 0.55 + 0.45*cos(6.2831*(h + vec3(0.0,0.33,0.67))); float sw = 0.5 + 0.5*sin(a*3.0 + p.y*8.0 - t*3.0); col = mix(col, vec3(1.0), sw*0.25); float fr = pow(1.0 - abs(vN.z), 2.0); col += fr*0.9 + flash; gl_FragColor = vec4(col, 1.0); }' });
    const core = new T3.Mesh(new T3.SphereGeometry(26, 40, 30), mat); g.add(core);
    // an original emblem: a little house with a heart
    const em = new T3.Group(); const eM = new T3.MeshBasicMaterial({ color: '#ffffff' });
    const hs = new T3.Shape(); hs.moveTo(-10, -9); hs.lineTo(10, -9); hs.lineTo(10, 3); hs.lineTo(13, 3); hs.lineTo(0, 14); hs.lineTo(-13, 3); hs.lineTo(-10, 3); hs.lineTo(-10, -9);
    const hm = new T3.Mesh(new T3.ShapeGeometry(hs), eM); hm.position.z = 26.5; em.add(hm);
    const ht = new T3.Mesh(new T3.ShapeGeometry(heartShape(4.5)), new T3.MeshBasicMaterial({ color: '#ff4d6d' })); ht.position.set(0, -1.5, 26.8); ht.rotation.z = Math.PI; em.add(ht);
    g.add(em);
    const sp = new T3.Sprite(new T3.SpriteMaterial({ map: glow(), color: '#fff2c0', transparent: true, depthWrite: false, blending: T3.AdditiveBlending })); sp.scale.set(120, 120, 1); g.add(sp);
    g.userData = { mat, sp, em };
    return g;
  }
  function holderFor(rig) {
    if (rig.itemHolder && rig.itemHolder.parent) return rig.itemHolder;
    const hand = rig.bones && rig.bones.RightHand, fore = rig.bones && rig.bones.RightForeArm; if (!hand || !fore) return null;
    rig.root.updateMatrixWorld(true);
    const P = b => b.getWorldPosition(new T3.Vector3());
    const dir = P(hand).sub(P(fore)).normalize().applyQuaternion(hand.getWorldQuaternion(new T3.Quaternion()).invert());
    const ws = hand.getWorldScale(new T3.Vector3()).x / Math.max(0.01, rig.root.scale.x);
    const h = new T3.Group(); h.quaternion.setFromUnitVectors(new T3.Vector3(0, 1, 0), dir); h.scale.setScalar(1 / ws); h.position.set(0, 0, 0);
    hand.add(h); rig.itemHolder = h; return h;
  }
  function hook(scene) {
    if (!G.items) return;
    if (!group || group.parent !== scene) { group = new T3.Group(); scene.add(group); }
    const live = new Set();
    for (const it of G.items) {
      if (it.dead) continue; live.add(it);
      let m = meshes.get(it.id);
      if (!m) { m = { root: new T3.Group(), spin: new T3.Group(), model: build(it.type) }; m.root.add(m.spin); m.spin.add(m.model); meshes.set(it.id, m); }
      const scale = 1.25;
      if (it.held) {
        const f = it.held, rig = R3.rigOf(f), h = rig && rig.glb ? holderFor(rig) : null;
        if (h) {
          if (m.model.parent !== h) h.add(m.model);
          m.model.position.set(0, it.type === 'blaster' ? -4 : -6, 0); m.model.rotation.set(0, 0, 0); m.model.scale.setScalar(scale * (f.sc / f.baseSc));
          if (it.type === 'mallet') m.model.rotation.x = Math.sin(G.frame * 0.35) * 0.9;
          if (it.type === 'blaster') m.model.rotation.x = -1.2;
          for (const b of rig.blades) b.visible = false;
          m.root.visible = false; continue;
        }
        if (m.model.parent !== m.spin) { m.spin.add(m.model); }
        m.root.visible = true; if (m.root.parent !== group) group.add(m.root);
        m.root.position.set(f.x + f.facing * 26 * f.sc, -(f.y - 50 * f.sc), 30); m.spin.rotation.set(0, 0, -f.facing * 0.6); m.model.scale.setScalar(scale); continue;
      }
      if (m.model.parent !== m.spin) { m.model.position.set(0, 0, 0); m.model.rotation.set(0, 0, 0); m.model.scale.setScalar(scale); m.spin.add(m.model); }
      if (m.root.parent !== group) group.add(m.root);
      m.model.scale.setScalar(scale);
      const lie = LIE[it.type];
      m.root.position.set(it.x, -it.y, 0);
      if (it.box) { m.spin.rotation.set(0, Math.sin(it.t * 0.02) * 0.15, 0); m.model.position.y = 0; }
      else if (it.thrown) { m.spin.rotation.set(0, 0, it.t * 0.4 * -sgn(it.vx || 1)); m.model.position.y = -10; }
      else { m.spin.rotation.set(0, it.t * 0.04, 0); m.model.rotation.z = lie || 0; m.model.position.y = lie ? 8 + Math.sin(it.t * 0.08) * 2 : Math.sin(it.t * 0.08) * 3 + 4; }
      m.root.visible = !(it.t > 980 && !it.box && it.grounded && G.frame % 8 < 4);
      m.model.traverse(o => { if (o.material && o.userData.rainbow) o.material.color.setHSL((G.frame * 0.01) % 1, 1, 0.6); });
    }
    for (const [id, m] of meshes) {
      const it = G.items.find(i => i.id === id);
      if (!it || it.dead) { if (m.model.parent) m.model.parent.remove(m.model); if (m.root.parent) m.root.parent.remove(m.root); meshes.delete(id); }
    }
    // the orb
    if (G.orb) {
      if (!orbMesh) orbMesh = makeOrb(); if (orbMesh.parent !== group) group.add(orbMesh);
      const o = G.orb; orbMesh.visible = true; orbMesh.position.set(o.x, -o.y, 20); orbMesh.rotation.y = G.frame * 0.03; orbMesh.rotation.x = Math.sin(G.frame * 0.02) * 0.3;
      orbMesh.userData.mat.uniforms.t.value = G.frame / 60; orbMesh.userData.mat.uniforms.flash.value = o.flash / 8;
      const p = 1 + Math.sin(G.frame * 0.15) * 0.12; orbMesh.userData.sp.scale.set(120 * p, 120 * p, 1);
    } else if (orbMesh) orbMesh.visible = false;
    // giant fighters
    for (const f of G.fighters) {
      const rig = R3.rigOf(f); if (!rig || !rig.root) continue;
      const k = f.sc / (f.baseSc || f.sc); rig.gk = rig.gk ? rig.gk + (k - rig.gk) * 0.15 : k;
      if (Math.abs(rig.gk - 1) > 0.01) { rig.root.scale.setScalar(rig.gk); rig.root.position.y += (rig.hipRest || 40) * (rig.gk - 1); } else rig.root.scale.setScalar(1);
      if (!f.item && rig.itemHolder) { /* nothing held: the sword (if any) comes back on its own */ }
    }
  }
  if (typeof R3 !== 'undefined' && R3.addHook) R3.addHook(hook);
  window.ITEMS = { reset, spawnBox, spawnItem, spawnOrb, startFinale, ITEMS };
})();
