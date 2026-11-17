/*
   EBaC (Electronic Bible and Concordance)
   written by Neil G. Brookins
*/
/*
** net.c -- downloading, for the importer of translations (import.c): the
** one part of it that differs by platform (see "The importers in C" in
** readme.architecture).
**
**   UNIX       curl, or else wget, as the shell importers used: run
**              directly (fork and exec, no shell), the file written by them
**   a web      the browser's own fetch(), through Emscripten (Asyncify
**   browser    waits for it), the file written into Emscripten's files;
**              only from a site that lets any page read it (CORS):
**              raw.githubusercontent.com does
**   others     none: the vintage systems have no importer
**
** net_get(URL, FILE)    the file at URL into FILE
** net_version(URL)      the file's fingerprint, its ETag, without the file
** Each returns 0 or -1; on -1, net_problem says why, as a sentence's end
** ("couldn't download URL (404)").
** let_page_draw(NOW)    in a browser, a moment for the page to draw what
**                       was written (see net.h)
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "both.h"
#include "net.h"

char net_problem[512];

static void problem(const char *what, const char *url, const char *why)
{
        if (why != NULL && *why != '\0')
                sprintf(net_problem, "%.40s %.400s (%.60s)", what, url, why);
        else
                sprintf(net_problem, "%.40s %.400s", what, url);
}

#ifdef __EMSCRIPTEN__
/* ---------------------------------------------------------------- browser */
#include <emscripten.h>

/*
** fetch() the URL; write the file; return the HTTP status (200), or 0 if
** there was no answer at all (no Internet; a site that doesn't let pages
** read it gives none either, as the browser sees it). method "HEAD" writes
** no file, and gives the ETag in etag (if the site lets it be read).
*/
EM_ASYNC_JS(int, js_fetch, (const char *url, const char *path, int head,
                            char *etag, int etag_size), {
        try {
                const r = await fetch(UTF8ToString(url),
                                      { method: head ? "HEAD" : "GET", cache: "no-store" });
                if (head) {
                        const e = r.headers.get("ETag") || "";
                        stringToUTF8(e, etag, etag_size);
                } else if (r.ok) {
                        FS.writeFile(UTF8ToString(path), new Uint8Array(await r.arrayBuffer()));
                }
                return r.status;
        } catch (e) {
                return 0;
        }
});

void let_page_draw(int now)
{
        static double last = 0;
        double t = emscripten_get_now();

        if (now || t - last >= 100)
        {
                emscripten_sleep(0);
                last = emscripten_get_now();
        }
}

int net_get(const char *url, const char *path)
{
        char status[16];
        int code = js_fetch(url, path, 0, NULL, 0);

        if (code >= 200 && code < 300)
                return (0);
        if (code == 0)
                problem("couldn't download", url,
                        "is this computer connected to the Internet?");
        else
        {
                sprintf(status, "%d", code);
                problem("couldn't download", url, status);
        }
        return (-1);
}

int net_version(const char *url, char *version, int size)
{
        version[0] = '\0';
        if (js_fetch(url, NULL, 1, version, size) < 200)
                version[0] = '\0';
        return (version[0] != '\0' ? 0 : -1);
}

/* (the browser lets a page read Content-Length and Last-Modified of any
   site it may read at all; ETag only if the site says so) */
EM_ASYNC_JS(int, js_head, (const char *url, char *etag, char *length, char *modified,
                           int size), {
        try {
                const r = await fetch(UTF8ToString(url), { method: "HEAD", cache: "no-store" });
                stringToUTF8(r.headers.get("ETag") || "", etag, size);
                stringToUTF8(r.headers.get("Content-Length") || "", length, size);
                stringToUTF8(r.headers.get("Last-Modified") || "", modified, size);
                return r.status;
        } catch (e) {
                return 0;
        }
});

int net_head(const char *url, char *etag, char *length, char *modified, int size)
{
        etag[0] = length[0] = modified[0] = '\0';
        return (js_head(url, etag, length, modified, size) > 0 ? 0 : -1);
}

#else
#ifdef UNIXV
/* ------------------------------------------------------------------- UNIX */
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

