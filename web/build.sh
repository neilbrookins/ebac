#!/bin/sh
# Build EBaC to run in a web browser, with nothing on a server but files
# (see web/README.md): the same C source, compiled to WebAssembly by
# Emscripten, in a page with xterm.js, the terminal ttyd uses, and
# xterm-pty, which gives the compiled program a terminal (keys, raw mode,
# the window's size) in the page.
#
# Needs: Emscripten (emcc) on the PATH; make and a C compiler, to build the
# concordance index natively first; and, in web/node_modules (npm install
# in web/), @xterm/xterm, @xterm/addon-fit and xterm-pty.
#
#   sh web/build.sh            the site, in web/build/site
set -e
cd "$(dirname "$0")/.."
OUT=web/build
SITE=$OUT/site
NM=web/node_modules

# The concordance index (each translation's .dat files): built by the
# native index program, as for every other build; the browser only reads it.
make --no-print-directory index indexes

rm -rf "$OUT"
mkdir -p "$SITE" "$OUT/root"

# The version, as the Information screen shows it
COMMIT=$(git log -1 --format='%h, %cs' 2>/dev/null || sed -n '/Format/!s/^commit //p' version.txt)

# The translations that come with the site, as EBaC's folder (BIBLEROOT)
# has them: the King James Version, for now, with its index. The page
# copies them into EBaC's folder in the browser's storage (see index.html),
# again when the site is newer (.site: which site they came with).
cp -RL AV "$OUT/root/AV"
grep '^AV ' trans > "$OUT/root/trans"
printf '%s\n' "$COMMIT" > "$OUT/root/.site"

# EBaC's relay to Project Gutenberg (web/relay/gutenberg-relay.js, a
# Cloudflare Worker): a web page can't read what Gutenberg's sites send
# (CORS), so P under A downloads through it. Without it, no P.
RELAY=${GUTENBERG_RELAY-https://ebac-gutenberg.hybridcarssucklg.workers.dev}
case $RELAY in *RELAY*) RELAY="" ;; esac

emcc -O2 -Wall -Wextra -DUNIXV -DNORAMDISK -DXTERM_BUILTIN \
    -DEBAC_COMMIT="\"$COMMIT\"" -DEBAC_PACKAGE='"web browser"' \
    ${RELAY:+-DEBAC_GUTENBERG_RELAY="\"$RELAY\""} \
    bible.c file.c userint.c process.c curses.c utf8.c import.c net.c indexlib.c \
    -sASYNCIFY -sFORCE_FILESYSTEM -sALLOW_MEMORY_GROWTH -sENVIRONMENT=web \
    -sEXIT_RUNTIME=1 \
    -sEXPORTED_RUNTIME_METHODS=ENV,FS,addRunDependency,removeRunDependency \
    -lidbfs.js \
    --js-library="$NM/xterm-pty/emscripten-pty.js" \
    --preload-file "$OUT/root@/ebac-site" \
    -o "$SITE/ebac.mjs"

# The page, and what it loads (all served from the site itself); the
# touch gestures are the Docker image's and the Home Assistant app's own.
# (EXIT_RUNTIME, above: so the page hears when EBaC ends, and says so;
# idbfs: the browser's storage, IndexedDB, for EBaC's home folder and
# its folder of translations.)
cp web/index.html docker/gestures.js "$SITE/"
cp "$NM/@xterm/xterm/lib/xterm.js" "$NM/@xterm/xterm/css/xterm.css" "$SITE/"
cp "$NM/@xterm/addon-fit/lib/addon-fit.js" "$SITE/"
cp "$NM/xterm-pty/index.mjs" "$SITE/xterm-pty.mjs"
ls -l "$SITE"
