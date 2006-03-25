/*	$OpenBSD: ns32k.c,v 1.4 2006/03/25 19:06:36 espie Exp $	*/
/*	$NetBSD: ns32k.c,v 1.3 1995/04/19 07:16:13 cgd Exp $	*/

#ifndef lint
static char rcsid[] = "$OpenBSD: ns32k.c,v 1.4 2006/03/25 19:06:36 espie Exp $";
#endif /* not lint */

#include "gprof.h"

/*
 * gprof -c isn't currently supported...
 */
void
findcall(nltype *parentp, unsigned long p_lowpc, unsigned long p_highpc)
{
}
