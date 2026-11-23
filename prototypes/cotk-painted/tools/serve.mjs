// Shared helper: open a page from the prototype folder in headless Chromium,
// with the three.js CDN served from the local npm copy (the CDN is blocked in the workspace).
import { createRequire } from 'module';
const { chromium } = createRequire(import.meta.url)('playwright');
import fs from 'fs';
import path from 'path';
const NM = process.env.THREE_NM;
const ROOT = path.resolve(path.dirname(new URL(import.meta.url).pathname), '..');
const types = { '.js': 'text/javascript', '.mjs': 'text/javascript', '.html': 'text/html', '.png': 'image/png', '.jpg': 'image/jpeg', '.json': 'application/json', '.webp': 'image/webp' };
export async function open(rel, { width = 1024, height = 1024, mobile = false } = {}) {
  const browser = await chromium.launch({ args: ['--use-gl=angle', '--use-angle=swiftshader', '--enable-unsafe-swiftshader'] });
  const ctx = await browser.newContext({ viewport: { width, height }, deviceScaleFactor: 1, hasTouch: mobile, isMobile: mobile });
  const page = await ctx.newPage();
  const errors = [];
  page.on('pageerror', e => errors.push(String(e)));
  page.on('console', m => { if (m.type() === 'error') errors.push(m.text()); });
  await page.route('https://cdn.jsdelivr.net/npm/three@0.160.0/**', route => {
    const p = route.request().url().replace('https://cdn.jsdelivr.net/npm/three@0.160.0/', '');
    route.fulfill({ path: path.join(NM, 'three', p), contentType: 'text/javascript' });
  });
  await page.route('http://local/**', route => {
    const u = new URL(route.request().url());
    const f = path.join(ROOT, decodeURIComponent(u.pathname));
    if (!fs.existsSync(f)) return route.fulfill({ status: 404, body: 'nf' });
    route.fulfill({ path: f, contentType: types[path.extname(f)] || 'application/octet-stream' });
  });
  await page.goto('http://local/' + rel);
  return { browser, page, errors };
}
