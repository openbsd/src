/* $OpenBSD: pmap.c,v 1.58 2018/09/12 11:58:28 kettenis Exp $ */
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
#include "machine/cpufunc.h"
#include "machine/pcb.h"

#include <machine/db_machdep.h>
#include <ddb/db_extern.h>
#include <ddb/db_output.h>

void pmap_setttb(struct proc *p);
void pmap_free_asid(pmap_t pm);

/* We run userland code with ASIDs that have the low bit set. */
#define ASID_USER	1

static inline void
ttlb_flush(pmap_t pm, vaddr_t va)
{
	vaddr_t resva;

	resva = ((va >> PAGE_SHIFT) & ((1ULL << 44) - 1));
	if (pm == pmap_kernel()) {
		cpu_tlb_flush_all_asid(resva);
	} else {
		resva |= (uint64_t)pm->pm_asid << 48;
		cpu_tlb_flush_asid(resva);
		resva |= (uint64_t)ASID_USER << 48;
		cpu_tlb_flush_asid(resva);
	}
}

struct pmap kernel_pmap_;
struct pmap pmap_tramp;

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
void pmap_vp_destroy_l2_l3(pmap_t pm, struct pmapvp1 *vp1);
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

/* Allocator for VP pool. */
void *pmap_vp_page_alloc(struct pool *, int, int *);
void pmap_vp_page_free(struct pool *, void *);

struct pool_allocator pmap_vp_allocator = {
	pmap_vp_page_alloc, pmap_vp_page_free, sizeof(struct pmapvp0)
};


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
paddr_t pmap_steal_avail(size_t size, int align, void **kva);
void pmap_remove_avail(paddr_t base, paddr_t end);
vaddr_t pmap_map_stolen(vaddr_t);
void pmap_physload_avail(void);
extern caddr_t msgbufaddr;

vaddr_t vmmap;
vaddr_t zero_page;
vaddr_t copy_src_page;
vaddr_t copy_dst_page;

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

static inline void
pmap_lock(struct pmap *pmap)
{
	if (pmap != pmap_kernel())
		mtx_enter(&pmap->pm_mtx);
}

static inline void
pmap_unlock(struct pmap *pmap)
{
	if (pmap != pmap_kernel())
		mtx_leave(&pmap->pm_mtx);
}

/* virtual to physical helpers */
static inline int
VP_IDX0(vaddr_t va)
{
	return (va >> VP_IDX0_POS) & VP_IDX0_MASK;
}

static inline int
VP_IDX1(vaddr_t va)
{
	return (va >> VP_IDX1_POS) & VP_IDX1_MASK;
}

static inline int
VP_IDX2(vaddr_t va)
{
	return (va >> VP_IDX2_POS) & VP_IDX2_MASK;
}

static inline int
VP_IDX3(vaddr_t va)
{
	return (va >> VP_IDX3_POS) & VP_IDX3_MASK;
}

const uint64_t ap_bits_user[8] = {
	[PROT_NONE]				= ATTR_PXN|ATTR_UXN|ATTR_AP(2),
	[PROT_READ]				= ATTR_PXN|ATTR_UXN|ATTR_AF|ATTR_AP(3),
	[PROT_WRITE]				= ATTR_PXN|ATTR_UXN|ATTR_AF|ATTR_AP(1),
	[PROT_WRITE|PROT_READ]			= ATTR_PXN|ATTR_UXN|ATTR_AF|ATTR_AP(1),
	[PROT_EXEC]				= ATTR_PXN|ATTR_AF|ATTR_AP(2),
	[PROT_EXEC|PROT_READ]			= ATTR_PXN|ATTR_AF|ATTR_AP(3),
	[PROT_EXEC|PROT_WRITE]			= ATTR_PXN|ATTR_AF|ATTR_AP(1),
	[PROT_EXEC|PROT_WRITE|PROT_READ]	= ATTR_PXN|ATTR_AF|ATTR_AP(1),
};

const uint64_t ap_bits_kern[8] = {
	[PROT_NONE]				= ATTR_PXN|ATTR_UXN|ATTR_AP(2),
	[PROT_READ]				= ATTR_PXN|ATTR_UXN|ATTR_AF|ATTR_AP(2),
	[PROT_WRITE]				= ATTR_PXN|ATTR_UXN|ATTR_AF|ATTR_AP(0),
	[PROT_WRITE|PROT_READ]			= ATTR_PXN|ATTR_UXN|ATTR_AF|ATTR_AP(0),
	[PROT_EXEC]				= ATTR_UXN|ATTR_AF|ATTR_AP(2),
	[PROT_EXEC|PROT_READ]			= ATTR_UXN|ATTR_AF|ATTR_AP(2),
	[PROT_EXEC|PROT_WRITE]			= ATTR_UXN|ATTR_AF|ATTR_AP(0),
	[PROT_EXEC|PROT_WRITE|PROT_READ]	= ATTR_UXN|ATTR_AF|ATTR_AP(0),
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

	if (pm->have_4_level_pt) {
		vp1 = pm->pm_vp.l0->vp[VP_IDX0(va)];
		if (vp1 == NULL) {
			vp1 = pool_get(&pmap_vp_pool, PR_NOWAIT | PR_ZERO);
			if (vp1 == NULL) {
				if ((flags & PMAP_CANFAIL) == 0)
					panic("%s: unable to allocate L1",
					    __func__);
				return ENOMEM;
			}
			pmap_set_l1(pm, va, vp1, 0);
		}
	} else {
		vp1 = pm->pm_vp.l1;
	}

	vp2 = vp1->vp[VP_IDX1(va)];
	if (vp2 == NULL) {
		vp2 = pool_get(&pmap_vp_pool, PR_NOWAIT | PR_ZERO);
		if (vp2 == NULL) {
			if ((flags & PMAP_CANFAIL) == 0)
				panic("%s: unable to allocate L2", __func__);
			return ENOMEM;
		}
		pmap_set_l2(pm, va, vp2, 0);
	}

	vp3 = vp2->vp[VP_IDX2(va)];
	if (vp3 == NULL) {
		vp3 = pool_get(&pmap_vp_pool, PR_NOWAIT | PR_ZERO);
		if (vp3 == NULL) {
			if ((flags & PMAP_CANFAIL) == 0)
				panic("%s: unable to allocate L3", __func__);
			return ENOMEM;
		}
		pmap_set_l3(pm, va, vp3, 0);
	}

	vp3->vp[VP_IDX3(va)] = pted;
	return 0;
}

