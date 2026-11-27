'use strict';
// Super Family Smash: online play with room codes, through the page's `room` capability (people who have this game's
// Claude link open, signed in). Built on presence only, which every admitted viewer may set, so anyone the game is
// shared with can host or join:
//  - the HOST creates a room (a 4-letter code), runs the real match and keeps a compact snapshot of it in its presence
//    (~30 times a second): fighters, items, the Super Orb, finales, timer and result, plus the CPUs' inputs;
//  - each GUEST joins with the code and keeps their controller in their presence (held buttons + a press counter per
//    button, so a quick tap is never lost between updates);
//  - every screen simulates the match from everyone's inputs (so your own fighter answers at once and every effect
//    plays), and guests are corrected by the host's snapshot, which is the referee.
(() => {
  if (typeof G === 'undefined') return;
  const $ = id => document.getElementById(id);
  const CODE_CH = 'abcdefghjkmnpqrstuvwxyz23456789';
  const B = ['attack', 'special', 'jump', 'shield', 'grab', 'smash'];
  let room = null, R = null, roomP = null; // capability, joined room, its promise
  const N = window.NET = { role: null, code: null, me: null, matchId: 0, lobby: null, fi: 0, nick: '' };

  // ---------------------------------------------------------------- input packing
  const q = v => Math.round((v || 0) * 100);
  function packInput(raw, cnt) { let b = 0; B.forEach((k, i) => { if (raw[k]) b |= 1 << i; }); return { b, x: q(raw.x), y: q(raw.y), sx: q(raw.sx), sy: q(raw.sy), c: cnt.slice(), a: raw.analog ? 1 : 0 }; }
  const counter = () => ({ cnt: B.map(() => 0), prev: {} });
  function countPresses(st, raw) { B.forEach((k, i) => { if (raw[k] && !st.prev[k]) st.cnt[i]++; st.prev[k] = !!raw[k]; }); }
  // a controller fed by someone else's packed input; a press the sender saw but we missed between updates still lands
  function netCtrl(getPacked) {
    const seen = B.map(() => -1);
    return { poll: () => {
      const o = emptyIn(), p = getPacked(); if (!p) return o;
      o.x = (p.x || 0) / 100; o.y = (p.y || 0) / 100; o.sx = (p.sx || 0) / 100; o.sy = (p.sy || 0) / 100; o.analog = !!p.a;
      B.forEach((k, i) => { const held = !!(p.b & (1 << i)), c = (p.c && p.c[i]) || 0; if (seen[i] < 0) seen[i] = c; o[k] = held || c > seen[i]; seen[i] = c; });
      if (o.smash && !o.sx && !o.sy) o.smash = false;
      return o;
    } };
  }

  // ---------------------------------------------------------------- room helpers
  const peers = () => (R ? R.peers() : []);
  const host = () => peers().find(p => p.presence && p.presence.role === 'host');
  const byPeer = id => peers().find(p => p.peer === id);
  const meP = () => peers().find(p => p.sameTab);
  function setP(patch) { if (R) R.presence(patch).catch(() => {}); }
  async function getRoom() {
    if (room) return room; if (!window.claude || !claude.use) return null;
    room = await claude.use('room'); return room;
  }
  async function enter(code, role) {
    const rm = await getRoom(); if (!rm) throw new Error('offline');
    if (R) { try { await R.leave(); } catch (e) {} R = null; }
    R = await rm.join('sfs-' + code); N.code = code; N.role = role;
    N.me = (meP() || {}).peer || null;
    R.onPeers(onPeers, () => leaveRoom('The connection to the room was lost.'));
    setP({ sfs: 1, role, fi: N.fi, nick: N.nick || '' });
    return R;
  }
  async function leaveRoom(msg) {
    if (G.net) endNetMatch();
    if (R) { try { await R.leave(); } catch (e) {} } R = null; N.role = null; N.code = null; N.lobby = null; renderLobby();
    if (msg) toast(msg);
  }

  // ---------------------------------------------------------------- lobby state (host decides it)
  function hostLobby() {
    const L = N.lobby || (N.lobby = { stage: cfg.stage, stocks: cfg.stocks, time: cfg.time, cpus: [], start: 0, speed: G.speed || 0.8 });
    // slots: the host first, then guests in the order they arrived, then CPUs (up to 8)
    const me = meP(), guests = peers().filter(p => !p.sameTab && p.presence && p.presence.sfs && p.presence.role === 'guest').sort((a, b) => a.updatedAt - b.updatedAt);
    const slots = [{ peer: me ? me.peer : 'host', fi: N.fi, kind: 'host' }];
    for (const g of guests) if (slots.length < 8) slots.push({ peer: g.peer, fi: g.presence.fi | 0, kind: 'guest' });
    for (const c of L.cpus) if (slots.length < 8) slots.push({ peer: null, fi: c.fi, kind: 'cpu', lv: c.lv });
    L.slots = slots; return L;
  }
  function publishLobby() { if (N.role === 'host') { const L = hostLobby(); setP({ L: { stage: L.stage, stocks: L.stocks, time: L.time, slots: L.slots, start: L.start, cfgM: L.cfgM || null, speed: L.speed } }); } }
  function onPeers(ch) {
    if (N.role === 'host') {
      publishLobby();
      // a guest who leaves mid-match just stands still (their controller goes quiet)
    } else if (N.role === 'guest') {
      const h = host();
      if (!h) { if (G.net && G.mode === 'fight') { leaveRoom('The host left the room.'); showScreen('hub'); } }
      else { const L = h.presence.L; if (L) { N.lobby = L; if (L.start && L.start !== N.matchId && L.cfgM) startNetMatch(L); } }
    }
    renderLobby();
  }

  // ---------------------------------------------------------------- the match
  const st = { local: counter() };
  function startNetMatch(L) {
    N.matchId = L.start;
    const cfgM = L.cfgM;
    G.netSaveSpeed = G.speed; G.speed = L.speed || 0.8;
    startMatch({ stage: cfgM.stage, stocks: cfgM.stocks, time: cfgM.time, slots: cfgM.slots.map(s => ({ type: s.type === 'cpu' ? 'cpu' : 'human', fighter: s.fi, level: s.lv || 5 })) });
    G.net = { role: N.role, guest: N.role === 'guest', slots: cfgM.slots, lastSnap: null };
    // controllers: mine is my own keyboard / controller / touch; everyone else's comes over the room
    G.fighters.forEach((f, k) => {
      const s = cfgM.slots[k]; f.netSlot = s;
      if (s.peer && s.peer === N.me) { f.ctrl = humanCtrl(0); f.netMine = true; return; }
      if (s.type === 'cpu') { if (N.role === 'guest') f.ctrl = netCtrl(() => { const h = host(), g = h && h.presence.g; return g && g.ci && g.ci[k]; }); return; } // the host's AI drives CPUs everywhere
      f.ctrl = netCtrl(() => { const p = byPeer(s.peer); return p && p.presence && p.presence.inp; });
    });
    toast(N.role === 'host' ? 'Online match started' : 'Joined the match');
  }
  function endNetMatch() { if (!G.net) return; G.net = null; if (G.netSaveSpeed) G.speed = G.netSaveSpeed; }
  // host: begin, from the lobby
  function hostStart() {
    const L = hostLobby(); if (L.slots.length < 2) { toast('Add a CPU or wait for a friend to join'); return; }
    L.cfgM = { stage: L.stage, stocks: L.stocks, time: L.time, slots: L.slots.map(s => ({ type: s.kind === 'cpu' ? 'cpu' : 'human', peer: s.peer, fi: s.fi, lv: s.lv || 5 })) };
    L.start = Date.now(); publishLobby(); startNetMatch(L);
  }

  // per frame: send my input; host sends the snapshot; guests apply it
  const r1 = v => Math.round(v * 10) / 10;
  const moveNameOf = f => { const mv = f.move; if (!mv) return 0; if (!f._mvn || f._mvn.mv !== mv) f._mvn = { mv, n: Object.keys(f.moves).find(k => f.moves[k] === mv) || 0 }; return f._mvn.n; };
  function snapshot() {
    const F = G.fighters.map(f => [r1(f.x), r1(f.y), r1(f.vx), r1(f.vy), f.facing, f.state, f.t | 0, moveNameOf(f), f.mt | 0, r1(f.percent), f.stocks, f.meter | 0, f.od | 0, f.final ? 1 : 0, f.hitstop | 0, f.intang | 0, f.grounded ? 1 : 0, f.anim | 0]);
    const I = (G.items || []).filter(it => !it.dead).slice(0, 14).map(it => [it.id, it.type, r1(it.x), r1(it.y), it.held ? G.fighters.indexOf(it.held) : -1]);
    const ci = {}; G.fighters.forEach((f, k) => { if (f.cpu && f.prevRaw) ci[k] = packInput(f.prevRaw, (f._ncnt || (f._ncnt = counter())).cnt); });
    const c = G.cine; const C = c ? [c.type, c.phase, c.t, G.fighters.indexOf(c.f)] : 0;
    const O = G.orb ? [r1(G.orb.x), r1(G.orb.y), G.orb.hp | 0] : 0;
    return { m: N.matchId, fr: G.frame, cd: G.countdown, tl: G.timeLeft, en: G.ending, F, I, O, C, ci };
  }
  function applySnap(g) {
    N.dbg = N.dbg || { n: 0, skip: 0 }; if (!g || g.m !== N.matchId) { N.dbg.skip++; N.dbg.why = g ? g.m + ' vs ' + N.matchId : 'none'; return; } N.dbg.n++; N.dbg.fr = g.fr;
    if (Math.abs((G.countdown || 0) - g.cd) > 6) G.countdown = g.cd;
    if (Math.abs((G.timeLeft || 0) - g.tl) > 30) G.timeLeft = g.tl;
    if (g.en && !G.ending) { G.ending = 1; G.banner = { text: G.timeLeft === 0 && G.cfg.time ? 'TIME!' : 'GAME!', t: 999, color: '#ffcf3f', big: true }; Sfx.boom && Sfx.boom(); }
    g.F.forEach((a, k) => {
      const f = G.fighters[k]; if (!f) return;
      const [x, y, vx, vy, fa, state, t, mn, mt, pct, stocks, meter, od, fin, hs, intang, gr, anim] = a;
      const err = Math.hypot(f.x - x, f.y - y), mine = f.netMine;
      // big error: snap; small: ease in (my own fighter is trusted a little more, so it stays responsive)
      const k2 = err > 140 ? 1 : mine ? 0.18 : 0.4; f.x += (x - f.x) * k2; f.y += (y - f.y) * k2;
      if (!mine || err > 140) { f.vx = vx; f.vy = vy; }
      if (pct > f.percent + 0.5) { fx.spark(f.x, f.y - f.h * 0.5, '#ffffff', 6, 6); } // a hit the host saw that we didn't
      f.percent = pct; f.meter = meter; f.od = od; f.final = !!fin;
      if (stocks < f.stocks) { f.stocks = stocks; } else f.stocks = stocks;
      if (!mine || f.state === 'dead' || state === 'dead' || state === 'respawn' || state === 'hitstun' || state === 'down') {
        if (f.state !== state) { f.state = state; f.t = t; } f.facing = fa;
        if (state === 'attack' && mn && f.moves[mn] && f.move !== f.moves[mn]) { f.move = f.moves[mn]; f.mt = mt; f.hitIds = new Map(); }
        if (state === 'attack' && Math.abs(f.mt - mt) > 4) f.mt = mt;
        f.hitstop = hs; f.intang = intang; f.anim = anim;
      }
      f.grounded = !!gr && f.grounded;
    });
    // items: the host's list is the truth
    if (G.items) {
      const keep = new Set(g.I.map(i => i[0]));
      for (const it of G.items) if (!keep.has(it.id)) it.dead = true;
      for (const [id, type, x, y, hk] of g.I) {
        let it = G.items.find(i => i.id === id && !i.dead);
        if (!it && ITEMS.spawnItem) { it = ITEMS.spawnItem(type, x, y); if (it) it.id = id; }
        if (!it) continue;
        const holder = hk >= 0 ? G.fighters[hk] : null;
        if (holder) { if (it.held !== holder) { if (holder.item && holder.item !== it) holder.item.held = null; it.held = holder; holder.item = it; } }
        else { if (it.held) { if (it.held.item === it) it.held.item = null; it.held = null; } it.x += (x - it.x) * 0.5; it.y += (y - it.y) * 0.5; }
      }
    }
    // the Super Orb and finales
    if (g.O) { if (!G.orb && ITEMS.spawnOrb) ITEMS.spawnOrb(); if (G.orb) { G.orb.x += (g.O[0] - G.orb.x) * 0.5; G.orb.y += (g.O[1] - G.orb.y) * 0.5; G.orb.hp = g.O[2]; } } else if (G.orb) G.orb = null;
    if (g.C && !G.cine) { const f = G.fighters[g.C[3]]; if (f && ITEMS.startFinale) ITEMS.startFinale(f); }
  }
  function tick() {
    if (!R || !N.role) return;
    if (G.net && G.mode === 'fight') {
      const mine = G.fighters.find(f => f.netMine);
      if (mine && mine.prevRaw) { countPresses(st.local, mine.prevRaw); setP({ inp: packInput(mine.prevRaw, st.local.cnt) }); }
      if (N.role === 'host') {
        for (const f of G.fighters) if (f.cpu && f.prevRaw) countPresses(f._ncnt || (f._ncnt = counter()), f.prevRaw);
        setP({ g: snapshot() });
      } else { const h = host(); if (h && h.presence.g && h.presence.g !== G.net.lastSnap) { G.net.lastSnap = h.presence.g; applySnap(h.presence.g); } }
    }
    if (G.net && G.mode !== 'fight' && G.mode !== 'results') endNetMatch();
  }
  // run right after each simulation step
  const step0 = window.__clash && window.__clash.step;
  setInterval(tick, 1000 / 40);

  // ---------------------------------------------------------------- the ONLINE screen
  const scr = document.createElement('div'); scr.id = 'online'; scr.className = 'screen';
  scr.innerHTML = `<div class="card onl">
    <h3>ONLINE</h3>
    <p class="onl-sub" id="onlSub">Play with friends who have this game's link open. Make a room, then tell them the code.</p>
    <div class="onl-start" id="onlStart">
      <button class="big" id="onlCreate" data-say="PLAY">CREATE ROOM</button>
      <div class="onl-join"><input id="onlCode" maxlength="4" autocomplete="off" autocapitalize="off" spellcheck="false" placeholder="CODE" aria-label="Room code"><button class="big alt" id="onlJoin">JOIN</button></div>
    </div>
    <div class="onl-lobby" id="onlLobby" hidden>
      <div class="onl-code">ROOM <b id="onlCodeShow">----</b> <button id="onlCopy" class="sm">COPY</button></div>
      <div class="onl-players" id="onlPlayers"></div>
      <div class="onl-label">YOUR FIGHTER</div><div class="row onl-pick" id="onlPick"></div>
      <div class="onl-host" id="onlHostOpts">
        <div class="onl-label">STAGE</div><div class="row" id="onlStage"></div>
        <div class="onl-label">STOCKS · TIME</div><div class="row" id="onlRules"></div>
        <div class="row"><button id="onlCpu">+ ADD CPU</button><button id="onlCpuX">− REMOVE CPU</button></div>
      </div>
      <div class="onl-acts"><button class="big alt" id="onlLeave">LEAVE</button><button class="big" id="onlGo">START MATCH</button></div>
      <p class="onl-sub" id="onlWait" hidden>Waiting for the host to start the match…</p>
    </div>
    <button class="big alt" id="onlBack">BACK</button>
  </div>`;
  document.body.appendChild(scr);
  const css = document.createElement('style'); css.textContent = `
    #online .card.onl { width:min(720px, 94vw); max-height:calc(100vh - 16px); overflow-y:auto; }
    #online [hidden] { display:none !important; }
    .onl-sub { color:#c9c3e6; margin:4px 0 12px; }
    .onl-start { display:flex; flex-wrap:wrap; gap:12px; justify-content:center; align-items:center; margin:8px 0 14px; }
    .onl-join { display:flex; gap:8px; align-items:center; }
    #onlCode { width:7.5em; font:900 22px/1 inherit; letter-spacing:6px; text-transform:uppercase; text-align:center; padding:10px; border-radius:12px; border:2px solid #ffffff33; background:#0b0d1a; color:#ffe08a; }
    #onlCode:focus { outline:none; border-color:#ffd45a; }
    .onl-code { font:800 15px/1 inherit; letter-spacing:2px; color:#c9c3e6; margin:6px 0 12px; display:flex; gap:10px; align-items:center; justify-content:center; }
    .onl-code b { font-size:34px; letter-spacing:8px; color:#ffe08a; text-transform:uppercase; }
    .onl-players { display:grid; grid-template-columns:repeat(auto-fill, minmax(150px, 1fr)); gap:8px; margin-bottom:12px; }
    .onl-pl { display:flex; gap:8px; align-items:center; padding:6px 8px; border-radius:12px; background:#ffffff10; border:1px solid #ffffff22; min-width:0; }
    .onl-pl img { width:38px; height:38px; border-radius:9px; object-fit:cover; }
    .onl-pl b { font-size:14px; } .onl-pl small { color:#9aa3c8; display:block; font-size:11px; }
    .onl-pl.me { border-color:#ffd45a; }
    .onl-label { font:800 11px/1 inherit; letter-spacing:2px; color:#9aa3c8; margin:10px 0 6px; text-align:left; }
    .onl-pick { flex-wrap:wrap; gap:6px; justify-content:flex-start; }
    .onl-pick button { width:54px; height:54px; padding:0; border-radius:10px; overflow:hidden; border:2px solid transparent; }
    .onl-pick button img { width:100%; height:100%; object-fit:cover; }
    .onl-pick button.on { border-color:#ffd45a; box-shadow:0 0 14px #ffd45a88; }
    #online .row { flex-wrap:wrap; }
    .onl-acts { display:flex; gap:12px; justify-content:center; margin:14px 0 6px; }
    .onl-toast { position:fixed; left:50%; bottom:max(18px, env(safe-area-inset-bottom, 0px)); transform:translateX(-50%); z-index:9500; background:#14182cee; color:#fff; padding:10px 18px; border-radius:12px; border:1px solid #ffd45a66; font:700 14px/1.3 inherit; pointer-events:none; opacity:0; transition:opacity .25s; }
    .onl-toast.on { opacity:1; }
    @media (max-height:560px) and (orientation:landscape) { .onl-players { grid-template-columns:repeat(4, 1fr); } .onl-pick button { width:44px; height:44px; } .onl-code b { font-size:26px; } }`;
  document.head.appendChild(css);
  const tt = document.createElement('div'); tt.className = 'onl-toast'; document.body.appendChild(tt);
  let ttT = 0; function toast(m) { tt.textContent = m; tt.classList.add('on'); clearTimeout(ttT); ttT = setTimeout(() => tt.classList.remove('on'), 2600); }

  const FAM = () => window.CLASH_FAM || {};
  const face = def => FAM()['face_' + def.id] || FAM()[def.id] || '';
  const avail = () => FIGHTERS.map((d, i) => ({ d, i })).filter(o => typeof fighterUnlocked !== 'function' || fighterUnlocked(o.d));
  function renderLobby() {
    if (!scr.classList.contains('on')) return;
    const inRoom = !!N.role; $('onlStart').hidden = inRoom; $('onlLobby').hidden = !inRoom;
    if (!inRoom) return;
    $('onlCodeShow').textContent = (N.code || '').toUpperCase();
    const L = N.role === 'host' ? hostLobby() : (N.lobby || { slots: [] });
    const me = N.me;
    $('onlPlayers').innerHTML = (L.slots || []).map((s, k) => { const d = FIGHTERS[s.fi] || FIGHTERS[0]; const lab = s.kind === 'cpu' ? 'CPU' : s.kind === 'host' ? 'HOST' : 'PLAYER'; const img = face(d); return `<div class="onl-pl ${s.peer && s.peer === me ? 'me' : ''}">${img ? `<img src="${img}" alt="">` : ''}<div><b>P${k + 1} · ${d.name}</b><small>${lab}${s.peer && s.peer === me ? ' · YOU' : ''}</small></div></div>`; }).join('') || '<p class="onl-sub">Connecting…</p>';
    $('onlPick').innerHTML = avail().map(({ d, i }) => `<button data-fi="${i}" class="${i === N.fi ? 'on' : ''}" aria-label="${d.name}">${face(d) ? `<img src="${face(d)}" alt="">` : d.name}</button>`).join('');
    const isHost = N.role === 'host'; $('onlHostOpts').hidden = !isHost; $('onlGo').hidden = !isHost; $('onlWait').hidden = isHost;
    if (isHost) {
      $('onlStage').innerHTML = Object.entries(STAGES).map(([k, v]) => `<button data-st="${k}" class="${L.stage === k ? 'on' : ''}">${v.name}</button>`).join('');
      $('onlRules').innerHTML = [1, 2, 3, 4, 5].map(n => `<button data-sk="${n}" class="${L.stocks === n ? 'on' : ''}">${n}</button>`).join('') + [0, 3, 5].map(n => `<button data-tm="${n}" class="${L.time === n ? 'on' : ''}">${n ? n + ' MIN' : '∞'}</button>`).join('');
    }
  }
  scr.addEventListener('click', e => {
    const b = e.target.closest('button'); if (!b) return;
    if (b.dataset.fi != null) { N.fi = +b.dataset.fi; setP({ fi: N.fi }); if (N.role === 'host') publishLobby(); renderLobby(); return; }
    const L = N.lobby;
    if (b.dataset.st && L) { L.stage = b.dataset.st; publishLobby(); renderLobby(); return; }
    if (b.dataset.sk && L) { L.stocks = +b.dataset.sk; publishLobby(); renderLobby(); return; }
    if (b.dataset.tm != null && L) { L.time = +b.dataset.tm; publishLobby(); renderLobby(); return; }
  });
  async function create() {
    const code = Array.from({ length: 4 }, () => CODE_CH[Math.floor(Math.random() * CODE_CH.length)]).join('');
    try { await enter(code, 'host'); N.lobby = null; hostLobby(); publishLobby(); renderLobby(); toast('Room ' + code.toUpperCase() + ' is open'); }
    catch (e) { noRoom(); }
  }
  async function join() {
    const code = ($('onlCode').value || '').toLowerCase().replace(/[^a-z0-9]/g, '');
    if (code.length !== 4) { toast('Room codes have 4 letters'); return; }
    try {
      await enter(code, 'guest'); renderLobby();
      setTimeout(() => { if (N.role === 'guest' && !host()) leaveRoom('No room with that code. Check it and try again.'); }, 6000);
    } catch (e) { noRoom(); }
  }
  function noRoom() { toast('Online play works when this game is opened from its Claude link while signed in.'); $('onlSub').textContent = 'Online play needs this game opened from its Claude link while you are signed in to Claude. Local play works everywhere.'; }
  $('onlCreate').onclick = create; $('onlJoin').onclick = join;
  $('onlCode').addEventListener('keydown', e => { if (e.key === 'Enter') join(); });
  $('onlGo').onclick = hostStart;
  $('onlLeave').onclick = () => leaveRoom();
  $('onlBack').onclick = () => { showScreen('hub'); };
  $('onlCpu').onclick = () => { const L = N.lobby; if (!L) return; const a = avail(); L.cpus.push({ fi: a[Math.floor(Math.random() * a.length)].i, lv: 5 }); publishLobby(); renderLobby(); };
  $('onlCpuX').onclick = () => { const L = N.lobby; if (!L) return; L.cpus.pop(); publishLobby(); renderLobby(); };
  $('onlCopy').onclick = () => { const t = (N.code || '').toUpperCase(); try { navigator.clipboard.writeText(t).then(() => toast('Copied ' + t), () => toast('Room code: ' + t)); } catch (e) { toast('Room code: ' + t); } };

  // the ONLINE tile on the hub
  function addTile() {
    const tiles = document.querySelector('#hub .tiles'); if (!tiles || tiles.querySelector('[data-mode="online"]')) return;
    const t = document.createElement('button'); t.className = 'tile'; t.dataset.mode = 'online'; t.style.cssText = '--c1:#0d7a5f;--c2:#27c79a';
    t.innerHTML = '<b>ONLINE</b><small>Make a room and share the code. Friends join from their own phone or PC.</small><i>🌐</i>';
    t.addEventListener('click', () => { N.fi = N.fi || 0; showScreen('online'); renderLobby(); getRoom().then(r => { if (!r) noRoom(); }); });
    tiles.appendChild(t);
  }
  addTile(); document.addEventListener('DOMContentLoaded', addTile);
  // after an online match the results buttons go back to the room
  for (const id of ['bAgain', 'bMenu']) { const b = $(id); if (b) b.addEventListener('click', e => { if (!G.net && !N.role) return; e.stopImmediatePropagation(); endNetMatch(); showScreen('online'); renderLobby(); }, true); }
  // no pausing an online match
  window.addEventListener('keydown', e => { if (G.net && (e.key === 'Escape' || e.key === 'p' || e.key === 'P')) e.stopImmediatePropagation(); }, true);
})();
