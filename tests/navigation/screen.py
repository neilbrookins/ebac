"""A small VT100 emulator: what a terminal would show (from the tests' pty
driver). Reverse video and underline are kept as marks in each cell."""
import os, pty, sys, time, select, struct, fcntl, termios, re

def read_all(fd, wait=0.6):
    out = b""
    end = time.time() + wait
    while True:
        r, _, _ = select.select([fd], [], [], max(0, end - time.time()))
        if not r:
            break
        try:
            chunk = os.read(fd, 65536)
        except OSError:
            break
        if not chunk:
            break
        out += chunk
        end = time.time() + 0.25
    return out

class Screen:
    def __init__(s, rows, cols):
        s.rows, s.cols = rows, cols
        s.clear()
        s.rev = s.ul = False
        s.log = []
    def clear(s):
        s.g = [[" "] * s.cols for _ in range(s.rows)]
        s.r = s.c = 0
    def scroll(s):
        s.g.pop(0); s.g.append([" "] * s.cols)
    def put(s, ch):
        if s.c >= s.cols:
            s.c = 0; s.nl()
        if s.rev: ch = "\x01" + ch
        elif s.ul: ch = "\x02" + ch
        s.g[s.r][s.c] = ch; s.c += 1
    def nl(s):
        s.r += 1
        if s.r >= s.rows:
            s.r = s.rows - 1; s.scroll()
    def feed(s, data):
        t = data.decode("latin1")
        i = 0
        while i < len(t):
            ch = t[i]
            if ch == "\x1b":
                m = re.match(r"\x1b\[([0-9;?]*)([A-Za-z@])", t[i:])
                if m:
                    s.csi(m.group(1), m.group(2)); i += m.end(); continue
                m = re.match(r"\x1b[()][0-9A-Za-z]|\x1b[=>78]", t[i:])
                if m:
                    i += m.end(); continue
                s.log.append("unknown ESC " + repr(t[i:i+6])); i += 1; continue
            if ch == "\r": s.c = 0
            elif ch == "\n": s.nl()
            elif ch == "\b": s.c = max(0, s.c - 1)
            elif ch == "\x07": s.log.append("BEL")
            elif ch == "\x0f" or ch == "\x0e": pass
            elif ord(ch) >= 32: s.put(ch)
            else: s.log.append("ctl %r" % ch)
            i += 1
    def csi(s, args, f):
        a = [int(x) if x.isdigit() else 0 for x in args.lstrip("?").split(";")] if args else []
        n = (a[0] if a and a[0] else 1)
        if f == "H":
            s.r = (a[0] - 1 if a and a[0] else 0); s.c = (a[1] - 1 if len(a) > 1 and a[1] else 0)
        elif f == "J":
            if (a[0] if a else 0) == 2:
                s.g = [[" "] * s.cols for _ in range(s.rows)]; s.log.append("CLEAR")
            elif (a[0] if a else 0) == 0:
                if s.r == 0 and s.c == 0: s.log.append("CLEAR")
                s.g[s.r][s.c:] = [" "] * (s.cols - s.c)
                for r in range(s.r + 1, s.rows): s.g[r] = [" "] * s.cols
        elif f == "K": s.g[s.r][s.c:] = [" "] * (s.cols - s.c)
        elif f == "C": s.c = min(s.cols - 1, s.c + n)
        elif f == "D": s.c = max(0, s.c - n)
        elif f == "A": s.r = max(0, s.r - n)
        elif f == "B": s.r = min(s.rows - 1, s.r + n)
        elif f == "m":
            for v in (a or [0]):
                if v == 0: s.rev = s.ul = False
                elif v == 7: s.rev = True
                elif v == 27: s.rev = False
                elif v == 4: s.ul = True
                elif v == 24: s.ul = False
        elif f in "hlr": pass
        else: s.log.append("unknown CSI %r%s" % (args, f))
    def dump(s):
        lines = []
        for row in s.g:
            out, mode = "", None
            for cell in row:
                m = cell[0] if len(cell) == 2 else None
                if m != mode:
                    if mode == "\x01": out += "»"
                    if mode == "\x02": out += "_"
                    if m == "\x01": out += "«"
                    if m == "\x02": out += "_"
                    mode = m
                out += cell[-1]
            if mode == "\x01": out += "»"
            if mode == "\x02": out += "_"
            lines.append(out.rstrip())
        while lines and not lines[-1]: lines.pop()
        return "\n".join("|" + l for l in lines)

