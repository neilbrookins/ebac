/* 
   EBaC (Electronic Bible and Concordance)
   written by Neil G. Brookins
   http://ebac.sourceforge.net/
*/
#include "bible.h"
#include "screen.h"

#ifdef UNIXV
#       include <sys/utsname.h>  /* uname(), for the Information screen */
#       include <errno.h>
#       include <fcntl.h>       /* fcntl(), for run_importer() */
#endif

/*
** The version. EBAC_RELEASE is set here; the makefile passes the commit
** it was built from and its date, such as "715d8dd, 2026-10-24" (from git,
** or from version.txt in a downloaded archive), as EBAC_COMMIT, and a
** package such as the Home Assistant app can name itself in EBAC_PACKAGE.
*/
#define EBAC_RELEASE "2.3.4"
char ebac_release[] = EBAC_RELEASE;   /* (for ebac-api's info too) */
#ifndef EBAC_COMMIT
#       define EBAC_COMMIT ""
#endif
#ifndef EBAC_PACKAGE
#       define EBAC_PACKAGE ""
#endif

/* how this program was started, for the Information screen */
static char program_path[512] = "";

#ifdef UNIXV
/*
** The translation importer, add-translation.sh in BIBLEROOT (a shell
** script), if it is there; otherwise "". The main menu offers A)dd a
** translation only when it is set.
*/
static char importer[600] = "";
/* and the importer of Project Gutenberg's texts, add-gutenberg.sh, for
   P)roject Gutenberg, likewise */
static char gutenberg[600] = "";
#endif

#define WIDTH (s_cols - 1)
#define MAXINDENT 26
char spaces[]="                          ";  /* MAXINDENT spaces */

/* this is for both the searching word length and the longest word
** in the text itself. Used in the routine that prints word to screen
** in inverse video. */
#define MAXWORDLEN 40

/*
** ARROWKEYS: the keyboard has arrow and page keys: the MM/1, and UNIX
** terminals (read through ncurses). KEYHELP(text) is then text, to add to
** a help line with %s, or else "". (Only #ifdef is used, and no string
** constants placed side by side, so that K&R compilers take this too.)
*/
#ifdef USE_NCURSES
#       define ARROWKEYS
#endif
#ifdef OSK
#       define ARROWKEYS
#endif
#ifdef ARROWKEYS
#       define KEYHELP(text) text
#else
#       define KEYHELP(text) ""
#endif

/*
** The keys that pick an entry from a list of search results.
** Entries 1 to 9 are labelled with digits, and can also be picked with the
** letters under the digits on a keyboard (q for 1 ... o for 9), so a phone's
** on-screen keyboard needn't be switched to numbers and back.
** Entries 10 and up are labelled with letters that mean nothing else on that
** screen: never q to o (entries 1 to 9), n or p (next and previous screen)
** or x (exit).
*/
static char digit_keys[] = "qwertyuio";       /* entries 1 to 9 */
static char more_tags[]  = "abcdfghjklmsvz";  /* entries 10, 11, ... */
#define MAXTAGS (9 + (int) sizeof(more_tags) - 1)

static int last_len = 0;
struct bcv_type bcv;

/* functions used only in this file */
static int main_menu P((void));
static void remove_char P((char *strg));
static int get_number P((void));
static struct bcv_type *bcv_menu P((void));
static int count_lines P((int tag, char *bookname, char *strg, int indent,
                       int top_of_screen));
static void dis_one_line P((char *strg));
static void dis_verse P((char *strg, int indent, int firstTime));
static void display_verse P((int tag, struct bcv_array_type *bcv_arrays,
                          int verIndent, int top_of_screen));
static long int calculate_fv P((long int last_verse));
static long int scrolled_top P((long int last_shown));
static void rev_printf P((char *string));
static void print_visual P((char *s, char *hi));
static int check_limits P((long int cur_verse));
static void view_in_context P((long int cur_verse));
#ifdef RAMDISK
static long int vlist P((struct TEMPFILE *tf, long int rnum));
static void view_match_list P((struct TEMPFILE *verse_list));
#else
static void view_match_list P((long int *verse_list));
#endif
static void prog_info P((void));
static void prog_diagnostics P((void));
static void set_program_path P((char *argv0));
#ifdef UNIXV
static int on_path P((char *name));
static void find_importer P((char *found, int size, char *name));
static void run_importer P((char *script, char *what));
#endif
static int tag_label P((int tag));
static int key_to_tag P((int key));
static int screen_key P((void));

static int main_menu(void)
{
        cls();
        printf("\n\n");
        printf("Welcome to EBaC, Electronic Bible and Concordance\n");
        printf("Selected translation: %s", etla);
        /* just made the default (d and a number, on C)hange: said once */
        if (default_translation_set > 0)
                printf(", now the default");
        else if (default_translation_set < 0)
                printf(" (couldn't make it the default: trans)");
        default_translation_set = 0;
        printf("\n");
        underline();
        printf("\nMain Menu\n");
        underend();
        /* a blank line between the choices, so each is easy to tap on a
           touch screen (see gestures.js in the Home Assistant app) */
        printf("C)hange to a different translation\n\n");
#ifdef UNIXV
        if (importer[0] != '\0')
                printf("A)dd or remove a translation\n\n");
        if (gutenberg[0] != '\0')
                printf("P)roject Gutenberg: add a text\n\n");
#endif
        printf("V)iew by:   Bookname Chapter:Verse\n\n");
        printf("S)earch by: Keyword(s)\n\n");
        printf("I)nformation about EBaC\n\n");
        printf("E)xit Program\n\n");
#ifdef UNIXV
        if (importer[0] != '\0' && gutenberg[0] != '\0')
                printf("Please press C, A, P, V, S, I, or E now.\n");
        else if (importer[0] != '\0')
                printf("Please press C, A, V, S, I, or E now.\n");
        else if (gutenberg[0] != '\0')
                printf("Please press C, P, V, S, I, or E now.\n");
        else
#endif
        printf("Please press C, V, S, I, or E now.\n");
/*
** I wish I understood why this is needed, but with the \r, the display
** seems to behave in the same way that OS-9 systems behave without the \r.
*/
#ifdef VMS
        putchar('\r');
#endif
        scr_mode("menu");
        return(screen_key());   /* KEY_REDRAW if the window is resized */
}

/*
** Purpose: the value of the Roman numeral at the start of a book's name, as
**          the 2 of "IIKings"; 0 if it has none. The numeral must be
**          followed by a capital letter, so the "I" of "Isaiah" is not one.
**          *rest is set to the rest of the name ("Kings").
*/
static int roman_start(char *name, char **rest)
{
        int i, value = 0, digit, next;

        for (i = 0; name[i] == 'I' || name[i] == 'V' || name[i] == 'X'; i++)
                ;
        if (i == 0 || !isupper(UCHAR(name[i])))
                return (0);
        *rest = &name[i];
        while (--i >= 0)        /* from the right: IV is 4, VI is 6 */
        {
                digit = (name[i] == 'I') ? 1 : (name[i] == 'V') ? 5 : 10;
                next = (name[i + 1] == 'I') ? 1 : (name[i + 1] == 'V') ? 5 :
                       (name[i + 1] == 'X') ? 10 : 0;
                value += (digit < next) ? -digit : digit;
        }
        return (value);
}

