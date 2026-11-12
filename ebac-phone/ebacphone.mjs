// ebacphone.mjs URL -- EBaC's web page (the Docker image's, as the Home
// Assistant app's) on an emulated Pixel 9 (412x923 CSS pixels, scale 2.625,
// touch; in an iframe, as Home Assistant shows apps), portrait then
// landscape: EBaC's columns, and how many would fit with nothing set aside
// for a scrollbar.
import puppeteer from "puppeteer-core";
import http from "node:http";

const target = process.argv[2] || "http://localhost:7681/";
const host = http.createServer((req, res) => {
    res.writeHead(200, { "Content-Type": "text/html" });
    res.end(`<!doctype html><meta name=viewport content="width=device-width, initial-scale=1">
<style>html,body{margin:0;height:100%}iframe{border:0;width:100%;height:100%;display:block}</style>
<iframe src="${target}"></iframe>`);
}).listen(8099);
const browser = await puppeteer.launch({
    executablePath: process.env.CHROME || "/usr/bin/google-chrome",
    args: ["--no-sandbox", "--use-angle=swiftshader", "--enable-unsafe-swiftshader"],
});
const sleep = ms => new Promise(r => setTimeout(r, ms));
const page = await browser.newPage();
const portrait = { width: 412, height: 923, deviceScaleFactor: 2.625, isMobile: true, hasTouch: true };
await page.setViewport(portrait);
await page.goto("http://localhost:8099/", { waitUntil: "networkidle0" });
const frame = () => page.frames().find(f => f.url().startsWith(target));
await frame().waitForFunction(() => window.term &&
    Array.from({ length: window.term.rows }, (_, i) =>
        window.term.buffer.active.getLine(i)?.translateToString() || "").join("\n").includes("Main Menu"),
    { timeout: 30000 });
async function report(name) {
    await sleep(2500);
    const m = await frame().evaluate(() => {
        const t = window.term, core = t._core;
        const cell = core._renderService.dimensions.css.cell.width;
        const parent = parseFloat(getComputedStyle(t.element.parentElement).width);
        const el = getComputedStyle(t.element);
        const pad = parseFloat(el.paddingLeft) + parseFloat(el.paddingRight);
        const vp = document.querySelector(".xterm-viewport");
        return {
            cols: t.cols, cell, parent, pad, scrollback: t.options.scrollback,
            setAside: core.viewport.scrollBarWidth,
            drawnBar: vp.offsetWidth - vp.clientWidth,
            without: Math.floor((parent - pad) / cell),
            blank: Math.round(parent - pad - t.cols * cell),
            webgl: !!document.querySelector(".xterm canvas"),
        };
    });
    console.log(`${name.padEnd(10)} EBaC has ${m.cols} columns; ${m.without} would fit with nothing set aside;` +
        ` ${m.blank}px blank at the right. [${m.setAside}px set aside for a scrollbar, ${m.drawnBar}px drawn;` +
        ` scrollback ${m.scrollback}; cell ${m.cell.toFixed(2)}px; container ${m.parent}px, padding ${m.pad}px;` +
        ` ${m.webgl ? "WebGL/canvas" : "DOM"} renderer]`);
}
await report("portrait");
await page.setViewport({ ...portrait, width: 923, height: 412, isLandscape: true });
await report("landscape");
await browser.close();
host.close();
