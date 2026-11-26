// Moves a Meshy rig's arm joints (elbow, wrist) to where the person's real arms are, from a pose found in their front
// photo. Use when the auto-rig ran the arm bones along something else (Super Joe's cape: the rig put his hands at the
// cape's tips). Only joint positions change (no rotations), and the inverse bind matrices are recomputed to match.
// Run: node tools/refit_arms.mjs <in.glb> <out.glb> <pose.json from the front photo>
//   pose.json: image pixels { shL, shR, elL, elR, wrL, wrR, hipL, hipR } (MediaPipe; L = the person's left)
// Needs @gltf-transform/core + extensions (NODE_PATH to a folder that has them).
import { NodeIO } from '@gltf-transform/core';
import { ALL_EXTENSIONS } from '@gltf-transform/extensions';
import fs from 'fs';
const [, , inFile, outFile, poseFile] = process.argv;
const pose = JSON.parse(fs.readFileSync(poseFile, 'utf8'));
const io = new NodeIO().registerExtensions(ALL_EXTENSIONS);
const doc = await io.read(inFile), root = doc.getRoot();
const skin = root.listSkins()[0], joints = skin.listJoints(), byName = Object.fromEntries(joints.map(j => [j.getName(), j]));
// ---- 4x4 helpers (column-major, like glTF)
const mul = (a, b) => { const o = new Array(16).fill(0); for (let c = 0; c < 4; c++) for (let r = 0; r < 4; r++) for (let k = 0; k < 4; k++) o[c * 4 + r] += a[k * 4 + r] * b[c * 4 + k]; return o; };
function inv(m) { const a = m, o = new Array(16); const [a00, a01, a02, a03, a10, a11, a12, a13, a20, a21, a22, a23, a30, a31, a32, a33] = a;
  const b00 = a00 * a11 - a01 * a10, b01 = a00 * a12 - a02 * a10, b02 = a00 * a13 - a03 * a10, b03 = a01 * a12 - a02 * a11, b04 = a01 * a13 - a03 * a11, b05 = a02 * a13 - a03 * a12,
    b06 = a20 * a31 - a21 * a30, b07 = a20 * a32 - a22 * a30, b08 = a20 * a33 - a23 * a30, b09 = a21 * a32 - a22 * a31, b10 = a21 * a33 - a23 * a31, b11 = a22 * a33 - a23 * a32;
  const d = 1 / (b00 * b11 - b01 * b10 + b02 * b09 + b03 * b08 - b04 * b07 + b05 * b06);
  o[0] = (a11 * b11 - a12 * b10 + a13 * b09) * d; o[1] = (a02 * b10 - a01 * b11 - a03 * b09) * d; o[2] = (a31 * b05 - a32 * b04 + a33 * b03) * d; o[3] = (a22 * b04 - a21 * b05 - a23 * b03) * d;
  o[4] = (a12 * b08 - a10 * b11 - a13 * b07) * d; o[5] = (a00 * b11 - a02 * b08 + a03 * b07) * d; o[6] = (a32 * b02 - a30 * b05 - a33 * b01) * d; o[7] = (a20 * b05 - a22 * b02 + a23 * b01) * d;
  o[8] = (a10 * b10 - a11 * b08 + a13 * b06) * d; o[9] = (a01 * b08 - a00 * b10 - a03 * b06) * d; o[10] = (a30 * b04 - a31 * b02 + a33 * b00) * d; o[11] = (a21 * b02 - a20 * b04 - a23 * b00) * d;
  o[12] = (a11 * b07 - a10 * b09 - a12 * b06) * d; o[13] = (a00 * b09 - a01 * b07 + a02 * b06) * d; o[14] = (a31 * b01 - a30 * b03 - a32 * b00) * d; o[15] = (a20 * b03 - a21 * b01 + a22 * b00) * d; return o; }
const xf = (m, p) => [m[0] * p[0] + m[4] * p[1] + m[8] * p[2] + m[12], m[1] * p[0] + m[5] * p[1] + m[9] * p[2] + m[13], m[2] * p[0] + m[6] * p[1] + m[10] * p[2] + m[14]];
const W = n => n.getWorldMatrix(), P = n => { const m = W(n); return [m[12], m[13], m[14]]; };
// ---- inverse bind matrices: check the formula on the original file first
const ibmAcc = skin.getInverseBindMatrices(), ibm = ibmAcc.getArray();
const meshNode = root.listNodes().find(n => n.getSkin() === skin), MW = meshNode ? W(meshNode) : [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1];
const calcIBM = j => inv(W(j)); // Meshy exports: IBM = inverse of the joint world matrix at bind (MW kept for reference)
let err = 0; joints.forEach((j, k) => { const c = calcIBM(j); for (let q = 0; q < 16; q++) err = Math.max(err, Math.abs(c[q] - ibm[k * 16 + q])); });
console.log('IBM formula check (max error, should be ~0):', err.toExponential(2));
if (process.env.DEBUG) { const e = f => { let m = 0; joints.forEach((j, k) => { const c = f(j); for (let q = 0; q < 16; q++) m = Math.max(m, Math.abs(c[q] - ibm[k * 16 + q])); }); return m.toExponential(2); };
  console.log('inv(W)', e(j => inv(W(j))), 'MW', JSON.stringify(MW.map(v => +v.toFixed(3))), 'meshNode', meshNode && meshNode.getName(), 'parent', meshNode && meshNode.getParentNode() && meshNode.getParentNode().getName());
  const j = byName.Hips, k = joints.indexOf(j); console.log('Hips W', JSON.stringify(W(j).map(v => +v.toFixed(3))), 'IBM', JSON.stringify(Array.from(ibm.slice(k * 16, k * 16 + 16)).map(v => +v.toFixed(3)))); }
if (err > 1e-3) { console.log('formula does not match this file; stopping'); process.exit(1); }
// ---- map photo pixels to model space: x from the shoulders, y from shoulder and hip heights
const sL = P(byName.LeftArm), sR = P(byName.RightArm), hL = P(byName.LeftUpLeg), hR = P(byName.RightUpLeg);
const ax = (sL[0] - sR[0]) / (pose.shL[0] - pose.shR[0]), bx = sL[0] - ax * pose.shL[0];
const ySh = (sL[1] + sR[1]) / 2, yHip = (hL[1] + hR[1]) / 2, iSh = (pose.shL[1] + pose.shR[1]) / 2, iHip = (pose.hipL[1] + pose.hipR[1]) / 2;
const ay = (ySh - yHip) / (iSh - iHip), by = ySh - ay * iSh;
const toModel = (px, z) => [ax * px[0] + bx, ay * px[1] + by, z];
for (const [side, s] of [['Left', 'L'], ['Right', 'R']]) {
  const arm = byName[side + 'Arm'], fore = byName[side + 'ForeArm'], hand = byName[side + 'Hand'], z = P(arm)[2];
  const before = [P(fore), P(hand)].map(p => p.map(v => v.toFixed(3)).join(','));
  for (const [bone, px] of [[fore, pose['el' + s]], [hand, pose['wr' + s]]]) {
    const target = toModel(px, z), parentW = W(bone.getParentNode());
    bone.setTranslation(xf(inv(parentW), target));
  }
  console.log(side, 'elbow/wrist', before.join(' | '), '->', [P(fore), P(hand)].map(p => p.map(v => v.toFixed(3)).join(',')).join(' | '));
}
// ---- new inverse bind matrices to match the moved joints
const out = new Float32Array(ibm.length); joints.forEach((j, k) => out.set(calcIBM(j), k * 16)); ibmAcc.setArray(out);
await io.write(outFile, doc); console.log('written', outFile);
