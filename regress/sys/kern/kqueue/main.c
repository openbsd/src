/*	$OpenBSD: main.c,v 1.4 2003/06/12 05:06:47 mickey Exp $	*/
/*
 *	Written by Artur Grabowski <art@openbsd.org> 2002 Public Domain
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

int do_pipe(void);
int check_inheritance(void);
int do_process(void);
int do_random(void);
int do_pty(void);

int
main(int argc, char **argv)
{
	extern char *__progname;
	int ret, c;

	ret = 0;
	while ((c = getopt(argc, argv, "fPprT")) != -1) {
		switch (c) {
		case 'p':
			ret |= do_pipe();
			break;
		case 'f':
			ret |= check_inheritance();
			break;
		case 'P':
			ret |= do_process();
			break;
		case 'r':
			ret |= do_random();
			break;
		case 'T':
			ret |= do_pty();
			break;
		default:
			fprintf(stderr, "Usage: %s -[fPprt]\n", __progname);
			exit(1);
		}
	}

	return (ret);
}
