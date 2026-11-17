#!/usr/bin/env python3
"""
A on EBaC's main menu, then T (thiagobodruk/bible), S
(scrollmapper/bible_databases) or P (Project Gutenberg): the screens of the importer in C (in EBaC
itself) against those of the shell importer it replaces (add-
thiagobodruk.sh, add-scrollmapper.sh, which EBaC ran in its place), key by
key, as the terminal shows them, at a phone's size and at 80 columns.

Each EBaC runs in a folder of its own: the AV, and for the old one its
importer scripts, made to take the source from a copy (their SOURCE,
NAMES_SOURCE and REGISTRY_SOURCE lines changed); the new one is given the
copies by EBAC_IMPORT_SOURCE and EBAC_NAMES_SOURCE (IANA's names it keeps).

Usage:
    python3 tests/importscreens.py OLD_BIBLE NEW_BIBLE SOURCE_COPY NAMES_COPY
                                   [thiagobodruk|scrollmapper|gutenberg [REGISTRY_COPY]]
SOURCE_COPY, for gutenberg: a folder of its texts and pages (17.txt, 17.html).
OLD_BIBLE: EBaC as built before (with its index program and its shell
importers beside it: a checkout of commit 72ace7e, the last with them, built).
REGISTRY_COPY: a folder with IANA's language-subtag-registry, for the old
importer (scrollmapper/bible_databases has codes Unicode has no names for).
"""
import os
import re
import shutil
import sys
import tempfile

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "navigation"))
from screen import Screen  # noqa: E402

import pty, select, time, fcntl, termios, struct, codecs  # noqa: E402

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# (keys, then what the screen must show before going on; L: the source's
# letter)
STEPS = [
    ("a", "Where from?"),
    ("L", "Choose a language"),
    ("n\r", "Choose a language"),
    ("p\r", "Choose a language"),
    ("99\r", "Not in the list: 99"),
    ("1 2\r", "One language at a time"),
    ("1\r", "Add or remove translations"),
    ("n\r", "Add or remove translations"),
    ("p\r", "Add or remove translations"),
    ("zz r3\r", "Not in the list: zz; not added: r3"),
    ("3\r", "Do you agree to these terms (y/n)?"),
    ("maybe\r", "Please type y or n."),
    ("y\r", "Press any key to go back to EBaC."),
    (" ", "Main Menu"),
    ("a", "Where from?"),
    ("L", "Choose a language"),
    ("1\r", "Add or remove translations"),
    ("r3\r", "Press any key to go back to EBaC."),
    (" ", "Main Menu"),
]

# Project Gutenberg: by ebook number
GUTENBERG_STEPS = [
    ("a", "Where from?"),
    ("p", "Ebook number (Enter alone: back):"),
    ("99\r", "Press any key to go back to EBaC."),
    (" ", "Main Menu"),
    ("a", "Where from?"),
    ("p", "Ebook number (Enter alone: back):"),
    ("rx\r", "Press any key to go back to EBaC."),
    (" ", "Main Menu"),
    ("a", "Where from?"),
    ("p", "Ebook number (Enter alone: back):"),
    ("1u\r", "Do you agree to these terms (y/n)?"),
    ("maybe\r", "Please type y or n."),
    ("y\r", "Press any key to go back to EBaC."),
    (" ", "Main Menu"),
    ("a", "Where from?"),
    ("p", "Added from Project Gutenberg:"),
    ("r2\r", "Press any key to go back to EBaC."),
    (" ", "Main Menu"),
    ("a", "Where from?"),
    ("p", "Added from Project Gutenberg:"),
    ("r1\r", "Press any key to go back to EBaC."),
    (" ", "Main Menu"),
    ("a", "Where from?"),
    ("p", "Ebook number (Enter alone: back):"),
    ("\r", "Main Menu"),
]


def run(binary, root, home, rows, cols, env_extra, letter, steps):
    env = dict(os.environ, TERM="xterm", HOME=home, BIBLEROOT=root, LC_ALL="C", **env_extra)
    pid, fd = pty.fork()
    if pid == 0:
        os.chdir(root)
        os.execve(binary, [binary], env)
    fcntl.ioctl(fd, termios.TIOCSWINSZ, struct.pack("HHHH", rows, cols, 0, 0))
    scr = Screen(rows, cols)
    dec = codecs.getincrementaldecoder("utf-8")("replace")

    def feed(b):
        scr.feed("".join(c if ord(c) < 256 else "~" for c in dec.decode(b)).encode("latin1"))

    def read_until(text, most=60):
        end = time.time() + most
        quiet_since = time.time()
        while time.time() < end:
            r, _, _ = select.select([fd], [], [], 0.1)
            if r:
                try:
                    feed(os.read(fd, 65536))
                except OSError:
                    break
                quiet_since = time.time()
            elif text in dump() and time.time() - quiet_since > 0.4:
                return True
        return text in dump()

    def dump():
        return "\n".join(l[1:].rstrip() if l.startswith("|") else l.rstrip()
                         for l in scr.dump().split("\n"))

    screens = []
    read_until("Main Menu")
    for keys, wanted in steps:
        keys = letter if keys == "L" else keys
        os.write(fd, keys.encode())
        ok = read_until(wanted)
        screens.append((keys, ok, dump()))
    os.kill(pid, 9)
    os.waitpid(pid, 0)
    return screens


