/* $OpenBSD: pmap.c,v 1.16 2017/02/05 13:08:03 patrick Exp $ */
/*
 * Copyright (c) 2008-2009,2014-2016 Dale Rahn <drahn@dalerahn.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/systm.h>
#include <sys/pool.h>
#include <sys/atomic.h>

#include <uvm/uvm.h>

#include "arm64/vmparam.h"
#include "arm64/pmap.h"
#include "machine/pcb.h"

#include <machine/db_machdep.h>
#include <ddb/db_extern.h>
#include <ddb/db_output.h>

#if 0
#define STATIC /* static */
#define __inline /* inline */
#else
#define STATIC static
#define __inline inline

#endif

void pmap_setttb(struct proc *p, paddr_t pcb_pagedir, struct pcb *);
void arm64_tlbi_asid(vaddr_t va, int asid);
void pmap_free_asid(pmap_t pm);

// using PHYS_TO_VM_PAGE does not do what is desired, so instead of
// using that API, create our own which will store ranges of memory
// and default to caching those pages when mapped

struct {
	uint64_t start;
	uint64_t end;
} pmap_memregions[8];

int pmap_memcount = 0;

int
pmap_pa_is_mem(uint64_t pa)
{
	int i;
	// NOTE THIS REQUIRES TABLE TO BE SORTED
	for (i = 0; i < pmap_memcount; i++) {
		if (pa < pmap_memregions[i].start)
			return 0;
		if (pa < pmap_memregions[i].end)
			return 1;
	}
	return 0;
}

unsigned int
dcache_line_size(void)
{
	uint64_t ctr;
	unsigned int dcl_size;

	/* Accessible from all security levels */
	ctr = READ_SPECIALREG(ctr_el0);

	/*
	 * Relevant field [19:16] is LOG2
	 * of the number of words in DCache line
	 */
	dcl_size = CTR_DLINE_SIZE(ctr);

	/* Size of word shifted by cache line size */
	return (sizeof(int) << dcl_size);
}

/* Write back D-cache to PoC */
void
dcache_wb_poc(vaddr_t addr, vsize_t len)
{
	uint64_t cl_size;
	vaddr_t end;

	cl_size = dcache_line_size();

	/* Calculate end address to clean */
	end = addr + len;
	/* Align start address to cache line */
	addr = addr & ~(cl_size - 1);

	for (; addr < end; addr += cl_size)
		__asm __volatile("dc cvac, %x0" :: "r" (addr) : "memory");
	__asm __volatile("dsb ish");
}

#if 0
/* Write back and invalidate D-cache to PoC */
STATIC __inline void
dcache_wbinv_poc(vaddr_t sva, paddr_t pa, vsize_t size)
{
	// XXX needed?
	for (off = 0; off <size; off += CACHE_LINE_SIZE)
		__asm __volatile("dc CVAC,%0"::"r"(va+off));
}
#endif

STATIC __inline void
ttlb_flush(pmap_t pm, vaddr_t va)
{
	arm64_tlbi_asid(va, pm->pm_asid);
}

STATIC __inline void
ttlb_flush_range(pmap_t pm, vaddr_t va, vsize_t size)
{
	vaddr_t eva = va + size;

	__asm __volatile("dsb sy");
	// if size is over 512 pages, just flush the entire cache !?!?!
	if (size >= (512 * PAGE_SIZE)) {
		__asm __volatile("tlbi	vmalle1is");
		return ;
	}

	for ( ; va < eva; va += PAGE_SIZE)
		arm64_tlbi_asid(va, pm->pm_asid);
	__asm __volatile("dsb sy");
}

void
arm64_tlbi_asid(vaddr_t va, int asid)
{
	vaddr_t resva;
	if (asid == -1) {
		resva = ((va>>PAGE_SHIFT) & (1ULL << 44) -1) ;
		__asm volatile ("TLBI VAALE1IS, %x0":: "r"(resva));
		return;
	}
        resva = ((va >> PAGE_SHIFT) & (1ULL << 44) -1) |
	    ((unsigned long long)asid << 48);
	__asm volatile ("TLBI VAE1IS, %x0" :: "r"(resva));
}

struct pmap kernel_pmap_;

LIST_HEAD(pted_pv_head, pte_desc);

struct pte_desc {
	LIST_ENTRY(pte_desc) pted_pv_list;
	uint64_t pted_pte;
	pmap_t pted_pmap;
	vaddr_t pted_va;
};

/* VP routines */
int pmap_vp_enter(pmap_t pm, vaddr_t va, struct pte_desc *pted, int flags);
struct pte_desc *pmap_vp_remove(pmap_t pm, vaddr_t va);
void pmap_vp_destroy(pmap_t pm);
struct pte_desc *pmap_vp_lookup(pmap_t pm, vaddr_t va, uint64_t **);

/* PV routines */
void pmap_enter_pv(struct pte_desc *pted, struct vm_page *);
void pmap_remove_pv(struct pte_desc *pted);

void _pmap_kenter_pa(vaddr_t va, paddr_t pa, vm_prot_t prot, int flags,
    int cache);

void pmap_allocate_asid(pmap_t);

struct pmapvp0 {
	uint64_t l0[VP_IDX0_CNT];
	struct pmapvp1 *vp[VP_IDX0_CNT];
};

struct pmapvp1 {
	uint64_t l1[VP_IDX1_CNT];
	struct pmapvp2 *vp[VP_IDX1_CNT];
};

struct pmapvp2 {
	uint64_t l2[VP_IDX2_CNT];
	struct pmapvp3 *vp[VP_IDX2_CNT];
};

struct pmapvp3 {
	uint64_t l3[VP_IDX3_CNT];
	struct pte_desc *vp[VP_IDX3_CNT];
};
CTASSERT(sizeof(struct pmapvp0) == sizeof(struct pmapvp1));
CTASSERT(sizeof(struct pmapvp0) == sizeof(struct pmapvp2));
CTASSERT(sizeof(struct pmapvp0) == sizeof(struct pmapvp3));


void pmap_remove_pted(pmap_t pm, struct pte_desc *pted);
void pmap_kremove_pg(vaddr_t va);
void pmap_set_l1(struct pmap *pm, uint64_t va, struct pmapvp1 *l1_va, paddr_t l1_pa);
void pmap_set_l2(struct pmap *pm, uint64_t va, struct pmapvp2 *l2_va, paddr_t l2_pa);
void pmap_set_l3(struct pmap *pm, uint64_t va, struct pmapvp3 *l3_va, paddr_t l3_pa);


/* XXX */
void
pmap_fill_pte(pmap_t pm, vaddr_t va, paddr_t pa, struct pte_desc *pted,
    vm_prot_t prot, int flags, int cache);
void pmap_pte_insert(struct pte_desc *pted);
void pmap_pte_remove(struct pte_desc *pted, int);
void pmap_pte_update(struct pte_desc *pted, uint64_t *pl3);
void pmap_kenter_cache(vaddr_t va, paddr_t pa, vm_prot_t prot, int cacheable);
void pmap_pinit(pmap_t pm);
void pmap_release(pmap_t pm);
//vaddr_t pmap_steal_memory(vsize_t size, vaddr_t *start, vaddr_t *end);
paddr_t arm_kvm_stolen;
paddr_t pmap_steal_avail(size_t size, int align, void **kva);
void pmap_remove_avail(paddr_t base, paddr_t end);
void pmap_avail_fixup(void);
vaddr_t pmap_map_stolen(vaddr_t);
void pmap_physload_avail(void);
extern caddr_t msgbufaddr;


/* XXX - panic on pool get failures? */
struct pool pmap_pmap_pool;
struct pool pmap_pted_pool;
struct pool pmap_vp_pool;

/* list of L1 tables */

int pmap_initialized = 0;

struct mem_region {
	vaddr_t start;
	vsize_t size;
};

struct mem_region pmap_avail_regions[10];
struct mem_region pmap_allocated_regions[10];
struct mem_region *pmap_avail = &pmap_avail_regions[0];
struct mem_region *pmap_allocated = &pmap_allocated_regions[0];
int pmap_cnt_avail, pmap_cnt_allocated;
uint64_t pmap_avail_kvo;


/* virtual to physical helpers */
STATIC __inline int
VP_IDX0(vaddr_t va)
{
	return (va >> VP_IDX0_POS) & VP_IDX0_MASK;
}

STATIC __inline int
VP_IDX1(vaddr_t va)
{
	return (va >> VP_IDX1_POS) & VP_IDX1_MASK;
}

STATIC __inline int
VP_IDX2(vaddr_t va)
{
	return (va >> VP_IDX2_POS) & VP_IDX2_MASK;
}

STATIC __inline int
VP_IDX3(vaddr_t va)
{
	return (va >> VP_IDX3_POS) & VP_IDX3_MASK;
}

#if 0
STATIC __inline vaddr_t
VP_IDXTOVA(int kern, int idx0, int idx1, int idx2, int idx3,
    int mask)
{
	vaddr_t va = 0;
	if (kern)
		va |= 0xffffff8000000000ULL;
	va |= (long)(idx0 & VP_IDX0_MASK) << VP_IDX0_POS;
	va |= (long)(idx1 & VP_IDX1_MASK) << VP_IDX1_POS;
	va |= (long)(idx2 & VP_IDX2_MASK) << VP_IDX2_POS;
	va |= (long)(idx3 & VP_IDX3_MASK) << VP_IDX3_POS;
	va |= mask & 0xfff; // page offset;

	return va;
}
#endif

const struct kmem_pa_mode kp_lN = {
	.kp_constraint = &no_constraint,
	.kp_maxseg = 1,
	.kp_align = 4096,
	.kp_zero = 1,
};


/*
 * This is used for pmap_kernel() mappings, they are not to be removed
 * from the vp table because they were statically initialized at the
 * initial pmap initialization. This is so that memory allocation
 * is not necessary in the pmap_kernel() mappings.
 * Otherwise bad race conditions can appear.
 */
