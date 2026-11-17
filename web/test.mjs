// test.mjs SITE -- EBaC in a web browser, in headless Chrome: serves the
// built site (web/build/site), opens it at a desktop's size and a phone's,
// and checks that the main menu comes, a verse can be read (V, John 3:16),
// a search finds (S, jesus wept), and a resize changes EBaC's width; and
// that A adds a translation (from GitHub) that is kept, and removes it,
// lists scrollmapper/bible_databases's languages, and adds a text from
// Project Gutenberg through EBaC's relay.
// Prints each screen it checks. Exits 1 if any check fails.
//
//   node web/test.mjs web/build/site     (puppeteer-core; CHROME: the browser)
import puppeteer from "puppeteer-core";
import http from "node:http";
import fs from "node:fs";
import path from "node:path";

const site = path.resolve(process.argv[2] || "web/build/site");
const types = { ".html": "text/html", ".js": "text/javascript", ".mjs": "text/javascript",
                ".css": "text/css", ".wasm": "application/wasm", ".data": "application/octet-stream",
                ".png": "image/png", ".webmanifest": "application/manifest+json" };
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
async function check(page, name, text, ms) {
    const ok = await waitFor(page, text, ms);
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

        // F and f: the font a pixel larger, then smaller again; the
        // terminal refitted (fewer columns), and the size kept
        {
            const font = () => page.evaluate(() => [window.term.options.fontSize, window.term.cols,
                                                    localStorage.getItem("ebac-font-size")]);
            await check(page, "the main menu offers F and f", "F/f) Font: LARGER / smaller");
            const [size0, cols0] = await font();
            await keys(page, "F");
            await sleep(1000);
            const [size1, cols1, kept1] = await font();
            const ok1 = size1 === size0 + 1 && cols1 < cols0 && kept1 === String(size1);
            console.log(`${ok1 ? "ok  " : "FAIL"} F: font ${size0} to ${size1}, ${cols0} to ${cols1} columns, kept ${kept1}`);
            if (!ok1) failed++;
            await check(page, "redrawn at the new size", "Main Menu");
            await keys(page, "f");
            await sleep(1000);
            const [size2, cols2, kept2] = await font();
            const ok2 = size2 === size0 && cols2 === cols0 && kept2 === String(size0);
            console.log(`${ok2 ? "ok  " : "FAIL"} f: font back to ${size2}, ${cols2} columns, kept ${kept2}`);
            if (!ok2) failed++;

            // the wheel, on the main menu: a step a notch, up larger (with
            // a mouse: not the phone's, which has none)
            if (!view.hasTouch) {
                await page.mouse.move(view.width / 2, view.height / 2);
                await page.mouse.wheel({ deltaY: -40 });       // part of a notch: none
                await sleep(500);
                const [w0] = await font();
                await page.mouse.wheel({ deltaY: -60 });       // the rest: a step
                await sleep(1000);
                const [w1] = await font();
                await page.mouse.wheel({ deltaY: 500 });       // a fast turn: a step only
                await sleep(1000);
                const [w2] = await font();
                const ok4 = w0 === size0 && w1 === size0 + 1 && w2 === size0;
                console.log(`${ok4 ? "ok  " : "FAIL"} the wheel: font ${w0}, ${w1}, ${w2}`);
                if (!ok4) failed++;
                await check(page, "the main menu, after the wheel", "Main Menu");
            }

            // a pinch (two fingers apart): larger, a step or a few, not more
            if (view.hasTouch) {
                const cdp = await page.createCDPSession();
                const touch = (type, pts) => cdp.send("Input.dispatchTouchEvent",
                    { type, touchPoints: pts.map(([x, y], id) => ({ x, y, id })) });
                const pinch = async (a, b) => {
                    const cx = view.width / 2, cy = view.height / 2;
                    await touch("touchStart", [[cx - a, cy], [cx + a, cy]]);
                    for (let i = 1; i <= 5; i++) {
                        const d = a + (b - a) * i / 5;
                        await touch("touchMove", [[cx - d, cy], [cx + d, cy]]);
                        await sleep(30);
                    }
                    await touch("touchEnd", []);
                    await sleep(1000);
                };
                await pinch(50, 75);            // 1.5 times apart: a step
                const [p1] = await font();
                await pinch(30, 180);           // 6 times: still only a few
                const [p2] = await font();
                await pinch(180, 30);           // and together: back
                const [p3] = await font();
                await pinch(75, 50);
                const [p4] = await font();
                const ok3 = p1 === size0 + 1 && p2 > p1 && p2 <= p1 + 4 && p3 === p1 && p4 === size0;
                console.log(`${ok3 ? "ok  " : "FAIL"} pinches: font ${size0}, ${p1}, ${p2}, ${p3}, ${p4}`);
                if (!ok3) failed++;
                await keys(page, "\x0c");
                await check(page, "the main menu, after the pinches", "Main Menu");
            }
        }

        // the touch gestures (gestures.js), on the phone
        if (view.hasTouch) {
            // S, then the keyboard (the window shorter, 28 rows or so): the
            // search screen's help fits, its first line still on the screen
            await keys(page, "s");
            await check(page, "S: the search screen", "Enter Search word(s) here:");
            await page.setViewport({ ...view, height: 450 });
            await sleep(1500);
            const sl = (await screen(page)).split("\n");
            const fits = sl.some(l => l.startsWith("Search line editing keys:")) &&
                         sl.some(l => l.startsWith("Enter Search word(s) here:")) &&
                         sl.every(l => !l.startsWith("TRL") && !l.startsWith("e or CTRL"));
            const rowsNow = await page.evaluate(() => window.term.rows);
            console.log(`${fits ? "ok  " : "FAIL"} the search screen fits ${rowsNow} rows, its first line kept`);
            if (!fits) failed++;
            await page.keyboard.press("Enter");     // (a blank search: back)
            await page.setViewport(view);
            await sleep(1000);
            await check(page, "back to the main menu", "Main Menu");
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

            // A, T: a translation added (downloaded, from thiagobodruk/bible
            // on GitHub). Each step's line shows as it starts, not all at
            // the end (the page draws while EBaC works); a key or a tap
            // while it works doesn't pass over "Press any key"; then EBaC
            // shows it at once (V, not the translation shown before).
            const view316 = async (name, text) => {
                await keys(page, "v");
                await keys(page, "johannes\r");
                await keys(page, "3\r");
                await keys(page, "16\r");
                await check(page, name, text);
                await keys(page, "x");
                await sleep(500);
                await keys(page, "x");
                await check(page, "back to the main menu", "Main Menu");
            };
            const addAfrikaans = async () => {
                await keys(page, "a");
                await check(page, "A: where from?", "Where from?");
                await keys(page, "t");
                await check(page, "T: the languages (from GitHub)", "Choose a language", 60000);
                await keys(page, "2\r");
                await check(page, "2: Afrikaans", "AFR53-TB");
                await keys(page, "1\r");
                await check(page, "1: its license", "Do you agree to these terms (y/n)?");
                await keys(page, "y\r");
                // (what the screen shows meanwhile, every tenth of a second)
                let seen = "", end = Date.now() + 180000, tapped = false;
                while (Date.now() < end) {
                    const now = await screen(page);
                    if (now.includes("Press any key to go back to EBaC.")) break;
                    if (/writing \d+ books/.test(now) && !now.includes("is ready") && !seen)
                        seen = "writing";
                    if (/building the concordance index/.test(now) && !now.includes("is ready"))
                        seen = "building";
                    // a key, and on the phone a tap, while it works
                    if (seen && !tapped) {
                        tapped = true;
                        await page.keyboard.press("Enter");
                        if (view.hasTouch) await page.touchscreen.tap(view.width / 2, view.height / 2);
                    }
                    await sleep(100);
                }
                const shown = seen === "building";
                console.log(`${shown ? "ok  " : "FAIL"} each step shown as it starts (last seen before the end: ${seen || "none"})`);
                if (!shown) failed++;
                await check(page, "y: downloaded, written, indexed", "Press any key to go back to EBaC.", 1000);
                await check(page, "the index was built", "is ready");
                await sleep(2000);
                const held = (await screen(page)).includes("Press any key to go back to EBaC.");
                console.log(`${held ? "ok  " : "FAIL"} "Press any key" waits, though ${view.hasTouch ? "Enter and a tap came" : "Enter came"} while it worked`);
                if (!held) failed++;
                await keys(page, " ");
                await check(page, "back to EBaC, with it", "Selected translation: AFR53-TB");
                await view316("V at once: John 3:16 in it (Johannes)", "Want so lief het God");
            };
            const removeAfrikaans = async (back) => {
                await keys(page, "a");
                await keys(page, "t");
                await check(page, "A, T: the languages", "Choose a language", 60000);
                await keys(page, "2\r");
                await check(page, "2: Afrikaans, added already", "* 1) AFR53-TB");
                await keys(page, "r1\r");
                await check(page, "r1: removed", "AFR53-TB: removed.");
                await keys(page, " ");
                await check(page, "back to EBaC, with the AV", back);
            };
            // (on the phone: added and removed, its page shares the storage)
            if (view.hasTouch) {
                await addAfrikaans();
                await removeAfrikaans("Selected translation: AV");
            }
            // kept in the browser's storage, so it is there when EBaC starts
            // again; then removed, and gone the next time
            if (!view.hasTouch) {
                const restart = async () => {
                    await keys(page, "e");
                    await sleep(1000);
                    await Promise.all([page.waitForNavigation({ waitUntil: "load" }), page.keyboard.press("Enter")]);
                };
                await addAfrikaans();
                await restart();
                await check(page, "EBaC started again: it is still there", "Selected translation: AFR53-TB");
                await view316("V: John 3:16 in it (Johannes)", "Want so lief het God");
                await removeAfrikaans("Selected translation: AV");
                await restart();
                await check(page, "EBaC started again: still the AV", "Selected translation: AV");
                const left = await page.evaluate(() => new Promise((resolve) => {
                    const r = indexedDB.open("/ebac");
                    r.onsuccess = () => {
                        const db = r.result, out = [];
                        const c = db.transaction(db.objectStoreNames[0]).objectStore(db.objectStoreNames[0]).openCursor();
                        c.onsuccess = (e) => {
                            const cur = e.target.result;
                            if (cur) { if (String(cur.key).startsWith("/ebac/AFR53-TB")) out.push(cur.key); cur.continue(); }
                            else resolve(out.length);
                        };
                    };
                    r.onerror = () => resolve(-1);
                }));
                console.log(`${left === 0 ? "ok  " : "FAIL"} and its files are gone from the browser's storage (${left} left)`);
                if (left !== 0) failed++;
                // A, S: scrollmapper/bible_databases's languages, from GitHub;
                // Enter alone, back to EBaC
                await keys(page, "a");
                await check(page, "A: where from? (S too)", "S)crollMapper/bible_databases");
                await keys(page, "s");
                await check(page, "S: its languages (from GitHub)", "Ancient Hebrew", 60000);
                await keys(page, "\r");
                await check(page, "Enter: back to EBaC", "Main Menu");

                // A, P: a text from Project Gutenberg, by its ebook number,
                // through EBaC's relay (web/relay: Gutenberg's sites don't
                // let a page read them); from its mirror while
                // www.gutenberg.org is down. Added, read, and removed.
                await keys(page, "a");
                await check(page, "A: where from? (P too)", "P)roject Gutenberg");
                await keys(page, "p");
                await check(page, "P: its ebook number", "Ebook number (Enter alone: back):");
                // the text's download held back 3 seconds: meanwhile the
                // spinner turns where the cursor is (\ | / -), then is gone
                await page.setRequestInterception(true);
                const hold = (r) => {
                    if (r.method() === "GET" && /pg17\.txt|17\.txt\.utf-8/.test(r.url()))
                        setTimeout(() => r.continue(), 3000);
                    else
                        r.continue();
                };
                page.on("request", hold);
                await keys(page, "17\r");
                const turns = new Set();
                for (let i = 0; i < 30; i++) {
                    const [line, x] = await page.evaluate(() => {
                        const b = window.term.buffer.active;
                        return [b.getLine(b.baseY + b.cursorY).translateToString(true), b.cursorX];
                    });
                    if (i % 5 === 0)
                        console.log(`     (cursor line ${JSON.stringify(line)}, column ${x})`);
                    if (x === 1 && /^[\\|\/-]$/.test(line))
                        turns.add(line);
                    await sleep(100);
                }
                console.log(`${turns.size >= 3 ? "ok  " : "FAIL"} a held download: the spinner turns (${[...turns].join(" ")})`);
                if (turns.size < 3) failed++;
                page.off("request", hold);
                await page.setRequestInterception(false);
                await check(page, "17: its license", "Do you agree to these terms (y/n)?", 180000);
                await keys(page, "y\r");
                await check(page, "y: downloaded (through the relay), written, indexed",
                            "Press any key to go back to EBaC.", 180000);
                await check(page, "the Book of Mormon is ready", "BOM-PG: done.");
                await keys(page, " ");
                await check(page, "back to EBaC, with it", "Selected translation: BOM-PG");
                await keys(page, "v");
                await keys(page, "1 nephi\r");
                await keys(page, "1\r");
                await keys(page, "1\r");
                await check(page, "V: 1 Nephi 1:1", "having been born of goodly parents");
                await keys(page, "x");
                await sleep(500);
                await keys(page, "x");
                await check(page, "back to the main menu", "Main Menu");
                await keys(page, "a");
                await keys(page, "p");
                await check(page, "A, P: it is listed", "Added from Project Gutenberg:");
                await keys(page, "r1\r");
                await check(page, "r1: removed", "BOM-PG: removed.");
                await keys(page, " ");
                await check(page, "back to EBaC, with the AV", "Selected translation: AV");
            }
        }
    }
    if (errors.length) {
        console.log("     page errors: " + errors.join(" | "));
    }
    await page.close();
}
// The shortcut on the home screen: the page can be installed (Chrome's
// own judgement, with its icons), and offers it on a phone in a bar above
// the terminal, which fits to what is left; closed, it stays closed.
{
    console.log("\n== the home-screen shortcut");
    const report = (ok, what) => { console.log(`${ok ? "ok  " : "FAIL"} ${what}`); if (!ok) failed++; };
    const page = await browser.newPage();
    await page.setViewport({ width: 1200, height: 900 });
    await page.goto("http://localhost:8097/", { waitUntil: "load" });
    await check(page, "a computer: the main menu", "Main Menu");
    const cdp = await page.createCDPSession();
    const { installabilityErrors } = await cdp.send("Page.getInstallabilityErrors");
    report(installabilityErrors.length === 0,
           "Chrome can install it: " + (installabilityErrors.map(e => e.errorId).join(", ") || "no errors"));
    const manifest = await (await fetch("http://localhost:8097/manifest.webmanifest")).json();
    for (const icon of [...manifest.icons.map(i => i.src), "icons/icon-180.png"]) {
        const r = await fetch("http://localhost:8097/" + icon);
        report(r.ok && r.headers.get("content-type") === "image/png", `its icon ${icon}`);
    }
    report(!(await page.evaluate(() => window.ebacInstallBar())), "no bar on a computer");
    await page.close();

    // an Android phone: the bar once Chrome offers to install (an offer
    // made up here: headless Chrome makes none)
    const phone = { width: 412, height: 923, deviceScaleFactor: 2.625, isMobile: true, hasTouch: true };
    const android = await browser.newPage();
    await android.setViewport(phone);
    await android.goto("http://localhost:8097/", { waitUntil: "load" });
    await check(android, "Android: the main menu", "Main Menu");
    // (Chrome may offer it itself, as the page loads: the bar is then
    // there already; the offer made up here is then the one taken)
    const own = await android.evaluate(() => window.ebacInstallBar());
    console.log(`     (Chrome itself offered to install it: ${own ? "yes" : "no"})`);
    await android.evaluate(() => {
        const e = new Event("beforeinstallprompt", { cancelable: true });
        e.prompt = () => { window.prompted = true; };
        e.userChoice = Promise.resolve({ outcome: "accepted" });
        window.dispatchEvent(e);
    });
    await sleep(1000);
    const rows1 = await android.evaluate(() => window.term.rows);
    report(await android.evaluate(() => window.ebacInstallBar()), "Android: the bar shown");
    await keys(android, "\x0c");
    await check(android, "the main menu, below the bar", "Main Menu");
    await android.click("#install .add");
    await sleep(1000);
    const rows2 = await android.evaluate(() => window.term.rows);
    report(await android.evaluate(() => window.prompted === true && !window.ebacInstallBar()) &&
           rows2 > rows1,
           `Install: Chrome's own offer made; the bar gone, the terminal ${rows1} rows to ${rows2}`);
    await android.close();

    // an iPhone: the bar says how (Share, then Add to Home Screen); its
    // close button closes it, and it stays closed
    const iphone = await browser.newPage();
    await iphone.setUserAgent("Mozilla/5.0 (iPhone; CPU iPhone OS 18_0 like Mac OS X) " +
                              "AppleWebKit/605.1.15 (KHTML, like Gecko) Version/18.0 Mobile/15E148 Safari/604.1");
    await iphone.setViewport({ width: 390, height: 844, deviceScaleFactor: 3, isMobile: true, hasTouch: true });
    await iphone.goto("http://localhost:8097/", { waitUntil: "load" });
    await check(iphone, "iPhone: the main menu", "Main Menu");
    const said = await iphone.evaluate(() => window.ebacInstallBar() &&
                                       document.querySelector("#install .text").textContent);
    report(/Share, then Add to Home Screen/.test(said || ""), `iPhone: the bar says how: ${said}`);
    await iphone.click("#install .close");
    await sleep(500);
    report(!(await iphone.evaluate(() => window.ebacInstallBar())) &&
           (await iphone.evaluate(() => localStorage.getItem("ebac-install-bar"))) === "closed",
           "✕: closed, and kept closed");
    await iphone.reload({ waitUntil: "load" });
    await check(iphone, "iPhone, again: the main menu", "Main Menu");
    report(!(await iphone.evaluate(() => window.ebacInstallBar())), "iPhone, again: no bar");
    await iphone.close();
}

await browser.close();
server.close();
console.log(failed ? `\n${failed} checks failed` : "\nall checks passed");
process.exit(failed ? 1 : 0);
