/* 
   EBaC (Electronic Bible and Concordance)
   written by Neil G. Brookins
   http://ebac.sourceforge.net/
*/
/* Purpose:
Creates an index of every word in the Bible.
Requires list of books of the Bible in file "booklist" in the current
working directory. Program reads files in this list and creates a binary
tree in memory of each word. An inorder traversal is used to generate
the indices which are text files.
(The index files, their format and why: see readme.architecture.)
*/

#include "both.h"
#include "utf8.h"
#ifdef OSK
#       define malloc _lmalloc
#endif

FILE *book_list, *book_file, *cvarray_file, *bvarray_file, *chapter_file,
     *words_file, *references_file, *position_file, *plainpos_file,
     *format_file;
char book_name[80];
#define MAXWORD 80
char word[MAXWORD];                     /* current word being processed */
char raw_word[MAXWORD];                 /* the same, as it was in the book */
int raw_length;                         /* its length as it was in the book,
                                           before punctuation was removed */
int ended_line = FALSE;                 /* the last word ended its line */
int line_start;                         /* whether it starts a line: a verse
                                           number only counts there, so that
                                           a reference in the text ("Psa.
                                           2:7") isn't taken for one */
long int sequence = 0;                  /* sequential verse number */
long int chapter = 0;
/*
** Positions in the verse (see readme.architecture): the full position
** counts every word, the plain one only words not in [brackets] or
** {braces} (0 for a word in them). The last word of a verse has its full
** position flagged LAST, and the last word not in brackets its plain one.
*/
long int full_position = 0;            /* the word's full position */
long int plain_count = 0;              /* words not in brackets so far */
long int plain_position = 0;           /* the word's plain position */
int in_brackets = FALSE;                /* within [ ... ] or { ... } */
struct list *last_word = NULL;          /* the verse's last word so far */
struct list *last_plain = NULL;         /* and last not in brackets */
struct tree
{
        char *item;                     /* one word */
        struct tree *left;              /* pointer to child */
        struct tree *right;
        struct list *verse_head;        /* the word's occurrences: their */
        struct list *verse_tail;        /* verses, in order */
        long int count;                 /* number of times word occurs */
};
struct list
{
        long int number;                /* verse number */
        long int full;                  /* full position in the verse */
        long int plain;                 /* plain position (0: bracketed) */
        struct list *next;              /* points to next element of list */
};

struct tree *node = NULL;

long int offset; /* number of bytes offset from the beginning
                   of a book that a particular verse begins at. */
int disable_newverse = 0;/*0,1,or2 mean: enabled, about to enable, disabled */

/* functions in this file */
static void fputl(long int offset, FILE *fileptr);
static void mfclose(FILE *fileptr, char *string);
static FILE *mfopen(char *string, char *mode);
static void read_word(FILE *book_file);
static void remove_head_punctuation(void);
static void remove_tail_punctuation(void);
static void make_into_lowercase(void);
static struct tree *store_word(struct tree *node);
static void note_last(struct list *occurrence);
static int new_verse(void);
static int new_chapter(void);
static void inorder_traversal(struct tree *node);
static long int l_bit_twiddle(long int number);
static void visit_node(struct tree *node);
static void print_list(struct list *listptr);
static void end_verse(void);
static void note_brackets(void);

/*
Purpose: similar to the fputc function, but it works with long int
         instead of char. This should take the most significant byte
         of the long int first and write it out to the file.
*/
static void fputl(long int offset, FILE *fileptr)
{
        fputc((char) ((long) offset >> 24), fileptr);
        fputc((char) ((long) offset >> 16), fileptr);
        fputc((char) ((long) offset >> 8), fileptr);
        fputc((char) offset, fileptr);
}

/*
Purpose: the function fclose is called many times in this application
         and we always want to check for error status. This saves space
         by not having to repeat the code many times.
*/
static void mfclose(FILE *fileptr, char *string)
{
        if(fclose(fileptr) == EOF)
        {
                printf("Can't close file: %s\n", string);
                exit(1);
        }
}

/*
Purpose: the function fopen is called many times in this application
         and we always want to check for error status. This saves space
         by not having to repeat the code many times.
*/
static FILE *mfopen(char *string, char *mode)
{
        FILE *fileptr;

        fileptr = fopen(string, mode);
        if (fileptr == (FILE *) NULL)
        {
                printf("Can't Open \"%s\" for ", string);
                if(mode[0] == 'w')
                 printf("writing.\n");
                if(mode[0] == 'r')
                 printf("reading. Are you in the correct working directory?\n");
                exit(1);
        }
        return(fileptr);
}

