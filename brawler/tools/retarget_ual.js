// Retargets the Quaternius Universal Animation Library (CC0) clips onto each family member's Meshy skeleton.
// Run: node tools/retarget_ual.js <ual1.glb> <ual2.glb>   (needs Playwright; writes anims_ual.js)
// Method: for every mapped bone, world rotation(t) = source world(t) * inv(source rest) * inv(R) * target rest,
// where R turns the source rest bone direction onto the target rest direction (the library is T-pose, the scans are
// A-pose). Converted to local rotations, sampled at 30 fps and stored like anims.js (int16 quaternions + hips height).
const { chromium } = require('playwright'); const path = require('path'); const fs = require('fs');
const FAMILY = ['ray', 'norah', 'ollie', 'tyler', 'violet', 'layne', 'dad', 'mom', 'jayson'];
const MAP1 = { 'DEF-hips': 'Hips', 'DEF-spine001': 'Spine02', 'DEF-spine002': 'Spine01', 'DEF-spine003': 'Spine', 'DEF-neck': 'neck', 'DEF-head': 'Head',
  'DEF-shoulderL': 'LeftShoulder', 'DEF-upper_armL': 'LeftArm', 'DEF-forearmL': 'LeftForeArm', 'DEF-handL': 'LeftHand', 'DEF-shoulderR': 'RightShoulder', 'DEF-upper_armR': 'RightArm', 'DEF-forearmR': 'RightForeArm', 'DEF-handR': 'RightHand',
  'DEF-thighL': 'LeftUpLeg', 'DEF-shinL': 'LeftLeg', 'DEF-footL': 'LeftFoot', 'DEF-toeL': 'LeftToeBase', 'DEF-thighR': 'RightUpLeg', 'DEF-shinR': 'RightLeg', 'DEF-footR': 'RightFoot', 'DEF-toeR': 'RightToeBase' };
const MAP2 = { pelvis: 'Hips', spine_01: 'Spine02', spine_02: 'Spine01', spine_03: 'Spine', neck_01: 'neck', Head: 'Head',
  clavicle_l: 'LeftShoulder', upperarm_l: 'LeftArm', lowerarm_l: 'LeftForeArm', hand_l: 'LeftHand', clavicle_r: 'RightShoulder', upperarm_r: 'RightArm', lowerarm_r: 'RightForeArm', hand_r: 'RightHand',
  thigh_l: 'LeftUpLeg', calf_l: 'LeftLeg', foot_l: 'LeftFoot', ball_l: 'LeftToeBase', thigh_r: 'RightUpLeg', calf_r: 'RightLeg', foot_r: 'RightFoot', ball_r: 'RightToeBase' };
(async () => {
  const b = await chromium.launch({ args: ['--use-gl=swiftshader'] }); const p = await b.newPage();
  p.on('console', m => { if (m.type() === 'error') console.log('page:', m.text().slice(0, 300)); });
  await p.goto('file://' + path.resolve(__dirname, '..', 'index.html') + '?noao'); await p.waitForFunction('window.THREE && window.CLASH_MODELS', null, { timeout: 120000 });
  const out = {};
  for (const [file, MAP] of [[process.argv[2], MAP1], [process.argv[3], MAP2]]) {
    const src = fs.readFileSync(file).toString('base64');
    const res = await p.evaluate(async ({ src, MAP, FAMILY }) => {
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
        for (const clip of S.animations) {
          if (/TPose|_RM$|Driving|Swim|Pistol|Torch|Lantern|Phone|Farm|Chest_Open|Consume|Zombie|Rail|Sitting|Fixing|PickUp_Table|Push|Interact|TreeChopping|Walk_Carry|Walk_Formal/.test(clip.name)) continue;
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
          out['ual_' + clip.name] = { n, dur: +dur.toFixed(4), bones, rot: btoa(s), hy };
        }
        result[id] = out;
      }
      return result;
    }, { src, MAP, FAMILY });
    for (const id in res) out[id] = Object.assign(out[id] || {}, res[id]);
    console.log(path.basename(file), Object.keys(res.dad).length, 'clips');
  }
  fs.writeFileSync(path.resolve(__dirname, '..', 'anims_ual.js'), '// generated by tools/retarget_ual.js from the Quaternius Universal Animation Library 1 + 2 (CC0) - do not edit\nwindow.CLASH_ANIMS_UAL = ' + JSON.stringify(out) + ';\n');
  console.log('written', (fs.statSync(path.resolve(__dirname, '..', 'anims_ual.js')).size / 1e6).toFixed(1), 'MB'); await b.close();
})();
