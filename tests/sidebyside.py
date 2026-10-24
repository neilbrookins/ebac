#!/usr/bin/env python3
"""
Side-by-side test for EBaC: RAMDISK versus NORAMDISK.

EBaC can be compiled two ways. NORAMDISK keeps search results in memory
(malloc/free); RAMDISK keeps them in temporary files instead, for small
machines like the Tandy CoCo 3. The two are different code, but someone
using the program must never be able to tell them apart.

This script runs both builds through the same scripted sessions, each in
its own pseudo-terminal, and checks that everything they write to the
screen is byte-for-byte identical. It also checks that each program exits
when asked, that the RAMDISK build deletes all of its temporary files,
and (when built with -fsanitize) that no memory errors were reported.

Usage:
    make test
or by hand:
    python3 tests/sidebyside.py NORAMDISK_BINARY RAMDISK_BINARY [BIBLEROOT]

BIBLEROOT defaults to the top of this repository. Needs Python 3 and a
UNIX-like system with pseudo-terminals (Linux, BSD, macOS). Exit status
is 0 if every scenario is identical, 1 otherwise.

To add a test, add a line to SCENARIOS. Keys are written as Python
strings, so "\\r" is Enter, "\\x07" is CTRL-G, and so on. Each string is
typed, then the script waits for the screen to settle before the next.
A step "<resize ROWSxCOLS>" resizes the window instead of typing.
"""
import glob
import os
import pty
import select
import shutil
import signal
import struct
import sys
import tempfile
import termios
import fcntl
import threading
import time

ROWS, COLS = 24, 80

