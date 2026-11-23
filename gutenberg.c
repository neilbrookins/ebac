/*
   EBaC (Electronic Bible and Concordance)
   written by Neil G. Brookins
*/
/*
** gutenberg.c -- texts from Project Gutenberg, by ebook number: the
** importer in C (import.c includes this file, so that the two share the
** importer's state and parts), in place of add-gutenberg.sh, and checked
** byte for byte against it (tests/importers.sh). What each part does, and
** why, is said there too (in git: commit 72ace7e, the last with it); this
** follows it step by step, awk by awk:
**
**   bible --import gutenberg [--agree] [--file PATH] [--page PATH] NUMBER
**   bible --import gutenberg --remove NAME | --list | --catalog
**   bible --import gutenberg --license NUMBER | --check NUMBER
**
** (and --at-end, --agreed-how TEXT, as the other sources take them). P
** under A on EBaC's main menu runs it in EBaC itself (gb_in_ebac()).
**
** Gutenberg's texts are laid out in many ways, so each is known by its
** ebook number, from KNOWN below: its short name, and its books (their
** names, and the headings that start them). A text not in the list is
** refused; each new one is looked at by hand and added to the list.
*/

/* ---- The texts this importer knows (add-gutenberg.sh's KNOWN) ----------
**
** A line per text: its ebook number, short name(s), format, title and
** language; then a line per book: "book", its name (as EBaC shows it), and
** the start of the heading that begins it in the text ("text": a book
** without chapters or verses; "translation": one of several in a text).
** The formats: see add-gutenberg.sh, and split_chapters() and the others
** below.
*/
static const char *gb_known =
"16955\tYUSUFALI-PG PICKTHALL-PG SHAKIR-PG\tsidebyside\tThe Qur'an in three translations\ten\n"
"translation\tY\tYUSUFALI-PG\n"
"translation\tP\tPICKTHALL-PG\n"
"translation\tS\tSHAKIR-PG\n"
"book\tQur'an\t1\n"
"8294\tWEB-PG\tverses\tThe World English Bible\ten\n"
"1581\tDRC-PG\tparagraphs\tThe Douay-Rheims Bible\ten\n"
"book\tGenesis\tTHE BOOK OF GENESIS\n"
"book\tExodus\tTHE BOOK OF EXODUS\n"
"book\tLeviticus\tTHE BOOK OF LEVITICUS\n"
"book\tNumbers\tTHE BOOK OF NUMBERS\n"
"book\tDeuteronomy\tTHE BOOK OF DEUTERONOMY\n"
"book\tJosue\tTHE BOOK OF JOSUE\n"
"book\tJudges\tTHE BOOK OF JUDGES\n"
"book\tRuth\tTHE BOOK OF RUTH\n"
"book\t1 Kings\tTHE FIRST BOOK OF SAMUEL, OTHERWISE CALLED THE FIRST BOOK OF KINGS\n"
"book\t2 Kings\tTHE SECOND BOOK OF SAMUEL, OTHERWISE CALLED THE SECOND BOOK OF KINGS\n"
"book\t3 Kings\tTHE THIRD BOOK OF KINGS\n"
"book\t4 Kings\tTHE FOURTH BOOK OF KINGS\n"
"book\t1 Paralipomenon\tTHE FIRST BOOK OF PARALIPOMENON\n"
"book\t2 Paralipomenon\tTHE SECOND BOOK OF PARALIPOMENON\n"
"book\t1 Esdras\tTHE FIRST BOOK OF ESDRAS\n"
"book\t2 Esdras\tTHE BOOK OF NEHEMIAS, WHICH IS CALLED THE SECOND OF ESDRAS\n"
"book\tTobias\tTHE BOOK OF TOBIAS\n"
"book\tJudith\tTHE BOOK OF JUDITH\n"
"book\tEsther\tTHE BOOK OF ESTHER\n"
"book\tJob\tTHE BOOK OF JOB\n"
"book\tPsalms\tTHE BOOK OF PSALMS\n"
"book\tProverbs\tTHE BOOK OF PROVERBS\n"
"book\tEcclesiastes\tECCLESIASTES\n"
"book\tCanticle of Canticles\tSOLOMON\342\200\231S CANTICLE OF CANTICLES\n"
"book\tWisdom\tTHE BOOK OF WISDOM\n"
"book\tEcclesiasticus\tECCLESIASTICUS\n"
"book\tIsaias\tTHE PROPHECY OF ISAIAS\n"
"book\tJeremias\tTHE PROPHECY OF JEREMIAS\n"
"book\tLamentations\tTHE LAMENTATIONS OF JEREMIAS\n"
"book\tBaruch\tTHE PROPHECY OF BARUCH\n"
"book\tEzechiel\tTHE PROPHECY OF EZECHIEL\n"
"book\tDaniel\tTHE PROPHECY OF DANIEL\n"
"book\tOsee\tTHE PROPHECY OF OSEE\n"
"book\tJoel\tTHE PROPHECY OF JOEL\n"
"book\tAmos\tTHE PROPHECY OF AMOS\n"
"book\tAbdias\tTHE PROPHECY OF ABDIAS\n"
"book\tJonas\tTHE PROPHECY OF JONAS\n"
"book\tMicheas\tTHE PROPHECY OF MICHEAS\n"
"book\tNahum\tTHE PROPHECY OF NAHUM\n"
"book\tHabacuc\tTHE PROPHECY OF HABACUC\n"
"book\tSophonias\tTHE PROPHECY OF SOPHONIAS\n"
"book\tAggeus\tTHE PROPHECY OF AGGEUS\n"
"book\tZacharias\tTHE PROPHECY OF ZACHARIAS\n"
"book\tMalachias\tTHE PROPHECY OF MALACHIAS\n"
"book\t1 Machabees\tTHE FIRST BOOK OF MACHABEES\n"
"book\t2 Machabees\tTHE SECOND BOOK OF MACHABEES\n"
"book\tMatthew\tTHE HOLY GOSPEL OF JESUS CHRIST ACCORDING TO SAINT MATTHEW\n"
"book\tMark\tTHE HOLY GOSPEL OF JESUS CHRIST ACCORDING TO ST. MARK\n"
"book\tLuke\tTHE HOLY GOSPEL OF JESUS CHRIST ACCORDING TO ST. LUKE\n"
"book\tJohn\tTHE HOLY GOSPEL OF JESUS CHRIST ACCORDING TO ST. JOHN\n"
"book\tActs\tTHE ACTS OF THE APOSTLES\n"
"book\tRomans\tTHE EPISTLE OF ST. PAUL THE APOSTLE TO THE ROMANS\n"
"book\t1 Corinthians\tTHE FIRST EPISTLE OF ST. PAUL TO THE CORINTHIANS\n"
"book\t2 Corinthians\tTHE SECOND EPISTLE OF ST. PAUL TO THE CORINTHIANS\n"
"book\tGalatians\tTHE EPISTLE OF ST. PAUL TO THE GALATIANS\n"
"book\tEphesians\tTHE EPISTLE OF ST. PAUL TO THE EPHESIANS\n"
"book\tPhilippians\tTHE EPISTLE OF ST. PAUL TO THE PHILIPPIANS\n"
"book\tColossians\tTHE EPISTLE OF ST. PAUL TO THE COLOSSIANS\n"
"book\t1 Thessalonians\tTHE FIRST EPISTLE OF ST. PAUL TO THE THESSALONIANS\n"
"book\t2 Thessalonians\tTHE SECOND EPISTLE OF ST. PAUL TO THE THESSALONIANS\n"
"book\t1 Timothy\tTHE FIRST EPISTLE OF ST. PAUL TO TIMOTHY\n"
"book\t2 Timothy\tTHE SECOND EPISTLE OF ST. PAUL TO TIMOTHY\n"
"book\tTitus\tTHE EPISTLE OF ST. PAUL TO TITUS\n"
"book\tPhilemon\tTHE EPISTLE OF ST. PAUL TO PHILEMON\n"
"book\tHebrews\tTHE EPISTLE OF ST. PAUL TO THE HEBREWS\n"
"book\tJames\tTHE CATHOLIC EPISTLE OF ST. JAMES THE APOSTLE\n"
"book\t1 Peter\tTHE FIRST EPISTLE OF ST. PETER THE APOSTLE\n"
"book\t2 Peter\tTHE SECOND EPISTLE OF ST. PETER THE APOSTLE\n"
"book\t1 John\tTHE FIRST EPISTLE OF ST. JOHN THE APOSTLE\n"
"book\t2 John\tTHE SECOND EPISTLE OF ST. JOHN THE APOSTLE\n"
"book\t3 John\tTHE THIRD EPISTLE OF ST. JOHN THE APOSTLE\n"
"book\tJude\tTHE CATHOLIC EPISTLE OF ST. JUDE\n"
"book\tApocalypse\tTHE APOCALYPSE OF ST. JOHN THE APOSTLE\n"
"text\tPrayer of Manasses\tAPPENDICES\n"
"text\t3 Esdras\tTHE THIRD BOOKE OF ESDRAS.\n"
"text\t4 Esdras\tTHE FOVRTH BOOKE OF ESDRAS.\n"
"text\tAbdias 1610\tBOOKS FOR COMPARISON\n"
"text\tJude 1582\tTHE CATHOLIKE EPISTLE OF IVDE THE APOSTLE.\n"
"text\tPreface 1582\tTHE PREFACE TO THE READER\n"
"text\tHard Words 1582\tHARD VVORDES EXPLICATED\n"
"17\tBOM-PG\tchapters\tThe Book of Mormon\ten\n"
"book\t1 Nephi\tTHE FIRST BOOK OF NEPHI\n"
"book\t2 Nephi\tTHE SECOND BOOK OF NEPHI\n"
"book\tJacob\tTHE BOOK OF JACOB\n"
"book\tEnos\tTHE BOOK OF ENOS\n"
"book\tJarom\tTHE BOOK OF JAROM\n"
"book\tOmni\tTHE BOOK OF OMNI\n"
"book\tWords of Mormon\tTHE WORDS OF MORMON\n"
"book\tMosiah\tTHE BOOK OF MOSIAH\n"
"book\tAlma\tTHE BOOK OF ALMA\n"
"book\tHelaman\tTHE BOOK OF HELAMAN\n"
"book\t3 Nephi\tTHIRD BOOK OF NEPHI\n"
"book\t4 Nephi\tFOURTH NEPHI\n"
"book\tMormon\tTHE BOOK OF MORMON\n"
"book\tEther\tTHE BOOK OF ETHER\n"
"book\tMoroni\tTHE BOOK OF MORONI\n";

/* The address of an ebook's plain text in UTF-8, from its number, and of
   its page (title, last update, copyright status); as add-gutenberg.sh */
#define GB_PLAIN_TEXT "https://www.gutenberg.org/ebooks/%s.txt.utf-8"
#define GB_PAGE "https://www.gutenberg.org/ebooks/%s"
#define GB_LICENSE "https://www.gutenberg.org/policy/license.html"
/* The layout of the books written from a text (add-gutenberg.sh's LAYOUT):
     2: November 5, 2026: notes marked with a "-" */
#define GB_LAYOUT "2"

/* ------------------------------------------------------------------------
** Strings, as awk has them: made as needed, all let go of at once (gb_free)
*/
static char **gb_made = NULL;
static int gb_nmade = 0, gb_made_size = 0;

static char *gb_keep(char *s)
{
        if (gb_nmade == gb_made_size)
        {
                gb_made_size = gb_made_size * 2 + 1024;
                gb_made = (char **) realloc(gb_made, sizeof(char *) * (size_t) gb_made_size);
        }
        gb_made[gb_nmade++] = s;
        return (s);
}

static void gb_free(void)
{
        int i;

        for (i = 0; i < gb_nmade; i++)
                free(gb_made[i]);
        gb_nmade = 0;
}

/* s's n bytes */
static char *gb_n(const char *s, size_t n)
{
        char *t = (char *) malloc(n + 1);

        memcpy(t, s, n);
        t[n] = '\0';
        return (gb_keep(t));
}

static char *gb_s(const char *s)
{
        return (gb_n(s, strlen(s)));
}

/* a joined to b */
static char *gb_cat(const char *a, const char *b)
{
        size_t na = strlen(a), nb = strlen(b);
        char *t = (char *) malloc(na + nb + 1);

        memcpy(t, a, na);
        memcpy(t + na, b, nb + 1);
        return (gb_keep(t));
}

static char *gb_cat3(const char *a, const char *b, const char *c)
{
        return (gb_cat(gb_cat(a, b), c));
}

/* awk's substr(s, m, n), m from 1; n < 0: to the end */
static char *gb_sub(const char *s, long m, long n)
{
        long len = (long) strlen(s), end;

        if (n < 0)
                n = len + 1;
        end = m + n;            /* (one past the last, from 1) */
        if (m < 1)
                m = 1;
        if (end > len + 1)
                end = len + 1;
        if (end <= m)
                return (gb_s(""));
        return (gb_n(s + m - 1, (size_t) (end - m)));
}

/* awk's index(s, t): from 1; 0 if not there */
static long gb_index(const char *s, const char *t)
{
        const char *p = strstr(s, t);

        return (p != NULL ? (long) (p - s) + 1 : 0);
}

static int gb_starts(const char *line, const char *s)
{
        return (strncmp(line, s, strlen(s)) == 0);
}

static char *gb_num(long n)
{
        char b[32];

        sprintf(b, "%ld", n);
        return (gb_s(b));
}

/* awk's s + 0: the number its start is (0 if none) */
static long gb_value(const char *s)
{
        while (*s == ' ' || *s == '\t' || *s == '\n')
                s++;
        return (strtol(s, NULL, 10));
}

/*
** plain(s): white space as single spaces, with none at either end; every
** other character as the ebook has it (add-gutenberg.sh's own plain(),
** not importer.sh's: a no-break space, tab, CR, VT and FF become spaces)
*/
static char *gb_plain(const char *s)
{
        char *t = gb_s(s), *r, *w;

        for (r = w = t; *r != '\0'; )
        {
                if (UCHAR(r[0]) == 0302 && UCHAR(r[1]) == 0240)
                {
                        *w++ = ' ';
                        r += 2;
                }
                else if (*r == '\t' || *r == '\r' || *r == '\v' || *r == '\f')
                {
                        *w++ = ' ';
                        r++;
                }
                else
                        *w++ = *r++;
        }
        *w = '\0';
        for (r = w = t; *r != '\0'; r++)        /* (runs of spaces as one) */
                if (!(*r == ' ' && w > t && w[-1] == ' '))
                        *w++ = *r;
        *w = '\0';
        if (t[0] == ' ')
                memmove(t, t + 1, strlen(t));
        if ((w = t + strlen(t)) > t && w[-1] == ' ')
                w[-1] = '\0';
        return (t);
}

