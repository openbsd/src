/*	$OpenBSD: pmap.h,v 1.20 2011/10/09 17:08:22 miod Exp $	*/
/*
 * Mach Operating System
 * Copyright (c) 1991 Carnegie Mellon University
 * Copyright (c) 1991 OMRON Corporation
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 */
#ifndef _M88K_PMAP_H_
#define _M88K_PMAP_H_

#include <machine/mmu.h>

#ifdef	_KERNEL

/*
 * PMAP structure
 */

struct pmap {
	sdt_entry_t		*pm_stab;	/* virtual pointer to sdt */
	apr_t			 pm_apr;
	int			 pm_count;	/* reference count */
	struct pmap_statistics	pm_stats;	/* pmap statistics */
};

/* 	The PV (Physical to virtual) List.
 *
 * For each vm_page_t, pmap keeps a list of all currently valid virtual
 * mappings of that page. An entry is a pv_entry_t; the list is the
 * pv_head_table. This is used by things like pmap_remove, when we must
 * find and remove all mappings for a particular physical page.
 */
/* XXX - struct pv_entry moved to vmparam.h because of include ordering issues */

typedef struct pmap *pmap_t;
typedef struct pv_entry *pv_entry_t;

extern	pmap_t		kernel_pmap;
extern	struct pmap	kernel_pmap_store;
extern	caddr_t		vmmap;
extern	apr_t		default_apr;

#define	pmap_kernel()			(&kernel_pmap_store)
#define pmap_resident_count(pmap)	((pmap)->pm_stats.resident_count)
#define	pmap_wired_count(pmap)		((pmap)->pm_stats.wired_count)

#define pmap_copy(dp,sp,d,l,s)		do { /* nothing */ } while (0)
#if !defined(M88110)
#define pmap_update(pmap)		do { /* nothing */ } while (0)
#endif

#define	pmap_clear_modify(pg)		pmap_unsetbit(pg, PG_M)
#define	pmap_clear_reference(pg)	pmap_unsetbit(pg, PG_U)

void	pmap_bootstrap(paddr_t, paddr_t);
void	pmap_bootstrap_cpu(cpuid_t);
void	pmap_cache_ctrl(vaddr_t, vaddr_t, u_int);
void	pmap_page_uncache(paddr_t);
#define pmap_unuse_final(p)		/* nothing */
#define	pmap_remove_holes(map)		do { /* nothing */ } while (0)
int	pmap_set_modify(pmap_t, vaddr_t);
boolean_t pmap_unsetbit(struct vm_page *, int);

int	pmap_translation_info(pmap_t, vaddr_t, paddr_t *, uint32_t *);
/*
 * pmap_translation_info() return values
 */
#define	PTI_INVALID	0
#define	PTI_PTE		1
#define	PTI_BATC	2

#define	pmap_map_direct(pg)		((vaddr_t)VM_PAGE_TO_PHYS(pg))
#define	pmap_unmap_direct(va)		PHYS_TO_VM_PAGE((paddr_t)va)
#define	__HAVE_PMAP_DIRECT
#define	PMAP_STEAL_MEMORY

#endif	/* _KERNEL */

#endif /* _M88K_PMAP_H_ */
