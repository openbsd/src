/*	$OpenBSD: ns32k.c,v 1.2 1996/06/26 05:33:56 deraadt Exp $	*/
/*	$NetBSD: ns32k.c,v 1.3 1995/04/19 07:16:13 cgd Exp $	*/

#ifndef lint
static char rcsid[] = "$OpenBSD: ns32k.c,v 1.2 1996/06/26 05:33:56 deraadt Exp $";
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
