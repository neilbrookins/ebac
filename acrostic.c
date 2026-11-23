/*
** acrostic.c: the Hebrew alphabet's letters in a translation, set apart as
** headings (included by import.c; see "The Hebrew alphabet's letters" in
** readme.architecture). The designer, November 18, 2026.
**
** Psalm 119, and other psalms and chapters (Psalms 9-10, 25, 34, 37, 111,
** 112, 145; Proverbs 31:10-31; Lamentations 1-4; Nahum 1), are acrostics:
** each stanza, verse or half-verse starts with the next letter of the
** Hebrew alphabet. Many translations print the letter's name, or the letter
** itself, as a divider: "...with all their hearts. Beth" (the NLT's Psalm
** 119:8, the name of the stanza that follows), "ALEPH. Blessed are...",
** "(א) I will praise the Lord", "Bet ב". It isn't the verse's text, but in
** the text as plain words it reads as if it were. So each one found is
** taken out of the words around it and kept as a heading of its own, on
** its own line with "===" in the margin, as a chapter's heading has (the
** designer, November 22, 2026: they are headings the translation adds, as
** the Song of Songs' speakers, "Young Woman", are; first set apart with a
** note's "---", November 18): before the words
** it was before, after the words it was after, in the middle of a verse
** if it was there (a verse's text may then go on after the note, on a line
** with a blank margin).
**
** Found without knowing the translation's language: the names are learned
** from the translation's own Psalm 119, the only chapter in the Bible with
** 176 verses, where the name of each stanza's letter, if the translation
** prints them, ends verse 8, 16, ... 168, after the verse's last clause
** ("Beth", "BETH.", "ב BETH", "Bet ב", "בBeth", "SIN AND SHIN" for the
** stanza of both Sin and Shin), or starts verse 9, 17, ... 169 ("BETH.
** Wherewith...", "[Beth.]", "- BETH -", "ב Bet"). Only Aleph, at verse 1,
** isn't learned so: its name is known by its spelling (see is_aleph()),
** or by the letter itself. A letter (א to ת) is known in any translation.
**
** Then a name, or a letter, is taken as a divider only where it is set
** apart from the words around it: at the start of a verse, after a
** clause's punctuation, or with the letter beside it; and only in Psalm
** 119 at its stanzas' places, or in a chapter with eight of the letters or
** more (an acrostic), in the alphabet's order (a few may be missing, as
** in Psalms 9-10), so that "He", the pronoun, isn't taken for He, the
** letter, nor a word for a letter's name in any chapter but these.
*/

#define ALEPHBET        22      /* letters: Sin and Shin are one */
#define LETTER_NAME     64      /* (bytes: a name's longest) */
#define ACROSTICS       64      /* acrostic chapters, at most */

typedef struct
{
        char name[ALEPHBET][LETTER_NAME];  /* as this translation spells
                                              each ("" not known) */
        int named;              /* TRUE: its Psalm 119 named its letters */
        int hebrew;             /* TRUE: in Hebrew's script (Hebrew, Yiddish):
                                   its letters are its words' letters, so a
                                   divider is a letter standing alone, and
                                   only at Psalm 119's stanzas */
        int letters_only;       /* TRUE: not named, but it may mark them by
                                   the letters themselves ("(א)", "ב Bet"):
                                   in a translation not in Hebrew's script,
                                   where a letter alone is a word */
        int p119_book;          /* Psalm 119: its book (by its ### line),
                                   chapter; 0 if none */
        long p119_chap;
        long before_chap, before_last;  /* the chapter before it, and its
                                           last verse (Aleph may end it) */
        int n;                  /* the other acrostic chapters */
        int book[ACROSTICS];
        long chap[ACROSTICS];
} Alphabet;

/* The letters, in order: their second byte in UTF-8 (the first is 0xD7) */
static const unsigned char letter_byte[ALEPHBET] = {
        0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9B,
        0x9C, 0x9E, 0xA0, 0xA1, 0xA2, 0xA4, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA
};

