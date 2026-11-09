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
** The importers of translations, shell scripts in BIBLEROOT, each "" if it
** isn't there: thiagobodruk/bible's (add-thiagobodruk.sh), the one for
** scrollmapper/bible_databases (add-translation.sh), and Project Gutenberg's
** (add-gutenberg.sh). The main menu offers A)dd a translation when any is
** there, and A)dd offers each (see add_menu()).
*/
static char thiagobodruk[600] = "";
static char scrollmapper[600] = "";
static char gutenberg[600] = "";
#define ANY_IMPORTER \
        (thiagobodruk[0] != '\0' || scrollmapper[0] != '\0' || gutenberg[0] != '\0')
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
#ifdef UNIXV
static void add_menu P((void));
#endif
static void remove_char P((char *strg));
static int get_number P((void));
static struct bcv_type *bcv_menu P((void));
static int count_lines P((int tag, char *bookname, char *strg, int indent,
                       int top_of_screen));
static void dis_one_line P((char *strg));
static int mostly_rtl P((char *s));
static int mostly_hebrew P((char *s));
static void count_letters P((char *s, int *rtl, int *hebrew, int *ltr));
static void hebrew_number P((int n, char *out));
static void reference P((struct bcv_array_type *bcv_arrays, char *out));
static void print_margin P((char *label, int cols, int right));
static void dis_verse P((char *strg, int indent, int firstTime));
static void dis_part P((char *strg, int indent, int firstTime));
static int count_part P((int tag, char *bookname, char *strg, int indent,
                        int top_of_screen));
static void display_verse P((int tag, struct bcv_array_type *bcv_arrays,
                          int verIndent, int top_of_screen));
static int show_tall_verse P((struct bcv_array_type *bcv_arrays, int shown, int *used));
static int is_tall P((long int v));
static int tall_part_before P((long int v, int skip));
static int lines_from P((long int first, long int last));
static long int fill_back P((long int last));
static long int book_start P((long int v));
static int lines_below P((long int first, long int last));
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

#ifdef UNIXV
/*
** A)dd: where to add a text from (November 2026; the designer's request).
** Each place has an importer of its own, a shell script, run only when it
** is chosen here, each known by its letter: T)hiagobodruk/bible (add-
** thiagobodruk.sh, its XML Bibles, the first since November 8),
** S)crollmapper/bible_databases (add-translation.sh; G for GitHub
** before), both by language, and P)roject Gutenberg, by ebook number
** (add-gutenberg.sh). A new place is another line here and another
** script. Each importer adds, and removes, only its own texts; the short
** name of a text from thiagobodruk/bible ends in "-TB" (KJVA-TB), and of
** one from Project Gutenberg in "-PG" (DRC-PG), so that the same
** translation from each place can be kept side by side.
*/
static void add_menu(void)
{
        int key;

        for (;;)
        {
                cls();
                printf("\n\n");
                underline();
                printf("Add or remove a translation\n");
                underend();
                printf("\nWhere from?\n\n");
                if (thiagobodruk[0] != '\0')
                        printf("T)hiagobodruk/bible on GitHub: Bibles,\n"
                               "   by language (short names end in -TB)\n\n");
                if (scrollmapper[0] != '\0')
                        printf("S)crollmapper/bible_databases on GitHub:\n"
                               "   Bible translations, by language\n\n");
                if (gutenberg[0] != '\0')
                        printf("P)roject Gutenberg: a text, by its ebook\n"
                               "   number (its short name ends in -PG)\n\n");
                printf("X) Back to the main menu\n\n");
                printf("Please press %s%s%sor X now.\n",
                       thiagobodruk[0] != '\0' ? "T (1), " : "",
                       scrollmapper[0] != '\0' ? "S (2), " : "",
                       gutenberg[0] != '\0' ? "P (3), " : "");
                scr_mode("menu");
                key = tolower(screen_key());
                if ((key == 't' || key == '1') && thiagobodruk[0] != '\0')
                {
                        run_importer(thiagobodruk, "thiagobodruk/bible importer");
                        return;         /* (only if it couldn't start) */
                }
                if ((key == 's' || key == '2') && scrollmapper[0] != '\0')
                {
                        run_importer(scrollmapper, "scrollmapper importer");
                        return;
                }
                if ((key == 'p' || key == '3') && gutenberg[0] != '\0')
                {
                        run_importer(gutenberg, "Project Gutenberg importer");
                        return;
                }
                if (key == 'x' || key == 0x1b || key == '\r')
                        return;
                if (key != KEY_REDRAW)
                {
                        putchar('\7');
                        fflush(stdout);
                }
        }
}
#endif

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
        if (ANY_IMPORTER)
                printf("A)dd or remove a translation\n\n");
