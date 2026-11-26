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
    dragon: { front: null, back: null, belly: -0.3, shZ: 0.2, hipZ: -0.2, pivY: -0.1, kneeY: -0.35, neckZ: 0.32, neckY: 0.2, tailZ: -0.22, tailY: -0.15, tailDir: -1, wings: true },
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
      L.set([fl, fr, bl, br], i * 4);
      const legs = fl + fr + bl + br;
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
      const grp = new T.Group();
      for (const o of parts) {
        const geo = o.geometry.clone(); geo.applyMatrix4(o.matrixWorld); geo.translate(-c.x, -c.y, -c.z); geo.scale(norm, norm, norm); bake(geo, P);
        const mat = o.material.clone(); mat.metalness = 0; mat.roughness = 0.85; mat.envMapIntensity = 0.45; if (P.wings) mat.side = T.DoubleSide;
        mat.onBeforeCompile = sh => { Object.assign(sh.uniforms, U); sh.vertexShader = sh.vertexShader.replace('#include <common>', '#include <common>\n' + VERT).replace('#include <begin_vertex>', BODY); };
        const mm = new T.Mesh(geo, mat); mm.castShadow = true; mm.receiveShadow = true; mm.frustumCulled = false; grp.add(mm); rig.look.mats.push(mat);
      }
      const k = H / (size.y * norm); grp.scale.setScalar(k); grp.position.y = (size.y * norm / 2) * k; // feet on the ground
      rig.len = Math.max(size.x, size.z) * norm * k;
      inner.add(grp); inner.position.y = -H * 0.5; body.position.y = H * 0.5;
    });
    return rig;
  }
  const clamp = (v, a, b) => (v < a ? a : v > b ? b : v), ease = (a, b, k) => a + (b - a) * k;
  const moveName = f => { const mv = f.move; if (!mv) return ''; if (!f._mvn || f._mvn.mv !== mv) f._mvn = { mv, n: Object.keys(f.moves).find(k => f.moves[k] === mv) || '' }; return f._mvn.n; };

  // targets for the joints this frame
  function pose(rig, f, rot, a, tMenu, once) {
    const H = rig.H, P = rig.plan, dog = rig.pet === 'dog', cat = rig.pet === 'cat', drg = rig.pet === 'dragon';
    const t = f ? f.anim : (tMenu || 0), S = f ? f.state : 'idle';
    const J = { hip: [0, 0, 0, 0], knee: [0, 0, 0, 0], arch: 0, sway: 0, hp: 0, hy: 0, hl: 0, wag: dog ? 0.06 : 0.035, tlift: 0, tsw: 0, tspd: dog ? 0.35 : 0.06, bob: 0, pitch: 0, flip: 0, lunge: 0, side: 0, sy: 1, a1: 0.2, a2: 0.1, sweep: 0 };
    if (!f && once) rig.happy = 80;
    const speed = f ? Math.abs(f.vx) / Math.max(1, f.def.run) : 0;
    // gait cycle advances with ground speed so the paws plant instead of sliding
    const gaitHz = S === 'run' ? 0.03 + speed * 0.05 : S === 'walk' ? 0.025 : 0;
    rig.ph = (rig.ph + gaitHz) % 1;
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
        if (drg) { J.a1 = 0.1 + 0.05 * Math.sin(t * 0.05); J.a2 = -0.3; J.sweep = 0.5; }
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
    // ease every joint toward its target (fast for attacks so hits still read, softer otherwise)
    const M = rig.mem, k = S === 'attack' ? 0.4 : 0.2;
    const e = (key, v) => (M[key] = M[key] == null ? v : ease(M[key], v, k));
    const U = rig.U;
    U.uHip.value.set(e('h0', J.hip[0]), e('h1', J.hip[1]), e('h2', J.hip[2]), e('h3', J.hip[3]));
    U.uKnee.value.set(e('k0', J.knee[0]), e('k1', J.knee[1]), e('k2', J.knee[2]), e('k3', J.knee[3]));
    rig.tailT += J.tspd;
    U.uTail.value.set(rig.tailT, e('wag', J.wag), e('tl', J.tlift), e('tsw', J.tsw));
    U.uHead.value.set(e('hp', J.hp), e('hy', J.hy), e('hl', J.hl), 0);
    U.uSpine.value.set(e('arch', J.arch), e('sway', J.sway), Math.sin(t * 0.07), P.neckZ);
    U.uWing.value.set(e('a1', J.a1), e('a2', J.a2), e('swp', J.sweep), P.neckY);
    // whole body: bob, lunge, pitch (rears up about the hind feet), flips spin about the middle
    const pitch = e('pitch', J.pitch), lunge = e('lunge', J.lunge), bob = e('bob', J.bob), sy = e('sy', J.sy), side = e('side', J.side);
    const rearUp = Math.min(0, pitch) * -1, back = (rig.len || H * 2) * 0.42;
    rig.body.position.set(0, H * 0.5 + bob + Math.sin(rearUp) * back * 0.9, lunge - (1 - Math.cos(rearUp)) * back);
    rig.body.rotation.set(pitch + J.flip, 0, side);
    rig.body.scale.set(1 + (1 - sy) * 0.4, sy, 1);
    if (f && a) { rig.root.position.x += f.x - a.ox; rig.root.position.y += a.oy - f.y; }
    rig.spin.rotation.z = 0;
  }
  window.PETRIG = { build, pose, load, ready: id => !!scenes[id] };
})();
