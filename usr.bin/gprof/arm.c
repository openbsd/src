/*	$OpenBSD: arm.c,v 1.2 2006/03/25 19:06:35 espie Exp $	*/
/*	$NetBSD: arm32.c,v 1.1 1996/04/01 21:51:22 mark Exp $	*/

#ifndef lint
static char rcsid[] = "$OpenBSD: arm.c,v 1.2 2006/03/25 19:06:35 espie Exp $";
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
