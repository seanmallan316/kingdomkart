// Keeps only the listed triangles of a model's (first) mesh, e.g. to cut a scanned-on cape off a fighter.
// Run: node tools/cut_tris.mjs <in.glb> <out.glb> <keep_tris.json (array of triangle indices)>
// Needs @gltf-transform/core + extensions + functions (NODE_PATH to a folder that has them).
import { NodeIO } from '@gltf-transform/core';
import { ALL_EXTENSIONS } from '@gltf-transform/extensions';
import { prune } from '@gltf-transform/functions';
import fs from 'fs';
const [, , inFile, outFile, keepFile] = process.argv;
const keep = JSON.parse(fs.readFileSync(keepFile, 'utf8'));
const io = new NodeIO().registerExtensions(ALL_EXTENSIONS); const doc = await io.read(inFile);
const prim = doc.getRoot().listMeshes()[0].listPrimitives()[0], idx = prim.getIndices().getArray();
const out = new Uint32Array(keep.length * 3); keep.forEach((t, k) => out.set(idx.subarray(t * 3, t * 3 + 3), k * 3));
prim.getIndices().setArray(out); await doc.transform(prune()); await io.write(outFile, doc);
console.log('triangles', idx.length / 3, '->', keep.length);
