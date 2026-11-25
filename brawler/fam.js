'use strict';
// Super Family Smash: the family banner along the bottom of the opening screen. Nine leaning photo cards in their own
// colours: they rise in one by one, a spotlight ripples across them, they lean toward the pointer, the one you point at
// grows and shows its name, and choosing one makes that fighter cheer (and the 3D family member on stage celebrate).
(() => {
  const P = window.CLASH_FAM; if (!P) return;
  const title = document.getElementById('title'); if (!title) return;
  const FAM = [['tyler', 'TYLER', '#5f86a6'], ['violet', 'VIOLET', '#a8a4d8'], ['layne', 'LAYNE', '#ff7216'], ['dad', 'DAD', '#c0262f'], ['mom', 'MOM', '#e4aa30'],
    ['jayson', 'JAYSON', '#00af62'], ['norah', 'NORAH', '#f7a8c8'], ['ollie', 'OLLIE', '#6fb8e3'], ['ray', 'RAY', '#fbe11a']];
  const strip = document.createElement('div'); strip.className = 'famstrip'; strip.setAttribute('aria-label', 'The family');
  strip.innerHTML = FAM.map(([id, name, c], i) => `<button class="fam" data-id="${id}" style="--fc:${c};--i:${i}" aria-label="${name}"><span class="ph"><img src="${P[id]}" alt="" draggable="false"></span><span class="glow"></span><span class="nm">${name}</span></button>`).join('');
  title.appendChild(strip);
  const cards = [...strip.querySelectorAll('.fam')];

  // a spotlight ripples across the cards while nobody is pointing at them
  let lit = 0, hovering = false;
  setInterval(() => {
    if (hovering || !title.classList.contains('on') || document.hidden) return;
    cards.forEach(c => c.classList.remove('lit')); cards[lit % cards.length].classList.add('lit'); lit++;
  }, 1300);
  // lean toward the pointer, with the photo sliding inside its frame for depth
  strip.addEventListener('pointermove', e => {
    hovering = true; cards.forEach(c => c.classList.remove('lit'));
    for (const c of cards) {
      const r = c.getBoundingClientRect(), dx = (e.clientX - (r.left + r.width / 2)) / innerWidth, dy = (e.clientY - (r.top + r.height / 2)) / innerHeight;
      c.style.setProperty('--ry', (dx * 26).toFixed(2) + 'deg'); c.style.setProperty('--rx', (-dy * 10).toFixed(2) + 'deg'); c.style.setProperty('--px', (-dx * 18).toFixed(1) + 'px');
    }
  });
  strip.addEventListener('pointerleave', () => { hovering = false; for (const c of cards) { c.style.setProperty('--ry', '0deg'); c.style.setProperty('--rx', '0deg'); c.style.setProperty('--px', '0px'); } });
  // choose a card: that fighter cheers and their 3D self on stage celebrates
  strip.addEventListener('click', e => {
    const c = e.target.closest('.fam'); if (!c) return; e.stopPropagation();
    try { Sfx.init(); } catch (er) {}
    const id = c.dataset.id, d = typeof FIGHTERS !== 'undefined' && FIGHTERS.find(f => f.id === id);
    if (d && Sfx.voice) Sfx.voice({ def: d, port: 9 }, Math.random() < 0.5 ? 'win' : 'atk2', true);
    if (typeof R3 !== 'undefined' && R3.menuReact) R3.menuReact(id, Math.random() < 0.6 ? 'victory' : 'highkick');
    c.classList.remove('pop'); void c.offsetWidth; c.classList.add('pop');
  });
  strip.addEventListener('pointerdown', e => { if (e.target.closest('.fam')) e.stopPropagation(); });
})();
