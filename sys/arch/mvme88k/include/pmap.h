/*	$OpenBSD: pmap.h,v 1.28 2003/01/24 00:51:52 miod Exp $ */
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
#ifndef _MACHINE_PMAP_H_
#define _MACHINE_PMAP_H_

#include <machine/mmu.h>		/* batc_template_t, BATC_MAX, etc.*/
#include <machine/pcb.h>		/* pcb_t, etc.*/
#include <machine/psl.h>		/* get standard goodies		*/

typedef sdt_entry_t *sdt_ptr_t;

/*
 * PMAP structure
 */
typedef struct pmap *pmap_t;

/* #define PMAP_USE_BATC */
struct pmap {
	sdt_ptr_t		sdt_paddr;	/* physical pointer to sdt */
	sdt_ptr_t		sdt_vaddr;	/* virtual pointer to sdt */
	int			ref_count;	/* reference count */
	struct simplelock	lock;
	struct pmap_statistics	stats;		/* pmap statistics */

	/* cpus using of this pmap; NCPU must be <= 32 */
	u_int32_t		cpus_using;

#ifdef	PMAP_USE_BATC
	batc_template_t		i_batc[BATC_MAX];	/* instruction BATCs */
	batc_template_t		d_batc[BATC_MAX];	/* data BATCs */
#endif

#ifdef	DEBUG
	pmap_t			next;
	pmap_t			prev;
#endif
}; 

#define PMAP_NULL ((pmap_t) 0)

/* 	The PV (Physical to virtual) List.
 *
 * For each vm_page_t, pmap keeps a list of all currently valid virtual
 * mappings of that page. An entry is a pv_entry_t; the list is the
 * pv_head_table. This is used by things like pmap_remove, when we must
 * find and remove all mappings for a particular physical page.
 */
typedef  struct pv_entry {
	struct pv_entry	*next;	/* next pv_entry */
	pmap_t		pmap;	/* pmap where mapping lies */
	vaddr_t		va;	/* virtual address for mapping */
} *pv_entry_t;

#ifdef	_KERNEL

extern	pmap_t		kernel_pmap;
extern	struct pmap	kernel_pmap_store;
extern	caddr_t		vmmap;

#define	pmap_kernel()			(&kernel_pmap_store)
#define pmap_resident_count(pmap)	((pmap)->stats.resident_count)
#define	pmap_wired_count(pmap)		((pmap)->stats.wired_count)
#define pmap_phys_address(frame)        ((paddr_t)(ptoa(frame)))

#define pmap_update(pmap)	/* nothing (yet) */

void pmap_bootstrap(vaddr_t, paddr_t *, paddr_t *, vaddr_t *, vaddr_t *);
void pmap_cache_ctrl(pmap_t, vaddr_t, vaddr_t, u_int);

#endif	/* _KERNEL */

#endif /* _MACHINE_PMAP_H_ */
