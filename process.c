/* 
   EBaC (Electronic Bible and Concordance)
   written by Neil G. Brookins
   http://ebac.sourceforge.net/
*/
#include "bible.h"
#include "screen.h"

/* Allow the user to type two, or more full lines of search parameters */
#define MAXSEARCHLEN 256
/* these search parameters may contain up to this many words to be
** highlighted using reverse video */
#define MAXHIGHWORDS 30
char *rev_word[MAXHIGHWORDS] = { 0 };
int cur_revword = 0;
char count_word[MAXCOUNTWORD] = "";
#ifdef RAMDISK
        struct TEMPFILE tempfile[20];
        int TFtop = 0;
#else
        struct stackptr
        {
                short int *node;
                struct stackptr *next;
        };
        struct stackptr *top = NULL;
#endif

/* functions used only in this file */
static char *fgets_cr P((char *s, int n, FILE *iop));
static int compare P((short int first, short int second, int prox));
static void not_oper P((void));
static void or_oper P((void));
static void and_oper P((int prox));
static void add_rev P((char *word));
static int del_rev P((void));
static void store_book P((int bnumber));
static int is_operator P((char *arg));
static int valid_search P((char *line));
static char *next_term P((char **pos));
#ifdef RAMDISK
static void push P((struct TEMPFILE *verse_list));
static struct TEMPFILE *pop P((void));
#else
static void push P((short int *verse_list));
static short int *pop P((void));
#endif

char LOGFILE[256];
void initlogname(void)
{
#ifdef OSK
        char *getenv();

        strcpy(LOGFILE, getenv("TEMPFILE"));
        if ((*LOGFILE == '\0') || (LOGFILE[strlen(LOGFILE) - 1] != '/'))
        {
printf("You must set TEMPFILE in your .login or startup\n");
printf("For example:  setenv TEMPFILE /r0/\n");
                exit_program(0);
        }
        strcat(LOGFILE, "ebac_log");
#endif
#ifdef COCO
        strcpy (LOGFILE, "/r0/ebac_log");
#endif
#ifdef UNIXV
/*
** Keep the search history in the user's home directory, where it is
** private and survives a reboot. If $HOME is not usable, fall back to
** the old location in /tmp. LOGNAME is often unset (cron, containers,
** some remote logins), so don't depend on it.
*/
        char *home, *user;

        home = getenv("HOME");
        if (home != NULL && *home != '\0' &&
            strlen(home) + sizeof("/.ebac_log") <= sizeof(LOGFILE))
        {
                strcpy (LOGFILE, home);
                strcat (LOGFILE, "/.ebac_log");
        }
        else
        {
                user = getenv("LOGNAME");
                if (user == NULL || *user == '\0')
                        user = getenv("USER");
                if (user == NULL || *user == '\0' ||
                    strlen(user) + sizeof("/tmp/ebac_log_") > sizeof(LOGFILE))
                        user = "unknown";
                strcpy (LOGFILE, "/tmp/ebac_log_");
                strcat (LOGFILE, user);
        }
#endif
#ifdef VMS
        strcpy (LOGFILE, "SYS$SCRATCH:ebac_log.");
#endif
#ifdef MSDOS
        char *getenv();

        strcpy(LOGFILE, getenv("TEMPFILE"));
        if (*LOGFILE == '\0')
        {
printf("You must set TEMPFILE in your autoexec.bat\n");
printf("For example:  set TEMPFILE=C:\\\n");
                exit_program(0);
        }
        strcat(LOGFILE, "ebac_log");
#endif
}

/*
** Just like fgets, but it terminates on \r instead of \n
** This just happens to be the same thing on OS-9 systems.
*/
static char *fgets_cr(char *s, int n, FILE *iop)
{
        register int c;
        register char *cs;

        cs = s;
        while (--n > 0 && (c = getc(iop)) != EOF)
                if ((*cs++ = c) == '\r')
                        break;
        *cs = '\0';
        return((c == EOF && cs == s) ? NULL : s);
}

