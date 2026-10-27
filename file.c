/* 
   EBaC (Electronic Bible and Concordance)
   written by Neil G. Brookins
   http://ebac.sourceforge.net/
*/
#include "bible.h"
#include "screen.h"
#ifdef VMS
#       include "vmsio.h"
#       include signal
#endif
#ifdef COCO
#	include <sgstat.h>
	struct sgbuf oldbuffer, newbuffer;
#endif
#ifdef OSK
#	include <sgstat.h>
	struct sgbuf oldbuffer, newbuffer;
#endif
#ifdef UNIXV
#       include <stdlib.h>              /* used by getenv() */
#       include <unistd.h>              /* read(), chdir() */
#       include <errno.h>
#       include <termios.h>             /* POSIX replacement for termio */
#       include <signal.h>
#       include <sys/types.h>
#       include <sys/time.h>            /* struct timeval for select() */
#       include <sys/select.h>          /* select() */
        static struct termios oldtermio; /* original tty mode */
        static struct termios newtermio; /* cbreak/noecho tty mode */
        static int tty_saved = FALSE;    /* TRUE once oldtermio is valid */
#endif

FILE *book_list, *book_file, *cvarray_file, *bvarray_file, *chapter_file,
     *words_file, *references_file, *position_file, *plainpos_file;
int a_new_translation = FALSE;

/* number of allowable translations in trans file is two less than this */
/* how many translations 'trans' may list (fewer on the CoCo, for memory) */
#ifdef COCO
#	define    MAXTRANS 20
#else
#	define    MAXTRANS 200
#endif
char trans[MAXTRANS][80]; /* room for 80 char description of translation */

/* extended three letter acronym, such as NIV or NRSV */
#define   MAX_ETLA 20
char etla[MAX_ETLA];/*global variable with name of current translation in use*/
long int numverses, numchapters, numbooks, sizeofwordsdat;
long int num_found;  /* Number of matches in a word search */
char *bible_root = "";  /* the folder EBaC's files are in (BIBLEROOT) */
char *start_trans = NULL; /* the translation to start with, from "bible BSB" */
#ifdef UNIXV
/* BIBLEROOT as a full path, found once EBaC is in it, so that a translation
   is found as root_dir/NAME. (Not ../NAME: a translation folder may be a
   link to a folder elsewhere, as in the Home Assistant app, whose AV is a
   link into the app's image; ".." from there is the image's folder.) */
static char root_dir[1024] = "";
#endif
long int num_verses;  /* Number of verses in item on stack */
/*
** The books (from bvarray.dat), and pach[]: the sequential verse number of
** each chapter's first verse (from chapter.dat). Verse numbers are long:
** 32 bits, as a translation may have more than 32,767 verses (with the
** Apocrypha), and an int is 16 bits on the CoCo.
*/
#ifndef COCO
        struct bva *book = NULL;
        long int *pach = NULL;
#else
        struct bva book[67];
	long int pach[1190];
#endif
long int offset; /* number of bytes offset from the beginning of a book that
                    a particular verse begins at. */
int disable_newverse = 0;/*0,1,or2 mean: enabled, about to enable, disabled */

#ifdef MSDOS
#	define SLASH '\\'
#else
#	define SLASH '/'
#endif

/* functions used only in this file */
static void open_new_trans P((void));
static void print_trans P((int i, int used));
static int index_format_ok P((char *name));
static int next_number P((FILE *file, long int *n));
static long int getcvarraydat P((long int sequ_verse_number));
static char *read_word P((FILE *file_ptr));
static int verse_number P((char *s, int len));
static long int lookup P((long int start, long int end, char *word));
#ifdef UNIXV
static void set_raw_tty P((void));
static void contsignal P((int signo));
#endif

/*
** Purpose: reset anything that needs to be fixed before the program exits.
*/
void exit_program(int errnum)
{
#ifdef OSK
        /* system("tmode echo eof=1b"); */
	_ss_opt(1, &oldbuffer);
#endif
#ifdef COCO
        /* system("tmode echo eof=1b"); */
	_ss_opt(1, &oldbuffer);
#endif
#ifdef UNIXV
        tty_suspend();
#endif
        exit(errnum);
}

#ifdef UNIXV
/*
** Purpose: put the terminal back the way it was before EBaC started, so
**          that another program can use it (or EBaC can exit);
**          tty_resume() sets it up for EBaC again.
*/
void tty_suspend(void)
{
#       ifdef USE_NCURSES
        scr_end();      /* e.g. turn off keypad transmit mode */
#       endif
        fflush(stdout);
        if (tty_saved)
                tcsetattr(0, TCSADRAIN, &oldtermio);
}

void tty_resume(void)
{
        set_raw_tty();
#       ifdef USE_NCURSES
        scr_resume();
#       endif
}
#endif

#ifdef VMS
int ugetchar(void)
{
        char c;
        vms_ttyread(&c, 1, 0);
        return UCHAR(c);
}
#endif

/* TRUE while a screen that should be redrawn on a resize waits for a key */
int redraw_on_resize = FALSE;

#ifdef UNIXV
/* read_key_byte() returns this when the window was resized meanwhile */
#define RESIZED (-2)

/*
** Purpose: read one character from the keyboard, waiting at most
**          "timeout" milliseconds, or for ever if timeout is negative.
** Returns: the character, 0..255, or -1 if none arrived in time.
**          While waiting for ever, it also returns RESIZED as soon as the
**          window is resized, if redraw_on_resize is set.
*/
static int read_key_byte(int timeout)
{
        unsigned char c;
        int n;
        fd_set fds;
        struct timeval tv;
#ifdef USE_NCURSES
        int rfd = scr_resize_fd(); /* readable after a resize */
        char junk[16];

        /*
        ** Wait for a key or a resize, whichever comes first. A resize is
        ** only reported to screens that redraw for it; otherwise it is just
        ** noted (the new size is used at the next screen clear) and the
        ** wait goes on.
        */
        while (timeout < 0 && rfd >= 0)
        {
                FD_ZERO(&fds);
                FD_SET(0, &fds);
                FD_SET(rfd, &fds);
                n = select(rfd + 1, &fds, NULL, NULL, NULL);
                if (n < 0)
                {
                        if (errno == EINTR)
                                continue;
                        break;          /* fall back to a plain read */
                }
                if (FD_ISSET(rfd, &fds))
                {
                        while (read(rfd, junk, sizeof(junk)) > 0)
                                ;       /* clear the wake-up(s) */
                        if (redraw_on_resize)
                                return RESIZED;
                }
                if (FD_ISSET(0, &fds))
                        break;          /* a key is waiting */
        }
#endif

        if (timeout >= 0)
        {
                do{
                        FD_ZERO(&fds);
                        FD_SET(0, &fds);
                        tv.tv_sec = timeout / 1000;
                        tv.tv_usec = (timeout % 1000) * 1000L;
                        n = select(1, &fds, NULL, NULL, &tv);
                }while (n < 0 && errno == EINTR);
                if (n <= 0)
                        return -1;
        }
        do{
                n = read(0, &c, (unsigned)1);
        }while (n < 0 && errno == EINTR); /* interrupted by a signal */
        if (n != 1) /* end of input, or the terminal went away */
                exit_program(0);
        return c;
}

