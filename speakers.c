/*
** speakers.c: the Song of Songs' speakers in a translation, set apart as
** headings (included by import.c, after acrostic.c; see "The speakers"
** in readme.architecture). The designer, November 22, 2026.
**
** Many translations print who speaks in the Song of Songs, as headings a
** translator adds ("The headings identifying the speakers are not in the
** original text": the NLT's note on 1:1). In the text as plain words they
** are run into the verses: at a verse's end, naming who speaks next
** ("...more wonderful than any other. Young Woman", the NLT's 1:1; "She",
** "Beloved", "Esposa", "Te Wahine"), or in the middle of one ("...more
** than wine! Beloved They are right to love you.", the World English
** Bible's 1:4). Each one found is taken out of the words around it and
** kept as a heading of its own, a part after a tab that starts with "= ",
** shown on its own line with "===" in the margin (see next_part() in
** userint.c), and left out of what voice says. Its words are kept as the
** translation writes them.
**
** Found without knowing the book or the language, from the text itself:
** a label is a few words (one to five) of letters, the first a capital,
** after a clause's end, at a verse's end. Such words end a verse now and
** then in any book, where a sentence goes on into the next verse ("...and
** said to him, Stay with them"), so a book is taken to have speakers only
** if its labels look like a cast: two different ones found three times or
** more, changing from one to the other more than once; few of them,
** telling most of what was found (eight at most found twice or more, and
** those seven in ten of all found); and many, twice as many as the book
** has chapters. Psalms' "Interlude" (the NLT's Selah), found 62 times, is
** one label, never changing to another: it is the Hebrew text's own word,
** and stays in its verse. Then, in that book only, every label of that
** shape at a verse's end is one (the NLT's "The Young Woman's Brothers",
** found once), and in the middle of a verse, one of those found at a
** verse's end, with a sentence starting after it.
**
** Not set apart: a label with a colon, before the words spoken ("Ela:
** Beije-me...", "Sulammith: Je suis noire...", in four translations of
** scrollmapper/bible_databases). The prophets have the same shape ("Thus
** says the Lord God:", "Miqueias diz:"), often enough to look like a cast,
** and they are the text: a label with a colon is left where it is, rather
** than guessed at; its colon already sets it apart.
*/

#define SPEAKER_WORDS   5       /* (the most words in a label) */
#define SPEAKERS        64      /* (the most labels learned in a book) */
#define SPEAKER_LEN     96      /* (the longest label, in bytes) */
#define CAST_MOST       8       /* (the most speakers found twice or more) */

typedef struct
{
        char name[SPEAKERS][SPEAKER_LEN];
        int count[SPEAKERS];
        int n;
        int found;              /* (labels found, of every name) */
        int changes;            /* (from one name to another, in order) */
        int last;               /* (the last one's index, -1: none yet) */
        int cast;               /* (TRUE: the book has speakers) */
} Cast;

/* Whether a capital letter starts p, in any script that has them: a letter
   that isn't its own small letter ("Ƙ", "Ä", "Л") */
static int is_capital(const char *p)
{
        char one[8], low[8];
        int n = u8_len((char *) p);

        if (n <= 0 || n >= (int) sizeof(one) || !u8_is_letter((char *) p))
                return (FALSE);
        memcpy(one, p, (size_t) n);
        one[n] = '\0';
        strcpy(low, one);
        u8_lower(low);
        return (strcmp(low, one) != 0);
}

/* Whether p starts a label's word: a letter, or an apostrophe ("L'une") */
static int label_char(const char *p)
{
        return (u8_is_letter((char *) p) || u8_is_apostrophe((char *) p) ||
                *p == '\'');
}

