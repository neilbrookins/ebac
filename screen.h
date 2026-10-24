/*
   EBaC (Electronic Bible and Concordance)
   written by Neil G. Brookins
   http://ebac.sourceforge.net/
*/
/* screen.h -- terminal control (was curses.h) */

#include "both.h"       /* P(), for the declarations below */

/*
** On UNIX the terminal capabilities come from the terminfo interface of
** the ncurses library (see curses.c). Define USE_TERMCAP to build a UNIX
** system that still has the old termcap library instead.
** The other systems keep their original code: termcap on OS-9/68000, and
** fixed escape sequences in cure.c for the CoCo, VMS and MS-DOS.
*/
#ifdef UNIXV
#ifndef USE_TERMCAP
#       define USE_NCURSES
#endif
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
extern void     scr_put P((char *s));
/* clear the screen, first picking up any change in the window size */
extern void     scr_clear P((void));
/* undo terminal settings made by initscreen(), before exiting */
extern void     scr_end P((void));
/* readable when the window has been resized (see ugetchar() in file.c) */
extern int      scr_resize_fd P((void));
/* tell a web terminal page which screen is showing (see curses.c) */
extern void     scr_mode P((char *name));

/*
** Keys that send several characters are translated to the single control
** character the MM/1 keyboard sent for the same key, which is what the
** rest of the program understands. scr_key_lookup() matches the
** characters read after an ESC against this terminal's key sequences.
*/
#define KEY_LEFT        0x02    /* CTRL-B */
#define KEY_DEL         0x04    /* CTRL-D */
#define KEY_RIGHT       0x06    /* CTRL-F */
#define KEY_DOWN        0x0e    /* CTRL-N, MM/1 down arrow */
#define KEY_UP          0x10    /* CTRL-P, MM/1 up arrow */
#define KEY_END         0x12    /* CTRL-R */
#define KEY_PGDN        0x16    /* CTRL-V, MM/1 page down */
#define KEY_HOME        0x18    /* CTRL-X */
#define KEY_PGUP        0x1a    /* CTRL-Z, MM/1 page up */
#define KEYSEQ_NONE     0
#define KEYSEQ_PREFIX   1
#define KEYSEQ_MATCH    2
extern int      scr_key_lookup P((char *seq, int len, int *code));

#define standout()      scr_put(SO)
#define standend()      scr_put(SE)
#define underline()     scr_put(US)
#define underend()      scr_put(UE)
#define cls()           scr_clear()
#define nd_space()	scr_put(ND)

#else /* termcap, or the fixed sequences in cure.c */

#define scr_mode(name)  /* only the ncurses build talks to web pages */

/* termcap library (cure.c supplies its own tputs) */
extern int      tgetent P((char *bp, char *name));
extern int      tgetnum P((char *id));
extern char     *tgetstr P((char *id, char **area));
extern char     *tgoto P((char *cap, int col, int row));
extern int      tputs P((char *str, int affcnt, int (*outc)(int)));

extern char     *termtype;      /* name of terminal entry */
extern short    ospeed;         /* tty speed, eg B2400 */
#ifdef OSK
extern char PC_;        /* Pad char */
#else
extern char PC;         /* Pad char */
#endif
extern char     ERASEKEY;       /* taken from the ioctl structure */


extern int fputchar P((int c));

#define standout()      tputs(SO, 1, fputchar)
#define standend()      tputs(SE, 1, fputchar)
#define underline()     tputs(US, 1, fputchar)
#define underend()      tputs(UE, 1, fputchar)
#define cls()           tputs(CL, 1, fputchar)
#define nd_space()	tputs(ND, 1, fputchar)

#endif /* USE_NCURSES */
