/*
** utf8.h -- characters of UTF-8 text, for EBaC and its index program
**           (EBaC 2.3). See utf8.c.
*/
int u8_len P((char *s));
long u8_code P((char *s));
int u8_is_letter P((char *s));
int u8_width P((char *s));
int u8_cols P((char *s, int nbytes));
int u8_fit P((char *s, int cols));
int u8_prev P((char *s, int i));
void u8_lower P((char *s));
int u8_is_rtl P((char *s));
int u8_display_order P((char *s, int n, int *start, int *len, int max));
