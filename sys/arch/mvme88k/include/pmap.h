/* $OpenBSD: pmap.h,v 1.36 2004/07/25 11:06:42 miod Exp $ */
/* public domain */

#ifndef	_MVME88K_PMAP_H_
#define	_MVME88K_PMAP_H_

#include <m88k/pmap.h>

#ifdef	_KERNEL
vaddr_t	pmap_bootstrap_md(vaddr_t);
#endif

#endif	_MVME88K_PMAP_H_
