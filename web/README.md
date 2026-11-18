# EBaC in a web browser

EBaC itself, the same C source as every other build, compiled to
WebAssembly so it runs inside a web page: nothing to install, and nothing
on a server but files. The page is published with GitHub Pages:

**https://neilbrookins.github.io/ebac/**

It comes with the King James Version (AV), and A on the main menu adds
others, from the same three places as every other build. The first visit
downloads EBaC and the text (about 15 megabytes).

## How it works

- **The program**: `bible.c file.c userint.c process.c curses.c utf8.c`,
  compiled by [Emscripten](https://emscripten.org/) with `-DUNIXV
  -DNORAMDISK -DXTERM_BUILTIN`. `XTERM_BUILTIN` (in curses.c) gives EBaC
  xterm's few terminal capabilities without ncurses or terminfo, which a
  browser doesn't have; built natively with it, every screen of
  `tests/sidebyside.py` is the same as the ncurses build's.
- **The terminal**: [xterm.js](https://xtermjs.org/), the terminal ttyd uses
  in the Docker image and the Home Assistant app, with
  [xterm-pty](https://github.com/mame/xterm-pty), which gives the program
  what a terminal gives it elsewhere: its keys a byte at a time, raw mode,
  the window's size, and a signal when it changes. EBaC waits for keys by
  Emscripten's Asyncify, so it needs no special headers from the web
  server (GitHub Pages can't send them).
- **The touch gestures**: the Docker image's and the Home Assistant app's
  own, `docker/gestures.js`, unchanged: the page is laid out as ttyd's
  (`#terminal-container`), and EBaC runs with `EBAC_GESTURES`, so it says
  which screen it shows; and the font: F and f on the main menu, a pinch,
  or the mouse wheel on the main menu, a pixel a step, kept in the
  browser's storage (`ebac-font-size`).
- **The end**: E on the main menu ends EBaC; the page then says so, and
  Enter or a tap starts it again, as ttyd's page offers to reconnect.
- **The home screen**: `manifest.webmanifest` makes the page installable
  (its name, EBaC; its icon, in `icons/`, drawn by `icons/draw.py`: the
  Home Assistant app's red Bible, on a white tile, the same on Android
  and the iPhone; and opening full screen). A page can't add a shortcut
  itself, so on a touch screen a bar above the terminal offers it until
  it is taken or closed (✕): on Android, its **Install** button asks
  Chrome to (Chrome's own offer is held back for it); on an iPhone or
  iPad, it says how (Share, then Add to Home Screen), as Apple's browsers
  let no page ask. Not shown when EBaC was opened from its icon (the
  manifest's `start_url` ends `?from=home-screen`, as a browser that
  opens the icon as an ordinary page doesn't say so otherwise), nor once
  installed. On a Fire tablet, Silk's icon opens it as a page, with the
  address bar; Chrome's opens it full screen (README.md says so).
- **Offline, and updates**: a service worker (`sw.js`; `build.sh` fills
  in its version, the commit and a fingerprint of every file, and the
  list of files) keeps the site's files in the browser, so EBaC opens at
  once, from them, with or without the Internet. The browser looks for a
  changed `sw.js` when the page opens, and the page asks it to whenever
  EBaC comes back to the screen (a home-screen app can stay running for
  days) and every 6 hours: in the background, never waited for. A newer
  one downloads its files beside the old, and a bar ("A newer EBaC is
  ready") offers it: **Update** saves EBaC's files, lets the newer one
  take over, and reloads the page; ✕, not now. Only the site's own files
  are kept: translations (GitHub, the relay) come from the network.
- **The files**: the AV and its concordance index, built natively by
  `index` as for every build, packed into `ebac.data`, and copied into
  EBaC's folder (`BIBLEROOT=/ebac`) the first time, and again when the
  site is newer.
- **The browser's storage** (IndexedDB): EBaC's folder and its home
  folder are kept there, so the search history, and the translations
  added with A, are there the next time.
- **Adding translations**: A, then T (thiagobodruk/bible) or S
  (scrollmapper/bible_databases), downloaded from GitHub, or P (Project
  Gutenberg), downloaded through EBaC's relay (below), by the importer in
  C, in EBaC itself, as in every other build.

## Building

    cd web && npm install && cd ..
    sh web/build.sh                  # the site, in web/build/site
    node web/test.mjs web/build/site # checks it in headless Chrome

`build.sh` needs Emscripten's `emcc` on the PATH (pinned to 3.1.69 in
`.github/workflows/web.yml`: xterm-pty works by replacing some of
Emscripten's own functions, and is tested with that version and the
latest). The workflow builds and tests each change, and publishes the site
from `main`.

## The relay to Project Gutenberg

A web page may read what it downloads from another site only if that
site's answer says it may (the header `Access-Control-Allow-Origin`:
CORS). The browser enforces it: it sends the page's address with the
request (`Origin`), and gives the page nothing if the answer doesn't
allow it. GitHub's answers allow any page (thiagobodruk/bible and
scrollmapper/bible_databases); Project Gutenberg's don't, neither
www.gutenberg.org's nor its mirror's (gutenberg.pglaf.org), so P (under
A) downloads through a relay of EBaC's own:
[`relay/gutenberg-relay.js`](relay/gutenberg-relay.js), a Cloudflare
Worker (the free plan is plenty). It gets the same file from Gutenberg,
as any program can, and passes it on unchanged, with the header added,
for EBaC's page only, and only what the importer asks for (an ebook's
page and its text, from those two sites).

EBaC's relay runs at `https://ebac-gutenberg.hybridcarssucklg.workers.dev`,
the default in `build.sh`. A build takes another from `GUTENBERG_RELAY`
in the environment; built with `GUTENBERG_RELAY=` (none), P isn't
offered.

To set one up, in Cloudflare's dashboard: **Compute**, **Workers &
Pages**, **Create application**, **Start with Hello World!**; name it
(such as `ebac-gutenberg`); **Deploy**; then **Edit code**, replace the
code with `relay/gutenberg-relay.js`, and **Deploy**. Its address is
`https://NAME.SUBDOMAIN.workers.dev` (SUBDOMAIN: the account's own,
fixed when the account is made). It lets only EBaC's page
(neilbrookins.github.io) and pages on this computer (`localhost`, for
`test.mjs`) read through it; another site's copy of the page needs its
address added to `PAGES` in the relay.

The text itself comes from the mirror first (run by Project Gutenberg,
and the same file, byte for byte), and from www.gutenberg.org only if
the mirror can't be had, here and everywhere else EBaC runs: Gutenberg
says its website is "intended for human users only". Its page (title,
last update, copyright status) is only on www.gutenberg.org. The relay
streams: each file goes on to the browser as it comes, never held whole
in the Worker's memory.

