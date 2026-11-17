# EBaC in a web browser

EBaC itself, the same C source as every other build, compiled to
WebAssembly so it runs inside a web page: nothing to install, and nothing
on a server but files. The page is published with GitHub Pages:

**https://neilbrookins.github.io/ebac/**

It reads the King James Version (AV). The first visit downloads EBaC and
the text (about 15 megabytes).

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
  which screen it shows.
- **The end**: E on the main menu ends EBaC; the page then says so, and
  Enter or a tap starts it again, as ttyd's page offers to reconnect.
- **The files**: the AV and its concordance index, built natively by
  `index` as for every build, packed into `ebac.data`, and copied into
  EBaC's folder (`BIBLEROOT=/ebac`) the first time, and again when the
  site is newer.
- **The browser's storage** (IndexedDB): EBaC's folder and its home
  folder are kept there, so the search history, and the translations
  added with A, are there the next time.
- **Adding translations**: A, then T (thiagobodruk/bible) or S
  (scrollmapper/bible_databases), downloaded from GitHub, by the importer
  in C, in EBaC itself.

## Building

    cd web && npm install && cd ..
    sh web/build.sh                  # the site, in web/build/site
    node web/test.mjs web/build/site # checks it in headless Chrome

`build.sh` needs Emscripten's `emcc` on the PATH (pinned to 3.1.69 in
`.github/workflows/web.yml`: xterm-pty works by replacing some of
Emscripten's own functions, and is tested with that version and the
latest). The workflow builds and tests each change, and publishes the site
from `main`.

## Not yet

- Adding texts from Project Gutenberg, until its relay is set up (see
  below): without it, P isn't offered here.

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

To set it up, in Cloudflare's dashboard: **Workers & Pages**, **Create**,
**Create Worker**; name it `ebac-gutenberg`; **Deploy**; then **Edit
code**, replace the code with `relay/gutenberg-relay.js`, and **Deploy**.
Its address is then `https://ebac-gutenberg.SUBDOMAIN.workers.dev`
(SUBDOMAIN: the account's). The build takes it from `GUTENBERG_RELAY`
(the default in `build.sh`, or in the environment); without one, P
isn't offered.

The text itself comes from the mirror first (run by Project Gutenberg,
and the same file, byte for byte), and from www.gutenberg.org only if
the mirror can't be had, here and everywhere else EBaC runs: Gutenberg
says its website is "intended for human users only". Its page (title,
last update, copyright status) is only on www.gutenberg.org. The relay
streams: each file goes on to the browser as it comes, never held whole
in the Worker's memory.

