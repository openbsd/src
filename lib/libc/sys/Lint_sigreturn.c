/*	$OpenBSD: Lint_sigreturn.c,v 1.2 2004/09/14 22:18:56 deraadt Exp $	*/
/*	$NetBSD: Lint_sigreturn.c,v 1.1 1997/11/06 00:53:18 cgd Exp $	*/

/*
 * This file placed in the public domain.
 * Chris Demetriou, November 5, 1997.
 */

#include <signal.h>

/*ARGSUSED*/
int
sigreturn(struct sigcontext *scp)
{
	return (0);
}
