'use strict';
// Norah's Family Finale: Pink Fluff, her spirit dragon (the 3D model made from the family's Pink Fluff artwork).
// The scanned model has no skeleton, so it is animated in the vertex shader, following real flight (reference footage):
// - wings: a slow, powerful downstroke and a quicker upstroke from the shoulder, with the outer wing lagging behind so
//   the tips trail and curl; held out in a glide, swept back in a dive, big forward back-beats in a flare;
// - tail: a travelling wave that snakes from the hips to the tip and trails the motion;
// - neck: follows with a lag, lunges forward on a bite; body: breathing, rises on each downstroke;
// - whole body: pitches along the flight path and banks hard into turns.
// items.js drives the flight (G.cine.dr = { x, y, z, vx, vy, vz, mode, chomp, bank, speed }); this file only draws.
(() => {
  if (typeof R3 === 'undefined' || !R3.addHook || !window.THREE) return;
  const T = window.THREE, HEIGHT = 430, TAU = Math.PI * 2;
  const U = { uA1: { value: 0 }, uA2: { value: 0 }, uSweep: { value: 0 }, uTailT: { value: 0 }, uTailA: { value: 0.1 }, uNeck: { value: 0 }, uLunge: { value: 0 }, uBreath: { value: 0 }, uJaw: { value: 0 } };
  let D = null;
  // per-vertex weights from the model's own shape: head at +z, tail at -z, wings out along x
  function bake(geo) {
    const p = geo.attributes.position, n = p.count, W = new Float32Array(n * 4);
    for (let i = 0; i < n; i++) {
      const x = p.getX(i), y = p.getY(i), z = p.getZ(i), ax = Math.abs(x);
      const ss = (a, b, v) => { const t = Math.max(0, Math.min(1, (v - a) / (b - a))); return t * t * (3 - 2 * t); };
      const inWingZ = ss(-0.32, -0.2, z) * (1 - ss(0.36, 0.5, z));
      const notLeg = 1 - (1 - ss(-0.36, -0.22, y)) * (1 - ss(0.3, 0.42, ax)); // the feet at the bottom near the body stay put
      W[i * 4] = ss(0.24, 0.42, ax) * inWingZ * notLeg;        // inner wing (moves with the shoulder)
      W[i * 4 + 1] = ss(0.5, 0.72, ax) * inWingZ * notLeg;     // outer wing (lags, trails)
      W[i * 4 + 2] = ss(-0.22, -0.76, z) * (1 - W[i * 4]);       // tail
      W[i * 4 + 3] = ss(0.32, 0.72, z) * (1 - ss(0.3, 0.5, ax)); // neck and head
    }
    geo.setAttribute('aW', new T.BufferAttribute(W, 4));
  }
  const VERT = `
    attribute vec4 aW;
    uniform float uA1, uA2, uSweep, uTailT, uTailA, uNeck, uLunge, uBreath, uJaw;
    vec2 rot(vec2 v, float a) { float c = cos(a), s = sin(a); return vec2(c * v.x - s * v.y, s * v.x + c * v.y); }`;
  const BODY = `
    #include <begin_vertex>
    {
      float side = sign(transformed.x);
      // wings: rotate (x,y) about the shoulder, then the outer part about the (moved) elbow
      vec2 sh = vec2(0.26 * side, 0.16);
      vec2 q = transformed.xy - sh; q = rot(q, side * uA1 * aW.x); vec2 p1 = sh + q;
      vec2 el = sh + rot(vec2(0.62 * side, 0.16) - sh, side * uA1);
      vec2 r2 = p1 - el; r2 = rot(r2, side * uA2 * aW.y); vec2 p2 = el + r2;
      transformed.xy = mix(transformed.xy, p2, step(0.001, aW.x));
      transformed.z -= uSweep * aW.x * (abs(transformed.x) - 0.26) * 0.9; // swept back in a dive or upstroke
      // tail: a wave travelling from hips to tip
      float u = aW.z, ph = uTailT - (-transformed.z) * 7.0;
      transformed.y += u * u * uTailA * sin(ph);
      transformed.x += u * u * uTailA * 0.8 * sin(ph * 0.8 + 1.3);
      // neck: lag and lunge
      float h = aW.w;
      transformed.x += h * h * uNeck * 0.12;
      transformed.y += h * h * (0.03 * sin(uTailT * 0.5) - uLunge * 0.05);
      transformed.z += h * h * uLunge * 0.22;
      // breathing in the chest
      float body = (1.0 - aW.x) * (1.0 - aW.z) * (1.0 - h);
      transformed.xy *= 1.0 + body * uBreath * 0.03;
    }`;
  function build(scene) {
    const g = new T.Group(), roll = new T.Group(), model = new T.Group(); g.add(roll); roll.add(model); scene.add(g);
    const load = window.PETRIG && window.PETRIG.load;
    if (load) load('fluff', sc => {
      const m = sc.clone(true); m.updateMatrixWorld(true);
      const parts = [];
      m.traverse(o => { if (o.isMesh) parts.push(o); });
      // normalise every mesh into one shared space (model units, centred, about 1.9 wide) so the weights line up
      const box = new T.Box3().setFromObject(m), size = box.getSize(new T.Vector3()), c = box.getCenter(new T.Vector3()), norm = 1.9 / Math.max(size.x, size.z);
      const grp = new T.Group();
      for (const o of parts) {
        const geo = o.geometry.clone(); geo.applyMatrix4(o.matrixWorld); geo.translate(-c.x, -c.y, -c.z); geo.scale(norm, norm, norm); bake(geo);
        const mat = o.material.clone(); mat.side = T.DoubleSide; mat.emissive = new T.Color('#ff4aa8'); mat.emissiveIntensity = 0.1;
        mat.onBeforeCompile = sh => { Object.assign(sh.uniforms, U); sh.vertexShader = sh.vertexShader.replace('#include <common>', '#include <common>\n' + VERT).replace('#include <begin_vertex>', BODY); };
        const mm = new T.Mesh(geo, mat); mm.frustumCulled = false; grp.add(mm);
      }
      const k = HEIGHT / (size.y * norm); grp.scale.setScalar(k);
      // put the snout (front, upper part) at the group origin, where items.js carries the caught fighters
      grp.position.set(0, -0.12 * k, -0.62 * k);
      model.add(grp);
    });
    const glow = new T.PointLight('#ff6ad5', 2.5, 1300, 1.4); g.add(glow); glow.position.set(0, 60, 0);
    return { g, roll, model, glow, phase: 0, a1: 0, a2: 0, sweep: 0, lunge: 0, pitch: 0 };
  }
  const tgt = new T.Vector3(), ease = (a, b, k) => a + (b - a) * k;
  function update(d) {
    if (!d.trail.length) { D.g.visible = false; return; }
    D.g.visible = true;
    // flight pose targets by mode
    const m = d.mode;
    const rate = m === 'climb' ? 0.105 : m === 'flare' ? 0.13 : m === 'enter' ? 0.07 : m === 'out' ? 0.09 : 0.05; // wingbeats per frame (fraction of a cycle)
    D.phase = (D.phase + rate) % 1;
    const ph = D.phase * TAU;
    // asymmetric stroke: the downstroke (ph 0..~0.55) is slower and stronger than the upstroke
    const stroke = Math.cos(ph + 0.45 * Math.sin(ph));
    let a1 = 0.18 + 0.75 * stroke, a2 = 0.15 + 0.55 * Math.cos(ph - 0.9), sweep = 0.12 + 0.2 * Math.max(0, Math.sin(ph));
    if (m === 'dive') { a1 = -0.05; a2 = -0.55; sweep = 0.95; }             // wings tucked back and folded
    else if (m === 'enter' && D.phase > 0.55 && d.t % 160 > 80) { a1 = 0.22; a2 = 0.08; sweep = 0.05; } // long glides between beats
    else if (m === 'flare') { a1 = 0.35 + 0.85 * stroke; a2 = 0.55 * Math.cos(ph - 0.7); sweep = -0.25; } // wings forward, braking
    D.a1 = ease(D.a1, a1, 0.25); D.a2 = ease(D.a2, a2, 0.22); D.sweep = ease(D.sweep, sweep, 0.15);
    U.uA1.value = D.a1; U.uA2.value = D.a2; U.uSweep.value = D.sweep;
    U.uTailT.value += 0.09 + d.speed * 0.004; U.uTailA.value = ease(U.uTailA.value, m === 'dive' ? 0.04 : 0.11, 0.05);
    U.uNeck.value = ease(U.uNeck.value, -d.bank * 0.6, 0.06);
    D.lunge = ease(D.lunge, d.chomp > 0 ? 1 : m === 'flare' ? 0.5 : 0, 0.3); U.uLunge.value = D.lunge;
    U.uBreath.value = Math.sin(d.t * 0.08);
    // body: face along the velocity (pitch follows the path), bank into turns, rise a little on each downstroke
    const lift = (m === 'dive' ? 0 : Math.max(0, -Math.sin(ph))) * 16;
    D.g.position.set(d.x, -d.y + lift, d.z);
    tgt.set(d.x + d.vx * 10, -d.y - d.vy * 10 + lift, d.z + d.vz * 10);
    if (m === 'flare') tgt.y += 260; // rears up to brake
    D.g.lookAt(tgt);
    D.roll.rotation.z = ease(D.roll.rotation.z, d.bank, 0.12);
  }
  R3.addHook(scene => {
    const c = G.cine, d = c && c.dr;
    if (!d) { if (D) D.g.visible = false; return; }
    if (!D || D.g.parent !== scene) { if (D && D.g.parent) D.g.parent.remove(D.g); D = build(scene); }
    update(d);
  });
  window.__dragonU = U;
})();
