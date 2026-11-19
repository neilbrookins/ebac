/*
   EBaC (Electronic Bible and Concordance)
   written by Neil G. Brookins
*/
/*
** import.c -- the importer of translations, in C (see "The importers in
** C" in readme.architecture): what importer.sh and the shell importers of
** each source did, so that adding a translation builds for every platform
** EBaC runs on, a web browser too, from one source. Only downloading
** differs by platform (net.c).
**
** Ported a source at a time, each checked byte for byte against the shell
** importer's output (the folder it writes, and what it says;
** tests/importers.sh), and A's screens key by key (tests/importscreens.py):
** thiagobodruk/bible, scrollmapper/bible_databases, and Project Gutenberg
** (gutenberg.c). The shell importers were then retired (November 17,
** 2026); the comments here still name their parts, as git keeps them
** (commit 72ace7e, the last with them). A on EBaC's main menu runs it in
** EBaC itself (import_in_ebac()); from the command line:
**
**   bible --import SOURCE [--source PLACE] [--names-source PLACE]
**         [--agree] [--agreed-how TEXT] [--at-end] [NAME ...]
**         [--remove NAME ...]
**   bible --import SOURCE --list
**
** SOURCE: thiagobodruk or scrollmapper (gutenberg: see gutenberg.c); with
** no NAME, the lists of languages and translations, to choose from. For
** programs (ebac-admin,
** which adds and removes translations for the EBaC integration's voice
** commands), plain lines separated by tabs:
**
**   --catalog         every language ("language", code, English name, own
**                     name, number of translations) and translation
**                     ("translation", name, language code, title, 1 if
**                     added), one per line
**   --license NAME    its title, license and details page, a line each
**                     ("Title:", "License:", "Details:")
**   --check NAME      "added" or "not added"; then, if added and the source
**                     has the same version, "same version", and the date
**                     downloaded
**   --at-end          adding: the translation goes at the end of 'trans',
**                     not the top (so that EBaC doesn't start with it)
**   --agreed-how TEXT with --agree: how it was agreed to, for its license
**                     file ("by voice, from Kitchen")
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <errno.h>
#include <setjmp.h>
#include "both.h"
#include "net.h"
#include "index.h"
#include "import.h"
#include "bible.h"
#include "screen.h"

/* (the spinner, if it is turning, erased before anything else is written:
   see let_page_draw() in net.h) */
#define printf busy_printf
#define fprintf busy_fprintf
#ifdef UNIXV
#       include <unistd.h>
#       include <sys/stat.h>
#       include <sys/types.h>
#       include <dirent.h>
#       include <fcntl.h>
#       include <sys/ioctl.h>
#       ifndef __EMSCRIPTEN__
#               include <sys/statvfs.h>
#       endif
#endif

#define LINE 65536              /* the longest line read (a verse); each
                                   such buffer is static: too big for a
                                   stack of 64 KB, as EBaC has in a web
                                   browser, where a deeper stack writes
                                   over the program's other memory */

/* ------------------------------------------------------------------------
** A growing string
*/
typedef struct
{
        char *s;
        size_t len, size;
} Str;

static void str_add(Str *b, const char *s, size_t n)
{
        if (b->len + n + 1 > b->size)
        {
                b->size = (b->len + n + 1) * 2 + 64;
                b->s = (char *) realloc(b->s, b->size);
                if (b->s == NULL)
                {
                        printf("Out of memory.\n");
                        exit(1);
                }
        }
        memcpy(b->s + b->len, s, n);
        b->len += n;
        b->s[b->len] = '\0';
}

static void str_adds(Str *b, const char *s)
{
        str_add(b, s, strlen(s));
}

static void str_clear(Str *b)
{
        b->len = 0;
        if (b->s != NULL)
                b->s[0] = '\0';
}

/* ------------------------------------------------------------------------
** The state of one run
*/
static char here[1024];         /* EBaC's folder: 'trans', the translations */
static char tmp[1100];          /* a folder for downloads, removed at the end */
static char problem[4096];      /* why the last step failed */
static const char *source;      /* where the source's files are */
static int agree_all = FALSE;
static const char *agreed_how = "";
static int at_end = FALSE;
static int failed = 0;

/* The source, as importer.sh's PROGRAM, SOURCE, OWN_MARK and functions */
typedef struct
{
        const char *program;    /* "add-thiagobodruk" */
        const char *source;     /* the default SOURCE */
        const char *mark;       /* OWN_MARK, "" for none */
        const char *repo;       /* where, as usage() says it */
        const char *example;    /* a short name, as usage() gives one */
        int (*all_translations)(void);
        void (*license_of)(const char *name);
        void (*version_of)(const char *name, char *version, int size);
        int (*fetch_text)(const char *name, const char *path);
} Source;

static const Source *src;

/* The translations, from all_translations: as $tmp/all's lines */
typedef struct
{
        char *name;             /* the short name, as EBaC knows it */
        char *code;             /* its language's code */
        char *title;
        char *in_source;        /* its name in the source */
} Translation;

static Translation *all = NULL;
static int nall = 0, all_size = 0;

static char license_line[1024], details_line[1024];

/* ------------------------------------------------------------------------
** Text as importer.sh's awk programs have it
*/

/* spaces(s): white space as Unicode counts it as single ASCII spaces, none
   at either end (importer.sh's spaces(), plain()). In place. */
static void spaces(char *s)
{
        static const char *wide[] = {
                "\302\240", "\302\205", "\341\232\200", "\343\200\200",
                "\342\201\237", "\342\200\200", "\342\200\201", "\342\200\202",
                "\342\200\203", "\342\200\204", "\342\200\205", "\342\200\206",
                "\342\200\207", "\342\200\210", "\342\200\211", "\342\200\212",
                "\342\200\250", "\342\200\251", "\342\200\257", NULL
        };
        char *r = s, *w = s;
        int k, n = 0;

        while (*r != '\0')
        {
                for (k = 0; wide[k] != NULL; k++)
                {
                        n = (int) strlen(wide[k]);
                        if (strncmp(r, wide[k], (size_t) n) == 0)
                                break;
                }
                if (wide[k] != NULL)
                {
                        *w++ = ' ';
                        r += n;
                }
                else if (*r == '\t' || *r == '\r' || *r == '\v' || *r == '\f' ||
                         (*r >= '\034' && *r <= '\037'))
                {
                        *w++ = ' ';
                        r++;
                }
                else
                        *w++ = *r++;
        }
        *w = '\0';
        /* runs of spaces as one, none at either end */
        for (r = w = s; *r != '\0'; r++)
                if (!(*r == ' ' && (w == s || w[-1] == ' ')))
                        *w++ = *r;
        if (w > s && w[-1] == ' ')
                w--;
        *w = '\0';
}

#define plain spaces

/* The first n characters of s (UTF-8: a continuation byte is no new one) */
static void first_chars(char *s, int n)
{
        int count = 0;
        char *p;

        for (p = s; *p != '\0'; p++)
                if ((UCHAR(*p) & 0xC0) != 0x80 && ++count > n)
                {
                        *p = '\0';
                        return;
                }
}

static int all_digits(const char *s)
{
        if (*s == '\0')
                return (FALSE);
        for (; *s != '\0'; s++)
                if (!isdigit(UCHAR(*s)))
                        return (FALSE);
        return (TRUE);
}

/* ------------------------------------------------------------------------
** Files
*/

static int read_line(FILE *f, char *line, int size)
{
        int n;

        if (fgets(line, size, f) == NULL)
                return (FALSE);
        n = (int) strlen(line);
        if (n > 0 && line[n - 1] == '\n')
                line[--n] = '\0';
        return (TRUE);
}

static int copy_file(const char *from, const char *to)
{
        FILE *in, *out;
        char buf[8192];
        size_t n;

        if ((in = fopen(from, "rb")) == NULL)
                return (-1);
        if ((out = fopen(to, "wb")) == NULL)
        {
                fclose(in);
                return (-1);
        }
        while ((n = fread(buf, 1, sizeof(buf), in)) > 0)
                fwrite(buf, 1, n, out);
        fclose(in);
        return (fclose(out) == 0 ? 0 : -1);
}

static int exists(const char *path)
{
        struct stat st;

        return (stat(path, &st) == 0);
}

static int is_link(const char *path)
{
        struct stat st;

        return (lstat(path, &st) == 0 && S_ISLNK(st.st_mode));
}

/* rm -rf: a folder and all in it (or a file) */
static int remove_all(const char *path)
{
        struct stat st;
        DIR *d;
        struct dirent *e;
        char inner[2048];
        int ok = 0;

        if (lstat(path, &st) != 0)
                return (0);
        if (!S_ISDIR(st.st_mode))
                return (unlink(path));
        if ((d = opendir(path)) == NULL)
                return (-1);
        while ((e = readdir(d)) != NULL)
        {
                if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
                        continue;
                sprintf(inner, "%.1000s/%.1000s", path, e->d_name);
                if (remove_all(inner) != 0)
                        ok = -1;
        }
        closedir(d);
        if (rmdir(path) != 0)
                ok = -1;
        return (ok);
}

static void today(char *date)
{
        time_t now = time(NULL);
        struct tm *t = localtime(&now);

        sprintf(date, "%04d-%02d-%02d", t->tm_year + 1900, t->tm_mon + 1, t->tm_mday);
}

/* POSIX cksum: its CRC and the length, as "cksum CRC LENGTH" (version_of,
   for a folder as the source) */
static int cksum(const char *path, char *out)
{
        static unsigned long table[256];
        static int made = FALSE;
        unsigned long crc = 0, len = 0, n;
        FILE *f;
        int c, i, j;

        if (!made)
        {
                for (i = 0; i < 256; i++)
                {
                        unsigned long r = (unsigned long) i << 24;

                        for (j = 0; j < 8; j++)
                                r = (r & 0x80000000UL) ? ((r << 1) ^ 0x04C11DB7UL) : (r << 1);
                        table[i] = r & 0xFFFFFFFFUL;
                }
                made = TRUE;
        }
        if ((f = fopen(path, "rb")) == NULL)
                return (-1);
        while ((c = getc(f)) != EOF)
        {
                crc = ((crc << 8) ^ table[((crc >> 24) ^ (unsigned long) c) & 0xFF]) & 0xFFFFFFFFUL;
                len++;
        }
        fclose(f);
        for (n = len; n != 0; n >>= 8)
                crc = ((crc << 8) ^ table[((crc >> 24) ^ n) & 0xFF]) & 0xFFFFFFFFUL;
        crc = ~crc & 0xFFFFFFFFUL;
        sprintf(out, "cksum %lu %lu", crc, len);
        return (0);
}

/* ------------------------------------------------------------------------
** Downloading: fetch_from() (a web address, or a folder)
*/
static int is_web(const char *place)
{
        return (strncmp(place, "http://", 7) == 0 || strncmp(place, "https://", 8) == 0);
}

static int fetch_from(const char *from, const char *path, const char *file)
{
        char url[2048];
        size_t n = strlen(from);

        if (is_web(from))
        {
                sprintf(url, "%.*s/%.1000s", (int) (n > 0 && from[n - 1] == '/' ? n - 1 : n),
                        from, path);
                if (net_get(url, file) != 0)
                {
                        strcpy(problem, net_problem);
                        return (-1);
                }
                return (0);
        }
        sprintf(url, "%.1000s/%.1000s", from, path);
        if (!exists(url))
        {
                sprintf(problem, "couldn't read %.1000s (No such file or directory)", url);
                return (-1);
        }
        if (copy_file(url, file) != 0)
        {
                sprintf(problem, "couldn't read %.1000s", url);
                return (-1);
        }
        return (0);
}

static int fetch(const char *path, const char *file)
{
        return (fetch_from(source, path, file));
}

/* the fingerprint of a file of the source: its ETag, or its cksum */
static void version_at(const char *path, char *version, int size)
{
        char url[2048];
        size_t n = strlen(source);

        version[0] = '\0';
        if (is_web(source))
        {
                sprintf(url, "%.*s/%.1000s", (int) (n > 0 && source[n - 1] == '/' ? n - 1 : n),
                        source, path);
                net_version(url, version, size);
        }
        else
        {
                sprintf(url, "%.1000s/%.1000s", source, path);
                if (exists(url))
                        cksum(url, version);
        }
}

/* ------------------------------------------------------------------------
** Room on the disk (importer.sh's enough_room): 1 GB free on the disk of
** the translations, and of the downloads
*/
#define MIN_FREE_MB 1024L

static int room_on(const char *folder, const char *what)
{
#if defined(UNIXV) && !defined(__EMSCRIPTEN__)
        struct statvfs fs;
        unsigned long long free_kb;

        if (statvfs(folder, &fs) != 0)
        {
                sprintf(problem, "couldn't find how much space is free on %.500s, so nothing is added", what);
                return (FALSE);
        }
        free_kb = (unsigned long long) fs.f_bavail * fs.f_frsize / 1024;
        if (free_kb < (unsigned long long) MIN_FREE_MB * 1024)
        {
                sprintf(problem, "not enough disk space: %llu MB free on %.500s, and adding needs at least %ld MB free",
                        free_kb / 1024, what, MIN_FREE_MB);
                return (FALSE);
        }
#else
        (void) folder;
        (void) what;
#endif
        return (TRUE);
}