static void read_word(FILE *book_file)
{
int i = 0; /* index */
int c;

/* Skip over leading white space, noting whether it ended a line (the first
   word of the book starts one too) */
line_start = (ftell(book_file) == 0 || ended_line);
do{
    word[i] = (char) fgetc(book_file);
    if (word[i] == '\n')
        line_start = TRUE;
    else if (word[i] == ' ')
        line_start = FALSE;
}while (word[i] == ' ' || word[i] == '\n');
/* A new line (a new verse, in the importer's text) ends any {note} or
   [brackets] left open: one of them unclosed, a slip in a text (as in a
   verse of one Japanese translation), would otherwise hide the verse
   numbers, or bracket the words, of the rest of the book. */
if (line_start)
{
    disable_newverse = 0;
    in_brackets = FALSE;
}

/* Read until a delimeter is found, or EOF. A word too long for word[] is
   cut short, but all of it is read, and counted in raw_length. */
raw_length = 1;
for(;;)
{
    c = fgetc(book_file);
    if (c == ' ' || c == '\n' || feof(book_file))
    {
        ended_line = (c == '\n');      /* so the next word starts a line */
        break;
    }
    raw_length++;
    if (i < MAXWORD - 2)
        word[++i] = (char) c;
}
word[++i] = '\0';  /* Need to terminate the string */
if (word[0] == 0x1a) /* We have reached EOF */
{
        fgetc(book_file); /* This makes feof(book_file) be TRUE */
        return;
}
i = -1;
while (word[++i])
{
        if (word[i] == '{')
                disable_newverse = 2;
        if (word[i] == '}')
                disable_newverse = 1;
}
strcpy(raw_word, word);
remove_head_punctuation();
remove_tail_punctuation();
make_into_lowercase();
}

/*
** Punctuation is taken off the start and end of a word, a character at a
** time: a character, in UTF-8, may be more than one byte, and a letter
** such as the é of "creó" is not punctuation (see utf8.c).
*/
static void remove_head_punctuation(void)
{
  int i, n;

  while (word[0] != '\0' && !u8_is_letter(word))
  {
          n = u8_len(word);
          for (i = 0; word[i + n - 1] != '\0'; i++)
                  word[i] = word[i + n];
  }
}

static void remove_tail_punctuation(void)
{
  int last;

  while (word[0] != '\0' &&
         !u8_is_letter(&word[last = u8_prev(word, strlen(word))]))
          word[last] = '\0';
}

static void make_into_lowercase(void)
{
        u8_lower(word);
}

/*
** Purpose: stores a word into a binary tree in RAM.
** Characteristics: words are in all lowercase. This affects the ordering
** of the elements in the tree. Duplicate words are stored once, but
** a new reference is appended to the linked list.
*/
static struct tree *store_word(struct tree *node)
{
int n;
struct tree *newnode;
char *newitem;
struct list *newnumber;

        if (node == NULL)
        {
/*
** We need a new node on the binary tree for this word which has not
** been seen before.
*/
                newnode = (struct tree *) malloc (sizeof (struct tree));
                if (newnode == NULL)
                {
                        printf ("malloc failed! size: %lu\n",
				(unsigned long) sizeof (struct tree));
                        exit(1);
                }
                node = newnode;

                newitem = (char *) malloc (strlen(word) + 1);
                if (newitem == NULL)
                {
                        printf ("malloc failed! (a word)\n");
                        exit(1);
                }
                strcpy(newitem, word);
                node->item = newitem;
                node->left = NULL;
                node->right = NULL;

                newnumber = (struct list *) malloc (sizeof(struct list));
                if (newnumber == NULL)
                {
                        printf ("malloc failed! (a verse)\n");
                        exit(1);
                }
                newnumber->number = sequence;
                newnumber->full = full_position;
                newnumber->plain = plain_position;
                newnumber->next = NULL;
                note_last(newnumber);
                node->verse_head = node->verse_tail = newnumber;
                node->count = 1;
                return(node);
        }
        n = strcmp(word, node->item);
        if (n == 0) /* Word is already in tree. Add new verse to list. */
        {
            /* (every word is indexed, however often it occurs: before
               EBaC 2.1, one occurring 65,000 times or more wasn't) */
            node->count++;
            /* every occurrence is stored, so a verse number repeats when
               the word is in that verse more than once (before EBaC 2.1,
               each verse once): so that each occurrence can have its
               position in the verse, in position.dat and plainpos.dat */
            newnumber = (struct list *) malloc (sizeof(struct list));
            if (newnumber == NULL)
            {
                    printf ("malloc failed! (a verse)\n");
                    exit(1);
            }
            node->verse_tail->next = newnumber;
            node->verse_tail = newnumber;
            node->verse_tail->number = sequence;
            node->verse_tail->full = full_position;
            node->verse_tail->plain = plain_position;
            node->verse_tail->next = NULL;
            note_last(newnumber);
            return(node);
        }
        if (n < 0)
        {
                node->left = store_word(node->left); /* resursive */
                return(node);
        }
        if (n > 0)
        {
                node->right = store_word(node->right); /* resursive */
                return(node);
        }
        printf("This sentence is NEVER printed to the screen.\n");
        return(node);
}

