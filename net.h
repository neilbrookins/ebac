/* net.h -- downloading, for the importer of translations (net.c) */
#ifndef NET_H
#define NET_H

/* why the last call failed, as the end of a sentence */
extern char net_problem[];

/* the file at url into path: 0, or -1 (net_problem says why) */
extern int net_get P((const char *url, const char *path));

/* the file's fingerprint (its ETag), without it, into version (size
   bytes): 0, or -1 (version "") if it can't be had */
extern int net_version P((const char *url, char *version, int size));

/* What the web server says of the file, without it (of the last answer,
   after any redirects): its ETag, Content-Length and Last-Modified, each
   "" if not said (size bytes each). 0, or -1 if there was no answer. */
extern int net_head P((const char *url, char *etag, char *length, char *modified, int size));

/* In a web browser, the page draws what EBaC wrote only when EBaC waits:
   while it works (writing a translation's books, building its index),
   nothing new shows, and the page doesn't answer. let_page_draw(TRUE)
   waits a moment for it, so what was just written shows now;
   let_page_draw(FALSE), called often in a long step, does so at most
   ten times a second. Everywhere, a long step (one that has gone on
   for more than a moment since let_page_draw(TRUE)) turns a spinner, \ |
   / -, where the cursor is, on a terminal (net.c). */
extern void let_page_draw P((int now));

/* several downloads as one step, for the spinner: busy_many(TRUE) before
   the first, busy_many(FALSE) after the last; the spinner turns from the
   first's start, and isn't erased between them (net.c) */
extern void busy_many P((int on));

/* printf() and fprintf(), the spinner (above) erased first, so nothing is
   written after it: import.c's printf and fprintf are these */
extern int busy_printf P((const char *format, ...));
extern int busy_fprintf P((FILE *f, const char *format, ...));

#endif
