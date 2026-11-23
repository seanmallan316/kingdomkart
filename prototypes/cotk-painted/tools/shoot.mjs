// Screenshot the game at a phone size and a PC size; walk the hero behind the tower; print console errors.
import { open } from './serve.mjs';
const out = process.argv[2];
for (const [name, w, h, mob] of [['phone', 412, 860, true], ['pc', 1280, 720, false]]) {
  const { browser, page, errors } = await open('index.html?autostart&seed=3', { width: w, height: h, mobile: mob });
  await page.waitForFunction('window.__game && window.__game.frames > 30', null, { timeout: 60000 });
  await page.screenshot({ path: `${out}/${name}_start.png` });
  // put the hero behind the tower to check the see-through outline
  await page.evaluate(() => { const g = window.__game; g.wraiths.forEach(w => w.obj.position.set(30, 0, 30)); g.hero.position.set(4.6, 0, -9.2); });
  await page.waitForTimeout(2500);
  await page.screenshot({ path: `${out}/${name}_behind.png` });
  // collect all sparks and light the beacon
  await page.evaluate(() => { const g = window.__game; for (const s of g.sparks) { g.hero.position.set(s.x, 0, s.z); } });
  for (const s of await page.evaluate(() => window.__game.sparks.map(s => [s.x, s.z]))) {
    await page.evaluate(([x, z]) => window.__game.hero.position.set(x, 0, z), s); await page.waitForTimeout(300);
  }
  await page.evaluate(() => window.__game.hero.position.set(1.6, 0, 1.6));
  await page.waitForTimeout(2200);
  await page.screenshot({ path: `${out}/${name}_reveal.png` });
  await page.waitForTimeout(1500);
  await page.evaluate(() => { window.__game.hero.position.set(1.3, 0, 4.5); document.getElementById('title').classList.add('hide'); });
  await page.waitForTimeout(1200);
  await page.evaluate(() => document.getElementById('title').classList.add('hide'));
  await page.screenshot({ path: `${out}/${name}_color.png` });
  await page.waitForTimeout(1500);
  const st = await page.evaluate(() => ({ state: window.__game.state, carried: window.__game.carried, reveal: window.__game.reveal, frames: window.__game.frames }));
  await page.screenshot({ path: `${out}/${name}_won.png` });
  console.log(name, JSON.stringify(st), 'errors:', errors);
  await browser.close();
}