struct pte_desc *
pmap_vp_lookup(pmap_t pm, vaddr_t va, uint64_t **pl3entry)
{
	struct pmapvp1 *vp1;
	struct pmapvp2 *vp2;
	struct pmapvp3 *vp3;
	struct pte_desc *pted;

	if (pm->have_4_level_pt) {
		if (pm->pm_vp.l0 == NULL) {
			return NULL;
		}
		vp1 = pm->pm_vp.l0->vp[VP_IDX0(va)];
	} else {
		vp1 = pm->pm_vp.l1;
	}
	if (vp1 == NULL) {
		return NULL;
	}

	vp2 = vp1->vp[VP_IDX1(va)];
	if (vp2 == NULL) {
		return NULL;
	}

	vp3 = vp2->vp[VP_IDX2(va)];
	if (vp3 == NULL) {
		return NULL;
	}

	pted = vp3->vp[VP_IDX3(va)];
	if (pl3entry != NULL)
		*pl3entry = &(vp3->l3[VP_IDX3(va)]);

	return pted;
}

/*
 * Remove, and return, pted at specified address, NULL if not present
 */
struct pte_desc *
pmap_vp_remove(pmap_t pm, vaddr_t va)
{
	struct pmapvp1 *vp1;
	struct pmapvp2 *vp2;
	struct pmapvp3 *vp3;
	struct pte_desc *pted;

	if (pm->have_4_level_pt) {
		vp1 = pm->pm_vp.l0->vp[VP_IDX0(va)];
		if (vp1 == NULL) {
			return NULL;
		}
	} else {
		vp1 = pm->pm_vp.l1;
	}

	vp2 = vp1->vp[VP_IDX1(va)];
	if (vp2 == NULL) {
		return NULL;
	}

	vp3 = vp2->vp[VP_IDX2(va)];
	if (vp3 == NULL) {
		return NULL;
	}

	pted = vp3->vp[VP_IDX3(va)];
	vp3->vp[VP_IDX3(va)] = NULL;

	return pted;
}


/*
 * Create a V -> P mapping for the given pmap and virtual address
 * with reference to the pte descriptor that is used to map the page.
 * This code should track allocations of vp table allocations
 * so they can be freed efficiently.
 *
 * XXX it may be possible to save some bits of count in the
 * upper address bits of the pa or the pte entry.
 * However that does make populating the other bits more tricky.
 * each level has 512 entries, so that mean 9 bits to store
 * stash 3 bits each in the first 3 entries?
 */
int
pmap_vp_enter(pmap_t pm, vaddr_t va, struct pte_desc *pted, int flags)
{
	struct pmapvp1 *vp1;
	struct pmapvp2 *vp2;
	struct pmapvp3 *vp3;

	int vp_pool_flags;
	if (pm == pmap_kernel()) {
		vp_pool_flags  = PR_NOWAIT;
	} else {
		vp_pool_flags  = PR_WAITOK |PR_ZERO;
	}

	if (pm->have_4_level_pt) {
		vp1 = pm->pm_vp.l0->vp[VP_IDX0(va)];
		if (vp1 == NULL) {
			vp1 = pool_get(&pmap_vp_pool, vp_pool_flags);
			if (vp1 == NULL) {
				if ((flags & PMAP_CANFAIL) == 0)
					return ENOMEM;
				panic("unable to allocate L1");
			}
			pmap_set_l1(pm, va, vp1, 0);
		}
	} else {
		vp1 = pm->pm_vp.l1;
	}

	vp2 = vp1->vp[VP_IDX1(va)];
	if (vp2 == NULL) {
		vp2 = pool_get(&pmap_vp_pool, vp_pool_flags);
		if (vp2 == NULL) {
			if ((flags & PMAP_CANFAIL) == 0)
				return ENOMEM;
			panic("unable to allocate L2");
		}
		pmap_set_l2(pm, va, vp2, 0);
	}

	vp3 = vp2->vp[VP_IDX2(va)];
	if (vp3 == NULL) {
		vp3 = pool_get(&pmap_vp_pool, vp_pool_flags);
		if (vp3 == NULL) {
			if ((flags & PMAP_CANFAIL) == 0)
				return ENOMEM;
			panic("unable to allocate L3");
		}
		pmap_set_l3(pm, va, vp3, 0);
	}

	vp3->vp[VP_IDX3(va)] = pted;
	return 0;
}

u_int32_t PTED_MANAGED(struct pte_desc *pted);
u_int32_t PTED_WIRED(struct pte_desc *pted);
u_int32_t PTED_VALID(struct pte_desc *pted);

u_int32_t
PTED_MANAGED(struct pte_desc *pted)
{
	return (pted->pted_va & PTED_VA_MANAGED_M);
}

u_int32_t
PTED_WIRED(struct pte_desc *pted)
{
	return (pted->pted_va & PTED_VA_WIRED_M);
}

u_int32_t
PTED_VALID(struct pte_desc *pted)
{
	return (pted->pted_pte != 0);
}

/*
 * PV entries -
 * manipulate the physical to virtual translations for the entire system.
 *
 * QUESTION: should all mapped memory be stored in PV tables? Or
 * is it alright to only store "ram" memory. Currently device mappings
 * are not stored.
 * It makes sense to pre-allocate mappings for all of "ram" memory, since
 * it is likely that it will be mapped at some point, but would it also
 * make sense to use a tree/table like is use for pmap to store device
 * mappings?
 * Further notes: It seems that the PV table is only used for pmap_protect
 * and other paging related operations. Given this, it is not necessary
 * to store any pmap_kernel() entries in PV tables and does not make
 * sense to store device mappings in PV either.
 *
 * Note: unlike other powerpc pmap designs, the array is only an array
 * of pointers. Since the same structure is used for holding information
 * in the VP table, the PV table, and for kernel mappings, the wired entries.
 * Allocate one data structure to hold all of the info, instead of replicating
 * it multiple times.
 *
 * One issue of making this a single data structure is that two pointers are
 * wasted for every page which does not map ram (device mappings), this
 * should be a low percentage of mapped pages in the system, so should not
 * have too noticable unnecessary ram consumption.
 */

void
pmap_enter_pv(struct pte_desc *pted, struct vm_page *pg)
{
	// XXX does this test mean that some pages try to be managed,
	// but this is called too soon?
	if (__predict_false(!pmap_initialized)) {
		return;
	}

	LIST_INSERT_HEAD(&(pg->mdpage.pv_list), pted, pted_pv_list);
	pted->pted_va |= PTED_VA_MANAGED_M;
}

void
pmap_remove_pv(struct pte_desc *pted)
{
	LIST_REMOVE(pted, pted_pv_list);
}

volatile int supportuserland;

int
pmap_enter(pmap_t pm, vaddr_t va, paddr_t pa, vm_prot_t prot, int flags)
{
	struct pte_desc *pted;
	struct vm_page *pg;
	int s;
	int need_sync = 0;
	int cache;
	int error;

	//if (!cold) printf("%s: %x %x %x %x %x %x\n", __func__, va, pa, prot, flags, pm, pmap_kernel());

	/* MP - Acquire lock for this pmap */

	s = splvm();
	pted = pmap_vp_lookup(pm, va, NULL);
	if (pted && PTED_VALID(pted)) {
		pmap_remove_pted(pm, pted);
		/* we lost our pted if it was user */
		if (pm != pmap_kernel())
			pted = pmap_vp_lookup(pm, va, NULL);
	}

	pm->pm_stats.resident_count++;

	/* Do not have pted for this, get one and put it in VP */
	if (pted == NULL) {
		pted = pool_get(&pmap_pted_pool, PR_NOWAIT | PR_ZERO);
		if (pted == NULL) {
			if ((flags & PMAP_CANFAIL) == 0) {
				error = ENOMEM;
				goto out;
			}
			panic("pmap_enter: failed to allocate pted");
		}
		if (pmap_vp_enter(pm, va, pted, flags)) {
			if ((flags & PMAP_CANFAIL) == 0) {
				error = ENOMEM;
				pool_put(&pmap_pted_pool, pted);
				goto out;
			}
			panic("pmap_enter: failed to allocate L2/L3");
		}
	}

	/* Calculate PTE */
	if (pmap_pa_is_mem(pa)) {
		pg = PHYS_TO_VM_PAGE(pa);
		/* max cacheable */
		cache = PMAP_CACHE_WB; /* managed memory is cacheable */
	} else {
		pg = NULL;
		cache = PMAP_CACHE_CI;
	}

	/*
	 * If it should be enabled _right now_, we can skip doing ref/mod
	 * emulation. Any access includes reference, modified only by write.
	 */
	if (pg != NULL &&
	    ((flags & PROT_MASK) || (pg->pg_flags & PG_PMAP_REF))) {
		pg->pg_flags |= PG_PMAP_REF;
		if ((prot & PROT_WRITE) && (flags & PROT_WRITE)) {
			pg->pg_flags |= PG_PMAP_MOD;
		}
	}

	pmap_fill_pte(pm, va, pa, pted, prot, flags, cache);

	if (pg != NULL) {
		pmap_enter_pv(pted, pg); /* only managed mem */
	}

	/*
	 * Insert into table, if this mapping said it needed to be mapped
	 * now.
	 */
	if (flags & (PROT_READ|PROT_WRITE|PROT_EXEC|PMAP_WIRED)) {
		pmap_pte_insert(pted);
	}

//	cpu_dcache_inv_range(va & ~PAGE_MASK, PAGE_SIZE);
//	cpu_sdcache_inv_range(va & ~PAGE_MASK, pa & ~PAGE_MASK, PAGE_SIZE);
//	cpu_drain_writebuf();
	ttlb_flush(pm, va & ~PAGE_MASK);

	if (prot & PROT_EXEC) {
		if (pg != NULL) {
			need_sync = ((pg->pg_flags & PG_PMAP_EXE) == 0);
			atomic_setbits_int(&pg->pg_flags, PG_PMAP_EXE);
		} else
			need_sync = 1;
	} else {
		/*
		 * Should we be paranoid about writeable non-exec
		 * mappings ? if so, clear the exec tag
		 */
		if ((prot & PROT_WRITE) && (pg != NULL))
			atomic_clearbits_int(&pg->pg_flags, PG_PMAP_EXE);
	}

#if 0
	/* only instruction sync executable pages */
	if (need_sync)
		pmap_syncicache_user_virt(pm, va);
#endif

	error = 0;
out:
	splx(s);
	/* MP - free pmap lock */
	return error;
}


