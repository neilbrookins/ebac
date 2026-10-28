/*
   EBaC (Electronic Bible and Concordance)
   written by Neil G. Brookins
*/
/*
** api.c -- ebac-api, EBaC for other programs (EBaC 2.4): a CGI program,
** which a web server runs once for each request, answering in JSON. It is
** built from the same source as bible, the interactive program (all but
** this main() and the screen), so that a verse, a search or a book's name
** is found by the same code. See "Voice" in readme.architecture.
**
** A web server gives the request in PATH_INFO and QUERY_STRING. By hand,
** for testing, it is the first argument:
**
**     BIBLEROOT=~/ebac ebac-api 'verse?ref=john+3:16'
**
** Requests (t= a translation, as named in trans; the first if none):
**
**   translations                 the translations: name, title
**   books?t=AV                   its books: name, full name, chapters,
**                                and how they may be spoken
**   parse?text=...&t=AV          a reference understood (refparse.c)
**   verse?ref=...&n=1&t=AV       the verses of a reference (n verses from
**                                it, if it is one verse)
**   next?svn=N&n=1&t=AV          the n verses after verse N (its
**                                sequential number); previous likewise
**   search?q=...&max=10&t=AV     a search in EBaC's own syntax
**
** An answer is {"...": ...}; an error is {"error": "..."}, with HTTP
** status 400. What the shared code would print (as an error) is caught,
** to be the error's text, so that it never spoils the JSON.
*/
#include "bible.h"
#include "refparse.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#define MAXQUERY 2048
#define MAXN 200                /* the most verses in one answer */

extern char trans[][80];
extern long int numverses, numbooks;
#ifndef COCO
extern struct bva *book;
extern long int *pach;
#endif

static char query[MAXQUERY];
static int cgi = FALSE;         /* run by a web server (not by hand) */
static int answered = FALSE;
static int saved_stdout = -1;   /* the real standard output */
static FILE *caught = NULL;     /* what the shared code prints, meanwhile */

/* ---- output -------------------------------------------------------- */

/* Purpose: start catching what is printed (it may be an error's text) */
static void catch_output(void)
{
        fflush(stdout);
        if ((caught = tmpfile()) == NULL)
                return;
        saved_stdout = dup(1);
        dup2(fileno(caught), 1);
}

/* Purpose: stop catching, and put what was caught in text (cut short) */
static void caught_text(char *text, int size)
{
        int n = 0, c;

        text[0] = '\0';
        if (caught == NULL)
                return;
        fflush(stdout);
        dup2(saved_stdout, 1);
        close(saved_stdout);
        rewind(caught);
        while ((c = getc(caught)) != EOF && n < size - 1)
                text[n++] = (c == '\n' || c == '\r') ? ' ' : (char) c;
        text[n] = '\0';
        while (n > 0 && text[n - 1] == ' ')
                text[--n] = '\0';
        for (n = 0; text[n] == ' '; n++)
                ;
        memmove(text, &text[n], strlen(&text[n]) + 1);
        fclose(caught);
        caught = NULL;
}

/* Purpose: print s as a JSON string (UTF-8 as it is; " \ and control
   characters escaped) */
static void json_string(char *s)
{
        putchar('"');
        for (; *s != '\0'; s++)
        {
                if (*s == '"' || *s == '\\')
                        printf("\\%c", *s);
                else if ((*s & 0xFF) < 0x20)
                        printf("\\u%04x", *s & 0xFF);
                else
                        putchar(*s);
        }
        putchar('"');
}

/* Purpose: begin the answer (after what was caught, which is dropped) */
static void begin_answer(int ok)
{
        char ignored[16];

        caught_text(ignored, sizeof(ignored));
        if (cgi)
        {
                if (!ok)
                        printf("Status: 400 Bad Request\r\n");
                printf("Content-Type: application/json; charset=utf-8\r\n\r\n");
        }
        answered = TRUE;
}

