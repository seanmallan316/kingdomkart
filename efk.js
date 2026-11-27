'use strict';
// Super Family Smash: Effekseer visual effects. The real Effekseer runtime (WebAssembly) draws official Effekseer
// sample effects into the game's own WebGL canvas, over the finished frame, through the game camera:
//   every hit      -> a toon impact burst (ToonHit), sized by damage;   heavy hits and launches -> the big blow (Blow1)
//   prop "BONK"    -> blow;     pulse attacks -> an expanding ring;     power-ups and respawns -> the heal aura (Cure1)
//   KOs            -> a pillar of light where the fighter left
// Everything is packed in efkjs.js (runtime + effects + textures as data), so it also works from a saved file.
(() => {
  if (typeof G === 'undefined' || typeof R3 === 'undefined' || !R3.addPost || !window.effekseer || !window.EFK_PACK) return;
  const PACK = window.EFK_PACK, E = window.EFK = { ready: false, on: true, fx: {} };
  let ctx = null, queue = [];
  // the packed files become in-memory files (blob URLs), which the runtime loads like ordinary downloads
  const bytes = d => { const b = atob(d.slice(d.indexOf(',') + 1)), u = new Uint8Array(b.length); for (let i = 0; i < b.length; i++) u[i] = b.charCodeAt(i); return u; };
  const mime = d => d.slice(5, d.indexOf(';'));
  const base = p => p.replace(/\\/g, '/').split('/').pop().toLowerCase();
  const urls = {}, byBase = {}, bufs = {};
  for (const k in PACK.files) { const u8 = bytes(PACK.files[k]); bufs[k] = u8.buffer; urls[k] = URL.createObjectURL(new Blob([u8], { type: mime(PACK.files[k]) })) + '#' + base(k); /* the runtime tells images from data by the extension */ const dir = k.split('/').slice(0, -1).join('/'); byBase[dir.split('/')[0] + ':' + base(k)] = urls[k]; byBase[base(k)] = byBase[base(k)] || urls[k]; }
  const redirectFor = root => path => { const k = path.replace(/\\/g, '/'); return urls[k] || byBase[root + ':' + base(k)] || byBase[base(k)] || path; };
  const wasmUrl = URL.createObjectURL(new Blob([bytes(PACK.wasm)], { type: 'application/wasm' }));
  function boot(gl) {
    E.booting = true;
    effekseer.initRuntime(wasmUrl, () => {
      try {
        ctx = effekseer.createContext(); ctx.init(gl, { instanceMaxCount: 4000, squareMaxCount: 8000 });
        if (ctx.setRestorationOfStatesFlag) ctx.setRestorationOfStatesFlag(true);
        // one effect at a time: the runtime tracks a single "effect being loaded", so parallel loads lose textures
        const names = Object.keys(PACK.effects).filter(n => bufs[PACK.effects[n]] && !/^Laser/.test(n)); // the lasers need a sound file that is not packed
        const next = i => {
          if (i >= names.length) { E.ready = true; return; } const name = names[i]; let done = false;
          const go = ok => { if (done) return; done = true; if (!ok) delete E.fx[name]; next(i + 1); };
          E.fx[name] = ctx.loadEffect(bufs[PACK.effects[name]], 1.0, () => go(true), (m, u) => { console.warn('effekseer effect skipped', name, m, u); go(false); }, redirectFor(PACK.effects[name].split('/')[0]));
          setTimeout(() => go(false), 8000);
        };
        next(0);
      } catch (e) { console.warn('effekseer unavailable', e); }
    }, () => console.warn('effekseer runtime failed to load'));
  }
  // play an effect at a game position (x right, y down), sized in game units
  E.play = (name, x, y, size = 40, o = {}) => {
    if (!E.on || !ctx || !E.ready || !E.fx[name] || G.fxLow) return null;
    const h = ctx.play(E.fx[name], x, -y, o.z || 30); if (!h) return null;
    h.setScale(size, size, size); if (o.rot) h.setRotation(o.rot[0], o.rot[1], o.rot[2]); if (o.speed) h.setSpeed(o.speed);
    return h;
  };
  R3.addPost((renderer, camera, dt) => {
    if (!ctx) { if (!E.booting) boot(renderer.getContext()); return; }
    if (!E.ready || !E.on) return;
    const paused = G.mode === 'fight' && G.paused;
    ctx.update(paused ? 0 : Math.min(4, (dt || 16.7) / 16.667) * (G.speed ? G.speed / 0.8 : 1));
    ctx.setProjectionMatrix(camera.projectionMatrix.elements); ctx.setCameraMatrix(camera.matrixWorldInverse.elements);
    ctx.draw(); renderer.resetState();
  });

  // ---------------------------------------------------------------- triggers
  const baseHit = applyHit;
  applyHit = function (att, vic, hb, dir, o) {
    const before = vic.percent, r = baseHit.apply(this, arguments), dmg = vic.percent - before;
    if (dmg > 0.5) {
      const x = o && o.px != null ? o.px : vic.x, y = o && o.py != null ? o.py : vic.y - vic.h * 0.5;
      if (dmg >= 9 || (hb && (hb.g || 0) >= 95)) { E.play('Blow1', x, y, 7 + Math.min(11, dmg * 0.55)); E.play('ToonHit', x, y, 9 + Math.min(12, dmg * 0.7)); }
      else E.play('ToonHit', x, y, 5.5 + dmg * 0.9);
    }
    return r;
  };
  // per frame: watch for KOs, respawns, pulses, power-ups and beam finales
  const seen = new WeakMap(); let pulseSeen = new WeakSet(), cineSeen = null;
  R3.addHook(() => {
    if (!E.ready || G.mode !== 'fight') return;
    for (const f of G.fighters) {
      const s = seen.get(f) || { state: f.state, item: null, star: 0, giant: 0 };
      if (f.state !== s.state) {
        if (f.state === 'dead' || f.state === 'out') E.play('Light', Math.max(G.stage.blast.l + 120, Math.min(G.stage.blast.r - 120, f.x)), Math.max(G.stage.blast.t + 160, Math.min(G.stage.blast.b - 200, f.y)), 46);
        if (f.state === 'respawn') E.play('Cure1', f.x, f.y - f.h * 0.5, 26);
      }
      if ((f.starT > 0 && !s.star) || (f.giantT > 0 && !s.giant) || (f.final && !s.final)) E.play('Cure1', f.x, f.y - f.h * 0.5, 24 * (f.sc || 1));
      s.state = f.state; s.star = f.starT > 0; s.giant = f.giantT > 0; s.final = !!f.final; seen.set(f, s);
    }
    for (const p of G.pulses || []) if (!pulseSeen.has(p)) { pulseSeen.add(p); E.play('Ring1', p.x, p.y + 40, Math.max(12, (p.R || 200) * 0.09)); }
  });
})();
