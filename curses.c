/* curses.c */

/* Author:
 *      Steve Kirkendall
 *      14407 SW Teal Blvd. #C
 *      Beaverton, OR 97005
 *      kirkenda@cs.pdx.edu
 */


/* This file contains the functions & variables needed for a tiny subset of
 * curses.  The principle advantage of this version of curses is its
 * extreme speed.  Disadvantages are potentially larger code, few supported
 * functions, limited compatibility with full curses, and only stdscr.
 */

#include "screen.h"

#ifdef USE_NCURSES
/*
** UNIX version, rewritten for the ncurses library.
**
** The termcap library (tgetent, tgetstr, tgetnum) is obsolete. This reads
** the same few capabilities through the terminfo interface of ncurses,
** which is the modern equivalent:
**
**      termcap    terminfo
**      tgetent    setupterm
**      tgetstr    tigetstr
**      tgetnum    tigetnum
**        so         smso       standout start
**        se         rmso       standout end
**        us         smul       underline start
**        ue         rmul       underline end
**        cl         clear      clear screen
**        nd         cuf1       non-destructive space (cursor right)
**        li         lines      number of rows
**        co         cols       number of columns
**
** Output still goes through stdio, exactly as before, so the rest of the
** program keeps using printf(). ncurses only supplies the escape
** sequences for this terminal and pads them correctly with tputs().
*/
#include <stdio.h>
#include <stdlib.h>
#include <term.h>

/* variables, publicly available & used in the macros */
int     s_lines;          /* number of rows */
int     s_cols;           /* number of columns */
char    *SO = "";       /* smso: standout start */
char    *SE = "";       /* rmso: standout end */
char    *US = "";       /* smul: underline start */
char    *UE = "";       /* rmul: underline end */
char    *CL;            /* clear: clear screen */
char	*ND;		/* cuf1: non-destructive space */

static char *termtype;  /* name of terminal entry */
static char *default_TERM = "vt100";

/* Used by tputs() to send each character to the screen */
static int fputchar(c)
int c;
{
	return putchar(c);
}

/* Send one terminal control string, with any padding it needs. */
void scr_put(s)
char *s;
{
	tputs(s, 1, fputchar);
}

/* This function fetches an optional string from terminfo */
static char *mayhave(name)
char    *name;  /* terminfo name of the capability */
{
	char    *val;

	val = tigetstr(name);
	if (val == NULL || val == (char *) -1) /* absent, or not a string */
		return NULL;
	return val;
}

/* This function fetches a required string from terminfo */
static char *musthave(name)
char    *name;  /* terminfo name of the capability */
{
	char    *val;

	val = mayhave(name);
	if (val == NULL)
	{
		fprintf(stderr, "The terminfo entry for \"%s\" lacks the %s capability\n",
			termtype, name);
		exit_program(2);
	}
	return val;
}

/*
* This function fetches a pair of strings from terminfo.  If one of them
* is missing, then the other one is ignored.
*/
static void pair(T, U, sT, sU)
char    **T;    /* where to store the first pointer */
char    **U;    /* where to store the second pointer */
char    *sT;    /* name of the first capability */
char    *sU;    /* name of the second capability */
{
	char    *t, *u;

	t = mayhave(sT);
	u = mayhave(sU);
	if (t != NULL && u != NULL)
	{
		*T = t;
		*U = u;
	}
	else
		*T = *U = "";
}

/*
** Get the window size. setupterm() has already replaced the terminfo
** defaults with the real size of the window (or $LINES and $COLUMNS).
*/
getsize()
{
	s_lines = tigetnum("lines");
	s_cols = tigetnum("cols");

	/* Make sure we got values that we can live with */
	if (s_lines < 10 || s_cols < 32)
	{
		fprintf(stderr, "Screen too small\n");
		exit_program(2);
	}
	return 0;
}

/* Initialize the screen package. */
initscreen()
{
	int     err;

	termtype = getenv("TERM");
	if (!termtype || !*termtype)
	{
		fprintf(stderr, "Environment variable TERM must be set!\n");
		fprintf(stderr, "Defaulting to %s...\n", default_TERM);
		termtype = default_TERM;
	}

	/* read the terminfo entry for this terminal */
	if (setupterm(termtype, 1, &err) != 0)
	{
		if (err == 0)
			fprintf(stderr, "Unrecognized TERM type \"%s\"\n", termtype);
		else
			fprintf(stderr, "Can't find the terminfo database\n");
		exit_program(3);
	}

	/* get strings */
	pair(&SO, &SE, "smso", "rmso");
	pair(&US, &UE, "smul", "rmul");
	CL = musthave("clear");   /* Clear Screen */
	ND = musthave("cuf1");    /* non-destructive space */

	getsize();
}

#else /* !USE_NCURSES: the original termcap version */

#ifdef OSK
#	include <sgstat.h>
#endif
#include <stdio.h>

static void      starttcap();

/* variables, publicly available & used in the macros */
char    *termtype;      /* name of terminal entry */
short   ospeed;         /* speed of the tty, eg B2400 */
#if OSK
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
char	*ND;		/* :nd=: non-destructive space */

