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

/*
 * The following functions do nothing useful.  They are included simply
 * as places to try setting breakpoints at.  They are explicitly
 * "one-line functions" to verify that this case works (some versions
 * of gcc have or have had problems with this).
 */

int marker1 () { return (0); }
int marker2 (a) int a; { return (1); }
void marker3 (a, b) char *a, *b; {}
void marker4 (d) long d; {}

/*
 *	This simple classical example of recursion is useful for
 *	testing stack backtraces and such.
 */

int
main (argc, argv, envp)
int argc;
char *argv[], **envp;
{
#ifdef usestubs
    set_debug_traps();
    breakpoint();
#endif
    if (argc == 123456) {
	fprintf (stderr, "usage:  factorial <number>\n");
	return 1;
    }
    printf ("%d\n", factorial (atoi ("6")));

    marker1 ();
    marker2 (43);
    marker3 ("stack", "trace");
    marker4 (177601976L);
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