/* Purpose: read a line from the keyboard. The input is terminated by <enter>.
            The backspace or delete keys can be used to correct errors.
   Bugs: Some braindead terminals don't move the cursor left upon receipt
         of the backspace '\b' character; therefore this should use the
         proper termcap entry for "move cursor left" instead of sending BS.
*/
char *get_line(int mode)
{
        int c, i;
        static char digit_letters[] = "qwertyuiop"; /* stand for 1-9, 0 */
        char *digit; /* letter standing for a digit, in DIGITS_MODE */
        int numeric; /* c may be part of a number, in DIGITS_MODE */
        static char line[MAXSEARCHLEN];
        int place = 0; /* location of cursor within, or after, line of text */
        FILE *fp = NULL; /* log file for the line of text */
        int file_is_closed = TRUE; /* the above file's status */
        int old_length; /* length of previous line; before up/down arrow */
        long int position; /* value returned by ftell() */

        line[0] = '\0';
        /* "search": its history can be stepped through (Up and Down) */
        scr_mode(mode == DIGITS_MODE ? "input" : "search");
        do{
                switch (c = ugetchar())     /* get a single char from user */
                {
                case 0x15:  /* Ctrl-U Kill line */
                        if (line[0] == '\0')
                        {
                            putchar('\7'); /* Beep when backspacing too far */
                            fflush(stdout);
                        }
                        else
                        {
                                while (line[place] != '\0')
                                {           /* move cursor to end of line */
                                        nd_space();
                                        ++place;
                                }           /* and erase the line */
                                Erase_word(strlen(line));
                                line[0] = '\0';
                                place = 0;
                        }
                        break;
                case '\b':   /* BS erase one char to left */
                case '\177': /* DEL for vt keyboards */
                        if (place == 0)
                        {
                            putchar('\7'); /* Beep when backspacing too far */
                            fflush(stdout);
                        }
                        else
                        {
                                putchar('\b');
                                printf("%s ", &line[place]);
                                i = 1 + strlen(&line[place]);
                                while(i--)
                                        putchar('\b');
                                fflush(stdout);
                                i = --place;
                                do{
                                        line[i] = line[i + 1];
                                }while(line[i++] != '\0');

                        }
                        break;
                case 0x4:  /* Ctrl-D, or Delete key: erase char under cursor */
                        if (line[place] == '\0')
                        {
                            putchar('\7'); /* Beep at the end of the line */
                            fflush(stdout);
                        }
                        else
                        {
                                printf("%s ", &line[place + 1]);
                                i = 1 + strlen(&line[place + 1]);
                                while(i--)
                                        putchar('\b');
                                fflush(stdout);
                                i = place;
                                do{
                                        line[i] = line[i + 1];
                                }while(line[i++] != '\0');
                        }
                        break;
                case 0x2:  /* left arrow-key */
                        if (place == 0)
                        {
                            putchar('\7'); /* Beep */
                            fflush(stdout);
                        }
                        else
                        {
                            putchar('\b');
                            fflush(stdout);
                            --place;
                        }
                        break;
                case 0x18: /* Ctrl-X, or Home key: jump to begining of line */
                case 0x1:  /* Ctrl-A, as in most Linux programs */
                        if (place == 0)
                            putchar('\7'); /* Beep */
                        while (place > 0)
                        {
                                putchar('\b');
                                --place;
                        }
                        fflush(stdout);
                        break;
                case 0x6:  /* right arrow-key */
                        if (line[place] == '\0')
                        {
                            putchar('\7'); /* Beep */
                            fflush(stdout);
                        }
                        else
                        {
                            nd_space();
                            fflush(stdout);
                            ++place;
                        }
                        break;
                case 0x12: /* Ctrl-R, or End key: jump to end of line */
                case 0x5:  /* Ctrl-E, as in most Linux programs */
                        if (line[place] == '\0')
                                putchar('\7');
                        while (line[place] != '\0')
                        {
                            nd_space();
                            ++place;
                        }
                        fflush(stdout);
                        break;
                case 0x10: /* up arrow-key */
                case 0x7: /* ctrl-g */
                        if (mode == DIGITS_MODE)
                                break;
                        if (file_is_closed)
                        {
                                if ((fp = fopen(LOGFILE, READMODE)) == NULL)
                                        break;
                                file_is_closed = FALSE;
                                         /* position to end of file */
                                fseek(fp, 0L, 2);
                        }
                        while (place > 0)
                        {
                                putchar('\b');
                                --place;
                        }
                        do{
                                if ((fseek(fp, -2L, 1) != 0) || (ftell(fp) < 0))
                                {
                                        /* cleareof(fp); do we need this? */
                                        fseek(fp, 0L, 0);
                                        break;
                                }
                        }while(fgetc(fp) != '\r');
                        old_length = strlen(line);

                        position = ftell(fp);
                        fgets_cr(line, MAXSEARCHLEN, fp);
                        fseek(fp, position, 0);

                        if ((i = strlen(line)) > 0)
                                line[i - 1] = '\0';
                        fputs(line, stdout);
                        place = strlen(line);
                        for(i = place; i < old_length; i++)
                                putchar(' ');
                        for(i = place; i < old_length; i++)
                                putchar('\b');
                        fflush(stdout);
                        break;
                case 0xe:  /* down arrow-key */
                case 0x16:  /* ctrl-v */
                        if ((mode == DIGITS_MODE) || file_is_closed)
                                break;
                        while (place > 0)
                        {
                                putchar('\b');
                                --place;
                        }
                        old_length = strlen(line);

                        fgets_cr(line, MAXSEARCHLEN, fp);
                        position = ftell(fp);
                        fgets_cr(line, MAXSEARCHLEN, fp);
                        fseek(fp, position, 0);

                        if ((i = strlen(line)) > 0)
                                line[i - 1] = '\0';
                        fputs(line, stdout);
                        place = strlen(line);
                        for(i = place; i < old_length; i++)
                                putchar(' ');
                        for(i = place; i < old_length; i++)
                                putchar('\b');
                        fflush(stdout);
                        break;
                default:
                        /*
                        ** When only a number can be typed (chapter and verse),
                        ** the letters under the digits on a keyboard stand for
                        ** them, q for 1 ... o for 9 and p for 0, and m stands
                        ** for the minus sign, so a phone's on-screen keyboard
                        ** needn't be switched to numbers. The digit or minus
                        ** sign is what appears on the screen.
                        */
                        if (mode == DIGITS_MODE && c > 0 && c < 256 &&
                            (digit = find_char(digit_letters, tolower(c))) != NULL)
                                c = "1234567890"[digit - digit_letters];
                        if (mode == DIGITS_MODE && (c == 'm' || c == 'M'))
                                c = '-';
                        /* a number may start with a minus sign */
                        numeric = isdigit(c) || (c == '-' && line[0] == '\0');
                        if ((mode == DIGITS_MODE) &&
                            (!numeric) && (line[0] != '\0'))
                                c = '\r';
                        if ((mode == DIGITS_MODE) &&
                            (!numeric) && (c != '\r'))
                        {
                                putchar('\7');
                                fflush(stdout);
                        }
                        else
                        {
                                putchar(c);        /* put onto video display */
                   /* Accept the whole line, even if cursor is in the middle */
                                if (c == '\r')
                                        place = strlen(line);
                                printf("%s", &line[place]);
                                i = strlen(&line[place]);
                                while(i--)
                                        putchar('\b');
                                fflush(stdout);
                                i = strlen(line);
                                if (i == (MAXSEARCHLEN - 1))
                                {
                                    printf("\nOut of room in string!\n");
                                    exit_program(1);
                                }
                                while(i >= place)
                                {
                                    line[i + 1] = line[i];
                                    --i;
                                }
                                line[place++] = c;
                        }
                }
        }while(c != '\r');
        if (!file_is_closed)
                mfclose(fp, LOGFILE); /* close file which WAS is read mode */
        if (mode != DIGITS_MODE) /* Don't log chapt, and verse numbers */
        {
                fp = mfopen(LOGFILE, APPENDMODE);
                if (fputs(line, fp) == -1)
                {
                        printf("Can't append to log file.\n");
                        exit_program(1);
                }
                mfclose(fp, LOGFILE);
        }

        i = 0; /* Make line lowercase to match index data which is lowercase */
        while ((line[i] = tolower(UCHAR(line[i]))) != '\0')
                i++;
        return((char *)line);
}