/* Purpose: answer with an error: why, and what the shared code printed */
static void fail(char *why)
{
        char text[1024];

        caught_text(text, sizeof(text));
        if (cgi)
                printf("Status: 400 Bad Request\r\nContent-Type: application/json; charset=utf-8\r\n\r\n");
        answered = TRUE;
        printf("{\"error\": ");
        if (text[0] != '\0' && why[0] != '\0')
        {
                strcat(text, " (");
                strncat(text, why, sizeof(text) - strlen(text) - 2);
                strcat(text, ")");
                json_string(text);
        }
        else
                json_string(text[0] != '\0' ? text : why);
        printf("}\n");
        exit(0);
}

/* Purpose: on exit without an answer (the shared code stopped, as for a
   missing file): answer with what it said */
static void at_exit(void)
{
        if (!answered)
        {
                answered = TRUE;
                fail("EBaC stopped");
        }
}

/* ---- the request --------------------------------------------------- */

static int hex(int c)
{
        return isdigit(c) ? c - '0' : tolower(c) - 'a' + 10;
}

/*
** Purpose: the value of parameter name in the query (URL-decoded: + is a
**          space, %XX a byte) in value; "" if none.
*/
static char *param(char *name, char *value, int size)
{
        char *p = query;
        int len = strlen(name), n = 0;

        value[0] = '\0';
        while (*p != '\0')
        {
                if (strncmp(p, name, len) == 0 && p[len] == '=')
                {
                        for (p += len + 1; *p != '\0' && *p != '&' && n < size - 1; p++)
                        {
                                if (*p == '+')
                                        value[n++] = ' ';
                                else if (*p == '%' && isxdigit(UCHAR(p[1])) && isxdigit(UCHAR(p[2])))
                                {
                                        value[n++] = (char) (hex(UCHAR(p[1])) * 16 + hex(UCHAR(p[2])));
                                        p += 2;
                                }
                                else
                                        value[n++] = *p;
                        }
                        value[n] = '\0';
                        return (value);
                }
                while (*p != '\0' && *p != '&')
                        p++;
                if (*p == '&')
                        p++;
        }
        return (value);
}

static long number_param(char *name, long dflt)
{
        char v[32];

        param(name, v, sizeof(v));
        if (v[0] == '\0')
                return (dflt);
        return (atol(v));
}

/* Purpose: choose the translation of t= (or the first of trans) */
static void choose_translation(void)
{
        char t[80], first[80];
        int i;

        param("t", t, sizeof(t));
        if (t[0] == '\0')
        {
                for (i = 0; trans[0][i] != '\0' && !isspace(UCHAR(trans[0][i])) && i < 79; i++)
                        first[i] = trans[0][i];
                first[i] = '\0';
                strcpy(t, first);
        }
        if (!use_translation(t))
                fail("");
}

/* ---- books and verses ---------------------------------------------- */

/* the number of chapters of book b (0 for the first) */
static int chapters_of(int b)
{
        return (int) (book[b + 1].chapt - book[b].chapt);
}

/* the sequential number of verse 1 of chapter c (1 for the first) of book b */
static long chapter_start(int b, int c)
{
        return pach[book[b].chapt + c - 1];
}

/* the number of verses of chapter c of book b */
static int verses_of(int b, int c)
{
        return (int) (pach[book[b].chapt + c] - pach[book[b].chapt + c - 1]);
}

/*
** Purpose: a book's full name, as the translation's text has it ("Song of
**          Solomon"), from the translation's booknames file (one per line,
**          in the order of booklist), if it has one; else as in booklist,
**          with a space after a number at its start ("1 Samuel").
*/
static char *full_name(int b, char *name)
{
        static char names[300][MAXBOOKNAME];
        static int loaded = -1;
        FILE *f;
        int n;

        if (loaded < 0)
        {
                loaded = 0;
                if ((f = fopen("booknames", "r")) != NULL)
                {
                        while (loaded < 300 && fgets(names[loaded], MAXBOOKNAME, f) != NULL)
                        {
                                n = strlen(names[loaded]);
                                while (n > 0 && (names[loaded][n - 1] == '\n' || names[loaded][n - 1] == '\r'))
                                        names[loaded][--n] = '\0';
                                loaded++;
                        }
                        fclose(f);
                }
        }
        if (b < loaded && names[b][0] != '\0')
                strcpy(name, names[b]);
        else if (isdigit(UCHAR(book[b].name[0])) && isalpha(UCHAR(book[b].name[1])))
                sprintf(name, "%c %s", book[b].name[0], &book[b].name[1]);
        else
                strcpy(name, book[b].name);
        return (name);
}