/* Which letter is at p (א ... ת, or a final form, ך ם ן ף ץ, as the NKJV
   prints Kaph, Mem, Nun, Tsadde): its place in the alphabet, or -1 */
static int letter_at(const char *p)
{
        static const unsigned char final_byte[] = { 0x9A, 0x9D, 0x9F, 0xA3, 0xA5 };
        static const int final_of[] = { 10, 12, 13, 16, 17 };
        int k;

        if (UCHAR(p[0]) != 0xD7)
                return (-1);
        for (k = 0; k < ALEPHBET; k++)
                if (UCHAR(p[1]) == letter_byte[k])
                        return (k);
        for (k = 0; k < 5; k++)
                if (UCHAR(p[1]) == final_byte[k])
                        return (final_of[k]);
        return (-1);
}

/* Whether a Hebrew point or mark is at p (a vowel, or Shin's dot: "שׁ") */
static int hebrew_mark_at(const char *p)
{
        return ((UCHAR(p[0]) == 0xD6 && UCHAR(p[1]) >= 0x91) ||
                (UCHAR(p[0]) == 0xD7 && (UCHAR(p[1]) == 0x81 || UCHAR(p[1]) == 0x82 ||
                                        UCHAR(p[1]) == 0x84 || UCHAR(p[1]) == 0x85 ||
                                        UCHAR(p[1]) == 0x87)));
}

/* The bytes of the letter at p, with the points and marks on it */
static size_t letter_len(const char *p)
{
        size_t n = 2;

        while (hebrew_mark_at(p + n))
                n += 2;
        return (n);
}

/* Whether a clause ends just before at (spaces before it skipped): its
   punctuation, in any of the scripts seen (. , ; : ! ? ) ] " ' ” ’ » — …
   and Hindi's ।, Arabic's ؛ ۔, Hebrew's ׃) */
static int clause_ends(const char *start, const char *at)
{
        static const char *marks[] = {
                "\342\200\235", "\342\200\231", "\302\273", "\342\200\224",
                "\342\200\246", "\340\245\244", "\330\233", "\333\224",
                "\327\203", "\343\200\202", NULL
        };
        const char *p = at;
        int k;
        size_t n;

        while (p > start && (p[-1] == ' ' || p[-1] == '\t'))
                p--;
        if (p == start)
                return (FALSE);
        if (strchr(".,;:!?)]\"'>", p[-1]) != NULL)
                return (TRUE);
        for (k = 0; marks[k] != NULL; k++)
        {
                n = strlen(marks[k]);
                if ((size_t) (p - start) >= n && strncmp(p - n, marks[k], n) == 0)
                        return (TRUE);
        }
        return (FALSE);
}

/* Whether the n bytes at w spell Aleph's name, as the translations seen
   spell it (Aleph, ALEPH, Alef, Álef, Álefe, Àlef, Алеф): the one letter
   not named at a stanza's end in Psalm 119, so not learned there */
static int is_aleph(const char *w, size_t n)
{
        static const char *names[] = {
                "aleph", "alef", "alefe", "\303\241lef", "\303\241lefe",
                "\303\240lef", "\303\201lef", "\303\201lefe", "\303\200lef",
                "\320\220\320\273\320\265\321\204",
                "\320\260\320\273\320\265\321\204",
                "\320\220\320\233\320\225\320\244", NULL
        };
        char low[16];
        size_t i;
        int k;

        if (n == 0 || n >= sizeof(low))
                return (FALSE);
        for (i = 0; i < n; i++)
                low[i] = (char) tolower(UCHAR(w[i]));
        low[n] = '\0';
        for (k = 0; names[k] != NULL; k++)
                if (strlen(names[k]) == n &&
                    (strcmp(low, names[k]) == 0 || memcmp(w, names[k], n) == 0))
                        return (TRUE);
        return (FALSE);
}

/* Whether a capital letter starts p: A to Z, or one of Latin-1's (À to Þ) */
static int capital_at(const char *p)
{
        return (isupper(UCHAR(p[0])) ||
                (UCHAR(p[0]) == 0xC3 && UCHAR(p[1]) >= 0x80 && UCHAR(p[1]) <= 0x9E));
}