#ifdef RAMDISK
static void push(struct TEMPFILE *verse_list)
{
        tempfile[TFtop].fname = verse_list->fname;
        strcpy(tempfile[TFtop].sname, verse_list->sname);
        ++TFtop;
}
#else
static void push(short int *verse_list)
{
        int count = 0, i = 0;
        struct stackptr *stack;

        do{
                count++; /* How many verses are there? */
        }while (verse_list[i++] != 0);

        if ((stack = (struct stackptr *) malloc (sizeof (struct stackptr))) ==
                                                                        NULL)
        {
             printf("Out of RAM\n");
             exit_program(1);
        }
/* allocate room for each verse */
        if ((stack->node = (short int *) malloc (count * sizeof(short int))) == NULL)
        {
             printf("Out of RAM\n");
             exit_program(1);
        }
        stack->next = top;
        top = stack;

        i = 0;
        do{
                stack->node[i] = verse_list[i];
        }while (verse_list[i++] != 0);
}
#endif

#ifdef RAMDISK
static struct TEMPFILE *pop(void)
{
        if (--TFtop < 0)
        {
                printf("Stack is empty. Cannot pop.\n");
                printf("To restart the program, type \"bible\"\n");
                exit_program(1);
        }
        return ((struct TEMPFILE *) &tempfile[TFtop]);
}
#else
static short int *pop(void)
{
        short int *verse_list; /* list of verses to be returned */
        struct stackptr *off;  /* item popped 'off' the stack */

/* We need better error handling than this for when the user types too many
** operators for the number of operands on the command line. */
        if (top == NULL)
        {
                printf("Stack is empty. Cannot pop.\n");
                printf("To restart the program, type \"bible\"\n");
                exit_program(1);
        }
        off = top;
        verse_list = top->node;
        top = top->next;
        free (off);
        return ((short int *) verse_list);
}
#endif

