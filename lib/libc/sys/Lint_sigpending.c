/*	$OpenBSD: Lint_sigpending.c,v 1.1 1998/02/08 22:45:12 tholo Exp $	*/
/*	$NetBSD: Lint_sigpending.c,v 1.1 1997/11/06 00:53:11 cgd Exp $	*/

/*
 * This file placed in the public domain.
 * Chris Demetriou, November 5, 1997.
 */

#include <signal.h>

/*ARGSUSED*/
int
sigpending(set)
	sigset_t *set;
{
	return (0);
}