/* Whether PROGRAM is on the PATH. */
static int on_path(const char *program)
{
        char *path = getenv("PATH"), dir[1024], *p;
        size_t n;

        if (path == NULL)
                path = "/usr/bin:/bin";
        while (*path != '\0')
        {
                p = strchr(path, ':');
                n = (p != NULL) ? (size_t) (p - path) : strlen(path);
                if (n > 0 && n + strlen(program) + 2 < sizeof(dir))
                {
                        memcpy(dir, path, n);
                        dir[n] = '/';
                        strcpy(dir + n + 1, program);
                        if (access(dir, X_OK) == 0)
                                return (TRUE);
                }
                path += n;
                if (*path == ':')
                        path++;
        }
        return (FALSE);
}

/*
** Run ARGV (no shell), its standard output into out (up to size - 1
** bytes; NULL to throw it away), its errors thrown away; or, with errors
** TRUE, its errors into out and its output thrown away. Returns its exit
** status, or -1 if it couldn't be run.
*/
static int run(char **argv, char *out, int size, int errors)
{
        int fds[2], status, n = 0, got;
        pid_t pid;

        if (pipe(fds) != 0)
                return (-1);
        pid = fork();
        if (pid < 0)
        {
                close(fds[0]);
                close(fds[1]);
                return (-1);
        }
        if (pid == 0)
        {
                FILE *null = fopen("/dev/null", "w");

                dup2(fds[1], errors ? 2 : 1);
                if (null != NULL)
                        dup2(fileno(null), errors ? 1 : 2);
                close(fds[0]);
                close(fds[1]);
                execvp(argv[0], argv);
                _exit(127);
        }
        close(fds[1]);
        for (;;)
        {
                char junk[256];

                if (out != NULL && n < size - 1)
                        got = (int) read(fds[0], out + n, (size_t) (size - 1 - n));
                else
                        got = (int) read(fds[0], junk, sizeof(junk));
                if (got < 0 && errno == EINTR)
                        continue;
                if (got <= 0)
                        break;
                if (out != NULL && n < size - 1)
                        n += got;
        }
        if (out != NULL)
                out[n] = '\0';
        close(fds[0]);
        while (waitpid(pid, &status, 0) < 0)
                if (errno != EINTR)
                        return (-1);
        return (WIFEXITED(status) ? WEXITSTATUS(status) : -1);
}

int net_get(const char *url, const char *path)
{
        char code[16], why[64];
        char *argv[12];
        int status;

        if (on_path("curl"))
        {
                argv[0] = "curl"; argv[1] = "-sSL"; argv[2] = "--max-time";
                argv[3] = "600"; argv[4] = "-o"; argv[5] = (char *) path;
                argv[6] = "-w"; argv[7] = "%{http_code}"; argv[8] = (char *) url;
                argv[9] = NULL;
                status = run(argv, code, sizeof(code), FALSE);
                if (status == 0 && atoi(code) >= 400)
                {
                        problem("couldn't download", url, code);
                        return (-1);
                }
                switch (status)
                {
                case 0:
                        return (0);
                case 6: case 7: case 28: case 35:
                        problem("couldn't download", url,
                                "is this computer connected to the Internet?");
                        return (-1);
                default:
                        sprintf(why, "curl error %d", status);
                        problem("couldn't download", url, why);
                        return (-1);
                }
        }
        if (on_path("wget"))
        {
                argv[0] = "wget"; argv[1] = "-q"; argv[2] = "-O";
                argv[3] = (char *) path; argv[4] = (char *) url; argv[5] = NULL;
                if (run(argv, NULL, 0, FALSE) != 0)
                {
                        problem("couldn't download", url, NULL);
                        return (-1);
                }
                return (0);
        }
        problem("couldn't download", url,
                "needs curl or wget, and neither is installed");
        return (-1);
}

