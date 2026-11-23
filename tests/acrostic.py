"""The Hebrew alphabet's letters set apart (acrostic.c), end to end.

A made-up translation, from a made-up copy of thiagobodruk/bible, is added
by the importer as anyone's would be, and its books are checked: Psalm
119's stanzas' names (at the ends of verses 8, 16, ... 168, and Aleph
before verse 1) each a note of its own; Lamentations 1, an acrostic of
names before each verse, spelled differently from Psalm 119's, with a
verse starting with "He" (the pronoun) right after one; a chapter that
isn't an acrostic left as it is, though a verse in it ends with a name.
The Song of Songs' speakers (speakers.c), set apart as headings too, at
a verse's end and within one; "Interlude" and a story's "He said:" not.
Then the passage screen: Aleph's heading first, with "===", and 119:1's
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
    ps120 = (verse(1, "Not an acrostic. Beth") + verse(2, "He is here.") +
             "".join(verse(n, "Sing to the Lord. Interlude") for n in range(3, 9)))
    lam = []
    for n in range(1, 23):
        t = "%s. Text of verse %d." % (LAM[n - 1], n)
        if n == 5:
            t = "He. He hath built against me."
        lam.append(verse(n, t))
    # The Song of Songs' speakers (speakers.c), as the NLT and the World
    # English Bible print them: at a verse's end, naming who speaks next,
    # one with a note run on into it (the NLT's 1:1), one in the middle
    # of a verse; two chapters, ten labels, three speakers
    song = [
        [verse(1, "Solomon's song. Young Woman#1:1 The headings are not in the original text."),
         verse(2, "Kiss me again."),
         verse(3, "Take me with you. Young Women of Jerusalem We will be glad. Young Woman They are right."),
         verse(4, "I am dark but beautiful. Young Man"),
         verse(5, "You are beautiful, my darling. Young Women of Jerusalem"),
         verse(6, "You are handsome, my love. Young Man"),
         verse(7, "Like a lily among thistles."),
         verse(8, "No label here, but a Name")],
        [verse(1, "I am the spring crocus. Young Man"),
         verse(2, "Like a lily among thistles. Young Woman"),
         verse(3, "Like the finest apple tree. Young Man"),
         verse(4, "Arise, my darling. The Young Woman's Brothers"),
         verse(5, "Catch all the foxes. Young Woman"),
         verse(6, "My lover is mine.")]]
    song_xml = "".join('<c n="%d">%s</c>' % (c + 1, "".join(vs)) for c, vs in enumerate(song))
    # A story told with "He said:" and the like (not a cast of speakers:
    # many, each found a time or two), and Psalms' "Interlude" (one label,
    # never changing), left as they are: in Psalm 120 below, and here
    story = "".join(verse(n, t) for n, t in enumerate([
        "He said: Go now.", "Joab said: I will.", "The king said: Stay.",
        "He said: Come.", "David said to him: Why?", "She said: Here."], 1))
    xml = ('<?xml version="1.0" encoding="utf-8"?><bible>'
           '<b id="2sm" n="2 Samuel"><c n="1">%s</c></b>'
           '<b id="sl" n="Psalms">%s<c n="118">%s</c><c n="119">%s</c><c n="120">%s</c></b>'
           '<b id="ct" n="Song of Songs">%s</b>'
           '<b id="lm" n="Lamentations"><c n="1">%s</c></b></bible>'
           % (story, before, ps118, "".join(ps119), ps120, song_xml, "".join(lam)))
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
        check(ps("119:1") == "\t= ALEPH.\tJoyful 1, who follow the Lord.",
              "119:1: Aleph's name, before the verse: " + repr(ps("119:1")))
        check(ps("119:8") == "Joyful 8, who follow the Lord.\t= Beth",
              "119:8: Beth, after the verse: " + repr(ps("119:8")))
        check(all(ps("119:%d" % (8 * k)).endswith("\t= " + NAMES[k]) for k in range(1, 22)),
              "every stanza's name, to Taw at 119:168")
        check(ps("119:176") == "Joyful 176, who follow the Lord.", "119:176 as it was")
        check(ps("120:1") == "Not an acrostic. Beth" and ps("120:2") == "He is here.",
              "Psalm 120, not an acrostic, as it was")
        check(ps("120:3") == "Sing to the Lord. Interlude",
              "Psalm 120's \"Interlude\" (one label, never changing): left in its verse")
        lm = lambda ref: lines[("Lamentations", ref)]
        check(lm("1:1") == "\t= Aleph.\tText of verse 1.", "Lamentations 1:1: " + repr(lm("1:1")))
        check(lm("1:3") == "\t= Ghimel.\tText of verse 3.",
              "Lamentations 1:3, spelled otherwise: " + repr(lm("1:3")))
        check(lm("1:5") == "\t= He.\tHe hath built against me.",
              "Lamentations 1:5, the letter He, but not the pronoun after it: " + repr(lm("1:5")))
        check(all(lm("1:%d" % n).startswith("\t= ") for n in range(1, 23)),
              "every verse of Lamentations 1 starts with its letter, set apart")

        # The Song of Songs' speakers, set apart as headings
        song = {}
        for line in open(os.path.join(books, "SongofSongs"), encoding="utf-8"):
            ref, _, text = line.rstrip("\n").partition("] ")
            song[ref[1:]] = text
        sg = lambda ref: song.get(ref)
        check(sg("1:1") == "Solomon's song.\t= Young Woman\t- 1:1 The headings are not in the original text.",
              "Song 1:1: the label, then the note run on into it: " + repr(sg("1:1")))
        check(sg("1:3") == "Take me with you.\t= Young Women of Jerusalem\tWe will be glad.\t= Young Woman\tThey are right.",
              "Song 1:3: labels in the middle of a verse: " + repr(sg("1:3")))
        check(sg("1:4") == "I am dark but beautiful.\t= Young Man", "Song 1:4: " + repr(sg("1:4")))
        check(sg("2:4") == "Arise, my darling.\t= The Young Woman's Brothers",
              "Song 2:4: a label found once, in a book with a cast: " + repr(sg("2:4")))
        check(sg("1:8") == "No label here, but a Name",
              "Song 1:8: words after a comma, not after a clause's end: " + repr(sg("1:8")))
        story = {}
        for line in open(os.path.join(books, "2Samuel"), encoding="utf-8"):
            ref, _, text = line.rstrip("\n").partition("] ")
            story[ref[1:]] = text
        check(story.get("1:1") == "He said: Go now." and story.get("1:2") == "Joab said: I will.",
              "2 Samuel's \"He said:\" and the like, not a cast: as they were")

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
        at = [i for i, row in enumerate(rows) if row.startswith("===") and "ALEPH." in row]
        check(bool(at) and rows[at[0] + 1].startswith("119:1   Joyful 1,"),
              "the screen: \"===  ALEPH.\", then 119:1 beside its words")
        os.kill(s.pid, 9)
    finally:
        if os.environ.get("ACROSTIC_KEEP"):     # (to look at, when one fails)
            print("kept: " + work)
        else:
            shutil.rmtree(work, ignore_errors=True)
    print("acrostic: %s" % ("all passed" if failed == 0 else "%d FAILED" % failed))
    sys.exit(1 if failed else 0)

main()
