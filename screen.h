/*
   EBaC (Electronic Bible and Concordance)
   written by Neil G. Brookins
   http://ebac.sourceforge.net/
*/
/* screen.h -- terminal control (was curses.h) */

/*
** On UNIX the terminal capabilities come from the terminfo interface of
** the ncurses library (see curses.c). Define USE_TERMCAP to build a UNIX
** system that still has the old termcap library instead.
** The other systems keep their original code: termcap on OS-9/68000, and
** fixed escape sequences in cure.c for the CoCo, VMS and MS-DOS.
*/
#if defined(UNIXV) && !defined(USE_TERMCAP)
#       define USE_NCURSES
#endif

#ifndef FALSE
# define TRUE            1
# define FALSE           0
#endif

/* extern variables, defined in curses.c or cure.c */
extern int      s_lines;          /* :li#: number of rows */
extern int      s_cols;           /* :co#: number of columns */
extern char     *SO;            /* :so=: standout start */
extern char     *SE;            /* :se=: standout end */
extern char     *US;            /* :us=: underline start */
extern char     *UE;            /* :ue=: underline end */
extern char     *CL;            /* :cl=: clear screen */
extern char	*ND;		/* :nd=: non-destructive space */

#ifdef USE_NCURSES

/* send one terminal control string to the screen; defined in curses.c */
extern void     scr_put(char *s);

#define standout()      scr_put(SO)
#define standend()      scr_put(SE)
#define underline()     scr_put(US)
#define underend()      scr_put(UE)
#define cls()           scr_put(CL)
#define nd_space()	scr_put(ND)

#else /* termcap, or the fixed sequences in cure.c */

/* termcap library (cure.c supplies its own tputs) */
extern int      tgetent(char *bp, char *name);
extern int      tgetnum(char *id);
extern char     *tgetstr(char *id, char **area);
extern char     *tgoto(char *cap, int col, int row);
extern int      tputs(char *str, int affcnt, int (*outc)(int));

extern char     *termtype;      /* name of terminal entry */
extern short    ospeed;         /* tty speed, eg B2400 */
#ifdef OSK
extern char PC_;        /* Pad char */
#else
extern char PC;         /* Pad char */
#endif
extern char     ERASEKEY;       /* taken from the ioctl structure */


extern int fputchar(int c);

#define standout()      tputs(SO, 1, fputchar)
#define standend()      tputs(SE, 1, fputchar)
#define underline()     tputs(US, 1, fputchar)
#define underend()      tputs(UE, 1, fputchar)
#define cls()           tputs(CL, 1, fputchar)
#define nd_space()	tputs(ND, 1, fputchar)

#endif /* USE_NCURSES */
