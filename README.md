# EBaC — Electronic Bible and Concordance

Written by Neil G. Brookins.

## Which way to install

EBaC comes four ways. They're the same program, made from the same source, so each gets every improvement at the same time; they differ only in how you install and use it. Pick the first that fits:

| | Way | Best for | You get | You need |
|---|---|---|---|---|
| 1 | **[The Home Assistant app](#installing-in-home-assistant)** | Anyone with Home Assistant | EBaC in Home Assistant's sidebar, on a phone, tablet or computer: tapping, swiping and clicking as well as the keyboard. Updates itself. Voice ("Genesis 1:1") with the EBaC integration. Each Home Assistant user has their own search history. | Home Assistant (with apps: Home Assistant OS) |
| 2 | **[Docker, in a web page](#installing-with-docker)** | Anyone without Home Assistant | The same web page as the app, with tapping, swiping and clicking, opened in any browser, on a phone too. One command to install; one to update. | Docker, which is free: the steps show how to get it, with one command on Linux or a Raspberry Pi, or an app on Windows or a Mac |
| 3 | **[Docker, in a terminal](#ebac-in-a-terminal-with-docker)** | Keyboard-only use, on a computer with Docker | EBaC in your terminal window, with no web page: keys only, no tapping or clicking. | Docker |
| 4 | **[Built from source](#installing-on-linux)** | Computers without Docker, and vintage ones | EBaC in a terminal window, built with a few typed commands: keys only. Runs on almost anything, down to an MS-DOS PC. | A C compiler, `make` and ncurses (the steps say how to get them) |

Ways 1 and 2 are the friendliest: the web page turns taps and swipes into EBaC's keys (tap a choice, swipe to page, tap a verse to open it). Ways 3 and 4 are the program alone, by keyboard.

For way 4, pick your computer:

| Your computer | Instructions |
|---|---|
| Linux, including a Raspberry Pi or a Chromebook | [Installing on Linux](#installing-on-linux) |
| Windows 10 or 11 | [Installing on Windows](#installing-on-windows) |
| An older Windows PC, with Cygwin | [Installing on Windows with Cygwin](#installing-on-windows-with-cygwin) |
| Apple Mac | [Installing on macOS](#installing-on-macos) |
| Solaris 11 | [Installing on Solaris 11](#installing-on-solaris-11) |
| FreeBSD, OpenBSD or NetBSD | [Installing on FreeBSD, OpenBSD or NetBSD](#installing-on-freebsd-openbsd-or-netbsd) |
| MM/1 (OS-9 68000) | [MM/1](#mm1-os-9-68000) |
| VAX/VMS | [VAX/VMS](#vaxvms) |
| Tandy Color Computer 3 (OS-9 6809) | [Version 2 on the Tandy CoCo 3](#version-2-on-the-tandy-coco-3-untested) |
| AT&T 3B1; SunOS 4 and Solaris 2; DEC Ultrix and OSF/1; HP-UX | [AT&T 3B1, Sun, DEC and HP UNIX](#att-3b1-sun-dec-and-hp-unix) |
| MS-DOS | [MS-DOS](#ms-dos) |

## What EBaC is

EBaC is a fast, text-mode Bible reader and concordance. It has two main jobs:

- **Bible:** type a book, chapter and verse, and read from there, paging forward or back a verse, a screen, or a book at a time.
- **Concordance:** type a word and get every verse that contains it, with the word highlighted. Then pick any verse to read it in context.

Unlike a printed concordance, EBaC shows complete verses. It indexes every word, even common ones like *I* and *am*. Searches find phrases ("in the beginning"), and can combine words and books with AND, OR, AND NOT and distances ("within 2 verses", "the next verse"). The King James (Authorized) Version is included.

## Contents

- [Which way to install](#which-way-to-install)
- [What EBaC is](#what-ebac-is)
- [Installing](#installing)
  - [Installing in Home Assistant](#installing-in-home-assistant)
  - [Installing with Docker](#installing-with-docker)
    - [Step 1: Get Docker](#step-1-get-docker)
    - [Step 2: Start EBaC](#step-2-start-ebac)
    - [Updating](#updating)
    - [Removing EBaC from Docker](#removing-ebac-from-docker)
    - [EBaC in a terminal with Docker](#ebac-in-a-terminal-with-docker)
  - [Installing on Linux](#installing-on-linux)
    - [First-time install (do this only once)](#first-time-install-do-this-only-once)
    - [Running EBaC](#running-ebac)
    - [Updating to the latest version](#updating-to-the-latest-version)
    - [If something goes wrong](#if-something-goes-wrong)
  - [Installing on Windows](#installing-on-windows)
  - [Installing on Windows with Cygwin](#installing-on-windows-with-cygwin)
  - [Installing on macOS](#installing-on-macos)
  - [Installing on Solaris 11](#installing-on-solaris-11)
  - [Installing on FreeBSD, OpenBSD or NetBSD](#installing-on-freebsd-openbsd-or-netbsd)
  - [Installing on vintage computers](#installing-on-vintage-computers)
    - [MM/1 (OS-9 68000)](#mm1-os-9-68000)
    - [VAX/VMS](#vaxvms)
    - [Version 2 on the Tandy CoCo 3 (untested)](#version-2-on-the-tandy-coco-3-untested)
    - [AT&T 3B1, Sun, DEC and HP UNIX](#att-3b1-sun-dec-and-hp-unix)
    - [MS-DOS](#ms-dos)
- [Searching](#searching)
- [Adding other translations](#adding-other-translations)
  - [From thiagobodruk/bible (T)](#from-thiagobodrukbible-t)
  - [From scrollmapper/bible_databases (S)](#from-scrollmapperbible_databases-s)
  - [From Project Gutenberg (P)](#from-project-gutenberg-p)
  - [For all three places](#for-all-three-places)
  - [Adding one by hand](#adding-one-by-hand)
  - [How `make` keeps the indexes up to date](#how-make-keeps-the-indexes-up-to-date)
- [History](#history)
- [What changed in 2.0](#what-changed-in-20)
- [For other programs: ebac-api](#for-other-programs-ebac-api)
- [Testing](#testing)
- [License](#license)

## Installing

EBaC runs on these computers. Pick yours; at the end of its steps, a link skips the others and goes on to [Searching](#searching).

EBaC needs nothing particular to one system: only a C compiler, `make`, the ncurses library, and a terminal window. So it should build on any Linux, BSD or other UNIX-like system that has them, beyond those below, even where it hasn't been tried. If you try one, please report how it went.

### Installing in Home Assistant

The steps, with a button that adds EBaC to your Home Assistant in one click, are with the app: **[Installing the EBaC app](https://github.com/neilbrookins/ebac-ha-app#installing)**. Nothing needs to be typed or built.

**Installed? Skip the other computers and go on to [Searching](#searching).** · [↑ Contents](#contents)

### Installing with Docker

EBaC in a web page, with tapping, swiping and clicking, without Home Assistant: the same page the Home Assistant app has. It runs in [Docker](https://www.docker.com/), a free program that runs ready-made programs ("containers") the same way on any computer. Two steps: get Docker (once), then start EBaC.

#### Step 1: Get Docker

Already have it? Type `docker --version` in a terminal: if it answers with a version, skip to step 2.

Docker's website offers two things, and you need only one:

- **Docker Engine** is Docker itself, the part that runs in the background. On **Linux** (including a Raspberry Pi) this is all you need, and your system can install it with one command, below.
- **Docker Desktop** is an app for **Windows** and **Mac** that brings Docker Engine along with it, with windows to manage it. (On Linux you don't need it.)

**Raspberry Pi OS, Debian, Ubuntu or Linux Mint** (type these in a terminal; the first asks for your password):

```sh
sudo apt update && sudo apt install -y docker.io
sudo usermod -aG docker $USER
```

Then **log out and back in** (or restart), so the second line takes effect: it lets you use Docker without typing `sudo` each time. Docker starts by itself, now and whenever the computer starts. (This is the Docker your system itself provides, kept up to date with your other updates. Docker's own newest version is also available, with [Docker's install steps](https://docs.docker.com/engine/install/), but the one above is all EBaC needs.)

**Fedora:**

```sh
sudo dnf install -y moby-engine
sudo systemctl enable --now docker
sudo usermod -aG docker $USER
```

then log out and back in.

**Windows 10 or 11, or a Mac:** download and install **[Docker Desktop](https://www.docker.com/products/docker-desktop/)** (free for personal use), and open it once; it then starts with the computer. Type the commands in step 2 in **PowerShell** (Windows) or **Terminal** (Mac).

**Raspberry Pi:** any Pi from the 2 on, with the 32-bit or 64-bit Raspberry Pi OS. (Not the original Pi 1 or the Pi Zero / Zero W, which Docker doesn't support; the Zero 2 W is fine.)

#### Step 2: Start EBaC

One command, typed in a terminal:

```sh
docker run -d --name ebac --restart unless-stopped -p 7681:7681 -v ebac-data:/data ghcr.io/neilbrookins/ebac
```

The first time, it downloads EBaC (a minute or so). Then open **http://localhost:7681** in a web browser on the same computer, or **http://*the computer's address*:7681** from a phone or another computer on your network (on a Raspberry Pi, `hostname -I` shows its address). EBaC starts again by itself whenever the computer does. The volume `ebac-data` keeps the translations you add and each user's search history, so they outlast updates.

**A login.** Without one, anyone who can reach port 7681 can use EBaC. To ask for a user name and password, add `-e EBAC_USER=yourname -e EBAC_PASSWORD=yourpassword` before the image's name. Behind a reverse proxy that logs users in itself (such as Authelia), add `-e EBAC_AUTH_HEADER=Remote-User` (the header that names the user) instead, so each person has their own search history. Don't put EBaC on the internet without one or the other.

**With Docker Compose** instead: [docker/docker-compose.yml](docker/docker-compose.yml) is ready to copy. `docker compose up -d` starts it; `docker compose pull && docker compose up -d` updates it (keeping the volume: see [Updating](#updating)).

**Building it yourself** instead of downloading it ready-made (it takes a few minutes, a little longer on a Raspberry Pi, and gives the same thing):

```sh
docker build -f docker/Dockerfile -t ebac https://github.com/neilbrookins/ebac.git#main
```

then use `ebac` in place of `ghcr.io/neilbrookins/ebac` in the commands above. (The Compose file has the lines for this too.)

**Voice, with Home Assistant but without apps** (Home Assistant Container or Core): the [EBaC integration](https://github.com/neilbrookins/ebac-ha-integration) can use this container too. Add `-p 8080:8080` to the `docker run` command, and give the integration the address `http://the computer's address:8080/cgi-bin/ebac`. To let voice add and remove translations, add `-e EBAC_ALLOW_ADD=yes` (and `-e EBAC_ALLOW_REMOVE=yes`); both are off unless you do.

**If something goes wrong:** `docker logs ebac` shows what EBaC said as it started. `-e EBAC_DEBUG=true` adds the troubleshooting box to the page (what it sees of the screen's size and the keyboard), for reporting a problem with tapping or the keyboard.

#### Updating

**Updating never loses your translations or search history.** Docker keeps three things apart:

| | What it is | What updating does to it |
|---|---|---|
| **The image** | EBaC itself, ready-made: the program, the King James text, the web page. Read-only. | Replaced by the newer one. |
| **The container** | One running copy of the image, named `ebac`. | Removed, and made again from the newer image. |
| **The volume** `ebac-data` | Everything EBaC keeps: the translations you've added, each user's search history. | **Nothing.** The new container uses the same volume, so everything is just as you left it. |

So updating is: get the newer image, remove the old container, and start a new one with the same volume:

```sh
docker pull ghcr.io/neilbrookins/ebac
docker rm -f ebac
docker run -d --name ebac --restart unless-stopped -p 7681:7681 -v ebac-data:/data ghcr.io/neilbrookins/ebac
```

`docker pull` prints a line for each **layer** of the image (the image is stored in parts) and downloads only the layers that changed: usually just EBaC's program and page, a few hundred kilobytes, while the King James text and the rest are kept from before. (Use the same options in the new `docker run` as you started it with, such as a login.)

Each update leaves the previous image on disk, unused and unnamed. To free that space now and then (your translations aren't touched):

```sh
docker image prune
```

`docker system df` shows how much space Docker's images, containers and volumes take.

#### Removing EBaC from Docker

Remove the container and the images:

```sh
docker rm -f ebac
docker rmi ghcr.io/neilbrookins/ebac
docker image prune
```

That leaves the volume, with your translations and search history, so that if you start EBaC again later (step 2), they're all still there. To remove those too, **for good** (they can't be got back, except from a backup you made of the volume):

```sh
docker volume rm ebac-data
```

(With Docker Compose: `docker compose down` removes the container; `docker compose down -v` the volume too.)


#### EBaC in a terminal with Docker

The same image runs EBaC in your terminal window, with no web page: keys only, no tapping or clicking (for those, use the web page above). Get Docker first ([step 1](#step-1-get-docker) above); then, with nothing to build:

```sh
docker run -it --rm -v ebac-data:/data ghcr.io/neilbrookins/ebac terminal
```

It uses the same volume as the web page, so translations added in one are there in the other. `-e EBAC_USER=yourname` keeps your search history apart from others'.

**Installed? Skip the other computers and go on to [Searching](#searching).** · [↑ Contents](#contents)

### Installing on Linux

Not sure which steps you need? Open a terminal and type `ls ~/ebac`:

- If it says **No such file or directory**, you don't have EBaC yet: follow [First-time install](#first-time-install-do-this-only-once).
- If it lists files (`bible`, `README`, `AV`, ...), you already have it: follow [Updating to the latest version](#updating-to-the-latest-version).

Type each command exactly as shown, pressing Enter after each line. Lines starting with `#` are comments; you don't need to type them.

**On a Chromebook,** turn on ChromeOS's Linux development environment first (**Settings → About ChromeOS → Developers → Linux development environment**, then **Set up**). It is Debian, so the steps below work in its **Terminal** app, with the commands for Debian. (This hasn't been tried on a Chromebook yet.)

#### First-time install (do this only once)

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

#### Running EBaC

Any time after installing, just type:

```sh
~/ebac/bible
```

At the main menu press **V** to view a passage (type the start of the book's name, then the chapter and verse numbers; the letters **q w e r t y u i o p** can be typed for **1 2 3 4 5 6 7 8 9 0**, and **m** for a minus sign, so a phone's on-screen keyboard can stay on letters. Chapter and verse count on from the start of the book, so they can run past its end into the next book, and **0** or a negative number counts back: John 3:**0** is John 2:25, Exodus chapter **0** is Genesis 50, and John 3:**-3** is John 2:22. Pressing Enter alone means 1), **S** to search, **C** to change translation, **A** to [add or remove one](#adding-other-translations), **I** for information, or **E** to exit. In any list or passage, press **?** for the keys available there. The arrow keys and **Page Up**/**Page Down** work too: in a passage, Down and Up step one verse at a time. If the window changes size, such as when a phone's on-screen keyboard appears or goes away, a passage or a list of search results is redrawn at once to fill the new size, starting from the same verse; **CTRL-L** redraws it too.

#### Updating to the latest version

**Don't run `git clone` again.** You already have EBaC; `git clone` only works the first time, and will just say `destination path 'ebac' already exists`. Instead, download only what changed, then rebuild:

```sh
cd ~/ebac
git pull
make
```

That's all. Your settings and your search history are kept.

- If `git pull` says `Already up to date.`, you already have the latest version.
- If `make` prints nothing at all, that's fine too: the update didn't change the program itself, only things like the documentation, so there was nothing to rebuild.

#### If something goes wrong

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

**Installed? Skip the other computers and go on to [Searching](#searching).** · [↑ Contents](#contents)

### Installing on Windows

On Windows 10 (version 2004 or later) and Windows 11, in every edition including Home, EBaC runs in **WSL**, the Windows Subsystem for Linux: Microsoft's own, free way to run Linux programs on Windows. (It replaced Microsoft's older *Services for UNIX*, which was removed in Windows 8.1.) WSL runs a real Linux, Ubuntu, in a window, so once it's set up, EBaC installs exactly as on Linux.

**1. Install WSL** (only once; skip this if you already have it). Right-click the **Start** button and select **Terminal (Admin)** (on Windows 10, **Windows PowerShell (Admin)**), then type:

```powershell
wsl --install
```

Restart the computer when it's done. (If it reports an error about virtualization, it has to be switched on in the computer's BIOS or UEFI settings, usually among the CPU options; Microsoft explains how in [Enable virtualization on Windows](https://support.microsoft.com/windows/c5578302-6e43-4b4b-a449-8ced115f58e1). A PC too old for WSL, such as one with a Core 2 Duo, can use [Cygwin](#installing-on-windows-with-cygwin) instead.) Afterwards, an **Ubuntu** window opens by itself (or open **Ubuntu** from the Start menu) and asks you to choose a user name and password for Linux. They can be different from your Windows ones; remember the password, as step 2 asks for it.

**2. In the Ubuntu window, follow [First-time install](#first-time-install-do-this-only-once) for Linux,** using the commands for Ubuntu. They download EBaC into your Linux home folder, which is the best place for it.

From then on, open **Ubuntu** from the Start menu and type `~/ebac/bible` to run EBaC; [updating](#updating-to-the-latest-version) is the same as on Linux. The window works with all the keys EBaC uses, including **Home**, **End**, **Page Up** and **Page Down**. (WSL also appears in the **Terminal** app, as an Ubuntu tab.)

**Installed? Skip the other computers and go on to [Searching](#searching).** · [↑ Contents](#contents)

### Installing on Windows with Cygwin

[Cygwin](https://www.cygwin.com/) is an older way of running UNIX programs on Windows, and still maintained; EBaC 1.3.0 was used and tested at length with it. Version 2 should build the same way (this hasn't been tried yet): when installing Cygwin with its `setup-x86_64.exe`, also select the packages **gcc-core**, **make**, **git** and **libncurses-devel** (and **python3** for `make test`), then type the Linux commands from step 2 of [First-time install](#first-time-install-do-this-only-once) onwards in the Cygwin terminal.

**Installed? Skip the other computers and go on to [Searching](#searching).** · [↑ Contents](#contents)

### Installing on macOS

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

**Installed? Skip the other computers and go on to [Searching](#searching).** · [↑ Contents](#contents)

### Installing on Solaris 11

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

**Installed? Skip the other computers and go on to [Searching](#searching).** · [↑ Contents](#contents)

### Installing on FreeBSD, OpenBSD or NetBSD

The steps are those [for Linux](#first-time-install-do-this-only-once), with the tools installed by the system's own package tool, and `gmake` typed wherever they say `make` (as on Solaris: EBaC's makefile needs GNU make, and the BSDs' own `make` is another). This hasn't been tried on a BSD yet; if something doesn't work, please report it.

```sh
# FreeBSD (ncurses is part of the system):
sudo pkg install git gmake
# OpenBSD (ncurses is part of the system; doas in place of sudo):
doas pkg_add git gmake
# NetBSD (its own curses is not ncurses, so ncurses is added):
sudo pkgin install git gmake ncurses
```

Then step 2 as on Linux, `gmake` for step 3, and step 4, `sh ~/ebac/set-bibleroot.sh`, which works with the BSDs' shells too (sh, ksh, csh, tcsh).

**Installed? Skip the other computers and go on to [Searching](#searching).** · [↑ Contents](#contents)

### Installing on vintage computers

EBaC began on the MM/1 in 1992 and was ported, in turn, to VAX/VMS, the Tandy CoCo 3, the AT&T 3B1, MS-DOS, and the UNIX systems of Sun, DEC and HP (see [History](#history)). The [install steps](#first-time-install-do-this-only-once) download only the latest version. **To build for a vintage system, or to read the history, download the full history** (a separate folder, `ebac-full`, so it doesn't disturb a Linux install), then switch to the original release:

```sh
git clone https://github.com/neilbrookins/ebac.git ebac-full
cd ebac-full
git checkout v1.3.0
```

In the full download, `git log v1.3.0..` lists each step since the original release. A folder made with `--depth 1` can get the full history later with `git fetch --unshallow --tags`.

Version 2.0 is written in ANSI C (C89, and also valid C99, C17 and C23). The OS-9 68000, VMS and MS-DOS code is still in the source behind its `#ifdef`s and may still build with those systems' ANSI compilers. The settings for each come from version 1.3.0's makefile (`git show v1.3.0:makefile` in the full download). None of them has been tried with version 2 yet; if you try one, please report how it went.

#### MM/1 (OS-9 68000)

EBaC's first computer. Build it with `CFLAGS= -DNORAMDISK -DOSK` (1.3.0's makefile used gcc, on the MM/1's Philips 68070). The text files need OS-9's line endings, a carriage return, as on the CoCo; the index files are the same on every computer, so ones built on Linux can be copied over.

#### VAX/VMS

Build it with MMS, using [`descrip.mms`](descrip.mms) and [`options_file.opt`](options_file.opt) in place of the makefile (`-DVMS` is set by VMS's C compiler itself). VMS-specific input and output are in [`vmsio.c`](vmsio.c).

#### Version 2 on the Tandy CoCo 3 (untested)

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

#### AT&T 3B1, Sun, DEC and HP UNIX

EBaC ran on the AT&T 3B1 (UNIX System V), Suns (SunOS 4.x and Solaris 2.x, back to Solaris 2.4), DEC's Ultrix and OSF/1, and the HP 9000 (HP-UX 10.x), built with 1.3.0's makefile as it is, which uses termcap (`-ltermcap`). Version 2 needs an ANSI C compiler (gcc, or a system's own, such as HP's `c89`) and ncurses; SunOS 4's own `cc` is K&R C, so there use gcc, or the K&R conversion made for the [CoCo](#version-2-on-the-tandy-coco-3-untested).

#### MS-DOS

Build it with `CFLAGS= -DRAMDISK -DMSDOS` (1.3.0's makefile names the Watcom C compiler, for an 8086 with about 512 KB of memory). [`readme.msdos`](readme.msdos) is 1.3.0's guide to installing it from floppy disks: the `BIBLEROOT`, `BIBLEDRIVE` and `TEMPFILE` settings in `AUTOEXEC.BAT`, and the `ANSI.SYS` driver EBaC needs.

[↑ Contents](#contents)

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

Phrases find words next to each other, in order, whatever the punctuation between them, and they run on from the end of one verse into the start of the next (so a phrase broken by a verse number is still found), across chapters and books too. Words in [square brackets] (in the King James Version, the words its translators supplied, in italics in printed Bibles) may be left out or included: `darkness upon the face` and `darkness was upon` both find Genesis 1:2, "darkness [was] upon the face of the deep". In a search with other operators, build a phrase with `.` after each word but the first: `in the . beginning .`. How the index makes this possible is in [readme.architecture](readme.architecture). Punctuation at the ends of a word is ignored, so a word copied from the screen with a period or the Hebrew sof pasuq after it is still found. With a translation written right to left (Hebrew, Arabic, ...), the search line is too: it starts at the right edge of the screen and grows to the left, English words and numbers in it reading left to right. If a search is malformed, EBaC tells you what is wrong. While typing a search, **Up** and **Down** (or CTRL-G and CTRL-V) recall earlier searches, which are kept in `~/.ebac_log` (the newest few thousand: past 64 KB, the oldest are dropped). **Left**, **Right**, **Home**, **End** and **Delete** edit the line. CTRL-B, CTRL-F, CTRL-A or CTRL-X, CTRL-E or CTRL-R, and CTRL-D do the same.

The top of the list says how many verses were found. For a search of one word alone it also says how many times the word occurs, which can be more, since a verse may have it twice: "56 verses, grass 62 times", with the word highlighted as it is in the verses.

In the list of results, press the label beside a verse (**1**, **2**, ... **9**, then **a**, **b**, ...) to read it in context, and **x** to come back to the list. The letters under the digits, **q w e r t y u i o**, also pick entries 1 to 9, so on a phone's on-screen keyboard you never have to switch to the number keys. A label can be typed even when its entry is below the bottom of the screen: if hiding a phone's keyboard showed entries 3, 4 and 5, typing **3** still opens that entry after the keyboard comes back and hides it.

See [`docs`](docs) for more examples.

[↑ Contents](#contents)

## Adding other translations

EBaC comes with the King James Version (`AV`). To add other translations, press **A** at EBaC's main menu. EBaC then asks **where to add from: one of three places**, each with its own collection and its own steps. Choose one:

1. **T** (or **1**): [**thiagobodruk/bible**](#from-thiagobodrukbible-t), on GitHub. Some 140 Bibles in 85 languages, including the New International Version and the New Living Translation. Their short names end in `-TB`.
2. **S** (or **2**): [**scrollmapper/bible_databases**](#from-scrollmapperbible_databases-s), on GitHub. 140 translations in 56 languages, from CrossWire, such as the American Standard Version, the Berean Standard Bible and the Douay-Rheims.
3. **P** (or **3**): [**Project Gutenberg**](#from-project-gutenberg-p). Particular texts, by ebook number: the Book of Mormon, the World English Bible, the Qur'an in three translations, and others. Their short names end in `-PG`.

**X** goes back to the main menu. (**S** was **G**, for GitHub, before there were two places there.)

A translation is removed from the same place it was added from. What works the same way in all three (licenses, removing, disk space, and which translation EBaC starts with) is under [For all three places](#for-all-three-places). A translation you already have as files can also be [added by hand](#adding-one-by-hand), with nothing downloaded.

### From thiagobodruk/bible (T)

[thiagobodruk/bible](https://github.com/thiagobodruk/bible) on GitHub has some 140 Bibles in 85 languages, as XML files, many of them not in scrollmapper/bible_databases, such as the New International Version and the New Living Translation. A translation from here has `-TB` (for Thiago Bodruk, whose collection it is) at the end of its short name: `KJVA-TB`, `NIV-TB`, `NLT-TB`. So the same Bible from each place can be kept side by side and compared, such as the King James Version from scrollmapper/bible_databases (`KJVA`) and from here (`KJVA-TB`).

To add one:

1. At the main menu, press **A**, then **T**.
2. EBaC lists the languages there are translations in, each by its own name and its name in English, with the number of translations, and a `*` before those you have added translations in. Type a language's number and press Enter.
3. EBaC lists that language's translations, a screenful at a time, with a `*` before those already added. Type the numbers of those you want and press Enter (such as `6`, or several, such as `6 14 31`; short names such as `NIV-TB` work too). Enter alone goes back to the languages, and Enter alone there, back to EBaC.
4. For each one, EBaC shows its license and asks whether you agree to it: `y` adds it, `n` skips it.
5. EBaC downloads it, reads each text out of its XML into its own book files, indexes it, and adds it. When it is done, read what it says and press any key: you are back at the main menu, reading from the new translation.

To remove one: **A**, **T**, then **r** and its number, such as `r6` (see [Removing a translation](#removing-a-translation)).

About licenses: the collection says of its texts, "All the Bible versions are property of their respective owners. All rights reserved to the owners." Some are in the public domain, such as the King James Version; others, such as the NIV and the NLT, are still under copyright, and what you may do with them is for their owners to say. EBaC shows this and asks whether you agree, as for every translation, and keeps your answer in the translation's `license` file.

Each book can also be typed by the collection's own short name for it, wherever a book name is typed (the **V** screen, `@` in a search, a reference): `gn` for Genesis, `jo` for John, `ez` for Ezekiel, `1sm` for 1 Samuel. They come from the Portuguese names (`sl` Psalms, `ap` Revelation, `tg` James), and are the same in almost every translation there, in Latin letters even where the book names are in Japanese or Korean. EBaC keeps them in the translation's folder, in a file called `abbreviations` (a translation added before November 8, 2026 gets it when added again).

Two things the source does its own way: its "King James Version with Apocrypha" (`KJVA-TB`) has the 66 books only, without the Apocrypha; and its text has, in some 300 verses, the King James margin notes run into the verse (Exodus 6:2: "I am the LORD : : or, JEHOVAH"). EBaC keeps the text exactly as the source gives it.

The same can be done without EBaC's menus:

```sh
cd ~/ebac
sh add-thiagobodruk.sh                  # list the translations, and ask which to add or remove
sh add-thiagobodruk.sh NIV-TB NLT-TB    # add these
sh add-thiagobodruk.sh --remove NIV-TB  # remove this one
sh add-thiagobodruk.sh --list           # just list them, by language
```

### From scrollmapper/bible_databases (S)

The 34 English translations include the American Standard Version, the Berean Standard Bible, the Lexham English Bible, Young's Literal Translation, Darby, Webster, the Geneva Bible of 1599, the Bible in Basic English, the New Heart English Bible, the Open English Bible, Tyndale, and several New Testaments. They come from the plain-text Bibles of [scrollmapper/bible_databases](https://github.com/scrollmapper/bible_databases), made from [CrossWire](https://crosswire.org/sword/modules/ModDisp.jsp?modType=Bibles)'s modules; CrossWire has a few more that aren't available as text there. Three include the Apocrypha: the King James Version with the Apocrypha (KJVA), the Douay-Rheims Bible and the Catholic Public Domain Version.

The list of languages and translations is read from the source each time, so new ones appear as they are added there: 140 translations in 56 languages at the time of writing, including Hebrew and Greek texts. The source gives each translation's language only as a code (`es`); the names of the languages, in English and in the language itself, come from Unicode's [Common Locale Data Repository](https://github.com/unicode-org/cldr-json), fetched the first time a language is seen and kept in `languages.cache` (fetched again after a month). For a language Unicode has no names for, such as `hbo`, the English name comes from IANA's [registry of language codes](https://www.iana.org/assignments/language-subtag-registry/language-subtag-registry): Ancient Hebrew, whose texts (the Westminster Leningrad Codex among them) have the vowel points. Some languages, such as Chinese, Japanese and Thai, are often written without spaces between words; as EBaC searches word by word, searching such a translation works poorly, and EBaC says so (on that language's list, and after adding one whose text has no spaces).

To add one:

1. At the main menu, press **A**, then **S**.
2. EBaC lists the languages there are translations in, each by its own name and its name in English, with the number of translations: `1) English (34)`, then, in order of their English names, such as `Español, Spanish (4)` and `Deutsch, German (12)`, with a `*` before those that have translations added. Type a language's number and press Enter.
3. EBaC lists that language's translations, a screenful at a time, with a `*` before those already added. Type the numbers of those you want and press Enter (such as `6`, or several, such as `6 14 31`; short names such as `BSB` work too). Enter alone goes back to the languages, and Enter alone there, back to EBaC.
4. For each one, EBaC shows its license (such as `Public Domain`, `GPL`, or `Copyrighted; Free non-commercial distribution`) and the address of its page at CrossWire, and asks whether you agree to it: `y` adds it, `n` skips it.
5. EBaC downloads the text, splits it into a file per book in a folder of its own (such as `BSB`) with its `booklist`, builds its concordance index, checks that the index has every verse, and adds it at the top of `trans`. It takes a few seconds per translation. When it is done, read what it says and press any key: you are back at the main menu, reading from the new translation.

To remove one: **A**, **S**, then **r** and its number, such as `r6` (see [Removing a translation](#removing-a-translation)).

**A**, **S** runs `add-translation.sh`, in the EBaC folder (with `importer.sh`, which it shares with `add-thiagobodruk.sh`): a shell script, which needs only the standard shell and awk, and `curl` or `wget` to download (Linux, macOS, WSL and Solaris have them, and so does the Home Assistant app). It can also be run by itself:

```sh
cd ~/ebac
sh add-translation.sh              # list the translations, and ask which to add or remove
sh add-translation.sh BSB LEB      # add these
sh add-translation.sh --remove BSB # remove this one
sh add-translation.sh --list       # just list them, by language
sh add-translation.sh --agree BSB  # from a script: agree to its license
```

Run from a script, with no one to answer the license question, a translation is added only with `--agree`, by which you agree to the license of each one named; the licenses are shown either way.

### From Project Gutenberg (P)

Some texts EBaC can read, such as the Book of Mormon, aren't in either collection on GitHub, but are on [Project Gutenberg](https://www.gutenberg.org) as plain text in chapters and verses. EBaC knows particular ones (listed below) by their ebook numbers. To add one:

1. Find it on gutenberg.org with its search, and check it is the one you want.
2. Note its ebook number: the **EBook-No.** on its page, or the number in its address (`gutenberg.org/ebooks/17` is 17, the Book of Mormon).
3. At EBaC's main menu, press **A**, then **P**, type the number and press Enter.

EBaC first reads that ebook's page on Gutenberg (`https://www.gutenberg.org/ebooks/17`, a few kilobytes), shows its title, when it was last updated and its copyright status, and asks whether you agree to its license. Only then, and only if there is room, does it download the plain text in UTF-8 (`https://www.gutenberg.org/ebooks/17.txt.utf-8`), split it into its books, index it, and add it at the top of `trans`, as **A** does; so a text you don't want is never downloaded. Its folder's `license` file keeps what you agreed to and the terms the text itself gives. (If the page can't be read, the text is downloaded first and its license read from it, as before.) The same version isn't downloaded twice: remove it first (**A**, **P**, then **r** and its number). (A text added before EBaC changed how it lays texts out, such as a Douay-Rheims added before its notes were marked, is added again over the old copy, in the new layout.) Each removes only its own texts: **P** those from Project Gutenberg, **S** and **T** those from GitHub. A text from Project Gutenberg has `-PG` at the end of its short name (the Book of Mormon is `BOM-PG`), so that the same Bible from both places, such as the Douay-Rheims (`DRC` from GitHub, `DRC-PG` from Project Gutenberg), can be kept side by side and compared; a text added before under its name without `-PG` is renamed so the next time **P** is used.

Gutenberg's texts are laid out in many ways, so EBaC knows each one by its ebook number, and is taught them one at a time; a number it doesn't know yet is refused, saying so. It knows:

| Ebook | Short name | Text |
|---|---|---|
| 17 | `BOM-PG` | The Book of Mormon |
| 1581 | `DRC-PG` | The Douay-Rheims Bible (Challoner revision), with Challoner's notes, each book's introduction and each chapter's summary, and the edition's appendices |
| 8294 | `WEB-PG` | The World English Bible, with its notes in {braces} as printed (searches find the words around a note; voice leaves notes out) |
| 16955 | `YUSUFALI-PG`, `PICKTHALL-PG`, `SHAKIR-PG` | The Qur'an in three translations (Abdullah Yusuf Ali, Marmaduke Pickthall, Mohammad Habib Shakir), added as three translations: to compare them side by side, open EBaC three times, one in each. One book, the Qur'an, with the surahs as its chapters (Qur'an 2:255), each surah's heading before its verse 1 |

Nothing the text gives is left out. What comes before each book's chapter 1 (its heading and summary; before the first book, the front matter too: in the Book of Mormon, the title page, the testimonies of the witnesses with their names, the contents) is kept as that book's chapter 0, row by row as the text lays it out: each title line, name or line of contents its own row, blank lines as blank rows, and each paragraph one row, wrapped to your screen (a line of the text is joined to the next only where the text wrapped it). EBaC shows chapter 0 as plain text above verse 1, without the 0s, which aren't in the text; searches find it, and comma or the up arrow from 1:1 goes back through it (references such as `1 Nephi 0:19` are shown in a list of search results). A verse taller than the screen, as a long paragraph can be on a phone, is shown a screenful at a time. In the Douay-Rheims, Challoner's notes are kept with the verse each follows, on lines of their own under it, each note's first line marked `---` in the margin where a verse's number goes, so that someone reading the verses aloud sees where the verse ends and the note begins (searches find notes; voice leaves them out), each chapter's summary and each psalm's title come before its verse 1, and the appendices (the Prayer of Manasses, 3 and 4 Esdras, and others from the 1582 and 1609-1610 printings, in their own spelling) are books at the end, as printed.

**A**, **P** runs `add-gutenberg.sh`, a shell script beside `add-translation.sh`, which can also be run by itself:

```sh
cd ~/ebac
sh add-gutenberg.sh 17
sh add-gutenberg.sh --remove BOM-PG
sh add-gutenberg.sh --list
```

### For all three places

#### Licenses

Not every translation is free to copy for any use, so EBaC shows each one's license and asks whether you agree to it every time, a fresh copy too, since a license can change. What you agreed to, and when, is kept in the translation's folder, in a file named `license`.

#### Adding one you already have

Choosing a translation already added downloads it again, as a fresh copy, and moves it to the top, but only if its text has changed in the source since you downloaded it: if it is the same version, EBaC says so ("you already have this version") and changes nothing. To download the same version again, remove it first (`r` and its number), then add it. Each translation's folder keeps a file named `version` for this: the text's fingerprint (the ETag the web server gives the file, which changes only when the text does, asked for without downloading it) and the date it was downloaded.

#### Removing a translation

On the list of translations, type **r** and its number, such as `r6`: its folder and its line in `trans` are deleted, so it is no longer offered, and its `*` is gone. Adding and removing can be mixed in one line, done in the order typed: `r10 11 13` removes 10 and adds 11 and 13. If anything in the line is wrong (a number not in the list, or `r` for a translation that isn't added), nothing is done, and the list says what was wrong. Each place removes only its own translations: **T** those from thiagobodruk/bible, **S** those from scrollmapper/bible_databases, **P** those from Project Gutenberg. Only added translations can be removed, not the King James (`AV`) that comes with EBaC.

Translations are added and removed for everyone using this copy of EBaC: another user's EBaC, or a voice assistant (the Home Assistant integration), may add or remove one while you are reading. EBaC reads `trans` again each time you press **C**, so the list always shows the translations as they are now. If the translation you are reading is removed, EBaC says so ("The translation KJ2 has been removed") and carries on with the first translation still in `trans`, rather than stopping.

#### Disk space

Adding needs at least 1 GB of free disk space where EBaC keeps its translations, and where it downloads them first (the temporary folder, `/tmp` unless `TMPDIR` says otherwise): with less on either, it says "not enough disk space" and adds nothing (removing still works). This keeps a script, or anyone speaking to a voice assistant, from filling the disk by adding one translation after another. Adding from Project Gutenberg checks the same.

#### Which translation EBaC uses

A translation just added is put first in `trans`, so EBaC reads from it at once, and starts with it from then on too.

On the **C** screen, a translation's number chooses it for now, and **d** before the number chooses it and makes it the default, the one EBaC starts with from then on: **d** then **3**, say (its line is moved to the top of `trans`, and the main menu says "now the default"). With more than nine translations, the **C** screen shows them a page at a time: type a number and press Enter, or **d** and a number, such as `d12` (**space** and **p** turn the page, **x** goes back). Adding a translation changes `trans`; if a later `git pull` then says `Your local changes ... would be overwritten`, see [If something goes wrong](#if-something-goes-wrong). In the Home Assistant app, added translations are kept in the app's own storage, so updates of the app keep them (and Home Assistant's backups include them).

#### Book names

Each translation keeps its own book names, as one word (EBaC's book prompt and `@book` searches take one word), shortened to fit the list of search results: `I Samuel` becomes `ISamuel`, so type `isam` for it, or `1s`: a number typed at the start of a book's name also matches a Roman numeral there, so `2th` finds `IIThessalonians` as well as `2Thessalonians`, in any translation (at the book prompt and in `@book` searches). A book's whole name, typed in full, is always that book, even where other names begin with it: `esther` is Esther, and `estherg` the longer one after it. A name that is still the start of several books lists them, one to a line, so you can see what more to type. Every character is kept as the source has it, curly quotes, dashes and odd spacing too (only other kinds of space become a plain one, as EBaC finds words between plain spaces). A search typed with a plain apostrophe finds a word however the text writes it: `don't` finds `don’t` (every kind of apostrophe counts as `'` where words are compared). Verse numbers keep the brackets they have there, `[1:1]`: EBaC reads both `[1:1]` and `1:1`.

#### Behind the scenes

Behind the scenes, EBaC hands over to this script (EBaC is replaced by it, so its memory is freed meanwhile), and when it is done, it starts a new EBaC, which reads `trans` again and so offers the new translation. EBaC can be started with any translation in `trans` the same way: `~/ebac/bible BSB`.

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

[↑ Contents](#contents)

## History

EBaC was begun in 1992 on the MM/1 (OS-9 68000), at first searching for a single word; searches combining several words with operators came in 1993. It was then ported, in turn, to VAX/VMS, the Tandy Color Computer 3 (OS-9 6809, 512K), the AT&T 3B1 (UNIX System V), MS-DOS and Windows (with Cygwin), then the UNIX systems of the 1990s: Sun's SunOS 4 and Solaris 2, DEC's Ultrix and OSF/1, and HP-UX on the HP 9000; the Information screen (**I** at the main menu) lists them in that order. Version 1.3.0 was published on [SourceForge](https://sourceforge.net/projects/ebac/) in 2009. Since 2026 it runs on Linux, macOS and Windows (WSL), and, last, as a [Home Assistant app](https://github.com/neilbrookins/ebac-ha-app): the one with the most features, as its touch screen and mouse support (tapping and swiping) are its own.

This repository starts with that release, unchanged, as its first commit and tagged **`v1.3.0`**. Each step needed to bring it up to date is a separate commit after it, with an explanation.

To build EBaC on one of those computers, see [Installing on vintage computers](#installing-on-vintage-computers).

[↑ Contents](#contents)

## What changed in 2.0

Modern compilers (GCC 14 and later, Clang 16 and later, and the C23 standard) reject the 1980s K&R style C the program was written in. The termcap library and `<sys/termio.h>` it used are gone from current Linux systems. Version 2.0:

- uses the **ncurses** library (its terminfo interface) instead of termcap, and POSIX `termios` instead of System V `termio`;
- has ANSI function prototypes and definitions throughout;
- fixes bugs that newer compilers and 64-bit, ARM (Raspberry Pi) systems exposed, plus a crash when `LOGNAME` is unset and an exit on malformed searches;
- follows the terminal window when it is resized.

The concordance index built on 64-bit Linux is byte-for-byte identical to the one shipped in 1.3.0. Since it can be rebuilt in seconds, it is no longer stored in the repository: `make` builds it. The `v1.3.0` release still includes its `.dat` files, for computers such as the CoCo that don't have the memory to run `index`.

[↑ Contents](#contents)

## For other programs: ebac-api

`make` also builds `ebac-api`, which answers requests in JSON instead of showing screens, for other programs such as Home Assistant's voice assistant. It understands references as they are spoken: `Genesis one colon one`, `John three sixteen`, `first Corinthians thirteen four`, `Jude three`. A web server runs it as a CGI program; you can also run it by hand, in the `ebac` folder:

```sh
export BIBLEROOT=$PWD
./ebac-api 'verse?ref=john+three+sixteen'
./ebac-api 'next?svn=26137'          # the verse after John 3:16
./ebac-api 'search?q=faith+hope&max=5'
```

Its requests (`translations`, `books`, `numbers`, `parse`, `verse`, `next`, `previous`, `search`, `info`, `license`) are described in [`readme.architecture`](readme.architecture), under "Voice". A translation's `booknames` file gives its books' full names to speak (`Song of Solomon`); downloading a translation writes it.

`ebac-api` only reads. A second CGI program, [`ebac-admin`](ebac-admin) (a shell script), adds and removes translations for other programs, with `add-translation.sh`. It does nothing that changes the disk unless the web server's setup allows it: `EBAC_ALLOW_ADD=yes` for adding, and `EBAC_ALLOW_REMOVE=yes` as well for removing. Its requests (`permissions`, `catalog`, `license`, `check`, `add`, `status`, `remove`) are described at its top. An add runs in the background and agrees to the translation's license as the request says (`agreed=by voice, from Kitchen`), recorded in the translation's `license` file. It goes at the end of `trans`, so EBaC still starts with the same translation.

[↑ Contents](#contents)

## Testing

EBaC compiles two ways. `-DNORAMDISK` keeps search results in memory. `-DRAMDISK` keeps them in temporary files instead, for machines with little memory, such as the CoCo. The two are different code, but a user must never be able to tell them apart.

```sh
make test
```

This builds both versions and runs them side by side through a set of scripted sessions: viewing, every kind of search, paging, and invalid input. It checks that the screens they produce are byte-for-byte identical. Every change to the code should keep this test passing, so the two versions never drift apart. To add memory-error checking as well, run `make test TESTSAN="-fsanitize=address,undefined"`. The test needs Python 3; to add a scenario, add a line to `SCENARIOS` in [`tests/sidebyside.py`](tests/sidebyside.py).

`make navtest` checks the passage screen's keys (**n p . , f b**) from every screen of a made-up translation with verses taller than the screen, at a phone's size and at 80 columns: each must move as it should, except at the first or last verse. It takes about ten minutes.

[↑ Contents](#contents)

## License

GNU General Public License, version 2 — the license the project has on SourceForge. See [`COPYING`](COPYING).

[↑ Contents](#contents)