/* Whether a name may end at p: the end, a space, or punctuation after it */
static int name_ends(const char *p)
{
        return (*p == '\0' || strchr(" \t.,;:])->", *p) != NULL ||
                letter_at(p) >= 0 || UCHAR(p[0]) == 0xE2);
}

/*
** Purpose: a divider at, if one starts there: its length in bytes (0:
**          none), and in *idx which letter it is. Its forms: a name
**          ("Beth"), with a period after it ("BETH."), in brackets
**          ("[Aleph.]"), between hyphens ("- ALEPH -"), with a dash after
**          it ("Álefe —"); with the letter before it ("ב BETH", "(א)
**          Aleph", "בBeth") or after it ("Bet ב"); or the letter alone
**          ("(א)", "א"). *glyph: whether the letter is part of it.
*/
static size_t divider_at(const char *at, Alphabet *a, int *idx, int *glyph)
{
        const char *p = at, *w;
        int g = -1, k, best = -1, dash = FALSE;
        size_t len, best_len = 0;

        *glyph = FALSE;
        if (a->hebrew)
        {
                /* (only a learned name, the letter, standing alone) */
                for (k = 0; k < ALEPHBET; k++)
                {
                        len = strlen(a->name[k]);
                        if (len > 0 && strncmp(p, a->name[k], len) == 0 &&
                            (p[len] == '\0' || p[len] == ' '))
                        {
                                *idx = k;
                                return (len);
                        }
                }
                return (0);
        }
        if (p[0] == '(' && letter_at(p + 1) >= 0 && p[1 + letter_len(p + 1)] == ')')
        {
                g = letter_at(p + 1);
                p += 2 + letter_len(p + 1);
        }
        else if (letter_at(p) >= 0)
        {
                g = letter_at(p);
                p += letter_len(p);
        }
        if (g >= 0 && *p == ' ')
                p++;
        w = p;                  /* (the name, if any) */
        if (*w == '[')
                w++;
        else if (w[0] == '<' && w[1] == '<')
                w += 2;         /* (as the World English Bible marks it:
                                   "<<ALEPH>>") */
        else if (w[0] == '-' && w[1] == ' ')
        {
                w += 2;
                dash = TRUE;
        }
        /* (in capitals or not: the Douay-Rheims has "BETH." in Psalm
           118, its 119, and "Beth." in Lamentations; but starting with a
           capital, as a name does) */
        for (k = 0; k < ALEPHBET; k++)
        {
                len = strlen(a->name[k]);
                if (len > best_len && strncasecmp(w, a->name[k], len) == 0 &&
                    (capital_at(w) || !isalpha(UCHAR(*w))) && name_ends(w + len))
                {
                        best = k;
                        best_len = len;
                }
        }
        if (best < 0)
        {
                const char *e = w;

                while (*e != '\0' && strchr(" \t.,;:])->", *e) == NULL && letter_at(e) < 0)
                        e++;
                if (is_aleph(w, (size_t) (e - w)))
                {
                        best = 0;
                        best_len = (size_t) (e - w);
                }
        }
        if (best < 0 && g >= 0 && a->name[g][0] == '\0' && w > at && w[-1] == ' ')
        {
                /* the letter, and then its name, not known, before the
                   verse's words: "ב Bet Com que meios..." (a name, then
                   a word, both in capitals; not "(א) I will praise") */
                const char *e = w, *x;

                while (*e != '\0' && *e != ' ')
                        e++;
                x = (*e == ' ') ? e + 1 : e;
                if (capital_at(w) && *x != '\0' && capital_at(x) && e - w < LETTER_NAME)
                {
                        best = g;
                        best_len = (size_t) (e - w);
                }
        }
        if (best >= 0)
        {
                p = w + best_len;
                if (*p == '.')
                        p++;
                /* (an opening bracket closed after the name, or it isn't
                   one: "[Heh Vav]", two letters in one, is left as it is) */
                if (w > at && w[-1] == '[')
                {
                        if (*p != ']')
                                return (0);
                        p++;
                }
                else if (w - at >= 2 && w[-1] == '<' && w[-2] == '<')
                {
                        if (p[0] != '>' || p[1] != '>')
                                return (0);
                        p += 2;
                }
                if (dash && p[0] == ' ' && p[1] == '-' && (p[2] == ' ' || p[2] == '\0'))
                        p += 2;
                if (p[0] == ' ' && letter_at(p + 1) >= 0 &&
                    (p[1 + letter_len(p + 1)] == ' ' || p[1 + letter_len(p + 1)] == '\0'))
                {
                        g = letter_at(p + 1);
                        p += 1 + letter_len(p + 1);
                }
                else if (p[0] == ' ' && UCHAR(p[1]) == 0xE2 && UCHAR(p[2]) == 0x80 &&
                         (UCHAR(p[3]) == 0x94 || UCHAR(p[3]) == 0x93) &&
                         (p[4] == ' ' || p[4] == '\0'))
                        p += 4;         /* (a dash after it: "Álefe —") */
                *idx = best;
        }
        else if (g >= 0 && w == p)
        {
                *idx = g;               /* (the letter alone) */
                p = (at[0] == '(') ? at + 2 + letter_len(at + 1) : at + letter_len(at);
        }
        else
                return (0);
        if (*p != '\0' && *p != ' ' && *p != '\t')
                return (0);
        *glyph = (g >= 0);
        return ((size_t) (p - at));
}

