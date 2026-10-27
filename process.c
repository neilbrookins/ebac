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
                long int *node;
                int width;      /* VERSES or OCCURRENCES (see bible.h) */
                struct stackptr *next;
        };
        static int popped_width; /* the width of the list pop() returned */
        struct stackptr *top = NULL;
#endif

/* functions used only in this file */
static char *fgets_cr P((char *s, int n, FILE *iop));
static int compare P((long int first, long int second, int prox));
static void not_oper P((void));
static void or_oper P((void));
static void and_oper P((int prox));
static int adjacent P((long int a_end, long int a_full, long int a_plain,
                       long int b_svn, long int b_full, long int b_plain));
static void phrase_oper P((void));
static void exact_oper P((long int n, int mode));
static int is_exact P((char *arg));
static void add_rev P((char *word));
static int del_rev P((void));
static void store_books P((char *set));
static int group_set P((char *spec, char *set, int report));
static int is_operator P((char *arg));
static int valid_search P((char *line));
static char *next_term P((char **pos));
#ifdef RAMDISK
static void push P((struct TEMPFILE *verse_list));
static struct TEMPFILE *pop P((void));
static void pop_verses P((struct TEMPFILE *out));
static void shift_match P((struct TEMPFILE *x, struct TEMPFILE *y, long int shift));
static int read_entry P((FILE *file, long int *e));
#else
static void push P((long int *verse_list, int width));
static long int *pop P((void));
static long int *pop_verses P((void));
static void shift_match P((long int *x, long int *y, long int shift));
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

/*
** The line being typed is UTF-8 (EBaC 2.3): a character may be more than one
** byte, and take no column on the screen (a mark on the letter before it),
** one, or two. The cursor moves, and a key erases, a whole character, with
** any marks after it; the screen is moved over by columns. (See utf8.c.)
*/

/* where the character before byte place of line starts, with its marks */
static int char_start(char *line, int place)
{
        int j = u8_prev(line, place);

        while (j > 0 && u8_width(&line[j]) == 0)
                j = u8_prev(line, j);
        return (j);
}

/* where the character at byte place of line ends (after its marks) */
static int char_end(char *line, int place)
{
        int j = place + u8_len(&line[place]);

        while (line[j] != '\0' && u8_width(&line[j]) == 0)
                j += u8_len(&line[j]);
        return (j);
}

/* move the cursor back n columns */
static void back_cols(int n)
{
        while (n-- > 0)
                putchar('\b');
}

/* show line from byte from on, then blank columns, and put the cursor back
   where the shown part began */
static void show_rest(char *line, int from, int blank)
{
        int i;

        printf("%s", &line[from]);
        for (i = 0; i < blank; i++)
                putchar(' ');
        back_cols(u8_cols(&line[from], strlen(&line[from])) + blank);
}

/* take bytes from to to out of line */
static void remove_bytes(char *line, int from, int to)
{
        do{
                line[from] = line[to++];
        }while(line[from++] != '\0');
}

