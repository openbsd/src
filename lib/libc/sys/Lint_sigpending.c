/*	$OpenBSD: Lint_sigpending.c,v 1.2 2004/09/14 22:18:56 deraadt Exp $	*/
/*	$NetBSD: Lint_sigpending.c,v 1.1 1997/11/06 00:53:11 cgd Exp $	*/

/*
 * This file placed in the public domain.
 * Chris Demetriou, November 5, 1997.
 */

#include <signal.h>

/*ARGSUSED*/
int
sigpending(sigset_t *set)
{
	return (0);
}