/* the characters of s (UTF-8: its bytes but continuation bytes) */
static long gb_chars(const char *s)
{
        long n = 0;

        for (; *s != '\0'; s++)
                if ((UCHAR(*s) & 0xC0) != 0x80)
                        n++;
        return (n);
}

/* the first word: from the first character not a space, to the next space */
static char *gb_first_word(const char *s)
{
        while (*s == ' ')
                s++;
        return (gb_n(s, strcspn(s, " ")));
}

/* a book's file name: its name with only letters and digits */
static char *gb_file_of(const char *name)
{
        char *t = gb_s(name), *r, *w;

        for (r = w = t; *r != '\0'; r++)
                if (isalnum(UCHAR(*r)) && UCHAR(*r) < 128)
                        *w++ = *r;
        *w = '\0';
        return (t);
}

/* line with spaces and tabs at either end left out */
static char *gb_trim(const char *line)
{
        size_t n;

        while (*line == ' ' || *line == '\t')
                line++;
        n = strlen(line);
        while (n > 0 && (line[n - 1] == ' ' || line[n - 1] == '\t'))
                n--;
        return (gb_n(line, n));
}

/* ------------------------------------------------------------------------
** A growing list of strings (an awk array numbered from 1)
*/
typedef struct
{
        char **v;               /* v[1..n] */
        long n, size;
} GbList;

static void gb_put_at(GbList *l, long i, char *s)
{
        if (i >= l->size)
        {
                long k = l->size;

                l->size = i * 2 + 64;
                l->v = (char **) realloc(l->v, sizeof(char *) * (size_t) l->size);
                for (; k < l->size; k++)
                        l->v[k] = NULL;
        }
        l->v[i] = s;
        if (i > l->n)
                l->n = i;
}

static void gb_push(GbList *l, char *s)
{
        gb_put_at(l, l->n + 1, s);
}

static char *gb_at(GbList *l, long i)
{
        return (i >= 1 && i < l->size && l->v[i] != NULL ? l->v[i] : (char *) "");
}

static void gb_list_free(GbList *l)
{
        free(l->v);
        l->v = NULL;
        l->n = l->size = 0;
}

/* ------------------------------------------------------------------------
** The text, as the split functions read it: the lines between Gutenberg's
** START and END lines ("*** START OF ..."), as they are (no carriage
** returns: taken out when downloaded)
*/
static GbList gb_body;

static int gb_read_body(const char *path)
{
        FILE *f;
        Str line = { NULL, 0, 0 };
        int c, body = FALSE, any;

        gb_list_free(&gb_body);
        if ((f = fopen(path, "r")) == NULL)
                return (-1);
        for (;;)
        {
                str_clear(&line);
                any = FALSE;
                while ((c = getc(f)) != EOF && c != '\n')
                {
                        char ch = (char) c;

                        if (c != '\r')
                                str_add(&line, &ch, 1);
                        any = TRUE;
                }
                if (c == EOF && !any)
                        break;
                if (line.s == NULL)
                        str_add(&line, "", 0);
                if (strncmp(line.s, "*** START OF ", 13) == 0)
                        body = TRUE;
                else if (strncmp(line.s, "*** END OF ", 11) == 0)
                        body = FALSE;
                else if (body)
                        gb_push(&gb_body, gb_s(line.s));
                if (c == EOF)
                        break;
        }
        free(line.s);
        fclose(f);
        return (0);
}

/* The text's book (and other) lines, from KNOWN */
typedef struct
{
        char kind[16];          /* "book", "text", "translation" */
        char *a, *b;            /* its name and heading; or key and name */
} GbDetail;

static GbDetail gb_detail[200];
static int gb_ndetail = 0;

/* the text's line of KNOWN: number, names, format, title, language; FALSE
   if it isn't there. With details, its book lines into gb_detail. */
static int gb_known_line(const char *number, char **f, int details)
{
        const char *p = gb_known, *e;
        int this = FALSE, found = FALSE, n, k;
        char *line, *field[8];

        gb_ndetail = 0;
        while (*p != '\0')
        {
                e = strchr(p, '\n');
                line = gb_n(p, (size_t) (e - p));
                p = e + 1;
                /* (its fields, by tab) */
                n = 0;
                field[n++] = line;
                for (k = 0; line[k] != '\0'; k++)
                        if (line[k] == '\t' && n < 8)
                        {
                                line[k] = '\0';
                                field[n++] = line + k + 1;
                        }
                for (; n < 8; n++)
                        field[n] = (char *) "";
                if (all_digits(field[0]))
                {
                        /* (awk's $1 == n: as numbers, if both are) */
                        this = all_digits(number) ? strtol(field[0], NULL, 10) == strtol(number, NULL, 10)
                                                  : strcmp(field[0], number) == 0;
                        if (this)
                        {
                                for (k = 0; k < 5; k++)
                                        f[k] = field[k];
                                found = TRUE;
                        }
                        continue;
                }
                if (this && details && gb_ndetail < 200 &&
                    (strcmp(field[0], "book") == 0 || strcmp(field[0], "text") == 0 ||
                     strcmp(field[0], "front") == 0 || strcmp(field[0], "translation") == 0))
                {
                        sprintf(gb_detail[gb_ndetail].kind, "%.15s", field[0]);
                        gb_detail[gb_ndetail].a = field[1];
                        gb_detail[gb_ndetail].b = field[2];
                        gb_ndetail++;
                }
        }
        return (found);
}

/* ------------------------------------------------------------------------
** Writing a text's folders: each book's file, and booklist and booknames,
** opened once and written on (awk's print > file)
*/
typedef struct
{
        char path[1400];
        FILE *f;
} GbOut;

static GbOut gb_outs[16];
static int gb_nouts = 0;

static FILE *gb_out(const char *path)
{
        int i;

        for (i = 0; i < gb_nouts; i++)
                if (gb_outs[i].f != NULL && strcmp(gb_outs[i].path, path) == 0)
                        return (gb_outs[i].f);
        for (i = 0; i < gb_nouts && gb_outs[i].f != NULL; i++)
                ;
        if (i == gb_nouts)
        {
                if (gb_nouts == 16)
                        return (NULL);
                gb_nouts++;
        }
        sprintf(gb_outs[i].path, "%.1390s", path);
        gb_outs[i].f = fopen(path, "w");
        return (gb_outs[i].f);
}

static void gb_close(const char *path)
{
        int i;

        for (i = 0; i < gb_nouts; i++)
                if (gb_outs[i].f != NULL && strcmp(gb_outs[i].path, path) == 0)
                {
                        fclose(gb_outs[i].f);
                        gb_outs[i].f = NULL;
                }
}

static void gb_close_all(void)
{
        int i;

        for (i = 0; i < gb_nouts; i++)
                if (gb_outs[i].f != NULL)
                {
                        fclose(gb_outs[i].f);
                        gb_outs[i].f = NULL;
                }
        gb_nouts = 0;
}

static void gb_print(const char *path, const char *a, const char *b)
{
        FILE *f = gb_out(path);

        if (f != NULL)
                fprintf(f, "%s%s\n", a, b);
}

/* A split's result: its problem (awk's fail(): the first only), or the
   books and verses written */
static int gb_failed;
static long gb_ok_books, gb_ok_verses;
static jmp_buf gb_stop;

static void gb_fail(const char *m)
{
        if (!gb_failed)
                sprintf(problem, "%.4000s", m);
        gb_failed = TRUE;
        longjmp(gb_stop, 1);
}

/* the width the text was wrapped at: as long as all but the longest 1 in
   100 of its lines (so that a line or two longer, as of a table, doesn't
   count) */
static long gb_wrap_width(GbList *text)
{
        static long count[1001];
        long i, n = 0, under = 0, wrap, len;

        memset(count, 0, sizeof(count));
        for (i = 1; i <= text->n; i++)
                if (gb_at(text, i)[0] != '\0')
                {
                        len = gb_chars(gb_at(text, i));
                        if (len <= 1000)
                                count[len]++;
                        n++;
                }
        for (wrap = 0; wrap < 1000; wrap++)
        {
                under += count[wrap];
                if (under >= n * 0.99)
                        break;
        }
        return (wrap);
}

/* ------------------------------------------------------------------------
** The "chapters" format (split_chapters in add-gutenberg.sh): books start
** with their heading (and a summary); a book of several chapters has a
** line per chapter, "Alma Chapter 5"; verses are "C:V text", wrapped, a
** blank line between them, and may start in the middle of a line, with
** the book's name before them or not.
*/
static struct
{
        char *folder;
        FILE *out;
        char *outpath;
        long b, nb, verses, wrap, chap, vnum, cur_c, cur_v;
        long mstart, mlen;
        int have, heading_mode;
        char *buf;
        GbList intro, rows;
        char *bookname[200], *heading[200], *kind[200];
        long counted[200];
} gc;

static void gc_put(long c, long v, const char *text)
{
        char *t = gb_plain(text);

        if (t[0] == '\0' && c > 0)
                return;
        if (gc.out != NULL)
        {
                if (t[0] == '\0')
                        fprintf(gc.out, "[%ld:%ld]\n", c, v);
                else
                        fprintf(gc.out, "[%ld:%ld] %s\n", c, v, t);
        }
        gc.verses++;
        if (c > 0 && v > 0)
                gc.counted[gc.b]++;
}

/* Chapter 0: the lines before a book's chapter 1 (intro), as rows, each a
   verse: a line goes on into the next only if it was wrapped there (the
   next line's first word not fitting after it within the text's width);
   else it ends its row. A blank line is a blank row; those at the start
   and end go. plain_each: (the paragraphs format) each row made plain
   first, and put as it is. */
static void gb_put_intro(GbList *intro, long wrap, void (*put)(long, long, const char *),
                         int plain_each)
{
        GbList rows = { NULL, 0, 0 };
        long i, n = 0, k, first, last;
        char *row;

        for (i = 1; i <= intro->n; i++)
        {
                if (gb_at(intro, i)[0] == '\0')
                {
                        gb_put_at(&rows, ++n, gb_s(""));
                        continue;
                }
                row = gb_at(intro, i);
                while (i < intro->n && gb_at(intro, i + 1)[0] != '\0' &&
                       gb_chars(gb_at(intro, i)) + 1 + gb_chars(gb_first_word(gb_at(intro, i + 1))) > wrap)
                {
                        i++;
                        row = gb_cat3(row, " ", gb_at(intro, i));
                }
                gb_put_at(&rows, ++n, row);
        }
        for (first = 1; first <= n && gb_at(&rows, first)[0] == '\0'; first++)
                ;
        for (last = n; last >= first && gb_at(&rows, last)[0] == '\0'; last--)
                ;
        k = 0;
        for (i = first; i <= last; i++)
                put(0, ++k, plain_each ? gb_plain(gb_at(&rows, i)) : gb_at(&rows, i));
        gb_list_free(&rows);
        intro->n = 0;
}

static void gc_flush(void)
{
        if (gc.have)
                gc_put(gc.cur_c, gc.cur_v, gc.buf);
        gc.have = FALSE;
        gc.buf = gb_s("");
}

static void gc_add(const char *text)
{
        if (text[0] == '\0')
                return;
        gc.buf = gc.buf[0] == '\0' ? gb_s(text) : gb_cat3(gc.buf, " ", text);
}

static void gc_start_book(long k)
{
        char *f;

        gc_flush();
        if (gc.outpath != NULL)
        {
                gb_close(gc.outpath);
                gc.out = NULL;
        }
        gc.b = k;
        f = gb_file_of(gc.bookname[k]);
        gc.outpath = gb_cat3(gc.folder, "/", f);
        gc.out = gb_out(gc.outpath);
        gb_print(gb_cat(gc.folder, "/booklist"), f, "");
        gb_print(gb_cat(gc.folder, "/booknames"), gc.bookname[k], "");
        gc.heading_mode = TRUE;
        gc.chap = gc.vnum = 0;
        gc.have = FALSE;
        gc.buf = gb_s("");
        gc.intro.n = 0;
}

/* the next verse expected ("C:V") in line, at a word: where it starts (and
   in mlen how long it is), with the book's name before it, if it is */
static long gc_find_verse(const char *line, long c, long v)
{
        char pat[64], *pre;
        long from = 1, i, j, n, len = (long) strlen(line), lp, plen;

        sprintf(pat, "%ld:%ld", c, v);
        plen = (long) strlen(pat);
        while ((i = gb_index(line + from - 1, pat)) > 0)
        {
                j = from + i - 1;
                n = j + plen;
                if ((j == 1 || line[j - 2] == ' ') && (n > len || line[n - 1] == ' '))
                {
                        pre = gb_cat(gc.bookname[gc.b], " ");
                        lp = (long) strlen(pre);
                        gc.mstart = j;
                        gc.mlen = plen;
                        if (j > lp && strncmp(line + j - lp - 1, pre, (size_t) lp) == 0 &&
                            (j - lp == 1 || line[j - lp - 2] == ' '))
                        {
                                gc.mstart = j - lp;
                                gc.mlen += lp;
                        }
                        return (gc.mstart);
                }
                from = j + 1;
        }
        return (0);
}

/* the rest of a line of verses: split where the next verses start */
static void gc_verses_in(const char *line)
{
        long m, nc, nv;

        for (;;)
        {
                nc = gc.chap;
                nv = gc.vnum + 1;
                if (gc.chap == 0)
                {
                        nc = 1;
                        nv = 1;
                }
                m = gc_find_verse(line, nc, nv);
                if (m == 0)
                {
                        gc_add(line);
                        return;
                }
                if (gc.chap == 0)
                {
                        /* verse 1:1: what came before it is chapter 0 */
                        if (m > 1)
                                gb_push(&gc.intro, gb_sub(line, 1, m - 1));
                        gb_put_intro(&gc.intro, gc.wrap, gc_put, FALSE);
                        gc.chap = 1;
                }
                else
                        gc_add(gb_sub(line, 1, m - 1));
                gc_flush();
                gc.vnum = nv;
                gc.cur_c = gc.chap;
                gc.cur_v = gc.vnum;
                gc.have = TRUE;
                line = gb_sub(line, m + gc.mlen, -1);
                while (*line == ' ')
                        line++;
                gc.heading_mode = FALSE;
        }
}

/* the start of a line: "1:1 " or "D Word 1:1 " (a digit, a word) */
static int gc_first_verse_line(const char *s)
{
        const char *p = s;

        if (strncmp(s, "1:1 ", 4) == 0)
                return (TRUE);
        if (!isdigit(UCHAR(p[0])) || p[1] != ' ')
                return (FALSE);
        p += 2;
        if (!isalpha(UCHAR(*p)))
                return (FALSE);
        while (isalpha(UCHAR(*p)))
                p++;
        return (*p == ' ' && strncmp(p + 1, "1:1 ", 4) == 0);
}

