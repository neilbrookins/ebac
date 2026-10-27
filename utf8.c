/*
   EBaC (Electronic Bible and Concordance)
   written by Neil G. Brookins
*/
/*
** utf8.c -- the characters of UTF-8 text (EBaC 2.3), for EBaC and for its
** index program. EBaC was written for ASCII text, where a character is a
** byte; a translation's text is now UTF-8, where a character other than
** ASCII is two to four bytes: Hebrew, Greek, or an accented letter (é is
** the two bytes C3 A9). The C library's isalnum() and tolower() know only
** ASCII here (in the "C" locale, as EBaC runs), so a word's accented
** letters were taken for punctuation. These functions know the letters of
** the alphabets a Bible may be in, and nothing depends on the computer's
** language settings, so that every system, the CoCo too, does the same.
**
** A character is given as a pointer to its first byte. A byte that isn't
** part of a well-formed UTF-8 character is a character of its own: not a
** letter, one column wide. (Bytes are masked with 0xFF, as the CoCo's
** compiler has no unsigned char; code points are long, as its int is 16
** bits.) See "UTF-8" in readme.architecture.
*/
#include "both.h"
#include "utf8.h"

#define BYTE(c) ((int) ((c) & 0xFF))
#define CONTINUATION(c) ((BYTE(c) & 0xC0) == 0x80)

/*
** Code points (beyond ASCII) that are not letters: punctuation, spaces,
** symbols. Any other character beyond ASCII is taken as a letter, or a
** mark on one (an accent, a Hebrew vowel point), which belongs to the word.
*/
static long not_letters[][2] = {
        { 0x0080L, 0x00A9L }, { 0x00ABL, 0x00B4L }, { 0x00B6L, 0x00B9L },
        { 0x00BBL, 0x00BFL }, { 0x00D7L, 0x00D7L }, { 0x00F7L, 0x00F7L },
        { 0x037EL, 0x037EL }, { 0x0387L, 0x0387L },     /* Greek ; and . */
        { 0x055AL, 0x055FL }, { 0x0589L, 0x058AL },     /* Armenian */
        { 0x05BEL, 0x05BEL }, { 0x05C0L, 0x05C0L },     /* Hebrew maqaf, paseq */
        { 0x05C3L, 0x05C3L }, { 0x05C6L, 0x05C6L },     /* sof pasuq, nun hafukha */
        { 0x0600L, 0x060FL }, { 0x061BL, 0x061FL },     /* Arabic */
        { 0x066AL, 0x066DL }, { 0x06D4L, 0x06D4L },
        { 0x0964L, 0x0965L },                           /* Devanagari dandas */
        { 0x0E3FL, 0x0E3FL }, { 0x0E4FL, 0x0E4FL }, { 0x0E5AL, 0x0E5BL },
        { 0x10FBL, 0x10FBL }, { 0x1360L, 0x1368L },     /* Georgian, Ethiopic */
        { 0x166DL, 0x166EL }, { 0x1680L, 0x1680L }, { 0x16EBL, 0x16EDL },
        { 0x2000L, 0x200BL }, { 0x200EL, 0x206FL },     /* spaces, dashes, quotes */
        { 0x2070L, 0x20CFL },                           /* super/subscripts, money */
        { 0x2100L, 0x214FL },                           /* letterlike symbols */
        { 0x2190L, 0x2BFFL },                           /* arrows, maths, shapes */
        { 0x2E00L, 0x2E7FL },                           /* more punctuation */
        { 0x3000L, 0x3004L }, { 0x3008L, 0x3020L },     /* CJK punctuation */
        { 0x3030L, 0x3030L }, { 0x303DL, 0x303DL }, { 0x30FBL, 0x30FBL },
        { 0xFD3EL, 0xFD3FL }, { 0xFE10L, 0xFE19L }, { 0xFE30L, 0xFE6FL },
        { 0xFEFFL, 0xFEFFL },
        { 0xFF01L, 0xFF0FL }, { 0xFF1AL, 0xFF20L }, { 0xFF3BL, 0xFF40L },
        { 0xFF5BL, 0xFF65L }, { 0xFFE0L, 0xFFFFL },
        { 0x1F000L, 0x1FAFFL },                         /* emoji, symbols */
        { 0L, 0L }
};

