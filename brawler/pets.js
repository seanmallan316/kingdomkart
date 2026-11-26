'use strict';
// Super Family Smash: the family pets as fighters (River the dog, Rosie the cat, Pink Fluff the dragon).
// Their 3D bodies come from the family's pictures (Meshy) as single meshes with no skeleton, so this file gives them
// one in the vertex shader: per-vertex weights are baked from each model's own shape (four legs with hip/shoulder
// and knee pivots, spine, neck and head, tail, and for the dragon two-part wings), and every frame a gait and pose
// system drives those joints like a real animal: walk (four-beat), trot (diagonal pairs), gallop / bound (spine
// flexing), crouch-and-spring jumps with tucked legs, pounces, rear-ups, paw swipes, buck kicks, tail wag or swish,
// head look-around and breathing. render3d.js calls PETRIG.build(def) and PETRIG.pose(rig, f, rot, a).
(() => {
  const T = window.THREE; if (!T) return;
  const TAU = Math.PI * 2, scenes = {}, waiting = {};
  function load(id, cb) {
    if (scenes[id]) return cb(scenes[id]);
    (waiting[id] = waiting[id] || []).push(cb); if (waiting[id].length > 1) return;
    const src = (window.CLASH_PETS || {})[id]; if (!src || !T.GLTFLoader) return;
    const s = atob(src), u8 = new Uint8Array(s.length); for (let i = 0; i < s.length; i++) u8[i] = s.charCodeAt(i);
    new T.GLTFLoader().parse(u8.buffer, '', gl => { scenes[id] = gl.scene; for (const f of waiting[id]) f(gl.scene); waiting[id] = []; }, e => console.warn('pet model failed', id, e));
  }
  for (const id of Object.keys(window.CLASH_PETS || {})) load(id, () => {});

  // body plans, in each model's normalised space (centred, longest side 1.9; head toward +z, back up +y)
  const PLAN = {
    dog: { front: [0.17, 0.66], back: [-0.8, -0.31], belly: -0.2, shZ: 0.42, hipZ: -0.57, pivY: 0.02, kneeY: -0.47, neckZ: 0.58, neckY: 0.22, tailZ: -0.74, tailY: 0.08, tailDir: 1 },
    cat: { front: [0.33, 0.63], back: [-0.63, -0.32], belly: -0.27, shZ: 0.48, hipZ: -0.47, pivY: -0.02, kneeY: -0.48, neckZ: 0.6, neckY: 0.12, tailZ: -0.56, tailY: 0.2, tailDir: 1 },
    // Pink Fluff (measured from the model): front feet z 0.36..0.62, hind feet z 0..0.26, belly at y -0.17, feet at -0.43;
    // the neck rises from the chest (z ~0.45) to the head (z 0.6..0.75); the tail lies on the floor and curls round to the left
    dragon: { front: [0.33, 0.66], back: [-0.04, 0.29], belly: -0.17, shZ: 0.47, hipZ: 0.13, pivY: -0.06, kneeY: -0.3, neckZ: 0.46, neckY: 0.04, tailZ: -0.04, tailY: -0.15, tailDir: -1, wings: true, drag: true },
  };
  const ss = (a, b, v) => { const t = Math.max(0, Math.min(1, (v - a) / (b - a))); return t * t * (3 - 2 * t); };
  function bake(geo, P) {
    const p = geo.attributes.position, n = p.count, L = new Float32Array(n * 4), B = new Float32Array(n * 4), K = new Float32Array(n * 2);
    for (let i = 0; i < n; i++) {
      const x = p.getX(i), y = p.getY(i), z = p.getZ(i), ax = Math.abs(x);
      let fl = 0, fr = 0, bl = 0, br = 0;
      const below = ss(P.belly + 0.08, P.belly - 0.06, y);
      if (P.front) { const w = below * ss(P.front[0] - 0.04, P.front[0] + 0.04, z) * (1 - ss(P.front[1] - 0.04, P.front[1] + 0.04, z)); if (x >= 0) fl = w; else fr = w; }
      if (P.back) { const w = below * ss(P.back[0] - 0.04, P.back[0] + 0.04, z) * (1 - ss(P.back[1] - 0.04, P.back[1] + 0.04, z)); if (x >= 0) bl = w; else br = w; }
      if (P.drag) { const near = 1 - ss(0.36, 0.46, ax); fl *= near; fr *= near; bl *= near; br *= near; } // the curled tail tip is not a foot
      L.set([fl, fr, bl, br], i * 4);
      const legs = fl + fr + bl + br;
      if (P.drag) { // dragon: tail measured along its curl, wings kept off the legs and the tail
        const along = Math.max(0, -z) + Math.max(0, ax - 0.2) * (z < 0.05 ? 1 : 0);
        // out past the body (where the wing fingers hang) the tail is only what lies on the floor, below the lowest wing tip
        const out = ss(0.38, 0.46, ax), tl = ss(0.02, 0.62, along) * ss(-0.13, -0.23, y) * (1 - out + out * ss(-0.29, -0.33, y)) * (z < 0.05 ? 1 : 0) * (1 - legs);
        const hd = ss(P.neckZ - 0.1, P.neckZ + 0.14, z) * (1 - legs) * ss(P.belly + 0.05, P.belly + 0.25, y) * (1 - ss(0.3, 0.42, ax));
        // the wing reaches down to its finger tips, but only outside the body's width (so the thighs don't follow the wings)
        const inZ = ss(-0.34, -0.2, z) * (1 - ss(0.36, 0.5, z)) * Math.max(ss(-0.24, -0.12, y), ss(0.4, 0.48, ax) * ss(-0.34, -0.29, y)) * (1 - legs) * (1 - tl);
        const w1 = ss(0.22, 0.4, ax) * inZ, w2 = ss(0.5, 0.74, ax) * inZ;
        B.set([tl, hd * (1 - w1), z, ss(P.kneeY + 0.06, P.kneeY - 0.06, y)], i * 4); K.set([w1, w2], i * 2);
        continue;
      }
      const tail = P.tailDir > 0 ? ss(P.tailZ + 0.06, P.tailZ - 0.14, z) * ss(P.tailY - 0.12, P.tailY + 0.05, y) : ss(P.tailZ, P.tailZ - 0.5, z) * (1 - ss(0.0, 0.3, y));
      const head = ss(P.neckZ - 0.12, P.neckZ + 0.08, z) * (1 - legs) * ss(P.belly, P.belly + 0.25, y);
      let wing = 0, wing2 = 0;
      if (P.wings) { const inZ = ss(-0.32, -0.2, z) * (1 - ss(0.36, 0.5, z)); wing = ss(0.24, 0.42, ax) * inZ; wing2 = ss(0.5, 0.72, ax) * inZ; }
      B.set([Math.max(0, tail * (1 - wing)), head * (1 - wing), z, ss(P.kneeY + 0.06, P.kneeY - 0.06, y)], i * 4);
      K.set([wing, wing2], i * 2);
    }
    geo.setAttribute('aLeg', new T.BufferAttribute(L, 4)); geo.setAttribute('aB', new T.BufferAttribute(B, 4)); geo.setAttribute('aWg', new T.BufferAttribute(K, 2));
  }
  const VERT = `
    attribute vec4 aLeg; attribute vec4 aB; attribute vec2 aWg;
    uniform vec4 uHip, uKnee; uniform vec4 uPiv; uniform vec4 uHead; uniform vec4 uTail; uniform vec4 uSpine; uniform vec4 uWing;
    vec2 rot2(vec2 v, float a) { float c = cos(a), s = sin(a); return vec2(c * v.x - s * v.y, s * v.x + c * v.y); }`;
  // uPiv: (shoulderZ, hipZ, pivotY, kneeY); uHead: (pitch, yaw, lunge, neckY/neckZ packed via uSpine.w); uTail: (time, wag, lift, swish)
  // uSpine: (arch, sway, breath, neckZ); uWing: (a1, a2, sweep, neckY)
  const BODY = `
    #include <begin_vertex>
    {
      float wl = aLeg.x + aLeg.y + aLeg.z + aLeg.w;
      float zc = aB.z;
      // spine: gallop flex (front and back halves pitch in opposite ways) and a little sideways sway
      float arch = uSpine.x, sway = uSpine.y;
      if (wl > 0.001) {
        float front = (aLeg.x + aLeg.y) / wl;
        float pz = mix(uPiv.y, uPiv.x, front);
        float hip = dot(aLeg, uHip) / wl, knee = dot(aLeg, uKnee) / wl;
        // hip / shoulder swing in the y-z plane
        vec2 q = vec2(transformed.z - pz, transformed.y - uPiv.z); q = rot2(q, hip * wl);
        vec2 kp = rot2(vec2(0.0, uPiv.w - uPiv.z), hip * wl);
        // knee / wrist bend for the lower part
        vec2 r = q - kp; r = rot2(r, knee * wl * aB.w * (front > 0.5 ? -1.0 : 1.0)); q = kp + r;
        transformed.z = pz + q.x; transformed.y = uPiv.z + q.y;
        zc = pz;
      }
      transformed.y += arch * (0.25 - zc * zc) * 0.35;
      transformed.x += sway * zc * zc * 0.25;
      // head and neck
      float h = aB.y;
      if (h > 0.001) {
        vec2 q = vec2(transformed.z - uSpine.w, transformed.y - uWing.w); q = rot2(q, uHead.x * h);
        transformed.z = uSpine.w + q.x + uHead.z * h; transformed.y = uWing.w + q.y;
        transformed.x += uHead.y * h * max(0.0, transformed.z - uSpine.w + 0.1);
      }
      // tail: wave travelling to the tip, wag side to side, lift
      float t = aB.x;
      if (t > 0.001) {
        float d = abs(transformed.z - zc) + abs(transformed.y) * 0.3;
        transformed.x += t * t * (uTail.y * sin(uTail.x - d * 6.0) + uTail.w);
        transformed.y += t * t * (uTail.z + 0.03 * sin(uTail.x * 0.7 - d * 4.0));
      }
      // wings (dragon): shoulder then elbow, outer wing lags
      if (aWg.x > 0.001) {
        float side = sign(transformed.x);
        vec2 sh = vec2(0.26 * side, 0.16);
        vec2 q = transformed.xy - sh; q = rot2(q, side * uWing.x * aWg.x); vec2 p1 = sh + q;
        vec2 el = sh + rot2(vec2(0.62 * side, 0.16) - sh, side * uWing.x);
        vec2 r = p1 - el; r = rot2(r, side * uWing.y * aWg.y); transformed.xy = el + r;
        transformed.z -= uWing.z * aWg.x * (abs(transformed.x) - 0.26) * 0.9;
      }
      // breathing
      float body = (1.0 - wl) * (1.0 - h) * (1.0 - t);
      transformed.y *= 1.0 + body * uSpine.z * 0.025; transformed.x *= 1.0 + body * uSpine.z * 0.03;
    }`;

  function build(def) {
    const P = PLAN[def.pet] || PLAN.dog;
    const rig = { def, pet: def.pet, plan: P, turnY: Math.PI / 2 - 0.42, blades: [], look: { mats: [] }, ph: 0, gait: 0, tailT: 0, mem: {} };
    const U = rig.U = { uHip: { value: new T.Vector4() }, uKnee: { value: new T.Vector4() }, uPiv: { value: new T.Vector4(P.shZ, P.hipZ, P.pivY, P.kneeY) },
      uHead: { value: new T.Vector4() }, uTail: { value: new T.Vector4() }, uSpine: { value: new T.Vector4(0, 0, 0, P.neckZ) }, uWing: { value: new T.Vector4(0.2, 0.1, 0, P.neckY) } };
    const root = new T.Group(), spin = new T.Group(), turn = new T.Group(), body = new T.Group(), inner = new T.Group();
    root.add(spin); spin.add(turn); turn.add(body); body.add(inner);
    rig.root = root; rig.spin = spin; rig.turn = turn; rig.body = body; rig.inner = inner;
    const H = 80 * def.scale * (def.hh || 1); rig.H = H; rig.hipRest = H * 0.5;
    load(def.id, scene => {
      const m = scene.clone(true); m.updateMatrixWorld(true);
      const parts = []; m.traverse(o => { if (o.isMesh) parts.push(o); });
      const box = new T.Box3().setFromObject(m), size = box.getSize(new T.Vector3()), c = box.getCenter(new T.Vector3()), norm = 1.9 / Math.max(size.x, size.y, size.z);
      const grp = new T.Group(), skinned = P.drag && window.DRAGONRIG, geos = [], meshes = [];
      for (const o of parts) {
        const geo = o.geometry.clone(); geo.applyMatrix4(o.matrixWorld); geo.translate(-c.x, -c.y, -c.z); geo.scale(norm, norm, norm); bake(geo, P); geos.push(geo);
        const mat = o.material.clone(); mat.metalness = 0; mat.roughness = 0.85; mat.envMapIntensity = 0.45; if (P.wings) mat.side = T.DoubleSide;
        if (!skinned) mat.onBeforeCompile = sh => { Object.assign(sh.uniforms, U); sh.vertexShader = sh.vertexShader.replace('#include <common>', '#include <common>\n' + VERT).replace('#include <begin_vertex>', BODY); };
        meshes.push([geo, mat]); rig.look.mats.push(mat);
      }
      // Pink Fluff gets a real skeleton (dragonrig.js): ~40 bones fitted to the model, skinned, moving in 3D
      const drig = skinned ? window.DRAGONRIG.make(geos, grp) : null;
      for (const [geo, mat] of meshes) { const mm = drig ? new T.SkinnedMesh(geo, mat) : new T.Mesh(geo, mat); mm.castShadow = true; mm.receiveShadow = true; mm.frustumCulled = false; grp.add(mm); }
      const k = H / (size.y * norm); grp.scale.setScalar(k); grp.position.y = (size.y * norm / 2) * k; // feet on the ground
      rig.len = Math.max(size.x, size.z) * norm * k;
      inner.add(grp); inner.position.y = -H * 0.5; body.position.y = H * 0.5;
      if (drig) { root.updateMatrixWorld(true); const sk = new T.Skeleton(drig.bones); grp.traverse(o => { if (o.isSkinnedMesh) o.bind(sk); }); rig.drig = drig; }
    });
    return rig;
  }
  const clamp = (v, a, b) => (v < a ? a : v > b ? b : v), ease = (a, b, k) => a + (b - a) * k;
  const moveName = f => { const mv = f.move; if (!mv) return ''; if (!f._mvn || f._mvn.mv !== mv) f._mvn = { mv, n: Object.keys(f.moves).find(k => f.moves[k] === mv) || '' }; return f._mvn.n; };

  // ---- Pink Fluff, from the family's three dragon videos: a slow snake-like tail curl, half-raised wings at rest,
  // head-low stalking, powerful asymmetric wingbeats (slow strong downstroke, quick upstroke, outer wing lagging),
  // coiled bites, breath with the neck stretched and wings raised, rear-up roars with wings fully spread, a take-off
  // and head-first dive slam with wings flat, and a full on-the-spot tail-whip spin.
  function flap(J, t, rate, amp, base) {
    const ph = t * rate, st = Math.cos(ph + 0.45 * Math.sin(ph));
    J.a1 = base + amp * st; J.a2 = base * 0.5 + amp * 0.7 * Math.cos(ph - 0.9); J.sweep = 0.1 + 0.2 * Math.max(0, Math.sin(ph));
    return Math.max(0, -Math.sin(ph)); // lift on the downstroke
  }
  function dragonPose(J, S, f, t, H, ph) {
    J.wag = 0.1; J.tspd = 0.05; J.tlift = 0.04;
    switch (S) {
      case 'idle': case 'respawn': case 'shieldDrop': case 'getup':
        if (f && G.ending && f.stocks > 0) { // victory: rears up and roars with the wings spread wide
          J.pitch = -0.55; J.a1 = 1.05 + 0.08 * Math.sin(t * 0.1); J.a2 = 0.55; J.sweep = -0.2; J.hp = -0.6; J.hl = 0.05; J.tlift = 0.12; J.bob = 0; J.hip = [1.0, 0.9, -0.3, -0.3]; J.knee = [0.5, 0.5, 0.4, 0.4]; break;
        }
        J.a1 = 0.35 + 0.06 * Math.sin(t * 0.03); J.a2 = -0.35; J.sweep = 0.35;
        J.hp = -0.05 + 0.08 * Math.sin(t * 0.02); J.hy = 0.25 * Math.sin(t * 0.009); J.bob = Math.sin(t * 0.05) * H * 0.008;
        J.hip = [0.05 * Math.sin(t * 0.013), -0.05 * Math.sin(t * 0.013), 0, 0]; break;
      case 'walk': J.hp = 0.18; J.hy = 0.15 * Math.sin(ph); J.a1 = 0.25; J.a2 = -0.3; J.sweep = 0.45; J.wag = 0.14; J.tspd = 0.12; break; // stalking, head low
      case 'run': J.a1 = 0.35 + 0.25 * Math.sin(t * 0.25); J.a2 = 0; J.sweep = 0.5; J.hp = 0.12; J.wag = 0.08; J.tspd = 0.2; break;
      case 'jumpsquat': J.a1 = 0.95; J.a2 = 0.35; J.sweep = 0; J.hp = 0.2; break; // wings up before the leap
      case 'air': case 'helpless': {
        const rising = f && f.vy < 0;
        if (rising || (t % 90) < 40) flap(J, t, rising ? 0.34 : 0.26, rising ? 0.9 : 0.7, 0.25); else { J.a1 = 0.5 + 0.08 * Math.sin(t * 0.05); J.a2 = 0.28; J.sweep = 0.12; } // beat, then glide
        J.hip = [0.5, 0.5, -0.6, -0.6]; J.knee = [1.0, 1.0, 1.0, 1.0]; J.tlift = -0.05; J.wag = 0.06; J.tspd = 0.1; break;
      }
      case 'shield': case 'shieldstun': J.a1 = -0.5; J.a2 = -0.6; J.sweep = -0.6; J.hp = 0.35; break; // wings wrapped forward
      case 'hitstun': J.a1 = 0.9; J.a2 = 0.5; J.sweep = 0.4; break;
    }
  }
  function dragonAttack(J, f, mv, nm, on, wind, u, H, t) {
    J.hip = [0, 0, 0, 0]; J.knee = [0, 0, 0, 0]; J.wag = 0.1; J.tspd = 0.08; J.tlift = 0.06; J.tsw = 0;
    const len = Math.max(1, mv.len || 30), prog = Math.min(1, u / len);
    if (/^(nair|bair|dtilt)/.test(nm) || mv.rollSpin) { // tail-whip: a full turn on the spot, wings tucked, tail out straight
      J.yaw = prog * Math.PI * 2; J.a1 = 0.15; J.a2 = -0.4; J.sweep = 0.6; J.wag = 0.02; J.tlift = 0.08; J.hp = 0.1; J.sy = 0.92;
      J.hip = [0.3, 0.3, -0.3, -0.3]; J.knee = [0.4, 0.4, 0.4, 0.4];
    } else if (/^(utilt|usmash|uair|uspec)/.test(nm)) { // rears up and roars, wings fully spread, front claws high
      J.pitch = -0.95 * on - 0.25 * wind; J.a1 = 1.15 * on + 0.4 * wind; J.a2 = 0.55 * on; J.sweep = -0.25; J.hp = -0.7 * on + 0.2 * wind;
      J.hip = [1.3 * on, 1.1 * on, -0.4 * on, -0.4 * on]; J.knee = [0.4, 0.4, 0.5 * on, 0.5 * on]; J.tlift = 0.12;
      if (/^uspec/.test(nm) || (f && !f.grounded)) { flap(J, t, 0.45, 1.0, 0.3); J.pitch = -0.4 * on; } // take-off: hard wingbeats
    } else if (/^(dsmash|dspec|dair)/.test(nm)) { // rises, then dives head-first and slams down, wings flat on the ground
      J.pitch = 0.45 * on - 0.5 * wind; J.a1 = 1.1 * wind - 0.25 * on; J.a2 = 0.3 * on; J.sweep = -0.1 * on;
      J.hip = [-0.7 * on + 0.8 * wind, -0.7 * on + 0.8 * wind, 0.3 * on, 0.3 * on]; J.knee = [0.2, 0.2, 0.8 * on, 0.8 * on];
      J.sy = 1 - 0.18 * on; J.hp = 0.45 * on - 0.3 * wind; J.tlift = 0.2 * on;
    } else if (/^(nspec|shoot|pulseB)/.test(nm)) { // breath: head rears back with wings up, then the neck stretches forward
      J.hp = -0.4 * wind + 0.2 * on; J.hl = -0.05 * wind + 0.2 * on; J.arch = -0.2 * wind; J.lunge = -H * 0.12 * on;
      J.a1 = 0.95 * wind + 0.85 * on; J.a2 = 0.45 * wind + 0.35 * on; J.sweep = 0.3 * on; J.tlift = 0.15;
      J.hip = [0.2 * on, 0.2 * on, -0.3 * on, -0.3 * on]; J.knee = [0.3, 0.3, 0.6, 0.6];
    } else if (/^sspec/.test(nm)) { // wing rush: wings swept back like a dart, head forward
      J.lunge = H * 0.6 * on; J.a1 = 0.1; J.a2 = -0.5; J.sweep = 0.9 * on; J.hp = 0.15; J.hl = 0.12 * on; J.pitch = 0.1 * on;
      J.hip = [0.8 * on, 0.8 * on, -0.8 * on, -0.8 * on]; J.knee = [0.6, 0.6, 0.6, 0.6];
    } else if (/^fair/.test(nm)) { // aerial claw swipe with a wing buffet
      flap(J, t, 0.5, 0.9, 0.3); J.hip = [1.2 * on, 0.9 * on, -0.5, -0.5]; J.knee = [0.3, 0.3, 0.9, 0.9]; J.pitch = -0.3 * on; J.hp = 0.1; J.hl = 0.1 * on;
    } else { // bite: coils with the head drawn back and wings rising, then lunges, neck stretched, wings flaring back
      J.sy = 1 - 0.1 * wind; J.lunge = H * (mv.smash ? 0.8 : 0.45) * on; J.hp = -0.35 * wind + 0.3 * on; J.hl = -0.06 * wind + 0.16 * on;
      J.a1 = 0.7 * wind + 0.75 * on; J.a2 = 0.4 * on; J.sweep = 0.55 * on; J.tlift = 0.1;
      J.hip = [0.6 * on - 0.3 * wind, 0.5 * on - 0.3 * wind, -0.5 * on + 0.3 * wind, -0.4 * on + 0.3 * wind]; J.knee = [0.3 + 0.6 * wind, 0.3 + 0.6 * wind, 0.7 * wind, 0.7 * wind];
    }
  }
  // Rosie's own moves, from her two videos
  function catAttack(J, f, mv, nm, on, wind, u, H) {
    J.hip = [0, 0, 0, 0]; J.knee = [0, 0, 0, 0]; J.tsw = 0; J.wag = 0.02; J.tlift = 0.15;
    const prog = Math.min(1, u / Math.max(1, mv.len || 30));
    if (/^(utilt|usmash)/.test(nm)) { // stands up on the hind legs and bats with both paws, boxing
      const b = Math.sin(u * 0.6); J.pitch = -1.1 * (on + wind * 0.6); J.hip = [1.2 + 0.5 * b * on, 1.2 - 0.5 * b * on, -0.45, -0.45]; J.knee = [0.3, 0.3, 0.6, 0.6]; J.hp = -0.4 * on;
    } else if (/^uspec/.test(nm)) { // upright tornado spin
      J.pitch = -0.9; J.yaw = prog * Math.PI * 4; J.hip = [1.0, 1.0, -0.3, -0.3]; J.knee = [0.4, 0.4, 0.5, 0.5]; J.tlift = 0.2;
    } else if (/^(fsmash|sspec|dash)/.test(nm)) { // claw dash: a long low lunge, one paw stretched far forward, tail straight out
      J.lunge = H * 0.8 * on; J.sy = 1 - 0.12 * (on + wind); J.pitch = 0.08 * on + 0.12 * wind;
      J.hip = [1.45 * on - 0.3 * wind, 0.4 * on, -0.9 * on + 0.4 * wind, -0.8 * on + 0.4 * wind]; J.knee = [0.1, 0.5, 0.9 * wind, 0.9 * wind]; J.hp = 0.05; J.hl = 0.05 * on; J.tlift = 0.05;
    } else if (/^(nspec|shoot|pulse)/.test(nm)) { // claw flurry: rapid alternating swipes, reared back
      const sw = Math.floor(u / 4) % 2; J.pitch = -0.35; J.hip[sw] = 1.7 - 0.9 * ((u % 4) / 4); J.hip[1 - sw] = 0.6; J.hip[2] = J.hip[3] = -0.2; J.knee = [0.2, 0.2, 0.5, 0.5]; J.hp = 0.1; J.tsw = 0.15 * Math.sin(u * 0.5);
    } else { // jab / ftilt / fair: a big overhead swipe, the paw raised high then raked down, alternating paws
      const sw = Math.floor(u / 16) % 2; J.pitch = -0.35 * (wind + on * 0.7); J.hip[sw] = 1.8 * wind + 0.7 * on; J.hip[1 - sw] = 0.25;
      J.hip[2] = J.hip[3] = -0.25; J.knee[sw] = 0.15; J.knee[2] = J.knee[3] = 0.4; J.lunge = H * 0.22 * on; J.hp = 0.15 * on - 0.1 * wind; J.tsw = 0.2;
    }
  }
  // River's own moves, from his two videos
  function dogAttack(J, f, mv, nm, on, wind, u, H) {
    J.hip = [0, 0, 0, 0]; J.knee = [0, 0, 0, 0];
    const prog = Math.min(1, u / Math.max(1, mv.len || 30));
    if (/^(nspec|shoot|pulseB)/.test(nm)) { // bark blast: legs braced wide, head drops, then thrusts forward; the body recoils
      J.hip = [-0.35, -0.35, 0.35, 0.35]; J.knee = [0.3, 0.3, 0.5, 0.5]; J.sy = 1 - 0.08 * wind;
      J.hp = 0.35 * wind - 0.15 * on; J.hl = -0.04 * wind + 0.12 * on; J.lunge = -H * 0.15 * on; J.pitch = 0.08 * wind - 0.1 * on; J.wag = 0.02; J.tlift = 0.18;
    } else if (/^nair/.test(nm)) { // mid-air twist: a full horizontal spin, legs tucked
      J.yaw = prog * Math.PI * 2; J.hip = [0.6, 0.6, -0.6, -0.6]; J.knee = [1.1, 1.1, 1.1, 1.1]; J.tlift = 0.1;
    } else if (/^dtilt/.test(nm)) { // play bow, then a low swipe: front end down, rear up, tail wagging hard
      J.pitch = 0.35 * (on + wind); J.hip = [-0.5 * on + 0.9 * wind, -0.3, 0.2, 0.2]; J.knee = [0.6, 0.9, 0.2, 0.2]; J.lunge = H * 0.3 * on; J.hp = 0.2; J.wag = 0.18; J.tspd = 0.7; J.tlift = 0.16;
    } else { // usmash / uspec: rears up on the hind legs and howls, front paws up
      J.pitch = -1.0 * on - 0.3 * wind; J.hip = [1.2 * on, 1.0 * on, -0.4 * on, -0.4 * on]; J.knee = [0.6, 0.6, 0.5 * on, 0.5 * on]; J.hp = -0.8 * on; J.tlift = 0.1;
    }
  }
  // targets for the joints this frame
  function pose(rig, f, rot, a, tMenu, once) {
    const H = rig.H, P = rig.plan, dog = rig.pet === 'dog', cat = rig.pet === 'cat', drg = rig.pet === 'dragon';
    const t = f ? f.anim : (tMenu || 0), S = f ? f.state : 'idle';
    // everything below runs in real time, whatever the frame rate (fr = how many 60 Hz frames this draw covers)
    const nowT = performance.now(), fr = Math.max(0, Math.min(6, (nowT - (rig.tLast || nowT)) / 16.667)) * (f && G.paused ? 0 : 1); rig.tLast = nowT;
    const J = { hip: [0, 0, 0, 0], knee: [0, 0, 0, 0], arch: 0, sway: 0, hp: 0, hy: 0, hl: 0, wag: dog ? 0.06 : 0.035, tlift: 0, tsw: 0, tspd: dog ? 0.35 : 0.06, bob: 0, pitch: 0, flip: 0, yaw: 0, lunge: 0, side: 0, sy: 1, a1: 0.2, a2: 0.1, sweep: 0 };
    if (!f && once) rig.happy = 80;
    const speed = f ? Math.abs(f.vx) / Math.max(1, f.def.run) : 0;
    // gait cycle advances with ground speed so the paws plant instead of sliding
    const gaitHz = S === 'run' ? 0.03 + speed * 0.05 : S === 'walk' ? 0.025 : 0;
    rig.ph = (rig.ph + gaitHz * fr * (G.speed || 0.8) / 0.8) % 1;
    const ph = rig.ph * TAU, sn = o => Math.sin(ph + o * TAU), lift = o => Math.max(0, Math.sin(ph + o * TAU + Math.PI / 2));
    function gait(kind, A, K) {
      // order: FL, FR, BL, BR. walk: four-beat (BL, FL, BR, FR). trot: diagonal pairs. gallop/bound: front pair then back pair.
      const off = kind === 'walk' ? [0.25, 0.75, 0, 0.5] : kind === 'trot' ? [0, 0.5, 0.5, 0] : [0, 0.12, 0.5, 0.6];
      for (let i = 0; i < 4; i++) { J.hip[i] = A * sn(off[i]); J.knee[i] = K * lift(off[i]); }
      if (kind === 'gallop') { J.arch = -0.5 * Math.sin(ph); J.bob = Math.abs(Math.sin(ph)) * H * 0.12; J.pitch = 0.08 * Math.sin(ph + 1); }
      else J.bob = Math.abs(Math.sin(ph * 2)) * H * 0.025;
      J.hp = 0.08 * Math.sin(ph * 2); J.sway = 0.08 * Math.sin(ph);
    }
    if (rig.happy > 0) { // menu celebration: hop, spin the tail
      rig.happy--; const u = rig.happy / 80; J.bob = Math.abs(Math.sin(u * Math.PI * 4)) * H * 0.3; J.hip = [0.5, 0.5, -0.4, -0.4]; J.knee = [0.6, 0.6, 0.3, 0.3]; J.wag = 0.14; J.tspd = 0.6; J.hp = -0.3; J.a1 = 0.6 * Math.sin(u * 30);
    } else switch (S) {
      case 'idle': case 'respawn': case 'shieldDrop': case 'getup': {
        J.hp = 0.12 * Math.sin(t * 0.017) - 0.05; J.hy = 0.3 * Math.sin(t * 0.011); // looks around
        if (cat) { J.tsw = 0.25 * Math.sin(t * 0.02); J.tlift = 0.1; } else J.tlift = 0.06;
        if (f && G.ending && f.stocks > 0) { J.bob = Math.abs(Math.sin(t * 0.15)) * H * 0.25; J.wag = 0.14; J.tspd = 0.7; J.hp = -0.3; }
        break;
      }
      case 'walk': gait('walk', 0.32, 0.5); J.hy = 0; if (drg) { J.a1 = 0.05; J.a2 = -0.3; J.sweep = 0.5; } break;
      case 'run': if (speed > 0.75 || cat) gait('gallop', 0.75, 0.9); else gait('trot', 0.5, 0.7); J.wag = dog ? 0.03 : 0.02; J.tlift = 0.12; J.hp = -0.05; if (drg) { J.a1 = 0.2 + 0.4 * Math.sin(t * 0.3); J.a2 = 0.1; J.sweep = 0.4; } break;
      case 'jumpsquat': case 'land': case 'crouch': J.sy = 0.82; J.knee = [0.9, 0.9, 0.9, 0.9]; J.hip = [-0.3, -0.3, 0.3, 0.3]; J.hp = 0.15; break;
      case 'air': case 'helpless': {
        if (f.flipT > 0) { J.flip = (1 - f.flipT / 24) * TAU; J.hip = [0.6, 0.6, -0.6, -0.6]; J.knee = [1.2, 1.2, 1.2, 1.2]; break; }
        const up = clamp(-f.vy / 10, -1, 1);
        // rising: front legs reach forward, back legs stretch back; falling: all four reach down for the landing
        J.hip = up > 0 ? [0.8 * up, 0.8 * up, -0.8 * up, -0.8 * up] : [0.3, 0.3, 0.1, 0.1]; J.knee = up > 0 ? [0.3, 0.3, 0.1, 0.1] : [0.2, 0.2, 0.3, 0.3];
        J.pitch = clamp(-f.vy * 0.03, -0.35, 0.4); J.tlift = 0.15; J.hp = -0.1 * up;
        if (drg) { const w = Math.sin(t * 0.32); J.a1 = 0.2 + 0.8 * w; J.a2 = 0.15 + 0.6 * Math.sin(t * 0.32 - 0.9); J.sweep = 0.1; }
        break;
      }
      case 'shield': case 'shieldstun': J.sy = 0.85; J.knee = [1, 1, 1, 1]; J.hip = [-0.2, -0.2, 0.2, 0.2]; J.hp = 0.3; J.tlift = -0.08; if (cat) J.arch = 0.6; if (drg) { J.a1 = -0.6; J.sweep = -0.3; } break; // cat arches her back
      case 'roll': case 'tech': case 'airdodge': case 'spotdodge': J.flip = (f.t / 26) * TAU; J.knee = [1.2, 1.2, 1.2, 1.2]; J.hip = [0.6, 0.6, -0.6, -0.6]; break;
      case 'hitstun': if (f.tumble) { J.flip = t * 0.35; J.hip = [0.9, -0.4, -0.9, 0.4]; J.knee = [0.6, 0.2, 0.6, 0.2]; } else { J.pitch = 0.3; J.hip = [0.5, 0.5, -0.5, -0.5]; J.hp = -0.3; J.wag = 0; } break;
      case 'down': J.side = 1.2; J.sy = 0.85; J.hip = [0.4, 0.2, -0.4, -0.2]; break;
      case 'ledge': J.pitch = -0.8; J.hip = [1.3, 1.3, -0.2, -0.2]; J.knee = [0.6, 0.6, 0.5, 0.5]; break;
      case 'grabbed': J.pitch = 0.3; J.hip = [0.6, 0.6, -0.5, -0.5]; break;
      case 'holding': J.lunge = H * 0.15; J.hp = 0.25; J.hip = [0.4, 0.4, -0.2, -0.2]; break;
      case 'attack': {
        const mv = f.move || {}, hbs = (mv.hb || []).concat(f.xhb || []), hs = hbs.length ? Math.min.apply(null, hbs.map(h => h.f[0])) : Math.max(4, mv.len * 0.3);
        const he = hbs.length ? Math.max.apply(null, hbs.map(h => h.f[1])) : hs + 6, u = f.mt;
        const s = u < hs ? -(u / hs) : u <= he ? 1 : Math.max(0, 1 - (u - he) / 12), on = Math.max(0, s), wind = Math.max(0, -s);
        const nm = moveName(f);
        if (drg && !mv.cine) { dragonAttack(J, f, mv, nm, on, wind, u, H, t); break; }
        if (dog && !mv.cine && /^(nspec|shoot|pulseB|nair|dtilt|usmash|uspec)/.test(nm)) { dogAttack(J, f, mv, nm, on, wind, u, H); break; }
        if (cat && !mv.cine && /^(jab|ftilt|utilt|usmash|uspec|fsmash|sspec|dash|nspec|shoot|pulse|fair)/.test(nm)) { catAttack(J, f, mv, nm, on, wind, u, H); break; }
        if (mv.cine === 'clones') { J.pitch = -0.95; J.hip = [1.35, 1.35, -0.4, -0.4]; J.knee = [0.2, 0.2, 0.5, 0.5]; J.hp = -0.5; J.tlift = 0.2; J.tsw = 0.1 * Math.sin(u * 0.2); break; } // Rosie stands tall, paws out, as the clones burst
        if (mv.cine) { if (mv.cine === 'storm') J.flip = u * 0.4; else { gait('gallop', 0.8, 1.0); J.hp = -0.2; } if (drg) { J.a1 = 0.3 + 0.7 * Math.sin(u * 0.3); J.a2 = 0.3 * Math.sin(u * 0.3 - 0.8); } break; }
        if (/^(nair|nspec|uspec|whirl)/.test(nm) || mv.rollSpin) { J.flip = (u / Math.max(1, mv.len)) * TAU * 1.5; J.knee = [1.2, 1.2, 1.2, 1.2]; J.hip = [0.7, 0.7, -0.7, -0.7]; if (drg) J.a1 = -0.4; }
        else if (/^(uair|utilt|usmash)/.test(nm)) { // rears up on the back legs, front paws (or claws) swipe upward
          J.pitch = -1.0 * on - 0.3 * wind; J.hip = [1.4 * on - 0.3 * wind, 1.2 * on, -0.5 * on, -0.5 * on]; J.knee = [0.4, 0.4, 0.6 * on, 0.6 * on]; J.hp = -0.6 * on; J.hl = 0.04 * on; if (drg) { J.a1 = 1.0 * on; J.sweep = -0.2; }
        }
        else if (/^(dtilt|dsmash|dspec|quake|dair)/.test(nm)) { // stomp / ground slam with both front paws
          J.pitch = 0.4 * on - 0.5 * wind; J.hip = [-0.6 * on + 1.0 * wind, -0.6 * on + 1.0 * wind, 0.2, 0.2]; J.knee = [0.2, 0.2, 0.8 * on, 0.8 * on]; J.sy = 1 - 0.15 * on; J.hp = 0.5 * on; if (drg) { J.a1 = 0.9 * wind - 0.3 * on; }
        }
        else if (/^(bair)/.test(nm)) { J.hip = [0.3, 0.3, -1.4 * on, -1.2 * on]; J.knee = [0.3, 0.3, 0.1, 0.1]; J.pitch = 0.4 * on; J.lunge = -H * 0.3 * on; J.hp = 0.5 * on; }
        else if (cat && /^(jab|ftilt|fair)/.test(nm)) { // cat paw swipe with one front leg
          const sw = u % 16 < 8 ? 0 : 1; J.hip[sw] = 1.3 * on - 0.4 * wind; J.knee[sw] = 0.2; J.hip[1 - sw] = 0.2; J.pitch = -0.25 * on; J.hp = 0.1; J.lunge = H * 0.2 * on; J.tsw = 0.2;
        }
        else { // pounce or lunging bite: crouch, then spring forward with front legs reaching and back legs driving
          J.lunge = H * (mv.smash ? 0.9 : 0.55) * on; J.pitch = -0.12 * on + 0.15 * wind; J.sy = 1 - 0.15 * wind;
          J.hip = [0.9 * on - 0.3 * wind, 0.8 * on - 0.3 * wind, -0.9 * on + 0.3 * wind, -0.8 * on + 0.3 * wind]; J.knee = [0.3 + 0.8 * wind, 0.3 + 0.8 * wind, 0.9 * wind, 0.9 * wind];
          J.hp = -0.25 * on + 0.2 * wind; J.hl = 0.06 * on; J.tlift = 0.12 * on;
          if (drg) { J.a1 = 0.8 * on - 0.2 * wind; J.a2 = 0.4 * on; J.sweep = -0.2 * on; }
        }
        break;
      }
    }
    if (drg && S !== 'attack' && !(rig.happy > 0)) dragonPose(J, S, f, t, H, ph);
    if (cat && !(rig.happy > 0)) { // Rosie, from her videos: tail up with a hooked tip on the move, slow swish and flick at rest, sit to win
      if (S === 'walk' || S === 'run') { J.tlift = 0.2; J.tsw = 0.06 * Math.sin(ph); J.wag = 0.02; }
      if (S === 'idle' && f && !G.ending && (t % 200) < 14) J.tsw += 0.25 * Math.sin((t % 200) * 0.45); // a tail flick now and then
      if ((S === 'idle' || S === 'respawn') && f && G.ending && f.stocks > 0) {
        J.pitch = -0.42; J.hip = [0.1, 0.1, 0.95, 0.95]; J.knee = [0, 0, 1.35, 1.35]; J.hp = 0.05; J.hy = 0.25 * Math.sin(t * 0.02); J.bob = 0;
        J.tlift = -0.08; J.tsw = 0.35; J.wag = 0.015; J.tspd = 0.03; // sits, tail curled round the paws
      }
    }
    if (dog && !(rig.happy > 0)) { // River, from his videos: tail held up in a gentle wag, head bobbing with each step, sit-and-howl to win
      if (S === 'walk' || S === 'run') { J.hp += 0.06 * Math.sin(ph * 2); J.tlift = 0.14; }
      if ((S === 'idle' || S === 'respawn') && f && G.ending && f.stocks > 0) {
        J.pitch = -0.38; J.hip = [0.15, 0.15, 0.9, 0.9]; J.knee = [0, 0, 1.3, 1.3]; J.hp = -0.85; J.hy = 0; J.bob = 0; J.wag = 0.1; J.tspd = 0.45; J.tlift = -0.05;
      }
    }
    // ease every joint toward its target (fast for attacks so hits still read, softer otherwise)
    const M = rig.mem, k = 1 - Math.pow(1 - (S === 'attack' ? 0.4 : 0.2), Math.max(0.2, fr));
    const e = (key, v) => (M[key] = M[key] == null ? v : ease(M[key], v, k));
    const U = rig.U;
    U.uHip.value.set(e('h0', J.hip[0]), e('h1', J.hip[1]), e('h2', J.hip[2]), e('h3', J.hip[3]));
    U.uKnee.value.set(e('k0', J.knee[0]), e('k1', J.knee[1]), e('k2', J.knee[2]), e('k3', J.knee[3]));
    rig.tailT += J.tspd * fr;
    U.uTail.value.set(rig.tailT, e('wag', J.wag), e('tl', J.tlift), e('tsw', J.tsw));
    U.uHead.value.set(e('hp', J.hp), e('hy', J.hy), e('hl', J.hl), 0);
    U.uSpine.value.set(e('arch', J.arch), e('sway', J.sway), Math.sin(t * 0.07), P.neckZ);
    U.uWing.value.set(e('a1', J.a1), e('a2', J.a2), e('swp', J.sweep), P.neckY);
    if (rig.drig) DRAGONRIG.drive(rig.drig, { hip: [M.h0, M.h1, M.h2, M.h3], knee: [M.k0, M.k1, M.k2, M.k3], wag: M.wag, tl: M.tl, tsw: M.tsw, tailT: rig.tailT,
      hp: M.hp, hy: M.hy, hl: M.hl, arch: M.arch, sway: M.sway, a1: M.a1, a2: M.a2, swp: M.swp }, t, S, rig);
    // whole body: bob, lunge, pitch (rears up about the hind feet), flips spin about the middle
    const pitch = e('pitch', J.pitch), lunge = e('lunge', J.lunge), bob = e('bob', J.bob), sy = e('sy', J.sy), side = e('side', J.side);
    const rearUp = Math.min(0, pitch) * -1, back = (rig.len || H * 2) * 0.42;
    rig.body.position.set(0, H * 0.5 + bob + Math.sin(rearUp) * back * 0.9, lunge - (1 - Math.cos(rearUp)) * back);
    rig.body.rotation.set(pitch + J.flip, J.yaw, side); // yaw: the dragon's on-the-spot tail-whip spin
    rig.body.scale.set(1 + (1 - sy) * 0.4, sy, 1);
    if (f && a) { rig.root.position.x += f.x - a.ox; rig.root.position.y += a.oy - f.y; }
    rig.spin.rotation.z = 0;
  }
  window.PETRIG = { build, pose, load, ready: id => !!scenes[id] };
})();
