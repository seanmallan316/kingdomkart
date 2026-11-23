// Shared block-out world for the painted-projection test, read from world.json
// (written by tools/make_world.py; the Godot scene reads the same file).
// Every object is a few plain shapes. Each mesh carries a "hint" colour that is only used when we
// render the block-out for the AI to paint over. In the game the same meshes are covered by the
// painting, beamed on from the game camera direction in the object's own space, so every copy of
// an object reuses one painting.
import * as THREE from 'three';

export const W = await (await fetch(new URL('./world.json', import.meta.url))).json();

// The one fixed view direction (from the target towards the camera). The camera never turns.
export const VIEW_DIR = new THREE.Vector3(...W.view_dir).normalize();

function gable(w, d, h) {
  // roof prism, ridge along x, base at y=0
  const x = w / 2, z = d / 2;
  const p = [
    -x, 0, z, x, 0, z, x, h, 0, -x, 0, z, x, h, 0, -x, h, 0, // front slope
    x, 0, -z, -x, 0, -z, -x, h, 0, x, 0, -z, -x, h, 0, x, h, 0, // back slope
    x, 0, z, x, 0, -z, x, h, 0, // right gable
    -x, 0, -z, -x, 0, z, -x, h, 0, // left gable
    -x, 0, -z, x, 0, -z, x, 0, z, -x, 0, -z, x, 0, z, -x, 0, z, // bottom
  ];
  const g = new THREE.BufferGeometry();
  g.setAttribute('position', new THREE.Float32BufferAttribute(p, 3));
  g.computeVertexNormals();
  return g;
}
const GEO = {
  box: s => new THREE.BoxGeometry(s[0], s[1], s[2]),
  cyl: s => new THREE.CylinderGeometry(s[0], s[1], s[2], s[3]),
  gable: s => gable(s[0], s[1], s[2]),
  ball: s => new THREE.SphereGeometry(s[0], 14, 10),
};

export function buildObject(id) {
  const g = new THREE.Group();
  for (const p of W.objects[id]) {
    const m = new THREE.Mesh(GEO[p.k](p.s), null);
    m.position.set(...p.p);
    m.rotation.set(p.rx || 0, p.ry || 0, 0);
    m.userData.hint = p.hint;
    m.userData.solid = p.solid;
    g.add(m);
  }
  return g;
}
export const OBJECT_IDS = Object.keys(W.objects);
export const LAYOUT = W.layout;
export const PATHS = W.paths;
export const BOUNDS = W.bounds;

// Orthographic projector for one object, in the object's own space.
export function projectorFor(group) {
  group.updateMatrixWorld(true);
  const bb = new THREE.Box3().setFromObject(group);
  const c = bb.getCenter(new THREE.Vector3());
  const cam = new THREE.OrthographicCamera(-1, 1, 1, -1, 0.1, 200);
  cam.position.copy(c).addScaledVector(VIEW_DIR, 50);
  cam.up.set(0, 1, 0);
  cam.lookAt(c);
  cam.updateMatrixWorld(true);
  // fit: project the 8 corners into view space
  const inv = cam.matrixWorldInverse;
  let mx = 0, my = 0;
  for (let i = 0; i < 8; i++) {
    const p = new THREE.Vector3(i & 1 ? bb.max.x : bb.min.x, i & 2 ? bb.max.y : bb.min.y, i & 4 ? bb.max.z : bb.min.z).applyMatrix4(inv);
    mx = Math.max(mx, Math.abs(p.x)); my = Math.max(my, Math.abs(p.y));
  }
  const half = Math.max(mx, my) * 1.12;
  Object.assign(cam, { left: -half, right: half, top: half, bottom: -half });
  cam.updateProjectionMatrix();
  const m = new THREE.Matrix4().multiplyMatrices(cam.projectionMatrix, cam.matrixWorldInverse);
  return { cam, matrix: m.toArray(), half };
}