/*
** Purpose: does a partial bookname typed with a number at the start ("2th",
**          in lower case) match a name with a Roman numeral there
**          ("IIThessalonians")? (A number in the name itself, as in
**          "2Thessalonians", is matched as ordinary letters are.)
*/
static int number_matches(char *typed, char *name)
{
        int number = 0, i = 0;
        char *rest = name, lower[MAXWORDLEN];

        while (isdigit(UCHAR(typed[i])) && number < 1000)
                number = number * 10 + (typed[i++] - '0');
        if (i == 0 || number == 0 || roman_start(name, &rest) != number)
                return (FALSE);
        strncpy(lower, rest, MAXWORDLEN - 1);
        lower[MAXWORDLEN - 1] = '\0';
        u8_lower(lower);
        return (strncmp(lower, &typed[i], strlen(&typed[i])) == 0);
}

/*
** Purpose: does a partial bookname, in lower case, match the start of a
**          book's name (any case), or (see number_matches()) its number?
*/
static int name_starts(char *typed, char *name)
{
        char lower[MAXWORDLEN];

        strncpy(lower, name, MAXWORDLEN - 1);   /* (its letters made small) */
        lower[MAXWORDLEN - 1] = '\0';
        u8_lower(lower);
        if (strncmp(lower, typed, strlen(typed)) == 0)
                return (TRUE);
        return (number_matches(typed, name));
}

/*
** Purpose: print the names of the books a partial bookname matches, for an
**          error: "1Thessalonians, 1Timothy and 1Peter".
*/
void print_book_matches(char *book_str)
{
#ifndef COCO
        extern struct bva *book;
#else
        extern struct bva book[];
#endif
        char typed[MAXWORDLEN];
        int i, count = 0, shown = 0;

        strncpy(typed, book_str, MAXWORDLEN - 1);
        typed[MAXWORDLEN - 1] = '\0';
        u8_lower(typed);
        for (i = 0; i < numbooks; i++)
                if (name_starts(typed, book[i].name))
                        count++;
        for (i = 0; i < numbooks; i++)
                if (name_starts(typed, book[i].name))
                        printf("%s%s", ++shown == 1 ? "" :
                               shown == count ? " and " : ", ", book[i].name);
}

/* Purpose: Takes a partial, or complete, bookname and sees how many
            books begin with that string. Prints partial string to the
            screen, or if match is unique prints complete name.

  Examples: The string "1t" will return the number 0 because it matches
            both 1Thessalonians and 1Timothy. The 0 indicates a non-
            unique match. "1t?" will be printed.

            The string "y" will return -1 because there are no books which
            begin with the string "y". "?" will be printed.

            The string "ex" will return 2 because it only matches one book
            and that book, Exodus, is the second book of the Bible. "Exodus"
            will be printed.

            A number typed at the start also matches a Roman numeral of the
            same value at the start of a name, as the translation writes it:
            "2th" matches IIThessalonians, and "3" IIIJohn. (So the same
            typing works in a translation with "2Thessalonians" and one
            with "IIThessalonians".) Only that way round: "i" matches only
            an I, so in the AV "is" is still Isaiah alone.
*/
int match_it(char *book_str,
             int update_screen)          /* boolean */
{
#ifndef COCO
        extern struct bva *book;
#else
        extern struct bva book[];
#endif
        int i, savei = 0;
        int count = 0;  /* Number of matches found */

	if (update_screen)
        	Erase_word(last_len);

        if (book_str[0] == '\0')
                count = numbooks;
        else
        {
                u8_lower(book_str);

                for (i = 0; i < numbooks; i++)
                {
                        if (name_starts(book_str, book[i].name))
                        {
                                count++;
                                savei = i;
                        }
                }
        }
        if (count == 0)
        {
		if (update_screen)
		{
                  remove_char(book_str);
                  last_len = u8_cols(book_str, strlen(book_str));
                  printf("%s?\b\7", book_str); /* Beep terminal to indicate user
                                                  has typed illegal letter */
                  fflush(stdout);
		}
                return (-1);
        }
        else if (count == 1)
        {
		if (update_screen)
		{
                  last_len = u8_cols(book[savei].name, strlen(book[savei].name));
                  printf("%s", book[savei].name); /* Print entire bookname */
                  fflush(stdout);
		}
                return (savei + 1);
        }
        else /* count > 1 */
        {
		if (update_screen)
		{
                  last_len = u8_cols(book_str, strlen(book_str));
                  printf("%s?\b", book_str); /*Print only what user has typed*/
                  fflush(stdout);
		}
                return (0);
        }
}

/*
** Purpose: Erase the word on the screen at current cursor location
** Bugs: Does not work on terminals that don't use \b to move the cursor
**       one char to the left -- these terminals are rare, but do exist.
*/
void Erase_word(int length)
{
        int i;
        putchar(' ');
        putchar('\b'); /* Erase the '?' character */
        for (i = 0; i < length; i++)
        {
                putchar('\b');
                putchar(' ');
                putchar('\b');
        }
	fflush(stdout);
}

/* Purpose: make a string shorter by one character.
   Example: "abc" becomes "ab"
*/
static void remove_char(char *strg)
{
        strg[u8_prev(strg, strlen(strg))] = '\0';      /* (a whole character) */
}

/*
** Purpose: read a chapter or verse number. Enter on its own means 1, the
**          first. 0 and negative numbers are allowed: they count back from
**          the start of the book or chapter (verse 0 is the last verse of the
**          chapter before; chapter 0 is the last chapter of the book before).
*/
static int get_number(void)
{
        int i, number, negative, digits;
        char *chrptr;

        chrptr = get_line(DIGITS_MODE);
        i = number = digits = 0;
        negative = (chrptr[0] == '-');
        if (negative)
                i++;
        while (isdigit(UCHAR(chrptr[i])))
        {
                if (number < 100000) /* far past any real chapter or verse */
                        number = number * 10 + (chrptr[i] - '0');
                i++;
                digits++;
        }
        if (digits == 0)  /* nothing typed (or only "-"): the default */
                return (1);
        return (negative ? -number : number);
}

/* Purpose: Prompt user to enter the book, chapt, and verse desired */
static struct bcv_type *bcv_menu(void)
{
        static char book_str[MAXWORDLEN];
        char typed[8];          /* a letter typed: in UTF-8, up to 4 bytes */
        int c, c2, n, k;

get_name_again:
        scr_mode("input");
        printf("\n(Use a blank line to abort...)\n");
        printf("Please enter: Book name\n");
        printf("?\b");
        fflush(stdout);
        last_len = 0;
        book_str[0] = '\0';
        for(;;)
        {
                c = ugetchar();
                if ((c > 0 && c < 128 && isalnum(c)) || (c >= 0xC2 && c <= 0xF4))
                {
                        /* a letter: beyond ASCII, all its bytes (UTF-8) */
                        typed[0] = (char) c;
                        n = 1;
                        if (c >= 0x80)
                                for (k = (c >= 0xF0) ? 4 : (c >= 0xE0) ? 3 : 2;
                                     n < k; n++)
                                {
                                        if (((c2 = ugetchar()) & 0xC0) != 0x80)
                                                break;  /* (cut short) */
                                        typed[n] = (char) c2;
                                }
                        typed[n] = '\0';
                        if (strlen(book_str) + n > (MAXWORDLEN - 1))
                        {
                            printf("\nOut of room in string to store word!\n");
                            exit_program(1);
                        }
                        strcat(book_str, typed);
                }
                else if (c == '\b' || c == '\177') /* Back space, or Delete */
                        remove_char(book_str);
                else if (c == '\30' || c == '\25')/*Ctrl-X, Ctrl-U Erase line*/
                        book_str[0] = '\0';
                else if ((book_str[0] >= '1') &&
                         (book_str[0] <= '3') &&
                         (book_str[1] == '\0') &&
                         (c == ' '))
                        continue;
                else
                        break;
/*
** Now we update the screen, erasing what used to be there and writing out
** the new string. The value returned by match_it() indicates how many
** books, or which book, begin with the string given.
** e.g. -1  none
**       0  more than one
**       1  Genesis
**       ....
**       66 Revelation
*/
                bcv.book_number = match_it(book_str, TRUE);
        }
        if (book_str[0] == '\0') /* Abort on blank line */
                return ((struct bcv_type *)NULL);
        if (bcv.book_number == -1)
        {
                printf("\nThere is no book by that name. Please try again.\n");
                goto get_name_again;
        }
        if (bcv.book_number == 0)
        {
                printf("\nThere is more than one book that matches that name.\n");
                printf("Please be more specific.\n");
                goto get_name_again;
        }