void *
pmap_vp_page_alloc(struct pool *pp, int flags, int *slowdown)
{
	struct kmem_dyn_mode kd = KMEM_DYN_INITIALIZER;

	kd.kd_waitok = ISSET(flags, PR_WAITOK);
	kd.kd_trylock = ISSET(flags, PR_NOWAIT);
	kd.kd_slowdown = slowdown;

	return km_alloc(pp->pr_pgsize, &kv_any, &kp_dirty, &kd);
}

void
pmap_vp_page_free(struct pool *pp, void *v)
{
	km_free(v, pp->pr_pgsize, &kv_any, &kp_dirty);
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
	/*
	 * XXX does this test mean that some pages try to be managed,
	 * but this is called too soon?
	 */
	if (__predict_false(!pmap_initialized))
		return;

	mtx_enter(&pg->mdpage.pv_mtx);
	LIST_INSERT_HEAD(&(pg->mdpage.pv_list), pted, pted_pv_list);
	pted->pted_va |= PTED_VA_MANAGED_M;
	mtx_leave(&pg->mdpage.pv_mtx);
}

void
pmap_remove_pv(struct pte_desc *pted)
{
	struct vm_page *pg = PHYS_TO_VM_PAGE(pted->pted_pte & PTE_RPGN);

	mtx_enter(&pg->mdpage.pv_mtx);
	LIST_REMOVE(pted, pted_pv_list);
	mtx_leave(&pg->mdpage.pv_mtx);
}

int
pmap_enter(pmap_t pm, vaddr_t va, paddr_t pa, vm_prot_t prot, int flags)
{
	struct pte_desc *pted;
	struct vm_page *pg;
	int error;
	int cache = PMAP_CACHE_WB;
	int need_sync = 0;

	if (pa & PMAP_NOCACHE)
		cache = PMAP_CACHE_CI;
	if (pa & PMAP_DEVICE)
		cache = PMAP_CACHE_DEV;
	pg = PHYS_TO_VM_PAGE(pa);

	pmap_lock(pm);
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
			if ((flags & PMAP_CANFAIL) == 0)
				panic("%s: failed to allocate pted", __func__);
			error = ENOMEM;
			goto out;
		}
		if (pmap_vp_enter(pm, va, pted, flags)) {
			if ((flags & PMAP_CANFAIL) == 0)
				panic("%s: failed to allocate L2/L3", __func__);
			error = ENOMEM;
			pool_put(&pmap_pted_pool, pted);
			goto out;
		}
	}

	/*
	 * If it should be enabled _right now_, we can skip doing ref/mod
	 * emulation. Any access includes reference, modified only by write.
	 */
	if (pg != NULL &&
	    ((flags & PROT_MASK) || (pg->pg_flags & PG_PMAP_REF))) {
		atomic_setbits_int(&pg->pg_flags, PG_PMAP_REF);
		if ((prot & PROT_WRITE) && (flags & PROT_WRITE)) {
			atomic_setbits_int(&pg->pg_flags, PG_PMAP_MOD);
			atomic_clearbits_int(&pg->pg_flags, PG_PMAP_EXE);
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

	ttlb_flush(pm, va & ~PAGE_MASK);

	if (pg != NULL && (flags & PROT_EXEC)) {
		need_sync = ((pg->pg_flags & PG_PMAP_EXE) == 0);
		atomic_setbits_int(&pg->pg_flags, PG_PMAP_EXE);
	}

	if (need_sync && (pm == pmap_kernel() || (curproc &&
	    curproc->p_vmspace->vm_map.pmap == pm)))
		cpu_icache_sync_range(va & ~PAGE_MASK, PAGE_SIZE);

	error = 0;
out:
	pmap_unlock(pm);
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

	pmap_lock(pm);
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
	pmap_unlock(pm);
}

/*
 * remove a single mapping, notice that this code is O(1)
 */
void
pmap_remove_pted(pmap_t pm, struct pte_desc *pted)
{
	pm->pm_stats.resident_count--;

	if (pted->pted_va & PTED_VA_WIRED_M) {
		pm->pm_stats.wired_count--;
		pted->pted_va &= ~PTED_VA_WIRED_M;
	}

	pmap_pte_remove(pted, pm != pmap_kernel());

	ttlb_flush(pm, pted->pted_va & ~PAGE_MASK);

	if (pted->pted_va & PTED_VA_EXEC_M) {
		pted->pted_va &= ~PTED_VA_EXEC_M;
	}

	if (PTED_MANAGED(pted))
		pmap_remove_pv(pted);

	pted->pted_pte = 0;
	pted->pted_va = 0;

	if (pm != pmap_kernel())
		pool_put(&pmap_pted_pool, pted);
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
	pmap_t pm = pmap_kernel();
	struct pte_desc *pted;

	pted = pmap_vp_lookup(pm, va, NULL);

	/* Do not have pted for this, get one and put it in VP */
	if (pted == NULL) {
		panic("pted not preallocated in pmap_kernel() va %lx pa %lx\n",
		    va, pa);
	}

	if (pted && PTED_VALID(pted))
		pmap_kremove_pg(va); /* pted is reused */

	pm->pm_stats.resident_count++;

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
}

void
pmap_kenter_pa(vaddr_t va, paddr_t pa, vm_prot_t prot)
{
	_pmap_kenter_pa(va, pa, prot, prot,
	    (pa & PMAP_NOCACHE) ? PMAP_CACHE_CI : PMAP_CACHE_WB);
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
	pmap_t pm = pmap_kernel();
	struct pte_desc *pted;
	int s;

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
 */
void
pmap_zero_page(struct vm_page *pg)
{
	paddr_t pa = VM_PAGE_TO_PHYS(pg);
	vaddr_t va = zero_page + cpu_number() * PAGE_SIZE;

	pmap_kenter_pa(va, pa, PROT_READ|PROT_WRITE);
	pagezero_cache(va);
	pmap_kremove_pg(va);
}

/*
 * Copy the given physical page.
 */
void
pmap_copy_page(struct vm_page *srcpg, struct vm_page *dstpg)
{
	paddr_t srcpa = VM_PAGE_TO_PHYS(srcpg);
	paddr_t dstpa = VM_PAGE_TO_PHYS(dstpg);
	vaddr_t srcva = copy_src_page + cpu_number() * PAGE_SIZE;
	vaddr_t dstva = copy_dst_page + cpu_number() * PAGE_SIZE;

	pmap_kenter_pa(srcva, srcpa, PROT_READ);
	pmap_kenter_pa(dstva, dstpa, PROT_READ|PROT_WRITE);
	memcpy((void *)dstva, (void *)srcva, PAGE_SIZE);
	pmap_kremove_pg(srcva);
	pmap_kremove_pg(dstva);
}

void
pmap_pinit(pmap_t pm)
{
	vaddr_t l0va;

	/* Allocate a full L0/L1 table. */
	if (pm->have_4_level_pt) {
		while (pm->pm_vp.l0 == NULL) {
			pm->pm_vp.l0 = pool_get(&pmap_vp_pool,
			    PR_WAITOK | PR_ZERO);
		}
		l0va = (vaddr_t)pm->pm_vp.l0->l0; /* top level is l0 */
	} else {
		while (pm->pm_vp.l1 == NULL) {

			pm->pm_vp.l1 = pool_get(&pmap_vp_pool,
			    PR_WAITOK | PR_ZERO);
		}
		l0va = (vaddr_t)pm->pm_vp.l1->l1; /* top level is l1 */

	}

	pmap_extract(pmap_kernel(), l0va, (paddr_t *)&pm->pm_pt0pa);

	pmap_allocate_asid(pm);
	pmap_reference(pm);
}

int pmap_vp_poolcache = 0; /* force vp poolcache to allocate late */

/*
 * Create and return a physical map.
 */
pmap_t
pmap_create(void)
{
	pmap_t pmap;

	pmap = pool_get(&pmap_pmap_pool, PR_WAITOK | PR_ZERO);

	mtx_init(&pmap->pm_mtx, IPL_VM);

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
	atomic_inc_int(&pm->pm_refs);
}

/*
 * Retire the given pmap from service.
 * Should only be called if the map contains no valid mappings.
 */
void
pmap_destroy(pmap_t pm)
{
	int refs;

	refs = atomic_dec_int_nv(&pm->pm_refs);
	if (refs > 0)
		return;

	/*
	 * reference count is zero, free pmap resources and free pmap.
	 */
	pmap_release(pm);
	pmap_free_asid(pm);
	pool_put(&pmap_pmap_pool, pm);
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

void
pmap_vp_destroy_l2_l3(pmap_t pm, struct pmapvp1 *vp1)
{
	struct pmapvp2 *vp2;
	struct pmapvp3 *vp3;
	struct pte_desc *pted;
	int j, k, l;

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
				
				pool_put(&pmap_pted_pool, pted);
			}
			pool_put(&pmap_vp_pool, vp3);
		}
		pool_put(&pmap_vp_pool, vp2);
	}
}

