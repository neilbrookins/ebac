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

	struct TEMPFILE *tmpopen(void);
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

/*
** CTRL-L redraws the passage and search-result screens. ugetchar() also
** returns it when the window is resized while one of those screens is
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
long int fgetl(FILE *fileptr);
short int fgeti(FILE *fileptr);
#ifdef RAMDISK
void fputi(short int offset, FILE *fileptr);
#endif
int new_verse(char *word);
int new_chapter(char *word);
int wordtochapt(char *word);
int wordtoverse(char *word);
short int i_bit_reset(short int number);
long int l_bit_reset(long int l_number);
short int bcv_search(struct bcv_type *bcvptr);

/* file.c */
void exit_program(int errnum);
void mfclose(FILE *fileptr, char *string);
FILE *mfopen(char *string, char *mode);
void initialize(void);
void change_translations(void);
int verse_to_book(short int sequ_verse_number);
struct bcv_array_type *get_verse(short int sequ_verse_number);
#ifdef RAMDISK
struct TEMPFILE *find_word(char *word);
#else
short int *find_word(char *word);
#endif

/* process.c */
void initlogname(void);
char *get_line(int mode);
#ifdef RAMDISK
struct TEMPFILE *process_words(void);
#else
short int *process_words(void);
#endif

/* userint.c */
int match_it(char *book_str, int update_screen);
void Erase_word(int length);

/* curses.c or cure.c */
void initscreen(void);

