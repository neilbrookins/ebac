"""Every navigation key from every screen of the passage view, near and on
verses taller than the screen (TT, from gen.py), checked against what it
should do. usage: check.py ROWS COLS [binary]"""
import sys, re, os
here = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, here)
from nav import Session
rows, cols = int(sys.argv[1]), int(sys.argv[2])
binary = os.path.abspath(sys.argv[3] if len(sys.argv) > 3 else "bible")
# (run in the folder TT was made in: gen.py, then index; see the makefile)

# the verses, in order, and their words
verses = []                          # (book, chapter, verse, words)
for bi, name in enumerate(open("TT/booklist").read().split(), 1):
    for line in open("TT/" + name):
        m = re.match(r"\[(\d+):(\d+)\] (.*)", line)
        verses.append((bi, int(m.group(1)), int(m.group(2)), len(m.group(3).split())))
index = {v[:3]: i for i, v in enumerate(verses)}
first_of_book = {}
for i, v in enumerate(verses):
    first_of_book.setdefault(v[0], i)

def start():
    s = Session(binary, ".", rows, cols)
    s.key("v"); s.key("alpha\r"); s.key("0\r")
    return s, s.key("1\r")[0]

def desc(screen):
    return " ".join("%d.%d:%d[%d-%d]" % x for x in screen) or "(nothing)"

# the screens n goes through, from the first verse to the last
s, st = start()
pages = [st]
while True:
    nxt, bel = s.key("n")
    if bel:
        break
    pages.append(nxt)
s.close()
print("%dx%d: %d screens forward with n" % (rows, cols, len(pages)))

def whole(item):
    i = index[item[:3]]
    return item[3] == 1 and item[4] == verses[i][3]

def top(screen):
    """the verse at the top (in order) and its first word shown: after '.'
    scrolled the screen, the first verse wholly on it"""
    if not screen:
        return (-1, -1)
    item = screen[0]
    if len(screen) > 1 and item[3] > 1:
        item = screen[1]
    return index[item[:3]], item[3]

def last(screen):
    if not screen:
        return (-1, -1)
    return index[screen[-1][:3]], screen[-1][4]

def expect(key, screen):
    """what the screen after key should be: a test of it, and words"""
    t, tw = top(screen)
    l, lw = last(screen)
    tall_part = len(screen) == 1 and not whole(screen[0])
    if key == "n":
        if l == len(verses) - 1 and lw == verses[l][3]:
            return "beep", None
        if lw < verses[l][3]:          # the rest of a tall verse
            return lambda r: top(r) == (l, lw + 1), "the rest of verse %d from word %d" % (l, lw + 1)
        return lambda r: top(r) == (l + 1, 1), "verse %d from its start" % (l + 1)
    if key == "p":
        if (t, tw) == (0, 1):
            return "beep", None
        if tw > 1:                     # the part before, of a tall verse
            return lambda r: last(r) == (t, tw - 1), "verse %d up to word %d" % (t, tw - 1)
        # (back a screen: verse t - 1 shown to its end; the screen may go on
        # past it when a tall verse before stops it going further back)
        return (lambda r: top(r)[0] < t and any(index[x[:3]] == t - 1 and
                x[4] == verses[t - 1][3] for x in r)), "a screen with verse %d's end, before verse %d" % (t - 1, t)
    if key == ".":
        if lw < verses[l][3]:
            return lambda r: top(r) == (l, lw + 1), "the rest of verse %d" % l
        if l == len(verses) - 1:
            return "end", None
        return (lambda r: any(index[x[:3]] == l + 1 and x[3] == 1 for x in r)
                and last(r)[0] == l + 1), "verse %d added" % (l + 1)
    if key == ",":
        if (t, tw) == (0, 1):
            return "beep", None
        if tw > 1:
            return lambda r: top(r)[0] == t and top(r)[1] < tw, "an earlier part of verse %d" % t
        return lambda r: top(r) == (t - 1, 1), "verse %d from its start" % (t - 1)
    if key == "f":
        b = verses[t][0]
        if b + 1 not in first_of_book:
            return "beep", None
        return lambda r: top(r) == (first_of_book[b + 1], 1), "book %d's first verse" % (b + 1)
    if key == "b":
        b = verses[t][0]
        if (t, tw) == (first_of_book[b], 1):
            if b - 1 not in first_of_book:
                return "beep", None
            return lambda r: top(r) == (first_of_book[b - 1], 1), "book %d's first verse" % (b - 1)
        return lambda r: top(r) == (first_of_book[b], 1), "book %d's first verse" % b

