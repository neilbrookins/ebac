! makefile for Electronic Bible
! Copyright (c) 1993 by Neil Brookins

CC=     cc
LINK=    link
CFLAGS= /optimmize/nodebug
OD=     
EXE= .exe
RFILES= bible.obj, file.obj, userint.obj, process.obj, cure.obj, vmsio.obj

bible$(EXE) : $(RFILES)
        $(LINK) $(LINKFLAGS) /nodebug options_file.opt/options, $(RFILES)

! this program needs over 9 meg RAM to run.
index$(EXE) : index.obj both.h
        $(LINK) $(LINKFLAGS) options_file.opt/options, index.obj

! this pauses for a keystroke during installation script.
pause$(EXE) : pause.obj
        $(LINK) $(LINKFLAGS) options_file.opt/options, pause.obj

! counts line lengths.
llength$(EXE) : llength.c
        $(CC) $(CFLAGS) $(OD)llength$(EXE) llength.c

! count number of verses in entire Bible.
verses$(EXE) : verses.c
        $(CC) $(CFLAGS) $(OD)verses$(EXE) verses.c

! test of menus with raw and buffered I/O
menu$(EXE) : menu.c
        $(CC) $(CFLAGS) $(OD)menu$(EXE) menu.c

clean :
        del bible.exe;*, *.obj;*
file.obj : bible.h both.h
userint.obj : bible.h screen.h both.h
bible.obj : bible.h both.h
process.obj : bible.h both.h
vmsio.obj : vmsio.h

!End of Makefile
