/*	$OpenBSD: pmap.h,v 1.4 1996/10/30 22:39:16 niklas Exp $	*/
/*	$NetBSD: pmap.h,v 1.9 1996/08/20 23:02:30 cgd Exp $	*/

#ifndef NEW_PMAP
#include <machine/pmap.old.h>
#else
#include <machine/pmap.new.h>
#endif

void pmap_unmap_prom __P((void));
