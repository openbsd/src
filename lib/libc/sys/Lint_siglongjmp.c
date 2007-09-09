/*	$OpenBSD: Lint_siglongjmp.c,v 1.1 2007/09/09 19:22:45 otto Exp $	*/

/* Public domain, Otto Moerbeek, 2007 */

#include <setjmp.h>

/*ARGSUSED*/
void
siglongjmp(sigjmp_buf env, int val)
{
}
