/*	$OpenBSD: alpha.c,v 1.3 2001/03/22 05:18:29 mickey Exp $	*/
/*	$NetBSD: alpha.c,v 1.1 1995/04/19 07:24:19 cgd Exp $	*/

#ifndef lint
static char rcsid[] = "$OpenBSD: alpha.c,v 1.3 2001/03/22 05:18:29 mickey Exp $";
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