/*
** The ETag the web server gives (the last, after any redirects), with a
** HEAD request: curl -sSIL, or wget's spider.
*/
int net_version(const char *url, char *version, int size)
{
        static char headers[16384];
        char *argv[8], *line, *next;
        int n;

        version[0] = '\0';
        if (on_path("curl"))
        {
                argv[0] = "curl"; argv[1] = "-sSIL"; argv[2] = "--max-time";
                argv[3] = "60"; argv[4] = (char *) url; argv[5] = NULL;
                run(argv, headers, sizeof(headers), FALSE);
        }
        else if (on_path("wget"))
        {
                /* (wget writes the headers to its errors) */
                argv[0] = "wget"; argv[1] = "-q"; argv[2] = "--spider";
                argv[3] = "--server-response"; argv[4] = (char *) url;
                argv[5] = NULL;
                run(argv, headers, sizeof(headers), TRUE);
        }
        else
                return (-1);
        for (line = headers; line != NULL && *line != '\0'; line = next)
        {
                next = strchr(line, '\n');
                if (next != NULL)
                        *next++ = '\0';
                while (*line == ' ')
                        line++;
                if ((line[0] == 'e' || line[0] == 'E') &&
                    (line[1] == 't' || line[1] == 'T') &&
                    (line[2] == 'a' || line[2] == 'A') &&
                    (line[3] == 'g' || line[3] == 'G') && line[4] == ':')
                {
                        line += 5;
                        while (*line == ' ')
                                line++;
                        n = (int) strcspn(line, "\r");
                        if (n >= size)
                                n = size - 1;
                        memcpy(version, line, (size_t) n);
                        version[n] = '\0';
                }
        }
        return (version[0] != '\0' ? 0 : -1);
}

/*
** What the server says of the file (as add-gutenberg.sh's version_of did): its
** headers, as curl -sSIL (or wget's spider) gives them, without carriage
** returns, spaces at the start of each line left out; each answer
** ("HTTP/...") starting afresh. The ETag and Content-Length are the
** second word of their lines; Last-Modified, all after its colon.
*/
int net_head(const char *url, char *etag, char *length, char *modified, int size)
{
        static char headers[16384];
        char *argv[8], *line, *next, *p, word[64];
        int i, status;

        etag[0] = length[0] = modified[0] = '\0';
        if (on_path("curl"))
        {
                argv[0] = "curl"; argv[1] = "-sSIL"; argv[2] = "--max-time";
                argv[3] = "60"; argv[4] = (char *) url; argv[5] = NULL;
                status = run(argv, headers, sizeof(headers), FALSE);
        }
        else if (on_path("wget"))
        {
                argv[0] = "wget"; argv[1] = "--spider"; argv[2] = "--server-response";
                argv[3] = (char *) url; argv[4] = NULL;
                status = run(argv, headers, sizeof(headers), TRUE);
        }
        else
                return (-1);
        for (line = headers; line != NULL && *line != '\0'; line = next)
        {
                next = strchr(line, '\n');
                if (next != NULL)
                        *next++ = '\0';
                for (p = line; *p != '\0'; p++)        /* (tr -d '\r') */
                        if (*p == '\r')
                        {
                                memmove(p, p + 1, strlen(p));
                                p--;
                        }
                while (*line == ' ' || *line == '\t')
                        line++;
                if (strncmp(line, "HTTP/", 5) == 0)
                {
                        etag[0] = length[0] = modified[0] = '\0';
                        continue;
                }
                /* (its first word, in lower case; and the second) */
                for (i = 0; line[i] != '\0' && line[i] != ' ' && line[i] != '\t' && i < 63; i++)
                        word[i] = (char) tolower(UCHAR(line[i]));
                word[i] = '\0';
                p = line + strcspn(line, " \t");
                while (*p == ' ' || *p == '\t')
                        p++;
                if (strcmp(word, "etag:") == 0)
                        sprintf(etag, "%.*s", size - 1 < (int) strcspn(p, " \t") ? size - 1 : (int) strcspn(p, " \t"), p);
                else if (strcmp(word, "content-length:") == 0)
                        sprintf(length, "%.*s", size - 1 < (int) strcspn(p, " \t") ? size - 1 : (int) strcspn(p, " \t"), p);
                else if (strcmp(word, "last-modified:") == 0)
                {
                        p = strchr(line, ':') + 1;
                        while (*p == ' ' || *p == '\t')
                                p++;
                        sprintf(modified, "%.*s", size - 1, p);
                }
        }
        return (status == 0 ? 0 : -1);
}

#else
/* ------------------------------------------------------------ the others */
int net_get(const char *url, const char *path)
{
        (void) path;
        problem("can't download", url, "not on this computer");
        return (-1);
}

int net_version(const char *url, char *version, int size)
{
        (void) url;
        (void) size;
        version[0] = '\0';
        return (-1);
}

int net_head(const char *url, char *etag, char *length, char *modified, int size)
{
        (void) url;
        (void) size;
        etag[0] = length[0] = modified[0] = '\0';
        return (-1);
}
#endif /* UNIXV */

/* (a terminal shows what is written at once) */
void let_page_draw(int now)
{
        (void) now;
}
#endif /* __EMSCRIPTEN__ */
