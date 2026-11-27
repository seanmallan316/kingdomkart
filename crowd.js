'use strict';
// Super Family Smash: living scenery with real pathfinding (Recast Navigation, WebAssembly).
// On the Driveway a flock of chickens lives on the lawn. Recast builds a navigation mesh of the yard from its
// geometry (the lawn, minus the driveway slab, the bike and the hoop pole), and a Detour crowd walks each chicken
// along real paths around those obstacles and around each other: they wander to random spots, stop to peck, and
// when a big hit lands nearby they panic, flap and run for a spot far from the blast, then drift back.
(() => {
  if (typeof G === 'undefined' || typeof R3 === 'undefined' || !window.THREE || !window.RECAST) return;
  const T = window.THREE, RC = window.RECAST, S = 0.01, CR = window.CROWD = { ready: false, birds: [] };
  let inited = null, nav = null, crowd = null, query = null, group = null, lastShake = 0, forStage = null;
  const boxTris = (pos, idx, x1, x2, z1, z2, y0, y1) => { // an obstacle: a solid box standing on the ground
    const b = pos.length / 3; pos.push(x1, y0, z1, x2, y0, z1, x2, y0, z2, x1, y0, z2, x1, y1, z1, x2, y1, z1, x2, y1, z2, x1, y1, z2);
    for (const f of [[0, 2, 1], [0, 3, 2], [4, 5, 6], [4, 6, 7], [0, 1, 5], [0, 5, 4], [1, 2, 6], [1, 6, 5], [2, 3, 7], [2, 7, 6], [3, 0, 4], [3, 4, 7]]) idx.push(b + f[0], b + f[1], b + f[2]);
  };
  function chicken(col) {
    const g = new T.Group(), M = (c, o) => new T.MeshStandardMaterial(Object.assign({ color: c, roughness: 0.8, flatShading: true }, o || {}));
    const body = new T.Mesh(new T.IcosahedronGeometry(11, 1), M(col)); body.scale.set(1, 0.85, 1.25); body.position.y = 15; g.add(body);
    const tail = new T.Mesh(new T.ConeGeometry(5, 12, 5), M(col)); tail.position.set(0, 22, -13); tail.rotation.x = -0.9; g.add(tail);
    const neck = new T.Group(); neck.position.set(0, 20, 9); g.add(neck);
    const head = new T.Mesh(new T.IcosahedronGeometry(6, 1), M(col)); head.position.set(0, 9, 3); neck.add(head);
    const beak = new T.Mesh(new T.ConeGeometry(2.2, 6, 4), M('#f2a900')); beak.rotation.x = Math.PI / 2; beak.position.set(0, 8, 10); neck.add(beak);
    const comb = new T.Mesh(new T.BoxGeometry(2, 5, 7), M('#d62828')); comb.position.set(0, 15, 3); neck.add(comb);
    const wings = [-1, 1].map(sx => { const w = new T.Mesh(new T.BoxGeometry(2, 9, 14), M(col)); w.position.set(sx * 10, 16, 0); g.add(w); return w; });
    const legs = [-1, 1].map(sx => { const l = new T.Mesh(new T.CylinderGeometry(0.9, 0.9, 9, 4), M('#f2a900')); l.position.set(sx * 4, 4.5, 0); g.add(l); return l; });
    g.traverse(o => { if (o.isMesh) o.castShadow = true; });
    return { g, neck, wings, legs };
  }
  async function build(scene) {
    if (!inited) inited = RC.init();
    await inited; if (forStage !== G.stage) return;
    const m = G.stage.main, pos = [], idx = [], X = 19, Z1 = -11, Z2 = 3.3;
    pos.push(-X, 0, Z1, X, 0, Z1, X, 0, Z2, -X, 0, Z2); idx.push(0, 2, 1, 0, 3, 2); // the yard
    boxTris(pos, idx, m.x1 * S - 0.35, m.x2 * S + 0.35, -1.95, 1.95, 0, 1.2);                 // the driveway slab (the fight)
    boxTris(pos, idx, (m.x2 + 260) * S - 0.5, (m.x2 + 260) * S + 0.5, -5.7, -4.7, 0, 3);     // the hoop pole
    boxTris(pos, idx, (m.x1 + 40) * S - 0.9, (m.x1 + 40) * S + 0.9, -2.8, -2.05, 0, 1);      // the bike
    const r = RC.generateSoloNavMesh(pos, idx, { cs: 0.15, ch: 0.1, walkableRadius: 2, walkableHeight: 4, walkableClimb: 1, walkableSlopeAngle: 45 });
    if (!r.success) { console.warn('navmesh failed', r.error); return; }
    nav = r.navMesh; query = new RC.NavMeshQuery(nav); crowd = new RC.Crowd(nav, { maxAgents: 24, maxAgentRadius: 0.4 });
    group = new T.Group(); scene.add(group); CR.birds = [];
    const cols = ['#ffffff', '#f3e9d2', '#b5651d', '#e8d5a3', '#fafafa', '#7a4a2a'], n = matchMedia('(pointer: coarse)').matches ? 9 : 16;
    for (let i = 0; i < n; i++) {
      const front = i % 3 === 0, c = query.findClosestPoint({ x: (Math.random() - 0.5) * 24, y: 0, z: front ? 2.6 : -3 - Math.random() * 6 });
      const agent = crowd.addAgent(c.point || c, { radius: 0.22, height: 0.4, maxAcceleration: 9, maxSpeed: 1.1, collisionQueryRange: 2.5, pathOptimizationRange: 6, separationWeight: 2 });
      const v = chicken(cols[i % cols.length]); v.g.scale.setScalar(0.8 + Math.random() * 0.5); group.add(v.g);
      CR.birds.push({ agent, v, wait: Math.random() * 1.5, panic: 0, yaw: Math.random() * 6, ph: Math.random() * 6, front });
    }
    CR.ready = true;
  }
  function clear() { if (group && group.parent) group.parent.remove(group); group = null; try { crowd && crowd.destroy(); nav && nav.destroy(); } catch (e) {} crowd = nav = query = null; CR.birds = []; CR.ready = false; }
  const target = (b, x, z, rad) => { const r = query.findRandomPointAroundCircle({ x, y: 0, z }, rad); const p = r && (r.randomPoint || r.point); if (p) b.agent.requestMoveTarget(p); };
  let lastT = performance.now();
  R3.addHook(scene => {
    const want = G.mode === 'fight' && G.stage && G.stage.theme === 'driveway' && !G.fxLow;
    if (!want) { if (group) { clear(); forStage = null; } return; }
    if (forStage !== G.stage) { clear(); forStage = G.stage; build(scene).catch(e => console.warn('crowd failed', e)); return; }
    if (!CR.ready) return; if (group.parent !== scene) scene.add(group);
    const now = performance.now(), dt = Math.min(0.1, (now - lastT) / 1000); lastT = now; if (G.paused) return;
    // a big hit: everyone nearby scatters away from it
    const boom = G.shake > 4.5 && lastShake <= 4.5; lastShake = G.shake;
    if (boom) { const hx = (G.fighters.reduce((a, f) => a + f.x, 0) / Math.max(1, G.fighters.length)) * S; for (const b of CR.birds) { const p = b.agent.position(); if (Math.abs(p.x - hx) < 12) { b.panic = 2.2 + Math.random(); b.agent.maxSpeed = 3.6; b.agent.maxAcceleration = 22; target(b, p.x + Math.sign(p.x - hx || (Math.random() - 0.5)) * 7, b.front ? 2.6 : p.z - 2, 2.5); } } }
    crowd.update(dt);
    for (const b of CR.birds) {
      const p = b.agent.position(), vel = b.agent.velocity(), sp = Math.hypot(vel.x, vel.z);
      if (b.panic > 0) { b.panic -= dt; if (b.panic <= 0) { b.agent.maxSpeed = 1.1; b.agent.maxAcceleration = 9; b.wait = 0; } }
      else if ((b.wait -= dt) <= 0) { b.wait = 2.5 + Math.random() * 6; target(b, p.x, p.z, 4 + Math.random() * 5); }
      if (sp > 0.08) { let d = Math.atan2(vel.x, vel.z) - b.yaw; while (d > Math.PI) d -= Math.PI * 2; while (d < -Math.PI) d += Math.PI * 2; b.yaw += d * Math.min(1, dt * 9); }
      b.ph += dt * (4 + sp * 9);
      const g = b.v.g; g.position.set(p.x / S, -8 + Math.abs(Math.sin(b.ph)) * sp * 2.2 + (b.panic > 0 ? Math.abs(Math.sin(b.ph * 1.7)) * 9 : 0), p.z / S); g.rotation.y = b.yaw;
      b.v.legs[0].rotation.x = Math.sin(b.ph) * 0.8 * Math.min(1, sp * 2); b.v.legs[1].rotation.x = -b.v.legs[0].rotation.x;
      const flap = b.panic > 0 ? Math.sin(now * 0.04 + b.ph) * 0.9 + 0.6 : 0; b.v.wings[0].rotation.z = -flap; b.v.wings[1].rotation.z = flap;
      // standing still: peck at the ground; walking: the head bobs with each step
      b.v.neck.rotation.x = sp < 0.1 && b.panic <= 0 ? Math.max(0, Math.sin(now * 0.004 + b.ph)) * 1.1 : Math.sin(b.ph * 2) * 0.18;
    }
  });
})();
