/*
 *	This simple program that passes different types of arguments
 *      on function calls.  Useful to test printing frames, stepping, etc.
 */

int callee4 (void)
{
  int A=1;
  int B=2;
  int C;

  C = A + B;
  return 0;
}
callee3 (char *strarg)
{
  callee4 ();
}

callee2 (int intarg, char *strarg)
{
  callee3 (strarg);
}

callee1 (int intarg, char *strarg, double fltarg)
{
  callee2 (intarg, strarg);
}

main ()
{
  callee1 (2, "A string argument.", 3.5);
  callee1 (2, "A string argument.", 3.5);

  printf ("Hello, World!");

  return 0;
}

/*
Local variables: 
change-log-default-name: "ChangeLog-mi"
End: 
*/