/* Purpose: a verse's text, without the [ ] and { } marks, for speech */
static void plain_text(char *from, char *to, int size)
{
        int n = 0;

        for (; *from != '\0' && n < size - 1; from++)
                if (*from != '[' && *from != ']' && *from != '{' && *from != '}')
                        to[n++] = *from;
        to[n] = '\0';
        while (n > 0 && isspace(UCHAR(to[n - 1])))
                to[--n] = '\0';
}

/* Purpose: whether verse svn has any text (the source has empty ones) */
static int has_text(long svn)
{
        struct bcv_array_type *v = get_verse(svn);
        char *p;

        for (p = v->data_array; *p != '\0'; p++)
                if (!isspace(UCHAR(*p)))
                        return (TRUE);
        return (FALSE);
}

/*
** Purpose: print verse svn as JSON: its reference, book, chapter, verse,
**          sequential number, text, and how to speak it ("John chapter 3,
**          verse 16. For God so loved ..."; after the first verse, only
**          "Verse 17. ...").
*/
/* Purpose: print verse svn as JSON; its "spoken" form says as much of
   the reference as is new since the verse before (prev; 0 for none): the
   book and chapter, the chapter, or only the verse's number ("16.", not
   "Verse 16.": in a run of verses, the word again and again distracts
   from the text, and the numbers counting up say what they are) */
static void print_verse(long svn, long prev)
{
        struct bcv_array_type *v;
        char name[MAXBOOKNAME * 2], text[LONGESTVERSE], spoken[LONGESTVERSE + 200];
        char prev_chapter[16];
        int b = verse_to_book(svn), n;

        full_name(b, name);
        prev_chapter[0] = '\0';        /* (get_verse()'s answer is static) */
        if (prev > 0)
        {
                strncpy(prev_chapter, get_verse(prev)->chapt_array, sizeof(prev_chapter) - 1);
                prev_chapter[sizeof(prev_chapter) - 1] = '\0';
        }
        v = get_verse(svn);
        plain_text(v->data_array, text, sizeof(text));
        if (prev == 0 || verse_to_book(prev) != b)
                sprintf(spoken, "%s chapter %s, verse %s. %s", name,
                        v->chapt_array, v->verse_array, text);
        else if (strcmp(prev_chapter, v->chapt_array) != 0)
                sprintf(spoken, "Chapter %s, verse %s. %s", v->chapt_array,
                        v->verse_array, text);
        else            /* (the next of a run: its number alone, "16.") */
                sprintf(spoken, "%s. %s", v->verse_array, text);
        printf("{\"ref\": ");
        sprintf(text, "%s %s:%s", name, v->chapt_array, v->verse_array);
        json_string(text);
        printf(", \"book\": ");
        json_string(name);
        printf(", \"chapter\": %s, \"verse\": %s, \"svn\": %ld, \"text\": ",
               v->chapt_array, v->verse_array, svn);
        strcpy(text, v->data_array);    /* (as the text has it, but the end) */
        n = strlen(text);
        while (n > 0 && isspace(UCHAR(text[n - 1])))
                text[--n] = '\0';
        json_string(text);
        printf(", \"spoken\": ");
        json_string(spoken);
        printf("}");
}

/* Purpose: answer with the verses from..to (svns), those with text, at
   most n of them, in order (step 1) or backwards (step -1) */
