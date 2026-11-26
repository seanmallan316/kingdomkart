'use strict';
// Super Family Smash: the family pets as fighters. Their 3D bodies come from the family's photos (Meshy), as a single
// mesh with no skeleton, so they are animated procedurally: the whole body trots, bobs, leans, pounces, rears up,
// flips and squashes, driven by the fighter's state. render3d.js calls PETRIG.build(def) and PETRIG.pose(rig, f, rot, a).
(() => {
  const T = window.THREE; if (!T) return;
  const TAU = Math.PI * 2, scenes = {}, waiting = {};
  // parse the packed GLBs once
  function load(id, cb) {
    if (scenes[id]) return cb(scenes[id]);
    (waiting[id] = waiting[id] || []).push(cb); if (waiting[id].length > 1) return;
    const src = (window.CLASH_PETS || {})[id]; if (!src || !T.GLTFLoader) return;
    const s = atob(src), u8 = new Uint8Array(s.length); for (let i = 0; i < s.length; i++) u8[i] = s.charCodeAt(i);
    new T.GLTFLoader().parse(u8.buffer, '', gl => { scenes[id] = gl.scene; for (const f of waiting[id]) f(gl.scene); waiting[id] = []; }, e => console.warn('pet model failed', id, e));
  }
  for (const id of Object.keys(window.CLASH_PETS || {})) load(id, () => {});

  function build(def) {
    const rig = { def, pet: def.pet, turnY: Math.PI / 2 - 0.42, blades: [], look: { mats: [] } };
    const root = new T.Group(), spin = new T.Group(), turn = new T.Group(), body = new T.Group(), inner = new T.Group();
    root.add(spin); spin.add(turn); turn.add(body); body.add(inner);
    rig.root = root; rig.spin = spin; rig.turn = turn; rig.body = body; rig.inner = inner;
    const H = 80 * def.scale * (def.hh || 1); rig.H = H; rig.hipRest = H * 0.5;
    load(def.id, scene => {
      const m = scene.clone(true);
      m.traverse(o => { if (o.isMesh) { o.castShadow = true; o.receiveShadow = true; o.frustumCulled = false; o.material = o.material.clone(); o.material.metalness = 0; o.material.roughness = 0.85; o.material.envMapIntensity = 0.45; rig.look.mats.push(o.material); } });
      const box = new T.Box3().setFromObject(m), size = box.getSize(new T.Vector3()), c = box.getCenter(new T.Vector3());
      const k = H / size.y; m.scale.setScalar(k); m.position.set(-c.x * k, -box.min.y * k, -c.z * k);
      rig.len = Math.max(size.x, size.z) * k;
      inner.add(m); inner.position.y = -H * 0.5; body.position.y = H * 0.5; // rotate about the middle of the body
    });
    return rig;
  }
  const clamp = (v, a, b) => (v < a ? a : v > b ? b : v);
  // pose: whole-body motion from the fighter's state
  function pose(rig, f, rot, a, tMenu, once) {
    const H = rig.H, t = f ? f.anim : (tMenu || 0);
    let bob = 0, pitch = 0, flip = 0, sx = 1, sy = 1, lunge = 0, side = 0;
    const S = f ? f.state : 'idle';
    if (!f && once) rig.happy = 70;
    if (rig.happy > 0) { rig.happy--; const u = rig.happy / 70; bob = Math.abs(Math.sin(u * Math.PI * 4)) * H * 0.35; pitch = -0.25 * Math.sin(u * Math.PI * 4); }
    else switch (S) {
      case 'idle': case 'respawn': case 'shieldDrop': case 'getup': sy = 1 + 0.025 * Math.sin(t * 0.08); sx = 1 - 0.012 * Math.sin(t * 0.08); pitch = 0.03 * Math.sin(t * 0.045); if (f && G.ending && f.stocks > 0) { bob = Math.abs(Math.sin(t * 0.15)) * H * 0.3; } break;
      case 'walk': bob = Math.abs(Math.sin(t * 0.25)) * H * 0.05; pitch = Math.sin(t * 0.5) * 0.05; break;
      case 'run': bob = Math.abs(Math.sin(t * 0.42)) * H * 0.12; pitch = Math.sin(t * 0.84) * 0.12; sy = 1 - 0.05 * Math.cos(t * 0.84); break;
      case 'jumpsquat': case 'land': case 'crouch': sy = 0.78; sx = 1.12; break;
      case 'air': case 'helpless': if (f.flipT > 0) flip = (1 - f.flipT / 24) * TAU; else pitch = clamp(-f.vy * 0.045, -0.5, 0.55); break;
      case 'shield': case 'shieldstun': sy = 0.82; sx = 1.08; pitch = 0.12; break;
      case 'roll': case 'tech': case 'airdodge': case 'spotdodge': flip = (f.t / 26) * TAU; sy = 0.85; break;
      case 'hitstun': if (f.tumble) flip = t * 0.35; else pitch = 0.35; break;
      case 'down': pitch = 0; sy = 0.7; side = 0.9; break;
      case 'ledge': pitch = -0.7; break;
      case 'grabbed': pitch = 0.3; break;
      case 'holding': lunge = H * 0.2; break;
      case 'attack': {
        const mv = f.move || {}, hbs = (mv.hb || []).concat(f.xhb || []), hs = hbs.length ? Math.min.apply(null, hbs.map(h => h.f[0])) : Math.max(4, mv.len * 0.3);
        const he = hbs.length ? Math.max.apply(null, hbs.map(h => h.f[1])) : hs + 6, u = f.mt;
        const strike = u < hs ? -(u / hs) : u <= he ? 1 : Math.max(0, 1 - (u - he) / 10); // wind-up, hit, recover
        const nm = Object.keys(f.moves).find(k => f.moves[k] === mv) || '';
        if (mv.cine) { if (mv.cine === 'storm') flip = u * 0.4; else if (mv.cine === 'rush') { bob = Math.abs(Math.sin(u * 0.6)) * H * 0.15; pitch = -0.2; } else { pitch = -0.35; bob = Math.abs(Math.sin(u * 0.2)) * H * 0.2; } break; }
        if (/^(nair|nspec|uspec|whirl)/.test(nm) || mv.rollSpin) { flip = (u / Math.max(1, mv.len)) * TAU * 1.5; sy = 0.9; }
        else if (/^(uair|utilt|usmash)/.test(nm)) { pitch = -0.9 * Math.max(0, strike); lunge = 0; }
        else if (/^(dair|dtilt|dsmash|dspec|quake)/.test(nm)) { sy = 1 - 0.3 * Math.max(0, strike); sx = 1 + 0.2 * Math.max(0, strike); pitch = 0.25 * Math.max(0, strike); }
        else if (/^(bair)/.test(nm)) { lunge = -H * 0.6 * Math.max(0, strike); pitch = 0.2; }
        else { lunge = H * (mv.smash ? 0.9 : 0.6) * strike; pitch = mv.smash ? -0.2 * strike : 0.1 * Math.max(0, strike); sy = strike < 0 ? 1 + 0.08 * strike : 1 + 0.05 * strike; }
        break;
      }
    }
    // body group: bob, pounce forward, tilt, squash; flips spin the whole body about its middle
    rig.body.position.set(0, H * 0.5 + bob, lunge);
    rig.body.rotation.set(pitch + flip, 0, side);
    rig.body.scale.set(sx, sy, sx * (1 + (1 - sy) * 0.3));
    // keep the pet's feet on the fighter's position (the human anchor maths is for two legs)
    if (f && a) { rig.root.position.x += f.x - a.ox; rig.root.position.y += a.oy - f.y; }
    rig.spin.rotation.z = 0;
  }
  window.PETRIG = { build, pose, load, ready: id => !!scenes[id] };
})();
