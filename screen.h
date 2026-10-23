/* 
   EBaC (Electronic Bible and Concordance)
   written by Neil G. Brookins
   http://ebac.sourceforge.net/
*/
/* screen.h -- terminal control (was curses.h) */

extern char     *tgoto();
extern char     *tgetstr();
extern      tputs();

#ifndef FALSE
# define TRUE            1
# define FALSE           0
#endif

/* extern variables, defined in curses.c */
extern char     *termtype;      /* name of terminal entry */
extern short    ospeed;         /* tty speed, eg B2400 */
#ifdef OSK
extern char PC_;        /* Pad char */
#else
extern char PC;         /* Pad char */
#endif
extern int      s_lines;          /* :li#: number of rows */
extern int      s_cols;           /* :co#: number of columns */
extern char     *SO;            /* :so=: standout start */
extern char     *SE;            /* :se=: standout end */
extern char     *US;            /* :us=: underline start */
extern char     *UE;            /* :ue=: underline end */
extern char     *CL;            /* :cl=: clear screen */
extern char	*ND;		/* :nd=: non-destructive space */
extern char     ERASEKEY;       /* taken from the ioctl structure */


extern int fputchar();

#define standout()      tputs(SO, 1, fputchar)
#define standend()      tputs(SE, 1, fputchar)
#define underline()     tputs(US, 1, fputchar)
#define underend()      tputs(UE, 1, fputchar)
#define cls()           tputs(CL, 1, fputchar)
#define nd_space()	tputs(ND, 1, fputchar)