void
pmap_vp_destroy(pmap_t pm)
{
	struct pmapvp0 *vp0;
	struct pmapvp1 *vp1;
	int i;

	/*
	 * XXX Is there a better way to share this code between 3 and
	 * 4 level tables?  Split the lower levels into a different
	 * function?
	 */
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
		vp0->vp[i] = NULL;

		pmap_vp_destroy_l2_l3(pm, vp1);
		pool_put(&pmap_vp_pool, vp1);
	}
	pool_put(&pmap_vp_pool, vp0);
	pm->pm_vp.l0 = NULL;
}

vaddr_t virtual_avail, virtual_end;

static inline uint64_t
VP_Lx(paddr_t pa)
{
	/*
	 * This function takes the pa address given and manipulates it
	 * into the form that should be inserted into the VM table.
	 */
	return pa | Lx_TYPE_PT;
}

/*
 * Allocator for growing the kernel page tables.  We use a dedicated
 * submap to make sure we have the space to map them as we are called
 * when address space is tight!
 */

struct vm_map *pmap_kvp_map;

const struct kmem_va_mode kv_kvp = {
	.kv_map = &pmap_kvp_map,
	.kv_wait = 0
};

void *
pmap_kvp_alloc(void)
{
	return km_alloc(sizeof(struct pmapvp0), &kv_kvp, &kp_zero, &kd_nowait);
}

struct pte_desc *
pmap_kpted_alloc(void)
{
	static struct pte_desc *pted;
	static int npted;

	if (npted == 0) {
		pted = km_alloc(PAGE_SIZE, &kv_kvp, &kp_zero, &kd_nowait);
		if (pted == NULL)
			return NULL;
		npted = PAGE_SIZE / sizeof(struct pte_desc);
	}

	npted--;
	return pted++;
}

/*
 * In pmap_bootstrap() we allocate the page tables for the first GB
 * of the kernel address space.
 */
vaddr_t pmap_maxkvaddr = VM_MIN_KERNEL_ADDRESS + 1024 * 1024 * 1024;

vaddr_t
pmap_growkernel(vaddr_t maxkvaddr)
{
	struct pmapvp1 *vp1 = pmap_kernel()->pm_vp.l1;
	struct pmapvp2 *vp2;
	struct pmapvp3 *vp3;
	struct pte_desc *pted;
	paddr_t pa;
	int lb_idx2, ub_idx2;
	int i, j, k;
	int s;

	if (maxkvaddr <= pmap_maxkvaddr)
		return pmap_maxkvaddr;

	/*
	 * Not strictly necessary, but we use an interrupt-safe map
	 * and uvm asserts that we're at IPL_VM.
	 */
	s = splvm();

	for (i = VP_IDX1(pmap_maxkvaddr); i <= VP_IDX1(maxkvaddr - 1); i++) {
		vp2 = vp1->vp[i];
		if (vp2 == NULL) {
			vp2 = pmap_kvp_alloc();
			if (vp2 == NULL)
				goto fail;
			pmap_extract(pmap_kernel(), (vaddr_t)vp2, &pa);
			vp1->vp[i] = vp2;
			vp1->l1[i] = VP_Lx(pa);
		}

		if (i == VP_IDX1(pmap_maxkvaddr)) {
			lb_idx2 = VP_IDX2(pmap_maxkvaddr);
		} else {
			lb_idx2 = 0;
		}

		if (i == VP_IDX1(maxkvaddr - 1)) {
			ub_idx2 = VP_IDX2(maxkvaddr - 1);
		} else {
			ub_idx2 = VP_IDX2_CNT - 1;
		}

		for (j = lb_idx2; j <= ub_idx2; j++) {
			vp3 = vp2->vp[j];
			if (vp3 == NULL) {
				vp3 = pmap_kvp_alloc();
				if (vp3 == NULL)
					goto fail;
				pmap_extract(pmap_kernel(), (vaddr_t)vp3, &pa);
				vp2->vp[j] = vp3;
				vp2->l2[j] = VP_Lx(pa);
			}

			for (k = 0; k <= VP_IDX3_CNT - 1; k++) {
				if (vp3->vp[k] == NULL) {
					pted = pmap_kpted_alloc();
					if (pted == NULL)
						goto fail;
					vp3->vp[k] = pted;
					pmap_maxkvaddr += PAGE_SIZE;
				}
			}
		}
	}
	KASSERT(pmap_maxkvaddr >= maxkvaddr);

fail:
	splx(s);
	
	return pmap_maxkvaddr;
}

