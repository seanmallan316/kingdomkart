import { open } from './serve.mjs';
import fs from 'fs';
const { browser, page, errors } = await open('tools/bake.html');
await page.waitForFunction('window.ready === true', null, { timeout: 30000 });
const out = await page.evaluate(() => window.bake());
const dir = new URL('../assets/blockout/', import.meta.url).pathname;
const meta = {};
for (const [id, v] of Object.entries(out)) {
  fs.writeFileSync(dir + id + '.png', Buffer.from(v.png.split(',')[1], 'base64'));
  meta[id] = { matrix: v.matrix };
}
fs.writeFileSync(dir + 'projectors.json', JSON.stringify(meta, null, 1));
console.log('baked', Object.keys(out).join(' '), 'errors:', errors);
await browser.close();