static void gb_split_chapters(const char *folder)
{
        GbList text = { NULL, 0, 0 };
        long i, k, first_verse = 0, book1 = 0, lines;
        char msg[4600], *line, *chap_line;

        memset(&gc, 0, sizeof(gc));
        gc.folder = gb_s(folder);
        gc.buf = gb_s("");
        for (k = 0; k < gb_ndetail; k++)
                if (strcmp(gb_detail[k].kind, "book") == 0 && gc.nb < 199)
                {
                        gc.nb++;
                        gc.bookname[gc.nb] = gb_detail[k].a;
                        gc.heading[gc.nb] = gb_detail[k].b;
                }
        for (i = 1; i <= gb_body.n; i++)
                gb_push(&text, gb_trim(gb_at(&gb_body, i)));
        lines = text.n;
        if (setjmp(gb_stop) != 0)
        {
                gb_list_free(&text);
                gb_list_free(&gc.intro);
                return;
        }
        if (lines == 0)
                gb_fail("no text between Gutenberg's START and END lines");
        gc.wrap = gb_wrap_width(&text);
        /* Where book 1 starts: its heading's last line before the first
           verse (the Contents list names it too) */
        for (i = 1; i <= lines; i++)
                if (gc_first_verse_line(gb_at(&text, i)))
                {
                        first_verse = i;
                        break;
                }
        if (!first_verse)
                gb_fail("no verse 1:1 in it");
        for (i = first_verse; i >= 1; i--)
                if (gb_starts(gb_at(&text, i), gc.heading[1]))
                {
                        book1 = i;
                        break;
                }
        if (!book1)
        {
                sprintf(msg, "no heading \"%.4000s\" before its first verse", gc.heading[1]);
                gb_fail(msg);
        }
        /* Book 1: its chapter 0 starts with the front matter */
        gc_start_book(1);
        for (i = 1; i < book1; i++)
                gb_push(&gc.intro, gb_at(&text, i));
        for (i = book1; i <= lines; i++)
        {
                if ((i & 255) == 0)
                        let_page_draw(FALSE);
                line = gb_at(&text, i);
                if (line[0] == '\0')
                {
                        if (gc.heading_mode)
                                gb_push(&gc.intro, gb_s(""));   /* (a blank row) */
                        continue;
                }
                /* the next book: its heading, once this one has verses */
                if (gc.b < gc.nb && !gc.heading_mode && gb_starts(line, gc.heading[gc.b + 1]))
                {
                        gc_start_book(gc.b + 1);
                        gb_push(&gc.intro, line);
                        continue;
                }
                /* a line numbering a chapter: "Alma Chapter 5" (dropped: the
                   chapter number is the verses') */
                if (strcmp(line, gb_cat3(gc.bookname[gc.b], " Chapter ", gb_num(gc.chap + 1))) == 0 ||
                    (gc.chap == 0 && strcmp(line, gb_cat(gc.bookname[gc.b], " Chapter 1")) == 0))
                        continue;
                chap_line = gb_cat(gc.bookname[gc.b], " Chapter ");
                if (gb_starts(line, chap_line))
                {
                        /* (awk: line ~ / Chapter [0-9]+$/) */
                        const char *p = strstr(line, " Chapter "), *q = NULL;

                        for (; p != NULL; p = strstr(p + 1, " Chapter "))
                                q = p;
                        q += 9;
                        if (isdigit(UCHAR(*q)))
                        {
                                while (isdigit(UCHAR(*q)))
                                        q++;
                                if (*q == '\0')
                                {
                                        sprintf(msg, "\"%.2000s\" out of order (in %.1000s %ld)",
                                                line, gc.bookname[gc.b], gc.chap);
                                        gb_fail(msg);
                                }
                        }
                }
                if (gc.heading_mode)
                {
                        /* its heading and summary, until verse 1:1 */
                        if (gc_find_verse(line, 1, 1) == 0)
                        {
                                gb_push(&gc.intro, line);
                                continue;
                        }
                }
                /* (a new chapter: the verse after it is C:1) */
                if (!gc.heading_mode && gc_find_verse(line, gc.chap + 1, 1) == 1 &&
                    gc_find_verse(line, gc.chap, gc.vnum + 1) == 0)
                {
                        gc_flush();
                        gc.chap++;
                        gc.vnum = 0;
                }
                gc_verses_in(line);
        }
        gc_flush();
        if (gc.b != gc.nb)
        {
                sprintf(msg, "found %ld of its %ld books (the next: \"%.4000s\")",
                        gc.b, gc.nb, gc.heading[gc.b + 1]);
                gb_fail(msg);
        }
        for (k = 1; k <= gc.nb; k++)
                if (!gc.counted[k])
                {
                        sprintf(msg, "%.4000s has no verses", gc.bookname[k]);
                        gb_fail(msg);
                }
        gb_ok_books = gc.nb;
        gb_ok_verses = gc.verses;
        gb_list_free(&text);
        gb_list_free(&gc.intro);
}

/* ------------------------------------------------------------------------
** The "paragraphs" format (split_paragraphs in add-gutenberg.sh): books
** start with their heading (and an introduction); a chapter starts with
** "Genesis Chapter 1" (its summary after it); a verse is a paragraph,
** "1:1. In the beginning...", and the paragraphs between verses are notes
** on the verse before them, or headings (in capitals) of the verse after
** them. A "text" book (an appendix) is rows only, as chapter 0.
*/
static struct
{
        char *folder, *outpath;
        FILE *out;
        long b, nb, verses, wrap, chap, vnum;
        long notes, renumbered, repeated, gaps;
        int have, intro_mode, wrote0;
        char *vb, *pre, *pend;
        GbList intro, para, saved;
        long np, saved_np;
        char *bookname[200], *heading[200], *kind[200];
        long counted[200];
} gp;

static void gp_put(long c, long v, const char *text)
{
        if (text[0] == '\0' && c > 0)
                return;
        if (gp.out != NULL)
                fprintf(gp.out, "[%ld:%ld]%s%s\n", c, v, text[0] == '\0' ? "" : " ", text);
        gp.verses++;
        if (c > 0 && v > 0)
                gp.counted[gp.b]++;
}

/* (a tab on its own between spaces: EBaC and its index read words between
   spaces, so a word is never joined to the tab) */
static char *gp_part(const char *s, const char *p)
{
        return (s[0] == '\0' ? gb_s(p) : gb_cat3(s, " \t ", p));
}

static void gp_flush_verse(void)
{
        if (gp.have)
                gp_put(gp.chap, gp.vnum, gp.vb);
        gp.have = FALSE;
        gp.vb = gb_s("");
}

static void gp_start_book(long k)
{
        char *f, *bits, *tab;

        if (gp.b > 0)
        {
                gp_flush_verse();
                if (gp.intro_mode)
                        gb_put_intro(&gp.intro, gp.wrap, gp_put, TRUE);
                gb_close(gp.outpath);
                gp.out = NULL;
        }
        gp.b = k;
        f = gb_file_of(gp.bookname[k]);
        gp.outpath = gb_cat3(gp.folder, "/", f);
        gp.out = gb_out(gp.outpath);
        gb_print(gb_cat(gp.folder, "/booklist"), f, "");
        gb_print(gb_cat(gp.folder, "/booknames"), gp.bookname[k], "");
        gp.intro_mode = TRUE;
        gp.chap = gp.vnum = 0;
        gp.have = FALSE;
        gp.vb = gb_s("");
        gp.pre = gb_s("");
        gp.intro.n = 0;
        if (gp.pend[0] != '\0')         /* (a heading of this book) */
        {
                /* (awk: split(pend, bits, / \t /)) */
                bits = gp.pend;
                for (;;)
                {
                        tab = strstr(bits, " \t ");
                        gb_push(&gp.intro, tab != NULL ? gb_n(bits, (size_t) (tab - bits)) : gb_s(bits));
                        gb_push(&gp.intro, gb_s(""));
                        if (tab == NULL)
                                break;
                        bits = tab + 3;
                }
                gp.pend = gb_s("");
        }
}

static int gp_chapter_heading(const char *line, long n)
{
        char *h = gb_cat3(gp.bookname[gp.b], " Chapter ", gb_num(n));

        return (strcmp(line, h) == 0 || strcmp(line, gb_cat(h, ".")) == 0);
}

static int gp_heading_like(const char *t)
{
        const char *p;
        int upper = FALSE;

        for (p = t; *p != '\0'; p++)
        {
                if (*p >= 'a' && *p <= 'z')
                        return (FALSE);
                if (*p >= 'A' && *p <= 'Z')
                        upper = TRUE;
        }
        return (upper && strlen(t) <= 60);
}

/* awk: match(p, /^[0-9]+a?:[0-9]+[.:]? ?/): its length (RLENGTH), or -1 */
static long gp_verse_label(const char *p)
{
        const char *q = p;

        if (!isdigit(UCHAR(*q)))
                return (-1);
        while (isdigit(UCHAR(*q)))
                q++;
        if (*q == 'a')
                q++;
        if (*q != ':' || !isdigit(UCHAR(q[1])))
                return (-1);
        q++;
        while (isdigit(UCHAR(*q)))
                q++;
        if (*q == '.' || *q == ':')
                q++;
        if (*q == ' ')
                q++;
        return ((long) (q - p));
}

/* a paragraph (para) of a book with chapters, once its chapter 1 has
   begun; 2 if it began a new book */
static int gp_paragraph(void)
{
        char *f, *p, *rest, *lab, *num, *t, msg[4600], *c;
        long i, n, len, colon;

        f = gb_at(&gp.para, 1);
        p = gb_at(&gp.para, 1);
        for (i = 2; i <= gp.np; i++)
                p = gb_cat3(p, " ", gb_at(&gp.para, i));
        gp.np = 0;
        gp.para.n = 0;
        if (gp.b < gp.nb && gb_starts(f, gp.heading[gp.b + 1]))
        {
                gp_start_book(gp.b + 1);
                gb_push(&gp.intro, f);
                return (2);             /* (the rest of it: its introduction) */
        }
        if (gp_chapter_heading(f, gp.chap + 1))
        {
                gp_flush_verse();
                gp.chap++;
                gp.vnum = 0;
                gp.pre = gp.pend;
                gp.pend = gb_s("");
                gp.wrote0 = FALSE;
                rest = gb_sub(p, (long) strlen(f) + 2, -1);
                if (rest[0] != '\0')
                        gp.pre = gp_part(gp.pre, gb_plain(rest));
                return (0);
        }
        /* (awk: f ~ / Chapter [0-9]+\.?$/ && starts(f, bookname[b] " Chapter ")) */
        if (gb_starts(f, gb_cat(gp.bookname[gp.b], " Chapter ")))
        {
                const char *q = NULL, *s;

                for (s = strstr(f, " Chapter "); s != NULL; s = strstr(s + 1, " Chapter "))
                        q = s;
                q += 9;
                if (isdigit(UCHAR(*q)))
                {
                        while (isdigit(UCHAR(*q)))
                                q++;
                        if (*q == '.')
                                q++;
                        if (*q == '\0')
                        {
                                sprintf(msg, "\"%.2000s\" out of order (in %.1000s %ld)",
                                        f, gp.bookname[gp.b], gp.chap);
                                gb_fail(msg);
                        }
                }
        }
        if ((len = gp_verse_label(p)) >= 0)
        {
                colon = gb_index(p, ":");
                lab = gb_sub(p, 1, colon - 1);
                num = gb_sub(p, colon + 1, len - colon);
                for (c = t = num; *c != '\0'; c++)
                        if (isdigit(UCHAR(*c)))
                                *t++ = *c;
                *t = '\0';
                rest = gb_sub(p, len + 1, -1);
                if (strcmp(lab, gb_num(gp.chap)) != 0 && strcmp(lab, gb_cat(gb_num(gp.chap), "a")) != 0)
                {
                        sprintf(msg, "verse %.100s:%.100s in %.1000s chapter %ld", lab, num,
                                gp.bookname[gp.b], gp.chap);
                        gb_fail(msg);
                }
                n = gb_value(num);
                if (strcmp(lab, gb_num(gp.chap)) == 0 && n == gp.vnum && gp.have)
                {
                        /* the same number again: more of that verse, on a
                           line of its own */
                        gp.vb = gp_part(gp.vb, gb_plain(rest));
                        gp.repeated++;
                        return (0);
                }
                if (strcmp(lab, gb_num(gp.chap)) != 0 || n < gp.vnum)
                {
                        n = gp.vnum + 1;
                        gp.renumbered++;
                }
                else if (n > gp.vnum + 1)
                        gp.gaps++;
                if (gp.vnum == 0)
                {
                        if (gp.pre[0] != '\0')
                        {
                                gp_put(gp.chap, 0, gp.pre);
                                gp.pre = gb_s("");
                                gp.wrote0 = TRUE;
                        }
                        if (n > 1 && !gp.wrote0)
                        {
                                sprintf(msg, "%.1000s %ld starts at verse %ld, with nothing before it",
                                        gp.bookname[gp.b], gp.chap, n);
                                gb_fail(msg);
                        }
                }
                gp_flush_verse();
                t = gb_plain(rest);
                gp.vb = gp.pend[0] != '\0' ? gb_cat3(gp.pend, " \t ", t) : t;
                gp.pend = gb_s("");
                gp.vnum = n;
                gp.have = TRUE;
                return (0);
        }
        t = gb_plain(p);
        if (gp.vnum == 0)
        {
                gp.pre = gp_part(gp.pre, t);
                return (0);
        }
        if (gp_heading_like(t) || gp.pend[0] != '\0')
        {
                gp.pend = gp_part(gp.pend, t);
                gp.notes++;
                return (0);
        }
        /* (a note: "-" first, alone, so it is shown with "---" in the
           margin and not spoken) */
        gp.vb = gp_part(gp.vb, gb_cat("- ", t));
        gp.notes++;
        return (0);
}

