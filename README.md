# EBaC — Electronic Bible and Concordance

> [!NOTE]
> **Do you use Home Assistant?** Then install **[EBaC for Home Assistant](https://github.com/neilbrookins/ebac-ha)** instead of this. It's the easier way: there's nothing to type or build, it updates itself, and besides the keyboard it works by tapping, swiping and clicking, on a phone, a tablet or a computer's web browser. (Home Assistant is a free smart-home system, often run on a small computer such as a Raspberry Pi and used through a web browser or its phone app.) If typing commands and building a program sounds unfamiliar, and you have Home Assistant or would like to, that version is the one for you.
>
> **This page is EBaC to install on a computer yourself.** You download it and build it with a few typed commands, and it runs in a terminal window, used with the keyboard. Pick your computer to go straight to its instructions:
>
> | Your computer | Instructions |
> |---|---|
> | Linux, including a Raspberry Pi | [Installing on Linux](#installing-on-linux) |
> | Windows 10 or 11 | [Installing on Windows](#installing-on-windows) |
> | Apple Mac | [Installing on macOS](#installing-on-macos) |
> | Solaris 11 | [Installing on Solaris 11](#installing-on-solaris-11) |
> | Tandy Color Computer 3 (OS-9) | [Version 2 on the Tandy CoCo 3](#version-2-on-the-tandy-coco-3-untested) |
> | MM/1, VAX/VMS, MS-DOS and other vintage computers | [History, and building on vintage computers](#history-and-building-on-vintage-computers) |

Written by Neil G. Brookins.

EBaC is a fast, text-mode Bible reader and concordance. It has two main jobs:

- **Bible:** type a book, chapter and verse, and read from there, paging forward or back a verse, a screen, or a book at a time.
- **Concordance:** type a word and get every verse that contains it, with the word highlighted. Then pick any verse to read it in context.

Unlike a printed concordance, EBaC shows complete verses. It indexes every word, even common ones like *I* and *am*. Searches find phrases ("in the beginning"), and can combine words and books with AND, OR, AND NOT and distances ("within 2 verses", "the next verse"). The King James (Authorized) Version is included.

## Installing in Home Assistant

[![Open your Home Assistant instance and show the add app repository dialog with this repository URL pre-filled.](https://my.home-assistant.io/badges/supervisor_add_addon_repository.svg)](https://my.home-assistant.io/redirect/supervisor_add_addon_repository/?repository_url=https%3A%2F%2Fgithub.com%2Fneilbrookins%2Febac-ha)

Click the button above to add the EBaC repository to your Home Assistant, or add it by hand:

1. In Home Assistant, go to **Settings → Apps** and select **Install app**.
2. Open the menu (⋮) in the top-right corner and select **Repositories**.
3. Paste `https://github.com/neilbrookins/ebac-ha` and select **Add**, then close the dialog.
4. Find **EBaC (Electronic Bible and Concordance)** in the list (you may need to refresh the page) and select it.
5. Select **Install**. Home Assistant builds EBaC on your machine, which takes a few minutes, longer on a Raspberry Pi.
6. Select **Start**, then turn on **Show in sidebar**.

Click **EBaC** in the sidebar to use it; it's available to every user. Each user keeps their own search history. **Updating:** when a new version comes out, Home Assistant offers it under **Settings → Updates**; select **Update**.

The app needs Home Assistant OS or a Supervised install (the kinds that have **Settings → Apps**) on a 64-bit system. Its packaging lives at [neilbrookins/ebac-ha](https://github.com/neilbrookins/ebac-ha).

## Installing on Linux

Not sure which steps you need? Open a terminal and type `ls ~/ebac`:

- If it says **No such file or directory**, you don't have EBaC yet: follow [First-time install](#first-time-install-do-this-only-once).
- If it lists files (`bible`, `README`, `AV`, ...), you already have it: follow [Updating to the latest version](#updating-to-the-latest-version).

Type each command exactly as shown, pressing Enter after each line. Lines starting with `#` are comments; you don't need to type them.

### First-time install (do this only once)

**1. Install the tools EBaC needs to build.** This asks for your password.

```sh
# On Raspberry Pi OS, Debian or Ubuntu:
sudo apt update
sudo apt install git build-essential libncurses-dev
# On Fedora instead:  sudo dnf install git gcc make ncurses-devel
```

**2. Download EBaC into your home folder.** This creates a folder called `ebac`. (`--depth 1` downloads only the latest version, not the project's whole history, which halves the download. Updating works the same.)

```sh
cd ~
git clone --depth 1 https://github.com/neilbrookins/ebac.git
```

**3. Build the program.** This also builds the concordance index, the `.dat` files that make searching fast, from the Bible text; it takes a few seconds.

```sh
cd ~/ebac
make
```

**4. Tell EBaC where its files are.** Type or paste this line. It works out which shell your terminal uses (bash, zsh, ksh, csh, tcsh, fish, ...) and adds the setting to that shell's own settings files, in that shell's own words, so it works whichever one you have. It says which files it changed. This only needs to be done once, and doing it again does no harm.

```sh
sh ~/ebac/set-bibleroot.sh
```

Then close the terminal window and open a new one, so that it picks up the setting.

**5. Run EBaC.**

```sh
~/ebac/bible
```

### Running EBaC

Any time after installing, just type:

```sh
~/ebac/bible
```

At the main menu press **V** to view a passage (type the start of the book's name, then the chapter and verse numbers; the letters **q w e r t y u i o p** can be typed for **1 2 3 4 5 6 7 8 9 0**, and **m** for a minus sign, so a phone's on-screen keyboard can stay on letters. Chapter and verse count on from the start of the book, so they can run past its end into the next book, and **0** or a negative number counts back: John 3:**0** is John 2:25, Exodus chapter **0** is Genesis 50, and John 3:**-3** is John 2:22. Pressing Enter alone means 1), **S** to search, **C** to change translation, **A** to [add or remove one](#downloading-a-translation), **I** for information, or **E** to exit. In any list or passage, press **?** for the keys available there. The arrow keys and **Page Up**/**Page Down** work too: in a passage, Down and Up step one verse at a time. If the window changes size, such as when a phone's on-screen keyboard appears or goes away, a passage or a list of search results is redrawn at once to fill the new size, starting from the same verse; **CTRL-L** redraws it too.

### Updating to the latest version

**Don't run `git clone` again.** You already have EBaC; `git clone` only works the first time, and will just say `destination path 'ebac' already exists`. Instead, download only what changed, then rebuild:

```sh
cd ~/ebac
git pull
make
```

That's all. Your settings and your search history are kept.

- If `git pull` says `Already up to date.`, you already have the latest version.
- If `make` prints nothing at all, that's fine too: the update didn't change the program itself, only things like the documentation, so there was nothing to rebuild.

### If something goes wrong

- **`destination path 'ebac' already exists`:** you already have EBaC. Use [Updating to the latest version](#updating-to-the-latest-version) instead.
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
- **`Error: getenv(BIBLEROOT) failed`** when starting: step 4 wasn't done, or the terminal window was open before it was. Do step 4 (`sh ~/ebac/set-bibleroot.sh`), then open a new terminal window.

## Installing on Windows

On Windows 10 (version 2004 or later) and Windows 11, in every edition including Home, EBaC runs in **WSL**, the Windows Subsystem for Linux: Microsoft's own, free way to run Linux programs on Windows. (It replaced Microsoft's older *Services for UNIX*, which was removed in Windows 8.1.) WSL runs a real Linux, Ubuntu, in a window, so once it's set up, EBaC installs exactly as on Linux.

**1. Install WSL** (only once; skip this if you already have it). Right-click the **Start** button and select **Terminal (Admin)** (on Windows 10, **Windows PowerShell (Admin)**), then type:

```powershell
wsl --install
```

Restart the computer when it's done. (If it reports an error about virtualization, it has to be switched on in the computer's BIOS or UEFI settings, usually among the CPU options; Microsoft explains how in [Enable virtualization on Windows](https://support.microsoft.com/windows/c5578302-6e43-4b4b-a449-8ced115f58e1). A PC too old for WSL, such as one with a Core 2 Duo, can use Cygwin instead, below.) Afterwards, an **Ubuntu** window opens by itself (or open **Ubuntu** from the Start menu) and asks you to choose a user name and password for Linux. They can be different from your Windows ones; remember the password, as step 2 asks for it.

**2. In the Ubuntu window, follow [First-time install](#first-time-install-do-this-only-once) for Linux,** using the commands for Ubuntu. They download EBaC into your Linux home folder, which is the best place for it.

From then on, open **Ubuntu** from the Start menu and type `~/ebac/bible` to run EBaC; [updating](#updating-to-the-latest-version) is the same as on Linux. The window works with all the keys EBaC uses, including **Home**, **End**, **Page Up** and **Page Down**. (WSL also appears in the **Terminal** app, as an Ubuntu tab.)

**With Cygwin instead.** [Cygwin](https://www.cygwin.com/) is an older way of running UNIX programs on Windows, and still maintained; EBaC 1.3.0 was used and tested at length with it. Version 2 should build the same way (this hasn't been tried yet): when installing Cygwin with its `setup-x86_64.exe`, also select the packages **gcc-core**, **make**, **git** and **libncurses-devel** (and **python3** for `make test`), then type the Linux commands from step 2 of [First-time install](#first-time-install-do-this-only-once) onwards in the Cygwin terminal.

## Installing on macOS

The steps are the same as [for Linux](#first-time-install-do-this-only-once), except for installing the tools, and a few keys. They work in the Mac's own Terminal app, and in other terminal windows such as iTerm2 or xterm. (EBaC has been built and tested here with the same tools a Mac has, including its version of `make`, but not yet on a Mac itself; if something doesn't work, please report it.)

**1. Install the tools EBaC needs** with Apple's Command Line Tools, instead of `apt`. Type this, then select **Install** in the window that appears and wait for it to finish (a few minutes). If it says the tools are already installed, you already have everything.

```sh
xcode-select --install
```

This gives you the compiler, `make` and `git`, and the ncurses library EBaC uses; nothing else (such as Homebrew) is needed.

**2. Download and build EBaC** with the same commands as on Linux (steps 2 and 3):

```sh
cd ~
git clone --depth 1 https://github.com/neilbrookins/ebac.git
cd ~/ebac
make
```

**3. Tell EBaC where its files are,** with the same line as on Linux. Macs since macOS 10.15 Catalina use the zsh shell and older ones bash, which keep their settings in different files; the line works out which one your Terminal uses, so you don't need to know. Then close the Terminal window and open a new one.

```sh
sh ~/ebac/set-bibleroot.sh
```

Then run EBaC with `~/ebac/bible`, and update it with `git pull` and `make`, as on Linux.

**Keys in the Mac's Terminal app.** Terminal keeps **Home**, **End**, **Page Up** and **Page Down** for scrolling its own window, so EBaC doesn't see them. Hold **Shift** with them to send them to EBaC (on a laptop keyboard, **Shift**+**fn** with the arrow keys), or use EBaC's own keys instead: **space** and **p** for the next and previous screen, and **CTRL-A** and **CTRL-E** for the start and end of the search line. The arrow keys work as they are. The Mac's **delete** key erases to the left; **fn**+**delete** erases to the right.

## Installing on Solaris 11

The steps are the same as [for Linux](#first-time-install-do-this-only-once), with two differences. (This hasn't been tried on a Solaris computer yet; if something doesn't work, please report it.)

**1. Install the tools with `pkg`** instead of `apt`. (`pfexec` works in place of `sudo`.)

```sh
sudo pkg install developer/gcc developer/build/gnu-make developer/versioning/git library/ncurses
```

**2. Type `gmake` wherever these instructions say `make`.** EBaC's makefile needs GNU make, which Solaris calls `gmake`; Solaris's own `make` stops with errors. So step 3 is:

```sh
cd ~/ebac
gmake
```

and updating is `git pull`, then `gmake`.

Step 4, telling EBaC where its files are, is the same line as on Linux, `sh ~/ebac/set-bibleroot.sh`. It works with whichever shell you use, whether Solaris's usual bash or ksh, csh or another, and for both terminal windows and logins over SSH.

The rest is taken care of. Solaris keeps the ncurses headers EBaC needs in their own folder, `/usr/include/ncurses`, apart from its own older curses: the makefile finds them there (or asks `pkg-config`, if it is installed). It also uses Solaris's `nawk`, as its `/usr/bin/awk` is the original 1970s version. On SPARC computers, the index files `gmake` builds are the same as everywhere else: they are written one byte at a time, the same on every kind of processor.

Over SSH from Linux, a Mac or Windows, EBaC works in any terminal window. On Solaris's own console, ncurses needs to know the console's terminal type; if EBaC says it can't, log in over SSH instead.

## Searching

Searches use postfix (reverse Polish) notation. Put the operator after the two things it combines:

| Search | Finds |
|---|---|
| `amen` | every verse containing "amen" |
| `in the beginning` | the phrase "in the beginning" (words alone are a phrase) |
| `in the . beginning . @gen 0` | the phrase, in Genesis |
| `love hate 0` | verses with both "love" and "hate" |
| `love hate 2` | "love" and "hate" within 2 verses of each other |
| `cain abel +1` | "abel" in the verse after one with "cain" (and both verses) |
| `amen @rev 0` | "amen" in the book of Revelation |
| `food @gen -` | "food" anywhere except Genesis |
| `blessed =gospels 0` | "blessed" in the four Gospels (a group of books) |
| `faith =nt 0` | "faith" in the New Testament |
| `grace =rom..phile 0` | "grace" from Romans to Philemon (a range of books) |

Operators: a number for AND (`0` = same verse; `2` = within two verses, either way), `+2`, `-2` and `~2` for exactly two verses after, before, or apart either way, `,` for OR, `-` for AND NOT, and `.` for a phrase: the word just before it comes next. Books are written `@` plus enough of the name to be unique. Words with letters beyond ASCII (accented letters, Hebrew, Greek) are searched for like any other, case ignored and accents matched exactly as typed: `señor`, `creó`.

Groups of books are written `=` plus enough of the group's name to be unique (`=n`, `=go`), and a range of books `=` plus two books (`=rom..phile`, in the translation's own order). The groups are defined in the file `groups`, not in the program: `ot` and `nt`; `law`, `history`, `poetry`, `majorprophets` and `minorprophets`; `gospels`, `synoptics`, `paul`, `pastoral`, `general` and `john`. Each translation keeps its own arrangement of books: the Old Testament of the Douay-Rheims Bible includes Tobit and the others, as published, and the King James Version with the Apocrypha (KJVA) has `apocrypha` as well, from `groups.KJVA`. Edit `groups` to add your own; [readme.architecture](readme.architecture) describes the file.

Phrases find words next to each other, in order, whatever the punctuation between them, and they run on from the end of one verse into the start of the next (so a phrase broken by a verse number is still found), across chapters and books too. Words in [square brackets] (in the King James Version, the words its translators supplied, in italics in printed Bibles) may be left out or included: `darkness upon the face` and `darkness was upon` both find Genesis 1:2, "darkness [was] upon the face of the deep". In a search with other operators, build a phrase with `.` after each word but the first: `in the . beginning .`. How the index makes this possible is in [readme.architecture](readme.architecture). If a search is malformed, EBaC tells you what is wrong. While typing a search, **Up** and **Down** (or CTRL-G and CTRL-V) recall earlier searches, which are kept in `~/.ebac_log`. **Left**, **Right**, **Home**, **End** and **Delete** edit the line. CTRL-B, CTRL-F, CTRL-A or CTRL-X, CTRL-E or CTRL-R, and CTRL-D do the same.

The top of the list says how many verses were found. For a search of one word alone it also says how many times the word occurs, which can be more, since a verse may have it twice: "56 verses, grass 62 times", with the word highlighted as it is in the verses.

In the list of results, press the label beside a verse (**1**, **2**, ... **9**, then **a**, **b**, ...) to read it in context, and **x** to come back to the list. The letters under the digits, **q w e r t y u i o**, also pick entries 1 to 9, so on a phone's on-screen keyboard you never have to switch to the number keys. A label can be typed even when its entry is below the bottom of the screen: if hiding a phone's keyboard showed entries 3, 4 and 5, typing **3** still opens that entry after the keyboard comes back and hides it.

See [`docs`](docs) for more examples.

## Adding other translations

### Downloading a translation

At EBaC's main menu, press **A** to add or remove a translation. EBaC lists the English translations it can download, a screenful at a time, with a `*` before those already added: type the numbers of those you want and press Enter (such as `6`, or several, such as `6 14 31`; short names such as `BSB` work too), or press Enter alone to go back. For each one it downloads the text, splits it into a file per book in a folder of its own (such as `BSB`) with its `booklist`, builds its concordance index, checks that the index has every verse, and adds it at the top of `trans`. It takes a few seconds per translation; when it is done, read what it says and press any key, and you are back at the main menu with the new translation chosen; as it is first in `trans`, EBaC starts with it from then on too (move another line to the top of `trans` to change that). Choosing one already added downloads it again, as a fresh copy, and moves it to the top.

To remove a translation, type **r** and its number, such as `r6`: its folder and its line in `trans` are deleted, so it is no longer offered, and its `*` is gone. Adding and removing can be mixed in one line, done in the order typed: `r10 11 13` removes 10 and adds 11 and 13. If anything in the line is wrong (a number not in the list, or `r` for a translation that isn't added), nothing is done, and the list says what was wrong. Only downloaded translations can be removed, not the King James (AV) that comes with EBaC.

The **A** choice runs `add-translation.sh`, in the EBaC folder: a shell script, which needs only the standard shell and awk, and `curl` or `wget` to download (Linux, macOS, WSL and Solaris have them, and so does the Home Assistant app). It can also be run by itself:

```sh
cd ~/ebac
sh add-translation.sh              # list the translations, and ask which to add or remove
sh add-translation.sh BSB LEB      # add these
sh add-translation.sh --remove BSB # remove this one
sh add-translation.sh --list       # just list them
```

Behind the scenes, EBaC hands over to this script (EBaC is replaced by it, so its memory is freed meanwhile), and when it is done, it starts a new EBaC, which reads `trans` again and so offers the new translation. EBaC can be started with any translation in `trans` the same way: `~/ebac/bible BSB`.

The 34 translations it offers include the American Standard Version, the Berean Standard Bible, the Lexham English Bible, Young's Literal Translation, Darby, Webster, the Geneva Bible of 1599, the Bible in Basic English, the New Heart English Bible, the Open English Bible, Tyndale, and several New Testaments. They come from the plain-text Bibles of the [bible_databases](https://github.com/scrollmapper/bible_databases) project, made from [CrossWire](https://crosswire.org/sword/modules/ModDisp.jsp?modType=Bibles)'s modules; CrossWire has a few more that aren't available as text there. Three include the Apocrypha: the King James Version with the Apocrypha (KJVA), the Douay-Rheims Bible and the Catholic Public Domain Version.

Each translation keeps its own book names, as one word (EBaC's book prompt and `@book` searches take one word), shortened to fit the list of search results: `I Samuel` becomes `ISamuel`, so type `isam` for it, or `1s`: a number typed at the start of a book's name also matches a Roman numeral there, so `2th` finds `IIThessalonians` as well as `2Thessalonians`, in any translation (at the book prompt and in `@book` searches). Curly quotes and dashes become plain ones, as in EBaC's own text, so that searches find the words beside them. Verse numbers keep the brackets they have there, `[1:1]`: EBaC reads both `[1:1]` and `1:1`.

With more than nine translations, the **C** screen shows them a page at a time: type a number and press Enter (**space** and **p** turn the page, **x** goes back). Adding a translation changes `trans`; if a later `git pull` then says `Your local changes ... would be overwritten`, see [If something goes wrong](#if-something-goes-wrong). In the Home Assistant app, added translations are kept in the app's own storage, so updates of the app keep them (and Home Assistant's backups include them).

### Adding one by hand

Each translation is a folder (`AV` is the King James Version) holding one text file per book, plus a `booklist` file naming those books in order, one per line. A translation can cover only some books, such as just the New Testament: its `booklist` lists only those. In a book's file, each verse starts a line with its chapter and verse number, `1:1` or `[1:1]`; a verse may go on over the following lines, which start with a space.

To add one, for example the NIV:

1. Make a folder `NIV` next to `AV`, and put its book files and its `booklist` in it.
2. Add a line for it to the `trans` file: the folder name, then a description:
   ```
   NIV  New International Version
   ```
3. Type `make`. It builds the NIV's concordance index (the `.dat` files).

The **C** key at the main menu now offers the new translation.

### How `make` keeps the indexes up to date

`make` reads the list of translations from `trans`, and each translation's books from its `booklist`; neither is written into the makefile. For each translation, it runs `index` again if any of its `.dat` files is missing, or older than its `booklist`, any of its books, or the `index` program. So after correcting a book's text, `make` re-indexes that translation, and when nothing has changed it does nothing.

In `trans`, blank lines and lines starting with `#` are comments. Every folder should be mentioned there: a folder that isn't a translation gets a comment line starting with its name, such as `# tests` for the test scripts. `make` warns about a folder `trans` doesn't mention, so a new translation isn't forgotten, and `make test` treats that as an error. Folder names starting with `.` are ignored.

The original [`README`](README) describes the file layout in more detail.

## History, and building on vintage computers

EBaC dates from 1993. Up to 2014 it ran on the Tandy Color Computer 3 (OS-9 6809, 512K), the MM/1 (OS-9 68000), VAX/VMS, MS-DOS, Windows (with Cygwin), and the UNIX systems of the 1990s. Version 1.3.0 was published on [SourceForge](https://sourceforge.net/projects/ebac/) in 2009.

This repository starts with that release, unchanged, as its first commit and tagged **`v1.3.0`**. Each step needed to bring it up to date is a separate commit after it, with an explanation.

The [install steps](#first-time-install-do-this-only-once) download only the latest version. **To build for a vintage system, or to read the history, download the full history** (a separate folder, `ebac-full`, so it doesn't disturb a Linux install), then switch to the original release:

```sh
git clone https://github.com/neilbrookins/ebac.git ebac-full
cd ebac-full
git checkout v1.3.0
```

In the full download, `git log v1.3.0..` lists each step since the original release. A folder made with `--depth 1` can get the full history later with `git fetch --unshallow --tags`.

Version 2.0 is written in ANSI C (C89, and also valid C99, C17 and C23). The OS-9 68000, VMS and MS-DOS code is still in the source behind its `#ifdef`s and may still build with those systems' ANSI compilers, but only the Linux build has been tested.

### Version 2 on the Tandy CoCo 3 (untested)

The CoCo's Microware C compiler only takes the older K&R C, and the CoCo hasn't the memory to build the concordance index. On a Linux computer, `make coco` prepares everything it needs:

```sh
make coco
```

This writes `coco/build/ebac-coco.tar`, and, if [ToolShed](https://github.com/nitros9project/toolshed)'s `os9` program is installed, `coco/build/ebac-coco.dsk`, an OS-9 disk image (8 MB, for DriveWire, a CoCoSDC or an emulator). Each holds one folder, `BIBLE`, which goes in `/dd/BIBLE` on the CoCo:

- `BIBLE/SRC`: the program's source, converted to K&R C by [ansi2knr](coco/ansi2knr.c) (by L. Peter Deutsch; GPL), and a makefile with 1.3.0's CoCo settings (`-DRAMDISK -DCOCO`). Compile it there to make `bible`.
- `BIBLE/AV`: the King James text and its index. The index files are the same on every computer, so the ones built on Linux work on the CoCo unchanged.
- `BIBLE/trans`: the list of translations.

Text files are given OS-9's line endings (a carriage return), and books whose names begin with a digit get the CoCo's names, so `1Chronicles` is stored as `x1Chronicle`.

To make this possible, the source avoids what K&R compilers lack: declarations put their parameters in `P((...))`, which is `()` on the CoCo (see `both.h`); there is no `void`, `unsigned char`, `#if defined`, or string constants placed side by side on the CoCo; and EBaC has its own replacements for `strchr()` and `strtok()`. The converted source passes GCC's `-Wtraditional` check, but it has not been compiled on a CoCo yet. Whether version 2 fits in the CoCo's memory is also still to be found out.

## What changed in 2.0

Modern compilers (GCC 14 and later, Clang 16 and later, and the C23 standard) reject the 1980s K&R style C the program was written in. The termcap library and `<sys/termio.h>` it used are gone from current Linux systems. Version 2.0:

- uses the **ncurses** library (its terminfo interface) instead of termcap, and POSIX `termios` instead of System V `termio`;
- has ANSI function prototypes and definitions throughout;
- fixes bugs that newer compilers and 64-bit, ARM (Raspberry Pi) systems exposed, plus a crash when `LOGNAME` is unset and an exit on malformed searches;
- follows the terminal window when it is resized.

The concordance index built on 64-bit Linux is byte-for-byte identical to the one shipped in 1.3.0. Since it can be rebuilt in seconds, it is no longer stored in the repository: `make` builds it. The `v1.3.0` release still includes its `.dat` files, for computers such as the CoCo that don't have the memory to run `index`.

## Testing

EBaC compiles two ways. `-DNORAMDISK` keeps search results in memory. `-DRAMDISK` keeps them in temporary files instead, for machines with little memory, such as the CoCo. The two are different code, but a user must never be able to tell them apart.

```sh
make test
```

This builds both versions and runs them side by side through a set of scripted sessions: viewing, every kind of search, paging, and invalid input. It checks that the screens they produce are byte-for-byte identical. Every change to the code should keep this test passing, so the two versions never drift apart. To add memory-error checking as well, run `make test TESTSAN="-fsanitize=address,undefined"`. The test needs Python 3; to add a scenario, add a line to `SCENARIOS` in [`tests/sidebyside.py`](tests/sidebyside.py).

## License

GNU General Public License, version 2 — the license the project has on SourceForge. See [`COPYING`](COPYING).
