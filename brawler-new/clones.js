'use strict';
// Rosie's Family Finale, the clone burst (from her video): eight glowing ghost copies of her burst out of an energy ring,
// claws first, then dive at the nearest foes. items.js moves them (G.cine.clones = [{ x, y, vx, vy }]); this file draws
// each one as a real copy of her 3D model in her claw-dash pose, see-through and glowing violet, facing where it flies.
(() => {
  if (typeof R3 === 'undefined' || !R3.addHook || !window.THREE || !window.PETRIG) return;
  const T = window.THREE, N = 8;
  let pool = null, poolScene = null;
  function ghost(rig) {
    for (const m of rig.look.mats) {
      m.transparent = true; m.opacity = 0.55; m.depthWrite = false; m.blending = T.AdditiveBlending;
      m.emissive = new T.Color('#9b6bff'); m.emissiveIntensity = 0.9; m.needsUpdate = true;
    }
    rig.root.traverse(o => { if (o.isMesh) { o.castShadow = false; o.renderOrder = 6; } });
  }
  function build(scene, def) {
    pool = []; poolScene = scene;
    for (let k = 0; k < N; k++) {
      const rig = PETRIG.build(def); rig.root.visible = false; scene.add(rig.root);
      const c = { rig, ghosted: false, fake: { anim: 0, state: 'attack', vx: 0, vy: 0, def: { run: 6 }, grounded: true, flipT: 0, stocks: 1, t: 0, mt: 14, move: { len: 40, hb: [{ f: [12, 24] }] }, moves: {}, xhb: [] } };
      c.fake.moves.fsmash = c.fake.move; // her claw-dash lunge: one paw stretched far forward
      pool.push(c);
    }
  }
  R3.addHook(scene => {
    const c = typeof G !== 'undefined' && G.cine, list = c && c.type === 'clones' && c.clones;
    if (!list) { if (pool) for (const p of pool) p.rig.root.visible = false; return; }
    if (!pool || poolScene !== scene) { if (pool) for (const p of pool) if (p.rig.root.parent) p.rig.root.parent.remove(p.rig.root); build(scene, c.f.def); }
    list.forEach((cl, k) => {
      const p = pool[k]; if (!p) return; const rig = p.rig;
      if (!p.ghosted && rig.look.mats.length) { ghost(rig); p.ghosted = true; }
      p.fake.anim++; PETRIG.pose(rig, p.fake, 0, null);
      rig.root.visible = true;
      rig.root.position.set(cl.x, -cl.y - rig.H * 0.4, 40 * Math.sin(k * 1.7)); // a little depth spread so they don't overlap
      // face along the flight: head toward +x, then roll the whole body onto the direction of travel
      const vx = cl.vx || Math.cos(cl.a), vy = -(cl.vy || Math.sin(cl.a)), ang = Math.atan2(vy, vx);
      if (vx >= 0) { rig.turn.rotation.y = Math.PI / 2; rig.root.rotation.z = ang; } else { rig.turn.rotation.y = -Math.PI / 2; rig.root.rotation.z = ang - Math.PI; }
      const pulse = 0.85 + 0.15 * Math.sin((p.fake.anim + k * 5) * 0.4); rig.root.scale.setScalar(pulse);
    });
    for (let k = list.length; k < pool.length; k++) pool[k].rig.root.visible = false;
  });
})();
