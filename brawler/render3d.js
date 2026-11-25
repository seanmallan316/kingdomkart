'use strict';
// 3D scene for Clash Core: stage, lights and fighters built from simple shapes in code (three.js).
// The fight rules stay in game.js; this file only draws what game.js simulates.
// World units are the same as the 2D game: x right, y DOWN in the sim, so three.js y = -sim y.

const R3 = (() => {
  const FOV = 28, TURN = Math.PI / 2 - 0.42;
  const L = 18, T = 26, U = 15; // leg segment, torso, arm segment (same as the sim's poses)
  let renderer, scene, camera, stageGroup, fighterGroup, sun, platMeshes = [], rocks = [];
  const rigs = new Map();
  let pRenderer, pScene, pCamera; const pCache = {};
  let ok = false;

  const std = (color, o) => new THREE.MeshStandardMaterial(Object.assign({ color, roughness: 0.72, metalness: 0.02 }, o || {}));
  const mesh = (geo, mat, x = 0, y = 0, z = 0) => { const m = new THREE.Mesh(geo, mat); m.position.set(x, y, z); m.castShadow = true; return m; };
  const sphere = (r, mat, x, y, z, sx = 1, sy = 1, sz = 1) => { const m = mesh(new THREE.SphereGeometry(r, 20, 14), mat, x, y, z); m.scale.set(sx, sy, sz); return m; };
  const box = (w, h, d, mat, x, y, z) => mesh(new THREE.BoxGeometry(w, h, d), mat, x, y, z);
  const capsuleDown = (len, r, mat) => { const m = mesh(new THREE.CapsuleGeometry(r, len, 4, 12), mat, 0, -len / 2, 0); return m; };

  function lights(sc, forStage) {
    sc.add(new THREE.HemisphereLight(0xe6eeff, 0x4a3d55, 2.1));
    const d = new THREE.DirectionalLight(0xfff1dc, 2.6); d.position.set(-420, 900, 700);
    if (forStage) {
      d.castShadow = true; d.shadow.mapSize.set(1024, 1024);
      const c = d.shadow.camera; c.left = -800; c.right = 800; c.top = 800; c.bottom = -400; c.near = 100; c.far = 3000;
      d.shadow.bias = -0.0015; d.shadow.normalBias = 2;
    }
    sc.add(d); sc.add(d.target);
    const rim = new THREE.DirectionalLight(0x8fb4ff, 1.1); rim.position.set(500, 300, -600); sc.add(rim);
    return d;
  }

  function init() {
    if (ok || !window.THREE) return ok;
    try {
      renderer = new THREE.WebGLRenderer({ canvas: document.getElementById('gl'), antialias: true, alpha: true });
    } catch (e) { console.warn('WebGL not available', e); return false; }
    renderer.shadowMap.enabled = true; renderer.shadowMap.type = THREE.PCFSoftShadowMap;
    scene = new THREE.Scene();
    camera = new THREE.PerspectiveCamera(FOV, 1, 20, 9000);
    sun = lights(scene, true);
    stageGroup = new THREE.Group(); fighterGroup = new THREE.Group(); scene.add(stageGroup, fighterGroup);
    ok = true; resize();
    return true;
  }
  function resize() {
    if (!ok) return;
    renderer.setPixelRatio(Math.min(2, window.devicePixelRatio || 1));
    renderer.setSize(innerWidth, innerHeight, false);
  }
  addEventListener('resize', resize);

  // ------------------------------------------------------------ stage
  function clearGroup(g) {
    for (const c of [...g.children]) { g.remove(c); c.traverse(o => { if (o.geometry) o.geometry.dispose(); if (o.material) { if (o.material.map) o.material.map.dispose(); o.material.dispose(); } }); }
  }
  function buildStage(st, plats) {
    if (!init()) return;
    clearGroup(stageGroup); platMeshes = []; rocks = [];
    document.getElementById('sky').style.background = `linear-gradient(180deg, ${st.sky[0]} 0%, ${st.sky[1]} 60%, ${st.sky[2]} 100%)`;
    scene.fog = new THREE.Fog(new THREE.Color(st.sky[1]), 1900, 5200);
    const m = st.main, w = m.x2 - m.x1, cx = (m.x1 + m.x2) / 2, D = 230;
    const topMat = std(st.top, { roughness: 0.55 }), gMat = std(st.ground, { roughness: 0.9, flatShading: true });
    const glow = new THREE.MeshStandardMaterial({ color: st.trim, emissive: st.trim, emissiveIntensity: 0.9 });
    const slab = box(w + 6, 12, D, topMat, cx, -m.y - 6, 0); slab.receiveShadow = true; slab.castShadow = false; stageGroup.add(slab);
    stageGroup.add(box(w + 8, 3, 4, glow, cx, -m.y - 1.5, D / 2 + 1));
    stageGroup.add(box(8, 14, D + 4, glow, m.x1 - 1, -m.y - 6, 0), box(8, 14, D + 4, glow, m.x2 + 1, -m.y - 6, 0));
    const sh = new THREE.Shape(); const y0 = -m.y - 12, yd = -m.y - m.depth;
    sh.moveTo(m.x1, y0); sh.lineTo(m.x2, y0); sh.lineTo(m.x2, yd); sh.lineTo(m.x2 * 0.55, yd - 90); sh.lineTo(cx, yd - 150); sh.lineTo(m.x1 * 0.55, yd - 90); sh.lineTo(m.x1, yd); sh.closePath();
    const body = new THREE.Mesh(new THREE.ExtrudeGeometry(sh, { depth: D * 0.92, bevelEnabled: true, bevelSize: 6, bevelThickness: 6, bevelSegments: 1 }), gMat);
    body.position.z = -D * 0.46; body.receiveShadow = true; stageGroup.add(body);
    const strip = new THREE.MeshStandardMaterial({ color: st.top, emissive: st.top, emissiveIntensity: 0.55 });
    for (let k = -3; k <= 3; k++) stageGroup.add(box(4, 46 + Math.abs(k) * 8, 3, strip, cx + k * (w / 8), -m.y - 46 - Math.abs(k) * 3, D * 0.46 + 6));
    for (const p of plats) {
      const g = new THREE.Group();
      const pw = p.x2 - p.x1;
      const top = box(pw, 7, 120, topMat, 0, -3.5, 0); top.receiveShadow = true; g.add(top);
      g.add(box(pw + 2, 2, 3, glow, 0, -1, 61));
      g.add(box(pw * 0.5, 5, 60, gMat, 0, -9, 0));
      stageGroup.add(g); platMeshes.push({ g, p });
    }
    // far floating rocks
    let seed = 11 + w; const rnd = () => (seed = (seed * 16807) % 2147483647) / 2147483647;
    const rMat = std(new THREE.Color(st.ground).lerp(new THREE.Color(st.sky[1]), 0.35), { flatShading: true, roughness: 1 });
    for (let k = 0; k < 16; k++) {
      const r = 50 + rnd() * 90; const rock = new THREE.Mesh(new THREE.IcosahedronGeometry(r, 0), rMat);
      rock.position.set((rnd() - 0.5) * 5200, (rnd() - 0.75) * 1300, -1300 - rnd() * 1800); rock.scale.set(1.5, 0.8 + rnd() * 0.5, 1);
      rock.rotation.set(rnd() * 3, rnd() * 3, rnd() * 3); rock.userData = { by: rock.position.y, ph: rnd() * 6 };
      stageGroup.add(rock); rocks.push(rock);
    }
    sun.target.position.set(cx, -m.y, 0);
  }

  // ------------------------------------------------------------ fighters
  // Looks: each returns the materials and the head / torso / foot builders for one fighter.
  function lookGeneric(def) {
    const body = std(def.color), dark = std(def.dark), acc = std(def.accent, { emissive: def.accent, emissiveIntensity: 0.25 });
    return {
      mats: [body, dark, acc], sleeve: body, hand: acc, thigh: dark, shin: dark, armR: 4.2, legR: 5,
      head(g) {
        if (def.acc === 'helm') { g.add(box(24, 24, 24, body, 0, 12, 0)); g.add(box(20, 6, 3, acc, 0, 13, 12)); }
        else { g.add(sphere(12, body, 0, 12, 0)); g.add(box(15, 5, 4, std('#f4f7ff', { emissive: '#f4f7ff', emissiveIntensity: 0.4 }), 0, 13, 9.6)); }
        if (def.acc === 'scarf') { g.add(mesh(new THREE.TorusGeometry(8, 2.6, 8, 16), acc, 0, 1, 0)).rotation.x = Math.PI / 2; g.add(box(4, 3, 22, acc, -5, 0, -16)); }
      },
      torso(g) { g.add(mesh(new THREE.CapsuleGeometry(9.5, T - 10, 4, 14), body, 0, T / 2, 0)); g.add(sphere(3.4, acc, 0, T * 0.6, 8.6)); },
      foot(g) { g.add(sphere(5, dark, 0, -L, 3.5, 1, 0.75, 1.7)); },
      hand(g, side) {
        g.add(sphere(4.6, acc, 0, -U, 0));
        if (def.acc === 'blade') { const b = box(2.4, 6, 44 * def.reach, std('#e8f4ff', { metalness: 0.7, roughness: 0.25, emissive: '#bfe3ff', emissiveIntensity: 0.2 }), 0, -U - 2, 0); b.rotation.x = Math.PI / 2; b.position.y = -U - 22 * def.reach; b.userData.blade = side; g.add(b); g.add(box(8, 2, 8, dark, 0, -U - 3, 0)); }
      },
      extras(rig) {
        if (def.acc === 'orbs') { rig.orbs = [0, 1].map(() => { const o = sphere(4.2, std(def.accent, { emissive: def.accent, emissiveIntensity: 1 }), 0, 0, 0); rig.torso.add(o); return o; }); }
      },
    };
  }

  // The kids: hand-modelled from the family reference photos (no photo is stored or uploaded).
  function shirtPrint(kind) {
    const c = document.createElement('canvas'); c.width = 256; c.height = 256; const g = c.getContext('2d');
    g.textAlign = 'center'; g.textBaseline = 'middle';
    if (kind === 'varsity') {
      g.font = '900 100px "Arial Black", Impact, sans-serif';
      g.lineWidth = 12; g.strokeStyle = '#f4f1e6'; g.strokeText('JCS', 128, 128); g.fillStyle = '#f4f1e6'; g.fillText('JCS', 128, 128);
      g.lineWidth = 3; g.strokeStyle = '#1c3fd6'; g.strokeText('JCS', 128, 128);
    } else if (kind === 'stars') {
      const navy = '#1f2a5c', star = (x, y) => { g.fillStyle = '#fff'; g.beginPath(); for (let i = 0; i < 10; i++) { const r = i % 2 ? 4 : 10, a = -Math.PI / 2 + i * Math.PI / 5; g.lineTo(x + Math.cos(a) * r, y + Math.sin(a) * r); } g.fill(); };
      g.font = '900 44px "Arial Black", Impact, sans-serif';
      g.fillStyle = navy; g.fillRect(30, 20, 196, 50); star(52, 45); star(204, 45); g.fillStyle = '#ff5a4f'; g.fillText('RED', 128, 47);
      g.fillStyle = '#fff'; g.fillText('WHITE', 128, 98);
      g.fillStyle = navy; g.fillRect(30, 124, 196, 50); star(52, 149); star(204, 149); g.fillStyle = '#ff5a4f'; g.fillText('AND', 128, 151);
      g.fillStyle = navy; g.fillText('COOL', 128, 208);
    }
    g.globalCompositeOperation = 'destination-out'; // worn print
    for (let k = 0; k < 300; k++) { g.fillStyle = 'rgba(0,0,0,' + (0.3 + Math.random() * 0.6) + ')'; g.fillRect(Math.random() * 256, Math.random() * 256, 1.5 + Math.random() * 2, 1.5 + Math.random() * 2); }
    const t = new THREE.CanvasTexture(c); t.colorSpace = THREE.SRGBColorSpace; return t;
  }
  const KIDS = {
    ray: { shirt: '#1c3fd6', shirtDk: '#1733b0', jeans: '#27304a', clog: '#1f9c9a', hole: '#0e5f5e', hair: '#4a3424', hoodie: true, print: 'varsity', sock: true },
    ollie: { shirt: '#e3271d', shirtDk: '#bd1d15', jeans: '#5d7fb3', clog: '#2b3566', hole: '#161c3d', hair: '#54402f', hoodie: false, print: 'stars', sword: true },
  };
  function lookKid(def) {
    const o = KIDS[def.id];
    const skin = std('#e9b792', { roughness: 0.6 }), hair = std(o.hair, { roughness: 0.95 }), shirt = std(o.shirt, { roughness: 0.9 }),
      shirtDk = std(o.shirtDk, { roughness: 0.95 }), jeans = std(o.jeans, { roughness: 0.9 }), clog = std(o.clog, { roughness: 0.5 }),
      dark = std('#2a1c16'), white = std('#fbfbf6', { roughness: 0.4 }), lip = std('#b9584f'), blush = std('#f0a08c', { transparent: true, opacity: 0.35 });
    return {
      mats: [skin, hair, shirt, shirtDk, jeans, clog], sleeve: shirt, fore: o.hoodie ? shirt : skin, hand: skin, thigh: jeans, shin: jeans, armR: o.hoodie ? 4.8 : 4.1, legR: 5.2,
      head(g) {
        const R = 14.5, cy = 14;
        g.add(sphere(R, skin, 0, cy, 0, 1, 1.04, 1));
        const cap = mesh(new THREE.SphereGeometry(R + 0.7, 24, 14, 0, Math.PI * 2, 0, Math.PI * 0.46), hair, 0, cy + 0.6, -0.8); cap.rotation.x = -0.5; g.add(cap);
        g.add(sphere(3.4, skin, -R + 0.4, cy - 1.5, -0.5, 0.6, 1, 0.9), sphere(3.4, skin, R - 0.4, cy - 1.5, -0.5, 0.6, 1, 0.9));
        for (const s of [-1, 1]) {
          g.add(sphere(2.3, white, s * 5.2, cy + 1.6, 12.6, 1, 1.1, 0.6));
          g.add(sphere(1.5, dark, s * 5.2, cy + 1.5, 13.7, 1, 1.1, 0.5));
          const br = box(5, 1, 1, hair, s * 5.3, cy + 5.4, 13.1); br.rotation.z = -s * 0.12; g.add(br);
          g.add(sphere(2.6, blush, s * 8.6, cy - 3, 10.9, 1, 0.7, 0.4));
        }
        g.add(sphere(1.6, skin, 0, cy - 1.2, 14.5));
        const smile = mesh(new THREE.TorusGeometry(3.6, 0.6, 6, 16, Math.PI), lip, 0, cy - 4.6, 13.2); smile.rotation.z = Math.PI; smile.castShadow = false; g.add(smile);
        g.add(box(2, 1.6, 0.8, white, -1.5, cy - 5.6, 13.5), box(2, 1.6, 0.8, white, 1.5, cy - 5.6, 13.5)); // front teeth with a gap
        g.add(sphere(4.2, skin, 0, 1.5, 0, 1, 1.2, 1)); // neck
      },
      torso(g) {
        const R = o.hoodie ? 11.5 : 10.4;
        const t = mesh(new THREE.CapsuleGeometry(R, T - 12, 4, 16), shirt, 0, T / 2 + 1, 0); t.scale.z = 0.86; g.add(t);
        if (o.hoodie) {
          const band = mesh(new THREE.TorusGeometry(10.8, 2.3, 8, 20), shirtDk, 0, 3.2, 0); band.rotation.x = Math.PI / 2; band.scale.y = 0.86; g.add(band);
          g.add(sphere(9.5, shirt, 0, T - 1.5, -8.5, 1.15, 0.72, 0.85)); // hood resting on the back
          const pocket = mesh(new THREE.CylinderGeometry(R + 0.6, R + 0.6, 7.5, 14, 1, true, -0.62, 1.24), shirtDk, 0, 9, 0); pocket.scale.z = 0.86; g.add(pocket);
        } else {
          const hem = mesh(new THREE.CylinderGeometry(R + 0.4, R + 0.9, 5, 18, 1, true), shirt, 0, 2.5, 0); hem.scale.z = 0.86; hem.material.side = THREE.DoubleSide; g.add(hem);
        }
        const collar = mesh(new THREE.TorusGeometry(o.hoodie ? 6.4 : 5.2, o.hoodie ? 2.2 : 1.1, 8, 18), o.hoodie ? shirt : shirtDk, 0, T - 0.5, 1); collar.rotation.x = Math.PI / 2 - 0.35; g.add(collar);
        const lm = new THREE.MeshStandardMaterial({ map: shirtPrint(o.print), transparent: true, roughness: 0.9, alphaTest: 0.25 });
        const ph = o.print === 'varsity' ? 15 : 15.5;
        const letters = mesh(new THREE.CylinderGeometry(R + 0.45, R + 0.45, ph, 16, 1, true, -0.8, 1.6), lm, 0, o.print === 'varsity' ? 17.5 : 14.5, 0); letters.scale.z = 0.86; letters.castShadow = false; g.add(letters);
      },
      foot(g) {
        g.add(sphere(5.6, clog, 0, -L + 0.6, 4.2, 1.08, 0.78, 1.85));
        const strap = mesh(new THREE.TorusGeometry(5.3, 1.1, 6, 14, Math.PI), clog, 0, -L + 1.5, -1.5); strap.rotation.y = Math.PI / 2; strap.rotation.x = -0.5; g.add(strap);
        const hole = std(o.hole);
        for (let i = 0; i < 6; i++) g.add(sphere(0.75, hole, (i % 3 - 1) * 2.6, -L + 4.6 - (i > 2 ? 0.5 : 0), 7 + (i > 2 ? 3.2 : 0)));
        if (o.sock) g.add(sphere(4.8, std('#f2f2ee'), 0, -L + 3, 0.5, 0.9, 0.6, 0.9));
      },
      hand(g, side) {
        if (o.hoodie) g.add(mesh(new THREE.CylinderGeometry(5.3, 5.3, 3.4, 12), shirtDk, 0, -U + 2.6, 0));
        g.add(sphere(4.3, skin, 0, -U - 1.6, 0));
        if (o.sword) { // foam toy sword, held in whichever hand is nearest the camera
          const sw = new THREE.Group(); sw.userData.blade = side; sw.position.y = -U - 2;
          sw.add(mesh(new THREE.CylinderGeometry(1.6, 1.6, 9, 8), std('#2b3566'), 0, 0, 0));
          sw.add(box(11, 2.4, 3.4, std('#ffd23f'), 0, -5, 0));
          const bl = mesh(new THREE.CapsuleGeometry(2.6, 34 * def.reach, 4, 10), std('#f4f6fa', { roughness: 0.55, emissive: '#9fc4ff', emissiveIntensity: 0.18 }), 0, -8 - 17 * def.reach, 0); bl.scale.z = 0.55; sw.add(bl);
          sw.rotation.x = -0.25; g.add(sw);
        }
      },
      extras() {},
    };
  }
  const LOOKS = { ray: lookKid, ollie: lookKid };

  function buildRig(def) {
    const look = (LOOKS[def.id] || lookGeneric)(def);
    const rig = { def, look, arm: {}, leg: {}, turnY: TURN, blades: [] };
    const root = new THREE.Group(), spin = new THREE.Group(), turn = new THREE.Group();
    root.add(spin); spin.add(turn); rig.root = root; rig.spin = spin; rig.turn = turn;
    const torso = new THREE.Group(); turn.add(torso); rig.torso = torso; look.torso(torso);
    const head = new THREE.Group(); head.position.y = T; torso.add(head); rig.head = head; look.head(head);
    for (const side of ['L', 'R']) {
      const sx = side === 'L' ? 1 : -1;
      const arm = new THREE.Group(); arm.position.set(sx * 12, T - 3.5, 0); torso.add(arm);
      const splay = new THREE.Group(); splay.rotation.z = sx * 0.16; arm.add(splay);
      splay.add(capsuleDown(U, look.armR, look.sleeve));
      const fore = new THREE.Group(); fore.position.y = -U; splay.add(fore);
      fore.add(capsuleDown(U, look.armR * 0.95, look.fore || look.sleeve)); look.hand(fore, side);
      rig.arm[side] = { sh: arm, el: fore };
      const leg = new THREE.Group(); leg.position.set(sx * 5.6, 1, 0); turn.add(leg);
      leg.add(capsuleDown(L, look.legR, look.thigh));
      const shin = new THREE.Group(); shin.position.y = -L; leg.add(shin);
      shin.add(capsuleDown(L - 2, look.legR * 0.9, look.shin)); look.foot(shin);
      rig.leg[side] = { hip: leg, knee: shin };
    }
    look.extras(rig);
    root.traverse(o => { if (o.userData && o.userData.blade) rig.blades.push(o); });
    root.scale.setScalar(def.scale);
    return rig;
  }

  function poseRig(rig, pose, rot, facing, hipH, t) {
    const [lean, afS, afE, abS, abE, lfH, lfK, lbH, lbK] = pose;
    const near = facing > 0 ? 'R' : 'L', far = facing > 0 ? 'L' : 'R';
    rig.spin.position.y = hipH / rig.def.scale; rig.spin.rotation.z = -rot;
    rig.torso.rotation.x = lean; rig.head.rotation.x = -lean * 0.6;
    const A = (a, s, e) => { a.sh.rotation.x = -s - lean; a.el.rotation.x = -e; };
    A(rig.arm[near], afS, afE); A(rig.arm[far], abS, abE);
    const Lg = (l, h, k) => { l.hip.rotation.x = -h; l.knee.rotation.x = -k; };
    Lg(rig.leg[near], lfH, lfK); Lg(rig.leg[far], lbH, lbK);
    for (const b of rig.blades) b.visible = b.userData.blade === near;
    if (rig.orbs) rig.orbs.forEach((o, k) => { const a = t * 0.08 + k * Math.PI; o.position.set(Math.cos(a) * 24, T * 0.7 + Math.sin(a * 1.3) * 6, Math.sin(a) * 24); });
  }

  function resetRigs() { if (!ok) return; clearGroup(fighterGroup); rigs.clear(); }

  function syncFighter(f) {
    let rig = rigs.get(f);
    if (!rig) { rig = buildRig(f.def); rigs.set(f, rig); fighterGroup.add(rig.root); rig.turnY = f.facing * TURN; }
    const gone = f.state === 'dead' || f.state === 'out' || f.hidden;
    rig.root.visible = !gone; if (gone) return;
    const { pose, rot } = computePose(f);
    const a = fighterAnchor(f, pose);
    rig.root.position.set(a.ox, -a.oy, 0);
    let want = f.facing * TURN;
    if (f.state === 'ledge') want = f.facing * (Math.PI / 2 + 0.5); // hang facing the wall
    rig.turnY += (want - rig.turnY) * 0.35; rig.turn.rotation.y = rig.turnY;
    if (f.state === 'attack' && f.move && f.move.twirl && f.mt > 4 && f.mt < f.move.len - 8) rig.turn.rotation.y = rig.turnY + f.mt * 0.75;
    poseRig(rig, pose, rot, f.facing, a.hipH, G.frame);
    // flashes: invincible blink, hurt tint, overdrive glow
    let col = 0x000000, k = 0;
    if ((f.invinc > 0 && G.frame % 6 < 3) || (f.intang > 0 && f.state !== 'ledge' && G.frame % 4 < 2)) { col = 0xffffff; k = 0.55; }
    else if (f.state === 'hitstun' && f.hitstop > 0) { col = 0xff5a5a; k = 0.6; }
    else if (f.od > 0) { col = new THREE.Color(f.def.accent).getHex(); k = 0.3 + 0.15 * Math.sin(G.frame * 0.4); }
    if (rig.flashKey !== col + ':' + (k > 0)) {
      rig.flashKey = col + ':' + (k > 0);
      for (const m of rig.look.mats) { if (!m.userData.baseE) m.userData.baseE = { c: m.emissive.getHex(), i: m.emissiveIntensity }; if (k > 0) { m.emissive.setHex(col); m.emissiveIntensity = k; } else { m.emissive.setHex(m.userData.baseE.c); m.emissiveIntensity = m.userData.baseE.i; } }
    }
  }

  function render(shx, shy) {
    if (!ok) return false;
    for (const pm of platMeshes) pm.g.position.set((pm.p.x1 + pm.p.x2) / 2, -pm.p.y, 0);
    for (const r of rocks) r.position.y = r.userData.by + Math.sin(G.frame * 0.006 + r.userData.ph) * 18;
    for (const f of G.fighters) syncFighter(f);
    const s = camScale(), d = VH / (2 * s * Math.tan((FOV * DEG) / 2));
    const cx = G.cam.x - shx / s, cy = -G.cam.y + shy / s;
    camera.aspect = VW / VH; camera.far = d + 5000; camera.position.set(cx, cy, d); camera.lookAt(cx, cy, 0); camera.updateProjectionMatrix();
    renderer.render(scene, camera);
    return true;
  }
  function show(on) { const el = document.getElementById('gl'); if (el) el.style.visibility = on ? 'visible' : 'hidden'; }

  // ------------------------------------------------------------ fighter-select portraits
  function portrait(canvas, def) {
    const c2 = canvas.getContext('2d'); c2.clearRect(0, 0, canvas.width, canvas.height);
    const g = c2.createRadialGradient(100, 66, 5, 100, 66, 95); g.addColorStop(0, def.color + '66'); g.addColorStop(1, 'transparent');
    c2.fillStyle = g; c2.fillRect(0, 0, canvas.width, canvas.height);
    if (!window.THREE) return;
    if (!pCache[def.id]) {
      try {
        if (!pRenderer) {
          pRenderer = new THREE.WebGLRenderer({ antialias: true, alpha: true, preserveDrawingBuffer: true }); pRenderer.setSize(800, 480, false);
          pScene = new THREE.Scene(); lights(pScene, false); pCamera = new THREE.PerspectiveCamera(24, 400 / 240, 10, 2000);
        }
        const rig = buildRig(def); rig.turn.rotation.y = 0.5;
        poseRig(rig, POSES.idle, 0, 1, 33 * def.scale, 20);
        pScene.add(rig.root);
        const h = 100 * def.scale; pCamera.position.set(0, h * 0.5, h * 2.6); pCamera.lookAt(0, h * 0.46, 0);
        pRenderer.render(pScene, pCamera);
        const copy = document.createElement('canvas'); copy.width = 800; copy.height = 480; copy.getContext('2d').drawImage(pRenderer.domElement, 0, 0);
        pCache[def.id] = copy; pScene.remove(rig.root);
      } catch (e) { console.warn('portrait failed', e); return; }
    }
    c2.drawImage(pCache[def.id], 0, 0, canvas.width, canvas.height);
  }

  return { init, buildStage, resetRigs, render, portrait, show, get ok() { return ok; } };
})();
