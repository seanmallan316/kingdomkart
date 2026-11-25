'use strict';
// Main menu hub, story mode, training and options for Super Family Smash. Loaded after game.js and fx.js.
(() => {
  const el = id => document.getElementById(id);
  const save = { get(k, d) { try { const v = localStorage.getItem('clash_' + k); return v == null ? d : JSON.parse(v); } catch (e) { return d; } }, set(k, v) { try { localStorage.setItem('clash_' + k, JSON.stringify(v)); } catch (e) { /* storage can be blocked; the game still works */ } } };
  const idx = id => FIGHTERS.findIndex(f => f.id === id);
  const KIDS = ['ray', 'norah', 'ollie', 'tyler', 'violet', 'layne', 'jayson', 'mom', 'dad'];

  // ---------------------------------------------------------------- live 3D scene behind each menu
  const has3D = () => typeof R3 !== 'undefined' && R3.menuSet;
  let sceneId = 'title', sceneExtra = null;
  const REACT = ['victory', 'jab', 'kick', 'spin', 'upper', 'highkick'];
  function setScene(id, extra) {
    sceneId = id; sceneExtra = extra || null; if (!has3D()) return;
    const c = cast(), wide = innerWidth > innerHeight * 1.2;
    if (id === 'title' && wide) R3.menuSet(KIDS.map((k, i) => ({ id: k, x: (i - 4) * 82, z: (i % 2) * -55, face: (4 - i) * 0.07 })), { x: 0, y: 200, d: 1120, lx: 0, ly: 205 });
    else if (id === 'title') R3.menuSet(KIDS.map((k, i) => (i < 5 ? { id: k, x: (i - 2) * 86, z: 45, face: (2 - i) * 0.1 } : { id: k, x: (i - 6.5) * 92, z: -70, face: (6.5 - i) * 0.1, scale: 1.05 })), { x: 0, y: 270, d: 2150, lx: 0, ly: 215 });  // portrait: two rows
    else if ((id === 'hub' || id === 'options') && wide) R3.menuSet([{ id: hero, x: 122, face: -0.4, scale: 1.12 }, { id: c.sib1, x: 222, z: -95, face: -0.55 }, { id: c.sib2, x: 28, z: -115, face: -0.15 }], { x: -30, y: 92, d: 520, lx: -30, ly: 60 });
    else if (id === 'hub' || id === 'options') { // portrait: the family lined up in the ring under the tiles
      const i0 = KIDS.indexOf(hero), five = [-2, -1, 0, 1, 2].map(o => KIDS[(i0 + o + KIDS.length) % KIDS.length]);
      R3.menuSet(five.map((k, i) => ({ id: k, x: (i - 2) * 88, z: i === 2 ? 30 : -20, face: (2 - i) * 0.12, scale: i === 2 ? 1.18 : 1 })), { x: 0, y: 330, d: 1750, lx: 0, ly: 300 });
    }
    else if (id === 'select') R3.menuSet(cfg.slots.map((s, i) => ({ s, i })).filter(o => o.s.type !== 'off').map(o => ({ key: 'slot' + o.i, id: FIGHTERS[o.s.fighter].id, x: (o.i - 1.5) * 128, face: (1.5 - o.i) * 0.16, once: extra === o.i ? 'jab' : null })),
      { x: 0, y: 62, d: wide ? 780 : 1250, lx: 0, ly: wide ? -78 : -120 });
    else if (id === 'story') {
      const foes = (extra && extra.foes) || [], ally = extra && extra.ally, list = [{ id: hero, x: foes.length ? -105 : 0, face: foes.length ? 0.85 : 0.2, scale: 1.05 }];
      if (ally) list.push({ key: 'ally', id: ally, x: -200, z: -70, face: 0.8 });
      foes.forEach((f, i) => list.push({ key: 'foe' + i, id: f.id, x: 105 + i * 95, z: -i * 60, face: -0.85, scale: f.big ? 1.5 : 1 }));
      R3.menuSet(list, { x: 0, y: 66, d: wide ? 660 : 1050, lx: 0, ly: wide ? -62 : -110 });
    } else if (id === 'results' && extra) R3.menuSet([{ key: 'win', id: extra, x: 0, face: 0.15, scale: 1.15, once: 'victory' }], { x: 0, y: 84, d: wide ? 520 : 820, lx: 0, ly: wide ? 4 : -30 });
  }
  window.refreshMenuScene = () => { setScene(sceneId, sceneExtra); tileArt(); };
  // tile artwork drawn from the real fighters: the hero on Story, two family silhouettes on Versus
  function tileArt() {
    if (typeof R3 === 'undefined') return; const c = cast();
    const put = (sel, id) => { const cv = document.querySelector(sel); if (cv) R3.portrait(cv, FIGHTERS[idx(id)]); };
    put('.art.hero', hero); put('.art.sil.a', c.sib1); put('.art.sil.b', c.sib2);
  }
  // arena atmosphere over the 3D scene: rows of stadium lights, sweeping coloured beams and drifting sparkles
  const sparks = Array.from({ length: 70 }, () => ({ x: Math.random(), y: Math.random(), s: 0.6 + Math.random() * 2.2, v: 0.0004 + Math.random() * 0.0012, p: Math.random() * 6.28, h: [45, 320, 190, 270, 30][Math.floor(Math.random() * 5)] }));
  const baseBg = drawMenuBg; let bgT = 0;
  drawMenuBg = function () {
    baseBg(); if (G.mode === 'fight') return; bgT++;
    const W = VW, H = VH, horizon = H * 0.86;
    ctx.save(); ctx.globalCompositeOperation = 'lighter';
    [[0.12, '#ff5ea8'], [0.34, '#7a5cff'], [0.66, '#6cf0ff'], [0.88, '#ffb347']].forEach(([px, col], i) => { // beams from the ring
      const sway = Math.sin(bgT * 0.008 + i * 1.7) * 0.22, x0 = W * px, len = H * 0.75, a = -Math.PI / 2 + sway + (px - 0.5) * -0.5;
      const g = ctx.createLinearGradient(x0, horizon, x0 + Math.cos(a) * len, horizon + Math.sin(a) * len); g.addColorStop(0, col + '66'); g.addColorStop(1, col + '00');
      ctx.fillStyle = g; ctx.beginPath(); ctx.moveTo(x0 - 8, horizon); ctx.lineTo(x0 + Math.cos(a - 0.11) * len, horizon + Math.sin(a - 0.11) * len); ctx.lineTo(x0 + Math.cos(a + 0.11) * len, horizon + Math.sin(a + 0.11) * len); ctx.lineTo(x0 + 8, horizon); ctx.fill();
    });
    for (const side of [0, 1]) for (let r = 0; r < 3; r++) for (let k = 0; k < 6; k++) { // stadium light banks, top corners
      const x = side ? W - 14 - k * 20 - r * 6 : 14 + k * 20 + r * 6, y = H * 0.2 + r * 22 + k * 5, tw = 0.45 + 0.35 * Math.sin(bgT * 0.05 + k + r * 2 + side);
      const g = ctx.createRadialGradient(x, y, 0, x, y, 13); g.addColorStop(0, 'rgba(255,255,255,' + tw + ')'); g.addColorStop(1, 'rgba(180,200,255,0)'); ctx.fillStyle = g; ctx.fillRect(x - 13, y - 13, 26, 26);
    }
    for (const s of sparks) { // sparkles drifting up
      s.y -= s.v; if (s.y < -0.02) { s.y = 1.02; s.x = Math.random(); }
      const a = 0.35 + 0.65 * Math.abs(Math.sin(bgT * 0.04 + s.p)), x = s.x * W + Math.sin(bgT * 0.01 + s.p) * 12, y = s.y * H;
      ctx.fillStyle = 'hsla(' + s.h + ',100%,75%,' + a + ')'; ctx.beginPath(); ctx.arc(x, y, s.s, 0, 6.283); ctx.fill();
      if (s.s > 2) { ctx.fillStyle = 'hsla(' + s.h + ',100%,85%,' + a * 0.5 + ')'; ctx.fillRect(x - s.s * 3, y - 0.5, s.s * 6, 1); ctx.fillRect(x - 0.5, y - s.s * 3, 1, s.s * 6); }
    }
    ctx.restore();
  };
  addEventListener('resize', () => { if (G.mode !== 'fight') setScene(sceneId, sceneExtra); });
  const voiceOf = id => { const d = FIGHTERS.find(f => f.id === id); if (d && Sfx.voice) Sfx.voice({ def: d, port: 9 }, 'atk1', true); };
  function react(key, clip, speak) { if (!has3D()) return; const id = R3.menuReact(key, clip || REACT[Math.floor(Math.random() * REACT.length)]); if (id && speak) voiceOf(id); }
  const baseShow = showScreen;
  showScreen = id => { baseShow(id); if (id && id !== 'pause' && id !== 'story' && id !== 'results') setScene(id); };
  // the title line-up fidgets on its own, and reacts when you tap the background
  setInterval(() => { if (G.mode !== 'fight' && sceneId === 'title' && !document.hidden) react(KIDS[Math.floor(Math.random() * KIDS.length)]); }, 2400);
  el('title').addEventListener('pointerdown', e => { if (e.target.closest('button')) return; Sfx.init(); react(KIDS[Math.floor(Math.random() * KIDS.length)], null, true); });

  // ---------------------------------------------------------------- story: The Golden Clog (original)
  // {sib1}/{sib2} are the two siblings the player did not pick.
  const bigBrick = () => Object.assign({}, FIGHTERS[idx('brick')], { id: 'brick', name: 'BIG BRICK', scale: 1.62, weight: 170, power: 1.3, speed: 1.2, run: 6.4, jump: 13.6, djump: 13.2 });
  const CHAPTERS = [
    { title: 'Warm-Up', text: 'The Golden Clog, the family trophy, is gone from the shelf. Before the search starts, {sib1} wants to check you are ready.', stage: 'proving',
      foes: h => [{ id: h.sib1, level: 3 }] },
    { title: 'My Turn!', text: '{sib2} says a trophy hunt needs the best fighter in the house, and reckons that is not you. Settle it.', stage: 'skyforge',
      foes: h => [{ id: h.sib2, level: 4 }] },
    { title: 'The Quickstep', text: 'Muddy footprints lead up to the sky platforms. Vex, the fastest of the Shadow Squad, is waiting, and he is not handing anything over.', stage: 'driftway',
      foes: () => [{ id: 'vex', level: 5 }] },
    { title: 'Light Show', text: 'Vex points the way to Nova, who has been using the Golden Clog as a lamp. She fights from far away, so get in close.', stage: 'skyforge',
      foes: () => [{ id: 'nova', level: 6 }] },
    { title: 'Double Trouble', text: 'Kai and Vex team up to block the road. Good thing {sib1} just caught up with you. Two against two!', stage: 'proving',
      foes: () => [{ id: 'kai', level: 5 }, { id: 'vex', level: 5 }], ally: h => ({ id: h.sib1, level: 6 }) },
    { title: 'Big Brick', text: 'The boss of the Shadow Squad has the Golden Clog on a chain around his neck. He is huge, slow and very hard to launch. Wear him down.', stage: 'skyforge',
      foes: () => [{ def: bigBrick(), level: 8, stocks: 2 }], boss: true },
  ];
  const ENDING = 'Big Brick tumbles off the stage and the Golden Clog drops into your hands. Back home it goes on the shelf, with one new rule: whoever wins the next family match gets to hold it.';
  let hero = save.get('hero', 'ray');
  const prog = () => save.get('story', {});
  // the rival and the ally are the next two family members after the hero in the list
  const cast = () => { const i = KIDS.indexOf(hero), n = KIDS.length; return { sib1: KIDS[(i + 1) % n], sib2: KIDS[(i + 2) % n] }; };
  const fill = t => { const c = cast(); return t.replace('{sib1}', FIGHTERS[idx(c.sib1)].name).replace('{sib2}', FIGHTERS[idx(c.sib2)].name); };
  const nameOf = f => (f.def ? f.def.name : FIGHTERS[idx(f.id)].name);

  function dots(now, done) { return '<div class="dots">' + CHAPTERS.map((c, i) => `<u class="${i < done ? 'done' : ''} ${i === now ? 'now' : ''}"></u>`).join('') + '</div>'; }
  function storyHome() {
    const p = prog(), done = Math.min(CHAPTERS.length, p[hero] || 0);
    el('storyCard').innerHTML = `<div class="chap">Story mode</div><h3>THE GOLDEN CLOG</h3>
      <p>Pick your fighter. Each one keeps their own progress.</p>
      <div class="pick">${KIDS.map(k => `<button data-hero="${k}" class="${k === hero ? 'on' : ''}" style="--c:${FIGHTERS[idx(k)].color}"><b>${FIGHTERS[idx(k)].name}</b><small>${Math.min(CHAPTERS.length, p[k] || 0)} / ${CHAPTERS.length}</small></button>`).join('')}</div>
      ${dots(done < CHAPTERS.length ? done : -1, done)}
      <div><button class="big alt" data-act="hub">BACK</button><button class="big" data-act="go">${done >= CHAPTERS.length ? 'PLAY AGAIN' : done ? 'CONTINUE' : 'START'}</button></div>`;
    showScreen('story'); setScene('story');
  }
  function chapterCard(i) {
    const c = CHAPTERS[i], h = cast(), foes = c.foes(h), ally = c.ally && c.ally(h);
    el('storyCard').innerHTML = `<div class="chap">Chapter ${i + 1} of ${CHAPTERS.length}</div><h3>${c.title.toUpperCase()}</h3>${dots(i, i)}
      <p>${fill(c.text)}</p>
      <div class="vs"><span style="border-bottom:3px solid ${FIGHTERS[idx(hero)].color}">${FIGHTERS[idx(hero)].name}${ally ? ' + ' + nameOf(ally) : ''}</span>VS<span style="border-bottom:3px solid #ff5a5a">${foes.map(nameOf).join(' + ')}</span></div>
      <div><button class="big alt" data-act="home">BACK</button><button class="big" data-act="fight" data-i="${i}">FIGHT!</button></div>`;
    showScreen('story'); setScene('story', { foes: foes.map(f => ({ id: f.def ? f.def.id : f.id, big: !!f.def })), ally: ally && ally.id });
  }
  function fightChapter(i) {
    const c = CHAPTERS[i], h = cast(), foes = c.foes(h), ally = c.ally && c.ally(h);
    const slot = (f, team, type) => ({ type, fighter: f.def ? 0 : idx(f.id), def: f.def, level: f.level || 5, team, stocks: f.stocks });
    const slots = [slot({ id: hero }, 1, 'human')];
    if (ally) slots.push(slot(ally, 1, 'cpu'));
    for (const f of foes) slots.push(slot(f, 2, 'cpu'));
    while (slots.length < 4) slots.push({ type: 'off' });
    Sfx.init(); startMatch({ stage: c.stage, stocks: 3, slots, story: { i } });
  }
  function storyResult() {
    const i = G.cfg.story.i, me = G.fighters[0], won = G.fighters.some(f => f.team === 1 && f.stocks > 0) && !G.fighters.some(f => f.team === 2 && f.stocks > 0);
    G.mode = 'results';
    if (won) { const p = prog(); p[hero] = Math.max(p[hero] || 0, i + 1); save.set('story', p); }
    const last = i === CHAPTERS.length - 1;
    el('storyCard').innerHTML = won
      ? (last ? `<div class="chap">The end</div><h3>THE CLOG COMES HOME</h3>${dots(-1, CHAPTERS.length)}<p>${ENDING}</p><div><button class="big" data-act="hub">MAIN MENU</button></div>`
        : `<div class="chap">Chapter ${i + 1} cleared</div><h3>VICTORY!</h3>${dots(i + 1, i + 1)}<p>${me.stats.kos} KO${me.stats.kos === 1 ? '' : 's'}, ${me.stats.dealt.toFixed(0)}% damage dealt, ${me.stocks} stock${me.stocks === 1 ? '' : 's'} left.</p><div><button class="big alt" data-act="hub">MAIN MENU</button><button class="big" data-act="chapter" data-i="${i + 1}">NEXT CHAPTER</button></div>`)
      : `<div class="chap">Chapter ${i + 1}</div><h3>SO CLOSE</h3>${dots(i, i)}<p>${CHAPTERS[i].boss ? 'Big Brick is heavy, so he takes a lot of damage before he flies. Stay out of reach of his charge and hit him after he misses.' : 'Shake it off and go again. Use your shield, and save your smash attacks for when their damage is high.'}</p><div><button class="big alt" data-act="hub">MAIN MENU</button><button class="big" data-act="fight" data-i="${i}">TRY AGAIN</button></div>`;
    showScreen('story'); setScene('story'); if (won) react(hero, 'victory', true);
  }
  el('storyCard').addEventListener('click', e => {
    const b = e.target.closest('button'); if (!b) return; Sfx.init(); Sfx.blip();
    if (b.dataset.hero) { hero = b.dataset.hero; save.set('hero', hero); storyHome(); react(hero, 'victory', true); return; }
    const a = b.dataset.act, i = +b.dataset.i;
    if (a === 'hub') openHub(); else if (a === 'home') storyHome(); else if (a === 'fight') fightChapter(i); else if (a === 'chapter') chapterCard(i);
    else if (a === 'go') { const d = prog()[hero] || 0; chapterCard(d >= CHAPTERS.length ? 0 : d); }
  });
  // story matches end on the story card; training never ends
  const baseResults = showResults;
  showResults = function () {
    if (G.cfg && G.cfg.story) return storyResult();
    baseResults();
    const w = [...G.fighters].sort((a, b) => (b.stocks - a.stocks) || (b.outT - a.outT) || (a.percent - b.percent))[0];
    if (w) { setScene('results', w.def.id); voiceOf(w.def.id); }
  };

  // ---------------------------------------------------------------- hub
  function openHub() {
    G.mode = 'title'; G.paused = false;
    const p = prog(), best = Math.max(0, ...KIDS.map(k => p[k] || 0));
    el('storyProg').textContent = best >= CHAPTERS.length ? 'COMPLETE' : best ? `CHAPTER ${best + 1} OF ${CHAPTERS.length}` : 'NEW';
    showScreen('hub'); tileArt(); const t = document.querySelector('#hub .tile'); if (t) t.focus({ preventScroll: true });
  }
  el('hub').addEventListener('click', e => {
    const b = e.target.closest('.tile'); if (!b) return; Sfx.init(); Sfx.blip();
    const m = b.dataset.mode;
    if (m === 'story') storyHome();
    else if (m === 'versus') { buildSelect(); showScreen('select'); }
    else if (m === 'watch') startDemo();
    else if (m === 'options') showScreen('options');
    else if (m === 'training') {
      startMatch({ stage: 'proving', stocks: 99, training: true, slots: [{ type: 'human', fighter: idx(hero), level: 1 }, { type: 'cpu', fighter: idx(cast().sib1), level: 1 }, { type: 'off' }, { type: 'off' }] });
      G.fighters[1].ai.cool = 1e9; G.banner = { text: 'TRAINING  ·  Esc to leave', t: 240, color: '#8affea' };
    }
  });
  // tiles lean toward the pointer, and the fighters react to the tile you are on
  const TILE_CLIP = { story: 'victory', versus: 'kick', training: 'jab', watch: 'spin', options: 'block' };
  for (const t of document.querySelectorAll('#hub .tile')) {
    t.addEventListener('pointermove', e => { const r = t.getBoundingClientRect(); t.style.setProperty('--ry', ((e.clientX - r.left) / r.width - 0.5) * 14 + 'deg'); t.style.setProperty('--rx', (0.5 - (e.clientY - r.top) / r.height) * 12 + 'deg'); t.style.setProperty('--mx', ((e.clientX - r.left) / r.width) * 100 + '%'); });
    t.addEventListener('pointerleave', () => { t.style.setProperty('--ry', '0deg'); t.style.setProperty('--rx', '0deg'); });
    const on = () => { if (t._hot) return; t._hot = true; Sfx.blip && Sfx.ctx && Sfx.blip(); react(hero, TILE_CLIP[t.dataset.mode]); };
    t.addEventListener('pointerenter', on); t.addEventListener('focus', on);
    t.addEventListener('pointerleave', () => { t._hot = false; }); t.addEventListener('blur', () => { t._hot = false; });
  }
  // fighter select: the newly chosen fighter steps up and calls out
  const baseBuild = buildSelect; let lastPick = [];
  buildSelect = function () {
    baseBuild();
    const now = cfg.slots.map(s => (s.type === 'off' ? null : s.fighter)); let changed = null;
    now.forEach((f, i) => { if (f != null && lastPick.length && lastPick[i] !== f) changed = i; });
    lastPick = now;
    if (el('select').classList.contains('on')) { setScene('select', changed); if (changed != null) voiceOf(FIGHTERS[now[changed]].id); }
  };

  // arrow keys move between tiles
  addEventListener('keydown', e => {
    if (!el('hub').classList.contains('on')) return;
    const tiles = [...document.querySelectorAll('#hub .tile')], i = tiles.indexOf(document.activeElement);
    const d = { ArrowRight: 1, ArrowDown: 1, ArrowLeft: -1, ArrowUp: -1 }[e.code]; if (!d) return;
    tiles[(i + d + tiles.length) % tiles.length].focus(); Sfx.blip(); e.preventDefault();
  });
  // Play: the logo pulses and rushes the camera, the screen flashes into flame, then the main menu appears
  let introBusy = false;
  const vid = el('introVid'); let vidOK = true; vid.addEventListener('error', () => { vidOK = false; });
  el('bPlay').onclick = () => {
    Sfx.init(); if (introBusy) return;
    if (still) { Sfx.blip(); openHub(); return; }
    introBusy = true; const intro = el('intro'), title = el('title');
    const done = () => { intro.classList.remove('on', 'vid'); title.classList.remove('leaving'); introBusy = false; };
    const fallback = () => { // the built-in zoom-and-flame transition, used if the video cannot play
      intro.classList.remove('vid'); intro.classList.add('on'); Sfx.od && Sfx.od(); setTimeout(() => Sfx.bigHit && Sfx.bigHit(), 760); setTimeout(openHub, 900); setTimeout(done, 1550);
    };
    if (!vidOK) return fallback();
    // the supplied start animation: the logo zooms in and bursts; the main menu appears under the burst
    let swapped = false; const swap = () => { if (!swapped) { swapped = true; openHub(); } };
    vid.currentTime = 0; vid.volume = Sfx.volume == null ? 1 : Sfx.volume;
    vid.ontimeupdate = () => { if (vid.currentTime >= 2.95) swap(); };
    vid.onended = () => { swap(); done(); };
    const p = vid.play();
    const started = () => { intro.classList.add('on', 'vid'); title.classList.add('leaving'); setTimeout(() => { if (introBusy && intro.classList.contains('vid')) { swap(); done(); vid.pause(); } }, 5200); };
    if (p && p.then) p.then(started).catch(() => { vidOK = false; fallback(); }); else started();
  };
  el('bBack').onclick = () => { Sfx.blip(); openHub(); };
  el('bQuit').onclick = () => openHub();
  el('bMenu').onclick = () => openHub();
  el('bOptBack').onclick = () => { Sfx.blip(); openHub(); };

  // ---------------------------------------------------------------- logo flames (drawn in code)
  const flameSets = [...document.querySelectorAll('.logo:not(.img)')].map(logo => {
    const cv = document.createElement('canvas'); cv.className = 'flames'; cv.width = 420; cv.height = 240; logo.prepend(cv);
    return { logo, cv, g: cv.getContext('2d'), ps: [] };
  });
  const still = window.matchMedia && matchMedia('(prefers-reduced-motion: reduce)').matches;
  function flames() {
    requestAnimationFrame(flames);
    if (G.mode === 'fight') return;
    for (const f of flameSets) {
      if (!f.logo.offsetParent) continue; // only the logo on the visible screen
      const { g, cv, ps } = f, W = cv.width, H = cv.height;
      // fire rises from behind the whole name, taller toward the right-hand end
      for (let i = 0; i < (still ? 0 : 10); i++) { const u = Math.random(), tall = 0.75 + u * 0.6; ps.push({ x: W * (0.13 + u * 0.72), y: H * (0.5 + Math.random() * 0.14), vx: (Math.random() - 0.4) * 1.1, vy: -(1.3 + Math.random() * 2.4) * tall, r: 13 + Math.random() * 17, t: 0, life: (24 + Math.random() * 26) * tall, spark: Math.random() < 0.14 }); }
      g.clearRect(0, 0, W, H); g.globalCompositeOperation = 'lighter';
      for (const p of ps) {
        p.t++; p.x += p.vx + Math.sin((p.t + p.r) * 0.2) * 0.7; p.y += p.vy; p.vy *= 0.985;
        const k = 1 - p.t / p.life; if (k <= 0) continue;
        if (p.spark) { g.fillStyle = 'rgba(255,230,150,' + k + ')'; g.beginPath(); g.arc(p.x + Math.sin(p.t * 0.3 + p.r) * 6, p.y - p.t * 1.1, 1.4, 0, 6.283); g.fill(); continue; }
        const r = p.r * (0.35 + k * 0.8), gr = g.createRadialGradient(p.x, p.y, 0, p.x, p.y, r);
        gr.addColorStop(0, 'rgba(255,' + Math.round(160 + 95 * k) + ',' + Math.round(40 + 150 * k * k) + ',' + 0.62 * k + ')'); gr.addColorStop(0.45, 'rgba(255,' + Math.round(70 + 80 * k) + ',18,' + 0.34 * k + ')'); gr.addColorStop(1, 'rgba(190,25,10,0)');
        g.fillStyle = gr; g.beginPath(); g.ellipse(p.x, p.y, r * 0.55, r * 1.5, 0, 0, 6.283); g.fill();
      }
      f.ps = ps.filter(p => p.t < p.life);
    }
  }
  requestAnimationFrame(flames);

  // ---------------------------------------------------------------- options
  const toggleFull = () => { const d = document; if (d.fullscreenElement) d.exitFullscreen(); else if (d.documentElement.requestFullscreen) d.documentElement.requestFullscreen().catch(() => {}); };
  el('optFull').onclick = () => { Sfx.blip(); toggleFull(); };
  addEventListener('keydown', e => { if (e.code === 'KeyF' && G.mode !== 'fight' && !e.repeat) toggleFull(); });
  const vol = el('optVol'); vol.value = save.get('vol', 100); Sfx.volume = vol.value / 100;
  vol.oninput = () => { Sfx.init(); Sfx.setVolume(vol.value / 100); save.set('vol', +vol.value); Sfx.blip(); };
  G.fxLow = !!save.get('fxLow', false);
  const fxBtns = () => el('optFx').querySelectorAll('button').forEach(b => b.classList.toggle('on', (b.dataset.q === '1') === G.fxLow));
  el('optFx').onclick = e => { const b = e.target.closest('button'); if (!b) return; G.fxLow = b.dataset.q === '1'; save.set('fxLow', G.fxLow); fxBtns(); Sfx.blip(); };
  fxBtns();
})();
