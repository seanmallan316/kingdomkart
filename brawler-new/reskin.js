'use strict';
// Super Family Smash (90's Version): clean skin weights for the scanned fighter models.
// Meshy's automatic rig left patches of sleeve and trouser tied to the wrong bones, so clothes stuck to the body (or
// to the other limb) and stretched as arms and legs moved. This recomputes every vertex's bone weights from the bones
// themselves, once per model when it loads:
//   1. each bone becomes a segment (bone -> child joint) in the bind pose;
//   2. a vertex may only follow bones of its own body part: arm bones only take vertices out past that shoulder,
//      each leg only its own side below the hips, the rest belongs to the spine / neck / head;
//   3. within that part, weight falls off with distance to each segment (the nearest two or three bones);
//   4. weights are then smoothed across the surface (welding UV seams) so every joint bends like fabric, not paper.
//   5. a fighter with def.cape (Super Joe's towel cape): cape fabric between the legs (above the knees) follows the spine,
//      so kicks don't drag the hem along with the leg. Arm-side fabric stays with the arms (see below).
(() => {
  const T = window.THREE; if (!T) return;
  const CHILD = { Hips: 'Spine02', Spine02: 'Spine01', Spine01: 'Spine', Spine: 'neck', neck: 'Head', Head: 'head_end',
    LeftShoulder: 'LeftArm', LeftArm: 'LeftForeArm', LeftForeArm: 'LeftHand', RightShoulder: 'RightArm', RightArm: 'RightForeArm', RightForeArm: 'RightHand',
    LeftUpLeg: 'LeftLeg', LeftLeg: 'LeftFoot', LeftFoot: 'LeftToeBase', RightUpLeg: 'RightLeg', RightLeg: 'RightFoot', RightFoot: 'RightToeBase' };
  const PART = n => /^Left(Shoulder|Arm|ForeArm|Hand)/.test(n) ? 'armL' : /^Right(Shoulder|Arm|ForeArm|Hand)/.test(n) ? 'armR'
    : /^Left(UpLeg|Leg|Foot|Toe)/.test(n) ? 'legL' : /^Right(UpLeg|Leg|Foot|Toe)/.test(n) ? 'legR' : 'core';
  function reskin(mesh, opts) {
    const sk = mesh.skeleton, geo = mesh.geometry; if (!sk || !geo || geo.userData.reskinned) return;
    const bones = sk.bones, nb = bones.length, P = geo.attributes.position, n = P.count;
    const idxOf = {}; bones.forEach((b, i) => { idxOf[b.name] = i; });
    // bind-pose joint positions, in the same space as the bind-pose vertices
    const J = bones.map((b, i) => new T.Vector3().setFromMatrixPosition(new T.Matrix4().copy(sk.boneInverses[i]).invert()));
    const seg = bones.map((b, i) => {
      const nm = b.name, ch = CHILD[nm]; let e;
      if (ch && idxOf[ch] != null) e = J[idxOf[ch]].clone();
      else if (/Hand$/.test(nm)) { const f = idxOf[nm.replace('Hand', 'ForeArm')]; e = J[i].clone().add(J[i].clone().sub(J[f]).multiplyScalar(0.45)); }
      else if (/Toe/.test(nm)) { const f = idxOf[nm.replace('ToeBase', 'Foot')]; e = J[i].clone().add(J[i].clone().sub(J[f]).multiplyScalar(0.5)); }
      else e = J[i].clone().add(new T.Vector3(0, 0.001, 0));
      return { a: J[i], b: e, part: PART(nm), skip: /head_end|headfront/.test(nm) };
    });
    const g = k => J[idxOf[k]] || null;
    const hips = g('Hips'), shL = g('LeftArm'), shR = g('RightArm'), upL = g('LeftUpLeg'), upR = g('RightUpLeg');
    if (!hips || !shL || !shR || !upL || !upR) return;
    const midX = hips.x, side = Math.sign(shL.x - midX) || 1; // which way +x points for the left side
    const shoulderEdge = s => (s === 'armL' ? Math.abs(shL.x - midX) : Math.abs(shR.x - midX)) * 0.8;
    const crotchY = Math.min(upL.y, upR.y) - 0.02 * Math.abs(hips.y);
    const torsoLen = Math.abs((shL.y + shR.y) / 2 - hips.y) || 1;
    const kneeY = Math.max((g('LeftLeg') || hips).y, (g('RightLeg') || hips).y); // cape fabric is only re-homed above the knees
    // texture lookup (cape only): is the surface at this vertex near-white?
    let texWhite = () => false;
    if (opts && opts.cape && mesh.material && mesh.material.map && mesh.material.map.image && geo.attributes.uv) {
      try {
        const img = mesh.material.map.image, cw = 256, ch = 256, cv = document.createElement('canvas'); cv.width = cw; cv.height = ch;
        const cx = cv.getContext('2d'); cx.drawImage(img, 0, 0, cw, ch); const px = cx.getImageData(0, 0, cw, ch).data, UV = geo.attributes.uv, flip = mesh.material.map.flipY;
        const map = mesh.material.map; map.updateMatrix(); const e = map.matrix.elements; // quantized models carry a texture transform
        texWhite = i => { const u0 = UV.getX(i), v0 = UV.getY(i), u1 = e[0] * u0 + e[3] * v0 + e[6], v1 = e[1] * u0 + e[4] * v0 + e[7];
          const u = ((u1 % 1) + 1) % 1, vv = ((v1 % 1) + 1) % 1, x = Math.min(cw - 1, Math.floor(u * cw)), y = Math.min(ch - 1, Math.floor((flip ? 1 - vv : vv) * ch)), o = (y * cw + x) * 4;
          const r = px[o], g = px[o + 1], b = px[o + 2]; return Math.min(r, g, b) > 165 && Math.max(r, g, b) - Math.min(r, g, b) < 40; };
      } catch (e) { console.warn('cape texture read failed', e); }
    }
    const bm = mesh.bindMatrix, v = new T.Vector3(), ab = new T.Vector3(), av = new T.Vector3();
    const W = new Float32Array(n * nb);
    // this three.js build has no BufferAttribute.getComponent, so read components through getX/getY/getZ/getW
    const comp = (a, i, c) => (c === 0 ? a.getX(i) : c === 1 ? a.getY(i) : c === 2 ? a.getZ(i) : a.getW(i));
    const SI0 = { getComponent: (i, c) => comp(geo.attributes.skinIndex, i, c) }, SW0 = { getComponent: (i, c) => comp(geo.attributes.skinWeight, i, c) };
    const partSum = { core: 0, armL: 0, armR: 0, legL: 0, legR: 0 };
    for (let i = 0; i < n; i++) {
      v.fromBufferAttribute(P, i).applyMatrix4(bm);
      // the body part Meshy mostly gave this vertex to
      for (const k in partSum) partSum[k] = 0;
      for (let c = 0; c < 4; c++) { const w = SW0.getComponent(i, c); if (w > 0) partSum[seg[SI0.getComponent(i, c)].part] += w; }
      let part = 'core', pw = -1; for (const k in partSum) if (partSum[k] > pw) { pw = partSum[k]; part = k; }
      if (opts && opts.cape && part !== 'core') { // cape fabric: anything well clear of that limb's bones hangs from the spine
        let dmin = 1e9;
        for (let j = 0; j < nb; j++) { const sg = seg[j]; if (sg.skip || sg.part !== part || /Shoulder/.test(bones[j].name)) continue;
          ab.subVectors(sg.b, sg.a); av.subVectors(v, sg.a); const t = Math.max(0, Math.min(1, av.dot(ab) / (ab.lengthSq() || 1e-9))); dmin = Math.min(dmin, av.addScaledVector(ab, -t).length()); }
        // the cape is the bright white fabric: white texels away from the limb's own surface go to the spine
        // (socks and shoes are white too but sit right on the leg bones, so they stay with the legs)
        const white = texWhite(i);
        if (false && window.__dbg) { let nm = ''; let dd = 1e9; for (let j = 0; j < nb; j++) { const sg = seg[j]; if (sg.skip || sg.part !== part) continue; ab.subVectors(sg.b, sg.a); av.subVectors(v, sg.a); const t = Math.max(0, Math.min(1, av.dot(ab) / (ab.lengthSq() || 1e-9))); const d = av.addScaledVector(ab, -t).length(); if (d < dd) { dd = d; nm = bones[j].name; } } const k = nm.replace(/Left|Right/, '') + (white ? ' W' : ' -') + ' ' + (Math.floor(dmin / torsoLen * 10) / 10); window.__dbg[k] = (window.__dbg[k] || 0) + 1; }
        // (arms: left alone. Super Joe's scan fused his arms into the cape, so splitting them tears the fabric; the cape
        //  flaring with his arms reads as a cape, a torn arm does not)
        if (part.startsWith('arm') ? false
          : v.y > kneeY && (dmin > torsoLen * 0.42 || (white && dmin > torsoLen * 0.16))) part = 'core';
      }
      // within that part: Meshy's weights (cross-part pulls removed) mixed with distance-to-bone weights
      const allowed = j => seg[j].part === part || (part !== 'core' && /^(Spine|Hips)$/.test(bones[j].name) && (part.startsWith('arm') ? bones[j].name === 'Spine' : bones[j].name === 'Hips'));
      let tot = 0, dtot = 0; const D = [];
      for (let j = 0; j < nb; j++) {
        const sg = seg[j]; if (sg.skip || !allowed(j)) continue;
        ab.subVectors(sg.b, sg.a); av.subVectors(v, sg.a); const L2 = ab.lengthSq() || 1e-9;
        const t = Math.max(0, Math.min(1, av.dot(ab) / L2)); const d = av.addScaledVector(ab, -t).length() + 1e-3;
        let w = 1 / Math.pow(d, 4); if (seg[j].part !== part) w *= 0.25; D.push([j, w]); dtot += w;
      }
      for (let c = 0; c < 4; c++) { const j = SI0.getComponent(i, c), w = SW0.getComponent(i, c); if (w > 0 && allowed(j)) { W[i * nb + j] += w * 0.5; tot += w * 0.5; } }
      if (tot > 0) for (let j = 0; j < nb; j++) W[i * nb + j] *= 0.5 / tot; // Meshy half, normalised to 0.5
      for (const [j, w] of D) W[i * nb + j] += (dtot > 0 ? w / dtot : 0) * (tot > 0 ? 0.5 : 1);
    }
    // weld vertices that share a position (UV seams) and build neighbour lists for smoothing
    const key = new Map(), rep = new Int32Array(n);
    for (let i = 0; i < n; i++) { const k = Math.round(P.getX(i) * 2000) + ',' + Math.round(P.getY(i) * 2000) + ',' + Math.round(P.getZ(i) * 2000); if (!key.has(k)) key.set(k, i); rep[i] = key.get(k); }
    const nbr = new Map(), add = (a, b) => { a = rep[a]; b = rep[b]; if (a === b) return; (nbr.get(a) || nbr.set(a, new Set()).get(a)).add(b); };
    const I = geo.index; const tri = I ? I.array : null, nt = I ? I.count : n;
    for (let k = 0; k < nt; k += 3) { const a = tri ? tri[k] : k, b = tri ? tri[k + 1] : k + 1, c = tri ? tri[k + 2] : k + 2; add(a, b); add(b, a); add(b, c); add(c, b); add(a, c); add(c, a); }
    for (let it = 0; it < 2; it++) {
      const N = new Float32Array(W);
      for (const [a, set] of nbr) {
        const o = a * nb; for (let j = 0; j < nb; j++) { let s = W[o + j] * 2; for (const b of set) s += W[b * nb + j]; N[o + j] = s / (2 + set.size); }
      }
      W.set(N);
    }
    // keep the 4 strongest bones per vertex
    const SI = new Uint16Array(n * 4), SW = new Float32Array(n * 4);
    for (let i = 0; i < n; i++) {
      const r = rep[i], o = r * nb, top = [];
      for (let j = 0; j < nb; j++) if (W[o + j] > 0.002) top.push([W[o + j], j]);
      top.sort((x, y) => y[0] - x[0]); const t4 = top.slice(0, 4); const s = t4.reduce((a, x) => a + x[0], 0) || 1;
      t4.forEach(([w, j], k) => { SI[i * 4 + k] = j; SW[i * 4 + k] = w / s; });
    }
    geo.setAttribute('skinIndex', new T.Uint16BufferAttribute(SI, 4));
    geo.setAttribute('skinWeight', new T.Float32BufferAttribute(SW, 4));
    geo.userData.reskinned = true;
  }
  window.RESKIN = (scene, id) => { const d = id && typeof FIGHTERS !== 'undefined' && FIGHTERS.find(f => f.id === id), opts = { cape: !!(d && d.cape) };
    scene.updateMatrixWorld(true); scene.traverse(o => { if (o.isSkinnedMesh) try { reskin(o, opts); } catch (e) { console.warn('reskin failed', e); } }); };
})();
