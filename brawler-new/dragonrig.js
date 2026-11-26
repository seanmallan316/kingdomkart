'use strict';
// Super Family Smash (90's Version): a real 3D skeleton for a rig-less creature fighter (built for a dragon). A scanned creature with no rig gets one fitted
// to the model when it loads, from the shape itself (the part masks pets.js bakes: legs, tail, neck/head, wings):
//   hips + a 3-bone spine to the chest; a 5-bone neck curving up to the head; a 10-bone tail along its real curl (the
//   centre line of the tail, traced from root to tip); 4 bones along each wing's leading edge (shoulder, elbow, wrist,
//   tip); 4 bones per leg (hip/shoulder, knee, ankle, toe).
// Skin weights: each vertex follows the bones of its own body part (distance to each bone segment, nearest two),
// blended with the spine where parts meet, so joints bend smoothly. Everything rotates in 3D, so the dragon can curve
// its neck to look around (and at the camera), snake its tail side to side and up and down, fold and sweep its wings,
// and flex its spine, instead of hinging in one flat plane.
(() => {
  const T = window.THREE; if (!T) return;
  const V = (x, y, z) => new T.Vector3(x, y, z);
  const mean = a => { const v = V(0, 0, 0); for (const p of a) v.add(p); return a.length ? v.multiplyScalar(1 / a.length) : null; };
  function segDist(p, a, b) { const ab = b.clone().sub(a), t = Math.max(0, Math.min(1, p.clone().sub(a).dot(ab) / (ab.lengthSq() || 1e-9))); return p.distanceTo(a.clone().addScaledVector(ab, t)); }

  // distance through the shape itself (not a straight line): points go into small cubes and a flood fill from the
  // start finds every cube's path length, so a tail that curls back on itself is still measured root to tip
  function geodesic(points, start, cell) {
    const key = p => Math.floor(p.x / cell) + ',' + Math.floor(p.y / cell) + ',' + Math.floor(p.z / cell);
    const vox = new Map(); points.forEach((p, i) => { const k = key(p); let v = vox.get(k); if (!v) vox.set(k, v = { c: V(0, 0, 0), n: 0, d: Infinity, k }); v.c.add(p); v.n++; });
    for (const v of vox.values()) v.c.multiplyScalar(1 / v.n);
    let s0 = null, best = Infinity; for (const v of vox.values()) { const d = v.c.distanceTo(start); if (d < best) { best = d; s0 = v; } }
    const heap = [], push = v => { heap.push(v); let i = heap.length - 1; while (i) { const pa = (i - 1) >> 1; if (heap[pa].d <= heap[i].d) break; [heap[pa], heap[i]] = [heap[i], heap[pa]]; i = pa; } };
    const pop = () => { const top = heap[0], last = heap.pop(); if (heap.length) { heap[0] = last; let i = 0; for (;;) { const l = 2 * i + 1, r = l + 1; let m = i; if (l < heap.length && heap[l].d < heap[m].d) m = l; if (r < heap.length && heap[r].d < heap[m].d) m = r; if (m === i) break; [heap[m], heap[i]] = [heap[i], heap[m]]; i = m; } } return top; };
    if (s0) { s0.d = 0; push(s0); }
    while (heap.length) {
      const v = pop(), [x, y, z] = v.k.split(',').map(Number);
      for (let dx = -1; dx <= 1; dx++) for (let dy = -1; dy <= 1; dy++) for (let dz = -1; dz <= 1; dz++) {
        if (!dx && !dy && !dz) continue; const w = vox.get((x + dx) + ',' + (y + dy) + ',' + (z + dz)); if (!w) continue;
        const nd = v.d + v.c.distanceTo(w.c); if (nd < w.d) { w.d = nd; push(w); }
      }
    }
    const reach = [...vox.values()].filter(v => v.d < Infinity);
    return points.map(p => { const v = vox.get(key(p)); if (v && v.d < Infinity) return v.d; let b = null, bd = Infinity; for (const r of reach) { const d = r.c.distanceTo(p); if (d < bd) { bd = d; b = r; } } return b ? b.d + bd : 0; });
  }
  // a chain of n joints along a part: centroids of equal slices of the path length from the start
  function chain(points, start, n, cell) {
    const d = geodesic(points, start, cell), dMax = Math.max(...d, 1e-3), out = [];
    for (let k = 0; k < n; k++) { const a0 = dMax * k / n, a1 = dMax * (k + 1) / n; const m = mean(points.filter((p, i) => d[i] >= a0 && d[i] < a1 + (k === n - 1 ? 1e-6 : 0))); out.push(m || (out[k - 1] ? out[k - 1].clone() : start.clone())); }
    let tipI = 0; d.forEach((x, i) => { if (x > d[tipI]) tipI = i; });
    return { j: out, tip: points[tipI] ? points[tipI].clone() : out[n - 1].clone() };
  }
  // distance across the mesh surface (vertices welded at seams), from several sources at once: label[i] = nearest source
  function surfaceLabels(g, sources) {
    const P = g.attributes.position, I = g.index, n = P.count, keyOf = i => Math.round(P.getX(i) * 4000) + ',' + Math.round(P.getY(i) * 4000) + ',' + Math.round(P.getZ(i) * 4000);
    const weld = new Int32Array(n), first = new Map(); for (let i = 0; i < n; i++) { const k = keyOf(i); if (!first.has(k)) first.set(k, i); weld[i] = first.get(k); }
    const adj = new Map(), link = (a, b) => { a = weld[a]; b = weld[b]; if (a === b) return; (adj.get(a) || adj.set(a, []).get(a)).push(b); };
    const tri = I ? I.array : null, nt = I ? I.count : n;
    for (let t = 0; t < nt; t += 3) { const a = tri ? tri[t] : t, b = tri ? tri[t + 1] : t + 1, c = tri ? tri[t + 2] : t + 2; link(a, b); link(b, a); link(b, c); link(c, b); link(a, c); link(c, a); }
    const dist = new Float64Array(n).fill(Infinity), lab = new Int8Array(n).fill(-1), heap = []; // 64-bit: a 32-bit store would round and re-queue forever
    const up = i => { while (i) { const pa = (i - 1) >> 1; if (heap[pa][0] <= heap[i][0]) break; [heap[pa], heap[i]] = [heap[i], heap[pa]]; i = pa; } };
    const down = () => { let i = 0; for (;;) { const l = 2 * i + 1, r = l + 1; let m = i; if (l < heap.length && heap[l][0] < heap[m][0]) m = l; if (r < heap.length && heap[r][0] < heap[m][0]) m = r; if (m === i) break; [heap[m], heap[i]] = [heap[i], heap[m]]; i = m; } };
    const v = V(0, 0, 0);
    for (let i = 0; i < n; i++) { if (weld[i] !== i) continue; v.set(P.getX(i), P.getY(i), P.getZ(i)); sources.forEach((src, li) => { for (const c of src) if (v.distanceTo(c) < 0.07 && dist[i] > 0) { dist[i] = 0; lab[i] = li; heap.push([0, i]); up(heap.length - 1); } }); }
    const len = (a, b) => Math.hypot(P.getX(a) - P.getX(b), P.getY(a) - P.getY(b), P.getZ(a) - P.getZ(b));
    while (heap.length) {
      const [d, i] = heap[0]; const last = heap.pop(); if (heap.length) { heap[0] = last; down(); }
      if (d > dist[i]) continue;
      for (const j of adj.get(i) || []) { const nd = d + len(i, j); if (nd < dist[j]) { dist[j] = nd; lab[j] = lab[i]; heap.push([nd, j]); up(heap.length - 1); } }
    }
    const out = new Int8Array(n); for (let i = 0; i < n; i++) out[i] = lab[weld[i]];
    return out;
  }
  function make(geos, grp) {
    // ---- collect points by part
    const pts = { leg: [[], [], [], []], tail: [], head: [], wing: [[], []], body: [] };
    for (const g of geos) {
      const P = g.attributes.position, L = g.attributes.aLeg, B = g.attributes.aB, K = g.attributes.aWg;
      for (let i = 0; i < P.count; i += 2) {
        const p = V(P.getX(i), P.getY(i), P.getZ(i));
        const legs = [L.getX(i), L.getY(i), L.getZ(i), L.getW(i)], tl = B.getX(i), hd = B.getY(i), wg = K.getX(i);
        let k = legs.indexOf(Math.max(...legs));
        if (legs[k] > 0.5) pts.leg[k].push(p); else if (tl > 0.5) pts.tail.push(p); else if (hd > 0.5) pts.head.push(p); else if (wg > 0.5) pts.wing[p.x >= 0 ? 0 : 1].push(p); else pts.body.push(p);
      }
    }
    // ---- joints
    const J = {};
    // legs (order FL, FR, BL, BR): top (pushed into the body), knee, ankle, toe and toe tip
    const legJ = pts.leg.map(a => {
      const s = a.slice().sort((p, q) => q.y - p.y), n = s.length, band = (u0, u1) => mean(s.slice(Math.floor(n * u0), Math.max(Math.floor(n * u0) + 1, Math.floor(n * u1))));
      const top = band(0, 0.15).add(V(0, 0.05, 0)), knee = band(0.4, 0.6), ankle = band(0.75, 0.88), toe = band(0.9, 1);
      const tip = s.slice(Math.floor(n * 0.85)).reduce((b, p) => p.z > b.z ? p : b, toe).clone();
      return { top, knee, ankle, toe, tip };
    });
    const zFront = (legJ[0].top.z + legJ[1].top.z) / 2, zHind = (legJ[2].top.z + legJ[3].top.z) / 2;
    const yFront = (legJ[0].top.y + legJ[1].top.y) / 2 + 0.04, yHind = (legJ[2].top.y + legJ[3].top.y) / 2 + 0.03;
    const spine = [0, 1, 2, 3].map(k => V(0, yHind + (yFront - yHind) * k / 3, zHind + (zFront - zHind) * k / 3));
    // tail: centre line along its real curl (path length from the root)
    const NT = 10, tc = chain(pts.tail, spine[0].clone().add(V(0, 0, -0.1)), NT, 0.035), tail = tc.j, tailTip = tc.tip;
    // neck and head: path length from the chest up the neck to the head
    const NN = 6, nc = chain(pts.head, spine[3].clone().add(V(0, 0.02, 0.04)), NN, 0.035), neck = nc.j;
    const snout = pts.head.reduce((b, p) => p.z > b.z ? p : b, neck[NN - 1]).clone();
    neck[NN - 1] = neck[NN - 2].clone().lerp(snout, 0.35); // the head pivots at the top of the neck and points at the snout (not the horns)
    // wings: leading edge (top of each span band)
    const wings = pts.wing.map((a, side) => {
      const sg = side ? -1 : 1, ax = a.map(p => Math.abs(p.x)), xMax = Math.max(...ax, 0.6), out = [];
      for (let k = 0; k < 4; k++) {
        const x0 = 0.2 + (xMax - 0.2) * k / 4, x1 = 0.2 + (xMax - 0.2) * (k + 1) / 4, band = a.filter((p, i) => ax[i] >= x0 && ax[i] < x1).sort((p, q) => q.y - p.y);
        const m = mean(band.slice(0, Math.max(1, Math.floor(band.length * 0.2))));
        out.push(m ? V(sg * (x0 + (k ? 0 : 0)), m.y, m.z) : V(sg * x0, spine[3].y + 0.15, spine[2].z));
      }
      const tip = a.reduce((b, p) => Math.abs(p.x) > Math.abs(b.x) ? p : b, out[3]).clone();
      out[0] = V(sg * 0.16, spine[3].y + 0.08, (spine[2].z + spine[3].z) / 2); // the shoulder sits on the back
      return { j: out, tip };
    });
    // ---- bones
    const bones = [], B = {};
    const bone = (name, parent, pos) => { const b = new T.Bone(); b.name = name; b.userData.w = pos.clone(); if (parent) { b.position.copy(pos).sub(parent.userData.w); parent.add(b); } else b.position.copy(pos); bones.push(b); B[name] = b; return b; };
    const hips = bone('hips', null, spine[0]); const s1 = bone('spine1', hips, spine[1]), s2 = bone('spine2', s1, spine[2]), chest = bone('chest', s2, spine[3]);
    let par = chest; neck.forEach((p, k) => { par = bone(k < NN - 1 ? 'neck' + k : 'head', par, p); });
    par = hips; tail.forEach((p, k) => { par = bone('tail' + k, par, p); });
    wings.forEach((w, s) => { let pp = chest; w.j.forEach((p, k) => { pp = bone('wing' + s + '_' + k, pp, p); }); });
    legJ.forEach((l, i) => { const at = i < 2 ? chest : hips; const a = bone('leg' + i + '_0', at, l.top), b2 = bone('leg' + i + '_1', a, l.knee), c = bone('leg' + i + '_2', b2, l.ankle); bone('leg' + i + '_3', c, l.toe); });
    // segments (bone -> next joint) per part, for the skin weights
    const seg = (name, a, b) => ({ b: bones.indexOf(B[name]), a, e: b });
    const parts = {
      body: [seg('hips', spine[0], spine[1]), seg('spine1', spine[1], spine[2]), seg('spine2', spine[2], spine[3]), seg('chest', spine[3], neck[0])],
      tail: tail.map((p, k) => seg('tail' + k, p, tail[k + 1] || tailTip)),
      head: neck.map((p, k) => seg(k < NN - 1 ? 'neck' + k : 'head', p, neck[k + 1] || snout)),
      wing: wings.map((w, s) => w.j.map((p, k) => seg('wing' + s + '_' + k, p, w.j[k + 1] || w.tip))),
      leg: legJ.map((l, i) => [seg('leg' + i + '_0', l.top, l.knee), seg('leg' + i + '_1', l.knee, l.ankle), seg('leg' + i + '_2', l.ankle, l.toe), seg('leg' + i + '_3', l.toe, l.tip)]),
    };
    // ---- skin
    const headJ = neck[NN - 1], neckTop = neck[NN - 2], headI = bones.indexOf(B.head);
    const nearest = (p, list, keep) => { const r = list.map(s => [s.b, 1 / (Math.pow(segDist(p, s.a, s.e), 4) + 1e-7)]).sort((x, y) => y[1] - x[1]).slice(0, keep); const t = r.reduce((a, x) => a + x[1], 0) || 1; return r.map(([b, w]) => [b, w / t]); };
    for (const g of geos) {
      const P = g.attributes.position, L = g.attributes.aLeg, Bm = g.attributes.aB, K = g.attributes.aWg, n = P.count;
      const SI = new Uint16Array(n * 4), SW = new Float32Array(n * 4), p = V(0, 0, 0);
      // sources: each wing's leading edge (elbow and wrist), the tail's first joints, and the spine
      const labels = surfaceLabels(g, [[wings[0].j[1], wings[0].j[2]], [wings[1].j[1], wings[1].j[2]], [tail[0], tail[1], tail[2]], [spine[0], spine[1], spine[2], spine[3]]]);
      for (let i = 0; i < n; i++) {
        p.set(P.getX(i), P.getY(i), P.getZ(i));
        let legs = [L.getX(i), L.getY(i), L.getZ(i), L.getW(i)], tl = Bm.getX(i), hd = Bm.getY(i), wg = Math.max(K.getX(i), K.getY(i));
        // out past the body: tail or wing by which is nearer across the surface (not in a straight line), so the wing's
        // trailing edge never sticks to the tail that lies under it
        if (Math.abs(p.x) > 0.3 && Math.max(...legs) < 0.5 && hd < 0.5) {
          const lb = labels[i];
          if (lb === 0 || lb === 1) { wg = 1; tl = 0; } else if (lb === 2) { tl = 1; wg = 0; } else if (Math.abs(p.x) > 0.42) { wg = tl > 0.5 ? 0 : 1; tl = tl > 0.5 ? 1 : 0; }
          if (lb !== 3) { hd = 0; legs = [0, 0, 0, 0]; }
        }
        // the skull (face, horns, frill) moves as one piece with the head bone
        if (hd > 0.3 && (p.distanceTo(headJ) < 0.19 || (p.y > neckTop.y - 0.03 && p.z > neckTop.z - 0.12))) { SI[i * 4] = headI; SW[i * 4] = 1; for (let c = 1; c < 4; c++) { SI[i * 4 + c] = 0; SW[i * 4 + c] = 0; } continue; }
        const acc = new Map(), addW = (list, m, keep) => { if (m <= 0.001) return; for (const [b, w] of nearest(p, list, keep)) acc.set(b, (acc.get(b) || 0) + w * m); };
        let rest = 1;
        legs.forEach((m, k) => { if (m > 0.001) { addW(parts.leg[k], m, 2); rest -= m; } });
        if (tl > 0.001) { addW(parts.tail, tl, 2); rest -= tl; }
        if (hd > 0.001) { addW(parts.head, hd, 2); rest -= hd; }
        if (wg > 0.001) { addW(parts.wing[p.x >= 0 ? 0 : 1], wg, 2); rest -= wg; }
        addW(parts.body, Math.max(0, rest), 2);
        const top = [...acc.entries()].sort((x, y) => y[1] - x[1]).slice(0, 4), s = top.reduce((a, x) => a + x[1], 0) || 1;
        top.forEach(([b, w], k) => { SI[i * 4 + k] = b; SW[i * 4 + k] = w / s; });
      }
      g.setAttribute('skinIndex', new T.Uint16BufferAttribute(SI, 4)); g.setAttribute('skinWeight', new T.Float32BufferAttribute(SW, 4));
    }
    grp.add(hips);
    return { bones, B, NN, NT };
  }

  // ---- drive the bones from the pose targets (E: the eased joint values pets.js computes) plus 3D motion layers
  const q = new T.Quaternion(), eu = new T.Euler();
  const rot = (b, x, y, z) => { if (b) b.quaternion.setFromEuler(eu.set(x, y, z, 'YXZ')); };
  function drive(d, E, t, S, rig) {
    const B = d.B, tt = t * 1;
    // spine: arch (pitch) and a sinuous side sway, carried along the three bones
    const sway = E.sway + (S === 'walk' || S === 'run' ? 0 : 0.04 * Math.sin(tt * 0.025));
    rot(B.spine1, -E.arch * 0.3, sway * 0.5, 0); rot(B.spine2, -E.arch * 0.3, -sway * 0.3, 0); rot(B.chest, -E.arch * 0.3, -sway * 0.4, 0);
    // neck: an S-curve that coils back and strikes forward (lunge), head pitch / yaw spread down the chain, and now and
    // then the head turns to look out of the screen (at the camera) so the face reads in 3D
    const look = (S === 'idle' ? 0.55 : 0.2) * Math.pow(Math.max(0, Math.sin(tt * 0.006 + 1)), 3);
    const yawAll = E.hy + look, coil = -E.hl * 4; // hl > 0 stretches the neck out, < 0 coils it back
    for (let k = 0; k < d.NN - 1; k++) {
      const w = k / (d.NN - 2);
      rot(B['neck' + k], E.hp * 0.16 + coil * (k < 2 ? -0.22 : 0.18) + 0.03 * Math.sin(tt * 0.02 - k * 0.6), yawAll * 0.18 + 0.025 * Math.sin(tt * 0.013 - k * 0.5), 0.02 * Math.sin(tt * 0.017 - k));
    }
    rot(B.head, E.hp * 0.3 - coil * 0.1, yawAll * 0.25, 0);
    // tail: a travelling wave in 3D (side to side and up and down), growing toward the tip, plus the swish / lift
    for (let k = 0; k < d.NT; k++) {
      const g = 0.35 + k * 0.1, ph = E.tailT - k * 0.55;
      rot(B['tail' + k], -E.tl * 0.25 + 0.05 * Math.sin(ph * 0.7 + 0.8) * g, (E.wag * 2.2 * Math.sin(ph) + E.tsw * 0.35) * g, 0);
    }
    // wings: raise (shoulder roll), sweep (yaw), and the elbow / wrist / tip folding with the outer part lagging
    for (let s = 0; s < 2; s++) {
      const sg = s ? -1 : 1, w = k => B['wing' + s + '_' + k];
      rot(w(0), 0, sg * E.swp * 0.8, sg * (E.a1 - 0.15));
      rot(w(1), 0, sg * Math.max(0, -E.a2) * 0.5, sg * E.a2 * 0.7);
      rot(w(2), 0, sg * Math.max(0, -E.a2) * 0.6, sg * E.a2 * 0.5);
      rot(w(3), 0, 0, sg * E.a2 * 0.35);
    }
    // legs: hip / shoulder swing, knee bend (front legs bend back at the wrist, hind legs forward at the hock), ankle
    // keeps the foot roughly flat
    for (let i = 0; i < 4; i++) {
      const front = i < 2, hip = E.hip[i], kn = E.knee[i] * (front ? 1 : -1);
      rot(B['leg' + i + '_0'], -hip, 0, 0); rot(B['leg' + i + '_1'], kn * 0.9, 0, 0); rot(B['leg' + i + '_2'], (hip - kn * 0.9) * 0.6, 0, 0); rot(B['leg' + i + '_3'], 0, 0, 0);
    }
  }
  window.DRAGONRIG = { make, drive };
})();