static int enough_room(void)
{
        char what[1200], *slash;

        strcpy(what, tmp);
        if ((slash = strrchr(what, '/')) != NULL)
                *slash = '\0';
        if (!room_on(here, "the disk EBaC keeps its translations on"))
                return (FALSE);
        {
                char about[1300];

                sprintf(about, "the disk it downloads to (%.1200s)", what);
                return (room_on(tmp, about));
        }
}

/* ------------------------------------------------------------------------
** 'trans'
*/

/* the first word of a line, as awk's $1 */
static void first_word(const char *line, char *word, int size)
{
        int n;

        while (*line == ' ' || *line == '\t')
                line++;
        n = (int) strcspn(line, " \t");
        if (n >= size)
                n = size - 1;
        memcpy(word, line, (size_t) n);
        word[n] = '\0';
}

static int is_listed(const char *name)
{
        char path[1100], word[256];
        static char line[LINE];
        FILE *f;
        int found = FALSE;

        sprintf(path, "%s/trans", here);
        if ((f = fopen(path, "r")) == NULL)
                return (FALSE);
        while (!found && read_line(f, line, sizeof(line)))
        {
                first_word(line, word, sizeof(word));
                if (word[0] != '\0' && word[0] != '#' && strcmp(word, name) == 0)
                        found = TRUE;
        }
        fclose(f);
        return (found);
}

/* trans without the line of NAME, added to b */
static void without_line(const char *name, Str *b)
{
        char path[1100], word[256];
        static char line[LINE];
        FILE *f;

        sprintf(path, "%s/trans", here);
        if ((f = fopen(path, "r")) == NULL)
                return;
        while (read_line(f, line, sizeof(line)))
        {
                first_word(line, word, sizeof(word));
                if (strcmp(word, name) != 0)
                {
                        str_adds(b, line);
                        str_adds(b, "\n");
                }
        }
        fclose(f);
}

/* 'trans' becomes TEXT, all at once (importer.sh's replace_trans): written
   beside it and renamed into place, its permissions kept; through a
   symbolic link, in place */
static int replace_trans(const char *text)
{
        char path[1100], new_path[1200];
        FILE *f;
        struct stat st;

        sprintf(path, "%s/trans", here);
        if (is_link(path))
        {
                if ((f = fopen(path, "w")) == NULL)
                        return (-1);
                fputs(text, f);
                return (fclose(f) == 0 ? 0 : -1);
        }
        sprintf(new_path, "%s.%ld", path, (long) getpid());
        if ((f = fopen(new_path, "w")) == NULL)
                return (-1);
        fputs(text, f);
        if (fclose(f) != 0 ||
            (stat(path, &st) == 0 && chmod(new_path, st.st_mode & 07777) != 0) ||
            rename(new_path, path) != 0)
        {
                unlink(new_path);
                return (-1);
        }
        return (0);
}

/* whether it has been added: trans lists it, or its folder is here (not a
   link: the Home Assistant app's own are links) */
static int is_added(const char *name)
{
        char path[1300];

        if (is_listed(name))
                return (TRUE);
        sprintf(path, "%s/%s", here, name);
        if (is_link(path))
                return (FALSE);
        sprintf(path, "%s/%s/booklist", here, name);
        return (exists(path));
}

/* whether FOLDER holds another importer's text (importer.sh's others_text) */
static int others_text(const char *folder)
{
        char path[1300];
        const char *base = strrchr(folder, '/') ? strrchr(folder, '/') + 1 : folder;

        sprintf(path, "%s/gutenberg", folder);
        if (strcmp(src->mark, "gutenberg") != 0 && exists(path))
        {
                sprintf(problem, "%.200s is a text from Project Gutenberg (A, then P, removes it)", base);
                return (TRUE);
        }
        sprintf(path, "%s/thiagobodruk", folder);
        if (strcmp(src->mark, "thiagobodruk") != 0 && exists(path))
        {
                sprintf(problem, "%.200s is a text from thiagobodruk/bible (A, then T, removes it)", base);
                return (TRUE);
        }
        return (FALSE);
}

/* the version kept when it was added: "FINGERPRINT", and its date */
static int kept_version(const char *name, char *version, char *date)
{
        char path[1300], *p;
        static char line[LINE];
        FILE *f;

        version[0] = date[0] = '\0';
        sprintf(path, "%s/%s/version", here, name);
        if ((f = fopen(path, "r")) == NULL)
                return (FALSE);
        while (read_line(f, line, sizeof(line)))
        {
                if (strncmp(line, "Version:", 8) == 0)
                {
                        /* (awk: the fields after the first, joined by a space) */
                        Str b = { NULL, 0, 0 };
                        char *tok;

                        for (tok = strtok(line + 8, " \t"); tok != NULL; tok = strtok(NULL, " \t"))
                        {
                                if (b.len > 0)
                                        str_adds(&b, " ");
                                str_adds(&b, tok);
                        }
                        strcpy(version, b.s ? b.s : "");
                        free(b.s);
                }
                else if (strncmp(line, "Downloaded:", 11) == 0)
                {
                        p = line + 11;
                        while (*p == ' ' || *p == '\t')
                                p++;
                        sscanf(p, "%31s", date);
                }
        }
        fclose(f);
        return (version[0] != '\0');
}

/* ------------------------------------------------------------------------
** The list of translations
*/
static void add_translation(const char *name, const char *code, const char *title,
                            const char *in_source)
{
        if (nall == all_size)
        {
                all_size = all_size * 2 + 64;
                all = (Translation *) realloc(all, sizeof(Translation) * (size_t) all_size);
        }
        all[nall].name = strdup(name);
        all[nall].code = strdup(code);
        all[nall].title = strdup(title);
        all[nall].in_source = strdup(in_source);
        nall++;
}

static Translation *find(const char *word)
{
        int i, j;

        for (i = 0; i < nall; i++)
        {
                for (j = 0; word[j] != '\0' && all[i].name[j] != '\0' &&
                     tolower(UCHAR(word[j])) == tolower(UCHAR(all[i].name[j])); j++)
                        ;
                if (word[j] == '\0' && all[i].name[j] == '\0')
                        return (&all[i]);
        }
        return (NULL);
}

/* ------------------------------------------------------------------------
** thiagobodruk/bible (add-thiagobodruk.sh)
*/
#define TB_LIST "json/index.json"
#define TB_LICENSE_PAGE "https://github.com/thiagobodruk/bible#what-about-the-license-and-the-copyrights"

/* Every translation: the list is JSON, read by its quoted strings, whatever
   its lines (as add-thiagobodruk.sh's awk reads it: the text split at each
   ", a string followed by ":" a key, the next string its value; a version
   ends at the "}" after it) */
static int tb_all_translations(void)
{
        char path[1200], *text, *p, **tok = NULL, key_abbr[512];
        FILE *f;
        long size;
        int ntok = 0, i, k;
        struct { char *key, *value; } value[64];
        int nvalue = 0;

        sprintf(path, "%s/list.json", tmp);
        if (fetch(TB_LIST, path) != 0)
                return (-1);
        if ((f = fopen(path, "rb")) == NULL)
                return (-1);
        fseek(f, 0L, 2);
        size = ftell(f);
        fseek(f, 0L, 0);
        text = (char *) malloc((size_t) size + 1);
        size = (long) fread(text, 1, (size_t) size, f);
        text[size] = '\0';
        fclose(f);
        /* the tokens, as awk's records with RS = "\"" (1 to NR) */
        tok = (char **) malloc(sizeof(char *) * (size_t) (size + 2));
        tok[ntok++] = NULL;             /* (awk counts from 1) */
        for (p = text;;)
        {
                char *q = strchr(p, '"');

                tok[ntok++] = p;
                if (q == NULL)
                        break;
                *q = '\0';
                p = q + 1;
        }
        /* (awk's last record: a trailing newline goes, as awk ends records) */
        {
                char *last = tok[ntok - 1];
                size_t n = strlen(last);

                if (n > 0 && last[n - 1] == '\n')
                        last[n - 1] = '\0';
                if (*tok[ntok - 1] == '\0')
                        ntok--;         /* (no empty last record) */
        }
#define TOKEN(i) ((i) < ntok ? tok[i] : "")
        for (i = 2; i <= ntok - 1; i += 2)
        {
                const char *between = TOKEN(i + 1);
                const char *s;

                /* a key: the next token is only spaces and a ":" */
                for (s = between; *s == ' ' || *s == '\t' || *s == '\r' || *s == '\n'; s++)
                        ;
                if (*s == ':')
                {
                        for (s++; *s == ' ' || *s == '\t' || *s == '\r' || *s == '\n'; s++)
                                ;
                        if (*s == '\0')
                        {
                                for (k = 0; k < nvalue; k++)
                                        if (strcmp(value[k].key, TOKEN(i)) == 0)
                                                break;
                                if (k == nvalue && nvalue < 64)
                                        value[nvalue++].key = TOKEN(i);
                                if (k < 64)
                                        value[k].value = (char *) TOKEN(i + 2);
                                i += 2;
                        }
                }
                key_abbr[0] = '\0';
                for (k = 0; k < nvalue; k++)
                        if (strcmp(value[k].key, "abbreviation") == 0)
                                sprintf(key_abbr, "%.500s", value[k].value);
                if (strchr(TOKEN(i + 1), '}') != NULL && key_abbr[0] != '\0')
                {
                        char code[512], name[600], title[2048], *u;
                        const char *name_value = "";

                        u = strchr(key_abbr, '_');
                        if (u != NULL)
                                sprintf(code, "%.*s", (int) (u - key_abbr), key_abbr);
                        else
                                code[0] = '\0';
                        for (u = code; *u; u++)
                                *u = (char) tolower(UCHAR(*u));
                        sprintf(name, "%s-TB", strchr(key_abbr, '_') ? strchr(key_abbr, '_') + 1 : key_abbr);
                        for (u = name; *u; u++)
                                *u = (char) toupper(UCHAR(*u));
                        for (k = 0; k < nvalue; k++)
                                if (strcmp(value[k].key, "name") == 0)
                                        name_value = value[k].value;
                        sprintf(title, "%.2000s", name_value);
                        plain(title);
                        add_translation(name, code, title, key_abbr);
                        nvalue = 0;
                }
        }
#undef TOKEN
        free(tok);
        free(text);
        if (nall == 0)
        {
                sprintf(problem, "found no translations in the list at %.900s", source);
                return (-1);
        }
        return (0);
}

static void tb_license_of(const char *name)
{
        (void) name;
        strcpy(license_line, "the source says: property of its owner, all rights reserved");
        strcpy(details_line, TB_LICENSE_PAGE);
}

static void tb_version_of(const char *name, char *version, int size)
{
        char path[1200];
        Translation *t = find(name);

        sprintf(path, "xml/%s.xml", t ? t->in_source : "");
        version_at(path, version, size);
}

/* the UTF-8 bytes of character n, as add-thiagobodruk.sh's utf8() */
static void utf8_of(long n, Str *out)
{
        char b[5];
        size_t len;

        if (n < 128) { b[0] = (char) n; len = 1; }
        else if (n < 2048)
        {
                b[0] = (char) (192 + n / 64); b[1] = (char) (128 + n % 64); len = 2;
        }
        else if (n < 65536)
        {
                b[0] = (char) (224 + n / 4096); b[1] = (char) (128 + (n / 64) % 64);
                b[2] = (char) (128 + n % 64); len = 3;
        }
        else
        {
                b[0] = (char) (240 + n / 262144); b[1] = (char) (128 + (n / 4096) % 64);
                b[2] = (char) (128 + (n / 64) % 64); b[3] = (char) (128 + n % 64); len = 4;
        }
        if (len == 1 && n == 0)
                return;         /* (awk's "%c" of 0: nothing) */
        str_add(out, b, len);
}

/* XML's named and numbered characters as themselves (unescape()) */
static void unescape(const char *s, Str *out)
{
        const char *amp, *semi;
        char ent[64];
        int n;

        str_clear(out);
        while ((amp = strchr(s, '&')) != NULL)
        {
                str_add(out, s, (size_t) (amp - s));
                s = amp;
                semi = strchr(s, ';');
                n = semi ? (int) (semi - s - 1) : -1;
                if (n >= 0 && n < (int) sizeof(ent))
                {
                        memcpy(ent, s + 1, (size_t) n);
                        ent[n] = '\0';
                }
                else
                        ent[0] = '\0';
                if (semi != NULL && strcmp(ent, "amp") == 0) str_adds(out, "&");
                else if (semi != NULL && strcmp(ent, "lt") == 0) str_adds(out, "<");
                else if (semi != NULL && strcmp(ent, "gt") == 0) str_adds(out, ">");
                else if (semi != NULL && strcmp(ent, "quot") == 0) str_adds(out, "\"");
                else if (semi != NULL && strcmp(ent, "apos") == 0) str_adds(out, "'");
                else if (semi != NULL && ent[0] == '#' && all_digits(ent + 1))
                        utf8_of(atol(ent + 1), out);
                else if (semi != NULL && ent[0] == '#' && (ent[1] == 'x' || ent[1] == 'X') &&
                         ent[2] != '\0' && strspn(ent + 2, "0123456789abcdefABCDEF") == strlen(ent + 2))
                        utf8_of(strtol(ent + 2, NULL, 16), out);
                else
                {
                        str_adds(out, "&");
                        s++;
                        continue;
                }
                s = semi + 1;
        }
        str_adds(out, s);
}

