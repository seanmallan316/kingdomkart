'use strict';
// Super Family Smash: the announcer reads out whatever menu option is highlighted (mouse, touch, keyboard or controller),
// and the game turns sideways into landscape on phones and tablets.
(() => {
  const SEL = 'button, .tile, .pc';
  const inMenu = () => typeof G === 'undefined' || G.mode !== 'fight' || G.paused;
  const store = { get(k, d) { try { const v = localStorage.getItem('sfs_' + k); return v == null ? d : JSON.parse(v); } catch (e) { return d; } },
    set(k, v) { try { localStorage.setItem('sfs_' + k, JSON.stringify(v)); } catch (e) {} } };

  // ------------------------------------------------------------ menu announcer
  let voiceOn = store.get('menuVoice', true);
  const NUM = { 0: 'ZERO', 1: 'ONE', 2: 'TWO', 3: 'THREE', 4: 'FOUR', 5: 'FIVE', 6: 'SIX', 7: 'SEVEN', 8: 'EIGHT' };
  const ALIAS = { TOGGLE: 'FULLSCREEN', LOCKEDFIGHTER: 'LOCKED', FIGHT: '!annFight', TWOMIN: 'TWOMINUTES', THREEMIN: 'THREEMINUTES',
    FIVEMIN: 'FIVEMINUTES', EIGHTMIN: 'EIGHTMINUTES', NEWGAME: 'START', SELECT: 'FIGHTERSELECT' };
  const has = k => window.CLASH_SFX && window.CLASH_SFX['say_' + k];
  // the words to read for an element: its label, its title text, the fighter or item name, or failing that its text
  function phrase(el) {
    if (el.dataset.say) return el.dataset.say;
    if (el.classList.contains('add')) return 'ADD FIGHTER';
    const lab = el.getAttribute('aria-label'); if (lab) return lab;
    for (const q of [':scope > b', ':scope .rn', ':scope > .nm', ':scope .nm']) { const n = el.querySelector(q); if (n && n.textContent.trim().length > 1) return n.textContent; }
    return el.textContent;
  }
  function keyFor(el) {
    let t = (phrase(el) || '').toUpperCase().replace('∞', ' INFINITE ').replace(/🎮/g, '');
    t = t.replace(/\b(\d)\b/g, (m, d) => NUM[d] || d);
    const k = t.replace(/[^A-Z]/g, '');
    if (ALIAS[k]) return ALIAS[k];
    if (has(k)) return k;
    const first = t.trim().split(/\s+/)[0].replace(/[^A-Z]/g, ''); // "PLAYER 3" -> PLAYER
    return has(first) ? first : null;
  }
  let last = null, lastKey = null, lastT = 0, timer = 0;
  function speak(el, force) {
    if (!voiceOn || !el || !inMenu() || typeof Sfx === 'undefined') return;
    const k = keyFor(el); if (!k) return;
    const now = performance.now();
    if (!force && k === lastKey && now - lastT < 1200) return; // same thing again straight away: stay quiet
    clearTimeout(timer);
    if (force) { lastKey = k; lastT = now; if (k[0] === '!') { if (Sfx.announce) Sfx.announce(k.slice(1)); } else if (Sfx.say) Sfx.say(k); return; }
    // a short wait so sweeping across many options only reads the one you stop on
    timer = setTimeout(() => {
      lastKey = k; lastT = performance.now();
      if (k[0] === '!') { if (Sfx.announce) Sfx.announce(k.slice(1)); } else if (Sfx.say) Sfx.say(k);
    }, 70);
  }
  const target = e => { const el = e.target && e.target.closest && e.target.closest(SEL); return el && !el.closest('#touch') && !el.disabled ? el : null; };
  document.addEventListener('pointerover', e => { const el = target(e); if (el && el !== last) { last = el; speak(el); } });
  document.addEventListener('pointerout', e => { if (last && !last.contains(e.relatedTarget)) last = null; });
  document.addEventListener('focusin', e => { const el = target(e); if (el && el !== last) { last = el; speak(el); } });
  // picking a fighter (roster tile, story hero, Locker) always calls out the name, like a fighting game's character select
  const PICK = '.rt:not(.lock), [data-hero], #lkPick button';
  document.addEventListener('click', e => {
    const el = e.target.closest && e.target.closest(PICK); if (!el) return;
    if (el.matches('.rand')) return speak(el, true); // "Random!"
    speak(el, true);
  }, true);
  window.MENU_SAY = { keyFor, speak, get on() { return voiceOn; } };

  // ------------------------------------------------------------ landscape
  // Phones and tablets: going into a fight (or pressing PLAY) goes full screen and locks the screen sideways where the
  // browser allows it (Android). Where it does not (iPhone Safari), a "turn your phone" card shows while held upright.
  let wantLand = store.get('landscape', true);
  const touch = matchMedia('(pointer: coarse)').matches || 'ontouchstart' in window;
  async function goLandscape() {
    if (!wantLand || !touch) return;
    const d = document, de = d.documentElement;
    try { if (!d.fullscreenElement && de.requestFullscreen) await de.requestFullscreen({ navigationUI: 'hide' }); } catch (e) {}
    try { if (screen.orientation && screen.orientation.lock) await screen.orientation.lock('landscape'); } catch (e) {}
    check();
  }
  const card = document.createElement('div'); card.id = 'rotate';
  card.innerHTML = '<div class="ph">📱</div><b>TURN YOUR PHONE SIDEWAYS</b><small>Super Family Smash plays best in landscape.</small>' + (screen.orientation && screen.orientation.lock ? '<button id="rotGo" data-say="LANDSCAPE">ROTATE FOR ME</button>' : '') + '<button id="rotKeep">KEEP UPRIGHT</button>';
  document.body.appendChild(card);
  let dismissed = false;
  const portrait = () => innerHeight > innerWidth * 1.1;
  function check() { card.classList.toggle('on', wantLand && touch && !dismissed && portrait()); }
  const go = card.querySelector('#rotGo'); if (go) go.onclick = e => { e.stopPropagation(); const w = wantLand; wantLand = true; goLandscape().then(() => { wantLand = w; check(); }); };
  card.querySelector('#rotKeep').onclick = e => { e.stopPropagation(); dismissed = true; check(); };
  addEventListener('resize', check); addEventListener('orientationchange', () => setTimeout(check, 200));
  if (screen.orientation && screen.orientation.addEventListener) screen.orientation.addEventListener('change', check);
  // the first tap anywhere (a user gesture, which full screen and the lock need) and every fight start
  document.addEventListener('pointerup', function once(e) {
    if (e.target.closest && e.target.closest('#bPlay, #bFight, [data-act="fight"], [data-act="go"], .tile')) goLandscape();
  }, true);
  setTimeout(check, 300);

  // ------------------------------------------------------------ two new rows in Options
  function addRows() {
    const opts = document.querySelector('#options .card'); if (!opts || document.getElementById('optSay')) return;
    const anchor = opts.querySelector('#optFx'); const row = anchor && anchor.closest('.optrow'); if (!row) return;
    const mk = (html) => { const d = document.createElement('div'); d.className = 'optrow'; d.innerHTML = html; row.after(d); return d; };
    const r2 = mk('<span>Screen <small style="color:var(--dim)">(phones and tablets)</small></span><div class="row" id="optLand"><button data-l="1" data-say="LANDSCAPE">LANDSCAPE</button><button data-l="0" data-say="AUTO">AUTO</button></div>');
    const r1 = mk('<span>Menu announcer</span><div class="row" id="optSay"><button data-v="1">ON</button><button data-v="0">OFF</button></div>');
    const paint = () => {
      r1.querySelectorAll('button').forEach(b => b.classList.toggle('on', (b.dataset.v === '1') === voiceOn));
      r2.querySelectorAll('button').forEach(b => b.classList.toggle('on', (b.dataset.l === '1') === wantLand));
    };
    r1.onclick = e => { const b = e.target.closest('button'); if (!b) return; voiceOn = b.dataset.v === '1'; store.set('menuVoice', voiceOn); paint(); if (voiceOn) { lastKey = null; speak(b); } };
    r2.onclick = e => {
      const b = e.target.closest('button'); if (!b) return; wantLand = b.dataset.l === '1'; store.set('landscape', wantLand); dismissed = false; paint();
      if (wantLand) goLandscape(); else { try { screen.orientation.unlock(); } catch (er) {} check(); }
    };
    paint();
  }
  addRows(); document.addEventListener('DOMContentLoaded', addRows);
})();

// the title screen's background video only runs while the title is on screen
(() => {
  const v = document.getElementById('titleBg'), t = document.getElementById('title'); if (!v || !t) return;
  v.muted = true; v.defaultMuted = true; v.volume = 0;
  const sync = () => { const on = t.classList.contains('on'); if (on && v.paused) v.play().catch(() => {}); else if (!on && !v.paused) v.pause(); };
  new MutationObserver(sync).observe(t, { attributes: true, attributeFilter: ['class'] });
  addEventListener('pointerdown', sync, true); sync();
})();