        printf("\n\nPlease enter: Chapter number    (default = 1)\n");
        printf("?\b");
        fflush(stdout);
        bcv.chapter_number = get_number();

        printf("\n\nPlease enter: Verse number    (default = 1)\n");
        printf("?\b");
        fflush(stdout);
        bcv.verse_number = get_number();

        return(&bcv);
}

/*
** Purpose: how many bytes of a verse's text fit on a line of width columns:
**          at least one character, so that a line always moves on. (Its
**          characters may be several bytes, or two columns, in UTF-8.)
*/
static int line_fit(char *strg, int width)
{
        int fit = u8_fit(strg, width);

        if (fit == 0 && strg[0] != '\0')
                fit = u8_len(strg);
        return (fit);
}

/* figure out how many lines on the screen it will take to print this verse */
static int count_lines(int tag, char *bookname, char *strg, int indent, int top_of_screen)
{
        static char previous_book[MAXBOOKNAME] = { '\0' };
        int a_lines = 0, i, fit, indentWidth = WIDTH - indent;

        if (tag == -1)
        {
           if (bookname[0] != '\0' &&
              (top_of_screen || strcmp(bookname, previous_book)))
           {
              if (top_of_screen == FALSE)
                a_lines++;
              a_lines++;
           }
        }
        strcpy(previous_book, bookname);
        fit = line_fit(strg, indentWidth);
        if (strg[fit] == '\0')
                return(1 + a_lines);
 for (i = fit; i > 0; i--)
  if (strg[i] == ' ')
   return(count_lines(tag, bookname, &strg[i + 1],  /* recursive! */
                      indent, FALSE) + 1 + a_lines);
 return(count_lines(tag, bookname, &strg[fit], /* here too! */
                    indent, FALSE) + 1 + a_lines);
}

/* Purpose: Display one line of text to the screen. Highlight, via reverse
            video, the word being searched for.
*/
#define BYTE_IS(c, b) (((c) & 0xFF) == (b))

static void dis_one_line(char *strg)
{
    int i = 0, j, k, n, from;
    char wrd[MAXWORDLEN];
    char lower_wrd[MAXWORDLEN];
    char *hi;           /* for each byte of the line: TRUE if highlighted */
    extern char *rev_word[];

    if ((hi = (char *) malloc(strlen(strg) + 1)) == NULL)
    {
        printf("Out of memory!!\n");
        exit_program(1);
    }
    for(;;)
    {
        while (strg[i] != '\0' && !u8_is_letter(&strg[i]))
                hi[i++] = FALSE; /* commas, spaces, etc: shown as they are */
        if (strg[i] == '\0') /* We is done with this line */
                break;

        /* a word: its letters (a letter may be several bytes, in UTF-8) */
        from = i;
        j = 0;
        while (u8_is_letter(&strg[i]) ||
                      (strg[i] == '\'') || /* allow "day's" to be highlighted */
                      (strg[i] == '-') ||   /* same with "father-in-law" */
                      (BYTE_IS(strg[i], 0xD6) && BYTE_IS(strg[i + 1], 0xBE)))
                                            /* and Hebrew words joined by maqaf */
        {
                n = u8_len(&strg[i]);
                if (j + n < MAXWORDLEN)
                {
                        strncpy(&wrd[j], &strg[i], n);
                        j += n;
                }
                i += n;
        }

        wrd[j] = '\0';
        strcpy(lower_wrd, wrd);
        u8_lower(lower_wrd);
/*
** We need to loop here to see if lower_wrd is the same as ANY of the multiple
** words used in the searching command.
*/
        k = -1;
        while (rev_word[++k] != 0)
        {
	/* printf("Comparing '%s' with '%s'\n", lower_wrd, rev_word[k]); */
                if (strcmp (lower_wrd, rev_word[k]) == 0)
                        break;
        }
        for (; from < i; from++)
                hi[from] = (rev_word[k] != 0);
    }
    print_visual(strg, hi);
    putchar('\n');
    free(hi);
}

/*
** Purpose: print a string as it is to be seen: with any right-to-left
**          text (Hebrew) turned about, so that it reads right to left on a
**          terminal that shows every line from left to right (see
**          u8_display_order() in utf8.c). hi, if not NULL, says which bytes
**          are highlighted (reverse video), and goes with them as they
**          move. Text without any right-to-left characters is printed as
**          it is.
*/
#define MAXVISUAL 400   /* characters: more than a line of the screen */

static void print_visual(char *s, char *hi)
{
        int start[MAXVISUAL], len[MAXVISUAL];
        int n, k, j, on = FALSE;

        n = u8_display_order(s, (int) strlen(s), start, len, MAXVISUAL);
        for (k = 0; k < n; k++)
        {
                if (hi != NULL && hi[start[k]] != on)
                {
                        on = hi[start[k]];
                        if (on)
                                standout();
                        else
                                standend();
                }
                for (j = 0; j < len[k]; j++)
                        putchar(s[start[k] + j]);
        }
        if (on)
                standend();
}

/*
** A verse taller than the screen (see view_in_context()) is shown a part at
** a time: dis_verse() leaves out its first dv_skip lines and then shows at
** most dv_left (-1: all of it). With dv_body_only, display_verse() shows
** neither the book's heading nor the verse's number (a later part).
*/
static int dv_skip = 0, dv_left = -1, dv_body_only = FALSE;

/* Purpose: one line of a verse, as dis_verse() wraps it: indented, but for
   the first, which follows the verse's number; or left out (see above) */
static void verse_line(char *strg, int indent, int firstTime)
{
        if (dv_skip > 0)
        {
                dv_skip--;
                return;
        }
        if (dv_left == 0)
                return;
        if (dv_left > 0)
                dv_left--;
        if (firstTime == FALSE || dv_body_only)
                printf("%s", spaces + MAXINDENT - indent);
        dis_one_line(strg);
}