void pmap_setup_avail(uint64_t ram_start, uint64_t ram_end, uint64_t kvo);

/*
 * Initialize pmap setup.
 * ALL of the code which deals with avail needs rewritten as an actual
 * memory allocation.
 */
CTASSERT(sizeof(struct pmapvp0) == 8192);

int mappings_allocated = 0;
int pted_allocated = 0;

extern char __text_start[], _etext[];
extern char __rodata_start[], _erodata[];

vaddr_t
pmap_bootstrap(long kvo, paddr_t lpt1, long kernelstart, long kernelend,
    long ram_start, long ram_end)
{
	void  *va;
	paddr_t pa, pt1pa;
	struct pmapvp1 *vp1;
	struct pmapvp2 *vp2;
	struct pmapvp3 *vp3;
	struct pte_desc *pted;
	vaddr_t vstart;
	int i, j, k;
	int lb_idx2, ub_idx2;

	pmap_setup_avail(ram_start, ram_end, kvo);

	/*
	 * in theory we could start with just the memory in the
	 * kernel, however this could 'allocate' the bootloader and
	 * bootstrap vm table, which we may need to preserve until
	 * later.
	 */
	printf("removing %lx-%lx\n", ram_start, kernelstart+kvo);
	pmap_remove_avail(ram_start, kernelstart+kvo);
	printf("removing %lx-%lx\n", kernelstart+kvo, kernelend+kvo);
	pmap_remove_avail(kernelstart+kvo, kernelend+kvo);

	/*
	 * KERNEL IS ASSUMED TO BE 39 bits (or less), start from L1,
	 * not L0 ALSO kernel mappings may not cover enough ram to
	 * bootstrap so all accesses initializing tables must be done
	 * via physical pointers
	 */

	pt1pa = pmap_steal_avail(2 * sizeof(struct pmapvp1), Lx_TABLE_ALIGN,
	    &va);
	vp1 = (struct pmapvp1 *)pt1pa;
	pmap_kernel()->pm_vp.l1 = (struct pmapvp1 *)va;
	pmap_kernel()->pm_privileged = 1;
	pmap_kernel()->pm_asid = 0;

	pmap_tramp.pm_vp.l1 = (struct pmapvp1 *)va + 1;
	pmap_tramp.pm_privileged = 1;
	pmap_tramp.pm_asid = 0;

	/* allocate Lx entries */
	for (i = VP_IDX1(VM_MIN_KERNEL_ADDRESS);
	    i <= VP_IDX1(pmap_maxkvaddr - 1);
	    i++) {
		mappings_allocated++;
		pa = pmap_steal_avail(sizeof(struct pmapvp2), Lx_TABLE_ALIGN,
		    &va);
		vp2 = (struct pmapvp2 *)pa; /* indexed physically */
		vp1->vp[i] = va;
		vp1->l1[i] = VP_Lx(pa);

		if (i == VP_IDX1(VM_MIN_KERNEL_ADDRESS)) {
			lb_idx2 = VP_IDX2(VM_MIN_KERNEL_ADDRESS);
		} else {
			lb_idx2 = 0;
		}
		if (i == VP_IDX1(pmap_maxkvaddr - 1)) {
			ub_idx2 = VP_IDX2(pmap_maxkvaddr - 1);
		} else {
			ub_idx2 = VP_IDX2_CNT - 1;
		}
		for (j = lb_idx2; j <= ub_idx2; j++) {
			mappings_allocated++;
			pa = pmap_steal_avail(sizeof(struct pmapvp3),
			    Lx_TABLE_ALIGN, &va);
			vp3 = (struct pmapvp3 *)pa; /* indexed physically */
			vp2->vp[j] = va;
			vp2->l2[j] = VP_Lx(pa);

		}
	}
	/* allocate Lx entries */
	for (i = VP_IDX1(VM_MIN_KERNEL_ADDRESS);
	    i <= VP_IDX1(pmap_maxkvaddr - 1);
	    i++) {
		/* access must be performed physical */
		vp2 = (void *)((long)vp1->vp[i] + kvo);

		if (i == VP_IDX1(VM_MIN_KERNEL_ADDRESS)) {
			lb_idx2 = VP_IDX2(VM_MIN_KERNEL_ADDRESS);
		} else {
			lb_idx2 = 0;
		}
		if (i == VP_IDX1(pmap_maxkvaddr - 1)) {
			ub_idx2 = VP_IDX2(pmap_maxkvaddr - 1);
		} else {
			ub_idx2 = VP_IDX2_CNT - 1;
		}
		for (j = lb_idx2; j <= ub_idx2; j++) {
			/* access must be performed physical */
			vp3 = (void *)((long)vp2->vp[j] + kvo);

			for (k = 0; k <= VP_IDX3_CNT - 1; k++) {
				pted_allocated++;
				pa = pmap_steal_avail(sizeof(struct pte_desc),
				    4, &va);
				pted = va;
				vp3->vp[k] = pted;
			}
		}
	}

	pa = pmap_steal_avail(Lx_TABLE_ALIGN, Lx_TABLE_ALIGN, &va);
	memset((void *)pa, 0, Lx_TABLE_ALIGN);
	pmap_kernel()->pm_pt0pa = pa;

	/* now that we have mapping space for everything, lets map it */
	/* all of these mappings are ram -> kernel va */

	/*
	 * enable mappings for existing 'allocated' mapping in the bootstrap
	 * page tables
	 */
	extern uint64_t *pagetable;
	extern char _end[];
	vp2 = (void *)((long)&pagetable + kvo);
	struct mem_region *mp;
	ssize_t size;
	for (mp = pmap_allocated; mp->size != 0; mp++) {
		/* bounds may be kinda messed up */
		for (pa = mp->start, size = mp->size & ~0xfff;
		    size > 0;
		    pa+= L2_SIZE, size -= L2_SIZE)
		{
			paddr_t mappa = pa & ~(L2_SIZE-1);
			vaddr_t mapva = mappa - kvo;
			int prot = PROT_READ | PROT_WRITE;

			if (mapva < (vaddr_t)_end)
				continue;

			if (mapva >= (vaddr_t)__text_start &&
			    mapva < (vaddr_t)_etext)
				prot = PROT_READ | PROT_EXEC;
			else if (mapva >= (vaddr_t)__rodata_start &&
			    mapva < (vaddr_t)_erodata)
				prot = PROT_READ;

			vp2->l2[VP_IDX2(mapva)] = mappa | L2_BLOCK |
			    ATTR_IDX(PTE_ATTR_WB) | ATTR_SH(SH_INNER) |
			    ATTR_nG | ap_bits_kern[prot];
		}
	}

	pmap_avail_fixup();

	/*
	 * At this point we are still running on the bootstrap page
	 * tables however all memory for the final page tables is
	 * 'allocated' and should now be mapped.  This means we are
	 * able to use the virtual addressing to enter the final
	 * mappings into the new mapping tables.
	 */
	vstart = pmap_map_stolen(kernelstart);

	void (switch_mmu_kernel)(long);
	void (*switch_mmu_kernel_table)(long) =
	    (void *)((long)&switch_mmu_kernel + kvo);
	switch_mmu_kernel_table(pt1pa);

	printf("all mapped\n");

	curcpu()->ci_curpm = pmap_kernel();

	vmmap = vstart;
	vstart += PAGE_SIZE;

	return vstart;
}

