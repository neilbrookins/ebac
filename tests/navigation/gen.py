"""Make a test translation, TT, whose every word says where it is:
a word is BOOK c CHAPTER v VERSE n K (such as a1c0v2n17), so a screen
shows exactly which verses, and which words of them, it holds."""
import os, sys
root = sys.argv[1]
SHORT, TALL = 12, 250          # words (a tall verse: several screens)
books = [
  ("Alpha", [(0, [SHORT, TALL]),                     # chapter 0: front matter
             (1, [TALL] + [SHORT]*5 + [TALL, SHORT]),  # 1:0 tall, 1:6 tall
             (2, [SHORT, SHORT, TALL])]),           # 2:3 tall: the book's last
  ("Beta",  [(1, [TALL, TALL] + [SHORT]*8)]),       # 1:1, 1:2 tall: the book's first
  ("Gamma", [(1, [SHORT]*20 + [TALL])]),            # 1:21 tall: the last of all
]
d = os.path.join(root, "TT")
os.makedirs(d, exist_ok=True)
with open(os.path.join(d, "booklist"), "w") as bl:
    for bi, (name, chapters) in enumerate(books, 1):
        bl.write(name + "\n")
        with open(os.path.join(d, name), "w") as f:
            for c, verses in chapters:
                first = 0 if (c == 0 or (c == 1 and name == "Alpha")) else 1
                if c == 0: first = 1
                for k, n in enumerate(verses):
                    v = k + first
                    words = " ".join("a%dc%dv%dn%d" % (bi, c, v, j) for j in range(1, n + 1))
                    f.write("[%d:%d] %s\n" % (c, v, words))
with open(os.path.join(root, "trans"), "w") as t:
    t.write("TT  Navigation test\n")