static void gb_split_paragraphs(const char *folder)
{
        GbList text = { NULL, 0, 0 };
        long i, k, book1 = 0, lines;
        char msg[4600], *line;

        memset(&gp, 0, sizeof(gp));
        gp.folder = gb_s(folder);
        gp.vb = gp.pre = gp.pend = gb_s("");
        for (k = 0; k < gb_ndetail; k++)
                if ((strcmp(gb_detail[k].kind, "book") == 0 || strcmp(gb_detail[k].kind, "text") == 0) &&
                    gp.nb < 199)
                {
                        gp.nb++;
                        gp.kind[gp.nb] = gb_detail[k].kind;
                        gp.bookname[gp.nb] = gb_detail[k].a;
                        gp.heading[gp.nb] = gb_detail[k].b;
                }
        for (i = 1; i <= gb_body.n; i++)
                gb_push(&text, gb_trim(gb_at(&gb_body, i)));
        lines = text.n;
        if (setjmp(gb_stop) != 0)
        {
                gb_list_free(&text);
                gb_list_free(&gp.intro);
                gb_list_free(&gp.para);
                gb_list_free(&gp.saved);
                return;
        }
        if (lines == 0)
                gb_fail("no text between Gutenberg's START and END lines");
        gp.wrap = gb_wrap_width(&text);
        for (i = 1; i <= lines; i++)
                if (strcmp(gb_at(&text, i), gp.heading[1]) == 0)
                {
                        book1 = i;
                        break;
                }
        if (!book1)
        {
                sprintf(msg, "no heading \"%.4000s\"", gp.heading[1]);
                gb_fail(msg);
        }
        gp_start_book(1);
        for (i = 1; i < book1; i++)
                gb_push(&gp.intro, gb_at(&text, i));
        for (i = book1; i <= lines; i++)
        {
                if ((i & 255) == 0)
                        let_page_draw(FALSE);
                line = gb_at(&text, i);
                if (gp.intro_mode)
                {
                        if (gp.b < gp.nb && strcmp(gp.kind[gp.b], "text") == 0 &&
                            gb_starts(line, gp.heading[gp.b + 1]))
                        {
                                gp_start_book(gp.b + 1);
                                gb_push(&gp.intro, line);
                                continue;
                        }
                        if (strcmp(gp.kind[gp.b], "book") == 0 && gp_chapter_heading(line, 1))
                        {
                                gb_put_intro(&gp.intro, gp.wrap, gp_put, TRUE);
                                gp.intro_mode = FALSE;
                                gp.np = 1;
                                gp.para.n = 0;
                                gb_put_at(&gp.para, 1, line);
                                continue;
                        }
                        gb_push(&gp.intro, line);
                        continue;
                }
                if (line[0] == '\0')
                {
                        if (gp.np)
                        {
                                if (gp_paragraph() == 2)
                                {
                                        /* (a new book: the rest of its heading
                                           paragraph is its introduction) */
                                        for (k = 2; k <= gp.saved_np; k++)
                                                gb_push(&gp.intro, gb_at(&gp.saved, k));
                                        gb_push(&gp.intro, gb_s(""));
                                }
                        }
                        continue;
                }
                gp.np++;
                gb_put_at(&gp.para, gp.np, line);
                gp.saved_np = gp.np;
                gb_put_at(&gp.saved, gp.np, line);
        }
        if (gp.np && !gp.intro_mode)
                gp_paragraph();
        if (gp.have && gp.pend[0] != '\0')
        {
                gp.vb = gp_part(gp.vb, gp.pend);
                gp.pend = gb_s("");
        }
        gp_flush_verse();
        if (gp.intro_mode)
                gb_put_intro(&gp.intro, gp.wrap, gp_put, TRUE);
        if (gp.b != gp.nb)
        {
                sprintf(msg, "found %ld of its %ld books (the next: \"%.4000s\")",
                        gp.b, gp.nb, gp.heading[gp.b + 1]);
                gb_fail(msg);
        }
        for (k = 1; k <= gp.nb; k++)
                if (strcmp(gp.kind[k], "book") == 0 && !gp.counted[k])
                {
                        sprintf(msg, "%.4000s has no verses", gp.bookname[k]);
                        gb_fail(msg);
                }
        fflush(stdout);
        fprintf(stderr, "notes %ld, numbered on %ld, repeated %ld, gaps %ld\n",
                gp.notes, gp.renumbered, gp.repeated, gp.gaps);
        gb_ok_books = gp.nb;
        gb_ok_verses = gp.verses;
        gb_list_free(&text);
        gb_list_free(&gp.intro);
        gb_list_free(&gp.para);
        gb_list_free(&gp.saved);
}

/* ------------------------------------------------------------------------
** The "verses" format (split_verses in add-gutenberg.sh): each book starts
** with a line "Book 01 Genesis" (its name from there); a verse is
** "001:001 text", its wrapped lines indented under it; anything before
** the first book is its chapter 0, a row a line.
*/
static void gv_put(FILE *out, long c, long v, const char *text, long *verses)
{
        if (out != NULL)
        {
                if (text[0] == '\0')
                        fprintf(out, "[%ld:%ld]\n", c, v);
                else
                        fprintf(out, "[%ld:%ld] %s\n", c, v, text);
        }
        (*verses)++;
}

/* awk's $n (fields between spaces and tabs), from 1 */
static char *gb_field(const char *line, int n)
{
        const char *p = line;
        size_t len;

        for (;;)
        {
                while (*p == ' ' || *p == '\t')
                        p++;
                if (*p == '\0')
                        return (gb_s(""));
                len = strcspn(p, " \t");
                if (--n == 0)
                        return (gb_n(p, len));
                p += len;
        }
}

static GbList gv_front;

static void gv_split(const char *folder)
{
        GbList *frontp = &gv_front;
        long b = 0, chap = 0, vnum = 0, verses = 0, n, c, v, i, first, last, k;
        int have = FALSE;
        char *name = gb_s(""), *outpath = NULL, *vb = gb_s(""), *line, *f1, *p, *f;
        char msg[4600];
        FILE *out = NULL;

#define front (*frontp)
#define GV_FLUSH() { if (have) gv_put(out, chap, vnum, gb_plain(vb), &verses); have = FALSE; }
        for (k = 1; k <= gb_body.n; k++)
        {
                if ((k & 255) == 0)
                        let_page_draw(FALSE);
                line = gb_at(&gb_body, k);
                /* /^Book [0-9]+ / */
                if (strncmp(line, "Book ", 5) == 0 && isdigit(UCHAR(line[5])))
                {
                        for (p = line + 5; isdigit(UCHAR(*p)); p++)
                                ;
                        if (*p == ' ')
                        {
                                GV_FLUSH();
                                if (b > 0 && chap == 0)
                                {
                                        sprintf(msg, "%.4000s has no verses", name);
                                        gb_fail(msg);
                                }
                                n = gb_value(gb_field(line, 2));
                                if (n != b + 1)
                                {
                                        sprintf(msg, "book %ld after book %ld", n, b);
                                        gb_fail(msg);
                                }
                                b = n;
                                while (*p == ' ')
                                        p++;
                                name = gb_s(p);
                                for (i = (long) strlen(name); i > 0 && name[i - 1] == ' '; i--)
                                        name[i - 1] = '\0';
                                if (outpath != NULL)
                                        gb_close(outpath);
                                f = gb_file_of(name);
                                outpath = gb_cat3(folder, "/", f);
                                out = gb_out(outpath);
                                gb_print(gb_cat(folder, "/booklist"), f, "");
                                gb_print(gb_cat(folder, "/booknames"), name, "");
                                chap = vnum = 0;
                                if (b == 1)     /* (what came before: its chapter 0) */
                                {
                                        for (first = 1; first <= front.n && gb_at(&front, first)[0] == '\0'; first++)
                                                ;
                                        for (last = front.n; last >= first && gb_at(&front, last)[0] == '\0'; last--)
                                                ;
                                        for (i = first; i <= last; i++)
                                                gv_put(out, 0, i - first + 1, gb_plain(gb_at(&front, i)), &verses);
                                }
                                continue;
                        }
                }
                if (b == 0)
                {
                        gb_push(&front, line);
                        continue;
                }
                /* /^[0-9]+:[0-9]+ / */
                p = line;
                if (isdigit(UCHAR(*p)))
                {
                        while (isdigit(UCHAR(*p)))
                                p++;
                        if (*p == ':' && isdigit(UCHAR(p[1])))
                        {
                                p++;
                                while (isdigit(UCHAR(*p)))
                                        p++;
                                if (*p == ' ')
                                {
                                        GV_FLUSH();
                                        f1 = gb_field(line, 1);
                                        c = gb_value(gb_sub(f1, 1, gb_index(f1, ":") - 1));
                                        v = gb_value(gb_sub(f1, gb_index(f1, ":") + 1, -1));
                                        if (!((c == chap && v == vnum + 1) || (c == chap + 1 && v == 1)))
                                        {
                                                sprintf(msg, "verse %ld:%ld of %.4000s after %ld:%ld",
                                                        c, v, name, chap, vnum);
                                                gb_fail(msg);
                                        }
                                        chap = c;
                                        vnum = v;
                                        vb = gb_sub(line, (long) strlen(f1) + 2, -1);
                                        have = TRUE;
                                        continue;
                                }
                        }
                }
                /* /^ +[^ ]/: more of the verse */
                p = line;
                while (*p == ' ')
                        p++;
                if (p > line && *p != '\0')
                {
                        if (!have)
                        {
                                sprintf(msg, "a line of %.1000s outside a verse: %.3000s", name, line);
                                gb_fail(msg);
                        }
                        vb = gb_cat3(vb, " ", line);
                        continue;
                }
                if (*p == '\0')         /* /^ *$/ */
                        continue;
                sprintf(msg, "a line not understood in %.1000s: %.3000s", name, line);
                gb_fail(msg);
        }
        GV_FLUSH();
#undef GV_FLUSH
        if (b == 0)
                gb_fail("no books in the text");
        if (chap == 0)
        {
                sprintf(msg, "%.4000s has no verses", name);
                gb_fail(msg);
        }
        gb_ok_books = b;
        gb_ok_verses = verses;
#undef front
}

static void gb_split_verses(const char *folder)
{
        if (setjmp(gb_stop) == 0)
                gv_split(folder);
        gb_list_free(&gv_front);
}

/* ------------------------------------------------------------------------
** The "sidebyside" format (split_sidebyside in add-gutenberg.sh): several
** translations in one text, each verse a number "002.255" and then a
** paragraph per translation, each starting with its key ("Y: "); each
** translation into its own folder, here/NAME, as a text of its own. A
** chapter starts with " Chapter 2:", then its heading lines (its verse 0);
** the books are given by "book" lines, the first chapter of each with them.
*/
#define GS_MAX 8
static struct
{
        char *here;
        long nk, nb, b, chap, vnum, slips, paras;
        int have, heading_mode, brk;
        char *key[GS_MAX + 1], *tname[GS_MAX + 1], *title[GS_MAX + 1], *text[GS_MAX + 1];
        char *outpath[GS_MAX + 1];
        FILE *out[GS_MAX + 1];
        long count[GS_MAX + 1];
        int got[GS_MAX + 1];
        char *bookname[200];
        long bookstart[200];
        long pk;
        char *pb, *heading;
        GbList front, fkey;
} gs;

static long gs_which(const char *k)
{
        long i;

        for (i = 1; i <= gs.nk; i++)
                if (strcmp(gs.key[i], k) == 0)
                        return (i);
        return (0);
}

static void gs_put(long k, long c, long v, const char *text)
{
        if (gs.out[k] != NULL)
        {
                if (text[0] == '\0')
                        fprintf(gs.out[k], "[%ld:%ld]\n", c, v);
                else
                        fprintf(gs.out[k], "[%ld:%ld] %s\n", c, v, text);
        }
        gs.count[k]++;
}

static void gs_start_book(long n)
{
        long k;
        char *f, *dir;

        gs.b = n;
        for (k = 1; k <= gs.nk; k++)
        {
                if (gs.outpath[k] != NULL)
                        gb_close(gs.outpath[k]);
                f = gb_file_of(gs.bookname[gs.b]);
                dir = gb_cat3(gs.here, "/", gs.tname[k]);
                gs.outpath[k] = gb_cat3(dir, "/", f);
                gs.out[k] = gb_out(gs.outpath[k]);
                gb_print(gb_cat(dir, "/booklist"), f, "");
                gb_print(gb_cat(dir, "/booknames"), gs.bookname[gs.b], "");
        }
}

/* (each part plain, the tabs between them kept) */
static char *gs_plain_parts(const char *s)
{
        const char *tab;
        char *t;

        if (s[0] == '\0')
                return (gb_s(""));
        tab = strchr(s, '\t');
        t = gb_plain(tab != NULL ? gb_n(s, (size_t) (tab - s)) : s);
        while (tab != NULL)
        {
                s = tab + 1;
                tab = strchr(s, '\t');
                t = gb_cat3(t, " \t ", gb_plain(tab != NULL ? gb_n(s, (size_t) (tab - s)) : s));
        }
        return (t);
}

/* the paragraph being read: of translation pk, or a heading */
static void gs_end_para(void)
{
        char msg[600];

        if (gs.pk != 0)
        {
                if (gs.got[gs.pk])
                {
                        sprintf(msg, "two of %.100s: at %ld:%ld", gs.key[gs.pk], gs.chap, gs.vnum);
                        gb_fail(msg);
                }
                gs.got[gs.pk] = TRUE;
                gs.text[gs.pk] = gs_plain_parts(gs.pb);
        }
        gs.pk = 0;
        gs.pb = gb_s("");
        gs.brk = FALSE;
}

static void gs_flush_verse(void)
{
        long k;
        char msg[600];

        gs_end_para();
        if (!gs.have)
                return;
        for (k = 1; k <= gs.nk; k++)
        {
                if (!gs.got[k])
                {
                        sprintf(msg, "no %.100s: at %ld:%ld", gs.key[k], gs.chap, gs.vnum);
                        gb_fail(msg);
                }
                gs_put(k, gs.chap, gs.vnum, gs.text[k]);
                gs.got[k] = FALSE;
        }
        gs.have = FALSE;
}

static void gs_put_front(void)
{
        long k, i, n;
        int blank;
        char *row;
        GbList rows = { NULL, 0, 0 };

        for (k = 1; k <= gs.nk; k++)
        {
                n = 0;
                row = gb_s("");
                blank = FALSE;
                for (i = 1; i <= gs.front.n; i++)
                {
                        char *line = gb_at(&gs.front, i), *fk = gb_at(&gs.fkey, i);

                        if (line[0] == '\0')
                        {
                                if (row[0] != '\0')
                                {
                                        gb_put_at(&rows, ++n, row);
                                        row = gb_s("");
                                }
                                blank = TRUE;
                                continue;
                        }
                        if (fk[0] != '\0' && gb_value(fk) != k)
                                continue;
                        if (blank && n > 0)
                                gb_put_at(&rows, ++n, gb_s(""));
                        blank = FALSE;
                        row = row[0] == '\0' ? line : gb_cat3(row, " ", line);
                }
                if (row[0] != '\0')
                        gb_put_at(&rows, ++n, row);
                for (i = 1; i <= n; i++)
                        gs_put(k, 0, i, gb_plain(gb_at(&rows, i)));
        }
        gb_list_free(&rows);
}