# (name, keystrokes) -- every scenario should end by exiting with "e"
SCENARIOS = [
    ("main menu, info, exit",   ["i", " ", " ", " ", "e"]),
    ("view John 3:16, page",    ["v", "joh\r", "3\r", "16\r", " ", " ", "p", "p", "x", "e"]),
    ("view verse step . ,",     ["v", "gen\r", "1\r", "1\r", ".", ".", ".", ",", ",", "x", "e"]),
    ("view next/prev book f b", ["v", "rut\r", "4\r", "20\r", "f", "f", "b", "b", "b", "x", "e"]),
    ("view end of Bible",       ["v", "rev\r", "22\r", "21\r", ".", " ", "p", "x", "e"]),
    # The ends of the Bible: b back to Genesis 1:1 (from Exodus, from within
    # Genesis); b, ',' at Genesis 1:1, and f, space, '.' at the end, beep
    # and stay; and nothing past Revelation 22:21 is drawn.
    ("back a book to Genesis",  ["v", "exo\r", "\r", "\r", "b", "b", ",", "x",
                                 "v", "gen\r", "25\r", "\r", "b", "x", "e"]),
    ("past the ends of the Bible", ["v", "rev\r", "22\r", "15\r", "f", " ", ".", "p", " ", " ", "x",
                                 "v", "rev\r", "22\r", "22\r", "x", "v", "gen\r", "1\r", "0\r", ",", "x", "e"]),
    ("view help screen",        ["v", "psa\r", "23\r", "\r", "?", "z", "x", "e"]),
    ("view bad book, retry",    ["v", "zz", "\r", "1co\r", "13\r", "4\r", "x", "e"]),
    ("view ambiguous book",     ["v", "jo\r", "joh\r", "\r", "\r", "x", "e"]),
    ("search one word",         ["s", "amen\r", " ", " ", " ", "x", "e"]),
    ("search word not found",   ["s", "xyzzyq\r", "x", "e"]),
    ("search and 0",            ["s", "love hate 0\r", " ", " ", "p", "x", "e"]),
    ("search proximity 2",      ["s", "love hate 2\r", " ", " ", " ", "p", "p", "x", "e"]),
    ("search or ,",             ["s", "faith hope ,\r", " ", " ", "x", "e"]),
    ("search and-not -",        ["s", "food @gen -\r", " ", " ", "x", "e"]),
    ("search book only",        ["s", "@jude\r", " ", " ", "x", "e"]),
    ("search in book",          ["s", "amen @rev 0\r", " ", "x", "e"]),
    ("gospels deep stack",      ["s", "blessed @mat @mar @lu @joh , , , 0\r", " ", " ", " ", "x", "e"]),
    ("gospels shallow stack",   ["s", "@mat @mar , @lu , @joh , blessed 0\r", " ", " ", " ", "x", "e"]),
    ("search complex",          ["s", "god love 0 lord fear 0 , @psa 0 praise -\r", " ", " ", "x", "e"]),
    ("very common word",        ["s", "the\r", " ", " ", " ", "x", "e"]),
    ("two common words",        ["s", "the and 0 of ,\r", " ", "x", "e"]),
    ("large not",               ["s", "and the -\r", " ", " ", "x", "e"]),
    ("view from list 1",        ["s", "jesus wept 0\r", "1", "x", "x", "e"]),
    ("view from list letters",  ["s", "grace\r", "3", " ", "x", "a", "x", " ", "c", "x", "x", "e"]),
    ("list help screen",        ["s", "mercy\r", "?", "z", "x", "e"]),
    ("several searches in row", ["s", "light\r", "x", "s", "darkness\r", "x", "s", "light darkness 1\r", " ", "x", "e"]),
    ("history recall ctrl-g",   ["s", "peace\r", "x", "s", "joy\r", "x", "s", "\x07\x07\r", "x", "s", "\x07\x07\x16\r", "x", "e"]),
    ("line editing",            ["s", "lovx\x7fe\x02\x02\x02\x06\x06\x06 hate\x180 \x12 0\r", "x", "e"]),
    ("invalid: blank",          ["s", "\r", "e"]),
    ("invalid: operator first", ["s", "love 0\r", " ", "e"]),
    ("invalid: no operator",    ["s", "love hate\r", " ", "s", "love\r", "x", "e"]),
    ("invalid: unknown book",   ["s", "amen @zzz 0\r", " ", "e"]),
    ("change translation",      ["c", "1", "s", "amen\r", "x", "e"]),
    # Keys that send ESC sequences (TERM=xterm, so terminfo gives ESC O x),
    # plus the Linux-console forms (ESC [ x), which are accepted too.
    ("arrow keys edit line",    ["s", "lovx\x7f", "e\x1bOD\x1bOD\x1b[C", "Z", "\x1bOH", "\x1b[3~",
                                 "\x1bOF", " hate 0", "\x1bOH", "\x1bOC", "\x1b[3~", "\r", "x", "e"]),
    ("history with arrows",     ["s", "peace\r", "x", "s", "joy\r", "x", "s", "\x1bOA", "\x1bOA",
                                 "\x1bOB", "\x1b[A", "\r", "x", "e"]),
    ("ctrl-a ctrl-e ctrl-d",    ["s", "hate", "\x01", "love ", "\x05", " 0", "\x01\x04", "l", "\r", "x", "e"]),
    ("view arrows, page keys",  ["v", "gen\r", "1\r", "1\r", "\x1bOB", "\x1bOB", "\x1b[A", "\x1b[6~",
                                 "\x1b[6~", "\x1b[5~", "\x1bOA", "x", "e"]),
    ("list page keys",          ["s", "amen\r", "\x1b[6~", "\x1b[6~", "\x1b[5~", "x", "e"]),
    ("unused keys ignored",     ["s", "\x1bOP", "\x1b[2~", "\x1b[15~", "amen\r", "x", "e"]),
    ("ESC key leaves view",     ["v", "joh\r", "1\r", "1\r", "\x1b", "s", "amen\r", "\x1b", "e"]),
    ("ESC then key at once",    ["v", "joh\r", "1\r", "1\r", "\x1bx", "e"]),
    # q w e r t y u i o pick list entries 1-9, like the digits above them;
    # entries 10 and up are labelled a b c d f g h j k l m s v z
    ("letters for 1-9 in list", ["s", "jesus wept ,\r", "q", "x", "w", "x", "E", "x", "o", "x",
                                 "1", "x", "z", "x", "x", "e"]),  # z: entry 23, below the screen
    ("letters past 9 in list",  ["s", "the\r", " ", "a", "x", "d", "x", "f", "x", "p", "x", "e"]),
    # chapter and verse prompts: q w e r t y u i o p are 1 2 3 4 5 6 7 8 9 0
    # Resizing the window redraws the passage and result screens at once,
    # with no key pressed, from the same first verse or entry.
    ("resize passage",          ["v", "joh\r", "3\r", "16\r", "<resize 40x80>", "<resize 14x80>",
                                 "<resize 24x50>", "<resize 24x80>", "x", "e"]),
    ("resize after stepping",   ["v", "mat\r", "28\r", "15\r", ".", ".", ".", ".", ".", ".",
                                 "<resize 40x80>", "<resize 12x80>", ".", "<resize 30x80>", "x", "e"]),
    ("resize result list",      ["s", "amen\r", "<resize 40x80>", "<resize 14x80>", " ",
                                 "<resize 40x80>", "<resize 12x60>", "p", "<resize 24x80>", "x", "e"]),
    ("rapid resizes",           ["s", "love\r"] + ["<resize 40x80>", "<resize 16x80>"] * 8 +
                                ["<resize 30x80>", "q", "<resize 18x80>", "x", "x", "e"]),
    ("resize while typing",     ["s", "lo", "<resize 30x80>", "ve\r", "x", "<resize 24x80>", "e"]),
    ("ctrl-l redraws",          ["v", "gen\r", "\r", "\r", "\x0c", ".", "\x0c", "x",
                                 "s", "light\r", "\x0c", "x", "e"]),
    # Labels past the bottom of the screen pick entries further down the
    # list (as if the window were taller), up to the end of the list.
    ("labels past the screen",  ["<resize 12x80>", "s", "faith @gal 0\r", "3", "x", "e", "x", "m", "x",
                                 "s", " ", "x", " ", "3", "x", "x", "e"]),
    ("labels past list end",    ["<resize 12x80>", "s", "jesus wept 0\r", "3", "x", "4", " ", "w", "x", "x", "e"]),
    # Chapter and verse are offsets: 0 and negative numbers count back,
    # Enter alone means 1, and past either end of the Bible shows "doesn't exist".
    ("zero and negative refs",  ["v", "joh\r", "3\r", "0\r", "x", "v", "exo\r", "0\r", "\r", "x",
                                 "v", "joh\r", "-2\r", "-3\r", "x", "v", "gen\r", "0\r", "\r", "x",
                                 "v", "rev\r", "99\r", "\r", "x", "v", "rut\r", "p\r", "p\r", "x",
                                 "v", "joh\r", "e\r", "Me\r", "x", "v", "joh\r", "mw\r", "\r", "x", "e"]),
    ("letters as digits in view", ["v", "gen\r", "r\r", "r\r", "x", "v", "psa\r", "qqo\r", "Qpt\r", "x",
                                   "v", "joh\r", "e1\r", "q", "p\r", "x", "e"]),
]