/*
** these three funtions remove two items from the list, combine their elements
** via some method into a non-dynamic array; push result onto the stack;
** free memory used by original two items.
*/

/* Purpose: Create a list of numbers in "result" whose elements are in the
**          list "keep" but not in the list "remove".
** Example: given that keep = 1, 2, 0
**                 and remove = 2, 6, 0
**                then result = 1, 0     (the 0 marks the end of the list)
*/
#ifdef RAMDISK
static void not_oper(void)
{
        struct TEMPFILE *result;
        struct TEMPFILE *rfile;
        struct TEMPFILE *kfile;
        short int remove, keep;

        result = tmpopen();
        rfile = pop();
        kfile = pop();

        rewind(rfile->fname);
        remove = fgeti(rfile->fname);

        rewind(kfile->fname);
        keep = fgeti(kfile->fname);
        
        num_verses = 0;
        while (remove != 0 && keep != 0)
        {
                if (keep > remove)
                {
                        remove = fgeti(rfile->fname);
                        continue;
                }
                if (keep < remove)
                {
                        fputi(keep, result->fname);
                        ++num_verses;
                        keep = fgeti(kfile->fname);
                        continue;
                }
                keep = fgeti(kfile->fname);
                remove = fgeti(rfile->fname);
        }
        fputi(keep, result->fname);
        while(keep != 0)
        {
                keep = fgeti(kfile->fname);
                fputi(keep, result->fname);
                ++num_verses;
        }
        mfclose(rfile->fname, rfile->sname);
        unlink(rfile->sname);
        mfclose(kfile->fname, kfile->sname);
        unlink(kfile->sname);
        push(result);
}
#else
static void not_oper(void)
{
        short int *result;
        short int *remove, *r, *keep, *k;
        int i = 0;

  if ((result = (short int *) malloc(ALLVERSE * sizeof(short int))) == NULL)
        {
             printf("Out of RAM\n");
             exit_program(1);
        }
        r = remove = pop();
        k = keep = pop();
        num_verses = -1;
        while (*remove != 0 && *keep != 0)
        {
                if (*keep > *remove)
                {
                        remove++;
                        continue;
                }
                if (*keep < *remove)
                {
                        result[i++] = *keep++;
                        ++num_verses;
                        continue;
                }
/* The "continue" statement above causes these two lines to be skipped when
** "if" evaluates as TRUE */
                keep++;
                remove++;
        }
        do{
                result[i++] = *keep;
                ++num_verses;
        }while (*keep++ != 0);
        free (r);
        free (k);
        push (result);
        free (result);
}
#endif

