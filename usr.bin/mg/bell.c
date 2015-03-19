/*	$OpenBSD: bell.c,v 1.3 2015/03/19 21:22:15 bcallah Exp $	*/

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