/*
** Purpose: the name after a verse's last clause, at a stanza's end in
**          Psalm 119 ("Beth" of "...all their hearts. Beth"; "BETH" of
**          "utterly. ב BETH."), into name; FALSE if there is none: no
**          clause before it, or more words than a name (one; three for
**          the stanza of Sin and Shin, "Sin and Shin"), or a word in small
**          letters (a sentence, not a name: "teach me").
*/
static int stanza_name(const char *verse, int idx, char *name)
{
        static char v[LINE];
        char *end, *p, *t, core[LETTER_NAME * 2];
        int words = 0, n = 0, in_word = FALSE;

        strcpy(v, verse);
        end = v + strlen(v);
        while (end > v && end[-1] == ' ')
                *--end = '\0';
        if (end > v && end[-1] == '.')
                *--end = '\0';
        for (p = end; p > v; p--)
                if (p[-1] == ' ' && clause_ends(v, p - 1))
                        break;
        if (p == v)
                return (FALSE);
        /* the name: its words without the letter, or brackets, beside them
           (the letter alone, if that is all: a Yiddish Bible's) */
        for (t = p; *t != '\0'; t++)
        {
                if (letter_at(t) >= 0 && strlen(p) > letter_len(t))
                {
                        t += letter_len(t) - 1;
                        continue;
                }
                if (hebrew_mark_at(t))
                {
                        t++;
                        continue;
                }
                if (*t == '(' || *t == ')' || *t == '[' || *t == ']')
                        continue;
                if (*t == ' ')
                {
                        in_word = FALSE;
                        if (n > 0 && core[n - 1] != ' ')
                                core[n++] = ' ';
                        continue;
                }
                if (!in_word)
                {
                        words++;
                        in_word = TRUE;
                        if (words == 1 && isalpha(UCHAR(*t)) && !isupper(UCHAR(*t)))
                                return (FALSE);
                }
                if (n >= LETTER_NAME - 1)
                        return (FALSE);
                core[n++] = *t;
        }
        while (n > 0 && core[n - 1] == ' ')
                n--;
        core[n] = '\0';
        if (n == 0 || words > (idx == 20 ? 3 : 1))
                return (FALSE);
        strcpy(name, core);
        return (TRUE);
}

