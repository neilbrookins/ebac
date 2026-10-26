/* curses.c */

/* A compiler asked for strict ISO C hides POSIX functions such as
** sigaction(); ask for them. (Must come before any #include.) */
#ifdef UNIXV
#ifdef __STRICT_ANSI__
#ifndef _POSIX_C_SOURCE
#	define _POSIX_C_SOURCE 200809L
#endif
#endif
#endif

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
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>          /* TIOCGWINSZ */
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

void exit_program(int errnum);    /* in file.c */

static char *termtype;  /* name of terminal entry */
static volatile sig_atomic_t resized = 0; /* set when the window changes */

/*
** A pipe the resize signal writes a byte to, so that a program waiting
** for a key can also be woken by a resize (see scr_resize_fd()).
*/
static int resize_pipe[2] = { -1, -1 };
static char *mayhave(char *name);
static char *KS = NULL; /* smkx: keypad transmit mode on */
static char *KE = NULL; /* rmkx: keypad transmit mode off */

/*
** Keys that send a sequence of characters (arrows, Home, End, ...).
** EBaC was written on the MM/1, whose keyboard sent a single control
** character for each of these keys, and the rest of the program still
** works in those characters. ugetchar() reads the sequence a key sends
** on this terminal and hands the program the MM/1 character instead,
** so every key behaves the same on every terminal.
*/
#define MAXKEYS 48
static struct
{
	char    *seq;   /* what the terminal sends */
	int     code;   /* what EBaC sees */
} keys[MAXKEYS];
static int nkeys = 0;

/* Remember one key. Only sequences that begin with ESC are translated;
** a key that sends a single character is already in the program's terms. */
static void add_key(char *seq, int code)
{
	if (seq == NULL || seq[0] != '\033' || seq[1] == '\0' || nkeys >= MAXKEYS)
		return;
	keys[nkeys].seq = seq;
	keys[nkeys].code = code;
	nkeys++;
}

/*
** Read the key sequences for this terminal from terminfo, then add the
** common ANSI/VT100 sequences, in both cursor-key modes, as a fallback
** for terminals whose terminfo entry is incomplete or TERM is set wrong.
** Entries found first win, so the terminal's own description comes first.
*/
static void init_keys(void)
{
	static char *fallback[] = {
		"\033[A", "\033OA", "\033[B", "\033OB",     /* up, down */
		"\033[C", "\033OC", "\033[D", "\033OD",     /* right, left */
		"\033[H", "\033OH", "\033[1~", "\033[7~",   /* home */
		"\033[F", "\033OF", "\033[4~", "\033[8~",   /* end */
		"\033[5~", "\033[6~", "\033[3~"             /* page up/down, delete */
	};
	static int fallback_code[] = {
		KEY_UP, KEY_UP, KEY_DOWN, KEY_DOWN,
		KEY_RIGHT, KEY_RIGHT, KEY_LEFT, KEY_LEFT,
		KEY_HOME, KEY_HOME, KEY_HOME, KEY_HOME,
		KEY_END, KEY_END, KEY_END, KEY_END,
		KEY_PGUP, KEY_PGDN, KEY_DEL
	};
	int     i;

	add_key(mayhave("kcuu1"), KEY_UP);     /* up arrow */
	add_key(mayhave("kcud1"), KEY_DOWN);   /* down arrow */
	add_key(mayhave("kcuf1"), KEY_RIGHT);  /* right arrow */
	add_key(mayhave("kcub1"), KEY_LEFT);   /* left arrow */
	add_key(mayhave("khome"), KEY_HOME);   /* Home */
	add_key(mayhave("kend"),  KEY_END);    /* End */
	add_key(mayhave("kpp"),   KEY_PGUP);   /* Page Up */
	add_key(mayhave("knp"),   KEY_PGDN);   /* Page Down */
	add_key(mayhave("kdch1"), KEY_DEL);    /* Delete */
	for (i = 0; i < (int) (sizeof(fallback_code) / sizeof(fallback_code[0])); i++)
		add_key(fallback[i], fallback_code[i]);
}

/*
** Purpose: look up the characters read so far after an ESC.
** Returns: KEYSEQ_MATCH (and the key's code in *code) if they are a whole
**          key sequence, KEYSEQ_PREFIX if they are the start of one, or
**          KEYSEQ_NONE if no key sends them.
*/
int scr_key_lookup(char *seq, int len, int *code)
{
	int     i, prefix = FALSE;

	for (i = 0; i < nkeys; i++)
	{
		int keylen = (int) strlen(keys[i].seq);

		if (keylen < len || memcmp(keys[i].seq, seq, len) != 0)
			continue;
		if (keylen == len)
		{
			*code = keys[i].code;
			return KEYSEQ_MATCH;
		}
		prefix = TRUE;
	}
	return prefix ? KEYSEQ_PREFIX : KEYSEQ_NONE;
}

/* Undo what initscreen() did to the terminal, before the program exits. */
void scr_end(void)
{
	if (KE != NULL)
	{
		scr_put(KE);
		fflush(stdout);
	}
}
static char *default_TERM = "vt100";

/* Used by tputs() to send each character to the screen */
static int fputchar(int c)
{
	return putchar(c);
}

/* Send one terminal control string, with any padding it needs. */
void scr_put(char *s)
{
	tputs(s, 1, fputchar);
}

/* This function fetches an optional string from terminfo */
static char *mayhave(char *name)         /* terminfo name of the capability */
{
	char    *val;

	val = tigetstr(name);
	if (val == NULL || val == (char *) -1) /* absent, or not a string */
		return NULL;
	return val;
}