/* Purpose: combine two lists of numbers using a logical OR function.
** Example: given that first = 1, 2, 0
**                 and second = 2, 6, 0
**                then result = 1, 2, 6, 0
*/
#ifdef RAMDISK
static void or_oper(void)
{
        struct TEMPFILE *result;
        struct TEMPFILE *sec_file;
        struct TEMPFILE *fir_file;
        short int second, first;

        result = tmpopen();
        sec_file = pop();
        fir_file = pop();

        rewind(sec_file->fname);
        second = fgeti(sec_file->fname);

        rewind(fir_file->fname);
        first = fgeti(fir_file->fname);
        
        num_verses = 0;
        while (second != 0 && first != 0)
        {
                if (first > second)
                {
                        fputi(second, result->fname);
                        ++num_verses;
                        second = fgeti(sec_file->fname);
                        continue;
                }
                if (first < second)
                {
                        fputi(first, result->fname);
                        ++num_verses;
                        first = fgeti(fir_file->fname);
                        continue;
                }
                fputi(first, result->fname);
                ++num_verses;
                first = fgeti(fir_file->fname);
                second = fgeti(sec_file->fname);
        }
        while(first != 0)
        {
                fputi(first, result->fname);
                ++num_verses;
                first = fgeti(fir_file->fname);
        }
        fputi(second, result->fname);
        while(second != 0)
        {
                second = fgeti(sec_file->fname);
                fputi(second, result->fname);
                ++num_verses;
        }
        mfclose(sec_file->fname, sec_file->sname);
        unlink(sec_file->sname);
        mfclose(fir_file->fname, fir_file->sname);
        unlink(fir_file->sname);
        push(result);
}
#else
static void or_oper(void)
{
        short int *result;
        short int *second, *s, *first, *f;
        int i = 0;

  if ((result = (short int *) malloc(ALLVERSE * sizeof(short int))) == NULL)
        {
             printf("Out of RAM\n");
             exit_program(1);
        }
        s = second = pop();
        f = first = pop();
        num_verses = -1;
        while (*second != 0 && *first != 0)
        {
                if (*first > *second)
                {
                        result[i++] = *second++;
                        ++num_verses;
                        continue;
                }
                if (*first < *second)
                {
                        result[i++] = *first++;
                        ++num_verses;
                        continue;
                }
/* The "continue" statement above causes these two lines to be skipped when
** "if" evaluates as TRUE */
                result[i++] = *first++; /* first and second are the same */
                ++num_verses;
                second++;
        }

        while (*first != 0)
        {
                result[i++] = *first++;
                ++num_verses;
        }

        do{
                result[i++] = *second;
                ++num_verses;
        }while (*second++ != 0);

        free (s);
        free (f);
        push (result);
        free (result);
}
#endif

static int compare(short int first, short int second, int prox)
{
        if (first == second)
                return (0);
        if (first < second)
                if (first + prox >= second)
                        return (0);
                else
                        return (-1);
        else
                if (second + prox >= first)
                        return (0);
                else
                        return (1);
}

