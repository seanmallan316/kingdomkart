'use strict';
// New Challengers: the luxury menu layer. Every menu selection wobbles or spins with a burst of gold, a gold selector
// ring glides to whatever you point at, gold dust drifts behind the menus, the cursor leaves sparkles, screens change
// with a gold sweep, the winner gets confetti and the 3D menu scene leans with the pointer. Gameplay is untouched.
(() => {
  const TAU = Math.PI * 2, reduce = matchMedia('(prefers-reduced-motion: reduce)').matches;
  const inMenu = () => typeof G === 'undefined' || G.mode !== 'fight' || G.paused;
  const SEL = 'button, .tile, .pc';
  const isMenuEl = el => el && !el.closest('#touch') && !el.disabled;

  // ------------------------------------------------------------ overlay canvas for bursts, dust, sparkles, confetti
  const cv = document.createElement('canvas'); cv.id = 'luxfx'; document.body.appendChild(cv);
  const g = cv.getContext('2d'); let W = 0, H = 0, DPR = 1;
  const size = () => { DPR = Math.min(2, devicePixelRatio || 1); W = innerWidth; H = innerHeight; cv.width = W * DPR; cv.height = H * DPR; };
  size(); addEventListener('resize', size);
  const sel = document.createElement('div'); sel.id = 'luxsel'; document.body.appendChild(sel);
  const wipe = document.createElement('div'); wipe.id = 'luxwipe'; document.body.appendChild(wipe);

  let parts = [];
  const GOLD = ['#fff6c9', '#ffe08a', '#ffc94a', '#e8a23a', '#ffffff'];
  const pick = a => a[Math.floor(Math.random() * a.length)];
  function burst(x, y, accent, big) {
    const n = big ? 46 : 26, cols = accent ? GOLD.concat([accent, accent]) : GOLD;
    for (let i = 0; i < n; i++) {
      const a = Math.random() * TAU, s = (big ? 5 : 3.5) + Math.random() * (big ? 9 : 6);
      parts.push({ k: Math.random() < 0.22 ? 'star' : 'spark', x, y, vx: Math.cos(a) * s, vy: Math.sin(a) * s - 1, g: 0.12, d: 0.93, t: 0, life: 36 + Math.random() * 26, size: 2 + Math.random() * (big ? 5 : 3.5), c: pick(cols), rot: Math.random() * TAU, vr: (Math.random() - 0.5) * 0.4 });
    }
    parts.push({ k: 'ring', x, y, t: 0, life: 26, size: big ? 150 : 90, c: accent || '#ffe08a' });
    parts.push({ k: 'ring', x, y, t: -4, life: 30, size: big ? 220 : 130, c: '#ffffff' });
    parts.push({ k: 'rays', x, y, t: 0, life: 22, size: big ? 200 : 120, c: accent || '#ffe08a', rot: Math.random() * TAU });
    parts.push({ k: 'flash', x, y, t: 0, life: 10, size: big ? 120 : 70, c: '#fff4d0' });
    kick();
  }
  function confetti(n = 160) {
    const cols = ['#ffd23f', '#ff6ad5', '#6cf0ff', '#8affea', '#ffffff', '#ff8a3d', '#b48cff'];
    for (let i = 0; i < n; i++) parts.push({ k: 'conf', x: Math.random() * W, y: -20 - Math.random() * H * 0.6, vx: (Math.random() - 0.5) * 2, vy: 1.5 + Math.random() * 2.5, g: 0.02, d: 0.995, t: 0, life: 360, size: 5 + Math.random() * 6, c: pick(cols), rot: Math.random() * TAU, vr: (Math.random() - 0.5) * 0.3, w: Math.random() * TAU });
    kick();
  }
  // ambient gold dust (soft bokeh), only behind the menus
  const dust = Array.from({ length: 34 }, () => ({ x: Math.random(), y: Math.random(), r: 1 + Math.random() * 3.5, s: 0.0004 + Math.random() * 0.0012, ph: Math.random() * TAU, a: 0.15 + Math.random() * 0.35 }));
  let running = false, frame = 0;
  function kick() { if (!running) { running = true; requestAnimationFrame(tick); } }
  function star(x, y, r, rot) { g.beginPath(); for (let i = 0; i < 10; i++) { const rr = i % 2 ? r * 0.45 : r, a = rot + i * Math.PI / 5; g.lineTo(x + Math.cos(a) * rr, y + Math.sin(a) * rr); } g.closePath(); g.fill(); }
  function tick() {
    frame++;
    g.setTransform(DPR, 0, 0, DPR, 0, 0); g.clearRect(0, 0, W, H);
    const menu = inMenu();
    if (menu && !reduce) {
      g.globalCompositeOperation = 'lighter';
      for (const d of dust) {
        d.y -= d.s; if (d.y < -0.05) { d.y = 1.05; d.x = Math.random(); }
        const x = (d.x + Math.sin(frame * 0.004 + d.ph) * 0.02) * W, y = d.y * H, tw = 0.6 + 0.4 * Math.sin(frame * 0.03 + d.ph);
        const gr = g.createRadialGradient(x, y, 0, x, y, d.r * 4); gr.addColorStop(0, `rgba(255,220,140,${d.a * tw})`); gr.addColorStop(1, 'rgba(255,200,100,0)');
        g.fillStyle = gr; g.beginPath(); g.arc(x, y, d.r * 4, 0, TAU); g.fill();
      }
    }
    g.globalCompositeOperation = 'lighter';
    for (const p of parts) {
      p.t++; if (p.t < 0) continue;
      const u = p.t / p.life, a = Math.max(0, 1 - u);
      if (p.k === 'spark' || p.k === 'star' || p.k === 'conf' || p.k === 'trail') { p.vx *= p.d || 0.95; p.vy = p.vy * (p.d || 0.95) + (p.g || 0); p.x += p.vx; p.y += p.vy; p.rot = (p.rot || 0) + (p.vr || 0); }
      if (p.k === 'spark' || p.k === 'trail') { g.fillStyle = p.c; g.globalAlpha = a; g.beginPath(); g.arc(p.x, p.y, p.size * (1 - u * 0.5), 0, TAU); g.fill(); g.globalAlpha = a * 0.35; g.beginPath(); g.arc(p.x, p.y, p.size * 3, 0, TAU); g.fill(); }
      else if (p.k === 'star') { g.fillStyle = p.c; g.globalAlpha = a; star(p.x, p.y, p.size * 1.8, p.rot); }
      else if (p.k === 'ring') { g.strokeStyle = p.c; g.globalAlpha = a * 0.9; g.lineWidth = 4 * a + 1; g.beginPath(); g.arc(p.x, p.y, p.size * (1 - Math.pow(1 - u, 3)), 0, TAU); g.stroke(); }
      else if (p.k === 'rays') { g.strokeStyle = p.c; g.globalAlpha = a * 0.7; g.lineWidth = 3; for (let k = 0; k < 12; k++) { const an = p.rot + k * TAU / 12 + u * 0.6, r0 = p.size * 0.25 * (1 + u), r1 = p.size * (0.5 + u * 0.6); g.beginPath(); g.moveTo(p.x + Math.cos(an) * r0, p.y + Math.sin(an) * r0); g.lineTo(p.x + Math.cos(an) * r1, p.y + Math.sin(an) * r1); g.stroke(); } }
      else if (p.k === 'flash') { const gr = g.createRadialGradient(p.x, p.y, 0, p.x, p.y, p.size); gr.addColorStop(0, `rgba(255,250,230,${a})`); gr.addColorStop(1, 'rgba(255,220,150,0)'); g.globalAlpha = 1; g.fillStyle = gr; g.beginPath(); g.arc(p.x, p.y, p.size, 0, TAU); g.fill(); }
      else if (p.k === 'conf') { g.globalCompositeOperation = 'source-over'; g.globalAlpha = Math.min(1, a * 3); g.fillStyle = p.c; g.save(); g.translate(p.x + Math.sin(p.t * 0.05 + p.w) * 12, p.y); g.rotate(p.rot); g.scale(1, Math.abs(Math.sin(p.t * 0.12 + p.w))); g.fillRect(-p.size / 2, -p.size / 4, p.size, p.size / 2); g.restore(); g.globalCompositeOperation = 'lighter'; }
    }
    g.globalAlpha = 1; g.globalCompositeOperation = 'source-over';
    parts = parts.filter(p => p.t < p.life && p.y < H + 40);
    if (parts.length || (menu && !reduce)) requestAnimationFrame(tick); else { running = false; g.clearRect(0, 0, W, H); }
  }
  kick();

  // ------------------------------------------------------------ select effects: wobble the icon, or spin it with a burst
  const sound = (n, v, r) => { try { if (typeof Sfx !== 'undefined' && Sfx.sample) Sfx.sample(n, v, r); } catch (e) {} };
  const WOBBLE = [{ scale: '1 1', rotate: '0deg' }, { scale: '1.2 0.82', rotate: '-5deg' }, { scale: '0.86 1.14', rotate: '4deg' }, { scale: '1.09 0.93', rotate: '-2deg' }, { scale: '0.97 1.03', rotate: '1deg' }, { scale: '1 1', rotate: '0deg' }];
  const SPIN = [{ rotate: '0deg', scale: '1' }, { rotate: '200deg', scale: '1.45' }, { rotate: '360deg', scale: '1' }];
  const FLIP = [{ rotate: 'y 0deg', scale: '1' }, { rotate: 'y 180deg', scale: '1.18' }, { rotate: 'y 360deg', scale: '1' }];
  function styleFor(el) {
    if (el.classList.contains('tile')) return 'spin';
    if (el.classList.contains('rt')) return 'flip';
    if (el.classList.contains('pc')) return 'wobble';
    if (el.matches('.big')) return 'wobble';
    if (el.closest('.slot')) return 'wobble';
    el._n = (el._n || 0) + 1; return el.closest('.pick, .row') ? 'flip' : 'wobble';
  }
  function accentOf(el) { const s = getComputedStyle(el); return (s.getPropertyValue('--c2') || s.getPropertyValue('--c') || '').trim() || null; }
  function fire(el, px, py) {
    if (!isMenuEl(el)) return;
    const r = el.getBoundingClientRect(), cx = px != null ? px : r.left + r.width / 2, cy = py != null ? py : r.top + r.height / 2;
    const mode = styleFor(el), acc = accentOf(el), big = el.classList.contains('tile') || el.matches('.big');
    if (!reduce) burst(cx, cy, acc, big);
    sound('itemGet', big ? 0.5 : 0.32, big ? 1.1 : 1.5 + Math.random() * 0.3);
    if (navigator.vibrate) try { navigator.vibrate(big ? 18 : 10); } catch (e) {}
    const mx = r.left + r.width / 2, my = r.top + r.height / 2;
    // the button may be redrawn by its click handler, so animate whatever stands there afterwards
    setTimeout(() => {
      const t = el.isConnected ? el : (document.elementFromPoint(mx, my) || {}).closest ? document.elementFromPoint(mx, my).closest(SEL) : null; if (!t || reduce) return;
      if (mode === 'spin') { const icon = t.querySelector(':scope > i') || t; icon.animate(SPIN, { duration: 640, easing: 'cubic-bezier(.2,.9,.3,1.25)' }); t.animate(WOBBLE, { duration: 520, easing: 'ease-out', composite: 'add' }); }
      else if (mode === 'flip') t.animate(FLIP, { duration: 480, easing: 'cubic-bezier(.2,.9,.3,1.2)' });
      else { const icon = t.querySelector(':scope > canvas') || t; icon.animate(WOBBLE, { duration: 560, easing: 'ease-out' }); }
    }, 0);
  }
  document.addEventListener('pointerdown', e => { const el = e.target.closest(SEL); if (el && inMenu()) fire(el, e.clientX, e.clientY); }, true);
  document.addEventListener('keydown', e => { if ((e.key === 'Enter' || e.key === ' ') && inMenu()) { const el = document.activeElement; if (el && el.matches && el.matches(SEL)) fire(el); } }, true);

  // ------------------------------------------------------------ the gold selector ring glides to the hovered / focused option
  let hot = null;
  function moveSel(el) {
    hot = el;
    if (!el || !inMenu() || !el.offsetParent) { sel.classList.remove('on'); return; }
    const r = el.getBoundingClientRect(), pad = el.classList.contains('tile') ? 7 : 5, rad = parseFloat(getComputedStyle(el).borderRadius) || 10;
    Object.assign(sel.style, { left: r.left - pad + 'px', top: r.top - pad + 'px', width: r.width + pad * 2 + 'px', height: r.height + pad * 2 + 'px', borderRadius: rad + pad + 'px' });
    sel.classList.add('on');
  }
  document.addEventListener('pointerover', e => {
    const el = e.target.closest(SEL); if (!el || !isMenuEl(el) || el === hot || !inMenu()) return;
    moveSel(el); sound('itemGet', 0.08, 2.6 + Math.random() * 0.4);
    if (!reduce && !el.classList.contains('tile')) el.animate([{ scale: '1' }, { scale: '1.07' }, { scale: '1' }], { duration: 260, easing: 'ease-out' });
  });
  document.addEventListener('pointerout', e => { if (hot && !hot.contains(e.relatedTarget)) { hot = null; sel.classList.remove('on'); } });
  document.addEventListener('focusin', e => { const el = e.target.closest && e.target.closest(SEL); if (el && isMenuEl(el)) moveSel(el); });
  setInterval(() => { if (hot) { if (!hot.isConnected || !inMenu()) moveSel(null); else moveSel(hot); } }, 250);

  // ------------------------------------------------------------ pointer: sparkle trail, shine that follows the cursor, 3D menu parallax
  window.MENU_PARALLAX = { x: 0, y: 0 }; const par = { x: 0, y: 0 };
  let lastTrail = 0;
  document.addEventListener('pointermove', e => {
    par.x = (e.clientX / innerWidth - 0.5) * 2; par.y = (e.clientY / innerHeight - 0.5) * 2;
    const b = e.target.closest && e.target.closest('button.big, .row button, .pick button');
    if (b) { const r = b.getBoundingClientRect(); b.style.setProperty('--sx', ((e.clientX - r.left) / r.width) * 100 + '%'); b.style.setProperty('--sy', ((e.clientY - r.top) / r.height) * 100 + '%'); }
    if (e.pointerType === 'mouse' && inMenu() && !reduce && performance.now() - lastTrail > 24) {
      lastTrail = performance.now();
      parts.push({ k: 'trail', x: e.clientX, y: e.clientY, vx: (Math.random() - 0.5) * 0.8, vy: Math.random() * 0.6, g: 0.02, d: 0.97, t: 0, life: 26, size: 1.4 + Math.random() * 1.8, c: pick(GOLD) }); kick();
    }
  });
  (function lean() { const P = window.MENU_PARALLAX; P.x += (par.x - P.x) * 0.06; P.y += (par.y - P.y) * 0.06; requestAnimationFrame(lean); })();

  // ------------------------------------------------------------ screen changes: a gold sweep, and confetti for the winner
  if (typeof showScreen === 'function') {
    const base = showScreen;
    showScreen = function (id) {
      const r = base.apply(this, arguments);
      if (id && id !== 'pause' && !reduce) { wipe.classList.remove('go'); void wipe.offsetWidth; wipe.classList.add('go'); }
      if (id === 'results') setTimeout(() => confetti(220), 250);
      moveSel(null);
      return r;
    };
  }
  window.LUX = { burst, confetti };
})();
