'use strict';
// Norah's Family Finale: Pink Fluff, her spirit dragon (the 3D model made from the family's own Pink Fluff artwork),
// swoops in, snaps up each opponent and hurls them away. items.js drives the flight path (G.cine.dr = { x, y, vx, vy,
// trail[], chomp }); this file only draws the dragon, banking and bobbing along it, lunging on every chomp.
(() => {
  if (typeof R3 === 'undefined' || !R3.addHook || !window.THREE) return;
  const T = window.THREE, Z = 140, HEIGHT = 420;
  let D = null;
  function build(scene) {
    const g = new T.Group(), holder = new T.Group(), model = new T.Group(); g.add(holder); holder.add(model); scene.add(g);
    const load = window.PETRIG && window.PETRIG.load;
    if (load) load('fluff', sc => {
      const m = sc.clone(true);
      m.traverse(o => { if (o.isMesh) { o.castShadow = true; o.frustumCulled = false; o.material = o.material.clone(); o.material.emissive = new T.Color('#ff4aa8'); o.material.emissiveIntensity = 0.12; o.material.side = T.DoubleSide; } });
      const box = new T.Box3().setFromObject(m), size = box.getSize(new T.Vector3()), c = box.getCenter(new T.Vector3()), k = HEIGHT / size.y;
      // the snout (front, upper part of the model) sits at the group's origin, where items.js carries the caught fighters
      m.scale.setScalar(k); m.position.set(-c.x * k, -(box.min.y + size.y * 0.66) * k, -box.max.z * k + 30);
      model.add(m);
    });
    const glow = new T.PointLight('#ff6ad5', 3, 1200, 1.4); g.add(glow);
    return { g, holder, glow };
  }
  const tgt = new T.Vector3();
  function update(d) {
    const tr = d.trail; if (!tr.length) { D.g.visible = false; return; }
    D.g.visible = true;
    const hx = tr[0].x, hy = tr[0].y;
    D.g.position.set(hx, -hy, Z);
    tgt.set(hx + d.vx * 10, -hy - d.vy * 10, Z); D.g.lookAt(tgt); // face along the flight
    const t = d.t;
    D.holder.rotation.z = Math.sin(t * 0.09) * 0.18 - Math.max(-0.5, Math.min(0.5, d.vy * 0.02)); // bank into turns
    D.holder.position.set(0, Math.sin(t * 0.22) * 14, d.chomp > 0 ? 50 * (d.chomp / 10) : 0); // wing-beat bob, lunge on a bite
    const breathe = 1 + 0.04 * Math.sin(t * 0.22); D.holder.scale.set(1, breathe, 1);
    D.glow.position.set(0, 40, 80);
  }
  R3.addHook(scene => {
    const c = G.cine, d = c && c.dr;
    if (!d) { if (D) D.g.visible = false; return; }
    if (!D || D.g.parent !== scene) { if (D && D.g.parent) D.g.parent.remove(D.g); D = build(scene); }
    update(d);
  });
})();