/*
 * Remove the given range of mapping entries.
 */
void
pmap_remove(pmap_t pm, vaddr_t sva, vaddr_t eva)
{
	struct pte_desc *pted;
	vaddr_t va;

	for (va = sva; va < eva; va += PAGE_SIZE) {
		pted = pmap_vp_lookup(pm, va, NULL);

		if (pted == NULL)
			continue;

		if (pted->pted_va & PTED_VA_WIRED_M) {
			pm->pm_stats.wired_count--;
			pted->pted_va &= ~PTED_VA_WIRED_M;
		}

		if (PTED_VALID(pted))
			pmap_remove_pted(pm, pted);
	}
}

/*
 * remove a single mapping, notice that this code is O(1)
 */
void
pmap_remove_pted(pmap_t pm, struct pte_desc *pted)
{
	int s;

	s = splvm();
	pm->pm_stats.resident_count--;

	if (pted->pted_va & PTED_VA_WIRED_M) {
		pm->pm_stats.wired_count--;
		pted->pted_va &= ~PTED_VA_WIRED_M;
	}

	__asm __volatile("dsb sy");
	//dcache_wbinv_poc(va & ~PAGE_MASK, pted->pted_pte & PTE_RPGN, PAGE_SIZE);

	pmap_pte_remove(pted, pm != pmap_kernel());

	ttlb_flush(pm, pted->pted_va & ~PAGE_MASK);

	if (pted->pted_va & PTED_VA_EXEC_M) {
		pted->pted_va &= ~PTED_VA_EXEC_M;
	}

	pted->pted_pte = 0;

	if (PTED_MANAGED(pted))
		pmap_remove_pv(pted);

	if (pm != pmap_kernel())
		pool_put(&pmap_pted_pool, pted);
	splx(s);
}


/*
 * Enter a kernel mapping for the given page.
 * kernel mappings have a larger set of prerequisites than normal mappings.
 *
 * 1. no memory should be allocated to create a kernel mapping.
 * 2. a vp mapping should already exist, even if invalid. (see 1)
 * 3. all vp tree mappings should already exist (see 1)
 *
 */
void
_pmap_kenter_pa(vaddr_t va, paddr_t pa, vm_prot_t prot, int flags, int cache)
{
	struct pte_desc *pted;
	int s;
	pmap_t pm;

	//if (!cold) printf("%s: %x %x %x %x %x\n", __func__, va, pa, prot, flags, cache);

	pm = pmap_kernel();

	/* MP - lock pmap. */
	s = splvm();

	pted = pmap_vp_lookup(pm, va, NULL);

	/* Do not have pted for this, get one and put it in VP */
	if (pted == NULL) {
		panic("pted not preallocated in pmap_kernel() va %lx pa %lx\n",
		    va, pa);
	}

	if (pted && PTED_VALID(pted))
		pmap_kremove_pg(va); /* pted is reused */

	pm->pm_stats.resident_count++;

	if (cache == PMAP_CACHE_DEFAULT) {
		if (pmap_pa_is_mem(pa)) {
			/* MAXIMUM cacheability */
			cache = PMAP_CACHE_WB; /* managed memory is cacheable */
		} else {
			printf("entering page unmapped %llx %llx\n", va, pa);
			cache = PMAP_CACHE_CI;
		}
	}

	flags |= PMAP_WIRED; /* kernel mappings are always wired. */
	/* Calculate PTE */
	pmap_fill_pte(pm, va, pa, pted, prot, flags, cache);

	/*
	 * Insert into table
	 * We were told to map the page, probably called from vm_fault,
	 * so map the page!
	 */
	pmap_pte_insert(pted);

	ttlb_flush(pm, va & ~PAGE_MASK);

	splx(s);
}

void
pmap_kenter_pa(vaddr_t va, paddr_t pa, vm_prot_t prot)
{
	_pmap_kenter_pa(va, pa, prot, prot, PMAP_CACHE_DEFAULT);
}

void
pmap_kenter_cache(vaddr_t va, paddr_t pa, vm_prot_t prot, int cacheable)
{
	_pmap_kenter_pa(va, pa, prot, prot, cacheable);
}

/*
 * remove kernel (pmap_kernel()) mapping, one page
 */
void
pmap_kremove_pg(vaddr_t va)
{
	struct pte_desc *pted;
	pmap_t pm;
	int s;

	//if (!cold) printf("%s: %x\n", __func__, va);

	pm = pmap_kernel();
	pted = pmap_vp_lookup(pm, va, NULL);
	if (pted == NULL)
		return;

	if (!PTED_VALID(pted))
		return; /* not mapped */

	s = splvm();

	pm->pm_stats.resident_count--;

	/*
	 * Table needs to be locked here as well as pmap, and pv list.
	 * so that we know the mapping information is either valid,
	 * or that the mapping is not present in the hash table.
	 */
	pmap_pte_remove(pted, 0);

	ttlb_flush(pm, pted->pted_va & ~PAGE_MASK);

	if (pted->pted_va & PTED_VA_EXEC_M)
		pted->pted_va &= ~PTED_VA_EXEC_M;

	if (PTED_MANAGED(pted))
		pmap_remove_pv(pted);

	if (pted->pted_va & PTED_VA_WIRED_M)
		pm->pm_stats.wired_count--;

	/* invalidate pted; */
	pted->pted_pte = 0;
	pted->pted_va = 0;

	splx(s);
}

/*
 * remove kernel (pmap_kernel()) mappings
 */
void
pmap_kremove(vaddr_t va, vsize_t len)
{
	//if (!cold) printf("%s: %x %x\n", __func__, va, len);
	for (len >>= PAGE_SHIFT; len >0; len--, va += PAGE_SIZE)
		pmap_kremove_pg(va);
}


void
pmap_fill_pte(pmap_t pm, vaddr_t va, paddr_t pa, struct pte_desc *pted,
    vm_prot_t prot, int flags, int cache)
{
	pted->pted_va = va;
	pted->pted_pmap = pm;

	switch (cache) {
	case PMAP_CACHE_WB:
		break;
	case PMAP_CACHE_WT:
		break;
	case PMAP_CACHE_CI:
		break;
	case PMAP_CACHE_PTE:
		break;
	case PMAP_CACHE_DEV:
		break;
	default:
		panic("pmap_fill_pte:invalid cache mode");
	}
	pted->pted_va |= cache;

	pted->pted_va |= prot & (PROT_READ|PROT_WRITE|PROT_EXEC);

	if (flags & PMAP_WIRED) {
		pted->pted_va |= PTED_VA_WIRED_M;
		pm->pm_stats.wired_count++;
	}

	pted->pted_pte = pa & PTE_RPGN;
	pted->pted_pte |= flags & (PROT_READ|PROT_WRITE|PROT_EXEC);
}


/*
 * Garbage collects the physical map system for pages which are
 * no longer used. Success need not be guaranteed -- that is, there
 * may well be pages which are not referenced, but others may be collected
 * Called by the pageout daemon when pages are scarce.
 */
void
pmap_collect(pmap_t pm)
{
	/* This could return unused v->p table layers which
	 * are empty.
	 * could malicious programs allocate memory and eat
	 * these wired pages? These are allocated via pool.
	 * Are there pool functions which could be called
	 * to lower the pool usage here?
	 */
}

/*
 * Fill the given physical page with zeros.
 * SMP: need multiple zero pages, one for each cpu.
 * XXX
 */
void
pmap_zero_page(struct vm_page *pg)
{
	//printf("%s\n", __func__);
	paddr_t pa = VM_PAGE_TO_PHYS(pg);

	/* simple_lock(&pmap_zero_page_lock); */
	pmap_kenter_pa(zero_page, pa, PROT_READ|PROT_WRITE);

	// XXX use better zero operation?
	bzero((void *)zero_page, PAGE_SIZE);

	// XXX better way to unmap this?
	pmap_kremove_pg(zero_page);
}

/*
 * copy the given physical page with zeros.
 * SMP: need multiple copy va, one set for each cpu.
 */
void
pmap_copy_page(struct vm_page *srcpg, struct vm_page *dstpg)
{
	//printf("%s\n", __func__);
	paddr_t srcpa = VM_PAGE_TO_PHYS(srcpg);
	paddr_t dstpa = VM_PAGE_TO_PHYS(dstpg);
	/* simple_lock(&pmap_copy_page_lock); */

	pmap_kenter_pa(copy_src_page, srcpa, PROT_READ);
	pmap_kenter_pa(copy_dst_page, dstpa, PROT_READ|PROT_WRITE);

	bcopy((void *)copy_src_page, (void *)copy_dst_page, PAGE_SIZE);

	pmap_kremove_pg(copy_src_page);
	pmap_kremove_pg(copy_dst_page);
	/* simple_unlock(&pmap_copy_page_lock); */
}

