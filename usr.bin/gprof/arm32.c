/*	$OpenBSD: arm32.c,v 1.3 2001/03/22 05:18:30 mickey Exp $	*/
/*	$NetBSD: arm32.c,v 1.1 1996/04/01 21:51:22 mark Exp $	*/

#ifndef lint
static char rcsid[] = "$OpenBSD: arm32.c,v 1.3 2001/03/22 05:18:30 mickey Exp $";
#endif /* not lint */

#include "gprof.h"

/*
 * gprof -c isn't currently supported...
 */
void
findcall( parentp , p_lowpc , p_highpc )
    nltype		*parentp;
    unsigned long	p_lowpc;
    unsigned long	p_highpc;
{
}