#endif
        printf("V)iew by:   Bookname Chapter:Verse\n\n");
        printf("S)earch by: Keyword(s)\n\n");
        printf("I)nformation about EBaC\n\n");
        printf("E)xit Program\n\n");
#ifdef UNIXV
        if (ANY_IMPORTER)
                printf("Please press C, A, V, S, I, or E now.\n");
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
** Purpose: is a bookname typed in full ("esther", in lower case) a book's
**          whole name (any case), or with a number for its Roman numeral
**          ("1esdras" for IEsdras)? Not just the start of it, as with
**          name_starts().
*/
static int name_is(char *typed, char *name)
{
        int number = 0, i = 0;
        char *rest = name, lower[MAXWORDLEN];

        strncpy(lower, name, MAXWORDLEN - 1);
        lower[MAXWORDLEN - 1] = '\0';
        u8_lower(lower);
        if (strcmp(lower, typed) == 0)
                return (TRUE);
        while (isdigit(UCHAR(typed[i])) && number < 1000)
                number = number * 10 + (typed[i++] - '0');
        if (i == 0 || number == 0 || roman_start(name, &rest) != number)
                return (FALSE);
        strncpy(lower, rest, MAXWORDLEN - 1);
        lower[MAXWORDLEN - 1] = '\0';
        u8_lower(lower);
        return (strcmp(lower, &typed[i]) == 0);
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

/*
** Purpose: print the names of the books a partial bookname matches, one to
**          a line, as the translation writes them, after "There is more
**          than one book that matches that name" at the V prompt (November
**          9, 2026, the designer's request): so it can be seen what more
**          to type. The books whose names start so; if none do, those
**          whose short names start so (as match_it() matches them).
*/
static void list_book_matches(char *book_str)
{
#ifndef COCO
        extern struct bva *book;
#else
        extern struct bva book[];
#endif
        char typed[MAXWORDLEN];
        int i, shown = 0;

        strncpy(typed, book_str, MAXWORDLEN - 1);
        typed[MAXWORDLEN - 1] = '\0';
        u8_lower(typed);
        for (i = 0; i < numbooks; i++)
                if (name_starts(typed, book[i].name))
                {
                        printf("%s\n", book[i].name);
                        shown++;
                }
#ifndef COCO
        if (shown == 0)
                for (i = 0; i < numbooks; i++)
                        if (book[i].abbr[0] != '\0' &&
                            strncmp(book[i].abbr, typed, strlen(typed)) == 0)
                                printf("%s\n", book[i].name);
#endif
}

#ifndef COCO
/*
** Purpose: the short names' part of match_it() (see there), when the
**          names have matched no book, or several (count): a short name
**          typed exactly is that book; else, if no name matched, those
**          whose short names start so. The new count (as match_it()'s, 1
**          for one book, its index in *found).
*/
static int match_abbr(char *typed, int count, int *found)
{
        extern struct bva *book;
        int i, starts = 0, at = 0, len = (int) strlen(typed);

        for (i = 0; i < numbooks; i++)
                if (book[i].abbr[0] != '\0' && strcmp(book[i].abbr, typed) == 0)
                {
                        *found = i;
                        return (1);
                }
        if (count > 0)
                return (count);         /* (the names' several, as before) */
        for (i = 0; i < numbooks; i++)
                if (book[i].abbr[0] != '\0' && strncmp(book[i].abbr, typed, len) == 0)
                {
                        starts++;
                        at = i;
                }
        if (starts == 1)
                *found = at;
        return (starts);
}
#endif

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

            A book's whole name, typed in full, is that book, even where it
            is also the start of others' names (November 9, 2026, the
            designer's request): "esther" is Esther, though EstherGreek
            begins so too, which "estherg" then chooses. Without this,
            Esther could never be chosen: a space or Enter ends the name,
            so there is nothing more to type. (No name in the AV is the
            start of another's.)

            A number typed at the start also matches a Roman numeral of the
            same value at the start of a name, as the translation writes it:
            "2th" matches IIThessalonians, and "3" IIIJohn. (So the same
            typing works in a translation with "2Thessalonians" and one
            with "IIThessalonians".) Only that way round: "i" matches only
            an I, so in the AV "is" is still Isaiah alone.

            A book's short name in the translation's source (its
            abbreviations file: thiagobodruk/bible's "gn" for Genesis;
            November 8, 2026, the designer's request) is matched too, after
            its name: typed exactly, it chooses that book even where the
            name's start fits several ("jo": John, not Joshua, Job, Joel or
            Jonah; "jos" is still Joshua); where nothing starts so, the
            start of a short name ("1s": 1sm, 1Samuel). So a book can be
            typed by its short name anywhere a name is typed (the V prompt,
            @ in a search, a reference), as it is typed.
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
                if (count > 1)          /* (one of them typed in full?) */
                        for (i = 0; i < numbooks; i++)
                                if (name_is(book_str, book[i].name))
                                {
                                        count = 1;
                                        savei = i;
                                        break;
                                }
#ifndef COCO
                if (count != 1)
                        count = match_abbr(book_str, count, &savei);
#endif
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
                list_book_matches(book_str);    /* (which ones) */
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

/*
** A tab in a verse's text starts a new line (November 2026): a text from
** Project Gutenberg keeps a note on a verse, or a heading before it, in the
** verse, on a line of its own (see split_paragraphs() in add-gutenberg.sh).
** So a verse is shown, and counted, a part at a time, each part after a tab
** starting a line (an empty part, a blank line), each wrapped as before.
**
** A part that is a note on the verse (commentary, not the verse's text)
** starts with a "-" alone after its tab (November 5, 2026): its first line
** has "---" in the margin, where a verse's number goes, in place of the
** "-", so that someone reading aloud sees where the verse ends (see
** "Notes" in readme.architecture). The index skips the "-" as it does the
** tab (a word without letters), and ebac-api leaves notes out of what it
** speaks (plain_text() in api.c).
*/

/* Purpose: the start of the part after a tab: past the spaces around it,
            and past the "-" of a note, if it is one (*note TRUE) */
static char *next_part(char *tab, int *note)
{
        char *p;

        for (p = tab + 1; *p == ' '; p++)
                ;
        *note = (p[0] == '-' && (p[1] == ' ' || p[1] == '\t' || p[1] == '\0'));
        if (*note)
                for (p++; *p == ' '; p++)
                        ;
        return (p);
}

/* figure out how many lines on the screen it will take to print this verse */
static int count_lines(int tag, char *bookname, char *strg, int indent, int top_of_screen)
{
        char *tab, *end, kept;
        int n, note;

        if ((tab = strchr(strg, '\t')) == NULL)
                return (count_part(tag, bookname, strg, indent, top_of_screen));
        for (end = tab; end > strg && end[-1] == ' '; end--)
                ;       /* (the spaces around a tab aren't shown) */
        kept = *end;
        *end = '\0';
        n = count_part(tag, bookname, strg, indent, top_of_screen);
        *end = kept;
        strg = next_part(tab, &note);
        return (n + count_lines(tag, bookname, strg, indent, FALSE));
}

/* the lines one part of a verse takes (its book's heading too, if it is
   shown before it) */
static int count_part(int tag, char *bookname, char *strg, int indent, int top_of_screen)
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
   return(count_part(tag, bookname, &strg[i + 1],  /* recursive! */
                      indent, FALSE) + 1 + a_lines);
 return(count_part(tag, bookname, &strg[fit], /* here too! */
                    indent, FALSE) + 1 + a_lines);
}

/* Purpose: Display one line of text to the screen. Highlight, via reverse
            video, the word being searched for. (The line isn't ended: a
            margin on the right may follow it; see verse_line().)
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
** dv_note: the next line is a note's first, with "---" in its margin.
**
** The margin: the 8 columns beside a verse's lines, at the end of their
** indent. dv_label: what the first line's margin shows, the verse's
** reference (NULL: shown already, by display_verse(), in a list).
** dv_right: the margin on the right, where a verse written right to left
** (Hebrew) begins: its reference first, read that way, as Hebrew Bibles
** have it. (In a list it stays on the left, under the pick letters.)
*/
static int dv_skip = 0, dv_left = -1, dv_body_only = FALSE, dv_note = FALSE;
static char *dv_label = NULL;
static int dv_right = FALSE;

/* Purpose: one line of a verse, as dis_verse() wraps it, with its margin
   (see above); or left out (see above) */
static void verse_line(char *strg, int indent, int firstTime)
{
        int note = dv_note;
        int n = (int) strlen(strg), pad;
        char *margin, kept;

        dv_note = FALSE;        /* (only a note's first line) */
        if (dv_skip > 0)
        {
                dv_skip--;
                return;
        }
        if (dv_left == 0)
                return;
        if (dv_left > 0)
                dv_left--;
        /* what the margin shows: the reference on the verse's first line,
           "---" on a note's first, else nothing */
        if (firstTime && !dv_body_only)
                margin = dv_label;
        else
                margin = note ? "---" : "";
        /* (a list's indent: the pick letter and book, then the margin) */
        if (margin != NULL && !dv_right)
        {
                printf("%s", spaces + MAXINDENT - (indent - 8));
                print_margin(margin, 8, FALSE);
        }
        if (mostly_rtl(strg))
        {
                /* right to left: its start at the end of the line (before a
                   margin on the right), and the spaces before it, on the
                   left, where its end is. (Terminals are assumed to take a
                   full line without wrapping, as modern ones do. Spaces
                   after its text, as a verse's last line can have, are
                   left out.) */
                while (n > 0 && strg[n - 1] == ' ')
                        n--;
                pad = s_cols - indent - u8_cols(strg, n);
                while (pad-- > 0)
                        putchar(' ');
        }
        kept = strg[n];
        strg[n] = '\0';
        dis_one_line(strg);
        strg[n] = kept;
        if (dv_right)
                print_margin(margin, indent, TRUE);
        putchar('\n');
}

/* Purpose: a margin of cols columns with label in it: at its left, or (on
   the right) at its right, the screen's edge */
static void print_margin(char *label, int cols, int right)
{
        int pad = cols - u8_cols(label, (int) strlen(label));

        if (right)
                while (pad-- > 0)
                        putchar(' ');
        print_visual(label, (char *) NULL);
        if (!right)
                while (pad-- > 0)
                        putchar(' ');
}

/*
** Purpose: a verse's reference, as its margin shows it: "1:13"; for a
**          verse in Hebrew, in Hebrew letters, as Hebrew Bibles number
**          them (alef:yod-gimel for 1:13, chapter first, read right to left).
*/
static void reference(struct bcv_array_type *bcv_arrays, char *out)
{
        char *chapter = bcv_arrays->chapt_array, *verse = bcv_arrays->verse_array;

        if (mostly_hebrew(bcv_arrays->data_array))
        {
                hebrew_number(atoi(chapter), out);
                strcat(out, ":");
                hebrew_number(atoi(verse), out + strlen(out));
        }
        else
                sprintf(out, "%s:%s", chapter, verse);
}

/*
** Purpose: n (0 to 999) in Hebrew letters, as Hebrew Bibles number
**          chapters and verses: hundreds, tens, then ones, each a letter
**          worth that much (176: qof ayin vav, 100 70 6). 15 and 16 are
**          9 and 6, 9 and 7 (tet vav, tet zayin), not 10 and 5, 10 and 6,
**          which spell the divine name. 0, which has no letter (verse 0,
**          the text before verse 1: see index.c), stays 0. In UTF-8: a
**          letter is 0xD7 and then 0x90 and its place in the alphabet
**          (final forms counted).
*/
static void hebrew_number(int n, char *out)
{
        static char ones[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8 };      /* alef .. tet */
        static char tens[] = { 9, 11, 12, 14, 16, 17, 18, 20, 22 };
                                        /* yod kaf lamed mem nun samekh ayin pe tsadi */
        static char hundreds[] = { 23, 24, 25, 26 };    /* qof resh shin tav */
        int k = 0;

        if (n == 0)
        {
                strcpy(out, "0");
                return;
        }
#define LETTER(place) (out[k++] = (char) 0xD7, out[k++] = (char) (0x90 + (place)))
        while (n >= 400)
        {
                LETTER(hundreds[3]);
                n -= 400;
        }
        if (n >= 100)
        {
                LETTER(hundreds[n / 100 - 1]);
                n %= 100;
        }
        if (n == 15 || n == 16)
        {
                LETTER(ones[8]);
                LETTER(ones[n - 10]);
                n = 0;
        }
        if (n >= 10)
        {
                LETTER(tens[n / 10 - 1]);
                n %= 10;
        }
        if (n >= 1)
                LETTER(ones[n - 1]);
#undef LETTER
        out[k] = '\0';
}

/*
** Purpose: whether a line of a verse is mostly right to left (Hebrew,
**          Arabic, any script u8_is_rtl() says is written right to left,
**          the same test that turns it about), so is aligned on the right:
**          more of its letters are right to left than left to right.
**          (Digits, which Hebrew text has among its letters, spaces and
**          punctuation don't count either way; a line half and half stays
**          on the left.)
*/
static int mostly_rtl(char *s)
{
        int rtl, hebrew, ltr;

        count_letters(s, &rtl, &hebrew, &ltr);
        return (rtl > ltr);
}

/* Purpose: whether a verse is mostly in Hebrew letters (so numbered in them:
            see reference()) */
static int mostly_hebrew(char *s)
{
        int rtl, hebrew, ltr;

        count_letters(s, &rtl, &hebrew, &ltr);
        return (hebrew > ltr + (rtl - hebrew));
}

/* Purpose: how many of s's letters are written right to left, how many of
            those are Hebrew (U+0590 to U+05FF), and how many left to right
            (marks, as vowel points, aren't letters; see mostly_rtl()) */
static void count_letters(char *s, int *rtl, int *hebrew, int *ltr)
{
        int i = 0;
        long code;

        *rtl = *hebrew = *ltr = 0;
        while (s[i] != '\0')
        {
                if (u8_width(&s[i]) == 0)
                        ;       /* (a mark, as a vowel point: not a letter) */
                else if (u8_is_rtl(&s[i]))
                {
                        (*rtl)++;
                        code = u8_code(&s[i]);
                        if (code >= 0x0590L && code <= 0x05FFL)
                                (*hebrew)++;
                }
                else if (u8_is_letter(&s[i]) && !isdigit(UCHAR(s[i])))
                        (*ltr)++;
                i += u8_len(&s[i]);
        }
}

/* Purpose: show a verse's text, a part at a time: each part after a tab on
            a line of its own (an empty one, a blank line; see count_lines()) */
static void dis_verse(char *strg, int indent, int firstTime)
{
        char *tab, *end, kept = '\0';

        for (;;)
        {
                end = NULL;
                if ((tab = strchr(strg, '\t')) != NULL)
                {
                        for (end = tab; end > strg && end[-1] == ' '; end--)
                                ;       /* (the spaces around it aren't shown) */
                        kept = *end;
                        *end = '\0';
                }
                if (strg[0] == '\0' && !firstTime)
                        verse_line(strg, indent, FALSE);        /* (blank) */
                else
                        dis_part(strg, indent, firstTime);
                if (tab == NULL)
                        return;
                *end = kept;
                strg = next_part(tab, &dv_note);
                firstTime = FALSE;
        }
}

/* Purpose: performs word wrapping by inserting a '\0' at appropriate location
            and calls dis_one_line to print one line of the verse, up to the
            '\0'. Then it recursively calls itself to handle the remainder
            of the verse.
*/
static void dis_part(char *strg, int indent, int firstTime)
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
                        dis_part(&strg[i + 1], indent, FALSE);
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
        dis_part(&strg[i], indent, FALSE);
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
        static char previous_book[MAXBOOKNAME] = { '\0' };
        static char label[40];  /* the reference: dv_label (see verse_line()) */

        reference(bcv_arrays, label);
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
        /* the reference, in the margin of the verse's first line: on the
           left in a list, after the book (shown here); in a passage,
           where verse_line() puts it, on the right for a verse written
           right to left. Verse 0 or chapter 0, the text before verse 1 or
           chapter 1 (see index.c), is plain text in a passage, without
           the 0, which isn't in the source (in a list, its reference is
           shown, as any verse's). */
        if (tag != -1)
        {
                print_margin(label, 8, FALSE);
                dv_label = NULL;
                dv_right = FALSE;
        }
        else
        {
                if (strcmp(bcv_arrays->chapt_array, "0") == 0 ||
                    strcmp(bcv_arrays->verse_array, "0") == 0)
                        label[0] = '\0';
                dv_label = label;
                dv_right = mostly_rtl(bcv_arrays->data_array);
        }
        dis_verse(bcv_arrays->data_array, verIndent, TRUE);
        strcpy(previous_book, bcv_arrays->book_array);
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
        long int verse = last_shown;

        while (verse > 1 && lines_below(verse - 1, last_shown) <= s_lines - 1)
                verse--;
        return verse;
}