/* an attribute of a tag: a="..." (the first " a=\"...\"" in it) */
static void attr(const char *tag, const char *a, Str *out)
{
        char pat[64], value[4096];
        const char *p = tag, *end;

        sprintf(pat, " %s=\"", a);
        str_clear(out);
        while ((p = strstr(p, pat)) != NULL)
        {
                end = strchr(p + strlen(pat), '"');
                if (end != NULL)
                {
                        sprintf(value, "%.*s", (int) (end - p - strlen(pat)) < 4000 ?
                                (int) (end - p - strlen(pat)) : 4000, p + strlen(pat));
                        unescape(value, out);
                        return;
                }
                p++;
        }
        str_adds(out, "");
}

/* its XML read into the plain-text layout (fetch_text) */
static int tb_fetch_text(const char *name, const char *text_path)
{
        char path[1200], *xml, *rec, *next, *close_at;
        Str chapter = { NULL, 0, 0 }, verse = { NULL, 0, 0 }, a = { NULL, 0, 0 },
            body = { NULL, 0, 0 }, idv = { NULL, 0, 0 };
        FILE *f, *out;
        long size, nr = 0;
        Translation *t = find(name);
        int bad = FALSE;

        sprintf(path, "xml/%s.xml", t ? t->in_source : "");
        {
                char file[1200];

                sprintf(file, "%s/xml", tmp);
                if (fetch(path, file) != 0)
                        return (-1);
                if ((f = fopen(file, "rb")) == NULL)
                {
                        strcpy(problem, "couldn't read its XML");
                        return (-1);
                }
        }
        fseek(f, 0L, 2);
        size = ftell(f);
        fseek(f, 0L, 0);
        xml = (char *) malloc((size_t) size + 1);
        size = (long) fread(xml, 1, (size_t) size, f);
        xml[size] = '\0';
        fclose(f);
        if ((out = fopen(text_path, "w")) == NULL)
        {
                free(xml);
                strcpy(problem, "couldn't read its XML");
                return (-1);
        }
        str_adds(&chapter, "");
        /* the records: what follows each "<" (awk, RS = "<": the first,
           before any tag, is skipped; awk takes the last newline off the
           last) */
        for (rec = strchr(xml, '<'); rec != NULL && !bad; rec = next)
        {
                char *tag, name_buf[256], *text;
                size_t n;

                let_page_draw(FALSE);   /* (in a browser: see net.h) */
                rec++;
                nr++;
                next = strchr(rec, '<');
                if (next != NULL)
                        *next = '\0';
                else
                {
                        n = strlen(rec);
                        if (n > 0 && rec[n - 1] == '\n')
                                rec[n - 1] = '\0';
                }
                close_at = strchr(rec, '>');
                if (close_at == NULL)
                {
                        sprintf(problem, "its XML: a tag without its end, at tag %ld", nr + 1);
                        bad = TRUE;
                        break;
                }
                *close_at = '\0';
                tag = rec;
                text = close_at + 1;
                n = strcspn(tag, " \t\r\n");
                if (n >= sizeof(name_buf))
                        n = sizeof(name_buf) - 1;
                memcpy(name_buf, tag, n);
                name_buf[n] = '\0';
                /* (a self-closing one, as b/: awk's sub(/.\/$/, first)) */
                n = strlen(name_buf);
                if (n >= 2 && name_buf[n - 1] == '/')
                {
                        name_buf[n - 2] = name_buf[0];
                        name_buf[n - 1] = '\0';
                }
                if (strcmp(name_buf, "b") == 0)
                {
                        attr(tag, "n", &a);
                        attr(tag, "id", &idv);
                        fprintf(out, "### %s\t%s\n", a.s, idv.s);
                        str_clear(&chapter);
                }
                else if (strcmp(name_buf, "c") == 0)
                {
                        attr(tag, "n", &a);
                        str_clear(&chapter);
                        str_adds(&chapter, a.s);
                }
                else if (strcmp(name_buf, "v") == 0)
                {
                        char *p;

                        attr(tag, "n", &a);
                        str_clear(&verse);
                        str_adds(&verse, a.s);
                        if (!all_digits(chapter.s) || !all_digits(verse.s))
                        {
                                sprintf(problem, "its XML: a verse numbered \"%.40s:%.40s\", not chapter:verse",
                                        chapter.s, verse.s);
                                bad = TRUE;
                                break;
                        }
                        for (p = text; *p; p++)
                                if (*p == '\r' || *p == '\n' || *p == '\t')
                                        *p = ' ';
                        unescape(text, &body);
                        fprintf(out, "[%s:%s]%s%s\n", chapter.s, verse.s,
                                body.len ? " " : "", body.s);
                }
                else if (!(strncmp(name_buf, "?xml", 4) == 0 && name_buf[4] == '\0') &&
                         strcmp(name_buf, "bible") != 0 && strcmp(name_buf, "/b") != 0 &&
                         strcmp(name_buf, "/c") != 0 && strcmp(name_buf, "/v") != 0 &&
                         strcmp(name_buf, "/bible") != 0 && strncmp(name_buf, "!--", 3) != 0)
                {
                        /* (substr(tag, 1, 30): bytes, as awk in the C locale) */
                        sprintf(problem, "its XML: a tag it doesn't know: <%.30s>", tag);
                        bad = TRUE;
                        break;
                }
                else if (name_buf[0] != '/' && text[strspn(text, " \t\r\n")] != '\0')
                {
                        sprintf(problem, "its XML: text outside a verse: %.40s", text);
                        bad = TRUE;
                        break;
                }
        }
        fclose(out);
        free(xml);
        free(chapter.s); free(verse.s); free(a.s); free(body.s); free(idv.s);
        return (bad ? -1 : 0);
}

static const Source thiagobodruk = {
        "add-thiagobodruk",
        "https://raw.githubusercontent.com/thiagobodruk/bible/master",
        "thiagobodruk",
        "thiagobodruk/bible", "NIV-TB",
        tb_all_translations, tb_license_of, tb_version_of, tb_fetch_text
};

/* ------------------------------------------------------------------------
** scrollmapper/bible_databases (add-scrollmapper.sh): its plain-text
** Bibles, one file each, already in the layout add_to() reads ("### Book",
** then "[1:1] In the beginning ...")
*/
#define SM_LIST "docs/main_readme/translation_list.md"
#define SM_SUFFIX "-SM"
#define CROSSWIRE "https://crosswire.org/sword/modules/"

/* Every translation, from the list's lines such as
   "- **SpaRV (es)**: SpaRV: La Santa Biblia Reina-Valera (1909)": each
   short name ends in -SM (which source it came from: November 10, 2026,
   the designer; the same name, KJV, is in more than one source). One added
   before names had -SM (WLC, not WLC-SM) is listed too, under its old
   name, so it shows as added and r removes it: a folder of that name,
   without another importer's mark, not one of EBaC's own (a link). */
static int sm_all_translations(void)
{
        char path[1200], name[1024], code[256];
        static char line[LINE], title[LINE / 2 + 16], old_title[LINE / 2 + 32];
        const char *p, *q;
        FILE *f;
        int i, n, listed;

        sprintf(path, "%s/list.md", tmp);
        if (fetch(SM_LIST, path) != 0 || (f = fopen(path, "r")) == NULL)
                return (-1);
        while (read_line(f, line, sizeof(line)))
        {
                /* (awk: /^- \*\*[^ \t]+ \([A-Za-z0-9-]+\)\*\*: /) */
                if (strncmp(line, "- **", 4) != 0)
                        continue;
                for (p = line + 4; *p != '\0' && *p != ' ' && *p != '\t'; p++)
                        ;
                if (p == line + 4 || strncmp(p, " (", 2) != 0)
                        continue;
                for (q = p + 2; isalnum(UCHAR(*q)) || *q == '-'; q++)
                        ;
                if (q == p + 2 || strncmp(q, ")**: ", 5) != 0)
                        continue;
                /* name: from the fifth character to the first space */
                p = strchr(line + 4, ' ');
                sprintf(name, "%.*s", (int) (p - (line + 4)) < 1000 ? (int) (p - (line + 4)) : 1000,
                        line + 4);
                /* code: after the first " (", to the next ")"; in lower case */
                p = strstr(line, " (") + 2;
                q = strchr(p, ')');
                sprintf(code, "%.*s", q != NULL && q - p < 250 ? (int) (q - p) : 250, p);
                for (i = 0; code[i] != '\0'; i++)
                        code[i] = (char) tolower(UCHAR(code[i]));
                /* title: after the first "**: ", the short name again left out */
                p = strstr(line, "**: ") + 4;
                for (q = p; *q != '\0' && *q != ' ' && *q != '\t'; q++)
                        ;
                if (q > p + 1 && q[-1] == ':' && *q == ' ')
                        p = q + 1;
                sprintf(title, "%.*s", LINE / 2, p);
                plain(title);
                {
                        char full[1100];

                        sprintf(full, "%s%s", name, SM_SUFFIX);
                        add_translation(full, code, title, name);
                }
        }
        fclose(f);
        if (nall == 0)
        {
                sprintf(problem, "found no translations in the list at %.900s", source);
                return (-1);
        }
        n = nall;
        for (i = 0; i < n; i++)
        {
                char folder[1300];

                sprintf(path, "%s/%s/booklist", here, all[i].in_source);
                sprintf(folder, "%s/%s", here, all[i].in_source);
                listed = exists(path) && !is_link(folder) && !others_text(folder);
                if (listed)
                {
                        sprintf(old_title, "%s (old name)", all[i].title);
                        add_translation(all[i].in_source, all[i].code, old_title,
                                        all[i].in_source);
                }
        }
        problem[0] = '\0';     /* (others_text() says whose, of one not listed) */
        return (0);
}

/* The license, from the translation's README.md in the source ("**License:**
   GPL"; "not stated" if it says "Null" or nothing): the README is "# NAME:
   TITLE", then "**License:** X" (or "**License**: X", or as LEB has it, the
   license alone on the next line). The details are its page at CrossWire,
   whose modules the texts are made from. */
static void sm_license_of(const char *name)
{
        char path[1300], readme[1200];
        static char line[LINE], next_line[LINE];
        Translation *t = find(name);
        FILE *f;
        int title = FALSE, found = FALSE;

        license_line[0] = '\0';
        next_line[0] = '\0';
        sprintf(path, "sources/%.200s/%.900s/README.md", t ? t->code : "", t ? t->in_source : "");
        sprintf(readme, "%s/readme", tmp);
        if (fetch(path, readme) == 0 && (f = fopen(readme, "r")) != NULL)
        {
                while (read_line(f, line, sizeof(line)))
                {
                        /* (awk: /^\*\*License:?\*\*:?/) */
                        const char *p = line;

                        if (strncmp(p, "**License", 9) == 0)
                        {
                                p += 9;
                                if (*p == ':')
                                        p++;
                                if (strncmp(p, "**", 2) == 0)
                                {
                                        p += 2;
                                        if (*p == ':')
                                                p++;
                                        sprintf(license_line, "%.1000s", p);
                                        plain(license_line);
                                        found = TRUE;
                                        break;
                                }
                        }
                        if (title && next_line[0] == '\0')
                        {
                                strcpy(next_line, line);
                                plain(next_line);       /* (a blank line: still none) */
                        }
                        if (strncmp(line, "# ", 2) == 0)
                                title = TRUE;
                }
                fclose(f);
                if (!found && next_line[0] >= 'A' && next_line[0] <= 'Z')
                        sprintf(license_line, "%.1000s", next_line);
        }
        if (license_line[0] == '\0' || strcmp(license_line, "Null") == 0 ||
            strcmp(license_line, "null") == 0)
                strcpy(license_line, "not stated");
        sprintf(details_line, "%sModInfo.jsp?modName=%.900s", CROSSWIRE, t ? t->in_source : "");
}

static void sm_version_of(const char *name, char *version, int size)
{
        char path[1200];
        Translation *t = find(name);

        sprintf(path, "formats/txt/%.1000s.txt", t ? t->in_source : "");
        version_at(path, version, size);
}

static int sm_fetch_text(const char *name, const char *text_path)
{
        char path[1200];
        Translation *t = find(name);

        sprintf(path, "formats/txt/%.1000s.txt", t ? t->in_source : "");
        return (fetch(path, text_path));
}

static const Source scrollmapper = {
        "add-scrollmapper",
        "https://raw.githubusercontent.com/scrollmapper/bible_databases/master",
        "",
        "scrollmapper/bible_databases", "BSB-SM",
        sm_all_translations, sm_license_of, sm_version_of, sm_fetch_text
};

/* ------------------------------------------------------------------------
** Adding one (importer.sh's add_to)
*/

/* a book's file name, from its heading (book_name(): ASCII punctuation and
   spaces go; 15 characters, or 14 for a name like 1Samuel; a second the
   same gets a digit) */
typedef struct
{
        char **names;
        int n, size;
} Taken;

static int taken_has(Taken *t, const char *w)
{
        int i;

        for (i = 0; i < t->n; i++)
                if (strcmp(t->names[i], w) == 0)
                        return (TRUE);
        return (FALSE);
}