/* Characters that take no column of their own: marks on the letter before
   them (accents, Hebrew points), and invisible ones */
static long zero_width[][2] = {
        { 0x0300L, 0x036FL }, { 0x0483L, 0x0489L }, { 0x0591L, 0x05BDL },
        { 0x05BFL, 0x05BFL }, { 0x05C1L, 0x05C2L }, { 0x05C4L, 0x05C5L },
        { 0x05C7L, 0x05C7L }, { 0x0610L, 0x061AL }, { 0x064BL, 0x065FL },
        { 0x0670L, 0x0670L }, { 0x06D6L, 0x06DCL }, { 0x06DFL, 0x06E4L },
        { 0x06E7L, 0x06E8L }, { 0x06EAL, 0x06EDL }, { 0x0711L, 0x0711L },
        { 0x0730L, 0x074AL }, { 0x0E31L, 0x0E31L }, { 0x0E34L, 0x0E3AL },
        { 0x0E47L, 0x0E4EL }, { 0x1AB0L, 0x1AFFL }, { 0x1DC0L, 0x1DFFL },
        { 0x200BL, 0x200FL }, { 0x202AL, 0x202EL }, { 0x2060L, 0x2064L },
        { 0x20D0L, 0x20FFL }, { 0xFE00L, 0xFE0FL }, { 0xFE20L, 0xFE2FL },
        { 0xFEFFL, 0xFEFFL },
        { 0L, 0L }
};

/* Characters two columns wide (Chinese, Japanese, Korean, emoji) */
static long double_width[][2] = {
        { 0x1100L, 0x115FL }, { 0x2E80L, 0x303EL }, { 0x3041L, 0x33FFL },
        { 0x3400L, 0x4DBFL }, { 0x4E00L, 0x9FFFL }, { 0xA000L, 0xA4CFL },
        { 0xAC00L, 0xD7A3L }, { 0xF900L, 0xFAFFL }, { 0xFE30L, 0xFE4FL },
        { 0xFF00L, 0xFF60L }, { 0xFFE0L, 0xFFE6L }, { 0x1F300L, 0x1F64FL },
        { 0x1F900L, 0x1F9FFL }, { 0x20000L, 0x3FFFDL },
        { 0L, 0L }
};