void
pmap_set_l1(struct pmap *pm, uint64_t va, struct pmapvp1 *l1_va, paddr_t l1_pa)
{
	uint64_t pg_entry;
	int idx0;

	if (l1_pa == 0) {
		/*
		 * if this is called from pmap_vp_enter, this is a
		 * normally mapped page, call pmap_extract to get pa
		 */
		pmap_extract(pmap_kernel(), (vaddr_t)l1_va, &l1_pa);
	}

	if (l1_pa & (Lx_TABLE_ALIGN-1))
		panic("misaligned L2 table\n");

	pg_entry = VP_Lx(l1_pa);

	idx0 = VP_IDX0(va);
	pm->pm_vp.l0->vp[idx0] = l1_va;
	pm->pm_vp.l0->l0[idx0] = pg_entry;
}

void
pmap_set_l2(struct pmap *pm, uint64_t va, struct pmapvp2 *l2_va, paddr_t l2_pa)
{
	uint64_t pg_entry;
	struct pmapvp1 *vp1;
	int idx0, idx1;

	if (l2_pa == 0) {
		/*
		 * if this is called from pmap_vp_enter, this is a
		 * normally mapped page, call pmap_extract to get pa
		 */
		pmap_extract(pmap_kernel(), (vaddr_t)l2_va, &l2_pa);
	}

	if (l2_pa & (Lx_TABLE_ALIGN-1))
		panic("misaligned L2 table\n");

	pg_entry = VP_Lx(l2_pa);

	idx0 = VP_IDX0(va);
	idx1 = VP_IDX1(va);
	if (pm->have_4_level_pt)
		vp1 = pm->pm_vp.l0->vp[idx0];
	else
		vp1 = pm->pm_vp.l1;
	vp1->vp[idx1] = l2_va;
	vp1->l1[idx1] = pg_entry;
}

void
pmap_set_l3(struct pmap *pm, uint64_t va, struct pmapvp3 *l3_va, paddr_t l3_pa)
{
	uint64_t pg_entry;
	struct pmapvp1 *vp1;
	struct pmapvp2 *vp2;
	int idx0, idx1, idx2;

	if (l3_pa == 0) {
		/*
		 * if this is called from pmap_vp_enter, this is a
		 * normally mapped page, call pmap_extract to get pa
		 */
		pmap_extract(pmap_kernel(), (vaddr_t)l3_va, &l3_pa);
	}

	if (l3_pa & (Lx_TABLE_ALIGN-1))
		panic("misaligned L2 table\n");

	pg_entry = VP_Lx(l3_pa);

	idx0 = VP_IDX0(va);
	idx1 = VP_IDX1(va);
	idx2 = VP_IDX2(va);
	if (pm->have_4_level_pt)
		vp1 = pm->pm_vp.l0->vp[idx0];
	else
		vp1 = pm->pm_vp.l1;
	vp2 = vp1->vp[idx1];
	vp2->vp[idx2] = l3_va;
	vp2->l2[idx2] = pg_entry;
}

/*
 * activate a pmap entry
 */
void
pmap_activate(struct proc *p)
{
	pmap_t pm = p->p_vmspace->vm_map.pmap;
	int psw;

	psw = disable_interrupts();
	if (p == curproc && pm != curcpu()->ci_curpm)
		pmap_setttb(p);
	restore_interrupts(psw);
}

/*
 * deactivate a pmap entry
 */
void
pmap_deactivate(struct proc *p)
{
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
	if ((prot & PROT_EXEC) == 0) {
		pted->pted_va &= ~PROT_EXEC;
		pted->pted_pte &= ~PROT_EXEC;
	}
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
	struct pte_desc *pted;
	struct pmap *pm;

	if (prot != PROT_NONE) {
		mtx_enter(&pg->mdpage.pv_mtx);
		LIST_FOREACH(pted, &(pg->mdpage.pv_list), pted_pv_list) {
			pmap_page_ro(pted->pted_pmap, pted->pted_va, prot);
		}
		mtx_leave(&pg->mdpage.pv_mtx);
	}

	mtx_enter(&pg->mdpage.pv_mtx);
	while ((pted = LIST_FIRST(&(pg->mdpage.pv_list))) != NULL) {
		pmap_reference(pted->pted_pmap);
		pm = pted->pted_pmap;
		mtx_leave(&pg->mdpage.pv_mtx);

		pmap_lock(pm);

		/*
		 * We dropped the pvlist lock before grabbing the pmap
		 * lock to avoid lock ordering problems.  This means
		 * we have to check the pvlist again since somebody
		 * else might have modified it.  All we care about is
		 * that the pvlist entry matches the pmap we just
		 * locked.  If it doesn't, unlock the pmap and try
		 * again.
		 */
		mtx_enter(&pg->mdpage.pv_mtx);
		pted = LIST_FIRST(&(pg->mdpage.pv_list));
		if (pted == NULL || pted->pted_pmap != pm) {
			mtx_leave(&pg->mdpage.pv_mtx);
			pmap_unlock(pm);
			pmap_destroy(pm);
			mtx_enter(&pg->mdpage.pv_mtx);
			continue;
		}
		mtx_leave(&pg->mdpage.pv_mtx);

		pmap_remove_pted(pm, pted);
		pmap_unlock(pm);
		pmap_destroy(pm);

		mtx_enter(&pg->mdpage.pv_mtx);
	}
	/* page is being reclaimed, sync icache next use */
	atomic_clearbits_int(&pg->pg_flags, PG_PMAP_EXE);
	mtx_leave(&pg->mdpage.pv_mtx);
}

