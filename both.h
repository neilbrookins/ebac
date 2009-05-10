/* 
   EBaC (Electronic Bible and Concordance)
   written by Neil G. Brookins
   http://ebac.sourceforge.net/
*/
/*
**This file contains all declarations common to BOTH the index
** generator and the bible program.
*/

#ifdef VMS
#       include stdio.h
#       include ctype.h
#else
#       include <stdio.h>
#       include <ctype.h>
#endif


/*
** If a word occurs more than this number of times we do not store its
** verse locations; but we do count how many times it occurs.
*/
#define MAXREF 65000

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
        char ugetchar();
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
	char ugetchar();
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