#ifdef USE_NCURSES
/*
** Keys such as the arrows send several characters, starting with ESC.
** The rest of a sequence arrives at once, so if nothing follows an ESC
** within ESC_WAIT milliseconds, the user pressed the ESC key itself.
*/
#define ESC_WAIT 100
#define MAXSEQ   16

/* Characters read while checking for a key sequence, but not part of one */
static int pending[MAXSEQ];
static int npending = 0;

/*
** Purpose: an ESC has just been read. Read what follows and work out
**          which key was pressed.
** Returns: the MM/1 control character for a known key (an arrow becomes
**          CTRL-B, CTRL-F, ...), ESC for the ESC key by itself, or -1 for
**          a key EBaC has no use for (F1, Insert, ...), which is ignored.
*/
static int read_escape(void)
{
        char seq[MAXSEQ];
        int len = 1, c, code = 0, found, i;

        seq[0] = '\033';
        while ((found = scr_key_lookup(seq, len, &code)) == KEYSEQ_PREFIX &&
               len < MAXSEQ)
        {
                if ((c = read_key_byte(ESC_WAIT)) < 0)
                        break;           /* nothing more came */
                seq[len++] = (char) c;
        }
        if (found == KEYSEQ_MATCH)
                return code;
        if (len == 1)
                return '\033';          /* the ESC key by itself */

        if (seq[1] == '[' || seq[1] == 'O')
        {
                /*
                ** An ANSI key sequence we don't use. Skip the rest of it so
                ** its characters aren't taken as typing: "ESC O x" is
                ** complete at three characters, "ESC [ ..." ends with a
                ** character from '@' to '~'.
                */
                if (seq[1] == 'O')
                {
                        if (len == 2)
                                read_key_byte(ESC_WAIT);
                }
                else
                {
                        c = UCHAR(seq[len - 1]);
                        while (len < MAXSEQ && (len == 2 || c < '@' || c > '~'))
                        {
                                if ((c = read_key_byte(ESC_WAIT)) < 0)
                                        break;
                                len++;
                        }
                }
                return -1;
        }

        /* ESC, then an ordinary key typed quickly: deliver both */
        for (i = 1; i < len; i++)
                pending[npending++] = UCHAR(seq[i]);
        return '\033';
}
#endif /* USE_NCURSES */

int ugetchar(void)
{
        int c;

#ifdef USE_NCURSES
        for (;;)
        {
                if (npending > 0)
                {
                        c = pending[0];
                        npending--;
                        memmove(pending, pending + 1, npending * sizeof(pending[0]));
                        return c;
                }
                c = read_key_byte(-1);
                if (c == RESIZED)
                        return KEY_REDRAW;
                if (c != '\033')
                        return c;
                if ((c = read_escape()) >= 0)
                        return c;
                /* else: a key we don't use; wait for the next one */
        }
#else
        c = read_key_byte(-1);
        return c;
#endif
}
#endif

/*
Purpose: the function fclose is called many times in this application
         and we always want to check for error status. This saves space
         by not having to repeat the code many times.
*/
void mfclose(FILE *fileptr, char *string)
{
        if(fclose(fileptr) == EOF)
        {
                printf("Can't close file: %s\n", string);
                exit_program(1);
        }
}

/*
Purpose: the function fopen is called many times in this application
         and we always want to check for error status. This saves space
         by not having to repeat the code many times.
*/
FILE *mfopen(char *string, char *mode)
{
        FILE *fileptr;
#       ifdef COCO
/*
** CoCo file system doesn't let names begin with digits; so we prepend 'a'
** Hmmm, I just discovered that lha 2.11b doesn't follow this naming method
** that Rick's UUCP has used for years! So, I decided to change my method
** to prepend 'x' to the name and truncate the last character.
*/
        char NewName[80];

        if (isdigit(UCHAR(*string)))
        {
             /* *NewName = 'a'; */
                *NewName = 'x';
                strcpy(&NewName[1], string);
		NewName[strlen(NewName) - 1] = '\0'; /*truncate last character*/
                string = NewName;
        }
#       endif
#       ifdef VMS
/* VMS requires all files to have an extention; so we append '.' */
        char NewName[80];

        if (strchr(string, '.') == NULL)
        {
                strcpy(NewName, string);
                strcat(NewName, ".");  /* append a '.' if not already there */
                string = NewName;
        }
#       endif
#	ifdef MSDOS /* This code gets around MS-DOS's 8 character limit */
	char NewName[80];
	char *dot_ptr, *slash_ptr;

    strcpy(NewName, string); /* make a copy of string. (Save original) */

	slash_ptr = strrchr(NewName, '\\'); /* find last occurance of '\' */
	if (slash_ptr == NULL)
		slash_ptr = NewName; /* point to first char in filename */
	else
		slash_ptr++;      /* point to first char in filename; after '\' */

	dot_ptr = strchr(slash_ptr, '.');
	if (dot_ptr == NULL)              /* there is no file extention */
	{
		slash_ptr[8] = '\0';
	}
	else if ((dot_ptr - slash_ptr) > 8)
	{
		slash_ptr[8]  = '.';
		slash_ptr[9]  = dot_ptr[1]; /* the file's extention */
		slash_ptr[10] = dot_ptr[2];
		slash_ptr[11] = dot_ptr[3];
		slash_ptr[12] = '\0';
	}
    string = NewName;
#	endif

        fileptr = fopen(string, mode);
        if (fileptr == (FILE *) NULL)
        {
                printf("Can't Open \"%s\" for ", string);
                if(mode[0] == 'w')
                 printf("writing. Is the disk full?\n");
                else if(mode[0] == 'r')
                 printf("reading. Are you in the correct working directory?\n");
                else if(mode[0] == 'a')
                 printf("append.\n");
		else
		 printf("an error occurred!\n");
                exit_program(1);
        }
        return(fileptr);
}