/* Purpose: performs word wrapping by inserting a '\0' at appropriate location
            and calls dis_one_line to print one line of the verse, up to the
            '\0'. Then it recursively calls itself to handle the remainder
            of the verse.
*/
static void dis_verse(char *strg, int indent, int firstTime)
{
        int i, fit, indentWidth = WIDTH - indent;
        char *outstr;

        fit = line_fit(strg, indentWidth);
        if (strg[fit] == '\0')
        {
                if ((strg[0] == '\0') && (firstTime == FALSE))
                        return;
                verse_line(strg, indent, firstTime);
                return;
        }
        for (i = fit; i > 0; i--)
        {
                if (strg[i] == ' ')
                {
                        strg[i] = '\0';
                        verse_line(strg, indent, firstTime);
                        strg[i] = ' ';
                        dis_verse(&strg[i + 1], indent, FALSE);
                        return;
                }
        }
        outstr = (char *) malloc (strlen(strg) + 1);
        if (outstr == NULL)
        {
                printf("Out of memory!!\n");
                exit_program(1);
        }

        for (i = 0; i < fit; i++)
                outstr[i] = strg[i];
        outstr[i] = '\0';
        verse_line(outstr, indent, firstTime);
        free(outstr);
        dis_verse(&strg[i], indent, FALSE);
        return;
}

/* Purpose: the columns a book's name takes, no more than most (for the spaces
            before it, so that the names line up) */
static int book_cols(char *name, int most)
{
        int cols = u8_cols(name, strlen(name));

        return (cols > most ? most : cols);
}

/* Purpose: Display header info for a verse, such as bookname, chapter and
            verse numbers etc.
*/
static void display_verse(int tag, struct bcv_array_type *bcv_arrays, int verIndent, int top_of_screen)
{
        int cv_length;
        static char previous_book[MAXBOOKNAME] = { '\0' };
        if (tag != -1)
        {
            printf("%c)", tag_label(tag));
            if (top_of_screen || strcmp(bcv_arrays->book_array, previous_book))
            {
				   if (isdigit (UCHAR(bcv_arrays->book_array[0])) &&
					    isupper (UCHAR(bcv_arrays->book_array[1])) &&
					    islower (UCHAR(bcv_arrays->book_array[2])) )
               {
                  printf("%s", spaces + MAXINDENT + book_cols(bcv_arrays->book_array, 14) + 1 - 15);
                  printf("%c ", bcv_arrays->book_array[0]);
                  print_visual(&bcv_arrays->book_array[1], (char *) NULL);
                  putchar(' ');
               }
				   else
					{
                  printf("%s", spaces + MAXINDENT + book_cols(bcv_arrays->book_array, 15) - 15);
                  print_visual(bcv_arrays->book_array, (char *) NULL);
                  putchar(' ');
               }
            }
            else
               printf("%s", spaces + MAXINDENT - 16);
        }
        else if (dv_body_only)
           ;    /* (a later part of a tall verse: its text only) */
        else
        {
           if (bcv_arrays->book_array[0] != '\0' &&
              (top_of_screen || strcmp(bcv_arrays->book_array, previous_book)))
           {
              int indent, width;

              if (top_of_screen == FALSE)
                 printf("\n");
              /* 28 spaces on a screen of 80 columns or more; one fewer
                 for every 2 columns less (14 on a phone's 52), and never
                 so many that the heading wraps */
              indent = 28;
              if (s_cols < 80)
                 indent -= (80 - s_cols) / 2;
              width = u8_cols(bcv_arrays->book_array, strlen(bcv_arrays->book_array)) +
                      3 + strlen(etla);       /* (and " (AV)") */
              if (isdigit(UCHAR(bcv_arrays->book_array[0])) &&
                  isupper(UCHAR(bcv_arrays->book_array[1])))
                 width++;                     /* ("1 John") */
              if (indent > s_cols - 1 - width)
                 indent = s_cols - 1 - width;
              if (indent < 0)
                 indent = 0;
              while (indent-- > 0)
                 putchar(' ');
              underline();
				  if (isdigit (UCHAR(bcv_arrays->book_array[0])) &&
					   isupper (UCHAR(bcv_arrays->book_array[1])) &&
					   islower (UCHAR(bcv_arrays->book_array[2])) )
              {
                 printf("%c ", bcv_arrays->book_array[0]);
                 print_visual(&bcv_arrays->book_array[1], (char *) NULL);
              }
				  else
                 print_visual(bcv_arrays->book_array, (char *) NULL);
              underend();
              printf(" (%s)\n", etla);
           }
        }
        if (dv_body_only)
                ;       /* (indented by verse_line()) */
        else if (tag == -1 && (strcmp(bcv_arrays->chapt_array, "0") == 0 ||
                          strcmp(bcv_arrays->verse_array, "0") == 0))
        {
                /* verse 0 or chapter 0, the text before verse 1 or chapter
                   1 (see index.c): plain text, without the 0, which isn't
                   in the source (in a list of search results, its
                   reference is shown, as any verse's) */
                printf("%s", spaces + MAXINDENT - 8);
        }
        else
        {
                printf("%s:", bcv_arrays->chapt_array);
                printf("%s", bcv_arrays->verse_array);
                cv_length = strlen(bcv_arrays->chapt_array);
                cv_length++;  /* Count the colon */
                cv_length += strlen(bcv_arrays->verse_array);
                printf("%s", spaces + cv_length + MAXINDENT - 8);
        }
        dis_verse(bcv_arrays->data_array, verIndent, TRUE);
        strcpy(previous_book, bcv_arrays->book_array);
}

static long int calculate_fv(long int last_verse)
{
        int num_lines = s_lines;
        char top_of_screen = TRUE; /* I don't think this is right */
        struct bcv_array_type *bcv_arrays;
        for(;;)
        {
            if ((last_verse - 1) < 1)
                return (1);
            while (!check_limits(last_verse - 1))
                last_verse--;
            bcv_arrays = get_verse(--last_verse);
            num_lines -= count_lines(-1, bcv_arrays->book_array,
               bcv_arrays->data_array, 8, top_of_screen);
            top_of_screen = FALSE;
            if (num_lines < 1)
                break;
        }
        return (last_verse + 1);
}

/*
** Purpose: after stepping forward with '.', the passage screen has scrolled
**          and no longer starts with a heading. Find the first verse still
**          wholly on the screen, given the last verse shown: the earliest
**          verse from which the verses through last_shown fit above the
**          bottom line.
*/
static long int scrolled_top(long int last_shown)
{
        int num_lines = s_lines - 1;  /* the bottom line holds the cursor */
        long int verse = last_shown;
        struct bcv_array_type *bcv_arrays;

        /* count_lines() compares each verse's book with the previous one
        ** it was given; start it on this verse's book */
        bcv_arrays = get_verse(last_shown);
        count_lines(-1, bcv_arrays->book_array, bcv_arrays->data_array, 8, FALSE);
        for(;;)
        {
            if (!check_limits(verse))
                return verse + 1;
            bcv_arrays = get_verse(verse);
            num_lines -= count_lines(-1, bcv_arrays->book_array,
               bcv_arrays->data_array, 8, FALSE);
            if (num_lines < 0)          /* this verse is partly off the top */
                return verse + 1;
            if (verse == 1)
                return 1;
            verse--;
        }
}

/* maybe this should be in a macro for speed? */
static void rev_printf(char *string)
{
        standout();
        print_visual(string, (char *) NULL);    /* (Hebrew turned about) */
        standend();
}

static int check_limits(long int cur_verse)
{
        if (cur_verse >= (numverses + 1))
                return (FALSE);
        if (cur_verse <= 0)
                return (FALSE);
        return (TRUE);
}

