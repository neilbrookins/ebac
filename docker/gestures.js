/*
 * Touch gestures for EBaC in the Home Assistant app.
 *
 * This script is added to the web page of ttyd, the web terminal EBaC runs
 * in. The terminal itself never passes swipes on to the program, and every
 * tap makes a phone show its keyboard; so the gestures are recognised here,
 * in the page, and turned into the keys EBaC already understands.
 *
 * EBaC says which screen is showing with a private escape sequence,
 * "ESC ] 7766 ; name BEL" (only when EBAC_GESTURES is set; see scr_mode()
 * in EBaC's curses.c), and the gestures depend on it:
 *
 *   screen    tap                              swipe
 *   menu      the nearest of C, A, V, S, I    -
 *             or E (under A: G, P or X):
 *             that key
 *   trans     the nearest numbered translation  left/right: back, unchanged
 *   transenter  (more than 9 translations, shown a page at a time)
 *             a numbered translation: its       up/down: next/previous page;
 *             number and Enter                  left/right: back, unchanged
 *   list      a result (any line of it): its    up: next page, down: previous
 *             label                             left/right: back (x)
 *   passage   -                                 up/down: next/previous verse;
 *                                               a long swipe: next/previous
 *                                               screen; left/right: back (x);
 *                                               with two fingers, up/down: the
 *                                               next/previous book (f, b)
 *   anykey    anywhere: a key ("Press any key") any: a key
 *   search    keyboard up: Enter               down: the previous search in
 *                                               the history, up: the next
 *                                               (Up and Down arrows), any length;
 *                                               left/right: cursor left/right,
 *                                               a long one: to that end of the
 *                                               line (CTRL-A, CTRL-E; on a
 *                                               Hebrew line too, as it looks)
 *   input     keyboard up: Enter               left: erase a character
 *   (book,                                      (Backspace), a long one: the
 *   chapter,                                    whole entry (CTRL-U);
 *   verse)                                      right: Enter
 *   addlist   -                                 up: next page, down: previous
 *   (A: the                                     (what was typed erased, then
 *   lists of                                    n or p and Enter); left/right:
 *   languages,                                  Enter (what was typed, such as
 *   translations)                               1 for English; or alone, back)
 *
 * A tap this script uses doesn't bring up the keyboard, except on View,
 * Search, Add a translation and Project Gutenberg in the main menu, which ask for typing next. Any other tap works
 * as before: it brings up the keyboard.
 *
 * Letters a phone keyboard holds back as a word being composed are sent to
 * EBaC at once (see "Keyboards that hold back what is typed", below).
 *
 * The terminal's scrollback (the lines that have scrolled off the top) is
 * turned off. EBaC redraws the whole screen itself, so those lines are only
 * old screens, and the terminal would scroll back to them on a swipe down,
 * fighting the swipe's own job (previous verse or page). The terminal's own
 * touch scrolling is also kept from seeing the swipes this script uses.
 *
 * Hiding a phone's keyboard leaves the terminal's typing box focused, and
 * the phone then brings the keyboard back as soon as the screen is redrawn,
 * shrinking the window in the middle of a page: a page back would skip the
 * lines in between. So a gesture made while the keyboard is hidden lets go
 * of the typing box first, and the keyboard stays hidden. (While it's
 * showing, gestures leave it showing.) A tap anywhere else still brings it
 * up.
 */