/*
** Purpose: the label that ends a part's text (from start to end, the end
**          past any spaces): where it starts, or NULL. A run of one to
**          SPEAKER_WORDS words of letters only, the first a capital,
**          after a clause's end (clause_ends(), acrostic.c).
*/
static const char *label_ending(const char *start, const char *end)
{
        const char *p = end, *w;
        int words = 0;

        while (p > start && p[-1] == ' ')
                p--;
        if (p == start)
                return (NULL);
        for (;;)
        {
                /* (a word: back to the space before it) */
                w = p;
                while (w > start && w[-1] != ' ')
                        w--;
                if (w == p)
                        return (NULL);
                {
                        const char *q;

                        for (q = w; q < p; q += u8_len((char *) q))
                                if (!label_char(q))
                                        return (NULL);  /* (punctuation:
                                                           not a label's) */
                }
                words++;
                if (words > SPEAKER_WORDS)
                        return (NULL);
                /* (the clause before it ends here: the label starts at w) */
                if (w > start && clause_ends(start, w))
                        return (is_capital(w) ? w : NULL);
                if (w == start)
                        return (NULL);  /* (a part that is only words: not
                                           set apart from anything) */
                p = w;
                while (p > start && p[-1] == ' ')
                        p--;
        }
}

/* A label found (n bytes at w): counted, by its name */
static void cast_add(Cast *k, const char *w, size_t n)
{
        int i;

        if (n == 0 || n >= SPEAKER_LEN)
                return;
        for (i = 0; i < k->n; i++)
                if (strlen(k->name[i]) == n && strncmp(k->name[i], w, n) == 0)
                        break;
        if (i == k->n)
        {
                if (k->n >= SPEAKERS)
                        return;
                memcpy(k->name[i], w, n);
                k->name[i][n] = '\0';
                k->count[i] = 0;
                k->n++;
        }
        k->count[i]++;
        k->found++;
        if (k->last >= 0 && k->last != i)
                k->changes++;
        k->last = i;
}

/* Whether n bytes at w are one of the labels learned */
static int cast_has(Cast *k, const char *w, size_t n)
{
        int i;

        for (i = 0; i < k->n; i++)
                if (strlen(k->name[i]) == n && strncmp(k->name[i], w, n) == 0)
                        return (TRUE);
        return (FALSE);
}

/* Whether the labels found look like a cast (see the top): set in k->cast */
static void cast_decide(Cast *k, int chapters)
{
        int i, twice = 0, thrice = 0, found = 0;

        for (i = 0; i < k->n; i++)
                if (k->count[i] >= 2)
                {
                        twice++;
                        found += k->count[i];
                        if (k->count[i] >= 3)
                                thrice++;
                }
        k->cast = (thrice >= 2 && twice <= CAST_MOST && k->changes >= 2 &&
                   found >= 2 * chapters && found * 10 >= k->found * 7);
}

/* Whether a part (from p, to its tab or the end) is a note or a heading:
   "- ..." or "= ..." */
static int marked_part(const char *p)
{
        return ((p[0] == '-' || p[0] == '=') && (p[1] == ' ' || p[1] == '\t' ||
                                                  p[1] == '\0'));
}

/* Into out: the words from `from` to `to` (the spaces before `to` left
   out), if any, then a tab, so that what is added next is a part of its
   own; at the verse's start, the tab alone (a heading before its words:
   see lead_note() in userint.c) */
static void words_then_tab(Str *out, const char *from, const char *to)
{
        while (to > from && to[-1] == ' ')
                to--;
        if (to > from)
                str_add(out, from, (size_t) (to - from));
        if (out->len == 0 || out->s[out->len - 1] != '\t')
                str_adds(out, "\t");
}

/* A label as a heading, into out (n bytes at w) */
static void heading(Str *out, const char *w, size_t n)
{
        str_adds(out, "= ");
        str_add(out, w, n);
}