static void book_file_name(const char *heading, char *word, Taken *taken)
{
        const unsigned char *p;
        char *w = word, other[256];
        int limit, n;

        for (p = (const unsigned char *) heading; *p; p++)
        {
                /* (awk: [\001-\057\072-\100\133-\140\173-\177]) */
                if ((*p >= 001 && *p <= 057) || (*p >= 072 && *p <= 0100) ||
                    (*p >= 0133 && *p <= 0140) || (*p >= 0173 && *p <= 0177))
                        continue;
                if (*p == 0302 && p[1] == 0240)
                {
                        p++;
                        continue;
                }
                *w++ = (char) *p;
        }
        *w = '\0';
        if (word[0] == '\0')
                strcpy(word, "Book");
        limit = (isdigit(UCHAR(word[0])) && isupper(UCHAR(word[1])) && islower(UCHAR(word[2]))) ? 14 : 15;
        first_chars(word, limit);
        if (taken_has(taken, word))
                for (n = 2; n < 10; n++)
                {
                        strcpy(other, word);
                        first_chars(other, limit - 1);
                        sprintf(other + strlen(other), "%d", n);
                        if (!taken_has(taken, other))
                        {
                                strcpy(word, other);
                                break;
                        }
                }
        if (taken->n == taken->size)
        {
                taken->size = taken->size * 2 + 16;
                taken->names = (char **) realloc(taken->names, sizeof(char *) * (size_t) taken->size);
        }
        taken->names[taken->n++] = strdup(word);
}

/* whether line is "[C:V]..." (awk: /^\[[0-9]+:[0-9]+\]/); its C and V */
static int verse_line(const char *line, long *c, long *v, const char **rest)
{
        const char *p = line;

        if (*p++ != '[' || !isdigit(UCHAR(*p)))
                return (FALSE);
        *c = atol(p);
        while (isdigit(UCHAR(*p)))
                p++;
        if (*p++ != ':' || !isdigit(UCHAR(*p)))
                return (FALSE);
        *v = atol(p);
        while (isdigit(UCHAR(*p)))
                p++;
        if (*p != ']')
                return (FALSE);
        *rest = p + 1;
        return (TRUE);
}

/* the number of verses the index found: bvarray.dat's last verse */
static long index_verses(const char *folder)
{
        char path[1300];
        unsigned char b[8];
        FILE *f;

        sprintf(path, "%s/bvarray.dat", folder);
        if ((f = fopen(path, "rb")) == NULL)
                return (-1);
        if (fseek(f, -8L, 2) != 0 || fread(b, 1, 8, f) != 8)
        {
                fclose(f);
                return (-1);
        }
        fclose(f);
        return ((((long) b[0] * 256 + b[1]) * 256 + b[2]) * 256 + b[3]);
}

/* A row of hyphens standing alone after a verse's words becomes a note of
   its own, and what follows it another (see add_to in importer.sh) */
static void hyphen_rows(const char *t, Str *out)
{
        const char *rest = t, *p, *start, *end;
        int found;

        str_clear(out);
        for (;;)
        {
                /* match(rest, /(^| )---+( |$)/): the leftmost */
                found = FALSE;
                for (p = rest; *p; p++)
                {
                        const char *q = p;

                        if (q != rest && *q != ' ')
                                continue;
                        start = q;
                        if (*q == ' ')
                                q++;
                        if (strncmp(q, "---", 3) != 0)
                                continue;
                        while (*q == '-')
                                q++;
                        if (*q == ' ' || *q == '\0')
                        {
                                end = (*q == ' ') ? q + 1 : q;
                                found = TRUE;
                                break;
                        }
                }
                if (!found || !(out->len > 0 || start > rest))
                        break;
                str_add(out, rest, (size_t) (start - rest));
                str_adds(out, "\t- ");
                for (p = start; p < end; p++)
                        if (*p != ' ')
                                str_add(out, p, 1);
                rest = end;
                if (*rest != '\0')
                        str_adds(out, "\t- ");
        }
        str_adds(out, rest);
}

#include "acrostic.c"

/* (make_index()'s index_each_step: see let_page_draw() in net.h) */
static void page_draws(void)
{
        let_page_draw(FALSE);
}

static int add_to(Translation *tr, const char *version_now, const char *agreed)
{
        char folder[1200], text_path[1200], path[1400], date[32];
        static char line[LINE];
        FILE *f, *out = NULL, *booklist = NULL, *booknames = NULL, *abbrev = NULL;
        int books = 0, b, kept = 0, i;
        long total = 0, c, v, cur;
        long *verses = NULL;
        int *has_text = NULL;
        char **headings = NULL, **bad = NULL, **files = NULL;
        int cap = 0;
        int ok = 0;
        const char *rest;
        Taken taken = { NULL, 0, 0 };
        Str t = { NULL, 0, 0 }, row = { NULL, 0, 0 }, trans = { NULL, 0, 0 };
        char *chapter_now = NULL;

        sprintf(folder, "%s/%s", here, tr->name);
        if (others_text(folder))
                return (-1);
        if (!enough_room())
                return (-1);
        printf("%s: downloading...\n", tr->name);
        fflush(stdout);
        let_page_draw(TRUE);
        sprintf(text_path, "%s/text", tmp);
        if (src->fetch_text(tr->name, text_path) != 0)
                return (-1);

        /* Read the text once to check it, and to plan the books */
        if ((f = fopen(text_path, "r")) == NULL)
        {
                strcpy(problem, "couldn't read the text");
                return (-1);
        }
        {
                long nr = 0;
                int have_chapter = FALSE;
                long chapter = 0;

                while (read_line(f, line, sizeof(line)))
                {
                        let_page_draw(FALSE);   /* (in a browser: see net.h) */
                        nr++;
                        if (strncmp(line, "### ", 4) == 0)
                        {
                                char *h = line + 4, *tab = strchr(h, '\t');

                                if (tab != NULL)
                                        *tab = '\0';
                                if (books + 1 >= cap)
                                {
                                        cap = cap * 2 + 32;
                                        headings = (char **) realloc(headings, sizeof(char *) * (size_t) cap);
                                        bad = (char **) realloc(bad, sizeof(char *) * (size_t) cap);
                                        verses = (long *) realloc(verses, sizeof(long) * (size_t) cap);
                                        has_text = (int *) realloc(has_text, sizeof(int) * (size_t) cap);
                                        files = (char **) realloc(files, sizeof(char *) * (size_t) cap);
                                }
                                books++;
                                spaces(h);
                                headings[books] = strdup(h);
                                bad[books] = NULL;
                                verses[books] = 0;
                                has_text[books] = FALSE;
                                files[books] = NULL;
                                have_chapter = FALSE;
                                continue;
                        }
                        {
                                static char copy[LINE];

                                strcpy(copy, line);
                                spaces(copy);
                                if (copy[0] == '\0')
                                        continue;
                        }
                        if (books == 0 || !verse_line(line, &c, &v, &rest))
                        {
                                sprintf(problem, "line %ld isn't a book heading or a verse: %.60s", nr, line);
                                fclose(f);
                                ok = -1;
                                goto done;
                        }
                        verses[books]++;
                        {
                                static char copy[LINE];

                                strcpy(copy, rest);
                                plain(copy);
                                if (copy[0] != '\0')
                                        has_text[books] = TRUE;
                        }
                        if ((!have_chapter || c != chapter) && v != 1 && bad[books] == NULL)
                        {
                                char msg[128];

                                sprintf(msg, "%ld starts at verse %ld, not 1", c, v);
                                bad[books] = strdup(msg);
                        }
                        chapter = c;
                        have_chapter = TRUE;
                }
        }
        fclose(f);
        for (b = 1; b <= books; b++)
                if (has_text[b])
                {
                        kept++;
                        total += verses[b];
                }
        if (kept == 0)
        {
                strcpy(problem, "its text in the source is empty (only verse numbers)");
                ok = -1;
                goto done;
        }
        for (b = 1; b <= books; b++)
                if (has_text[b] && bad[b] != NULL)
                {
                        sprintf(problem, "%.200s %.100s", headings[b], bad[b]);
                        ok = -1;
                        goto done;
                }
        for (b = 1; b <= books; b++)
                if (has_text[b])
                {
                        char word[256];

                        book_file_name(headings[b], word, &taken);
                        files[b] = strdup(word);
                }

        printf("%s: writing %d books into the folder %s...\n", tr->name, kept, tr->name);
        fflush(stdout);
        let_page_draw(TRUE);
        remove_all(folder);
        if (mkdir(folder, 0777) != 0)
        {
                sprintf(problem, "couldn't make the folder %.1000s", folder);
                ok = -1;
                goto done;
        }
        sprintf(path, "%s/booklist", folder);
        booklist = fopen(path, "w");
        for (b = 1; b <= books; b++)
                if (files[b] != NULL)
                        fprintf(booklist, "%s\n", files[b]);
        fclose(booklist);

        /* each planned book's verses into its file */
        if ((f = fopen(text_path, "r")) == NULL)
        {
                strcpy(problem, "couldn't write the books");
                ok = -1;
                goto done;
        }
        b = 0;
        while (read_line(f, line, sizeof(line)))
        {
                let_page_draw(FALSE);   /* (in a browser: see net.h) */
                if (strncmp(line, "### ", 4) == 0)
                {
                        char *h = line + 4, *tab = strchr(h, '\t'), code[1024];

                        b++;
                        if (out != NULL)
                                fclose(out);
                        out = NULL;
                        code[0] = '\0';
                        if (tab != NULL)
                        {
                                sprintf(code, "%.1000s", tab + 1);
                                spaces(code);
                                *tab = '\0';
                        }
                        if (b <= books && files[b] != NULL)
                        {
                                sprintf(path, "%s/%s", folder, files[b]);
                                out = fopen(path, "w");
                                if (booknames == NULL)
                                {
                                        sprintf(path, "%s/booknames", folder);
                                        booknames = fopen(path, "w");
                                }
                                plain(h);
                                fprintf(booknames, "%s\n", h);
                                if (code[0] != '\0')
                                {
                                        if (abbrev == NULL)
                                        {
                                                sprintf(path, "%s/abbreviations", folder);
                                                abbrev = fopen(path, "w");
                                        }
                                        fprintf(abbrev, "%s\t%s\n", code, files[b]);
                                }
                        }
                        continue;
                }
                {
                        static char copy[LINE];

                        strcpy(copy, line);
                        spaces(copy);
                        if (out == NULL || copy[0] == '\0')
                                continue;
                }
                if (!verse_line(line, &c, &v, &rest))
                        continue;
                {
                        static char copy[LINE];

                        strcpy(copy, rest);
                        plain(copy);
                        hyphen_rows(copy, &row);
                }
                if (row.len > 0)
                        fprintf(out, "[%ld:%ld] %s\n", c, v, row.s);
                else
                        fprintf(out, "[%ld:%ld]\n", c, v);
        }
        fclose(f);
        if (out != NULL)
                fclose(out);
        if (booknames != NULL)
                fclose(booknames);
        if (abbrev != NULL)
                fclose(abbrev);

        /* the Hebrew alphabet's letters set apart (acrostic.c) */
        letters_apart(folder);

        printf("%s: building the concordance index...\n", tr->name);
        fflush(stdout);
        let_page_draw(TRUE);
        {
                char back[1100];
                int built;

                if (getcwd(back, sizeof(back)) == NULL || chdir(folder) != 0)
                {
                        strcpy(problem, "building the index failed");
                        ok = -1;
                        goto done;
                }
                index_each_step = page_draws;
                built = make_index(TRUE);
                index_each_step = NULL;
                if (chdir(back) != 0 || built != 0)
                {
                        strcpy(problem, "building the index failed");
                        ok = -1;
                        goto done;
                }
        }
        cur = index_verses(folder);
        if (cur != total)
        {
                sprintf(problem, "the index has %ld verses, but the text has %ld", cur, total);
                ok = -1;
                goto done;
        }

        today(date);
        sprintf(path, "%s/license", folder);
        if ((f = fopen(path, "w")) == NULL)
        {
                sprintf(problem, "couldn't write %s/license", tr->name);
                ok = -1;
                goto done;
        }
        fprintf(f, "License: %s\nDetails: %s\nAgreed: %s (%s)\n", license_line, details_line, date, agreed);
        fclose(f);
        if (src->mark[0] != '\0')
        {
                sprintf(path, "%s/%s", folder, src->mark);
                if ((f = fopen(path, "w")) == NULL)
                {
                        sprintf(problem, "couldn't write %s/%s", tr->name, src->mark);
                        ok = -1;
                        goto done;
                }
                fprintf(f, "%s\n", source);
                fclose(f);
        }
        sprintf(path, "%s/version", folder);
        if (version_now[0] != '\0')
        {
                if ((f = fopen(path, "w")) == NULL)
                {
                        sprintf(problem, "couldn't write %s/version", tr->name);
                        ok = -1;
                        goto done;
                }
                fprintf(f, "Version: %s\nDownloaded: %s\n", version_now, date);
                fclose(f);
        }
        else
                unlink(path);

        /* at the top of trans (or, with --at-end, at the end) */
        {
                char first[2048];

                sprintf(first, "%-11s %s\n", tr->name, tr->title);
                if (at_end)
                {
                        if (!is_listed(tr->name))
                        {
                                char tpath[1100];
                                FILE *tf;

                                sprintf(tpath, "%s/trans", here);
                                if ((tf = fopen(tpath, "r")) != NULL)
                                {
                                        while (read_line(tf, line, sizeof(line)))
                                        {
                                                str_adds(&trans, line);
                                                str_adds(&trans, "\n");
                                        }
                                        fclose(tf);
                                }
                                str_adds(&trans, first);
                                replace_trans(trans.s);
                        }
                }
                else
                {
                        str_adds(&trans, first);
                        without_line(tr->name, &trans);
                        replace_trans(trans.s);
                }
        }
        printf("%s: done. %s is ready: start EBaC and press C to choose it.\n", tr->name, tr->title);

        /* words without spaces between them: a note */
        if ((f = fopen(text_path, "r")) != NULL)
        {
                long with_text = 0, spaced = 0;

                while (read_line(f, line, sizeof(line)))
                {
                        let_page_draw(FALSE);   /* (in a browser: see net.h) */
                        char *p;
                        size_t n;

                        if (!verse_line(line, &c, &v, &rest) || *rest != ' ')
                                continue;
                        p = strstr(line, "] ") + 2;
                        n = strlen(p);
                        while (n > 0 && (p[n - 1] == ' ' || p[n - 1] == '\t' || p[n - 1] == '\r'))
                                p[--n] = '\0';
                        if (n == 0)
                                continue;
                        with_text++;
                        if (strchr(p, ' ') != NULL)
                                spaced++;
                }
                fclose(f);
                if (with_text > 0 && spaced * 2 < with_text)
                        printf("%s: note: its text has no spaces between words, so searching it works poorly.\n",
                               tr->name);
        }
done:
        for (b = 1; b <= books; b++)
        {
                free(headings[b]);
                free(bad[b]);
                free(files[b]);
        }
        free(headings); free(bad); free(verses); free(has_text); free(files);
        for (i = 0; i < taken.n; i++)
                free(taken.names[i]);
        free(taken.names);
        free(t.s); free(row.s); free(trans.s);
        free(chapter_now);
        return (ok);
}

