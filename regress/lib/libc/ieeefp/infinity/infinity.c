/*	$OpenBSD: infinity.c,v 1.1 2004/01/15 18:53:24 miod Exp $	*/
/*
 * Written by Miodrag Vallat, 2004 - Public Domain
 * Inspired from Perl's t/op/arith test #134
 */

#include <math.h>
#include <signal.h>

void
sigfpe(int signum)
{
	/* looks like we don't handle fp overflow correctly... */
	_exit(1);
}

int
main(int argc, char *argv[])
{
	double d, u;
	int i;

	signal(SIGFPE, sigfpe);

	d = 1.0;
	for (i = 2000; i != 0; i--) {
		d = d * 2.0;
	}

	/* result should be _positive_ infinity */
	return ((isinf(d) && copysign(1.0, d) > 0.0) ? 0 : 1);
}
