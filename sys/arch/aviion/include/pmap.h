/* $OpenBSD: pmap.h,v 1.2 2007/12/19 21:51:29 miod Exp $ */
/* public domain */

#ifndef	_AVIION_PMAP_H_
#define	_AVIION_PMAP_H_

#include <m88k/pmap.h>

#ifdef	_KERNEL
#define	pmap_bootstrap_md(va)		(va)
#endif

#endif	_AVIION_PMAP_H_