/* add one; on failure, no half-made folder left (unless listed before) */
static int add(Translation *tr, const char *version_now, const char *agreed)
{
        char folder[1200];
        int was_listed = is_listed(tr->name);

        sprintf(folder, "%s/%s", here, tr->name);
        if (add_to(tr, version_now, agreed) != 0)
        {
                if (!was_listed && exists(folder))
                        remove_all(folder);
                return (-1);
        }
        return (0);
}

/* remove one added (importer.sh's remove) */
static int remove_one(const char *name)
{
        char folder[1200], word[256], tpath[1100];
        static char line[LINE];
        Str rest = { NULL, 0, 0 };
        FILE *f;
        int any = FALSE;

        sprintf(folder, "%s/%s", here, name);
        if (is_link(folder))
        {
                strcpy(problem, "it is one of EBaC's own");
                return (-1);
        }
        if (others_text(folder))
                return (-1);
        if (!is_added(name))
        {
                strcpy(problem, "it isn't added");
                return (-1);
        }
        without_line(name, &rest);
        sprintf(tpath, "%s/trans", here);
        (void) tpath;
        {
                char *p = rest.s ? rest.s : "", *nl;

                for (; *p; p = nl ? nl + 1 : p + strlen(p))
                {
                        nl = strchr(p, '\n');
                        sprintf(line, "%.*s", (int) (nl ? nl - p : (long) strlen(p)) < LINE - 1 ?
                                (int) (nl ? nl - p : (long) strlen(p)) : LINE - 1, p);
                        first_word(line, word, sizeof(word));
                        if (word[0] != '\0' && word[0] != '#')
                                any = TRUE;
                        if (nl == NULL)
                                break;
                }
        }
        if (!any)
        {
                free(rest.s);
                strcpy(problem, "it is the only translation in trans");
                return (-1);
        }
        if (replace_trans(rest.s ? rest.s : "") != 0)
        {
                free(rest.s);
                strcpy(problem, "couldn't change trans");
                return (-1);
        }
        free(rest.s);
        /* (out of the folder while it is deleted: EBaC may be in it, the
           translation it shows, and some systems, as a browser's, won't
           delete the folder a program is in; then back, if it is still
           there) */
        {
                char was[1200];
                int back = getcwd(was, sizeof(was)) != NULL && chdir(here) == 0;
                int ok = remove_all(folder) == 0;

                if (back && chdir(was) != 0)
                {
                        /* (it was the folder deleted: EBaC changes
                           translation next) */
                }
                if (!ok)
                {
                        sprintf(problem, "couldn't delete the folder %s", name);
                        return (-1);
                }
        }
        (void) f;
        printf("%s: removed.\n", name);
        return (0);
}

/* the columns a character takes (importer.sh's char_cols): 2 for Chinese,
   Japanese and Korean, 0 for a mark on the letter before it, 1 for the
   rest; by its first two bytes, as the awk program has it */
static int char_cols(const unsigned char *c)
{
        unsigned b = c[0], b2 = c[1];
        int cont = (b2 >= 0x80 && b2 <= 0xBF) ? (int) (b2 - 0x7F) : 0;

        if ((b >= 0xE3 && b <= 0xED) || (b == 0xEF && (b2 == 0xBC || b2 == 0xBD)))
                return (2);
        if (b == 0xCC || (b == 0xCD && cont <= 48) ||
            (b == 0xD6 && cont >= 18 && b2 != 0xBE) ||
            (b == 0xD7 && (b2 == 0x81 || b2 == 0x82 || b2 == 0x84 || b2 == 0x85 || b2 == 0x87)))
                return (0);
        return (1);
}

/* as much of s as fits in n columns (cut_cols()), in place */
static void cut_cols(char *s, int n)
{
        int i = 0, len, used = 0, w;

        while (s[i] != '\0')
        {
                for (len = 1; s[i + len] != '\0' && (UCHAR(s[i + len]) & 0xC0) == 0x80; len++)
                        ;
                w = char_cols((unsigned char *) s + i);
                if (used + w > n)
                {
                        s[i] = '\0';
                        return;
                }
                used += w;
                i += len;
        }
}

/* the window's columns (screen_size()): the terminal's, else $COLUMNS,
   else 80; at least 20 */
static int screen_cols(void)
{
        int cols = 0;
#if defined(UNIXV) && defined(TIOCGWINSZ)
        struct winsize size;
        int fd = open("/dev/tty", O_RDONLY);

        if (fd >= 0)
        {
                if (ioctl(fd, TIOCGWINSZ, &size) == 0)
                        cols = size.ws_col;
                close(fd);
        }
#endif
        if (cols <= 0 && getenv("COLUMNS") != NULL && all_digits(getenv("COLUMNS")))
                cols = atoi(getenv("COLUMNS"));
        if (cols <= 0)
                cols = 80;
        if (cols < 20)
                cols = 20;
        return (cols);
}

/* ------------------------------------------------------------------------
** The terminal: in EBaC (A on the main menu), EBaC's own (its keys, its
** screen's size, its gestures); from a command line, stdin and stdout
*/
static int in_ebac = FALSE;     /* run by EBaC, at its terminal */

static int at_terminal(void)
{
        return (in_ebac || isatty(0));
}

/* the screen's size (screen_size()): EBaC's, or the terminal's, else
   $LINES and $COLUMNS, else 24 by 80; at least 8 by 20 */
static int rows = 24, cols = 80;

static void screen_size(void)
{
        if (in_ebac)
        {
                rows = s_lines;
                cols = s_cols;
        }
        else
        {
                rows = 0;
                cols = screen_cols();
#if defined(UNIXV) && defined(TIOCGWINSZ)
                {
                        struct winsize size;
                        int fd = open("/dev/tty", O_RDONLY);

                        if (fd >= 0)
                        {
                                if (ioctl(fd, TIOCGWINSZ, &size) == 0)
                                        rows = size.ws_row;
                                close(fd);
                        }
                }
#endif
                if (rows <= 0 && getenv("LINES") != NULL && all_digits(getenv("LINES")))
                        rows = atoi(getenv("LINES"));
                if (rows <= 0)
                        rows = 24;
        }
        if (rows < 8)
                rows = 8;
        if (cols < 20)
                cols = 20;
}

static void clear_screen(void)
{
        if (in_ebac)
        {
                cls();          /* (and EBaC's size of the screen, now) */
                fflush(stdout);
        }
}

/* the screen the importer shows, for the web page's gestures */
static void typing_mode(const char *mode)
{
        if (in_ebac)
                scr_mode((char *) mode);
}

/* What has been typed so far, kept across a redraw (see read_answer()) */
static char typed[1024];

/*
** read_answer: a line typed, into answer. Returns 0; 1 at the end of
** input; 2, with nothing read yet, if the window changes size meanwhile, so
** that the caller can draw the screen again for the new size (as when a
** phone's keyboard comes up or goes down); with through_resize, it reads
** on instead (the license question). What was typed before a redraw is
** shown again, so what is on the screen is what Enter sends. Backspace
** takes back a character (a whole one, in UTF-8), CTRL-U the line; CTRL-D,
** on an empty line, is the end of input; other control keys are left out.
** (importer.sh's read_line.) Not at a terminal: a whole line.
*/
static int read_answer(char *answer, int size, int through_resize)
{
        int c, n;

        answer[0] = '\0';
        if (!in_ebac)
        {
                if (fgets(answer, size, stdin) == NULL)
                {
                        answer[0] = '\0';
                        return (1);
                }
                n = (int) strlen(answer);
                if (n > 0 && answer[n - 1] == '\n')
                        answer[--n] = '\0';
                return (0);
        }
        printf("%s", typed);    /* (shown again, after a redraw) */
        fflush(stdout);
        for (;;)
        {
                redraw_on_resize = !through_resize;
                c = ugetchar();
                redraw_on_resize = FALSE;
                if (c == KEY_REDRAW && !through_resize)
                        return (2);
                if (c == '\r' || c == '\n')
                {
                        printf("\n");
                        sprintf(answer, "%.*s", size - 1, typed);
                        typed[0] = '\0';
                        return (0);
                }
                n = (int) strlen(typed);
                if (c == 0x7F || c == '\b')
                {
                        if (n > 0)
                        {
                                while (n > 0 && (UCHAR(typed[n - 1]) & 0xC0) == 0x80)
                                        n--;
                                typed[n > 0 ? n - 1 : 0] = '\0';
                                printf("\b \b");
                        }
                }
                else if (c == 0x15)             /* CTRL-U */
                {
                        while (typed[0] != '\0')
                        {
                                n = (int) strlen(typed);
                                while (n > 0 && (UCHAR(typed[n - 1]) & 0xC0) == 0x80)
                                        n--;
                                typed[n > 0 ? n - 1 : 0] = '\0';
                                printf("\b \b");
                        }
                }
                else if (c == 0x04)             /* CTRL-D */
                {
                        if (n == 0)
                                return (1);
                }
                else if (c < 32 || c > 255)
                        ;                       /* (another control key) */
                else if (n < (int) sizeof(typed) - 1)
                {
                        typed[n] = (char) c;
                        typed[n + 1] = '\0';
                        putchar(c);
                }
                fflush(stdout);
        }
}

/* Before going back to EBaC: a key, so what was written can be read */
static void press_any_key(void)
{
        printf("\nPress any key to go back to EBaC. ");
        typing_mode("anykey");
        fflush(stdout);
        if (in_ebac)
        {
                /* (all of it shown first, in a browser; and a key pressed,
                   or a tap, while it worked doesn't pass over it unread) */
                let_page_draw(TRUE);
                discard_typeahead();
                ugetchar();
        }
        else
        {
                char line[256];

                if (fgets(line, sizeof(line), stdin) == NULL)
                        line[0] = '\0';
        }
        printf("\n");
}

/* ------------------------------------------------------------------------
** The names of languages (importer.sh's language_names and languages)
*/
#define NAMES_SOURCE "https://raw.githubusercontent.com/unicode-org/cldr-json/main/cldr-json/cldr-localenames-full/main"
#define CACHE_FORMAT "# languages.cache: code, English name, own name, code named (EBaC 2.4.1)"
static const char *names_source = NAMES_SOURCE;

/*
** The English names of the codes Unicode has none for, as IANA's registry
** of language codes (BCP 47) gives them: the shell importer downloaded the
** registry, which a web page may not (it doesn't allow other sites), so
** the few the sources use are kept here. A code in neither is shown as it
** is, as before.
*/
static const char *iana_names[][2] = {
        { "bea", "Beaver" },            /* (scrollmapper/bible_databases) */
        { "hbo", "Ancient Hebrew" },    /* (scrollmapper/bible_databases) */
        { "sml", "Central Sama" },      /* (scrollmapper/bible_databases) */
        { "tsg", "Tausug" },            /* (scrollmapper/bible_databases) */
        { NULL, NULL }
};

typedef struct
{
        char *code, *english, *own, *whose;
} Names;

static Names *cache = NULL;
static int ncache = 0;

static Names *cached(const char *code)
{
        int i;

        for (i = 0; i < ncache; i++)
                if (strcmp(cache[i].code, code) == 0)
                        return (&cache[i]);
        return (NULL);
}

/* split a line at its tabs, into up to n fields (as awk's $1 ...) */
static int fields(char *line, char **f, int n)
{
        int k = 0;

        f[k++] = line;
        while (k < n && (line = strchr(line, '\t')) != NULL)
        {
                *line++ = '\0';
                f[k++] = line;
        }
        while (k < n)
                f[k++] = "";
        return (k);
}

