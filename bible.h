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

/* number of chapters in the bible, less one (the last index into pach[]) */
extern long int numchapters;

/* A guess at the length of the longest verse in bytes plus a little
** more just in case we guessed wrong. (Longer verses are cut short.
** Translations from other sources have verses of over 2000.) */
#ifdef COCO
#	define LONGESTVERSE 700
#else
#	define LONGESTVERSE 4000
#endif

/*
** The lists of a search (see readme.architecture): a verse list has one
** number per entry, a verse; an occurrence list four: the verse where a
** word or phrase starts, the verse of its last word, and that word's full
** and plain positions. Either ends with an entry whose first number is 0.
*/
#define VERSES 1
#define OCCURRENCES 4

#ifdef RAMDISK
	struct TEMPFILE
	{
		FILE *fname;
		char sname[35];
		int width;      /* numbers per entry: VERSES or OCCURRENCES */
	};

	struct TEMPFILE *tmpopen P((void));
#else
/* size of an array to store a list of all the verses: the translation's
   number of verses, plus one for the 0 that ends a list */
#	define ALLVERSE ((size_t) numverses + 1)
#endif

struct bcv_array_type
{
        char book_array[MAXBOOKNAME];
        char chapt_array[MAXCHAPTNAME];
        char verse_array[MAXVERSENAME];
        char data_array[LONGESTVERSE];
};

/* a book: the sequential verse number its first verse follows, and the
   number of chapters before it (from bvarray.dat), and its name */
struct bva
{
        long int verse;
        long int chapt;
        char name[MAXBOOKNAME];
};

/* The number of times a word occurs in the bible. Set during word searching */
extern long int num_found;
/* the word searched for, when the search was that one word alone (so the
** list's header can say how many times it occurs), else ""; process.c */
#define MAXCOUNTWORD 41
/* the folder EBaC's files are in (BIBLEROOT), once known; file.c */
extern char *bible_root;
/* the search history file; process.c */
extern char LOGFILE[];
extern char count_word[MAXCOUNTWORD];
extern long int num_verses;

#define NORMAL_MODE 0
#define DIGITS_MODE 1

/*
** CTRL-L redraws the main menu, and the passage and search-result screens.
** ugetchar() also returns it when the window is resized while one of them is
** waiting for a key: they set redraw_on_resize while they wait.
*/
#define KEY_REDRAW 0x0c
extern int redraw_on_resize;

struct bcv_type
{
        int book_number;
        int chapter_number;
        int verse_number;
};

extern char etla[]; /* name of current translation in use*/

/*
** Function prototypes for the bible program, grouped by the file that
** defines them.
*/

/* bible.c */
long int fgetl P((FILE *fileptr));
#ifdef RAMDISK
void fputl P((long int number, FILE *fileptr));
#endif
char *find_char P((char *s, int c));
int new_verse P((char *word));
int new_chapter P((char *word));
int wordtochapt P((char *word));
int wordtoverse P((char *word));
long int l_bit_reset P((long int l_number));
long int bcv_search P((struct bcv_type *bcvptr));

/* file.c */
void exit_program P((int errnum));
void mfclose P((FILE *fileptr, char *string));
FILE *mfopen P((char *string, char *mode));
void initialize P((void));
extern char *start_trans;
#ifdef UNIXV
void tty_suspend P((void));
void tty_resume P((void));
#endif
void change_translations P((void));
int verse_to_book P((long int sequ_verse_number));
struct bcv_array_type *get_verse P((long int sequ_verse_number));
FILE *root_fopen P((char *name));
#ifdef RAMDISK
struct TEMPFILE *find_word P((char *word, int positions));
#else
long int *find_word P((char *word, int positions));
#endif

/* process.c */
void initlogname P((void));
void load_groups P((void));
char *get_line P((int mode));
#ifdef RAMDISK
struct TEMPFILE *process_words P((void));
#else
long int *process_words P((void));
#endif

/* userint.c */
int match_it P((char *book_str, int update_screen));
void print_book_matches P((char *book_str));
void Erase_word P((int length));

/* curses.c or cure.c */
void initscreen P((void));

