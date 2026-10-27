/*
** refparse.h -- understanding references as they are spoken or typed
**               (EBaC 2.4). See refparse.c.
*/
#define MAXSPOKEN 64            /* a number's spoken form, at most */

int parse_reference P((char *text, int *book, int *chapter, int *verse, int *to, char *why));
int spoken_numbers P((int n, char forms[][MAXSPOKEN], int max));
int number_of P((char *text));
