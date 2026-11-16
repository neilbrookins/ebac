// firstsearch.mjs SITE -- an experiment: EBaC's first search after the
// page opens, on an emulated phone, with the window made shorter (as a
// phone's keyboard does) at various moments. Prints the screen's top line
// and how much of the screen is used, for each.
import puppeteer from "puppeteer-core";
import http from "node:http";
import fs from "node:fs";
import path from "node:path";

const site = path.resolve(process.argv[2] || "web/build/site");
const types = { ".html": "text/html", ".js": "text/javascript", ".mjs": "text/javascript",
                ".css": "text/css", ".wasm": "application/wasm" };
http.createServer((req, res) => {
    const p = path.join(site, decodeURIComponent(new URL(req.url, "http://x").pathname));
    const file = p.endsWith("/") ? path.join(p, "index.html") : p;
    if (!fs.existsSync(file)) { res.writeHead(404); return res.end(); }
    res.writeHead(200, { "Content-Type": types[path.extname(file)] || "application/octet-stream" });
    fs.createReadStream(file).pipe(res);
}).listen(8096);
const browser = await puppeteer.launch({ executablePath: process.env.CHROME || "/usr/bin/google-chrome", args: ["--no-sandbox"] });
const sleep = ms => new Promise(r => setTimeout(r, ms));
const view = { width: 412, height: 923, deviceScaleFactor: 2.625, isMobile: true, hasTouch: true };
const screen = page => page.evaluate(() => {
    const t = window.term, b = t.buffer.active, out = [];
    for (let i = 0; i < t.rows; i++) out.push(b.getLine(b.viewportY + i).translateToString(true));
    return out;
});
for (const when of ["never", "before S", "after S", "after typing", "at Enter", "300ms after Enter", "1s after Enter"]) {
    const ctx = await browser.createBrowserContext();
    const page = await ctx.newPage();
    await page.setViewport(view);
    await page.goto("http://localhost:8096/");
    for (let i = 0; i < 100 && !(await screen(page).catch(() => [])).join("\n").includes("Main Menu"); i++) await sleep(200);
    const shrink = () => page.setViewport({ ...view, height: Math.round(view.height * 0.55) });
    if (when === "before S") await shrink();
    await page.keyboard.sendCharacter("s");
    if (when === "after S") { await sleep(100); await shrink(); }
    for (const c of "food") { await page.keyboard.sendCharacter(c); await sleep(50); }
    if (when === "after typing") await shrink();
    const enter = page.keyboard.press("Enter");
    if (when === "at Enter") await shrink();
    await enter;
    if (when === "300ms after Enter") { await sleep(300); await shrink(); }
    if (when === "1s after Enter") { await sleep(1000); await shrink(); }
    await sleep(2500);
    const lines = await screen(page);
    const rows = lines.length, last = lines.map(l => !!l.trim()).lastIndexOf(true);
    console.log(`${when.padEnd(18)} rows ${rows}; top "${lines[0].trim().slice(0, 24)}"; last line used ${last + 1}`);
    await ctx.close();
}
await browser.close();
process.exit(0);
