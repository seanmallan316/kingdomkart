// Turns video motion capture (tools/mocap_video.py JSON) into bone animation on every family member's Meshy skeleton.
// Run: node tools/retarget_video.js <mocap.json> <out.js> [segments.json]
// - MediaPipe world joints (metres, x right, y down, z away from the camera) become game space with
//   forward (+z, where the fighter faces) = the direction the performer strikes in the video (image right), up = +y.
// - Jitter is removed with a centred Gaussian filter (wider on the torso than the fast limbs).
// - Hips and chest are solved as full orientations from their left-right and up axes, the three spine bones blend
//   between them, the head follows the ears; arms and legs are solved bone by bone down each chain, turning the
//   bone (as carried by its parent) onto the tracked joint direction with the shortest rotation, so no twist builds up.
// - Body height (crouches, lunges, jumps) comes from the hips' height above the floor in the image.
// Output is stored like anims_ual.js: int16 quaternions per bone per frame + hips height, one clip per segment.
const { chromium } = require('playwright'); const path = require('path'); const fs = require('fs');
const FAMILY = ['ray', 'norah', 'ollie', 'tyler', 'violet', 'layne', 'dad', 'mom', 'jayson'];
const [, , mocapFile, outFile, segFile] = process.argv;
const mocap = JSON.parse(fs.readFileSync(mocapFile, 'utf8'));
const SEGS = segFile ? JSON.parse(fs.readFileSync(segFile, 'utf8')) : { mc_full: [0, mocap.frames.length / mocap.fps] };

// ---- prepare joints (Node side): fill gaps, convert axes, smooth
const F = mocap.frames, N = F.length, NJ = 33;
for (let i = 0; i < N; i++) if (!F[i].w) { const j = i ? i - 1 : F.findIndex(f => f.w); F[i].w = F[j].w; F[i].n = F[j].n; }
const P = F.map(f => f.w.map(([x, y, z]) => [z, -y, x]));          // world joints in game axes
const IMG = F.map(f => f.n.map(([x, y]) => [x, y]));               // image joints (0..1)
function smooth(arr, sigma, idx) {
  const r = Math.ceil(sigma * 3), K = []; let s = 0; for (let k = -r; k <= r; k++) { const w = Math.exp(-k * k / (2 * sigma * sigma)); K.push(w); s += w; }
  const out = arr.map(fr => fr.map(p => p.slice()));
  for (let i = 0; i < arr.length; i++) for (const j of idx) for (let c = 0; c < arr[0][j].length; c++) {
    let a = 0; for (let k = -r; k <= r; k++) a += arr[Math.min(arr.length - 1, Math.max(0, i + k))][j][c] * K[k + r]; out[i][j][c] = a / s;
  }
  return out;
}
const TORSO = [0, 7, 8, 11, 12, 23, 24], LIMB = [13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 25, 26, 27, 28, 29, 30, 31, 32];
let PS = smooth(P, 2.0, TORSO); PS = smooth(PS, 1.1, LIMB);
const IS = smooth(IMG, 1.5, [23, 24, 27, 28, 29, 30, 31, 32]);
// hips height above the floor, in metres: image height of the hips above the floor line, scaled by the body's real size
const mid = (a, b) => a.map((v, k) => (v + b[k]) / 2);
const floorY = (() => { const ys = []; for (let i = 0; i < N; i++) ys.push(Math.max(IS[i][29][1], IS[i][30][1], IS[i][31][1], IS[i][32][1])); ys.sort((a, b) => a - b); return ys[Math.floor(ys.length * 0.9)]; })();
const legM = (() => { const h = mid(PS[0][23], PS[0][24]), a = mid(PS[0][29], PS[0][30]); return h[1] - a[1]; })();
const legI = floorY - mid(IS[0][23], IS[0][24])[1];
const hipH = IS.map(fr => (floorY - mid(fr[23], fr[24])[1]) * legM / legI);
const restH = hipH.slice(Math.round(13 * mocap.fps), Math.round(14.5 * mocap.fps)).reduce((a, b) => a + b, 0) / Math.max(1, Math.round(1.5 * mocap.fps)) || hipH[0];