/* This function fetches a required string from terminfo */
static char *musthave(char *name)        /* terminfo name of the capability */
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
static void pair(char **T,               /* where to store the first pointer */
                 char **U,               /* where to store the second pointer */
                 char *sT,               /* name of the first capability */
                 char *sU)               /* name of the second capability */
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

#ifdef SIGWINCH
/* The window was resized. Note it; the new size is read at the next screen
** clear, which is when the program lays out a new page. Also wake anyone
** waiting for a key, through the pipe, so the page can be redrawn now. */
static void winch(int signo)
{
	int saved_errno = errno;

	(void) signo;
	resized = 1;
	if (resize_pipe[1] >= 0 && write(resize_pipe[1], "r", 1) < 0)
	{
		/* pipe full: a wake-up is already waiting */
	}
	errno = saved_errno;
}
#endif

/*
** Purpose: the file descriptor that becomes readable when the window is
**          resized, for the keyboard reader to watch; -1 if there is none.
**          Reading from it (it never blocks) clears the wake-up.
*/
int scr_resize_fd(void)
{
	return resize_pipe[0];
}

/*
** Purpose: say which screen is showing, for a web page that runs EBaC in
**          a terminal and lets the user tap and swipe (the Home Assistant
**          app). It sends a private escape sequence, "ESC ] 7766 ; name
**          BEL", that the page watches for. Screens:
**            menu     main menu          trans    choose a translation
**            transenter  the same, with more than 9: a number and Enter
**            passage  reading a passage  list     list of search results
**            anykey   "Press any key"    input    typing a book or number
**            search   typing a search
**          Only sent when EBAC_GESTURES is set in the environment, so an
**          ordinary terminal never sees it.
*/
/*
** Purpose: the terminal library and its version, for the Information
**          screen (such as "ncurses 6.4.20240113"; macOS has 5.7).
*/
extern const char *curses_version(void);     /* in ncurses, from curses.h */

char *scr_library(void)
{
	return (char *) curses_version();
}

void scr_mode(char *name)
{
	static int enabled = -1;

	if (enabled < 0)
		enabled = (getenv("EBAC_GESTURES") != NULL);
	if (enabled)
	{
		printf("\033]7766;%s\007", name);
		fflush(stdout);
	}
}

/* Clear the screen, using the new window size if it has changed. */
void scr_clear(void)
{
#ifdef TIOCGWINSZ
	struct winsize size;

	if (resized)
	{
		resized = 0;
		if (ioctl(1, TIOCGWINSZ, &size) == 0 &&
		    size.ws_row >= 10 && size.ws_col >= 32)
		{
			s_lines = size.ws_row;
			s_cols = size.ws_col;
		}
	}
#endif
	tputs(CL, 1, fputchar);
}

/*
** Get the window size. setupterm() has already replaced the terminfo
** defaults with the real size of the window (or $LINES and $COLUMNS).
*/
static void getsize(void)
{
	s_lines = tigetnum("lines");
	s_cols = tigetnum("cols");

	/* Make sure we got values that we can live with */
	if (s_lines < 10 || s_cols < 32)
	{
		fprintf(stderr, "Screen too small\n");
		exit_program(2);
	}
}

/* Initialize the screen package. */
void initscreen(void)
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
#ifdef SIGWINCH
	{
		struct sigaction sa;

		/* Neither end of the pipe may block, nor pass to other programs. */
		if (pipe(resize_pipe) == 0)
		{
			fcntl(resize_pipe[0], F_SETFL, O_NONBLOCK);
			fcntl(resize_pipe[1], F_SETFL, O_NONBLOCK);
			fcntl(resize_pipe[0], F_SETFD, FD_CLOEXEC);
			fcntl(resize_pipe[1], F_SETFD, FD_CLOEXEC);
		}
		else
			resize_pipe[0] = resize_pipe[1] = -1;

		memset(&sa, 0, sizeof(sa));
		sa.sa_handler = winch;
		sigemptyset(&sa.sa_mask);
		sa.sa_flags = SA_RESTART; /* other system calls just carry on */
		sigaction(SIGWINCH, &sa, NULL);
	}
#endif

	/*
	** Many terminals send different sequences for the arrow keys unless
	** they are told to "transmit" the keypad; the key sequences in
	** terminfo are the ones sent in that mode, so switch it on.
	*/
	KS = mayhave("smkx");
	KE = mayhave("rmkx");
	if (KS != NULL)
	{
		scr_put(KS);
		fflush(stdout);
	}
	init_keys();
}

#else /* !USE_NCURSES: the original termcap version */

#ifdef OSK
#	include <sgstat.h>
#endif
#include <stdio.h>
#ifdef UNIXV
#	include <stdlib.h>
#	include <unistd.h>
#	include <signal.h>
#endif

static void      starttcap(char *term);
static void      getsize(int signo);

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
int fputchar(int c)
{
	putchar(c);
	return 0;
}

/* Initialize the Curses package. */
void initscreen(void)
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
static void mayhave(char **T,            /* where to store the returned pointer */
                    char *s)             /* name of the capability */
{
	char    *val;

	val = tgetstr(s, &capbuf);
	if (val)
	{
        	*T = val;
	}
}


/* This function fetches a required string from termcap */
static void musthave(char **T,           /* where to store the returned pointer */
                     char *s)            /* name of the capability */
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
static void pair(char **T,               /* where to store the first pointer */
                 char **U,               /* where to store the second pointer */
                 char *sT,               /* name of the first capability */
                 char *sU)               /* name of the second capability */
{
   mayhave(T, sT);
   mayhave(U, sU);
   if (!**T || !**U)
   {
        *T = *U = "";
   }
}



/* Read everything from termcap */
static void starttcap(char *term)
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
static void getsize(int signo)
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

return;
}


#endif /* USE_NCURSES */
