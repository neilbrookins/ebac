/*
   EBaC (Electronic Bible and Concordance)
   written by Neil G. Brookins
*/
/*
** refparse.c -- understand a Bible reference as it is spoken, or typed
** (EBaC 2.4): "Genesis one colon one", "Genesis I colon I" (as speech to
** text writes "one" after a word), "John three sixteen", "first
** Corinthians thirteen four", "Psalm one hundred nineteen verse one",
** "john 3:16-18". See "Voice" in readme.architecture.
**
**   [read] [the] BOOK [chapter] NUMBER [colon | verse | :] NUMBER
**          [to | through | - NUMBER]
**
** BOOK is one or more words, joined and matched as after @ (match_it(),
** with its Roman numerals), the longest that names one book: "song of
** solomon"; a number before it, as a digit, a word ("first", "one",
** "1st") or a Roman numeral ("I"), is a digit ("first corinthians" is
** matched as 1corinthians, and finds ICorinthians too). NUMBER is digits,
** English words ("sixteen", "one hundred and nineteen", "twenty-three")
** or a Roman numeral. English first: the words for numbers are of
** English, as speech in another language would need its own.
*/
#include "bible.h"
#include <string.h>

#define MAXTOKENS 24
#define MAXTOKEN 64

static char tokens[MAXTOKENS][MAXTOKEN];
static int ntokens;

static char *units[] = { "zero", "one", "two", "three", "four", "five", "six",
        "seven", "eight", "nine", "ten", "eleven", "twelve", "thirteen",
        "fourteen", "fifteen", "sixteen", "seventeen", "eighteen", "nineteen",
        NULL };
static char *tens[] = { "", "", "twenty", "thirty", "forty", "fifty", "sixty",
        "seventy", "eighty", "ninety", NULL };
static char *ordinals[] = { "", "first", "second", "third", NULL };
static char *short_ordinals[] = { "", "1st", "2nd", "3rd", NULL };

static int word_in P((char *word, char *list[]));
static int roman P((char *word));
static int number_at P((int *i));
static void tokenize P((char *text));
static int book_at P((int *i));
static int is_range_word P((char *word));
static int range_to P((int *i, int *verse, int *to, char *why));

/* Purpose: the place of word in list, or -1 */
static int word_in(char *word, char *list[])
{
        int i;

        for (i = 0; list[i] != NULL; i++)
                if (list[i][0] != '\0' && strcmp(word, list[i]) == 0)
                        return (i);
        return (-1);
}

/* Purpose: the value of a Roman numeral, in small letters ("xix"); 0 if
   word isn't one */
static int roman(char *word)
{
        int i, value = 0, digit, next;

        for (i = 0; word[i] != '\0'; i++)
                if (word[i] != 'i' && word[i] != 'v' && word[i] != 'x' &&
                    word[i] != 'l' && word[i] != 'c')
                        return (0);
        for (i = 0; word[i] != '\0'; i++)
        {
                digit = word[i] == 'i' ? 1 : word[i] == 'v' ? 5 :
                        word[i] == 'x' ? 10 : word[i] == 'l' ? 50 : 100;
                next = word[i + 1] == 'i' ? 1 : word[i + 1] == 'v' ? 5 :
                       word[i + 1] == 'x' ? 10 : word[i + 1] == 'l' ? 50 :
                       word[i + 1] == 'c' ? 100 : 0;
                value += (digit < next) ? -digit : digit;
        }
        return (value > 0 ? value : 0);
}