static int gs_all(const char *s, const char *chars)
{
        if (*s == '\0')
                return (FALSE);
        return (strspn(s, chars) == strlen(s));
}

/* titles of the translations, for 'trans' (the split's "title" lines) */
static char *gs_titles[GS_MAX + 1], *gs_title_names[GS_MAX + 1];
static int gs_ntitles = 0;

static void gb_split_sidebyside(const char *here_dir)
{
        long i, k, c, v;
        char msg[4600], *line, *colon;

        memset(&gs, 0, sizeof(gs));
        gs.here = gb_s(here_dir);
        gs.pb = gs.heading = gb_s("");
        gs_ntitles = 0;
        for (k = 0; k < gb_ndetail; k++)
        {
                if (strcmp(gb_detail[k].kind, "translation") == 0 && gs.nk < GS_MAX)
                {
                        gs.nk++;
                        gs.key[gs.nk] = gb_detail[k].a;
                        gs.tname[gs.nk] = gb_detail[k].b;
                        gs.title[gs.nk] = gb_s("");
                }
                if (strcmp(gb_detail[k].kind, "book") == 0 && gs.nb < 199)
                {
                        gs.nb++;
                        gs.bookname[gs.nb] = gb_detail[k].a;
                        gs.bookstart[gs.nb] = gb_value(gb_detail[k].b);
                }
        }
        if (setjmp(gb_stop) != 0)
        {
                gb_list_free(&gs.front);
                gb_list_free(&gs.fkey);
                return;
        }
        for (i = 1; i <= gb_body.n; i++)
        {
                if ((i & 255) == 0)
                        let_page_draw(FALSE);
                line = gb_at(&gb_body, i);
                /* /^ Chapter [0-9]+:$/ */
                if (strncmp(line, " Chapter ", 9) == 0 && isdigit(UCHAR(line[9])))
                {
                        const char *p = line + 9;

                        while (isdigit(UCHAR(*p)))
                                p++;
                        if (strcmp(p, ":") == 0)
                        {
                                gs_flush_verse();
                                c = gb_value(line + 9);
                                if (c != gs.chap + 1)
                                {
                                        sprintf(msg, "chapter %ld after chapter %ld", c, gs.chap);
                                        gb_fail(msg);
                                }
                                if (gs.chap == 0)
                                {
                                        if (gs.nk == 0 || gs.nb == 0)
                                                gb_fail("no translations or books for it in this script");
                                        gs_start_book(1);
                                        gs_put_front();
                                }
                                else if (gs.b < gs.nb && c == gs.bookstart[gs.b + 1])
                                        gs_start_book(gs.b + 1);
                                gs.chap = c;
                                gs.vnum = 0;
                                gs.heading = gb_s("");
                                gs.heading_mode = TRUE;
                                continue;
                        }
                }
                if (gs_all(line, "-"))          /* /^-+$/ */
                        continue;
                if (gs.chap == 0)
                {
                        /* (before the first chapter: which translation each
                           key is) */
                        char *t = line;
                        size_t n;

                        while (*t == ' ')
                                t++;
                        n = strlen(t);
                        while (n > 0 && t[n - 1] == ' ')
                                n--;
                        t = gb_n(t, n);
                        gb_push(&gs.front, t);
                        gb_put_at(&gs.fkey, gs.front.n, gb_s(""));
                        /* match(line, /^[^ :]+: = /) */
                        n = strcspn(t, " :");
                        if (n > 0 && strncmp(t + n, ": = ", 4) == 0)
                        {
                                k = gs_which(gb_n(t, n));
                                if (k)
                                {
                                        gb_put_at(&gs.fkey, gs.front.n, gb_num(k));
                                        gs.title[k] = gb_s(t + n + 4);
                                }
                        }
                        continue;
                }
                if (gs.heading_mode && !gs_all(line, "0123456789."))
                {
                        char *t = gb_plain(line);

                        if (t[0] != '\0')
                                gs.heading = gs.heading[0] == '\0' ? t : gb_cat3(gs.heading, " \t ", t);
                        continue;
                }
                if (gs_all(line, "0123456789."))
                {
                        gs_flush_verse();
                        if (gs.heading_mode)
                        {
                                for (k = 1; k <= gs.nk; k++)
                                        gs_put(k, gs.chap, 0, gs.heading);
                                gs.heading_mode = FALSE;
                        }
                        /* /^[0-9]+\.[0-9]+$/, of 7 characters */
                        {
                                size_t d = strspn(line, "0123456789");

                                if (strlen(line) == 7 && d > 0 && line[d] == '.' &&
                                    line[d + 1] != '\0' && strspn(line + d + 1, "0123456789") == strlen(line + d + 1))
                                {
                                        c = gb_value(gb_sub(line, 1, 3));
                                        v = gb_value(gb_sub(line, 5, -1));
                                        if (c != gs.chap || v != gs.vnum + 1)
                                        {
                                                sprintf(msg, "verse %.100s after %ld:%ld", line, gs.chap, gs.vnum);
                                                gb_fail(msg);
                                        }
                                }
                                else
                                {
                                        /* (written wrongly: the next verse, if
                                           the next number line says so) */
                                        v = gs.vnum + 1;
                                        gs.slips++;
                                }
                        }
                        gs.vnum = v;
                        gs.have = TRUE;
                        continue;
                }
                /* /^[^ :]+: / && have: a translation's paragraph */
                if (gs.have)
                {
                        size_t n = strcspn(line, " :");

                        if (n > 0 && line[n] == ':' && line[n + 1] == ' ')
                        {
                                k = gs_which(gb_n(line, n));
                                if (k)
                                {
                                        gs_end_para();
                                        gs.pk = k;
                                        colon = strchr(line, ':');
                                        gs.pb = gb_sub(line, (long) (colon - line) + 3, -1);
                                        continue;
                                }
                        }
                }
                /* (a blank line: the paragraph may go on after it, a new
                   paragraph of the same translation, on a line of its own;
                   until a key, or the next verse) */
                if (line[0] == '\0')
                {
                        if (gs.pk != 0)
                                gs.brk = TRUE;
                        continue;
                }
                if (gs.pk == 0)
                {
                        sprintf(msg, "a line not understood, at %ld:%ld: %.4000s", gs.chap, gs.vnum, line);
                        gb_fail(msg);
                }
                gs.pb = gb_cat3(gs.pb, gs.brk ? "\t" : " ", line);
                if (gs.brk)
                        gs.paras++;
                gs.brk = FALSE;
        }
        gs_flush_verse();
        if (gs.chap == 0)
                gb_fail("no chapters in the text");
        for (k = 1; k <= gs.nk; k++)
        {
                if (gs.title[k][0] == '\0')
                {
                        sprintf(msg, "the text doesn't say which translation %.100s is", gs.key[k]);
                        gb_fail(msg);
                }
                if (gs.count[k] != gs.count[1])
                        gb_fail("the translations have different numbers of verses");
                gs_title_names[gs_ntitles] = gs.tname[k];
                gs_titles[gs_ntitles++] = gb_plain(gs.title[k]);
        }
        fflush(stdout);
        fprintf(stderr, "number lines written wrongly, taken as the next verse: %ld, paragraphs within a verse: %ld\n",
                gs.slips, gs.paras);
        gb_ok_books = gs.nb;
        gb_ok_verses = gs.count[1];
        gb_list_free(&gs.front);
        gb_list_free(&gs.fkey);
}

/* ------------------------------------------------------------------------
** Its page on gutenberg.org (page_facts in add-gutenberg.sh): the rows of
** its table of facts ("Title", "Last Update" or "Most Recently Updated",
** "Copyright Status", found by the words in them), as plain text. Of the
** title, only the main title (Gutenberg puts the subtitle after a colon).
*/
/* every s in h replaced by r */
static char *gb_replace(const char *h, const char *s, const char *r)
{
        Str b = { NULL, 0, 0 };
        const char *p;
        char *t;

        str_add(&b, "", 0);
        while ((p = strstr(h, s)) != NULL)
        {
                str_add(&b, h, (size_t) (p - h));
                str_adds(&b, r);
                h = p + strlen(s);
        }
        str_adds(&b, h);
        t = gb_s(b.s);
        free(b.s);
        return (t);
}

/* the start of a tag "<NAME..." (either case), its NAME of two letters,
   then any characters but ">", then ">": where it starts, and its end */
static const char *gb_tag(const char *h, const char *name, int closing, const char **end)
{
        const char *p;

        for (p = h; (p = strchr(p, '<')) != NULL; p++)
        {
                const char *q = p + 1;

                if (closing)
                {
                        if (*q != '/')
                                continue;
                        q++;
                }
                if (tolower(UCHAR(q[0])) != name[0] || tolower(UCHAR(q[1])) != name[1])
                        continue;
                q += 2;
                if (closing)
                {
                        if (*q != '>')
                                continue;
                        *end = q + 1;
                        return (p);
                }
                q += strcspn(q, ">");
                if (*q != '>')
                        return (NULL);
                *end = q + 1;
                return (p);
        }
        return (NULL);
}

/* awk's text(): tags out, the entities read, spaces run together */
static char *gb_text(const char *h)
{
        char *t = gb_s(h), *r, *w;
        Str b = { NULL, 0, 0 };
        const char *p, *q;

        t = gb_replace(t, "\r", " ");
        t = gb_replace(t, "\302\240", " ");
        /* <br...> as " : ", any other tag as a space */
        str_add(&b, "", 0);
        for (p = t; *p != '\0'; )
        {
                if (*p == '<' && (q = strchr(p, '>')) != NULL)
                {
                        if ((p[1] == 'b' || p[1] == 'B') && (p[2] == 'r' || p[2] == 'R'))
                                str_adds(&b, " : ");
                        else
                                str_adds(&b, " ");
                        p = q + 1;
                }
                else
                        str_add(&b, p++, 1);
        }
        t = gb_s(b.s);
        free(b.s);
        t = gb_replace(t, "&amp;", "&");
        t = gb_replace(t, "&quot;", "\"");
        /* (&#39;|&#x27;|&apos;: one pass, leftmost first) */
        {
                Str c = { NULL, 0, 0 };

                str_add(&c, "", 0);
                for (p = t; *p != '\0'; )
                {
                        if (strncmp(p, "&#39;", 5) == 0)
                        {
                                str_adds(&c, "'");
                                p += 5;
                        }
                        else if (strncmp(p, "&#x27;", 6) == 0 || strncmp(p, "&apos;", 6) == 0)
                        {
                                str_adds(&c, "'");
                                p += 6;
                        }
                        else
                                str_add(&c, p++, 1);
                }
                t = gb_s(c.s);
                free(c.s);
        }
        t = gb_replace(t, "&lt;", "<");
        t = gb_replace(t, "&gt;", ">");
        t = gb_replace(t, "&nbsp;", " ");
        for (r = w = t; *r != '\0'; r++)        /* [ \t]+ as one space */
        {
                if (*r == ' ' || *r == '\t')
                {
                        if (w > t && w[-1] == ' ' && (r > t && (r[-1] == ' ' || r[-1] == '\t')))
                                continue;
                        *w++ = ' ';
                }
                else
                        *w++ = *r;
        }
        *w = '\0';
        if (t[0] == ' ')
                memmove(t, t + 1, strlen(t));
        if ((w = t + strlen(t)) > t && w[-1] == ' ')
                w[-1] = '\0';
        return (t);
}

static char gb_title[4096], gb_updated[4096], gb_rights[4096];

static void gb_page_facts(const char *path)
{
        FILE *f;
        Str page = { NULL, 0, 0 };
        const char *p, *row, *next, *s, *e;
        int got_title = FALSE, got_updated = FALSE, got_rights = FALSE, c, start = TRUE;

        gb_title[0] = gb_updated[0] = gb_rights[0] = '\0';
        if ((f = fopen(path, "r")) == NULL)
                return;
        /* (its lines, each after a space: awk's page = page " " $0) */
        str_add(&page, "", 0);
        while ((c = getc(f)) != EOF)
        {
                char ch = (char) c;

                if (start)
                        str_adds(&page, " ");
                start = (c == '\n');
                if (c != '\n')
                        str_add(&page, &ch, 1);
        }
        fclose(f);
        /* split(page, rows, /<[Tt][Rr][ >]/): the rows after the first */
        for (p = page.s; ; )
        {
                const char *q;

                for (q = p; (q = strchr(q, '<')) != NULL; q++)
                        if ((q[1] == 't' || q[1] == 'T') && (q[2] == 'r' || q[2] == 'R') &&
                            (q[3] == ' ' || q[3] == '>'))
                                break;
                if (q == NULL)
                        break;
                row = q + 4;
                for (next = row; (next = strchr(next, '<')) != NULL; next++)
                        if ((next[1] == 't' || next[1] == 'T') && (next[2] == 'r' || next[2] == 'R') &&
                            (next[3] == ' ' || next[3] == '>'))
                                break;
                {
                        char *r = next != NULL ? gb_n(row, (size_t) (next - row)) : gb_s(row);
                        char *label, *value, *rest, *l, *w;
                        const char *key = NULL;

                        p = next != NULL ? next : row + strlen(row);
                        if ((s = gb_tag(r, "th", FALSE, &e)) == NULL)
                                goto next_row;
                        rest = gb_s(e);
                        if ((s = gb_tag(rest, "th", TRUE, &e)) == NULL)
                                goto next_row;
                        label = gb_text(gb_n(rest, (size_t) (s - rest)));
                        for (l = w = label; *l != '\0'; l++)
                        {
                                char c = (char) tolower(UCHAR(*l));

                                if ((c >= 'a' && c <= 'z') || c == ' ')
                                        *w++ = c;
                        }
                        *w = '\0';
                        rest = gb_s(e);
                        if ((s = gb_tag(rest, "td", FALSE, &e)) == NULL)
                                goto next_row;
                        rest = gb_s(e);
                        if ((s = gb_tag(rest, "td", TRUE, &e)) != NULL)
                                rest = gb_n(rest, (size_t) (s - rest));
                        value = gb_plain(gb_text(rest));
                        if (value[0] == '\0')
                                goto next_row;
                        /* (by the words in the label, not all of it) */
                        {
                                char *t = label;
                                size_t n;

                                while (*t == ' ')
                                        t++;
                                n = strlen(t);
                                while (n > 0 && t[n - 1] == ' ')
                                        n--;
                                if (n == 5 && strncmp(t, "title", 5) == 0)
                                {
                                        char *colon = strchr(value, ':');

                                        if (colon != NULL)
                                                *(colon > value && colon[-1] == ' ' ? colon - 1 : colon) = '\0';
                                        key = "title";
                                }
                                else if (strstr(label, "update") != NULL && strstr(label, "release") == NULL)
                                        key = "updated";
                                else if (strstr(label, "copyright") != NULL)
                                        key = "rights";
                        }
                        if (key == NULL)
                                goto next_row;
                        if (strcmp(key, "title") == 0 && !got_title++)
                                sprintf(gb_title, "%.4000s", value);
                        else if (strcmp(key, "updated") == 0 && !got_updated++)
                                sprintf(gb_updated, "%.4000s", value);
                        else if (strcmp(key, "rights") == 0 && !got_rights++)
                                sprintf(gb_rights, "%.4000s", value);
                }
next_row:
                if (next == NULL)
                        break;
        }
        free(page.s);
}

