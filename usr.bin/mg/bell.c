/*	$OpenBSD: bell.c,v 1.4 2016/01/03 19:37:08 lum Exp $	*/

/*
 * This file is in the public domain.
 *
 * Author: Mark Lumsden <mark@showcomplex.com>
 *
 */

/*
 * Control how mg communicates with the user.
 */

#include <sys/queue.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

#include "def.h"
#include "macro.h"

void
bellinit(void)
{
	doaudiblebell = 1;
	dovisiblebell = 0;
}

void
dobeep(void)
{
	if (doaudiblebell) {
		ttbeep();
	}
	if (dovisiblebell) {
		sgarbf = TRUE;
		update(CNONE);
		if (inmacro)	/* avoid delaying macro execution. */
			return;
		usleep(50000);
	}
}

/* ARGSUSED */
int
toggleaudiblebell(int f, int n)
{
	if (f & FFARG)
		doaudiblebell = n > 0;
	else
		doaudiblebell = !doaudiblebell;

	return (TRUE);
}

/* ARGSUSED */
int
togglevisiblebell(int f, int n)
{
	if (f & FFARG)
		dovisiblebell = n > 0;
	else
		dovisiblebell = !dovisiblebell;

	return (TRUE);
}