#ifdef UNIXV
/*
** Input modes we keep from the user's settings; all others (including
** ICRNL, so that the Enter key arrives as '\r') are turned off.
** IXANY is an X/Open extension, so only use it where it exists.
*/
#ifdef IXANY
#       define KEEP_IFLAGS (IXON|IXOFF|IXANY|ISTRIP|IGNBRK)
#else
#       define KEEP_IFLAGS (IXON|IXOFF|ISTRIP|IGNBRK)
#endif

/* Put the terminal into cbreak/noecho mode: one key at a time, no echo. */
static void set_raw_tty(void)
{
    newtermio = oldtermio;
    newtermio.c_iflag &= KEEP_IFLAGS;
    newtermio.c_lflag &= ISIG;          /* no ICANON, no ECHO */
    newtermio.c_cc[VINTR] = 3; /* CTRL-C */
    newtermio.c_cc[VMIN] = 1;
    newtermio.c_cc[VTIME] = 0;
    tcsetattr(0, TCSADRAIN, &newtermio);
}

/* Restore raw mode after the program is resumed from CTRL-Z. */
static void contsignal(int signo)
{
    (void) signo;
    set_raw_tty();
}
#endif

void initialize(void)
{
int i, j, len;
FILE *trans_file;

/*
** Put system into "raw" mode so that we can read just one character
** at a time with making the user hit enter.
*/
#ifdef OSK
        setbuf(stdin, NULL);
	if (_gs_opt(1, &oldbuffer) < 0)
		exit(1);
	if (_gs_opt(1, &newbuffer) < 0)
		exit(1);
	newbuffer.sg_pause =
	newbuffer.sg_echo =
	newbuffer.sg_eofch = 0;
	if (_ss_opt(1, &newbuffer) < 0)
		exit(1);

        intercept(exit_program);
#	define BIBLEROOT "/dd/BIBLE"
#endif

#ifdef COCO
        pflinit(); /* tell linker to include library for printing longs */
        setbuf(stdin, NULL);
	if (_gs_opt(1, &oldbuffer) < 0)
		exit(1);
	if (_gs_opt(1, &newbuffer) < 0)
		exit(1);
	newbuffer.sg_pause =
	newbuffer.sg_echo =
	newbuffer.sg_eofch = 0;
	if (_ss_opt(1, &newbuffer) < 0)
		exit(1);

        intercept(exit_program);
#	define BIBLEROOT "/dd/BIBLE"
#endif
#ifdef VMS
	char *BIBLEROOT;
	BIBLEROOT = getenv("BIBLEROOT");
	if(BIBLEROOT == NULL)
	{
		printf("Error: getenv(BIBLEROOT) failed.\n");
                printf("Please set BIBLEROOT environment to the directory where EBaC is located.\n");
		exit(1);
	}
        VMS_read_raw = TRUE;
        vms_open_tty();
        signal(SIGINT, exit_program);
#endif
#ifdef UNIXV
	char *BIBLEROOT;
	BIBLEROOT = getenv("BIBLEROOT");
	if(BIBLEROOT == NULL)
	{
		printf("Error: getenv(BIBLEROOT) failed.\n");
                printf("Please set BIBLEROOT environment to the directory where EBaC is located.\n");
		exit(1);
	}

        if (!isatty(0) || tcgetattr(0, &oldtermio) != 0)
        {
                printf("EBaC must be run from an interactive terminal.\n");
                exit(1);
        }
        tty_saved = TRUE;
        set_raw_tty();
        signal(SIGINT, exit_program);
        signal(SIGCONT, contsignal);
#endif
#ifdef MSDOS
/*
** We have to change our working directory in two steps because the
** chdir command will only change directories within the same drive
** letter.
*/
	char t_path[80] = "";
	strcpy(t_path, getenv("BIBLEDRIVE"));
	if ((*t_path == '\0') || (strlen(t_path) != 2) || (t_path[1] != ':'))
	{
		printf("You must set BIBLEDRIVE in your autoexec.bat\n");
		printf("This file is read automagicly when you boot up.\n");
		printf("Did you forget to re-boot after editing the file?\n");
		exit_program(1);
	}
	system(t_path);

	strcpy(t_path, getenv("BIBLEROOT"));
	if(*t_path == '\0')
	{
		printf("You must set BIBLEROOT in your autoexec.bat\n");
		exit_program(1);
	}
	bible_root = t_path;            /* for the Information screen */
	if (chdir(t_path) != 0)
	{
		printf("Can't chdir to %s\n", t_path);
		exit_program(0);
	}
#else
        bible_root = BIBLEROOT;         /* for the Information screen */
        if (chdir(BIBLEROOT) != 0)
        {
        	printf("Can't chdir to %s\n", BIBLEROOT);
                printf("Please set BIBLEROOT environment to the directory where EBaC is located.\n");
        	exit_program(0);
        }
#endif
#ifdef UNIXV
        if (getcwd(root_dir, sizeof(root_dir) - MAX_ETLA - 2) == NULL)
                root_dir[0] = '\0';    /* (then ../NAME, as before) */
#endif
        initscreen();
	initlogname();
/*
** Each line of trans is a translation's folder name, then a description.
** Blank lines, and lines starting with '#' (comments, for example to note
** a folder that is not a translation), are skipped.
*/
        trans_file = mfopen("trans", "r");
        i = 0;
        while ((fgets(trans[i], 80, trans_file) != NULL) && (i + 2 < MAXTRANS))
        {
        	char *p = trans[i];

        	/* a description too long for trans[i]: keep its start, end it
        	   with a newline, and read over the rest of the line */
        	if (find_char(trans[i], '\n') == NULL && !feof(trans_file))
        	{
        		int c, k;

        		while ((c = fgetc(trans_file)) != '\n' && c != EOF)
        			;
        		k = 78;         /* (not in the middle of a character) */
        		while (k > 0 && (trans[i][k] & 0xC0) == 0x80)
        			k--;
        		trans[i][k] = '\n';
        		trans[i][k + 1] = '\0';
        	}
        	while (*p == ' ' || *p == '\t')
        		++p;
        	if (*p != '#' && *p != '\n' && *p != '\r' && *p != '\0')
        		++i;    /* keep this line; otherwise read over it */
        }
        trans[i][0] = '\0';
        mfclose(trans_file, "trans");
        if (trans[0][0] == '\0')
        {
        	printf("The file 'trans' lists no translations.\n");
        	exit_program(1);
        }

/*
** Start with the translation named when EBaC was started ("bible BSB"; the
** translation importer starts EBaC this way after adding one), if trans
** lists it; otherwise with the first in the list.
*/
        j = 0;
        if (start_trans != NULL && (len = strlen(start_trans)) > 0)
        	for (i = 0; trans[i][0] != '\0'; i++)
        		if (strncmp(trans[i], start_trans, len) == 0 &&
        		    isspace(UCHAR(trans[i][len])))
        		{
        			j = i;
        			break;
        		}
        i = 0;
        while (!isspace(UCHAR(trans[j][i])) && trans[j][i] != '\0' &&
               (i < MAX_ETLA - 1))
        {
        	etla[i] = trans[j][i];
        	++i;
        }
        etla[i] = '\0';
        if (chdir(etla) != 0)
        {
        	printf("Can't chdir to %s\n", etla);
        	exit_program(0);
        }
        if (!index_format_ok(etla))
        	exit_program(1);
	open_new_trans();
}
        
