/*	$OpenBSD: pmap.h,v 1.4 2002/03/14 01:26:36 millert Exp $	*/

#include <powerpc/pmap.h>

#ifndef	_LOCORE
paddr_t vtophys(vaddr_t);
#endif	/* _LOCORE */