/* ------------------------------------------------------------------------
** Adding and removing (add-gutenberg.sh's add_number, add, remove)
*/
static char gb_number[64], gb_names[512], gb_name[128], gb_format[64], gb_url[600];
static char gb_etag[512], gb_size[128], gb_modified[256], gb_terms[8192];
static char gb_license_text[4400], gb_checksum[128], gb_agreed[512];
static const char *gb_from_file = "", *gb_from_page = "";
static char gb_adding[512];
static char gb_start_with[MAX_START];
static Str gb_renamed;

/* (for tests: the addresses of the text and its page instead, as
   "file:///copies/%s.txt"; EBAC_GUTENBERG_TEXT, EBAC_GUTENBERG_PAGE,
   EBAC_GUTENBERG_MIRROR) */
static const char *gb_plain_text_address(void)
{
        const char *e = getenv("EBAC_GUTENBERG_TEXT");

        return (e != NULL && *e != '\0' ? e : GB_PLAIN_TEXT);
}

static const char *gb_page_address(void)
{
        const char *e = getenv("EBAC_GUTENBERG_PAGE");

        return (e != NULL && *e != '\0' ? e : GB_PAGE);
}

/* Gutenberg's mirror's copy of the text (its two %s: the number), where
   the text is downloaded from (November 17, 2026, the designer's
   decision), its version too: the file www.gutenberg.org's text address
   leads to (cache/epub/N/pgN.txt), the same, byte for byte. Its page, with
   its title, last update and copyright status, is only on
   www.gutenberg.org. */
#define GB_MIRROR_TEXT "https://gutenberg.pglaf.org/cache/epub/%s/pg%s.txt"

static void gb_mirror_address(char *url)
{
        const char *e = getenv("EBAC_GUTENBERG_MIRROR");

        sprintf(url, e != NULL && *e != '\0' ? e : GB_MIRROR_TEXT, gb_number, gb_number);
}

/*
** Where to download an address from (EBAC_GUTENBERG_RELAY, in a web
** browser): a web page may read what another site sends only if that
** site says so (CORS), and Project Gutenberg's sites don't; so EBaC's web
** page asks a relay of its own (web/relay/gutenberg-relay.js), which gets
** the same file from Gutenberg and passes it on, with the word that the
** page may read it: https://RELAY/www.gutenberg.org/ebooks/17.txt.utf-8.
** Elsewhere, the address itself.
*/
static char *gb_via(const char *url)
{
#ifdef EBAC_GUTENBERG_RELAY
        if (strncmp(url, "https://", 8) == 0)
                return (gb_cat3(EBAC_GUTENBERG_RELAY, "/", url + 8));
#endif
        return (gb_s(url));
}

static int gb_get(const char *url, const char *path)
{
        char *via = gb_via(url), *at;
        int r = net_get(via, path);

        /* (what went wrong said of Gutenberg's address, not the relay's) */
        if (r != 0 && strcmp(via, url) != 0 && (at = strstr(net_problem, via)) != NULL)
        {
                char rest[600];

                strcpy(rest, at + strlen(via));
                sprintf(at, "%.300s%s", url, rest);
        }
        return (r);
}

/* each of the text's names (gb_names, separated by spaces), in turn */
static int gb_each(int i, char *n)
{
        const char *p = gb_names;
        size_t len;

        for (;;)
        {
                while (*p == ' ')
                        p++;
                if (*p == '\0')
                        return (FALSE);
                len = strcspn(p, " ");
                if (i-- == 0)
                {
                        sprintf(n, "%.*s", (int) (len < 127 ? len : 127), p);
                        return (TRUE);
                }
                p += len;
        }
}

static int gb_has_mark(const char *name)
{
        char path[1300];

        sprintf(path, "%s/%.200s/gutenberg", here, name);
        return (exists(path));
}

/* kept_field NAME FIELD: a line of the version file of the copy kept (the
   last such line), after its "FIELD:" */
static char *gb_kept_field(const char *name, const char *field)
{
        char path[1300], *v = gb_s(""), *p;
        static char line[LINE];
        FILE *f;
        size_t n = strlen(field);

        sprintf(path, "%s/%.200s/version", here, name);
        if ((f = fopen(path, "r")) == NULL)
                return (v);
        while (read_line(f, line, sizeof(line)))
        {
                p = line;
                while (*p == ' ' || *p == '\t')
                        p++;
                if (strncmp(p, field, n) == 0 && p[n] == ':' &&
                    (p[n + 1] == '\0' || p[n + 1] == ' ' || p[n + 1] == '\t'))
                {
                        p = strchr(line, ':') + 1;
                        while (*p == ' ' || *p == '\t')
                                p++;
                        v = gb_s(p);
                }
        }
        fclose(f);
        return (v);
}

static int gb_same_layout(const char *name)
{
        return (strcmp(gb_kept_field(name, "Layout"), GB_LAYOUT) == 0);
}

/* whether the copy of NAME kept is the version offered now: by its
   fingerprint; or by its size and date both; or by the date its page says
   it was updated */
static int gb_same_version(const char *name)
{
        if (!gb_has_mark(name) || !gb_same_layout(name))
                return (FALSE);
        if (gb_etag[0] != '\0' && strcmp(gb_etag, gb_kept_field(name, "Version")) == 0)
                return (TRUE);
        if (gb_size[0] != '\0' && gb_modified[0] != '\0' &&
            strcmp(gb_size, gb_kept_field(name, "Size")) == 0 &&
            strcmp(gb_modified, gb_kept_field(name, "Modified")) == 0)
                return (TRUE);
        if (gb_updated[0] != '\0' && strcmp(gb_updated, gb_kept_field(name, "Updated")) == 0)
                return (TRUE);
        return (FALSE);
}

static int gb_all_same_version(void)
{
        char n[128];
        int i;

        for (i = 0; gb_each(i, n); i++)
                if (!gb_same_version(n))
                        return (FALSE);
        return (TRUE);
}

static int gb_all_same_checksum(void)
{
        char n[128];
        int i;

        for (i = 0; gb_each(i, n); i++)
                if (!gb_has_mark(n) || strcmp(gb_checksum, gb_kept_field(n, "Checksum")) != 0 ||
                    !gb_same_layout(n))
                        return (FALSE);
        return (TRUE);
}

static void gb_already_have(void)
{
        sprintf(problem, "you already have this version (downloaded %.100s). To download it again, remove it first (r and its number), then add it",
                gb_kept_field(gb_name, "Downloaded"));
}

/* a line of trans: its first word, and the rest, its words joined by
   single spaces (awk's $1 = "" and the record made again) */
static int gb_trans_fields(const char *line, char *name, int size, char **rest)
{
        Str b = { NULL, 0, 0 };
        const char *p = line;
        size_t len;
        int k = 0;

        name[0] = '\0';
        str_add(&b, "", 0);
        for (;;)
        {
                while (*p == ' ' || *p == '\t')
                        p++;
                if (*p == '\0')
                        break;
                len = strcspn(p, " \t");
                if (k++ == 0)
                        sprintf(name, "%.*s", (int) (len < (size_t) size - 1 ? len : (size_t) size - 1), p);
                else
                {
                        if (b.len > 0)
                                str_adds(&b, " ");
                        str_add(&b, p, len);
                }
                p += len;
        }
        *rest = gb_s(b.s);
        free(b.s);
        return (k > 0);
}

/* The texts added from Project Gutenberg: listed in trans, with a folder
   with a file named gutenberg. Their names and titles (from 1). */
static GbList gb_ours_names, gb_ours_titles;

static void gb_ours(void)
{
        char path[1100], name[256], *title;
        static char line[LINE];
        FILE *f;

        gb_ours_names.n = gb_ours_titles.n = 0;
        sprintf(path, "%s/trans", here);
        if ((f = fopen(path, "r")) == NULL)
                return;
        while (read_line(f, line, sizeof(line)))
        {
                if (!gb_trans_fields(line, name, sizeof(name), &title) || name[0] == '#')
                        continue;
                if (gb_has_mark(name))
                {
                        gb_push(&gb_ours_names, gb_s(name));
                        gb_push(&gb_ours_titles, title);
                }
        }
        fclose(f);
}

/* trans_title NAME: its title, as trans lists it */
static char *gb_trans_title(const char *n)
{
        char path[1100], name[256], *title;
        static char line[LINE];
        FILE *f;

        sprintf(path, "%s/trans", here);
        if ((f = fopen(path, "r")) == NULL)
                return (gb_s(""));
        while (read_line(f, line, sizeof(line)))
                if (gb_trans_fields(line, name, sizeof(name), &title) && strcmp(name, n) == 0)
                {
                        fclose(f);
                        return (title);
                }
        fclose(f);
        return (gb_s(""));
}

/* Short names end in -PG (November 2026): a text added before, under its
   name without -PG (BOM), is renamed: its folder, and its line in trans,
   in its place there. What is said, into gb_renamed. */
static void gb_rename_old_names(void)
{
        const char *p = gb_known, *e;
        char *line, newn[256], old[256], from[1300], to[1300], path[1100], word[256];
        static char tline[LINE];

        str_clear(&gb_renamed);
        while (*p != '\0')
        {
                e = strchr(p, '\n');
                line = gb_n(p, (size_t) (e - p));
                p = e + 1;
                if (!isdigit(UCHAR(line[0])) || strchr(line, '\t') == NULL)
                        continue;
                {
                        char *t = strchr(line, '\t') + 1;
                        size_t n = strcspn(t, "\t");

                        if (!all_digits(gb_n(line, (size_t) (strchr(line, '\t') - line))))
                                continue;
                        sprintf(newn, "%.*s", (int) (n < 255 ? n : 255), t);
                }
                {
                        size_t n = strlen(newn);

                        if (n < 3 || strcmp(newn + n - 3, "-PG") != 0)
                                continue;
                        sprintf(old, "%.*s", (int) (n - 3), newn);
                }
                if (strchr(old, ' ') != NULL)
                        continue;
                sprintf(from, "%s/%s", here, old);
                sprintf(to, "%s/%s", here, newn);
                if (!gb_has_mark(old) || exists(to))
                        continue;
                if (rename(from, to) != 0)
                        continue;
                sprintf(path, "%s/trans", here);
                {
                        FILE *f = fopen(path, "r");
                        Str b = { NULL, 0, 0 };

                        if (f != NULL)
                        {
                                str_add(&b, "", 0);
                                while (read_line(f, tline, sizeof(tline)))
                                {
                                        first_word(tline, word, sizeof(word));
                                        if (strcmp(word, old) == 0)
                                        {
                                                /* (its title: after the name and
                                                   the space after it) */
                                                char *t = tline, buf[LINE / 4];

                                                /* (awk: sub(/^[^ \t]+[ \t]+/, "")) */
                                                while (*t != '\0' && *t != ' ' && *t != '\t')
                                                        t++;
                                                if (t > tline && (*t == ' ' || *t == '\t'))
                                                {
                                                        while (*t == ' ' || *t == '\t')
                                                                t++;
                                                }
                                                else
                                                        t = tline;
                                                snprintf(buf, sizeof(buf), "%-11s %s\n", newn, t);
                                                str_adds(&b, buf);
                                        }
                                        else
                                        {
                                                str_adds(&b, tline);
                                                str_adds(&b, "\n");
                                        }
                                }
                                fclose(f);
                                replace_trans(b.s);
                                free(b.s);
                        }
                }
                {
                        char said[700];

                        sprintf(said, "%s: renamed %s (from Project Gutenberg).\n", old, newn);
                        str_adds(&gb_renamed, said);
                }
        }
}

/* remove NAME */
static int gb_remove(const char *name)
{
        Str rest = { NULL, 0, 0 };
        char path[1300], word[256];
        const char *p, *e;
        int any = FALSE;

        if (!gb_has_mark(name))
        {
                strcpy(problem, "it wasn't added from Project Gutenberg (A, then T or S, removes those from GitHub)");
                return (-1);
        }
        str_add(&rest, "", 0);
        without_line(name, &rest);
        for (p = rest.s; *p != '\0'; p = e + 1)
        {
                e = strchr(p, '\n');
                first_word(p, word, sizeof(word));
                if (word[0] != '\0' && word[0] != '#')
                        any = TRUE;
                if (e == NULL)
                        break;
        }
        if (!any)
        {
                free(rest.s);
                strcpy(problem, "it is the only translation in trans");
                return (-1);
        }
        if (replace_trans(rest.s) != 0)
        {
                free(rest.s);
                strcpy(problem, "couldn't change trans");
                return (-1);
        }
        free(rest.s);
        sprintf(path, "%s/%.200s", here, name);
        /* (out of the folder while it is deleted: EBaC may be in it; then
           back, if it is still there) */
        {
                char was[1200];
                int back = getcwd(was, sizeof(was)) != NULL && chdir(here) == 0;
                int ok = remove_all(path) == 0;

                if (back && chdir(was) != 0)
                {
                        /* (it was the folder deleted: EBaC changes
                           translation next) */
                }
                if (!ok)
                {
                        sprintf(problem, "couldn't delete the folder %.200s", name);
                        return (-1);
                }
        }
        printf("%s: removed.\n", name);
        return (0);
}