static void answer_verses(long from, long to, int step, long n)
{
        long svn, shown = 0, prev = 0;
        long list[MAXN];
        int count = 0, i;

        for (svn = from; svn >= 1 && svn <= numverses && count < n && count < MAXN;
             svn += step)
        {
                if (has_text(svn))
                        list[count++] = svn;
                if (svn == to)
                        break;
        }
        if (count == 0)
                fail(step > 0 ? "no verse after it" : "no verse before it");
        begin_answer(TRUE);
        printf("{\"translation\": ");
        json_string(etla);
        printf(", \"verses\": [");
        for (i = 0; i < count; i++)
        {
                /* (backwards: shown in order, the earliest first) */
                long s = step > 0 ? list[i] : list[count - 1 - i];

                printf(i > 0 ? ",\n  " : "\n  ");
                print_verse(s, prev);
                prev = s;
                shown++;
        }
        printf("]}\n");
}

/* ---- the requests -------------------------------------------------- */

/*
** The translations of trans, in order: name and title. Read from the file
** itself, whole lines: the shared code keeps 80 bytes of each (as on the
** CoCo), which cuts a long title, sooner in Cyrillic (two bytes a letter).
** The same lines are skipped: blank, and comments (#).
*/
static void translations(void)
{
        char path[1100], line[1024], *t, *title;
        char *root = getenv("BIBLEROOT");
        FILE *f;
        int k, n = 0;

        sprintf(path, "%.1000s/trans", root != NULL ? root : ".");
        if ((f = fopen(path, "r")) == NULL)
                fail("can't read the list of translations (trans)");
        begin_answer(TRUE);
        printf("{\"translations\": [");
        while (fgets(line, sizeof(line), f) != NULL)
        {
                k = strlen(line);
                if (k > 0 && line[k - 1] != '\n' && !feof(f))
                {
                        int c;          /* (longer still: read over the rest) */

                        while ((c = fgetc(f)) != '\n' && c != EOF)
                                ;
                }
                while (k > 0 && isspace(UCHAR(line[k - 1])))
                        line[--k] = '\0';
                for (t = line; *t == ' ' || *t == '\t'; t++)
                        ;
                if (*t == '#' || *t == '\0')
                        continue;
                for (title = t; *title != '\0' && !isspace(UCHAR(*title)); title++)
                        ;
                if (*title != '\0')
                        *title++ = '\0';
                while (isspace(UCHAR(*title)))
                        title++;
                printf(n++ > 0 ? ",\n  {\"name\": " : "\n  {\"name\": ");
                json_string(t);
                printf(", \"title\": ");
                json_string(title);
                printf("}");
        }
        fclose(f);
        printf("]}\n");
}

/*
** The ways a book may be spoken: its full name, and, for a numbered book
** ("1 Samuel", "I Samuel"), with the number as a digit, a word ("First
** Samuel"), "1st" and a Roman numeral.
*/
static void print_spoken(char *full)
{
        static char *words[] = { "", "First", "Second", "Third" };
        static char *romans[] = { "", "I", "II", "III" };
        static char *shorts[] = { "", "1st", "2nd", "3rd" };
        char rest[MAXBOOKNAME * 2], v[MAXBOOKNAME * 2];
        int n = 0, i;

        if (full[0] >= '1' && full[0] <= '3' && full[1] == ' ')
        {
                n = full[0] - '0';
                strcpy(rest, &full[2]);
        }
        for (i = 3; i >= 1 && n == 0; i--)
                if (strncmp(full, romans[i], strlen(romans[i])) == 0 &&
                    full[strlen(romans[i])] == ' ')
                {
                        n = i;
                        strcpy(rest, &full[strlen(romans[i]) + 1]);
                }
        printf("[");
        json_string(full);
        if (n > 0)
        {
                snprintf(v, sizeof(v), "%d %s", n, rest);
                if (strcmp(v, full) != 0)
                {
                        printf(", ");
                        json_string(v);
                }
                snprintf(v, sizeof(v), "%s %s", words[n], rest);
                printf(", ");
                json_string(v);
                snprintf(v, sizeof(v), "%s %s", shorts[n], rest);
                printf(", ");
                json_string(v);
                snprintf(v, sizeof(v), "%s %s", romans[n], rest);
                if (strcmp(v, full) != 0)
                {
                        printf(", ");
                        json_string(v);
                }
        }
        printf("]");
}

