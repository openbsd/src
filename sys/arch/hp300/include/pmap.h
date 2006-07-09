/*	$OpenBSD: pmap.h,v 1.15 2006/07/09 19:41:23 miod Exp $	*/

#ifndef	_MACHINE_PMAP_H_
#define	_MACHINE_PMAP_H_

#include <m68k/pmap_motorola.h>

#if !defined(M68020)
#define	pmap_map_direct(pg)	((vaddr_t)VM_PAGE_TO_PHYS(pg))
#define	pmap_unmap_direct(va)	PHYS_TO_VM_PAGE((paddr_t)va)
#define	__HAVE_PMAP_DIRECT
#endif

#ifdef	_KERNEL
void pmap_init_md(void);
#define	PMAP_INIT_MD()	pmap_init_md()
#endif

#endif	/* _MACHINE_PMAP_H_ */
