'use strict';
// Super Family Smash: controllers. Works with any controller the browser sees, wired or Bluetooth (Xbox, PlayStation,
// Switch Pro, 8BitDo and generic pads), up to 8 players.
// - Each human player slot takes the next connected controller in order (P1 the first pad, the next human the second...),
//   plus keyboard (first two humans) and touch (first human).
// - Pads with a non-standard layout are calibrated at rest, so triggers resting at -1 do not read as a held stick,
//   and a hat-switch d-pad is decoded.
// - Connect / disconnect messages, rumble on hits, and full menu navigation with a controller.
(() => {
  const list = () => { const ps = navigator.getGamepads ? navigator.getGamepads() : []; const out = []; for (const p of ps || []) if (p && p.connected !== false) out.push(p); return out.sort((a, b) => a.index - b.index); };
  const padFor = k => { const l = list(); return l[k] || null; };
  const short = id => (id || 'Controller').replace(/\(.*?\)/g, '').replace(/STANDARD GAMEPAD|Vendor: \w+|Product: \w+/gi, '').replace(/\s+/g, ' ').trim().slice(0, 28) || 'Controller';
  window.padLabel = k => { const p = padFor(k); return p ? '🎮 ' + short(p.id) : k === 0 ? (matchMedia('(pointer: coarse)').matches ? '✋ TOUCH' : '⌨ KEYS') : k === 1 ? '⌨ ARROWS' : '🎮 NONE YET'; };

  // ------------------------------------------------------------ reading a pad
  const rest = new Map(); // pad index -> axis values at rest (for non-standard pads)
  function readPad(p) {
    const o = emptyIn(); if (!p) return o;
    const std = p.mapping === 'standard';
    if (!std && !rest.has(p.index)) rest.set(p.index, p.axes.map(v => (Math.abs(v) > 0.5 ? v : 0)));
    const r = rest.get(p.index) || [];
    const ax = n => { const v = (p.axes[n] || 0) - (std ? 0 : r[n] || 0); return Math.abs(v) < 0.2 ? 0 : Math.max(-1, Math.min(1, v)); };
    const b = n => !!(p.buttons[n] && (p.buttons[n].pressed || p.buttons[n].value > 0.5));
    o.x = ax(0); o.y = ax(1);
    let du = b(12), dd = b(13), dl = b(14), dr = b(15);
    if (!std) { // hat switch on axis 9 (common on Bluetooth pads in non-standard mode), or a d-pad on axes 6/7
      const h = p.axes[9];
      if (h != null && h >= -1.05 && h <= 1.01) { const k = Math.round((h + 1) * 3.5) % 8; du = du || k === 7 || k === 0 || k === 1; dr = dr || (k >= 1 && k <= 3); dd = dd || (k >= 3 && k <= 5); dl = dl || (k >= 5 && k <= 7); }
      if (p.axes.length >= 8 && p.axes.length < 9) { if (ax(6) < -0.5) dl = true; if (ax(6) > 0.5) dr = true; if (ax(7) < -0.5) du = true; if (ax(7) > 0.5) dd = true; }
    }
    if (dl) o.x = -1; if (dr) o.x = 1; if (du) o.y = -1; if (dd) o.y = 1;
    o.analog = !(du || dd || dl || dr);
    o.attack = b(0); o.special = b(1); o.jump = b(2) || b(3); o.shield = b(4) || b(5) || b(6); o.grab = b(7);
    const rx = ax(2), ry = std ? ax(3) : (p.axes.length > 5 ? ax(5) : ax(3));
    if (Math.hypot(rx, ry) > 0.6) { o.smash = true; o.sx = Math.abs(rx) > Math.abs(ry) ? sgn(rx) : 0; o.sy = Math.abs(ry) >= Math.abs(rx) ? sgn(ry) : 0; }
    o._start = b(9); o._select = b(8);
    return o;
  }
  const prevStart = {};
  padPoll = function (k) {
    const p = padFor(k); const o = readPad(p); if (!p) return o;
    if (o._start && !prevStart[p.index] && G.mode === 'fight') togglePause();
    prevStart[p.index] = o._start; return o;
  };
  // human player k (0 = first human) uses the k-th keyboard map, the k-th connected controller and, for the first, touch
  humanCtrl = function (port) {
    const k = G.cfg && G.cfg.slots ? G.cfg.slots.slice(0, port).filter(s => s.type === 'human').length : port;
    return { k, poll: () => mergeIn([keyPoll(k), padPoll(k), k === 0 ? touchPoll() : emptyIn()]) };
  };

  // ------------------------------------------------------------ rumble on hits (if the controller supports it)
  function rumble(f, strong, ms) {
    if (!f || f.cpu || !f.ctrl || f.ctrl.k == null) return; const p = padFor(f.ctrl.k); const a = p && p.vibrationActuator;
    if (a && a.playEffect) try { a.playEffect('dual-rumble', { duration: ms, strongMagnitude: Math.min(1, strong), weakMagnitude: Math.min(1, strong * 0.7) }); } catch (e) {}
  }
  const baseHit = applyHit;
  applyHit = function (att, vic, hb, dir, o) {
    const r = baseHit(att, vic, hb, dir, o || {});
    if (vic && vic.state === 'hitstun') { const d = hb.d || 5; rumble(vic, 0.25 + d / 30, 90 + d * 6); if (att && !(o && o.projectile)) rumble(att, 0.12 + d / 60, 60); }
    return r;
  };
  const baseKo = ko;
  ko = function (f) { rumble(f, 1, 420); return baseKo(f); };

  // ------------------------------------------------------------ connect / disconnect messages
  const toast = document.createElement('div'); toast.id = 'padtoast'; document.body.appendChild(toast); let tT = 0;
  function say(t) { toast.textContent = t; toast.classList.add('on'); clearTimeout(tT); tT = setTimeout(() => toast.classList.remove('on'), 2600); }
  addEventListener('gamepadconnected', e => {
    const k = list().findIndex(p => p.index === e.gamepad.index);
    say('🎮 Controller ' + (k + 1) + ' connected: ' + short(e.gamepad.id));
    try { Sfx.init(); Sfx.sample && Sfx.sample('itemGet', 0.5, 1.2); } catch (er) {}
    if (document.getElementById('select').classList.contains('on') && typeof buildSelect === 'function') buildSelect();
    const p = e.gamepad.vibrationActuator; if (p && p.playEffect) try { p.playEffect('dual-rumble', { duration: 200, strongMagnitude: 0.5, weakMagnitude: 0.5 }); } catch (er) {}
  });
  addEventListener('gamepaddisconnected', e => {
    rest.delete(e.gamepad.index);
    say('Controller disconnected: ' + short(e.gamepad.id));
    if (G.mode === 'fight' && !G.paused && G.fighters.some(f => !f.cpu)) togglePause();
    if (document.getElementById('select').classList.contains('on') && typeof buildSelect === 'function') buildSelect();
  });

  // ------------------------------------------------------------ menus with a controller: move, A to choose, B to go back
  const prev = {}; let repeatT = 0, lastDir = '';
  const visible = el => { const r = el.getBoundingClientRect(); return r.width > 0 && r.height > 0 && getComputedStyle(el).visibility !== 'hidden' && !el.closest('#touch'); };
  const items = () => { const s = document.querySelector('.screen.on'); return s ? [...s.querySelectorAll('button, .tile, .pc')].filter(visible) : []; };
  function moveFocus(dx, dy) {
    const all = items(); if (!all.length) return;
    let cur = document.activeElement; if (!all.includes(cur)) { all[0].focus({ preventScroll: true }); return; }
    const a = cur.getBoundingClientRect(), ax = a.left + a.width / 2, ay = a.top + a.height / 2;
    let best = null, bs = 1e9;
    for (const el of all) {
      if (el === cur) continue; const r = el.getBoundingClientRect(), x = r.left + r.width / 2 - ax, y = r.top + r.height / 2 - ay;
      const along = x * dx + y * dy, side = Math.abs(x * dy - y * dx); if (along <= 4) continue;
      const sc = along + side * 2.2; if (sc < bs) { bs = sc; best = el; }
    }
    if (best) { best.focus({ preventScroll: true }); best.scrollIntoView({ block: 'nearest', inline: 'nearest' }); try { Sfx.blip(); } catch (e) {} }
  }
  function back() {
    const s = document.querySelector('.screen.on'); if (!s) return;
    const b = s.querySelector('#bBack, #bOptBack, #bQuit, #bMenu') || [...s.querySelectorAll('button')].find(x => /BACK/i.test(x.textContent));
    if (b) b.click(); else if (s.id === 'story') { const h = document.getElementById('hub'); if (h) { showScreen('hub'); } }
  }
  function press(el, k) {
    if (!el) return; window.__padK = k == null ? null : k; setTimeout(() => { window.__padK = null; }, 0); const r = el.getBoundingClientRect(), x = r.left + r.width / 2, y = r.top + r.height / 2;
    el.dispatchEvent(new PointerEvent('pointerdown', { bubbles: true, clientX: x, clientY: y, pointerType: 'gamepad' }));
    el.click();
  }
  (function menuLoop() {
    requestAnimationFrame(menuLoop);
    const pads = list(); if (!pads.length) return;
    const fighting = G.mode === 'fight' && !G.paused;
    for (const p of pads) {
      const o = readPad(p), was = prev[p.index] || {}; prev[p.index] = o;
      if (fighting) continue;
      const tap = n => o[n] && !was[n];
      // direction with auto-repeat
      const dir = Math.abs(o.x) > 0.6 || Math.abs(o.y) > 0.6 ? (Math.abs(o.x) > Math.abs(o.y) ? (o.x > 0 ? 'r' : 'l') : (o.y > 0 ? 'd' : 'u')) : '';
      const now = performance.now();
      if (dir && (dir !== lastDir || now > repeatT)) { repeatT = now + (dir !== lastDir ? 380 : 140); lastDir = dir; moveFocus(dir === 'r' ? 1 : dir === 'l' ? -1 : 0, dir === 'd' ? 1 : dir === 'u' ? -1 : 0); }
      if (!dir && lastDir && pads.every(q => { const s = prev[q.index] || {}; return Math.abs(s.x) < 0.6 && Math.abs(s.y) < 0.6; })) lastDir = '';
      if (tap('attack') || (tap('_start') && !document.querySelector('#title.on'))) {
        try { Sfx.init(); } catch (e) {}
        const el = document.activeElement && items().includes(document.activeElement) ? document.activeElement : items()[0];
        if (document.querySelector('#title.on')) press(document.getElementById('bPlay')); else press(el, pads.indexOf(p));
      } else if (tap('_start') && document.querySelector('#title.on')) { try { Sfx.init(); } catch (e) {} press(document.getElementById('bPlay')); }
      if (tap('special') || tap('_select')) back();
    }
  })();
})();
