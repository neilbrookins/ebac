// mobile.mjs URL -- ttyd on a phone, as Home Assistant shows it: in an
// iframe filling a page, on an emulated Pixel 9 (412x923 CSS pixels,
// device scale 2.625, touch), then turned to landscape and back.
// At each step it reports the terminal's columns, what it would fit
// (as the fit addon works it out, without fitting), and what the shell
// was told (stty size).
import puppeteer from "puppeteer-core";
import http from "node:http";

const target = process.argv[2] || "http://localhost:7681/";
const host = http.createServer((req, res) => {
    res.writeHead(200, { "Content-Type": "text/html" });
    res.end(`<!doctype html><meta name=viewport content="width=device-width, initial-scale=1">
<style>html,body{margin:0;height:100%}iframe{border:0;width:100%;height:100%;display:block}</style>
<iframe src="${target}"></iframe>`);
}).listen(8099);

const portrait = { width: 412, height: 923, deviceScaleFactor: 2.625, isMobile: true, hasTouch: true };
const landscape = { ...portrait, width: 923, height: 412, isLandscape: true };
const browser = await puppeteer.launch({
    executablePath: process.env.CHROME || "/usr/bin/google-chrome",
    args: ["--no-sandbox", "--use-angle=swiftshader", "--enable-unsafe-swiftshader"],
});
const page = await browser.newPage();
const logs = [];
page.on("console", m => logs.push(m.text()));
await page.setViewport(portrait);
await page.goto("http://localhost:8099/", { waitUntil: "networkidle0" });
const frame = () => page.frames().find(f => f.url().startsWith(target));
const sleep = ms => new Promise(r => setTimeout(r, ms));
await frame().waitForFunction(() => {
    const t = window.term;
    if (!t) return false;
    for (let i = 0; i < t.rows; i++)
        if (t.buffer.active.getLine(i)?.translateToString().trim()) return true;
    return false;
}, { timeout: 20000 });
await sleep(2000);

let n = 0;
async function step(name) {
    const f = frame();
    const cols = await f.evaluate(() => window.term.cols);
    // what fit() would give, without changing anything
    const fits = await f.evaluate(() => {
        // (as FitAddon.proposeDimensions() works it out)
        const t = window.term, core = t._core;
        const cell = core._renderService.dimensions.css.cell.width;
        const parent = getComputedStyle(t.element.parentElement);
        const el = getComputedStyle(t.element);
        const pad = parseInt(el.paddingLeft) + parseInt(el.paddingRight);
        const bar = t.options.scrollback === 0 ? 0 : (core.viewport?.scrollBarWidth || 0);
        return cell ? Math.max(2, Math.floor((parseInt(parent.width) - pad - bar) / cell)) : null;
    });
    // the shell's idea of the size
    await f.focus(".xterm-helper-textarea");
    const mark = `M${++n}`;
    await page.keyboard.type(`echo ${mark} $(stty size)\n`);
    await sleep(1500);
    const lines = await f.evaluate(() => {
        const b = window.term.buffer.active, out = [];
        for (let i = 0; i < b.length; i++) out.push(b.getLine(i).translateToString(true));
        return out;
    });
    const at = lines.findLastIndex(l => l.startsWith(mark + " "));
    const stty = at >= 0 ? lines[at].slice(mark.length + 1).trim() : "?";
    const width = await f.evaluate(() => window.innerWidth);
    console.log(`${name.padEnd(28)} iframe ${width}px: terminal ${cols} cols` +
        (fits !== null && fits !== cols ? `, fits ${fits} (NOT FITTED)` : " (fitted)") +
        `; the shell is told ${stty} (rows cols)`);
}

await step("portrait, as loaded");
await page.setViewport(landscape); await sleep(1500);
await step("landscape");
await page.setViewport(portrait); await sleep(1500);
await step("portrait again");
console.log("renderer: " + (logs.filter(l => /renderer/i.test(l)).join("; ") || "no message"));
await browser.close();
host.close();