/*
** Purpose: the name before a stanza's first verse's words, in Psalm 119,
**          as other translations have it ("BETH. Wherewith shall a young
**          man...", "[Beth.] With what...", "- BETH - How may...", "ב Bet
**          Com que..."), into name: set apart by a period, bracket or
**          hyphen after it, or by its letter before it. FALSE: none.
*/
static int stanza_head(const char *verse, char *name)
{
        const char *p = verse, *w;
        int glyph = FALSE;
        size_t n;

        if (p[0] == '(' && letter_at(p + 1) >= 0 && p[1 + letter_len(p + 1)] == ')')
        {
                p += 2 + letter_len(p + 1);
                glyph = TRUE;
        }
        else if (letter_at(p) >= 0)
        {
                p += letter_len(p);
                glyph = TRUE;
        }
        while (*p == ' ')
                p++;
        if (*p == '[')
                p++;
        else if (p[0] == '<' && p[1] == '<')
                p += 2;
        else if (p[0] == '-' && p[1] == ' ')
                p += 2;
        for (w = p; *p != '\0' && strchr(" .]>", *p) == NULL; p++)
                ;
        n = (size_t) (p - w);
        if (n == 0 || n >= LETTER_NAME || (isalpha(UCHAR(*w)) && !isupper(UCHAR(*w))))
                return (FALSE);
        if (!(glyph || *p == '.' || *p == ']' || *p == '>' || (p[0] == ' ' && p[1] == '-')))
                return (FALSE);
        memcpy(name, w, n);
        name[n] = '\0';
        return (TRUE);
}

/* the verses of one chapter, as read: kept while it is read */
#define CHAPTER_VERSES 200
static char *chapter_verse[CHAPTER_VERSES + 1];

static void chapter_clear(void)
{
        int i;

        for (i = 0; i <= CHAPTER_VERSES; i++)
        {
                free(chapter_verse[i]);
                chapter_verse[i] = NULL;
        }
}

/* Psalm 119's names learned, from its verses (chapter_verse) */
static void learn_names(Alphabet *a)
{
        int k, found = 0, heads = 0, letters = 0;
        char head[LETTER_NAME];
        const char *p;

        if (chapter_verse[1] != NULL)
                for (p = chapter_verse[1]; *p != '\0'; p++)
                        if (letter_at(p) >= 0)
                                letters++;
        a->hebrew = (letters > 2);

        for (k = 1; k < ALEPHBET; k++)
        {
                a->name[k][0] = '\0';
                if (chapter_verse[8 * k] != NULL &&
                    stanza_name(chapter_verse[8 * k], k, a->name[k]))
                        found++;
        }
        /* or else before each stanza's first verse (not in Hebrew's
           script: there a word starts with a letter) */
        if (found < 15 && !a->hebrew)
        {
                for (k = 1; k < ALEPHBET; k++)
                        if (chapter_verse[8 * k + 1] != NULL &&
                            stanza_head(chapter_verse[8 * k + 1], head))
                                heads++;
                if (heads > found)
                {
                        found = 0;
                        for (k = 1; k < ALEPHBET; k++)
                        {
                                a->name[k][0] = '\0';
                                if (chapter_verse[8 * k + 1] != NULL &&
                                    stanza_head(chapter_verse[8 * k + 1], a->name[k]))
                                        found++;
                        }
                }
        }
        /* (enough of them that they are names, not the ends of sentences
           that happen to be short) */
        a->named = (found >= 15);
        a->letters_only = (!a->named && !a->hebrew);
        if (!a->named)
                for (k = 1; k < ALEPHBET; k++)
                        a->name[k][0] = '\0';
}

/*
** Purpose: whether a divider at v + i is set apart enough to be one: at the
**          verse's start, after a clause, or with its letter beside it
*/
static int set_apart(const char *v, size_t i, int glyph)
{
        return (i == 0 || glyph || clause_ends(v, v + i));
}

/* The distinct letters set apart in one verse, added to seen[] */
static void letters_seen(const char *v, Alphabet *a, int *seen)
{
        size_t i, len;
        int idx, glyph;

        for (i = 0; v[i] != '\0'; i++)
        {
                if (i > 0 && v[i - 1] != ' ')
                        continue;
                if ((len = divider_at(v + i, a, &idx, &glyph)) > 0 &&
                    set_apart(v, i, glyph))
                        seen[idx] = TRUE;
        }
}

