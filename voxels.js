'use strict';
// Super Family Smash: destructible voxel towers (a native take on what Voxel Tools does in Godot).
// Two towers of small cubes stand at the back corners of every stage, themed to it (toy bricks in the bedroom, sugar
// cubes in the kitchen, red brick on the driveway, timber in the treehouse). They are real voxel grids:
//  - attacks, pulse attacks, fast physics props and fighters sent flying carve out whatever cubes they touch;
//  - after every hit the grid is checked for support: any cube no longer connected to the ground breaks loose;
//  - every cube that leaves becomes a real rigid body (Jolt) thrown along the blow, so walls crumble and scatter.
// The towers are scenery: they don't block fighters. They rebuild at the start of each match.
(() => {
  if (typeof G === 'undefined' || typeof R3 === 'undefined' || !window.THREE) return;
  const T = window.THREE, V = 14, VX = window.VOXELS = { towers: [] };
  const LOOK = {
    bedroom: { cols: ['#e03131', '#1c7ed6', '#f59f00', '#37b24d', '#f783ac'], mix: 'cell' },
    kitchen: { cols: ['#ffffff', '#f4f1ea', '#fff8e7'], mix: 'rand' },
    driveway: { cols: ['#b5533c', '#a44a35', '#c0614a', '#8f3f2e'], mix: 'rand' },
    treehouse: { cols: ['#b9824a', '#a06a36', '#c99557', '#8a5a2b'], mix: 'row' },
  };
  let group = null, forStage = null;
  function tower(x0, zc, look, depth, seed) {
    const W = 6, H = 11, D = depth, cells = new Map(), key = (i, j, k) => i + ',' + j + ',' + k;
    const im = new T.InstancedMesh(new T.BoxGeometry(V - 0.6, V - 0.6, V - 0.6), new T.MeshStandardMaterial({ roughness: 0.7 }), W * H * D), o = new T.Object3D(), c = new T.Color();
    let n = 0; const cols = look.cols;
    for (let j = 0; j < H; j++) for (let i = 0; i < W; i++) for (let k = 0; k < D; k++) {
      if (j >= H - 1 && i % 2 === 1) continue;                                   // battlements
      if (j >= 2 && j <= 4 && i >= 2 && i <= 3 && D > 1 && k === D - 1) continue; // a window in the front face
      const ci = look.mix === 'row' ? j : look.mix === 'cell' ? (i * 7 + j * 3 + k * 5 + seed) : Math.floor(Math.abs(Math.sin(i * 12.9 + j * 78.2 + k * 37.7 + seed)) * 97);
      const cell = { i, j, k, n, col: cols[ci % cols.length], x: x0 + (i - (W - 1) / 2) * V, y: G.mainSurf.y - V / 2 - j * V, z: zc + (k - (D - 1) / 2) * V, on: true };
      o.position.set(cell.x, -cell.y, cell.z); o.scale.setScalar(1); o.updateMatrix(); im.setMatrixAt(n, o.matrix); im.setColorAt(n, c.set(cell.col).multiplyScalar(0.9 + Math.abs(Math.sin(n * 3.3)) * 0.2)); cells.set(key(i, j, k), cell); n++;
    }
    im.count = n; im.castShadow = true; im.receiveShadow = true; im.instanceColor.needsUpdate = true; group.add(im);
    return { im, cells, key, W, H, D, x0, o };
  }
  function build(scene) {
    if (group && group.parent) group.parent.remove(group); group = new T.Group(); scene.add(group); VX.towers = [];
    const m = G.mainSurf, depth = (G.stage.main.depth || 120), D = Math.max(1, Math.min(3, Math.floor((depth / 2 - 8) / V))), zc = -(depth / 2) + D * V / 2 + 4;
    const look = LOOK[G.stage.theme] || LOOK.driveway;
    VX.towers.push(tower(m.x1 + 64, zc, look, D, 1), tower(m.x2 - 64, zc, look, D, 2));
  }
  const hide = new T.Matrix4().makeScale(0, 0, 0);
  function knock(t, cell, vx, vy) {
    if (!cell.on) return; cell.on = false; t.im.setMatrixAt(cell.n, hide); t.dirty = true;
    if (window.PHYS && PHYS.cube) PHYS.cube(cell.x, cell.y, cell.z, V / 2 - 0.5, cell.col, vx + (Math.random() - 0.5) * 3, vy + Math.random() * 3, (Math.random() - 0.3) * 3);
  }
  // carve everything within r of (x, y); returns how many cubes went
  function carve(x, y, r, vx, vy) {
    let n = 0;
    for (const t of VX.towers) {
      if (Math.abs(x - t.x0) > r + t.W * V) continue;
      for (const cell of t.cells.values()) if (cell.on && Math.hypot(cell.x - x, cell.y - y) < r + V * 0.5) { knock(t, cell, vx, vy); n++; }
      if (n) settle(t);
    }
    return n;
  }
  // anything not connected to the ground row falls
  function settle(t) {
    const seen = new Set(), stack = [];
    for (const cell of t.cells.values()) if (cell.on && cell.j === 0) { seen.add(cell); stack.push(cell); }
    while (stack.length) { const c = stack.pop(); for (const [di, dj, dk] of [[1, 0, 0], [-1, 0, 0], [0, 1, 0], [0, -1, 0], [0, 0, 1], [0, 0, -1]]) { const nb = t.cells.get(t.key(c.i + di, c.j + dj, c.k + dk)); if (nb && nb.on && !seen.has(nb)) { seen.add(nb); stack.push(nb); } } }
    for (const cell of t.cells.values()) if (cell.on && !seen.has(cell)) knock(t, cell, (Math.random() - 0.5) * 2, 0.5);
  }
  VX.carve = carve;
  const baseStep = step; step = function () {
    baseStep(); if (G.mode !== 'fight' || !VX.towers.length || G.countdown > 0) return;
    let broke = 0;
    for (const a of G.fighters) {
      for (const h of activeHBs(a)) { if (h.grab) continue; const w = hbWorld(a, h), ang = (h.a === 361 || h.a == null ? 38 : h.a) * Math.PI / 180, sp = 4 + (h.d || 4) * 0.5; broke += carve(w.x, w.y, w.r * 0.85, Math.cos(ang) * a.facing * sp, Math.sin(ang) * sp); }
      if (a.state === 'hitstun' && Math.hypot(a.vx + (a.lvx || 0), a.vy + (a.lvy || 0)) > 9) broke += carve(a.x, a.y - a.h * 0.5, a.w * 0.6, (a.vx + (a.lvx || 0)) * 0.5, -(a.vy + (a.lvy || 0)) * 0.5); // a fighter sent flying smashes through
    }
    for (const p of G.pulses || []) broke += carve(p.x, p.y, 0, 0, 0) + ringCarve(p);
    if (window.PHYS && PHYS.ready) for (const p of PHYS.props) { const v = p.b.GetLinearVelocity(), sp = Math.hypot(v.GetX(), v.GetY()); if (sp > 7) { const q = p.b.GetPosition(); broke += carve(q.GetX() * 100, -q.GetY() * 100, p.r, v.GetX() * 0.5, v.GetY() * 0.5); } }
    if (broke) { G.shake = Math.max(G.shake, Math.min(6, 1 + broke * 0.25)); if (broke > 2 && Sfx.sample) Sfx.sample('itemBreak', Math.min(0.7, 0.2 + broke * 0.03), 1.3); }
  };
  function ringCarve(p) { let n = 0; for (const t of VX.towers) { for (const cell of t.cells.values()) { if (!cell.on) continue; const d = Math.hypot(cell.x - p.x, cell.y - p.y); if (d < p.r && d > p.r - 26) { knock(t, cell, (cell.x - p.x) / (d || 1) * 7, -(cell.y - p.y) / (d || 1) * 7 + 2); n++; } } if (n) settle(t); } return n; }
  if (window.__clash) window.__clash.step = step;
  R3.addHook(scene => {
    if (G.mode !== 'fight' && G.mode !== 'results') { if (group) group.visible = false; return; }
    if (forStage !== G.fighters || !group || group.parent !== scene) { forStage = G.fighters; build(scene); } // a new match: fresh towers
    group.visible = true;
    for (const t of VX.towers) if (t.dirty) { t.dirty = false; t.im.instanceMatrix.needsUpdate = true; }
  });
})();