failures = 0
cases = 0
for i, page in enumerate(pages):
    for key in "nNp.,fb".replace("N", ""):
        for before in ("", "."):      # from the screen, and after a step with '.'
            s, st = start()
            for _ in range(i):
                st = s.key("n")[0]
            if before:
                st, bel = s.key(".")
                if bel:
                    s.close(); continue
            here = st
            test, what = expect(key, here)
            got, bel = s.key(key)
            s_text = s.text()
            s.close()
            cases += 1
            if test == "beep":
                ok = bel and got == here
            elif test == "end":           # ('.' at the last verse: says so)
                ok = got == here and "at the END" in s_text
            else:
                ok = not bel and test(got)
            if not ok:
                failures += 1
                print("FAIL from %s%s: %r should give %s; gave %s%s" % (
                    desc(here), " (after '.')" if before else "", key,
                    {"beep": "a beep", "end": "'at the END'"}.get(test, what) if isinstance(test, str) else what, desc(got), " BEEP" if bel else ""))
print("%d of %d cases right" % (cases - failures, cases))

# Sweeps: from the end, p and ',' back to the start; from the start, '.' to
# the end: each key moving every time, and every word of every verse seen
def sweep(keys_to_start, key, limit=400):
    s, st = start()
    for k in keys_to_start:
        st, _ = s.key(k)
    seen = set()
    for x in st:
        seen.update((x[:3], n) for n in range(x[3], x[4] + 1))
    moves = 0
    while moves < limit:
        nxt, bel = s.key(key)
        if bel or "at the END" in s.text():
            break
        if nxt == st:
            print("FAIL sweep %r: stuck at %s" % (key, desc(st)))
            break
        st = nxt
        moves += 1
        for x in st:
            seen.update((x[:3], n) for n in range(x[3], x[4] + 1))
    s.close()
    missing = [v for v in verses for n in range(1, v[3] + 1) if (v[:3], n) not in seen]
    where = desc(st)
    print("sweep %r: %d moves, ended at %s; %s" % (key, moves, where[:60],
          "every word seen" if not missing else "%d words never seen, as %s" % (len(missing), missing[:3])))
    return st

end_keys = "n" * (len(pages) - 1)
sweep("", "n")
sweep(end_keys, "p")
sweep("", ".")
sweep(end_keys, ",")   # (',' shows each verse from its start: a tall
                       # verse's later parts aren't reached by it)

# and every key from each screen p goes through, back from the end
s, st = start()
for k in end_keys:
    st = s.key(k)[0]
back = [st]
while True:
    nxt, bel = s.key("p")
    if bel:
        break
    back.append(nxt)
s.close()
for i, page in enumerate(back):
    for key in "np.,fb":
        s, st = start()
        for k in end_keys + "p" * i:
            st = s.key(k)[0]
        here = st
        test, what = expect(key, here)
        got, bel = s.key(key)
        s_text = s.text()
        s.close()
        cases += 1
        if test == "beep":
            ok = bel and got == here
        elif test == "end":
            ok = got == here and "at the END" in s_text
        else:
            ok = not bel and test(got)
        if not ok:
            failures += 1
            print("FAIL from %s (back with p): %r should give %s; gave %s%s" % (
                desc(here), key, what if not isinstance(test, str) else test, desc(got), " BEEP" if bel else ""))
print("in all, %d of %d cases right" % (cases - failures, cases))
sys.exit(1 if failures else 0)
