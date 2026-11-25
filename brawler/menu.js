'use strict';
// Main menu hub, story mode, training and options for Clash Core. Loaded after game.js and fx.js.
(() => {
  const el = id => document.getElementById(id);
  const save = { get(k, d) { try { const v = localStorage.getItem('clash_' + k); return v == null ? d : JSON.parse(v); } catch (e) { return d; } }, set(k, v) { try { localStorage.setItem('clash_' + k, JSON.stringify(v)); } catch (e) { /* storage can be blocked; the game still works */ } } };
  const idx = id => FIGHTERS.findIndex(f => f.id === id);
  const KIDS = ['ray', 'norah', 'ollie'];

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
  const cast = () => { const o = KIDS.filter(k => k !== hero); return { sib1: o[0], sib2: o[1] }; };
  const fill = t => { const c = cast(); return t.replace('{sib1}', FIGHTERS[idx(c.sib1)].name).replace('{sib2}', FIGHTERS[idx(c.sib2)].name); };
  const nameOf = f => (f.def ? f.def.name : FIGHTERS[idx(f.id)].name);

  function dots(now, done) { return '<div class="dots">' + CHAPTERS.map((c, i) => `<u class="${i < done ? 'done' : ''} ${i === now ? 'now' : ''}"></u>`).join('') + '</div>'; }
  function storyHome() {
    const p = prog(), done = Math.min(CHAPTERS.length, p[hero] || 0);
    el('storyCard').innerHTML = `<div class="chap">Story mode</div><h3>THE GOLDEN CLOG</h3>
      <p>Pick your fighter. Each one keeps their own progress.</p>
      <div class="pick">${KIDS.map(k => `<button data-hero="${k}" class="${k === hero ? 'on' : ''}"><canvas width="200" height="120"></canvas><b>${FIGHTERS[idx(k)].name}</b><br><small>${Math.min(CHAPTERS.length, p[k] || 0)} / ${CHAPTERS.length}</small></button>`).join('')}</div>
      ${dots(done < CHAPTERS.length ? done : -1, done)}
      <div><button class="big alt" data-act="hub">BACK</button><button class="big" data-act="go">${done >= CHAPTERS.length ? 'PLAY AGAIN' : done ? 'CONTINUE' : 'START'}</button></div>`;
    el('storyCard').querySelectorAll('.pick button').forEach(b => drawPortrait(b.querySelector('canvas'), FIGHTERS[idx(b.dataset.hero)]));
    showScreen('story');
  }
  function chapterCard(i) {
    const c = CHAPTERS[i], h = cast(), foes = c.foes(h), ally = c.ally && c.ally(h);
    el('storyCard').innerHTML = `<div class="chap">Chapter ${i + 1} of ${CHAPTERS.length}</div><h3>${c.title.toUpperCase()}</h3>${dots(i, i)}
      <p>${fill(c.text)}</p>
      <div class="vs"><span style="color:${FIGHTERS[idx(hero)].color}">${FIGHTERS[idx(hero)].name}${ally ? ' + ' + nameOf(ally) : ''}</span>VS<span>${foes.map(nameOf).join(' + ')}</span></div>
      <div><button class="big alt" data-act="home">BACK</button><button class="big" data-act="fight" data-i="${i}">FIGHT!</button></div>`;
    showScreen('story');
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
    showScreen('story');
  }
  el('storyCard').addEventListener('click', e => {
    const b = e.target.closest('button'); if (!b) return; Sfx.init(); Sfx.blip();
    if (b.dataset.hero) { hero = b.dataset.hero; save.set('hero', hero); storyHome(); return; }
    const a = b.dataset.act, i = +b.dataset.i;
    if (a === 'hub') openHub(); else if (a === 'home') storyHome(); else if (a === 'fight') fightChapter(i); else if (a === 'chapter') chapterCard(i);
    else if (a === 'go') { const d = prog()[hero] || 0; chapterCard(d >= CHAPTERS.length ? 0 : d); }
  });
  // story matches end on the story card; training never ends
  const baseResults = showResults;
  showResults = function () { if (G.cfg && G.cfg.story) storyResult(); else baseResults(); };

  // ---------------------------------------------------------------- hub
  function openHub() {
    G.mode = 'title'; G.paused = false;
    const p = prog(), best = Math.max(0, ...KIDS.map(k => p[k] || 0));
    el('storyProg').textContent = best >= CHAPTERS.length ? 'COMPLETE' : best ? `CHAPTER ${best + 1} OF ${CHAPTERS.length}` : 'NEW';
    showScreen('hub'); const t = document.querySelector('#hub .tile'); if (t) t.focus({ preventScroll: true });
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
  // arrow keys move between tiles
  addEventListener('keydown', e => {
    if (!el('hub').classList.contains('on')) return;
    const tiles = [...document.querySelectorAll('#hub .tile')], i = tiles.indexOf(document.activeElement);
    const d = { ArrowRight: 1, ArrowDown: 1, ArrowLeft: -1, ArrowUp: -1 }[e.code]; if (!d) return;
    tiles[(i + d + tiles.length) % tiles.length].focus(); Sfx.blip(); e.preventDefault();
  });
  el('bPlay').onclick = () => { Sfx.init(); Sfx.blip(); openHub(); };
  el('bBack').onclick = () => { Sfx.blip(); openHub(); };
  el('bQuit').onclick = () => openHub();
  el('bMenu').onclick = () => openHub();
  el('bOptBack').onclick = () => { Sfx.blip(); openHub(); };

  // ---------------------------------------------------------------- options
  const vol = el('optVol'); vol.value = save.get('vol', 100); Sfx.volume = vol.value / 100;
  vol.oninput = () => { Sfx.init(); Sfx.setVolume(vol.value / 100); save.set('vol', +vol.value); Sfx.blip(); };
  G.fxLow = !!save.get('fxLow', false);
  const fxBtns = () => el('optFx').querySelectorAll('button').forEach(b => b.classList.toggle('on', (b.dataset.q === '1') === G.fxLow));
  el('optFx').onclick = e => { const b = e.target.closest('button'); if (!b) return; G.fxLow = b.dataset.q === '1'; save.set('fxLow', G.fxLow); fxBtns(); Sfx.blip(); };
  fxBtns();
})();
