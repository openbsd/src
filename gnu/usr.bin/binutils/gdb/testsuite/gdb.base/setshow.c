/* IMPORTANT: leave this comment in the first line of this source file. */
/*            this will guarantee that line 1 contains no code. */

#include <stdio.h>

int 
main(argc, argv)
     int argc;
     char **argv;
{
  int i = 1;

  while (i < argc)
    printf ("%s ", argv[i++]);
  return 0;
}
