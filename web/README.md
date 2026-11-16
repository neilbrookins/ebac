# EBaC in a web browser

EBaC itself, the same C source as every other build, compiled to
WebAssembly so it runs inside a web page: nothing to install, and nothing
on a server but files. The page is published with GitHub Pages:

**https://neilbrookins.github.io/ebac/**

It reads the King James Version (AV). The first visit downloads EBaC and
the text (about 15 megabytes); the browser keeps them after that.

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
- **The files**: the AV and its concordance index, built natively by
  `index` as for every build, packed into `ebac.data` and read from memory
  as EBaC's folder (`BIBLEROOT=/ebac`).

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

- Adding translations (the importers are shell scripts, which a browser
  can't run).
- The touch gestures of the Docker and Home Assistant page (gestures.js).
