/*	$OpenBSD: m88k.c,v 1.2 2001/03/22 05:18:30 mickey Exp $	*/
/*	$NetBSD: m88k.c,v 1.4 1995/04/19 07:16:07 cgd Exp $	*/

#ifndef lint
static char rcsid[] = "$OpenBSD: m88k.c,v 1.2 2001/03/22 05:18:30 mickey Exp $";
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