static int new_verse(void)
{
        int i;
        char c;

        if (disable_newverse == 2)
                return(FALSE);
        if (disable_newverse == 1)
        {
                disable_newverse = 0;
                return(FALSE);
        }
        for(i = 0; i < (int) strlen(word); i++)
        {
                c = word[i];
                if (!(isdigit((unsigned char) c) || (c == ':')))
                        return(FALSE);
        } /* word contains digit(s), colon(s), or both. */
        for(i = 1; i < (int) strlen(word) - 1; i++)
        {
           if (word[i] == ':' && isdigit((unsigned char) word[i - 1]) && isdigit((unsigned char) word[i + 1]))
                return(TRUE);  /* word contains both digits and ':' */
        }
        return(FALSE);  /* word consists of only digits, eg: 1234 */
                        /* or, contains only colon(s),   eg: :    */
}

/* Purpose: returns TRUE iff word contains ":1" which indicates a new
            chapter has been reached.
   Bugs: Assumes that new_verse() is TRUE, which it always will be prior
         to calling new_chapter().
*/
static int new_chapter(void)
{
        int i;

        for(i=1;i < ((int) strlen(word) - 1); i++)
        {
                if (word[i] == ':')
                {
                        if (word[i + 1] == '1' && word[i + 2] == '\0')
                                return(TRUE);
                        else
                                return(FALSE);
                }
        }
        printf("Fatal logic error!\n");
        exit(1);
}

static void inorder_traversal(struct tree *node)
{
    if (node->left != NULL)
        inorder_traversal(node->left); /* resursive */
    visit_node(node);
    if (node->right != NULL)
        inorder_traversal(node->right); /* resursive */
}

static long int l_bit_twiddle(long int number)
{
        if ((number & (long) EOR) == (long) EOR)
        {
                number |= (long) 0x20000000;
                number &= (long) 0xffffff00; /* Clear low byte. The 00 can be anything
                                     except EOR and the code will work. */
        }
        if ((number & ((long) EOR << 8)) == ((long) EOR << 8))
        {
                number |= (long) 0x40000000;
                number &= (long) 0xffff00ff; /* Clear byte. The 00 can be anything
                                     except EOR and the code will work. */
        }
        if ((number & ((long) EOR << 16)) == ((long) EOR << 16))
        {
                number |= (long) 0x80000000;
                number &= (long) 0xff00ffff; /* Clear byte. The 00 can be anything
                                     except EOR and the code will work. */
        }
        return(number);
}

/*
** The word's record in references.dat: its count, then its verses, then
** EOR. Every number is 4 bytes; none starts with the byte EOR (0xFF), as
** none comes near 0xFF000000, so they need no bit twiddling, unlike the
** offsets in words.dat. (Before EBaC 2.1, 2 bytes, twiddled.)
*/
static void visit_node(struct tree *node)
{
        long int offset;

        fprintf(words_file, "%s", node->item);
/* Store current file position of the references_file into the words_file */
        offset = ftell(references_file);
/*
** printf("current ref offset is: %ld\n", offset);
*/
        offset = l_bit_twiddle(offset);
/*
** printf("bit twiddle ref offset is: %ld\n", offset);
*/
        fputl(offset, words_file);
        fputc(EOR, words_file);

        fputl(node->count, references_file);
        fputl(node->count, position_file);
        fputl(node->count, plainpos_file);
        print_list(node->verse_head);
        fputc(EOR, references_file);
        fputc(EOR, position_file);
        fputc(EOR, plainpos_file);
}

/*
** The occurrences: their verses in references.dat, and their positions,
** at the same offsets, in position.dat and plainpos.dat (the three files
** are laid out alike: see readme.architecture).
*/
static void print_list(struct list *listptr)
{
        for (; listptr != NULL; listptr = listptr->next)
        {
                fputl(listptr->number, references_file);
                fputl(listptr->full, position_file);
                fputl(listptr->plain, plainpos_file);
        }
}

/*
** Purpose: note an occurrence just stored as the verse's last word so far,
**          and, unless it is in brackets, its last plain word so far.
*/
static void note_last(struct list *occurrence)
{
        last_word = occurrence;
        if (occurrence->plain != 0)
                last_plain = occurrence;
}

