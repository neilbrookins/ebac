#include <stdio.h>

main()
{
int c;
int i = 0;

  while ((c = getchar()) != EOF)
  {
          i++;
          if (c == EOL)
          {
                if (i > 55)
                  printf("Line length: %d\n", i);
                i = 0;
          }
  }
}

