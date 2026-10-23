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
        static struct termios oldtermio; /* original tty mode */
        static struct termios newtermio; /* cbreak/noecho tty mode */
        static int tty_saved = FALSE;    /* TRUE once oldtermio is valid */
#endif

FILE *book_list, *book_file, *cvarray_file, *bvarray_file, *chapter_file,
     *words_file, *common_file, *references_file;
int a_new_translation = FALSE;

/* number of allowable translations in trans file is two less than this */
#define    MAXTRANS 20
char trans[MAXTRANS][80]; /* room for 80 char description of translation */

/* extended three letter acronym, such as NIV or NRSV */
#define   MAX_ETLA 20
char etla[MAX_ETLA];/*global variable with name of current translation in use*/
long int numverses, numchapters, numbooks, sizeofwordsdat;
int num_found;  /* Number of matches in a word search */
int num_verses;  /* Number of verses in item on stack */
#ifndef COCO
        struct bva *book;
        short int *pach;
#else
        struct bva book[67];
	short int pach[1190];
#endif
long int offset; /* number of bytes offset from the beginning of a book that
                    a particular verse begins at. */
int disable_newverse = 0;/*0,1,or2 mean: enabled, about to enable, disabled */

/* functions used only in this file */
static void open_new_trans(void);
static long int getcvarraydat(short int sequ_verse_number);
static char *read_word(FILE *file_ptr);
static long int lookup(long int start, long int end, char *word);
#ifdef UNIXV
static void set_raw_tty(void);
static void contsignal(int signo);
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
        if (tty_saved)
                tcsetattr(0, TCSADRAIN, &oldtermio);
#endif
        exit(errnum);
}

#ifdef VMS
int ugetchar(void)
{
        char c;
        vms_ttyread(&c, 1, 0);
        return (unsigned char) c;
}
#endif

#ifdef UNIXV
int ugetchar(void)
{
        char c;
        int n;

        do{
                n = read(0, &c, (unsigned)1);
        }while (n < 0 && errno == EINTR); /* interrupted by a signal */
        if (n != 1) /* end of input, or the terminal went away */
                exit_program(0);
        return (unsigned char) c; /* 0..255, like getchar() */
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

        if (isdigit((unsigned char) *string))
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
int i;
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
                printf("Please set BIBLEROOT environment to the directory where EBaC in located.\n");
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
                printf("Please set BIBLEROOT environment to the directory where EBaC in located.\n");
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
	if (chdir(t_path) != 0)
	{
		printf("Can't chdir to %s\n", t_path);
		exit_program(0);
	}
#else
        if (chdir(BIBLEROOT) != 0)
        {
        	printf("Can't chdir to %s\n", BIBLEROOT);
                printf("Please set BIBLEROOT environment to the directory where EBaC in located.\n");
        	exit_program(0);
        }
#endif
        initscreen();
	initlogname();
/*
** This file, trans, must use SPACES, not TABS to separate fields!
*/
        trans_file = mfopen("trans", "r");
        i = 0;
        while ((fgets(trans[i], 80, trans_file) != NULL) && (i + 2 < MAXTRANS))
        	++i;
        trans[i][0] = '\0';
        mfclose(trans_file, "trans");

        i = 0;
        while ((trans[0][i] != ' ') && (i < MAX_ETLA))
        {
        	etla[i] = trans[0][i]; /* default to first trans in list */
        	++i;
        }
        etla[i] = '\0';
        if (chdir(etla) != 0)
        {
        	printf("Can't chdir to %s\n", etla);
        	exit_program(0);
        }
	open_new_trans();
}
        
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
        numbooks /= 4L;
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
                book[i].verse = fgeti(bvarray_file);
                book[i].chapt = fgeti(bvarray_file);
        }
        mfclose(bvarray_file, "bvarray.dat");

        chapter_file = mfopen("chapter.dat", READMODE);
#ifndef COCO
        fseek(chapter_file, 0L, 2);
        numchapters = ftell(chapter_file) / 2L;
    if ((pach = (short int *) malloc (numchapters * sizeof(short int))) == NULL)        {
                printf("Out of RAM\n");
                exit_program(1);
        }
        rewind(chapter_file);
	--numchapters;
#else
        numchapters = 1189;
#endif
        for (i = 0; i <= numchapters; i++)
                pach[i] = fgeti(chapter_file);
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
** My original plans were to use the data in this file somehow...
** This file contains "common" words that occur too often to be indexed.
** The format of the file is, the word stored in ASCII, followed
** immidiately by the number of times the word occurs, in binary,
** followed by an End Of Record character, FF.
** Current plans are to eliminate this file by indexing ALL the words.
** This would mean that the index generator would need 9 meg RAM
** to run, and the references.dat file it creates would be over 1000K.
*/
/*
**      common_file = mfopen("common.dat", READMODE);
*/
        references_file = mfopen("references.dat", READMODE);
}

