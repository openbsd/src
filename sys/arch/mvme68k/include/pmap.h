/*	$OpenBSD: pmap.h,v 1.9 2001/11/30 20:58:18 miod Exp $	*/

#ifndef	_MACHINE_PMAP_H_
#define	_MACHINE_PMAP_H_

#include <m68k/pmap_motorola.h>

#ifdef	_KERNEL
void pmap_init_md __P((void));
#define	PMAP_INIT_MD()	pmap_init_md()
#endif

#endif	/* _MACHINE_PMAP_H_ */