/*
** Purpose: the ways each number is said, from 1 to the most chapters or
**          verses of any book or chapter of the translation (176, for
**          Psalm 119, in AV): for the EBaC integration's sentences.
*/
static void numbers(void)
{
        char forms[8][MAXSPOKEN];
        int most = 0, b, c, n, k, count;

        choose_translation();
        for (b = 0; b < numbooks; b++)
        {
                if (chapters_of(b) > most)
                        most = chapters_of(b);
                for (c = 1; c <= chapters_of(b); c++)
                        if (verses_of(b, c) > most)
                                most = verses_of(b, c);
        }
        begin_answer(TRUE);
        printf("{\"translation\": ");
        json_string(etla);
        printf(", \"most\": %d, \"numbers\": [", most);
        for (n = 1; n <= most; n++)
        {
                count = spoken_numbers(n, forms, 8);
                printf(n > 1 ? ",\n  " : "\n  ");
                printf("{\"number\": %d, \"spoken\": [", n);
                for (k = 0; k < count; k++)
                {
                        if (k > 0)
                                printf(", ");
                        json_string(forms[k]);
                }
                printf("]}");
        }
        printf("]}\n");
}

static void books(void)
{
        char name[MAXBOOKNAME * 2];
        int b;

        choose_translation();
        begin_answer(TRUE);
        printf("{\"translation\": ");
        json_string(etla);
        printf(", \"books\": [");
        for (b = 0; b < numbooks; b++)
        {
                printf(b > 0 ? ",\n  {\"name\": " : "\n  {\"name\": ");
                json_string(book[b].name);
                printf(", \"full_name\": ");
                json_string(full_name(b, name));
                printf(", \"number\": %d, \"chapters\": %d, \"spoken\": ", b + 1, chapters_of(b));
                print_spoken(name);
                printf("}");
        }
        printf("]}\n");
}

/*
** Purpose: a reference, from its words, checked against the translation:
**          the first and last verses (svns) it means.
*/
/*
** Purpose: the verses a reference names, in the translation in use; *b,
**          *c, *v and *last as understood (the book from 1). Of a book
**          of one chapter, a number is its verse: "Jude three" is Jude 1:3,
**          "Jude three to five" Jude 1:3-5.
*/
static void reference(char *text, long *from, long *to, int *b, int *c, int *v, int *last)
{
        char why[200], msg[400], name[MAXBOOKNAME * 2];
        int bk;

        if (!parse_reference(text, b, c, v, last, why))
        {
                sprintf(msg, "Not a reference I know: %s", why);
                fail(msg);
        }
        bk = *b - 1;
        full_name(bk, name);
        if (chapters_of(bk) == 1 && *c > 1 && *v == 0)
        {
                *v = *c;                /* "Jude three" */
                *c = 1;
        }
        if (*c == 0 && *v > 0)
        {
                if (chapters_of(bk) != 1)
                {
                        sprintf(msg, "Which chapter of %s?", name);
                        fail(msg);
                }
                *c = 1;                 /* "Jude three to five" */
        }
        if (*c == 0)
                *c = 1;                 /* a book alone: its first verse */
        if (*c > chapters_of(bk))
        {
                sprintf(msg, "%s has %d chapter%s", name, chapters_of(bk),
                        chapters_of(bk) == 1 ? "" : "s");
                fail(msg);
        }
        if (*v == 0)
                *v = 1;                 /* a chapter alone: its first verse */
        if (*v > verses_of(bk, *c) || *last > verses_of(bk, *c))
        {
                sprintf(msg, "%s %d has %d verses", name, *c, verses_of(bk, *c));
                fail(msg);
        }
        *from = chapter_start(bk, *c) + *v - 1;
        *to = *last > 0 ? chapter_start(bk, *c) + *last - 1 : 0;
}

static void parse(void)
{
        char text[512], name[MAXBOOKNAME * 2];
        int b, c, v, last;
        long from, to;

        choose_translation();
        param("ref", text, sizeof(text));
        reference(text, &from, &to, &b, &c, &v, &last);
        begin_answer(TRUE);
        printf("{\"translation\": ");
        json_string(etla);
        printf(", \"book\": ");
        json_string(full_name(b - 1, name));
        printf(", \"book_number\": %d, \"chapter\": %d, \"verse\": %d, \"to\": %d, \"svn\": %ld}\n",
               b, c, v, last, from);
}