(function () {
    "use strict";

    var MODE_OSC = 7766;    /* EBaC's "which screen" escape sequence */
    var TAP_MAX = 24;       /* px: a touch moving less than this is a tap */
    var LONG_SWIPE = 0.3;   /* a sideways swipe longer than this share of
                               the terminal's width is a long one: to the
                               start or end of the line, say */
    var LONG_SCROLL = 0.2;  /* a swipe up or down in a passage longer than
                               this share of the terminal's height is a long
                               one: a whole screen (0.3 before 2.2.3, which
                               took more of a swipe than seemed natural) */

    var mode = "";          /* the screen EBaC last said it was showing */
    var term = null;        /* the terminal being listened to */
    var touchTerm = null;   /* the terminal the touch handlers are on */
    var start = null;       /* where the current touch began */
    var two = null;         /* a two-finger touch: where the two fingers'
                               midpoint began, and where it is now */
    var lastTouchEnd = 0;   /* to ignore the click a browser adds after a tap */
    var fullHeight = {};    /* the tallest the window has been, per width */
    var ROTATE_TIME = 1500; /* ms: a lost focus this close to a change in
                               width comes from turning the phone */
    var lettingGo = false;  /* this script is taking the focus away itself */
    var lastWidth = window.innerWidth;
    var widthChanged = 0;   /* when the window's width last changed */
    var focusLost = 0;      /* when the typing box last lost the focus */

    /*
    ** Troubleshooting: with the app's "debug_overlay" option on, the page
    ** is given window.EBAC_DEBUG, and a small box in the top right corner
    ** lists what this script sees: the focus coming and going, the window
    ** changing size, and its attempts to take the focus back. (For a phone
    ** whose behaviour can't be reproduced elsewhere: a screenshot shows it.)
    */
    var debugBox = null, debugLines = [], debugStart = Date.now();

    function debug(text) {
        if (!window.EBAC_DEBUG)
            return;
        if (!debugBox && document.body) {
            debugBox = document.createElement("div");
            debugBox.style.cssText = "position:fixed;top:0;right:0;z-index:99999;" +
                "max-width:70%;padding:2px 4px;font:9px/1.2 monospace;" +
                "white-space:pre;overflow:hidden;pointer-events:none;" +
                "background:rgba(255,255,160,0.85);color:#000";
            document.body.appendChild(debugBox);
        }
        debugLines.push(((Date.now() - debugStart) / 1000).toFixed(1) + " " + text);
        if (debugLines.length > 32)
            debugLines.shift();
        if (debugBox)
            debugBox.textContent = debugLines.join("\n");
    }

    /* the element with the focus in a document, looking inside shadow roots
       (as Home Assistant's page uses them) */
    function deepActive(doc) {
        var a = doc.activeElement;
        while (a && a.shadowRoot && a.shadowRoot.activeElement)
            a = a.shadowRoot.activeElement;
        return a;
    }

    /* the focus, in short: this page focused (P) or not (p) and what has it;
       then Home Assistant's page around it (T or t) and what has it there */
    function focusState() {
        var a = document.activeElement, text, top;

        text = (document.hasFocus() ? "P " : "p ") +
            (term && a === term.textarea ? "box" : a ? a.tagName.toLowerCase() : "none");
        try {
            if (window.parent !== window) {
                top = window.parent.document;
                a = deepActive(top);
                text += " / " + (top.hasFocus() ? "T " : "t ") +
                    (a ? a.tagName.toLowerCase() + (a.id ? "#" + a.id : "") : "none");
            }
        } catch (err) {
            text += " / ?";             /* (another origin: can't see it) */
        }
        return text;
    }

    function send(key) {
        term.input(key, true);
    }

    /* the window whose visible area the keyboard changes: Home Assistant's
       own page, around the frame this page is in (same site, so it can be
       read), or this page by itself */
    function outerWindow() {
        try {
            if (window.top !== window && window.top.document)
                return window.top;
        } catch (err) {
            /* (another site: only this page) */
        }
        return window;
    }

    /*
    ** The height of the part of this page that can be seen: above the
    ** keyboard. On some phones (the Home Assistant app on Android, say) the
    ** keyboard is drawn over the page without making it smaller, and only
    ** the outer page's visual viewport shrinks; this frame keeps its full
    ** height. So it is measured there: from this frame's top to the bottom
    ** of what can be seen.
    */
    function windowHeight() {
        var w = outerWindow(), vv = w.visualViewport, top = 0, h;

        if (!vv)
            return window.innerHeight;
        if (w !== window) {
            try {
                top = window.frameElement ?
                    window.frameElement.getBoundingClientRect().top : 0;
            } catch (err) {
                top = 0;
            }
        }
        h = vv.offsetTop + vv.height - top;
        return (h > 0 && h < window.innerHeight) ? h : window.innerHeight;
    }

    /*
    ** Fit the terminal to the part of the page above the keyboard, so that
    ** EBaC (and the translation importers) are told only the rows that can
    ** be seen, and draw their screens to fit: ttyd sizes the terminal to
    ** its container (#terminal-container) when the window is resized, so
    ** the container is made that height and ttyd told to fit again. With
    ** no keyboard over the page, it is its full height again.
    */
    var fitTimer = null;
    function fitToVisible() {
        var box = document.getElementById("terminal-container"),
            h = Math.floor(windowHeight()), want;

        if (!box)
            return;
        want = (h < window.innerHeight - 2) ? h + "px" : "";
        if (box.style.height === want)
            return;
        box.style.height = want;
        debug("fit to " + (want || "the full height"));
        window.dispatchEvent(new Event("resize"));      /* ttyd fits */
    }
    function fitSoon() {
        if (fitTimer)
            clearTimeout(fitTimer);
        fitTimer = setTimeout(function () {
            fitTimer = null;
            fitToVisible();
        }, 60);
    }

    var keyboardUp = false;     /* as of the last resize */
    var firstSeen = {};         /* when each width was first seen */

    /* remember the window's full height (no keyboard) for this width, and
       whether the keyboard is up. (Not judged for a width first seen in the
       last moment, as while the phone turns: its full height isn't known
       yet, so the keyboard is taken to be as it was.) */
    function noteHeight() {
        var w = window.innerWidth, h = windowHeight(), now = Date.now();
        if (!fullHeight[w] || h > fullHeight[w])
            fullHeight[w] = h;
        if (!firstSeen[w])
            firstSeen[w] = now;
        if (now - firstSeen[w] > ROTATE_TIME)
            keyboardUp = h < 0.85 * fullHeight[w];
    }

    /* before a gesture that needs no typing: if the typing box is focused
       but the keyboard is hidden (the window is its full height), let go of
       the typing box, so the phone doesn't bring the keyboard back */
    function keepKeyboardHidden() {
        var box = term.textarea;
        noteHeight();
        if (box && document.activeElement === box &&
                windowHeight() >= 0.85 * fullHeight[window.innerWidth]) {
            lettingGo = true;
            box.blur();
            lettingGo = false;
        }
    }

    /*
    ** Keep the keyboard working when the phone is turned. Home Assistant
    ** lays its page out again when the window's width crosses 870 px (its
    ** sidebar changes between a pop-over and a docked one), as a phone's
    ** does when turned sideways, and that moves the panel this page is in;
    ** the browser then takes the focus from the terminal's typing box. The
    ** keyboard stays up, but what is typed goes nowhere (the cursor shows
    ** as an outline). So when the typing box loses the focus close to a
    ** change in the window's width, and this script didn't let go of it,
    ** it is given the focus back. (A change in height alone, such as the
    ** keyboard coming or going, is left alone.)
    */
    function refocus() {
        focusLost = 0;
        /* (tried a few times: the focus may be taken more than once while
           the phone turns) */
        [50, 400, 1200].forEach(function (delay) {
            setTimeout(function () {
                if (!term || !term.textarea ||
                        (document.hasFocus() && document.activeElement === term.textarea))
                    return;
                /* the focus may have gone out of this page altogether (to
                   the Home Assistant page around it), with the typing box
                   still this page's active element: take the focus back
                   first */
                debug("refocus @" + delay + ": " + focusState());
                window.focus();
                term.focus();
                if (!document.hasFocus() && window.frameElement) {
                    /* through Home Assistant's page: give the focus to the
                       frame this page is in, and then to the typing box */
                    try {
                        window.frameElement.focus();
                        term.focus();
                    } catch (err) {
                        /* (not allowed: nothing more to try) */
                    }
                    debug("  via frame: " + focusState());
                } else {
                    debug("  now " + focusState());
                }
            }, delay);
        });
    }

    function onBlur() {
        debug("box blur" + (lettingGo ? " (ours)" : "") +
              (widthChanged ? ", " + (Date.now() - widthChanged) + " ms after width change" : ""));
        if (lettingGo)
            return;
        focusLost = Date.now();
        if (focusLost - widthChanged < ROTATE_TIME)
            refocus();
    }

    /*
    ** When the phone's own focus leaves the web page altogether (in the
    ** Home Assistant Companion app, turning the phone does this), no script
    ** can bring it back: only a touch can. The keyboard stays up for a few
    ** seconds, but what is typed goes nowhere. So if the page loses the
    ** focus while the keyboard is showing, and neither it nor Home
    ** Assistant's page around it has the focus, a bar asks for a tap; it
    ** goes as soon as the focus comes back.
    */
    var tapBar = null;

    function topHasFocus() {
        try {
            return window.parent !== window && window.parent.document.hasFocus();
        } catch (err) {
            return false;
        }
    }

    function showTapBar() {
        if (!tapBar) {
            tapBar = document.createElement("div");
            tapBar.textContent = "Tap here to type";
            tapBar.style.cssText = "position:fixed;left:0;right:0;bottom:0;z-index:99998;" +
                "padding:10px;text-align:center;font:bold 16px sans-serif;" +
                "background:#ffd54f;color:#000;";
            /* a tap on the bar is a tap on the terminal: bring up the keyboard */
            tapBar.addEventListener("click", function () {
                hideTapBar();
                if (term)
                    term.focus();
            });
        }
        if (!tapBar.parentNode && document.body)
            document.body.appendChild(tapBar);
        debug("tap bar shown");
    }

    function hideTapBar() {
        if (tapBar && tapBar.parentNode) {
            tapBar.parentNode.removeChild(tapBar);
            debug("tap bar hidden");
        }
    }

    function onPageBlur() {
        debug("blur check: keyboard " + (keyboardUp ? "up" : "down"));
        if (lettingGo || !keyboardUp || !term ||
                document.activeElement !== term.textarea ||
                document.visibilityState !== "visible")
            return;
        /* (a moment later: the focus may be on its way to somewhere else in
           the page, such as Home Assistant's own menu) */
        setTimeout(function () {
            if (!document.hasFocus() && !topHasFocus() &&
                    document.visibilityState === "visible")
                showTapBar();
        }, 300);
    }

    function noteWidth() {
        debug("resize " + window.innerWidth + "x" + window.innerHeight +
              (window.visualViewport ? " vv " + Math.round(window.visualViewport.height) : "") +
              ", " + focusState());
        if (window.innerWidth === lastWidth)
            return;
        lastWidth = window.innerWidth;
        widthChanged = Date.now();
        if (focusLost && widthChanged - focusLost < ROTATE_TIME)
            refocus();
    }

    /* the text on a row of the screen, 0 = top */
    function rowText(row) {
        var buf = term.buffer.active;
        var line = buf.getLine(buf.viewportY + row);
        return line ? line.translateToString(true) : "";
    }

    /* where a point on the page is, in rows from the top of the screen:
       2.5 is the middle of row 2 (rows count from 0) */
    function rowPos(y) {
        var screen = term.element.querySelector(".xterm-screen");
        var r = screen.getBoundingClientRect();
        return (y - r.top) / (r.height / term.rows);
    }

    /* the row of the screen at a point on the page, or -1 */
    function rowAt(y) {
        var row = Math.floor(rowPos(y));
        return (row >= 0 && row < term.rows) ? row : -1;
    }

    /* the main menu choice nearest a point: its letter, or null. EBaC puts
       a blank line between the choices, and the half of it nearer a choice
       goes with that choice, so a tap a little off still finds it. */
    function nearestChoice(y) {
        var pos = rowPos(y), best = null, bestDist = 1, r, m, d;

        for (r = 0; r < term.rows; r++) {
            m = /^\s*([A-Z])\)/.exec(rowText(r));
            if (!m)
                continue;
            d = Math.abs(pos - (r + 0.5));
            if (d < bestDist) {
                best = m[1];
                bestDist = d;
            }
        }
        return best;
    }

    /* the numbered translation ("3) ...") nearest a point, on the screen
       for changing translation: its number, or null. As on the main menu,
       EBaC puts a blank line between them when they fit so, and the half of
       it nearer a translation goes with that one; one to a line, the
       nearest is the one tapped. */
    function nearestTranslation(y) {
        var pos = rowPos(y), best = null, bestDist = 1, r, m, d;

        for (r = 0; r < term.rows; r++) {
            m = /^\s*([0-9]+)\)/.exec(rowText(r));
            if (!m)
                continue;
            d = Math.abs(pos - (r + 0.5));
            if (d < bestDist) {
                best = m[1];
                bestDist = d;
            }
        }
        return best;
    }

    /* what a tap does: { key, keyboard } or null for nothing */
    function tapAction(y) {
        var m, r, text, key, row = rowAt(y);

        if (row < 0)
            return null;
        switch (mode) {
        case "menu":
            key = nearestChoice(y);
            if (!key)
                return null;
            key = key.toLowerCase();
            /* View and Search ask for typing next, and so do GitHub and
               Project Gutenberg (under Add): let the keyboard come up. Add
               itself is another menu, to tap. */
            return { key: key, keyboard: key === "v" || key === "s" || key === "g" || key === "p" };
        case "trans":
            key = nearestTranslation(y);
            return key ? { key: key, keyboard: false } : null;
        case "transenter":
            key = nearestTranslation(y);
            return key ? { key: key + "\r", keyboard: false } : null;
        case "list":
            /* a result's label is on its first line; its other lines start
               with spaces. Anything else (the header, "--More--", a blank
               line) isn't a result. */
            for (r = row; r >= 0; r--) {
                text = rowText(r);
                m = /^([1-9a-z])\)/.exec(text);
                if (m)
                    return { key: m[1], keyboard: false };
                if (!/^\s+\S/.test(text))
                    return null;
            }
            return null;
        case "anykey":
            return { key: " ", keyboard: false };
        case "input":
        case "search":
            /* typing a line: while the keyboard is up, a tap is Enter; while
               it's hidden, a tap brings it up (null: an ordinary tap), as
               it always has, so it can always be got back */
            return keyboardUp ? { key: "\r", keyboard: true } : null;
        }
        return null;
    }

    /* the key for a swipe, or null for nothing */
    function swipeKey(dx, dy) {
        var box = term.element.getBoundingClientRect();
        var height = box.height;
        var forward = dy < 0;   /* finger moved up: on to what comes next */
        var longSide = Math.abs(dx) >= LONG_SWIPE * box.width;

        if (mode === "anykey")
            return " ";
        if (mode === "addlist") {
            /* the importer's lists (add-translation.sh): the same on both */
            if (Math.abs(dx) > Math.abs(dy))
                return "\r";                    /* Enter: the number typed */
            /* (CTRL-U first: the terminal erases a number half typed, so
               that "1" and a swipe up isn't "1n") */
            return forward ? "\x15n\r" : "\x15p\r";
        }
        if (Math.abs(dx) > Math.abs(dy)) {      /* left or right */
            if (mode === "passage" || mode === "list" || mode === "trans" ||
                    mode === "transenter")
                return "x";                     /* back */
            if (mode === "search") {            /* move the cursor */
                if (dx < 0)
                    /* (CTRL-A, CTRL-E: the left and right ends as the line
                       is shown, which on a Hebrew line are its end and its
                       start; the keyboard's Home and End keep their own
                       meaning, its start and end) */
                    return longSide ? "\x01" : "\x02";  /* left end, Left */
                return longSide ? "\x05" : "\x06";      /* right end, Right */
            }
            if (mode === "input" && dx < 0)     /* erase */
                /* CTRL-U, or Backspace as the keyboard's own sends it
                   (DEL): EBaC takes either, but a line read by the shell
                   (the importers' questions, such as P's address) only
                   DEL, and shows ^H for CTRL-H */
                return longSide ? "\x15" : "\x7f";
            if (mode === "input")               /* right: Enter */
                return "\r";
            return null;
        }
        if (mode === "list" || mode === "transenter")
            return forward ? " " : "p";
        if (mode === "search")      /* the history: Down and Up arrows */
            return forward ? "\x0e" : "\x10";
        if (mode === "passage") {
            if (Math.abs(dy) >= LONG_SCROLL * height)
                return forward ? " " : "p";     /* a screen */
            return forward ? "." : ",";         /* a verse */
        }
        return null;
    }

    function gestureScreen() {
        return mode === "menu" || mode === "trans" || mode === "transenter" ||
               mode === "list" ||
               mode === "passage" || mode === "anykey" || mode === "search" ||
               mode === "input" || mode === "addlist";
    }

    /* the point midway between the first two fingers */
    function midpoint(touches) {
        return { x: (touches[0].clientX + touches[1].clientX) / 2,
                 y: (touches[0].clientY + touches[1].clientY) / 2 };
    }

    /*
    ** Two fingers rarely touch, or lift, at the same instant. So a touch
    ** becomes a two-finger one as soon as a second finger joins it, stays
    ** one until every finger has lifted, and is then judged by how far the
    ** midpoint of the two fingers moved. Three or more fingers: nothing.
    */
    function onTouchStart(e) {
        if (e.touches.length === 1 && !two) {
            start = { x: e.touches[0].clientX, y: e.touches[0].clientY };
        } else if (e.touches.length === 2 && (start || !two)) {
            start = null;               /* not a one-finger gesture now */
            two = { from: midpoint(e.touches), to: midpoint(e.touches) };
        } else {
            start = null;               /* three fingers, or a lost touch */
            two = { from: null, to: null };     /* ignore until all lift */
        }
        if (gestureScreen())
            e.stopPropagation();        /* not for the terminal's scrolling */
    }

    function onTouchMove(e) {
        if (two && two.from && e.touches.length === 2)
            two.to = midpoint(e.touches);
        if (!(start || two) || !gestureScreen())
            return;
        /* keep the page from scrolling or refreshing under a swipe, and the
           terminal from scrolling its view */
        e.stopPropagation();
        if (e.cancelable)
            e.preventDefault();
    }

    /* the key for a two-finger swipe, or null */
    function twoFingerKey(dx, dy) {
        if (mode !== "passage" || Math.abs(dy) <= Math.abs(dx) ||
                Math.abs(dy) < TAP_MAX)
            return null;
        return dy < 0 ? "f" : "b";      /* up: the next book; down: back */
    }

    function onTouchEnd(e) {
        var t, dx, dy, action, key;

        if (two) {
            if (e.touches.length > 0)
                return;                 /* wait for the last finger */
            key = two.from ? twoFingerKey(two.to.x - two.from.x,
                                          two.to.y - two.from.y) : null;
            two = null;
            lastTouchEnd = Date.now();
            if (key !== null) {
                keepKeyboardHidden();
                send(key);
            }
            if (e.cancelable)
                e.preventDefault();     /* not a tap, not for the keyboard */
            return;
        }
        if (!start || e.changedTouches.length !== 1)
            return;
        t = e.changedTouches[0];
        dx = t.clientX - start.x;
        dy = t.clientY - start.y;
        start = null;
        lastTouchEnd = Date.now();

        if (Math.abs(dx) < TAP_MAX && Math.abs(dy) < TAP_MAX) {
            action = tapAction(t.clientY);
            if (!action)
                return;                 /* an ordinary tap: keyboard */
            if (!action.keyboard)
                keepKeyboardHidden();
            send(action.key);
            if (!action.keyboard && e.cancelable) {
                e.preventDefault();     /* no keyboard for this tap */
                e.stopPropagation();
            }
            return;
        }
        key = swipeKey(dx, dy);
        if (key !== null) {
            keepKeyboardHidden();
            send(key);
            if (e.cancelable)
                e.preventDefault();
        }
    }

    /*
    ** The mouse wheel: up and down arrows, as many as the lines the wheel
    ** moved, so EBaC goes back or on a verse for each (a notch of an
    ** ordinary wheel: about six). The terminal (xterm.js) does this itself
    ** only when it keeps no scrolled-off lines; it keeps one (see
    ** noScrollback), and would scroll that, showing nothing new, so the
    ** wheel is turned into arrows here, as it did (November 5, 2026).
    ** Only on a screen EBaC names; elsewhere (the translation importers)
    ** the terminal does as it does.
    */
    var wheelLines = 0;         /* part of a line, carried to the next turn */
    function onWheel(e) {
        var screen, cell, n, seq, out = "";

        if (!namedScreen() || e.deltaY === 0 || e.shiftKey)
            return;
        if (e.deltaMode === 1)                  /* (lines) */
            wheelLines += e.deltaY;
        else if (e.deltaMode === 2)             /* (pages) */
            wheelLines += e.deltaY * term.rows;
        else {                                  /* (pixels) */
            screen = term.element.querySelector(".xterm-screen") || term.element;
            cell = screen.getBoundingClientRect().height / term.rows;
            wheelLines += e.deltaY / (cell > 0 ? cell : 16);
        }
        e.preventDefault();
        e.stopImmediatePropagation();
        n = (wheelLines < 0) ? Math.ceil(wheelLines) : Math.floor(wheelLines);
        wheelLines -= n;
        seq = "\x1b" + ((term.modes && term.modes.applicationCursorKeysMode) ? "O" : "[") +
              (n < 0 ? "A" : "B");
        for (n = Math.abs(n); n > 0; n--)
            out += seq;
        if (out)
            send(out);
    }

    /*
    ** A click with the mouse does what a tap does: a menu line, a
    ** translation, a result, and on "Press any key", goes on (November 5,
    ** 2026: before, a click there did nothing, and the screen could only
    ** be left by a key). But not a click that ends selecting text, to copy
    ** it (the mouse dragged across it): that leaves the screen as it is.
    */
    function onClick(e) {
        var action;

        if (Date.now() - lastTouchEnd < 1000)
            return;                     /* the click a tap already handled */
        if (term && term.hasSelection && term.hasSelection())
            return;                     /* text selected, to copy */
        action = tapAction(e.clientY);
        if (action)
            send(action.key);
    }

    /*
    ** Keyboards that hold back what is typed. Some phone keyboards (the
    ** Amazon keyboard on a Fire, for one) treat letters as a word being
    ** composed, and hand the word over only when space or Enter ends it.
    ** The terminal waits for the word, so a single key at the main menu
    ** went nowhere until space was pressed, and then turned up later, at
    ** the start of the next search; and a book's name wasn't completed as
    ** it was typed. EBaC itself acts on every key as it comes, even on its
    ** typing screens (completing a book's name, editing the search line),
    ** so on every screen EBaC names, this script takes the letters itself
    ** as the keyboard composes them, and sends each one at once. A word is
    ** followed to its end, even if the screen changes on the way, so
    ** nothing is sent twice or lost. (Before EBaC has named a screen, the
    ** terminal is left to itself.)
    **
    ** The terminal's typing box holds what the keyboard has typed, and the
    ** letters sent are the change in it: new letters, and a Backspace (DEL)
    ** for each one taken away (the keyboard correcting a word, say).
    */
    var composing = false;  /* a word being composed is this script's */
    var boxSeen = "";       /* the typing box's text, as last sent */
    var realKeyDown = false;    /* a key of its own (not 229) is down: the
                                   terminal sends what it types (a space,
                                   say, then also typed into the box) */

    /* a screen EBaC has named: its letters are sent at once */
    function namedScreen() {
        return mode !== "";
    }

    function takesTyping() {
        return composing || namedScreen();
    }

    /* send the change in the typing box since it was last looked at */
    function sendBoxChange() {
        var now = term.textarea.value, same = 0, out = "", i;

        while (same < boxSeen.length && same < now.length &&
                boxSeen.charAt(same) === now.charAt(same))
            same++;
        for (i = same; i < boxSeen.length; i++)
            out += "\x7f";
        out += now.substring(same);
        boxSeen = now;
        if (out) {
            debug("sent " + JSON.stringify(out));
            send(out);
        }
    }

    function emptyBox() {
        term.textarea.value = "";
        boxSeen = "";
    }

    function onKeyDown(e) {
        if (term && e.target === term.textarea && e.keyCode !== 229)
            realKeyDown = true;
        if (!term || e.target !== term.textarea || !takesTyping())
            return;
        if (e.keyCode === 229) {        /* a phone keyboard's "some key" */
            debug("key 229" + (composing ? " (composing)" : ""));
            if (!composing)
                boxSeen = term.textarea.value;
            e.stopImmediatePropagation();   /* (the change is sent below) */
        } else if (composing) {
            /* a key of its own (Enter, say): the terminal sends it, and may
               empty the box; carry on from what the box then holds */
            setTimeout(function () { boxSeen = term.textarea.value; }, 0);
        }
    }

    function onCompositionStart(e) {
        if (!term || e.target !== term.textarea || !namedScreen())
            return;
        composing = true;
        boxSeen = term.textarea.value;
        debug("compose start");
        e.stopImmediatePropagation();
    }

    function onCompositionUpdate(e) {
        if (!composing || e.target !== term.textarea)
            return;
        debug("compose " + JSON.stringify(e.data));
        e.stopImmediatePropagation();
    }

    function onCompositionEnd(e) {
        if (!composing || e.target !== term.textarea)
            return;
        debug("compose end " + JSON.stringify(e.data));
        e.stopImmediatePropagation();
        setTimeout(function () {        /* (the box holds the final word now) */
            sendBoxChange();
            composing = false;
            emptyBox();
        }, 0);
    }

    function onKeyUp(e) {
        if (e.keyCode !== 229)
            realKeyDown = false;
    }

    /*
    ** Erasing what the keyboard thinks is still there. After a paste (or a
    ** word sent), this script empties the typing box, but a phone keyboard
    ** (Gboard, say) may still hold the text it thinks the box has, and
    ** turn Backspace into "delete the letter before the cursor" with no key
    ** of its own. With the box empty, nothing changes, and nothing reached
    ** EBaC: after a few, Backspace seemed to stop working. So when the
    ** keyboard asks to delete backwards and there is nothing before the
    ** cursor in the box, this script sends the erase key (DEL) itself, one
    ** for each request. (With text in the box, the change is sent as
    ** before, by onInput.)
    */
    function onBeforeInput(e) {
        var box = term && term.textarea;

        if (!box || e.target !== box || !takesTyping() || realKeyDown)
            return;
        if (e.inputType !== "deleteContentBackward" &&
                e.inputType !== "deleteWordBackward")
            return;
        if (composing || box.selectionStart !== 0 || box.selectionEnd !== 0)
            return;
        debug("beforeinput " + e.inputType + ", box empty: sent DEL");
        send("\x7f");
        e.stopImmediatePropagation();
        if (e.cancelable)
            e.preventDefault();
    }

    /*
    ** A paste is the terminal's own: it sends the text from the paste event
    ** but lets the browser put it in the typing box too. Sent again from
    ** the box, a paste on a phone (with no key down, unlike Ctrl+V) reached
    ** EBaC twice: "jesusjesus". So a paste in the box is only cleared away.
    */
    function onInput(e) {
        if (!term || e.target !== term.textarea || !takesTyping() ||
                (realKeyDown && !composing))
            return;
        debug("input " + e.inputType + " " + JSON.stringify(e.data));
        e.stopImmediatePropagation();
        if (e.inputType === "insertFromPaste") {
            if (!composing)
                emptyBox();
            else
                boxSeen = term.textarea.value;
            return;
        }
        sendBoxChange();
        if (!composing)
            emptyBox();
    }

    /*
    ** Copying right-to-left text in reading order. EBaC turns Hebrew (and
    ** any right-to-left script) about to show it, as the terminal shows
    ** every row left to right (u8_display_order() in EBaC's utf8.c), so a
    ** selection, copied as it is shown, had its letters backwards: pasted
    ** into a search, it found nothing. So when a selection has right-to-
    ** left text in it, it is put on the clipboard turned back, by the same
    ** rule, which undoes itself: each run of right-to-left letters (with
    ** the spaces and marks between them) turned about, numbers in it kept
    ** as they are, and each letter's marks (vowel points) staying with it.
    ** The spaces that align a row on the right are taken off, and the rows
    ** joined by a space, so a phrase wrapped on the screen pastes as one
    ** line (as the search line needs). Other selections are left alone.
    */
    function isRtl(ch) {
        var c = ch.codePointAt(0);
        return (c >= 0x0590 && c <= 0x08FF) || (c >= 0xFB1D && c <= 0xFDFF) ||
               (c >= 0xFE70 && c <= 0xFEFF) || (c >= 0x10800 && c <= 0x10FFF) ||
               (c >= 0x1E800 && c <= 0x1EFFF);
    }

    function readingOrder(row) {
        var cl = row.match(/\P{M}\p{M}*|\p{M}+/gu) || [], a, b, m, last, i, j, t;
        var digit = function (x) { return /^[0-9]/.test(x); };
        for (a = 0; a < cl.length; a++) {
            if (!isRtl(cl[a]))
                continue;
            last = a;
            for (m = a + 1; m < cl.length; m++) {
                if (isRtl(cl[m]))
                    last = m;
                else if (/^\p{L}/u.test(cl[m]))
                    break;              /* left to right again */
            }
            for (b = a, m = last; b < m; b++, m--) {
                t = cl[b]; cl[b] = cl[m]; cl[m] = t;
            }
            for (b = a; b <= last; b++) {       /* numbers as they were */
                if (!digit(cl[b]))
                    continue;
                for (m = b; m < last && digit(cl[m + 1]); m++)
                    ;
                for (i = b, j = m; i < j; i++, j--) {
                    t = cl[i]; cl[i] = cl[j]; cl[j] = t;
                }
                b = m;
            }
            a = last;
        }
        return cl.join("");
    }

    function onCopy(e) {
        var text, rows;

        if (!term || !term.hasSelection || !term.hasSelection() || !e.clipboardData)
            return;
        text = term.getSelection();
        if (!Array.from(text).some(isRtl))
            return;
        rows = text.split(/\r?\n/).map(function (row) {
            return readingOrder(row.trim());
        }).filter(function (row) { return row !== ""; });
        e.clipboardData.setData("text/plain", rows.join(" "));
        e.preventDefault();
        e.stopImmediatePropagation();
        debug("copied in reading order: " + JSON.stringify(rows.join(" ")));
    }

    /* listen for EBaC's "which screen" messages: as soon as the terminal
       exists, before any output arrives */
    function listen(t) {
        term = t;
        mode = "";
        composing = false;
        noScrollback(t);
        t.parser.registerOscHandler(MODE_OSC, function (name) {
            if (name !== mode)
                debug("screen " + name);
            mode = name;
            return true;
        });
        /* (in the troubleshooting box: the terminal's size, as EBaC and
           the importers are told it, each time it changes) */
        if (window.EBAC_DEBUG && t.onResize)
            t.onResize(function (size) {
                debug("terminal " + size.rows + " rows x " + size.cols +
                      (window.visualViewport ? ", vv " + Math.round(window.visualViewport.height) : ""));
            });
    }

    /*
    ** Keep almost no scrolled-off lines, so there is nothing to scroll back
    ** to: one, not none. With none, the terminal (xterm.js) can break while
    ** the window is resized quickly ("Cannot read properties of undefined
    ** (reading 'loadCell')", and in lineFeed, "isWrapped"): its screen and
    ** its lines no longer agree, it stops drawing what comes, and so stops
    ** telling ttyd it is ready for more; ttyd waits, EBaC waits on ttyd,
    ** and the page no longer answers keys until it is reloaded (found
    ** dragging a browser window's size about, at up to 271 columns). With
    ** one line kept, 160 bursts of resizing at random sizes up to 280
    ** columns gave no errors, where with none the first few froze it.
    ** (November 5, 2026.)
    */
    var SCROLLBACK = 1;
    function noScrollback(t) {
        try {
            if (t.options)
                t.options.scrollback = SCROLLBACK;
            else
                t.setOption("scrollback", SCROLLBACK);   /* older terminals */
        } catch (err) {
            /* leave it */
        }
    }

    /* handle touches on the terminal: once it is on the page */
    function attach(t) {
        var el = t.element;

        touchTerm = t;
        el.style.touchAction = "none";  /* the page mustn't pan or zoom here */
        el.addEventListener("touchstart", onTouchStart, { capture: true, passive: true });
        el.addEventListener("touchmove", onTouchMove, { capture: true, passive: false });
        el.addEventListener("touchend", onTouchEnd, { capture: true, passive: false });
        el.addEventListener("touchcancel", function () {
            start = null;
            two = null;
        }, true);
        el.addEventListener("click", onClick, true);
        el.addEventListener("wheel", onWheel, { capture: true, passive: false });
        /* (on the terminal, capturing: before the typing box's own
           handlers, which these keep from seeing what they take) */
        el.addEventListener("keydown", onKeyDown, true);
        el.addEventListener("keyup", onKeyUp, true);
        el.addEventListener("compositionstart", onCompositionStart, true);
        el.addEventListener("compositionupdate", onCompositionUpdate, true);
        el.addEventListener("compositionend", onCompositionEnd, true);
        el.addEventListener("beforeinput", onBeforeInput, true);
        el.addEventListener("input", onInput, true);
        if (t.textarea) {
            t.textarea.addEventListener("blur", onBlur);
            t.textarea.addEventListener("focus", function () {
                focusLost = 0;
                debug("box focus");
            });
        }
    }

    function check() {
        var t = window.term;

        if (t && t.parser && t !== term)
            listen(t);
        /* ttyd applies its settings when it connects: keep scrollback off */
        if (t && t === term && t.options && t.options.scrollback !== SCROLLBACK)
            noScrollback(t);
        if (t && t.element && t === term && t !== touchTerm)
            attach(t);
    }

    /*
    ** ttyd makes its terminal after the page loads, and again if it has to
    ** reconnect. Catch the moment it does (it sets window.term), so the
    ** handlers are in place before EBaC's first screen arrives and says
    ** which screen it is. As a fallback, also look for it regularly.
    */
    (function () {
        var current = window.term;

        try {
            Object.defineProperty(window, "term", {
                configurable: true,
                get: function () { return current; },
                set: function (t) { current = t; setTimeout(check, 0); check(); }
            });
        } catch (err) {
            /* not possible here: the regular check below still works */
        }
    }());
    setInterval(check, 50);
    firstSeen[window.innerWidth] = 1;   /* (the starting width: not turning) */
    noteHeight();
    window.addEventListener("copy", onCopy, true);
    window.addEventListener("resize", noteHeight);
    window.addEventListener("resize", noteWidth);
    window.addEventListener("blur", onPageBlur);
    window.addEventListener("focus", hideTapBar);
    document.addEventListener("visibilitychange", function () {
        if (document.visibilityState !== "visible")
            hideTapBar();
    });
    if (window.EBAC_DEBUG) {
        debug("page loaded, " + window.innerWidth + "x" + window.innerHeight);
        window.addEventListener("blur", function () { debug("page blur, " + focusState()); });
        window.addEventListener("focus", function () { debug("page focus, " + focusState()); });
        document.addEventListener("visibilitychange", function () {
            debug("visibility " + document.visibilityState);
        });
        window.addEventListener("orientationchange", function () { debug("orientationchange"); });
        window.addEventListener("pagehide", function () { debug("pagehide"); });
        window.addEventListener("pageshow", function () { debug("pageshow"); });
        document.addEventListener("focusin", function (e) {
            if (!term || e.target !== term.textarea)
                debug("focusin " + e.target.tagName.toLowerCase());
        });
    }
    if (window.visualViewport)
        window.visualViewport.addEventListener("resize", noteHeight);
    (function () {
        var outer = outerWindow(), vv = outer.visualViewport;
        if (vv) {
            vv.addEventListener("resize", fitSoon);
            vv.addEventListener("resize", noteHeight);
            vv.addEventListener("scroll", fitSoon);
        }
        if (outer !== window)
            outer.addEventListener("resize", fitSoon);
    }());

    /* for testing: which screen the page thinks EBaC is showing */
    window.ebacGestureMode = function () { return mode; };
    /* for testing: the key a tap at this height on the page would send */
    window.ebacTapKey = function (y) {
        var action = tapAction(y);
        return action ? action.key : null;
    };
}());