/*
** Purpose: the number at token *i: digits, a Roman numeral, or English
**          words ("one hundred and nineteen", "twenty three"), moving *i
**          past it.
** Returns: the number, or -1 (and *i as it was) if there is none there.
*/
static int number_at(int *i)
{
        int k = *i, value = 0, n, any = FALSE;
        char *t;

        if (k >= ntokens)
                return (-1);
        t = tokens[k];
        if (isdigit(UCHAR(t[0])))
        {
                for (n = 0; isdigit(UCHAR(t[n])); n++)
                        value = value * 10 + (t[n] - '0');
                if (t[n] != '\0' || value > 9999)
                        return (-1);
                *i = k + 1;
                return (value);
        }
        if ((n = roman(t)) > 0)
        {
                *i = k + 1;
                return (n);
        }
        /* English: [UNIT hundred [and]] [TENS [UNIT] | UNIT-or-TEEN] */
        if (strcmp(t, "a") == 0 && k + 1 < ntokens &&
            strcmp(tokens[k + 1], "hundred") == 0)
        {
                value = 100;            /* "a hundred and nineteen" */
                k += 2;
                any = TRUE;
        }
        else if ((n = word_in(t, units)) >= 1 && n <= 9 && k + 1 < ntokens &&
                 strcmp(tokens[k + 1], "hundred") == 0)
        {
                value = n * 100;
                k += 2;
                any = TRUE;
        }
        if (any && k < ntokens && strcmp(tokens[k], "and") == 0 &&
            k + 1 < ntokens && (word_in(tokens[k + 1], units) >= 0 ||
                                word_in(tokens[k + 1], tens) >= 0))
                k++;
        if (k < ntokens && (n = word_in(tokens[k], tens)) >= 2)
        {
                value += n * 10;
                k++;
                any = TRUE;
                if (k < ntokens && (n = word_in(tokens[k], units)) >= 1 && n <= 9)
                {
                        value += n;
                        k++;
                }
        }
        else if (k < ntokens && (n = word_in(tokens[k], units)) >= 0)
        {
                value += n;
                k++;
                any = TRUE;
        }
        if (!any)
                return (-1);
        *i = k;
        return (value);
}

