// Light web copy of Tavin: keep a few animations, simplify the mesh, shrink the texture.
import { NodeIO } from '@gltf-transform/core';
import { simplify, weld, prune, dedup, quantize, resample } from '@gltf-transform/functions';
import { MeshoptSimplifier } from 'meshoptimizer';
import { execFileSync } from 'child_process';
import fs from 'fs';
const [src, out, texJpg] = process.argv.slice(2);
const KEEP = new Set(['Idle_Loop', 'Jog_Fwd_Loop', 'Walk_Loop', 'Hit_Knockback', 'Yes', 'Interact']);
const io = new NodeIO();
const doc = await io.read(src);
for (const a of doc.getRoot().listAnimations()) if (!KEEP.has(a.getName())) a.dispose();
await MeshoptSimplifier.ready;
const steps = [dedup(), resample(), prune()];
if (process.env.SIMPLIFY) steps.unshift(weld(), simplify({ simplifier: MeshoptSimplifier, ratio: +process.env.SIMPLIFY, error: 0.03 }));
if (process.env.QUANT) steps.push(quantize());
await doc.transform(...steps);
// swap in the smaller texture made by Python
for (const t of doc.getRoot().listTextures()) { t.setImage(fs.readFileSync(texJpg)); t.setMimeType('image/jpeg'); }
await io.write(out, doc);
console.log('tris', doc.getRoot().listMeshes().map(m => m.listPrimitives().map(p => p.getIndices()?.getCount() / 3)));
