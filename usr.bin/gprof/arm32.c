/*	$NetBSD: arm32.c,v 1.1 1996/04/01 21:51:22 mark Exp $	*/

#ifndef lint
static char rcsid[] = "$NetBSD: arm32.c,v 1.1 1996/04/01 21:51:22 mark Exp $";
#endif /* not lint */

#include "gprof.h"

/*
 * gprof -c isn't currently supported...
 */
findcall( parentp , p_lowpc , p_highpc )
    nltype		*parentp;
    unsigned long	p_lowpc;
    unsigned long	p_highpc;
{
}
