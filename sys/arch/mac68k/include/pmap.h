/*	$OpenBSD: pmap.h,v 1.20 2011/03/23 16:54:36 pirofti Exp $	*/

#ifndef	_MACHINE_PMAP_H_
#define	_MACHINE_PMAP_H_

#include <m68k/pmap_motorola.h>

#ifdef	_KERNEL
void pmap_init_md(void);
#define	PMAP_INIT_MD()	pmap_init_md()
#endif	/* _KERNEL */

#endif	/* _MACHINE_PMAP_H_ */