/* Purpose: read a line from the keyboard. The input is terminated by <enter>.
            The backspace or delete keys can be used to correct errors.
   Bugs: Some braindead terminals don't move the cursor left upon receipt
         of the backspace '\b' character; therefore this should use the
         proper termcap entry for "move cursor left" instead of sending BS.
*/
char *get_line(int mode)
{
        int c, c2, i;
        static char digit_letters[] = "qwertyuiop"; /* stand for 1-9, 0 */
        char *digit; /* letter standing for a digit, in DIGITS_MODE */
        int numeric; /* c may be part of a number, in DIGITS_MODE */
        static char line[MAXSEARCHLEN];
        int place = 0; /* location of cursor within, or after, line of text */
        FILE *fp = NULL; /* log file for the line of text */
        int file_is_closed = TRUE; /* the above file's status */
        int old_length; /* columns of previous line; before up/down arrow */
        int width;      /* columns of the character erased */
        char typed[8];  /* a character typed: in UTF-8, up to 4 bytes */
        int n;
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
                                for (i = u8_cols(&line[place], strlen(&line[place]));
                                     i > 0; i--)
                                        nd_space(); /* move cursor to end of line */
                                Erase_word(u8_cols(line, strlen(line)));
                                                    /* and erase the line */
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
                                i = char_start(line, place);
                                width = u8_cols(&line[i], place - i);
                                back_cols(width);
                                remove_bytes(line, i, place);
                                place = i;
                                show_rest(line, place, width);
                                fflush(stdout);
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
                                i = char_end(line, place);
                                width = u8_cols(&line[place], i - place);
                                remove_bytes(line, place, i);
                                show_rest(line, place, width);
                                fflush(stdout);
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
                            i = char_start(line, place);
                            back_cols(u8_cols(&line[i], place - i));
                            fflush(stdout);
                            place = i;
                        }
                        break;
                case 0x18: /* Ctrl-X, or Home key: jump to begining of line */
                case 0x1:  /* Ctrl-A, as in most Linux programs */
                        if (place == 0)
                            putchar('\7'); /* Beep */
                        back_cols(u8_cols(line, place));
                        place = 0;
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
                            for (i = char_end(line, place); place < i; place++)
                                putchar(line[place]);  /* (over it, by its width) */
                            fflush(stdout);
                        }
                        break;
                case 0x12: /* Ctrl-R, or End key: jump to end of line */
                case 0x5:  /* Ctrl-E, as in most Linux programs */
                        if (line[place] == '\0')
                                putchar('\7');
                        for (i = u8_cols(&line[place], strlen(&line[place]));
                             i > 0; i--)
                            nd_space();         /* (by columns) */
                        place = strlen(line);
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
                        back_cols(u8_cols(line, place));
                        place = 0;
                        do{
                                if ((fseek(fp, -2L, 1) != 0) || (ftell(fp) < 0))
                                {
                                        /* cleareof(fp); do we need this? */
                                        fseek(fp, 0L, 0);
                                        break;
                                }
                        }while(fgetc(fp) != '\r');
                        old_length = u8_cols(line, strlen(line));

                        position = ftell(fp);
                        fgets_cr(line, MAXSEARCHLEN, fp);
                        fseek(fp, position, 0);

                        if ((i = strlen(line)) > 0)
                                line[i - 1] = '\0';
                        fputs(line, stdout);
                        place = strlen(line);
                        for(i = u8_cols(line, place); i < old_length; i++)
                                putchar(' ');
                        for(i = u8_cols(line, place); i < old_length; i++)
                                putchar('\b');
                        fflush(stdout);
                        break;
                case 0xe:  /* down arrow-key */
                case 0x16:  /* ctrl-v */
                        if ((mode == DIGITS_MODE) || file_is_closed)
                                break;
                        back_cols(u8_cols(line, place));
                        place = 0;
                        old_length = u8_cols(line, strlen(line));

                        fgets_cr(line, MAXSEARCHLEN, fp);
                        position = ftell(fp);
                        fgets_cr(line, MAXSEARCHLEN, fp);
                        fseek(fp, position, 0);

                        if ((i = strlen(line)) > 0)
                                line[i - 1] = '\0';
                        fputs(line, stdout);
                        place = strlen(line);
                        for(i = u8_cols(line, place); i < old_length; i++)
                                putchar(' ');
                        for(i = u8_cols(line, place); i < old_length; i++)
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
                        /* a character beyond ASCII: all its bytes (UTF-8) */
                        typed[0] = (char) c;
                        n = 1;
                        if (c >= 0xC2 && c <= 0xF4)
                        {
                                i = (c >= 0xF0) ? 4 : (c >= 0xE0) ? 3 : 2;
                                while (n < i)
                                {
                                        c2 = ugetchar();
                                        if ((c2 & 0xC0) != 0x80)
                                                break;  /* (cut short) */
                                        typed[n++] = (char) c2;
                                }
                        }
                        if (mode == DIGITS_MODE && c > 0 && c < 128 &&
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
                                if (c < 128)       /* (a digit, - or Enter for it) */
                                {
                                        typed[0] = (char) c;
                                        n = 1;
                                }
                                for (i = 0; i < n; i++)
                                        putchar(typed[i]); /* put onto video display */
                   /* Accept the whole line, even if cursor is in the middle */
                                if (c == '\r')
                                        place = strlen(line);
                                show_rest(line, place, 0);
                                fflush(stdout);
                                i = strlen(line);
                                if (i + n > (MAXSEARCHLEN - 1))
                                {
                                    printf("\nOut of room in string!\n");
                                    exit_program(1);
                                }
                                while(i >= place)
                                {
                                    line[i + n] = line[i];
                                    --i;
                                }
                                for (i = 0; i < n; i++)
                                    line[place++] = typed[i];
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

        /* Make line lowercase to match index data which is lowercase */
        u8_lower(line);
        return((char *)line);
}

#ifdef RAMDISK
static void push(struct TEMPFILE *verse_list)
{
        tempfile[TFtop].fname = verse_list->fname;
        strcpy(tempfile[TFtop].sname, verse_list->sname);
        tempfile[TFtop].width = verse_list->width;
        ++TFtop;
}
#else
/* push a copy of a list: width numbers per entry, ending with an entry
   whose first number is 0 */
