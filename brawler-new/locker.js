'use strict';
// The Locker. Every fighter has their own unlockable outfits, hats, back items and weapons, bought
// with Stars earned in matches and story mode. Everything is built in 3D and fitted to the scanned fighter models' bones,
// and shows in matches, menus and the Locker's spinning preview. Weapons give a little extra reach.
(() => {
  const T = window.THREE; if (!T) return;
  // ------------------------------------------------------------ catalog
  const CAT = {
    outfit: { label: 'OUTFIT', items: [
      { id: 'classic', name: 'Classic', icon: '👕', price: 0 },
      { id: 'shadow', name: 'Shadow', icon: '🌑', price: 150, tint: '#6a6a8e', rough: 0.6, em: '#2a1a5a', ei: 0.25 },
      { id: 'frost', name: 'Frost', icon: '❄️', price: 200, tint: '#cfeaff', metal: 0.25, rough: 0.25, em: '#4aa8ff', ei: 0.18 },
      { id: 'silver', name: 'Chrome', icon: '🪩', price: 220, tint: '#e6ecf5', metal: 0.85, rough: 0.22, env: 1.2 },
      { id: 'lava', name: 'Lava', icon: '🌋', price: 250, tint: '#ffb08a', em: '#ff3a00', ei: 0.35, rough: 0.5 },
      { id: 'neon', name: 'Neon', icon: '💜', price: 260, tint: '#ffffff', em: 'ACCENT', ei: 0.45 },
      { id: 'gold', name: 'Gold Statue', icon: '🏆', price: 400, tint: '#ffcf6a', metal: 0.88, rough: 0.26, env: 1.3 },
    ] },
    hat: { label: 'HAT', items: [
      { id: 'none', name: 'No Hat', icon: '🚫', price: 0 }, { id: 'party', name: 'Party Hat', icon: '🥳', price: 60 }, { id: 'cap', name: 'Ball Cap', icon: '🧢', price: 80 },
      { id: 'cowboy', name: 'Cowboy Hat', icon: '🤠', price: 120 }, { id: 'wizard', name: 'Wizard Hat', icon: '🧙', price: 160 }, { id: 'viking', name: 'Viking Helm', icon: '⚔️', price: 180 },
      { id: 'crown', name: 'Royal Crown', icon: '👑', price: 300 },
    ] },
    back: { label: 'BACK', items: [
      { id: 'none', name: 'Nothing', icon: '🚫', price: 0 }, { id: 'cape', name: 'Hero Cape', icon: '🦸', price: 150 }, { id: 'jetpack', name: 'Jetpack', icon: '🚀', price: 250 },
      { id: 'angel', name: 'Angel Wings', icon: '🪽', price: 300 }, { id: 'dragon', name: 'Dragon Wings', icon: '🐉', price: 350 },
    ] },
    weapon: { label: 'WEAPON', items: [
      { id: 'none', name: 'Bare Hands', icon: '✊', price: 0 }, { id: 'pan', name: 'Frying Pan', icon: '🍳', price: 90 }, { id: 'foam', name: 'Foam Sword', icon: '🗡️', price: 100 },
      { id: 'hockey', name: 'Hockey Stick', icon: '🏒', price: 120 }, { id: 'hammer', name: 'Toy Hammer', icon: '🔨', price: 150 }, { id: 'wand', name: 'Magic Wand', icon: '🪄', price: 200 },
      { id: 'guitar', name: 'Rock Guitar', icon: '🎸', price: 220 }, { id: 'laser', name: 'Laser Sword', icon: '⚡', price: 300 },
    ] },
  };
  const find = (cat, id) => CAT[cat].items.find(i => i.id === id) || CAT[cat].items[0];
  // ------------------------------------------------------------ saved state
  const KEY = 'ncg_locker'; // own save key, so this game never reads the family game's saves
  let S = (() => { try { return JSON.parse(localStorage.getItem(KEY)) || null; } catch (e) { return null; } })() || { stars: 300, own: {}, eq: {} };
  const persist = () => { try { localStorage.setItem(KEY, JSON.stringify(S)); } catch (e) {} };
  const eqOf = id => Object.assign({ outfit: 'classic', hat: 'none', back: 'none', weapon: 'none' }, S.eq[id] || {});
  const owns = (id, cat, item) => find(cat, item).price === 0 || !!(S.own[id] && S.own[id][cat + ':' + item]);

  // ------------------------------------------------------------ 3D pieces (sizes in world units; s = the fighter's scale)
  const M = (c, o) => new T.MeshStandardMaterial(Object.assign({ color: c, roughness: 0.5, metalness: 0 }, o || {}));
  const mesh = (geo, mat, x = 0, y = 0, z = 0) => { const m = new T.Mesh(geo, mat); m.position.set(x, y, z); m.castShadow = true; return m; };
  function hat(type, s, col) {
    const g = new T.Group(), r = 5.6 * s;
    if (type === 'crown') {
      const gold = M('#ffd23f', { metalness: 0.85, roughness: 0.25, emissive: '#8a5a00', emissiveIntensity: 0.25 });
      g.add(mesh(new T.CylinderGeometry(r * 0.92, r * 0.86, r * 0.55, 24, 1, true), Object.assign(gold.clone(), { side: T.DoubleSide }), 0, r * 0.1, 0));
      for (let k = 0; k < 6; k++) { const a = k / 6 * Math.PI * 2; g.add(mesh(new T.ConeGeometry(r * 0.16, r * 0.55, 8), gold, Math.cos(a) * r * 0.86, r * 0.6, Math.sin(a) * r * 0.86)); g.add(mesh(new T.SphereGeometry(r * 0.09, 10, 8), M(k % 2 ? '#ff3b6b' : '#3bb8ff', { emissive: k % 2 ? '#ff3b6b' : '#3bb8ff', emissiveIntensity: 0.6, roughness: 0.1 }), Math.cos(a) * r * 0.9, r * 0.12, Math.sin(a) * r * 0.9)); }
    } else if (type === 'cap') {
      const m = M(col, { roughness: 0.8 }); g.add(mesh(new T.SphereGeometry(r * 1.02, 24, 12, 0, Math.PI * 2, 0, Math.PI / 2), m, 0, -r * 0.15, 0));
      const brim = mesh(new T.CylinderGeometry(r * 0.9, r * 0.9, r * 0.08, 24, 1, false, -Math.PI / 2, Math.PI), m, 0, -r * 0.12, r * 0.55); brim.scale.z = 1.3; g.add(brim);
      g.add(mesh(new T.SphereGeometry(r * 0.12, 8, 6), M('#ffffff'), 0, r * 0.86, 0));
    } else if (type === 'wizard') {
      const m = M('#5a2ad6', { roughness: 0.7, emissive: '#2a0a6a', emissiveIntensity: 0.3 });
      const cone = mesh(new T.ConeGeometry(r * 1.05, r * 3.2, 24), m, 0, r * 1.5, -r * 0.1); cone.rotation.x = -0.18; g.add(cone);
      g.add(mesh(new T.CylinderGeometry(r * 1.9, r * 1.9, r * 0.1, 32), m, 0, -r * 0.05, 0));
      for (let k = 0; k < 4; k++) g.add(mesh(new T.SphereGeometry(r * 0.12, 8, 6), M('#ffd23f', { emissive: '#ffd23f', emissiveIntensity: 1 }), Math.cos(k * 1.7) * r * 0.6, r * (0.5 + k * 0.5), Math.sin(k * 1.7) * r * 0.6 + r * 0.2));
    } else if (type === 'viking') {
      const steel = M('#b8c0cc', { metalness: 0.8, roughness: 0.3 }), hornM = M('#fff3dc', { roughness: 0.4 });
      g.add(mesh(new T.SphereGeometry(r * 1.05, 24, 12, 0, Math.PI * 2, 0, Math.PI / 2), steel, 0, -r * 0.2, 0));
      g.add(mesh(new T.TorusGeometry(r * 1.02, r * 0.1, 8, 28), M('#c99a3a', { metalness: 0.7, roughness: 0.3 }), 0, -r * 0.18, 0)).rotation.x = Math.PI / 2;
      for (const sx of [-1, 1]) { const h = mesh(new T.ConeGeometry(r * 0.28, r * 1.6, 12), hornM, sx * r * 1.15, r * 0.45, 0); h.rotation.z = -sx * 0.9; g.add(h); }
    } else if (type === 'party') {
      const c = document.createElement('canvas'); c.width = 64; c.height = 64; const x = c.getContext('2d'); for (let k = 0; k < 8; k++) { x.fillStyle = ['#ff4d8d', '#ffd23f', '#4dd9ff', '#8affea'][k % 4]; x.fillRect(0, k * 8, 64, 8); }
      const t = new T.CanvasTexture(c); t.colorSpace = T.SRGBColorSpace;
      const cone = mesh(new T.ConeGeometry(r * 0.7, r * 2.2, 24), M('#ffffff', { map: t }), r * 0.2, r * 0.9, 0); cone.rotation.z = -0.25; g.add(cone);
      g.add(mesh(new T.SphereGeometry(r * 0.25, 10, 8), M('#ffffff', { roughness: 1 }), r * 0.5, r * 2.0, 0));
    } else if (type === 'cowboy') {
      const m = M('#8a5a2e', { roughness: 0.85 }); g.add(mesh(new T.CylinderGeometry(r * 0.85, r * 0.95, r * 1.05, 24), m, 0, r * 0.35, 0));
      const brim = mesh(new T.CylinderGeometry(r * 2.0, r * 2.0, r * 0.1, 32), m, 0, -r * 0.12, 0); g.add(brim);
      g.add(mesh(new T.CylinderGeometry(r * 0.97, r * 0.97, r * 0.2, 24), M('#2a1a0a'), 0, 0.02, 0));
    }
    return g;
  }
  function back(type, s, accent) {
    const g = new T.Group(), u = s;
    if (type === 'cape') {
      const geo = new T.PlaneGeometry(13 * u, 30 * u, 8, 14), p = geo.attributes.position;
      for (let i = 0; i < p.count; i++) { const x = p.getX(i), y = p.getY(i), v = (15 * u - y) / (30 * u); p.setXYZ(i, x * (0.75 + v * 0.5), y - 15 * u, -v * v * 9 * u + Math.sin(x * 0.3) * 0.6 * u); }
      geo.computeVertexNormals(); const cape = mesh(geo, M('#c8102e', { side: T.DoubleSide, roughness: 0.6, emissive: '#3a0008', emissiveIntensity: 0.3 })); cape.userData.cape = true; g.add(cape);
      g.add(mesh(new T.TorusGeometry(5.5 * u, 0.6 * u, 8, 20, Math.PI), M('#ffd23f', { metalness: 0.8, roughness: 0.25 }), 0, 0, 0)).rotation.set(Math.PI / 2, 0, 0);
    } else if (type === 'jetpack') {
      const m = M('#c9ced6', { metalness: 0.75, roughness: 0.3 }), red = M('#ff3b3b', { roughness: 0.4 });
      for (const sx of [-1, 1]) { g.add(mesh(new T.CylinderGeometry(3 * u, 3 * u, 14 * u, 16), m, sx * 3.5 * u, -6 * u, -4 * u)); g.add(mesh(new T.ConeGeometry(3 * u, 3.5 * u, 16), red, sx * 3.5 * u, 2.7 * u, -4 * u));
        const fl = mesh(new T.ConeGeometry(2.2 * u, 9 * u, 12, 1, true), M('#ffd23f', { emissive: '#ff8a1e', emissiveIntensity: 2, transparent: true, opacity: 0.85, side: T.DoubleSide }), sx * 3.5 * u, -17 * u, -4 * u); fl.rotation.x = Math.PI; fl.userData.flame = true; g.add(fl); }
    } else if (type === 'angel' || type === 'dragon') {
      const sh = new T.Shape(); sh.moveTo(0, 0);
      if (type === 'angel') { sh.bezierCurveTo(6, 10, 20, 22, 34, 24); sh.bezierCurveTo(30, 16, 32, 10, 28, 6); sh.bezierCurveTo(24, 4, 26, -2, 20, -4); sh.bezierCurveTo(14, -8, 8, -6, 0, 0); }
      else { sh.bezierCurveTo(6, 14, 18, 26, 34, 28); sh.lineTo(28, 18); sh.lineTo(32, 10); sh.lineTo(24, 8); sh.lineTo(26, 0); sh.lineTo(12, 2); sh.lineTo(0, 0); }
      const m = type === 'angel' ? M('#ffffff', { side: T.DoubleSide, emissive: '#fff3d0', emissiveIntensity: 0.45, roughness: 0.8 }) : M('#b43ad8', { side: T.DoubleSide, emissive: '#5a1a6a', emissiveIntensity: 0.5, roughness: 0.4 });
      for (const sx of [-1, 1]) { const w = mesh(new T.ShapeGeometry(sh, 16), m, sx * 2 * u, 0, -5 * u); w.scale.set(sx * u, u, u); w.rotation.y = sx * -0.5; w.userData.wing = sx; g.add(w); }
    }
    return g;
  }
  function weapon(type, s, accent) {
    const g = new T.Group(), u = s, handle = M('#3a2a1e', { roughness: 0.8 });
    if (type === 'foam') { g.add(mesh(new T.CylinderGeometry(0.9 * u, 0.9 * u, 6 * u, 10), M('#2f7dff'), 0, 1 * u)); g.add(mesh(new T.BoxGeometry(7 * u, 1.2 * u, 2 * u), M('#ffd23f'), 0, 4.2 * u)); g.add(mesh(new T.BoxGeometry(2.6 * u, 26 * u, 1.2 * u), M('#ff8a1e', { roughness: 0.9 }), 0, 17.5 * u)); }
    else if (type === 'hammer') { g.add(mesh(new T.CylinderGeometry(0.8 * u, 0.9 * u, 20 * u, 10), M('#ffd23f'), 0, 7 * u)); g.add(mesh(new T.CylinderGeometry(4 * u, 4 * u, 10 * u, 18), M('#ff3b3b', { roughness: 0.35 }), 0, 18 * u)).rotation.z = Math.PI / 2; }
    else if (type === 'laser') { g.add(mesh(new T.CylinderGeometry(1.1 * u, 1.1 * u, 7 * u, 14), M('#c9ced6', { metalness: 0.9, roughness: 0.2 }), 0, 1 * u)); const b = mesh(new T.CylinderGeometry(0.8 * u, 0.8 * u, 30 * u, 12), M(accent, { emissive: accent, emissiveIntensity: 3, roughness: 0.1 }), 0, 19.5 * u); b.castShadow = false; g.add(b);
      const glow = mesh(new T.CylinderGeometry(1.8 * u, 1.8 * u, 31 * u, 12), new T.MeshBasicMaterial({ color: accent, transparent: true, opacity: 0.25, depthWrite: false }), 0, 19.5 * u); glow.castShadow = false; glow.userData.noAO = true; g.add(glow); }
    else if (type === 'hockey') { g.add(mesh(new T.CylinderGeometry(0.8 * u, 0.8 * u, 30 * u, 10), M('#c98b4b'), 0, 10 * u)); const bl = mesh(new T.BoxGeometry(8 * u, 2.4 * u, 1 * u), M('#1b1b24'), 3.5 * u, 25 * u); bl.rotation.z = 0.35; g.add(bl); }
    else if (type === 'wand') { g.add(mesh(new T.CylinderGeometry(0.5 * u, 0.7 * u, 16 * u, 10), M('#2a1a3a', { roughness: 0.4 }), 0, 6 * u)); const st = new T.Shape(); for (let i = 0; i <= 10; i++) { const r = (i % 2 ? 1.2 : 3) * u, a = i * Math.PI / 5 + Math.PI / 2; i ? st.lineTo(Math.cos(a) * r, Math.sin(a) * r) : st.moveTo(Math.cos(a) * r, Math.sin(a) * r); }
      const star = mesh(new T.ExtrudeGeometry(st, { depth: 0.8 * u, bevelEnabled: false }), M('#ffd23f', { emissive: '#ffd23f', emissiveIntensity: 1.6 }), 0, 15 * u); star.userData.spin = true; g.add(star); }
    else if (type === 'pan') { g.add(mesh(new T.CylinderGeometry(0.8 * u, 0.9 * u, 10 * u, 10), handle, 0, 3 * u)); const pan = mesh(new T.CylinderGeometry(6.5 * u, 5.5 * u, 1.6 * u, 24), M('#2a2a30', { metalness: 0.6, roughness: 0.35 }), 0, 14 * u); pan.rotation.x = Math.PI / 2; g.add(pan); }
    else if (type === 'guitar') { g.add(mesh(new T.BoxGeometry(1.8 * u, 16 * u, 1 * u), M('#3a2a1e'), 0, 4 * u)); g.add(mesh(new T.BoxGeometry(3 * u, 4 * u, 1 * u), M('#1b1b24'), 0, -5 * u));
      const body = new T.Group(); body.position.y = 17 * u; for (const [y, r] of [[0, 5.5], [6.5, 4.2]]) { const b = mesh(new T.CylinderGeometry(r * u, r * u, 2.4 * u, 24), M('#ff3b3b', { roughness: 0.3, metalness: 0.2 }), 0, y * u); b.rotation.x = Math.PI / 2; body.add(b); } g.add(body); }
    return g;
  }
  // a holder on a bone whose local axes are: +y = from bone a to bone b (in the rest pose), +z = the facing direction
  // worked out once, in the rest pose, then reused (re-dressing mid-animation must not use the animated pose)
  function calc(rig, bone, upA, upB, fwdA, fwdB) {
    const B = rig.bones; if (!B[bone] || !B[upA] || !B[upB]) return null;
    rig.root.updateMatrixWorld(true);
    const P = n => B[n].getWorldPosition(new T.Vector3());
    const up = P(upB).sub(P(upA)).normalize();
    let fwd = fwdA && B[fwdA] && B[fwdB] ? P(fwdB).sub(P(fwdA)) : new T.Vector3(0, 0, 1); fwd.addScaledVector(up, -fwd.dot(up)).normalize(); if (fwd.lengthSq() < 0.5) fwd.set(0, 0, 1);
    const x = new T.Vector3().crossVectors(up, fwd).normalize();
    const qw = new T.Quaternion().setFromRotationMatrix(new T.Matrix4().makeBasis(x, up, fwd));
    const bq = B[bone].getWorldQuaternion(new T.Quaternion()).invert();
    return { bone, q: bq.multiply(qw), s: 1 / (B[bone].getWorldScale(new T.Vector3()).x / Math.max(0.01, rig.root.scale.x)) };
  }
  // a natural grip: the held thing runs across the closed fist (perpendicular to the forearm), pointing forward and a
  // little up, with its handle in the palm rather than at the wrist joint. Worked out once in the rest pose.
  function gripCalc(rig) {
    const B = rig.bones; if (!B.RightHand || !B.RightForeArm) return null;
    rig.root.updateMatrixWorld(true);
    const P = n => B[n].getWorldPosition(new T.Vector3());
    const wrist = P('RightHand'), fore = wrist.clone().sub(P('RightForeArm')), L = fore.length(), dir = fore.clone().normalize();
    let fwd = B.headfront && B.Head ? P('headfront').sub(P('Head')) : new T.Vector3(0, 0, 1); fwd.y = 0; if (fwd.lengthSq() < 1e-6) fwd.set(0, 0, 1); fwd.normalize();
    const up = fwd.clone().add(new T.Vector3(0, 0.55, 0)); up.addScaledVector(dir, -up.dot(dir)).normalize();
    const z = new T.Vector3().crossVectors(dir, up).normalize();
    const qw = new T.Quaternion().setFromRotationMatrix(new T.Matrix4().makeBasis(dir, up, z));
    const bq = B.RightHand.getWorldQuaternion(new T.Quaternion()).invert();
    const palm = B.RightHand.worldToLocal(wrist.clone().addScaledVector(dir, L * 0.24));
    return { bone: 'RightHand', q: bq.multiply(qw), s: 1 / (B.RightHand.getWorldScale(new T.Vector3()).x / Math.max(0.01, rig.root.scale.x)), p: palm };
  }
  window.GRIP_HOLDER = rig => { if (!rig.attach) return null; if (!rig.attach.grip) return null; const h = holder(rig, 'grip'); return h; };
  function holder(rig, slot) {
    const a = rig.attach[slot]; if (!a) return null;
    const h = new T.Group(); h.quaternion.copy(a.q); h.scale.setScalar(a.s); if (a.p) h.position.copy(a.p); rig.bones[a.bone].add(h); return h;
  }

  // ------------------------------------------------------------ dress a rig in its fighter's equipped look
  window.DRESS = function (rig) {
    if (!rig || !rig.glb || !rig.bones) return;
    const id = rig.def.id, E = eqOf(id), s = rig.def.scale, accent = rig.def.accent || '#ff6ad5';
    if (!rig.attach) rig.attach = { hat: calc(rig, 'head_end', 'Head', 'head_end', 'Head', 'headfront'), back: calc(rig, 'Spine', 'Spine', 'neck', 'Head', 'headfront'), weapon: gripCalc(rig) }; rig.attach.grip = rig.attach.weapon;
    if (rig.dress) for (const o of rig.dress) if (o.parent) o.parent.remove(o);
    rig.dress = []; rig.noBlades = false; rig.dressWeapon = null;
    // outfit: re-colour the scanned model's materials
    const O = find('outfit', E.outfit);
    for (const m of rig.look.mats) {
      const u = m.userData; if (!u.base) u.base = { c: m.color.clone(), me: m.metalness, ro: m.roughness, e: m.emissive.clone(), ei: m.emissiveIntensity, env: m.envMapIntensity };
      m.color.copy(u.base.c).multiply(new T.Color(O.tint || '#ffffff')); m.metalness = O.metal != null ? O.metal : u.base.me; m.roughness = O.rough != null ? O.rough : u.base.ro;
      m.envMapIntensity = O.env != null ? O.env : u.base.env;
      if (O.em) { m.emissive.set(O.em === 'ACCENT' ? accent : O.em); m.emissiveIntensity = O.ei; } else { m.emissive.copy(u.base.e); m.emissiveIntensity = u.base.ei; }
      u.baseE = { c: m.emissive.getHex(), i: m.emissiveIntensity }; m.needsUpdate = true;
    }
    rig.flashKey = null;
    if (E.hat !== 'none') { const h = holder(rig, 'hat'); if (h) { const o = hat(E.hat, s, rig.def.color); o.position.y = -2.2 * s; h.add(o); rig.dress.push(h); } }
    if (E.back !== 'none') { const h = holder(rig, 'back'); if (h) { const o = back(E.back, s, accent); o.position.set(0, E.back === 'cape' ? 6 * s : 2 * s, -5.5 * s); h.add(o); rig.dress.push(h); rig.dressBack = o; } }
    if (rig.def.cape) towelCape(rig); // a fighter's own cape (Super Joe's towel), separate from Locker items
    if (E.weapon !== 'none') { const h = holder(rig, 'weapon'); if (h) { const o = weapon(E.weapon, s, accent); o.position.y = -1 * s; h.add(o); rig.dress.push(h); rig.dressWeapon = h; rig.noBlades = true; } }
  };
  // ------------------------------------------------------------ a fighter's own cape, as cloth (def.cape)
  // Super Joe's towel cape was fused to his arms in the scan, so it was cut off the model (tools/cut_tris.mjs) and is
  // rebuilt here: a white terry sheet that wraps over both shoulders from the upper back, hangs to the knees, and
  // swings with the fighter's movement (trails out behind when running, flies up when falling, flutters in the wind).
  let terry = null;
  function terryTex() {
    if (terry) return terry; const c = document.createElement('canvas'); c.width = c.height = 128; const g = c.getContext('2d');
    g.fillStyle = '#f3f1ea'; g.fillRect(0, 0, 128, 128);
    for (let k = 0; k < 2600; k++) { const v = 205 + Math.random() * 50 | 0; g.fillStyle = `rgb(${v},${v},${v - 6})`; g.fillRect(Math.random() * 128, Math.random() * 128, 1.6, 1.6); }
    terry = new T.CanvasTexture(c); terry.wrapS = terry.wrapT = T.RepeatWrapping; terry.repeat.set(3, 5); terry.colorSpace = T.SRGBColorSpace; return terry;
  }
  function towelCape(rig) {
    const h = holder(rig, 'back'); if (!h) return; const u = rig.def.scale, C = rig.def.cape === true ? {} : rig.def.cape;
    const W = (C.w || 23) * u, L = (C.len || 50) * u, nx = 14, ny = 22;
    const geo = new T.PlaneGeometry(1, 1, nx, ny), n = geo.attributes.position.count;
    const base = new Float32Array(n * 2); for (let i = 0; i < n; i++) { base[i * 2] = geo.attributes.position.getX(i) + 0.5; base[i * 2 + 1] = 0.5 - geo.attributes.position.getY(i); } // x 0..1 across, v 0..1 down
    const m = new T.Mesh(geo, new T.MeshStandardMaterial({ map: terryTex(), color: '#ffffff', roughness: 0.95, metalness: 0, side: T.DoubleSide, envMapIntensity: 0.4 }));
    m.castShadow = true; m.frustumCulled = false; m.position.set(0, (C.y != null ? C.y : 7) * u, (C.z != null ? C.z : -3) * u);
    h.add(m); rig.dress.push(h); rig.towel = { m, geo, base, W, L, lift: 0, liftV: 0, side: 0, t: 0 };
    shapeCape(rig.towel, 0);
  }
  function shapeCape(c, dt, f) {
    // swing: how far the cape is blown back (0 hanging .. 1 streaming), on a soft spring
    let target = 0.08, side = 0;
    if (f) { const sp = Math.abs(f.vx) / Math.max(1, f.def.run || 8); target = 0.1 + Math.min(1, sp) * 0.75 + (f.grounded ? 0 : 0.25) + (f.vy > 2 ? 0.35 : 0) + (f.state === 'attack' ? 0.15 : 0); side = -f.vx * f.facing * 0.01; }
    target = Math.min(0.85, target); c.liftV += (target - c.lift) * 30 * dt; c.liftV *= Math.pow(0.02, dt); c.lift += c.liftV * dt; c.t += dt;
    const P = c.geo.attributes.position, B = c.base, Lf = c.lift, t = c.t;
    for (let i = 0; i < P.count; i++) {
      const a = B[i * 2], v = B[i * 2 + 1], x0 = (a - 0.5);
      const wrap = Math.pow(Math.abs(x0) * 2, 2.2) * (1 - v) * (1 - v);        // the top corners wrap forward over the shoulders
      // blown back more toward the hem, while gravity pulls the hem back down (a curve, not a flat board)
      const ang = (0.08 + Lf * 1.05) * (0.45 + v * 0.75) - Lf * v * v * 0.55;
      const d = v * c.L * (1 - Lf * 0.15), flut = Math.sin(t * (6 + Lf * 6) + v * 6 + x0 * 5) * (0.02 + Lf * 0.06) * d;
      const x = x0 * c.W * (0.82 + v * 0.35 - Lf * v * 0.25);                  // gathers a little when streaming
      const curl = x0 * x0 * c.W * (0.55 - v * 0.2);                            // the sides curve forward around the body
      const fold = Math.sin(a * Math.PI * 5 + v * 2) * c.W * 0.018 * (0.4 + v);  // soft folds down the length
      P.setXYZ(i, x, -d * Math.cos(ang) - wrap * c.W * 0.05, -d * Math.sin(ang) + wrap * c.W * 0.32 + curl + fold + flut);
    }
    P.needsUpdate = true; c.geo.computeVertexNormals();
  }
  let capeLast = performance.now();
  if (typeof R3 !== 'undefined' && R3.addHook) R3.addHook(() => {
    const now = performance.now(), dt = Math.min(0.05, (now - capeLast) / 1000); capeLast = now;
    for (const f of (G.fighters || [])) { const rig = R3.rigOf(f); if (rig && rig.towel) shapeCape(rig.towel, G.paused ? 0 : dt, f); }
  });
  // little animations (jetpack flames, wings, wand star) and hiding the weapon while an item is held
  if (typeof R3 !== 'undefined' && R3.addHook) R3.addHook(() => {
    const t = performance.now() / 1000;
    for (const f of (G.fighters || [])) {
      const rig = R3.rigOf(f); if (!rig || !rig.dress) continue;
      if (rig.dressWeapon) rig.dressWeapon.visible = !f.item && !(f.state === 'dead' || f.state === 'out');
      if (rig.dressBack) rig.dressBack.traverse(o => { if (o.userData.flame) o.scale.y = 0.8 + 0.4 * Math.sin(t * 30 + o.position.x); if (o.userData.wing) o.rotation.y = o.userData.wing * (-0.5 - 0.25 * Math.sin(t * 6)); });
    }
  });
  // weapons give a little more reach in matches
  const baseStart = startMatch;
  startMatch = function (cfg) {
    baseStart(cfg);
    for (const f of G.fighters) if (eqOf(f.def.id).weapon !== 'none') { f.def = Object.assign({}, f.def, { reach: (f.def.reach || 1) * 1.06 }); f.baseDef = f.def; }
    G.starsDone = false;
  };
  if (window.__clash) window.__clash.startMatch = startMatch;

  // ------------------------------------------------------------ earning Stars
  const toast = document.createElement('div'); toast.id = 'starToast'; document.body.appendChild(toast);
  function award() {
    if (G.starsDone || !G.cfg || G.cfg.training) return; G.starsDone = true;
    const humans = G.fighters.filter(f => !f.cpu); if (!humans.length) return;
    const order = [...G.fighters].sort((a, b) => (b.stocks - a.stocks) || (b.outT - a.outT) || (a.percent - b.percent));
    let total = 0;
    for (const f of humans) {
      let n = 20 + f.stats.kos * 15;
      const won = G.cfg.story ? G.fighters.some(v => v.team === f.team && v.stocks > 0) && !G.fighters.some(v => v.team !== f.team && v.stocks > 0) : order[0] === f;
      if (won) { n += G.cfg.story ? 100 : 60; S.wins = (S.wins || 0) + 1; if (G.cfg.story && G.cfg.story.i === 5) S.storyDone = true; }
      total += n;
    }
    setTimeout(checkUnlocks, 1800);
    S.stars += total; persist();
    toast.innerHTML = `<b>+${total} ⭐</b><small>STARS EARNED · ${S.stars} total · spend them in the LOCKER</small>`;
    toast.classList.remove('on'); void toast.offsetWidth; toast.classList.add('on'); setTimeout(() => toast.classList.remove('on'), 4200);
    try { Sfx.sample && Sfx.sample('itemGet', 0.8, 0.9); } catch (e) {}
  }
  const baseStep = step;
  step = function () {
    baseStep();
    if (G.mode === 'fight' && G.ending === 2) award();
  };
  // ------------------------------------------------------------ unlockable fighters
  // unlock rules per fighter id (S.wins, S.storyDone and friends are tracked here); empty until the new cast arrives
  const RULES = {};
  window.isUnlocked = id => !!(S.unlocked && S.unlocked[id]);
  const news = document.createElement('div'); news.id = 'newChal'; document.body.appendChild(news);
  function checkUnlocks() {
    S.unlocked = S.unlocked || {};
    const fresh = Object.keys(RULES).filter(id => !S.unlocked[id] && RULES[id]());
    if (!fresh.length) return;
    for (const id of fresh) S.unlocked[id] = true; persist();
    showNew(fresh);
  }
  function showNew(list) {
    const id = list[0], d = FIGHTERS.find(f => f.id === id); if (!d) return;
    const F = window.CLASH_FAM || {};
    news.innerHTML = `<div class="nc-in" style="--c:${d.color};--c2:${d.accent}"><small>A NEW CHALLENGER!</small><div class="nc-card">${F[id] ? `<img src="${F[id]}" alt="">` : ''}</div><b>${d.name}</b><em>joins the battle!</em><span>tap to continue</span></div>`;
    news.classList.add('on'); if (window.LUX) { LUX.burst(innerWidth / 2, innerHeight / 2, d.accent, true); setTimeout(() => LUX.confetti(160), 300); }
    try { Sfx.sample && Sfx.sample(d.pet === 'dragon' ? 'dragonRoar' : 'orbBreak', 0.9); Sfx.voice && Sfx.voice({ def: d, port: 9 }, 'win', true); } catch (e) {}
    news.onclick = () => { news.classList.remove('on'); if (list.length > 1) setTimeout(() => showNew(list.slice(1)), 300); if (typeof buildSelect === 'function' && document.getElementById('select').classList.contains('on')) buildSelect(); };
  }
  window.LOCKER_UNLOCK = id => { S.unlocked = S.unlocked || {}; S.unlocked[id] = true; persist(); };
  if (window.__clash) window.__clash.step = step;

  // ------------------------------------------------------------ the Locker screen
  const css = document.createElement('style'); css.textContent = `
  #locker { justify-content:flex-start; align-items:stretch; padding:max(10px,2vh) clamp(10px,2vw,26px); gap:10px; background:linear-gradient(90deg, rgba(10,10,22,0) 0%, rgba(10,10,22,.15) 40%, rgba(10,10,22,.9) 60%) !important; }
  .lkhead { display:flex; align-items:center; gap:14px; }
  .lkhead b { font-size:clamp(22px,3.6vw,38px); font-style:italic; font-weight:900; letter-spacing:2px; background:var(--gold); -webkit-background-clip:text; background-clip:text; color:transparent; filter:drop-shadow(0 3px 0 rgba(0,0,0,.5)); }
  .lkhead .st { margin-left:auto; font-weight:900; font-size:clamp(16px,2.4vw,24px); padding:6px 14px; border-radius:30px; color:#2a1a00; background:var(--gold); box-shadow:0 0 22px rgba(255,190,60,.5); }
  .lkhead .big { margin:0; }
  .lkbody { flex:1; display:grid; grid-template-columns:1fr minmax(320px, 46%); gap:14px; min-height:0; }
  .lkstage { position:relative; cursor:grab; touch-action:none; }
  .lkstage .who { position:absolute; left:0; bottom:8px; font-weight:900; font-style:italic; font-size:clamp(26px,4.4vw,52px); color:#fff; text-shadow:2px 2px 0 #000, 0 0 22px var(--c); }
  .lkstage .hint { position:absolute; left:0; top:4px; color:#e8d6a8; font-size:12px; letter-spacing:2px; }
  .lkpanel { display:flex; flex-direction:column; gap:10px; min-height:0; }
  .lkpick { display:grid; grid-template-columns:repeat(9, 1fr); gap:5px; }
  .lkpick button { aspect-ratio:1; border-radius:10px; overflow:hidden; padding:0; cursor:pointer; border:2px solid transparent; background:linear-gradient(#10121f,#10121f) padding-box, linear-gradient(160deg,#fff1c4,var(--c),#6b4a12) border-box; opacity:.65; transition:opacity .2s, transform .2s; }
  .lkpick button img { width:100%; height:100%; object-fit:cover; display:block; }
  .lkpick button.on { opacity:1; transform:translateY(-3px); box-shadow:0 0 18px var(--c); }
  .lktabs { display:flex; gap:6px; } .lktabs button { flex:1; font:inherit; font-weight:900; letter-spacing:2px; padding:8px 4px; border-radius:10px; cursor:pointer; color:#f3e6c4; border:1px solid transparent;
    background:linear-gradient(180deg, rgba(36,40,66,.9), rgba(18,20,38,.9)) padding-box, linear-gradient(135deg,#8a6a2a,#e8c06a,#7a5a1e) border-box; font-size:clamp(11px,1.3vw,14px); }
  .lktabs button.on { color:#2a1a00; background:var(--gold) padding-box, linear-gradient(135deg,#fff,#c99a3a) border-box; }
  .lkgrid { display:grid; grid-template-columns:repeat(auto-fill, minmax(118px, 1fr)); gap:8px; overflow:auto; padding:4px; align-content:start; }
  .lkit { position:relative; font:inherit; color:#fff; text-align:center; cursor:pointer; border-radius:14px; padding:10px 6px 8px; border:2px solid transparent;
    background:linear-gradient(160deg, rgba(34,38,64,.92), rgba(14,16,32,.94)) padding-box, linear-gradient(160deg,#8a6a2a,#3a3048,#8a6a2a) border-box; transition:transform .15s; }
  .lkit:hover { transform:translateY(-3px); }
  .lkit .ic { font-size:clamp(28px,3.4vw,40px); line-height:1.1; display:block; filter:drop-shadow(0 4px 6px rgba(0,0,0,.5)); }
  .lkit .nm { display:block; font-weight:900; font-size:12px; letter-spacing:1px; margin-top:4px; }
  .lkit .pr { display:inline-block; margin-top:5px; font-weight:900; font-size:11px; padding:2px 9px; border-radius:20px; color:#2a1a00; background:var(--gold); }
  .lkit.locked .ic { filter:grayscale(1) brightness(.6) drop-shadow(0 4px 6px rgba(0,0,0,.5)); } .lkit.locked::after { content:"🔒"; position:absolute; top:6px; right:8px; font-size:13px; }
  .lkit.owned .pr { background:rgba(255,255,255,.12); color:#e8d6a8; }
  .lkit.eq { background:linear-gradient(160deg, rgba(90,70,20,.9), rgba(30,24,10,.94)) padding-box, var(--gold) border-box; box-shadow:0 0 20px rgba(255,190,60,.45); } .lkit.eq .pr { background:#4dd97a; color:#062a12; }
  .lkit.poor { animation:lkShake .35s; } @keyframes lkShake { 25% { translate:-6px 0; } 75% { translate:6px 0; } }
  #newChal { position:fixed; inset:0; z-index:59; display:none; place-items:center; background:radial-gradient(circle, rgba(40,20,60,.75), rgba(5,5,12,.95)); cursor:pointer; }
  #newChal.on { display:grid; }
  #newChal .nc-in { text-align:center; animation:ncIn .7s cubic-bezier(.2,.9,.3,1.4) both; }
  @keyframes ncIn { from { scale:.4; opacity:0; rotate:-8deg; } to { scale:1; opacity:1; rotate:0deg; } }
  #newChal small { display:block; font-weight:900; letter-spacing:6px; font-size:clamp(14px,2.4vw,24px); color:#fff; text-shadow:0 0 18px var(--c2); animation:pulse 1.2s infinite; }
  #newChal .nc-card { width:min(46vw, 260px); aspect-ratio:3/4; margin:14px auto; border-radius:18px; overflow:hidden; transform:skewX(-6deg); box-shadow:0 0 0 3px #fff1c4, 0 0 60px var(--c2); background:var(--c); }
  #newChal .nc-card img { width:100%; height:100%; object-fit:cover; object-position:50% 10%; }
  #newChal b { display:block; font-size:clamp(40px,8vw,90px); font-weight:900; font-style:italic; background:var(--gold); -webkit-background-clip:text; background-clip:text; color:transparent; filter:drop-shadow(0 4px 0 rgba(0,0,0,.6)); }
  #newChal em { display:block; font-style:italic; font-weight:900; color:#fff; font-size:clamp(16px,2.6vw,26px); }
  #newChal span { display:block; margin-top:16px; color:#e8d6a8; font-size:12px; letter-spacing:3px; }
  #starToast { position:fixed; left:50%; top:16%; z-index:57; transform:translate(-50%,-30px) scale(.8); opacity:0; pointer-events:none; text-align:center; transition:all .45s cubic-bezier(.2,.9,.3,1.4); }
  #starToast.on { opacity:1; transform:translate(-50%,0) scale(1); }
  #starToast b { display:block; font-size:clamp(34px,6vw,64px); font-weight:900; font-style:italic; background:var(--gold); -webkit-background-clip:text; background-clip:text; color:transparent; filter:drop-shadow(0 4px 0 rgba(0,0,0,.5)) drop-shadow(0 0 20px rgba(255,190,60,.6)); }
  #starToast small { color:#ffe9b0; font-weight:800; letter-spacing:2px; font-size:12px; text-shadow:0 2px 4px #000; }
  @media (max-aspect-ratio: 6/5) { #locker { background:linear-gradient(180deg, rgba(10,10,22,0) 0%, rgba(10,10,22,.1) 30%, rgba(10,10,22,.92) 46%) !important; }
    .lkbody { grid-template-columns:1fr; grid-template-rows:minmax(200px, 38vh) 1fr; } .lkgrid { grid-template-columns:repeat(3, 1fr); } .lkhead .big { padding:6px 16px; font-size:14px; } }`;
  document.head.appendChild(css);
  const scr = document.createElement('div'); scr.id = 'locker'; scr.className = 'screen';
  scr.innerHTML = `<div class="lkhead"><b>LOCKER</b><span class="st">⭐ <em id="lkStars">0</em></span><button class="big alt" id="lkBack">BACK</button></div>
    <div class="lkbody"><div class="lkstage" id="lkStage"><span class="hint">DRAG TO SPIN</span><span class="who" id="lkWho"></span></div>
    <div class="lkpanel"><div class="lkpick" id="lkPick"></div><div class="lktabs" id="lkTabs"></div><div class="lkgrid" id="lkGrid"></div></div></div>`;
  document.body.insertBefore(scr, document.getElementById('touch'));
  const $ = id => document.getElementById(id);
  const FAM = () => (typeof rosterIdx === 'function' ? rosterIdx() : []).map(k => FIGHTERS[k].id).filter(id => !(FIGHTERS.find(f => f.id === id) || {}).pet);
  let who = null, tab = 'outfit', spin = 0.35, spinV = 0, dragging = false, lastX = 0;
  const def = id => FIGHTERS.find(f => f.id === id);
  function preview(react) {
    if (typeof R3 === 'undefined' || !R3.menuSet) return;
    const wide = innerWidth > innerHeight * 1.2;
    R3.menuSet([{ key: 'locker', id: who, x: wide ? -170 : 0, face: spin, scale: 1.35, once: react ? 'victory' : null }], wide ? { x: -40, y: 95, d: 600, lx: -40, ly: 72 } : { x: 0, y: 150, d: 760, lx: 0, ly: 150 });
  }
  function render() {
    $('lkStars').textContent = S.stars;
    const d = def(who); $('lkWho').textContent = d ? d.name : ''; $('lkStage').style.setProperty('--c', d ? d.color : '#fff');
    const F = window.CLASH_FAM || {};
    $('lkPick').innerHTML = FAM().filter(def).map(id => `<button data-id="${id}" class="${id === who ? 'on' : ''}" style="--c:${def(id).color}" aria-label="${def(id).name}">${F['face_' + id] ? `<img src="${F['face_' + id]}" alt="">` : def(id).name}</button>`).join('');
    $('lkTabs').innerHTML = Object.entries(CAT).map(([k, c]) => `<button data-tab="${k}" class="${k === tab ? 'on' : ''}">${c.label}</button>`).join('');
    const E = eqOf(who);
    $('lkGrid').innerHTML = CAT[tab].items.map(it => {
      const own = owns(who, tab, it.id), eq = E[tab] === it.id;
      return `<button class="lkit ${own ? 'owned' : 'locked'} ${eq ? 'eq' : ''}" data-it="${it.id}"><span class="ic">${it.icon}</span><span class="nm">${it.name}</span><span class="pr">${eq ? 'EQUIPPED' : own ? 'EQUIP' : it.price + ' ⭐'}</span></button>`;
    }).join('');
  }
  scr.addEventListener('click', e => {
    const b = e.target.closest('button'); if (!b) return;
    try { Sfx.init(); } catch (er) {}
    if (b.id === 'lkBack') { showScreen('hub'); return; }
    if (b.dataset.id) { who = b.dataset.id; spin = 0.35; render(); preview(true); return; }
    if (b.dataset.tab) { tab = b.dataset.tab; render(); return; }
    if (b.dataset.it) {
      const it = find(tab, b.dataset.it);
      if (!owns(who, tab, it.id)) {
        if (S.stars < it.price) { b.classList.remove('poor'); void b.offsetWidth; b.classList.add('poor'); try { Sfx.tone && Sfx.tone(150, 0.12, 'square', 0.08, 90); } catch (er) {} return; }
        S.stars -= it.price; (S.own[who] = S.own[who] || {})[tab + ':' + it.id] = true; try { Sfx.sample && Sfx.sample('itemBreak', 0.7); } catch (er) {}
      }
      (S.eq[who] = S.eq[who] || {})[tab] = it.id; persist();
      if (typeof R3 !== 'undefined' && R3.redress) R3.redress(who);
      render(); preview(true);
    }
  });
  // drag the preview to spin the fighter round
  const stage = $('lkStage');
  stage.addEventListener('pointerdown', e => { dragging = true; lastX = e.clientX; stage.setPointerCapture(e.pointerId); });
  stage.addEventListener('pointermove', e => { if (!dragging) return; spinV = (e.clientX - lastX) * 0.012; spin += spinV; lastX = e.clientX; preview(); });
  stage.addEventListener('pointerup', () => { dragging = false; });
  setInterval(() => { if (!scr.classList.contains('on') || dragging) return; spinV *= 0.94; spin += spinV + 0.004; preview(); }, 33);
  function open() {
    const cast = FAM(); if (!cast.includes(who)) who = cast[0] || null;
    if (!who) { // nobody to dress yet
      if (typeof R3 !== 'undefined' && R3.menuSet) R3.menuSet([], { x: 0, y: 92, d: 620, lx: 0, ly: 60 });
      $('lkStars').textContent = S.stars; $('lkWho').textContent = ''; $('lkPick').innerHTML = ''; $('lkTabs').innerHTML = '';
      $('lkGrid').innerHTML = '<div class="emptyroster mini" style="grid-column:1/-1"><div class="er-ring"><i></i><i></i><i></i><b>?</b></div><div class="er-txt"><h4>NO FIGHTERS YET</h4><p>Outfits, hats, capes and weapons unlock here once the first fighter joins the roster.</p></div></div>';
      showScreen('locker'); return;
    }
    render(); showScreen('locker'); preview(true);
  }
  // a LOCKER tile on the main menu
  const tiles = document.querySelector('#hub .tiles');
  if (tiles) {
    const t = document.createElement('button'); t.className = 'tile'; t.dataset.mode = 'locker'; t.style.cssText = '--c1:#8a5a12;--c2:#e0a83a';
    t.innerHTML = '<b>LOCKER</b><small>Unlock outfits, hats, capes and weapons for every fighter with Stars.</small><i>👑</i>'; tiles.appendChild(t);
    t.addEventListener('click', e => { e.stopPropagation(); open(); });
  }
  window.LOCKER = { open, catalog: CAT, state: () => S, equipped: eqOf, addStars(n) { S.stars += n; persist(); } };
})();
