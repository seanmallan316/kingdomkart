'use strict';
// Menu music: the family's own song plays on the title screen, the main menu and fighter select, looping. It starts on
// the first tap or key (browsers block sound before that), dips under the intro video, fades out when a match starts
// and fades back in on the menus. It follows the sound volume in Options.
(() => {
  const src = window.CLASH_MUSIC && window.CLASH_MUSIC.menu; if (!src) return;
  const a = new Audio(src); a.loop = true; a.preload = 'auto'; a.volume = 0;
  let want = 0, cur = 0, started = false;
  const vol = () => { try { return (typeof Sfx !== 'undefined' && Sfx.volume != null ? Sfx.volume : 1) * 0.55; } catch (e) { return 0.55; } };
  function target() {
    const fight = typeof G !== 'undefined' && (G.mode === 'fight' || G.mode === 'results');
    const intro = document.getElementById('intro'), vid = intro && intro.classList.contains('vid');
    const menus = ['title', 'hub', 'select', 'options', 'locker', 'story'].some(id => { const e = document.getElementById(id); return e && e.classList.contains('on'); });
    return !fight && menus ? (vid ? 0.15 : 1) : 0;
  }
  function start() { if (started) return; started = true; a.play().catch(() => { started = false; }); }
  addEventListener('pointerdown', start, true); addEventListener('keydown', start, true);
  addEventListener('gamepadconnected', start);
  let last = performance.now();
  setInterval(() => {
    const now = performance.now(), dt = Math.min(1000, now - last) / 1000; last = now;
    want = target() * vol();
    cur += (want - cur) * Math.min(1, dt * (want > cur ? 1.4 : 3)); // about 1.5 s fade in, 0.7 s fade out
    if (Math.abs(cur - want) < 0.002) cur = want;
    a.volume = Math.max(0, Math.min(1, cur));
    if (started && cur > 0.003 && a.paused) a.play().catch(() => {});
    if (cur <= 0.003 && !a.paused && want === 0) a.pause();
  }, 50);
  window.MENU_MUSIC = a;
})();