/*
** Purpose: whether the index in the current folder (translation name) is
**          in the format this EBaC reads: format.dat, which the index
**          program writes, starts with the line INDEX_FORMAT. An index
**          made by an older EBaC has no format.dat. If not, says so.
*/
static int index_format_ok(char *name)
{
        FILE *format_file;
        char line[80];
        int ok = FALSE;

        if ((format_file = fopen("format.dat", "r")) != NULL)
        {
                if (fgets(line, sizeof(line), format_file) != NULL &&
                    strncmp(line, INDEX_FORMAT, strlen(INDEX_FORMAT)) == 0 &&
                    (line[strlen(INDEX_FORMAT)] == '\n' ||
                     line[strlen(INDEX_FORMAT)] == '\0'))
                        ok = TRUE;
                fclose(format_file);
        }
        if (!ok)
        {
                printf("\nThe index of the translation %s was made by another\n", name);
                printf("version of EBaC (or is missing). Make it again: run\n");
                printf("\"make\" in the EBaC folder (%s).\n", bible_root);
        }
        return (ok);
}

/*
** Purpose: open a file in the EBaC folder (BIBLEROOT), such as "groups",
**          for reading, from the translation's folder (where EBaC is).
** Returns: the file, or NULL if there is none.
*/
FILE *root_fopen(char *name)
{
        char path[1024 + 64];

        if (strlen(name) > 60)
                return (NULL);
#ifdef UNIXV
        if (root_dir[0] != '\0')
        {
                sprintf(path, "%s%c%s", root_dir, SLASH, name);
                return (fopen(path, "r"));
        }
#endif
        sprintf(path, "..%c%s", SLASH, name);
        return (fopen(path, "r"));
}

/*
** The index files: their formats, and why, are in readme.architecture.
** Every number in bvarray.dat and chapter.dat is 4 bytes.
*/
static void open_new_trans(void)
{
        int i;

        bvarray_file = mfopen("bvarray.dat", READMODE);
#ifndef COCO
        if (fseek(bvarray_file, 0L, 2) == EOF)
        {
            printf("fseek failed on bvarrary\n");
            exit_program(0);
        }
        numbooks = ftell(bvarray_file);
        numbooks /= 8L;         /* a verse number and a chapter count each */
        free(book);             /* (the last translation's) */
    if ((book=(struct bva *) malloc(numbooks * sizeof(struct bva))) == NULL)
        {
                printf("Out of RAM\n");
                exit_program(1);
        }
        rewind(bvarray_file);
	--numbooks;
#else
    numbooks = 66;
#endif
        for (i = 0; i <= numbooks; i++)
        {
                book[i].verse = fgetl(bvarray_file);
                book[i].chapt = fgetl(bvarray_file);
        }
        mfclose(bvarray_file, "bvarray.dat");

        chapter_file = mfopen("chapter.dat", READMODE);
#ifndef COCO
        fseek(chapter_file, 0L, 2);
        numchapters = ftell(chapter_file) / 4L;
        free(pach);             /* (the last translation's) */
    if ((pach = (long int *) malloc (numchapters * sizeof(long int))) == NULL)        {
                printf("Out of RAM\n");
                exit_program(1);
        }
        rewind(chapter_file);
	--numchapters;
#else
        numchapters = 1189;
#endif
        for (i = 0; i <= numchapters; i++)
                pach[i] = fgetl(chapter_file);
        mfclose(chapter_file, "chapter.dat");

        book_list = mfopen("booklist", "r");
        for (i = 0; i < numbooks; i++)
        {
                if (fgets(book[i].name, MAXBOOKNAME, book_list) == NULL)
                {
                        printf("booklist has fewer than %ld books\n", numbooks);
                        exit_program(1);
                }
/* get rid of the "\n" */
                book[i].name[strlen(book[i].name) - 1] = '\0';
        }
        mfclose(book_list, "booklist");

        cvarray_file = mfopen("cvarray.dat", READMODE);
        fseek(cvarray_file, 0L, 2);
        numverses = ftell(cvarray_file) / 4L;

        words_file = mfopen("words.dat", READMODE);
        fseek(words_file, 0L, 2);
        sizeofwordsdat = ftell(words_file);
/*
** This is not needed since we are going to fseek() before we read anything.
*/
        rewind(words_file);

/*
** (Before EBaC 2.1, words occurring too often to be indexed were listed in
** common.dat. Every word is indexed now, as was planned here long ago.)
*/
        references_file = mfopen("references.dat", READMODE);
        position_file = mfopen("position.dat", READMODE);
        plainpos_file = mfopen("plainpos.dat", READMODE);

        load_groups();  /* the groups of books (=nt), for this translation */
}

