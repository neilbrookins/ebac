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
        // EBaC's own idea of the size: on the Information screen's
        // diagnostic page (pressing on, page by page, until it comes)
        await keys(page, "i");
        await check(page, "I: the Information screen", "Electronic Bible and Concordance");
        let m = null;
        for (let i = 0; i < 6 && !m; i++) {
            m = (await screen(page)).match(/Window: +(\d+) rows, (\d+) columns/);
            if (!m) { await keys(page, " "); await sleep(500); }
        }
        const same = m && +m[1] === size[0] && +m[2] === size[1];
        console.log(`${same ? "ok  " : "FAIL"} EBaC is told the same size` +
                    (m ? ` (${m[1]} rows, ${m[2]} columns)` : ": no Window line found"));
        if (!same) failed++;
        for (let i = 0; i < 6 && !(await screen(page)).includes("Main Menu"); i++) {
            await keys(page, " "); await sleep(500);
        }
        await check(page, "back to the main menu", "Main Menu");
        await keys(page, "v");
        await keys(page, "john\r");
        await keys(page, "3\r");
        await keys(page, "16\r");
        await check(page, "V: John 3:16", "For God so loved");
        // the window made shorter and taller again, as a phone's keyboard
        // coming and going does: EBaC draws the passage again each time,
        // with no key pressed (its heading on the top line; the screen full)
        for (const [what, height] of [["shorter", Math.round(view.height * 0.55)], ["taller again", view.height]]) {
            await page.setViewport({ ...view, height });
            await sleep(1500);
            const lines = (await screen(page)).split("\n");
            const rows = await page.evaluate(() => window.term.rows);
            const used = lines.filter(l => l.trim()).length;
            const ok = lines[0].includes("John") && used >= rows - 8;
            console.log(`${ok ? "ok  " : "FAIL"} the window ${what} (${rows} rows): drawn again at once` +
                        ` (top line "${lines[0].trim().slice(0, 20)}", ${used} lines used)`);
            if (!ok) failed++;
        }
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
        await page.setViewport(view);
        await sleep(1000);
        await keys(page, "\x0c");
        await check(page, "back to the first size", "Main Menu");

        // the touch gestures (gestures.js), on the phone
        if (view.hasTouch) {
            const mode = () => page.evaluate(() => window.ebacGestureMode());
            const menu = await mode();
            console.log(`${menu === "menu" ? "ok  " : "FAIL"} gestures: EBaC says it shows the main menu (${menu})`);
            if (menu !== "menu") failed++;
            await keys(page, "v");
            await keys(page, "john\r");
            await keys(page, "3\r");
            await keys(page, "16\r");
            await check(page, "V: John 3:16 again", "For God so loved");
            const top = async () => (await screen(page)).split("\n").find(l => /^\d+:\d+ /.test(l)) || "";
            const before = await top();
            // a short swipe up: the next verse
            const x = view.width / 2, y = view.height / 2;
            await page.touchscreen.touchStart(x, y);
            for (let i = 1; i <= 5; i++) await page.touchscreen.touchMove(x, y - i * 20);
            await page.touchscreen.touchEnd();
            await sleep(1000);
            const after = await top();
            const moved = before.startsWith("3:16") && after.startsWith("3:17");
            console.log(`${moved ? "ok  " : "FAIL"} gestures: a swipe up goes on a verse` +
                        ` ("${before.slice(0, 4)}" then "${after.slice(0, 4)}")`);
            if (!moved) failed++;
            await keys(page, "x");
            await sleep(500);
            await keys(page, "x");
            await check(page, "back to the main menu", "Main Menu");
        }

        // E: EBaC ends; the page says so, and Enter starts it again
        await keys(page, "e");
        await sleep(1000);
        const shown = await page.evaluate(() => {
            const e = document.getElementById("ended");
            return getComputedStyle(e).display !== "none" ? e.textContent : "";
        });
        const said = shown.includes("EBaC has ended");
        console.log(`${said ? "ok  " : "FAIL"} E: the page says EBaC has ended`);
        if (!said) failed++;
        const tapOnly = shown.includes("Tap to start it again") && !shown.includes("\u23CE");
        const right = view.hasTouch ? tapOnly : shown.includes("\u23CE");
        console.log(`${right ? "ok  " : "FAIL"} and how to start it again, for ${view.hasTouch ? "a touch screen" : "a keyboard"}: "${shown.replace("EBaC has ended.", "")}"`);
        if (!right) failed++;
        if (said) {
            await Promise.all([page.waitForNavigation({ waitUntil: "load" }), page.keyboard.press("Enter")]);
            await check(page, "Enter: EBaC starts again", "Main Menu");
            // the search history, kept from before (S, then Up: the last search)
            await keys(page, "s");
            await sleep(500);
            await page.keyboard.press("ArrowUp");
            await check(page, "S, Up: the search before EBaC ended is still there", "jesus wept");
            await page.keyboard.press("Enter");
            await check(page, "and finds it again", "11:35");
            await keys(page, "x");
            await check(page, "back to the main menu", "Main Menu");
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
