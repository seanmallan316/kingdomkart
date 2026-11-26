// Retargets one fighter's Meshy animation-library clips onto other fighters' Meshy skeletons, so a new fighter gets the
// full library set (kicks, sweeps, spins, aerials) without buying it again. Same method as tools/retarget_ual.js.
// Run: node tools/retarget_meshy.js <dir of <src>_<clip>.glb> <src id> <id,id,...>   (writes anims_lib.js)
const { chromium } = require('playwright'); const path = require('path'); const fs = require('fs');
let FAMILY = process.argv[4].split(',');
const CLIPDIR = process.argv[2], SRC = process.argv[3];
const MB = ['Hips', 'Spine02', 'Spine01', 'Spine', 'neck', 'Head', 'LeftShoulder', 'LeftArm', 'LeftForeArm', 'LeftHand', 'RightShoulder', 'RightArm', 'RightForeArm', 'RightHand', 'LeftUpLeg', 'LeftLeg', 'LeftFoot', 'LeftToeBase', 'RightUpLeg', 'RightLeg', 'RightFoot', 'RightToeBase'];
const MAPM = Object.fromEntries(MB.map(n => [n, n]));
(async () => {
  const b = await chromium.launch({ args: ['--use-gl=swiftshader'] }); const p = await b.newPage();
  p.on('console', m => { if (m.type() === 'error') console.log('page:', m.text().slice(0, 300)); });
  await p.goto('file://' + path.resolve(__dirname, '..', 'index.html') + '?noao'); await p.waitForFunction('window.THREE && window.CLASH_MODELS', null, { timeout: 120000 });
  if (!FAMILY) FAMILY = await p.evaluate(() => Object.keys(window.CLASH_MODELS));
  const out = {};
  for (const fn of fs.readdirSync(CLIPDIR).filter(f => f.startsWith(SRC + '_') && f.endsWith('.glb')).sort()) {
    const file = path.join(CLIPDIR, fn), MAP = MAPM, CLIPNAME = fn.slice(SRC.length + 1, -4);
    const src = fs.readFileSync(file).toString('base64');
    const res = await p.evaluate(async ({ src, MAP, FAMILY, CLIPNAME }) => {
      const T = THREE, FPS = 30;
      const parse = b64 => new Promise((ok, no) => { const s = atob(b64), u = new Uint8Array(s.length); for (let i = 0; i < s.length; i++) u[i] = s.charCodeAt(i); new T.GLTFLoader().parse(u.buffer, '', ok, no); });
      const S = await parse(src); const sScene = S.scene; sScene.updateMatrixWorld(true);
      const sBones = {}; sScene.traverse(o => { if (o.isBone) sBones[o.name] = o; });
      const WQ = o => o.getWorldQuaternion(new T.Quaternion()), WP = o => o.getWorldPosition(new T.Vector3());
      // source facing: toes forward; turn the source so it faces +z like the scans
      const fwd = (n1, n2) => WP(sBones[n2]).sub(WP(sBones[n1]));
      const lf = Object.keys(MAP).find(k => MAP[k] === 'LeftFoot'), lt = Object.keys(MAP).find(k => MAP[k] === 'LeftToeBase');
      const f0 = fwd(lf, lt); f0.y = 0; f0.normalize(); const Y = new T.Quaternion().setFromUnitVectors(f0, new T.Vector3(0, 0, 1));
      const srcNames = Object.keys(MAP).filter(n => sBones[n]);
      const childOf = {}; for (const n of srcNames) { const kid = srcNames.find(k => sBones[k].parent === sBones[n] && !/shoulder|clavicle|thigh/i.test(k) || (sBones[k].parent === sBones[n] && srcNames.filter(q => sBones[q].parent === sBones[n]).length === 1)); if (kid) childOf[n] = kid; }
      // the spine's child is the next spine / neck, not a shoulder or thigh
      const sRestQ = {}, sRestDir = {}; const sHips = srcNames.find(n => MAP[n] === 'Hips'), sHipY = WP(sBones[sHips]).y;
      for (const n of srcNames) { sRestQ[n] = Y.clone().multiply(WQ(sBones[n])); if (childOf[n]) sRestDir[n] = WP(sBones[childOf[n]]).sub(WP(sBones[n])).applyQuaternion(Y).normalize(); }
      const result = {};
      for (const id of FAMILY) {
        const tg = await parse(CLASH_MODELS[id]); const tScene = tg.scene; tScene.updateMatrixWorld(true);
        const tB = {}; tScene.traverse(o => { if (o.isBone) tB[o.name] = o; });
        const C = {}, tRestQ = {}, order = [];
        tScene.traverse(o => { if (o.isBone) order.push(o.name); });
        for (const n of srcNames) {
          const tn = MAP[n]; if (!tB[tn]) continue; tRestQ[tn] = WQ(tB[tn]);
          let R = new T.Quaternion();
          if (childOf[n] && tB[MAP[childOf[n]]]) { const td = WP(tB[MAP[childOf[n]]]).sub(WP(tB[tn])).normalize(); R.setFromUnitVectors(sRestDir[n], td); }
          C[n] = sRestQ[n].clone().invert().multiply(R.clone().invert()).multiply(tRestQ[tn]);
        }
        const tHips = tB.Hips, tHipWorld = WP(tHips).y, perLocal = tHipWorld / Math.max(1e-6, tHips.position.y), hipK = tHipWorld / sHipY;
        const parentWQ = tHips.parent ? WQ(tHips.parent) : new T.Quaternion();
        const out = {};
        for (const clip of [S.animations.slice().sort((a, b) => b.tracks.length - a.tracks.length)[0]].filter(Boolean)) {
          if (false && /TPose|_RM$|Driving|Swim|Pistol|Torch|Lantern|Phone|Farm|Chest_Open|Consume|Zombie|Rail|Sitting|Fixing|PickUp_Table|Push|Interact|TreeChopping|Walk_Carry|Walk_Formal/.test(clip.name)) continue;
          const mixer = new T.AnimationMixer(sScene), act = mixer.clipAction(clip); act.play();
          const dur = clip.duration, n = Math.max(2, Math.round(dur * FPS) + 1);
          const bones = order.filter(b => srcNames.some(s => MAP[s] === b));
          const rot = new Int16Array(n * bones.length * 4), hy = [];
          const W = {};
          for (let i = 0; i < n; i++) {
            mixer.setTime(dur * i / (n - 1)); sScene.updateMatrixWorld(true);
            for (const s of srcNames) { const tn = MAP[s]; if (C[s]) W[tn] = Y.clone().multiply(WQ(sBones[s])).multiply(C[s]); }
            bones.forEach((bn, j) => {
              const par = tB[bn].parent, pw = par && par.isBone ? (W[par.name] || WQ(par)) : parentWQ;
              const q = pw.clone().invert().multiply(W[bn]).normalize();
              const o = (i * bones.length + j) * 4; rot[o] = Math.round(q.x * 32767); rot[o + 1] = Math.round(q.y * 32767); rot[o + 2] = Math.round(q.z * 32767); rot[o + 3] = Math.round(q.w * 32767);
            });
            hy.push(+(((WP(sBones[sHips]).y - sHipY) * hipK) / perLocal).toFixed(2));
          }
          const u8 = new Uint8Array(rot.buffer); let s = ''; for (let k = 0; k < u8.length; k++) s += String.fromCharCode(u8[k]);
          out[CLIPNAME] = { n, dur: +dur.toFixed(4), bones, rot: btoa(s), hy };
        }
        result[id] = out;
      }
      return result;
    }, { src, MAP, FAMILY, CLIPNAME });
    for (const id in res) out[id] = Object.assign(out[id] || {}, res[id]);
    
  }
  fs.writeFileSync(path.resolve(__dirname, '..', 'anims_lib.js'), '// generated by tools/retarget_meshy.js: ' + SRC + "'s Meshy library clips retargeted onto " + FAMILY.join(', ') + ' - do not edit\nwindow.CLASH_ANIMS = Object.assign(window.CLASH_ANIMS || {}, ' + JSON.stringify(out) + ');\n');
  console.log('written', Object.keys(out).map(k => k + ' ' + Object.keys(out[k]).length).join(', '), (fs.statSync(path.resolve(__dirname, '..', 'anims_lib.js')).size / 1e6).toFixed(1), 'MB'); await b.close();
})();
