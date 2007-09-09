/*	$OpenBSD: Lint_sigsetjmp.c,v 1.1 2007/09/09 19:22:45 otto Exp $	*/

/* Public domain, Otto Moerbeek, 2007 */

#include <setjmp.h>

/*ARGSUSED*/
int
sigsetjmp(sigjmp_buf env, int savemask)
{
	return 0;
}
