/*	$OpenBSD: alpha.c,v 1.2 1996/06/26 05:33:46 deraadt Exp $	*/
/*	$NetBSD: alpha.c,v 1.1 1995/04/19 07:24:19 cgd Exp $	*/

#ifndef lint
static char rcsid[] = "$OpenBSD: alpha.c,v 1.2 1996/06/26 05:33:46 deraadt Exp $";
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
