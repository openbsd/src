/*	$OpenBSD: gettimeofday.c,v 1.1 2002/02/21 09:21:30 nordin Exp $	*/
/*
 *	Written by Thomas Nordin <nordin@openbsd.org> 2002 Public Domain.
 */
#include <err.h>
#include <stdio.h>

#include <sys/time.h>

int
main()
{
	struct timeval s;
	struct timeval t1;
	struct timeval t2;

	if (gettimeofday(&s, NULL) == -1)
		err(1, "gettimeofday");

	do {
		if (gettimeofday(&t1, NULL) == -1)
			err(1, "gettimeofday");
		if (gettimeofday(&t2, NULL) == -1)
			err(1, "gettimeofday");

		if (timercmp(&t2, &t1, <))
			errx(1, "time of day decreased");
        } while (t1.tv_sec - s.tv_sec < 7);

        return 0;
}
