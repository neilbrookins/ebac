/* 
   EBaC (Electronic Bible and Concordance)
   written by Neil G. Brookins
   http://ebac.sourceforge.net/
*/
/*
**This file contains all declarations common to BOTH the index
** generator and the bible program.
*/

#ifndef BOTH_H
#define BOTH_H

#ifdef VMS
#       include stdio.h
#       include ctype.h
#else
#       include <stdio.h>
#       include <ctype.h>
#endif

/*
** K&R ("traditional") C, for compilers older than the 1989 ANSI standard,
** such as Microware C on the CoCo. The source is ANSI C; "make coco" runs
** it through ansi2knr, which rewrites the function definitions. The rest
** is done here:
**   P((int a, char *b))  the parameters of a declaration, or () for K&R
**   UCHAR(c)             c as an unsigned char, for the ctype functions
**                        (the CoCo's compiler has no unsigned char)
**   NOVOID               the compiler has no "void": use int instead
*/
#ifdef COCO
#       define KNR
#       define NOVOID
#endif
#ifdef KNR
#       define P(args) ()
#       define UCHAR(c) (c)
#else
#       define P(args) args
#       define UCHAR(c) ((unsigned char) (c))
#endif
#ifdef NOVOID
#       define void int
#endif


/*
** The first line of format.dat, which the index program writes with the
** index files: EBaC reads a translation's index only if it is this format.
** (Format 1, before EBaC 2.1, had no format.dat. See readme.architecture.)
*/
#define INDEX_FORMAT "EBaC index format 2"

/*
** In position.dat and plainpos.dat, the flag on the position of the last
** word of a verse (and of its last word not in brackets): so that a phrase
** can run on from it into the next verse. (See readme.architecture.)
*/
#define LAST_POSITION 0x40000000L

/*
** VMS has the binary "b" mode like the Macintosh, but when I tested it,
** it behaved the same as the non-binary mode. I couldn't tell any difference.
** MSDOS requires binary to not convert CR/LF <---> LF
*/
#ifdef MSDOS
#       define READMODE     "rb"
#       define WRITEMODE    "wb"
#       define WRITE_UPDATE "w+b"
#	define APPENDMODE   "ab"
#else
#       define READMODE     "r"
#       define WRITEMODE    "w"
#       define WRITE_UPDATE "w+"
#	define APPENDMODE   "a"
#endif

/* This is the End Of Record marker used in variable length record files */
/* #define EOR '\xff' */
#define EOR 255
#ifdef COCO /* without "unsigned chars" the coco represents 255 as -1 */
#	define S_EOR (-1)
#endif

#ifdef VMS
        int ugetchar P((void));
#       define malloc VAXC$MALLOC_OPT /* these run faster, and */
#       define free VAXC$FREE_OPT     /* have the same functionality */
#endif

#ifdef OSK
#       define ugetchar() getchar()
#endif
#ifdef COCO
#       define ugetchar() getchar()
#endif
#ifdef UNIXV
#	include <stdlib.h>	/* malloc, free, exit, getenv, atoi */
#	include <string.h>	/* strlen, strcpy, strcmp, ... */
#	include <unistd.h>	/* read, chdir, unlink, getpid */
	int ugetchar P((void));
#	define TRUE 1
#	define FALSE 0
#endif
#ifdef MSDOS
# 	define ugetchar() getch()
#	include <malloc.h>
#	include <string.h>
#	include <stdlib.h>
#	define TRUE 1
#	define FALSE 0
#endif


/* 0, 1, or 2 mean: enabled, about to enable, disabled respectively. */
extern int disable_newverse;

#endif /* BOTH_H */
