/* 
   EBaC (Electronic Bible and Concordance)
   written by Neil G. Brookins
   http://ebac.sourceforge.net/
*/
#include "bible.h"

long int fgetl(FILE *fileptr)
{
        long int offset;
        offset = fgetc(fileptr);
        offset <<= 8;
        offset |= (0xff & fgetc(fileptr));
        offset <<= 8;
        offset |= (0xff & fgetc(fileptr));
        offset <<= 8;
        offset |= (0xff & fgetc(fileptr));
        if(feof(fileptr))
                printf("Fatal error! EOF reached\n");
        return(offset);
}

short int fgeti(FILE *fileptr)
{
        short int offset;
        offset = fgetc(fileptr);
        offset <<= 8;
        offset |= (0xff & fgetc(fileptr));
        if((feof(fileptr)) && ((0xff00 & offset) != 0xff00))
                printf("Fatal error! EOF reached\n");
        return(offset);
}

#ifdef RAMDISK
/*
** Purpose: similar to the fputc function, but it works with short int
**          instead of char. This should take the most significant byte
**          of the short int first and write it out to the file.
*/
void fputi(short int offset, FILE *fileptr)
{
        fputc((char) ((short) offset >> 8), fileptr);
        fputc((char) offset, fileptr);
}
#endif

int new_verse(char *word)
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
    for(i = 0; i < strlen(word); i++)
    {
            c = word[i];
            if (!(isdigit(c) || c == ':'))
                    return(FALSE);
    } /* word contains digit(s), colon(s), or both. */
    for(i = 1; i < strlen(word) - 1; i++)
    {
      if (word[i] == ':' && isdigit(word[i - 1]) && isdigit(word[i + 1]))
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
int new_chapter(char *word)
{
        int i;

        for(i=1;i < (strlen(word) - 1); i++)
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

int wordtochapt(char *word)
{
        int chapt = 0;
        int i;
        for(i = 0; i < strlen(word); i++)
        {
                if (word[i] == ':')
                        break;
                chapt *= 10;
                chapt += word[i] - '0';
        }
        return(chapt);
}

int wordtoverse(char *word)
{
        int verse = 0;
        int i;
        for(i = 0; i < strlen(word); i++)
                if (word[i] == ':')
                        break;
        for(i++; i < strlen(word); i++)
        {
                verse *= 10;
                verse += word[i] - '0';
        }
        return(verse);
}

short int i_bit_reset(short int number)
{
        if (number & 0x8000) /* High bit set? */
        {
                number &= 0x7fff; /* Clear high bit */
                number |= 0x00ff; /* Set low byte. */
        }
        return(number);
}

long int l_bit_reset(long int l_number)
{
        if (l_number & 0x20000000L)
        {
                l_number &= 0xdfffffffL; /* Clear bit */
                l_number |= 0x000000ffL; /* Set byte. */
        }
        if (l_number & 0x40000000L)
        {
                l_number &= 0xbfffffffL; /* Clear bit */
                l_number |= 0x0000ff00L; /* Set byte. */
        }
        if (l_number & 0x80000000L)
        {
                l_number &= 0x7fffffffL; /* Clear bit */
                l_number |= 0x00ff0000L; /* Set byte. */
        }
        return((long) l_number);
}

short int bcv_search(struct bcv_type *bcvptr)
{
#ifndef COCO
    extern short int *pach;
    extern struct bva *book;
#else
    extern short int pach[];
    extern struct bva book[];
#endif
int seq_chapter;

/*
**printf("booknum: %d\n", bcvptr->book_number);
*/
seq_chapter = book[(bcvptr->book_number) - 1].chapt;
/*
**printf("Should be zero on book: %d\n", seq_chapter);
*/

seq_chapter += bcvptr->chapter_number - 1;

/*
** printf("then chapt: %d\n", seq_chapter);
** printf("Returned before verse: %d\n", pach[seq_chapter]);
*/

return(pach[seq_chapter] + bcvptr->verse_number - 1);
}