/* The books of a translation's folder, in order (its booklist): their
   files' names, *n of them (freed by free_books()) */
static char **folder_books(const char *folder, int *n)
{
        static char line[LINE];
        char path[1300], **names = NULL;
        int size = 0;
        FILE *f;

        *n = 0;
        sprintf(path, "%.1200s/booklist", folder);
        if ((f = fopen(path, "r")) == NULL)
                return (NULL);
        while (read_line(f, line, sizeof(line)))
        {
                if (line[0] == '\0')
                        continue;
                if (*n >= size)
                {
                        size = size * 2 + 80;
                        names = (char **) realloc(names, sizeof(char *) * (size_t) size);
                }
                names[(*n)++] = strdup(line);
        }
        fclose(f);
        return (names);
}

static void free_books(char **names, int n)
{
        while (n > 0)
                free(names[--n]);
        free(names);
}

/* A verse's text, its parts' tabs as spaces (to learn and count from) */
static void as_one_line(char *s)
{
        for (; *s != '\0'; s++)
                if (*s == '\t')
                        *s = ' ';
}

/*
** Purpose: read a translation's books, as written into its folder, for
**          its Psalm 119, to learn the names from, and then for the
**          chapters that are acrostics, with eight letters or more set
**          apart (or four, going on from the chapter before, one: Psalm
**          10, Psalm 9's second half). A book is known by its place in
**          the booklist, from 1.
*/
static void read_alphabet(const char *folder, Alphabet *a)
{
        static char line[LINE], copy[LINE];
        char path[1300], **names;
        FILE *f;
        int pass, nbooks, book, k, count, seen[ALEPHBET];
        long c, v, chap, last_chap, last_verse, most;
        const char *rest;

        memset(a, 0, sizeof(*a));
        if ((names = folder_books(folder, &nbooks)) == NULL)
                return;
        for (pass = 1; pass <= 2; pass++)
        {
                for (book = 1; book <= nbooks; book++)
                {
                        sprintf(path, "%.1000s/%.200s", folder, names[book - 1]);
                        if ((f = fopen(path, "r")) == NULL)
                                continue;
                        chap = -1;
                        most = 0;
                        last_chap = -1;
                        last_verse = 0;
                        memset(seen, 0, sizeof(seen));
                        for (;;)
                        {
                                int more = read_line(f, line, sizeof(line));

                                let_page_draw(FALSE);   /* (in a browser) */
                                if (more && !verse_line(line, &c, &v, &rest))
                                        continue;
                                /* the chapter just read, as another begins */
                                if (!more || c != chap)
                                {
                                        if (pass == 1 && chap >= 0 && most == 176 &&
                                            a->p119_book == 0)
                                        {
                                                a->p119_book = book;
                                                a->p119_chap = chap;
                                                learn_names(a);
                                                a->before_chap = last_chap;
                                                a->before_last = last_verse;
                                        }
                                        if (pass == 2 && chap >= 0 && most != 176 &&
                                            a->n < ACROSTICS && !a->hebrew)
                                        {
                                                for (count = 0, k = 0; k < ALEPHBET; k++)
                                                        count += seen[k];
                                                if (count >= 8 ||
                                                    (count >= 4 && a->n > 0 &&
                                                     a->book[a->n - 1] == book &&
                                                     a->chap[a->n - 1] == chap - 1))
                                                {
                                                        a->book[a->n] = book;
                                                        a->chap[a->n++] = chap;
                                                }
                                        }
                                        if (chap >= 0 && most != 176)
                                        {
                                                last_chap = chap;
                                                last_verse = most;
                                        }
                                        if (pass == 1)
                                                chapter_clear();
                                        memset(seen, 0, sizeof(seen));
                                        most = 0;
                                }
                                if (!more)
                                        break;
                                chap = c;
                                if (v > most)
                                        most = v;
                                while (*rest == ' ')
                                        rest++;
                                strcpy(copy, rest);
                                as_one_line(copy);
                                plain(copy);
                                if (pass == 1 && v >= 0 && v <= CHAPTER_VERSES)
                                {
                                        free(chapter_verse[v]);
                                        chapter_verse[v] = strdup(copy);
                                }
                                if (pass == 2)
                                        letters_seen(copy, a, seen);
                        }
                        fclose(f);
                        chapter_clear();
                }
                if (pass == 1 && !a->named && !a->letters_only)
                        break;          /* (none to take out) */
        }
        free_books(names, nbooks);
}

