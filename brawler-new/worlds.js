'use strict';
// Super Family Smash (90's Version) worlds: full 3D stages with textured surfaces, a sky dome, layered scenery in front of and behind
// the fight, lighting per world, generated 3D props (window.CLASH_PROPS) and scenery that reacts to the fight.
// render3d.js calls WORLDS.build(st, api) when a stage loads and WORLDS.update(api) every frame.
(() => {
  const TAU = Math.PI * 2;
  let T = null; // THREE, set on first build
  const rnd = (() => { let s = 1; return { seed(v) { s = v; }, next() { s = (s * 16807) % 2147483647; return s / 2147483647; } }; })();
  const R = (a, b) => a + rnd.next() * (b - a);

  // ------------------------------------------------------------ procedural textures (canvas), each with a matching bump
  const texCache = {};
  function canvasTex(key, w, h, draw, repX = 1, repY = 1) {
    if (texCache[key]) { const t = texCache[key]; return { map: t.map.clone(), bump: t.bump.clone(), rep: [repX, repY] }; }
    const c = document.createElement('canvas'); c.width = w; c.height = h; const g = c.getContext('2d');
    const b = document.createElement('canvas'); b.width = w; b.height = h; const bg = b.getContext('2d');
    draw(g, bg, w, h);
    const map = new T.CanvasTexture(c), bump = new T.CanvasTexture(b);
    map.colorSpace = T.SRGBColorSpace;
    for (const t of [map, bump]) { t.wrapS = t.wrapT = T.RepeatWrapping; t.anisotropy = 8; }
    texCache[key] = { map, bump };
    return { map: map.clone(), bump: bump.clone(), rep: [repX, repY] };
  }
  function texMat(tex, o) {
    for (const t of [tex.map, tex.bump]) { t.repeat.set(tex.rep[0], tex.rep[1]); t.needsUpdate = true; }
    return new T.MeshStandardMaterial(Object.assign({ map: tex.map, bumpMap: tex.bump, bumpScale: 2.2, roughness: 0.78, metalness: 0, envMapIntensity: 0.35 }, o || {}));
  }
  const noise = (g, w, h, n, a, col) => { for (let i = 0; i < n; i++) { g.fillStyle = col || `rgba(0,0,0,${Math.random() * a})`; g.fillRect(Math.random() * w, Math.random() * h, 1 + Math.random() * 2, 1 + Math.random() * 2); } };
  const TEX = {
    planks: (base = '#b07a45') => canvasTex('planks' + base, 512, 512, (g, b, w, h) => {
      const rows = 6, ph = h / rows;
      for (let r = 0; r < rows; r++) {
        const off = (r % 2) * w * 0.37; const shade = 0.82 + Math.random() * 0.3;
        g.fillStyle = base; g.globalAlpha = 1; g.fillRect(0, r * ph, w, ph);
        g.fillStyle = `rgba(${shade > 1 ? 255 : 0},${shade > 1 ? 230 : 0},${shade > 1 ? 200 : 0},${Math.abs(1 - shade) * 0.6})`; g.fillRect(0, r * ph, w, ph);
        for (let k = 0; k < 26; k++) { g.strokeStyle = `rgba(60,30,10,${0.08 + Math.random() * 0.12})`; g.lineWidth = 1 + Math.random() * 1.5; g.beginPath(); const y = r * ph + Math.random() * ph; g.moveTo(0, y); for (let x = 0; x <= w; x += 32) g.lineTo(x, y + Math.sin(x * 0.02 + k) * 3); g.stroke(); }
        if (Math.random() < 0.6) { g.fillStyle = 'rgba(70,35,12,.35)'; g.beginPath(); g.ellipse(Math.random() * w, r * ph + ph / 2, 10, 6, 0, 0, TAU); g.fill(); }
        g.fillStyle = 'rgba(40,20,8,.75)'; g.fillRect(0, r * ph, w, 3); g.fillRect((off + w * 0.6) % w, r * ph, 3, ph); g.fillRect(off % w, r * ph, 3, ph);
        b.fillStyle = '#999'; b.fillRect(0, r * ph, w, ph); b.fillStyle = '#222'; b.fillRect(0, r * ph, w, 4); b.fillRect((off + w * 0.6) % w, r * ph, 4, ph); b.fillRect(off % w, r * ph, 4, ph);
      }
      noise(g, w, h, 3000, 0.12);
    }),
    bark: () => canvasTex('bark', 256, 512, (g, b, w, h) => {
      g.fillStyle = '#5b3f27'; g.fillRect(0, 0, w, h); b.fillStyle = '#888'; b.fillRect(0, 0, w, h);
      for (let k = 0; k < 70; k++) { const x = Math.random() * w, ww = 4 + Math.random() * 10; g.fillStyle = `rgba(30,18,8,${0.3 + Math.random() * 0.4})`; g.fillRect(x, 0, ww * 0.4, h); b.fillStyle = '#222'; b.fillRect(x, 0, ww * 0.4, h); g.fillStyle = `rgba(140,100,60,${Math.random() * 0.25})`; g.fillRect(x + ww * 0.5, Math.random() * h, ww * 0.6, 20 + Math.random() * 80); }
      noise(g, w, h, 4000, 0.2);
    }, 2, 4),
    gingham: (c = '#e8493f') => canvasTex('gingham' + c, 256, 256, (g, b, w, h) => {
      g.fillStyle = '#fbf7ee'; g.fillRect(0, 0, w, h); const n = 8, s = w / n;
      g.fillStyle = c; g.globalAlpha = 0.5; for (let i = 0; i < n; i += 2) { g.fillRect(i * s, 0, s, h); g.fillRect(0, i * s, w, s); } g.globalAlpha = 1;
      g.fillStyle = c; for (let i = 0; i < n; i += 2) for (let j = 0; j < n; j += 2) g.fillRect(i * s, j * s, s, s);
      for (let y = 0; y < h; y += 2) { g.fillStyle = 'rgba(0,0,0,.035)'; g.fillRect(0, y, w, 1); }
      b.fillStyle = '#777'; b.fillRect(0, 0, w, h); for (let y = 0; y < h; y += 3) { b.fillStyle = '#999'; b.fillRect(0, y, w, 1); } for (let x = 0; x < w; x += 3) { b.fillStyle = '#666'; b.fillRect(x, 0, 1, h); }
    }, 6, 2),
    asphalt: () => canvasTex('asphalt', 512, 512, (g, b, w, h) => {
      g.fillStyle = '#55585f'; g.fillRect(0, 0, w, h); b.fillStyle = '#777'; b.fillRect(0, 0, w, h);
      for (let i = 0; i < 26000; i++) { const v = 60 + Math.random() * 70; g.fillStyle = `rgb(${v},${v},${v + 5})`; const x = Math.random() * w, y = Math.random() * h; g.fillRect(x, y, 1.5, 1.5); b.fillStyle = Math.random() < 0.5 ? '#aaa' : '#444'; b.fillRect(x, y, 1.5, 1.5); }
      g.strokeStyle = 'rgba(20,20,25,.6)'; g.lineWidth = 2; for (let k = 0; k < 4; k++) { g.beginPath(); let x = Math.random() * w, y = Math.random() * h; g.moveTo(x, y); for (let s = 0; s < 8; s++) { x += R(-30, 30); y += R(5, 30); g.lineTo(x, y); } g.stroke(); }
    }, 3, 1),
    chalk: () => canvasTex('chalk', 1024, 256, (g, b, w, h) => { // hopscotch and doodles in chalk, layered over asphalt
      g.clearRect(0, 0, w, h); b.fillStyle = '#808080'; b.fillRect(0, 0, w, h);
      g.lineWidth = 6; g.lineCap = 'round';
      const cols = ['#ffe9f2', '#d9f5ff', '#fff6c2', '#e3ffd9'];
      for (let i = 0; i < 6; i++) { g.strokeStyle = cols[i % 4]; g.globalAlpha = 0.75; g.strokeRect(330 + i * 62, 80, 58, 90); g.font = 'bold 40px sans-serif'; g.fillStyle = cols[i % 4]; g.fillText(String(i + 1), 345 + i * 62, 140); }
      g.strokeStyle = '#fff6c2'; g.beginPath(); g.arc(140, 120, 50, 0, TAU); g.stroke(); for (let k = 0; k < 10; k++) { const a = k / 10 * TAU; g.beginPath(); g.moveTo(140 + Math.cos(a) * 65, 120 + Math.sin(a) * 65); g.lineTo(140 + Math.cos(a) * 90, 120 + Math.sin(a) * 90); g.stroke(); }
      g.strokeStyle = '#ffd0e8'; g.beginPath(); g.moveTo(820, 160); g.bezierCurveTo(840, 60, 900, 60, 900, 110); g.bezierCurveTo(900, 60, 960, 60, 980, 160); g.lineTo(900, 220); g.closePath(); g.stroke();
      g.globalAlpha = 1;
    }),
    quilt: () => canvasTex('quilt', 512, 512, (g, b, w, h) => {
      const cols = ['#6c8cff', '#ff8fb8', '#ffd36b', '#7fd6b0', '#b48cff', '#ff9e6b'], n = 4, s = w / n;
      for (let i = 0; i < n; i++) for (let j = 0; j < n; j++) {
        g.fillStyle = cols[(i * 3 + j * 5) % cols.length]; g.fillRect(i * s, j * s, s, s);
        if ((i + j) % 2) { g.fillStyle = 'rgba(255,255,255,.35)'; for (let k = 0; k < 6; k++) { g.beginPath(); g.arc(i * s + R(10, s - 10), j * s + R(10, s - 10), R(4, 9), 0, TAU); g.fill(); } }
        else { g.strokeStyle = 'rgba(255,255,255,.35)'; g.lineWidth = 6; for (let k = -s; k < s; k += 22) { g.beginPath(); g.moveTo(i * s + k, j * s); g.lineTo(i * s + k + s, j * s + s); g.stroke(); } }
        g.strokeStyle = 'rgba(255,255,255,.8)'; g.setLineDash([7, 6]); g.lineWidth = 2; g.strokeRect(i * s + 6, j * s + 6, s - 12, s - 12); g.setLineDash([]);
        const gr = b.createRadialGradient(i * s + s / 2, j * s + s / 2, 4, i * s + s / 2, j * s + s / 2, s * 0.7); gr.addColorStop(0, '#bbb'); gr.addColorStop(1, '#333'); b.fillStyle = gr; b.fillRect(i * s, j * s, s, s);
      }
    }, 3, 1),
    brick: () => canvasTex('brick', 512, 512, (g, b, w, h) => {
      g.fillStyle = '#d9cfc0'; g.fillRect(0, 0, w, h); b.fillStyle = '#222'; b.fillRect(0, 0, w, h);
      const bw = 64, bh = 28; for (let r = 0; r * bh < h; r++) for (let c = -1; c * bw < w; c++) { const x = c * bw + (r % 2) * bw / 2, y = r * bh; const v = R(-18, 18); g.fillStyle = `rgb(${172 + v},${82 + v * 0.6},${60 + v * 0.5})`; g.fillRect(x + 2, y + 2, bw - 4, bh - 4); b.fillStyle = '#aaa'; b.fillRect(x + 2, y + 2, bw - 4, bh - 4); }
      noise(g, w, h, 5000, 0.15);
    }, 4, 2),
    wallpaper: (c1, c2) => canvasTex('wp' + c1 + c2, 256, 256, (g, b, w, h) => {
      g.fillStyle = c1; g.fillRect(0, 0, w, h); b.fillStyle = '#888'; b.fillRect(0, 0, w, h);
      g.fillStyle = c2; for (let i = 0; i < 4; i++) g.fillRect(i * 64 + 26, 0, 12, h);
      g.fillStyle = 'rgba(255,255,255,.18)'; for (let i = 0; i < 18; i++) { const x = R(0, w), y = R(0, h); g.beginPath(); for (let k = 0; k < 10; k++) { const a = k * Math.PI / 5, rr = k % 2 ? 3 : 7; g.lineTo(x + Math.cos(a) * rr, y + Math.sin(a) * rr); } g.fill(); }
    }, 10, 6),
    grass: () => canvasTex('grass', 256, 256, (g, b, w, h) => {
      g.fillStyle = '#5b9e45'; g.fillRect(0, 0, w, h); b.fillStyle = '#777'; b.fillRect(0, 0, w, h);
      for (let i = 0; i < 9000; i++) { const v = R(-30, 30); g.fillStyle = `rgb(${80 + v},${150 + v},${60 + v * 0.5})`; const x = R(0, w), y = R(0, h); g.fillRect(x, y, 1, 3); b.fillStyle = v > 0 ? '#bbb' : '#444'; b.fillRect(x, y, 1, 3); }
    }, 20, 8),
    // a sprig of leaves on a transparent card (tinted per instance)
    leaf: (shape = 'oak') => canvasTex('leaf' + shape, 256, 256, (g, b, w, h) => {
      g.clearRect(0, 0, w, h); b.fillStyle = '#808080'; b.fillRect(0, 0, w, h);
      const n = shape === 'hedge' ? 46 : 30;
      for (let i = 0; i < n; i++) {
        const a = Math.random() * TAU, d = Math.sqrt(Math.random()) * (w * 0.36), x = w / 2 + Math.cos(a) * d, y = h / 2 + Math.sin(a) * d;
        const L = shape === 'hedge' ? 16 + Math.random() * 10 : 26 + Math.random() * 18, W = L * (shape === 'hedge' ? 0.55 : 0.48), rot = a + Math.random() * 0.8 - 0.4;
        g.save(); g.translate(x, y); g.rotate(rot);
        const v = 150 + Math.random() * 105, gr = g.createLinearGradient(-L, 0, L, 0); gr.addColorStop(0, `rgb(${v * 0.62|0},${v * 0.62|0},${v * 0.62|0})`); gr.addColorStop(1, `rgb(${v|0},${v|0},${v|0})`);
        g.fillStyle = gr; g.beginPath(); g.moveTo(-L, 0); g.bezierCurveTo(-L * 0.3, -W, L * 0.5, -W * 0.9, L, 0); g.bezierCurveTo(L * 0.5, W * 0.9, -L * 0.3, W, -L, 0); g.fill();
        g.strokeStyle = 'rgba(40,40,40,.35)'; g.lineWidth = 1.4; g.beginPath(); g.moveTo(-L, 0); g.lineTo(L * 0.85, 0); g.stroke();
        g.restore();
      }
    }),
    blades: () => canvasTex('blades', 128, 128, (g, b, w, h) => {
      g.clearRect(0, 0, w, h); b.fillStyle = '#808080'; b.fillRect(0, 0, w, h);
      for (let i = 0; i < 34; i++) {
        const x = 10 + Math.random() * (w - 20), top = Math.random() * h * 0.45, lean = (Math.random() - 0.5) * 30, bw = 2 + Math.random() * 3;
        const gr = g.createLinearGradient(0, h, 0, top); const v = 170 + Math.random() * 85; gr.addColorStop(0, '#3a3a3a'); gr.addColorStop(1, `rgb(${v|0},${v|0},${v|0})`);
        g.fillStyle = gr; g.beginPath(); g.moveTo(x - bw, h); g.quadraticCurveTo(x + lean * 0.3, h * 0.6, x + lean, top); g.quadraticCurveTo(x + lean * 0.3 + bw * 0.4, h * 0.6, x + bw, h); g.fill();
      }
    }),
    rug: () => canvasTex('rug', 512, 512, (g, b, w, h) => {
      g.fillStyle = '#3c3f8f'; g.fillRect(0, 0, w, h); b.fillStyle = '#888'; b.fillRect(0, 0, w, h);
      for (let r = 240; r > 20; r -= 40) { g.strokeStyle = r % 80 ? '#ffd36b' : '#ff8fb8'; g.lineWidth = 14; g.beginPath(); g.arc(w / 2, h / 2, r, 0, TAU); g.stroke(); }
      noise(g, w, h, 20000, 0.18); noise(b, w, h, 20000, 0.5);
    }),
  };

  // ------------------------------------------------------------ small builders
  let api = null, world = null;
  const M = (c, o) => new T.MeshStandardMaterial(Object.assign({ color: c, roughness: 0.75, metalness: 0, envMapIntensity: 0.35 }, o || {}));
  const G2 = (c, k = 1) => new T.MeshStandardMaterial({ color: c, emissive: c, emissiveIntensity: k });
  function add(parent, geo, mat, x = 0, y = 0, z = 0, cast = true) { const m = new T.Mesh(geo, mat); m.position.set(x, y, z); m.castShadow = cast; m.receiveShadow = true; parent.add(m); return m; }
  function rbox(w, h, d, r = 6) { // rounded box: a beveled extrusion, centred on its middle
    r = Math.min(r, w / 2 - 1, h / 2 - 1, d / 2 - 1);
    const s = new T.Shape(), hw = w / 2 - r, hh = h / 2 - r, c = Math.min(r, hw * 0.9, hh * 0.9); // inner outline with rounded corners; the bevel adds r all round
    s.moveTo(-hw + c, -hh); s.lineTo(hw - c, -hh); s.quadraticCurveTo(hw, -hh, hw, -hh + c); s.lineTo(hw, hh - c); s.quadraticCurveTo(hw, hh, hw - c, hh); s.lineTo(-hw + c, hh); s.quadraticCurveTo(-hw, hh, -hw, hh - c); s.lineTo(-hw, -hh + c); s.quadraticCurveTo(-hw, -hh, -hw + c, -hh);
    const geo = new T.ExtrudeGeometry(s, { depth: Math.max(1, d - 2 * r), bevelEnabled: true, bevelSize: r, bevelThickness: r, bevelSegments: 4, curveSegments: 6 });
    geo.translate(0, 0, -(d - 2 * r) / 2); geo.computeVertexNormals();
    // world-space style UVs so textures tile evenly over the front, top and sides
    const p = geo.attributes.position, n = geo.attributes.normal, uv = geo.attributes.uv;
    for (let i = 0; i < p.count; i++) { const ax = Math.abs(n.getX(i)), ay = Math.abs(n.getY(i)); let u, v; if (ay > 0.6) { u = p.getX(i); v = p.getZ(i); } else if (ax > 0.6) { u = p.getZ(i); v = p.getY(i); } else { u = p.getX(i); v = p.getY(i); } uv.setXY(i, u / 256, v / 256); }
    return geo;
  }
  function skyDome(top, mid, bottom) {
    const mat = new T.ShaderMaterial({ side: T.BackSide, depthWrite: false, fog: false,
      uniforms: { a: { value: new T.Color(top) }, b: { value: new T.Color(mid) }, c: { value: new T.Color(bottom) } },
      vertexShader: 'varying vec3 vp; void main(){ vp = normalize(position); gl_Position = projectionMatrix * modelViewMatrix * vec4(position,1.0); }',
      fragmentShader: 'uniform vec3 a; uniform vec3 b; uniform vec3 c; varying vec3 vp; void main(){ float h = vp.y; vec3 col = h > 0.05 ? mix(b, a, smoothstep(0.05, 0.75, h)) : mix(b, c, smoothstep(0.05, -0.4, h)); gl_FragColor = vec4(col, 1.0); }' });
    const m = new T.Mesh(new T.SphereGeometry(9000, 32, 16), mat); m.renderOrder = -10; return m;
  }
  function softBlob(r, mat, x, y, z, sx = 1, sy = 1, sz = 1) { let g = new T.IcosahedronGeometry(r, 4); g.deleteAttribute('normal'); g.deleteAttribute('uv'); if (T.mergeVertices) g = T.mergeVertices(g, 0.5); const p = g.attributes.position; for (let i = 0; i < p.count; i++) { const v = new T.Vector3().fromBufferAttribute(p, i), k = 1 + Math.sin(v.x * 0.05 + v.y * 0.07) * 0.06 + Math.sin(v.z * 0.09) * 0.05; p.setXYZ(i, v.x * k, v.y * k, v.z * k); } g.computeVertexNormals(); const m = new T.Mesh(g, mat); m.position.set(x, y, z); m.scale.set(sx, sy, sz); m.castShadow = true; m.receiveShadow = true; return m; }

  // ------------------------------------------------------------ detail kit: real geometry instead of painted boxes
  const WIND = { value: 0 };
  // leaves and grass bend in the wind: the top of each card moves, the base stays put
  function windy(mat, amp) {
    mat.onBeforeCompile = sh => {
      sh.uniforms.uTime = WIND;
      sh.vertexShader = 'uniform float uTime;\n' + sh.vertexShader.replace('#include <begin_vertex>', `#include <begin_vertex>
        #ifdef USE_INSTANCING
          float wph = instanceMatrix[3].x * 0.011 + instanceMatrix[3].z * 0.017 + instanceMatrix[3].y * 0.007;
        #else
          float wph = 0.0;
        #endif
        transformed.x += sin(uTime * 1.7 + wph) * uv.y * ${amp.toFixed(2)};
        transformed.z += cos(uTime * 1.3 + wph * 1.3) * uv.y * ${(amp * 0.6).toFixed(2)};`);
    };
    return mat;
  }
  // a leafy canopy: a soft dark core plus hundreds of leaf cards on its surface, shaded darker inside and underneath
  function canopy(parent, x, y, z, rx, ry, rz, pal, n = 220, size = 120, shape = 'oak') {
    const g = new T.Group(); g.position.set(x, y, z); parent.add(g);
    const core = softBlob(1, M(pal[0], { roughness: 1 }), 0, 0, 0, rx * 0.82, ry * 0.82, rz * 0.82); g.add(core);
    const tex = TEX.leaf(shape); const mat = windy(new T.MeshStandardMaterial({ map: tex.map, alphaTest: 0.45, side: T.DoubleSide, roughness: 0.85, envMapIntensity: 0.4 }), size * 0.06);
    const geo = new T.PlaneGeometry(size, size); geo.translate(0, size * 0.3, 0);
    const im = new T.InstancedMesh(geo, mat, n); im.castShadow = false; im.receiveShadow = true;
    const o = new T.Object3D(), c = new T.Color(), lo = new T.Color(pal[0]), mid = new T.Color(pal[1]), hi = new T.Color(pal[2]), sunD = new T.Vector3(-0.4, 0.8, 0.45).normalize();
    for (let i = 0; i < n; i++) {
      const u = new T.Vector3(R(-1, 1), R(-0.7, 1), R(-1, 1)).normalize(), k = R(0.78, 1.04);
      o.position.set(u.x * rx * k, u.y * ry * k, u.z * rz * k);
      o.lookAt(o.position.x * 2, o.position.y * 2 + 40, o.position.z * 2); o.rotateZ(R(0, TAU)); o.rotateX(R(-0.5, 0.5)); o.scale.setScalar(R(0.75, 1.25));
      o.updateMatrix(); im.setMatrixAt(i, o.matrix);
      const t = Math.max(0, Math.min(1, 0.45 + u.y * 0.35 + u.dot(sunD) * 0.3 + R(-0.12, 0.12)));
      c.copy(lo).lerp(mid, Math.min(1, t * 1.6)); if (t > 0.62) c.lerp(hi, (t - 0.62) / 0.38); im.setColorAt(i, c);
    }
    g.add(im); return g;
  }
  // an organic trunk: furrowed bark, slightly wavy, flaring out into roots at the base
  function trunkGeo(h, r0, r1) {
    const geo = new T.CylinderGeometry(r1, r0, h, 56, 48, true), p = geo.attributes.position;
    for (let i = 0; i < p.count; i++) {
      const x = p.getX(i), y = p.getY(i), z = p.getZ(i), a = Math.atan2(z, x), v = (y + h / 2) / h;
      const flare = 1 + Math.pow(Math.max(0, 1 - v * 3.2), 3) * 0.8 * (1 + 0.45 * Math.sin(a * 5 + 1));
      const k = flare * (1 + 0.045 * Math.sin(a * 19 + v * 7) + 0.03 * Math.sin(a * 37 - v * 13) + 0.04 * Math.sin(v * 9 + a * 2));
      p.setXYZ(i, x * k + Math.sin(v * 5) * 18, y, z * k);
    }
    geo.computeVertexNormals(); return geo;
  }
  // a curved, tapering branch along a smooth path
  function branchGeo(pts, r0, r1) {
    const curve = new T.CatmullRomCurve3(pts.map(q => new T.Vector3(q[0], q[1], q[2]))), segs = 40, rad = 14;
    const geo = new T.TubeGeometry(curve, segs, 1, rad, false), p = geo.attributes.position, P = new T.Vector3(), V = new T.Vector3();
    for (let i = 0; i <= segs; i++) { curve.getPointAt(i / segs, P); const r = r0 + (r1 - r0) * Math.pow(i / segs, 0.8); for (let j = 0; j <= rad; j++) { const k = i * (rad + 1) + j; V.fromBufferAttribute(p, k).sub(P).multiplyScalar(r).add(P); p.setXYZ(k, V.x, V.y, V.z); } }
    geo.computeVertexNormals(); return { geo, curve };
  }
  // a floor of separate planks with gaps between them, each a slightly different shade; the tops sit at y = 0
  function plankFloor(parent, x1, x2, z1, z2, mat, pw = 30, th = 10, gap = 3) {
    const rows = Math.floor((z2 - z1) / (pw + gap)), list = [];
    for (let r = 0; r < rows; r++) { let x = x1 + (r % 3) * -60; while (x < x2) { const len = R(170, 330), a = Math.max(x, x1), b = Math.min(x + len, x2); if (b - a > 20) list.push([a, b, z1 + r * (pw + gap) + pw / 2 + gap / 2]); x += len + gap; } }
    const geo = rbox(100, th, pw, 3), im = new T.InstancedMesh(geo, mat, list.length), o = new T.Object3D(), c = new T.Color();
    list.forEach(([a, b, z], i) => { o.position.set((a + b) / 2, -th / 2 - R(0, 0.6), z); o.scale.set((b - a) / 100, 1, 1); o.rotation.set(0, R(-0.004, 0.004), 0); o.updateMatrix(); im.setMatrixAt(i, o.matrix); const v = R(0.78, 1.12); c.setRGB(v, v * R(0.95, 1), v * R(0.9, 1)); im.setColorAt(i, c); });
    im.castShadow = true; im.receiveShadow = true; parent.add(im); return im;
  }
  // tufts of grass blades over an area (crossed cards, swaying in the wind)
  function grassField(parent, x1, x2, z1, z2, n, y = 0, h = 46) {
    const tex = TEX.blades(), mat = windy(new T.MeshStandardMaterial({ map: tex.map, alphaTest: 0.4, side: T.DoubleSide, roughness: 1, envMapIntensity: 0.3 }), 6);
    const geo = new T.PlaneGeometry(h * 1.3, h); geo.translate(0, h / 2, 0);
    const im = new T.InstancedMesh(geo, mat, n), o = new T.Object3D(), c = new T.Color(), a = new T.Color('#4f8f38'), b = new T.Color('#9fd06a');
    for (let i = 0; i < n; i++) { o.position.set(R(x1, x2), y, R(z1, z2)); o.rotation.set(0, R(0, TAU), 0); o.scale.setScalar(R(0.7, 1.35)); o.updateMatrix(); im.setMatrixAt(i, o.matrix); im.setColorAt(i, c.copy(a).lerp(b, R(0, 1))); }
    im.receiveShadow = true; parent.add(im); return im;
  }
  // a plump pillow: thick in the middle, thin at the seams; top at y = 0
  function pillowGeo(w, h, d) {
    let geo = new T.BoxGeometry(w, h, d, 28, 8, 12); geo.deleteAttribute('normal'); if (T.mergeVertices) geo = T.mergeVertices(geo, 0.01);
    const p = geo.attributes.position;
    for (let i = 0; i < p.count; i++) {
      const nx = p.getX(i) / (w / 2), ny = p.getY(i) / (h / 2), nz = p.getZ(i) / (d / 2), puff = (1 - Math.pow(Math.abs(nx), 6)) * (1 - Math.pow(Math.abs(nz), 4));
      const yy = ny * (h / 2) * (0.25 + 0.75 * puff), xx = p.getX(i), zz = p.getZ(i) * (0.82 + 0.18 * puff);
      p.setXYZ(i, xx, yy - h * 0.5, zz);
    }
    geo.computeVertexNormals(); return geo;
  }
  // a tufted quilt top: pillowy squares with stitched dips between them, rolling down at the edges; peaks at y = 0
  function tuftedTop(w, d, cell, mat) {
    const geo = new T.PlaneGeometry(w, d, Math.round(w / 6), Math.round(d / 6)); geo.rotateX(-Math.PI / 2);
    const p = geo.attributes.position;
    for (let i = 0; i < p.count; i++) {
      const x = p.getX(i), z = p.getZ(i), puff = Math.sqrt(Math.abs(Math.sin(Math.PI * x / cell) * Math.sin(Math.PI * z / cell)));
      const e = Math.min(1, Math.min(w / 2 - Math.abs(x), d / 2 - Math.abs(z)) / 26);
      p.setY(i, -7 * (1 - puff) - (1 - e * e) * 16);
    }
    geo.computeVertexNormals(); return new T.Mesh(geo, mat);
  }

  // a whole tree: furrowed trunk, two curving limbs and a cluster of leafy canopies
  function tree(parent, x, y, z, h, r, pal, leaves = 200) {
    const g = new T.Group(); g.position.set(x, y, z); parent.add(g);
    const bt = TEX.bark(); bt.rep = [3, 4]; const bark = texMat(bt, { bumpScale: 6, roughness: 0.95 });
    add(g, trunkGeo(h, r * 1.3, r * 0.6), bark, 0, h / 2, 0);
    for (const sx of [-1, 1]) add(g, branchGeo([[0, h * 0.7, 0], [sx * h * 0.18, h * 0.86, 0], [sx * h * 0.34, h * 0.98, sx * 20]], r * 0.45, r * 0.18).geo, bark, 0, 0, 0);
    const cr = h * 0.32;
    canopy(g, 0, h * 1.08, 0, cr * 1.2, cr * 0.8, cr, pal, leaves, cr * 0.5);
    for (const sx of [-1, 1]) canopy(g, sx * h * 0.36, h * 0.98, sx * 30, cr * 0.85, cr * 0.6, cr * 0.75, pal, Math.round(leaves * 0.7), cr * 0.45);
    return g;
  }
  // ------------------------------------------------------------ props generated with Meshy (base64 GLBs)
  const propScenes = {}, propWait = {};
  function loadProp(name, cb) {
    if (propScenes[name]) return cb(propScenes[name]);
    const src = (window.CLASH_PROPS || {})[name]; if (!src || !T.GLTFLoader) return;
    (propWait[name] = propWait[name] || []).push(cb); if (propWait[name].length > 1) return;
    try {
      const s = atob(src), u8 = new Uint8Array(s.length); for (let i = 0; i < s.length; i++) u8[i] = s.charCodeAt(i);
      new T.GLTFLoader().parse(u8.buffer, '', gl => { propScenes[name] = gl.scene; gl.scene.traverse(o => { if (o.isMesh) { o.castShadow = true; o.receiveShadow = true; if (o.material) { o.material.metalness = Math.min(o.material.metalness || 0, 0.2); o.material.roughness = Math.max(o.material.roughness || 0.6, 0.5); } } }); for (const f of propWait[name]) f(gl.scene); propWait[name] = []; }, () => {});
    } catch (e) { /* the world still works without this prop */ }
  }
  // place a prop: its base on y, scaled to height h, turned by ry; registered so it can react to the fight
  function prop(parent, name, x, y, z, h, ry = 0, react = 1) {
    const holder = new T.Group(); holder.position.set(x, y, z); parent.add(holder);
    loadProp(name, scene => {
      const m = scene.clone(true), box = new T.Box3().setFromObject(m), size = box.getSize(new T.Vector3()), c = box.getCenter(new T.Vector3());
      const k = h / Math.max(1e-3, size.y); m.scale.setScalar(k); m.position.set(-c.x * k, -box.min.y * k, -c.z * k); m.rotation.y = 0;
      const turn = new T.Group(); turn.rotation.y = ry; turn.add(m); holder.add(turn);
    });
    if (react) world.wobblers.push({ o: holder, x, z, r: h * 0.6 + 40, a: 0, v: 0, k: react });
    return holder;
  }

  // ------------------------------------------------------------ shared stage pieces
  function mainSlab(g, m, mat, sideMat, depth, thick, r = 10) {
    const w = m.x2 - m.x1;
    const top = add(g, rbox(w, thick, depth, r), [sideMat, mat], (m.x1 + m.x2) / 2, -thick / 2, 0, false);
    // ExtrudeGeometry groups: 0 = caps (front/back), 1 = sides (incl. top); use the top material on the sides
    top.material = [sideMat, mat]; return top;
  }
  function platMesh(pw, mat, sideMat, thick = 14, depth = 120, r = 6) { const g = new T.Group(); const m = add(g, rbox(pw, thick, depth, r), [sideMat, mat], 0, -thick / 2, 0); m.receiveShadow = true; return g; }
  function lights(L) {
    const s = api.sun; s.color.set(L.sun); s.intensity = L.sunI * 1.15; s.position.set(L.sunPos[0], L.sunPos[1], L.sunPos[2]);
    const hemi = api.scene.children.find(o => o.isHemisphereLight); if (hemi) { hemi.color.set(L.sky); hemi.groundColor.set(L.ground); hemi.intensity = L.hemiI * (api.envTex ? 0.55 : 0.8); }
    const rim = api.scene.children.find(o => o.isDirectionalLight && o !== s); if (rim) { rim.color.set(L.rim); rim.intensity = L.rimI * 1.4; }
    api.renderer.toneMappingExposure = (L.exposure || 1) * 0.95;
  }
  function particles(g, n, spread, make) { const list = []; for (let i = 0; i < n; i++) { const p = make(i); g.add(p.o); list.push(p); } world.particles.push(...list); return list; }

  // ------------------------------------------------------------ the worlds
  const BUILD = {
    // TREEHOUSE: a plank deck built round a huge oak, sunny afternoon, leaves drifting, birds on the railing
    treehouse(g, st) {
      const m = st.main;
      lights({ sun: '#fff1d2', sunI: 3.0, sunPos: [-600, 1100, 900], sky: '#cfe8ff', ground: '#4b6b3a', hemiI: 1.5, rim: '#ffe2b0', rimI: 1.2, exposure: 1.0 });
      g.add(skyDome('#3d8fe8', '#a7d8ff', '#d9f0ff')); api.scene.fog = new T.Fog('#bfe3ff', 2200, 7000);
      const plank = TEX.planks('#b98552'), dark = TEX.planks('#7a5232');
      // deck: separate planks over dark joists, with a chunky front beam
      plankFloor(g, m.x1, m.x2, -128, 128, texMat(plank, { bumpScale: 1.5 }));
      add(g, rbox(m.x2 - m.x1 - 20, 22, 240, 4), M('#3a2515', { roughness: 1 }), 0, -22, 0, false);
      const beam = texMat(TEX.planks('#8a5a34'));
      add(g, rbox(m.x2 - m.x1 + 16, 34, 26, 5), beam, 0, -26, 132); add(g, rbox(m.x2 - m.x1 + 16, 34, 26, 5), beam, 0, -26, -132);
      for (let k = 0; k <= 8; k++) add(g, rbox(22, 30, 270, 4), texMat(TEX.planks('#6d4529')), m.x1 + 10 + k * (m.x2 - m.x1 - 20) / 8, -40, 0);
      // support beams under the deck, into the trunk
      const beamM = texMat(TEX.planks('#8a5a34'));
      for (const x of [m.x1 + 60, 0, m.x2 - 60]) { const b = add(g, rbox(26, 300, 26, 5), beamM, x, -170, -40); b.rotation.z = x ? -Math.sign(x) * 0.42 : 0; }
      // railing at the back of the deck
      const rail = texMat(TEX.planks('#9a6a3e'));
      for (let k = 0; k <= 10; k++) add(g, rbox(14, 84, 14, 4), rail, m.x1 + 20 + k * ((m.x2 - m.x1 - 40) / 10), 42, -118);
      add(g, rbox(m.x2 - m.x1 - 10, 12, 16, 5), rail, 0, 86, -118); add(g, rbox(m.x2 - m.x1 - 10, 9, 12, 4), rail, 0, 46, -118);
      // rope ladder hanging off the front-left
      const rope = M('#d8b47a', { roughness: 1 });
      for (const sx of [-1, 1]) add(g, new T.CylinderGeometry(2.2, 2.2, 260, 6), rope, m.x1 + 70 + sx * 26, -150, 128);
      for (let k = 0; k < 7; k++) add(g, rbox(64, 7, 8, 3), rail, m.x1 + 70, -40 - k * 34, 128);
      // the great oak, procedural trunk plus a generated tree for the canopy silhouette
      const barkT = TEX.bark(); barkT.rep = [5, 7]; const bark = texMat(barkT, { bumpScale: 7, roughness: 0.95 });
      add(g, trunkGeo(3700, 260, 150), bark, 0, -450, -470);
      const pal = ['#24572c', '#3f9346', '#8fd06a'], pal2 = ['#2a5e2a', '#4fa24a', '#b4dc6e'];
      // branches curve out of the trunk; each ends in a leafy canopy
      const limbs = [
        [[[-60, 200, -470], [-380, 380, -440], [-700, 520, -380], [-980, 600, -420]], 85, 22, [-1000, 640, -420, 330]],
        [[[60, 260, -470], [400, 430, -450], [720, 560, -400], [1000, 640, -440]], 88, 22, [1020, 690, -440, 340]],
        [[[0, 500, -480], [-120, 760, -520], [-320, 960, -560]], 70, 20, [-340, 1000, -580, 360]],
        [[[0, 520, -480], [160, 800, -520], [380, 980, -560]], 70, 20, [400, 1020, -590, 360]],
        [[[-40, -250, -460], [-360, -330, -420], [-680, -330, -380]], 62, 18, [-720, -320, -400, 250]],
        [[[40, -280, -460], [380, -360, -420], [720, -350, -390]], 62, 18, [760, -340, -410, 250]],
      ];
      limbs.forEach(([pts, r0, r1, c], i) => { add(g, branchGeo(pts, r0, r1).geo, bark, 0, 0, 0); const cn = canopy(g, c[0], c[1], c[2], c[3] * 1.25, c[3] * 0.8, c[3], i % 2 ? pal : pal2, 260, 120); world.sway.push({ o: cn, ph: i, amp: 0.012 }); });
      const top = canopy(g, 0, 1200, -650, 620, 380, 420, pal, 420, 140); world.sway.push({ o: top, ph: 9, amp: 0.006 });
      for (const [x, y, z, r] of [[-1500, 820, -1300, 420], [1500, 780, -1300, 420], [-600, 1400, -1100, 400], [700, 1380, -1100, 400]]) canopy(g, x, y, z, r * 1.2, r * 0.8, r, pal2, 260, 150);
      prop(g, 'treehouse', -640, 230, -430, 430, 0.5, 0.4);
      prop(g, 'birdhouse', m.x2 - 40, 0, -112, 120, -0.4, 1.4);
      tree(g, 1900, -1400, -1900, 2300, 120, ['#2f5e33', '#4f9150', '#a4d27a'], 220); tree(g, -2000, -1400, -2100, 2200, 115, ['#2f5e33', '#4f9150', '#a4d27a'], 220);
      // distant hills and clouds
      const hill = M('#7cc46a', { roughness: 1 }), hill2 = M('#9fd88a', { roughness: 1 }), leafC = M('#2f7d3f', { roughness: 0.9 });
      for (let k = -3; k <= 3; k++) { const h = softBlob(900, k % 2 ? hill : hill2, k * 1500, -1300, -3200, 1.4, 0.55, 0.6); g.add(h); }
      const cloud = M('#ffffff', { roughness: 1, emissive: '#ffffff', emissiveIntensity: 0.15 });
      for (let k = 0; k < 7; k++) { const c = new T.Group(); [[0, 0, 160], [170, 30, 120], [-160, 20, 120], [70, 80, 110]].forEach(([dx, dy, r]) => c.add(softBlob(r, cloud, dx, dy, 0, 1.3, 0.75, 1))); c.position.set(-3000 + k * 1000, 650 + (k % 3) * 260, -2400); g.add(c); world.drift.push({ o: c, vx: 0.25 + (k % 3) * 0.1, wrap: 3600 }); }
      // foreground leaves at the bottom corners, out of focus
      for (const sx of [-1, 1]) { const b = canopy(g, sx * 760, -560, 380, 300, 200, 160, ['#173d1d', '#2c6a30', '#5aa04a'], 200, 120); world.sway.push({ o: b, ph: sx, amp: 0.02 }); }
      for (let k = 0; k < 9; k++) canopy(g, -3600 + k * 900, -900 + (k % 2) * 120, -2700, 520, 380, 300, ['#3d6e3a', '#5f9a52', '#9fca7a'], 140, 220);
      // drifting leaves
      const leafGeo = new T.PlaneGeometry(14, 9); const leafMats = ['#5fc068', '#e0b040', '#d97a2f'].map(c => new T.MeshStandardMaterial({ color: c, side: T.DoubleSide, roughness: 1 }));
      particles(g, 26, 0, i => { const o = new T.Mesh(leafGeo, leafMats[i % 3]); o.position.set(R(-900, 900), R(-300, 900), R(-300, 200)); return { o, kind: 'leaf', vy: R(0.4, 1.1), ph: R(0, TAU) }; });
      // birds perched on the railing; they scatter on a big hit
      const birdM = M('#4a6fd6'), birdM2 = M('#e8493f'), beak = M('#ffb020');
      for (let k = 0; k < 3; k++) { const b = new T.Group(); add(b, new T.SphereGeometry(9, 12, 10), k % 2 ? birdM2 : birdM, 0, 9, 0); add(b, new T.SphereGeometry(6, 10, 8), k % 2 ? birdM2 : birdM, 7, 17, 0); add(b, new T.ConeGeometry(2.4, 7, 6), beak, 13, 17, 0).rotation.z = -Math.PI / 2; b.position.set(-200 + k * 170, 92, -118); g.add(b); world.birds.push({ o: b, home: b.position.clone(), fly: 0, vx: 0, vy: 0 }); }
    },
    // KITCHEN TABLE: a giant breakfast table, warm morning light through a big window, steam curling from mugs
    kitchen(g, st) {
      const m = st.main;
      lights({ sun: '#ffe6bf', sunI: 3.2, sunPos: [-900, 900, 600], sky: '#fff2dc', ground: '#a86d3d', hemiI: 1.4, rim: '#ffd9a0', rimI: 1.0, exposure: 1.0 });
      g.add(skyDome('#ffe0b0', '#ffe9cc', '#e7b27a')); api.scene.fog = new T.Fog('#f6dcb4', 2400, 8000);
      const cloth = TEX.gingham('#e8493f');
      mainSlab(g, m, texMat(cloth, { bumpScale: 0.8, roughness: 0.95 }), texMat(TEX.planks('#8a5a34')), 300, 40, 6);
      // tablecloth hanging down the front, with soft folds
      const drape = new T.PlaneGeometry(m.x2 - m.x1 + 30, 120, 40, 4); const dp = drape.attributes.position; for (let i = 0; i < dp.count; i++) { const x = dp.getX(i), y = dp.getY(i); dp.setZ(i, Math.sin(x * 0.06) * 5 * (0.5 - y / 120)); } drape.computeVertexNormals();
      const dm = texMat(TEX.gingham('#e8493f'), { side: T.DoubleSide, roughness: 0.95 }); dm.map.repeat.set(6, 1); add(g, drape, dm, 0, -60, 152);
      // table legs and the kitchen behind
      const wood = texMat(TEX.planks('#7a4c2a'));
      for (const sx of [-1, 1]) for (const sz of [-1, 1]) add(g, new T.CylinderGeometry(26, 20, 1500, 14), wood, sx * (m.x2 - 50), -790, sz * 110);
      add(g, new T.PlaneGeometry(9000, 4000), texMat(TEX.wallpaper('#f7e3bd', '#f1d29e'), { roughness: 1 }), 0, 400, -1300, false);
      const counter = M('#e9e2d6', { roughness: 0.4 }), cab = M('#4f8a7b', { roughness: 0.6 }), knob = M('#d9b45a', { metalness: 0.6, roughness: 0.3 });
      add(g, rbox(4200, 70, 600, 10), counter, 0, -260, -1020); add(g, rbox(4200, 700, 560, 12), cab, 0, -650, -1030);
      for (let k = -6; k <= 6; k++) { add(g, rbox(300, 600, 20, 10), M('#5c9c8b', { roughness: 0.6 }), k * 320, -650, -740); add(g, new T.SphereGeometry(12, 10, 8), knob, k * 320 + 110, -420, -725); }
      for (let k = -5; k <= 5; k++) if (Math.abs(k) > 1) { add(g, rbox(300, 420, 300, 10), cab, k * 320, 900, -1180); }
      // the big window, glowing with morning light
      const view = document.createElement('canvas'); view.width = 1024; view.height = 720; { const v = view.getContext('2d');
        const sk = v.createLinearGradient(0, 0, 0, 720); sk.addColorStop(0, '#7fc4ff'); sk.addColorStop(0.6, '#d8efff'); sk.addColorStop(1, '#fff4d6'); v.fillStyle = sk; v.fillRect(0, 0, 1024, 720);
        v.fillStyle = 'rgba(255,255,255,.85)'; for (let k = 0; k < 5; k++) { const cx = 120 + k * 210, cy = 90 + (k % 2) * 50; for (let j = 0; j < 5; j++) { v.beginPath(); v.arc(cx + j * 26 - 50, cy + Math.sin(j * 2) * 10, 26 + (j % 2) * 10, 0, TAU); v.fill(); } }
        v.fillStyle = '#9ccf7a'; v.beginPath(); v.moveTo(0, 520); for (let x = 0; x <= 1024; x += 32) v.lineTo(x, 500 - Math.sin(x * 0.006) * 40); v.lineTo(1024, 720); v.lineTo(0, 720); v.fill();
        for (let k = 0; k < 14; k++) { const x = k * 80 + Math.random() * 30, y = 470 + Math.random() * 40, r = 50 + Math.random() * 50; v.fillStyle = '#5a3a22'; v.fillRect(x - 6, y, 12, 90); for (let j = 0; j < 9; j++) { v.fillStyle = ['#3f8a3f', '#5fae4f', '#86c95f'][j % 3]; v.beginPath(); v.arc(x + Math.random() * r - r / 2, y - Math.random() * r * 0.9, r * 0.45, 0, TAU); v.fill(); } }
        v.fillStyle = '#ffffff'; v.fillRect(0, 600, 1024, 14); for (let x = 0; x < 1024; x += 40) v.fillRect(x, 560, 12, 60);
        v.fillStyle = '#6fb653'; v.fillRect(0, 618, 1024, 102); }
      const viewT = new T.CanvasTexture(view); viewT.colorSpace = T.SRGBColorSpace;
      const win = new T.Group(); add(win, new T.PlaneGeometry(1300, 900), new T.MeshBasicMaterial({ map: viewT, toneMapped: false }), 0, 0, -12, false); const fr = M('#fbf7ee');
      for (const sx of [-1, 1]) { const cur = new T.PlaneGeometry(260, 1100, 30, 1), cp = cur.attributes.position; for (let i = 0; i < cp.count; i++) cp.setZ(i, Math.sin(cp.getX(i) * 0.09) * 14); cur.computeVertexNormals(); add(win, cur, M('#f6efe0', { side: T.DoubleSide, roughness: 1 }), sx * 760, -20, 40); }
      for (const x of [-650, 0, 650]) add(win, rbox(40, 960, 40, 8), fr, x, 0, 10); for (const y of [-450, 0, 450]) add(win, rbox(1340, 40, 40, 8), fr, 0, y, 10);
      win.position.set(-300, 760, -1270); g.add(win);
      const sunbeam = new T.Mesh(new T.PlaneGeometry(900, 2600), new T.MeshBasicMaterial({ color: '#fff3d0', transparent: true, opacity: 0.12, depthWrite: false })); sunbeam.position.set(-200, 200, -600); sunbeam.rotation.z = 0.6; g.add(sunbeam);
      // pendant lamps
      for (const x of [-700, 700]) { add(g, new T.CylinderGeometry(1.5, 1.5, 700, 6), M('#333'), x, 1150, -500); add(g, new T.ConeGeometry(90, 70, 24, 1, true), M('#2f6f62', { side: T.DoubleSide }), x, 780, -500); const bulb = add(g, new T.SphereGeometry(22, 14, 10), G2('#fff1c8', 2), x, 755, -500, false); const pl = new T.PointLight('#ffd9a0', 0.9, 1400, 1.6); pl.position.copy(bulb.position); g.add(pl); }
      // breakfast on the table
      prop(g, 'mug', -560, 0, -125, 115, 0.6, 1.2); prop(g, 'mug', 600, 0, -130, 100, 2.4, 1.2);
      prop(g, 'fruitbowl', -40, 0, -135, 95, 0, 0.9);
      prop(g, 'toaster', m.x1 - 160, -260, -980, 260, 0.3, 0);
      const shaker = (x, c) => { const s = new T.Group(); add(s, new T.CylinderGeometry(26, 30, 100, 20), M(c, { roughness: 0.2 }), 0, 50, 0); add(s, new T.CylinderGeometry(20, 26, 22, 20), M('#c9ced6', { metalness: 0.8, roughness: 0.25 }), 0, 111, 0); s.position.set(x, 0, -110); g.add(s); world.wobblers.push({ o: s, x, z: -110, r: 90, a: 0, v: 0, k: 1.6 }); };
      shaker(-500, '#fbf7ee'); shaker(-440, '#333842');
      // steam rising from the mugs
      const steamM = new T.MeshBasicMaterial({ color: '#ffffff', transparent: true, opacity: 0.18, depthWrite: false });
      for (const x of [-560, 600]) particles(g, 6, 0, i => { const o = new T.Mesh(new T.SphereGeometry(12, 8, 6), steamM.clone()); o.position.set(x, 110 + i * 26, -128); return { o, kind: 'steam', x0: x, y0: 105, ph: i / 6 }; });
      // a slice of toast pops up from the toaster now and then
      const toast = add(g, rbox(90, 100, 18, 12), M('#d99a4e', { roughness: 1 }), m.x1 - 160, -140, -980); world.toast = { o: toast, y0: -140, t: R(100, 300), vy: 0 };
    },
    // DRIVEWAY: the house at golden hour, chalk on the asphalt, hoop and bike, cones that get knocked flying
    driveway(g, st) {
      const m = st.main;
      lights({ sun: '#ffc27a', sunI: 3.4, sunPos: [-1400, 600, 700], sky: '#ffd8a8', ground: '#5a6b3a', hemiI: 1.3, rim: '#ff9a5a', rimI: 1.6, exposure: 1.0 });
      g.add(skyDome('#6a6ad8', '#ffae6b', '#ffd9a0')); api.scene.fog = new T.Fog('#f7b98a', 2600, 9000);
      const asphalt = texMat(TEX.asphalt(), { roughness: 0.95, bumpScale: 3 });
      mainSlab(g, m, asphalt, texMat(TEX.asphalt(), { color: '#9a9aa2' }), 320, 60, 8);
      const chalk = TEX.chalk(); const cm = new T.MeshStandardMaterial({ map: chalk.map, transparent: true, roughness: 1, depthWrite: false, polygonOffset: true, polygonOffsetFactor: -3, polygonOffsetUnits: -3 }); const ch = add(g, new T.PlaneGeometry(m.x2 - m.x1 - 20, 200), cm, 0, 0.6, 10, false); ch.rotation.x = -Math.PI / 2;
      // lawn, kerb and the house
      const grass = texMat(TEX.grass(), { roughness: 1 }); const lawn = add(g, new T.PlaneGeometry(12000, 7000), grass, 0, -8, -1500, false); lawn.rotation.x = -Math.PI / 2;
      const brick = texMat(TEX.brick(), { bumpScale: 3 });
      add(g, rbox(3000, 1100, 80, 6), brick, 0, 520, -1400);
      const door = add(g, rbox(1100, 720, 30, 12), M('#f2f2ee', { roughness: 0.5 }), 0, 360, -1350); for (let k = 1; k < 6; k++) add(g, rbox(1100, 6, 8, 3), M('#cfcfc9'), 0, k * 120, -1332);
      const drive = add(g, new T.PlaneGeometry(1200, 1100), texMat(TEX.asphalt(), { roughness: 0.95, polygonOffset: true, polygonOffsetFactor: -2, polygonOffsetUnits: -2 }), 0, -2, -850, false); drive.rotation.x = -Math.PI / 2;
      for (const x of [-1050, 1050]) { add(g, rbox(300, 260, 20, 8), G2('#ffd88a', 0.9), x, 640, -1356, false); add(g, rbox(320, 20, 30, 6), M('#fbf7ee'), x, 500, -1350); }
      const roof = add(g, new T.ConeGeometry(2200, 600, 4, 1), M('#6a4a4a', { roughness: 0.9 }), 0, 1370, -1580); roof.rotation.y = Math.PI / 4; roof.scale.z = 0.4;
      prop(g, 'hoop', m.x2 + 260, 0, -520, 560, -0.3, 0.5);
      prop(g, 'bike', m.x1 + 40, 0, -230, 170, 0.5, 1.0);
      const cones = [[m.x1 + 20, 70], [m.x2 - 20, 70], [m.x2 - 90, -120]].map(([x, z]) => prop(g, 'cone', x, 0, z, 80, 0, 2.2));
      world.cones = cones;
      // hedges, trees and the street beyond
      const hedge = M('#3f8f45', { roughness: 1 }), hedge2 = M('#57a85a', { roughness: 1 });
      for (let k = 0; k < 14; k++) { const x = -2200 + k * 340; if (Math.abs(x) > 600) canopy(g, x, 80, -1150 + (k % 2) * 40, 170, 120, 130, ['#1f4d22', '#3b8a3c', '#86c25a'], 220, 70, 'hedge'); }
      const gq = matchMedia('(pointer: coarse)').matches ? 0.45 : 1; grassField(g, -2600, 2600, 190, 320, Math.round(2600 * gq), -8, 11); grassField(g, -2600, 2600, -1150, -185, Math.round(7000 * gq), -8, 17);
      const kerb = M('#c9c5bb', { roughness: 0.9 }); add(g, rbox(m.x2 - m.x1 + 40, 16, 22, 5), kerb, 0, -2, 171); add(g, rbox(m.x2 - m.x1 + 40, 16, 22, 5), kerb, 0, -2, -171);
      tree(g, -2300, -10, -1700, 1300, 70, ['#5a4a1e', '#a0782e', '#ffc25a'], 200); tree(g, 2400, -10, -1800, 1250, 70, ['#3d5a22', '#7c9a3a', '#e0c060'], 200);
      const ball = add(g, new T.SphereGeometry(36, 24, 18), M('#ff7a1e', { roughness: 0.6 }), m.x1 + 160, 36, -120); world.ball = { o: ball, x: m.x1 + 160, vx: 0, vy: 0, y: 36, spin: 0, min: m.x1 + 40, max: m.x2 - 40 };
      // fireflies come out at dusk
      particles(g, 18, 0, () => { const o = new T.Mesh(new T.SphereGeometry(3, 6, 4), G2('#fff59a', 2.5)); o.position.set(R(-1400, 1400), R(80, 600), R(-1100, -300)); return { o, kind: 'fly', ph: R(0, TAU), base: o.position.clone() }; });
    },
    // BUNK BEDS: a bedroom at night, fighting on a quilt, pillows that squash, fairy lights, moonlight and toys
    bedroom(g, st) {
      const m = st.main;
      lights({ sun: '#ffe2c4', sunI: 2.4, sunPos: [-700, 1000, 700], sky: '#b8b4ff', ground: '#4a3050', hemiI: 1.1, rim: '#9fb4ff', rimI: 1.8, exposure: 1.15 });
      g.add(skyDome('#0b1035', '#1d2466', '#2a1f4a')); api.scene.fog = new T.Fog('#1d2060', 2200, 7000);
      const quilt = TEX.quilt();
      const side = texMat(TEX.quilt(), { roughness: 0.95 });
      mainSlab(g, m, side, side, 290, 80, 24).position.y = -62;
      const qt = TEX.quilt(); qt.rep = [(m.x2 - m.x1) / 256, 300 / 256]; const topM = texMat(qt, { roughness: 0.95, bumpScale: 3 });
      const qtop = tuftedTop(m.x2 - m.x1 + 4, 300, 64, topM); qtop.position.set((m.x1 + m.x2) / 2, 0, 0); qtop.receiveShadow = true; qtop.castShadow = true; g.add(qtop);
      // (no piping: the quilt rolls straight into the mattress)
      // pillow platforms (soft, they squash when you land)
      world.platMat = M('#f6efe4', { roughness: 1 });
      // the bunk bed frame around the fight, the top bunk above
      const frame = texMat(TEX.planks('#c58a4a'));
      for (const sx of [-1, 1]) for (const sz of [-1, 1]) add(g, rbox(40, 1400, 40, 8), frame, sx * (m.x2 - m.x1) / 2 + sx * 30, 440, sz * 165 - 20);
      add(g, rbox(m.x2 - m.x1 + 100, 50, 360, 10), frame, 0, 640, -20); add(g, rbox(m.x2 - m.x1 + 80, 70, 320, 24), texMat(TEX.quilt()), 0, 700, -20);
      for (let k = 1; k < 7; k++) add(g, rbox(90, 14, 20, 5), frame, m.x2 + 70, k * 95 - 60, 150);
      for (const sx of [-1, 1]) add(g, rbox(18, 700, 18, 5), frame, m.x2 + 70 + sx * 40, 240, 150);
      // the room: wallpaper, window with the moon, rug, shelf and toys
      add(g, new T.PlaneGeometry(9000, 4000), texMat(TEX.wallpaper('#3a3f7e', '#444a8e'), { roughness: 1 }), 0, 500, -1200, false);
      const rug = add(g, new T.CircleGeometry(1400, 48), texMat(TEX.rug(), { roughness: 1, polygonOffset: true, polygonOffsetFactor: -2, polygonOffsetUnits: -2 }), 0, -412, -500, false); rug.rotation.x = -Math.PI / 2; rug.scale.y = 0.6;
      const floor = add(g, new T.PlaneGeometry(9000, 4000), texMat(TEX.planks('#6b4a8a')), 0, -425, -1600, false); floor.rotation.x = -Math.PI / 2;
      const win = new T.Group(); add(win, new T.PlaneGeometry(800, 800), G2('#18287a', 1), 0, 0, 0, false); const moon = add(win, new T.SphereGeometry(130, 32, 20), G2('#fff6c8', 1.6), 150, 140, 30, false);
      for (let k = 0; k < 30; k++) add(win, new T.SphereGeometry(3, 6, 4), G2('#ffffff', 2), R(-380, 380), R(-380, 380), 5, false);
      const wf = M('#fbf7ee'); for (const x of [-410, 0, 410]) add(win, rbox(34, 860, 34, 8), wf, x, 0, 12); for (const y of [-410, 0, 410]) add(win, rbox(860, 34, 34, 8), wf, 0, y, 12);
      win.position.set(980, 700, -1180); g.add(win); const ml = new T.PointLight('#aebcff', 1.2, 3000, 1.4); ml.position.set(980, 700, -900); g.add(ml);
      // fairy lights along a curve; they flicker brighter on big hits
      const cols = ['#ff6ad5', '#ffd23f', '#6cf0ff', '#8affea', '#ff8a3d'];
      for (let k = 0; k <= 30; k++) { const u = k / 30, x = -1400 + u * 2800, y = 1050 - Math.pow(Math.sin(u * Math.PI * 3), 2) * 120; const b = add(g, new T.SphereGeometry(11, 10, 8), G2(cols[k % 5], 1.6), x, y, -1100, false); world.fairy.push(b); }
      for (let k = 0; k < 4; k++) { const pl = new T.PointLight(cols[k], 0.6, 1600, 1.6); pl.position.set(-1050 + k * 700, 980, -900); g.add(pl); }
      prop(g, 'teddy', m.x1 + 80, 0, -150, 170, 0.5, 1.0);
      prop(g, 'blocks', m.x2 - 110, 0, -150, 110, -0.4, 1.4);
      prop(g, 'lamp', -1050, -420, -600, 360, 0.3, 0); const lampL = new T.PointLight('#ffcf8a', 1.4, 1800, 1.5); lampL.position.set(-1050, -120, -520); g.add(lampL);
      prop(g, 'bunkbed', 1500, -425, -1250, 1100, -0.5, 0);
      // dust motes floating in the moonlight
      particles(g, 40, 0, () => { const o = new T.Mesh(new T.SphereGeometry(1.6, 5, 4), G2('#dfe6ff', 1.5)); o.position.set(R(-900, 900), R(-100, 800), R(-500, 100)); return { o, kind: 'mote', ph: R(0, TAU), base: o.position.clone() }; });
    },
  };

  // ------------------------------------------------------------ interface used by render3d.js
  window.WORLDS = {
    has(theme) { return !!BUILD[theme]; },
    build(st, a) {
      api = a; T = window.THREE; rnd.seed(7 + st.main.x2);
      world = { theme: st.theme, wobblers: [], sway: [], drift: [], particles: [], birds: [], fairy: [], cones: [], ball: null, toast: null, platMat: null, squash: new Map(), mem: new WeakMap(), flash: 0, t: 0 };
      a.scene.environment = a.envTex || null;
      const g = new T.Group(); BUILD[st.theme](g, st); a.stageGroup.add(g);
      document.getElementById('sky').style.background = '#000';
      return world;
    },
    // a platform to stand on, matching the world
    plat(st, p) {
      const pw = p.x2 - p.x1;
      if (st.theme === 'bedroom') { const g = new T.Group(); const pil = add(g, pillowGeo(pw + 10, 46, 120), world.platMat, 0, 0, 0); world.squash.set(g, { o: pil, s: 0, v: 0 }); return g; }
      const top = st.theme === 'kitchen' ? texMat(TEX.planks('#c98d55')) : st.theme === 'driveway' ? M('#9aa0aa') : texMat(TEX.planks('#b98552'));
      const side = st.theme === 'kitchen' ? M('#8a5a34') : texMat(TEX.planks('#7a5232'));
      const g = platMesh(pw, top, side, 16, 120, 6);
      if (st.theme === 'kitchen') { // a floating serving tray with handles
        for (const sx of [-1, 1]) add(g, new T.TorusGeometry(14, 4, 8, 16, Math.PI), M('#c9ced6', { metalness: 0.7, roughness: 0.3 }), sx * (pw / 2 + 4), -4, 0).rotation.z = sx * Math.PI / 2;
      }
      if (st.theme === 'treehouse') { // a swing made of planks, hanging from the branches on ropes
        g.clear(); const pm = texMat(TEX.planks('#b98552'), { bumpScale: 1.5 }); const inner = new T.Group(); g.add(inner);
        for (let k = 0; k < 4; k++) { const pl = add(inner, rbox(pw - R(0, 14), 12, 27, 3), pm, R(-4, 4), -6, -45 + k * 30); pl.material = pm; }
        for (const sx of [-1, 1]) add(inner, rbox(24, 14, 130, 3), texMat(TEX.planks('#6d4529')), sx * (pw / 2 - 30), -18, 0);
        const rope = M('#d8b47a', { roughness: 1 }); for (const sx of [-1, 1]) for (const sz of [-1, 1]) add(inner, new T.CylinderGeometry(2.2, 2.2, 640, 6), rope, sx * (pw / 2 - 12), 318, sz * 50);
        world.squash.set(g, { o: inner, s: 0, v: 0 }); return g;
      }
      world.squash.set(g, { o: g.children[0], s: 0, v: 0 });
      return g;
    },
    update(a) {
      if (!world) return; WIND.value = world.t / 60; const t = ++world.t, F = (window.G && G.fighters) || [];
      // react to the fight: big launches and landings shake nearby scenery, ring-outs shake everything
      let shock = 0;
      for (const f of F) {
        let m = world.mem.get(f); if (!m) { m = { st: f.state, gr: f.grounded, y: f.y }; world.mem.set(f, m); }
        const sp = Math.hypot(f.lvx || 0, f.lvy || 0), x = f.x, z = 0;
        const hit = f.state === 'hitstun' && m.st !== 'hitstun', land = f.grounded && !m.gr, ko = (f.state === 'dead' || f.state === 'out') && m.st !== 'dead' && m.st !== 'out';
        if (hit || land || (f.state === 'hitstun' && sp > 10)) {
          const power = ko ? 3 : hit ? 0.6 + Math.min(2, sp / 12) : land ? 0.35 : 0.25;
          for (const w of world.wobblers) { const d = Math.hypot(w.x - x, (w.z - z) * 0.5); if (d < w.r + 120) w.v += (Math.random() < 0.5 ? -1 : 1) * 0.035 * power * w.k * (1 - d / (w.r + 120)); }
          if (land && f.ground) for (const [g, s] of world.squash) if (Math.abs(g.position.x - f.x) < 200 && Math.abs(g.position.y + f.y) < 30) s.v -= 0.05;
          if (world.ball && Math.abs(world.ball.x - x) < 120 && f.grounded) { world.ball.vx += Math.sign(world.ball.x - x || 1) * (2 + sp * 0.4); world.ball.vy += 4 + sp * 0.3; }
          for (const c of world.cones) if (!c.userData.knocked && Math.abs(c.position.x - x) < 90 && (hit || sp > 6)) { c.userData.knocked = 1; c.userData.vx = Math.sign(c.position.x - x || 1) * R(4, 8); c.userData.vy = R(6, 10); c.userData.vr = R(-0.3, 0.3); }
          if (hit && sp > 14) shock = Math.max(shock, 1);
        }
        if (ko) shock = 2;
        m.st = f.state; m.gr = f.grounded;
      }
      if (shock) { world.flash = Math.max(world.flash, shock * 30); for (const b of world.birds) if (!b.fly) { b.fly = 1; b.vx = R(-6, 6); b.vy = R(5, 9); } for (const w of world.wobblers) w.v += R(-0.02, 0.02) * shock * w.k; }
      // springs
      for (const w of world.wobblers) { w.v += -w.a * 0.12; w.v *= 0.9; w.a += w.v; w.o.rotation.z = w.a; w.o.rotation.x = w.a * 0.4; }
      for (const [g, s] of world.squash) { s.v += -s.s * 0.2; s.v *= 0.82; s.s += s.v; s.o.scale.y = 1 + s.s; s.o.scale.x = 1 - s.s * 0.5; }
      for (const s of world.sway) { s.o.rotation.z = Math.sin(t * 0.01 + s.ph) * s.amp; }
      for (const d of world.drift) { d.o.position.x += d.vx; if (d.o.position.x > d.wrap) d.o.position.x = -d.wrap; }
      for (const p of world.particles) {
        const o = p.o;
        if (p.kind === 'leaf') { o.position.y -= p.vy; o.position.x += Math.sin(t * 0.02 + p.ph) * 1.2 + 0.4; o.rotation.set(t * 0.03 + p.ph, t * 0.02, t * 0.025 + p.ph); if (o.position.y < -500) { o.position.y = 950; o.position.x = R(-900, 900); } }
        else if (p.kind === 'steam') { const u = ((t * 0.006 + p.ph) % 1); o.position.set(p.x0 + Math.sin(u * 9 + p.ph * 6) * 14, p.y0 + u * 260, -160); o.scale.setScalar(0.5 + u * 1.8); o.material.opacity = 0.22 * Math.sin(u * Math.PI); }
        else if (p.kind === 'fly') { o.position.set(p.base.x + Math.sin(t * 0.013 + p.ph) * 60, p.base.y + Math.sin(t * 0.021 + p.ph * 2) * 40, p.base.z); o.material.emissiveIntensity = 1 + Math.max(0, Math.sin(t * 0.08 + p.ph * 3)) * 3; }
        else if (p.kind === 'mote') { o.position.set(p.base.x + Math.sin(t * 0.004 + p.ph) * 40, p.base.y + Math.sin(t * 0.006 + p.ph * 2) * 30, p.base.z); }
      }
      for (const b of world.birds) {
        if (b.fly === 1) { b.o.position.x += b.vx; b.o.position.y += b.vy; b.vy += 0.05; b.o.rotation.z = Math.sin(t * 0.8) * 0.4; if (b.o.position.y > 1600) { b.fly = 2; b.t = 240; } }
        else if (b.fly === 2 && --b.t <= 0) { b.fly = 0; b.o.position.copy(b.home); b.o.rotation.z = 0; }
        else if (!b.fly) b.o.position.y = b.home.y + Math.abs(Math.sin(t * 0.05 + b.home.x)) * (t % 300 < 30 ? 6 : 0);
      }
      for (const c of world.cones) { const u = c.userData; if (u.knocked === 1) { c.position.x += u.vx; c.position.y += u.vy; u.vy -= 0.45; c.rotation.z += u.vr; if (c.position.y < -900) { u.knocked = 2; u.t = 600; } } else if (u.knocked === 2 && --u.t <= 0) { u.knocked = 0; c.position.y = 0; c.rotation.z = 0; c.position.x = c.userData.home != null ? c.userData.home : c.position.x; } if (u.home == null) u.home = c.position.x; }
      if (world.ball) { const b = world.ball; b.vy -= 0.5; b.y += b.vy; if (b.y < 36) { b.y = 36; b.vy = -b.vy * 0.55; if (Math.abs(b.vy) < 1) b.vy = 0; } b.vx *= 0.985; b.x += b.vx; if (b.x < b.min || b.x > b.max) { b.vx = -b.vx * 0.7; b.x = Math.max(b.min, Math.min(b.max, b.x)); } b.o.position.set(b.x, b.y, -120); b.o.rotation.z -= b.vx / 36; }
      if (world.toast) { const s = world.toast; if (--s.t <= 0 && !s.vy) { s.vy = 11; s.t = R(300, 600); } if (s.vy || s.o.position.y > s.y0) { s.o.position.y += s.vy; s.vy -= 0.4; if (s.o.position.y <= s.y0) { s.o.position.y = s.y0; s.vy = 0; } } }
      if (world.fairy.length) { world.flash *= 0.95; for (let i = 0; i < world.fairy.length; i++) world.fairy[i].material.emissiveIntensity = 1.2 + Math.sin(t * 0.05 + i * 1.7) * 0.4 + world.flash * 0.08; }
    },
  };
})();
