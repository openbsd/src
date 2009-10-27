/*	$OpenBSD: m88k.c,v 1.4 2009/10/27 23:59:38 deraadt Exp $	*/
/*	$NetBSD: m88k.c,v 1.4 1995/04/19 07:16:07 cgd Exp $	*/

#include "gprof.h"

/*
 * gprof -c isn't currently supported...
 */
void
findcall(nltype *parentp, unsigned long p_lowpc, unsigned long p_highpc)
{
}