/*
** cur_verse is the verse at the top of the screen to draw. After drawing,
** it is one past the next verse to show (the one that didn't fit), which
** the keys below rely on; at the end of the Bible, that next verse is one
** past Revelation's last, numverses + 1. A key with nowhere to go (back a
** book or a verse from Genesis 1:1, on a book or a screen from the end)
** beeps and leaves the screen as it is.
*/
/*
** Purpose: show the first verse of a screen when it is taller than the
**          screen (as a long introduction can be): the part of it from
**          line shown of its text, as much as fits. Returns the lines of
**          its text shown so far (shown and this part's), or 0 if this was
**          its last part.
*/
static int show_tall_verse(struct bcv_array_type *bcv_arrays, int shown)
{
        int total, text_lines, avail = s_lines - 1, part;

        total = count_lines(-1, bcv_arrays->book_array, bcv_arrays->data_array, 8, TRUE);
        text_lines = count_lines(-1, "", bcv_arrays->data_array, 8, FALSE);
        if (shown == 0)
        {
                part = avail - (total - text_lines);    /* (its heading first) */
                if (part < 1)
                        part = 1;
        }
        else
                part = avail;
        if (shown + part > text_lines)
                part = text_lines - shown;
        dv_skip = shown;
        dv_left = part;
        dv_body_only = (shown > 0);
        display_verse(-1, bcv_arrays, 8, TRUE);
        dv_skip = 0;
        dv_left = -1;
        dv_body_only = FALSE;
        shown += part;
        return (shown >= text_lines ? 0 : shown);
}

static void view_in_context(long int cur_verse)
{
        long int first_verse, start;
        int num_lines;
        char exit_now = FALSE, top_of_screen;
        struct bcv_array_type *bcv_arrays;
        /* a verse taller than the screen, shown a part at a time: which
           verse, the lines of it shown before this screen, after it (0 when
           its last part is showing), and whether n goes on in it */
        long int tall_verse = 0;
        int tall_shown = 0, tall_next = 0, tall_continue = FALSE;

        if (cur_verse < 1)
                cur_verse = 1;
        if (cur_verse > numverses)
                cur_verse = numverses;
        do{
                cls(); /* may update s_lines if the window was resized */
                num_lines = s_lines;
                first_verse = cur_verse; /* a legal value */
                top_of_screen = TRUE;
                tall_shown = (tall_continue && cur_verse == tall_verse) ? tall_next : 0;
                tall_continue = FALSE;
                tall_next = 0;
                if (check_limits(cur_verse))
                {
                    bcv_arrays = get_verse(cur_verse);
                    if (tall_shown > 0 ||
                        count_lines(-1, bcv_arrays->book_array,
                                    bcv_arrays->data_array, 8, TRUE) > s_lines - 1)
                    {
                        /* taller than the screen: a part of it, alone */
                        tall_verse = cur_verse;
                        tall_next = show_tall_verse(bcv_arrays, tall_shown);
                        /* (one past the next to show: itself, or the next) */
                        cur_verse += (tall_next > 0) ? 1 : 2;
                        top_of_screen = FALSE;
                        num_lines = 0;
                    }
                }
                for(;num_lines > 0;)
                {
                    if (!check_limits(cur_verse))
                    {
                        cur_verse++;    /* the end of the Bible: no more */
                        break;
                    }
                    bcv_arrays = get_verse(cur_verse++);
                    num_lines -= count_lines(-1, bcv_arrays->book_array,
                       bcv_arrays->data_array, 8, top_of_screen);
                    if (num_lines < 1)
                            break;
/*
**    printf("%d-", cur_verse - 1);
*/
                    display_verse(-1, bcv_arrays, 8, top_of_screen);
                    top_of_screen = FALSE;
                }
                printf(" Press ");
                rev_printf("?");
/*
** These backspaces should be replaced with an escape sequence from
** the termcap library to move the cursor to the left.
*/
                printf(" for menu.\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b");
                fflush(stdout);
get_command:    scr_mode("passage");
                switch (tolower(screen_key()))
                {
                case 'f':  /* the first verse of the next book */
                        start = cur_verse - 1;
                        while (start <= numverses &&
                               verse_to_book(start - 1) == verse_to_book(start))
                                start++;
                        if (start > numverses)  /* in Revelation */
                        {
                                putchar('\7');     /* beep */
                                fflush(stdout);
                                goto get_command;
                        }
                        cur_verse = start;
                        break;
                case 'b':  /* the first verse of this book, or, from there,
                              of the book before */
                        if (first_verse == -1)
                                first_verse = calculate_fv(cur_verse);
                        start = first_verse - 1;
                        if (start < 1)          /* at Genesis 1:1 */
                        {
                                putchar('\7');     /* beep */
                                fflush(stdout);
                                goto get_command;
                        }
                        while (start > 1 &&
                               verse_to_book(start - 1) == verse_to_book(start))
                                start--;
                        cur_verse = start;
                        break;
                case 'n':
                case ' ':
                case 0x16: /* Page Down on MM/1 */
                        if (cur_verse - 1 > numverses)  /* the end is showing */
                        {
                                putchar('\7');     /* beep */
                                fflush(stdout);
                                goto get_command;
                        }
/*
** This verse didn't fit on the current screen; so it becomes the first
** verse on the next screen (or, of a tall verse, its next part)
*/
                        tall_continue = (tall_next > 0);
                        cur_verse--;
                        break;
                case '.':
                case '>':
                case 0xe: /* Down Arrow on MM/1 */
                        if (tall_next > 0)      /* (within a tall verse: on in it) */
                        {
                                tall_continue = TRUE;
                                cur_verse--;
                                break;
                        }
                        if (check_limits(cur_verse - 1))
                        {
                                bcv_arrays = get_verse(cur_verse++ - 1);
                                display_verse(-1, bcv_arrays, 8, top_of_screen);
                                first_verse = -1; /* an illegal value */
                        }
                        else
                        {
                                printf("Error!  Can't advance one verse.");
                                printf(" You are at the END.\n");
#                               ifdef VMS
                                        putchar('\r');
#                               endif
                        }
                        goto get_command;
                case 'p':
                case 0x1a: /* Page Up on MM/1 */
                        if (first_verse == -1)
                                first_verse = calculate_fv(cur_verse);
                        cur_verse = calculate_fv(first_verse);
                        break;
                case ',':
                case '<':
                case 0x10: /* Up Arrow on MM/1 */
                        if (first_verse == -1)
                                first_verse = calculate_fv(cur_verse - 1);
                        if (first_verse <= 1)   /* at Genesis 1:1 */
                        {
                                putchar('\7');     /* beep */
                                fflush(stdout);
                                goto get_command;
                        }
                        cur_verse = first_verse - 1;
                        break;
                case 'x':
                case 0x1b: /* ESC */
                        exit_now = TRUE;
                        break;
                case KEY_REDRAW: /* CTRL-L, or the window was resized */
/*
** Draw this screen again from the same first verse, for the window's new
** size. After stepping with '.' the screen has scrolled, so find the first
** verse still wholly on it from the last verse shown, using the size the
** screen was drawn for: the new size is only picked up by the cls() that
** redraws it. (cur_verse is then two past the last verse shown: drawing the
** page stopped one past the verse that didn't fit, and '.' showed that one.)
*/
                        if (first_verse == -1)
                                first_verse = scrolled_top(cur_verse - 2);
                        cur_verse = first_verse;
                        if (tall_verse == first_verse)
                        {
                                /* (a tall verse: the same part again) */
                                tall_next = tall_shown;
                                tall_continue = (tall_shown > 0);
                        }
                        break;
                default:
                        cls();
                        underline();
                        printf("\n\n\n\n\n\nHelp Menu\n\n");
                        underend();
                        rev_printf("space bar");
                        printf(" or ");
                        rev_printf("n");
                        printf(" next screen%s\n", KEYHELP(", or Page Down"));
                        rev_printf("p");
                        printf("              previous screen%s\n", KEYHELP(", or Page Up"));
                        rev_printf(".");
                        printf("         or ");
                        rev_printf(">");
                        printf(" next verse%s\n", KEYHELP(", or Down arrow"));
                        rev_printf(",");
                        printf("         or ");
                        rev_printf("<");
                        printf(" previous verse%s\n", KEYHELP(", or Up arrow"));
                        rev_printf("f");
                        printf("              forward one book\n");
                        rev_printf("b");
                        printf("              backs up one book\n");
                        rev_printf("x");
                        printf("              exit to Main Menu\n");
			printf("\n(Press any Key) ");
			fflush(stdout);
                        if (first_verse == -1)
                                first_verse = calculate_fv(cur_verse - 1);
                        cur_verse = first_verse;
			scr_mode("anykey");
			ugetchar();
                }
        }while (!(exit_now));
}