void
pmap_pinit(pmap_t pm)
{
	// Build time asserts on some data structs. (vp3 is not same size)

	bzero(pm, sizeof (struct pmap));
	vaddr_t l0va;

	/* Allocate a full L0/L1 table. */
	if (pm->have_4_level_pt) {
		while (pm->pm_vp.l0 == NULL) {
			pm->pm_vp.l0 = pool_get(&pmap_vp_pool,
			    PR_WAITOK | PR_ZERO);
		}
		l0va = (vaddr_t) pm->pm_vp.l0->l0; // top level is l0
	} else {
		while (pm->pm_vp.l1 == NULL) {

			pm->pm_vp.l1 = pool_get(&pmap_vp_pool,
			    PR_WAITOK | PR_ZERO);
		}
		l0va = (vaddr_t) pm->pm_vp.l1->l1; // top level is l1

	}

	//pmap_allocate_asid(pm); // by default global (allocate asid later!?!)
	pm->pm_asid = -1;

	pmap_extract(pmap_kernel(), l0va, (paddr_t *)&pm->pm_pt0pa);

	pmap_reference(pm);
}

int pmap_vp_poolcache = 0; // force vp poolcache to allocate late.
/*
 * Create and return a physical map.
 */
pmap_t
pmap_create()
{
	pmap_t pmap;
	int s;

	s = splvm();
	pmap = pool_get(&pmap_pmap_pool, PR_WAITOK);
	splx(s);
	pmap_pinit(pmap);
	if (pmap_vp_poolcache == 0) {
		pool_setlowat(&pmap_vp_pool, 20);
		pmap_vp_poolcache = 20;
	}
	return (pmap);
}

/*
 * Add a reference to a given pmap.
 */
void
pmap_reference(pmap_t pm)
{
	/* simple_lock(&pmap->pm_obj.vmobjlock); */
	pm->pm_refs++;
	/* simple_unlock(&pmap->pm_obj.vmobjlock); */
}

/*
 * Retire the given pmap from service.
 * Should only be called if the map contains no valid mappings.
 */
void
pmap_destroy(pmap_t pm)
{
	int refs;
	int s;

	/* simple_lock(&pmap->pm_obj.vmobjlock); */
	refs = --pm->pm_refs;
	/* simple_unlock(&pmap->pm_obj.vmobjlock); */
	if (refs > 0)
		return;

	if (pm->pm_asid != -1) {
		pmap_free_asid(pm);
	}
	/*
	 * reference count is zero, free pmap resources and free pmap.
	 */
	pmap_release(pm);
	s = splvm();
	pool_put(&pmap_pmap_pool, pm);
	splx(s);
}

/*
 * Release any resources held by the given physical map.
 * Called when a pmap initialized by pmap_pinit is being released.
 */
void
pmap_release(pmap_t pm)
{
	pmap_vp_destroy(pm);
}

void pmap_vp_destroy_l2_l3(pmap_t pm, struct pmapvp1 *vp1);
void
pmap_vp_destroy_l2_l3(pmap_t pm, struct pmapvp1 *vp1)
{
	int j, k, l, s;
	struct pmapvp2 *vp2;
	struct pmapvp3 *vp3;
	struct pte_desc *pted;

	for (j = 0; j < VP_IDX1_CNT; j++) {
		vp2 = vp1->vp[j];
		if (vp2 == NULL)
			continue;
		vp1->vp[j] = NULL;
		for (k = 0; k < VP_IDX2_CNT; k++) {
			vp3 = vp2->vp[k];
			if (vp3 == NULL)
				continue;
			vp2->vp[k] = NULL;
			for (l = 0; l < VP_IDX3_CNT; l++) {
				pted = vp3->vp[l];
				if (pted == NULL)
					continue;
				vp3->vp[l] = NULL;
				
				s = splvm();
				pool_put(&pmap_pted_pool, pted);
				splx(s);
			}
			pool_put(&pmap_vp_pool, vp3);
		}
		pool_put(&pmap_vp_pool, vp2);
	}
}

void
pmap_vp_destroy(pmap_t pm)
{
	int i;
	struct pmapvp0 *vp0;
	struct pmapvp1 *vp1;

	// Is there a better way to share this code between 3 and 4 level tables
	// split the lower levels into a different function?
	if (!pm->have_4_level_pt) {
		pmap_vp_destroy_l2_l3(pm, pm->pm_vp.l1);
		pool_put(&pmap_vp_pool, pm->pm_vp.l1);
		pm->pm_vp.l1 = NULL;
		return;
	}

	vp0 = pm->pm_vp.l0;
	for (i = 0; i < VP_IDX0_CNT; i++) {
		vp1 = vp0->vp[i];
		if (vp1 == NULL)
			continue;

		pmap_vp_destroy_l2_l3(pm, vp1);

		vp0->vp[i] = NULL;
		pool_put(&pmap_vp_pool, vp1);
	}
	pm->pm_vp.l0 = NULL;
	pool_put(&pmap_vp_pool, vp0);
}

/*
 * Similar to pmap_steal_avail, but operating on vm_physmem since
 * uvm_page_physload() has been called.
 */
vaddr_t
pmap_steal_memory(vsize_t size, vaddr_t *start, vaddr_t *end)
{
	int segno;
	u_int npg;
	vaddr_t va;
	paddr_t pa;
	struct vm_physseg *seg;

	size = round_page(size);
	npg = atop(size);

	for (segno = 0, seg = vm_physmem; segno < vm_nphysseg; segno++, seg++) {
		if (seg->avail_end - seg->avail_start < npg)
			continue;
		/*
		 * We can only steal at an ``unused'' segment boundary,
		 * i.e. either at the start or at the end.
		 */
		if (seg->avail_start == seg->start ||
		    seg->avail_end == seg->end)
			break;
	}
	if (segno == vm_nphysseg)
		va = 0;
	else {
		if (seg->avail_start == seg->start) {
			pa = ptoa(seg->avail_start);
			seg->avail_start += npg;
			seg->start += npg;
		} else {
			pa = ptoa(seg->avail_end) - size;
			seg->avail_end -= npg;
			seg->end -= npg;
		}
		/*
		 * If all the segment has been consumed now, remove it.
		 * Note that the crash dump code still knows about it
		 * and will dump it correctly.
		 */
		if (seg->start == seg->end) {
			if (vm_nphysseg-- == 1)
				panic("pmap_steal_memory: out of memory");
			while (segno < vm_nphysseg) {
				seg[0] = seg[1]; /* struct copy */
				seg++;
				segno++;
			}
		}

		va = (vaddr_t)pa;	/* 1:1 mapping */
		bzero((void *)va, size);
	}

	if (start != NULL)
		*start = VM_MIN_KERNEL_ADDRESS;
	if (end != NULL)
		*end = VM_MAX_KERNEL_ADDRESS;

	return (va);
}

vaddr_t virtual_avail, virtual_end;

STATIC __inline uint64_t
VP_Lx(paddr_t pa)
{
	// This function takes the pa address given and manipulates it
	// into the form that should be inserted into the VM table;
	return pa | Lx_TYPE_PT;
}

void pmap_setup_avail( uint64_t ram_start, uint64_t ram_end, uint64_t kvo);
/*
 * Initialize pmap setup.
 * ALL of the code which deals with avail needs rewritten as an actual
 * memory allocation.
 */
CTASSERT(sizeof(struct pmapvp0) == 8192);

int mappings_allocated = 0;
int pted_allocated = 0;

