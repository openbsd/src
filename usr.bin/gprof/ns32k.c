/*	$OpenBSD: ns32k.c,v 1.5 2009/10/27 23:59:38 deraadt Exp $	*/
/*	$NetBSD: ns32k.c,v 1.3 1995/04/19 07:16:13 cgd Exp $	*/

#include "gprof.h"

/*
 * gprof -c isn't currently supported...
 */
void
findcall(nltype *parentp, unsigned long p_lowpc, unsigned long p_highpc)
{
}