#ifdef RAMDISK
static long int vlist(struct TEMPFILE *tf, /* temp file */
                      long int rnum)    /* record number in file */
{
	/* (a verse number in the file is 4 bytes: not sizeof(long int),
	   which is 8 on some systems) */
	if (fseek(tf->fname, rnum * 4L, 0) == -1)
	{
		printf("fseek failed on RAM DISK!\n");
		exit_program(1);
	}
	return (fgetl(tf->fname));
}
#endif

#ifdef RAMDISK
static void view_match_list(struct TEMPFILE *verse_list)
{
#else
static void view_match_list(long int *verse_list)
{
#endif
        long int i = 0, first_i, j;     /* entries in the list */
        int num_lines, tag, n;
	int ui; /* User Input */
        char count[24 + MAXCOUNTWORD], times[24 + MAXCOUNTWORD];
        long int cur_verse;
        char exit_now = FALSE, top_of_screen;
        struct bcv_array_type *bcv_arrays;
        do{
                cls();
                num_lines = s_lines;
                if (i == 0)
                {
/*
** "--Beginning of list-- 56 verses", and when the search was one word
** alone, how many times it occurs: ", grass 62 times", with the word in
** reverse video as it is in the verses below (a verse can have it more
** than once). That part is left out if it doesn't fit on the line. A
** count of one word in a search with others, or limited to a book, would
** be of the whole Bible, not of this list, so none is shown.
*/
#define HEAD_TEXT       "--Beginning of list--"
                        rev_printf(HEAD_TEXT);
                        sprintf(count, " %ld verse%s", num_verses,
                                num_verses == 1 ? "" : "s");
                        fputs(count, stdout);
                        if (count_word[0] != '\0')
                        {
                                sprintf(times, " %ld time%s", num_found,
                                        num_found == 1 ? "" : "s");
                                /* ", " word times: up to the last column, as
                                   modern terminals (xterm and the like) take a
                                   full line before the newline without an
                                   extra blank line. (Change s_cols to WIDTH if
                                   one doesn't.) */
                                if ((int) (sizeof(HEAD_TEXT) - 1 + strlen(count) +
                                        2 + u8_cols(count_word, strlen(count_word)) +
                                        strlen(times))
                                        <= s_cols)
                                {
                                        fputs(", ", stdout);
                                        rev_printf(count_word);
                                        fputs(times, stdout);
                                }
                        }
                        putchar('\n');
                        num_lines--;
                }
                first_i = i;
                tag = 1;
                top_of_screen = TRUE;
                for(;;)
                {
#ifdef RAMDISK
                    cur_verse = vlist(verse_list, i++);
#else
                    cur_verse = verse_list[i++];
#endif
                    if (cur_verse == 0)
                        break;
                    if (tag > MAXTAGS) /* no label left for another entry */
                        break;
                    bcv_arrays = get_verse(cur_verse);
                    n = count_lines(tag, bcv_arrays->book_array,
                             bcv_arrays->data_array, MAXINDENT, top_of_screen);
                    if (top_of_screen && n > num_lines - 1 && num_lines > 1)
                    {
                        /* taller than the screen (a long introduction): as
                           much of it as fits, to choose it by (its label
                           shows it all, a screenful at a time) */
                        dv_left = num_lines - 1;
                        display_verse(tag++, bcv_arrays, MAXINDENT, top_of_screen);
                        dv_left = -1;
                        top_of_screen = FALSE;
                        num_lines = 1;  /* (the next entry: the next screen) */
                        continue;
                    }
                    num_lines -= n;
                    if (num_lines < 1)
                        break;
                    display_verse(tag++, bcv_arrays, MAXINDENT, top_of_screen);
                    top_of_screen = FALSE;
                }
/*
** The prompt line. In a window too narrow for all of it (a phone, say),
** the middle part is left out, so the line doesn't wrap. The lengths are
** of the text as printed; sizeof counts the '\0' too, hence the - 1s.
*/
#define END_TEXT        "--End of list--"
#define END_HOW         "   Press <x> to exit."
#define MORE_TEXT       "--More--"
#define MORE_HOW        "   Press <space> to advance one screen."
#define MENU_TEXT       " (Press ? for menu.)"
#define FITS(t, how)    ((int) (sizeof(t) - 1 + sizeof(how) - 1 + \
                                sizeof(MENU_TEXT) - 1) <= WIDTH)
                standout();
                if (cur_verse == 0)
                {
                        printf(END_TEXT);
                        standend();
                        if (FITS(END_TEXT, END_HOW))
                                printf(END_HOW);
                }
                else
                {
                        printf(MORE_TEXT);
                        standend();
                        if (FITS(MORE_TEXT, MORE_HOW))
                                printf(MORE_HOW);
                }
                printf(" (Press ");
                rev_printf("?");
                printf(" for menu.)");
                fflush(stdout);
                scr_mode("list");
                switch (ui = tolower(screen_key()))
                {
                case KEY_REDRAW: /* CTRL-L, or the window was resized */
                        i = first_i;  /* same first entry, new size */
                        break;
                case 'n':
                case ' ':
		case 0x16: /* Page Down on MM/1 */
                        i--;
                        break;
                case 'p':
		case 0x1a: /* Page Up on MM/1 */
                   num_lines = s_lines;
                   n = 0; /* entries that fit on the previous screen */
                   for(;;)
                   {
                     if (--first_i < 0)
                     {
                        first_i = -1;
                        break;
                     }
                     if (++n > MAXTAGS) /* no label left for another entry */
                        break;
#ifdef RAMDISK
                     cur_verse = vlist(verse_list, first_i);
#else
                     cur_verse = verse_list[first_i];
#endif
                     bcv_arrays = get_verse(cur_verse);
                     num_lines -= count_lines(tag, bcv_arrays->book_array,
                               bcv_arrays->data_array, MAXINDENT, FALSE);
                     if (num_lines < 1)
                        break;
                   }
                   i = first_i + 1;
                   break;
                case 'x':
		case 0x1b: /* ESC */
                        exit_now = TRUE;
                        break;
		default:
/*
** A label picks the entry that far down the list from the first one shown,
** even if it is past the bottom of the screen: on a phone, entries shown
** while the keyboard was hidden disappear when it reappears, but their
** labels can still be typed. The entry must exist in the list.
*/
		    cur_verse = 0;
		    if ((n = key_to_tag(ui)) > 0)
		    {
			for (j = first_i; j < first_i + n; j++)
			{
#ifdef RAMDISK
			    cur_verse = vlist(verse_list, j);
#else
			    cur_verse = verse_list[j];
#endif
			    if (cur_verse == 0) /* the list ends before it */
				break;
			}
		    }
		    if (cur_verse != 0)
		    {
                        view_in_context(cur_verse - NUM_SHOW_BEFORE);
                        i = first_i;   /* back to this page afterwards */
		    }
		    else
		    {
			cls();
                        underline();
                        printf("\n\n\n\n\n\nHelp Menu\n\n");
                        underend();
                        printf("Press:\n");
                        rev_printf("number");
                        printf(" or ");
                        rev_printf("letter");
                        printf(" views that verse in context\n");
                        printf("       (");
                        rev_printf("q w e r t y u i o");
                        printf(" also choose 1 to 9)\n");
                        rev_printf("space bar");
                        printf(" or ");
                        rev_printf("n");
                        printf(" next screen%s\n", KEYHELP(", or Page Down"));
                        rev_printf("p");
                        printf(" previous screen%s\n", KEYHELP(", or Page Up"));
                        rev_printf("x");
                        printf(" exits to Main Menu\n");
			printf("\n(Press any Key) ");
			fflush(stdout);
			scr_mode("anykey");
			ugetchar();
			i = first_i;
		    }
                }
        }while (!(exit_now));
}