/* agree NUMBER: show Gutenberg's license, and ask */
static int gb_agree(void)
{
        printf("\n%s: %s\n", gb_name, gb_title);
        if (gb_updated[0] != '\0')
                printf("Last updated: %s\n", gb_updated);
        printf("License: %s\n", gb_license_text);
        printf("Details: https://www.gutenberg.org/ebooks/%s\n", gb_number);
        if (agree_all)
        {
                if (agreed_how[0] != '\0')
                {
                        printf("Agreed to, %s.\n", agreed_how);
                        sprintf(gb_agreed, "%.500s", agreed_how);
                }
                else
                {
                        printf("Agreed to, with --agree.\n");
                        strcpy(gb_agreed, "with --agree");
                }
                return (TRUE);
        }
        if (!at_terminal())
        {
                strcpy(problem, "no one to agree to its license (add --agree to agree to it)");
                return (FALSE);
        }
        for (;;)
        {
                char answer[256], *a, *p;
                size_t n;

                printf("Do you agree to these terms (y/n)? ");
                typing_mode("input");
                if (read_answer(answer, sizeof(answer), TRUE) == 1)
                        strcpy(answer, "n");
                /* (read -r: spaces and tabs at either end left out) */
                for (a = answer; *a == ' ' || *a == '\t'; a++)
                        ;
                n = strlen(a);
                while (n > 0 && (a[n - 1] == ' ' || a[n - 1] == '\t'))
                        a[--n] = '\0';
                for (p = a; *p != '\0'; p++)
                        *p = (char) tolower(UCHAR(*p));
                if (strcmp(a, "y") == 0 || strcmp(a, "yes") == 0)
                {
                        strcpy(gb_agreed, "typed y");
                        return (TRUE);
                }
                if (strcmp(a, "n") == 0 || strcmp(a, "no") == 0)
                {
                        strcpy(problem, "its license wasn't agreed to");
                        return (FALSE);
                }
                printf("Please type y or n.\n");
        }
}

/* the folders of a text not wholly added (stopped, or failed) */
static void gb_drop_adding(void)
{
        char save[512], n[128], path[1300];
        int i;

        strcpy(save, gb_names);
        strcpy(gb_names, gb_adding);
        for (i = 0; gb_each(i, n); i++)
        {
                sprintf(path, "%s/%s", here, n);
                remove_all(path);
        }
        strcpy(gb_names, save);
        gb_adding[0] = '\0';
}

/* the text's own title: its first "Title: " line, plain */
static void gb_text_title(char *title)
{
        char path[1200];
        static char line[LINE];
        FILE *f;

        title[0] = '\0';
        sprintf(path, "%s/text", tmp);
        if ((f = fopen(path, "r")) != NULL)
        {
                while (read_line(f, line, sizeof(line)))
                        if (strncmp(line, "Title: ", 7) == 0)
                        {
                                sprintf(title, "%.4000s", gb_plain(line + 7));
                                break;
                        }
                fclose(f);
        }
        if (title[0] == '\0')
                sprintf(title, "Project Gutenberg ebook %s", gb_number);
}

/* add: the text, as each of its names */
static int gb_add(void)
{
        char n[128], folder[1300], path[1400], date[32];
        long found, total;
        int i, k, count;
        FILE *f;

        if (strcmp(gb_name, gb_names) == 0)
                printf("%s: writing its books into the folder %s...\n", gb_name, gb_name);
        else
                printf("Ebook %s: writing its translations into the folders %s...\n", gb_number, gb_names);
        fflush(stdout);
        let_page_draw(TRUE);
        strcpy(gb_adding, gb_names);
        for (i = 0; gb_each(i, n); i++)
        {
                sprintf(folder, "%s/%s", here, n);
                remove_all(folder);
                if (mkdir(folder, 0777) != 0)
                {
                        sprintf(problem, "couldn't make the folder %.1200s", folder);
                        return (-1);
                }
        }
        sprintf(path, "%s/text", tmp);
        gb_read_body(path);
        gb_failed = FALSE;
        gb_ok_books = gb_ok_verses = 0;
        gs_ntitles = 0;
        sprintf(folder, "%s/%s", here, gb_name);
        if (strcmp(gb_format, "chapters") == 0)
                gb_split_chapters(folder);
        else if (strcmp(gb_format, "paragraphs") == 0)
                gb_split_paragraphs(folder);
        else if (strcmp(gb_format, "verses") == 0)
                gb_split_verses(folder);
        else if (strcmp(gb_format, "sidebyside") == 0)
                gb_split_sidebyside(here);
        else
        {
                sprintf(problem, "this script doesn't know the format %.100s", gb_format);
                gb_failed = TRUE;
        }
        gb_close_all();
        gb_list_free(&gb_body);
        if (gb_failed)
                return (-1);
        total = gb_ok_verses;
        for (i = 0; gb_each(i, n); i++)
        {
                char back[1100];
                int built;

                sprintf(folder, "%s/%s", here, n);
                /* the Hebrew alphabet's letters set apart (acrostic.c), and
                   the Song of Songs' speakers (speakers.c) */
                letters_apart(folder);
                speakers_apart(folder);
                printf("%s: building the concordance index...\n", n);
                fflush(stdout);
                let_page_draw(TRUE);
                if (getcwd(back, sizeof(back)) == NULL || chdir(folder) != 0)
                {
                        sprintf(problem, "building the index of %s failed", n);
                        return (-1);
                }
                index_each_step = page_draws;
                built = make_index(TRUE);
                index_each_step = NULL;
                if (chdir(back) != 0 || built != 0)
                {
                        sprintf(problem, "building the index of %s failed", n);
                        return (-1);
                }
                found = index_verses(folder);
                if (found != total)
                {
                        sprintf(problem, "the index of %s has %ld verses, but the text has %ld", n, found, total);
                        return (-1);
                }
                today(date);
                sprintf(path, "%s/license", folder);
                if ((f = fopen(path, "w")) == NULL)
                {
                        sprintf(problem, "couldn't write %s/license", n);
                        return (-1);
                }
                fprintf(f, "License: %s\n", gb_license_text);
                if (gb_terms[0] != '\0')
                        fprintf(f, "Terms: %s\n", gb_terms);
                if (gb_updated[0] != '\0')
                        fprintf(f, "Updated: %s\n", gb_updated);
                fprintf(f, "Details: https://www.gutenberg.org/ebooks/%s\n", gb_number);
                fprintf(f, "Agreed: %s (%s)\n", date, gb_agreed);
                if (fclose(f) != 0)
                {
                        sprintf(problem, "couldn't write %s/license", n);
                        return (-1);
                }
                sprintf(path, "%s/version", folder);
                if ((f = fopen(path, "w")) != NULL)
                {
                        if (gb_etag[0] != '\0')
                                fprintf(f, "Version: %s\n", gb_etag);
                        if (gb_size[0] != '\0')
                                fprintf(f, "Size: %s\n", gb_size);
                        if (gb_modified[0] != '\0')
                                fprintf(f, "Modified: %s\n", gb_modified);
                        if (gb_updated[0] != '\0')
                                fprintf(f, "Updated: %s\n", gb_updated);
                        fprintf(f, "Checksum: %s\n", gb_checksum);
                        fprintf(f, "Downloaded: %s\n", date);
                        fprintf(f, "Layout: %s\n", GB_LAYOUT);
                        fclose(f);
                }
                sprintf(path, "%s/gutenberg", folder);
                if ((f = fopen(path, "w")) == NULL)
                {
                        sprintf(problem, "couldn't write %s/gutenberg", n);
                        return (-1);
                }
                fprintf(f, "Ebook: %s\n", gb_number);
                fprintf(f, "Address: %s\n", gb_url);
                if (fclose(f) != 0)
                {
                        sprintf(problem, "couldn't write %s/gutenberg", n);
                        return (-1);
                }
        }
        /* (into trans, at the top, in the order of names: so the last
           first; with --at-end, at the end, in their order) */
        for (count = 0; gb_each(count, n); count++)
                ;
        for (k = 0; k < count; k++)
        {
                Str trans = { NULL, 0, 0 };
                char line[4600];
                const char *t = gb_title;

                gb_each(at_end ? k : count - 1 - k, n);
                for (i = 0; i < gs_ntitles; i++)
                        if (strcmp(gs_title_names[i], n) == 0 && gs_titles[i][0] != '\0')
                                t = gs_titles[i];
                snprintf(line, sizeof(line), "%-11s %s\n", n, t);
                str_add(&trans, "", 0);
                if (!at_end)
                        str_adds(&trans, line);
                without_line(n, &trans);
                if (at_end)
                        str_adds(&trans, line);
                replace_trans(trans.s);
                free(trans.s);
        }
        gb_adding[0] = '\0';
        for (i = 0; gb_each(i, n); i++)
                printf("%s: done. %s is ready: start EBaC and press C to choose it.\n", n, gb_trans_title(n));
        return (0);
}

/* download_text: the text, into tmp/text, without carriage returns; and in
   gb_terms, the license its header gives */
static int gb_download_text(void)
{
        char path[1200], raw[1200];
        static char line[LINE];
        FILE *in, *out;
        int c, on = FALSE;
        Str t = { NULL, 0, 0 };

        printf("%s: downloading...\n", gb_name);
        fflush(stdout);
        let_page_draw(TRUE);
        sprintf(raw, "%s/text.raw", tmp);
        sprintf(path, "%s/text", tmp);
        if (gb_from_file[0] != '\0')
        {
                if (copy_file(gb_from_file, raw) != 0)
                {
                        sprintf(problem, "couldn't read %.1000s", gb_from_file);
                        return (-1);
                }
        }
        else
        {
                /* From Gutenberg's own mirror first (gutenberg.pglaf.org:
                   "high-speed", run by Project Gutenberg), so as not to load
                   www.gutenberg.org, which is "intended for human users
                   only"; from www.gutenberg.org if the mirror can't be had */
                char mirror[700], why[600];

                gb_mirror_address(mirror);
                if (gb_get(mirror, raw) != 0)
                {
                        strcpy(why, net_problem);
                        printf("%s: %s; downloading from www.gutenberg.org...\n", gb_name, why);
                        fflush(stdout);
                        let_page_draw(TRUE);
                        if (gb_get(gb_url, raw) != 0)
                        {
                                strcpy(problem, net_problem);
                                return (-1);
                        }
                }
        }
        if ((in = fopen(raw, "rb")) == NULL || (out = fopen(path, "wb")) == NULL)
        {
                if (in != NULL)
                        fclose(in);
                strcpy(problem, "couldn't read the download");
                return (-1);
        }
        while ((c = getc(in)) != EOF)
                if (c != '\r')
                        putc(c, out);
        fclose(in);
        if (fclose(out) != 0)
        {
                strcpy(problem, "couldn't read the download");
                return (-1);
        }
        unlink(raw);
        /* its terms: from "This eBook is for the use of anyone", to the next
           blank line */
        str_add(&t, "", 0);
        if ((in = fopen(path, "r")) != NULL)
        {
                while (read_line(in, line, sizeof(line)))
                {
                        if (strncmp(line, "This ", 5) == 0 && (line[5] == 'e' || line[5] == 'E') &&
                            (line[6] == 'b' || line[6] == 'B') &&
                            strncmp(line + 7, "ook is for the use of anyone", 28) == 0)
                                on = TRUE;
                        if (on && line[strspn(line, " \t")] == '\0')
                                break;
                        if (on)
                        {
                                str_adds(&t, " ");
                                str_adds(&t, line);
                        }
                }
                fclose(in);
        }
        sprintf(gb_terms, "%.8000s", gb_plain(t.s));
        free(t.s);
        cksum(path, line);
        sprintf(gb_checksum, "%.120s", line + 6);       /* ("cksum " left out) */
        if (gb_all_same_checksum())
        {
                gb_already_have();
                return (-1);
        }
        return (0);
}

/* version_of URL: what the server says of the file, asked for without
   downloading it: its fingerprint (ETag), length and Last-Modified date;
   with --file, its checksum as the fingerprint */
static void gb_version_of(void)
{
        gb_etag[0] = gb_size[0] = gb_modified[0] = '\0';
        if (gb_from_file[0] != '\0')
        {
                if (cksum(gb_from_file, gb_etag) != 0)
                        gb_etag[0] = '\0';
                return;
        }
        {
                char mirror[700];

                /* (of the mirror's copy: the one downloaded) */
                gb_mirror_address(mirror);
                net_head(gb_via(mirror), gb_etag, gb_size, gb_modified, 120);
        }
}

static int is_dir(const char *path)
{
        struct stat st;

        return (stat(path, &st) == 0 && S_ISDIR(st.st_mode));
}

/* add_number NUMBER: from the ebook number to the text added */
static int gb_add_number(const char *arg)
{
        char *f[5], path[1200], text_title[4100];
        int i, page_read = FALSE;
        const char *page_why = "couldn't download its page";
        char n[128];

        if (!all_digits(arg))
        {
                strcpy(problem, "not an ebook number (such as 17)");
                return (-1);
        }
        sprintf(gb_number, "%ld", strtol(arg, NULL, 10));       /* (no leading zeros) */
        if (!gb_known_line(gb_number, f, TRUE))
        {
                /* (how to ask for it, for anyone: each text's layout is
                   added by hand, in KNOWN. The address opens the issue
                   form for asking, .github/ISSUE_TEMPLATE/ebook.yml, its
                   number filled in; the designer, November 18, 2026.
                   Followed by a space, so that the period after the
                   message isn't taken as part of it.) */
                sprintf(problem, "ebook %s isn't one EBaC can add yet. To ask for it, go to https://github.com/neilbrookins/ebac/issues/new?template=ebook.yml&ebook=%s (a form, its number filled in)",
                        gb_number, gb_number);
                return (-1);
        }
        sprintf(gb_names, "%.500s", f[1]);
        sprintf(gb_name, "%.*s", (int) strcspn(f[1], " "), f[1]);
        sprintf(gb_format, "%.60s", f[2]);
        for (i = 0; gb_each(i, n); i++)
        {
                sprintf(path, "%s/%s", here, n);
                if (is_dir(path) && !gb_has_mark(n))
                {
                        sprintf(problem, "a translation named %s is there already, not from Project Gutenberg", n);
                        return (-1);
                }
        }
        sprintf(gb_url, gb_plain_text_address(), gb_number);
        gb_version_of();
        gb_updated[0] = '\0';
        if (gb_all_same_version())
        {
                gb_already_have();
                return (-1);
        }
        /* Its page first: its title, last update and copyright status,
           agreed to before the text is downloaded. (Without its page, the
           text is downloaded first, and its license read from it.) */
        gb_title[0] = gb_license_text[0] = gb_terms[0] = '\0';
        sprintf(path, "%s/page", tmp);
        if (gb_from_page[0] != '\0')
                page_read = copy_file(gb_from_page, path) == 0;
        else if (gb_from_file[0] == '\0')
        {
                char url[700];

                sprintf(url, gb_page_address(), gb_number);
                page_read = gb_get(url, path) == 0;
        }
        if (page_read)
        {
                gb_page_facts(path);
                if (gb_rights[0] != '\0')
                        sprintf(gb_license_text, "%.4000s Project Gutenberg License (%s)", gb_rights, GB_LICENSE);
                else
                {
                        page_read = FALSE;      /* (not as expected: as without it) */
                        page_why = "its page has no copyright status";
                }
                if (gb_all_same_version())
                {
                        gb_already_have();
                        return (-1);
                }
        }
        if (page_read)
        {
                if (gb_title[0] == '\0')
                        sprintf(gb_title, "Project Gutenberg ebook %s", gb_number);
                if (!gb_agree() || !enough_room() || gb_download_text() != 0)
                        return (-1);
        }
        else
        {
                if (gb_from_file[0] == '\0' && gb_from_page[0] == '\0')
                        printf("%s: couldn't read its page on gutenberg.org (%s); its license is read from the text\n",
                               gb_name, page_why);
                if (!enough_room() || gb_download_text() != 0)
                        return (-1);
                strcpy(gb_license_text, gb_terms);
                if (gb_license_text[0] == '\0')
                        sprintf(gb_license_text, "Project Gutenberg License (%s)", GB_LICENSE);
                gb_terms[0] = '\0';
                gb_text_title(gb_title);
                if (!gb_agree())
                        return (-1);
        }
        /* (the text's own title, as before, for trans) */
        gb_text_title(text_title);
        strcpy(gb_title, text_title);
        return (gb_add());
}

