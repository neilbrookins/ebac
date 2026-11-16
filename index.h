/* index.h -- building a translation's concordance index (index.c) */
#ifndef INDEX_H
#define INDEX_H

/* Build the index of the translation in the current folder (its booklist
   and books), as the files *.dat: 0, or -1 if it failed (and said why).
   quiet: without "Reading from book: ..." for each book. */
extern int make_index P((int quiet));

#endif