/* Purpose: Combine two lists of numbers using a proximity sensitive logical
**          AND function. A number, x, in one list will be copied into
**          the result list if and only if there exists at least one number, y,
**          in the other list such that the range, plus or minus prox,
**          from y contains x.
** Options: When prox is set to 0, every number in the result is in both
**          first and second lists. When prox is set to 32000, this function
**          acts the same as the OR function. (Assuming non-null lists)
**
** Example: given that prox = 2
**                 and first = 1, 2, 3, 4, 5, 10, 11, 12, 0
**                 and second = 5, 6, 7, 8, 20, 21, 0
**                then result = 3, 4, 5, 6, 7, 0
*/
#ifdef RAMDISK
static void and_oper(int prox)
{
    struct TEMPFILE *result;
    struct TEMPFILE *sec_file;
    struct TEMPFILE *fir_file;
    struct TEMPFILE protect_file;
    struct TEMPFILE *swap_file;
    short int second, first;
    char second_time = FALSE;
    int diff;

    result = tmpopen();
    sec_file = pop();

/*
** the memory location which pop() returns here is overwritten by the
** call to push() near the end of this module. We protect the structure
** by copying it to a local struct, then point to it.
*/
    fir_file = pop();
    protect_file.fname = fir_file->fname;
    strcpy(protect_file.sname, fir_file->sname);
    fir_file = &protect_file;

    rewind(sec_file->fname);
    second = fgeti(sec_file->fname);
    rewind(fir_file->fname);
    first = fgeti(fir_file->fname);
/*
** A special case where the second of two words being "anded" together
** was not found in the index.
*/
    if ((second == 0) || (first == 0))
    {
        fputi(0, result->fname);
        mfclose(sec_file->fname, sec_file->sname);
        unlink(sec_file->sname);
        mfclose(fir_file->fname, fir_file->sname);
        unlink(fir_file->sname);
        push(result);
        num_verses = 0;
        return;
    }
    for(;;) /* This always loops twice. */
    {
        if (second_time)
        {
                result = tmpopen();
                rewind(sec_file->fname);
                second = fgeti(sec_file->fname);
                rewind(fir_file->fname);
                first = fgeti(fir_file->fname);
        }
        while (first != 0)
        {
                if ((diff = compare (first, second, prox)) == 0)
                        fputi(first, result->fname);
                if (diff < 0)
                {
                    for(;;)
                    {
                            if (ftell(sec_file->fname) > 2)
                            {
                                if (fseek(sec_file->fname, -4L, 1) != 0)
                                {
                                        printf("fseek failed!!\n");
                                        exit_program(1);
                                }
                                second = fgeti(sec_file->fname);
                            }
                            else
                                break;
                            if ((diff = compare (first, second, prox)) == 0)
                            {
                                    fputi(first, result->fname);
                                    break;
                            }
                            if (diff > 0)
                                    break;
                    }
                }
                if (diff > 0)
                {
                    for(;;)
                    {
                            second = fgeti(sec_file->fname);
                            if (second == 0) /* past last element */
                            {
                                if (fseek(sec_file->fname, -4L, 1) != 0)
                                {
                                        printf("fseek failed!!\n");
                                        exit_program(1);
                                }
                                second = fgeti(sec_file->fname);
                                break;
                            }
                            if ((diff = compare (first, second, prox)) == 0)
                            {
                                    fputi(first, result->fname);
                                    break;
                            }
                            if (diff < 0)
                                    break;
                    }
                }
                first = fgeti(fir_file->fname);
        }

        fputi(0, result->fname); /* terminate this list */
        if (second_time)
                break;
        second_time = TRUE;
        push (result);
/*
** Swap the two lists to run through the above algorithm a second time.
*/
        swap_file = fir_file;
        fir_file = sec_file;
        sec_file = swap_file;   
    }
    mfclose(sec_file->fname, sec_file->sname);
    unlink(sec_file->sname);
    mfclose(fir_file->fname, fir_file->sname);
    unlink(fir_file->sname);
    push (result);
/*
** The results so far consist of two lists of numbers. Since the AND
** operation by definition has one list of results we combine the two
** lists with an OR operation.
*/
    or_oper();
}
#else
static void and_oper(int prox)           /* proximity */
{
    short int *result;
    short int *second, *s, *first, *f, *swap;
    int i = 0;
    char second_time = FALSE;
    int diff;

    if ((result = (short int *) malloc(ALLVERSE * sizeof(short int))) == NULL)
    {
         printf("Out of RAM\n");
         exit_program(1);
    }
    s = second = pop();
    f = first = pop();
    if ((*second == 0) || (*first == 0))
    {
        result[0] = 0;
        free (s);
        free (f);
        push(result);
        free(result);
        num_verses = 0;
        return;
    }
    for(;;) /* This always loops twice. */
    {
        while (*first != 0)
        {
                if ((diff = compare (*first, *second, prox)) == 0)
                        result[i++] = *first;
                if (diff < 0)
                {
                    for(;;)
                    {
                            if (--second < s) /* past first element */
                            {
                                    second++;
                                    break;
                            }
                            if ((diff = compare (*first, *second, prox)) == 0)
                            {
                                    result[i++] = *first;
                                    break;
                            }
                            if (diff > 0)
                                    break;
                    }
                }
                if (diff > 0)
                {
                    for(;;)
                    {
                            ++second;
                            if (*second == 0) /* past last element */
                            {
                                    second--;
                                    break;
                            }
                            if ((diff = compare (*first, *second, prox)) == 0)
                            {
                                    result[i++] = *first;
                                    break;
                            }
                            if (diff < 0)
                                    break;
                    }
                }
                first++;
        }

        result[i++] = 0; /* terminate this list */
        if (second_time)
                break;
        second_time = TRUE;
        push (result);
/*
** Swap the two lists to run through the above algorithm a second time.
*/
        swap = f;
        f = s;
        s = swap;
/*
** Restore "first" and "second" to their original values which were saved
** in "f" and "s".
*/
        first = f;
        second = s;
        i = 0;

    }
    free (s);
    free (f);
    push (result);
    free (result);
/*
** The results so far consist of two lists of numbers. Since the AND
** operation by definition has one list of results we combine the two
** lists with an OR operation.
*/
    or_oper();
}
#endif