/*
** Purpose: let the user choose a translation from those 'trans' lists.
**          With up to 9, a single key (its number) chooses, as always. With
**          more, they are shown a screenful at a time, each on one line, and
**          the user types a number and presses Enter (q to p type 1 to 0, as
**          on the chapter and verse prompts); space or n shows the next page,
**          p the previous one (p after a digit is 0), and x, Esc, or Enter
**          alone, goes back. The screen is announced as "trans" or, when a
**          number needs Enter, "transenter" (for the Home Assistant app's
**          gestures; see scr_mode()).
*/
/*
** Purpose: print a translation's line of 'trans' (number i), on one line:
**          cut to the window's width, where used columns are taken already.
*/
static void print_trans(int i, int used)
{
	int k, n, w;

	for (k = 0, w = used; trans[i][k] != '\n' && trans[i][k] != '\0';
	     k += n)
	{
		n = u8_len(&trans[i][k]);       /* (a character: UTF-8) */
		if (w + u8_width(&trans[i][k]) > s_cols - 1)
			break;
		w += u8_width(&trans[i][k]);
		while (n-- > 0)
			putchar(trans[i][k++]);
		n = 0;
	}
	putchar('\n');
}

void change_translations(void)
{
	static char digit_letters[] = "qwertyuiop"; /* stand for 1-9, 0 */
	int i = -1;
	int c; /* character the user types */
	int n; /* number the user selects */
	int count; /* how many translations there are */
	int per_page, first, typed;
	int spaced;     /* a blank line between the translations */
	char *digit;
	char new_dir[1024 + MAX_ETLA + 2]; /* example: ../nrsv */
	char name[MAX_ETLA];    /* the translation chosen */

	while (trans[++i][0] != '\0')
		;
	count = i;
	first = 0;              /* the first translation on the page shown */
	typed = 0;              /* digits typed so far */
	n = 0;
	for(;;)
	{
		cls();
		/* (a shorter heading in a narrow window, so it stays on one line) */
		if (count <= 9 || s_cols - 1 >= 53)
			printf("Please select the number of the translation desired:\n\n");
		else
			printf("Choose a translation:\n\n");
		/*
		** With a blank line between the translations, each is easy to
		** tap on a touch screen, as on the main menu (see gestures.js in
		** the Home Assistant app): when they all fit on the screen so.
		** Otherwise one to a line, as many as fit. The window's size is
		** looked at again whenever it changes (a phone turned, say).
		*/
		if (count <= 9)
		{
			spaced = (2 * count + 2 <= s_lines); /* heading, list, cursor */
			for (i = 0; i < count; i++)
			{
				if (spaced && i > 0)
					putchar('\n');
				printf("%d) ", i + 1);
				print_trans(i, 3);
			}
			scr_mode("trans");
			redraw_on_resize = TRUE;
			c = ugetchar();
			redraw_on_resize = FALSE;
			if (c == KEY_REDRAW)
				continue;       /* drawn again, for the new size */
			if (!isdigit(c) && (digit = find_char(digit_letters, tolower(c))) != NULL)
				c = "1234567890"[digit - digit_letters];
			if (!isdigit(c))
				return;
			n = c - '0';
			if (n < 1 || n > count)
				return;
			break;
		}
		/* a page: as many as fit between the heading and the prompt
		   (all of them, spaced, if they fit so) */
		spaced = (2 * count - 1 <= s_lines - 4);
		per_page = spaced ? count : s_lines - 4;
		if (per_page < 1)
			per_page = 1;
		if (first >= count)
			first = count - 1;
		first -= first % per_page;
		for (i = first; i < count && i < first + per_page; i++)
		{
			if (spaced && i > first)
				putchar('\n');
			printf("%2d) ", i + 1);
			print_trans(i, 4);
		}
		printf("\n");
		if (s_cols - 1 >= 62)
			printf("Type a number and Enter (space, p: next, previous page; x: back): ");
		else
			printf("Number, Enter (space/p: page; x: back): ");
		fflush(stdout);
		typed = 0;
		n = 0;
		scr_mode("transenter");
		for(;;)
		{
			redraw_on_resize = TRUE;
			c = ugetchar();
			redraw_on_resize = FALSE;
			if (c == KEY_REDRAW)
				break;                  /* the same page, for the new size */
			if (typed == 0 && (c == ' ' || c == 'n' || c == 'N' || c == 0x16))
			{
				if (first + per_page < count)
				{
					first += per_page;
					break;
				}
				putchar('\7');
				fflush(stdout);
				continue;
			}
			if (typed == 0 && (c == 'p' || c == 'P' || c == 0x1a))
			{
				if (first > 0)
				{
					first -= per_page;
					break;
				}
				putchar('\7');
				fflush(stdout);
				continue;
			}
			if (c == 'x' || c == 'X' || c == 0x1b)
				return;
			if (c == '\n' || c == '\r')
			{
				if (typed == 0)
					return;         /* Enter alone: back */
				if (n >= 1 && n <= count)
					break;
				putchar('\7');         /* no such number: try again */
				for (; typed > 0; typed--)
					printf("\b \b");
				n = 0;
				fflush(stdout);
				continue;
			}
			if ((c == '\b' || c == '\177') && typed > 0)
			{
				printf("\b \b");
				typed--;
				n /= 10;
				fflush(stdout);
				continue;
			}
			if (!isdigit(c) && (digit = find_char(digit_letters, tolower(c))) != NULL)
				c = "1234567890"[digit - digit_letters];
			if (isdigit(c) && typed < 3)
			{
				putchar(c);
				typed++;
				n = n * 10 + (c - '0');
				fflush(stdout);
				continue;
			}
			putchar('\7');
			fflush(stdout);
		}
		if ((c == '\n' || c == '\r') && n >= 1 && n <= count)
			break;
	}
        --n;
        i = 0;
        while (!isspace(UCHAR(trans[n][i])) && trans[n][i] != '\0' &&
               (i < MAX_ETLA - 1))
        {
        	name[i] = trans[n][i];
        	++i;
        }
        name[i] = '\0';

/*
** Go to its folder first: if it can't be found, say so and carry on with
** the translation in use. (A failed chdir() leaves the folder as it was.)
*/
#ifdef UNIXV
	if (root_dir[0] != '\0')
		sprintf(new_dir, "%s%c%s", root_dir, SLASH, name);
	else
#endif
	sprintf(new_dir, "..%c%s", SLASH, name);
        if (chdir(new_dir) != 0)
        {
        	printf("\nCan't open %s: there is no folder %s in %s.\n",
        	       name, name, bible_root);
        	printf("Press any key to carry on with %s.\n", etla);
        	fflush(stdout);
        	scr_mode("anykey");
        	ugetchar();
        	return;
        }
        if (!index_format_ok(name))     /* an index EBaC can't read: back */
        {
#ifdef UNIXV
        	if (root_dir[0] != '\0')
        		sprintf(new_dir, "%s%c%s", root_dir, SLASH, etla);
        	else
#endif
        	sprintf(new_dir, "..%c%s", SLASH, etla);
        	if (chdir(new_dir) != 0)
        	{
        		printf("Can't chdir to %s\n", new_dir);
        		exit_program(1);
        	}
        	printf("Press any key to carry on with %s.\n", etla);
        	fflush(stdout);
        	scr_mode("anykey");
        	ugetchar();
        	return;
        }
        strcpy(etla, name);

        mfclose(cvarray_file, "cvarray.dat");
        mfclose(words_file, "words.dat");
        mfclose(references_file, "references.dat");
        mfclose(position_file, "position.dat");
        mfclose(plainpos_file, "plainpos.dat");

/*
** This is a flag which forces the closing, and opening of text files
** which have the same name, but in different directories.
** For example, reading "genesis" in the NIV directory, then reading
** "genesis" in the AV directory...
*/
	a_new_translation = TRUE;
	open_new_trans();
}

