/*	$NetBSD: ns32k.c,v 1.3 1995/04/19 07:16:13 cgd Exp $	*/

#ifndef lint
static char rcsid[] = "$NetBSD: ns32k.c,v 1.3 1995/04/19 07:16:13 cgd Exp $";
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
