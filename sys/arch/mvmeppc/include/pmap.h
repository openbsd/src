/*	$OpenBSD: pmap.h,v 1.6 2002/03/14 01:26:41 millert Exp $	*/

#include <powerpc/pmap.h>

#ifndef	_LOCORE
paddr_t vtophys(vaddr_t);
#endif	/* _LOCORE */
