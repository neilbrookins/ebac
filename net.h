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

#endif
