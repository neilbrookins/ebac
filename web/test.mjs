// test.mjs SITE -- EBaC in a web browser, in headless Chrome: serves the
// built site (web/build/site), opens it at a desktop's size and a phone's,
// and checks that the main menu comes, a verse can be read (V, John 3:16),
// a search finds (S, jesus wept), and a resize changes EBaC's width.
// Prints each screen it checks. Exits 1 if any check fails.
//
//   node web/test.mjs web/build/site     (puppeteer-core; CHROME: the browser)
import puppeteer from "puppeteer-core";
import http from "node:http";
import fs from "node:fs";
import path from "node:path";

const site = path.resolve(process.argv[2] || "web/build/site");
const types = { ".html": "text/html", ".js": "text/javascript", ".mjs": "text/javascript",
                ".css": "text/css", ".wasm": "application/wasm", ".data": "application/octet-stream" };
const server = http.createServer((req, res) => {
    const p = path.join(site, decodeURIComponent(new URL(req.url, "http://x").pathname));
    const file = p.endsWith("/") ? path.join(p, "index.html") : p;
    if (!file.startsWith(site) || !fs.existsSync(file)) { res.writeHead(404); return res.end(); }
    res.writeHead(200, { "Content-Type": types[path.extname(file)] || "application/octet-stream" });
    fs.createReadStream(file).pipe(res);
}).listen(8097);

const browser = await puppeteer.launch({
    executablePath: process.env.CHROME || "/usr/bin/google-chrome",
    args: ["--no-sandbox"],
});
const sleep = ms => new Promise(r => setTimeout(r, ms));
let failed = 0;

const screen = page => page.evaluate(() => {
    const t = window.term, b = t.buffer.active, out = [];
    for (let i = 0; i < t.rows; i++) out.push(b.getLine(b.viewportY + i).translateToString(true));
    return out.join("\n");
});
async function waitFor(page, text, ms = 20000) {
    const end = Date.now() + ms;
    while (Date.now() < end) {
        if ((await screen(page)).includes(text)) return true;
        await sleep(200);
    }
    return false;
}
async function check(page, name, text) {
    const ok = await waitFor(page, text);
    console.log(`${ok ? "ok  " : "FAIL"} ${name}`);
    if (!ok) {
        failed++;
        console.log((await screen(page)).split("\n").map(l => "     |" + l).join("\n"));
    }
    return ok;
}
async function keys(page, s) {
    for (const ch of s) {
        if (ch === "\r")
            await page.keyboard.press("Enter");
        else if (ch === "\x0c") {             // Ctrl-L
            await page.keyboard.down("Control");
            await page.keyboard.press("l");
            await page.keyboard.up("Control");
        } else
            await page.keyboard.sendCharacter(ch);
        await sleep(60);
    }
}

for (const [name, view] of [
    ["desktop 1200x900", { width: 1200, height: 900 }],
    ["phone 412x923", { width: 412, height: 923, deviceScaleFactor: 2.625, isMobile: true, hasTouch: true }],
]) {
    console.log(`\n== ${name}`);
    const page = await browser.newPage();
    const errors = [];
    page.on("pageerror", e => errors.push(String(e)));
    page.on("console", m => { if (m.type() === "error") errors.push(m.text()); });
    page.on("requestfailed", r => errors.push("failed: " + r.url()));
    page.on("response", r => { if (r.status() >= 400) errors.push(`${r.status()}: ${r.url()}`); });
    await page.setViewport(view);
    await page.goto("http://localhost:8097/", { waitUntil: "load" });
    if (await check(page, "the main menu", "Main Menu")) {
        const size = await page.evaluate(() => [window.term.rows, window.term.cols]);
        console.log(`     the terminal has ${size[0]} rows, ${size[1]} columns`);
        // EBaC's own idea of the size, as its Information screen says
        await keys(page, "i");
        if (await check(page, "I: the Information screen", "Window:")) {
            const m = (await screen(page)).match(/Window: +(\d+) rows, (\d+) columns/);
            const ok = m && +m[1] === size[0] && +m[2] === size[1];
            console.log(`${ok ? "ok  " : "FAIL"} EBaC is told the same size` + (m ? ` (${m[1]} rows, ${m[2]} columns)` : ""));
            if (!ok) failed++;
        }
        await keys(page, " ");
        await check(page, "back to the main menu", "Main Menu");
        await keys(page, "v");
        await keys(page, "john\r");
        await keys(page, "3\r");
        await keys(page, "16\r");
        await check(page, "V: John 3:16", "For God so loved");
        await keys(page, "x");
        await sleep(500);
        await keys(page, "x");
        await check(page, "back to the main menu", "Main Menu");
        await keys(page, "s");
        await keys(page, "jesus wept\r");
        await check(page, "S: jesus wept", "11:35");
        await keys(page, "x");
        await check(page, "back to the main menu again", "Main Menu");
        // a resize: EBaC lays out the next screen at the new width
        await page.setViewport({ ...view, width: Math.round(view.width * 0.7) });
        await sleep(1000);
        const narrower = await page.evaluate(() => window.term.cols);
        await keys(page, "\x0c");       // Ctrl-L: redraw
        await check(page, `resized to ${narrower} columns, redrawn`, "Main Menu");
        const wide = Math.max(...(await screen(page)).split("\n").map(l => l.length));
        if (wide > narrower) {
            console.log(`FAIL a line of ${wide} characters in ${narrower} columns`);
            failed++;
        }
    }
    if (errors.length) {
        console.log("     page errors: " + errors.join(" | "));
    }
    await page.close();
}
await browser.close();
server.close();
console.log(failed ? `\n${failed} checks failed` : "\nall checks passed");
process.exit(failed ? 1 : 0);
