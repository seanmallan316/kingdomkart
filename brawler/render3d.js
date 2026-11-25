'use strict';
// 3D scene for Super Family Smash: stage, lights and fighters built from simple shapes in code (three.js).
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
      d.castShadow = true; d.shadow.mapSize.set(4096, 4096); d.shadow.radius = 3;
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
    renderer.toneMapping = THREE.ACESFilmicToneMapping; renderer.toneMappingExposure = 0.98; // filmic contrast for the PC build
    renderer.shadowMap.enabled = true; renderer.shadowMap.type = THREE.PCFSoftShadowMap;
    scene = new THREE.Scene();
    camera = new THREE.PerspectiveCamera(FOV, 1, 20, 9000);
    sun = lights(scene, true);
    stageGroup = new THREE.Group(); fighterGroup = new THREE.Group(); scene.add(stageGroup, fighterGroup);
    ok = true; resize();
    try { const pm = new THREE.PMREMGenerator(renderer); envTex = pm.fromScene(new THREE.RoomEnvironment(), 0.04).texture; pm.dispose(); } catch (e) { envTex = null; }
    return true;
  }
  // ambient occlusion for the 3D worlds (soft shading where things meet); switched off by itself if the device is too slow
  let envTex = null, composer = null, aoPass = null, aoOn = !/[?&]noao/.test(location.search), slow = 0;
  function ensureComposer() {
    if (composer || !aoOn) return composer;
    try {
      composer = new THREE.EffectComposer(renderer); composer.setPixelRatio(Math.min(1.5, window.devicePixelRatio || 1)); composer.setSize(innerWidth, innerHeight);
      composer.addPass(new THREE.RenderPass(scene, camera));
      aoPass = new THREE.GTAOPass(scene, camera, innerWidth, innerHeight);
      aoPass.updateGtaoMaterial({ radius: 45, distanceExponent: 1.4, thickness: 12, scale: 1.2, samples: 12, distanceFallOff: 1 });
      aoPass.updatePdMaterial({ lumaPhi: 10, depthPhi: 2, normalPhi: 3, radius: 6, rings: 2, samples: 16 });
      aoPass.blendIntensity = 1.0;
      composer.addPass(aoPass); composer.addPass(new THREE.OutputPass());
    } catch (e) { console.warn('AO off', e); aoOn = false; composer = null; }
    return composer;
  }
  let lastT = 0;
  function present() {
    const now = performance.now(), dt = now - lastT; lastT = now;
    if (worldOn && aoOn && ensureComposer()) {
      if (dt > 0 && dt < 200) { slow = dt > 26 ? slow + 1 : Math.max(0, slow - 2); if (slow > 120) { aoOn = false; console.info('AO switched off for speed'); } }
      composer.render();
    } else renderer.render(scene, camera);
  }
  function resize() {
    if (!ok) return;
    renderer.setPixelRatio(Math.min(2.5, window.devicePixelRatio || 1));
    renderer.setSize(innerWidth, innerHeight, false);
    if (composer) { composer.setPixelRatio(Math.min(1.5, window.devicePixelRatio || 1)); composer.setSize(innerWidth, innerHeight); }
  }
  addEventListener('resize', resize);

  // ------------------------------------------------------------ stage
  function clearGroup(g) {
    const kill = m => { if (!m) return; for (const k of ['map', 'bumpMap']) if (m[k] && m[k].isTexture) m[k].dispose(); if (m.dispose) m.dispose(); };
    for (const c of [...g.children]) { g.remove(c); c.traverse(o => { if (o.geometry) o.geometry.dispose(); if (Array.isArray(o.material)) o.material.forEach(kill); else kill(o.material); }); }
  }

  // ------------------------------------------------------------ stage worlds, built from simple shapes
  const flat = (c, o) => std(c, Object.assign({ roughness: 1, flatShading: true }, o || {}));
  const glowM = (c, k = 1) => new THREE.MeshStandardMaterial({ color: c, emissive: c, emissiveIntensity: k });
  const cyl = (rt, rb, h, mat, x, y, z, seg = 10) => mesh(new THREE.CylinderGeometry(rt, rb, h, seg), mat, x, y, z);
  const blob = (r, mat, x, y, z, sx = 1, sy = 1, sz = 1) => { const b = mesh(new THREE.IcosahedronGeometry(r, 1), mat, x, y, z); b.scale.set(sx, sy, sz); return b; };
  const wall = (c, y = 200, z = -900) => { const p = new THREE.Mesh(new THREE.PlaneGeometry(6000, 3200), flat(c)); p.position.set(0, y, z); return p; };
  const DECOR = {
    treehouse(g, m, w) { // a deck high in a big tree, on a sunny day
      const bark = flat('#6b4a2e'), bark2 = flat('#5a3d25'), leaf = flat('#3f9b4f'), leaf2 = flat('#5fc068'), cloud = flat('#ffffff');
      g.add(cyl(130, 190, 2200, bark, 0, -500, -300, 9));
      const br = (x, y, rz, len) => { const b = cyl(34, 60, len, bark2, x, y, -300, 7); b.rotation.z = rz; g.add(b); };
      br(-330, 260, 1.05, 760); br(340, 330, -0.95, 800); br(-240, -260, 2.2, 600); br(260, -300, -2.2, 640);
      [[-620, 470, -340, 230], [-380, 640, -420, 260], [-60, 720, -460, 300], [300, 660, -400, 270], [620, 520, -340, 240], [-820, 300, -520, 200], [840, 330, -520, 210], [0, 520, -560, 340], [-470, -470, -360, 190], [480, -520, -360, 200]]
        .forEach(([x, y, z, r], i) => g.add(blob(r, i % 2 ? leaf : leaf2, x, y, z, 1.25, 0.8, 1)));
      for (let k = 0; k < 9; k++) g.add(box(12, 78, 12, bark2, m.x1 + 20 + k * ((w - 40) / 8), 39, -108)); // back railing
      g.add(box(w, 10, 12, bark, 0, 80, -108), box(w, 8, 10, bark, 0, 44, -108));
      for (let k = 0; k < 6; k++) { const c = new THREE.Group(), x = -2400 + k * 950, y = 520 - (k % 3) * 260; [[0, 0, 150], [150, 30, 120], [-150, 20, 110], [60, 70, 100]].forEach(([dx, dy, r]) => c.add(blob(r, cloud, dx, dy, 0, 1.3, 0.75, 1))); c.position.set(x, y, -1700); c.userData = { by: y, ph: k }; g.add(c); rocks.push(c); }
      const sunM = sphere(170, glowM('#fff3b0', 1.4), 1300, 900, -2300); sunM.castShadow = false; g.add(sunM);
    },
    kitchen(g, m, w) { // a giant kitchen table: you are the size of a salt shaker
      const wood = flat('#8a5a34'), white = flat('#fbf7ee'), red = flat('#e8493f'), steel = std('#c9ced6', { metalness: 0.7, roughness: 0.3 });
      for (const sx of [-1, 1]) for (const sz of [-1, 1]) g.add(cyl(26, 20, 1400, wood, sx * (w / 2 - 40), -760, sz * 80, 8));
      g.add(wall('#f6dfb8')); g.add(box(6000, 520, 30, flat('#e9c48f'), 0, -640, -880));
      const win = new THREE.Group(); win.add(box(900, 640, 20, glowM('#bfe6ff', 0.9), 0, 0, 0)); for (const x of [-460, 0, 460]) win.add(box(34, 700, 34, white, x, 0, 10)); for (const y of [-330, 0, 330]) win.add(box(960, 34, 34, white, 0, y, 10)); win.position.set(-500, 560, -870); g.add(win);
      const mug = (x, c) => { const u = new THREE.Group(); u.add(cyl(70, 62, 150, flat(c), 0, 75, 0, 16)); const h = mesh(new THREE.TorusGeometry(44, 13, 8, 14), flat(c), 74, 80, 0); u.add(h); u.position.set(x, 0, -150); g.add(u); };
      mug(-250, '#3f8fe8'); mug(300, '#ff8a3d');
      const bowl = new THREE.Group(); bowl.add(cyl(120, 70, 70, white, 0, 35, 0, 18)); [['#e8493f', -50, 95], ['#ffd23f', 30, 105], ['#57b85f', 80, 85], ['#ff8a3d', -5, 120]].forEach(([c, x, y]) => bowl.add(sphere(46, flat(c, { flatShading: false }), x, y, 0))); bowl.position.set(30, 0, -190); g.add(bowl);
      for (let k = 0; k < 2; k++) { const s2 = new THREE.Group(); s2.add(cyl(30, 34, 110, k ? flat('#333842') : white, 0, 55, 0, 12)); s2.add(cyl(22, 30, 24, steel, 0, 122, 0, 12)); s2.position.set(-520 + k * 80, 0, -140); g.add(s2); }
      for (let k = -4; k <= 4; k++) g.add(box(w / 9, 3, 230, k % 2 ? red : white, k * (w / 9), 1.6, 0)); // checked runner
    },
    driveway(g, m, w) { // the driveway at sunset: garage, hoop, hedges
      const grey = flat('#d9dce3'), dark = flat('#3a3d47'), bush = flat('#3f9b4f'), bush2 = flat('#57b85f'), brick = flat('#b5553c');
      g.add(box(2600, 900, 60, brick, 0, 450, -420)); g.add(box(980, 560, 30, grey, 0, 280, -385)); for (let k = 1; k < 6; k++) g.add(box(980, 8, 6, dark, 0, k * 93, -366));
      const roof = mesh(new THREE.CylinderGeometry(0, 1500, 420, 4), flat('#5a3d3d'), 0, 1110, -520); roof.rotation.y = Math.PI / 4; roof.scale.z = 0.5; g.add(roof);
      const hoop = new THREE.Group(); hoop.add(cyl(11, 13, 520, dark, 0, 260, 0, 8)); hoop.add(box(220, 150, 12, flat('#ffffff'), 0, 520, 30)); hoop.add(box(80, 60, 4, flat('#e8493f'), 0, 500, 38)); const ring = mesh(new THREE.TorusGeometry(42, 5, 8, 18), flat('#ff7a1e'), 0, 460, 84); ring.rotation.x = Math.PI / 2; hoop.add(ring); hoop.position.set(560, 0, -330); g.add(hoop);
      for (let k = 0; k < 9; k++) g.add(blob(110 + (k % 3) * 25, k % 2 ? bush : bush2, -1150 + k * 290, 60, -330 + (k % 2) * 40, 1.2, 0.9, 1));
      g.add(sphere(34, flat('#ff8a3d', { flatShading: false }), -470, 34, -150));
      const sunM = sphere(260, glowM('#ffd07a', 1.3), -900, 420, -2400); sunM.castShadow = false; g.add(sunM); g.add(box(8000, 30, 2400, flat('#6fae5a'), 0, -125, -1300));
      for (let k = -3; k <= 3; k++) g.add(box(60, 2, 14, flat('#fff7c2'), k * 110, 1.2, 60)); // chalk dashes
    },
    bedroom(g, m, w) { // bunk beds at night, lit by fairy lights
      const frame = flat('#c58a4a'), sheet = flat('#6c8cff'), pillow = flat('#fbf7ee');
      g.add(wall('#232a6e')); const win = new THREE.Group(); win.add(box(620, 620, 16, glowM('#1b2f8a', 0.8), 0, 0, 0)); const moon = sphere(110, glowM('#fff6c8', 1.5), 120, 110, 20); moon.castShadow = false; win.add(moon); for (const x of [-320, 0, 320]) win.add(box(26, 660, 26, pillow, x, 0, 10)); for (const y of [-320, 0, 320]) win.add(box(660, 26, 26, pillow, 0, y, 10)); win.position.set(620, 520, -880); g.add(win);
      for (const sx of [-1, 1]) g.add(box(34, 1200, 34, frame, sx * (w / 2 + 6), 250, -125)); g.add(box(w + 46, 30, 34, frame, 0, 470, -125), box(w + 46, 150, 20, frame, 0, 560, -135));
      g.add(box(w + 10, 60, 190, sheet, 0, 520, -30)); g.add(box(150, 44, 120, pillow, -w / 2 + 110, 570, -40));
      for (let k = 0; k < 6; k++) g.add(box(22, 440, 16, frame, w / 2 + 6, 40 + k * 0, -125 + 0)); for (let k = 1; k < 6; k++) g.add(box(16, 16, 120, frame, w / 2 + 40, k * 80, -70)); // ladder rungs
      const cols = ['#ff6ad5', '#ffd23f', '#6cf0ff', '#8affea', '#ff8a3d'];
      for (let k = 0; k <= 26; k++) { const u = k / 26, x = -900 + u * 1800, y = 760 - Math.sin(u * Math.PI * 3) ** 2 * 90; const b = sphere(13, glowM(cols[k % 5], 1.6), x, y, -700); b.castShadow = false; g.add(b); }
      [['#e8493f', -470, 0], ['#3f8fe8', -540, 0], ['#ffd23f', -505, 70], ['#57b85f', 470, 0], ['#ff8a3d', 540, 0]].forEach(([c, x, y]) => g.add(box(68, 68, 68, flat(c), x, y + 34 - 150, -200)));
      g.add(box(8000, 40, 2400, flat('#5b4a9a'), 0, -170, -400));
    },
  };

  let worldOn = false;
  function buildStage(st, plats) {
    if (!init()) return;
    clearGroup(stageGroup); platMeshes = []; rocks = [];
    worldOn = !!(st.theme && window.WORLDS && WORLDS.has(st.theme));
    if (worldOn) { // full 3D world (worlds.js)
      WORLDS.build(st, { scene, stageGroup, sun, renderer, camera, envTex });
      for (const p of plats) { const g = WORLDS.plat(st, p); stageGroup.add(g); platMeshes.push({ g, p }); }
      sun.target.position.set(0, 0, 0);
      return;
    }
    renderer.toneMappingExposure = 0.98; scene.environment = null;
    document.getElementById('sky').style.background = `linear-gradient(180deg, ${st.sky[0]} 0%, ${st.sky[1]} 60%, ${st.sky[2]} 100%)`;
    scene.fog = new THREE.Fog(new THREE.Color(st.sky[1]), 1900, 5200);
    const m = st.main, w = m.x2 - m.x1, cx = (m.x1 + m.x2) / 2, D = 230;
    const topMat = std(st.top, { roughness: 0.55 }), gMat = std(st.ground, { roughness: 0.9, flatShading: true });
    const glow = new THREE.MeshStandardMaterial({ color: st.trim, emissive: st.trim, emissiveIntensity: 0.9 });
    const slab = box(w + 6, 12, D, topMat, cx, -m.y - 6, 0); slab.receiveShadow = true; slab.castShadow = false; stageGroup.add(slab);
    stageGroup.add(box(w + 8, 3, 4, glow, cx, -m.y - 1.5, D / 2 + 1));
    stageGroup.add(box(8, 14, D + 4, glow, m.x1 - 1, -m.y - 6, 0), box(8, 14, D + 4, glow, m.x2 + 1, -m.y - 6, 0));
    const sh = new THREE.Shape(); const y0 = -m.y - 12, yd = -m.y - m.depth;
    if (st.theme) { sh.moveTo(m.x1, y0); sh.lineTo(m.x2, y0); sh.lineTo(m.x2, yd); sh.lineTo(m.x1, yd); sh.closePath(); } // themed stages are plain slabs (a deck, a table top, a mattress)
    else { sh.moveTo(m.x1, y0); sh.lineTo(m.x2, y0); sh.lineTo(m.x2, yd); sh.lineTo(m.x2 * 0.55, yd - 90); sh.lineTo(cx, yd - 150); sh.lineTo(m.x1 * 0.55, yd - 90); sh.lineTo(m.x1, yd); sh.closePath(); }
    const body = new THREE.Mesh(new THREE.ExtrudeGeometry(sh, { depth: D * 0.92, bevelEnabled: true, bevelSize: 6, bevelThickness: 6, bevelSegments: 1 }), gMat);
    body.position.z = -D * 0.46; body.receiveShadow = true; stageGroup.add(body);
    const strip = new THREE.MeshStandardMaterial({ color: st.top, emissive: st.top, emissiveIntensity: 0.55 });
    if (!st.theme) for (let k = -3; k <= 3; k++) stageGroup.add(box(4, 46 + Math.abs(k) * 8, 3, strip, cx + k * (w / 8), -m.y - 46 - Math.abs(k) * 3, D * 0.46 + 6));
    for (const p of plats) {
      const g = new THREE.Group();
      const pw = p.x2 - p.x1;
      const top = box(pw, 7, 120, topMat, 0, -3.5, 0); top.receiveShadow = true; g.add(top);
      g.add(box(pw + 2, 2, 3, glow, 0, -1, 61));
      g.add(box(pw * 0.5, 5, 60, gMat, 0, -9, 0));
      stageGroup.add(g); platMeshes.push({ g, p });
    }
    if (st.theme && DECOR[st.theme]) { const g = new THREE.Group(); DECOR[st.theme](g, m, w); g.traverse(o => { if (o.isMesh) o.receiveShadow = true; }); stageGroup.add(g); }
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
    else if (kind === 'gold') {
      g.font = '700 21px Arial, Helvetica, sans-serif'; g.fillStyle = '#b08a3a'; g.fillText('MAKE HEAVEN CROWDED', 128, 70);
    }
    if (kind === 'gold') { const t0 = new THREE.CanvasTexture(c); t0.colorSpace = THREE.SRGBColorSpace; return t0; }
    g.globalCompositeOperation = 'destination-out'; // worn print
    for (let k = 0; k < 300; k++) { g.fillStyle = 'rgba(0,0,0,' + (0.3 + Math.random() * 0.6) + ')'; g.fillRect(Math.random() * 256, Math.random() * 256, 1.5 + Math.random() * 2, 1.5 + Math.random() * 2); }
    const t = new THREE.CanvasTexture(c); t.colorSpace = THREE.SRGBColorSpace; return t;
  }
  // foam toy sword, pointing down its own -y axis; shown in whichever hand is nearest the camera
  function makeSword(def, side) {
    const sw = new THREE.Group(); sw.userData.blade = side; sw.position.y = -U - 2;
    sw.add(mesh(new THREE.CylinderGeometry(1.6, 1.6, 9, 8), std('#2b3566'), 0, 0, 0));
    sw.add(box(11, 2.4, 3.4, std('#ffd23f'), 0, -5, 0));
    const bl = mesh(new THREE.CapsuleGeometry(2.6, 34 * def.reach, 4, 10), std('#f4f6fa', { roughness: 0.55, emissive: '#9fc4ff', emissiveIntensity: 0.18 }), 0, -8 - 17 * def.reach, 0); bl.scale.z = 0.55; sw.add(bl);
    sw.rotation.x = -0.25; return sw;
  }
  const KIDS = {
    ray: { shirt: '#1c3fd6', shirtDk: '#1733b0', jeans: '#27304a', clog: '#1f9c9a', hole: '#0e5f5e', hair: '#4a3424', hoodie: true, print: 'varsity', sock: true },
    norah: { shirt: '#efe9dc', shirtDk: '#ddd5c4', jeans: '#f1c3cf', clog: '#ee7fa6', hole: '#ffffff', hair: '#b98a52', hoodie: true, print: 'gold', long: true, strand: '#a65fe0', sneaker: true, teeth: 'full', legR: 6.2, eye: '#4a2f1c' },
    ollie: { shirt: '#e3271d', shirtDk: '#bd1d15', jeans: '#5d7fb3', clog: '#2b3566', hole: '#161c3d', hair: '#54402f', hoodie: false, print: 'stars', sword: true },
  };
  function lookKid(def) {
    const o = KIDS[def.id];
    const skin = std('#e9b792', { roughness: 0.6 }), hair = std(o.hair, { roughness: 0.95 }), shirt = std(o.shirt, { roughness: 0.9 }),
      shirtDk = std(o.shirtDk, { roughness: 0.95 }), jeans = std(o.jeans, { roughness: 0.9 }), clog = std(o.clog, { roughness: 0.5 }),
      dark = std(o.eye || '#2a1c16'), white = std('#fbfbf6', { roughness: 0.4 }), lip = std('#b9584f'), blush = std('#f0a08c', { transparent: true, opacity: 0.35 });
    return {
      mats: [skin, hair, shirt, shirtDk, jeans, clog], sleeve: shirt, fore: o.hoodie ? shirt : skin, hand: skin, thigh: jeans, shin: jeans, armR: o.hoodie ? 4.8 : 4.1, legR: o.legR || 5.2,
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
        if (o.teeth === 'full') g.add(box(5.2, 1.5, 0.8, white, 0, cy - 5.5, 13.4));
        else g.add(box(2, 1.6, 0.8, white, -1.5, cy - 5.6, 13.5), box(2, 1.6, 0.8, white, 1.5, cy - 5.6, 13.5)); // front teeth with a gap
        if (o.long) { // long straight hair with a centre parting, and one sparkly coloured strand
          g.add(box(25, 36, 7, hair, 0, cy - 14, -10.5));
          for (const sd of [-1, 1]) { const lock = box(5.5, 34, 9, hair, sd * 13.2, cy - 12, -1.5); lock.rotation.z = sd * 0.05; g.add(lock); const fr = box(4, 26, 4, hair, sd * 11.6, cy - 7, 6.5); fr.rotation.z = sd * 0.1; g.add(fr); }
          g.add(box(1.6, 36, 1.6, std(o.strand, { emissive: o.strand, emissiveIntensity: 0.5, metalness: 0.4, roughness: 0.3 }), 13.4, cy - 13, 4));
        }
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
        if (o.hoodie && o.print === 'gold') for (const sd of [-1, 1]) g.add(mesh(new THREE.CylinderGeometry(0.6, 0.6, 11, 6), shirtDk, sd * 2.6, T - 9, R * 0.86 + 0.6)); // drawstrings
        const letters = mesh(new THREE.CylinderGeometry(R + 0.45, R + 0.45, ph, 16, 1, true, -0.8, 1.6), lm, 0, o.print === 'varsity' ? 17.5 : 14.5, 0); letters.scale.z = 0.86; letters.castShadow = false; g.add(letters);
      },
      foot(g) {
        if (o.sneaker) { // pink and white trainers
          const wh = std('#fbfbf8', { roughness: 0.5 });
          g.add(box(9.4, 1.8, 18, wh, 0, -L - 2.9, 4.2)); g.add(sphere(5.6, clog, 0, -L + 0.6, 3.6, 0.95, 0.82, 1.7));
          g.add(sphere(4.4, wh, 0, -L + 0.4, 10.5, 1, 0.7, 1)); g.add(box(4.4, 1, 7, wh, 0, -L + 4.6, 4.5));
          return;
        }
        g.add(sphere(5.6, clog, 0, -L + 0.6, 4.2, 1.08, 0.78, 1.85));
        const strap = mesh(new THREE.TorusGeometry(5.3, 1.1, 6, 14, Math.PI), clog, 0, -L + 1.5, -1.5); strap.rotation.y = Math.PI / 2; strap.rotation.x = -0.5; g.add(strap);
        const hole = std(o.hole);
        for (let i = 0; i < 6; i++) g.add(sphere(0.75, hole, (i % 3 - 1) * 2.6, -L + 4.6 - (i > 2 ? 0.5 : 0), 7 + (i > 2 ? 3.2 : 0)));
        if (o.sock) g.add(sphere(4.8, std('#f2f2ee'), 0, -L + 3, 0.5, 0.9, 0.6, 0.9));
      },
      hand(g, side) {
        if (o.hoodie) g.add(mesh(new THREE.CylinderGeometry(5.3, 5.3, 3.4, 12), shirtDk, 0, -U + 2.6, 0));
        g.add(sphere(4.3, skin, 0, -U - 1.6, 0));
        if (o.sword) g.add(makeSword(def, side));
      },
      extras() {},
    };
  }
  const LOOKS = { ray: lookKid, ollie: lookKid, norah: lookKid };

  // ---- scanned models (GLB with a skeleton). The sim's pose angles are turned into bone directions.
  const MODELS = {};
  const DRIVE = { LeftArm: 'LeftForeArm', LeftForeArm: 'LeftHand', RightArm: 'RightForeArm', RightForeArm: 'RightHand',
    LeftUpLeg: 'LeftLeg', LeftLeg: 'LeftFoot', RightUpLeg: 'RightLeg', RightLeg: 'RightFoot', Spine02: 'Spine' };
  function loadModels(done) {
    const src = window.CLASH_MODELS || {}, ids = Object.keys(src); let left = ids.length;
    if (!left || !window.THREE || !THREE.GLTFLoader) { if (done) done(); return; }
    const loader = new THREE.GLTFLoader();
    const fin = () => { if (--left === 0 && done) done(); };
    for (const id of ids) {
      try {
        const str = atob(src[id]), buf = new Uint8Array(str.length); for (let i = 0; i < str.length; i++) buf[i] = str.charCodeAt(i);
        loader.parse(buf.buffer, '', g => { MODELS[id] = g.scene; delete pCache[id]; fin(); }, e => { console.warn('model failed', id, e); fin(); });
      } catch (e) { console.warn('model failed', id, e); fin(); }
    }
  }
  function buildGlbRig(def) {
    const model = THREE.SkeletonUtils.clone(MODELS[def.id]);
    const rig = { def, glb: true, turnY: TURN, blades: [], look: { mats: [] } };
    model.traverse(o => { if (o.isMesh) { o.castShadow = true; o.frustumCulled = false; o.material = o.material.clone(); o.material.side = THREE.FrontSide; o.material.metalness = 0; o.material.roughness = 0.85; if (o.material.map && renderer) o.material.map.anisotropy = renderer.capabilities.getMaxAnisotropy(); rig.look.mats.push(o.material); } });
    model.updateMatrixWorld(true);
    const bones = {}, order = [];
    model.traverse(o => { if (o.isBone) { bones[o.name] = o; order.push(o); } });
    const P = n => bones[n].getWorldPosition(new THREE.Vector3());
    const box = new THREE.Box3().setFromObject(model), height = box.max.y - box.min.y;
    const k = (86 * def.scale) / height;
    rig.rest = order.map(b => ({ b, name: b.name, parent: b.parent && b.parent.isBone ? b.parent : null, local: b.quaternion.clone(),
      target: b.quaternion.clone(), world: b.getWorldQuaternion(new THREE.Quaternion()), dir: DRIVE[b.name] && bones[DRIVE[b.name]] ? P(DRIVE[b.name]).sub(P(b.name)).normalize() : null }));
    const hips = P('Hips');
    rig.thigh = P('LeftUpLeg').distanceTo(P('LeftLeg')) * k; rig.shin = P('LeftLeg').distanceTo(P('LeftFoot')) * k;
    rig.ankle = (P('LeftFoot').y - box.min.y) * k; rig.hipRest = (hips.y - box.min.y) * k; rig.hipDrop = (hips.y - P('LeftUpLeg').y) * k;
    const root = new THREE.Group(), spin = new THREE.Group(), turn = new THREE.Group();
    root.add(spin); spin.add(turn); turn.add(model);
    model.scale.setScalar(k); model.position.set(-hips.x * k, -hips.y * k, -hips.z * k);
    rig.root = root; rig.spin = spin; rig.turn = turn; rig.model = model; rig.bones = bones;
    rig.byName = {}; for (const r of rig.rest) rig.byName[r.name] = r;
    rig.wq = new Map(); rig.unit = 0.01 * k; // clip hips offsets are in the skeleton's centimetres
    rig.clips = clipsFor(def.id);
    if (rig.clips && !rig.clips.idle.impact && rig.clips.idle.impact !== 0) findImpacts(rig, rig.clips);
    if (KIDS[def.id] && KIDS[def.id].sword) for (const side of ['L', 'R']) {
      const hand = bones[side === 'L' ? 'LeftHand' : 'RightHand'], fore = side === 'L' ? 'LeftForeArm' : 'RightForeArm'; if (!hand) continue;
      const dir = P(hand.name).sub(P(fore)).normalize().applyQuaternion(hand.getWorldQuaternion(new THREE.Quaternion()).invert());
      const holder = new THREE.Group(); holder.quaternion.setFromUnitVectors(new THREE.Vector3(0, -1, 0), dir);
      const sw = makeSword(def, side); sw.position.y = 2; sw.rotation.x = 0; holder.add(sw); holder.scale.setScalar(def.scale / (k * 0.01)); holder.userData.sword = side;
      hand.add(holder); rig.blades.push(holder);
    }
    return rig;
  }
  const _q = new THREE.Quaternion(), _q2 = new THREE.Quaternion(), _v = new THREE.Vector3(), IDENT = new THREE.Quaternion();

  // ---- motion clips (Meshy animation library, stored as sampled bone rotations in anims.js)
  const CLIPS = {}; // char id -> clip name -> { n, dur, bones, rot(Int16Array), hy, impact }
  function clipsFor(id) {
    if (CLIPS[id]) return CLIPS[id];
    const src = (window.CLASH_ANIMS || {})[id]; if (!src) return (CLIPS[id] = null);
    const out = {};
    for (const name in src) {
      const c = src[name], str = atob(c.rot), u8 = new Uint8Array(str.length); for (let i = 0; i < str.length; i++) u8[i] = str.charCodeAt(i);
      out[name] = { n: c.n, dur: c.dur, bones: c.bones, rot: new Int16Array(u8.buffer), hy: c.hy || null, impact: null };
    }
    return (CLIPS[id] = out);
  }
  // write one clip frame (blended between two samples) into each bone's target; returns the hips height offset
  function sampleClip(rig, clip, time, loop) {
    let t = loop ? ((time % clip.dur) + clip.dur) % clip.dur : clamp(time, 0, clip.dur);
    const fpos = (t / clip.dur) * (clip.n - 1), i0 = Math.floor(fpos), i1 = Math.min(clip.n - 1, i0 + 1), w = fpos - i0, nb = clip.bones.length, R = clip.rot;
    for (let b = 0; b < nb; b++) {
      const r = rig.byName[clip.bones[b]]; if (!r) continue;
      const o0 = (i0 * nb + b) * 4, o1 = (i1 * nb + b) * 4;
      _q.set(R[o0], R[o0 + 1], R[o0 + 2], R[o0 + 3]).normalize(); _q2.set(R[o1], R[o1 + 1], R[o1 + 2], R[o1 + 3]).normalize();
      r.target.copy(_q).slerp(_q2, w);
    }
    return clip.hy ? clip.hy[i0] * (1 - w) + clip.hy[i1] * w : 0;
  }
  // the moment of furthest reach in each clip, so a punch lands on the same frame the game's hitbox does
  function findImpacts(rig, clips) {
    const ext = ['LeftHand', 'RightHand', 'LeftFoot', 'RightFoot'].map(n => rig.bones[n]).filter(Boolean), hips = rig.bones.Hips;
    const a = new THREE.Vector3(), h = new THREE.Vector3();
    for (const name in clips) {
      const c = clips[name]; let best = -1, bi = 0; const first = [];
      for (let i = 0; i < c.n; i++) {
        sampleClip(rig, c, (i / (c.n - 1)) * c.dur, false);
        for (const r of rig.rest) r.b.quaternion.copy(r.target);
        rig.model.updateMatrixWorld(true); hips.getWorldPosition(h);
        let m = 0;
        ext.forEach((e, k) => { e.getWorldPosition(a).sub(h); if (i === 0) first[k] = a.clone(); else m = Math.max(m, a.distanceTo(first[k])); });
        if (m > best) { best = m; bi = i; }
      }
      c.impact = (bi / (c.n - 1)) * c.dur;
    }
    for (const r of rig.rest) { r.b.quaternion.copy(r.local); r.target.copy(r.local); }
  }
  const MOVE_CLIP = { jab1: 'jab', jab2: 'jab2', jab3: 'round', ftilt: 'kick', utilt: 'upper', dtilt: 'sweep', dash: 'kick', fsmash: 'smash', usmash: 'highkick', dsmash: 'sweep',
    nair: 'spin', fair: 'airpunch', bair: 'round', uair: 'highkick', dair: 'slam', grab: 'grab', dashgrab: 'grab', throw: 'pitch', ledgeAtk: 'sweep', getupAtk: 'sweep', counterStrike: 'smash' };
  const CHAR_CLIP = {
    ray: { nspec: 'kick', uspec: 'jump', dspec: 'slam' },
    norah: { nspec: 'pitch', sspec: 'airpunch', uspec: 'jump', dspec: 'kick', run: 'sprint' },
    tyler: { nspec: 'smash', sspec: 'airpunch', uspec: 'jump', dspec: 'block' },
    violet: { nspec: 'cast', sspec: 'airpunch', uspec: 'cast', dspec: 'block', jab3: 'highkick', fsmash: 'round' },
    layne: { nspec: 'smash', sspec: 'airpunch', uspec: 'jump', dspec: 'slam' },
    dad: { nspec: 'cast', sspec: 'airpunch', uspec: 'jump', dspec: 'slam' },
    mom: { nspec: 'cast', sspec: 'pitch', uspec: 'jump', dspec: 'block' },
    jayson: { nspec: 'cast', uspec: 'jump', dspec: 'kick' },
    ollie: { jab1: 'slash', jab2: 'attack', jab3: 'thrust', ftilt: 'slash', utilt: 'upslash', dash: 'thrust', fsmash: 'attack', usmash: 'upslash', nair: 'bladespin', fair: 'slash', uair: 'upslash',
      nspec: 'bladespin', sspec: 'thrust', uspec: 'bladespin', dspec: 'block', counterStrike: 'attack' },
  };
  // which clip (and what time in it) shows the fighter's current state; null = use the hand-posed fallback
  function pickClip(rig, f) {
    const C = rig.clips; if (!C || !f) return null;
    const own = CHAR_CLIP[rig.def.id] || {}, S = f.state, t = f.t / 60;
    const get = n => C[own[n] || n] || C[n];
    const hold = (c, at) => ({ c, time: at, loop: false });
    if (G.ending && f.stocks > 0 && f.grounded && S === 'idle') return { c: C.victory, time: G.ending / 60, loop: true };
    switch (S) {
      case 'idle': case 'respawn': case 'shieldDrop': return { c: C.idle, time: f.anim / 60, loop: true };
      case 'walk': return { c: C.walk, time: f.anim / 60 * 1.3, loop: true };
      case 'run': return { c: get('run'), time: f.anim / 60 * 1.15, loop: true };
      case 'air': if (f.flipT > 0) return null; return f.vy < -1 ? hold(C.jump, C.jump.dur * 0.42) : { c: C.fall, time: f.anim / 60, loop: true, air: true };
      case 'helpless': return { c: C.fall, time: f.anim / 60, loop: true, air: true };
      case 'hitstun': return f.tumble ? { c: C.fly, time: C.fly.dur * 0.45, loop: false, air: true, keepRot: true } : hold(C.hit, Math.min(C.hit.dur * 0.6, 0.15 + t));
      case 'shield': case 'shieldstun': case 'spotdodge': return hold(C.block, Math.min(C.block.impact || 0.5, 0.2 + t));
      case 'roll': case 'tech': return hold(C.roll, (f.t / 30) * C.roll.dur);
      case 'down': return hold(C.knock, C.knock.dur);
      case 'getup': return hold(C.standup, C.standup.dur - 0.9 + (f.t / 22) * 0.9);
      case 'holding': return hold(C.grab, C.grab.impact || 1);
      case 'grabbed': return hold(C.hit, C.hit.dur * 0.3);
      case 'attack': {
        const mv = f.move, c = get(mv.name) || C[MOVE_CLIP[mv.name]]; const cc = C[own[mv.name]] || C[MOVE_CLIP[mv.name]];
        const clip = cc || null; if (!clip || mv.rollSpin) return null;
        const hbs = mv.hb.concat(f.xhb || []);
        const hs = Math.max(2, hbs.length ? Math.min.apply(null, hbs.map(h => h.f[0])) : mv.len * 0.3);
        const imp = clip.impact != null ? clip.impact : clip.dur * 0.4, lead = Math.min(imp, 0.45);
        const time = f.mt <= hs ? imp - lead + lead * (f.mt / hs) : imp + (f.mt - hs) / 60;
        return { c: clip, time, loop: false, air: !f.grounded };
      }
    }
    return null;
  }

  function poseGlb(rig, pose, rot, facing, f) {
    const [lean, afS, afE, abS, abE, lfH, lfK, lbH, lbK] = pose;
    const nL = facing < 0; // the left side is nearest the camera when facing left
    for (const b of rig.blades) b.visible = b.userData.sword === (nL ? 'L' : 'R');
    const pick = pickClip(rig, f) || (!f && rig.clips && rig.clips.idle ? { c: rig.clips.idle, time: 0.4, loop: true } : null);
    let hip = rig.hipRest;
    if (pick && pick.c) {
      for (const r of rig.rest) r.target.copy(r.local);
      const hy = sampleClip(rig, pick.c, pick.time, pick.loop);
      if (!pick.air) hip = clamp(rig.hipRest + hy * rig.unit, rig.hipRest * 0.18, rig.hipRest * 1.6);
      rig.spin.rotation.z = pick.keepRot || (f && f.state === 'air') ? -rot : 0;
    } else {
      const ang = { LeftArm: nL ? afS : abS, LeftForeArm: nL ? afS + afE : abS + abE, RightArm: nL ? abS : afS, RightForeArm: nL ? abS + abE : afS + afE,
        LeftUpLeg: nL ? lfH : lbH, LeftLeg: nL ? lfH + lfK : lbH + lbK, RightUpLeg: nL ? lbH : lfH, RightLeg: nL ? lbH + lbK : lfH + lfK };
      // hip height from the real leg lengths, so the feet meet the ground
      const fd = (h, kk) => rig.thigh * Math.cos(h) + rig.shin * Math.cos(h + kk);
      if (f && (f.grounded || f.state === 'ledgeclimb')) hip = clamp(Math.max(fd(lfH, lfK), fd(lbH, lbK)) + rig.ankle + rig.hipDrop, rig.hipRest * 0.4, rig.hipRest);
      rig.spin.rotation.z = -rot;
      const wq = rig.wq;
      for (const r of rig.rest) {
        const pq = r.parent ? wq.get(r.parent) : IDENT; let q;
        if (r.name === 'Spine02' && r.dir) { _v.set(0, Math.cos(lean), Math.sin(lean)); q = _q.setFromUnitVectors(r.dir, _v).clone().multiply(r.world); }
        else if (r.name in ang && r.dir) {
          const a = ang[r.name], side = r.name.startsWith('Left') ? 1 : -1, arm = r.name.indexOf('Arm') >= 0;
          _v.set(side * (arm ? 0.22 : 0.07), -Math.cos(a), Math.sin(a)).normalize();
          q = _q.setFromUnitVectors(r.dir, _v).clone().multiply(r.world);
        } else if (r.name === 'Head') { _q.setFromAxisAngle(_v.set(1, 0, 0), lean * 0.35); q = _q.clone().multiply(r.world); }
        else q = pq.clone().multiply(r.local);
        wq.set(r.b, q); r.target.copy(pq).invert().multiply(q);
      }
    }
    // ease every bone toward its target so moves flow into each other
    const k = f ? 0.42 : 1;
    rig.hipNow = rig.hipNow == null || !f ? hip : rig.hipNow + (hip - rig.hipNow) * 0.5;
    rig.spin.position.y = rig.hipNow;
    for (const r of rig.rest) r.b.quaternion.slerp(r.target, k);
  }

  function buildRig(def) {
    if (MODELS[def.id]) return buildGlbRig(def);
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
    if (!rig.glb && f.state === 'attack' && f.move && f.move.twirl && f.mt > 4 && f.mt < f.move.len - 8) rig.turn.rotation.y = rig.turnY + f.mt * 0.75;
    if (rig.glb) poseGlb(rig, pose, rot, f.facing, f); else poseRig(rig, pose, rot, f.facing, a.hipH, G.frame);
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
    fighterGroup.visible = true; if (menuGroup) menuGroup.visible = false;
    for (const pm of platMeshes) pm.g.position.set((pm.p.x1 + pm.p.x2) / 2, -pm.p.y, 0);
    for (const r of rocks) r.position.y = r.userData.by + Math.sin(G.frame * 0.006 + r.userData.ph) * 18;
    for (const f of G.fighters) syncFighter(f);
    if (worldOn) WORLDS.update();
    const s = camScale(), d = VH / (2 * s * Math.tan((FOV * DEG) / 2));
    const cx = G.cam.x - shx / s, cy = -G.cam.y + shy / s;
    // the camera sits a little above the action and looks down, so the stages read as 3D spaces
    camera.aspect = VW / VH; camera.far = d + 12000; camera.position.set(cx, cy + d * (worldOn ? 0.11 : 0), d); camera.lookAt(cx, cy, 0); camera.updateProjectionMatrix();
    present();
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
        const rig = buildRig(def); rig.turn.rotation.y = rig.glb ? 0.35 : 0.5;
        if (rig.glb) { poseGlb(rig, POSES.idle, 0, 1, null); rig.root.position.y = 0; } else poseRig(rig, POSES.idle, 0, 1, 33 * def.scale, 20);
        pScene.add(rig.root);
        const h = (rig.glb ? 86 : 100) * def.scale; pCamera.position.set(0, h * 0.52, h * 2.5); pCamera.lookAt(0, h * 0.5, 0);
        pRenderer.render(pScene, pCamera);
        const copy = document.createElement('canvas'); copy.width = 800; copy.height = 480; copy.getContext('2d').drawImage(pRenderer.domElement, 0, 0);
        pCache[def.id] = copy; pScene.remove(rig.root);
      } catch (e) { console.warn('portrait failed', e); return; }
    }
    c2.drawImage(pCache[def.id], 0, 0, canvas.width, canvas.height);
  }

  // ------------------------------------------------------------ menu scene: live fighters behind the menus
  let menuGroup = null, menuList = [], menuCamT = null, menuT = 0;
  const menuRigs = new Map(), menuCam = { x: 0, y: 110, d: 1000, lx: 0, ly: 70 };
  // list: [{ key, id, x, z, face, scale, once }]; cam: { x, y, d, lx, ly } eased toward
  function menuSet(list, cam) {
    if (!init()) return;
    if (!menuGroup) {
      menuGroup = new THREE.Group(); scene.add(menuGroup);
      // a boxing-style ring around the stage: corner posts and three glowing ropes on each side
      const post = std('#2a2f55', { metalness: 0.4, roughness: 0.4 }), cols = ['#ff5ea8', '#b48cff', '#6cc8ff'];
      const X = 330, Z = 105;
      for (const sx of [-1, 1]) for (const sz of [-1, 1]) { const p = mesh(new THREE.CylinderGeometry(6, 7, 96, 10), post, sx * X, 48, sz * Z); menuGroup.add(p); menuGroup.add(sphere(9, std('#ffd23f', { emissive: '#ffb347', emissiveIntensity: 0.7 }), sx * X, 100, sz * Z)); }
      cols.forEach((c, i) => { const m = new THREE.MeshStandardMaterial({ color: c, emissive: c, emissiveIntensity: 0.9 }), y = 34 + i * 26;
        { const r = mesh(new THREE.CylinderGeometry(1.8, 1.8, X * 2, 6), m, 0, y, -Z); r.rotation.z = Math.PI / 2; r.castShadow = false; menuGroup.add(r); }
        for (const sx of [-1, 1]) { const r = mesh(new THREE.CylinderGeometry(1.8, 1.8, Z * 2, 6), m, sx * X, y, 0); r.rotation.x = Math.PI / 2; r.castShadow = false; menuGroup.add(r); } });
    }
    if (!stageGroup.children.length) buildStage(STAGES.skyforge, []);
    const want = new Set(list.map(e => e.key || e.id));
    for (const [k, m] of menuRigs) m.rig.root.visible = want.has(k);
    menuList = [];
    for (const e of list) {
      const key = e.key || e.id, def = FIGHTERS.find(f => f.id === e.id); if (!def) continue;
      let m = menuRigs.get(key);
      if (!m || m.id !== e.id || m.glb !== !!MODELS[e.id]) {
        if (m) menuGroup.remove(m.rig.root);
        const rig = buildRig(def); menuGroup.add(rig.root);
        m = { rig, id: e.id, glb: !!rig.glb, x: e.x, t: Math.random() * 3, once: null, onceT: 0 }; menuRigs.set(key, m);
      }
      m.rig.root.visible = true; m.tx = e.x; m.z = e.z || 0; m.face = e.face == null ? 0.25 : e.face; m.scale = e.scale || 1;
      if (e.once) { m.once = e.once; m.onceT = 0; }
      menuList.push(m);
    }
    if (cam) menuCamT = cam;
  }
  function menuReact(key, clip) { const m = menuRigs.get(key); if (m && m.rig.root.visible) { m.once = clip; m.onceT = 0; return m.id; } return null; }
  function menuRender() {
    if (!ok || !menuGroup) return false;
    show(true); fighterGroup.visible = false; menuGroup.visible = true; menuT++;
    for (const m of menuList) {
      const rig = m.rig; m.x += (m.tx - m.x) * 0.14; m.t += 1 / 60;
      rig.root.position.set(m.x, 0, m.z); rig.root.scale.setScalar(rig.glb ? m.scale : rig.def.scale * m.scale);
      rig.turn.rotation.y += (m.face - rig.turn.rotation.y) * 0.15;
      if (rig.glb && rig.clips) {
        let c = rig.clips.idle, time = m.t, loop = true;
        const oc = m.once && rig.clips[m.once];
        if (oc) { m.onceT += 1 / 60; if (m.onceT < Math.min(oc.dur, 2.4)) { c = oc; time = m.onceT; loop = false; } else m.once = null; } else m.once = null;
        for (const r of rig.rest) r.target.copy(r.local);
        const hy = sampleClip(rig, c, time, loop);
        rig.spin.position.y = clamp(rig.hipRest + hy * rig.unit, rig.hipRest * 0.3, rig.hipRest * 1.6); rig.spin.rotation.z = 0;
        for (const r of rig.rest) r.b.quaternion.slerp(r.target, 0.3);
        for (const b of rig.blades) b.visible = b.userData.sword === 'R';
      } else poseRig(rig, POSES.idle, 0, 1, 33 * rig.def.scale, menuT);
    }
    if (menuCamT) for (const k in menuCamT) menuCam[k] += (menuCamT[k] - menuCam[k]) * 0.07;
    const sway = Math.sin(menuT * 0.006) * 22;
    camera.aspect = innerWidth / innerHeight; camera.far = 14000;
    camera.position.set(menuCam.x + sway, menuCam.y, menuCam.d); camera.lookAt(menuCam.lx, menuCam.ly, 0); camera.updateProjectionMatrix();
    renderer.render(scene, camera); return true;
  }

  // world positions (sim coordinates) of a fighter's hands, feet and sword tip, for motion trails
  const _lp = new THREE.Vector3();
  function limbPoints(f) {
    const rig = rigs.get(f); if (!rig || !rig.glb || !rig.root.visible) return null;
    const out = {};
    for (const n of ['LeftHand', 'RightHand', 'LeftFoot', 'RightFoot']) { const b = rig.bones[n]; if (b) { b.getWorldPosition(_lp); out[n] = { x: _lp.x, y: -_lp.y }; } }
    for (const h of rig.blades) if (h.visible) { _lp.set(0, -(12 + 34 * rig.def.reach), 0); h.localToWorld(_lp); out.Blade = { x: _lp.x, y: -_lp.y }; }
    return out;
  }
  return { init, buildStage, resetRigs, render, portrait, show, loadModels, limbPoints, menuSet, menuReact, menuRender, get ok() { return ok; } };
})();
