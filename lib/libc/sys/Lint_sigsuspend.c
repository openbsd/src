/*	$OpenBSD: Lint_sigsuspend.c,v 1.1 1998/02/08 22:45:14 tholo Exp $	*/
/*	$NetBSD: Lint_sigsuspend.c,v 1.1 1997/11/06 00:53:20 cgd Exp $	*/

/*
 * This file placed in the public domain.
 * Chris Demetriou, November 5, 1997.
 */

#include <signal.h>

/*ARGSUSED*/
int
sigsuspend(sigmask)
	const sigset_t *sigmask;
{
	return (0);
}