static long int getcvarraydat(long int sequ_verse_number)
{
/*
** printf("sequ is: %d seek to: %ld\n",
**     sequ_verse_number, ((long int) sequ_verse_number - 1) * 4);
*/
    if (fseek(cvarray_file, (sequ_verse_number - 1L) * 4L, 0) == EOF)    {
            printf("FSEEK failed on cvarray_file!\n");
            exit_program(1);
    }
    return(fgetl(cvarray_file));
}

int verse_to_book(long int sequ_verse_number)
{
        int i;
        for(i = 1; i < numbooks; i++)
                if(sequ_verse_number <= book[i].verse)
                        break;
        return(i - 1);
}

#define MAXWORD 80

/*
** A verse number only counts at the start of a line, as every verse starts
** one: a reference in the text ("Psa. 2:7"), or on a line it wraps onto
** (which starts with a space), isn't one. read_word() sets line_start for
** the word it reads; after a seek to a verse, the word is at a line start.
*/
static int line_start = FALSE;
static int after_seek = FALSE;
static int ended_line = FALSE;  /* the last word read ended its line */

/* Purpose: whether the len characters at s are a verse number, such as
            1:1: digits, a colon, digits. */
static int verse_number(char *s, int len)
{
        int i = 0, digits;

        for (digits = 0; i < len && isdigit(UCHAR(s[i])); i++)
                digits++;
        if (digits == 0 || i >= len || s[i] != ':')
                return (FALSE);
        for (i++, digits = 0; i < len && isdigit(UCHAR(s[i])); i++)
                digits++;
        return (digits > 0 && i == len);
}

static char *read_word(FILE *file_ptr)
{
int i = 0; /* index */
int c;
static char word[MAXWORD];

#ifdef VMS
        clearerr(file_ptr);
#endif
#ifdef UNIXV
        clearerr(file_ptr);
#endif
#ifdef COCO
        cleareof(file_ptr);
#endif
#ifdef OSK
        cleareof(file_ptr);
#endif
#ifdef MSDOS
        clearerr(file_ptr);
#endif
/* Skip over leading white space, noting whether it ended a line */
line_start = after_seek || ended_line;
after_seek = FALSE;
do{
    word[i] = (char) fgetc(file_ptr);
    if (word[i] == '\n')
        line_start = TRUE;
    else if (word[i] == ' ')
        line_start = FALSE;
}while (word[i] == ' ' || word[i] == '\n');

/* Read until a delimeter is found, or EOF. A word too long for word[] is
   cut short, but all of it is read. */
for(;;)
{
    c = fgetc(file_ptr);
    if (c == ' ' || c == '\n' || feof(file_ptr))
    {
        ended_line = (c == '\n');      /* so the next word starts a line */
        break;
    }
    if (i < MAXWORD - 2)
        word[++i] = (char) c;
}
word[++i] = '\0';  /* Need to terminate the string */
if (word[0] == 0x1a) /* We have reached EOF */
        fgetc(file_ptr); /* This makes feof(file_ptr) be TRUE */
/*
** A verse number may be in square brackets, [1:1], as in texts from
** other sources: take them off. ([was], a word in brackets, stays.)
*/
if (word[0] == '[' && i > 2 && word[i - 1] == ']' && verse_number(&word[1], i - 2))
{
        for (c = 0; c < i - 2; c++)
                word[c] = word[c + 1];
        word[i - 2] = '\0';
}
i = -1;
while (word[++i])
{
        if (word[i] == '{')
                disable_newverse = 2;
        if (word[i] == '}')
                disable_newverse = 1;
}
return((char *) word);
}