vaddr_t
pmap_bootstrap(long kvo, paddr_t lpt1,  long kernelstart, long kernelend,
    long ram_start, long ram_end)
{
	void  *va;
	paddr_t pa;
	struct pmapvp1 *vp1;
	struct pmapvp2 *vp2;
	struct pmapvp3 *vp3;
	struct pte_desc *pted;
	vaddr_t vstart;
	int i, j, k;
	int lb_idx2, ub_idx2;

	pmap_setup_avail(ram_start, ram_end, kvo);

	/* in theory we could start with just the memory in the kernel,
	 * however this could 'allocate' the bootloader and bootstrap
	 * vm table, which we may need to preserve until later.
	 *   pmap_remove_avail(kernelstart-kvo, kernelend-kvo);
	 */
	printf("removing %llx-%llx\n", ram_start, kernelstart+kvo); // preserve bootloader?
	pmap_remove_avail(ram_start, kernelstart+kvo); // preserve bootloader?
	printf("removing %llx-%llx\n", kernelstart+kvo, kernelend+kvo);
	pmap_remove_avail(kernelstart+kvo, kernelend+kvo);


	// KERNEL IS ASSUMED TO BE 39 bits (or less), start from L1, not L0
	// ALSO kernel mappings may not cover enough ram to bootstrap
	// so all accesses initializing tables must be done via physical
	// pointers

	pa = pmap_steal_avail(sizeof (struct pmapvp1), Lx_TABLE_ALIGN,
	    &va);
	vp1 = (struct pmapvp1 *)pa;
	pmap_kernel()->pm_vp.l1 = (struct pmapvp1 *)va;
	pmap_kernel()->pm_pt0pa = pa;
	pmap_kernel()->pm_asid = -1;

	// allocate Lx entries
	for (i = VP_IDX1(VM_MIN_KERNEL_ADDRESS);
	    i <= VP_IDX1(VM_MAX_KERNEL_ADDRESS);
	    i++) {
		mappings_allocated++;
		pa = pmap_steal_avail(sizeof (struct pmapvp2), Lx_TABLE_ALIGN,
		    &va);
		vp2 = (struct pmapvp2 *)pa; // indexed physically
		vp1->vp[i] = va;
		vp1->l1[i] = VP_Lx(pa);

		if (i == VP_IDX1(VM_MIN_KERNEL_ADDRESS)) {
			lb_idx2 = VP_IDX2(VM_MIN_KERNEL_ADDRESS);
		} else {
			lb_idx2 = 0;
		}
		if (i == VP_IDX1(VM_MAX_KERNEL_ADDRESS)) {
			ub_idx2 = VP_IDX2(VM_MAX_KERNEL_ADDRESS);
		} else {
			ub_idx2 = VP_IDX2_CNT-1;
		}
		for (j = lb_idx2; j <= ub_idx2; j++) {
			mappings_allocated++;
			pa = pmap_steal_avail(sizeof (struct pmapvp3),
			    Lx_TABLE_ALIGN, &va);
			vp3 = (struct pmapvp3 *)pa; // indexed physically
			vp2->vp[j] = va;
			vp2->l2[j] = VP_Lx(pa);

		}
	}
	// allocate Lx entries
	for (i = VP_IDX1(VM_MIN_KERNEL_ADDRESS);
	    i <= VP_IDX1(VM_MAX_KERNEL_ADDRESS);
	    i++) {
		// access must be performed physical
		vp2 = (void *)((long)vp1->vp[i] + kvo);

		if (i == VP_IDX1(VM_MIN_KERNEL_ADDRESS)) {
			lb_idx2 = VP_IDX2(VM_MIN_KERNEL_ADDRESS);
		} else {
			lb_idx2 = 0;
		}
		if (i == VP_IDX1(VM_MAX_KERNEL_ADDRESS)) {
			ub_idx2 = VP_IDX2(VM_MAX_KERNEL_ADDRESS);
		} else {
			ub_idx2 = VP_IDX2_CNT-1;
		}
		for (j = lb_idx2; j <= ub_idx2; j++) {
			// access must be performed physical
			vp3 = (void *)((long)vp2->vp[j] + kvo);

			for (k = 0; k <= VP_IDX3_CNT-1; k++) {
				pted_allocated++;
				pa = pmap_steal_avail(sizeof(struct pte_desc),
				    4, &va);
				pted = va;
				vp3->vp[k] = pted;
			}
		}
	}

	pmap_curmaxkvaddr = VM_MAX_KERNEL_ADDRESS;


	// XXX should this extend the l2 bootstrap mappings for kernel entries?

	/* now that we have mapping space for everything, lets map it */
	/* all of these mappings are ram -> kernel va */

	// enable mappings for existing 'allocated' mapping in the bootstrap
	// page tables
	extern uint64_t *pagetable;
	extern int *_end;
	vp2 = (void *)((long)&pagetable + kvo);
	struct mem_region *mp;
	ssize_t size;
	for (mp = pmap_allocated; mp->size != 0; mp++) {
		// bounds may be kinda messed up
		for (pa = mp->start, size = mp->size & ~0xfff;
		    size > 0;
		    pa+= L2_SIZE, size -= L2_SIZE)
		{
			paddr_t mappa = pa & ~(L2_SIZE-1);
			vaddr_t mapva = mappa - kvo;
			if (mapva < (vaddr_t)&_end)
				continue;
			vp2->l2[VP_IDX2(mapva)] = mappa | L2_BLOCK |
				ATTR_AF| ATTR_IDX(2);
		}
	}

	// At this point we are still running on the bootstrap page tables
	// however all memory allocated for the final page tables is
	// 'allocated' and should now be mapped
	// This means we are able to use the virtual addressing to
	// enter the final mappings into the new mapping tables.

#if 0
	vm_prot_t prot;

	for (mp = pmap_allocated; mp->size != 0; mp++) {
		vaddr_t va;
		paddr_t pa;
		vsize_t size;
		extern char *etext();
		//printf("mapping %08x sz %08x\n", mp->start, mp->size);
		for (pa = mp->start, va = pa + kvo, size = mp->size & ~0xfff;
		    size > 0;
		    va += PAGE_SIZE, pa+= PAGE_SIZE, size -= PAGE_SIZE)
		{
			pa = va - kvo;
			prot = PROT_READ|PROT_WRITE;
			if ((vaddr_t)va >= (vaddr_t)kernelstart &&
			    (vaddr_t)va < (vaddr_t)etext)
				prot |= PROT_EXEC; // XXX remove WRITE?
			pmap_kenter_cache(va, pa, prot, PMAP_CACHE_WB);
		}
	}
#endif
	pmap_avail_fixup();

	vstart = pmap_map_stolen(kernelstart);

	void (switch_mmu_kernel)(long);
	void (*switch_mmu_kernel_table)(long) =
	    (void *)((long)&switch_mmu_kernel + kvo);
	switch_mmu_kernel_table (pmap_kernel()->pm_pt0pa);

	printf("all mapped\n");

	printf("stolen 0x%x memory\n", arm_kvm_stolen);

	return vstart;
}

void
pmap_set_l1(struct pmap *pm, uint64_t va, struct pmapvp1 *l1_va, paddr_t l1_pa)
{
	uint64_t pg_entry;
	int idx0;

	if (l1_pa == 0) {
		// if this is called from pmap_vp_enter, this is a normally
		// mapped page, call pmap_extract to get pa
		pmap_extract(pmap_kernel(), (vaddr_t)l1_va, &l1_pa);
	}

	if (l1_pa & (Lx_TABLE_ALIGN-1)) {
		panic("misaligned L2 table\n");
	}

	pg_entry = VP_Lx(l1_pa);

	idx0 = VP_IDX0(va);
	pm->pm_vp.l0->vp[idx0] = l1_va;

	pm->pm_vp.l0->l0[idx0] = pg_entry;
	__asm __volatile("dsb sy");

	dcache_wb_poc((vaddr_t)&pm->pm_vp.l0->l0[idx0], 8);

	ttlb_flush_range(pm, va & ~PAGE_MASK, 1<<VP_IDX1_POS);
}

void
pmap_set_l2(struct pmap *pm, uint64_t va, struct pmapvp2 *l2_va, paddr_t l2_pa)
{
	uint64_t pg_entry;
	struct pmapvp1 *vp1;
	int idx0, idx1;

	if (l2_pa == 0) {
		// if this is called from pmap_vp_enter, this is a normally
		// mapped page, call pmap_extract to get pa
		pmap_extract(pmap_kernel(), (vaddr_t)l2_va, &l2_pa);
	}

	if (l2_pa & (Lx_TABLE_ALIGN-1)) {
		panic("misaligned L2 table\n");
	}

	pg_entry = VP_Lx(l2_pa);

	idx0 = VP_IDX0(va);
	idx1 = VP_IDX1(va);
	if (pm->have_4_level_pt) {
		vp1 = pm->pm_vp.l0->vp[idx0];
	} else {
		vp1 = pm->pm_vp.l1;
	}
	vp1->vp[idx1] = l2_va;
	vp1->l1[idx1] = pg_entry;

	__asm __volatile("dsb sy");
	dcache_wb_poc((vaddr_t)&vp1->l1[idx1], 8);

	ttlb_flush_range(pm, va & ~PAGE_MASK, 1<<VP_IDX2_POS);
}

void
pmap_set_l3(struct pmap *pm, uint64_t va, struct pmapvp3 *l3_va, paddr_t l3_pa)
{
	uint64_t pg_entry;
	struct pmapvp1 *vp1;
	struct pmapvp2 *vp2;
	int idx0, idx1, idx2;

	if (l3_pa == 0) {
		// if this is called from pmap_vp_enter, this is a normally
		// mapped page, call pmap_extract to get pa
		pmap_extract(pmap_kernel(), (vaddr_t)l3_va, &l3_pa);
	}

	if (l3_pa & (Lx_TABLE_ALIGN-1)) {
		panic("misaligned L2 table\n");
	}

	pg_entry = VP_Lx(l3_pa);

	idx0 = VP_IDX0(va);
	idx1 = VP_IDX1(va);
	idx2 = VP_IDX2(va);

	if (pm->have_4_level_pt) {
		vp1 = pm->pm_vp.l0->vp[idx0];
	} else {
		vp1 = pm->pm_vp.l1;
	}
	vp2 = vp1->vp[idx1];

	vp2->vp[idx2] = l3_va;
	vp2->l2[idx2] = pg_entry;
	__asm __volatile("dsb sy");
	dcache_wb_poc((vaddr_t)&vp2->l2[idx2], 8);

	ttlb_flush_range(pm, va & ~PAGE_MASK, 1<<VP_IDX3_POS);
}

/*
 * activate a pmap entry
 */
void
pmap_activate(struct proc *p)
{
	pmap_t pm;
	struct pcb *pcb;
	int psw;

	pm = p->p_vmspace->vm_map.pmap;
	pcb = &p->p_addr->u_pcb;

	// printf("%s: called on proc %p %p\n", __func__, p,  pcb->pcb_pagedir);
	if (pm != pmap_kernel() && pm->pm_asid == -1) {
		// this userland process has no asid, allocate one.
		pmap_allocate_asid(pm);
	}

	if (pm != pmap_kernel())
		pcb->pcb_pagedir = ((uint64_t)pm->pm_asid << 48) | pm->pm_pt0pa;

	psw = disable_interrupts();
	if (p == curproc && pm != pmap_kernel() && pm != curcpu()->ci_curpm) {
		// clean up old process
		if (curcpu()->ci_curpm != NULL) {
			atomic_dec_int(&curcpu()->ci_curpm->pm_active);
		}

		curcpu()->ci_curpm = pm;
		pmap_setttb(p, pcb->pcb_pagedir, pcb);
		atomic_inc_int(&pm->pm_active);
	}
	restore_interrupts(psw);
}

/*
 * deactivate a pmap entry
 */
void
pmap_deactivate(struct proc *p)
{
	pmap_t pm;
	int psw;
	psw = disable_interrupts();
	pm = p->p_vmspace->vm_map.pmap;
	if (pm == curcpu()->ci_curpm) {
		curcpu()->ci_curpm = NULL;
		atomic_dec_int(&pm->pm_active);
	}
	restore_interrupts(psw);
}

/*
 * Get the physical page address for the given pmap/virtual address.
 */
boolean_t
pmap_extract(pmap_t pm, vaddr_t va, paddr_t *pa)
{
	struct pte_desc *pted;

	pted = pmap_vp_lookup(pm, va, NULL);

	if (pted == NULL)
		return FALSE;

	if (pted->pted_pte == 0)
		return FALSE;

	if (pa != NULL)
		*pa = (pted->pted_pte & PTE_RPGN) | (va & PAGE_MASK);

	return TRUE;
}


