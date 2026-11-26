'use strict';
// Super Family Smash: clean skin weights for the scanned family models.
// Meshy's automatic rig left patches of sleeve and trouser tied to the wrong bones, so clothes stuck to the body (or
// to the other limb) and stretched as arms and legs moved. This recomputes every vertex's bone weights from the bones
// themselves, once per model when it loads:
//   1. each bone becomes a segment (bone -> child joint) in the bind pose;
//   2. a vertex may only follow bones of its own body part: arm bones only take vertices out past that shoulder,
//      each leg only its own side below the hips, the rest belongs to the spine / neck / head;
//   3. within that part, weight falls off with distance to each segment (the nearest two or three bones);
//   4. weights are then smoothed across the surface (welding UV seams) so every joint bends like fabric, not paper.
(() => {
  const T = window.THREE; if (!T) return;
  const CHILD = { Hips: 'Spine02', Spine02: 'Spine01', Spine01: 'Spine', Spine: 'neck', neck: 'Head', Head: 'head_end',
    LeftShoulder: 'LeftArm', LeftArm: 'LeftForeArm', LeftForeArm: 'LeftHand', RightShoulder: 'RightArm', RightArm: 'RightForeArm', RightForeArm: 'RightHand',
    LeftUpLeg: 'LeftLeg', LeftLeg: 'LeftFoot', LeftFoot: 'LeftToeBase', RightUpLeg: 'RightLeg', RightLeg: 'RightFoot', RightFoot: 'RightToeBase' };
  const PART = n => /^Left(Shoulder|Arm|ForeArm|Hand)/.test(n) ? 'armL' : /^Right(Shoulder|Arm|ForeArm|Hand)/.test(n) ? 'armR'
    : /^Left(UpLeg|Leg|Foot|Toe)/.test(n) ? 'legL' : /^Right(UpLeg|Leg|Foot|Toe)/.test(n) ? 'legR' : 'core';
  function reskin(mesh) {
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
    const bm = mesh.bindMatrix, v = new T.Vector3(), ab = new T.Vector3(), av = new T.Vector3();
    const W = new Float32Array(n * nb);
    const SI0 = geo.attributes.skinIndex, SW0 = geo.attributes.skinWeight;
    const partSum = { core: 0, armL: 0, armR: 0, legL: 0, legR: 0 };
    for (let i = 0; i < n; i++) {
      v.fromBufferAttribute(P, i).applyMatrix4(bm);
      // the body part Meshy mostly gave this vertex to
      for (const k in partSum) partSum[k] = 0;
      for (let c = 0; c < 4; c++) { const w = SW0.getComponent(i, c); if (w > 0) partSum[seg[SI0.getComponent(i, c)].part] += w; }
      let part = 'core', pw = -1; for (const k in partSum) if (partSum[k] > pw) { pw = partSum[k]; part = k; }
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
  window.RESKIN = scene => { scene.updateMatrixWorld(true); scene.traverse(o => { if (o.isSkinnedMesh) try { reskin(o); } catch (e) { console.warn('reskin failed', e); } }); };
})();
