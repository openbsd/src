#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>

RCSID("$Id: echo-n.c,v 1.1 2000/09/11 14:41:29 art Exp $");

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