/*
** Purpose: read a key for the main menu, and the passage and search-result
**          screens. If the
**          window is resized while waiting, return KEY_REDRAW at once, so
**          the screen is drawn again for the new size.
*/
static int screen_key(void)
{
        int c;

        redraw_on_resize = TRUE;
        c = ugetchar();
        redraw_on_resize = FALSE;
        return c;
}

/* Purpose: the label shown beside entry number "tag" in a list */
static int tag_label(int tag)
{
        if (tag <= 9)
                return '0' + tag; /* This only works on ASCII machines */
        return more_tags[tag - 10];
}

/*
** Purpose: which entry of the list a key picks.
** Returns: the entry number (1 for the first), or 0 if the key picks none.
*/
static int key_to_tag(int key)
{
        char *p;

        if (key >= '1' && key <= '9')
                return key - '0';
        if (key == '\0')
                return 0;
        if ((p = find_char(digit_keys, key)) != NULL)
                return 1 + (int) (p - digit_keys);
        if ((p = find_char(more_tags, key)) != NULL)
                return 10 + (int) (p - more_tags);
        return 0;
}

/*
** The first two Information pages. Each line is at most 50 characters, so
** that it fits a phone's screen (about 52 columns) without wrapping, and
** each page fits 24 lines.
*/
static void prog_info(void)
{
cls();
printf("Electronic Bible and Concordance (EBaC) %s\n", EBAC_RELEASE);
printf("  Copyright (C) 2004 by Neil G. Brookins\n");
if (EBAC_COMMIT[0] != '\0')
        printf("  Built from commit %s\n", EBAC_COMMIT);
printf("\n");
/* the translations of EBaC 1.3, from before the downloads (A) */
printf("Translations EBaC was first made for:\n");
printf("  King James (Authorized) Version\n");
printf("  New International Version\n");
printf("  New Revised Standard Version\n");
printf("  Revised Standard Version\n");
printf("  Webster's Bible\n");
printf("  Darby Bible\n");
printf("  Young's Literal Translation\n");
printf("\n");
printf("For help, or to report a problem, open an issue:\n");
printf("  The program:\n");
printf("    github.com/neilbrookins/ebac\n");
printf("  The Home Assistant app:\n");
printf("    github.com/neilbrookins/ebac-ha-app\n");
printf("  The Home Assistant voice integration:\n");
printf("    github.com/neilbrookins/ebac-ha-integration\n");
printf("  The texts to download (A on the main menu):\n");
printf("    github.com/scrollmapper/bible_databases\n");
printf("\n");
printf("(Press any Key) ");
fflush(stdout);
scr_mode("anykey");
ugetchar();
cls();
/* in the order EBaC was ported to them, one by one */
printf("EBaC has run on these computers, ported to each\n");
printf("in turn, in this order, from the first in 1992:\n");
printf("  MM/1, System V, TC-70  (OS-9 68000)\n");
printf("  Digital VAX            (OpenVMS)\n");
printf("  Tandy Color Computer 3 (OS-9 6809)\n");
printf("  AT&T 3B1               (UNIX System V)\n");
/* "Windows" was Cygwin, with which EBaC 1.3.0 was used and tested at
   length on Windows; today, Windows 10 and 11 run it in WSL (Linux) */
printf("  IBM-PC and compatibles (MS-DOS; Cygwin)\n");
printf("  Sun                    (SunOS 4.x, Solaris 2.x)\n");
printf("  DEC                    (Ultrix, OSF/1)\n");
printf("  HP 9000                (HP-UX 10.x)\n");
printf("  PCs and Raspberry Pi   (Linux, with ncurses)\n");
printf("  Windows 10 and 11      (WSL, or Cygwin)\n");
printf("  Apple Macintosh        (macOS)\n");
printf("  Home Assistant         (an app in its sidebar)\n");
printf("\n");
printf("(Press any Key) ");
fflush(stdout);
scr_mode("anykey");
ugetchar();
prog_diagnostics();
}

/*
** Purpose: the third Information page: which EBaC this is and where it
**          runs from, so that two copies (say, in two windows) can be told
**          apart, and how the terminal is set up, as a wrong TERM (one set
**          by a login script, say) makes the screen come out garbled.
**          (Whether this is the RAMDISK or NORAMDISK build isn't shown: the
**          two must look the same.)
*/
static void prog_diagnostics(void)
{
#ifdef UNIXV
        char *term;
        struct utsname sys;
#endif
        char *library;

        cls();
        underline();
        printf("Diagnostic information\n");
        underend();
        printf("\n");
        printf("Version:          %s", EBAC_RELEASE);
        if (EBAC_COMMIT[0] != '\0')
                printf(", commit %s", EBAC_COMMIT);
        printf("\n");
        if (EBAC_PACKAGE[0] != '\0')
                printf("                  %s\n", EBAC_PACKAGE);
        if (program_path[0] != '\0')
                printf("Program:          %s\n", program_path);
        printf("BIBLEROOT:        %s\n", bible_root);
        printf("Search history:   %s\n", LOGFILE);
#ifdef UNIXV
        term = getenv("TERM");
        printf("TERM:             %s\n", term != NULL ? term : "(not set)");
#endif
        printf("Window:           %d rows, %d columns\n", s_lines, s_cols);
        library = scr_library();
        if (library[0] != '\0')
                printf("Terminal library: %s\n", library);
#ifdef UNIXV
        if (uname(&sys) == 0)
                printf("System:           %s %s %s\n", sys.sysname, sys.release,
                       sys.machine);
        printf("\n");
        printf("If the screen looks garbled, TERM may not match your terminal\n");
        printf("window: a login script may set TERM=vt100 in an xterm, say. Set\n");
        printf("TERM to your terminal's type (xterm-256color, for most), and\n");
        printf("start EBaC again.\n");
#endif
        printf("\n");
        printf("(Press any Key) ");
        fflush(stdout);
        scr_mode("anykey");
        ugetchar();
}

