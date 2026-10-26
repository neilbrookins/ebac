#!/usr/bin/env python3
"""add-translation.py -- download a Bible translation and add it to EBaC.

    python3 add-translation.py             list the English translations,
                                           and ask which to add
    python3 add-translation.py BSB LEB     add these (by short name)
    python3 add-translation.py --list      just list them

The translations come from the plain-text Bibles of the bible_databases
project (https://github.com/scrollmapper/bible_databases), made from
CrossWire's SWORD modules. Each is one file: a "### Book" heading for each
book, then one verse per line, "[1:1] In the beginning ...". For each
translation chosen, this:

  1. downloads it;
  2. splits it into a file per book, in a folder named after it (such as
     BSB), and writes the folder's booklist: the books, in order;
  3. builds its concordance index with make (translation.mk), and checks
     that the index has every verse;
  4. adds it to 'trans', so that EBaC offers it under C)hange translation.

Book names are the translation's own, with spaces and punctuation left out
(EBaC's book prompt, its @book searches and the makefile take one word),
and cut short to fit the book column of EBaC's list of search results (15
characters; 14 for one such as "1Samuel", shown as "1 Samuel"). Books with
no text (the Old Testament of a New Testament translation) are left out.
Typographic punctuation becomes plain ASCII (curly quotes, dashes), as in
EBaC's own text, so that searches find words next to it; other characters
stay as they are. Verse numbers keep their brackets: EBaC reads [1:1] as
well as 1:1.

Translations with more verses than EBaC can number (32767), such as those
with the Apocrypha, aren't offered.

Needs only Python 3 (3.6 or later), and the tools that build EBaC (make).
"""

import argparse
import os
import re
import shutil
import subprocess
import sys
import urllib.error
import urllib.request

SOURCE = "https://raw.githubusercontent.com/scrollmapper/bible_databases/master"
LIST_PATH = "docs/main_readme/translation_list.md"
TEXT_PATH = "formats/txt/%s.txt"

MAX_VERSES = 32767      # EBaC numbers verses with 16 bits
# More verses than that (they include the Apocrypha); not offered for now
TOO_LARGE = {"KJVA", "DRC", "CPDV"}

# Typographic punctuation, as plain ASCII
PUNCTUATION = {
    "‘": "'", "’": "'",           # curly single quotes
    "“": '"', "”": '"',           # curly double quotes
    "—": "--", "―": "--",         # em dash, horizontal bar
    "–": "-",                          # en dash
    "…": "...",                        # ellipsis
    "¶": "",                           # pilcrow (paragraph mark)
}

HERE = os.path.dirname(os.path.abspath(__file__))


class Problem(Exception):
    """Something went wrong with one translation; the message says what."""


def fetch(source, path):
    """The text of a file of the source: a web address or a local folder."""
    if re.match(r"^https?://", source):
        url = source.rstrip("/") + "/" + path
        try:
            with urllib.request.urlopen(url, timeout=60) as reply:
                return reply.read().decode("utf-8")
        except urllib.error.HTTPError as err:
            raise Problem("couldn't download %s (%s)" % (url, err.code))
        except (urllib.error.URLError, OSError) as err:
            raise Problem("couldn't download %s (%s)" % (url, getattr(err, "reason", err)))
    try:
        with open(os.path.join(source, path), encoding="utf-8") as f:
            return f.read()
    except OSError as err:
        raise Problem("couldn't read %s (%s)" % (os.path.join(source, path), err.strerror))


def english_translations(source):
    """[(short name, title)], from the source's list of translations."""
    found = []
    for line in fetch(source, LIST_PATH).splitlines():
        # - **BSB (en)**: BSB: Berean Standard Bible
        m = re.match(r"^- \*\*(\S+) \(en\)\*\*: (?:\S+: )?(.*)$", line)
        if m and m.group(1) not in TOO_LARGE:
            found.append((m.group(1), plain(m.group(2))))
    if not found:
        raise Problem("found no English translations in the list at %s" % source)
    return found


def listed_translations():
    """The folders 'trans' lists, by name: {name: line}."""
    names = {}
    try:
        with open(os.path.join(HERE, "trans")) as f:
            for line in f:
                words = line.split()
                if words and not words[0].startswith("#"):
                    names[words[0]] = line
    except OSError:
        pass
    return names


def book_name(name, taken):
    """EBaC's name for a book: one word, short enough for its list."""
    word = "".join(ch for ch in name if ch.isalnum() and ord(ch) < 128) or "Book"
    # a name like 1Samuel is shown as "1 Samuel", one character wider
    limit = 14 if re.match(r"^[0-9][A-Z][a-z]", word) else 15
    word = word[:limit]
    if word in taken:                       # (made the same by shortening)
        for n in range(2, 10):
            other = word[:limit - 1] + str(n)
            if other not in taken:
                word = other
                break
    taken.add(word)
    return word


def plain(text):
    for fancy, simple in PUNCTUATION.items():
        text = text.replace(fancy, simple)
    return " ".join(text.split())