void
pmap_protect(pmap_t pm, vaddr_t sva, vaddr_t eva, vm_prot_t prot)
{
	if (prot & (PROT_READ | PROT_EXEC)) {
		pmap_lock(pm);
		while (sva < eva) {
			pmap_page_ro(pm, sva, prot);
			sva += PAGE_SIZE;
		}
		pmap_unlock(pm);
		return;
	}
	pmap_remove(pm, sva, eva);
}

void
pmap_init(void)
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
	tcr |= TCR_A1;
	WRITE_SPECIALREG(tcr_el1, tcr);

	pool_init(&pmap_pmap_pool, sizeof(struct pmap), 0, IPL_NONE, 0,
	    "pmap", NULL);
	pool_setlowat(&pmap_pmap_pool, 2);
	pool_init(&pmap_pted_pool, sizeof(struct pte_desc), 0, IPL_VM, 0,
	    "pted", NULL);
	pool_setlowat(&pmap_pted_pool, 20);
	pool_init(&pmap_vp_pool, sizeof(struct pmapvp0), PAGE_SIZE, IPL_VM, 0,
	    "vp", &pmap_vp_allocator);
	/* pool_setlowat(&pmap_vp_pool, 20); */

	pmap_initialized = 1;
}

void
pmap_proc_iflush(struct process *pr, vaddr_t va, vsize_t len)
{
	struct pmap *pm = vm_map_pmap(&pr->ps_vmspace->vm_map);
	vaddr_t kva = zero_page + cpu_number() * PAGE_SIZE;
	paddr_t pa;
	vsize_t clen;
	vsize_t off;

	/*
	 * If we're caled for the current processes, we can simply
	 * flush the data cache to the point of unification and
	 * invalidate the instruction cache.
	 */
	if (pr == curproc->p_p) {
		cpu_icache_sync_range(va, len);
		return;
	}

	/*
	 * Flush and invalidate through an aliased mapping.  This
	 * assumes the instruction cache is PIPT.  That is only true
	 * for some of the hardware we run on.
	 */
	while (len > 0) {
		/* add one to always round up to the next page */
		clen = round_page(va + 1) - va;
		if (clen > len)
			clen = len;

		off = va - trunc_page(va);
		if (pmap_extract(pm, trunc_page(va), &pa)) {
			pmap_kenter_pa(kva, pa, PROT_READ|PROT_WRITE);
			cpu_icache_sync_range(kva + off, clen);
			pmap_kremove_pg(kva);
		}

		len -= clen;
		va += clen;
	}
}

void
pmap_pte_insert(struct pte_desc *pted)
{
	/* put entry into table */
	/* need to deal with ref/change here */
	pmap_t pm = pted->pted_pmap;
	uint64_t *pl3;

	if (pmap_vp_lookup(pm, pted->pted_va, &pl3) == NULL) {
		panic("%s: have a pted, but missing a vp"
		    " for %lx va pmap %p", __func__, pted->pted_va, pm);
	}

	pmap_pte_update(pted, pl3);
}

void
pmap_pte_update(struct pte_desc *pted, uint64_t *pl3)
{
	uint64_t pte, access_bits;
	pmap_t pm = pted->pted_pmap;
	uint64_t attr = ATTR_nG;

	/* see mair in locore.S */
	switch (pted->pted_va & PMAP_CACHE_BITS) {
	case PMAP_CACHE_WB:
		/* inner and outer writeback */
		attr |= ATTR_IDX(PTE_ATTR_WB);
		attr |= ATTR_SH(SH_INNER);
		break;
	case PMAP_CACHE_WT:
		 /* inner and outer writethrough */
		attr |= ATTR_IDX(PTE_ATTR_WT);
		attr |= ATTR_SH(SH_INNER);
		break;
	case PMAP_CACHE_CI:
		attr |= ATTR_IDX(PTE_ATTR_CI);
		attr |= ATTR_SH(SH_INNER);
		break;
	case PMAP_CACHE_DEV:
		attr |= ATTR_IDX(PTE_ATTR_DEV);
		attr |= ATTR_SH(SH_INNER);
		break;
	default:
		panic("pmap_pte_insert: invalid cache mode");
	}

	if (pm->pm_privileged)
		access_bits = ap_bits_kern[pted->pted_pte & PROT_MASK];
	else
		access_bits = ap_bits_user[pted->pted_pte & PROT_MASK];

	pte = (pted->pted_pte & PTE_RPGN) | attr | access_bits | L3_P;
	*pl3 = pte;
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

	if (pm->have_4_level_pt)
		vp1 = pm->pm_vp.l0->vp[VP_IDX0(pted->pted_va)];
	else
		vp1 = pm->pm_vp.l1;
	if (vp1->vp[VP_IDX1(pted->pted_va)] == NULL) {
		panic("have a pted, but missing the l2 for %lx va pmap %p",
		    pted->pted_va, pm);
	}
	vp2 = vp1->vp[VP_IDX1(pted->pted_va)];
	if (vp2 == NULL) {
		panic("have a pted, but missing the l2 for %lx va pmap %p",
		    pted->pted_va, pm);
	}
	vp3 = vp2->vp[VP_IDX2(pted->pted_va)];
	if (vp3 == NULL) {
		panic("have a pted, but missing the l2 for %lx va pmap %p",
		    pted->pted_va, pm);
	}
	vp3->l3[VP_IDX3(pted->pted_va)] = 0;
	if (remove_pted)
		vp3->vp[VP_IDX3(pted->pted_va)] = NULL;

	ttlb_flush(pm, pted->pted_va);
}

/*
 * This function exists to do software referenced/modified emulation.
 * It's purpose is to tell the caller that a fault was generated either
 * for this emulation, or to tell the caller that it's a legit fault.
 */