static void verse(void)
{
        char ref[512];
        long from, to, n = number_param("n", 1);
        int b, c, v, last;

        choose_translation();
        param("ref", ref, sizeof(ref));
        reference(ref, &from, &to, &b, &c, &v, &last);
        if (to > 0)
                answer_verses(from, to, 1, to - from + 1);
        else
                answer_verses(from, 0, 1, n);
}

static void next(int step)
{
        long svn = number_param("svn", 0), n = number_param("n", 1);

        choose_translation();
        if (svn < 1 || svn > numverses)
                fail("svn: no such verse");
        answer_verses(svn + step, 0, step, n);
}

static void search(void)
{
        char q[256];                    /* (MAXSEARCHLEN, in process.c) */
        long max = number_param("max", 10), count = 0, i;
        long start = number_param("start", 1);  /* (the first wanted: 1 for
                                                   the first result) */
        long int *list;
        int len;

        choose_translation();
        param("q", q, sizeof(q) - 2);
        len = strlen(q);
        q[len] = '\r';                  /* (as typed: Enter at the end) */
        q[len + 1] = '\0';
        u8_lower(q);
        if ((list = process_line(q)) == NULL)
                fail("");
        while (list[count] != 0)
                count++;
        begin_answer(TRUE);
        printf("{\"translation\": ");
        json_string(etla);
        printf(", \"count\": %ld", count);
        if (count_word[0] != '\0')
                printf(", \"word\": \"%s\", \"times\": %ld", count_word, num_found);
        if (start < 1)
                start = 1;
        printf(", \"start\": %ld, \"verses\": [", start);
        /* each with its whole reference: results aren't a passage */
        for (i = start - 1; i < count && i < start - 1 + max; i++)
        {
                printf(i > start - 1 ? ",\n  " : "\n  ");
                print_verse(list[i], 0L);
        }
        printf("]}\n");
}

int main(int argc, char **argv)
{
        char *path, *q, request[64];
        int i;

        interactive = FALSE;
        atexit(at_exit);
        if (getenv("GATEWAY_INTERFACE") != NULL)
        {
                cgi = TRUE;
                path = getenv("PATH_INFO");
                q = getenv("QUERY_STRING");
                if (path == NULL)
                        path = "";
                while (*path == '/')
                        path++;
                strncpy(request, path, sizeof(request) - 1);
                request[sizeof(request) - 1] = '\0';
                strncpy(query, q != NULL ? q : "", sizeof(query) - 1);
        }
        else if (argc > 1)
        {
                /* by hand: ebac-api 'verse?ref=john+3:16' */
                for (i = 0; argv[1][i] != '\0' && argv[1][i] != '?' && i < 63; i++)
                        request[i] = argv[1][i];
                request[i] = '\0';
                if (argv[1][i] == '?')
                        strncpy(query, &argv[1][i + 1], sizeof(query) - 1);
        }
        else
        {
                printf("usage: ebac-api 'REQUEST?PARAMETERS', such as 'verse?ref=john+3:16'\n");
                printf("(see the top of api.c, or readme.architecture)\n");
                answered = TRUE;
                return (2);
        }
        catch_output();
        initialize();           /* (BIBLEROOT, trans: no terminal) */
        if (strcmp(request, "translations") == 0)
                translations();
        else if (strcmp(request, "books") == 0)
                books();
        else if (strcmp(request, "numbers") == 0)
                numbers();
        else if (strcmp(request, "parse") == 0)
                parse();
        else if (strcmp(request, "verse") == 0)
                verse();
        else if (strcmp(request, "next") == 0)
                next(1);
        else if (strcmp(request, "previous") == 0)
                next(-1);
        else if (strcmp(request, "search") == 0)
                search();
        else
                fail("no such request (translations, books, numbers, parse, verse, next, previous, search)");
        return (0);
}