/*
** Purpose: the labels in one verse's text, learned into the cast (learn
**          TRUE), or set apart into out (learn FALSE): a verse's parts
**          (after its tabs) each looked at but notes and headings.
*/
static void verse_speakers(const char *v, Cast *ends, int learn, Str *out)
{
        const char *part = v, *tab, *p, *q, *note, *text_end, *from, *at, *e;
        int words, k, last;

        if (!learn)
                str_clear(out);
        for (;;)
        {
                tab = strchr(part, '\t');
                if (tab == NULL)
                        tab = part + strlen(part);
                last = (*tab == '\0');
                if (marked_part(part))
                {
                        if (!learn)
                                str_add(out, part, (size_t) (tab - part));
                        goto next;
                }
                /* (a note run on into a label: "Young Woman#1:1 The
                   headings...", the NLT's 1:1, a "#" and a reference
                   starting it) */
                note = NULL;
                for (p = part + 1; p < tab; p++)
                        if (*p == '#' && u8_is_letter((char *) p - 1) && isdigit(UCHAR(p[1])))
                        {
                                note = p;
                                break;
                        }
                text_end = note != NULL ? note : tab;
                from = part;
                /* a label within the verse, one learned at a verse's end,
                   after a clause's end, with a sentence starting after it
                   ("...more than wine! Beloved They are right...") */
                for (p = part + 1; !learn && ends->cast && p < text_end; p++)
                {
                        if (!(p[-1] == ' ' && clause_ends(part, p)))
                                continue;
                        q = p;
                        for (words = 1; words <= SPEAKER_WORDS; words++)
                        {
                                while (q < text_end && label_char(q))
                                        q += u8_len((char *) q);
                                if (q >= text_end || *q != ' ')
                                        break;
                                for (k = 1; q[k] == ' '; k++)
                                        ;
                                if (cast_has(ends, p, (size_t) (q - p)) && is_capital(q + k))
                                {
                                        words_then_tab(out, from, p);
                                        heading(out, p, (size_t) (q - p));
                                        str_adds(out, "\t");
                                        from = q + k;
                                        p = from - 1;
                                        break;
                                }
                                q++;
                        }
                }
                /* a label at the end: the verse's last part's, or before a
                   note run on into it */
                at = (last || note != NULL) ? label_ending(from, text_end) : NULL;
                e = text_end;
                while (e > from && e[-1] == ' ')
                        e--;
                if (learn)
                {
                        if (at != NULL)
                                cast_add(ends, at, (size_t) (e - at));
                        goto next;
                }
                if (at != NULL && ends->cast)
                {
                        words_then_tab(out, from, at);
                        heading(out, at, (size_t) (e - at));
                }
                else if (e > from)
                        str_add(out, from, (size_t) (e - from));
                else if (out->len > 0 && out->s[out->len - 1] == '\t')
                        out->len--;     /* (nothing after a heading: no
                                           empty part) */
                if (note != NULL)
                {
                        str_adds(out, "\t- ");
                        str_add(out, note + 1, (size_t) (tab - note - 1));
                }
        next:
                if (last)
                        break;
                if (!learn)
                        str_adds(out, "\t");
                part = tab + 1;
        }
        if (!learn)
        {
                if (out->s == NULL)
                        str_adds(out, "");
                out->s[out->len] = '\0';
        }
}

/*
** Purpose: read a translation's books, as written into its folder, for
**          each one's labels (see the top), and write again the books
**          that have a cast of them, each label set apart. A chapter is
**          counted by its number changing.
*/
static void speakers_apart(const char *folder)
{
        static char line[LINE];
        static Cast ends;
        char path[1300], path2[1310], **names;
        FILE *f, *out;
        int nbooks, book, chapters;
        long c, v, chap;
        const char *rest;
        Str apart = { NULL, 0, 0 };

        if ((names = folder_books(folder, &nbooks)) == NULL)
                return;
        for (book = 1; book <= nbooks; book++)
        {
                sprintf(path, "%.1000s/%.200s", folder, names[book - 1]);
                if ((f = fopen(path, "r")) == NULL)
                        continue;
                memset(&ends, 0, sizeof(ends));
                ends.last = -1;
                chapters = 0;
                chap = -1;
                while (read_line(f, line, sizeof(line)))
                {
                        let_page_draw(FALSE);   /* (in a browser) */
                        if (!verse_line(line, &c, &v, &rest))
                                continue;
                        if (c != chap)
                        {
                                chap = c;
                                chapters++;
                        }
                        while (*rest == ' ')
                                rest++;
                        verse_speakers(rest, &ends, TRUE, NULL);
                }
                fclose(f);
                cast_decide(&ends, chapters);
                if (!ends.cast)
                        continue;
                sprintf(path2, "%s.new", path);
                if ((f = fopen(path, "r")) == NULL)
                        continue;
                if ((out = fopen(path2, "w")) == NULL)
                {
                        fclose(f);
                        continue;
                }
                while (read_line(f, line, sizeof(line)))
                {
                        let_page_draw(FALSE);
                        if (!verse_line(line, &c, &v, &rest))
                        {
                                fprintf(out, "%s\n", line);
                                continue;
                        }
                        while (*rest == ' ')
                                rest++;
                        verse_speakers(rest, &ends, FALSE, &apart);
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
