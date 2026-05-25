// Headless-browser smoke test for oscilloscope.html.
//
// Verifies that the page loads cleanly under Chromium, the text "hello"
// is processed into stroke contours, the animation canvas actually
// draws pixels of the configured beam colour, and that no JavaScript
// errors / unhandled rejections are emitted along the way.
//
// Run from the repo root:
//   node tests/web/smoke.mjs
// or pass an explicit URL:
//   node tests/web/smoke.mjs http://localhost:8000/oscilloscope.html

import { chromium } from 'playwright';
import { fileURLToPath } from 'node:url';
import { dirname, resolve } from 'node:path';
import { existsSync, mkdirSync } from 'node:fs';

const __filename = fileURLToPath(import.meta.url);
const __dirname  = dirname(__filename);
const repoRoot   = resolve(__dirname, '..', '..');
const htmlPath   = resolve(repoRoot, 'oscilloscope.html');
const target     = process.argv[2] || ('file://' + htmlPath);

if (!existsSync(htmlPath)) {
    console.error(`oscilloscope.html not found at ${htmlPath}`);
    process.exit(2);
}

const artifactsDir = resolve(repoRoot, 'tests', 'web', 'artifacts');
mkdirSync(artifactsDir, { recursive: true });

console.log(`Launching Chromium...`);
const browser = await chromium.launch();
const context = await browser.newContext({
    viewport: { width: 1280, height: 720 },
    deviceScaleFactor: 1,
});
const page = await context.newPage();

const jsErrors = [];
const consoleErrors = [];
page.on('pageerror',  e   => jsErrors.push(String(e)));
page.on('console',    msg => { if (msg.type() === 'error') consoleErrors.push(msg.text()); });

console.log(`Loading ${target}`);
await page.goto(target, { waitUntil: 'load' });

// Wait for the source-build pipeline to produce a stroke count in the status bar.
console.log('Waiting for source pipeline...');
await page.waitForFunction(() => {
    const s = document.getElementById('status')?.textContent || '';
    return /ストローク/.test(s);
}, { timeout: 15000 });

const status = await page.locator('#status').textContent();
console.log(`Status: ${status}`);

// Let a few animation frames run so the beam has time to draw something.
await page.waitForTimeout(800);

const drawReport = await page.evaluate(() => {
    const c = document.getElementById('screen');
    const ctx = c.getContext('2d');
    const { width: w, height: h } = c;
    const data = ctx.getImageData(0, 0, w, h).data;
    let nonBlack = 0;
    let maxR = 0, maxG = 0, maxB = 0;
    for (let i = 0; i < data.length; i += 4) {
        const r = data[i], g = data[i + 1], b = data[i + 2];
        if (r + g + b > 10) nonBlack++;
        if (r > maxR) maxR = r;
        if (g > maxG) maxG = g;
        if (b > maxB) maxB = b;
    }
    return { w, h, nonBlack, maxR, maxG, maxB };
});
console.log(`Canvas ${drawReport.w}x${drawReport.h}: nonBlackPx=${drawReport.nonBlack}  peakRGB=(${drawReport.maxR},${drawReport.maxG},${drawReport.maxB})`);

// Sanity: should have drawn at least *some* pixels by now.
const minPixels = 100;
const drewSomething = drawReport.nonBlack >= minPixels;

// Sanity: the beam is green/cyan by default, so the green channel
// must dominate the red channel on at least one pixel.
const greenDominant = drawReport.maxG > Math.max(40, drawReport.maxR * 1.3);

// Switch source to image tab to ensure the tab UI is interactive without errors,
// then switch back. Catches event-binding regressions.
await page.click('#tabImage');
await page.waitForTimeout(100);
await page.click('#tabText');
await page.waitForTimeout(100);

// Re-render at 4K briefly to validate the high-res path doesn't throw.
await page.selectOption('#resolution', '3840x2160');
await page.waitForTimeout(300);
const hiResOk = await page.evaluate(() => {
    const c = document.getElementById('screen');
    return c.width === 3840 && c.height === 2160;
});

const shot = resolve(artifactsDir, 'preview.png');
await page.screenshot({ path: shot, fullPage: false });

await browser.close();

let failed = false;
function fail(msg) { console.error(`FAIL: ${msg}`); failed = true; }

if (jsErrors.length)      fail(`pageerror events: ${JSON.stringify(jsErrors)}`);
if (consoleErrors.length) fail(`console.error events: ${JSON.stringify(consoleErrors)}`);
if (!drewSomething)       fail(`canvas drew only ${drawReport.nonBlack} non-black pixels (expected >= ${minPixels})`);
if (!greenDominant)       fail(`green not dominant in drawn pixels: ${JSON.stringify(drawReport)}`);
if (!hiResOk)             fail(`4K resolution switch did not resize canvas (got w/h after switch)`);

if (failed) {
    console.error('\nSmoke test FAILED');
    process.exit(1);
}
console.log(`\nSmoke test PASSED. Screenshot: ${shot}`);
