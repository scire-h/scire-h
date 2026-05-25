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

// Parse "<n> ストローク / <m> units" and require both > 0. A 0 unit total
// means the contour simplifier collapsed every polyline to a point, so
// nothing will actually be drawn even though strokes were detected.
const sm = /(\d+)\s*ストローク\s*\/\s*(\d+(?:\.\d+)?)\s*units/.exec(status || '');
const strokeCount = sm ? Number(sm[1]) : 0;
const unitCount   = sm ? Number(sm[2]) : 0;

// Let a few animation frames run so the beam has time to draw something.
await page.waitForTimeout(800);

const drawReport = await page.evaluate(() => {
    const c = document.getElementById('screen');
    const ctx = c.getContext('2d');
    const { width: w, height: h } = c;
    const data = ctx.getImageData(0, 0, w, h).data;
    let nonBlack = 0;
    let maxR = 0, maxG = 0, maxB = 0;
    let sumR = 0, sumG = 0, sumB = 0;
    // Tally only "lit" pixels (clearly above the dark vignette background)
    // so the average colour reflects the beam glow, not the empty screen.
    let litCount = 0, litR = 0, litG = 0, litB = 0;
    for (let i = 0; i < data.length; i += 4) {
        const r = data[i], g = data[i + 1], b = data[i + 2];
        const s = r + g + b;
        if (s > 10) { nonBlack++; sumR += r; sumG += g; sumB += b; }
        if (s > 90) { litCount++; litR += r; litG += g; litB += b; }
        if (r > maxR) maxR = r;
        if (g > maxG) maxG = g;
        if (b > maxB) maxB = b;
    }
    const avgR = litCount ? litR / litCount : 0;
    const avgG = litCount ? litG / litCount : 0;
    const avgB = litCount ? litB / litCount : 0;
    return { w, h, nonBlack, litCount, maxR, maxG, maxB, avgR, avgG, avgB };
});
console.log(`Canvas ${drawReport.w}x${drawReport.h}: nonBlackPx=${drawReport.nonBlack}  litPx=${drawReport.litCount}  peakRGB=(${drawReport.maxR},${drawReport.maxG},${drawReport.maxB})  avgLitRGB=(${drawReport.avgR.toFixed(1)},${drawReport.avgG.toFixed(1)},${drawReport.avgB.toFixed(1)})`);

// Sanity: should have drawn at least *some* pixels by now.
const minPixels = 100;
const drewSomething = drawReport.nonBlack >= minPixels;

// The hottest pixel along the beam core is intentionally clipped to
// (255,255,255) by the multi-pass bloom (outer halo and mid glow tint
// the surrounding pixels green, the inner core saturates to white).
// So check the *average* colour of lit pixels: across the soft halo
// the green channel should dominate the red.
const greenDominant = drawReport.litCount >= minPixels &&
                      drawReport.avgG > Math.max(20, drawReport.avgR * 1.2);

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
if (strokeCount < 1)      fail(`status parse: no strokes detected ("${status}")`);
if (unitCount   < 1)      fail(`status parse: zero path length ("${status}") -- contour pipeline collapsed`);
if (!drewSomething)       fail(`canvas drew only ${drawReport.nonBlack} non-black pixels (expected >= ${minPixels})`);
if (!greenDominant)       fail(`green not dominant in drawn pixels: ${JSON.stringify(drawReport)}`);
if (!hiResOk)             fail(`4K resolution switch did not resize canvas (got w/h after switch)`);

if (failed) {
    console.error('\nSmoke test FAILED');
    process.exit(1);
}
console.log(`\nSmoke test PASSED. Screenshot: ${shot}`);
