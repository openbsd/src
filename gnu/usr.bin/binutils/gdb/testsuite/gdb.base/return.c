#include <stdio.h>
/*  Test "return" command.  */

func1 ()
{
  printf("in func1\n");
}

int
func2 ()
{
  return -5;
}

double
func3 ()
{
  return -5.0;
}

int tmp2;
double tmp3;

main ()
{
  func1 ();
  printf("in main after func1\n");
  tmp2 = func2 ();
  tmp3 = func3 ();
  printf("exiting\n");
}
