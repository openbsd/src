/*	$OpenBSD: main.c,v 1.1 2002/02/27 17:11:51 art Exp $	*/
/*
 *	Written by Artur Grabowski <art@openbsd.org> 2002 Public Domain
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

extern int do_pipe(void);
extern int check_inheritance(void);

int
main(int argc, char **argv)
{
	int ret, c;

	ret = 0;
	while ((c = getopt(argc, argv, "pf")) != -1) {
		switch (c) {
		case 'p':
			ret |= do_pipe();
			break;
		case 'f':
			ret |= check_inheritance();
			break;
		default:
			fprintf(stderr, "Usage: kqtest -P|p\n");
			exit(1);
		}
	}

	return (ret);
}
