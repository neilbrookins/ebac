#!/usr/bin/env python3
"""
Book names typed in full: a book's whole name chooses it, even where it is
also the start of other books' names (November 9, 2026).

A translation's book names can start with another book's whole name (the
KJVA from thiagobodruk/bible has Esther and an "Esther" with more after
it). A space or Enter ends a book's name, so the shorter one can't be
typed any further: its whole name must choose it, or it can't be chosen.

When a name is the start of several and none of them whole, they are
listed, one to a line, after "There is more than one book that matches
that name" (November 9, 2026).

Unlike sidebyside.py, which checks that the two builds draw the same
screens, this checks what they draw: the book each name chooses. It makes
a translation for the test, PT, whose books are A, AB, AC and AD (A is the
start of all four), BX and BY (B is the start of both, and neither is B),
and IEsdras and IEsdrasGreek (the whole name with a number for the Roman
numeral: "1esdras"). Each check is made at the V prompt of both builds
(NORAMDISK and RAMDISK), and of ebac-api, the voice's (refparse.c), which
matches names the same way (match_it()).

Usage:
    make test
or by hand:
    python3 tests/booknames.py NORAMDISK_BINARY RAMDISK_BINARY EBAC_API INDEX
"""
import os
import re
import shutil
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from sidebyside import run  # noqa: E402  (a session in a pseudo-terminal)

BOOKS = ["A", "AB", "AC", "AD", "BX", "BY", "IEsdras", "IEsdrasGreek"]

# (what is typed at "Please enter: Book name", the book it chooses, or None
# for "more than one book", or "" for "no book by that name")
CHECKS = [
    ("a", "A"),             # the whole name: A, though AB, AC and AD begin so
    ("ab", "AB"),           # and a letter more: the longer one
    ("ad", "AD"),
    ("b", None),            # the start of BX and BY, and no book's whole name
    ("bx", "BX"),
    ("1esdras", "IEsdras"),         # a number for the Roman numeral
    ("iesdras", "IEsdras"),
    ("1esdrasg", "IEsdrasGreek"),
    ("1esd", None),
]


def make_translation(root, index):
    """PT: each verse says which book it is in ("the first verse of AB")."""
    folder = os.path.join(root, "PT")
    os.makedirs(folder)
    with open(os.path.join(folder, "booklist"), "w") as f:
        f.writelines(name + "\n" for name in BOOKS)
    for name in BOOKS:
        with open(os.path.join(folder, name), "w") as f:
            f.write("[1:1] the first verse of %s\n" % name)
            f.write("[1:2] the second verse of %s\n" % name)
    subprocess.run([index, "-y"], cwd=folder, check=True, stdout=subprocess.DEVNULL)
    with open(os.path.join(root, "trans"), "w") as f:
        f.write("PT  Prefix test\n")


def listed(text):
    """The books listed after "more than one book", one to a line."""
    after = text.split("Please be more specific.", 1)[1]
    after = after.split("(Use a blank line", 1)[0]
    return [line.strip() for line in after.splitlines() if line.strip()]


def at_prompt(binary, typed, root, home):
    """The book chosen at the V prompt: its name, None (more than one), or
    "" (no book of that name). For more than one, the books listed must be
    those whose names begin so, in order (else it is told as an error)."""
    os.makedirs(home, exist_ok=True)
    keys = ["v", typed, "\r"]
    out, exited = run(binary, keys + ["\r", "\r", "x", "e"], home, root)
    if not exited:
        return "(did not exit)"
    text = out.decode("utf-8", "replace")
    if "more than one book" in text:
        expected = [b for b in BOOKS if b.lower().startswith(typed)
                    or (typed[:1] == "1" and b.startswith("I") and b[1:].lower().startswith(typed[1:]))]
        shown = listed(text)
        return None if shown == expected else "(listed %s)" % ", ".join(shown)
    if "no book by that name" in text:
        return ""
    found = re.search(r"the first verse of (\w+)", text)
    return found.group(1) if found else "(no verse shown)"


def by_voice(api, typed, root):
    """The book ebac-api reads for "TYPED 1:1"."""
    answer = subprocess.run(
        [api, "verse?ref=%s+1:1" % typed], env=dict(os.environ, BIBLEROOT=root),
        capture_output=True, text=True, check=False,
    ).stdout
    if "no book of that name" in answer:
        return ""
    if '"error"' in answer:
        return None
    found = re.search(r'"book": "(\w+)"', answer)
    return found.group(1) if found else "(no book in answer)"


def main():
    if len(sys.argv) < 5:
        sys.exit(__doc__)
    noramdisk, ramdisk, api, index = map(os.path.abspath, sys.argv[1:5])
    tmp = tempfile.mkdtemp(prefix="ebac-names-")
    fails = 0
    try:
        root = os.path.join(tmp, "root")
        make_translation(root, index)
        for typed, book in CHECKS:
            got = {
                "NORAMDISK": at_prompt(noramdisk, typed, root, os.path.join(tmp, "n")),
                "RAMDISK": at_prompt(ramdisk, typed, root, os.path.join(tmp, "r")),
            }
            got["ebac-api"] = by_voice(api, typed, root)
            bad = [tag for tag in got if got[tag] != book]
            # (by voice, several books is "no book of that name": no book
            # is chosen, which is what matters)
            if book is None and got["ebac-api"] in (None, ""):
                bad = [tag for tag in bad if tag != "ebac-api"]
            fails += bool(bad)
            print("%s %-10s -> %-14s %s" % (
                "FAIL" if bad else "ok  ", typed,
                {None: "(several)", "": "(none)"}.get(book, book),
                "; ".join("%s chose %r" % (t, got[t]) for t in bad)))
            sys.stdout.flush()
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    print("\n%d of %d book names chose as they should" % (len(CHECKS) - fails, len(CHECKS)))
    sys.exit(1 if fails else 0)


if __name__ == "__main__":
    main()
