/* 
   EBaC (Electronic Bible and Concordance)
   written by Neil G. Brookins
   http://ebac.sourceforge.net/
*/
/* cure.c */

#include "screen.h"
#include <stdio.h>
#ifdef VMS
#       include curses
#endif

/* variables, publicly available & used in the macros */
char    *termtype;      /* name of terminal entry */
short   ospeed;         /* speed of the tty, eg B2400 */
#ifdef COCO
char    PC_;    /* Pad char */
#else
char    PC;             /* Pad char */
#endif
int     s_lines;          /* :li#: number of rows */
int     s_cols;           /* :co#: number of columns */
char    *SO = "";       /* :so=: standout start */
char    *SE = "";       /* :se=: standout end */
char    *US = "";       /* :us=: underline start */
char    *UE = "";       /* :ue=: underline end */
char    *CL;            /* :cl=: clear screen */
char	*ND;		/* :nd: non-destructive space */

/* Used by tputs() */
int fputchar(c)
int c;
{
        putchar(c);
        return 0;
}

tputs(cp, i, fp)
char *cp;
int i;
int (*fp)();
{
        fputs(cp, stdout);
}

/* Initialize the Curses package. */
initscreen()
{
#ifdef COCO
        s_lines = 24;
        s_cols = 80;
        SO = "\037\040\0<--reverse video on";
        SE = "\037\041\0<--normal video";
        US = "\037\042\0<--underline on";
        UE = "\037\043\0<--underline off";
        CL =       "\f\0<--clear screen";
	ND =     "\006\0<--cursor right";
#endif
#ifdef VMS
        initscr();
        s_lines = LINES;
        s_cols = COLS;
        SO = "\033[7m";
        SE = "\033[0m";
        US = "\033[4m";
        UE = "\033[0m";
        CL = "\033[2J\033[;H"; /* Clear screen and home cursor */
	ND = "\033[C";
#endif
#ifdef MSDOS
        s_lines = 25;
        s_cols = 80;
        SO = "\033[7m";
        SE = "\033[0m";
        US = "\033[4m";
        UE = "\033[0m";
        CL = "\033[2J";
	ND = "\033[C";
#endif
}