static void push(long int *verse_list, int width)
{
        size_t count, i;
        struct stackptr *stack;

        for (count = 0; verse_list[count] != 0; count += width)
                ;
        count++;        /* the 0 */

        if ((stack = (struct stackptr *) malloc (sizeof (struct stackptr))) ==
                                                                        NULL)
        {
             printf("Out of RAM\n");
             exit_program(1);
        }
/* allocate room for each verse */
        if ((stack->node = (long int *) malloc (count * sizeof(long int))) == NULL)
        {
             printf("Out of RAM\n");
             exit_program(1);
        }
        stack->width = width;
        stack->next = top;
        top = stack;

        for (i = 0; i < count; i++)
                stack->node[i] = verse_list[i];
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
static long int *pop(void)
{
        long int *verse_list; /* list of verses to be returned */
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
        popped_width = top->width;
        top = top->next;
        free (off);
        return ((long int *) verse_list);
}
#endif

/*
** Purpose: pop a list as a verse list, for the operators other than "."
**          (and for the list of verses found): an occurrence list (of a
**          word, or a phrase, in a search with phrases) is made into a
**          verse list first. Each occurrence gives every verse from where
**          it starts to its last word (a phrase may run from one verse into
**          the next). The occurrences are in order of their last verse, so
**          those verses less 0, those less 1, and so on, are each in order:
**          they are made into lists, and combined with OR.
*/
#ifdef RAMDISK
static void pop_verses(struct TEMPFILE *out)
{
        struct TEMPFILE *list;
        long int e[OCCURRENCES], prev, span, maxspan = 0, count = 0;

        *out = *pop();
        if (out->width != OCCURRENCES)
                return;
        rewind(out->fname);
        while (read_entry(out->fname, e))
                if (e[1] - e[0] > maxspan)
                        maxspan = e[1] - e[0];
        for (span = 0; span <= maxspan; span++)
        {
                list = tmpopen();
                rewind(out->fname);
                prev = 0;
                count = 0;
                while (read_entry(out->fname, e))
                        if (e[1] - e[0] >= span && e[1] - span != prev)
                        {
                                prev = e[1] - span;
                                fputl(prev, list->fname);
                                count++;
                        }
                fputl(0L, list->fname);
                push(list);
        }
        mfclose(out->fname, out->sname);
        unlink(out->sname);
        for (span = 1; span <= maxspan; span++)
                or_oper();
        if (maxspan == 0)
                num_verses = count;
        *out = *pop();
}

/* Purpose: read an occurrence list's next entry into e[].
   Returns: FALSE at the end of the list. */
static int read_entry(FILE *file, long int *e)
{
        int i;

        if ((e[0] = fgetl(file)) == 0)
                return (FALSE);
        for (i = 1; i < OCCURRENCES; i++)
                e[i] = fgetl(file);
        return (TRUE);
}
#else
static long int *pop_verses(void)
{
        long int *occ, *list, prev, span, maxspan = 0;
        size_t i, j, n;

        occ = pop();
        if (popped_width != OCCURRENCES)
                return (occ);
        for (n = 0; occ[n] != 0; n += OCCURRENCES)
                if (occ[n + 1] - occ[n] > maxspan)
                        maxspan = occ[n + 1] - occ[n];
        if ((list = (long int *) malloc((n / OCCURRENCES + 1) * sizeof(long int))) == NULL)
        {
                printf("Out of RAM\n");
                exit_program(1);
        }
        for (span = 0; span <= maxspan; span++)
        {
                prev = 0;
                for (i = j = 0; occ[i] != 0; i += OCCURRENCES)
                        if (occ[i + 1] - occ[i] >= span && occ[i + 1] - span != prev)
                                list[j++] = prev = occ[i + 1] - span;
                list[j] = 0;
                push(list, VERSES);
                if (maxspan == 0)
                        num_verses = (long int) j;
        }
        free(list);
        free(occ);
        for (span = 1; span <= maxspan; span++)
                or_oper();
        return (pop());
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
        struct TEMPFILE *rfile, rcopy;
        struct TEMPFILE *kfile, kcopy;
        long int remove, keep;

        /* (the lists first: making one into a verse list opens files, and
           tmpopen() gives the same structure each time) */
        pop_verses(&rcopy);
        rfile = &rcopy;
        pop_verses(&kcopy);
        kfile = &kcopy;
        result = tmpopen();

        rewind(rfile->fname);
        remove = fgetl(rfile->fname);

        rewind(kfile->fname);
        keep = fgetl(kfile->fname);
        
        num_verses = 0;
        while (remove != 0 && keep != 0)
        {
                if (keep > remove)
                {
                        remove = fgetl(rfile->fname);
                        continue;
                }
                if (keep < remove)
                {
                        fputl(keep, result->fname);
                        ++num_verses;
                        keep = fgetl(kfile->fname);
                        continue;
                }
                keep = fgetl(kfile->fname);
                remove = fgetl(rfile->fname);
        }
        fputl(keep, result->fname);
        while(keep != 0)
        {
                keep = fgetl(kfile->fname);
                fputl(keep, result->fname);
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
        long int *result;
        long int *remove, *r, *keep, *k;
        int i = 0;

  if ((result = (long int *) malloc(ALLVERSE * sizeof(long int))) == NULL)
        {
             printf("Out of RAM\n");
             exit_program(1);
        }
        r = remove = pop_verses();
        k = keep = pop_verses();
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
        push(result, VERSES);
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
        struct TEMPFILE *sec_file, sec_copy;
        struct TEMPFILE *fir_file, fir_copy;
        long int second, first;

        pop_verses(&sec_copy);  /* (the lists first, as in not_oper()) */
        sec_file = &sec_copy;
        pop_verses(&fir_copy);
        fir_file = &fir_copy;
        result = tmpopen();

        rewind(sec_file->fname);
        second = fgetl(sec_file->fname);

        rewind(fir_file->fname);
        first = fgetl(fir_file->fname);
        
        num_verses = 0;
        while (second != 0 && first != 0)
        {
                if (first > second)
                {
                        fputl(second, result->fname);
                        ++num_verses;
                        second = fgetl(sec_file->fname);
                        continue;
                }
                if (first < second)
                {
                        fputl(first, result->fname);
                        ++num_verses;
                        first = fgetl(fir_file->fname);
                        continue;
                }
                fputl(first, result->fname);
                ++num_verses;
                first = fgetl(fir_file->fname);
                second = fgetl(sec_file->fname);
        }
        while(first != 0)
        {
                fputl(first, result->fname);
                ++num_verses;
                first = fgetl(fir_file->fname);
        }
        fputl(second, result->fname);
        while(second != 0)
        {
                second = fgetl(sec_file->fname);
                fputl(second, result->fname);
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
        long int *result;
        long int *second, *s, *first, *f;
        int i = 0;

  if ((result = (long int *) malloc(ALLVERSE * sizeof(long int))) == NULL)
        {
             printf("Out of RAM\n");
             exit_program(1);
        }
        s = second = pop_verses();
        f = first = pop_verses();
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
        push(result, VERSES);
        free (result);
}
#endif

static int compare(long int first, long int second, int prox)
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
    struct TEMPFILE sec_copy, fir_copy;
    struct TEMPFILE *swap_file;
    long int second, first;
    char second_time = FALSE;
    int diff;

/*
** The memory location which pop() returns is overwritten by the call to
** push() near the end of this module: the lists are copied to local
** structs, and pointed to. (And first: making one into a verse list
** opens files, and tmpopen() gives the same structure each time.)
*/
    pop_verses(&sec_copy);
    sec_file = &sec_copy;
    pop_verses(&fir_copy);
    fir_file = &fir_copy;
    result = tmpopen();

    rewind(sec_file->fname);
    second = fgetl(sec_file->fname);
    rewind(fir_file->fname);
    first = fgetl(fir_file->fname);
/*
** A special case where the second of two words being "anded" together
** was not found in the index.
*/
    if ((second == 0) || (first == 0))
    {
        fputl(0L, result->fname);
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
                second = fgetl(sec_file->fname);
                rewind(fir_file->fname);
                first = fgetl(fir_file->fname);
        }
        while (first != 0)
        {
                if ((diff = compare (first, second, prox)) == 0)
                        fputl(first, result->fname);
                if (diff < 0)
                {
                    for(;;)
                    {
                            if (ftell(sec_file->fname) > 4) /* past the first */
                            {
                                /* back to the number before the one just
                                   read: two numbers of 4 bytes */
                                if (fseek(sec_file->fname, -8L, 1) != 0)
                                {
                                        printf("fseek failed!!\n");
                                        exit_program(1);
                                }
                                second = fgetl(sec_file->fname);
                            }
                            else
                                break;
                            if ((diff = compare (first, second, prox)) == 0)
                            {
                                    fputl(first, result->fname);
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
                            second = fgetl(sec_file->fname);
                            if (second == 0) /* past last element */
                            {
                                /* back to the last number (4 bytes each) */
                                if (fseek(sec_file->fname, -8L, 1) != 0)
                                {
                                        printf("fseek failed!!\n");
                                        exit_program(1);
                                }
                                second = fgetl(sec_file->fname);
                                break;
                            }
                            if ((diff = compare (first, second, prox)) == 0)
                            {
                                    fputl(first, result->fname);
                                    break;
                            }
                            if (diff < 0)
                                    break;
                    }
                }
                first = fgetl(fir_file->fname);
        }

        fputl(0L, result->fname); /* terminate this list */
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
    long int *result;
    long int *second, *s, *first, *f, *swap;
    int i = 0;
    char second_time = FALSE;
    int diff;

    if ((result = (long int *) malloc(ALLVERSE * sizeof(long int))) == NULL)
    {
         printf("Out of RAM\n");
         exit_program(1);
    }
    s = second = pop_verses();
    f = first = pop_verses();
    if ((*second == 0) || (*first == 0))
    {
        result[0] = 0;
        free (s);
        free (f);
        push(result, VERSES);
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
        push(result, VERSES);
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
    push(result, VERSES);
    free (result);
/*
** The results so far consist of two lists of numbers. Since the AND
** operation by definition has one list of results we combine the two
** lists with an OR operation.
*/
    or_oper();
}
#endif

/*
** Purpose: whether word B comes right after word A in a phrase (see
**          readme.architecture): in the same verse, B's full position is
**          A's plus 1, or neither is in brackets and B's plain position is
**          A's plus 1; or B is in the next verse (by sequential verse
**          number: the next chapter or book too), A is its verse's last
**          word (LAST_POSITION on its full position) and B its first, or
**          neither is in brackets, A is the last such word, and B the first.
**          a_end is the verse of A; b_svn the verse of B.
*/
static int adjacent(long int a_end, long int a_full, long int a_plain,
                    long int b_svn, long int b_full, long int b_plain)
{
        long int af = a_full & ~LAST_POSITION, ap = a_plain & ~LAST_POSITION;
        long int bf = b_full & ~LAST_POSITION, bp = b_plain & ~LAST_POSITION;

        if (a_end == b_svn)
                return (bf == af + 1 || (ap != 0 && bp != 0 && bp == ap + 1));
        if (a_end + 1 == b_svn)
                return (((a_full & LAST_POSITION) && bf == 1) ||
                        (ap != 0 && bp != 0 && (a_plain & LAST_POSITION) && bp == 1));
        return (FALSE);
}

/*
** Purpose: the "." operator. Pops two occurrence lists, B (a word) and A
**          (a word, or a phrase), and pushes the occurrence list of each B
**          right after an A: with A's start, and B's verse and positions,
**          so that it can be followed by another word. Both lists are in
**          order of their last verse and position, so the A's in B's verse
**          or the one before are found going through A once, alongside B.
** Example: given that A ("in") = 1:1/1, ... and B ("the") = 1:2/2, ...
**                then A B . includes 1:1 (1 to 1, "the" at 2)
*/
#ifdef RAMDISK
static void phrase_oper(void)
{
        struct TEMPFILE a, b, *result;
        long int ae[OCCURRENCES], be[OCCURRENCES], window = 0;
        long int last_start = 0, last_end = 0, last_full = 0;
        long int entry = OCCURRENCES * 4L; /* bytes per entry */

        b = *pop();
        a = *pop();
        result = tmpopen();
        result->width = OCCURRENCES;
        rewind(b.fname);
        num_verses = 0;
        while (read_entry(b.fname, be))
        {
                /* past the A's ending before the verse before B's */
                for (;;)
                {
                        if (fseek(a.fname, window, 0) != 0)
                        {
                                printf("fseek failed!!\n");
                                exit_program(1);
                        }
                        if (!read_entry(a.fname, ae) || ae[1] >= be[0] - 1)
                                break;
                        window += entry;
                }
                if (fseek(a.fname, window, 0) != 0)
                {
                        printf("fseek failed!!\n");
                        exit_program(1);
                }
                while (read_entry(a.fname, ae) && ae[1] <= be[0])
                        if (adjacent(ae[1], ae[2], ae[3], be[0], be[2], be[3]) &&
                            !(ae[0] == last_start && be[1] == last_end &&
                              be[2] == last_full))
                        {
                                fputl(ae[0], result->fname);
                                fputl(be[1], result->fname);
                                fputl(be[2], result->fname);
                                fputl(be[3], result->fname);
                                last_start = ae[0];
                                last_end = be[1];
                                last_full = be[2];
                                ++num_verses;
                        }
        }
        fputl(0L, result->fname);
        mfclose(b.fname, b.sname);
        unlink(b.sname);
        mfclose(a.fname, a.sname);
        unlink(a.sname);
        push(result);
}
#else
static void phrase_oper(void)
{
        long int *a, *b, *result;
        size_t ai = 0, bi, k, n = 0, room;

        b = pop();
        a = pop();
        for (room = 0; b[room] != 0; room += OCCURRENCES)
                ;
        room += 1;
        if ((result = (long int *) malloc(room * sizeof(long int))) == NULL)
        {
                printf("Out of RAM\n");
                exit_program(1);
        }
        num_verses = 0;
        for (bi = 0; b[bi] != 0; bi += OCCURRENCES)
        {
                /* past the A's ending before the verse before B's */
                while (a[ai] != 0 && a[ai + 1] < b[bi] - 1)
                        ai += OCCURRENCES;
                for (k = ai; a[k] != 0 && a[k + 1] <= b[bi]; k += OCCURRENCES)
                        if (adjacent(a[k + 1], a[k + 2], a[k + 3], b[bi], b[bi + 2], b[bi + 3]) &&
                            !(n > 0 && result[n - OCCURRENCES] == a[k] &&
                              result[n - 3] == b[bi + 1] && result[n - 2] == b[bi + 2]))
                        {
                                if (n + OCCURRENCES >= room)    /* (more A's than B's) */
                                {
                                        room *= 2;
                                        if ((result = (long int *) realloc(result, room * sizeof(long int))) == NULL)
                                        {
                                                printf("Out of RAM\n");
                                                exit_program(1);
                                        }
                                }
                                result[n++] = a[k];
                                result[n++] = b[bi + 1];
                                result[n++] = b[bi + 2];
                                result[n++] = b[bi + 3];
                                ++num_verses;
                        }
        }
        result[n] = 0;
        free(a);
        free(b);
        push(result, OCCURRENCES);
        free(result);
}
#endif

/*
** Purpose: the operators +N, -N and ~N: verses exactly N apart. Pops two
**          verse lists, second and first, and pushes the verses of each
**          that are exactly N verses from one of the other: with +, the
**          second's verse N after the first's; with -, N before; with ~,
**          either. (N alone, and_oper(), is within N, either way.)
**          Made of lists of the verses x of one list such that x + shift
**          is in the other (shift_match()), combined with OR.
** Example: given that first = 1, 5, 9, 0
**                 and second = 2, 5, 8, 0
**                then first second +1 is 1, 2, 0
**                 and first second ~1 is 1, 2, 8, 9, 0
*/
static void exact_oper(long int n, int mode)
{
        int lists = 0;
#ifdef RAMDISK
        struct TEMPFILE first, second;

        pop_verses(&second);
        pop_verses(&first);
        if (mode == '+' || mode == '~')
        {
                shift_match(&first, &second, n);
                shift_match(&second, &first, -n);
                lists += 2;
        }
        if (mode == '-' || mode == '~')
        {
                shift_match(&first, &second, -n);
                shift_match(&second, &first, n);
                lists += 2;
        }
        mfclose(second.fname, second.sname);
        unlink(second.sname);
        mfclose(first.fname, first.sname);
        unlink(first.sname);
#else
        long int *first, *second;

        second = pop_verses();
        first = pop_verses();
        if (mode == '+' || mode == '~')
        {
                shift_match(first, second, n);
                shift_match(second, first, -n);
                lists += 2;
        }
        if (mode == '-' || mode == '~')
        {
                shift_match(first, second, -n);
                shift_match(second, first, n);
                lists += 2;
        }
        free(second);
        free(first);
#endif
        while (--lists > 0)
                or_oper();
}

/*
** Purpose: push the list of the verses x in list x such that x + shift is
**          in list y. Both are in order, so they are gone through once,
**          together.
*/
#ifdef RAMDISK
static void shift_match(struct TEMPFILE *x, struct TEMPFILE *y, long int shift)
{
        struct TEMPFILE *result;
        long int xv, yv;

        result = tmpopen();
        rewind(x->fname);
        rewind(y->fname);
        yv = fgetl(y->fname);
        while ((xv = fgetl(x->fname)) != 0)
        {
                while (yv != 0 && yv < xv + shift)
                        yv = fgetl(y->fname);
                if (yv == 0)
                        break;
                if (yv == xv + shift)
                        fputl(xv, result->fname);
        }
        fputl(0L, result->fname);
        push(result);
}
#else
static void shift_match(long int *x, long int *y, long int shift)
{
        long int *result;
        size_t i = 0;

        if ((result = (long int *) malloc(ALLVERSE * sizeof(long int))) == NULL)
        {
                printf("Out of RAM\n");
                exit_program(1);
        }
        for (; *x != 0; x++)
        {
                while (*y != 0 && *y < *x + shift)
                        y++;
                if (*y == 0)
                        break;
                if (*y == *x + shift)
                        result[i++] = *x;
        }
        result[i] = 0;
        push(result, VERSES);
        free(result);
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
** Groups of books (EBaC 2.2): "=nt" in a search is every verse of a group
** of books, defined in the groups files, and "=rom..phm" every verse from
** one book to another. See "Groups of books" in readme.architecture.
**
** The files are read when a translation is chosen (load_groups()): each
** line, a name and its members, is kept in groups_text, and found by its
** name (group_name[], group_members[]). A set of books is a char for each
** book, TRUE if it is in the set.
*/
#ifndef COCO
#       define GROUPS_SIZE 8192 /* bytes of the groups files kept */
#       define MAXGROUPS 64
#       define MAXSETBOOKS 256  /* the most books a set can hold */
#else
#       define GROUPS_SIZE 1024
#       define MAXGROUPS 24
#       define MAXSETBOOKS 67
#endif
#define MAXGROUPLINE 256
#define MAXGROUPDEPTH 8         /* groups within groups */
#define NO_BOOK (-1)            /* book_index(): no such book */
#define BAD_BOOK (-2)           /* book_index(): an error, reported */

static char groups_text[GROUPS_SIZE];
static int groups_used = 0, num_groups = 0;
static char *group_name[MAXGROUPS], *group_members[MAXGROUPS];

static char *keep_text P((char *text));
static void read_groups P((FILE *file));
static int find_group P((char *typed, int report));
static int book_index P((char *typed, char *group, int strict, int report));
static int members_set P((char *members, char *set, char *group, int depth, int report));
static int member_set P((char *member, char *set, char *group, int depth, int report));

/* Purpose: keep a copy of text in groups_text. Returns: it, or NULL */
static char *keep_text(char *text)
{
        char *copy;

        if (groups_used + (int) strlen(text) + 1 > GROUPS_SIZE)
                return (NULL);
        copy = &groups_text[groups_used];
        strcpy(copy, text);
        groups_used += strlen(text) + 1;
        return (copy);
}

/*
** Purpose: read a groups file (if there is one: file may be NULL). Each
**          group replaces any of the same name read before it. A name is
**          kept in lower case.
*/
static void read_groups(FILE *file)
{
        char line[MAXGROUPLINE], *p, *name, *members;
        int i;

        if (file == NULL)
                return;
        while (fgets(line, sizeof(line), file) != NULL)
        {
                for (p = line; *p != '\0' && *p != '#' && *p != '\n' &&
                               *p != '\r'; p++)
                        ;
                *p = '\0';              /* (a comment, or the line's end) */
                for (p = line; *p == ' ' || *p == '\t'; p++)
                        ;
                if (*p == '\0')
                        continue;       /* a blank line */
                name = p;
                while (*p != '\0' && *p != ' ' && *p != '\t')
                {
                        *p = tolower(UCHAR(*p));
                        p++;
                }
                if (*p != '\0')
                        *p++ = '\0';
                while (*p == ' ' || *p == '\t')
                        p++;
                members = p;
                for (; *p != '\0'; p++)
                        if (*p == '\t')
                                *p = ' ';       /* (members are split at spaces) */
                for (i = 0; i < num_groups; i++)
                        if (strcmp(group_name[i], name) == 0)
                                break;
                if (i == num_groups)
                {
                        if (num_groups == MAXGROUPS ||
                            (group_name[i] = keep_text(name)) == NULL)
                                break;  /* (no room for more) */
                        num_groups++;
                }
                if ((group_members[i] = keep_text(members)) == NULL)
                {
                        num_groups = i; /* (no room: this group goes) */
                        break;
                }
        }
        fclose(file);
}

/*
** Purpose: read the groups for the translation now chosen (etla): from
**          "groups" and "groups.NAME" in the EBaC folder, then "groups" in
**          the translation's folder, each replacing the groups of the same
**          name before it.
*/
void load_groups(void)
{
        char name[80];

        groups_used = num_groups = 0;
        read_groups(root_fopen("groups"));
        if (strlen(etla) < sizeof(name) - 8)
        {
                sprintf(name, "groups.%s", etla);
                read_groups(root_fopen(name));
        }
        read_groups(fopen("groups", "r"));
}

/*
** Purpose: find the group a name, or the start of one, is: one group whose
**          name starts with it, or whose whole name it is.
** Returns: the group's index; or -1, if there is none or more than one
**          (and if report, says so).
*/
static int find_group(char *typed, int report)
{
        char lower[MAXGROUPLINE];
        int i, len, count = 0, found = -1, shown = 0;

        for (len = 0; typed[len] != '\0' && len < MAXGROUPLINE - 1; len++)
                lower[len] = tolower(UCHAR(typed[len]));
        lower[len] = '\0';
        for (i = 0; i < num_groups; i++)
                if (strcmp(group_name[i], lower) == 0)
                        return (i);     /* its whole name */
        if (len > 0)
                for (i = 0; i < num_groups; i++)
                        if (strncmp(group_name[i], lower, len) == 0)
                        {
                                count++;
                                found = i;
                        }
        if (count == 1)
                return (found);
        if (report)
        {
                if (len == 0)
                        printf("\n\"=\" needs the name of a group of books after it, as in:  =nt\n");
                else if (count == 0)
                        printf("\nThere is no group of books called \"%s\".\n", typed);
                else
                {
                        printf("\n=%s matches ", typed);
                        for (i = 0; i < num_groups; i++)
                                if (strncmp(group_name[i], lower, len) == 0)
                                        printf("%s=%s", ++shown == 1 ? "" :
                                               shown == count ? " and " : ", ",
                                               group_name[i]);
                        printf(". Type more of its name.\n");
                }
        }
        return (-1);
}

/*
** Purpose: the book a name, or the start of one, is (as after "@"), for a
**          group (named, if group isn't NULL) or a range.
** Returns: the book's index (0 for the first); NO_BOOK if the translation
**          has none of that name (and if strict, an error: BAD_BOOK); or
**          BAD_BOOK if it matches more than one. If report, an error is
**          explained.
*/
static int book_index(char *typed, char *group, int strict, int report)
{
        char lower[MAXGROUPLINE];
        int n;

        strncpy(lower, typed, MAXGROUPLINE - 1);
        lower[MAXGROUPLINE - 1] = '\0';
        n = (lower[0] == '\0') ? -1 : match_it(lower, FALSE);
        if (n > 0)
                return (n - 1);
        if (n == -1 && !strict)
                return (NO_BOOK);
        if (report)
        {
                printf("\n");
                if (group != NULL)
                        printf("In the group =%s, ", group);
                else
                        printf("In the range, ");
                if (n == -1)
                        printf("there is no book \"%s\".\n", typed);
                else
                {
                        printf("%s matches ", typed);
                        print_book_matches(typed);
                        printf(".\n");
                }
        }
        return (BAD_BOOK);
}

/*
** Purpose: the books of one member of a group (or of a search's "=a..b"):
**          a book, a range a..b, every book (*), or another group (=name),
**          into set (which it clears first).
** Returns: FALSE after an error (explained, if report).
*/
static int member_set(char *member, char *set, char *group, int depth, int report)
{
        char first[MAXGROUPLINE], *dots;
        int i, a, b, strict = (group == NULL);

        for (i = 0; i < numbooks; i++)
                set[i] = FALSE;
        if (strcmp(member, "*") == 0)
        {
                for (i = 0; i < numbooks; i++)
                        set[i] = TRUE;
                return (TRUE);
        }
        if (member[0] == '=')
        {
                if (depth >= MAXGROUPDEPTH)
                {
                        if (report)
                                printf("\nThe group =%s refers to itself (through =%s).\n",
                                       group, &member[1]);
                        return (FALSE);
                }
                if ((i = find_group(&member[1], report)) < 0)
                        return (FALSE);
                return (members_set(group_members[i], set, group_name[i],
                                    depth + 1, report));
        }
        if ((dots = strstr(member, "..")) != NULL)
        {
                strncpy(first, member, MAXGROUPLINE - 1);
                first[MAXGROUPLINE - 1] = '\0';
                first[dots - member] = '\0';
                if ((a = book_index(first, group, strict, report)) == BAD_BOOK ||
                    (b = book_index(dots + 2, group, strict, report)) == BAD_BOOK)
                        return (FALSE);
                if (a == NO_BOOK || b == NO_BOOK)
                        return (TRUE);  /* (no such book here: none) */
                if (a > b)
                {
                        if (report)
                        {
                                printf("\n");
                                if (group != NULL)
                                        printf("In the group =%s, ", group);
                                printf("%s comes after %s: a range goes from the earlier book.\n",
                                       first, dots + 2);
                        }
                        return (FALSE);
                }
                for (i = a; i <= b; i++)
                        set[i] = TRUE;
                return (TRUE);
        }
        if ((a = book_index(member, group, strict, report)) == BAD_BOOK)
                return (FALSE);
        if (a != NO_BOOK)
                set[a] = TRUE;
        return (TRUE);
}

/*
** Purpose: the books of a group's members (members, as in the groups file),
**          taken in order from no books: each adds its books, or with "-"
**          before it takes them away.
** Returns: FALSE after an error (explained, if report).
*/
static int members_set(char *members, char *set, char *group, int depth, int report)
{
        char copy[MAXGROUPLINE], *pos, *member;
        char *add;
        int i, remove, ok = TRUE;

        if ((add = (char *) malloc(MAXSETBOOKS)) == NULL)
        {
                printf("Out of RAM\n");
                exit_program(1);
        }
        for (i = 0; i < numbooks; i++)
                set[i] = FALSE;
        strncpy(copy, members, MAXGROUPLINE - 1);
        copy[MAXGROUPLINE - 1] = '\0';
        pos = copy;
        for (member = next_term(&pos); member != NULL; member = next_term(&pos))
        {
                remove = (member[0] == '-');
                if (!member_set(remove ? &member[1] : member, add, group,
                                depth, report))
                {
                        ok = FALSE;
                        break;
                }
                for (i = 0; i < numbooks; i++)
                        if (add[i])
                                set[i] = !remove;
        }
        free(add);
        return (ok);
}

/*
** Purpose: the books a search's "=" term means (spec, after the "="): a
**          group, or a range a..b, into set.
** Returns: FALSE after an error (explained, if report).
*/
static int group_set(char *spec, char *set, int report)
{
        int g;

        if (numbooks > MAXSETBOOKS)
        {
                if (report)
                        printf("\nThis translation has too many books for groups (%d at most).\n",
                               MAXSETBOOKS);
                return (FALSE);
        }
        if (strstr(spec, "..") != NULL)
                return (member_set(spec, set, (char *) NULL, 0, report));
        if ((g = find_group(spec, report)) < 0)
                return (FALSE);
        return (members_set(group_members[g], set, group_name[g], 1, report));
}

/*
** store the list of every verse of a set of books (set[i] TRUE for each
** book in it, as for @book a set of one) into an element on the stack
*/
static void store_books(char *set)
{
#ifdef RAMDISK
        struct TEMPFILE *book_file;
#else
        struct stackptr *stack;
        long int *cur_node;
#endif
        struct bcv_type bcv;
        long int first_verse[MAXSETBOOKS], last_verse[MAXSETBOOKS], v;
        int i;

        num_verses = 0;         /* a global variable */
        for (i = 0; i < numbooks && i < MAXSETBOOKS; i++)
        {
                if (!set[i])
                        continue;
                bcv.book_number = i + 1;
                bcv.chapter_number = 1;
                bcv.verse_number = 1;
                first_verse[i] = bcv_search(&bcv);
                bcv.book_number = i + 2;
                bcv.chapter_number = 1;
                bcv.verse_number = 0;
                last_verse[i] = bcv_search(&bcv);
                num_verses += last_verse[i] - first_verse[i] + 1;
        }
#ifdef RAMDISK
        book_file = tmpopen();
        for (i = 0; i < numbooks && i < MAXSETBOOKS; i++)
                if (set[i])
                        for (v = first_verse[i]; v <= last_verse[i]; v++)
                                fputl(v, book_file->fname);
        fputl(0L, book_file->fname); /* terminate the list */
        push(book_file);
#else
        if ((stack = (struct stackptr *) malloc (sizeof (struct stackptr))) ==
                                                                        NULL)
        {
             printf("Out of RAM\n");
             exit_program(1);
        }
        if ((cur_node = stack->node = (long int *) malloc(sizeof(long int) *
                                (num_verses + 1))) == NULL)
        {
             printf("Out of RAM\n");
             exit_program(1);
        }
        for (i = 0; i < numbooks && i < MAXSETBOOKS; i++)
                if (set[i])
                        for (v = first_verse[i]; v <= last_verse[i]; v++)
                                *cur_node++ = v;
        *cur_node = 0; /* terminate the list */
        stack->width = VERSES;
        stack->next = top;
        top = stack;
#endif
}

/*
** Purpose: TRUE if arg is one of the operators +N, -N or ~N (a +, - or ~
**          and digits): verses exactly N apart (see exact_oper()).
*/
static int is_exact(char *arg)
{
        int i;

        if ((arg[0] != '+' && arg[0] != '-' && arg[0] != '~') || arg[1] == '\0')
                return (FALSE);
        for (i = 1; arg[i] != '\0'; i++)
                if (!isdigit(UCHAR(arg[i])))
                        return (FALSE);
        return (TRUE);
}

/* Purpose: TRUE if arg is one of the search operators: - ! | , . a number,
            or +N, -N or ~N */
static int is_operator(char *arg)
{
        return (strcmp (arg, "-") == 0 || strcmp (arg, "!") == 0 ||
                strcmp (arg, "|") == 0 || strcmp (arg, ",") == 0 ||
                strcmp (arg, ".") == 0 || is_exact(arg) ||
                isdigit (UCHAR(*arg)));
}

/*
** Set by valid_search() for process_words(): whether the search has
** phrases (so its words are looked up with their positions), and whether
** it is words alone, with no operators or books: a phrase, as if "." came
** after every word but the first.
*/
static int phrase_search = FALSE;
static int plain_phrase = FALSE;

/*
** Purpose: check a search line before evaluating it. A postfix expression
**          needs exactly one more word (or @book) than it has operators,
**          and every operator needs two results before it. Without this
**          check a mistake emptied the stack and ended the program, or
**          left stale results behind to corrupt the next search. The "."
**          operator joins a word, or a phrase, to a word after it: so the
**          kind of each result is followed too (kind[]: 'w' a word, 'p' a
**          phrase, 's' any other list of verses). Words alone, two or more
**          with no operators and no books, are a phrase.
** Returns: TRUE if the line can be evaluated. Otherwise it explains the
**          problem, waits for a key, and returns FALSE.
*/
static int valid_search(char *line)
{
        static char copy[MAXSEARCHLEN], name[MAXSEARCHLEN];
        static char set[MAXSETBOOKS];
        char *arg, *pos;
        char kind[MAXHIGHWORDS + 1];
        int depth = 0;    /* number of results the stack would hold */
        int operands = 0; /* number of words and books */
        int operators = 0, books = 0;

        phrase_search = plain_phrase = FALSE;
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
                        operators++;
                        if (strcmp(arg, ".") == 0)
                        {
                                if (kind[depth - 1] != 'w')
                                {
                                        printf("\nThe operator \".\" needs a word just before it: the next word of\n");
                                        printf("the phrase. (For example:  in the . beginning .)\n");
                                        depth = -1;
                                        break;
                                }
                                if (kind[depth - 2] == 's')
                                {
                                        printf("\nThe operator \".\" joins words and phrases: not a book, or what\n");
                                        printf("another operator found.\n");
                                        depth = -1;
                                        break;
                                }
                                phrase_search = TRUE;
                                kind[depth - 2] = 'p';
                        }
                        else
                                kind[depth - 2] = 's';
                        depth--;
                        continue;
                }
                if (*arg == '@')
                {
                        strncpy(name, &arg[1], MAXSEARCHLEN - 1);
                        name[MAXSEARCHLEN - 1] = '\0';
                        switch (match_it(name, FALSE))
                        {
                        case -1:
                                printf("\nNo such book: %s\n", &arg[1]);
                                depth = -1;
                                break;
                        case 0:
                                printf("\n@%s matches ", &arg[1]);
                                print_book_matches(&arg[1]);
                                printf(". Type more of its name.\n");
                                depth = -1;
                                break;
                        }
                        if (depth < 0)
                                break;
                        books++;
                }
                if (*arg == '=')        /* a group of books, or a range */
                {
                        if (!group_set(&arg[1], set, TRUE))
                        {
                                depth = -1;
                                break;
                        }
                        books++;
                }
                if (++operands >= MAXHIGHWORDS)
                {
                        printf("\nToo many words; the limit is %d.\n", MAXHIGHWORDS - 1);
                        depth = -1;
                        break;
                }
                kind[depth++] = (*arg == '@' || *arg == '=') ? 's' : 'w';
        }
        if (arg == NULL) /* reached the end of the line without an error */
        {
                if (depth == 0) /* blank line: nothing to search for */
                        return FALSE;
                if (depth == 1)
                        return TRUE;
                if (operators == 0 && books == 0)       /* words alone: a phrase */
                {
                        phrase_search = plain_phrase = TRUE;
                        return TRUE;
                }
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
        static struct TEMPFILE result_list;
#else
long int *process_words(void)
{
        long int *verse_list;
#endif
        char *arg;  /* Points to argument within search line */
        int i;
        static char set[MAXSETBOOKS];   /* the books of @book or =group */
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
                else if (strcmp (arg, ".") == 0)
                        phrase_oper();
                else if (is_exact(arg))         /* +N, -N or ~N */
                        exact_oper(atol(&arg[1]), arg[0]);
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
                                for (i = 0; i < numbooks && i < MAXSETBOOKS; i++)
                                        set[i] = (i == book_number - 1);
                                store_books(set);
                                break;
                        }       
                }
                else if (*arg == '=')   /* a group of books, or a range */
                {
                        group_set(&arg[1], set, FALSE); /* (valid_search() checked it) */
                        add_rev("xyzzy");
                        store_books(set);
                }
                else if (strlen (arg) > 0)
                {
                        add_rev(arg);
                        if (terms == 1)
                        {
                                strncpy(count_word, arg, MAXCOUNTWORD - 1);
                                count_word[MAXCOUNTWORD - 1] = '\0';
                        }
#ifdef RAMDISK
                        verse_list = find_word(arg, phrase_search);
                        push(verse_list);
#else
                        verse_list = find_word(arg, phrase_search);
                        push(verse_list, phrase_search ? OCCURRENCES : VERSES);
#endif
                        /* words alone: each after the first joins the
                           phrase, as if "." came after it */
                        if (plain_phrase && terms > 1)
                                phrase_oper();
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
#ifdef RAMDISK
        /* (a static copy: the stack's place is used again) */
        pop_verses(&result_list);
        return(&result_list);
#else
        verse_list = pop_verses();
        return((long int *) verse_list);
#endif
}

