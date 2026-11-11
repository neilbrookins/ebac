// scrollbar.mjs -- how many columns xterm.js 5.5 and 6.0 give a phone, and
// (and the latest beta, ?v=b) what fills the strip at the right that the fit addon sets aside for a
// scrollbar. Serves page.html and node_modules from this folder; emulates a
// Pixel 9 (412x923 CSS pixels, scale 2.625, touch), with the browser's
// scrollbars as they are in headless Chrome and as overlays (as on Android).
import puppeteer from "puppeteer-core";
import http from "node:http";
import fs from "node:fs";
import path from "node:path";

const here = path.dirname(new URL(import.meta.url).pathname);
const server = http.createServer((req, res) => {
    const p = path.join(here, decodeURIComponent(new URL(req.url, "http://x").pathname));
    if (!p.startsWith(here) || !fs.existsSync(p) || fs.statSync(p).isDirectory()) { res.writeHead(404); return res.end(); }
    const type = p.endsWith(".html") ? "text/html" : p.endsWith(".css") ? "text/css" : "text/javascript";
    res.writeHead(200, { "Content-Type": type });
    fs.createReadStream(p).pipe(res);
}).listen(8098);

const shots = process.argv[2];   // folder for screenshots, or none
const phones = [["portrait", 412, 923], ["landscape", 923, 412]];
const cases = [
    ["xterm.js 5.5", "5", {}],
    ["xterm.js 6.0", "6", {}],
    ["6.0, scrollbar hidden", "6", { scrollbar: { showScrollbar: false } }],
    ["beta, as is", "b", {}],
    ["beta, scrollbar hidden", "b", { scrollbar: { showScrollbar: false } }],
];
const modes = [
    ["browser scrollbars as headless Chrome draws them", []],
    ["overlay scrollbars, as on Android", ["--enable-features=OverlayScrollbar"]],
];

for (const [modeName, flags] of modes) {
    const browser = await puppeteer.launch({
        executablePath: process.env.CHROME || "/usr/bin/google-chrome",
        args: ["--no-sandbox", ...flags],
    });
    console.log(`\n== ${modeName}`);
    for (const [orient, width, height] of phones) {
        for (const [name, v, opts] of cases) {
            const page = await browser.newPage();
            const errors = [];
            page.on("pageerror", e => errors.push(String(e)));
            await page.setViewport({ width, height, deviceScaleFactor: 2.625, isMobile: true, hasTouch: true, isLandscape: width > height });
            await page.goto(`http://localhost:8098/page.html?v=${v}&opts=${encodeURIComponent(JSON.stringify(opts))}`);
            try {
                await page.waitForFunction(() => window.ready, { timeout: 15000 });
            } catch {
                console.log(`${orient.padEnd(9)} ${name.padEnd(22)} did not start: ${errors.join("; ")}`);
                await page.close();
                continue;
            }
            const m = await page.evaluate(() => {
                const t = window.term;
                const dims = t.dimensions ?? t._core._renderService.dimensions;
                const cell = dims.css.cell.width;
                const c = document.getElementById("c").getBoundingClientRect();
                const textWidth = c.width - 10;                 // (the 5px padding each side)
                const probe = document.createElement("div");
                probe.style.cssText = "overflow:scroll;width:100px;height:100px;position:absolute;top:-200px";
                document.body.appendChild(probe);
                const nativeBar = probe.offsetWidth - probe.clientWidth;
                probe.remove();
                // what is drawn at the right: 6.0's own scrollbar, or 5.5's viewport's
                const bar6 = document.querySelector(".xterm .xterm-scrollable-element > .scrollbar.vertical");
                const vp = document.querySelector(".xterm-viewport");
                let drawn;
                if (bar6) {
                    const r = bar6.getBoundingClientRect();
                    const visible = getComputedStyle(bar6).display !== "none" && r.width > 0;
                    drawn = visible ? `its own scrollbar, ${Math.round(r.width)}px` : "no scrollbar";
                } else if (vp) {
                    const w = vp.offsetWidth - vp.clientWidth;
                    drawn = w ? `the browser's scrollbar, ${w}px` : "nothing (the browser's scrollbar takes no room)";
                }
                return {
                    cols: t.cols, cell, textWidth, nativeBar, drawn,
                    without: Math.floor(textWidth / cell),
                    blank: Math.round(textWidth - t.cols * cell),
                    reserve55: t._core?.viewport?.scrollBarWidth,
                    opt: JSON.stringify(t.options.scrollbar ?? null),
                };
            });
            console.log(`${orient.padEnd(9)} ${name.padEnd(22)} ${String(m.cols).padStart(3)} cols` +
                ` (${m.without} with nothing set aside); right of the text: ${m.blank}px,` +
                ` filled by ${m.drawn}` +
                (m.reserve55 !== undefined ? `; 5.5 sets aside ${m.reserve55}px` : "") +
                `; [cell ${m.cell.toFixed(2)}px, browser scrollbar ${m.nativeBar}px]`);
            if (shots) {
                fs.mkdirSync(shots, { recursive: true });
                const file = `${modeName.startsWith("overlay") ? "overlay" : "classic"}-${orient}-${name.replace(/[^a-z0-9]+/gi, "-")}.png`;
                await page.screenshot({ path: path.join(shots, file) });
            }
            await page.close();
        }
    }
    await browser.close();
}
server.close();
