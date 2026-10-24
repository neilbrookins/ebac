# makefile for Electronic Bible and Concordance
# Copyright (c) 1993 by Neil Brookins

#change this to the name of your C compiler (gcc and clang both work):
CC=gcc

#If you are using Linux, BSD, macOS, Cygwin, Solaris 11, or another modern
#UNIX (on Windows 10 and 11, WSL is Linux; EBaC 1.3.0 was also used and
#tested at length with Cygwin on Windows): this needs GNU make (on Solaris and BSD it is called gmake) and the
#ncurses development files, which provide <term.h>:
#  Debian, Ubuntu, Raspberry Pi OS:  sudo apt install libncurses-dev
#  Fedora, RHEL:                     sudo dnf install ncurses-devel
#  Arch:                             sudo pacman -S ncurses
#  macOS:                            included with the Xcode tools
#  Solaris 11:                       sudo pkg install library/ncurses
CFLAGS= -O2 -Wall -Wextra -DNORAMDISK -DUNIXV
RFILES= bible.o file.o userint.o process.o curses.o
LIBS= $(NCURSES_LIBS)

# Where ncurses is. pkg-config knows, where it is installed. Without it,
# look for the folder Solaris keeps ncurses' headers in, apart from its own
# older curses (whose <term.h> doesn't suit EBaC); otherwise the compiler's
# usual places are right. Only curses.c includes ncurses' headers.
NCURSES_CFLAGS := $(shell pkg-config --cflags ncurses 2>/dev/null)
NCURSES_LIBS := $(shell pkg-config --libs ncurses 2>/dev/null)
ifeq ($(strip $(NCURSES_LIBS)),)
NCURSES_LIBS := -lncurses
ifneq ($(wildcard /usr/include/ncurses/term.h),)
NCURSES_CFLAGS := -I/usr/include/ncurses
endif
endif

#If you are using an older UNIX that still has termcap instead of ncurses,
#(Solaris before 11, HP-UX, and other System V from the 1990s):
#CFLAGS= -O -DNORAMDISK -DUNIXV -DUSE_TERMCAP
#RFILES= bible.o file.o userint.o process.o curses.o
#LIBS= -ltermcap

#if you are using Microsoft MS-DOS, DOS, with about 512KB RAM:
#Intel 8086 CPU, using Watcom C compiler
#CFLAGS= -DRAMDISK -DMSDOS
#RFILES= bible.o file.o userint.o process.o cure.o
#LIBS=

#If you are using Microware OS-9 68000 on MM/1
#on Philips 68070 CPU, using gcc
#CFLAGS= -DNORAMDISK -DOSK
#RFILES= bible.o file.o userint.o process.o curses.o
#LIBS= -ltermcap

#If you are using Microware OS-9 6809 on Tandy CoCo 3
#on Motorola 6809 CPU, using Microware C
#CFLAGS= -DRAMDISK -DCOCO
#RFILES= bible.o file.o userint.o process.o cure.o
#LIBS=

#If you are using OpenVMS, VAX/VMS:
#on a VAXstation, using Digital C
#then use descrip.mms and options_file.opt instead of this file.
#The cflags -DVMS will be defined by default.

OD= -o ./
.c.o:
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $<

# The translations: the folder names listed in the 'trans' file (see
# translations.sh). Nothing about them is written in this makefile.
TRANS := $(shell sh translations.sh list)

all: bible index indexes

# Bring every translation's concordance index (its .dat files) up to date.
# translation.mk, run inside each translation's folder, reads its booklist
# and runs index there if any .dat file is missing or older than a book.
indexes: index
	@sh translations.sh check
	@for t in $(TRANS); do \
	    $(MAKE) --no-print-directory -C "$$t" -f "$(CURDIR)/translation.mk" \
	        INDEX="$(CURDIR)/index" || exit 1; \
	done

bible: $(RFILES)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OD)bible $(RFILES) $(LIBS)

# this program needs over 7.5 meg RAM to run.
index: index.c both.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $(OD)index index.c

# splits an archive across multiple floppy disks
split: split.c
	$(CC) $(CFLAGS) $(OD)split split.c
splitmsdos: splitmsdos.c
	$(CC) $(CFLAGS) $(OD)splitmsdos splitmsdos.c
splitmm1: splitmm1.c
	$(CC) $(CFLAGS) $(OD)splitmm1 splitmm1.c
lab: lab.c
	$(CC) $(CFLAGS) $(OD)lab lab.c

# counts line lengths.
llength: llength.c
	$(CC) $(CFLAGS) $(OD)llength llength.c

# count number of verses in entire Bible.
verses: verses.c
	$(CC) $(CFLAGS) $(OD)verses verses.c

# Test: build the program twice, once with NORAMDISK (results kept in
# memory) and once with RAMDISK (results kept in temporary files), and
# check that both produce byte-for-byte identical screens for a set of
# scripted sessions. Any change to the code must keep them identical.
# Needs Python 3. For memory checking as well, run:
#   make test TESTSAN="-fsanitize=address,undefined"
TESTCFLAGS= -O1 -g -Wall -Wextra -DUNIXV $(TESTSAN)
TESTSRC= bible.c file.c userint.c process.c curses.c
# The test also checks that every folder is mentioned in 'trans'.
test: $(TESTSRC) bible.h both.h screen.h tests/sidebyside.py indexes
	sh translations.sh check --strict
	rm -rf tests/build
	mkdir -p tests/build/noramdisk tests/build/ramdisk
	$(CC) $(TESTCFLAGS) $(NCURSES_CFLAGS) -DNORAMDISK -o tests/build/noramdisk/bible $(TESTSRC) $(LIBS)
	$(CC) $(TESTCFLAGS) $(NCURSES_CFLAGS) -DRAMDISK -o tests/build/ramdisk/bible $(TESTSRC) $(LIBS)
	python3 tests/sidebyside.py tests/build/noramdisk/bible tests/build/ramdisk/bible

#remove binaries and object files. (The .dat index files are kept.)
# The Tandy CoCo 3 (OS-9 6809): the source converted to K&R C, with the
# text and the concordance index (which the CoCo can't build itself), in
# coco/build/ebac-coco.tar, and coco/build/ebac-coco.dsk, an OS-9 disk
# image, if ToolShed's "os9" program is installed. See coco/mkcoco.sh.
coco: indexes coco/mkcoco.sh coco/splitdefs.awk coco/ansi2knr.c coco/makefile
	mkdir -p coco/build
	CC="$(CC)" sh coco/mkcoco.sh coco/build

clean:
	rm -f index bible split splitmsdos splitmm1 lab llength verses *.o
	rm -rf tests/build coco/build

.PHONY: all indexes test clean coco

file.o: bible.h screen.h both.h
userint.o: bible.h screen.h both.h
curses.o: curses.c screen.h both.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(NCURSES_CFLAGS) -c curses.c
cure.o: screen.h
bible.o: bible.h both.h
process.o: bible.h screen.h both.h

#End of Makefile