static void add_rev(char *word)
{
        rev_word[cur_revword++] = word;
        rev_word[cur_revword] = 0;
}

static int del_rev(void)
{
        if (cur_revword <= 0)
                return (FALSE);
        else
        {
                rev_word[--cur_revword] = 0;
                return (TRUE);
        }
}

/*
** store list of every verse in a particular book into element on stack
*/
static void store_book(int bnumber)
{
#ifdef RAMDISK
        struct TEMPFILE *book_file;
        short int cur_node;
#else
        struct stackptr *stack;
        short int *cur_node;
        short int i;
#endif
        struct bcv_type bcv;
        short int first_verse, last_verse;

        bcv.book_number = bnumber;
        bcv.chapter_number = 1;
        bcv.verse_number = 1;

        first_verse = bcv_search(&bcv);

        bcv.book_number = bnumber + 1;
        bcv.chapter_number = 1;
        bcv.verse_number = 0;

        last_verse = bcv_search(&bcv);
/*
** printf("Storing from: %d to %d. Hit any key.\n", first_verse, last_verse);
** ugetchar();
*/

        num_verses = last_verse - first_verse + 1; /* a global variable */
#ifdef RAMDISK
        book_file = tmpopen();
        for (cur_node = first_verse; cur_node <= last_verse; cur_node++)
        {
                fputi(cur_node, book_file->fname);
        }
        fputi(0, book_file->fname); /* terminate the list */
        push(book_file);
#else
        if ((stack = (struct stackptr *) malloc (sizeof (struct stackptr))) ==
                                                                        NULL)
        {
             printf("Out of RAM\n");
             exit_program(1);
        }
        if ((cur_node = stack->node = (short int *) malloc(sizeof(short int) *
                                (last_verse - first_verse + 2))) == NULL)
        {
             printf("Out of RAM\n");
             exit_program(1);
        }
        for (i = first_verse; i <= last_verse; i++)
        {
                *cur_node++ = i;
        }
        *cur_node = 0; /* terminate the list */
        stack->next = top;
        top = stack;
#endif
}

/* Purpose: TRUE if arg is one of the search operators: - ! | , or a number */
static int is_operator(char *arg)
{
        return (strcmp (arg, "-") == 0 || strcmp (arg, "!") == 0 ||
                strcmp (arg, "|") == 0 || strcmp (arg, ",") == 0 ||
                isdigit (UCHAR(*arg)));
}

/*
** Purpose: check a search line before evaluating it. A postfix expression
**          needs exactly one more word (or @book) than it has operators,
**          and every operator needs two results before it. Without this
**          check a mistake emptied the stack and ended the program, or
**          left stale results behind to corrupt the next search.
** Returns: TRUE if the line can be evaluated. Otherwise it explains the
**          problem, waits for a key, and returns FALSE.
*/
static int valid_search(char *line)
{
        static char copy[MAXSEARCHLEN];
        char *arg, *pos;
        int depth = 0;    /* number of results the stack would hold */
        int operands = 0; /* number of words and books */

        strcpy(copy, line);
        pos = copy;
        for (arg = next_term(&pos); arg != NULL; arg = next_term(&pos))
        {
                if (is_operator(arg))
                {
                        if (depth < 2)
                        {
                                printf("\nThe operator \"%s\" needs two words or books before it.\n", arg);
                                break;
                        }
                        depth--;
                        continue;
                }
                if (*arg == '@')
                {
                        switch (match_it(&arg[1], FALSE))
                        {
                        case -1:
                                printf("\nNo such book: %s\n", &arg[1]);
                                depth = -1;
                                break;
                        case 0:
                                printf("\nNot unique: %s\n", &arg[1]);
                                depth = -1;
                                break;
                        }
                        if (depth < 0)
                                break;
                }
                if (++operands >= MAXHIGHWORDS)
                {
                        printf("\nToo many words; the limit is %d.\n", MAXHIGHWORDS - 1);
                        depth = -1;
                        break;
                }
                depth++;
        }
        if (arg == NULL) /* reached the end of the line without an error */
        {
                if (depth == 0) /* blank line: nothing to search for */
                        return FALSE;
                if (depth == 1)
                        return TRUE;
                printf("\nThere are %d more words or books than operators can combine.\n",
                       depth - 1);
                printf("Put an operator after each pair, for example:  love hate 0\n");
        }
        printf("The search was not done.  (Press any Key) ");
        fflush(stdout);
        scr_mode("anykey");
        ugetchar();
        return FALSE;
}

