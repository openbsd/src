/*	$OpenBSD: Lint_sigprocmask.c,v 1.1 1998/02/08 22:45:13 tholo Exp $	*/
/*	$NetBSD: Lint_sigprocmask.c,v 1.1 1997/11/06 00:53:15 cgd Exp $	*/

/*
 * This file placed in the public domain.
 * Chris Demetriou, November 5, 1997.
 */

#include <signal.h>

/*ARGSUSED*/
int
sigprocmask(how, set, oset)
	int how;
	const sigset_t *set;
	sigset_t *oset;
{
	return (0);
}