void
pmap_page_ro(pmap_t pm, vaddr_t va, vm_prot_t prot)
{
	struct pte_desc *pted;
	uint64_t *pl3;

	/* Every VA needs a pted, even unmanaged ones. */
	pted = pmap_vp_lookup(pm, va, &pl3);
	if (!pted || !PTED_VALID(pted)) {
		return;
	}

	pted->pted_va &= ~PROT_WRITE;
	pted->pted_pte &= ~PROT_WRITE;
	pmap_pte_update(pted, pl3);

	ttlb_flush(pm, pted->pted_va & ~PAGE_MASK);

	return;
}

/*
 * Lower the protection on the specified physical page.
 *
 * There are only two cases, either the protection is going to 0,
 * or it is going to read-only.
 */
void
pmap_page_protect(struct vm_page *pg, vm_prot_t prot)
{
	int s;
	struct pte_desc *pted;

	//if (!cold) printf("%s: prot %x\n", __func__, prot);

	/* need to lock for this pv */
	s = splvm();

	if (prot == PROT_NONE) {
		while (!LIST_EMPTY(&(pg->mdpage.pv_list))) {
			pted = LIST_FIRST(&(pg->mdpage.pv_list));
			pmap_remove_pted(pted->pted_pmap, pted);
		}
		/* page is being reclaimed, sync icache next use */
		atomic_clearbits_int(&pg->pg_flags, PG_PMAP_EXE);
		splx(s);
		return;
	}

	LIST_FOREACH(pted, &(pg->mdpage.pv_list), pted_pv_list) {
		pmap_page_ro(pted->pted_pmap, pted->pted_va, prot);
	}
	splx(s);
}


void
pmap_protect(pmap_t pm, vaddr_t sva, vaddr_t eva, vm_prot_t prot)
{
	//if (!cold) printf("%s\n", __func__);

	int s;
	if (prot & (PROT_READ | PROT_EXEC)) {
		s = splvm();
		while (sva < eva) {
			pmap_page_ro(pm, sva, 0);
			sva += PAGE_SIZE;
		}
		splx(s);
		return;
	}
	pmap_remove(pm, sva, eva);
}

void
pmap_init()
{
	uint64_t tcr;

	/*
	 * Now that we are in virtual address space we don't need
	 * the identity mapping in TTBR0 and can set the TCR to a
	 * more useful value.
	 */
	tcr = READ_SPECIALREG(tcr_el1);
	tcr &= ~TCR_T0SZ(0x3f);
	tcr |= TCR_T0SZ(64 - USER_SPACE_BITS);
	WRITE_SPECIALREG(tcr_el1, tcr);

	pool_init(&pmap_pmap_pool, sizeof(struct pmap), 0, IPL_VM, 0,
	    "pmap", NULL);
	pool_setlowat(&pmap_pmap_pool, 2);
	pool_init(&pmap_pted_pool, sizeof(struct pte_desc), 0, IPL_VM, 0,
	    "pted", NULL);
	pool_setlowat(&pmap_pted_pool, 20);
	pool_init(&pmap_vp_pool, sizeof(struct pmapvp2), PAGE_SIZE, IPL_VM, 0,
	    "vp", NULL);
	//pool_setlowat(&pmap_vp_pool, 20);

	pmap_initialized = 1;
}

void
pmap_proc_iflush(struct process *pr, vaddr_t addr, vsize_t len)
{
	paddr_t pa;
	vsize_t clen;

	while (len > 0) {
		/* add one to always round up to the next page */
		clen = round_page(addr + 1) - addr;
		if (clen > len)
			clen = len;

		if (pmap_extract(pr->ps_vmspace->vm_map.pmap, addr, &pa)) {
			//syncicache((void *)pa, clen);
		}

		len -= clen;
		addr += clen;
	}
}


STATIC uint64_t ap_bits_user [8] = {
	[PROT_NONE]			= ATTR_nG|ATTR_PXN|ATTR_UXN|ATTR_AP(2),
	[PROT_READ]			= ATTR_nG|ATTR_PXN|ATTR_UXN|ATTR_AF|ATTR_AP(3),
	[PROT_WRITE]			= ATTR_nG|ATTR_PXN|ATTR_UXN|ATTR_AF|ATTR_AP(1),
	[PROT_WRITE|PROT_READ]		= ATTR_nG|ATTR_PXN|ATTR_UXN|ATTR_AF|ATTR_AP(1),
	[PROT_EXEC]			= ATTR_nG|ATTR_PXN|ATTR_AF|ATTR_AP(2),
	[PROT_EXEC|PROT_READ]		= ATTR_nG|ATTR_PXN|ATTR_AF|ATTR_AP(3),
	[PROT_EXEC|PROT_WRITE]		= ATTR_nG|ATTR_PXN|ATTR_AF|ATTR_AP(1),
	[PROT_EXEC|PROT_WRITE|PROT_READ]= ATTR_nG|ATTR_PXN|ATTR_AF|ATTR_AP(1),
};

STATIC uint64_t ap_bits_kern [8] = {
	[PROT_NONE]				= ATTR_PXN|ATTR_UXN|ATTR_AP(2),
	[PROT_READ]				= ATTR_PXN|ATTR_UXN|ATTR_AF|ATTR_AP(2),
	[PROT_WRITE]				= ATTR_PXN|ATTR_UXN|ATTR_AF|ATTR_AP(0),
	[PROT_WRITE|PROT_READ]			= ATTR_PXN|ATTR_UXN|ATTR_AF|ATTR_AP(0),
	[PROT_EXEC]				= ATTR_UXN|ATTR_AF|ATTR_AP(2),
	[PROT_EXEC|PROT_READ]			= ATTR_UXN|ATTR_AF|ATTR_AP(2),
	[PROT_EXEC|PROT_WRITE]			= ATTR_UXN|ATTR_AF|ATTR_AP(0),
	[PROT_EXEC|PROT_WRITE|PROT_READ]	= ATTR_UXN|ATTR_AF|ATTR_AP(0),
};

void
pmap_pte_insert(struct pte_desc *pted)
{
	/* put entry into table */
	/* need to deal with ref/change here */
	pmap_t pm = pted->pted_pmap;
	uint64_t *pl3;

	if (pmap_vp_lookup(pm, pted->pted_va, &pl3) == NULL) {
		panic("pmap_pte_insert: have a pted, but missing a vp"
		    " for %x va pmap %x", __func__, pted->pted_va, pm);
	}

	pmap_pte_update(pted, pl3);
}

void
pmap_pte_update(struct pte_desc *pted, uint64_t *pl3)
{
	uint64_t pte, access_bits;
	pmap_t pm = pted->pted_pmap;
	uint64_t attr = 0;

	// see mair in locore.S
	switch (pted->pted_va & PMAP_CACHE_BITS) {
	case PMAP_CACHE_WB:
		attr |= ATTR_IDX(PTE_ATTR_WB); // inner and outer writeback
		attr |= ATTR_SH(SH_INNER);
		break;
	case PMAP_CACHE_WT: /* for the momemnt treating this as uncached */
		attr |= ATTR_IDX(PTE_ATTR_CI); // inner and outer uncached
		attr |= ATTR_SH(SH_INNER);
		break;
	case PMAP_CACHE_CI:
		attr |= ATTR_IDX(PTE_ATTR_DEV); // treat as device !?!?!?!
		break;
	case PMAP_CACHE_PTE:
		attr |= ATTR_IDX(PTE_ATTR_CI); // inner and outer uncached, XXX?
		attr |= ATTR_SH(SH_INNER);
		break;
	default:
		panic("pmap_pte_insert: invalid cache mode");
	}

	// kernel mappings are global, so nG should not be set

	if (pm == pmap_kernel()) {
		access_bits = ap_bits_kern[pted->pted_pte & PROT_MASK];
	} else {
		access_bits = ap_bits_user[pted->pted_pte & PROT_MASK];
	}

	pte = (pted->pted_pte & PTE_RPGN) | attr | access_bits | L3_P;

	*pl3 = pte;
	dcache_wb_poc((vaddr_t) pl3, 8);
	__asm __volatile("dsb sy");
}

void
pmap_pte_remove(struct pte_desc *pted, int remove_pted)
{
	/* put entry into table */
	/* need to deal with ref/change here */
	struct pmapvp1 *vp1;
	struct pmapvp2 *vp2;
	struct pmapvp3 *vp3;
	pmap_t pm = pted->pted_pmap;

	if (pm->have_4_level_pt) {
		vp1 = pm->pm_vp.l0->vp[VP_IDX0(pted->pted_va)];
	} else {
		vp1 = pm->pm_vp.l1;
	}
	if (vp1->vp[VP_IDX1(pted->pted_va)] == NULL) {
		panic("have a pted, but missing the l2 for %x va pmap %x",
		    pted->pted_va, pm);
	}
	vp2 = vp1->vp[VP_IDX1(pted->pted_va)];
	if (vp2 == NULL) {
		panic("have a pted, but missing the l2 for %x va pmap %x",
		    pted->pted_va, pm);
	}
	vp3 = vp2->vp[VP_IDX2(pted->pted_va)];
	if (vp3 == NULL) {
		panic("have a pted, but missing the l2 for %x va pmap %x",
		    pted->pted_va, pm);
	}
	vp3->l3[VP_IDX3(pted->pted_va)] = 0;
	if (remove_pted)
		vp3->vp[VP_IDX3(pted->pted_va)] = NULL;
	__asm __volatile("dsb sy");

	dcache_wb_poc((vaddr_t)&vp3->l3[VP_IDX3(pted->pted_va)], 8);

	arm64_tlbi_asid(pted->pted_va, pm->pm_asid);
}

