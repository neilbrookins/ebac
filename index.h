/* index.h -- building a translation's concordance index (index.c) */
#ifndef INDEX_H
#define INDEX_H

/* Build the index of the translation in the current folder (its booklist
   and books), as the files *.dat: 0, or -1 if it failed (and said why).
   quiet: without "Reading from book: ..." for each book. */
extern int make_index P((int quiet));

/* If set, called as each book is read and as the index is written, so
   that a program building it can show what it has written meanwhile (in a
   web browser: import.c) */
extern void (*index_each_step) P((void));

#endif