static void read_cache(const char *path)
{
        char *f[4];
        static char line[LINE];
        FILE *fp;
        int i;

        for (i = 0; i < ncache; i++)
        {
                free(cache[i].code); free(cache[i].english);
                free(cache[i].own); free(cache[i].whose);
        }
        ncache = 0;
        if ((fp = fopen(path, "r")) == NULL)
                return;
        while (read_line(fp, line, sizeof(line)))
        {
                if (line[0] == '#')
                        continue;
                fields(line, f, 4);
                cache = (Names *) realloc(cache, sizeof(Names) * (size_t) (ncache + 1));
                cache[ncache].code = strdup(f[0]);
                cache[ncache].english = strdup(f[1]);
                cache[ncache].own = strdup(f[2]);
                cache[ncache].whose = strdup(f[3]);
                ncache++;
        }
        fclose(fp);
}

/* the "key": "value" lines of a languages.json, as two lists (awk's
   key() and value()) */
typedef struct
{
        char **key, **value;
        int n;
} Pairs;

static void read_pairs(const char *path, Pairs *p)
{
        char *q, *k, *v;
        static char line[LINE];
        FILE *fp;
        size_t n;

        p->key = p->value = NULL;
        p->n = 0;
        if ((fp = fopen(path, "r")) == NULL)
                return;
        while (read_line(fp, line, sizeof(line)))
        {
                /* /^[ \t]*"[^"]*": "/ */
                for (q = line; *q == ' ' || *q == '\t'; q++)
                        ;
                if (*q != '"')
                        continue;
                k = q + 1;
                if ((q = strchr(k, '"')) == NULL || strncmp(q, "\": \"", 4) != 0)
                        continue;
                *q = '\0';
                v = q + 4;
                /* sub(/",?[ \t\r]*$/, "") */
                n = strlen(v);
                while (n > 0 && (v[n - 1] == ' ' || v[n - 1] == '\t' || v[n - 1] == '\r'))
                        n--;
                if (n > 0 && v[n - 1] == ',')
                        n--;
                if (n > 0 && v[n - 1] == '"')
                {
                        v[n - 1] = '\0';
                        /* (and awk's tolower of the key) */
                        for (q = k; *q; q++)
                                *q = (char) tolower(UCHAR(*q));
                        p->key = (char **) realloc(p->key, sizeof(char *) * (size_t) (p->n + 1));
                        p->value = (char **) realloc(p->value, sizeof(char *) * (size_t) (p->n + 1));
                        p->key[p->n] = strdup(k);
                        p->value[p->n] = strdup(v);
                        p->n++;
                }
        }
        fclose(fp);
}

/* (awk's array: the last value for a key) */
static const char *pair(Pairs *p, const char *key)
{
        int i;

        for (i = p->n - 1; i >= 0; i--)
                if (strcmp(p->key[i], key) == 0)
                        return (p->value[i]);
        return (NULL);
}

static void free_pairs(Pairs *p)
{
        int i;

        for (i = 0; i < p->n; i++)
        {
                free(p->key[i]);
                free(p->value[i]);
        }
        free(p->key);
        free(p->value);
        p->key = p->value = NULL;
        p->n = 0;
}

/* (--catalog: plain lines only, so language_names() says nothing) */
static int names_quietly = FALSE;

static void language_names(void)
{
        char path[1200], en_path[1200], own_path[1200];
        static char line[LINE];
        FILE *fp;
        struct stat st;
        int i, j, any = FALSE;
        Pairs english, own;

        sprintf(path, "%s/languages.cache", here);
        /* a month old: the names again */
        if (stat(path, &st) == 0 && (time(NULL) - st.st_mtime) / 86400 > 30)
                unlink(path);
        /* (its first line says what it is) */
        if ((fp = fopen(path, "r")) != NULL)
        {
                if (!read_line(fp, line, sizeof(line)))
                        line[0] = '\0';
                fclose(fp);
                if (strcmp(line, CACHE_FORMAT) != 0)
                        unlink(path);
        }
        if (!exists(path) && (fp = fopen(path, "w")) != NULL)
        {
                fprintf(fp, "%s\n", CACHE_FORMAT);
                fclose(fp);
        }
        read_cache(path);
        for (i = 0; i < nall && !any; i++)
                if (cached(all[i].code) == NULL)
                        any = TRUE;
        if (any)
        {
                if (!names_quietly)
                {
                        printf("Getting the names of the languages (from Unicode)...\n");
                        fflush(stdout);
                }
                sprintf(en_path, "%s/en.json", tmp);
                if (fetch_from(names_source, "en/languages.json", en_path) != 0)
                        return;
                read_pairs(en_path, &english);
                sprintf(own_path, "%s/own.json", tmp);
                for (i = 0; i < nall; i++)
                {
                        const char *code = all[i].code;
                        char full[256], base[256], names_for[256], file[512], *dash, *seg;
                        char lower_full[256], lower_for[256];
                        const char *e = "", *o = "";
                        int shorter = FALSE;

                        /* (each code once, and not one kept already) */
                        for (j = 0; j < i; j++)
                                if (strcmp(all[j].code, code) == 0)
                                        break;
                        if (j < i || cached(code) != NULL)
                                continue;
                        /* Unicode's name for it: zh-hant is zh-Hant */
                        {
                                char work[256];

                                sprintf(work, "%.250s", code);
                                full[0] = '\0';
                                for (seg = strtok(work, "-"), j = 0; seg != NULL; seg = strtok(NULL, "-"), j++)
                                {
                                        size_t n = strlen(seg), k;

                                        if (j > 0)
                                                strcat(full, "-");
                                        if (j > 0 && n == 4)
                                        {
                                                char t[8];

                                                sprintf(t, "%c%s", toupper(UCHAR(seg[0])), seg + 1);
                                                strcat(full, t);
                                        }
                                        else if (j > 0 && n == 2)
                                        {
                                                for (k = 0; k < n; k++)
                                                        seg[k] = (char) toupper(UCHAR(seg[k]));
                                                strcat(full, seg);
                                        }
                                        else
                                                strcat(full, seg);
                                }
                        }
                        sprintf(base, "%.250s", code);
                        if ((dash = strchr(base, '-')) != NULL)
                                *dash = '\0';
                        strcpy(names_for, full);
                        sprintf(file, "%s/languages.json", full);
                        if (fetch_from(names_source, file, own_path) != 0)
                        {
                                fclose(fopen(own_path, "w"));
                                if (strcmp(base, full) != 0)
                                {
                                        sprintf(file, "%s/languages.json", base);
                                        if (fetch_from(names_source, file, own_path) == 0)
                                                strcpy(names_for, base);
                                        else
                                                fclose(fopen(own_path, "w"));
                                }
                        }
                        read_pairs(own_path, &own);
                        for (j = 0; full[j]; j++)
                                lower_full[j] = (char) tolower(UCHAR(full[j]));
                        lower_full[j] = '\0';
                        for (j = 0; names_for[j]; j++)
                                lower_for[j] = (char) tolower(UCHAR(names_for[j]));
                        lower_for[j] = '\0';
                        if (pair(&english, lower_full) != NULL)
                                e = pair(&english, lower_full);
                        else if (pair(&english, base) != NULL)
                        {
                                e = pair(&english, base);
                                shorter = TRUE;
                        }
                        if (strcmp(lower_for, lower_full) == 0 && pair(&own, lower_full) != NULL)
                                o = pair(&own, lower_full);
                        else if (pair(&own, base) != NULL)
                        {
                                o = pair(&own, base);
                                if (strcmp(lower_full, base) != 0)
                                        shorter = TRUE;
                        }
                        if ((fp = fopen(path, "a")) != NULL)
                        {
                                fprintf(fp, "%s\t%s\t%s\t%s\n", code, e, o, shorter ? base : code);
                                fclose(fp);
                        }
                        free_pairs(&own);
                }
                free_pairs(&english);
                read_cache(path);
        }
        /* codes Unicode has no English name for: IANA's (kept here); as
           the shell importer, only when some code was new */
        if (any)
        {
                int changed = FALSE;

                for (i = 0; i < ncache; i++)
                {
                        char base[256], *dash;

                        if (cache[i].english[0] != '\0')
                                continue;
                        sprintf(base, "%.250s", cache[i].code);
                        if ((dash = strchr(base, '-')) != NULL)
                                *dash = '\0';
                        for (j = 0; iana_names[j][0] != NULL; j++)
                                if (strcmp(iana_names[j][0], cache[i].code) == 0)
                                        break;
                        if (iana_names[j][0] == NULL)
                                for (j = 0; iana_names[j][0] != NULL; j++)
                                        if (strcmp(iana_names[j][0], base) == 0)
                                                break;
                        if (iana_names[j][0] != NULL)
                        {
                                free(cache[i].english);
                                free(cache[i].whose);
                                cache[i].english = strdup(iana_names[j][1]);
                                cache[i].whose = strdup(iana_names[j][0]);
                                changed = TRUE;
                        }
                }
                if (changed && (fp = fopen(path, "w")) != NULL)
                {
                        fprintf(fp, "%s\n", CACHE_FORMAT);
                        for (i = 0; i < ncache; i++)
                                fprintf(fp, "%s\t%s\t%s\t%s\n", cache[i].code, cache[i].english,
                                        cache[i].own, cache[i].whose);
                        fclose(fp);
                }
        }
}

/* s with its first letter a capital (title_first()), in place: ASCII,
   Latin with accents, Greek, Cyrillic and Armenian */
static void title_first(char *s)
{
        unsigned c = UCHAR(s[0]), v = UCHAR(s[1]), code;

        if (c >= 'a' && c <= 'z')
        {
                s[0] = (char) toupper(c);
                return;
        }
        if (v < 0x80 || v > 0xBF)
                return;
        if (c == 0xC3 && v >= 160 && v <= 190 && v != 183)
                s[1] = (char) (v - 32);
        else if (c == 0xC4 || c == 0xC5)
        {
                code = (c == 0xC4 ? 256 : 320) + v - 128;
                if (((code >= 256 && code <= 311) || (code >= 330 && code <= 375)) && code % 2 == 1)
                        code--;
                else if (((code >= 313 && code <= 328) || (code >= 377 && code <= 382)) && code % 2 == 0)
                        code--;
                else
                        return;
                if (code < 320)
                {
                        s[0] = (char) 0xC4;
                        s[1] = (char) (code - 256 + 128);
                }
                else
                {
                        s[0] = (char) 0xC5;
                        s[1] = (char) (code - 320 + 128);
                }
        }
        else if (c == 0xCE && v >= 177 && v <= 191)
                s[1] = (char) (v - 32);
        else if (c == 0xCF && v >= 128 && v <= 137 && v != 130)
        {
                s[0] = (char) 0xCE;
                s[1] = (char) (v + 32);
        }
        else if (c == 0xD0 && v >= 176 && v <= 191)
                s[1] = (char) (v - 32);
        else if (c == 0xD1 && v >= 128 && v <= 143)
        {
                s[0] = (char) 0xD0;
                s[1] = (char) (v + 32);
        }
        else if (c == 0xD1 && v >= 144 && v <= 159)
        {
                s[0] = (char) 0xD0;
                s[1] = (char) (v - 16);
        }
        else if (c == 0xD5 || c == 0xD6)
        {
                code = (c == 0xD5 ? 1344 : 1408) + v - 128;
                if (code < 1377 || code > 1414)
                        return;
                code -= 48;
                if (code < 1344)
                {
                        s[0] = (char) 0xD4;
                        s[1] = (char) (code - 1280 + 128);
                }
                else
                {
                        s[0] = (char) 0xD5;
                        s[1] = (char) (code - 1344 + 128);
                }
        }
}

/* whether s is in a script written without spaces between words */
static int no_spaces(const char *s)
{
        const unsigned char *p = (const unsigned char *) s;

        for (; *p; p++)
        {
                if (*p == 0343 && p[1] != 0 &&
                    ((p[1] >= 0201 && p[1] <= 0203) || (p[1] >= 0220 && p[1] <= 0277)))
                        return (TRUE);
                if (*p >= 0344 && *p <= 0351 && p[1] >= 0200 && p[1] <= 0277)
                        return (TRUE);
                if (*p == 0340 && p[1] >= 0270 && p[1] <= 0273)
                        return (TRUE);
                if (*p == 0341 && ((p[1] >= 0200 && p[1] <= 0202) || p[1] == 0236 || p[1] == 0237))
                        return (TRUE);
        }
        return (FALSE);
}

/* The languages, in the order shown (English first, then by their names in
   English): $tmp/langs's lines */
typedef struct
{
        char *code, *shown, *order;
        int count, some, nosp;
} Lang;

static Lang *langs = NULL;
static int nlangs = 0;

static int lang_order(const void *a, const void *b)
{
        const Lang *x = (const Lang *) a, *y = (const Lang *) b;
        int c = strcmp(x->order, y->order);
        char lx[4096], ly[4096];

        if (c != 0)
                return (c);
        /* (sort's last resort: the whole line) */
        sprintf(lx, "%.1000s\t%.1000s\t%.1000s\t%d\t%d\t%d", x->order, x->code, x->shown, x->count, x->some, x->nosp);
        sprintf(ly, "%.1000s\t%.1000s\t%.1000s\t%d\t%d\t%d", y->order, y->code, y->shown, y->count, y->some, y->nosp);
        return (strcmp(lx, ly));
}

