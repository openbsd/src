/*	$OpenBSD: nap.c,v 1.2 1998/09/15 05:12:32 pjanzen Exp $	*/
/*	$NetBSD: nap.c,v 1.5 1997/10/18 20:03:36 christos Exp $	*/

/* nap.c		 Larn is copyrighted 1986 by Noah Morgan. */
#ifndef lint
static char rcsid[] = "$OpenBSD: nap.c,v 1.2 1998/09/15 05:12:32 pjanzen Exp $";
#endif				/* not lint */

#include <unistd.h>
#include "header.h"
#include "extern.h"

/*
 *	routine to take a nap for n milliseconds
 */
void
nap(x)
	int	x;
{
	if (x <= 0)
		return;		/* eliminate chance for infinite loop */
	lflush();
	usleep(x * 1000);
}
