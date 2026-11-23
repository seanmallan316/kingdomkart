// Plays both stages at a phone size and a PC size, takes screenshots, prints state and console errors.
import { open } from './serve.mjs';
const out = process.argv[2];
for (const [name, w, h, mob] of [['phone', 412, 860, true], ['pc', 1280, 720, false]]) {
  const { browser, page, errors } = await open('index.html?autostart&seed=3', { width: w, height: h, mobile: mob });
  await page.waitForFunction('window.__game && window.__game.frames > 30', null, { timeout: 90000 });
  const shot = n => page.screenshot({ path: `${out}/${name}_${n}.png` });
  const tp = (x, z) => page.evaluate(([x, z]) => window.__game.hero.position.set(x, 0, z), [x, z]);
  const park = () => page.evaluate(() => window.__game.wraiths.forEach(w => { w.obj.position.set(40, 0, -40); w.home.set(40, -40); }));
  const collect = async () => { for (const s of await page.evaluate(() => window.__game.sparks.map(s => [s.x, s.z]))) { await tp(...s); await page.waitForTimeout(350); } };
  await shot('start');
  await park();
  const gateBlocked = await page.evaluate(() => window.__game.blocked(0, 17, .3));
  await tp(4.6, -9.2); await page.waitForTimeout(2000); await shot('behind');
  await collect(); await tp(1.6, 1.6); await page.waitForTimeout(2200); await shot('reveal');
  await page.waitForTimeout(3000);                                   // stage 2 begins
  const s1 = await page.evaluate(() => ({ stage: window.__game.stage, gateOpen: !window.__game.blocked(0, 17, .3), burn: window.__game.blocked(5, 20, .3), bridge: !window.__game.blocked(0, 20, .3), loch: window.__game.blocked(5, 43, .3), jetty: !window.__game.blocked(0, 43, .3) }));
  await park();
  await tp(0.5, 24); await page.waitForTimeout(2500); await shot('loch_veiled');
  await tp(0, 37.5); await page.waitForTimeout(2500); await shot('shore_veiled');
  await collect(); await tp(1.4, 35); await page.waitForTimeout(5000); await shot('shore_color');
  await page.evaluate(() => document.getElementById('title').classList.add('hide'));
  await tp(0.5, 24.5); await page.waitForTimeout(2600);
  await page.evaluate(() => document.getElementById('title').classList.add('hide'));
  await shot('loch_color');
  const st = await page.evaluate(() => ({ state: window.__game.state, stage: window.__game.stage, carried: window.__game.carried, tavin: !!window.__game.tavin }));
  console.log(name, 'gateBlockedAtStart', gateBlocked, JSON.stringify(s1), JSON.stringify(st), 'errors:', errors);
  await browser.close();
}
