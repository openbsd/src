#ifndef lint
static char rcsid[] = "$NetBSD: nap.c,v 1.4 1995/04/27 22:15:28 mycroft Exp $";
#endif /* not lint */

/* nap.c		 Larn is copyrighted 1986 by Noah Morgan. */
#include <signal.h>
#include <sys/types.h>

/*
 *	routine to take a nap for n milliseconds
 */
nap(x)
	register int x;
	{
	if (x<=0) return; /* eliminate chance for infinite loop */
	lflush();
	usleep(x*1000);
	}
