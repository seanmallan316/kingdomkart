'use strict';
// Super Family Smash (90's Version): energy ribbon trails on every strike, like the glowing swooshes in martial-arts reference videos.
// While a fighter attacks, each hand and foot that is moving fast lays down a ribbon: the path is smoothed into a curve
// (Catmull-Rom), the ribbon always faces the camera, tapers at the tail, fades out over about a quarter of a second,
// and glows white-hot in the middle and in the fighter's own colour at the edges (additive light).
(() => {
  if (typeof R3 === 'undefined' || !R3.addHook || !window.THREE) return;
  const T = window.THREE, LIFE = 0.24, MAXP = 48, SUB = 4;
  const LIMBS = ['LeftHand', 'RightHand', 'LeftFoot', 'RightFoot'];
  const mat = new T.ShaderMaterial({
    transparent: true, depthWrite: false, blending: T.AdditiveBlending, side: T.DoubleSide,
    uniforms: {},
    vertexShader: `attribute float aA; attribute float aU; attribute vec3 aC; varying float vA; varying float vU; varying vec3 vC;
      void main() { vA = aA; vU = aU; vC = aC; gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0); }`,
    fragmentShader: `varying float vA; varying float vU; varying vec3 vC;
      void main() { float e = 1.0 - abs(vU * 2.0 - 1.0); float core = smoothstep(0.55, 1.0, e); float glow = smoothstep(0.0, 1.0, e);
        vec3 c = mix(vC, vec3(1.0), core * 0.85); gl_FragColor = vec4(c * (glow * 0.9 + core * 0.8) * vA, 1.0); }`,
  });
  const trails = new Map(); // fighter -> { limbs: [{ pts: [{p, t}], last }], h, col, mesh }
  let group = null, clock = performance.now() / 1000;
  const tmp = new T.Vector3(), v = new T.Vector3(), side = new T.Vector3(), tan = new T.Vector3();
  function make(f, rig) {
    const geo = new T.BufferGeometry(), n = LIMBS.length * MAXP * SUB * 2;
    geo.setAttribute('position', new T.BufferAttribute(new Float32Array(n * 3), 3).setUsage(T.DynamicDrawUsage));
    geo.setAttribute('aA', new T.BufferAttribute(new Float32Array(n), 1).setUsage(T.DynamicDrawUsage));
    geo.setAttribute('aU', new T.BufferAttribute(new Float32Array(n), 1).setUsage(T.DynamicDrawUsage));
    geo.setAttribute('aC', new T.BufferAttribute(new Float32Array(n * 3), 3).setUsage(T.DynamicDrawUsage));
    geo.setIndex(new T.BufferAttribute(new Uint32Array(n * 3), 1).setUsage(T.DynamicDrawUsage));
    const mesh = new T.Mesh(geo, mat); mesh.frustumCulled = false; mesh.renderOrder = 5;
    // the fighter's own colour (Ray: blue, like the video); greyish colours glow in the accent colour instead
    let col = new T.Color((f.def && f.def.color) || '#7fb4ff'); const hsl = {}; col.getHSL(hsl);
    if (hsl.s < 0.3 && f.def && f.def.accent) { col = new T.Color(f.def.accent); col.getHSL(hsl); } col.setHSL(hsl.h, Math.min(1, hsl.s * 1.1 + 0.15), Math.max(0.55, hsl.l)); // keep it bright enough to glow
    // body height in world units, for ribbon width and the speed threshold
    const a = rig.bones.Head, b = rig.bones.LeftFoot; let h = 100;
    if (a && b) { a.updateWorldMatrix(true, false); b.updateWorldMatrix(true, false); h = Math.max(20, a.getWorldPosition(new T.Vector3()).distanceTo(b.getWorldPosition(new T.Vector3())) * 1.15); }
    return { limbs: LIMBS.map(() => ({ pts: [], last: null })), h, col, mesh };
  }
  // Catmull-Rom point between p1 and p2
  const cr = (p0, p1, p2, p3, t, out) => {
    const t2 = t * t, t3 = t2 * t;
    return out.set(0, 0, 0).addScaledVector(p0, -0.5 * t3 + t2 - 0.5 * t).addScaledVector(p1, 1.5 * t3 - 2.5 * t2 + 1)
      .addScaledVector(p2, -1.5 * t3 + 2 * t2 + 0.5 * t).addScaledVector(p3, 0.5 * t3 - 0.5 * t2);
  };
  function build(tr, cam, now) {
    const geo = tr.mesh.geometry, P = geo.attributes.position.array, A = geo.attributes.aA.array, U = geo.attributes.aU.array, Cc = geo.attributes.aC.array, I = geo.index.array;
    let vi = 0, ii = 0; const c = tr.col;
    for (const L of tr.limbs) {
      const pts = L.pts; if (pts.length < 2) continue;
      const curve = [];
      for (let k = 0; k < pts.length - 1; k++) {
        const p0 = pts[Math.max(0, k - 1)].p, p1 = pts[k].p, p2 = pts[k + 1].p, p3 = pts[Math.min(pts.length - 1, k + 2)].p;
        for (let s = 0; s < SUB; s++) { const u = s / SUB; curve.push({ p: cr(p0, p1, p2, p3, u, new T.Vector3()), t: pts[k].t + (pts[k + 1].t - pts[k].t) * u }); }
      }
      curve.push({ p: pts[pts.length - 1].p.clone(), t: pts[pts.length - 1].t });
      const start = vi;
      for (let k = 0; k < curve.length; k++) {
        const q = curve[k], age = (now - q.t) / LIFE, life = Math.max(0, 1 - age), along = k / (curve.length - 1);
        tan.subVectors(curve[Math.min(curve.length - 1, k + 1)].p, curve[Math.max(0, k - 1)].p);
        v.subVectors(cam.position, q.p); side.crossVectors(tan, v).normalize();
        const w = tr.h * 0.05 * (0.25 + 0.75 * along) * (0.4 + 0.6 * life); // thin tail, full at the fist / foot
        const a = Math.pow(life, 1.3) * (0.35 + 0.65 * along);
        for (let e = 0; e < 2; e++) {
          tmp.copy(q.p).addScaledVector(side, e ? w : -w);
          P[vi * 3] = tmp.x; P[vi * 3 + 1] = tmp.y; P[vi * 3 + 2] = tmp.z; A[vi] = a; U[vi] = e;
          Cc[vi * 3] = c.r; Cc[vi * 3 + 1] = c.g; Cc[vi * 3 + 2] = c.b; vi++;
        }
        if (k) { const o = start + (k - 1) * 2; I[ii++] = o; I[ii++] = o + 1; I[ii++] = o + 2; I[ii++] = o + 1; I[ii++] = o + 3; I[ii++] = o + 2; }
      }
    }
    geo.setDrawRange(0, ii);
    for (const k of ['position', 'aA', 'aU', 'aC']) geo.attributes[k].needsUpdate = true; geo.index.needsUpdate = true;
    tr.mesh.visible = ii > 0;
  }
  R3.addHook((scene, cam) => {
    if (typeof G === 'undefined' || !G.fighters) return;
    const now = performance.now() / 1000, dt = Math.min(0.1, now - clock); clock = now;
    if (!group || group.parent !== scene) { if (group && group.parent) group.parent.remove(group); group = new T.Group(); scene.add(group); trails.clear(); }
    if (G.fxLow) { group.visible = false; return; } group.visible = true;
    const live = new Set();
    for (const f of G.fighters) {
      const rig = R3.rigOf(f); if (!rig || !rig.bones || !rig.bones.Head) continue;
      let tr = trails.get(f); if (!tr) { tr = make(f, rig); trails.set(f, tr); group.add(tr.mesh); }
      live.add(f);
      const striking = f.state === 'attack' && f.move && !f.move.cine && !G.paused;
      LIMBS.forEach((n, k) => {
        const L = tr.limbs[k], bone = rig.bones[n];
        while (L.pts.length && now - L.pts[0].t > LIFE) L.pts.shift();
        if (!bone || !striking || f.state === 'dead') { L.last = null; return; }
        bone.updateWorldMatrix(true, false); const p = bone.getWorldPosition(new T.Vector3());
        // only fast limbs draw: the strike, not the guard hand
        const fast = L.last && dt > 0 && p.distanceTo(L.last) / dt > tr.h * 2.2;
        if (fast) { if (!L.pts.length) L.pts.push({ p: L.last.clone(), t: now - dt }); L.pts.push({ p: p.clone(), t: now }); if (L.pts.length > MAXP) L.pts.shift(); }
        L.last = p;
      });
      build(tr, cam, now);
    }
    for (const [f, tr] of trails) if (!live.has(f)) { group.remove(tr.mesh); tr.mesh.geometry.dispose(); trails.delete(f); }
  });
})();