/*
 * This function exists to do software referenced/modified emulation.
 * It's purpose is to tell the caller that a fault was generated either
 * for this emulation, or to tell the caller that it's a legit fault.
 */
int pmap_fault_fixup(pmap_t pm, vaddr_t va, vm_prot_t ftype, int user)
{
	struct pte_desc *pted;
	struct vm_page *pg;
	paddr_t pa;
	// pl3 is pointer to the L3 entry to update for this mapping.
	// will be valid if a pted exists.
	uint64_t *pl3 = NULL;

	//printf("fault pm %x va %x ftype %x user %x\n", pm, va, ftype, user);

	/* Every VA needs a pted, even unmanaged ones. */
	pted = pmap_vp_lookup(pm, va, &pl3);
	if (!pted || !PTED_VALID(pted)) {
		return 0;
	}

	/* There has to be a PA for the VA, get it. */
	pa = (pted->pted_pte & PTE_RPGN);

	/* If it's unmanaged, it must not fault. */
	pg = PHYS_TO_VM_PAGE(pa);
	if (pg == NULL) {
		return 0;
	}

	/*
	 * Check based on fault type for mod/ref emulation.
	 * if L3 entry is zero, it is not a possible fixup
	 */
	if (*pl3 == 0)
		return 0;


	/*
	 * Check the fault types to find out if we were doing
	 * any mod/ref emulation and fixup the PTE if we were.
	 */
	if ((ftype & PROT_WRITE) && /* fault caused by a write */
	    !(pted->pted_pte & PROT_WRITE) && /* and write is disabled now */
	    (pted->pted_va & PROT_WRITE)) { /* but is supposedly allowed */

		/*
		 * Page modified emulation. A write always includes
		 * a reference.  This means that we can enable read and
		 * exec as well, akin to the page reference emulation.
		 */
		pg->pg_flags |= PG_PMAP_MOD;
		pg->pg_flags |= PG_PMAP_REF;

		/* Thus, enable read, write and exec. */
		pted->pted_pte |=
		    (pted->pted_va & (PROT_READ|PROT_WRITE|PROT_EXEC));
		pmap_pte_update(pted, pl3);

		/* Flush tlb. */
		ttlb_flush(pm, va & ~PAGE_MASK);

		return 1;
	} else if ((ftype & PROT_EXEC) && /* fault caused by an exec */
	    !(pted->pted_pte & PROT_EXEC) && /* and exec is disabled now */
	    (pted->pted_va & PROT_EXEC)) { /* but is supposedly allowed */

		/*
		 * Exec always includes a reference. Since we now know
		 * the page has been accesed, we can enable read as well
		 * if UVM allows it.
		 */
		pg->pg_flags |= PG_PMAP_REF;

		/* Thus, enable read and exec. */
		pted->pted_pte |= (pted->pted_va & (PROT_READ|PROT_EXEC));
		pmap_pte_update(pted, pl3);

		/* Flush tlb. */
		ttlb_flush(pm, va & ~PAGE_MASK);

		return 1;
	} else if ((ftype & PROT_READ) && /* fault caused by a read */
	    !(pted->pted_pte & PROT_READ) && /* and read is disabled now */
	    (pted->pted_va & PROT_READ)) { /* but is supposedly allowed */

		/*
		 * Page referenced emulation. Since we now know the page
		 * has been accessed, we can enable exec as well if UVM
		 * allows it.
		 */
		pg->pg_flags |= PG_PMAP_REF;

		/* Thus, enable read and exec. */
		pted->pted_pte |= (pted->pted_va & (PROT_READ|PROT_EXEC));
		pmap_pte_update(pted, pl3);

		/* Flush tlb. */
		ttlb_flush(pm, va & ~PAGE_MASK);

		return 1;
	}

	/* didn't catch it, so probably broken */
	return 0;
}

void pmap_postinit(void) {}
void
pmap_map_section(vaddr_t l1_addr, vaddr_t va, paddr_t pa, int flags, int cache)
{
panic("%s called", __func__);
#if 0
	uint64_t *l1 = (uint64_t *)l1_addr;
	uint64_t cache_bits;
	int ap_flag;

	switch (flags) {
	case PROT_READ:
		ap_flag = L1_S_AP2|L1_S_AP0|L1_S_XN;
		break;
	case PROT_READ | PROT_WRITE:
		ap_flag = L1_S_AP0|L1_S_XN;
		break;
	}

	switch (cache) {
	case PMAP_CACHE_WB:
		cache_bits = L1_MODE_MEMORY;
		break;
	case PMAP_CACHE_WT: /* for the momemnt treating this as uncached */
		cache_bits = L1_MODE_DISPLAY;
		break;
	case PMAP_CACHE_CI:
		cache_bits = L1_MODE_DEV;
		break;
	case PMAP_CACHE_PTE:
		cache_bits = L1_MODE_PTE;
		break;
	}

	l1[va>>VP_IDX1_POS] = (pa & L1_S_RPGN) | ap_flag | cache_bits | L1_TYPE_S;
#endif
}

void    pmap_map_entry(vaddr_t l1, vaddr_t va, paddr_t pa, int i0, int i1) {}
vsize_t pmap_map_chunk(vaddr_t l1, vaddr_t va, paddr_t pa, vsize_t sz, int prot, int cache)
{
	for (; sz > 0; sz -= PAGE_SIZE, va += PAGE_SIZE, pa += PAGE_SIZE) {
		pmap_kenter_cache(va, pa, prot, cache);
	}
	return 0;
}


void pmap_update()
{
}

char *memhook;
vaddr_t zero_page;
vaddr_t copy_src_page;
vaddr_t copy_dst_page;


int pmap_is_referenced(struct vm_page *pg)
{
	return ((pg->pg_flags & PG_PMAP_REF) != 0);
}

int pmap_is_modified(struct vm_page *pg)
{
	return ((pg->pg_flags & PG_PMAP_MOD) != 0);
}

int pmap_clear_modify(struct vm_page *pg)
{
	struct pte_desc *pted;
	uint64_t *pl3 = NULL;

	//printf("%s\n", __func__);
	// XXX locks
	int s;

	s = splvm();

	pg->pg_flags &= ~PG_PMAP_MOD;

	LIST_FOREACH(pted, &(pg->mdpage.pv_list), pted_pv_list) {
		if (pmap_vp_lookup(pted->pted_pmap, pted->pted_va & ~PAGE_MASK, &pl3) == NULL)
			panic("failed to look up pte\n");
		*pl3  |= ATTR_AP(2);
		pted->pted_pte &= ~PROT_WRITE;

		ttlb_flush(pted->pted_pmap, pted->pted_va & ~PAGE_MASK);
	}
	splx(s);

	return 0;
}

/*
 * When this turns off read permissions it also disables write permissions
 * so that mod is correctly tracked after clear_ref; FAULT_READ; FAULT_WRITE;
 */
int pmap_clear_reference(struct vm_page *pg)
{
	struct pte_desc *pted;

	//printf("%s\n", __func__);

	// XXX locks
	int s;

	s = splvm();

	pg->pg_flags &= ~PG_PMAP_REF;

	LIST_FOREACH(pted, &(pg->mdpage.pv_list), pted_pv_list) {
		pted->pted_pte &= ~PROT_MASK;
		pmap_pte_insert(pted);
		ttlb_flush(pted->pted_pmap, pted->pted_va & ~PAGE_MASK);
	}
	splx(s);

	return 0;
}

void pmap_copy(pmap_t src_pmap, pmap_t dst_pmap, vaddr_t src, vsize_t sz, vaddr_t dst)
{
	//printf("%s\n", __func__);
}

void pmap_unwire(pmap_t pm, vaddr_t va)
{
	struct pte_desc *pted;

	//printf("%s\n", __func__);

	pted = pmap_vp_lookup(pm, va, NULL);
	if ((pted != NULL) && (pted->pted_va & PTED_VA_WIRED_M)) {
		pm->pm_stats.wired_count--;
		pted->pted_va &= ~PTED_VA_WIRED_M;
	}
}

void pmap_remove_holes(struct vmspace *vm)
{
	/* NOOP */
}

void pmap_virtual_space(vaddr_t *start, vaddr_t *end)
{
	*start = virtual_avail;
	*end = virtual_end;
}

vaddr_t  pmap_curmaxkvaddr;

void pmap_avail_fixup(void);

void
pmap_setup_avail( uint64_t ram_start, uint64_t ram_end, uint64_t kvo)
{
	/* This makes several assumptions
	 * 1) kernel will be located 'low' in memory
	 * 2) memory will not start at VM_MIN_KERNEL_ADDRESS
	 * 3) several MB of memory starting just after the kernel will
	 *    be premapped at the kernel address in the bootstrap mappings
	 * 4) kvo will be the 64 bit number to add to the ram address to
	 *    obtain the kernel virtual mapping of the ram. KVO == PA -> VA
	 * 5) it is generally assumed that these translations will occur with
	 *    large granularity, at minimum the translation will be page
	 *    aligned, if not 'section' or greater.
	 */

	pmap_avail_kvo = kvo;
	pmap_avail[0].start = ram_start;
	pmap_avail[0].size = ram_end-ram_start;


	// XXX - support more than one region
	pmap_memregions[0].start = ram_start;
	pmap_memregions[0].end = ram_end;
	pmap_memcount = 1;

	/* XXX - multiple sections */
	physmem = atop(pmap_avail[0].size);

	pmap_cnt_avail = 1;

	pmap_avail_fixup();
}

void
pmap_avail_fixup(void)
{
	struct mem_region *mp;
	vaddr_t align;
	vaddr_t end;

	mp = pmap_avail;
	while(mp->size !=0) {
		align = round_page(mp->start);
		if (mp->start != align) {
			pmap_remove_avail(mp->start, align);
			mp = pmap_avail;
			continue;
		}
		end = mp->start+mp->size;
		align = trunc_page(end);
		if (end != align) {
			pmap_remove_avail(align, end);
			mp = pmap_avail;
			continue;
		}
		mp++;
	}
}