/* Purpose: finds the next word, book or operator of a search line, from
            *pos on. They are separated by spaces, or the line's carriage
            return. Ends it with a '\0' and moves *pos past it. (Like
            strtok(), which the C libraries of older systems may not have.)
   Returns: the term, or NULL at the end of the line.
*/
static char *next_term(char **pos)
{
        char *p = *pos, *start;

        while (*p == ' ' || *p == '\r')
                p++;
        if (*p == '\0')
        {
                *pos = p;
                return (NULL);
        }
        start = p;
        while (*p != '\0' && *p != ' ' && *p != '\r')
                p++;
        if (*p != '\0')
                *p++ = '\0';
        *pos = p;
        return (start);
}

/* Purpose: get search term(s) from the user. Process into a list of verses
** Returns: the list, or NULL if the line was blank or not a valid search.
*/
#ifdef RAMDISK
struct TEMPFILE *process_words(void)
{
        struct TEMPFILE *verse_list;
#else
short int *process_words(void)
{
        short int *verse_list;
#endif
        char *arg;  /* Points to argument within search line */
        int i;
        char end_of_list;
        int book_number;
        int terms = 0; /* words, books and operators in the search */
        count_word[0] = '\0';
        while (del_rev()) /* Remove all words from list which had been */
                ;         /* displayed in reverse video. */
        arg = get_line(NORMAL_MODE); /* Get search terms from user. */
        if (!valid_search(arg))
                return (NULL);
        while (*arg++ == ' ') /* Get rid of leading spaces */
                ;
        arg--;
        end_of_list = FALSE;
        for(;;)
        {
                i = -1;
                while(arg[++i] != '\r')
                {
                        if (arg[i] == ' ')
                        {
                                arg[i] = '\0';
                                break;
                        }
                }
                if (arg[i] == '\r')
                {
                        arg[i] = '\0';
                        end_of_list = TRUE;
                }
                if (strlen (arg) > 0)
                        terms++;
                if ((strcmp (arg, "-") == 0) || (strcmp (arg, "!") == 0))
                {
                        del_rev(); /* Don't highlight removed words */
                        not_oper();
                }
                else if ((strcmp (arg, "|") == 0) || (strcmp (arg, ",") == 0))
                        or_oper();
                else if (isdigit (UCHAR(*arg))) /* only looks at first digit */
                        and_oper(atoi(arg));
                else if (*arg == '@')
                {
                        switch (book_number = match_it(&arg[1], FALSE))
                        {
                        case -1:
                                printf("No such book: %s\n", &arg[1]);
                                break;
                        case 0:
                                printf("Not unique: %s\n", &arg[1]);
                                break;
                        default:
/* this add_rev() is a kluge so the and_not doesn't remove wanted stuff */
                                add_rev("xyzzy");
                                store_book(book_number);
                                break;
                        }       
                }
                else if (strlen (arg) > 0)
                {
                        add_rev(arg);
                        if (terms == 1)
                        {
                                strncpy(count_word, arg, MAXCOUNTWORD - 1);
                                count_word[MAXCOUNTWORD - 1] = '\0';
                        }
                        verse_list = find_word(arg);
                        push(verse_list);
                }

                if (end_of_list)
                        break;
                while (*arg++ != '\0') /* Move to next argument on line */
                        ;
                while (*arg++ == ' ') /* Get rid of leading spaces */
                        ;
                arg--;
        }
        if (terms != 1)
                count_word[0] = '\0'; /* a count of one word would mislead */
        verse_list = pop();
#ifdef RAMDISK
        return((struct TEMPFILE *) verse_list);
#else
        return((short int *) verse_list);
#endif
}

