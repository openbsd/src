/*	$OpenBSD: sh.c,v 1.2 2009/10/27 23:59:38 deraadt Exp $	*/

#include "gprof.h"

/*
 * gprof -c isn't currently supported...
 */

/* XXX */
void
findcall(nltype *parentp, unsigned long p_lowpc, unsigned long p_highpc)
{
}