(async () => {
  const b = await chromium.launch({ args: ['--use-gl=swiftshader'] }); const p = await b.newPage();
  p.on('console', m => { if (m.type() === 'error') console.log('page:', m.text().slice(0, 300)); });
  await p.goto('file://' + path.resolve(__dirname, '..', 'index.html') + '?noao'); await p.waitForFunction('window.THREE && window.CLASH_MODELS', null, { timeout: 120000 });
  const res = await p.evaluate(async ({ PS, hipH, restH, legM, SEGS, fps, FAMILY }) => {
    const T = THREE;
    const parse = b64 => new Promise((ok, no) => { const s = atob(b64), u = new Uint8Array(s.length); for (let i = 0; i < s.length; i++) u[i] = s.charCodeAt(i); new T.GLTFLoader().parse(u.buffer, '', ok, no); });
    const V = a => new T.Vector3(a[0], a[1], a[2]), M = (a, b) => V(a).add(V(b)).multiplyScalar(0.5);
    const basis = (right, up) => { const x = right.clone().normalize(), y = up.clone().sub(x.clone().multiplyScalar(up.dot(x))).normalize(), z = new T.Vector3().crossVectors(x, y); return new T.Quaternion().setFromRotationMatrix(new T.Matrix4().makeBasis(x, y, z)); };
    // limb chains: bone -> [tracked start joint, tracked end joint]
    const LIMBS = [['LeftArm', 11, 13], ['LeftForeArm', 13, 15], ['LeftHand', 15, [17, 19]], ['RightArm', 12, 14], ['RightForeArm', 14, 16], ['RightHand', 16, [18, 20]],
      ['LeftUpLeg', 23, 25], ['LeftLeg', 25, 27], ['LeftFoot', 29, 31], ['RightUpLeg', 24, 26], ['RightLeg', 26, 28], ['RightFoot', 30, 32]];
    const J = (fr, j) => Array.isArray(j) ? M(fr[j[0]], fr[j[1]]) : V(fr[j]);
    const result = {};
    for (const id of FAMILY) {
      const g = await parse(CLASH_MODELS[id]), sc = g.scene; sc.updateMatrixWorld(true);
      const B = {}, order = []; sc.traverse(o => { if (o.isBone) { B[o.name] = o; order.push(o.name); } });
      const WP = n => B[n].getWorldPosition(new T.Vector3()), WQ = o => o.getWorldQuaternion(new T.Quaternion());
      const restW = {}, restL = {}; for (const n of order) { restW[n] = WQ(B[n]); restL[n] = B[n].quaternion.clone(); }
      const childPos = { LeftArm: 'LeftForeArm', LeftForeArm: 'LeftHand', RightArm: 'RightForeArm', RightForeArm: 'RightHand', LeftUpLeg: 'LeftLeg', LeftLeg: 'LeftFoot', LeftFoot: 'LeftToeBase', RightUpLeg: 'RightLeg', RightLeg: 'RightFoot', RightFoot: 'RightToeBase' };
      const restDir = {};
      for (const [n] of LIMBS) {
        let d; if (childPos[n] && B[childPos[n]]) d = WP(childPos[n]).sub(WP(n));
        else { const f = n.replace('Hand', 'ForeArm'); d = WP(n).sub(WP(f)); } // hands carry on along the forearm
        restDir[n] = d.normalize();
      }
      const rHips = basis(WP('LeftUpLeg').sub(WP('RightUpLeg')), WP('neck').sub(WP('Hips')));
      const rChest = basis(WP('LeftArm').sub(WP('RightArm')), WP('neck').sub(WP('Hips')));
      const rHead = basis(WP('LeftArm').sub(WP('RightArm')), (B.head_end ? WP('head_end') : WP('Head').add(new T.Vector3(0, 0.1, 0))).sub(WP('neck')));
      const tHipW = WP('Hips').y, perLocal = tHipW / Math.max(1e-6, B.Hips.position.y), hipK = tHipW / legM;
      const parentWQ = B.Hips.parent ? WQ(B.Hips.parent) : new T.Quaternion();
      const bones = order.filter(n => n !== 'head_end' && !/headfront/.test(n));
      const solve = fr => {
        const W = {};
        const hc = M(fr[23], fr[24]), cc = M(fr[11], fr[12]), ec = M(fr[7], fr[8]);
        const qH = basis(V(fr[23]).sub(V(fr[24])), cc.clone().sub(hc)).multiply(rHips.clone().invert());
        const qC = basis(V(fr[11]).sub(V(fr[12])), cc.clone().sub(hc)).multiply(rChest.clone().invert());
        const qD = basis(V(fr[7]).sub(V(fr[8])), ec.clone().sub(cc)).multiply(rHead.clone().invert());
        const set = (n, q) => { if (B[n]) W[n] = q.clone().multiply(restW[n]); };
        set('Hips', qH); set('Spine02', qH.clone().slerp(qC, 0.34)); set('Spine01', qH.clone().slerp(qC, 0.67)); set('Spine', qC);
        set('neck', qC.clone().slerp(qD, 0.5)); set('Head', qD);
        // anything not solved yet rides on its parent with its rest local rotation (shoulders, toes, fingers)
        const carry = n => { const par = B[n].parent; const pw = par && par.isBone ? (W[par.name] || carry(par.name)) : parentWQ; return W[n] || (W[n] = pw.clone().multiply(restL[n])); };
        for (const [n, a, e] of LIMBS) {
          if (!B[n]) continue;
          const par = B[n].parent, pw = par && par.isBone ? carry(par.name) : parentWQ;
          const prov = pw.clone().multiply(restL[n]);
          const d0 = restDir[n].clone().applyQuaternion(prov.clone().multiply(restW[n].clone().invert()));
          const d = J(fr, e).sub(J(fr, a)); if (d.lengthSq() < 1e-8) { W[n] = prov; continue; }
          W[n] = new T.Quaternion().setFromUnitVectors(d0, d.normalize()).multiply(prov);
        }
        for (const n of bones) carry(n);
        return W;
      };
      const out = {};
      for (const name in SEGS) {
        const [t0, t1] = SEGS[name], i0 = Math.max(0, Math.round(t0 * fps)), i1 = Math.min(PS.length - 1, Math.round(t1 * fps)), n = i1 - i0 + 1;
        const rot = new Int16Array(n * bones.length * 4), hy = [];
        for (let i = 0; i < n; i++) {
          const W = solve(PS[i0 + i]);
          bones.forEach((bn, j) => {
            const par = B[bn].parent, pw = par && par.isBone ? W[par.name] : parentWQ;
            let q = pw.clone().invert().multiply(W[bn]).normalize(); if (q.w < 0) q.set(-q.x, -q.y, -q.z, -q.w);
            const o = (i * bones.length + j) * 4; rot[o] = Math.round(q.x * 32767); rot[o + 1] = Math.round(q.y * 32767); rot[o + 2] = Math.round(q.z * 32767); rot[o + 3] = Math.round(q.w * 32767);
          });
          hy.push(+(((hipH[i0 + i] - restH) * hipK) / perLocal).toFixed(2));
        }
        const u8 = new Uint8Array(rot.buffer); let s = ''; for (let k = 0; k < u8.length; k++) s += String.fromCharCode(u8[k]);
        out[name] = { n, dur: +((n - 1) / fps).toFixed(4), bones, rot: btoa(s), hy };
      }
      result[id] = out;
    }
    return result;
  }, { PS, hipH, restH, legM, SEGS, fps: mocap.fps, FAMILY });
  fs.writeFileSync(outFile, '// generated by tools/retarget_video.js from the family\'s own reference video (Ray\'s moves) - do not edit\nwindow.CLASH_ANIMS_MC = ' + JSON.stringify(res) + ';\n');
  console.log('written', Object.keys(res.ray).length, 'clips,', (fs.statSync(outFile).size / 1e6).toFixed(2), 'MB'); await b.close();
})();