/*
** Purpose: the lines verses first to last take below others, as '.' adds
**          them to a screen: a book's heading (and the blank line before
**          it) only where the book changes, from the verse before first.
**          (Counted from first on, in order, as they are drawn: so that a
**          heading is counted with the verse it is above.)
*/
static int lines_below(long int first, long int last)
{
        int lines = 0;
        long int v;
        struct bcv_array_type *bcv_arrays;

        /* count_lines() compares each verse's book with the one before */
        bcv_arrays = get_verse(first > 1 ? first - 1 : first);
        count_lines(-1, bcv_arrays->book_array, "", 8, FALSE);
        for (v = first; v <= last; v++)
        {
                bcv_arrays = get_verse(v);
                lines += count_lines(-1, bcv_arrays->book_array,
                                     bcv_arrays->data_array, 8, FALSE);
        }
        return (lines);
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
** Purpose: show a part of a verse taller than the screen (as a long
**          introduction can be), at the top of the screen: from line shown
**          of its text, as much as fits. Returns the lines of its text
**          shown so far (shown and this part's), or 0 if this was its last
**          part; and in *used, the lines of the screen this part took.
*/
static int show_tall_verse(struct bcv_array_type *bcv_arrays, int shown, int *used)
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
        *used = part + (shown == 0 ? total - text_lines : 0);
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

/* Purpose: whether verse v is taller than the screen, alone at its top */
static int is_tall(long int v)
{
        struct bcv_array_type *bcv_arrays;

        if (!check_limits(v))
                return (FALSE);
        bcv_arrays = get_verse(v);
        return (count_lines(-1, bcv_arrays->book_array, bcv_arrays->data_array,
                            8, TRUE) > s_lines - 1);
}

/*
** Purpose: the parts of tall verse v: where (lines of its text before it)
**          the part before the one at line skip starts: a screenful
**          ending just before it; with skip -1, where its last part
**          starts. (The first part is shorter: the book's heading is above
**          it.)
*/
static int tall_part_before(long int v, int skip)
{
        struct bcv_array_type *bcv_arrays = get_verse(v);
        int total, text_lines, avail = s_lines - 1, start = 0, part;

        total = count_lines(-1, bcv_arrays->book_array, bcv_arrays->data_array, 8, TRUE);
        text_lines = count_lines(-1, "", bcv_arrays->data_array, 8, FALSE);
        part = avail - (total - text_lines);
        if (part < 1)
                part = 1;
        if (skip >= 0)
        {
                /* the screenful ending just before line skip (its parts
                   needn't start where they would from its first part at a
                   screen's top: its start may have been shown below other
                   verses); from its start if that is shown with it */
                if (skip <= part)
                        return (0);
                start = skip - avail;
                return (start < 1 ? 1 : start);
        }
        skip = text_lines;
        while (start + part < skip)
        {
                start += part;
                part = avail;
        }
        return (start);
}

/* Purpose: the lines verses first to last take, drawn from the top of a
   screen (as view_in_context() draws them) */
static int lines_from(long int first, long int last)
{
        int lines = 0;
        long int v;
        struct bcv_array_type *bcv_arrays;

        for (v = first; v <= last; v++)
        {
                bcv_arrays = get_verse(v);
                lines += count_lines(-1, bcv_arrays->book_array,
                                     bcv_arrays->data_array, 8, v == first);
        }
        return (lines);
}

/*
** Purpose: the first verse of a screen of whole verses ending with verse
**          last (for p): as many before it as fit, but not a verse taller
**          than the screen, which is shown alone.
*/
static long int fill_back(long int last)
{
        long int first = last;

        while (first > 1 && !is_tall(first - 1) &&
               lines_from(first - 1, last) <= s_lines - 1)
                first--;
        return (first);
}

/* Purpose: the first verse of the book verse v is in */
static long int book_start(long int v)
{
        while (v > 1 && verse_to_book(v - 1) == verse_to_book(v))
                v--;
        return (v);
}

/*
** The passage screen. A screen shows whole verses from top, the verse at
** its top, as many as fit, and below them, if the next is taller than the
** screen, as much of its start as fits; or, of a verse taller than the
** screen at its top, one part of it, from line skip of its text: alone, or,
** its last part, with the verses after it below it, as many as fit (the
** same rule: stop only at one that doesn't fit). After drawing, next_top and
** next_skip say what the next screen starts with (n), and last_shown is
** the last verse on it (its last part, or not). '.' adds a verse at the
** bottom, scrolling the screen (scrolled): the verse at its top is then
** the first still wholly on it.
**
** The keys, each from the verse at the top (the only exception: at the
** first or last verse of the translation, there is nowhere to go, and it
** beeps):
**   n, p       the next screen, the previous (a tall verse: its next part,
**              or its part before)
**   . ,        the next verse, at the bottom (a tall verse: on a screen of
**              its own; within one, its next part); the previous verse, at
**              the top (within a tall verse, its part before)
**   f, b       the first verse of the next book; of this book, or, from its
**              first, of the book before
*/
static void view_in_context(long int cur_verse)
{
        long int top, next_top = 0, last_shown = 0, start;
        int skip = 0, next_skip = 0, num_lines, scrolled = FALSE;
        int n, text_lines, part, head_shown = FALSE;
        char exit_now = FALSE, top_of_screen;
        struct bcv_array_type *bcv_arrays;

        top = cur_verse;
        if (top < 1)
                top = 1;
        if (top > numverses)
                top = numverses;
        do{
                cls(); /* may update s_lines if the window was resized */
                scrolled = FALSE;
                next_skip = 0;
                head_shown = FALSE;
                num_lines = s_lines;
                top_of_screen = TRUE;
                cur_verse = top;
                if (skip > 0 || is_tall(top))
                {
                        /* taller than the screen: a part of it; its last
                           part, with the verses after it below it */
                        bcv_arrays = get_verse(top);
                        next_skip = show_tall_verse(bcv_arrays, skip, &n);
                        next_top = top;
                        last_shown = top;
                        top_of_screen = FALSE;
                        if (next_skip > 0)
                                head_shown = TRUE;      /* (no room: alone) */
                        else
                        {
                                num_lines -= n;
                                cur_verse = top + 1;
                                /* (count_lines() compares each verse's book
                                   with the one before: this one's) */
                                count_lines(-1, bcv_arrays->book_array, "", 8, FALSE);
                        }
                }
                if (!head_shown)
                {
                        for(;;)
                        {
                            if (!check_limits(cur_verse))
                                break;          /* the end: no more */
                            bcv_arrays = get_verse(cur_verse);
                            n = count_lines(-1, bcv_arrays->book_array,
                               bcv_arrays->data_array, 8, top_of_screen);
                            if (num_lines - n < 1)
                            {
                                /* A verse taller than the screen below the
                                   others: its start, in the room left (at
                                   least two lines of it), and the rest on
                                   the next screens, so that a screen isn't
                                   left near empty before it */
                                text_lines = count_lines(-1, "", bcv_arrays->data_array, 8, FALSE);
                                part = (num_lines - 1) - (n - text_lines);
                                if (!top_of_screen && part >= 2 && is_tall(cur_verse))
                                {
                                    bcv_arrays = get_verse(cur_verse);
                                    dv_left = part;
                                    display_verse(-1, bcv_arrays, 8, FALSE);
                                    dv_left = -1;
                                    next_skip = part;
                                    last_shown = cur_verse;
                                    next_top = cur_verse;
                                    head_shown = TRUE;
                                }
                                break;
                            }
                            num_lines -= n;
                            display_verse(-1, bcv_arrays, 8, top_of_screen);
                            top_of_screen = FALSE;
                            cur_verse++;
                        }
                        if (!head_shown)
                        {
                            next_top = cur_verse;   /* (the one that didn't fit) */
                            last_shown = cur_verse - 1;
                        }
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
                        if (scrolled)
                                top = scrolled_top(last_shown), skip = 0;
                        start = top;
                        while (start <= numverses &&
                               verse_to_book(start) == verse_to_book(top))
                                start++;
                        if (start > numverses)  /* in the last book */
                        {
                                putchar('\7');     /* beep */
                                fflush(stdout);
                                goto get_command;
                        }
                        top = start;
                        skip = 0;
                        break;
                case 'b':  /* the first verse of this book, or, from there,
                              of the book before */
                        if (scrolled)
                                top = scrolled_top(last_shown), skip = 0;
                        start = book_start(top);
                        if (start == top && skip == 0)
                        {
                                if (top <= 1)   /* at the first verse */
                                {
                                        putchar('\7');     /* beep */
                                        fflush(stdout);
                                        goto get_command;
                                }
                                start = book_start(top - 1);
                        }
                        top = start;
                        skip = 0;
                        break;
                case 'n':
                case ' ':
                case 0x16: /* Page Down on MM/1 */
                        if (next_top > numverses)  /* the end is showing */
                        {
                                putchar('\7');     /* beep */
                                fflush(stdout);
                                goto get_command;
                        }
                        top = next_top;
                        skip = next_skip;
                        break;
                case '.':
                case '>':
                case 0xe: /* Down Arrow on MM/1 */
                        if (next_skip > 0)      /* (within a tall verse: on in it) */
                        {
                                top = next_top;
                                skip = next_skip;
                                break;
                        }
                        if (!check_limits(last_shown + 1))
                        {
                                printf("Error!  Can't advance one verse.");
                                printf(" You are at the END.\n");
#                               ifdef VMS
                                        putchar('\r');
#                               endif
                                goto get_command;
                        }
                        if (is_tall(last_shown + 1))
                        {
                                /* (too tall to scroll in: a screen of its own) */
                                top = last_shown + 1;
                                skip = 0;
                                break;
                        }
                        /* the prompt out first, all of it (the verse's
                           first line may be shorter: an empty one is) */
                        printf("                  \b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b");
                        bcv_arrays = get_verse(++last_shown);
                        display_verse(-1, bcv_arrays, 8, top_of_screen);
                        top_of_screen = FALSE;
                        next_top = last_shown + 1;
                        next_skip = 0;
                        scrolled = TRUE;
                        goto get_command;
                case 'p':
                case 0x1a: /* Page Up on MM/1 */
                        if (scrolled)
                                top = scrolled_top(last_shown), skip = 0;
                        if (skip > 0)   /* (within a tall verse: its part before) */
                        {
                                skip = tall_part_before(top, skip);
                                break;
                        }
                        if (top <= 1)   /* at the first verse */
                        {
                                putchar('\7');     /* beep */
                                fflush(stdout);
                                goto get_command;
                        }
                        if (is_tall(top - 1))
                        {
                                top--;
                                skip = tall_part_before(top, -1);  /* its last part */
                                break;
                        }
                        top = fill_back(top - 1);
                        skip = 0;
                        break;
                case ',':
                case '<':
                case 0x10: /* Up Arrow on MM/1 */
                        if (scrolled)
                                top = scrolled_top(last_shown), skip = 0;
                        if (skip > 0)   /* (within a tall verse: its part before) */
                        {
                                skip = tall_part_before(top, skip);
                                break;
                        }
                        if (top <= 1)   /* at the first verse */
                        {
                                putchar('\7');     /* beep */
                                fflush(stdout);
                                goto get_command;
                        }
                        top--;
                        skip = 0;
                        break;
                case 'x':
                case 0x1b: /* ESC */
                        exit_now = TRUE;
                        break;
                case KEY_REDRAW: /* CTRL-L, or the window was resized */
/*
** Draw this screen again from the same verse at the top, for the window's
** new size. After stepping with '.' the screen has scrolled: the verse at
** its top is the first still wholly on it, found with the size the screen
** was drawn for (the new size is only picked up by the cls() that redraws
** it). A tall verse's part starts again from its first.
*/
                        if (scrolled)
                                top = scrolled_top(last_shown);
                        skip = 0;
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
                        if (scrolled)
                                top = scrolled_top(last_shown), skip = 0;
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
                     {
                        /* an entry taller than the screen, just before
                           this screen: a screen of its own (cut to fit,
                           as n shows it), not none */
                        if (n == 1)
                           first_i--;
                        break;
                     }
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
        find_importer(thiagobodruk, sizeof(thiagobodruk), "add-thiagobodruk.sh");
        find_importer(scrollmapper, sizeof(scrollmapper), "add-translation.sh");
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
			if (ANY_IMPORTER)
				add_menu();
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