int
pmap_fault_fixup(pmap_t pm, vaddr_t va, vm_prot_t ftype, int user)
{
	struct pte_desc *pted;
	struct vm_page *pg;
	paddr_t pa;
	uint64_t *pl3 = NULL;
	int need_sync = 0;
	int retcode = 0;

	pmap_lock(pm);

	/* Every VA needs a pted, even unmanaged ones. */
	pted = pmap_vp_lookup(pm, va, &pl3);
	if (!pted || !PTED_VALID(pted))
		goto done;

	/* There has to be a PA for the VA, get it. */
	pa = (pted->pted_pte & PTE_RPGN);

	/* If it's unmanaged, it must not fault. */
	pg = PHYS_TO_VM_PAGE(pa);
	if (pg == NULL)
		goto done;

	/*
	 * Check based on fault type for mod/ref emulation.
	 * if L3 entry is zero, it is not a possible fixup
	 */
	if (*pl3 == 0)
		goto done;

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
		atomic_setbits_int(&pg->pg_flags, PG_PMAP_MOD|PG_PMAP_REF);
		atomic_clearbits_int(&pg->pg_flags, PG_PMAP_EXE);

		/* Thus, enable read, write and exec. */
		pted->pted_pte |=
		    (pted->pted_va & (PROT_READ|PROT_WRITE|PROT_EXEC));
	} else if ((ftype & PROT_EXEC) && /* fault caused by an exec */
	    !(pted->pted_pte & PROT_EXEC) && /* and exec is disabled now */
	    (pted->pted_va & PROT_EXEC)) { /* but is supposedly allowed */

		/*
		 * Exec always includes a reference. Since we now know
		 * the page has been accesed, we can enable read as well
		 * if UVM allows it.
		 */
		atomic_setbits_int(&pg->pg_flags, PG_PMAP_REF);

		/* Thus, enable read and exec. */
		pted->pted_pte |= (pted->pted_va & (PROT_READ|PROT_EXEC));
	} else if ((ftype & PROT_READ) && /* fault caused by a read */
	    !(pted->pted_pte & PROT_READ) && /* and read is disabled now */
	    (pted->pted_va & PROT_READ)) { /* but is supposedly allowed */

		/*
		 * Page referenced emulation. Since we now know the page
		 * has been accessed, we can enable exec as well if UVM
		 * allows it.
		 */
		atomic_setbits_int(&pg->pg_flags, PG_PMAP_REF);

		/* Thus, enable read and exec. */
		pted->pted_pte |= (pted->pted_va & (PROT_READ|PROT_EXEC));
	} else {
		/* didn't catch it, so probably broken */
		goto done;
	}

	/* We actually made a change, so flush it and sync. */
	pmap_pte_update(pted, pl3);

	/* Flush tlb. */
	ttlb_flush(pm, va & ~PAGE_MASK);

	/*
	 * If this is a page that can be executed, make sure to invalidate
	 * the instruction cache if the page has been modified or not used
	 * yet.
	 */
	if (pted->pted_va & PROT_EXEC) {
		need_sync = ((pg->pg_flags & PG_PMAP_EXE) == 0);
		atomic_setbits_int(&pg->pg_flags, PG_PMAP_EXE);
		if (need_sync)
			cpu_icache_sync_range(va & ~PAGE_MASK, PAGE_SIZE);
	}

	retcode = 1;
done:
	pmap_unlock(pm);
	return retcode;
}

void
pmap_postinit(void)
{
	extern char trampoline_vectors[];
	paddr_t pa;
	vaddr_t minaddr, maxaddr;
	u_long npteds, npages;

	memset(pmap_tramp.pm_vp.l1, 0, sizeof(struct pmapvp1));
	pmap_extract(pmap_kernel(), (vaddr_t)trampoline_vectors, &pa);
	pmap_enter(&pmap_tramp, (vaddr_t)trampoline_vectors, pa,
	    PROT_READ | PROT_EXEC, PROT_READ | PROT_EXEC | PMAP_WIRED);

	/*
	 * Reserve enough virtual address space to grow the kernel
	 * page tables.  We need a descriptor for each page as well as
	 * an extra page for level 1/2/3 page tables for management.
	 * To simplify the code, we always allocate full tables at
	 * level 3, so take that into account.
	 */
	npteds = (VM_MAX_KERNEL_ADDRESS - pmap_maxkvaddr + 1) / PAGE_SIZE;
	npteds = roundup(npteds, VP_IDX3_CNT);
	npages = howmany(npteds, PAGE_SIZE / (sizeof(struct pte_desc)));
	npages += 2 * howmany(npteds, VP_IDX3_CNT);
	npages += 2 * howmany(npteds, VP_IDX3_CNT * VP_IDX2_CNT);
	npages += 2 * howmany(npteds, VP_IDX3_CNT * VP_IDX2_CNT * VP_IDX1_CNT);

	/*
	 * Use an interrupt safe map such that we don't recurse into
	 * uvm_map() to allocate map entries.
	 */
	minaddr = vm_map_min(kernel_map);
	pmap_kvp_map = uvm_km_suballoc(kernel_map, &minaddr, &maxaddr,
	    npages * PAGE_SIZE, VM_MAP_INTRSAFE, FALSE, NULL);
}

void
pmap_update(pmap_t pm)
{
}

int
pmap_is_referenced(struct vm_page *pg)
{
	return ((pg->pg_flags & PG_PMAP_REF) != 0);
}

int
pmap_is_modified(struct vm_page *pg)
{
	return ((pg->pg_flags & PG_PMAP_MOD) != 0);
}

int
pmap_clear_modify(struct vm_page *pg)
{
	struct pte_desc *pted;
	uint64_t *pl3 = NULL;

	atomic_clearbits_int(&pg->pg_flags, PG_PMAP_MOD);

	mtx_enter(&pg->mdpage.pv_mtx);
	LIST_FOREACH(pted, &(pg->mdpage.pv_list), pted_pv_list) {
		if (pmap_vp_lookup(pted->pted_pmap, pted->pted_va & ~PAGE_MASK, &pl3) == NULL)
			panic("failed to look up pte\n");
		*pl3  |= ATTR_AP(2);
		pted->pted_pte &= ~PROT_WRITE;

		ttlb_flush(pted->pted_pmap, pted->pted_va & ~PAGE_MASK);
	}
	mtx_leave(&pg->mdpage.pv_mtx);

	return 0;
}

/*
 * When this turns off read permissions it also disables write permissions
 * so that mod is correctly tracked after clear_ref; FAULT_READ; FAULT_WRITE;
 */
int
pmap_clear_reference(struct vm_page *pg)
{
	struct pte_desc *pted;

	atomic_clearbits_int(&pg->pg_flags, PG_PMAP_REF);

	mtx_enter(&pg->mdpage.pv_mtx);
	LIST_FOREACH(pted, &(pg->mdpage.pv_list), pted_pv_list) {
		pted->pted_pte &= ~PROT_MASK;
		pmap_pte_insert(pted);
		ttlb_flush(pted->pted_pmap, pted->pted_va & ~PAGE_MASK);
	}
	mtx_leave(&pg->mdpage.pv_mtx);

	return 0;
}