void change_translations(void)
{
#ifdef MSDOS
#	define SLASH '\\'
#else
#	define SLASH '/'
#endif
	int i = -1;
	int c; /* character the user types */
	int n; /* number the user selects */
	char new_dir[MAX_ETLA + 4]; /* example: ../nrsv */

	cls();
	printf("Please select the number of the translation desired:\n\n");
	while (trans[++i][0] != '\0')
		printf("%d) %s", i + 1, trans[i]);
	c = ugetchar();
	if (!isdigit(c))
		return;
	n = c - '0';
	if (n < 1 || n > i)
		return;
        --n;
        i = 0;
        while ((trans[n][i] != ' ') && (i < MAX_ETLA))
        {
        	etla[i] = trans[n][i]; /* default to first trans in list */
        	++i;
        }
        etla[i] = '\0';

        mfclose(cvarray_file, "cvarray.dat");
        mfclose(words_file, "words.dat");
        mfclose(references_file, "references.dat");

/*
** This is a flag which forces the closing, and opening of text files
** which have the same name, but in different directories.
** For example, reading "genesis" in the NIV directory, then reading
** "genesis" in the AV directory...
*/
	a_new_translation = TRUE;
	sprintf(new_dir, "..%c%s", SLASH, etla);
        if (chdir(new_dir) != 0)
        {
        	printf("Can't chdir to %s\n", new_dir);
        	exit_program(0);
        }
	open_new_trans();
}

static long int getcvarraydat(short int sequ_verse_number)
{
/*
** printf("sequ is: %d seek to: %ld\n",
**     sequ_verse_number, ((long int) sequ_verse_number - 1) * 4);
*/
    if (fseek(cvarray_file, ((long int) sequ_verse_number - 1L) * 4L, 0) == EOF)    {
            printf("FSEEK failed on cvarray_file!\n");
            exit_program(1);
    }
    return(fgetl(cvarray_file));
}

int verse_to_book(short int sequ_verse_number)
{
        int i;
        for(i = 1; i < numbooks; i++)
                if(sequ_verse_number <= book[i].verse)
                        break;
        return(i - 1);
}

static char *read_word(FILE *file_ptr)
{
int i = 0; /* index */
static char word[80];

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
/* Skip over leading white space */
do{
    word[i] = (char) fgetc(file_ptr);
}while (word[i] == ' ' || word[i] == '\n');

/* Read until a delimeter is found, or EOF */
do{
    word[++i] = (char) fgetc(file_ptr);
}while (word[i] != ' ' && word[i] != '\n' && !(feof(file_ptr)));

word[i] = '\0';  /* Need to terminate the string */
if (word[0] == 0x1a) /* We have reached EOF */
        fgetc(file_ptr); /* This makes feof(file_ptr) be TRUE */
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
struct bcv_array_type *get_verse(short int sequ_verse_number)
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
        static short int svn_previous = -1;

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
        while (!(new_verse(word) || feof(file_ptr)))
        {
                strcat(bcv_arrays.data_array, word);
                strcat(bcv_arrays.data_array, " ");
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
        return (&tf);
}

struct TEMPFILE *find_word(char *word)
#else
short int *find_word(char *word)
#endif
{
 long int refs_offset;
#ifdef RAMDISK
 struct TEMPFILE *tempfile;
 short int tsi; /* temporary short int */

 tempfile = tmpopen();
#else
 int i = 0;
 static short int *refs_list = NULL;

 if (refs_list == NULL) /* only malloc once; never free() */
  if ((refs_list = (short int *) malloc(ALLVERSE * sizeof(short int))) == NULL)
  {
        printf("Out of RAM\n");
        exit_program(1);
  }
#endif
        refs_offset = lookup((long) 0, (long) sizeofwordsdat, word);
/*
** printf("Word is at offset: %ld\n", refs_offset);
*/
        num_verses = 0;
        if (refs_offset < 0) /* Word not in index! */
        {
                num_found = 0;
#ifdef RAMDISK
                fputi(0, tempfile->fname);
#else
                refs_list[0] = 0;
#endif
        }else{ /* Word IS in index */
                if (fseek(references_file, refs_offset, 0) == EOF)
                {
                        fprintf(stderr, "fseek failed on references file!\n");
                        exit_program(1);
                }
                num_found = i_bit_reset(fgeti(references_file));
#ifdef RAMDISK
                tsi = fgeti(references_file);
                do{
                        if (ferror(tempfile->fname)) {
                                printf("Out of space on RAM disk!\n");
                                exit_program(1);
                        }
                        fputi(i_bit_reset(tsi), tempfile->fname);
                        ++num_verses;
                        tsi = fgeti(references_file);
                }while((tsi & 0xff00) != 0xff00);
                fputi(0, tempfile->fname);
                if (ferror(tempfile->fname)) {
                        printf("Out of space on RAM disk!\n");
                        exit_program(1);
                }
        }
        return((struct TEMPFILE *) tempfile);
#else
/* i == 0 here always */
                refs_list[i++] = fgeti(references_file);
                do{
                        if (i > (MAXREF - 1))
                        {
                                printf("Array out of bounds!\n");
                                exit_program(1);
                        }
                        refs_list[i - 1] = i_bit_reset(refs_list[i - 1]);
                	++num_verses;
                        refs_list[i++] = fgeti(references_file);
                }while((refs_list[i - 1] & 0xff00) != 0xff00);
                refs_list[i - 1] = 0; /* Mark End of list */
        }
        return((short int *) refs_list);
#endif
}