/* Which kind of chapter this is: 2 Psalm 119, 1 another acrostic, 3 the
   chapter before Psalm 119 (its last verse: Aleph, maybe), 0 none */
static int acrostic_kind(Alphabet *a, int book, long chap)
{
        int i;

        if (!a->named && !a->letters_only)
                return (0);
        if (book == a->p119_book && chap == a->p119_chap)
                return (2);
        if (book == a->p119_book && chap == a->before_chap)
                return (3);
        for (i = 0; i < a->n; i++)
                if (a->book[i] == book && a->chap[i] == chap)
                        return (1);
        return (0);
}

/*
** Purpose: the length of a letter's name at a verse's start, not spelled as
**          learned: one word, starting with a capital, of two to eight
**          letters, with a period after it ("Ghimel. Juda hath..."); 0 if
**          none. Only in an acrostic chapter, where the verses start so.
*/
static size_t other_spelling(const char *v)
{
        const char *p = v;
        int letters = 0;

        if (!capital_at(p))
                return (0);
        while (*p != '\0' && *p != '.' && *p != ' ')
        {
                if (UCHAR(*p) < 0x80 && !isalpha(UCHAR(*p)))
                        return (0);
                if ((UCHAR(*p) & 0xC0) != 0x80)
                        letters++;
                p++;
        }
        if (*p != '.' || (p[1] != ' ' && p[1] != '\t') || letters < 2 || letters > 8)
                return (0);
        return ((size_t) (p + 1 - v));
}

/*
** Purpose: a verse's text with its dividers set apart (into out): each a
**          part of its own, after a tab, that starts with "= " (a heading:
**          see "Notes" in readme.architecture, and next_part() in
**          userint.c); the words before it and
**          after it parts of their own. kind: acrostic_kind(). *expect:
**          the next letter in the alphabet's order (in an acrostic other
**          than Psalm 119, a divider is taken only if it is the next
**          letter, or a few further on, or Aleph, starting again; -1, as
**          a chapter starts: any).
*/
static void set_letters_apart(const char *v, Alphabet *a, int kind, long verse,
                              long last, int *expect, Str *out)
{
        size_t i, len, from = 0, n;
        int idx, glyph, take, parts = 0;

        str_clear(out);
        for (i = 0; v[i] != '\0'; i++)
        {
                if (i > 0 && v[i - 1] != ' ' && v[i - 1] != '\t')
                        continue;
                /* (in an acrostic, a name not spelled as in Psalm 119, at a
                   verse's start: the Douay-Rheims's "Ghimel." in
                   Lamentations, "GIMEL." in its Psalm 118) */
                if (kind == 1 && i == 0 && (len = other_spelling(v)) > 0)
                {
                        str_adds(out, "\t= ");
                        str_add(out, v, len);
                        parts++;
                        for (from = len; v[from] == ' ' || v[from] == '\t'; from++)
                                ;
                        i = from - 1;
                        continue;
                }
                if ((len = divider_at(v + i, a, &idx, &glyph)) == 0 ||
                    (parts > 0 && i == from) || /* (just after another: a
                                                   verse's first word, "He
                                                   hath built", after
                                                   "Ghimel.") */
                    !set_apart(v, i, glyph) ||
                    (!a->named && !glyph && !(kind == 2 && idx == 0)))
                        continue;   /* (not named: by its letter only; or
                                       Aleph's name, which is known, before
                                       Psalm 119:1, as the World English
                                       Bible's "<<ALEPH>>") */
                if (kind == 2)          /* Psalm 119: a stanza's place, at
                                           its end or its start; or its
                                           letter before each of its
                                           verses (the KJV Cambridge
                                           edition's "(א)") */
                        take = (v[i + len] == '\0' && verse % 8 == 0 &&
                                verse < 176 && idx == verse / 8) ||
                               (i == 0 && verse % 8 == 1 && idx == verse / 8) ||
                               (i == 0 && glyph && idx == (verse - 1) / 8) ||
                               (verse == 1 && idx == 0);
                else if (kind == 3)     /* the chapter before: its end */
                        take = (v[i + len] == '\0' && verse == last && idx == 0);
                else    /* (*expect -1: a chapter's first, any letter: Psalm
                           10 goes on from Psalm 9's; the same letter again:
                           Lamentations 3's three verses each) */
                        take = (*expect < 0 || idx == 0 ||
                                (idx >= *expect - 1 && idx <= *expect + 3));
                if (!take)
                        continue;
                *expect = idx + 1;
                /* the words before it */
                for (n = i; n > from && (v[n - 1] == ' ' || v[n - 1] == '\t'); n--)
                        ;
                if (n > from)
                {
                        if (parts > 0)
                                str_adds(out, "\t");
                        str_add(out, v + from, n - from);
                        parts++;
                }
                str_adds(out, "\t= ");
                str_add(out, v + i, len);
                parts++;
                for (from = i + len; v[from] == ' ' || v[from] == '\t'; from++)
                        ;
                i = from > 0 ? from - 1 : 0;
        }
        if (parts == 0)
        {
                str_adds(out, v);
                return;
        }
        if (v[from] != '\0')
        {
                str_adds(out, "\t");
                str_adds(out, v + from);
        }
}

