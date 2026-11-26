'use strict';
// Music: the family's own songs. The menu song plays on the title, main menu, fighter select, options, story and Locker
// screens; each stage can have its own song during matches (music/<theme>.mp3, e.g. treehouse). Songs loop, start on
// the first tap / key (browsers block sound before that), cross-fade when the screen changes, dip under the intro video
// and follow the sound volume in Options.
(() => {
  const SRC = window.CLASH_MUSIC || {}; if (!Object.keys(SRC).length) return;
  const tracks = {}; let started = false, last = performance.now();
  const track = name => { if (!SRC[name]) return null; if (!tracks[name]) { const a = new Audio(SRC[name]); a.loop = true; a.preload = 'auto'; a.volume = 0; tracks[name] = { a, v: 0 }; } return tracks[name]; };
  const vol = () => { try { return (typeof Sfx !== 'undefined' && Sfx.volume != null ? Sfx.volume : 1) * 0.55; } catch (e) { return 0.55; } };
  function want() {
    if (typeof G === 'undefined') return { name: 'menu', k: 1 };
    if (G.mode === 'fight' || G.mode === 'results') {
      const th = G.stage && G.stage.theme; const name = th && SRC[th] ? th : null;
      return { name, k: G.paused ? 0.35 : G.mode === 'results' ? 0.6 : 0.8 };
    }
    const intro = document.getElementById('intro'), vid = intro && intro.classList.contains('vid');
    return { name: 'menu', k: vid ? 0.15 : 1 };
  }
  function start() { if (started) return; started = true; const w = want(); const t = w.name && track(w.name); if (t) t.a.play().catch(() => { started = false; }); }
  addEventListener('pointerdown', start, true); addEventListener('keydown', start, true); addEventListener('gamepadconnected', start);
  setInterval(() => {
    const now = performance.now(), dt = Math.min(1000, now - last) / 1000; last = now;
    const w = want(); if (w.name) track(w.name);
    for (const name in tracks) {
      const t = tracks[name], target = started && name === w.name ? w.k * vol() : 0;
      t.v += (target - t.v) * Math.min(1, dt * (target > t.v ? 1.4 : 3));
      if (Math.abs(t.v - target) < 0.002) t.v = target;
      t.a.volume = Math.max(0, Math.min(1, t.v));
      if (target > 0 && t.a.paused) { if (name !== 'menu' && t.a.currentTime > 0 && t.v < 0.01) t.a.currentTime = 0; t.a.play().catch(() => {}); }
      if (t.v <= 0.003 && target === 0 && !t.a.paused) t.a.pause();
    }
  }, 50);
  window.MENU_MUSIC = track('menu').a; window.MUSIC = tracks;
})();
