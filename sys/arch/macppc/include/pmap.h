/*	$OpenBSD: pmap.h,v 1.3 2001/09/10 17:48:09 drahn Exp $	*/

#include <powerpc/pmap.h>

#ifndef	_LOCORE
paddr_t vtophys __P((vaddr_t));
#endif	/* _LOCORE */
