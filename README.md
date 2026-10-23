# EBaC — Electronic Bible and Concordance

Written by Neil G. Brookins.

EBaC is a fast, text-mode Bible reader and concordance. It has two main jobs:

- **Bible:** type a book, chapter and verse, and read from there, paging forward or back a verse, a screen, or a book at a time.
- **Concordance:** type a word and get every verse that contains it, with the word highlighted. Then pick any verse to read it in context.

Unlike a printed concordance, EBaC shows complete verses. It indexes every word, even common ones like *I* and *am*. Searches can combine words and books with AND, OR, AND NOT and proximity ("within 2 verses"). The King James (Authorized) Version is included.

## Building and running on Linux

You need a C compiler, `make`, and the ncurses development files.

```sh
# Debian, Ubuntu, Raspberry Pi OS:
sudo apt install build-essential libncurses-dev
# Fedora:  sudo dnf install gcc make ncurses-devel

git clone https://github.com/neilbrookins/ebac.git
cd ebac
make
export BIBLEROOT=$PWD      # where the 'trans' file and the AV folder are
./bible
```

Add the `export BIBLEROOT=...` line to your `~/.bashrc` to make it permanent.

At the main menu press **V** to view a passage, **S** to search, **C** to change translation, **I** for information, or **E** to exit. In any list or passage, press **?** for the keys available there.

## Searching

Searches use postfix (reverse Polish) notation. Put the operator after the two things it combines:

| Search | Finds |
|---|---|
| `amen` | every verse containing "amen" |
| `love hate 0` | verses with both "love" and "hate" |
| `love hate 2` | "love" and "hate" within 2 verses of each other |
| `amen @rev 0` | "amen" in the book of Revelation |
| `food @gen -` | "food" anywhere except Genesis |
| `@mat @mar , @lu , @joh , blessed 0` | "blessed" in the four Gospels |

Operators: a number for AND (`0` = same verse; `2` = within two verses), `,` for OR, `-` for AND NOT. Books are written `@` plus enough of the name to be unique. If a search is malformed, EBaC tells you what is wrong. **CTRL-G** recalls earlier searches; they are kept in `~/.ebac_log`.

See [`docs`](docs) for more examples.

## Adding other translations

Put the text files for a translation in a new folder under `$BIBLEROOT`. Add a line for it to the `trans` file, then build its index:

```sh
cd $BIBLEROOT/NIV && ../index
```

The original [`README`](README) describes the file layout.

## History, and building on vintage computers

EBaC dates from 1993. Up to 2014 it ran on the Tandy Color Computer 3 (OS-9 6809, 512K), the MM/1 (OS-9 68000), VAX/VMS, MS-DOS, and the UNIX systems of the 1990s. Version 1.3.0 was published on [SourceForge](https://sourceforge.net/projects/ebac/) in 2009.

This repository starts with that release, unchanged, as its first commit and tagged **`v1.3.0`**. Each step needed to bring it up to date is a separate commit after it, with an explanation. Run `git log v1.3.0..` to read them.

**To build for a vintage system, use the original release:**

```sh
git checkout v1.3.0
```

Version 2.0 is written in ANSI C (C89, and also valid C99, C17 and C23), which the CoCo's Microware C compiler cannot compile. The OS-9 68000, VMS and MS-DOS code is still in the source behind its `#ifdef`s and may still build with those systems' ANSI compilers, but only the Linux build has been tested.

## What changed in 2.0

Modern compilers (GCC 14 and later, Clang 16 and later, and the C23 standard) reject the 1980s K&R style C the program was written in. The termcap library and `<sys/termio.h>` it used are gone from current Linux systems. Version 2.0:

- uses the **ncurses** library (its terminfo interface) instead of termcap, and POSIX `termios` instead of System V `termio`;
- has ANSI function prototypes and definitions throughout;
- fixes bugs that newer compilers and 64-bit, ARM (Raspberry Pi) systems exposed, plus a crash when `LOGNAME` is unset and an exit on malformed searches;
- follows the terminal window when it is resized.

The concordance index built on 64-bit Linux is byte-for-byte identical to the one shipped in 1.3.0.

## License

GNU General Public License, version 2 — the license the project has on SourceForge. See [`COPYING`](COPYING).
