/*	$OpenBSD: pmap.h,v 1.18 2003/03/01 00:28:48 miod Exp $	*/

#ifndef	_MAC68K_PMAP_H_
#define	_MAC68K_PMAP_H_

#include <m68k/pmap_motorola.h>

#ifdef	_KERNEL

vaddr_t	pmap_map(vaddr_t, paddr_t, paddr_t, int);
void mac68k_set_pte(vaddr_t, paddr_t);

void pmap_init_md(void);
#define	PMAP_INIT_MD()	pmap_init_md()

#endif	/* _KERNEL */

#endif	/* _MAC68K_PMAP_H_ */
