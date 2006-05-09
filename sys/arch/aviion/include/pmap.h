/* $OpenBSD: pmap.h,v 1.1.1.1 2006/05/09 18:32:33 miod Exp $ */
/* public domain */

#ifndef	_AVIION_PMAP_H_
#define	_AVIION_PMAP_H_

#include <m88k/pmap.h>

#ifdef	_KERNEL
vaddr_t	pmap_bootstrap_md(vaddr_t);
#endif

#endif	_AVIION_PMAP_H_
