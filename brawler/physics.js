'use strict';
// Super Family Smash: real rigid-body physics (Jolt Physics, WebAssembly) for everything around the fighters.
// The fighters themselves keep their hand-tuned platform-fighter movement (it has to answer the same way every time);
// Jolt simulates the world they fight in:
//  - every stage gets physical props on the fight line (basketballs, cones, mugs, toy blocks, a teddy ...) that roll,
//    bounce, stack and tumble in full 3D; fighters shove them aside just by moving (each fighter is a kinematic capsule);
//  - any attack that touches a prop launches it along the attack's own knockback angle, harder for stronger moves;
//    a prop that has been launched is a weapon: if it hits another fighter fast enough it deals damage and knockback,
//    credited to whoever launched it (so you can serve a basketball into someone across the stage);
//  - pulse attacks blow props outward; props that leave the stage drop back in from above;
//  - item crates burst into planks that scatter, tumble and settle before fading.
// Online: the host's props are the truth; guests simulate too and are corrected from the host's snapshot.
(() => {
  if (typeof G === 'undefined' || typeof R3 === 'undefined' || !window.THREE) return;
  const T = window.THREE, S = 0.01; // 1 game unit = 1 cm
  const PH = window.PHYS = { ready: false, on: true, props: [], debris: [] };
  let J = null, jolt = null, sys = null, BI = null, v3 = null, rv3 = null, q4 = null, group = null, scene3 = null;
  const fighters = new Map(); // fighter -> body
  let statics = [], kin = [], seenBoxes = new Map();

  // ---------------------------------------------------------------- engine
  async function boot() {
    if (!window.JoltInit) return;
    try { J = await window.JoltInit(); } catch (e) { console.warn('physics unavailable', e); return; }
    const st = new J.JoltSettings(), of = new J.ObjectLayerPairFilterTable(2);
    of.EnableCollision(0, 1); of.EnableCollision(1, 1);
    const bpi = new J.BroadPhaseLayerInterfaceTable(2, 2);
    bpi.MapObjectToBroadPhaseLayer(0, new J.BroadPhaseLayer(0)); bpi.MapObjectToBroadPhaseLayer(1, new J.BroadPhaseLayer(1));
    st.mObjectLayerPairFilter = of; st.mBroadPhaseLayerInterface = bpi;
    st.mObjectVsBroadPhaseLayerFilter = new J.ObjectVsBroadPhaseLayerFilterTable(st.mBroadPhaseLayerInterface, 2, st.mObjectLayerPairFilter, 2);
    jolt = new J.JoltInterface(st); J.destroy(st);
    sys = jolt.GetPhysicsSystem(); BI = sys.GetBodyInterface();
    v3 = new J.Vec3(0, 0, 0); rv3 = new J.RVec3(0, 0, 0); q4 = new J.Quat(0, 0, 0, 1);
    v3.Set(0, -11.9, 0); sys.SetGravity(v3); // the game's own gravity (0.33 px / frame^2)
    PH.ready = true; if (G.mode === 'fight') build();
  }
  const V = (x, y, z) => { v3.Set(x, y, z); return v3; }, RV = (x, y, z) => { rv3.Set(x, y, z); return rv3; }, Q = (x, y, z, w) => { q4.Set(x, y, z, w); return q4; };
  function body(shape, x, y, z, motion, o = {}) {
    const cs = new J.BodyCreationSettings(shape, RV(x, y, z), Q(0, 0, 0, 1), motion, motion === J.EMotionType_Static ? 0 : 1);
    if (o.mass) { cs.mOverrideMassProperties = J.EOverrideMassProperties_CalculateInertia; cs.mMassPropertiesOverride.mMass = o.mass; }
    cs.mRestitution = o.rest != null ? o.rest : 0.3; cs.mFriction = o.fric != null ? o.fric : 0.6;
    if (o.damp != null) { cs.mLinearDamping = o.damp; cs.mAngularDamping = o.adamp != null ? o.adamp : 0.25; }
    const b = BI.CreateBody(cs); J.destroy(cs); BI.AddBody(b.GetID(), J.EActivation_Activate); return b;
  }
  const kill = b => { try { BI.RemoveBody(b.GetID()); BI.DestroyBody(b.GetID()); } catch (e) {} };
  const box = (hx, hy, hz) => new J.BoxShape(new J.Vec3(hx, hy, hz), Math.min(0.02, hx * 0.4, hy * 0.4, hz * 0.4), null);

  // ---------------------------------------------------------------- props: what each stage has lying around
  const KIT = {
    driveway: [['ball', -150, '#e8742a', 24], ['cone', 120], ['cone', 250], ['ball', 60, '#d94f2a', 20], ['ball', -280, '#f2f2f2', 18]],
    kitchen: [['mug', -160], ['apple', 90, '#c92a2a'], ['apple', 140, '#7ab82e'], ['orange', -40, '#f08c00'], ['mug', 250], ['toaster', -290]],
    bedroom: [['block', -120, '#e03131'], ['block', -96, '#1c7ed6'], ['block', -108, '#f59f00', 0, 1], ['teddy', 150], ['ball', 40, '#f783ac', 20], ['block', 240, '#37b24d']],
    treehouse: [['ball', -120, '#e8742a', 20], ['acorn', 60, '#8a5a2b'], ['acorn', 100, '#7a4a20'], ['birdhouse', 220], ['ball', -250, '#4dabf7', 18]],
  };
  // kind -> shape, weight, bounce, look
  const DEF = {
    ball: { r: 22, mass: 1.6, rest: 0.78, fric: 0.5, dmg: 1 },
    apple: { r: 13, mass: 1.0, rest: 0.45, fric: 0.6, dmg: 0.8 }, orange: { r: 14, mass: 1.1, rest: 0.5, fric: 0.6, dmg: 0.8 },
    acorn: { r: 13, mass: 1.2, rest: 0.4, fric: 0.7, dmg: 0.9 },
    block: { hx: 13, hy: 13, hz: 13, mass: 2.2, rest: 0.25, fric: 0.7, dmg: 1.1 },
    cone: { glb: 'cone', h: 62, cyl: [15, 31], mass: 2.0, rest: 0.35, fric: 0.7, dmg: 1 },
    mug: { glb: 'mug', h: 34, cyl: [14, 17], mass: 2.4, rest: 0.25, fric: 0.6, dmg: 1.2 },
    toaster: { glb: 'toaster', h: 46, hx: 30, hy: 23, hz: 18, mass: 5, rest: 0.15, fric: 0.7, dmg: 1.6 },
    teddy: { glb: 'teddy', h: 56, hx: 20, hy: 28, hz: 16, mass: 1.6, rest: 0.35, fric: 0.8, dmg: 0.7 },
    birdhouse: { glb: 'birdhouse', h: 52, hx: 20, hy: 26, hz: 20, mass: 3, rest: 0.2, fric: 0.7, dmg: 1.3 },
  };
  const glbs = {}, glbWait = {};
  function loadGLB(name, cb) {
    if (glbs[name]) return cb(glbs[name]);
    (glbWait[name] = glbWait[name] || []).push(cb); if (glbWait[name].length > 1) return;
    const src = (window.CLASH_PROPS || {})[name]; if (!src || !T.GLTFLoader) return;
    const s = atob(src), u8 = new Uint8Array(s.length); for (let i = 0; i < s.length; i++) u8[i] = s.charCodeAt(i);
    new T.GLTFLoader().parse(u8.buffer, '', gl => { glbs[name] = gl.scene; for (const f of glbWait[name]) f(gl.scene); glbWait[name] = []; }, () => {});
  }
  function look(kind, d, color, r) {
    const g = new T.Group();
    const std = (c, o) => new T.MeshStandardMaterial(Object.assign({ color: c, roughness: 0.6, metalness: 0.05 }, o || {}));
    const add = m => { m.castShadow = true; m.receiveShadow = true; g.add(m); return m; };
    if (d.glb) {
      loadGLB(d.glb, sc => { const m = sc.clone(true), bx = new T.Box3().setFromObject(m), size = bx.getSize(new T.Vector3()), c = bx.getCenter(new T.Vector3()), k = d.h / Math.max(1e-3, size.y); m.scale.setScalar(k); m.position.set(-c.x * k, -c.y * k, -c.z * k); m.traverse(o => { if (o.isMesh) { o.castShadow = true; o.receiveShadow = true; } }); g.add(m); });
    } else if (kind === 'block') {
      add(new T.Mesh(new T.BoxGeometry(d.hx * 2, d.hy * 2, d.hz * 2, 2, 2, 2), std(color, { roughness: 0.45 })));
      const dot = new T.Mesh(new T.CircleGeometry(d.hx * 0.45, 20), std('#ffffff', { roughness: 0.5 })); dot.position.z = d.hz + 0.2; g.add(dot);
    } else {
      const rr = r || d.r, m = add(new T.Mesh(new T.SphereGeometry(rr, 28, 20), std(color, { roughness: kind === 'ball' ? 0.75 : 0.5 })));
      if (kind === 'ball') { // seams, so you can see it spin
        const seam = std('#1b1b1b', { roughness: 0.9 });
        for (const rot of [[0, 0, 0], [Math.PI / 2, 0, 0], [0, 0, Math.PI / 2]]) { const t = new T.Mesh(new T.TorusGeometry(rr * 1.002, rr * 0.035, 6, 40), seam); t.rotation.set(rot[0], rot[1], rot[2]); m.add(t); }
      } else { const stem = new T.Mesh(new T.CylinderGeometry(1.2, 1.6, rr * 0.5, 6), std('#5b3a1a')); stem.position.y = rr; m.add(stem); if (kind === 'acorn') { const cap = new T.Mesh(new T.SphereGeometry(rr * 1.04, 20, 10, 0, Math.PI * 2, 0, Math.PI * 0.45), std('#4a2f14', { roughness: 0.95 })); m.add(cap); } }
    }
    return g;
  }
  function spawnProp(spec, k) {
    const [kind, x, color, r, stack] = spec, d = DEF[kind]; if (!d) return;
    const rr = (r || d.r || 0) * S, top = -G.mainSurf.y * S;
    let shape, half;
    if (d.cyl) { shape = new J.CylinderShape(d.cyl[1] * S, d.cyl[0] * S, 0.01, null); half = d.cyl[1] * S; }
    else if (d.hx) { shape = box(d.hx * S, d.hy * S, d.hz * S); half = d.hy * S; }
    else { shape = new J.SphereShape(rr, null); half = rr; }
    const y = top + half + 0.02 + (stack ? d.hy * 2 * S + 0.01 : 0), z = ((k % 3) - 1) * 0.08;
    const b = body(shape, x * S, y, z, J.EMotionType_Dynamic, { mass: d.mass, rest: d.rest, fric: d.fric, damp: 0.08, adamp: 0.3 });
    const mesh = look(kind, d, color || '#cccccc', r); group.add(mesh);
    PH.props.push({ kind, d, b, mesh, r: (r || d.r || Math.max(d.hx || 0, d.hy || 0, (d.cyl || [0])[0])), home: [x * S, y, z], owner: null, live: 0, cd: new Map(), out: 0 });
  }

  // ---------------------------------------------------------------- build / tear down with each match
  function clear() {
    for (const p of PH.props) kill(p.b); for (const d of PH.debris) kill(d.b); for (const b of statics) kill(b); for (const k of kin) kill(k.b); for (const b of fighters.values()) kill(b);
    PH.props = []; PH.debris = []; statics = []; kin = []; fighters.clear(); seenBoxes = new Map();
    if (group && group.parent) group.parent.remove(group); group = null;
  }
  function build() {
    if (!PH.ready || !G.stage || !G.mainSurf) return;
    clear(); group = new T.Group(); if (scene3) scene3.add(group);
    const m = G.mainSurf, w = (m.x2 - m.x1) / 2 * S, cx = (m.x1 + m.x2) / 2 * S, top = -m.y * S, depth = ((G.stage.main.depth || 120) / 2 + 30) * S;
    statics.push(body(box(w, 0.4, depth), cx, top - 0.4, 0, J.EMotionType_Static, { fric: 0.8, rest: 0.2 }));
    // invisible glass in front of and behind the fight line keeps props where they can be hit
    for (const sz of [-1, 1]) statics.push(body(box(30, 30, 0.1), 0, top + 10, sz * 0.75, J.EMotionType_Static, { fric: 0.1, rest: 0.3 }));
    for (const p of G.plats) { const pw = (p.x2 - p.x1) / 2 * S; const b = body(box(pw, 0.05, 0.5), (p.x1 + p.x2) / 2 * S, -p.y * S - 0.05, 0, p.move ? J.EMotionType_Kinematic : J.EMotionType_Static, { fric: 0.8 }); if (p.move) kin.push({ b, p }); else statics.push(b); }
    (KIT[G.stage.theme] || KIT.driveway).forEach(spawnProp);
  }
  function fighterBody(f) {
    let b = fighters.get(f); if (b) return b;
    const r = Math.max(0.12, f.w * 0.5 * S), hh = Math.max(0.05, f.h * 0.5 * S - r);
    b = body(new J.CapsuleShape(hh, r, null), f.x * S, -f.y * S + f.h * 0.5 * S, 0, J.EMotionType_Kinematic, { fric: 0.3, rest: 0.1 });
    fighters.set(f, b); return b;
  }

  // ---------------------------------------------------------------- the step (60 Hz, with the game)
  const guest = () => !!(G.net && G.net.guest);
  function hitProps() {
    for (const a of G.fighters) {
      const hbs = activeHBs(a); if (!hbs.length) continue;
      if (!a._ph || a._ph.mv !== a.move || a._ph.mt > a.mt) a._ph = { mv: a.move, mt: 0, set: new Set() }; a._ph.mt = a.mt;
      for (const h of hbs) {
        if (h.grab) continue; const w = hbWorld(a, h);
        for (const p of PH.props) {
          if (a._ph.set.has(p)) continue;
          const pos = p.b.GetPosition(), px = pos.GetX() / S, py = -pos.GetY() / S;
          // a swing reaches anything on the ground under it too (so a chest-high punch still boots the ball at your feet)
          const dx = Math.abs(px - w.x), near = Math.hypot(px - w.x, py - w.y) <= w.r + p.r, under = dx <= w.r + p.r && py > w.y && py <= a.y + 12 && Math.abs(a.y - py) < a.h;
          if (!near && !under) continue;
          a._ph.set.add(p);
          const dir = a.facing, ang = (h.a === 361 || h.a == null ? 38 : h.a) * Math.PI / 180;
          const sp = Math.min(24, 5 + (h.d || 4) * 0.75 + (h.b || 0) * 0.03 + (h.g || 0) * 0.035) * (a.od > 0 ? 1.2 : 1);
          BI.SetLinearVelocity(p.b.GetID(), V(Math.cos(ang) * dir * sp, Math.max(1.5, Math.sin(ang) * sp), (Math.random() - 0.5) * 0.6));
          BI.SetAngularVelocity(p.b.GetID(), V((Math.random() - 0.5) * 6, (Math.random() - 0.5) * 6, -dir * sp * 1.2));
          p.owner = a; p.live = 100; p.cd.clear();
          a.hitstop = Math.max(a.hitstop, 3); G.shake = Math.max(G.shake, 2 + sp * 0.2);
          Sfx.hit && Sfx.hit(Math.min(14, (h.d || 4))); fx.spark(px, py, '#ffffff', 8, 7); fx.ring && fx.ring(px, py, a.def.accent || '#fff', 26);
        }
      }
    }
    // pulse attacks blow props outward
    for (const pu of G.pulses || []) for (const p of PH.props) {
      const pos = p.b.GetPosition(), px = pos.GetX() / S, py = -pos.GetY() / S, dx = px - pu.x, dy = py - pu.y, d = Math.hypot(dx, dy);
      if (pu._ph && pu._ph.has(p) || d > pu.r + p.r || d < pu.r - 60) continue; (pu._ph || (pu._ph = new Set())).add(p);
      const k = 13 / (d || 1); BI.SetLinearVelocity(p.b.GetID(), V(dx * k, Math.max(3, -dy * k + 3), 0)); p.owner = pu.f || null; p.live = p.owner ? 90 : 0; p.cd.clear();
    }
  }
  function propsHitFighters() {
    if (guest()) return; // online: the host decides damage
    for (const p of PH.props) {
      if (p.live <= 0 || !p.owner) continue; p.live--;
      const vel = p.b.GetLinearVelocity(), vx = vel.GetX(), vy = vel.GetY(), sp = Math.hypot(vx, vy); if (sp < 5.5) continue;
      const pos = p.b.GetPosition(), px = pos.GetX() / S, py = -pos.GetY() / S;
      for (const v of G.fighters) {
        if (v === p.owner || (p.owner.team && p.owner.team === v.team) || !canBeHit(v) || (p.cd.get(v) || 0) > G.frame) continue;
        if (!circRect(px, py, p.r, hurtRect(v))) continue;
        p.cd.set(v, G.frame + 40);
        const dmg = Math.max(3, Math.min(17, sp * 0.62 * p.d.dmg)), dir = sgn(vx) || 1, up = Math.max(20, Math.min(70, Math.atan2(Math.max(0, vy) + 3, Math.abs(vx)) * 180 / Math.PI + 18));
        applyHit(p.owner, v, { d: dmg, a: up, b: 22 + dmg * 1.2, g: 62 + dmg * 2.2 }, dir, { px, py });
        BI.SetLinearVelocity(p.b.GetID(), V(-vx * 0.35, Math.abs(vy) * 0.3 + 3, 0)); p.live = Math.min(p.live, 20);
        G.notes && sp > 12 && G.notes.push({ x: px, y: py - 40, text: 'BONK!', color: '#ffe08a', t: 0, life: 50 });
      }
    }
  }
  function crateDebris() {
    // a crate that was here last frame and is gone (or dead) now has just been broken open
    const alive = new Set();
    for (const it of G.items || []) if (it.box && !it.dead) { alive.add(it.id); seenBoxes.set(it.id, { id: it.id, x: it.x, y: it.y, r: it.r, type: it.type }); }
    for (const [id, it] of [...seenBoxes]) {
      if (alive.has(id)) continue; seenBoxes.delete(id);
      if (it.y > G.stage.blast.b - 60 || PH.debris.length > 30) continue; // fell off the stage, or plenty of planks already
      const n = 11, big = it.type === 'crate';
      for (let k = 0; k < n; k++) {
        const hx = (big ? 14 : 9) * (0.6 + Math.random() * 0.7), hy = 2.2, hz = (big ? 5 : 4) * (0.7 + Math.random() * 0.6);
        const b = body(box(hx * S, hy * S, hz * S), (it.x + (Math.random() - 0.5) * it.r * 1.6) * S, (-it.y + it.r * (0.3 + Math.random() * 1.2)) * S, (Math.random() - 0.5) * 0.3, J.EMotionType_Dynamic, { mass: 0.25, rest: 0.3, fric: 0.7, damp: 0.1, adamp: 0.4 });
        const a = Math.random() * Math.PI * 2, sp = 2.5 + Math.random() * 5;
        BI.SetLinearVelocity(b.GetID(), V(Math.cos(a) * sp, 3 + Math.random() * 5, (Math.random() - 0.5) * 2));
        BI.SetAngularVelocity(b.GetID(), V((Math.random() - 0.5) * 14, (Math.random() - 0.5) * 14, (Math.random() - 0.5) * 14));
        const mat = new T.MeshStandardMaterial({ color: big ? ['#b9824a', '#a06a36', '#c99557'][k % 3] : ['#e9ecef', '#ff8787', '#ffd43b'][k % 3], roughness: 0.8, transparent: true });
        const mesh = new T.Mesh(new T.BoxGeometry(hx * 2, hy * 2, hz * 2), mat); mesh.castShadow = true; group.add(mesh);
        PH.debris.push({ b, mesh, t: 0, life: 230 + Math.random() * 60 });
      }
    }
    for (let i = PH.debris.length - 1; i >= 0; i--) { const d = PH.debris[i]; if (++d.t > d.life) { kill(d.b); group.remove(d.mesh); d.mesh.geometry.dispose(); d.mesh.material.dispose(); PH.debris.splice(i, 1); } else if (d.t > d.life - 40) d.mesh.material.opacity = (d.life - d.t) / 40; }
    while (PH.debris.length > 100) { const d = PH.debris.shift(); kill(d.b); group.remove(d.mesh); }
  }
  function physStep() {
    if (!PH.ready || !PH.on || !group) return;
    const dt = 1 / 60;
    for (const k of kin) BI.MoveKinematic(k.b.GetID(), RV((k.p.x1 + k.p.x2) / 2 * S, -k.p.y * S - 0.05, 0), Q(0, 0, 0, 1), dt);
    for (const f of G.fighters) {
      const b = fighterBody(f), gone = f.state === 'dead' || f.state === 'out' || f.hidden;
      BI.MoveKinematic(b.GetID(), gone ? RV(f.port * 3, -80, 0) : RV(f.x * S, (-f.y + f.h * 0.5) * S, 0), Q(0, 0, 0, 1), dt);
    }
    if (G.countdown <= 0) { hitProps(); propsHitFighters(); }
    crateDebris();
    jolt.Step(dt, 1);
    // props that leave the stage drop back in from above after a moment
    const bl = G.stage.blast;
    for (const p of PH.props) {
      const pos = p.b.GetPosition(), x = pos.GetX() / S, y = -pos.GetY() / S;
      if (x < bl.l || x > bl.r || y > bl.b || y < bl.t - 400) {
        if (++p.out > 70) { p.out = 0; p.owner = null; p.live = 0; BI.SetPositionAndRotation(p.b.GetID(), RV(p.home[0], -G.mainSurf.y * S + 5.5, p.home[2]), Q(0, 0, 0, 1), J.EActivation_Activate); BI.SetLinearVelocity(p.b.GetID(), V(0, 0, 0)); BI.SetAngularVelocity(p.b.GetID(), V(0, 0, 2)); }
      } else p.out = 0;
    }
  }
  // draw: copy each body's place and spin to its mesh
  R3.addHook(scene => {
    scene3 = scene; if (!group) return; if (group.parent !== scene) scene.add(group);
    group.visible = G.mode === 'fight' || G.mode === 'results';
    const put = o => { const p = o.b.GetPosition(), r = o.b.GetRotation(); o.mesh.position.set(p.GetX() / S, p.GetY() / S, p.GetZ() / S); o.mesh.quaternion.set(r.GetX(), r.GetY(), r.GetZ(), r.GetW()); };
    for (const p of PH.props) put(p); for (const d of PH.debris) put(d);
  });

  // a loose cube of rubble (voxel towers use this): a real rigid body that tumbles, settles and fades
  PH.cube = (x, y, z, h, color, vx, vy, vz) => {
    if (!PH.ready || !PH.on || !group || PH.debris.length > 90) return;
    const b = body(box(h * S, h * S, h * S), x * S, -y * S, z * S, J.EMotionType_Dynamic, { mass: 0.5, rest: 0.25, fric: 0.7, damp: 0.1, adamp: 0.35 });
    BI.SetLinearVelocity(b.GetID(), V(vx, vy, vz || 0)); BI.SetAngularVelocity(b.GetID(), V((Math.random() - 0.5) * 12, (Math.random() - 0.5) * 12, (Math.random() - 0.5) * 12));
    const mesh = new T.Mesh(new T.BoxGeometry(h * 2, h * 2, h * 2), new T.MeshStandardMaterial({ color, roughness: 0.7, transparent: true })); mesh.castShadow = true; group.add(mesh);
    PH.debris.push({ b, mesh, t: 0, life: 200 + Math.random() * 80 });
  };
  // ---------------------------------------------------------------- online: host's props are the truth
  const r2 = v => Math.round(v * 100) / 100;
  PH.netOut = () => PH.props.slice(0, 8).map(p => { const a = p.b.GetPosition(), r = p.b.GetRotation(), v = p.b.GetLinearVelocity(); return [r2(a.GetX()), r2(a.GetY()), r2(a.GetZ()), r2(r.GetX()), r2(r.GetY()), r2(r.GetZ()), r2(r.GetW()), r2(v.GetX()), r2(v.GetY())]; });
  PH.netIn = list => {
    if (!PH.ready || !list) return;
    list.forEach((a, i) => {
      const p = PH.props[i]; if (!p) return; const pos = p.b.GetPosition(), ex = a[0] - pos.GetX(), ey = a[1] - pos.GetY(), err = Math.hypot(ex, ey);
      if (err < 0.04) return; const k = err > 0.6 ? 1 : 0.35;
      BI.SetPositionAndRotation(p.b.GetID(), RV(pos.GetX() + ex * k, pos.GetY() + ey * k, a[2]), k === 1 ? Q(a[3], a[4], a[5], a[6]) : p.b.GetRotation(), J.EActivation_Activate);
      BI.SetLinearVelocity(p.b.GetID(), V(a[7], a[8], 0));
    });
  };

  // ---------------------------------------------------------------- hook into the game
  const baseStep = step; step = function () { baseStep(); if (G.mode === 'fight') { try { physStep(); } catch (e) { console.warn('physics step failed', e); PH.on = false; } } };
  if (window.__clash) window.__clash.step = step;
  const baseStart = startMatch; startMatch = function (cfg) { baseStart(cfg); PH.on = true; try { build(); } catch (e) { console.warn('physics build failed', e); } };
  if (window.__clash) window.__clash.startMatch = startMatch;
  boot();
})();