def run(binary, keys, home, bibleroot):
    """Run one session; return (output bytes, exited cleanly?)."""
    env = dict(os.environ, TERM="xterm", HOME=home, BIBLEROOT=bibleroot,
               ASAN_OPTIONS="detect_leaks=0")
    env.pop("LINES", None)
    env.pop("COLUMNS", None)
    pid, fd = pty.fork()
    if pid == 0:
        os.execve(binary, [binary], env)
    fcntl.ioctl(fd, termios.TIOCSWINSZ, struct.pack("HHHH", ROWS, COLS, 0, 0))
    out = bytearray()

    def drain(quiet, limit):
        """Read until nothing arrives for `quiet` seconds, or EOF."""
        end = time.time() + limit
        while time.time() < end:
            r, _, _ = select.select([fd], [], [], quiet)
            if not r:
                return True
            try:
                chunk = os.read(fd, 65536)
            except OSError:          # child exited and the pty closed
                return False
            if not chunk:
                return False
            out.extend(chunk)
        return True

    drain(1.0, 10)
    for k in keys:
        if k.startswith("<resize "):        # resize the window, as a user would
            rows, cols = map(int, k[8:-1].split("x"))
            fcntl.ioctl(fd, termios.TIOCSWINSZ, struct.pack("HHHH", rows, cols, 0, 0))
            drain(0.4, 20)
            continue
        for b in k.encode("latin1"):
            os.write(fd, bytes([b]))
        drain(0.4, 20)
    drain(0.5, 10)

    exited = False
    for _ in range(50):              # give it up to 5 seconds to exit
        p, _ = os.waitpid(pid, os.WNOHANG)
        if p == pid:
            exited = True
            break
        time.sleep(0.1)
    if not exited:
        os.kill(pid, signal.SIGKILL)
        os.waitpid(pid, 0)
    os.close(fd)
    return bytes(out), exited