/* remove a given region from avail memory */
void
pmap_remove_avail(paddr_t base, paddr_t end)
{
	struct mem_region *mp;
	int i;
	long mpend;

	/* remove given region from available */
	for (mp = pmap_avail; mp->size; mp++) {
		/*
		 * Check if this region holds all of the region
		 */
		mpend = mp->start + mp->size;
		if (base > mpend) {
			continue;
		}
		if (base <= mp->start) {
			if (end <= mp->start)
				break; /* region not present -??? */

			if (end >= mpend) {
				/* covers whole region */
				/* shorten */
				for (i = mp - pmap_avail;
				    i < pmap_cnt_avail;
				    i++) {
					pmap_avail[i] = pmap_avail[i+1];
				}
				pmap_cnt_avail--;
				pmap_avail[pmap_cnt_avail].size = 0;
			} else {
				mp->start = end;
				mp->size = mpend - end;
			}
		} else {
			/* start after the beginning */
			if (end >= mpend) {
				/* just truncate */
				mp->size = base - mp->start;
			} else {
				/* split */
				for (i = pmap_cnt_avail;
				    i > (mp - pmap_avail);
				    i--) {
					pmap_avail[i] = pmap_avail[i - 1];
				}
				pmap_cnt_avail++;
				mp->size = base - mp->start;
				mp++;
				mp->start = end;
				mp->size = mpend - end;
			}
		}
	}
	for (mp = pmap_allocated; mp->size != 0; mp++) {
		if (base < mp->start) {
			if (end == mp->start) {
				mp->start = base;
				mp->size += end - base;
				break;
			}
			/* lengthen */
			for (i = pmap_cnt_allocated; i > (mp - pmap_allocated);
			    i--) {
				pmap_allocated[i] = pmap_allocated[i - 1];
			}
			pmap_cnt_allocated++;
			mp->start = base;
			mp->size = end - base;
			return;
		}
		if (base == (mp->start + mp->size)) {
			mp->size += end - base;
			return;
		}
	}
	if (mp->size == 0) {
		mp->start = base;
		mp->size  = end - base;
		pmap_cnt_allocated++;
	}
}

/* XXX - this zeros pages via their physical address */
paddr_t
pmap_steal_avail(size_t size, int align, void **kva)
{
	struct mem_region *mp;
	long start;
	long remsize;
	arm_kvm_stolen += size; // debug only

	for (mp = pmap_avail; mp->size; mp++) {
		if (mp->size > size) {
			start = (mp->start + (align -1)) & ~(align -1);
			remsize = mp->size - (start - mp->start);
			if (remsize >= 0) {
				pmap_remove_avail(start, start+size);
				if (kva != NULL){
					*kva = (void *)(start - pmap_avail_kvo);
				}
				bzero((void*)(start), size);
				return start;
			}
		}
	}
	panic ("unable to allocate region with size %x align %x",
	    size, align);
	return 0; // XXX - only here because of ifdef
}

vaddr_t
pmap_map_stolen(vaddr_t kernel_start)
{
	int prot;
	struct mem_region *mp;
	uint64_t pa, va, e;
	extern char *etext();


	int oldprot = 0;
	printf("mapping self\n");
	for (mp = pmap_allocated; mp->size; mp++) {
		printf("start %16llx end %16llx\n", mp->start, mp->start + mp->size);
		printf("exe range %16llx %16llx\n", kernel_start,
		    (uint64_t)&etext);
		for (e = 0; e < mp->size; e += PAGE_SIZE) {
			/* XXX - is this a kernel text mapping? */
			/* XXX - Do we care about KDB ? */
			pa = mp->start + e;
			va = pa - pmap_avail_kvo;
			if (va < VM_MIN_KERNEL_ADDRESS ||
			    va >= VM_MAX_KERNEL_ADDRESS)
				continue;
			if ((vaddr_t)va >= (vaddr_t)kernel_start &&
			    (vaddr_t)va < (vaddr_t)&etext) {
				prot = PROT_READ|PROT_WRITE|
				    PROT_EXEC;
			} else {
				prot = PROT_READ|PROT_WRITE;
			}
			if (prot != oldprot) {
				printf("mapping  v %16llx p %16llx prot %x\n", va,
				    pa, prot);
				oldprot = prot;
			}
			pmap_kenter_cache(va, pa, prot, PMAP_CACHE_WB);
		}
	}
	printf("last mapping  v %16llx p %16llx\n", va, pa);
	return va + PAGE_SIZE;
}

void
pmap_physload_avail(void)
{
	struct mem_region *mp;
	uint64_t start, end;

	for (mp = pmap_avail; mp->size; mp++) {
		if (mp->size < PAGE_SIZE) {
			printf(" skipped - too small\n");
			continue;
		}
		start = mp->start;
		if (start & PAGE_MASK) {
			start = PAGE_SIZE + (start & PMAP_PA_MASK);
		}
		end = mp->start + mp->size;
		if (end & PAGE_MASK) {
			end = (end & PMAP_PA_MASK);
		}
		uvm_page_physload(atop(start), atop(end),
		    atop(start), atop(end), 0);

	}
}
void
pmap_show_mapping(uint64_t va)
{
	struct pmapvp1 *vp1;
	struct pmapvp2 *vp2;
	struct pmapvp3 *vp3;
	struct pte_desc *pted;
	printf("showing mapping of %llx\n", va);
	struct pmap *pm;
	if (va & 1ULL << 63) {
		pm = pmap_kernel();
	} else {
		pm = curproc->p_vmspace->vm_map.pmap;
	}

	if (pm->have_4_level_pt) {
		printf("  vp0 = %llx off %x\n",  pm->pm_vp.l0, VP_IDX0(va)*8);
		vp1 = pm->pm_vp.l0->vp[VP_IDX0(va)];
		if (vp1 == NULL) {
			return;
		}
	} else {
		vp1 = pm->pm_vp.l1;
	}
	uint64_t ttbr0, tcr;
	__asm volatile ("mrs     %x0, ttbr0_el1" : "=r"(ttbr0));
	__asm volatile ("mrs     %x0, tcr_el1" : "=r"(tcr));

	printf("  ttbr0 %llx %llx %llx tcr %llx\n", ttbr0, pm->pm_pt0pa,
	    curproc->p_addr->u_pcb.pcb_pagedir, tcr);
	printf("  vp1 = %llx\n", vp1);

	vp2 = vp1->vp[VP_IDX1(va)];
	printf("  vp2 = %llx lp2 = %llx idx1 off %x\n",
		vp2, vp1->l1[VP_IDX1(va)], VP_IDX1(va)*8);
	if (vp2 == NULL) {
		return;
	}

	vp3 = vp2->vp[VP_IDX2(va)];
	printf("  vp3 = %llx lp3 = %llx idx2 off %x\n",
		vp3, vp2->l2[VP_IDX2(va)], VP_IDX2(va)*8);
	if (vp3 == NULL) {
		return;
	}

	pted = vp3->vp[VP_IDX3(va)];
	printf("  pted = %p lp3 = %llx idx3 off  %x\n",
		pted, vp3->l3[VP_IDX3(va)], VP_IDX3(va)*8);

	return;
}

// in theory arm64 can support 16 bit asid should we support more?
// XXX should this be based on how many the specific cpu supports.
#define MAX_ASID 256
struct pmap *pmap_asid[MAX_ASID];
int pmap_asid_id_next = 1;

// stupid quick allocator, flush all asid when we run out
// XXX never searches, just flushes all on rollover (or out)
// XXXXX - this is not MP safe
// MPSAFE would need to check if slot is available, and skip if
// not and on revoking asids it would need to lock each pmap, see if it were
// active, if not active, free the asid, mark asid free , and then unlock
void
pmap_allocate_asid(pmap_t pm)
{
	int i, new_asid;

	if (pmap_asid_id_next == MAX_ASID) {
		// out of asid, flush all
		__asm __volatile("tlbi	vmalle1is");
		for (i = 0;i < MAX_ASID; i++) {
			if (pmap_asid[i] != NULL) {
				// printf("reclaiming asid %d from %p\n", i,
				//    pmap_asid[i] );
				pmap_asid[i]->pm_asid = -1;
				pmap_asid[i] = NULL;
			}
		}
		pmap_asid_id_next = 1;
	}

	// locks?
	new_asid = pmap_asid_id_next++;

	//printf("%s: allocating asid %d for pmap %p\n",
	//    __func__, new_asid, pm);

	pmap_asid[new_asid] = pm;
	pmap_asid[new_asid]->pm_asid = new_asid;
	return;
}

void
pmap_free_asid(pmap_t pm)
{
	//printf("freeing asid %d pmap %p\n", pm->pm_asid, pm);
	// XX locks
	int asid = pm->pm_asid;
	pm->pm_asid = -1;
	pmap_asid[asid] = NULL;
}

void
pmap_setttb(struct proc *p, paddr_t pagedir, struct pcb *pcb)
{
	// XXX process locks
	pmap_t pm = p->p_vmspace->vm_map.pmap;
	//int oasid = pm->pm_asid;

	if (pm != pmap_kernel()) {
		if (pm->pm_asid < 0) {
			pmap_allocate_asid(pm);
			pcb->pcb_pagedir = ((uint64_t)pm->pm_asid << 48) |
			    pm->pm_pt0pa;
			pagedir = pcb->pcb_pagedir;
		}
		//printf("switching userland to %p %p asid %d new asid %d\n",
		//    pm, pmap_kernel(), oasid, pm->pm_asid);

		__asm volatile("msr ttbr0_el1, %x0" :: "r"(pagedir));
		__asm volatile("dsb sy");
	} else {
		// XXX what to do if switching to kernel pmap !?!?
	}
}
