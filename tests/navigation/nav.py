"""Drive EBaC's passage screen in a pty and say what each screen shows.

A screen is described by the words of TT on it (see gen.py): for each
verse shown, its reference and the range of its words, in order. So
whether a key moved, and where, can be checked exactly."""
import os, pty, re, select, struct, fcntl, termios, time, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from screen import Screen

WORD = re.compile(r"a(\d+)c(\d+)v(\d+)n(\d+)")

class Session:
    def __init__(self, binary, root, rows, cols):
        self.rows, self.cols = rows, cols
        env = dict(os.environ, TERM="xterm", BIBLEROOT=root, HOME=root)
        self.pid, self.fd = pty.fork()
        if self.pid == 0:
            os.execve(binary, [binary], env)
        fcntl.ioctl(self.fd, termios.TIOCSWINSZ, struct.pack("HHHH", rows, cols, 0, 0))
        self.scr = Screen(rows, cols)
        self.drain()
    def waiting(self):
        try:
            with open("/proc/%d/stat" % self.pid) as f:
                return f.read().rsplit(")", 1)[1].split()[0] in ("S", "Z")
        except OSError:
            return True
    def drain(self):
        end = time.time() + 5
        while time.time() < end:
            r, _, _ = select.select([self.fd], [], [], 0.03)
            if r:
                try:
                    data = os.read(self.fd, 65536)
                except OSError:
                    return
                self.scr.feed(data)
            elif self.waiting():
                return
    def key(self, k):
        self.scr.log = []
        for ch in k.encode("latin1"):
            os.write(self.fd, bytes([ch]))
            time.sleep(0.005)
            self.drain()
        return self.state()
    def text(self):
        return "\n".join("".join(c[-1] for c in row) for row in self.scr.g)
    def state(self):
        """[(book, chapter, verse, first word, last word), ...] in screen
        order; and whether it beeped"""
        out = []
        for m in WORD.finditer(self.text()):
            b, c, v, n = map(int, m.groups())
            if out and out[-1][:3] == (b, c, v):
                out[-1] = (b, c, v, out[-1][3], n)
            else:
                out.append((b, c, v, n, n))
        return out, "BEL" in self.scr.log
    def close(self):
        try:
            os.kill(self.pid, 9); os.waitpid(self.pid, 0)
        except OSError:
            pass
