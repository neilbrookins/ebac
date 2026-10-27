# makefile for EBaC on the Tandy CoCo 3: OS-9 Level II, Microware C.
#
# These sources were made by "make coco" on another computer: they have
# been converted to K&R C (by ansi2knr), and the concordance index (the
# .dat files in /dd/BIBLE/AV) was built there, as it needs far more memory
# than the CoCo has. The settings are those of EBaC 1.3.0's makefile.

#change this to the name of your C compiler:
CC=cc

CFLAGS= -DRAMDISK -DCOCO
RFILES= bible.o file.o userint.o process.o cure.o utf8.o

OD= -o ./
.c.o:
	$(CC) $(CFLAGS) -c $<
	@echo ' '

bible: $(RFILES)
	$(CC) $(CFLAGS) $(OD)bible $(RFILES)
	@echo done.

file.o: bible.h screen.h both.h
userint.o: bible.h screen.h both.h
cure.o: screen.h both.h
bible.o: bible.h both.h
process.o: bible.h screen.h both.h
utf8.o: utf8.h both.h

#End of Makefile
