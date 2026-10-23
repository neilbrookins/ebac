# EBaC — Electronic Bible and Concordance

Written by Neil G. Brookins.

EBaC is a fast, text-mode Bible reader and concordance. It has two main jobs:

- **Bible:** type a book, chapter and verse, and read from there, paging forward or back a verse, a screen, or a book at a time.
- **Concordance:** type a word and get every verse that contains it, with the word highlighted. Then pick any verse to read it in context.

Unlike a printed concordance, EBaC shows complete verses. It indexes every word, even common ones like *I* and *am*. Searches can combine words and books with AND, OR, AND NOT and proximity ("within 2 verses"). The King James (Authorized) Version is included.

## Installing on Linux

Not sure which steps you need? Open a terminal and type `ls ~/ebac`:

- If it says **No such file or directory**, you don't have EBaC yet: follow **First-time install**.
- If it lists files (`bible`, `README`, `AV`, ...), you already have it: follow **Updating to the latest version**.

Type each command exactly as shown, pressing Enter after each line. Lines starting with `#` are comments; you don't need to type them.

### First-time install (do this only once)

**1. Install the tools EBaC needs to build.** This asks for your password.

```sh
# On Raspberry Pi OS, Debian or Ubuntu:
sudo apt install git build-essential libncurses-dev
# On Fedora instead:  sudo dnf install git gcc make ncurses-devel
```

**2. Download EBaC into your home folder.** This creates a folder called `ebac`.

```sh
cd ~
git clone https://github.com/neilbrookins/ebac.git
```

**3. Build the program.**

```sh
cd ~/ebac
make
```

**4. Tell EBaC where its files are.** This only needs to be done once; it's remembered for every new terminal window.

```sh
echo 'export BIBLEROOT=$HOME/ebac' >> ~/.bashrc
source ~/.bashrc
```

**5. Run EBaC.**

```sh
~/ebac/bible
```

### Running EBaC

Any time after installing, just type:

```sh
~/ebac/bible
```

At the main menu press **V** to view a passage, **S** to search, **C** to change translation, **I** for information, or **E** to exit. In any list or passage, press **?** for the keys available there. The arrow keys and **Page Up**/**Page Down** work too: in a passage, Down and Up step one verse at a time.

### Updating to the latest version

**Don't run `git clone` again.** You already have EBaC; `git clone` only works the first time, and will just say `destination path 'ebac' already exists`. Instead, download only what changed, then rebuild:

```sh
cd ~/ebac
git pull
make
```

That's all. Your settings and your search history are kept.

- If `git pull` says `Already up to date.`, you already have the latest version.
- If `make` says `Nothing to be done for 'all'`, that's fine too: the update didn't change the program itself, only things like the documentation.

### If something goes wrong

- **`destination path 'ebac' already exists`:** you already have EBaC. Use **Updating to the latest version** instead.
- **`fatal: not a git repository`:** you weren't in the EBaC folder. Type `cd ~/ebac` first, then try again.
- **`You are not currently on a branch`:** the folder was switched to an old version. Type `git checkout main`, then `git pull` and `make`.
- **`Your local changes ... would be overwritten`:** you changed a file that the update also changes, such as `trans`. To keep your change and still update, type:
  ```sh
  git stash
  git pull
  git stash pop
  make
  ```
- **`term.h: No such file or directory`** while building: step 1 wasn't done. Install the tools, then type `make` again.
- **`Error: getenv(BIBLEROOT) failed`** when starting: step 4 wasn't done, or the terminal window was open before it was. Do step 4, or open a new terminal window.

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

Operators: a number for AND (`0` = same verse; `2` = within two verses), `,` for OR, `-` for AND NOT. Books are written `@` plus enough of the name to be unique. If a search is malformed, EBaC tells you what is wrong. While typing a search, **Up** and **Down** (or CTRL-G and CTRL-V) recall earlier searches, which are kept in `~/.ebac_log`. **Left**, **Right**, **Home**, **End** and **Delete** edit the line. CTRL-B, CTRL-F, CTRL-A or CTRL-X, CTRL-E or CTRL-R, and CTRL-D do the same.

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

## Testing

EBaC compiles two ways. `-DNORAMDISK` keeps search results in memory. `-DRAMDISK` keeps them in temporary files instead, for machines with little memory, such as the CoCo. The two are different code, but a user must never be able to tell them apart.

```sh
make test
```

This builds both versions and runs them side by side through a set of scripted sessions: viewing, every kind of search, paging, and invalid input. It checks that the screens they produce are byte-for-byte identical. Every change to the code should keep this test passing, so the two versions never drift apart. To add memory-error checking as well, run `make test TESTSAN="-fsanitize=address,undefined"`. The test needs Python 3; to add a scenario, add a line to `SCENARIOS` in [`tests/sidebyside.py`](tests/sidebyside.py).

## License

GNU General Public License, version 2 — the license the project has on SourceForge. See [`COPYING`](COPYING).