def main():
    if len(sys.argv) < 5:
        sys.exit(__doc__)
    old, new, src, names = (os.path.abspath(a) for a in sys.argv[1:5])
    source = sys.argv[5] if len(sys.argv) > 5 else "thiagobodruk"
    registry = os.path.abspath(sys.argv[6]) if len(sys.argv) > 6 else None
    letter = {"thiagobodruk": "t", "scrollmapper": "s", "gutenberg": "p"}[source]
    steps = GUTENBERG_STEPS if source == "gutenberg" else STEPS
    tmp = tempfile.mkdtemp(prefix="ebac-importscreens-")
    fails = 0
    try:
        for rows, cols in ((40, 49), (24, 80)):
            roots = {}
            for which in ("old", "new"):
                root = os.path.join(tmp, which, "root")
                home = os.path.join(tmp, which, "home")
                shutil.rmtree(os.path.join(tmp, which), ignore_errors=True)
                os.makedirs(root)
                os.makedirs(home)
                with open(os.path.join(REPO, "trans")) as f:
                    av = [l for l in f if l.split()[:1] == ["AV"]]
                with open(os.path.join(root, "trans"), "w") as f:
                    f.writelines(av)
                os.symlink(os.path.join(REPO, "AV"), os.path.join(root, "AV"))
                roots[which] = (root, home)
            oldroot = roots["old"][0]
            olddir = os.path.dirname(old)
            for script in ("add-thiagobodruk.sh", "add-scrollmapper.sh", "add-gutenberg.sh",
                           "importer.sh"):
                with open(os.path.join(olddir, script)) as f:
                    text = f.read()
                if script == "add-%s.sh" % source:
                    text = re.sub(r"^SOURCE=.*$", "SOURCE=" + src, text, flags=re.M)
                    # (Project Gutenberg's: its texts and pages from the copy)
                    text = re.sub(r"^PLAIN_TEXT=.*$", "PLAIN_TEXT=file://%s/%%s.txt" % src, text, flags=re.M)
                    text = re.sub(r"^PAGE=.*$", "PAGE=file://%s/%%s.html" % src, text, flags=re.M)
                text = re.sub(r"^NAMES_SOURCE=.*$", "NAMES_SOURCE=" + names, text, flags=re.M)
                if registry:
                    text = re.sub(r"^REGISTRY_SOURCE=.*$", "REGISTRY_SOURCE=" + registry, text, flags=re.M)
                with open(os.path.join(oldroot, script), "w") as f:
                    f.write(text)
            shutil.copy(os.path.join(olddir, "index"), oldroot)
            shutil.copy(old, os.path.join(oldroot, "bible"))
            a = run(os.path.join(oldroot, "bible"), oldroot, roots["old"][1], rows, cols, {}, letter, steps)
            b = run(new, roots["new"][0], roots["new"][1], rows, cols,
                    {"EBAC_IMPORT_SOURCE": src, "EBAC_NAMES_SOURCE": names,
                     "EBAC_GUTENBERG_TEXT": "file://%s/%%s.txt" % src,
                     "EBAC_GUTENBERG_PAGE": "file://%s/%%s.html" % src}, letter, steps)
            for (keys, ok_a, sa), (_, ok_b, sb) in zip(a, b):
                same = sa == sb
                label = "%dx%d %r" % (rows, cols, keys)
                if same and ok_a:
                    print("ok   " + label)
                    continue
                fails += 1
                print("FAIL " + label + ("" if ok_a and ok_b else
                                          " (old shown: %s, new shown: %s)" % (ok_a, ok_b)))
                la, lb = sa.split("\n"), sb.split("\n")
                for i in range(max(len(la), len(lb))):
                    x = la[i] if i < len(la) else ""
                    y = lb[i] if i < len(lb) else ""
                    if x != y:
                        print("     row %d old: %r" % (i, x))
                        print("     row %d new: %r" % (i, y))
            for which in ("old", "new"):
                print("     %s trans: %s" % (which, open(os.path.join(roots[which][0], "trans")).read().split("\n")[:2]))
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    print("\n%s" % ("all screens the same" if not fails else "%d screens differ" % fails))
    sys.exit(1 if fails else 0)


if __name__ == "__main__":
    main()
