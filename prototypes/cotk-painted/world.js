// Shared block-out world for the painted-projection test.
// Every object is a few plain shapes. Each mesh carries a "hint" colour that is only used
// when we render the block-out for the AI to paint over. In the game the same meshes are
// covered by the painting, beamed on from the game camera direction (in the object's own space,
// so every copy of an object reuses one painting).
import * as THREE from 'three';

// The one fixed view direction (from the target towards the camera). The camera never turns.
export const VIEW_DIR = new THREE.Vector3(1, 1.15, 1).normalize();

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

function part(group, geo, hint, x, y, z, opts = {}) {
  const m = new THREE.Mesh(geo, null);
  m.position.set(x, y, z);
  if (opts.ry) m.rotation.y = opts.ry;
  if (opts.rx) m.rotation.x = opts.rx;
  if (opts.rz) m.rotation.z = opts.rz;
  m.userData.hint = hint;
  m.userData.solid = opts.solid !== false;
  group.add(m);
  return m;
}
const box = (w, h, d) => new THREE.BoxGeometry(w, h, d);
const cyl = (rt, rb, h, s = 12) => new THREE.CylinderGeometry(rt, rb, h, s);

export const OBJECTS = {
  cottage() {
    const g = new THREE.Group();
    part(g, box(7, 2.2, 3.4), '#e9e4d6', 0, 1.1, 0); // whitewashed walls
    part(g, gable(7.7, 4.1, 2.0), '#c9a24a', 0, 2.2, 0); // thatch
    part(g, box(0.8, 1.3, 0.8), '#8d8578', 2.6, 3.7, 0); // chimney
    part(g, box(0.95, 1.6, 0.08), '#3f6f8f', -0.6, 0.8, 1.72, { solid: false }); // blue door
    part(g, box(0.8, 0.7, 0.08), '#2b3440', 1.6, 1.3, 1.72, { solid: false }); // window
    part(g, box(0.8, 0.7, 0.08), '#2b3440', -2.6, 1.3, 1.72, { solid: false }); // window
    part(g, box(0.08, 0.7, 0.8), '#2b3440', 3.52, 1.3, 0, { solid: false }); // side window
    part(g, box(1.2, 0.5, 0.6), '#7a5a3a', 2.6, 0.25, 2.1); // peat stack
    return g;
  },
  tower() {
    const g = new THREE.Group();
    part(g, box(3.2, 7, 3.2), '#8f8678', 0, 3.5, 0); // stone tower
    const roof = cyl(0, 2.55, 2.6, 4);
    part(g, roof, '#4e5a66', 0, 8.3, 0, { ry: Math.PI / 4 }); // slate pyramid
    part(g, box(1.1, 2.0, 0.08), '#5a3b2a', 0, 1.0, 1.62, { solid: false }); // door
    part(g, box(0.4, 0.9, 0.08), '#2b3440', 0, 4.2, 1.62, { solid: false });
    part(g, box(0.08, 0.9, 0.4), '#2b3440', 1.62, 5.6, 0, { solid: false });
    part(g, box(3.5, 0.3, 3.5), '#a39a8b', 0, 7.0, 0); // string course
    return g;
  },
  stall() {
    const g = new THREE.Group();
    part(g, box(3.0, 1.0, 1.2), '#8a5a35', 0, 0.5, 0); // counter
    for (const [x, z] of [[-1.45, -0.55], [1.45, -0.55], [-1.45, 0.55], [1.45, 0.55]])
      part(g, cyl(0.07, 0.07, 2.6, 6), '#6b4a30', x, 1.3, z);
    part(g, box(0.6, 0.4, 0.5), '#b98a4a', -0.8, 1.2, 0); // crates and wares
    part(g, box(0.5, 0.35, 0.5), '#c45a3a', 0.1, 1.17, 0.1); // apples
    part(g, cyl(0.25, 0.25, 0.5, 10), '#7d6a4a', 0.9, 1.25, -0.1); // barrel of oats
    return g;
  },
  awning() {
    // separate piece so it can overlap the stall and the people under it (like the video)
    const g = new THREE.Group();
    part(g, box(3.5, 0.08, 1.9), '#b23a3a', 0, 0, 0, { rx: 0.32, solid: false });
    return g;
  },
  well() {
    const g = new THREE.Group();
    part(g, cyl(0.95, 1.0, 0.9, 16), '#8c8880', 0, 0.45, 0);
    part(g, cyl(0.7, 0.7, 0.06, 16), '#2d4a5a', 0, 0.9, 0, { solid: false }); // water
    part(g, box(0.15, 2.0, 0.15), '#6b4a30', -0.95, 1.0, 0);
    part(g, box(0.15, 2.0, 0.15), '#6b4a30', 0.95, 1.0, 0);
    part(g, gable(2.4, 1.7, 0.75), '#4e5a66', 0, 2.0, 0);
    part(g, cyl(0.2, 0.17, 0.3, 8), '#7a5a3a', 0.3, 1.3, 0, { solid: false }); // bucket
    return g;
  },
  pine() {
    const g = new THREE.Group();
    part(g, cyl(0.16, 0.22, 1.4, 8), '#6b4a30', 0, 0.7, 0);
    part(g, cyl(0, 1.5, 2.1, 9), '#2f5d3a', 0, 2.1, 0);
    part(g, cyl(0, 1.15, 1.8, 9), '#3a6d44', 0, 3.1, 0);
    part(g, cyl(0, 0.75, 1.5, 9), '#467a4c', 0, 4.0, 0);
    return g;
  },
  beacon() {
    const g = new THREE.Group();
    part(g, cyl(1.1, 1.3, 0.5, 14), '#8c8880', 0, 0.25, 0); // stone step
    part(g, cyl(0.45, 0.6, 1.0, 10), '#7a756c', 0, 1.0, 0); // pillar
    part(g, cyl(0.9, 0.45, 0.55, 12), '#3a3a3a', 0, 1.78, 0); // iron bowl
    return g;
  },
};

// Where things stand. Rotation is always 0 so every copy shares one painting.
export const LAYOUT = [
  { id: 'beacon', x: 0, z: 0 },
  { id: 'cottage', x: -6.5, z: -5.5 },
  { id: 'cottage', x: -9.5, z: 6.5 },
  { id: 'tower', x: 6.5, z: -7 },
  { id: 'stall', x: 6, z: 3.5 },
  { id: 'awning', x: 6, z: 3.5, y: 2.55 },
  { id: 'well', x: -3.5, z: 4 },
  { id: 'pine', x: 1.5, z: -10 },
  { id: 'pine', x: -12, z: -1 },
  { id: 'pine', x: 11, z: 9 },
  { id: 'pine', x: -2, z: 11.5 },
  { id: 'pine', x: 12, z: -2 },
  { id: 'pine', x: -12.5, z: -10 },
  { id: 'pine', x: 3, z: 10 },
];

// Dirt paths (polylines in x,z) and the cobbled square, used by the ground shader.
export const PATHS = [
  [[0, 0], [-4.5, -2.2]], [[0, 0], [4.2, -4.2]], [[0, 0], [4.5, 1.8]], [[0, 0], [-2.4, 3]],
  [[0, 0], [-7.5, 3.8]], [[0, 0], [0, 14]], [[0, 0], [14, 0]],
];
export const SQUARE_R = 3.6;
export const BOUNDS = 13.5;

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
