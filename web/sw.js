/*
 * EBaC's service worker (web/README.md): the page's files kept in the
 * browser, so that EBaC opens at once, and with no Internet at all (in
 * airplane mode, say), from the files it last had; and a newer EBaC
 * noticed, and offered (the page's "A newer EBaC is ready" bar), without
 * ever making anyone wait for the network.
 *
 * build.sh fills in VERSION (the commit, and a fingerprint of every file:
 * any change makes a new one) and FILES (the site's files). The browser
 * looks for a changed sw.js when the page opens and when the page asks
 * (when EBaC is brought back to the screen): a changed one is installed
 * beside this one, its files downloaded in the background, and waits;
 * the page offers it, and on Update asks it to take over ("update"),
 * then reloads.
 */
const VERSION = "@VERSION@";
const FILES = @FILES@;
const CACHE = "ebac-" + VERSION;

self.addEventListener("install", (e) => {
    // (no-cache: each file asked of the site again, so none is an older one
    // the browser kept; one it has, unchanged, isn't downloaded again)
    e.waitUntil(caches.open(CACHE).then((c) =>
        c.addAll(FILES.map((f) => new Request(f, { cache: "no-cache" })))));
});

// The one taking over: the files of every other version let go of
self.addEventListener("activate", (e) => {
    e.waitUntil(caches.keys()
        .then((names) => Promise.all(names
            .filter((n) => n.startsWith("ebac-") && n !== CACHE)
            .map((n) => caches.delete(n))))
        .then(() => self.clients.claim()));
});

self.addEventListener("message", (e) => {
    if (e.data === "update")
        self.skipWaiting();
});

// The site's own files from what is kept; anything else (a translation
// from GitHub, the relay to Project Gutenberg) from the network as ever
self.addEventListener("fetch", (e) => {
    const url = new URL(e.request.url);
    if (e.request.method !== "GET" || url.origin !== self.location.origin)
        return;
    e.respondWith(caches.open(CACHE)
        .then((c) => c.match(e.request, { ignoreSearch: true }))
        .then((kept) => kept || fetch(e.request)));
});
