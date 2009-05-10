/* 
   EBaC (Electronic Bible and Concordance)
   written by Neil G. Brookins
   http://ebac.sourceforge.net/
*/
/* Purpose:
Counts the number of verses in the entire Bible.
Requires list of books of the Bible in file "booklist" in the current
working directory. Program reads files in this list and counts the
occurances of a colon ":" surrounded by two digits.

Bugs: Won't work if text doesn't have verse numbers in cc:vv form where
cc == Chapter and vv == Verse. If the text itself contains the string
n:n where "n" is a digit, then this will be counted, incorrectly, as a verse.
*/

#include <stdio.h>
#include <ctype.h>
main()
{
FILE *book_list, *book_file;
char book_name[80];
int a, b, c;                            /* a single character */
short int numverses, totalverses;       /* a running count */

totalverses = 0;
book_list = fopen("booklist", "r");
if (book_list == (FILE *) NULL)
        {
        printf("Can't Open \"booklist\".\n");
        printf("(This is a list of books of the bible, one per line)\n");
        exit(1);
        }

while (fgets(book_name, sizeof(book_name), book_list) != NULL)
{
  book_file = fopen(book_name, "r");
  if (book_file == (FILE *) NULL)
  {
    book_name[strlen(book_name) - 1] = NULL;
    printf("Can't open book: %s\n", book_name);
    printf("\"%s\" needs to be in the current working directory.\n", book_name);
    exit(1);
  }
  printf("Reading from book: %s", book_name);
  fflush(stdout);
  a = fgetc(book_file);
  b = fgetc(book_file);
  c = fgetc(book_file);
  numverses = 0;
  do{
    if ((char)b == ':' && isdigit((char)a) && isdigit((char)c))
      numverses++;
    a = b;
    b = c;
    c = fgetc(book_file);
  }while (!(feof(book_file)));
  fclose(book_file);
  totalverses += numverses;
  printf("There are %d verses in %s", numverses, book_name);
  fflush(stdout);
}
fclose(book_list);
printf("There are %d total verses in the entire Bible.\n", totalverses);
}