def split_books(text):
    """[(book name, [(chapter, verse, text)])], from a translation's text."""
    books = []
    verses = None
    for number, line in enumerate(text.splitlines(), 1):
        if line.startswith("### "):
            verses = []
            books.append((line[4:].strip(), verses))
            continue
        if not line.strip():
            continue
        m = re.match(r"^\[(\d+):(\d+)\] ?(.*)$", line)
        if not m or verses is None:
            raise Problem("line %d isn't a book heading or a verse: %.60s" % (number, line))
        verses.append((int(m.group(1)), int(m.group(2)), plain(m.group(3))))
    return books


def check_book(name, verses):
    """EBaC finds a chapter by its verse 1: every chapter must have one."""
    chapter = None
    for c, v, _ in verses:
        if c != chapter:
            if v != 1:
                raise Problem("%s %d starts at verse %d, not 1" % (name, c, v))
            chapter = c


def index_verses(folder):
    """How many verses the index found: the last number in bvarray.dat."""
    with open(os.path.join(folder, "bvarray.dat"), "rb") as f:
        data = f.read()
    return (data[-4] << 8) | data[-3]


def make_command():
    # GNU make is called gmake on Solaris and the BSDs
    return shutil.which("gmake") or shutil.which("make") or "make"


def add(name, title, source):
    """Add one translation; on failure, leave no half-made folder behind
    (unless it was there, and listed in trans, before)."""
    folder = os.path.join(HERE, name)
    was_listed = name in listed_translations()
    try:
        add_to(folder, name, title, source)
    except Problem:
        if not was_listed and os.path.isdir(folder):
            shutil.rmtree(folder)
        raise


def add_to(folder, name, title, source):
    print("%s: downloading..." % name)
    books = split_books(fetch(source, TEXT_PATH % name))
    books = [(n, v) for n, v in books if any(t for _, _, t in v)]   # with text
    if not books:
        raise Problem("it has no text")
    total = sum(len(v) for _, v in books)
    if total > MAX_VERSES:
        raise Problem("it has %d verses, more than EBaC can number (%d)" % (total, MAX_VERSES))
    for n, v in books:
        check_book(n, v)

    print("%s: writing %d books into the folder %s..." % (name, len(books), name))
    if os.path.isdir(folder):
        shutil.rmtree(folder)
    os.makedirs(folder)
    taken = set()
    names = []
    for n, verses in books:
        file_name = book_name(n, taken)
        names.append(file_name)
        with open(os.path.join(folder, file_name), "w", encoding="utf-8", newline="\n") as f:
            for c, v, t in verses:
                f.write(("[%d:%d] %s\n" % (c, v, t)) if t else ("[%d:%d]\n" % (c, v)))
    with open(os.path.join(folder, "booklist"), "w", newline="\n") as f:
        f.write("".join(n + "\n" for n in names))

    print("%s: building the concordance index..." % name)
    make = make_command()
    index = os.path.join(HERE, "index")
    if subprocess.call([make, "-C", HERE, "index"], stdout=subprocess.DEVNULL) != 0:
        raise Problem("couldn't build EBaC's index program (%s index)" % make)
    if subprocess.call([make, "--no-print-directory", "-C", folder,
                        "-f", os.path.join(HERE, "translation.mk"), "INDEX=" + index]) != 0:
        raise Problem("building the index failed")
    found = index_verses(folder)
    if found != total:
        raise Problem("the index has %d verses, but the text has %d" % (found, total))

    listed = listed_translations()
    if name not in listed:
        with open(os.path.join(HERE, "trans"), "a") as f:
            f.write("%-11s %s\n" % (name, title))
    print("%s: done. %s is ready: start EBaC and press C to choose it." % (name, title))


def main():
    parser = argparse.ArgumentParser(
        description="Download Bible translations and add them to EBaC.")
    parser.add_argument("names", nargs="*", help="short names of translations to add, such as BSB")
    parser.add_argument("--list", action="store_true", help="only list the translations")
    parser.add_argument("--source", default=SOURCE,
                        help="where the translations are: a web address, or a "
                             "folder with a copy of bible_databases (default: %(default)s)")
    args = parser.parse_args()

    try:
        available = english_translations(args.source)
    except Problem as err:
        sys.exit("add-translation: " + str(err))
    titles = dict(available)
    listed = listed_translations()

    if args.list or not args.names:
        print("English translations that can be added to EBaC:\n")
        for i, (name, title) in enumerate(available, 1):
            mark = "  (added)" if name in listed else ""
            print("%3d) %-12s %s%s" % (i, name, title, mark))
        print()
        if args.list:
            return
        try:
            answer = input("Type the numbers or names of those to add, "
                           "or press Enter to stop: ")
        except EOFError:
            answer = ""
        for word in answer.replace(",", " ").split():
            if word.isdigit() and 1 <= int(word) <= len(available):
                args.names.append(available[int(word) - 1][0])
            else:
                args.names.append(word)
        if not args.names:
            return

    failed = 0
    by_lower = {n.lower(): n for n in titles}
    for word in args.names:
        name = by_lower.get(word.lower())
        if name is None:
            print("%s: not in the list of English translations." % word)
            if word.upper() in TOO_LARGE:
                print("    (It has more verses than EBaC can number.)")
            failed += 1
            continue
        try:
            add(name, titles[name], args.source)
        except Problem as err:
            print("%s: not added: %s." % (name, err))
            failed += 1
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