/*
** Capital letters and their small letters: { first, last, step, change }:
** each capital from first to last, every step code points, has its small
** letter at (capital + change). Only pairs whose UTF-8 is the same length,
** so that text can be made small in place.
*/
static long capitals[][4] = {
        { 0x00C0L, 0x00D6L, 1, 0x20 }, { 0x00D8L, 0x00DEL, 1, 0x20 },  /* Latin-1 */
        { 0x0100L, 0x012EL, 2, 1 }, { 0x0132L, 0x0136L, 2, 1 },        /* Latin Extended-A */
        { 0x0139L, 0x0147L, 2, 1 }, { 0x014AL, 0x0176L, 2, 1 },
        { 0x0178L, 0x0178L, 1, 0x00FFL - 0x0178L },                   /* Y with diaeresis */
        { 0x0179L, 0x017DL, 2, 1 },
        { 0x01CDL, 0x01DBL, 2, 1 }, { 0x01DEL, 0x01EEL, 2, 1 },        /* Latin Extended-B */
        { 0x01F8L, 0x021EL, 2, 1 }, { 0x0222L, 0x0232L, 2, 1 },
        { 0x0386L, 0x0386L, 1, 0x26 }, { 0x0388L, 0x038AL, 1, 0x25 },  /* Greek */
        { 0x038CL, 0x038CL, 1, 0x40 }, { 0x038EL, 0x038FL, 1, 0x3F },
        { 0x0391L, 0x03A1L, 1, 0x20 }, { 0x03A3L, 0x03ABL, 1, 0x20 },
        { 0x03D8L, 0x03EEL, 2, 1 },
        { 0x0400L, 0x040FL, 1, 0x50 }, { 0x0410L, 0x042FL, 1, 0x20 },  /* Cyrillic */
        { 0x0460L, 0x0480L, 2, 1 }, { 0x048AL, 0x04BEL, 2, 1 },
        { 0x04C1L, 0x04CDL, 2, 1 }, { 0x04D0L, 0x052EL, 2, 1 },
        { 0x0531L, 0x0556L, 1, 0x30 },                                /* Armenian */
        { 0x1E00L, 0x1E94L, 2, 1 }, { 0x1EA0L, 0x1EFEL, 2, 1 },        /* Latin Extended Additional */
        { 0x1F08L, 0x1F0FL, 1, -8 }, { 0x1F18L, 0x1F1DL, 1, -8 },      /* Greek with accents */
        { 0x1F28L, 0x1F2FL, 1, -8 }, { 0x1F38L, 0x1F3FL, 1, -8 },
        { 0x1F48L, 0x1F4DL, 1, -8 }, { 0x1F59L, 0x1F5FL, 2, -8 },
        { 0x1F68L, 0x1F6FL, 1, -8 }, { 0x1F88L, 0x1F8FL, 1, -8 },
        { 0x1F98L, 0x1F9FL, 1, -8 }, { 0x1FA8L, 0x1FAFL, 1, -8 },
        { 0x1FB8L, 0x1FB9L, 1, -8 }, { 0x1FBAL, 0x1FBBL, 1, -0x4A },
        { 0x1FBCL, 0x1FBCL, 1, -9 }, { 0x1FC8L, 0x1FCBL, 1, -0x56 },
        { 0x1FCCL, 0x1FCCL, 1, -9 }, { 0x1FD8L, 0x1FD9L, 1, -8 },
        { 0x1FDAL, 0x1FDBL, 1, -0x64 }, { 0x1FE8L, 0x1FE9L, 1, -8 },
        { 0x1FEAL, 0x1FEBL, 1, -0x70 }, { 0x1FECL, 0x1FECL, 1, -7 },
        { 0x1FF8L, 0x1FF9L, 1, -0x80 }, { 0x1FFAL, 0x1FFBL, 1, -0x7E },
        { 0x1FFCL, 0x1FFCL, 1, -9 },
        { 0x2160L, 0x216FL, 1, 0x10 },                                /* Roman numerals */
        { 0xFF21L, 0xFF3AL, 1, 0x20 },                                /* full-width */
        { 0L, 0L, 0, 0 }
};

static int in_table P((long table[][2], long code));
static int put_code P((long code, char *s, int len));

/* Purpose: is code in one of the ranges of table? */
static int in_table(long table[][2], long code)
{
        int i;

        for (i = 0; table[i][1] != 0L; i++)
                if (code >= table[i][0] && code <= table[i][1])
                        return (TRUE);
        return (FALSE);
}

/*
** Purpose: the number of bytes in the character that starts at s: 1 for
**          ASCII, and for a byte that isn't part of a well-formed character
**          (a lone continuation byte, or a character cut short); 0 at the
**          end of the string.
*/
int u8_len(char *s)
{
        int c = BYTE(s[0]), n, i;

        if (c == 0)
                return (0);
        if (c < 0x80)
                return (1);
        if (c >= 0xC2 && c <= 0xDF)
                n = 2;
        else if (c >= 0xE0 && c <= 0xEF)
                n = 3;
        else if (c >= 0xF0 && c <= 0xF4)
                n = 4;
        else
                return (1);
        for (i = 1; i < n; i++)
                if (!CONTINUATION(s[i]))
                        return (1);
        return (n);
}

/*
** Purpose: the code point of the character at s (for a byte that isn't part
**          of a well-formed character, 0x110000 plus the byte: no letter).
*/
long u8_code(char *s)
{
        int n = u8_len(s), i;
        long code;

        if (n == 0)
                return (0L);
        if (n == 1)
                return (BYTE(s[0]) < 0x80 ? (long) BYTE(s[0]) :
                        0x110000L + BYTE(s[0]));
        code = BYTE(s[0]) & (n == 2 ? 0x1F : n == 3 ? 0x0F : 0x07);
        for (i = 1; i < n; i++)
                code = (code << 6) | (BYTE(s[i]) & 0x3F);
        return (code);
}