/*
** Purpose: using the supplied svn this routine locates the information
** associated with that number.
** Example: when sequ_verse_number is 2, a structure containing this
**          information is returned:
**              Genesis
**              1
**              2
**              And the earth was without form, and void; and darkness....
**          All four parts of the structure are in character (string) form.
**          The complete text of the verse is present without ANY linefeeds
**          or carriage returns. It is the responsibility of the calling
**          function to perform word wrapping.
** Needed Improvements: A cache system needs to be designed to save the 50
**                      or so most recently used verses within static variables
**                      inside this function to avoid accessing the disk drives
**                      too often. This must be adjustable.
*/
struct bcv_array_type *get_verse(long int sequ_verse_number)
{
        static struct bcv_array_type bcv_arrays;
        long int offset;
        int book_num;
        static int bn_previous = -1;
        static char filename[40] = { '\0' };
        static FILE *file_ptr;
/* last word processed is saved for the next time function is called */
        static char *word;
/* stores argument from previous function invocation */
        static long int svn_previous = -1;

/*
** printf("start svn is: %d\n", sequ_verse_number);
*/
/*
** When we get called twice in a row with a request for the same verse number
** this saves us from doing a _LOT_ of work.
*/
        if (sequ_verse_number == svn_previous)
                return(&bcv_arrays);
        if (sequ_verse_number < 1)
        {
                svn_previous = -1;
                return(&bcv_arrays);
        }
        bcv_arrays.data_array[0] = '\0'; /* Get rid of text from last time */
        book_num = verse_to_book(sequ_verse_number);
/*
** If we need to look at the same file as we used last time there is no
** need to close and re-open it. Otherwise, close the old file and open the
** one we need. Depending on the OS's ability to keep multiple files open
** we might be able to speed this up by leaving several files open and
** seeing if the desired file is in the list of open files... I doubt if
** many OS's would allow all 66 files to be open at the same time :-)
*/
        if ((book_num != bn_previous) || a_new_translation)
        {
                if (filename[0] != '\0')
                        mfclose(file_ptr, filename);
/*
** printf("Booknum: %d name: %s\n", book_num, book[book_num].name);
*/
                strcpy(filename, book[book_num].name);
                file_ptr = mfopen(filename, READMODE);
        }
/*
** If we are already at the correct offset in the file we don't have to
** call fseek(). If the file was just opened and we want an offset of 0
** then I suppose we don't really need to call fseek. But the overhead
** to check that occurance may outweigh the few times fseek would be
** needlessly called.
*/
        if (svn_previous + 1 != sequ_verse_number || 
				book_num != bn_previous ||
				a_new_translation )
        {
/*
** printf("svn is: %d\n", sequ_verse_number);
*/
                offset = getcvarraydat(sequ_verse_number);
/*
** printf("Offset is: %ld\n", offset);
*/
                if (fseek(file_ptr, offset, 0) == EOF)
                {
                        printf("FSEEK failed on file: %s\n", filename);
                        exit_program(1);
                }
                after_seek = TRUE;      /* a verse starts a line */
                word = read_word(file_ptr);
        }
        a_new_translation = FALSE;
/*
** The word that is read here should be in the form c:v where c is the chapter
** and v is the verse. If this is not true then something went fatally wrong
** with the indexing system and we must terminate the program.
*/
        if (!(new_verse(word)))
        {
                printf("We were expecting a verse number here '%s'\n", word);
                exit_program(1);
        }
        strcpy(bcv_arrays.book_array, filename);
        sprintf(bcv_arrays.chapt_array, "%d", wordtochapt(word));
        sprintf(bcv_arrays.verse_array, "%d", wordtoverse(word));
        word = read_word(file_ptr);
        while (!((new_verse(word) && line_start) || feof(file_ptr)))
        {
                /* (a verse too long for data_array is cut short) */
                if (strlen(bcv_arrays.data_array) + strlen(word) + 2 <=
                    sizeof(bcv_arrays.data_array))
                {
                        strcat(bcv_arrays.data_array, word);
                        strcat(bcv_arrays.data_array, " ");
                }
                word = read_word(file_ptr);
        }
        svn_previous = sequ_verse_number;
        bn_previous = book_num;
        return(&bcv_arrays);
}

static long int lookup(long int start, long int end, char *word)
{
        int ignore;
#ifdef COCO /* Compiler pukes on "unsigned char" why? */
        static char dword[80];
#else
        static unsigned char dword[80];
#endif
        int i = 0;
        static long int words_offset, save_pos, refs_offset;
        if ((end - start) < 200)
        {
                if (ftell(words_file) != start)
                {
                        if (fseek(words_file, start, 0) == EOF)
                        {
                                printf("fseek failed on small words.dat\n");
                                exit_program(1);
                        }
                }
                do{
                        dword[i] = (char) getc(words_file);
#ifdef COCO
                }while (dword[i++] != S_EOR);
#else
                }while (dword[i++] != EOR);
#endif
                if (i < 6) /* Got EOF */
                        return(-1);
                refs_offset = dword[i - 5];
                refs_offset <<= 8;
                refs_offset |= (0xff & dword[i - 4]);
                refs_offset <<= 8;
                refs_offset |= (0xff & dword[i - 3]);
                refs_offset <<= 8;
                refs_offset |= (0xff & dword[i - 2]);
                refs_offset = l_bit_reset(refs_offset);
                dword[i - 5] = '\0';

                i = strcmp(word, (char *) dword);
                if (i == 0)
                {
/*
printf("sequ found the word we were looking for: %ld\n", refs_offset);
*/
                        return(refs_offset);
                }
                if (i < 0)
                {
/*
printf("found: %s giving up...\n", dword);
ugetchar();
*/
                        return(-1);
                }
                if (i > 0)
                {
/*
printf("found: %s 1st char:%d or hex:%x\n", dword, dword[0], dword[0]);
*/
                        return(lookup(ftell(words_file), end, word));
                }
        }
        words_offset = (end - start + 1L) / 2L + start;
/*
printf("Seeking to: %ld Between: %ld and %ld\n", words_offset, end, start);
*/
        if (fseek(words_file, words_offset, 0) == EOF)
        {
                printf("fseek failed on large words.dat\n");
                exit_program(1);
        }
        do{
                if ((ignore = getc(words_file)) == EOF)
                {
                        printf("getc failed on words.dat\n");
                        exit_program(1);
                }
/*
printf("from words: %x\n", ignore);
*/
        }while (ignore != EOR); /* ignore is an int. Therefore not signed */

        save_pos = ftell(words_file);
        do{
                dword[i] = (char) getc(words_file);
#ifdef COCO
        }while (dword[i++] != S_EOR);
#else
        }while (dword[i++] != EOR);
#endif
        refs_offset = dword[i - 5];
        refs_offset <<= 8;
        refs_offset |= (0xff & dword[i - 4]);
        refs_offset <<= 8;
        refs_offset |= (0xff & dword[i - 3]);
        refs_offset <<= 8;
        refs_offset |= (0xff & dword[i - 2]);
        refs_offset = l_bit_reset(refs_offset);
        dword[i - 5] = '\0';

        i = strcmp(word, (char *) dword);
        if (i == 0)
        {
/*
printf("random found the word we were looking for: %ld\n", refs_offset);
*/
                return(refs_offset);
        }
        if (i < 0)
                return(lookup(start, save_pos - 1, word));   /* recursive */
        return(lookup(ftell(words_file), end, word));        /* recursive */
}

