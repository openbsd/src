#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>

RCSID("$KTH: echo-n.c,v 1.1 1999/12/11 04:57:31 lha Exp $");

int
main (int argc, char **argv)
{
    int i;
    for (i = 1; i < argc ; i++) {
	printf ("%s", argv[i]);
	if (argc > i + 1)
	    printf (" ");
    }
    fflush (stdout);
    return 0;
}