static char     *capbuf;        /* capability string buffer */
char *default_TERM = "vt100";

/* Used by tputs() */
int fputchar(c)
int c;
{
	putchar(c);
	return 0;
}

/* Initialize the Curses package. */
initscreen()
{
        termtype = (char *) getenv("TERM");
        if (!termtype)
        {
                fprintf(stderr, "Environment variable TERM must be set!\n");
                fprintf(stderr, "Defaulting to vt100...\n");
                termtype = default_TERM;
             /* exit_program(); */
        }
        /* start termcap stuff */
        starttcap(termtype);
}

/* This function fetches an optional string from termcap */
static void mayhave(T, s)
char    **T;    /* where to store the returned pointer */
char    *s;     /* name of the capability */
{
	char    *val;

	val = tgetstr(s, &capbuf);
	if (val)
	{
        	*T = val;
	}
}


/* This function fetches a required string from termcap */
static void musthave(T, s)
char    **T;    /* where to store the returned pointer */
char    *s;     /* name of the capability */
{
  mayhave(T, s);
  if (!*T)
  {
        write(2, "This termcap entry lacks the :", (unsigned)30);
        write(2, s, (unsigned)2);
        write(2, "=: capability\n", (unsigned)14);
#if OSK
        write(2, "\l", 1);
#endif
        exit(2);
  }
}


/*
* This function fetches a pair of strings from termcap.  If one of them is
* missing, then the other one is ignored.
*/
static void pair(T, U, sT, sU)
char    **T;    /* where to store the first pointer */
char    **U;    /* where to store the second pointer */
char    *sT;    /* name of the first capability */
char    *sU;    /* name of the second capability */
{
   mayhave(T, sT);
   mayhave(U, sU);
   if (!**T || !**U)
   {
        *T = *U = "";
   }
}



/* Read everything from termcap */
static void starttcap(term)
char    *term;
{
static char     cbmem[800];
static char     kbuf[1024];

/* allocate memory for capbuf */
capbuf = cbmem;

/* get the termcap entry */
switch (tgetent(kbuf, term))
{
  case -1:
        write(2, "Can't read /etc/termcap\n", (unsigned)24);
#if OSK
        write(2, "\l", 1);
        exit_program(1);
#endif
        exit(2);

  case 0:
        write(2, "Unrecognized TERM type\n", (unsigned)23);
#if OSK
        write(2, "\l", 1);
        exit_program(1);
#endif
        exit(3);
}

/* get strings */
pair(&SO, &SE, "so", "se");
pair(&US, &UE, "us", "ue");
musthave(&CL, "cl");      /* Clear Screen */
musthave(&ND, "nd");      /* non-destructive space */

getsize(0);
}


/* This function gets the window size.  It uses the TIOCGWINSZ ioctl call if
* your system has it, or tgetnum("li") and tgetnum("co") if it doesn't.
* This function is called once during initialization, and thereafter it is
* called whenever the SIGWINCH signal is sent to this process.
*/
int getsize(signo)
int     signo;
{
int     num_lines;
int     num_cols;
#ifdef TIOCGWINSZ
struct winsize size;
#endif

#ifdef SIGWINCH
/* reset the signal vector */
signal(SIGWINCH, getsize);
#endif

/* get the window size, one way or another. */
num_lines = num_cols = 0;
#ifdef TIOCGWINSZ
/* This doesn't work with process launched from inetd */
/* if (ioctl(2, TIOCGWINSZ, &size) >= 0) */
/* { */
        /* num_lines = size.ws_row; */
        /* num_cols = size.ws_col; */
/* } */
num_lines = 24;
num_cols = 80;
#endif
#if AMIGA
/* Amiga gets window size by asking the console.device */
if (!strcmp(TERMTYPE, termtype))
{
    auto long len;
    auto char buf[30];

    Write(Output(), "\2330 q", 4); /* Ask the console.device */
    len = Read(Input(), buf, 29);
    buf[len] = '\000';
    sscanf(&buf[5], "%d;%d", &num_lines, &num_cols);
}
#endif
if ((num_lines == 0 || num_cols == 0) && signo == 0)
{
        s_lines = tgetnum("li");
        s_cols = tgetnum("co");
}
#if MSDOS
# ifdef RAINBOW
if (!strcmp(termtype, "rainbow"))
{
        /* Determine whether Rainbow is in 80-column or 132-column mode */
        num_cols = *(unsigned char far *)0xee000f57L;
}
else
# endif
{
        num_lines = v_rows();
        num_cols = v_cols();
}
#endif
if (num_lines >= 10 && num_cols >= 32)
{
        s_lines = num_lines;
        s_cols = num_cols;
}

/* Make sure we got values that we can live with */
if (s_lines < 10 || s_cols < 32)
{
        write(2, "Screen too small\n", (unsigned)17);
#if OSK
        write(2, "\l", 1);
#endif
        exit(2);
}

#if AMIGA
if (*o_lines != s_lines || *o_columns != s_cols)
{
        *o_lines = s_lines;
        *o_columns = s_cols;
}
#endif

return 0;
}


#endif /* USE_NCURSES */
