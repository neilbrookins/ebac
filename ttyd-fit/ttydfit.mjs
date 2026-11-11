// ttydfit.mjs URL [renderer] -- is ttyd's terminal fitted to its window once
// the page has settled? Opens the page in headless Chrome at three window
// sizes, waits for the shell's prompt and a moment more, then asks the
// terminal to fit itself (window.term.fit()): if that changes its columns or
// rows, the page had not fitted it (tsl0922/ttyd#1534). With a renderer, the
// rendererType client option is set by the URL's query.
import puppeteer from "puppeteer-core";

const base = process.argv[2] || "http://localhost:7681/";
const renderer = process.argv[3];
const url = renderer ? `${base}?rendererType=${renderer}` : base;
const sizes = [[1200, 900], [1366, 768], [800, 1000]];
const browser = await puppeteer.launch({
    executablePath: process.env.CHROME || "/usr/bin/google-chrome",
    args: ["--no-sandbox", "--use-angle=swiftshader", "--enable-unsafe-swiftshader"],
});
let failed = 0;
for (const [width, height] of sizes) {
    const page = await browser.newPage();
    const logs = [];
    page.on("console", m => logs.push(m.text()));
    await page.setViewport({ width, height });
    await page.goto(url, { waitUntil: "networkidle0" });
    await page.waitForFunction(() => {
        const t = window.term;
        if (!t) return false;
        for (let i = 0; i < t.rows; i++)
            if (t.buffer.active.getLine(i)?.translateToString().trim()) return true;
        return false;
    }, { timeout: 20000 });
    await new Promise(r => setTimeout(r, 2000));
    const size = () => page.evaluate(() => ({ cols: window.term.cols, rows: window.term.rows }));
    const before = await size();
    await page.evaluate(() => window.term.fit());
    await new Promise(r => setTimeout(r, 500));
    const after = await size();
    const ok = before.cols === after.cols && before.rows === after.rows;
    const loaded = logs.filter(l => /renderer (loaded|could not)/.test(l)).join("; ") || "no renderer message";
    console.log(`${ok ? "ok  " : "FAIL"} ${width}x${height}: ${before.cols}x${before.rows}` +
                (ok ? "" : `, but fits ${after.cols}x${after.rows}`) + `  (${loaded})`);
    if (!ok) failed++;
    await page.close();
}
await browser.close();
console.log(`${sizes.length - failed} of ${sizes.length} window sizes fitted` +
            (renderer ? ` (rendererType=${renderer})` : " (default renderer)"));
process.exit(failed ? 1 : 0);