/*
** Purpose: split the text into words, in small letters: ":" and a "-"
**          between numbers are words of their own ("3:16-18" is 3 : 16 -
**          18); a "-" between letters joins them ("twenty-three"); other
**          punctuation goes.
*/
static void tokenize(char *text)
{
        char buf[512], *p;
        int i, n;

        strncpy(buf, text, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        u8_lower(buf);
        ntokens = 0;
        for (p = buf; *p != '\0' && ntokens < MAXTOKENS; )
        {
                if (*p == ':' || (*p == '-' && (p == buf || !isalpha(UCHAR(p[-1])) ||
                                                !isalpha(UCHAR(p[1])))))
                {
                        tokens[ntokens][0] = *p++;
                        tokens[ntokens++][1] = '\0';
                        continue;
                }
                if (!u8_is_letter(p))
                {
                        p += u8_len(p);
                        continue;
                }
                i = 0;
                while (*p != '\0' && (u8_is_letter(p) || (*p == '-' && isalpha(UCHAR(p[1])))))
                {
                        if (*p == '-')
                        {
                                p++;            /* "twenty-three": two words */
                                break;
                        }
                        n = u8_len(p);
                        if (i + n < MAXTOKEN)
                        {
                                strncpy(&tokens[ntokens][i], p, n);
                                i += n;
                        }
                        p += n;
                        /* digits then letters: "1john" is 1 john; letters
                           then digits: "john3" is john 3 */
                        if (*p != '\0' && isdigit(UCHAR(p[-1])) != isdigit(UCHAR(*p)) &&
                            (isalpha(UCHAR(*p)) || isdigit(UCHAR(*p))) &&
                            !(isdigit(UCHAR(p[-1])) && (strncmp(p, "st", 2) == 0 ||
                              strncmp(p, "nd", 2) == 0 || strncmp(p, "rd", 2) == 0 ||
                              strncmp(p, "th", 2) == 0) && !isalpha(UCHAR(p[2]))))
                                break;
                }
                tokens[ntokens][i] = '\0';
                if (i > 0)
                        ntokens++;
        }
}

/*
** Purpose: the book named at token *i: the most words (up to 5) that name
**          one book, joined, as matched after @; a number first is a digit.
**          "psalm" finds Psalms; "revelations", Revelation (a final "s"
**          too many).
** Returns: the book's number (1 for the first), moving *i past it; 0 if
**          none (*i as it was).
*/
static int book_at(int *i)
{
        char joined[MAXTOKEN * 6], lead[16];
        int words, k, n, b, start = *i;

        for (words = 5; words >= 1; words--)
        {
                if (start + words > ntokens)
                        continue;
                joined[0] = '\0';
                for (k = start; k < start + words; k++)
                {
                        if (k == start && words > 1)
                        {
                                /* a number before the name: as a digit */
                                n = -1;
                                if ((n = word_in(tokens[k], ordinals)) < 1 &&
                                    (n = word_in(tokens[k], short_ordinals)) < 1 &&
                                    !((n = word_in(tokens[k], units)) >= 1 && n <= 3) &&
                                    !((n = roman(tokens[k])) >= 1 && n <= 3) &&
                                    !(isdigit(UCHAR(tokens[k][0])) && tokens[k][1] == '\0' &&
                                      (n = tokens[k][0] - '0') >= 1 && n <= 3))
                                        n = 0;
                                if (n > 0)
                                {
                                        sprintf(lead, "%d", n);
                                        strcat(joined, lead);
                                        continue;
                                }
                        }
                        if (!u8_is_letter(tokens[k]) || isdigit(UCHAR(tokens[k][0])))
                                break;          /* (a book's name is words) */
                        strcat(joined, tokens[k]);
                }
                if (k < start + words || joined[0] == '\0')
                        continue;
                if ((b = match_it(joined, FALSE)) > 0)
                {
                        *i = start + words;
                        return (b);
                }
                n = strlen(joined);
                if (n > 3 && joined[n - 1] == 's')
                {
                        joined[n - 1] = '\0';   /* "revelations" */
                        if ((b = match_it(joined, FALSE)) > 0)
                        {
                                *i = start + words;
                                return (b);
                        }
                }
        }
        return (0);
}

/*
** Purpose: understand a reference, spoken or typed (see the top of this
**          file), in the translation in use.
** Returns: TRUE with *book (1 for the first), *chapter and *verse (0 if
**          not given) and *to (the last verse of a range; 0 if none);
**          *chapter is 0 with a *verse for "BOOK N to M", which only a
**          book of one chapter can mean (the caller knows);
**          FALSE, with why[] saying what wasn't understood.
*/
int parse_reference(char *text, int *book, int *chapter, int *verse, int *to, char *why)
{
        int i = 0, n;

        *book = *chapter = *verse = *to = 0;
        tokenize(text);
        if (i < ntokens && (strcmp(tokens[i], "read") == 0 ||
                            strcmp(tokens[i], "show") == 0))
                i++;
        if (i < ntokens && strcmp(tokens[i], "the") == 0)
                i++;
        if (i + 1 < ntokens && strcmp(tokens[i], "book") == 0 &&
            strcmp(tokens[i + 1], "of") == 0)
                i += 2;
        if ((*book = book_at(&i)) == 0)
        {
                strcpy(why, "no book of that name");
                return (FALSE);
        }
        if (i < ntokens && strcmp(tokens[i], "chapter") == 0)
                i++;
        if (i == ntokens)
                return (TRUE);          /* a book alone */
        if ((n = number_at(&i)) < 1)
        {
                strcpy(why, "no chapter number");
                return (FALSE);
        }
        *chapter = n;
        if (i < ntokens && is_range_word(tokens[i]))
        {
                /* "Jude three to five": verses, with no chapter (the
                   caller knows whether the book has only one) */
                *verse = *chapter;
                *chapter = 0;
                return (range_to(&i, verse, to, why));
        }
        if (i < ntokens && (strcmp(tokens[i], ":") == 0 ||
                            strcmp(tokens[i], "colon") == 0 ||
                            strcmp(tokens[i], "verse") == 0 ||
                            strcmp(tokens[i], "verses") == 0))
                i++;
        if (i == ntokens)
                return (TRUE);          /* a chapter alone */
        if ((n = number_at(&i)) < 1)
        {
                strcpy(why, "no verse number");
                return (FALSE);
        }
        *verse = n;
        return (range_to(&i, verse, to, why));
}

/* Purpose: TRUE if word begins the end of a range: "-", "to", "through" */
static int is_range_word(char *word)
{
        return (strcmp(word, "-") == 0 || strcmp(word, "to") == 0 ||
                strcmp(word, "through") == 0 || strcmp(word, "thru") == 0);
}

/*
** Purpose: the rest of a reference after its first verse, at token *i:
**          nothing, or [to | through | -] NUMBER, the last verse (*to).
** Returns: TRUE; FALSE with why[] if there is something else.
*/
static int range_to(int *i, int *verse, int *to, char *why)
{
        int n;

        if (*i < ntokens && is_range_word(tokens[*i]))
        {
                (*i)++;
                if ((n = number_at(i)) < *verse)
                {
                        strcpy(why, "the range of verses isn't clear");
                        return (FALSE);
                }
                *to = n;
        }
        if (*i < ntokens)
        {
                sprintf(why, "not understood from \"%.40s\"", tokens[*i]);
                return (FALSE);
        }
        return (TRUE);
}