/*
** Purpose: the Hebrew alphabet's letters set apart, in the books of a
**          translation just written into folder (by add_to(), or
**          gutenberg.c), before its index is built: each book with Psalm
**          119, or another acrostic, written again.
*/
static void letters_apart(const char *folder)
{
        static Alphabet a;
        static char line[LINE];
        char path[1300], path2[1310], **names;
        FILE *f, *out;
        int nbooks, book, i, kind, expect = -1, wanted;
        long c, v, chap;
        const char *rest;
        Str apart = { NULL, 0, 0 };

        read_alphabet(folder, &a);
        if (a.p119_book == 0 && a.n == 0)
                return;
        if ((names = folder_books(folder, &nbooks)) == NULL)
                return;
        for (book = 1; book <= nbooks; book++)
        {
                wanted = (book == a.p119_book);
                for (i = 0; i < a.n; i++)
                        if (a.book[i] == book)
                                wanted = TRUE;
                if (!wanted)
                        continue;
                sprintf(path, "%.1000s/%.200s", folder, names[book - 1]);
                sprintf(path2, "%s.new", path);
                if ((f = fopen(path, "r")) == NULL)
                        continue;
                if ((out = fopen(path2, "w")) == NULL)
                {
                        fclose(f);
                        continue;
                }
                chap = -1;
                while (read_line(f, line, sizeof(line)))
                {
                        let_page_draw(FALSE);   /* (in a browser) */
                        if (!verse_line(line, &c, &v, &rest) ||
                            (kind = acrostic_kind(&a, book, c)) == 0)
                        {
                                fprintf(out, "%s\n", line);
                                continue;
                        }
                        if (c != chap)
                        {
                                chap = c;
                                expect = -1;
                        }
                        while (*rest == ' ')
                                rest++;
                        set_letters_apart(rest, &a, kind, v, a.before_last, &expect, &apart);
                        fprintf(out, "[%ld:%ld] %s\n", c, v, apart.s);
                }
                fclose(f);
                if (fclose(out) == 0)
                        rename(path2, path);
                else
                        remove(path2);
        }
        free(apart.s);
        free_books(names, nbooks);
}
