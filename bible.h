/* 
   EBaC (Electronic Bible and Concordance)
   written by Neil G. Brookins
   http://ebac.sourceforge.net/
*/
#include "both.h"

/* The number of verses to show before the selected verse when viewing
** a verse in context */
#define NUM_SHOW_BEFORE 3

/* the number of characters in: a1thessalonians.\n\0 */
#define MAXBOOKNAME 18

/* number of bytes in: "123\0" */
#define MAXCHAPTNAME 4

/* number of bytes in: "123\0" */
#define MAXVERSENAME 4

/* number of books in the bible */
extern long int numbooks;

/* number of verses in the bible */
extern long int numverses;

/* A guess at the length of the longest verse in bytes plus a little
** more just in case we guessed wrong */
#define LONGESTVERSE 700

#ifdef RAMDISK
	struct TEMPFILE
	{
		FILE *fname;
		char sname[35];
	};

	struct TEMPFILE *tmpopen();
#else
/* size of array to store list of all the verses */
#	define ALLVERSE 31103
#endif

struct bcv_array_type
{
        char book_array[MAXBOOKNAME];
        char chapt_array[MAXCHAPTNAME];
        char verse_array[MAXVERSENAME];
        char data_array[LONGESTVERSE];
};

struct bva
{
        short int verse;
        short int chapt;
        char name[MAXBOOKNAME];
};

/* The number of times a word occurs in the bible. Set during word searching */
extern int num_found;
extern int num_verses;

#define NORMAL_MODE 0
#define DIGITS_MODE 1

struct bcv_type
{
        int book_number;
        int chapter_number;
        int verse_number;
};

extern char etla[]; /* name of current translation in use*/

FILE *mfopen();

