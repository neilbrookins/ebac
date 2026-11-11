// webfit.mjs -- does the web page fit the terminal to its window, from the
// start? Opens EBaC's page (ttyd's, with gestures.js) in headless Chrome,
// waits for EBaC's main menu, and asks the terminal to fit itself again: if
// that changes its columns or rows, the page had not fitted it, and EBaC
// would draw short of the window's right edge until it was resized (found
// November 11, 2026). Run by .github/workflows/docker.yml against the image.
//
//   node tests/webfit.mjs [URL]        (puppeteer-core; CHROME: the browser)
import puppeteer from "puppeteer-core";

const url = process.argv[2] || "http://localhost:7681/";
const sizes = [[1200, 900], [1366, 768], [800, 1000]];
const browser = await puppeteer.launch({
    executablePath: process.env.CHROME || "/usr/bin/google-chrome",
    args: ["--no-sandbox", "--use-angle=swiftshader", "--enable-unsafe-swiftshader"],
});
let failed = 0;
for (const [width, height] of sizes) {
    const page = await browser.newPage();
    page.on("console", m => { if (/\[ttyd\]|EBaC/.test(m.text())) console.log("   page: " + m.text()); });
    await page.setViewport({ width, height });
    await page.goto(url, { waitUntil: "networkidle0" });
    await page.waitForFunction(
        () => window.term && window.term.buffer.active.getLine(0) &&
              Array.from({ length: window.term.rows }, (_, i) =>
                  window.term.buffer.active.getLine(i).translateToString()).join("\n").includes("Main Menu"),
        { timeout: 20000 });
    await new Promise(r => setTimeout(r, 2000));
    const state = () => page.evaluate(() => ({
        cols: window.term.cols, rows: window.term.rows,
        webgl: !!document.querySelector(".xterm canvas"),
    }));
    const before = await state();
    await page.evaluate(() => window.term.fit());
    await new Promise(r => setTimeout(r, 500));
    const after = await state();
    const ok = before.cols === after.cols && before.rows === after.rows;
    console.log(`${ok ? "ok  " : "FAIL"} ${width}x${height}: ${before.cols}x${before.rows}` +
                (ok ? "" : `, but fits ${after.cols}x${after.rows}`) +
                (before.webgl ? " (canvas renderer)" : " (DOM renderer)"));
    if (!ok)
        failed++;
    await page.close();
}
await browser.close();
console.log(`${sizes.length - failed} of ${sizes.length} window sizes fitted from the start`);
process.exit(failed ? 1 : 0);