/*
** Purpose: read the next number of a word's record in references.dat (a
**          verse, or with EBaC 2.1, in position.dat or plainpos.dat, a
**          position): 4 bytes, most significant first. The record ends
**          with EOR (0xFF), which no number starts with.
** Returns: FALSE at the end of the record (having read only the EOR, as
**          the last record ends the file), else TRUE, with the number in *n.
*/
static int next_number(FILE *file, long int *n)
{
        int c;

        if ((c = getc(file)) == EOF || c == EOR)
                return (FALSE);
        *n = c;
        *n = (*n << 8) | (0xff & getc(file));
        *n = (*n << 8) | (0xff & getc(file));
        *n = (*n << 8) | (0xff & getc(file));
        return (TRUE);
}

#ifdef RAMDISK
struct TEMPFILE *tmpopen(void)
{
        static struct TEMPFILE tf;
        static int i = 0;

#ifdef COCO
        sprintf(tf.sname, "/r0/ebac_%d.%d", getpid(), ++i);
#endif
#ifdef OSK
        sprintf(tf.sname, "/r0/ebac_%d.%d", getpid(), ++i);
#endif
#ifdef UNIXV
        sprintf(tf.sname, "/tmp/ebac_%d.%d", getpid(), ++i);
#endif
#ifdef VMS
        sprintf(tf.sname, "SYS$SCRATCH:EBAC_%d.%d", getpid(), ++i);
#endif
#ifdef MSDOS
	char t_path[80];

	strcpy(t_path, getenv("TEMPFILE"));
	if ((t_path[0] == '\0') ||
            (t_path[1] != ':') ||
            (t_path[strlen(t_path) - 1] != '\\'))
	{
		printf("You must set TEMPFILE in your autoexec.bat\n");
		printf("This file is read automagicly when you boot up.\n");
		printf("Did you forget to re-boot after editing the file?\n");
		exit_program(1);
	}
	if (i > 998)
		i = 0;
        sprintf(tf.sname, "%sTMP%d.%d", t_path, getpid(), ++i);
#endif
        tf.fname = mfopen(tf.sname, WRITE_UPDATE);
        tf.width = VERSES;      /* a verse list, unless the caller says */
        return (&tf);
}
#endif

/*
** Purpose: look a word up in the index, and make its list.
**          With positions FALSE, a verse list: each verse it is in, once
**          (references.dat only). With positions TRUE (a search with
**          phrases), an occurrence list: each occurrence, with its verse
**          as the start and end, and its full and plain positions, read
**          at the same offset from position.dat and plainpos.dat. (See
**          readme.architecture.)
*/
#ifdef RAMDISK
struct TEMPFILE *find_word(char *word, int positions)
{
#else
long int *find_word(char *word, int positions)
{
#endif
 long int refs_offset;
 long int svn, last, full, plain;
 int width = positions ? OCCURRENCES : VERSES;
#ifdef RAMDISK
 struct TEMPFILE *tempfile;

 tempfile = tmpopen();
 tempfile->width = width;
#else
 size_t i = 0;
 static long int *refs_list = NULL;
 static size_t refs_size = 0;   /* how many numbers refs_list holds */
 size_t need;
#endif
        refs_offset = lookup((long) 0, (long) sizeofwordsdat, word);
/*
** printf("Word is at offset: %ld\n", refs_offset);
*/
        num_verses = 0;
        num_found = 0;
        if (refs_offset >= 0) /* Word IS in index */
        {
                if (fseek(references_file, refs_offset, 0) == EOF ||
                    (positions &&
                     (fseek(position_file, refs_offset, 0) == EOF ||
                      fseek(plainpos_file, refs_offset, 0) == EOF)))
                {
                        fprintf(stderr, "fseek failed on references file!\n");
                        exit_program(1);
                }
                /* the word's record: its count, then the verse of each
                   occurrence (all 4 bytes), then EOR. A verse the word is
                   in more than once repeats: in a verse list, it is listed
                   once. position.dat and plainpos.dat are laid out alike. */
                num_found = fgetl(references_file);
                if (positions)
                {
                        fgetl(position_file);
                        fgetl(plainpos_file);
                }
        }
#ifndef RAMDISK
        /* kept from one search to the next; made bigger if needed */
        need = positions ? (size_t) num_found * OCCURRENCES + 1 : ALLVERSE;
        if (refs_list == NULL || refs_size < need)
        {
                free(refs_list);
                refs_size = need;
                if ((refs_list = (long int *) malloc(refs_size * sizeof(long int))) == NULL)
                {
                        printf("Out of RAM\n");
                        exit_program(1);
                }
        }
#endif
        last = 0;
        while (refs_offset >= 0 && next_number(references_file, &svn))
        {
                if (positions)
                {
                        if (!next_number(position_file, &full) ||
                            !next_number(plainpos_file, &plain))
                        {
                                printf("position.dat or plainpos.dat doesn't match references.dat\n");
                                exit_program(1);
                        }
                }
                else if (svn == last)
                        continue;
                if (svn != last)
                        ++num_verses;
                last = svn;
#ifdef RAMDISK
                fputl(svn, tempfile->fname);
                if (positions)
                {
                        fputl(svn, tempfile->fname);
                        fputl(full, tempfile->fname);
                        fputl(plain, tempfile->fname);
                }
                if (ferror(tempfile->fname)) {
                        printf("Out of space on RAM disk!\n");
                        exit_program(1);
                }
#else
                if (i + width >= refs_size)
                {
                        printf("Array out of bounds!\n");
                        exit_program(1);
                }
                refs_list[i++] = svn;
                if (positions)
                {
                        refs_list[i++] = svn;
                        refs_list[i++] = full;
                        refs_list[i++] = plain;
                }
#endif
        }
#ifdef RAMDISK
        fputl(0L, tempfile->fname);     /* Mark End of list */
        if (ferror(tempfile->fname)) {
                printf("Out of space on RAM disk!\n");
                exit_program(1);
        }
        return((struct TEMPFILE *) tempfile);
#else
        refs_list[i] = 0; /* Mark End of list */
        return(refs_list);
#endif
}
