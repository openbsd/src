/*	$OpenBSD: m68k.c,v 1.2 1996/06/26 05:33:54 deraadt Exp $	*/
/*	$NetBSD: m68k.c,v 1.4 1995/04/19 07:16:07 cgd Exp $	*/

#ifndef lint
static char rcsid[] = "$OpenBSD: m68k.c,v 1.2 1996/06/26 05:33:54 deraadt Exp $";
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
