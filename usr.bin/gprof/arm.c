/*	$OpenBSD: arm.c,v 1.3 2009/10/27 23:59:38 deraadt Exp $	*/
/*	$NetBSD: arm32.c,v 1.1 1996/04/01 21:51:22 mark Exp $	*/

#include "gprof.h"

/*
 * gprof -c isn't currently supported...
 */

/* XXX */
void
findcall(nltype *parentp, unsigned long p_lowpc, unsigned long p_highpc)
{
}