/* ------------------------------------------------------------------------
** For programs (the EBaC integration's voice, through ebac-admin): what
** can be added, a text's license, whether it is added
*/
static int gb_all_ours(void)
{
        char n[128];
        int i;

        for (i = 0; gb_each(i, n); i++)
                if (!gb_has_mark(n))
                        return (FALSE);
        return (TRUE);
}

/* each text of KNOWN, in turn: its fields */
static int gb_known_text(int i, char **f)
{
        const char *p = gb_known, *e;
        char *line;
        int k, n;

        while (*p != '\0')
        {
                e = strchr(p, '\n');
                line = gb_n(p, (size_t) (e - p));
                p = e + 1;
                n = (int) strcspn(line, "\t");
                if (n == 0 || !all_digits(gb_n(line, (size_t) n)))
                        continue;
                if (i-- > 0)
                        continue;
                f[0] = line;
                for (k = 1; k < 5; k++)
                {
                        char *tab = strchr(f[k - 1], '\t');

                        if (tab == NULL)
                                f[k] = (char *) "";
                        else
                        {
                                *tab = '\0';
                                f[k] = tab + 1;
                        }
                }
                if (strchr(f[4], '\t') != NULL)
                        *strchr(f[4], '\t') = '\0';
                return (TRUE);
        }
        return (FALSE);
}

static void gb_catalog(void)
{
        char *f[5], *langs[64];
        int counts[64], nlangs = 0, i, j;

        for (i = 0; gb_known_text(i, f); i++)
        {
                for (j = 0; j < nlangs && strcmp(langs[j], f[4]) != 0; j++)
                        ;
                if (j == nlangs && nlangs < 64)
                {
                        langs[nlangs] = gb_s(f[4]);
                        counts[nlangs++] = 0;
                }
                if (j < 64)
                        counts[j]++;
        }
        for (j = 0; j < nlangs; j++)
                printf("language\t%s\t%s\t%s\t%d\n", langs[j],
                       strcmp(langs[j], "en") == 0 ? "English" : langs[j],
                       strcmp(langs[j], "en") == 0 ? "English" : langs[j], counts[j]);
        for (i = 0; gb_known_text(i, f); i++)
        {
                sprintf(gb_names, "%.500s", f[1]);
                printf("translation\t%s\t%s\t%s\t%d\t%s\n", f[0], f[4], f[3], gb_all_ours() ? 1 : 0, f[1]);
        }
}

/* license_facts NUMBER: its title and license, from its page (the license
   from Gutenberg's own terms if the page can't be read) */
static void gb_license_facts(char **f)
{
        char path[1200], url[700];

        sprintf(gb_license_text, "Project Gutenberg License (%s)", GB_LICENSE);
        sprintf(path, "%s/page", tmp);
        sprintf(url, gb_page_address(), gb_number);
        if (gb_get(url, path) == 0)
        {
                gb_page_facts(path);
                if (gb_rights[0] != '\0')
                        sprintf(gb_license_text, "%.4000s Project Gutenberg License (%s)", gb_rights, GB_LICENSE);
        }
        /* (the title KNOWN gives it, not its page's) */
        sprintf(gb_title, "%.4000s", f[3]);
}

/* check_number NUMBER: "added" or "not added"; and, if what is kept is
   the version offered now, "same version", a tab, and when downloaded */
static void gb_check_number(char **f)
{
        char first[128];

        sprintf(gb_names, "%.500s", f[1]);
        if (gb_all_ours())
        {
                printf("added\n");
                sprintf(gb_url, gb_plain_text_address(), gb_number);
                gb_version_of();
                gb_updated[0] = '\0';
                if (gb_all_same_version())
                {
                        gb_each(0, first);
                        printf("same version\t%s\n", gb_kept_field(first, "Downloaded"));
                }
        }
        else
                printf("not added\n");
}

static void gb_usage(FILE *f)
{
        fprintf(f, "usage: bible --import gutenberg [--agree] [--file PATH] [--page PATH] NUMBER | --remove NAME | --list\n");
        fprintf(f, "Adds a text from Project Gutenberg to EBaC by its ebook number, or removes one.\n");
        fprintf(f, "For programs: --catalog, --license NUMBER, --check NUMBER, --at-end (into trans\n");
        fprintf(f, "last), --agreed-how TEXT (with --agree: how it was agreed to).\n");
}

/* ------------------------------------------------------------------------
** Asking for the ebook number (EBaC's P, under A), or r and a number to
** remove one; then adding or removing it. Returns FALSE for Enter alone.
*/
static int gb_ask(char *number_arg)
{
        char answer[256], *a;
        size_t n;
        long i;

        number_arg[0] = '\0';
        clear_screen();
        printf("Add a text from Project Gutenberg\n");
        printf("\n");
        if (gb_renamed.len > 0)
        {
                printf("%s", gb_renamed.s);
                printf("\n");
        }
        /* (lines of 50 columns at most: a phone's screen) */
        printf("Find it on gutenberg.org, check it is the one,\n");
        printf("and type its ebook number here: the EBook-No.\n");
        printf("on its page. EBaC knows, for now:\n");
        printf("  17    the Book of Mormon (BOM-PG)\n");
        printf("  1581  the Douay-Rheims Bible (DRC-PG)\n");
        printf("  8294  the World English Bible (WEB-PG)\n");
        printf("  16955 the Qur'an in three translations:\n");
        printf("        YUSUFALI-PG, PICKTHALL-PG, SHAKIR-PG\n");
        gb_ours();
        if (gb_ours_names.n > 0)
        {
                printf("\n");
                printf("Added from Project Gutenberg:\n");
                for (i = 1; i <= gb_ours_names.n; i++)
                        printf("%2ld) %s: %s\n", i, gb_at(&gb_ours_names, i), gb_at(&gb_ours_titles, i));
                printf("\n");
                printf("To remove one: r and its number (r1).\n");
        }
        printf("\n");
        printf("Ebook number (Enter alone: back): ");
        typing_mode("input");
        fflush(stdout);
        if (read_answer(answer, sizeof(answer), TRUE) == 1)
                answer[0] = '\0';
        /* (spaces at either end left out; the letters under the digits on a
           phone's keyboard are digits, qwertyuiop, but r and a number
           removes) */
        for (a = answer; *a == ' ' || *a == '\t'; a++)
                ;
        n = strlen(a);
        while (n > 0 && (a[n - 1] == ' ' || a[n - 1] == '\t'))
                a[--n] = '\0';
        {
                const char *from = "qwertyuiopQWERTYUIOP", *to = "12345678901234567890";
                char *p, *q;

                for (p = (*a == 'r' || *a == 'R') ? a + 1 : a; *p != '\0'; p++)
                        if ((q = strchr(from, *p)) != NULL)
                                *p = to[q - from];
        }
        if (*a == '\0')
                return (FALSE);
        if (*a == 'r' || *a == 'R')
        {
                if (a[1] == '\0' || !all_digits(a + 1))
                {
                        printf("%s: r and a number from the list, only (r1).\n", a);
                        failed = 1;
                        return (TRUE);
                }
                i = strtol(a + 1, NULL, 10);
                if (i < 1 || i > gb_ours_names.n)
                {
                        printf("%s: not in the list.\n", a);
                        failed = 1;
                }
                else if (gb_remove(gb_at(&gb_ours_names, i)) == 0)
                {
                        if (strcmp(gb_start_with, gb_at(&gb_ours_names, i)) == 0)
                                gb_start_with[0] = '\0';
                }
                else
                {
                        printf("%s: not removed: %s.\n", gb_at(&gb_ours_names, i), problem);
                        failed = 1;
                }
                return (TRUE);
        }
        sprintf(number_arg, "%.200s", a);
        return (TRUE);
}

/* the ebook number given: added, or said why not */
static void gb_add_given(const char *number_arg)
{
        gb_names[0] = gb_name[0] = '\0';
        if (gb_add_number(number_arg) == 0)
                sprintf(gb_start_with, "%.*s", MAX_START - 1, gb_name);
        else
        {
                gb_drop_adding();
                if (gb_names[0] != '\0' && strcmp(gb_names, gb_name) != 0)
                        printf("Ebook %s (%s): not added: %s.\n", number_arg, gb_names, problem);
                else
                        printf("%s: not added: %s.\n", gb_name[0] != '\0' ? gb_name : number_arg, problem);
                failed = 1;
        }
}

static void gb_reset(void)
{
        failed = 0;
        agree_all = at_end = FALSE;
        agreed_how = "";
        gb_from_file = gb_from_page = "";
        gb_adding[0] = '\0';
        typed[0] = '\0';
}

/* (after the arguments are read, as add-gutenberg.sh: the old names
   renamed, and the temporary folder made) */
static int gb_start(void)
{
        const char *t = getenv("TMPDIR");

        gb_rename_old_names();
        sprintf(tmp, "%.1000s/ebac-gutenberg.XXXXXX", (t != NULL && *t != '\0') ? t : "/tmp");
        if (mkdtemp(tmp) == NULL)
        {
                tmp[0] = '\0';
                return (-1);
        }
        return (0);
}

static void gb_end(void)
{
        cleanup();
        gb_free();
}

/* P under A, in EBaC itself (import_in_ebac()) */
static int gb_in_ebac(char *start_with)
{
        char number_arg[256];
        size_t n;

        gb_reset();
        if (gb_start() != 0)
                return (-1);
        in_ebac = TRUE;
        /* (EBaC's, renamed just now) */
        sprintf(gb_start_with, "%.*s", MAX_START - 1, start_with);
        n = strlen(gb_start_with);
        if (n > 0 && !(n >= 3 && strcmp(gb_start_with + n - 3, "-PG") == 0))
        {
                char renamed[MAX_START + 8], path[1300];

                sprintf(renamed, "%s-PG", gb_start_with);
                sprintf(path, "%s/%s", here, gb_start_with);
                if (gb_has_mark(renamed) && !exists(path) && strlen(renamed) < MAX_START)
                        strcpy(gb_start_with, renamed);
        }
        if (gb_ask(number_arg))
        {
                if (number_arg[0] != '\0')
                        gb_add_given(number_arg);
                press_any_key();
        }
        sprintf(start_with, "%.*s", MAX_START - 1, gb_start_with);
        in_ebac = FALSE;
        gb_end();
        return (0);
}

/* bible --import gutenberg ... */
static int gb_main(int argc, char **argv)
{
        const char *mode = "", *mode_number = "", *remove_name = "";
        char number_arg[256], *f[5];
        int i, list_only = FALSE, status = 0;

        number_arg[0] = '\0';
        gb_reset();
        for (i = 2; i < argc; i++)
        {
                const char *a = argv[i];

#define GB_NEEDS(n) if (i + (n) >= argc) { gb_usage(stderr); return (2); }
                if (strcmp(a, "--agree") == 0)
                        agree_all = TRUE;
                else if (strcmp(a, "--catalog") == 0)
                        mode = "catalog";
                else if (strcmp(a, "--license") == 0 || strcmp(a, "--check") == 0)
                {
                        GB_NEEDS(1);
                        mode = a + 2;
                        mode_number = argv[++i];
                }
                else if (strcmp(a, "--at-end") == 0)
                        at_end = TRUE;
                else if (strcmp(a, "--agreed-how") == 0)
                {
                        GB_NEEDS(1);
                        agreed_how = argv[++i];
                }
                else if (strcmp(a, "--list") == 0)
                        list_only = TRUE;
                else if (strcmp(a, "--file") == 0)
                {
                        GB_NEEDS(1);
                        gb_from_file = argv[++i];
                }
                else if (strcmp(a, "--page") == 0)
                {
                        GB_NEEDS(1);
                        gb_from_page = argv[++i];
                }
                else if (strcmp(a, "--remove") == 0)
                {
                        GB_NEEDS(1);
                        remove_name = argv[++i];
                }
                else if (strcmp(a, "-h") == 0 || strcmp(a, "--help") == 0)
                {
                        gb_usage(stdout);
                        return (0);
                }
                else if (a[0] == '-')
                {
                        gb_usage(stderr);
                        return (2);
                }
                else
                        sprintf(number_arg, "%.200s", a);
#undef GB_NEEDS
        }
        if (gb_start() != 0)
                return (1);
        if (gb_renamed.len > 0)
                printf("%s", gb_renamed.s);
        if (list_only)
        {
                gb_ours();
                for (i = 1; i <= gb_ours_names.n; i++)
                        printf("%s: %s\n", gb_at(&gb_ours_names, i), gb_at(&gb_ours_titles, i));
                gb_end();
                return (0);
        }
        if (strcmp(mode, "catalog") == 0)
        {
                gb_catalog();
                gb_end();
                return (0);
        }
        if (mode[0] != '\0')
        {
                if (!gb_known_line(mode_number, f, FALSE))
                {
                        fprintf(stderr, "ebook %s isn't one this importer knows (17, 1581, 8294, 16955).\n", mode_number);
                        gb_end();
                        return (1);
                }
                sprintf(gb_number, "%.60s", mode_number);
                if (strcmp(mode, "license") == 0)
                {
                        char url[700];

                        gb_license_facts(f);
                        sprintf(url, gb_page_address(), mode_number);
                        printf("Title: %s\nLicense: %s\nDetails: %s\n", gb_title, gb_license_text, url);
                }
                else
                        gb_check_number(f);
                gb_end();
                return (0);
        }
        if (remove_name[0] != '\0')
        {
                if (gb_remove(remove_name) != 0)
                {
                        printf("%s: not removed: %s.\n", remove_name, problem);
                        status = 1;
                }
                gb_end();
                return (status);
        }
        if (number_arg[0] == '\0' && !gb_ask(number_arg))
        {
                gb_end();
                return (0);
        }
        if (number_arg[0] != '\0')
                gb_add_given(number_arg);
        gb_end();
        return (failed ? 1 : 0);
}
