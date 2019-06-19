/*	$OpenBSD: main.c,v 1.14 2019/03/04 19:33:41 anton Exp $	*/
/*
 *	Written by Artur Grabowski <art@openbsd.org> 2002 Public Domain
 */

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "main.h"

int
main(int argc, char **argv)
{
	extern char *__progname;
	int n, ret, c;

	ret = 0;
	while ((c = getopt(argc, argv, "fFiIlpPrR:stT:")) != -1) {
		switch (c) {
		case 'f':
			ret |= check_inheritance();
			break;
		case 'F':
			ret |= do_fdpass();
			break;
		case 'i':
			ret |= do_timer();
			break;
		case 'I':
			ret |= do_invalid_timer();
			break;
		case 'l':
			ret |= do_flock();
			break;
		case 'p':
			ret |= do_pipe();
			break;
		case 'P':
			ret |= do_process();
			break;
		case 'r':
			ret |= do_random();
			break;
		case 'R':
			n = strtonum(optarg, 1, INT_MAX, NULL);
			ret |= do_regress(n);
			break;
		case 's':
			ret |= do_signal();
			break;
		case 't':
			ret |= do_tun();
			break;
		case 'T':
			n = strtonum(optarg, 1, INT_MAX, NULL);
			ret |= do_pty(n);
			break;
		default:
			fprintf(stderr, "usage: %s -[fFiIlpPrstT] [-R n]\n",
			    __progname);
			exit(1);
		}
	}

	return (ret);
}
