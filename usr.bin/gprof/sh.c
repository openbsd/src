/*	$OpenBSD: sh.c,v 1.1 2006/10/11 13:34:18 drahn Exp $	*/

#ifndef lint
static char rcsid[] = "$OpenBSD: sh.c,v 1.1 2006/10/11 13:34:18 drahn Exp $";
#endif /* not lint */

#include "gprof.h"

/*
 * gprof -c isn't currently supported...
 */

/* XXX */
void
findcall(nltype *parentp, unsigned long p_lowpc, unsigned long p_highpc)
{
}
