/*
 *	This simple classical example of recursion is useful for
 *	testing stack backtraces and such.
 */

#ifdef vxworks

#  include <stdio.h>

/* VxWorks does not supply atoi.  */
static int
atoi (z)
     char *z;
{
  int i = 0;

  while (*z >= '0' && *z <= '9')
    i = i * 10 + (*z++ - '0');
  return i;
}

/* I don't know of any way to pass an array to VxWorks.  This function
   can be called directly from gdb.  */

vxmain (arg)
char *arg;
{
  char *argv[2];

  argv[0] = "";
  argv[1] = arg;
  main (2, argv, (char **) 0);
}

#else /* ! vxworks */
#  include <stdio.h>
#endif /* ! vxworks */

main (argc, argv, envp)
int argc;
char *argv[], **envp;
{
#ifdef usestubs
    set_debug_traps();
    breakpoint();
#endif
    if (argc != 2) {
	printf ("usage:  factorial <number>\n");
	return 1;
    } else {
	printf ("%d\n", factorial (atoi (argv[1])));
    }
    return 0;
}

int factorial (value)
int value;
{
    if (value > 1) {
	value *= factorial (value - 1);
    }
    return (value);
}
