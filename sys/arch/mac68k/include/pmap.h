/*	$OpenBSD: pmap.h,v 1.19 2005/04/27 00:12:43 miod Exp $	*/

#ifndef	_MAC68K_PMAP_H_
#define	_MAC68K_PMAP_H_

#include <m68k/pmap_motorola.h>

#ifdef	_KERNEL
void pmap_init_md(void);
#define	PMAP_INIT_MD()	pmap_init_md()
#endif	/* _KERNEL */

#endif	/* _MAC68K_PMAP_H_ */