static void languages(void)
{
        int i, j;

        language_names();
        nlangs = 0;
        for (i = 0; i < nall; i++)
        {
                for (j = 0; j < nlangs; j++)
                        if (strcmp(langs[j].code, all[i].code) == 0)
                                break;
                if (j == nlangs)
                {
                        langs = (Lang *) realloc(langs, sizeof(Lang) * (size_t) (nlangs + 1));
                        langs[j].code = all[i].code;
                        langs[j].count = langs[j].some = langs[j].nosp = 0;
                        nlangs++;
                }
                langs[j].count++;
                if (is_listed(all[i].name))
                        langs[j].some = 1;
                if (no_spaces(all[i].title))
                        langs[j].nosp = 1;
        }
        for (j = 0; j < nlangs; j++)
        {
                Names *n = cached(langs[j].code);
                char e[1024], o[1024], shown[3000], le[1024], lo[1024];
                int k;

                sprintf(e, "%.1000s", n ? n->english : "");
                sprintf(o, "%.1000s", n ? n->own : "");
                if (o[0] != '\0')
                        title_first(o);
                if (o[0] != '\0' && no_spaces(o))
                        langs[j].nosp = 1;
                for (k = 0; e[k]; k++)
                        le[k] = (char) tolower(UCHAR(e[k]));
                le[k] = '\0';
                for (k = 0; o[k]; k++)
                        lo[k] = (char) tolower(UCHAR(o[k]));
                lo[k] = '\0';
                if (o[0] != '\0' && e[0] != '\0' && strcmp(lo, le) != 0)
                        sprintf(shown, "%s, %s", o, e);
                else if (e[0] != '\0')
                        strcpy(shown, e);
                else if (o[0] != '\0')
                        strcpy(shown, o);
                else
                        strcpy(shown, langs[j].code);
                if (n != NULL && n->whose[0] != '\0' && strcmp(n->whose, langs[j].code) != 0)
                        sprintf(shown + strlen(shown), " [%s]", langs[j].code);
                langs[j].shown = strdup(shown);
                if (strcmp(langs[j].code, "en") == 0)
                        langs[j].order = strdup("0");
                else if (e[0] != '\0')
                {
                        char ord[1100];

                        sprintf(ord, "1%s", le);
                        langs[j].order = strdup(ord);
                }
                else
                {
                        char ord[300];

                        sprintf(ord, "2%.290s", langs[j].code);
                        langs[j].order = strdup(ord);
                }
        }
        qsort(langs, (size_t) nlangs, sizeof(Lang), lang_order);
}

/* --catalog: for programs (the EBaC integration's voice) */
static void catalog(void)
{
        int i;

        names_quietly = TRUE;
        languages();
        names_quietly = FALSE;
        for (i = 0; i < nlangs; i++)
        {
                Names *n = cached(langs[i].code);

                printf("language\t%s\t%s\t%s\t%d\n", langs[i].code, n ? n->english : "",
                       n ? n->own : "", langs[i].count);
        }
        for (i = 0; i < nall; i++)
                printf("translation\t%s\t%s\t%s\t%d\n", all[i].name, all[i].code, all[i].title,
                       is_listed(all[i].name) ? 1 : 0);
}

/* --list: every translation, by language */
static void list(void)
{
        int i, j;

        printf("Translations that can be added to EBaC, by language:\n");
        languages();
        for (i = 0; i < nlangs; i++)
        {
                printf("\n%s:", langs[i].shown);
                for (j = 0; j < nall; j++)
                        if (strcmp(all[j].code, langs[i].code) == 0)
                                printf("%s%s", is_listed(all[j].name) ? " *" : " ", all[j].name);
                printf("\n");
        }
        printf("\n* = added already\n");
}

/* ------------------------------------------------------------------------
** Choosing (importer.sh's choose_language and choose)
*/
static void print_cut(const char *s, int n)
{
        char line[4096];

        sprintf(line, "%.4000s", s);
        cut_cols(line, n);
        printf("%s\n", line);
}

/* the words of a line, split at spaces (as sh's set -- $answer) */
static int words_of(char *line, char **w, int most)
{
        int n = 0;
        char *t;

        for (t = strtok(line, " \t"); t != NULL && n < most; t = strtok(NULL, " \t"))
                w[n++] = t;
        return (n);
}

static int lower_is(const char *s, const char *what)
{
        for (; *s && *what; s++, what++)
                if (tolower(UCHAR(*s)) != *what)
                        return (FALSE);
        return (*s == '\0' && *what == '\0');
}

/* page arithmetic: first on a page boundary for this size */
static int on_page(int first, int per_page, int count)
{
        int pages = (count + per_page - 1) / per_page;

        if (first < 0)
                first = 0;
        first = first / per_page * per_page;
        if (first > (pages - 1) * per_page)
                first = (pages - 1) * per_page;
        return (first);
}

static void prompt(int pages, const char *back)
{
        if (cols - 1 >= 66)
                printf("(%sEnter alone: %s): ", pages > 1 ? "n, p: next, previous page; " : "", back);
        else
                printf("(%sEnter: %s): ", pages > 1 ? "n/p: page; " : "", back);
        fflush(stdout);
}

/* A language: its index in langs, or -1 to stop */
static int choose_language(void)
{
        int first = 0, per_page, pages, count = nlangs, i, any, n, status;
        char note[1200] = "", answer[1024], words[1024], *w[64];

        for (;;)
        {
                clear_screen();         /* (and the screen's size now) */
                screen_size();
                per_page = rows - 6;
                if (per_page < 3)
                        per_page = 3;
                pages = (count + per_page - 1) / per_page;
                first = on_page(first, per_page, count);
                print_cut("Add or remove translations", cols - 1);
                {
                        char heading[200];

                        if (pages > 1)
                                sprintf(heading, "Choose a language (page %d of %d)", first / per_page + 1, pages);
                        else
                                strcpy(heading, "Choose a language");
                        print_cut(heading, cols - 1);
                }
                for (any = FALSE, i = 0; i < count; i++)
                        if (langs[i].some)
                                any = TRUE;
                printf("%s\n", any ? "* = has translations added" : "");
                for (i = first; i < first + per_page && i < count; i++)
                {
                        char line[4096];

                        sprintf(line, "%s%2d) %.1000s (%d)", langs[i].some ? "*" : " ", i + 1,
                                langs[i].shown, langs[i].count);
                        print_cut(line, cols - 1);
                }
                print_cut(note, cols - 1);
                if (cols - 1 >= 44)
                        printf("Type the number of one language, then Enter.\n");
                else
                        printf("One language: its number, Enter.\n");
                prompt(pages, in_ebac ? "back to EBaC" : "stop");
                typing_mode("addlist");
                status = read_answer(answer, sizeof(answer), FALSE);
                if (status == 2)
                        continue;       /* resized: draw the list again */
                note[0] = '\0';
                strcpy(words, answer);
                n = words_of(words, w, 64);
                if (n == 0 || lower_is(w[0], "x") || lower_is(w[0], "q") || status == 1)
                        return (-1);
                if (lower_is(w[0], "n"))
                {
                        if (first + per_page < count)
                                first += per_page;
                        continue;
                }
                if (lower_is(w[0], "p"))
                {
                        first -= per_page;
                        if (first < 0)
                                first = 0;
                        continue;
                }
                if (all_digits(w[0]) && n == 1 && atoi(w[0]) >= 1 && atoi(w[0]) <= count)
                        return (atoi(w[0]) - 1);
                if (n > 1)
                        strcpy(note, "One language at a time: translations come next.");
                else
                        sprintf(note, "Not in the list: %.1000s", answer);
        }
}

/* The translations of a language: which to add (+NAME) or remove (-NAME),
   into chosen; 0 if none (back to the languages) */
static int choose(Lang *lang, char **chosen, int most)
{
        int first = 0, per_page, pages, count = 0, i, any, n, k, status, nchosen;
        int *of = (int *) malloc(sizeof(int) * (size_t) (nall + 1));
        char note[4096] = "", answer[1024], words[1024], *w[64];

        for (i = 0; i < nall; i++)
                if (strcmp(all[i].code, lang->code) == 0)
                        of[count++] = i;
        for (;;)
        {
                clear_screen();         /* (and the screen's size now) */
                screen_size();
                per_page = rows - 6 - lang->nosp;
                if (per_page < 3)
                        per_page = 3;
                pages = (count + per_page - 1) / per_page;
                first = on_page(first, per_page, count);
                print_cut("Add or remove translations", cols - 1);
                {
                        char page[100], line[4096];

                        if (pages > 1)
                                sprintf(page, " (page %d of %d)", first / per_page + 1, pages);
                        else
                                page[0] = '\0';
                        sprintf(line, "%.3000s", lang->shown);
                        cut_cols(line, cols - 1 - (int) strlen(page));
                        printf("%s%s\n", line, page);
                }
                if (lang->nosp)
                        print_cut("Note: often written without spaces between words; searching such a translation works poorly.", cols - 1);
                for (any = FALSE, i = 0; i < count; i++)
                        if (is_listed(all[of[i]].name))
                                any = TRUE;
                printf("%s\n", any ? "* = added already" : "");
                for (i = first; i < first + per_page && i < count; i++)
                {
                        Translation *t = &all[of[i]];
                        char line[4096], title[4096], cut[4096];
                        int room;

                        sprintf(line, "%s%2d) %-12s", is_listed(t->name) ? "*" : " ", i + 1, t->name);
                        if (strlen(t->name) >= 12)
                                strcat(line, " ");      /* (a long name: a space still) */
                        sprintf(title, "%.3000s", t->title);
                        room = cols - 1 - (int) strlen(line);
                        strcpy(cut, title);
                        cut_cols(cut, room);
                        if (strcmp(cut, title) != 0)
                        {
                                if (room > 3)
                                {
                                        cut_cols(title, room - 3);
                                        strcat(title, "...");
                                }
                                else
                                        title[0] = '\0';
                        }
                        strcat(line, title);
                        print_cut(line, cols - 1);
                }
                print_cut(note, cols - 1);
                if (cols - 1 >= 66)
                        printf("Type numbers to add, as 6 14, or r6 to remove 6; then Enter.\n");
                else
                        printf("Add: 6 14, remove: r6; then Enter.\n");
                prompt(pages, "languages");
                typing_mode("addlist");
                status = read_answer(answer, sizeof(answer), FALSE);
                if (status == 2)
                        continue;       /* resized: draw the list again */
                note[0] = '\0';
                for (i = 0; answer[i]; i++)
                        if (answer[i] == ',')
                                answer[i] = ' ';
                strcpy(words, answer);
                n = words_of(words, w, 64);
                if (n == 0 || lower_is(w[0], "x") || lower_is(w[0], "q") || status == 1)
                {
                        free(of);
                        return (0);
                }
                if ((lower_is(w[0], "n") || lower_is(w[0], "p")) && n == 1)
                {
                        if (lower_is(w[0], "n"))
                        {
                                if (first + per_page < count)
                                        first += per_page;
                        }
                        else
                        {
                                first -= per_page;
                                if (first < 0)
                                        first = 0;
                        }
                        continue;
                }
                {
                        char unknown[1200] = "", not_added[1200] = "";

                        nchosen = 0;
                        for (k = 0; k < n; k++)
                        {
                                const char *word = w[k];
                                Translation *t = NULL;

                                if ((word[0] == 'r' || word[0] == 'R') && isdigit(UCHAR(word[1])))
                                {
                                        if (all_digits(word + 1) && atoi(word + 1) >= 1 &&
                                            atoi(word + 1) <= count)
                                                t = &all[of[atoi(word + 1) - 1]];
                                        if (t == NULL)
                                                sprintf(unknown + strlen(unknown), ", %.100s", word);
                                        else if (is_added(t->name))
                                        {
                                                if (nchosen < most)
                                                {
                                                        chosen[nchosen] = (char *) malloc(strlen(t->name) + 2);
                                                        sprintf(chosen[nchosen++], "-%s", t->name);
                                                }
                                        }
                                        else
                                                sprintf(not_added + strlen(not_added), ", %.100s", word);
                                        continue;
                                }
                                /* (lookup: a number, or a name in any case) */
                                if (all_digits(word) && atoi(word) >= 1 && atoi(word) <= count)
                                        t = &all[of[atoi(word) - 1]];
                                else
                                {
                                        int m;

                                        for (m = 0; m < count && t == NULL; m++)
                                                if (find(word) == &all[of[m]])
                                                        t = &all[of[m]];
                                }
                                if (t != NULL)
                                {
                                        if (nchosen < most)
                                        {
                                                chosen[nchosen] = (char *) malloc(strlen(t->name) + 2);
                                                sprintf(chosen[nchosen++], "+%s", t->name);
                                        }
                                }
                                else
                                        sprintf(unknown + strlen(unknown), ", %.100s", word);
                        }
                        if (unknown[0] != '\0' || not_added[0] != '\0')
                        {
                                for (k = 0; k < nchosen; k++)
                                        free(chosen[k]);
                                if (unknown[0] != '\0' && not_added[0] != '\0')
                                        sprintf(note, "Not in the list: %s; not added: %s", unknown + 2, not_added + 2);
                                else if (unknown[0] != '\0')
                                        sprintf(note, "Not in the list: %s", unknown + 2);
                                else
                                        sprintf(note, "Not added: %s", not_added + 2);
                                continue;
                        }
                        free(of);
                        return (nchosen);
                }
        }
}

