/*	$OpenBSD: pmap.h,v 1.17 2002/03/14 01:26:35 millert Exp $	*/

#ifndef	_MAC68K_PMAP_H_
#define	_MAC68K_PMAP_H_

#include <m68k/pmap_motorola.h>

#ifdef	_KERNEL

void mac68k_set_pte(vaddr_t va, paddr_t pge);

void pmap_init_md(void);
#define	PMAP_INIT_MD()	pmap_init_md()

#endif	/* _KERNEL */

#endif	/* _MAC68K_PMAP_H_ */
