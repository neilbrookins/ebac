#!/usr/bin/env python3
"""
Translations added or removed by another EBaC, or by voice, while this one
runs (November 9, 2026).

The importers (add-translation.sh, add-thiagobodruk.sh, add-gutenberg.sh)
add and remove translations for everyone: another user's EBaC, or the Home
Assistant integration's voice, can do it while this EBaC is running. This
checks, in both builds (NORAMDISK and RAMDISK), that:

  - C lists the translations as 'trans' has them now: one added since this
    EBaC started is there, one removed isn't (reread_trans());
  - the translation in use, removed meanwhile, doesn't stop EBaC: it says
    so, and reads from another, whether it is noticed at the main menu, in a
    passage (moving on to another book) or in a search's results
    (translation_removed()); the RAMDISK build's temporary files are deleted;
  - the importers' replace_trans() writes 'trans' all at once, keeping its
    permissions, and writes through a 'trans' that is a symbolic link.

The other EBaC's part is played by the shell, as importer.sh's remove()
does it: 'trans' without the line, then the folder deleted. The
translations are copies of the AV (KJ2, KJ3), in a folder made for the test.

Usage:
    make test
or by hand:
    python3 tests/otherebac.py NORAMDISK_BINARY RAMDISK_BINARY
"""
import glob
import os
import re
import shutil
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from sidebyside import run  # noqa: E402  (a session in a pseudo-terminal)

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
# (a step that starts "$ " is run by the shell, in the EBaC folder, between keys)
REMOVE_KJ2 = "$ grep -v '^KJ2 ' trans > t.new; cat t.new > trans; rm -f t.new; rm -rf KJ2"
ADD_KJ3 = "$ cp -r KJ3.spare KJ3 && printf 'KJ3  Third copy\\n' >> trans"
REMOVED = "The translation KJ2 has been removed"

# (name, steps, what must be on the screen, what must not, whether the last
# passage shown must be read from the AV)
CHECKS = [
    ("C lists one added since", [ADD_KJ3, "c", "x", "e"],
     ["3) KJ3  Third copy"], [], False),
    ("C leaves out one removed since", [REMOVE_KJ2, "c", "x", "e"],
     ["1) AV  King James Version"], ["2) KJ2"], False),
    ("removed in use, at the main menu", ["c", "2", REMOVE_KJ2, "\x0c", " ", "e"],
     [REMOVED, "Now reading from AV."], [], False),
    # (removed while the main menu is on the screen, then S: the search
    # line, reading the first verses, finds it gone, and gives up at once;
    # it took the x's that mean "back" as letters, until the line was full
    # and EBaC stopped: "Out of room in string!")
    ("removed in use, then S", ["c", "2", REMOVE_KJ2, "s", " ", "e"],
     [REMOVED, "Now reading from AV."], ["xxxx", "Out of room"], False),
    ("removed in use, in a passage", ["c", "2", "v", "joh\r", "3\r", "16\r", REMOVE_KJ2, "f",
                                      " ", "v", "gen\r", "1\r", "1\r", "x", "e"],
     [REMOVED, "Now reading from AV."], [], True),
    ("removed in use, in search results", ["c", "2", "s", "jesus wept\r", REMOVE_KJ2, "1", "f",
                                           " ", "v", "gen\r", "1\r", "1\r", "x", "e"],
     [REMOVED, "Now reading from AV."], [], True),
]


def plain(out):
    """The screen's text, without the terminal's control sequences."""
    text = out.decode("utf-8", "replace")
    return re.sub(r"\x1b\[[0-9;?]*[A-Za-z]|\x1b[()][A-Z0-9]|\x1b[=>]|[\x07\x08\r]", "", text)


def fresh_root(root):
    """The EBaC folder for one check: the AV, and KJ2 listed after it."""
    shutil.rmtree(root, ignore_errors=True)
    os.makedirs(root)
    os.symlink(os.path.join(REPO, "AV"), os.path.join(root, "AV"))
    for name in ("KJ2", "KJ3.spare"):
        shutil.copytree(os.path.join(REPO, "AV"), os.path.join(root, name), symlinks=True)
    with open(os.path.join(root, "trans"), "w") as f:
        f.write("AV  King James Version\nKJ2  Second copy\n")


