# makefile for Electronic Bible and Concordance
# Copyright (c) 1993 by Neil Brookins

#change this to the name of your C compiler:
CC=gcc

#If you are using UNIX, Solaris, HP-UX, Linux, BSD, or Cygwin:
CFLAGS= -O3 -DNORAMDISK -DUNIXV
RFILES= bible.o file.o userint.o process.o curses.o

#if you are using Microsoft MS-DOS, DOS, with about 512KB RAM:
#Intel 8086 CPU, using Watcom C compiler
#remove -ltermcap from below.
#CFLAGS= -DRAMDISK -DMSDOS
#RFILES= bible.o file.o userint.o process.o cure.o

#If you are using Microware OS-9 68000 on MM/1
#on Philips 68070 CPU, using gcc
#CFLAGS= -DNORAMDISK -DOSK
#RFILES= bible.o file.o userint.o process.o curses.o

#If you are using Microware OS-9 6809 on Tandy CoCo 3
#on Motorola 6809 CPU, using Microware C
#remove -ltermcap from below.
#CFLAGS= -DRAMDISK -DCOCO
#RFILES= bible.o file.o userint.o process.o cure.o

#If you are using OpenVMS, VAX/VMS:
#on a VAXstation, using Digital C
#then use descrip.mms and options_file.opt instead of this file.
#The cflags -DVMS will be defined by default.

OD= -o ./
.c.o:
	$(CC) $(CFLAGS) -c $<
	@echo ' '

all: bible index

bible: $(RFILES)
#	quantify $(CC) $(CFLAGS) $(OD)bible $(RFILES) -ltermcap
#	purify $(CC) $(CFLAGS) $(OD)bible $(RFILES) -ltermcap
	$(CC) $(CFLAGS) $(OD)bible $(RFILES) -ltermcap
	@echo done.

# this program needs over 7.5 meg RAM to run.
index: index.c both.h
	$(CC) $(CFLAGS) $(OD)index index.c

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

#remove binaries and object files.
clean:
	rm -f index bible *.o

file.o: bible.h curses.h both.h
userint.o: bible.h curses.h both.h
curses.o: curses.h
bible.o: bible.h both.h
process.o: bible.h curses.h both.h

#End of Makefile