/*
** Purpose: at the end of a verse (a new verse begins, or a book or the
**          text ends): flag its last word's full position, and its last
**          plain word's plain position, LAST; and start counting again.
*/
static void end_verse(void)
{
        if (last_word != NULL)
                last_word->full |= LAST_POSITION;
        if (last_plain != NULL)
                last_plain->plain |= LAST_POSITION;
        last_word = last_plain = NULL;
        full_position = plain_count = plain_position = 0;
        in_brackets = FALSE;
}

/*
** Purpose: the positions of the word about to be stored (from raw_word, as
**          it was in the book): whether it is in [brackets] or {braces}
**          (a [ or { at its start opens them, a ] or } at its end, before
**          any punctuation after it, closes them), and its full and plain
**          positions.
*/
static void note_brackets(void)
{
        int bracketed, i;

        if (raw_word[0] == '[' || raw_word[0] == '{')
                in_brackets = TRUE;
        bracketed = in_brackets;
        for (i = u8_prev(raw_word, strlen(raw_word));
             raw_word[0] != '\0' && !u8_is_letter(&raw_word[i]);
             i = u8_prev(raw_word, i))
        {
                if (raw_word[i] == ']' || raw_word[i] == '}')
                {
                        in_brackets = FALSE;
                        break;
                }
                if (i == 0)
                        break;
        }
        full_position++;
        if (bracketed)
                plain_position = 0;
        else
                plain_position = ++plain_count;
}

int main(int argc, char **argv)
{
#ifdef COCO
	pflinit(); /* needed to link libraries to print _long_ integers */
#endif
#ifdef OSK
	_mallocmin(262144);
#endif
/* "index -y" skips the question; the makefile uses it */
if (argc < 2 || strcmp(argv[1], "-y") != 0)
{
    printf("Are you sure you want to re-generate the Bible index?\n");
    printf("This will overwite the index files, *.dat, in your current\n");
    printf("working directory.\n\n");
    printf("Press \"y\" to continue, or \"n\" to abort\n");
    if (getchar() != (int) 'y')
            exit(0);
}
chapter_file = mfopen("chapter.dat", WRITEMODE);/* all *.dat files are binary */
cvarray_file = mfopen("cvarray.dat", WRITEMODE);
bvarray_file = mfopen("bvarray.dat", WRITEMODE);
book_list = mfopen("booklist", "r");            /* not a binary file */

while (fgets(book_name, sizeof(book_name), book_list) != NULL)
{
  book_name[strlen(book_name) - 1] = '\0';
  book_file = mfopen(book_name, "r");
  fputl(sequence, bvarray_file);
  fputl(chapter, bvarray_file);
  printf("Reading from book: %s\n", book_name);
  for(;;){
        read_word(book_file);
        if (feof(book_file))
                break;
        if (word[0] == '\0')
                continue;
        if (new_verse() && line_start)  /* (new_verse() first: it also keeps
                                           track of {notes}) */
        {
                end_verse();            /* the verse before it ends */
                sequence++;
                if (new_chapter())
                {
                        chapter++;
                        fputl(sequence, chapter_file);
                }
/*
** Where the verse number starts in the book: the word as it was there,
** such as [1:1] with its brackets, not as it is after its punctuation was
** removed.
*/
                offset = ftell(book_file);
                offset -= (long) raw_length;
                fputl(--offset,cvarray_file);
        }else
        {
                note_brackets();
                node = store_word(node);
        }
  }
  end_verse();                          /* the book's last verse ends */
  mfclose(book_file, book_name);
}
fputl(sequence, bvarray_file); /* Added, so that we see where the end of */
fputl(chapter, bvarray_file);  /* Revelation is. */
fputl(++sequence, chapter_file);
mfclose(book_list, "booklist");
mfclose(bvarray_file, "bvarray.dat");
mfclose(cvarray_file, "cvarray.dat");
mfclose(chapter_file, "chapter.dat");

words_file = mfopen("words.dat", WRITEMODE);
references_file = mfopen("references.dat", WRITEMODE);
position_file = mfopen("position.dat", WRITEMODE);
plainpos_file = mfopen("plainpos.dat", WRITEMODE);

inorder_traversal(node);

mfclose(words_file, "words.dat");
mfclose(references_file, "references.dat");
mfclose(position_file, "position.dat");
mfclose(plainpos_file, "plainpos.dat");

/* which format the index files are in, for EBaC to check */
format_file = mfopen("format.dat", WRITEMODE);
fprintf(format_file, "%s\n", INDEX_FORMAT);
mfclose(format_file, "format.dat");
#ifdef OSK
	system("attr *.dat -pr"); /* permit public read access */
#endif
} /* End of main() */

