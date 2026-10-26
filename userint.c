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
#define EBAC_RELEASE "2.1"
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
static void find_importer P((void));
static void run_importer P((void));
#endif
static int tag_label P((int tag));
static int key_to_tag P((int key));
static int screen_key P((void));

static int main_menu(void)
{
        cls();
        printf("\n\n");
        printf("Welcome to EBaC, Electronic Bible and Concordance\n");
        printf("Selected translation: %s\n", etla);
        underline();
        printf("\nMain Menu\n");
        underend();
        /* a blank line between the choices, so each is easy to tap on a
           touch screen (see gestures.js in the Home Assistant app) */
        printf("C)hange to a different translation\n\n");
#ifdef UNIXV
        if (importer[0] != '\0')
                printf("A)dd or remove a translation\n\n");
#endif
        printf("V)iew by:   Bookname Chapter:Verse\n\n");
        printf("S)earch by: Keyword(s)\n\n");
        printf("I)nformation about EBaC\n\n");
        printf("E)xit Program\n\n");
#ifdef UNIXV
        if (importer[0] != '\0')
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
*/
int match_it(char *book_str,
             int update_screen)          /* boolean */
{
#ifndef COCO
        extern struct bva *book;
#else
        extern struct bva book[];
#endif
        int i, j, savei = 0, len_str;
        int count = 0;  /* Number of matches found */
        char lower_name[MAXWORDLEN];

	if (update_screen)
        	Erase_word(last_len);

        if (book_str[0] == '\0')
                count = numbooks;
        else
        {
                for (i = 0; i < MAXWORDLEN && book_str[i] != '\0'; i++)
                        book_str[i] = tolower(UCHAR(book_str[i]));

                for (i = 0; i < numbooks; i++)
                {
                        len_str = strlen(book[i].name);
                        for (j = 0; j <= len_str; j++)
                                lower_name[j] = tolower(UCHAR(book[i].name[j]));
/*
**printf("%s--%s--%d--%s\n", book[i].name, lower_name, len_str, book_str);
*/
                        if (!(strncmp(lower_name, book_str, strlen(book_str))))
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
                  last_len = strlen(book_str);
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
                  last_len = strlen(book[savei].name);
                  printf("%s", book[savei].name); /* Print entire bookname */
                  fflush(stdout);
		}
                return (savei + 1);
        }
        else /* count > 1 */
        {
		if (update_screen)
		{
                  last_len = strlen(book_str);
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
        int i;
        if (strg[0] != '\0')
        {
                for (i = 1; i < MAXWORDLEN; i++)
                {
                        if (strg[i] == '\0')
                        {
                                strg[i - 1] = '\0';
                                break;
                        }
                }
        }
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
        int c, i;

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
                if (isalnum(c))
                {
                        for (i = 0; i < (MAXWORDLEN - 1); i++)
                        {
                                if (book_str[i] == '\0')
                                {
                                        book_str[i] = (char) c;
                                        book_str[i + 1] = '\0';
                                        break;
                                }
                        }
                        if (i == (MAXWORDLEN - 1))
                        {
                            printf("\nOut of room in string to store word!\n");
                            exit_program(1);
                        }
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

/* figure out how many lines on the screen it will take to print this verse */
static int count_lines(int tag, char *bookname, char *strg, int indent, int top_of_screen)
{
        static char previous_book[MAXBOOKNAME] = { '\0' };
        int a_lines = 0, i, indentWidth = WIDTH - indent;

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
        if ((int) strlen(strg) <= indentWidth)
                return(1 + a_lines);
 for (i = indentWidth; i > 0; i--)
  if (strg[i] == ' ')
   return(count_lines(tag, bookname, &strg[i + 1],  /* recursive! */
                      indent, FALSE) + 1 + a_lines);
 return(count_lines(tag, bookname, &strg[indentWidth], /* here too! */
                    indent, FALSE) + 1 + a_lines);
}

/* Purpose: Display one line of text to the screen. Highlight, via reverse
            video, the word being searched for.
*/
static void dis_one_line(char *strg)
{
    int i = 0, j = 0, k;
    char wrd[MAXWORDLEN];
    char lower_wrd[MAXWORDLEN];
    extern char *rev_word[];

    for(;;)
    {
        while (!(isalnum(UCHAR(strg[i]))))
        {
                if (strg[i] == '\0') /* We is done with this line */
                {
/* What did this line used to say? Was it a printf("\n")  ??? */
                        putchar('\n');
                        return;
                }
                putchar(strg[i++]); /* prints commas, spaces, etc normally */
        }

        while (isalnum(UCHAR(strg[i])) ||
                      (strg[i] == '\'') || /* allow "day's" to be highlighted */
							 (strg[i] == '-'))    /* same with "father-in-law" */
        {
                wrd[j] = strg[i++];
                lower_wrd[j] = tolower(UCHAR(wrd[j]));
                ++j;
        }

        lower_wrd[j] = wrd[j] = '\0';
        j = 0;
/*
** We need to loop here to see if lower_wrd is the same as ANY of the multiple
** words used in the searching command.
*/
        k = -1;
        while (rev_word[++k] != 0)
        {
	/* printf("Comparing '%s' with '%s'\n", lower_wrd, rev_word[k]); */
                if (strcmp (lower_wrd, rev_word[k]) == 0)
                {
                        rev_printf(wrd);
                        break;
                }
        }
        if (rev_word[k] == 0)
                printf("%s", wrd);
    }
}

/* Purpose: performs word wrapping by inserting a '\0' at appropriate location
            and calls dis_one_line to print one line of the verse, up to the
            '\0'. Then it recursively calls itself to handle the remainder
            of the verse.
*/
static void dis_verse(char *strg, int indent, int firstTime)
{
        int i, indentWidth = WIDTH - indent;
        char *outstr;

        if ((int) strlen(strg) <= indentWidth)
        {
                if ((strg[0] == '\0') && (firstTime == FALSE))
                        return;
                if (firstTime == FALSE)
                        printf("%s", spaces + MAXINDENT - indent);
                dis_one_line(strg);
                return;
        }
        for (i = indentWidth; i > 0; i--)
        {
                if (strg[i] == ' ')
                {
                        if (firstTime == FALSE)
                                printf("%s", spaces + MAXINDENT - indent);
                        strg[i] = '\0';
                        dis_one_line(strg);
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

        if (firstTime == FALSE)
                printf("%s", spaces + MAXINDENT - indent);
        for (i = 0; i < indentWidth; i++)
                outstr[i] = strg[i];
        outstr[i] = '\0';
        dis_one_line(outstr);
        free(outstr);
        dis_verse(&strg[i], indent, FALSE);
        return;
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
                  printf("%s", spaces + MAXINDENT + strlen(bcv_arrays->book_array) + 1 - 15);
                  printf("%c ", bcv_arrays->book_array[0]);
                  printf("%s ", &bcv_arrays->book_array[1]);
               }
				   else
					{
                  printf("%s", spaces + MAXINDENT + strlen(bcv_arrays->book_array) - 15);
                  printf("%s ", bcv_arrays->book_array);
               }
            }
            else
               printf("%s", spaces + MAXINDENT - 16);
        }
        else
        {
           if (bcv_arrays->book_array[0] != '\0' &&
              (top_of_screen || strcmp(bcv_arrays->book_array, previous_book)))
           {
              if (top_of_screen == FALSE)
                 printf("\n");
              printf("                            ");
              underline();
				  if (isdigit (UCHAR(bcv_arrays->book_array[0])) &&
					   isupper (UCHAR(bcv_arrays->book_array[1])) &&
					   islower (UCHAR(bcv_arrays->book_array[2])) )
              {
                 printf("%c ", bcv_arrays->book_array[0]);
                 printf("%s", &bcv_arrays->book_array[1]);
              }
				  else
                 printf("%s", bcv_arrays->book_array);
              underend();
              printf(" (%s)\n", etla);
           }
        }
        printf("%s:", bcv_arrays->chapt_array);
        printf("%s", bcv_arrays->verse_array);
        cv_length = strlen(bcv_arrays->chapt_array);
        cv_length++;  /* Count the colon */
        cv_length += strlen(bcv_arrays->verse_array);
        printf("%s", spaces + cv_length + MAXINDENT - 8);
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
        fputs(string, stdout);
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
static void view_in_context(long int cur_verse)
{
        long int first_verse, start;
        int num_lines;
        char exit_now = FALSE, top_of_screen;
        struct bcv_array_type *bcv_arrays;

        if (cur_verse < 1)
                cur_verse = 1;
        if (cur_verse > numverses)
                cur_verse = numverses;
        do{
                cls(); /* may update s_lines if the window was resized */
                num_lines = s_lines;
                first_verse = cur_verse; /* a legal value */
                top_of_screen = TRUE;
                for(;;)
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
** verse on the next screen
*/
                        cur_verse--;
                        break;
                case '.':
                case '>':
                case 0xe: /* Down Arrow on MM/1 */
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
                                        2 + strlen(count_word) + strlen(times))
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
                    num_lines -= count_lines(tag, bcv_arrays->book_array,
                             bcv_arrays->data_array, MAXINDENT, top_of_screen);
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

static void prog_info(void)
{
cls();
printf("Electronic Bible and Concordance (EBaC)      version %s\n", EBAC_RELEASE);
printf("     Copyright (C) 2004 by Neil G. Brookins\n");
if (EBAC_COMMIT[0] != '\0')
        printf("     Built from commit %s\n", EBAC_COMMIT);
printf("\n");
printf("The following translations are available for use with EBAC:\n");
printf("      King James (Authorized) Version\n");
printf("      New International Version\n");
printf("      New Revised Standard Version\n");
printf("      Revised Standard Version\n");
printf("      Webster's Bible\n");
printf("      Darby Bible\n");
printf("      Young's Literal Translation\n");
printf("\n");
printf("For technical help, and information:\n");
printf("      Neil G. Brookins\n");
printf("      http://ebac.sourceforge.net/\n");
printf("\n");
printf("(Press any Key) ");
fflush(stdout);
scr_mode("anykey");
ugetchar();
cls();
printf("EBaC currently supports the following types of computers:\n");
printf("      MM/1, System V, TC-70  (OS-9 68000)\n");
printf("      Digital VAX            (OpenVMS)\n");
printf("      Tandy Color Computer 3 (OS-9 6809)\n");
printf("      AT&T 3B1               (UNIX System V)\n");
/* "Windows" was Cygwin, with which EBaC 1.3.0 was used and tested at
   length on Windows; today, Windows 10 and 11 run it in WSL (Linux) */
printf("      IBM-PC and compatibles (MS-DOS, Windows with Cygwin)\n");
printf("      Sun                    (SunOS 4.x, Solaris 2.x)\n");
printf("      DEC                    (Ultrix, OSF/1)\n");
printf("      HP 9000                (HP-UX 10.x)\n");
printf("      PCs and Raspberry Pi   (Linux, with ncurses)\n");
printf("      Windows 10 and 11      (WSL, or Cygwin)\n");
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
static void find_importer(void)
{
        char *root = getenv("BIBLEROOT");
        static char name[] = "/add-translation.sh";

        importer[0] = '\0';
        if (root == NULL || *root == '\0' || !on_path("sh"))
                return;
        if (root[0] != '/' &&
            getcwd(importer, sizeof(importer) - 2) != NULL)
                strcat(importer, "/");
        if (strlen(importer) + strlen(root) + sizeof(name) > sizeof(importer))
        {
                importer[0] = '\0';
                return;
        }
        strcat(importer, root);
        strcat(importer, name);
        if (access(importer, R_OK) != 0)
                importer[0] = '\0';
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
static void run_importer(void)
{
        char *args[6];
        int fd, err;

        args[0] = "sh";
        args[1] = importer;
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
        printf("\nThe translation importer couldn't be started:\n");
        printf("sh %s: %s\n\n", importer, strerror(err));
        printf("Press any key to go back to the main menu.\n");
        fflush(stdout);
        scr_mode("anykey");
        ugetchar();
}
#endif

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
        find_importer();
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
				run_importer();
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

