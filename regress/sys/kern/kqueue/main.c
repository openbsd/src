/*	$OpenBSD: main.c,v 1.2 2002/03/02 21:48:05 art Exp $	*/
/*
 *	Written by Artur Grabowski <art@openbsd.org> 2002 Public Domain
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

extern int do_pipe(void);
extern int check_inheritance(void);
extern int do_process(void);

int
main(int argc, char **argv)
{
	int ret, c;

	ret = 0;
	while ((c = getopt(argc, argv, "pfP")) != -1) {
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
		default:
			fprintf(stderr, "Usage: kqtest -P|p|f\n");
			exit(1);
		}
	}

	return (ret);
}
