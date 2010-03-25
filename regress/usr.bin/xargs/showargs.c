/*	 $OpenBSD: showargs.c,v 1.1 2010/03/25 01:43:47 schwarze Exp $ */

/*
 * written by Ingo Schwarze <schwarze@openbsd.org> 2010
 * and placed in the public domain
 */

#include <stdio.h>

int
main(int argc, char *argv[]) {
	int i;

	for (i = 1; i < argc; i++)
		printf("%s|", argv[i]);
	putchar('\n');

	return 0;
}