def first_difference(a, b):
    la, lb = a.split(b"\n"), b.split(b"\n")
    for i, (x, y) in enumerate(zip(la, lb)):
        if x != y:
            return ("first difference at output line %d:\n"
                    "         NORAMDISK: %r\n         RAMDISK:   %r"
                    % (i + 1, x[:100], y[:100]))
    return "outputs differ in length: %d vs %d bytes" % (len(a), len(b))


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    builds = (("NORAMDISK", os.path.abspath(sys.argv[1])),
              ("RAMDISK", os.path.abspath(sys.argv[2])))
    here = os.path.dirname(os.path.abspath(__file__))
    bibleroot = os.path.abspath(sys.argv[3] if len(sys.argv) > 3
                                else os.path.join(here, ".."))
    for tag, b in builds:
        if not os.access(b, os.X_OK):
            sys.exit("%s binary not found: %s" % (tag, b))

    tmp = tempfile.mkdtemp(prefix="ebac-test-")
    fails = 0
    try:
        for name, keys in SCENARIOS:
            res = {}
            before = set(glob.glob("/tmp/ebac_*"))

            def one(tag, binary):
                home = os.path.join(tmp, tag)
                shutil.rmtree(home, ignore_errors=True)
                os.makedirs(home)
                out, exited = run(binary, keys, home, bibleroot)
                # the Information screen shows the program's own path and its
                # search history file, which differ between the two builds:
                # compare with them blanked out
                out = out.replace(os.path.abspath(binary).encode(), b"<program>")
                out = out.replace(home.encode(), b"<home>")
                res[tag] = (out, exited)

            # run both builds at the same time, each in its own terminal
            threads = [threading.Thread(target=one, args=b) for b in builds]
            for t in threads:
                t.start()
            for t in threads:
                t.join()
            left = set(glob.glob("/tmp/ebac_*")) - before

            problems = []
            a, b = res["NORAMDISK"], res["RAMDISK"]
            if a[0] != b[0]:
                problems.append(first_difference(a[0], b[0]))
            for tag, (out, exited) in res.items():
                if b"Sanitizer" in out or b"runtime error" in out:
                    problems.append(tag + ": sanitizer reported an error")
                if not exited:
                    problems.append(tag + ": did not exit")
            if left:
                problems.append("temporary files left behind: " +
                                ", ".join(sorted(left)))
                for f in left:
                    os.remove(f)

            fails += bool(problems)
            print("%s %-26s %7d bytes" % ("FAIL" if problems else "ok  ",
                                         name, len(a[0])))
            for p in problems:
                print("     " + p)
            sys.stdout.flush()
    finally:
        shutil.rmtree(tmp, ignore_errors=True)

    print("\n%d of %d scenarios identical in both builds"
          % (len(SCENARIOS) - fails, len(SCENARIOS)))
    sys.exit(1 if fails else 0)


if __name__ == "__main__":
    main()
