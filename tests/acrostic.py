"""The Hebrew alphabet's letters set apart (acrostic.c), end to end.

A made-up translation, from a made-up copy of thiagobodruk/bible, is added
by the importer as anyone's would be, and its books are checked: Psalm
119's stanzas' names (at the ends of verses 8, 16, ... 168, and Aleph
before verse 1) each a note of its own; Lamentations 1, an acrostic of
names before each verse, spelled differently from Psalm 119's, with a
verse starting with "He" (the pronoun) right after one; a chapter that
isn't an acrostic left as it is, though a verse in it ends with a name.
Then the passage screen: Aleph's note first, with "---", and 119:1's
number on the line after it.

usage: python3 tests/acrostic.py BIBLE (the program)
"""
import json, os, shutil, subprocess, sys, tempfile, time

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "navigation"))
from nav import Session

NAMES = ["Aleph", "Beth", "Gimel", "Daleth", "He", "Waw", "Zayin", "Heth", "Teth",
         "Yodh", "Kaph", "Lamedh", "Mem", "Nun", "Samekh", "Ayin", "Pe", "Tsadhe",
         "Qoph", "Resh", "Shin", "Taw"]
# Lamentations as the Douay-Rheims spells it, with a period after each
LAM = ["Aleph", "Beth", "Ghimel", "Daleth", "He", "Vau", "Zain", "Heth", "Teth",
       "Jod", "Caph", "Lamed", "Mem", "Nun", "Samech", "Ain", "Phe", "Sade", "Coph",
       "Res", "Sin", "Thau"]

def verse(n, text):
    return '<v n="%d">%s</v>' % (n, text)

def source(folder):
    """the made-up copy of thiagobodruk/bible"""
    os.makedirs(os.path.join(folder, "json"))
    os.makedirs(os.path.join(folder, "xml"))
    with open(os.path.join(folder, "json", "index.json"), "w") as f:
        json.dump([{"language": "English", "versions": [
            {"name": "Acrostic Test", "abbreviation": "en_acr"}]}], f)
    open(os.path.join(folder, "README.md"), "w").close()
    # (Psalms 1 to 117, a verse each: a book's chapters start at 1)
    before = "".join('<c n="%d">%s</c>' % (c, verse(1, "Psalm %d." % c)) for c in range(1, 118))
    ps118 = "".join(verse(n, "Give thanks %d." % n) for n in range(1, 30))
    ps119 = []
    for n in range(1, 177):
        t = "Joyful %d, who follow the Lord." % n
        if n == 1:
            t = "ALEPH. " + t
        if n % 8 == 0 and n < 176:
            t += " " + NAMES[n // 8]
        ps119.append(verse(n, t))
    ps120 = verse(1, "Not an acrostic. Beth") + verse(2, "He is here.")
    lam = []
    for n in range(1, 23):
        t = "%s. Text of verse %d." % (LAM[n - 1], n)
        if n == 5:
            t = "He. He hath built against me."
        lam.append(verse(n, t))
    xml = ('<?xml version="1.0" encoding="utf-8"?><bible>'
           '<b id="sl" n="Psalms">%s<c n="118">%s</c><c n="119">%s</c><c n="120">%s</c></b>'
           '<b id="lm" n="Lamentations"><c n="1">%s</c></b></bible>'
           % (before, ps118, "".join(ps119), ps120, "".join(lam)))
    with open(os.path.join(folder, "xml", "en_acr.xml"), "w", encoding="utf-8") as f:
        f.write(xml)

failed = 0

def check(ok, what):
    global failed
    print(("ok   " if ok else "FAIL ") + what)
    if not ok:
        failed += 1

def main():
    binary = os.path.abspath(sys.argv[1])
    work = tempfile.mkdtemp(prefix="ebac-acrostic-")
    try:
        src, root = os.path.join(work, "src"), os.path.join(work, "root")
        source(src)
        os.makedirs(root)
        here = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
        shutil.copytree(os.path.join(here, "AV"), os.path.join(root, "AV"))
        shutil.copy(os.path.join(here, "trans"), os.path.join(root, "trans"))
        env = dict(os.environ, BIBLEROOT=root, HOME=root)
        r = subprocess.run([binary, "--import", "thiagobodruk", "--source", src,
                            "--agree", "ACR-TB"], env=env, capture_output=True,
                           text=True, timeout=300)
        check("is ready" in r.stdout, "added: " + r.stdout.strip().splitlines()[-1])
        books = os.path.join(root, "ACR-TB")
        lines = {}
        for book in ("Psalms", "Lamentations"):
            for line in open(os.path.join(books, book), encoding="utf-8"):
                ref, _, text = line.rstrip("\n").partition("] ")
                lines[(book, ref[1:])] = text
        ps = lambda ref: lines[("Psalms", ref)]
        check(ps("119:1") == "\t- ALEPH.\tJoyful 1, who follow the Lord.",
              "119:1: Aleph's name, before the verse: " + repr(ps("119:1")))
        check(ps("119:8") == "Joyful 8, who follow the Lord.\t- Beth",
              "119:8: Beth, after the verse: " + repr(ps("119:8")))
        check(all(ps("119:%d" % (8 * k)).endswith("\t- " + NAMES[k]) for k in range(1, 22)),
              "every stanza's name, to Taw at 119:168")
        check(ps("119:176") == "Joyful 176, who follow the Lord.", "119:176 as it was")
        check(ps("120:1") == "Not an acrostic. Beth" and ps("120:2") == "He is here.",
              "Psalm 120, not an acrostic, as it was")
        lm = lambda ref: lines[("Lamentations", ref)]
        check(lm("1:1") == "\t- Aleph.\tText of verse 1.", "Lamentations 1:1: " + repr(lm("1:1")))
        check(lm("1:3") == "\t- Ghimel.\tText of verse 3.",
              "Lamentations 1:3, spelled otherwise: " + repr(lm("1:3")))
        check(lm("1:5") == "\t- He.\tHe hath built against me.",
              "Lamentations 1:5, the letter He, but not the pronoun after it: " + repr(lm("1:5")))
        check(all(lm("1:%d" % n).startswith("\t- ") for n in range(1, 23)),
              "every verse of Lamentations 1 starts with its letter, set apart")

        # The passage screen: Aleph's note, then 119:1 with its number
        s = Session(binary, root, 24, 49)
        s.key("c")
        num = None
        for row in s.text().splitlines():
            parts = row.split()
            if len(parts) > 1 and parts[1] == "ACR-TB":
                num = parts[0].rstrip(")")
        s.key(num + "\r")
        s.key("v")
        s.key("psalm 119:1\r")
        time.sleep(0.3)
        s.drain()
        rows = s.text().splitlines()
        if os.environ.get("ACROSTIC_KEEP"):
            print("\n".join(rows))
        at = [i for i, row in enumerate(rows) if row.startswith("---") and "ALEPH." in row]
        check(bool(at) and rows[at[0] + 1].startswith("119:1   Joyful 1,"),
              "the screen: \"---  ALEPH.\", then 119:1 beside its words")
        os.kill(s.pid, 9)
    finally:
        if os.environ.get("ACROSTIC_KEEP"):     # (to look at, when one fails)
            print("kept: " + work)
        else:
            shutil.rmtree(work, ignore_errors=True)
    print("acrostic: %s" % ("all passed" if failed == 0 else "%d FAILED" % failed))
    sys.exit(1 if failed else 0)

main()
