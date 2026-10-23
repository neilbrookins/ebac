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
*/

#include "both.h"
#ifdef OSK
#       define malloc _lmalloc
#endif

FILE *book_list, *book_file, *cvarray_file, *bvarray_file, *chapter_file,
     *words_file, *common_file, *references_file;
char book_name[80];
char word[80];                          /* current word being processed */
short int sequence = 0;                 /* sequential verse number */
short int chapter = 0;
struct tree
{
        char *item;                     /* one word */
        struct tree *left;              /* pointer to child */
        struct tree *right;
        struct list *verse_head;        /* first verse that word appears in */
        struct list *verse_tail;
/*
** There is a problem with this being just 2 bytes. Some words occur so many
** times that the high bit gets set! We have reserved the high bit here
** to indicate that the low order byte was FF and conflicted with the
** recognition of EOR which is also FF 
*/
        unsigned short int count;           /* number of times word occurs */
};
struct list
{
        short int number;               /* verse number */
        struct list *next;              /* points to next element of list */
};

struct tree *node = NULL;

long int offset; /* number of bytes offset from the beginning
                   of a book that a particular verse begins at. */
int disable_newverse = 0;/*0,1,or2 mean: enabled, about to enable, disabled */

/* functions in this file */
static void fputl(long int offset, FILE *fileptr);
static void fputi(short int offset, FILE *fileptr);
static void mfclose(FILE *fileptr, char *string);
static FILE *mfopen(char *string, char *mode);
static void read_word(FILE *book_file);
static void remove_head_punctuation(void);
static void remove_tail_punctuation(void);
static void make_into_lowercase(void);
static struct tree *store_word(struct tree *node);
static void free_node(struct list *listptr);
static int new_verse(void);
static int new_chapter(void);
static void inorder_traversal(struct tree *node);
static short int i_bit_twiddle(short int number);
static long int l_bit_twiddle(long int number);
static void visit_node(struct tree *node);
static void print_list(struct list *listptr);

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
** Purpose: similar to the fputc function, but it works with short int
**          instead of char. This should take the most significant byte
**          of the short int first and write it out to the file.
*/
static void fputi(short int offset, FILE *fileptr)
{
        fputc((char) ((short) offset >> 8), fileptr);
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

/* Skip over leading white space */
do{
    word[i] = (char) fgetc(book_file);
}while (word[i] == ' ' || word[i] == '\n');

/* Read until a delimeter is found, or EOF */
do{
    word[++i] = (char) fgetc(book_file);
}while (word[i] != ' ' && word[i] != '\n' && !(feof(book_file)));

word[i] = '\0';  /* Need to terminate the string */
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
remove_head_punctuation();
remove_tail_punctuation();
make_into_lowercase();
}

static void remove_head_punctuation(void)
{
  int i;
  if (isalnum((unsigned char) word[0]) || word[0] == '\0')
          return;
  else
  {
  for(i = 0; i < (int) strlen(word); i++)
          word[i] = word[i + 1];
  remove_head_punctuation();            /* Oooh! Neat! A recursive call! */
  }
}

static void remove_tail_punctuation(void)
{
  if (word[0] == '\0' || isalnum((unsigned char) word[strlen(word) - 1]))
          return;
  else
  {
          word[strlen(word) - 1] = '\0';
          remove_tail_punctuation();    /* Oooh! Neat! A recursive call! */
  }
}

static void make_into_lowercase(void)
{
        int i = 0;
        while ((word[i] = tolower((unsigned char) word[i])) != '\0')
                i++;
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
                        printf ("malloc failed! Is 2MAXREF too large?\n");
                        exit(1);
                }
                strcpy(newitem, word);
                node->item = newitem;
                node->left = NULL;
                node->right = NULL;

                newnumber = (struct list *) malloc (sizeof(struct list));
                if (newnumber == NULL)
                {
                        printf ("malloc failed! Is 3MAXREF too large?\n");
                        exit(1);
                }
                newnumber->number = sequence;
                newnumber->next = NULL;
                node->verse_head = node->verse_tail = newnumber;
                node->count = 1;
                return(node);
        }
        n = strcmp(word, node->item);
        if (n == 0) /* Word is already in tree. Add new verse to list. */
        {
            if (node->count++ == MAXREF) /* Word is common */
            {
                free_node(node->verse_head); /* return memory to OS */
                node->verse_head = NULL;
            }
            if (node->count <= MAXREF)
            {
                if (node->verse_tail->number == sequence)
                        return(node); /* Avoid storing same number twice */
                newnumber = (struct list *) malloc (sizeof(struct list));
                if (newnumber == NULL)
                {
                        printf ("malloc failed! Is 4MAXREF too large?\n");
                        exit(1);
                }
                node->verse_tail->next = newnumber;
                node->verse_tail = newnumber;
                node->verse_tail->number = sequence;
                node->verse_tail->next = NULL;
            }
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

static void free_node(struct list *listptr)
{
        if (listptr->next != NULL)
                free_node(listptr->next); /* resursive */
#ifdef OSK
        _lfree(listptr, (unsigned long) sizeof (struct list));
#else
        free(listptr);
#endif
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

static short int i_bit_twiddle(short int number)
{
        if ((number & EOR) == EOR)
        {
                number |= 0x8000;
                number &= 0xff00; /* Clear low byte. The 00 can be anything
                                     except EOR and the code will work. */
        }
        return(number);
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

static void visit_node(struct tree *node)
{
        long int offset;
        if (node->verse_head != NULL)
        { /* Not a common word */
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

                node->count = i_bit_twiddle(node->count);
                fputi(node->count, references_file);
                print_list(node->verse_head);
                fputc(EOR, references_file);
        }else{ /* Common word */
                fprintf(common_file, "%s", node->item);
                node->count = i_bit_twiddle(node->count);
                fputi(node->count, common_file);
                fputc(EOR, common_file);
        }
}

static void print_list(struct list *listptr)
{
        listptr->number = i_bit_twiddle(listptr->number);
        fputi(listptr->number, references_file);
        if (listptr->next != NULL)
                print_list(listptr->next); /* resursive */
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
    printf("This will overwite the six files, *.dat, in your current\n");
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
  fputi(sequence, bvarray_file);
  fputi(chapter, bvarray_file);
  printf("Reading from book: %s\n", book_name);
  for(;;){
        read_word(book_file);
        if (feof(book_file))
                break;
        if (word[0] == '\0')
                continue;
        if (new_verse())
        {
                sequence++;
                if (new_chapter())
                {
                        chapter++;
                        fputi(sequence, chapter_file);
                }
                offset = ftell(book_file);
                offset -= (long) strlen(word);
                fputl(--offset,cvarray_file);
        }else
                node = store_word(node);
  }
  mfclose(book_file, book_name);
}
fputi(sequence, bvarray_file); /* Added, so that we see where the end of */
fputi(chapter, bvarray_file);  /* Revelation is. */
fputi(++sequence, chapter_file);
mfclose(book_list, "booklist");
mfclose(bvarray_file, "bvarray.dat");
mfclose(cvarray_file, "cvarray.dat");
mfclose(chapter_file, "chapter.dat");

words_file = mfopen("words.dat", WRITEMODE);
common_file = mfopen("common.dat", WRITEMODE);
references_file = mfopen("references.dat", WRITEMODE);

inorder_traversal(node);

mfclose(words_file, "words.dat");
mfclose(common_file, "common.dat");
mfclose(references_file, "references.dat");
#ifdef OSK
	system("attr *.dat -pr"); /* permit public read access */
#endif
} /* End of main() */

