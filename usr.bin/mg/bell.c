/*	$OpenBSD: bell.c,v 1.1 2013/05/31 18:03:43 lum Exp $	*/

/*
 * This file is in the public domain.
 *
 * Author: Mark Lumsden <mark@showcomplex.com>
 *
 */

/*
 * Control how mg communicates with the user.
 */

#include "def.h"

void
bellinit(void)
{
	doaudiblebell = 1;
	dovisiblebell = 0;
	donebell = 0;
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
		usleep(50000);
	}
	donebell = 1;
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