def run_with_shell(binary, steps, root, home):
    """run(), with the shell's steps done between keys."""
    import sidebyside
    real_write = os.write
    shell = {}

    def write(fd, data):
        step = shell.get(data)
        if step is not None:
            subprocess.run(step, shell=True, cwd=root, check=False)
            return len(data)
        return real_write(fd, data)

    keys = []
    for step in steps:
        if step.startswith("$ "):
            marker = bytes([0xF0 + len(shell)])     # (a byte no key sends)
            shell[marker] = step[2:]
            keys.append(marker.decode("latin1"))
        else:
            keys.append(step)
    sidebyside.os.write = write
    try:
        return run(binary, keys, home, root)
    finally:
        sidebyside.os.write = real_write


def check_replace_trans(tmp):
    """importer.sh's and add-gutenberg.sh's replace_trans()."""
    problems = []
    for script in ("importer.sh", "add-gutenberg.sh"):
        with open(os.path.join(REPO, script)) as f:
            text = f.read()
        if "replace_trans() {" not in text:
            problems.append("%s: it has no replace_trans()" % script)
            continue
        start = text.index("replace_trans() {")
        function = text[start:text.index("\n}\n", start) + 3]
        here = os.path.join(tmp, "rt")
        shutil.rmtree(here, ignore_errors=True)
        os.makedirs(os.path.join(here, "real"))
        test = function + r'''
here="$1"; printf 'AV x\n' > "$here/trans"; chmod 664 "$here/trans"
printf 'AV x\nKJ2 y\n' > "$here/new"
replace_trans "$here/new" || echo "failed"
[ "$(wc -l < "$here/trans")" -eq 2 ] || echo "not written"
[ "$(stat -c %a "$here/trans" 2>/dev/null || stat -f %Lp "$here/trans")" = 664 ] || echo "permissions not kept"
ls "$here" | grep -q 'trans\.' && echo "a file left beside it"
rm "$here/trans"; printf 'AV x\n' > "$here/real/trans"; ln -s real/trans "$here/trans"
replace_trans "$here/new"
[ -L "$here/trans" ] || echo "the symbolic link replaced"
[ "$(wc -l < "$here/real/trans")" -eq 2 ] || echo "not written through the link"
'''
        result = subprocess.run(["sh", "-c", test, "sh", here], capture_output=True, text=True)
        problems += ["%s: %s" % (script, line) for line in result.stdout.split("\n") if line]
    return problems


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    builds = (("NORAMDISK", os.path.abspath(sys.argv[1])),
              ("RAMDISK", os.path.abspath(sys.argv[2])))
    tmp = tempfile.mkdtemp(prefix="ebac-other-")
    fails = 0
    try:
        for name, steps, wanted, unwanted, from_av in CHECKS:
            problems = []
            for tag, binary in builds:
                root = os.path.join(tmp, "root")
                home = os.path.join(tmp, "home-" + tag)
                fresh_root(root)
                shutil.rmtree(home, ignore_errors=True)
                os.makedirs(home)
                before = set(glob.glob("/tmp/ebac_*"))
                out, exited = run_with_shell(binary, steps, root, home)
                text = plain(out)
                for w in wanted:
                    if w not in text:
                        problems.append("%s: no %r" % (tag, w))
                for u in unwanted:
                    if u in text:
                        problems.append("%s: %r shown" % (tag, u))
                # (and reading on: Genesis 1:1, from the AV, after it)
                if from_av and "In the beginning" not in text.split(REMOVED)[-1]:
                    problems.append("%s: not reading on afterwards" % tag)
                if not exited:
                    problems.append("%s: did not exit when asked" % tag)
                if "Can't Open" in text:
                    problems.append("%s: stopped with an error" % tag)
                if b"Sanitizer" in out or b"runtime error" in out:
                    problems.append("%s: sanitizer reported an error" % tag)
                left = set(glob.glob("/tmp/ebac_*")) - before
                if left:
                    problems.append("%s: temporary files left: %s" % (tag, ", ".join(sorted(left))))
                    for f in left:
                        os.remove(f)
            fails += bool(problems)
            print("%s %s" % ("FAIL" if problems else "ok  ", name))
            for p in problems:
                print("     " + p)
            sys.stdout.flush()
        problems = check_replace_trans(tmp)
        fails += bool(problems)
        print("%s replace_trans() writes 'trans' all at once" % ("FAIL" if problems else "ok  "))
        for p in problems:
            print("     " + p)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    total = len(CHECKS) + 1
    print("\n%d of %d checks of translations changed by another EBaC passed" % (total - fails, total))
    sys.exit(1 if fails else 0)


if __name__ == "__main__":
    main()
