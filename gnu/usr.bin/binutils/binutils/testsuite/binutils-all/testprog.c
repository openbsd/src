/* This program is used to test objcopy and strip.  */

#include <stdio.h>

int common;
int global = 1;
static int local = 2;
static char string[] = "string";

int
fn ()
{
  return 3;
}

int
main ()
{
  if (common != 0
      || global != 1
      || local != 2
      || strcmp (string, "string") != 0)
    {
      printf ("failed\n");
      exit (1);
    }

  printf ("ok\n");
  exit (0);
}