void
pmap_copy(pmap_t dst_pmap, pmap_t src_pmap, vaddr_t dst_addr,
	vsize_t len, vaddr_t src_addr)
{
	/* NOOP */
}

void
pmap_unwire(pmap_t pm, vaddr_t va)
{
	struct pte_desc *pted;

	pted = pmap_vp_lookup(pm, va, NULL);
	if ((pted != NULL) && (pted->pted_va & PTED_VA_WIRED_M)) {
		pm->pm_stats.wired_count--;
		pted->pted_va &= ~PTED_VA_WIRED_M;
	}
}

void
pmap_remove_holes(struct vmspace *vm)
{
	/* NOOP */
}

void
pmap_virtual_space(vaddr_t *start, vaddr_t *end)
{
	*start = virtual_avail;
	*end = virtual_end;
}

void
pmap_setup_avail(uint64_t ram_start, uint64_t ram_end, uint64_t kvo)
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
	panic ("unable to allocate region with size %lx align %x",
	    size, align);
}

vaddr_t
pmap_map_stolen(vaddr_t kernel_start)
{
	struct mem_region *mp;
	paddr_t pa;
	vaddr_t va;
	uint64_t e;

	for (mp = pmap_allocated; mp->size; mp++) {
		for (e = 0; e < mp->size; e += PAGE_SIZE) {
			int prot = PROT_READ | PROT_WRITE;

			pa = mp->start + e;
			va = pa - pmap_avail_kvo;

			if (va < VM_MIN_KERNEL_ADDRESS ||
			    va >= VM_MAX_KERNEL_ADDRESS)
				continue;

			if (va >= (vaddr_t)__text_start &&
			    va < (vaddr_t)_etext)
				prot = PROT_READ | PROT_EXEC;
			else if (va >= (vaddr_t)__rodata_start &&
			    va < (vaddr_t)_erodata)
				prot = PROT_READ;

			pmap_kenter_cache(va, pa, prot, PMAP_CACHE_WB);
		}
	}

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
	struct pmap *pm;
	uint64_t ttbr0, tcr;

	printf("showing mapping of %llx\n", va);

	if (va & 1ULL << 63)
		pm = pmap_kernel();
	else
		pm = curproc->p_vmspace->vm_map.pmap;

	if (pm->have_4_level_pt) {
		printf("  vp0 = %p off %x\n",  pm->pm_vp.l0, VP_IDX0(va)*8);
		vp1 = pm->pm_vp.l0->vp[VP_IDX0(va)];
		if (vp1 == NULL)
			return;
	} else {
		vp1 = pm->pm_vp.l1;
	}

	__asm volatile ("mrs     %x0, ttbr0_el1" : "=r"(ttbr0));
	__asm volatile ("mrs     %x0, tcr_el1" : "=r"(tcr));
	printf("  ttbr0 %llx %llx tcr %llx\n", ttbr0, pm->pm_pt0pa, tcr);
	printf("  vp1 = %p\n", vp1);

	vp2 = vp1->vp[VP_IDX1(va)];
	printf("  vp2 = %p lp2 = %llx idx1 off %x\n",
		vp2, vp1->l1[VP_IDX1(va)], VP_IDX1(va)*8);
	if (vp2 == NULL)
		return;

	vp3 = vp2->vp[VP_IDX2(va)];
	printf("  vp3 = %p lp3 = %llx idx2 off %x\n",
		vp3, vp2->l2[VP_IDX2(va)], VP_IDX2(va)*8);
	if (vp3 == NULL)
		return;

	pted = vp3->vp[VP_IDX3(va)];
	printf("  pted = %p lp3 = %llx idx3 off  %x\n",
		pted, vp3->l3[VP_IDX3(va)], VP_IDX3(va)*8);
}

void
pmap_map_early(paddr_t spa, psize_t len)
{
	extern pd_entry_t pagetable_l0_ttbr0[];
	extern pd_entry_t pagetable_l1_ttbr0[];
	paddr_t pa, epa = spa + len;

	for (pa = spa & ~(L1_SIZE - 1); pa < epa; pa += L1_SIZE) {
		if (pagetable_l0_ttbr0[VP_IDX0(pa)] == 0)
			panic("%s: outside existing L0 entry", __func__);

		pagetable_l1_ttbr0[VP_IDX1(pa)] = pa | L1_BLOCK |
		    ATTR_IDX(PTE_ATTR_WB) | ATTR_SH(SH_INNER) |
		    ATTR_nG | ATTR_UXN | ATTR_AF | ATTR_AP(0);
	}
	__asm volatile("dsb sy");
	__asm volatile("isb");
}

/*
 * We allocate ASIDs in pairs.  The first ASID is used to run the
 * kernel and has both userland and the full kernel mapped.  The
 * second ASID is used for running userland and has only the
 * trampoline page mapped in addition to userland.
 */

#define NUM_ASID (1 << 16)
uint32_t pmap_asid[NUM_ASID / 32];

void
pmap_allocate_asid(pmap_t pm)
{
	uint32_t bits;
	int asid, bit;

	for (;;) {
		do {
			asid = arc4random() & (NUM_ASID - 2);
			bit = (asid & (32 - 1));
			bits = pmap_asid[asid / 32];
		} while (asid == 0 || (bits & (3U << bit)));

		if (atomic_cas_uint(&pmap_asid[asid / 32], bits,
		    bits | (3U << bit)) == bits)
			break;
	}
	pm->pm_asid = asid;
}

void
pmap_free_asid(pmap_t pm)
{
	uint32_t bits;
	int bit;

	KASSERT(pm != curcpu()->ci_curpm);
	cpu_tlb_flush_asid_all((uint64_t)pm->pm_asid << 48);
	cpu_tlb_flush_asid_all((uint64_t)(pm->pm_asid | ASID_USER) << 48);

	bit = (pm->pm_asid & (32 - 1));
	for (;;) {
		bits = pmap_asid[pm->pm_asid / 32];
		if (atomic_cas_uint(&pmap_asid[pm->pm_asid / 32], bits,
		    bits & ~(3U << bit)) == bits)
			break;
	}
}

void
pmap_setttb(struct proc *p)
{
	struct cpu_info *ci = curcpu();
	pmap_t pm = p->p_vmspace->vm_map.pmap;

	WRITE_SPECIALREG(ttbr0_el1, pmap_kernel()->pm_pt0pa);
	__asm volatile("isb");
	cpu_setttb(pm->pm_asid, pm->pm_pt0pa);
	ci->ci_flush_bp();
	ci->ci_curpm = pm;
}
