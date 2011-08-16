/*	$OpenBSD: pmap.h,v 1.8 2011/08/16 07:59:45 kettenis Exp $	*/

/*
 * Copyright (c) 2005 Michael Shalayeff
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _MACHINE_PMAP_H_
#define _MACHINE_PMAP_H_

#include <machine/pte.h>
#include <uvm/uvm_page.h>
#include <uvm/uvm_object.h>

#ifdef _KERNEL

struct pmap {
	simple_lock_data_t pm_lock;
	int		pm_refcount;
	struct vm_page	*pm_ptphint;
	struct pglist	pm_pglist;
	volatile u_int32_t *pm_pdir;	/* page dir (read-only after create) */
	pa_space_t	pm_space;	/* space id (read-only after create) */

	struct pmap_statistics	pm_stats;
};
typedef struct pmap *pmap_t;

struct pv_entry {			/* locked by its list's pvh_lock */
	struct pv_entry	*pv_next;
	struct pmap	*pv_pmap;	/* the pmap */
	vaddr_t		pv_va;		/* the virtual address */
	struct vm_page	*pv_ptp;	/* the vm_page of the PTP */
};

extern struct pmap kernel_pmap_store;

/*
 * pool quickmaps
 */
#define	pmap_map_direct(pg)	((vaddr_t)VM_PAGE_TO_PHYS(pg))
struct vm_page *pmap_unmap_direct(vaddr_t);
#define	__HAVE_PMAP_DIRECT

/*
 * according to the parisc manual aliased va's should be
 * different by high 12 bits only.
 */
#define	PMAP_PREFER(o,h)	pmap_prefer(o, h)
static __inline__ vaddr_t
pmap_prefer(vaddr_t offs, vaddr_t hint)
{
	vaddr_t pmap_prefer_hint = (hint & HPPA_PGAMASK) | (offs & HPPA_PGAOFF);
	if (pmap_prefer_hint < hint)
		pmap_prefer_hint += HPPA_PGALIAS;
	return pmap_prefer_hint;
}

/* pmap prefer alignment */
#define PMAP_PREFER_ALIGN()	(HPPA_PGALIAS)
/* pmap prefer offset within alignment */
#define PMAP_PREFER_OFFSET(of)	((of) & HPPA_PGAOFF)

#define	PMAP_GROWKERNEL
#define	PMAP_STEAL_MEMORY

#define	pmap_sid2pid(s)			(((s) + 1) << 1)
#define pmap_kernel()			(&kernel_pmap_store)
#define	pmap_resident_count(pmap)	((pmap)->pm_stats.resident_count)
#define	pmap_update(pm)			(void)(pm)
#define pmap_copy(dpmap,spmap,da,len,sa)

#define pmap_clear_modify(pg)	pmap_changebit(pg, 0, PTE_DIRTY)
#define pmap_clear_reference(pg) pmap_changebit(pg, PTE_REFTRAP, 0)
#define pmap_is_modified(pg)	pmap_testbit(pg, PTE_DIRTY)
#define pmap_is_referenced(pg)	pmap_testbit(pg, PTE_REFTRAP)

#define pmap_unuse_final(p)		/* nothing */
#define	pmap_remove_holes(map)		do { /* nothing */ } while (0)

void pmap_bootstrap(vaddr_t);
boolean_t pmap_changebit(struct vm_page *, pt_entry_t, pt_entry_t);
boolean_t pmap_testbit(struct vm_page *, pt_entry_t);
void pmap_write_protect(struct pmap *, vaddr_t, vaddr_t, vm_prot_t);
void pmap_remove(struct pmap *pmap, vaddr_t sva, vaddr_t eva);
void pmap_page_remove(struct vm_page *pg);

static __inline void
pmap_page_protect(struct vm_page *pg, vm_prot_t prot)
{
	if ((prot & UVM_PROT_WRITE) == 0) {
		if (prot & (UVM_PROT_RX))
			pmap_changebit(pg, 0, PTE_WRITE);
		else
			pmap_page_remove(pg);
	}
}

static __inline void
pmap_protect(struct pmap *pmap, vaddr_t sva, vaddr_t eva, vm_prot_t prot)
{
	if ((prot & UVM_PROT_WRITE) == 0) {
		if (prot & (UVM_PROT_RX))
			pmap_write_protect(pmap, sva, eva, prot);
		else
			pmap_remove(pmap, sva, eva);
	}
}

#endif /* _KERNEL */
#endif /* _MACHINE_PMAP_H_ */