/*
** Purpose: remember how this program was started, as a full path where
**          possible, before initialize() changes to BIBLEROOT.
*/
static void set_program_path(char *argv0)
{
        if (argv0 == NULL)
                return;
#ifdef UNIXV
        if (argv0[0] != '/' && find_char(argv0, '/') != NULL &&
            getcwd(program_path, sizeof(program_path) - 2) != NULL &&
            strlen(program_path) + strlen(argv0) + 2 <= sizeof(program_path))
        {
                strcat(program_path, "/");
                while (argv0[0] == '.' && argv0[1] == '/')
                        argv0 += 2;             /* ./bible: just bible */
                strcat(program_path, argv0);
                return;
        }
#endif
        if (strlen(argv0) < sizeof(program_path))
                strcpy(program_path, argv0);
}

#ifdef UNIXV
/* Purpose: TRUE if a program of this name is in a folder listed in PATH. */
static int on_path(char *name)
{
        char dir[512];
        char *p = getenv("PATH");
        int len;

        while (p != NULL && *p != '\0')
        {
                for (len = 0; p[len] != ':' && p[len] != '\0'; len++)
                        ;
                if (len == 0)                   /* an empty entry: here */
                        strcpy(dir, ".");
                else if (len + 1 < (int) sizeof(dir))
                {
                        strncpy(dir, p, len);
                        dir[len] = '\0';
                }
                else
                        dir[0] = '\0';
                if (dir[0] != '\0' &&
                    strlen(dir) + strlen(name) + 2 <= sizeof(dir))
                {
                        strcat(dir, "/");
                        strcat(dir, name);
                        if (access(dir, X_OK) == 0)
                                return TRUE;
                }
                p += len;
                if (*p == ':')
                        ++p;
        }
        return FALSE;
}

/*
** Purpose: find the translation importer (see importer, above). Done
**          before initialize() changes folder, so that a BIBLEROOT given
**          relative to the current folder works too.
*/
static void find_importer(char *found, int size, char *name)
{
        char *root = getenv("BIBLEROOT");

        found[0] = '\0';
        if (root == NULL || *root == '\0' || !on_path("sh"))
                return;
        if (root[0] != '/' &&
            getcwd(found, size - 2) != NULL)
                strcat(found, "/");
        if (strlen(found) + strlen(root) + strlen(name) + 2 > (size_t) size)
        {
                found[0] = '\0';
                return;
        }
        strcat(found, root);
        strcat(found, "/");
        strcat(found, name);
        if (access(found, R_OK) != 0)
                found[0] = '\0';
}

/*
** Purpose: A)dd a translation, from the main menu. EBaC is replaced by the
**          translation importer, so that none of its memory stays in use
**          while it runs, and the importer starts a new EBaC when it is
**          done: one that reads 'trans' again, so it lists any translation
**          just added (and starts with it). The importer is told how to start
**          EBaC, and which translation this one was showing.
** Returns: only if the importer couldn't be started, after saying so.
*/
static void run_importer(char *script, char *what)
{
        char *args[6];
        int fd, err;

        args[0] = "sh";
        args[1] = script;
        args[2] = "--from-ebac";
        args[3] = (program_path[0] != '\0') ? program_path : "bible";
        args[4] = etla;
        args[5] = NULL;

        cls();
        /* EBaC's open files aren't passed on to the importer, and so to the
           EBaC it starts (this only takes effect if execvp() works) */
        for (fd = 3; fd < 256; fd++)
                fcntl(fd, F_SETFD, FD_CLOEXEC);
        tty_suspend();
        execvp("sh", args);

        err = errno;                            /* still here: it failed */
        tty_resume();
        printf("\nThe %s couldn't be started:\n", what);
        printf("sh %s: %s\n\n", script, strerror(err));
        printf("Press any key to go back to the main menu.\n");
        fflush(stdout);
        scr_mode("anykey");
        ugetchar();
}
#endif

#ifndef EBAC_API        /* (ebac-api has its own: api.c) */
/* Main program! */
int main(int argc, char **argv)
{
#ifdef RAMDISK
	struct TEMPFILE *verse_list;
#else
        long int *verse_list;
#endif
        long int cur_verse;
        struct bcv_type *point_bcv;
        set_program_path(argc > 0 ? argv[0] : NULL);
        if (argc > 1)
                start_trans = argv[1];  /* "bible BSB": start with BSB */
#ifdef UNIXV
        find_importer(importer, sizeof(importer), "add-translation.sh");
        find_importer(gutenberg, sizeof(gutenberg), "add-gutenberg.sh");
#endif
        initialize();
        cls();
        for(;;)
        {
                switch (tolower(main_menu()))
                {
		case 'c':
			change_translations();
			break;
#ifdef UNIXV
		case 'a':
			if (importer[0] != '\0')
				run_importer(importer, "translation importer");
			else
				putchar('\7');
			break;
		case 'p':
			if (gutenberg[0] != '\0')
				run_importer(gutenberg, "Project Gutenberg importer");
			else
				putchar('\7');
			break;
#endif
                case 'v':
                        point_bcv = bcv_menu();
                        if (point_bcv != (struct bcv_type *) NULL)
                        {
                                cur_verse = bcv_search(point_bcv);
/*
** printf("We got: %d %d %d\n",
**  point_bcv->book_number, point_bcv->chapter_number, point_bcv->verse_number);** printf("Becomes: %d\n", cur_verse);
*/
                                view_in_context(cur_verse);
                        }
                        break;
                case 's':
cls();
printf("\n\nSearch line editing keys:\n");
#ifdef ARROWKEYS
printf("Up    or CTRL-g  get previous search   Left  or CTRL-b  back\n");
printf("Down  or CTRL-v  view next search      Right or CTRL-f  forward\n");
#else
printf("CTRL-g  get previous search            CTRL-b  back\n");
printf("CTRL-v  view next search               CTRL-f  forward\n");
#endif
#ifdef USE_NCURSES /* Home, End and Delete are read from terminfo */
printf("Home, CTRL-a or CTRL-x  beginning      End, CTRL-e or CTRL-r  end\n");
printf("CTRL-u  undo line                      Delete or CTRL-d  delete\n");
#else
printf("CTRL-a or CTRL-x  beginning            CTRL-e or CTRL-r  end\n");
printf("CTRL-u  undo line                      CTRL-d  delete\n");
#endif
printf("\nPrefix bookname with \"@\", as in: @Genesis\n");
printf("A group of books with \"=\", as in: =nt  =ot  =gospels  =rom..phile\n");
printf("Words alone are a phrase, as in: in the beginning\n");
printf("Use postfix (reverse Polish) notation to combine words with operators:\n");
printf("   0  and (same verse)     N  within N verses      ,  or\n");
printf("   -  and not             +N  exactly N after      .  phrase: next word\n");
printf("                          -N  exactly N before\n");
printf("                          ~N  exactly N apart\n");
printf("For example, \"love\" and \"hate\" within two verses in Matthew:\n");
printf("        love hate 2 @Mat 0\n\n");
printf("Enter Search word(s) here:\n");
                        verse_list = process_words();
                        if (verse_list == NULL) /* blank, or not valid */
                                break;
/*
** i = 0;
** do{
**     printf("Verse #%d is %d ", i, verse_list[i]);
** }while (verse_list[i++] != 0);
*/
                        view_match_list(verse_list);
#ifdef RAMDISK
			mfclose(verse_list->fname, verse_list->sname);
			unlink(verse_list->sname);
#else
                        free(verse_list);
#endif
                        break;
		case 'i':
			prog_info();
			break;
                case 'e':
                        exit_program(0);
                        break; /* not reached */
                case KEY_REDRAW: /* CTRL-L, or the window was resized */
                        break;   /* (the menu is drawn again) */
                default:
			putchar('\7');
                        break;
                }
        }
}
#endif