/*
** Purpose: is the character at s a letter or a digit (in any alphabet), or
**          a mark on a letter: part of a word, as isalnum() is for ASCII?
*/
int u8_is_letter(char *s)
{
        long code = u8_code(s);

        if (code < 0x80L)
                return (code != 0L && isalnum((int) code));
        if (code >= 0x110000L)
                return (FALSE);         /* (not well-formed) */
        return (!in_table(not_letters, code));
}

/* Purpose: the number of columns the character at s takes on the screen */
int u8_width(char *s)
{
        long code = u8_code(s);

        if (code < 0x80L || code >= 0x110000L)
                return (1);
        if (in_table(zero_width, code))
                return (0);
        if (in_table(double_width, code))
                return (2);
        return (1);
}

/* Purpose: the number of columns the first nbytes bytes of s take */
int u8_cols(char *s, int nbytes)
{
        int i = 0, n, cols = 0;

        while (i < nbytes && (n = u8_len(&s[i])) > 0)
        {
                cols += u8_width(&s[i]);
                i += n;
        }
        return (cols);
}

/*
** Purpose: the number of bytes at the start of s that fit in cols columns,
**          ending at the end of a character (with any marks after it).
*/
int u8_fit(char *s, int cols)
{
        int i = 0, n, w, used = 0;

        while ((n = u8_len(&s[i])) > 0)
        {
                w = u8_width(&s[i]);
                if (used + w > cols)
                        break;
                used += w;
                i += n;
        }
        return (i);
}

/*
** Purpose: where the character before byte i of s starts (0 if i is 0):
**          for moving back one character, as Backspace does.
*/
int u8_prev(char *s, int i)
{
        int j;

        if (i <= 0)
                return (0);
        for (j = i - 1; j > 0 && i - j < 4 && CONTINUATION(s[j]); j--)
                ;
        if (j + u8_len(&s[j]) == i)
                return (j);
        return (i - 1);         /* (not a well-formed character: one byte) */
}

/* Purpose: write code as UTF-8 at s, in len bytes. Returns: FALSE if it
   doesn't take exactly len bytes (then s is left as it was) */
static int put_code(long code, char *s, int len)
{
        if ((code < 0x80L ? 1 : code < 0x800L ? 2 : code < 0x10000L ? 3 : 4) != len)
                return (FALSE);
        switch (len)
        {
        case 1:
                s[0] = (char) code;
                break;
        case 2:
                s[0] = (char) (0xC0 | (code >> 6));
                s[1] = (char) (0x80 | (code & 0x3F));
                break;
        case 3:
                s[0] = (char) (0xE0 | (code >> 12));
                s[1] = (char) (0x80 | ((code >> 6) & 0x3F));
                s[2] = (char) (0x80 | (code & 0x3F));
                break;
        default:
                s[0] = (char) (0xF0 | (code >> 18));
                s[1] = (char) (0x80 | ((code >> 12) & 0x3F));
                s[2] = (char) (0x80 | ((code >> 6) & 0x3F));
                s[3] = (char) (0x80 | (code & 0x3F));
                break;
        }
        return (TRUE);
}

/*
** Purpose: make the letters of s small, in place: ASCII, and the capitals
**          of the table above (É to é, Σ to σ, or to ς at a word's end,
**          Д to д).
*/
void u8_lower(char *s)
{
        int i = 0, n, k;
        long code;

        while ((n = u8_len(&s[i])) > 0)
        {
                if (n == 1)
                        s[i] = (char) tolower(BYTE(s[i]));
                else
                {
                        code = u8_code(&s[i]);
                        for (k = 0; capitals[k][0] != 0L; k++)
                                if (code >= capitals[k][0] && code <= capitals[k][1] &&
                                    (code - capitals[k][0]) % capitals[k][2] == 0L)
                                {
                                        code += capitals[k][3];
                                        /* (Greek: a sigma ending a word is
                                           written as the final sigma) */
                                        if (code == 0x03C3L && i > 0 &&
                                            !u8_is_letter(&s[i + n]))
                                                code = 0x03C2L;
                                        put_code(code, &s[i], n);
                                        break;
                                }
                }
                i += n;
        }
}
