/*	$OpenBSD: pmap.h,v 1.5 2001/09/10 17:52:06 drahn Exp $	*/

#include <powerpc/pmap.h>

#ifndef	_LOCORE
paddr_t vtophys __P((vaddr_t));
#endif	/* _LOCORE */
