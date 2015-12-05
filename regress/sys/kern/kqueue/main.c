/*	$OpenBSD: main.c,v 1.8 2015/12/05 10:51:49 blambert Exp $	*/
/*
 *	Written by Artur Grabowski <art@openbsd.org> 2002 Public Domain
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

int do_pipe(void);
int check_inheritance(void);
int do_process(void);
int do_signal(void);
int do_random(void);
int do_pty(void);
int do_tun(void);
int do_fdpass(void);
int do_flock(void);
int do_timer(void);

int
main(int argc, char **argv)
{
	extern char *__progname;
	int ret, c;

	ret = 0;
	while ((c = getopt(argc, argv, "fFilpPrstT")) != -1) {
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
		case 's':
			ret |= do_signal();
			break;
		case 't':
			ret |= do_tun();
			break;
		case 'T':
			ret |= do_pty();
			break;
		default:
			fprintf(stderr, "Usage: %s -[fPprTt]\n", __progname);
			exit(1);
		}
	}

	return (ret);
}
