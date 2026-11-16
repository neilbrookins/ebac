/* import.h -- the importer of translations, in C (import.c) */
#ifndef IMPORT_H
#define IMPORT_H

/* "bible --import SOURCE ...": argv[0] is "--import" */
extern int import_main P((int argc, char **argv));

/* A on EBaC's main menu, for a source ("thiagobodruk"), in EBaC itself:
   root, EBaC's folder; start_with (MAX_START bytes), the translation shown
   now, and after, the one to show. 0, or -1 if not one of the importer's */
#define MAX_START 64
extern int import_in_ebac P((const char *which, const char *root, char *start_with));

#endif