/* agree: the license, and whether it is agreed to */
static int agree(Translation *tr, const char **how)
{
        char line[4096];

        src->license_of(tr->name);
        sprintf(line, "%.200s: %.3800s", tr->name, tr->title);
        cut_cols(line, screen_cols() - 1);
        printf("%s\n", line);
        printf("License: %s\n", license_line);
        printf("Details: %s\n", details_line);
        if (agree_all)
        {
                if (agreed_how[0] != '\0')
                {
                        printf("Agreed to, %s.\n", agreed_how);
                        *how = agreed_how;
                }
                else
                {
                        printf("Agreed to, with --agree.\n");
                        *how = "with --agree";
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
                char answer[256], *p;
                int status;

                printf("Do you agree to these terms (y/n)? ");
                typing_mode("input");
                /* (the window changing size, as when a phone's keyboard
                   comes up, isn't an answer: read on, without asking
                   again; what was typed is kept) */
                status = read_answer(answer, sizeof(answer), TRUE);
                if (status == 1)
                        strcpy(answer, "n");    /* (end of input) */
                for (p = answer; *p; p++)
                        *p = (char) tolower(UCHAR(*p));
                if (strcmp(answer, "y") == 0 || strcmp(answer, "yes") == 0)
                {
                        *how = "typed y";
                        return (TRUE);
                }
                if (strcmp(answer, "n") == 0 || strcmp(answer, "no") == 0)
                {
                        strcpy(problem, "its license wasn't agreed to");
                        return (FALSE);
                }
                printf("Please type y or n.\n");
        }
}

/* ------------------------------------------------------------------------
** The command line
*/
static void usage(FILE *f)
{
        const char *which = src->program + 4;   /* ("add-" left out) */

        fprintf(f, "usage: bible --import %s [--list] [--agree] [--source PLACE] [NAME ...] [--remove NAME ...]\n", which);
        fprintf(f, "Downloads Bible translations from %s,\n", src->repo);
        fprintf(f, "and adds them to EBaC, or removes them.\n");
        fprintf(f, "  NAME            short names of translations to add, such as %s\n", src->example);
        fprintf(f, "  --remove NAME   remove this translation (added before)\n");
        fprintf(f, "  --agree         agree to the license of each translation added (when\n");
        fprintf(f, "                  run with no one to answer, as from a script)\n");
        fprintf(f, "  --list          only list the translations\n");
        fprintf(f, "  --catalog, --license NAME, --check NAME, --at-end, --agreed-how TEXT:\n");
        fprintf(f, "                  for programs (see the comments at the top of import.c)\n");
        fprintf(f, "  --source PLACE  where the translations are: a web address, or a folder\n");
        fprintf(f, "                  with a copy of %s (default:\n", src->repo);
        fprintf(f, "                  %s)\n", src->source);
        fprintf(f, "  --names-source PLACE  where the names of languages are (Unicode's CLDR)\n");
}

static void cleanup(void)
{
        if (tmp[0] != '\0')
                remove_all(tmp);
        tmp[0] = '\0';
}

/* The translations named, chosen, or both: each added (+NAME) or removed
   (-NAME); start_with, the translation EBaC is to show after */
static void do_actions(char **chosen, int nchosen, char *start_with)
{
        int i, first = TRUE;

        for (i = 0; i < nchosen; i++)
        {
                Translation *t;
                const char *how = "";
                char version[512], kept[512], date[32];

                if (chosen[i] == NULL)
                        continue;
                /* a blank line between translations */
                if (!first)
                        printf("\n");
                first = FALSE;
                t = find(chosen[i] + 1);
                if (chosen[i][0] == '-')
                {
                        if (remove_one(t->name) == 0)
                        {
                                if (start_with != NULL && strcmp(start_with, t->name) == 0)
                                        start_with[0] = '\0';
                        }
                        else
                        {
                                printf("%s: not removed: %s.\n", t->name, problem);
                                failed++;
                        }
                        continue;
                }
                screen_size();
                src->version_of(t->name, version, sizeof(version));
                kept_version(t->name, kept, date);
                if (version[0] != '\0' && is_added(t->name) && strcmp(kept, version) == 0)
                {
                        printf("%s: you already have this version (downloaded %s). To download it again, remove it first (r and its number), then add it.\n",
                               t->name, date);
                        continue;
                }
                if (!agree(t, &how))
                {
                        printf("%s: not added: %s.\n", t->name, problem);
                        /* (no is an answer, not a failure; no one to answer is) */
                        if (!at_terminal())
                                failed++;
                        continue;
                }
                if (add(t, version, how) == 0)
                {
                        if (start_with != NULL)
                                sprintf(start_with, "%.*s", MAX_START - 1, t->name);
                }
                else
                {
                        printf("%s: not added: %s.\n", t->name, problem);
                        failed++;
                }
        }
        fflush(stdout);
}

/* the sources, by the name given on the command line */
static const Source *source_named(const char *name)
{
        if (strcmp(name, "thiagobodruk") == 0)
                return (&thiagobodruk);
        if (strcmp(name, "scrollmapper") == 0)
                return (&scrollmapper);
        return (NULL);
}

static int start_run(const Source *which)
{
        const char *t = getenv("TMPDIR");

        src = which;
        source = src->source;
        names_source = NAMES_SOURCE;
        agree_all = at_end = FALSE;
        agreed_how = "";
        failed = 0;
        typed[0] = '\0';
        nall = 0;
        sprintf(tmp, "%.1000s/ebac-add.XXXXXX", (t != NULL && *t != '\0') ? t : "/tmp");
        if (mkdtemp(tmp) == NULL)
        {
                tmp[0] = '\0';
                return (-1);
        }
        return (0);
}

/* Project Gutenberg's texts, by ebook number (gutenberg.c, sharing the
   parts above) */
#include "gutenberg.c"

/*
** Purpose: A on EBaC's main menu, for one source ("thiagobodruk"): the
**          languages, then a language's translations, each chosen added
**          or removed, at EBaC's own terminal; in EBaC itself, rather than
**          a program run in its place, so that it works where no other
**          program can be run (a web browser). root: EBaC's folder;
**          start_with (MAX_START bytes): the translation shown now, and
**          after, the one EBaC is to show (the last added; "" if the one
**          shown was removed).
** Returns: 0, or -1 if the source isn't one of the importer's.
*/
int import_in_ebac(const char *which, const char *root, char *start_with)
{
        const Source *s = source_named(which);
        char back[1100];
        char *chosen[256];
        int nchosen = 0, i, k;

        if (strcmp(which, "gutenberg") == 0 && getcwd(back, sizeof(back)) != NULL)
        {
                sprintf(here, "%.1000s", root);
                k = gb_in_ebac(start_with);
                if (chdir(back) != 0)
                        return (0);
                return (k);
        }
        if (s == NULL || getcwd(back, sizeof(back)) == NULL)
                return (-1);
        sprintf(here, "%.1000s", root);
        if (start_run(s) != 0)
                return (-1);
        in_ebac = TRUE;
        /* (a copy of the source, or of Unicode's names, instead: for a
           computer without the Internet, and for tests) */
        if (getenv("EBAC_IMPORT_SOURCE") != NULL && *getenv("EBAC_IMPORT_SOURCE") != '\0')
                source = getenv("EBAC_IMPORT_SOURCE");
        if (getenv("EBAC_NAMES_SOURCE") != NULL && *getenv("EBAC_NAMES_SOURCE") != '\0')
                names_source = getenv("EBAC_NAMES_SOURCE");
        clear_screen();
        if (src->all_translations() != 0)
        {
                printf("%s: %s\n", src->program, problem);
                failed++;       /* (said, and kept on the screen to be read) */
        }
        else
        {
                languages();
                for (;;)
                {
                        k = choose_language();
                        if (k < 0)
                                break;
                        nchosen = choose(&langs[k], chosen, 256);
                        if (nchosen > 0)
                                break;
                }
                if (nchosen > 0)
                        clear_screen();
                do_actions(chosen, nchosen, start_with);
                for (i = 0; i < nchosen; i++)
                        free(chosen[i]);
        }
        /* (straight back if nothing was chosen: there is nothing to read) */
        if (nchosen > 0 || failed > 0)
                press_any_key();
        cleanup();
        in_ebac = FALSE;
        if (chdir(back) != 0)
                return (0);
        return (0);
}

int import_main(int argc, char **argv)
{
        const char *mode = "", *mode_name = "";
        char **names;
        int nnames = 0, i, list_only = FALSE, catalog_only = FALSE;
        const char *dir;

        if (argc >= 2 && strcmp(argv[1], "gutenberg") == 0)
        {
                if ((dir = getenv("BIBLEROOT")) == NULL || *dir == '\0' || chdir(dir) != 0 ||
                    getcwd(here, sizeof(here)) == NULL)
                {
                        fprintf(stderr, "add-gutenberg: BIBLEROOT must name the folder EBaC is in.\n");
                        return (1);
                }
                return (gb_main(argc, argv));
        }
        if (argc < 2 || source_named(argv[1]) == NULL)
        {
                fprintf(stderr, "usage: bible --import thiagobodruk|scrollmapper|gutenberg ...\n");
                return (2);
        }
        src = source_named(argv[1]);
        if (start_run(src) != 0)
                return (1);
        atexit(cleanup);
        names = (char **) malloc(sizeof(char *) * (size_t) (argc + 256));
        for (i = 2; i < argc; i++)
        {
                const char *a = argv[i];

                if (strcmp(a, "--agree") == 0)
                        agree_all = TRUE;
                else if (strcmp(a, "--at-end") == 0)
                        at_end = TRUE;
                else if (strcmp(a, "--list") == 0)
                        list_only = TRUE;
                else if (strcmp(a, "--catalog") == 0)
                        catalog_only = TRUE;
                else if ((strcmp(a, "--license") == 0 || strcmp(a, "--check") == 0 ||
                          strcmp(a, "--agreed-how") == 0 || strcmp(a, "--source") == 0 ||
                          strcmp(a, "--names-source") == 0 || strcmp(a, "--remove") == 0))
                {
                        if (i + 1 >= argc)
                        {
                                usage(stderr);
                                return (2);
                        }
                        if (strcmp(a, "--license") == 0 || strcmp(a, "--check") == 0)
                        {
                                mode = a + 2;
                                mode_name = argv[++i];
                        }
                        else if (strcmp(a, "--agreed-how") == 0)
                                agreed_how = argv[++i];
                        else if (strcmp(a, "--source") == 0)
                                source = argv[++i];
                        else if (strcmp(a, "--names-source") == 0)
                                names_source = argv[++i];
                        else
                        {
                                char *r = (char *) malloc(strlen(argv[i + 1]) + 2);

                                sprintf(r, "-%s", argv[++i]);
                                names[nnames++] = r;
                        }
                }
                else if (strncmp(a, "--source=", 9) == 0)
                        source = a + 9;
                else if (strcmp(a, "-h") == 0 || strcmp(a, "--help") == 0)
                {
                        usage(stdout);
                        return (0);
                }
                else if (a[0] == '-')
                {
                        usage(stderr);
                        return (2);
                }
                else
                        names[nnames++] = (char *) a;
        }
        dir = getenv("BIBLEROOT");
        if (dir == NULL || *dir == '\0' || chdir(dir) != 0 || getcwd(here, sizeof(here)) == NULL)
        {
                fprintf(stderr, "%s: BIBLEROOT must name the folder EBaC is in.\n", src->program);
                return (1);
        }

        if (src->all_translations() != 0)
        {
                printf("%s: %s\n", src->program, problem);
                return (1);
        }
        if (catalog_only)
        {
                catalog();
                return (0);
        }
        if (mode[0] != '\0')
        {
                Translation *t = find(mode_name);
                char version[512], kept[512], date[32];

                if (t == NULL)
                {
                        fprintf(stderr, "%s: not in the list of translations.\n", mode_name);
                        return (1);
                }
                if (strcmp(mode, "license") == 0)
                {
                        src->license_of(t->name);
                        printf("Title: %s\nLicense: %s\nDetails: %s\n", t->title, license_line, details_line);
                }
                else if (!is_added(t->name))
                        printf("not added\n");
                else
                {
                        printf("added\n");
                        src->version_of(t->name, version, sizeof(version));
                        kept_version(t->name, kept, date);
                        if (version[0] != '\0' && strcmp(kept, version) == 0)
                                printf("same version\t%s\n", date);
                }
                return (0);
        }
        if (list_only)
        {
                list();
                return (0);
        }

        /* (by name: numbers are only for the list) */
        for (i = 0; i < nnames; i++)
        {
                const char *word = names[i];
                int sign = '+';
                Translation *t;

                if (word[0] == '-')
                {
                        sign = '-';
                        word++;
                }
                t = find(word);
                if (t == NULL)
                {
                        printf("%s: not in the list of translations.\n", word);
                        failed++;
                        names[i] = NULL;
                        continue;
                }
                {
                        char *n = (char *) malloc(strlen(t->name) + 2);

                        sprintf(n, "%c%s", sign, t->name);
                        names[i] = n;
                }
        }
        if (nnames == 0)
        {
                /* a language, then its translations, typed at a terminal
                   (one line at a time: not EBaC's screens) */
                int k;

                languages();
                for (;;)
                {
                        k = choose_language();
                        if (k < 0)
                                break;
                        nnames = choose(&langs[k], names, 256);
                        if (nnames > 0)
                                break;
                }
        }
        do_actions(names, nnames, NULL);
        return (failed == 0 ? 0 : 1);
}
