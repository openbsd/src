/*	$OpenBSD: pmap.c,v 1.77 2002/10/13 18:26:12 krw Exp $ */

/*
 * Copyright (c) 2001, 2002 Dale Rahn. All rights reserved.
 *
 *   
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Dale Rahn.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Effort sponsored in part by the Defense Advanced Research Projects
 * Agency (DARPA) and Air Force Research Laboratory, Air Force
 * Materiel Command, USAF, under agreement number F30602-01-2-0537.
 */  

#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/systm.h>
#include <sys/pool.h>

#include <uvm/uvm.h>

#include <machine/pcb.h>
#include <machine/powerpc.h>
#include <machine/pmap.h>

#include <machine/db_machdep.h>
#include <ddb/db_extern.h>
#include <ddb/db_output.h>

struct pmap kernel_pmap_;
static int npgs;
static struct mem_region *pmap_mem, *pmap_avail;
struct mem_region pmap_allocated[10];
int pmap_cnt_avail;
int pmap_cnt_allocated;


void * pmap_pvh;
void * pmap_attrib;
struct pte  *pmap_ptable;
int	pmap_ptab_cnt;
u_int	pmap_ptab_mask;
#define HTABSIZE (pmap_ptab_cnt * 64)

static u_int usedsr[NPMAPS / sizeof(u_int) / 8];
paddr_t zero_page;
paddr_t copy_src_page;
paddr_t copy_dst_page;

/* P->V table */
LIST_HEAD(pted_pv_head, pte_desc);

struct pte_desc {
	/* Linked list of phys -> virt entries */
	LIST_ENTRY(pte_desc) pted_pv_list;
	struct pte pted_pte;
	pmap_t pted_pmap;
	vaddr_t pted_va;
};

void print_pteg(pmap_t pm, vaddr_t va);

static inline void tlbsync(void);
static inline void tlbie(vaddr_t ea);
static inline void tlbia(void);

void pmap_attr_save(paddr_t pa, u_int32_t bits);
void pmap_page_ro(pmap_t pm, vaddr_t va);

/*
 * LOCKING structures.
 * This may not be correct, and doesn't do anything yet.
 */
#define pmap_simplelock_pm(pm)
#define pmap_simpleunlock_pm(pm)
#define pmap_simplelock_pv(pm)
#define pmap_simpleunlock_pv(pm)


/* VP routines */
void pmap_vp_enter(pmap_t pm, vaddr_t va, struct pte_desc *pted);
struct pte_desc *pmap_vp_remove(pmap_t pm, vaddr_t va);
void pmap_vp_destroy(pmap_t pm);
struct pte_desc *pmap_vp_lookup(pmap_t pm, vaddr_t va);

/* PV routines */
int pmap_enter_pv(struct pte_desc *pted, struct pted_pv_head *);
void pmap_remove_pv(struct pte_desc *pted);


/* pte hash table routines */
void pte_insert(struct pte_desc *pted);
void pmap_hash_remove(struct pte_desc *pted);
void pmap_fill_pte(pmap_t pm, vaddr_t va, paddr_t pa,
    struct pte_desc *pted, vm_prot_t prot, int flags, int cache);

void pmap_syncicache_user_virt(pmap_t pm, vaddr_t va);

void _pmap_kenter_pa(vaddr_t va, paddr_t pa, vm_prot_t prot, int flags,
    int cache);
void pmap_remove_pg(pmap_t pm, vaddr_t va);
void pmap_kremove_pg(vaddr_t va);

/* setup/initialization functions */
void pmap_avail_setup(void);
void pmap_avail_fixup(void);
void pmap_remove_avail(paddr_t base, paddr_t end);
void *pmap_steal_avail(size_t size, int align);

/* asm interface */
int pte_spill_r(u_int32_t va, u_int32_t msr, u_int32_t access_type,
    int exec_fault);

u_int32_t pmap_setusr(pmap_t pm, vaddr_t va);
void pmap_popusr(u_int32_t oldsr);

/* pte invalidation */
void pte_zap(struct pte *ptp, struct pte_desc *pted);

/* debugging */
void pmap_print_pted(struct pte_desc *pted, int(*print)(const char *, ...));

/* XXX - panic on pool get failures? */
struct pool pmap_pmap_pool;
struct pool pmap_vp_pool;
struct pool pmap_pted_pool;

int pmap_initialized = 0;
int physmem;

#define ATTRSHIFT	4

/* virtual to physical helpers */
static inline int
VP_SR(vaddr_t va)
{
	return (va >>VP_SR_POS) & VP_SR_MASK;
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

#if VP_IDX1_SIZE != VP_IDX2_SIZE 
#error pmap allocation code expects IDX1 and IDX2 size to be same
#endif
struct pmapvp {
	void *vp[VP_IDX1_SIZE];
};


/*
 * VP routines, virtual to physical translation information.
 * These data structures are based off of the pmap, per process.
 */

/*
 * This is used for pmap_kernel() mappings, they are not to be removed
 * from the vp table because they were statically initialized at the
 * initial pmap initialization. This is so that memory allocation 
 * is not necessary in the pmap_kernel() mappings.
 * otherwise bad race conditions can appear.
 */
struct pte_desc *
pmap_vp_lookup(pmap_t pm, vaddr_t va)
{
	struct pmapvp *vp1;
	struct pmapvp *vp2;
	struct pte_desc *pted;

	vp1 = pm->pm_vp[VP_SR(va)];
	if (vp1 == NULL) {
		return NULL;
	}

	vp2 = vp1->vp[VP_IDX1(va)];
	if (vp2 == NULL) {
		return NULL;
	}

	pted = vp2->vp[VP_IDX2(va)];

	return pted;
}

/*
 * Remove, and return, pted at specified address, NULL if not present
 */
struct pte_desc *
pmap_vp_remove(pmap_t pm, vaddr_t va)
{
	struct pmapvp *vp1;
	struct pmapvp *vp2;
	struct pte_desc *pted;

	vp1 = pm->pm_vp[VP_SR(va)];
	if (vp1 == NULL) {
		return NULL;
	}

	vp2 = vp1->vp[VP_IDX1(va)];
	if (vp2 == NULL) {
		return NULL;
	}

	pted = vp2->vp[VP_IDX2(va)];
	vp2->vp[VP_IDX2(va)] = NULL;

	return pted;
}

/*
 * Create a V -> P mapping for the given pmap and virtual address
 * with reference to the pte descriptor that is used to map the page.
 * This code should track allocations of vp table allocations
 * so they can be freed efficiently.
 * 
 * should this be called under splimp?
 */
void
pmap_vp_enter(pmap_t pm, vaddr_t va, struct pte_desc *pted)
{
	struct pmapvp *vp1;
	struct pmapvp *vp2;
	int s;

	pmap_simplelock_pm(pm);

	vp1 = pm->pm_vp[VP_SR(va)];
	if (vp1 == NULL) {
		s = splimp();
		vp1 = pool_get(&pmap_vp_pool, PR_NOWAIT);
		splx(s);
		bzero(vp1, sizeof (struct pmapvp));
		pm->pm_vp[VP_SR(va)] = vp1;
	}

	vp2 = vp1->vp[VP_IDX1(va)];
	if (vp2 == NULL) {
		s = splimp();
		vp2 = pool_get(&pmap_vp_pool, PR_NOWAIT);
		splx(s);
		bzero(vp2, sizeof (struct pmapvp));
		vp1->vp[VP_IDX1(va)] = vp2;
	}

	vp2->vp[VP_IDX2(va)] = pted;

	pmap_simpleunlock_pm(pm);
}

/* 
 * HELPER FUNCTIONS 
 */
static inline struct pted_pv_head *
pmap_find_pvh(paddr_t pa)
{
	int bank, off;
	bank = vm_physseg_find(atop(pa), &off);
	if (bank != -1) {
		return &vm_physmem[bank].pmseg.pvent[off];
	}
	return NULL;
}

static inline char *
pmap_find_attr(paddr_t pa)
{
	int bank, off;
	bank = vm_physseg_find(atop(pa), &off);
	if (bank != -1) {
		return &vm_physmem[bank].pmseg.attrs[off];
	}
	return NULL;
}

/* PTE manipulation/calculations */
static inline void
tlbie(vaddr_t va)
{
	__asm volatile ("tlbie %0" :: "r"(va));
}

static inline void
tlbsync(void)
{
	__asm volatile ("sync; tlbsync; sync");
}

static inline void
tlbia()
{
	vaddr_t va;

	__asm volatile ("sync");
	for (va = 0; va < 0x00040000; va += 0x00001000)
		tlbie(va);

	tlbsync();
}

static inline int
ptesr(sr_t *sr, vaddr_t va)
{
	return sr[(u_int)va >> ADDR_SR_SHIFT];
}

static inline int 
pteidx(sr_t sr, vaddr_t va)
{
	int hash;
	hash = (sr & SR_VSID) ^ (((u_int)va & ADDR_PIDX) >> ADDR_PIDX_SHIFT);
	return hash & pmap_ptab_mask;
}

#define PTED_VA_PTEGIDX_M	0x07
#define PTED_VA_HID_M		0x08
#define PTED_VA_MANAGED_M	0x10
#define PTED_VA_WIRED_M		0x20
#define PTED_VA_EXEC_M		0x40

static inline u_int32_t
PTED_HID(struct pte_desc *pted)
{
	return (pted->pted_va & PTED_VA_HID_M); 
}

static inline u_int32_t
PTED_PTEGIDX(struct pte_desc *pted)
{
	return (pted->pted_va & PTED_VA_PTEGIDX_M); 
}

static inline u_int32_t
PTED_MANAGED(struct pte_desc *pted)
{
	return (pted->pted_va & PTED_VA_MANAGED_M); 
}

static inline u_int32_t
PTED_WIRED(struct pte_desc *pted)
{
	return (pted->pted_va & PTED_VA_WIRED_M); 
}

static inline u_int32_t
PTED_VALID(struct pte_desc *pted)
{
	return (pted->pted_pte.pte_hi & PTE_VALID);
}

/*
 * PV entries -
 * manpulate the physical to virtual translations for the entire system.
 * 
 * QUESTION: should all mapped memory be stored in PV tables? or
 * is it alright to only store "ram" memory. Currently device mappings
 * are not stored.
 * It makes sense to pre-allocate mappings for all of "ram" memory, since
 * it is likely that it will be mapped at some point, but would it also
 * make sense to use a tree/table like is use for pmap to store device
 * mappings.
 * Futher notes: It seems that the PV table is only used for pmap_protect
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
 * have too noticable unnecssary ram consumption.
 */

int
pmap_enter_pv(struct pte_desc *pted, struct pted_pv_head *pvh)
{
	int first;

	if (__predict_false(!pmap_initialized)) {
		return 0;
	}
	if (pvh == NULL) {
		return 0;
	}

	first = LIST_EMPTY(pvh);
	LIST_INSERT_HEAD(pvh, pted, pted_pv_list);
	pted->pted_va |= PTED_VA_MANAGED_M;
	return first;
}

void
pmap_remove_pv(struct pte_desc *pted)
{
	LIST_REMOVE(pted, pted_pv_list);
}

void
pmap_attr_save(paddr_t pa, u_int32_t bits)
{
	int bank, pg;
	u_int8_t *attr;

	bank = vm_physseg_find(atop(pa), &pg);
	if (bank == -1)
		return;
	attr = &vm_physmem[bank].pmseg.attrs[pg];
	*attr |= (u_int8_t)(bits >> ATTRSHIFT);
}

int
pmap_enter(pm, va, pa, prot, flags)
	pmap_t pm;
	vaddr_t va;
	paddr_t pa;
	vm_prot_t prot;
	int flags;
{
	struct pte_desc *pted;
	struct pted_pv_head *pvh;
	int s;
	int need_sync;
	int cache;

	/* MP - Acquire lock for this pmap */

	s = splimp();
	pted = pmap_vp_lookup(pm, va);
	if (pted && PTED_VALID(pted)) {
		pmap_remove_pg(pm, va);
		/* we lost our pted if it was user */
		if (pm != pmap_kernel())
			pted = pmap_vp_lookup(pm, va);
	}

	pm->pm_stats.resident_count++;

	/* Do not have pted for this, get one and put it in VP */
	if (pted == NULL) {
		pted = pool_get(&pmap_pted_pool, PR_NOWAIT);	
		bzero(pted, sizeof (*pted));
		pmap_vp_enter(pm, va, pted);
	}

	/* Calculate PTE */
	pvh = pmap_find_pvh(pa);
	if (pvh != NULL)
		cache = PMAP_CACHE_WB; /* managed memory is cachable */
	else
		cache = PMAP_CACHE_CI;

	pmap_fill_pte(pm, va, pa, pted, prot, flags, cache);

	need_sync = pmap_enter_pv(pted, pvh);

	/*
	 * Insert into HTAB
	 * we were told to map the page, probably called from vm_fault,
	 * so map the page!
	 */
	pte_insert(pted);

        if (prot & VM_PROT_EXECUTE) {
		u_int sn = VP_SR(va);

        	pm->pm_exec[sn]++;
		if (pm->pm_sr[sn] & SR_NOEXEC) {
			pm->pm_sr[sn] &= ~SR_NOEXEC;

			/* set the current sr if not kernel used segemnts
			 * and this pmap is current active pmap
			 */
			if (sn != USER_SR && sn != KERNEL_SR && curpm == pm)
				asm volatile ("mtsrin %0,%1"
				    :: "r"(pm->pm_sr[sn]),
				       "r"(sn << ADDR_SR_SHIFT) );
		}
	}

	splx(s);

	/* only instruction sync executable pages */
	if (need_sync && (prot & VM_PROT_EXECUTE))
		pmap_syncicache_user_virt(pm, va);

	/* MP - free pmap lock */
	return KERN_SUCCESS;
}

/* 
 * Remove the given range of mapping entries.
 */
void
pmap_remove(pmap_t pm, vaddr_t va, vaddr_t endva)
{
	int i_sr, s_sr, e_sr;
	int i_vp1, s_vp1, e_vp1;
	int i_vp2, s_vp2, e_vp2;
	struct pmapvp *vp1;
	struct pmapvp *vp2;

	/* I suspect that if this loop were unrolled better 
	 * it would have better performance, testing i_sr and i_vp1
	 * in the middle loop seems excessive
	 */

	s_sr = VP_SR(va);
	e_sr = VP_SR(endva);
	for (i_sr = s_sr; i_sr <= e_sr; i_sr++) {
		vp1 = pm->pm_vp[i_sr];
		if (vp1 == NULL)
			continue;
		
		if (i_sr == s_sr)
			s_vp1 = VP_IDX1(va);
		else
			s_vp1 = 0;

		if (i_sr == e_sr)
			e_vp1 = VP_IDX1(endva);
		else
			e_vp1 = VP_IDX1_SIZE-1; 

		for (i_vp1 = s_vp1; i_vp1 <= e_vp1; i_vp1++) {
			vp2 = vp1->vp[i_vp1];
			if (vp2 == NULL)
				continue;

			if ((i_sr == s_sr) && (i_vp1 == s_vp1))
				s_vp2 = VP_IDX2(va);
			else
				s_vp2 = 0;

			if ((i_sr == e_sr) && (i_vp1 == e_vp1))
				e_vp2 = VP_IDX2(endva);
			else
				e_vp2 = VP_IDX2_SIZE; 

			for (i_vp2 = s_vp2; i_vp2 < e_vp2; i_vp2++) {
				if (vp2->vp[i_vp2] != NULL) {
					pmap_remove_pg(pm,
					    (i_sr << VP_SR_POS) |
					    (i_vp1 << VP_IDX1_POS) |
					    (i_vp2 << VP_IDX2_POS));
				}
			}
		}
	}
}
/*
 * remove a single mapping, notice that this code is O(1)
 */
void
pmap_remove_pg(pmap_t pm, vaddr_t va)
{
	struct pte_desc *pted;
	int s;

	/*
	 * HASH needs to be locked here as well as pmap, and pv list.
	 * so that we know the mapping information is either valid,
	 * or that the mapping is not present in the hash table.
	 */
	s = splimp();
	if (pm == pmap_kernel()) {
		pted = pmap_vp_lookup(pm, va);
		if (pted == NULL || !PTED_VALID(pted)) {
			splx(s);
			return;
		} 
	} else {
		pted = pmap_vp_remove(pm, va);
		if (pted == NULL || !PTED_VALID(pted)) {
			splx(s);
			return;
		}
	}
	pm->pm_stats.resident_count--;

	pmap_hash_remove(pted);

	if (pted->pted_va & PTED_VA_EXEC_M) {
		u_int sn = VP_SR(va);

		pted->pted_va &= ~PTED_VA_EXEC_M;
		pm->pm_exec[sn]--;
		if (pm->pm_exec[sn] == 0) {
			pm->pm_sr[sn] |= SR_NOEXEC;
			
			/* set the current sr if not kernel used segemnts
			 * and this pmap is current active pmap
			 */
			if (sn != USER_SR && sn != KERNEL_SR && curpm == pm)
				asm volatile ("mtsrin %0,%1"
				    :: "r"(pm->pm_sr[sn]),
				       "r"(sn << ADDR_SR_SHIFT) );
		}
	}

	pted->pted_pte.pte_hi &= ~PTE_VALID;

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
	struct pted_pv_head *pvh;

	pm = pmap_kernel();

	/* MP - lock pmap. */
	s = splimp();

	pted = pmap_vp_lookup(pm, va);
	if (pted && PTED_VALID(pted))
		pmap_kremove_pg(va); /* pted is reused */

	pm->pm_stats.resident_count++;

	/* Do not have pted for this, get one and put it in VP */
	if (pted == NULL) {
		/* XXX - future panic? */
		printf("pted not preallocated in pmap_kernel() va %x pa %x \n",
		    va, pa);
		pted = pool_get(&pmap_pted_pool, PR_NOWAIT);	
		bzero(pted, sizeof (*pted));
		pmap_vp_enter(pm, va, pted);
	}

	pvh = pmap_find_pvh(pa);
	if (cache == PMAP_CACHE_DEFAULT) {
		if (pvh != NULL)
			cache = PMAP_CACHE_WB; /* managed memory is cachable */
		else
			cache = PMAP_CACHE_CI;
	}

	/* Calculate PTE */
	pmap_fill_pte(pm, va, pa, pted, prot, flags, cache);

	/*
	 * Insert into HTAB
	 * we were told to map the page, probably called from vm_fault,
	 * so map the page!
	 */
	pte_insert(pted);
	pted->pted_va |= PTED_VA_WIRED_M;

        if (prot & VM_PROT_EXECUTE) {
		u_int sn = VP_SR(va);

        	pm->pm_exec[sn]++;
		if (pm->pm_sr[sn] & SR_NOEXEC) {
			pm->pm_sr[sn] &= ~SR_NOEXEC;

			/* set the current sr if not kernel used segemnts
			 * and this pmap is current active pmap
			 */
			if (sn != USER_SR && sn != KERNEL_SR && curpm == pm)
				asm volatile ("mtsrin %0,%1"
				    :: "r"(pm->pm_sr[sn]),
				       "r"(sn << ADDR_SR_SHIFT) );
		}
	}

	splx(s);

}

void
pmap_kenter_pa(vaddr_t va, paddr_t pa, vm_prot_t prot)
{
	_pmap_kenter_pa(va, pa, prot, 0, PMAP_CACHE_DEFAULT);
}

void
pmap_kenter_cache(vaddr_t va, paddr_t pa, vm_prot_t prot, int cacheable)
{
	_pmap_kenter_pa(va, pa, prot, 0, cacheable);
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

	pm = pmap_kernel();
	pted = pmap_vp_lookup(pm, va);
	if (pted == NULL)
		return;

	if (!PTED_VALID(pted))
		return; /* not mapped */

	s = splimp();

	pm->pm_stats.resident_count--;

	/*
	 * HASH needs to be locked here as well as pmap, and pv list.
	 * so that we know the mapping information is either valid,
	 * or that the mapping is not present in the hash table.
	 */
	pmap_hash_remove(pted);

	if (pted->pted_va & PTED_VA_EXEC_M) {
		u_int sn = VP_SR(va);

		pted->pted_va &= ~PTED_VA_EXEC_M;
		pm->pm_exec[sn]--;
		if (pm->pm_exec[sn] == 0) {
			pm->pm_sr[sn] |= SR_NOEXEC;

			/* set the current sr if not kernel used segemnts
			 * and this pmap is current active pmap
			 */
			if (sn != USER_SR && sn != KERNEL_SR && curpm == pm)
				asm volatile ("mtsrin %0,%1"
				    :: "r"(pm->pm_sr[sn]),
				       "r"(sn << ADDR_SR_SHIFT) );
		}
	}

	if (PTED_MANAGED(pted))
		pmap_remove_pv(pted);

	/* invalidate pted; */
	pted->pted_pte.pte_hi &= ~PTE_VALID;

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
pte_zap(struct pte *ptp, struct pte_desc *pted)
{
		ptp->pte_hi &= ~PTE_VALID;
		__asm volatile ("sync");
		tlbie(pted->pted_va);
		__asm volatile ("sync");
		tlbsync();
		__asm volatile ("sync");
		if (PTED_MANAGED(pted))
			pmap_attr_save(pted->pted_pte.pte_lo & PTE_RPGN,
			    ptp->pte_lo & (PTE_REF|PTE_CHG));
}

/*
 * remove specified entry from hash table.
 * all information is present in pted to look up entry
 * LOCKS... should the caller lock?
 */
void
pmap_hash_remove(struct pte_desc *pted)
{
	vaddr_t va = pted->pted_va;
	pmap_t pm = pted->pted_pmap;
	struct pte *ptp;
	int sr, idx;

	sr = ptesr(pm->pm_sr, va);
	idx = pteidx(sr, va);

	idx =  (idx ^ (PTED_HID(pted) ? pmap_ptab_mask : 0));
	/* determine which pteg mapping is present in */
	ptp = pmap_ptable + (idx * 8);
	ptp += PTED_PTEGIDX(pted); /* increment by index into pteg */

	/*
	 * We now have the pointer to where it will be, if it is currently
	 * mapped. If the mapping was thrown away in exchange for another
	 * page mapping, then this page is not currently in the HASH.
	 */
	if ((pted->pted_pte.pte_hi | (PTED_HID(pted) ? PTE_HID : 0))
	    == ptp->pte_hi) {
		pte_zap(ptp, pted);
	}
}

/*
 * What about execution control? Even at only a segment granularity.
 */
void
pmap_fill_pte(pmap_t pm, vaddr_t va, paddr_t pa, struct pte_desc *pted,
	vm_prot_t prot, int flags, int cache)
{
	sr_t sr;
	struct pte *pte;

	sr = ptesr(pm->pm_sr, va);
	pte = &pted->pted_pte;
	pte->pte_hi = ((sr & SR_VSID) << PTE_VSID_SHIFT) |
	    ((va >> ADDR_API_SHIFT) & PTE_API) | PTE_VALID;
	pte->pte_lo = (pa & PTE_RPGN);


	if ((cache == PMAP_CACHE_WB))
		pte->pte_lo |= PTE_M;
	else if ((cache == PMAP_CACHE_WT))
		pte->pte_lo |= (PTE_W | PTE_M);
	else
		pte->pte_lo |= (PTE_M | PTE_I | PTE_G);

	if (prot & VM_PROT_WRITE)
		pte->pte_lo |= PTE_RW;
	else
		pte->pte_lo |= PTE_RO;

	pted->pted_va = va & ~PAGE_MASK;

	if (prot & VM_PROT_EXECUTE)
		pted->pted_va  |= PTED_VA_EXEC_M;

	pted->pted_pmap = pm;
}

/*
 * read/clear bits from pte/attr cache, for reference/change
 * ack, copied code in the pte flush code....
 */
int
pteclrbits(paddr_t pa, u_int bit, u_int clear)
{
	char *pattr;
	u_int bits;
	int s;
	struct pte_desc *pted;
	struct pted_pv_head *pvh;

	pattr = pmap_find_attr(pa);

	/* check if managed memory */
	if (pattr == NULL)
		return 0;

	pvh = pmap_find_pvh(pa);

	/*
	 *  First try the attribute cache
	 */
	bits = (*pattr << ATTRSHIFT) & bit;
	if ((bits == bit) && (clear == 0))
		return bits;

	/* cache did not contain all necessary bits,
	 * need to walk thru pv table to collect all mappings for this
	 * page, copying bits to the attribute cache 
	 * then reread the attribute cache.
	 */
	/* need lock for this pv */
	s = splimp();

	LIST_FOREACH(pted, pvh, pted_pv_list) {
		vaddr_t va = pted->pted_va & PAGE_MASK;
		pmap_t pm = pted->pted_pmap;
		struct pte *ptp;
		int sr, idx;

		sr = ptesr(pm->pm_sr, va);
		idx = pteidx(sr, va);

		/* determine which pteg mapping is present in */
		ptp = pmap_ptable +
			(idx ^ (PTED_HID(pted) ? pmap_ptab_mask : 0)) * 8;
		ptp += PTED_PTEGIDX(pted); /* increment by index into pteg */

		/*
		 * We now have the pointer to where it will be, if it is
		 * currently mapped. If the mapping was thrown away in
		 * exchange for another page mapping, then this page is
		 * not currently in the HASH.
		 */
		if ((pted->pted_pte.pte_hi | (PTED_HID(pted) ? PTE_HID : 0))
		    == ptp->pte_hi) {
			bits |=	ptp->pte_lo & (PTE_REF|PTE_CHG);
			if (clear) {
				ptp->pte_hi &= ~PTE_VALID;
				__asm__ volatile ("sync");
				tlbie(va);
				tlbsync();
				ptp->pte_lo &= ~bit;
				__asm__ volatile ("sync");
				ptp->pte_hi |= PTE_VALID;
			}
		}
	}

	bits |= (*pattr << ATTRSHIFT) & bit;
	if (clear)
		*pattr &= ~(bit >> ATTRSHIFT);
	else
		*pattr |= (bits >> ATTRSHIFT);

	splx(s);
	return bits;
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
	 * Is there pool functions which could be called
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
#ifdef USE_DCBZ
	int i;
	paddr_t addr = zero_page;
#endif

	/* simple_lock(&pmap_zero_page_lock); */
	pmap_kenter_pa(zero_page, pa, VM_PROT_READ|VM_PROT_WRITE);
#ifdef USE_DCBZ
	for (i = PAGE_SIZE/CACHELINESIZE; i>0; i--) {
		__asm volatile ("dcbz 0,%0" :: "r"(addr));
		addr += CACHELINESIZE;
	}
#else
	bzero((void *)zero_page, PAGE_SIZE);
#endif
	pmap_kremove_pg(zero_page);
	
	/* simple_unlock(&pmap_zero_page_lock); */
}

/*
 * copy the given physical page with zeros.
 */
void
pmap_copy_page(struct vm_page *srcpg, struct vm_page *dstpg)
{
	paddr_t srcpa = VM_PAGE_TO_PHYS(srcpg);
	paddr_t dstpa = VM_PAGE_TO_PHYS(dstpg);
	/* simple_lock(&pmap_copy_page_lock); */

	pmap_kenter_pa(copy_src_page, srcpa, VM_PROT_READ);
	pmap_kenter_pa(copy_dst_page, dstpa, VM_PROT_READ|VM_PROT_WRITE);

	bcopy((void *)copy_src_page, (void *)copy_dst_page, PAGE_SIZE);
	
	pmap_kremove_pg(copy_src_page);
	pmap_kremove_pg(copy_dst_page);
	/* simple_unlock(&pmap_copy_page_lock); */
}

int pmap_id_avail = 0;

void
pmap_pinit(pmap_t pm)
{
	int i, k, try, tblidx, tbloff;
	int s, seg;

	bzero(pm, sizeof (struct pmap));

	pmap_reference(pm);

	/*
	 * Allocate segment registers for this pmap.
	 * try not to reuse pmap ids, to spread the hash table usage.
	 */
again:
	for (i = 0; i < NPMAPS; i++) {
		try = pmap_id_avail + i;
		try = try % NPMAPS; /* truncate back into bounds */
		tblidx = try / (8 * sizeof usedsr[0]);
		tbloff = try % (8 * sizeof usedsr[0]);
		if ((usedsr[tblidx] & (1 << tbloff)) == 0) {
			/* pmap create lock? */
			s = splimp();
			if ((usedsr[tblidx] & (1 << tbloff)) == 1) {
				/* entry was stolen out from under us, retry */
				splx(s); /* pmap create unlock */
				goto again;
			}
			usedsr[tblidx] |= (1 << tbloff); 
			pmap_id_avail = try + 1;
			splx(s); /* pmap create unlock */

			seg = try << 4;
			for (k = 0; k < 16; k++) {
				pm->pm_sr[k] = (seg + k) | SR_NOEXEC;
			}
			return;
		}
	}
	panic("out of pmap slots");
}

/* 
 * Create and return a physical map.
 */
pmap_t 
pmap_create()
{
	pmap_t pmap;
	int s;

	s = splimp();
	pmap = pool_get(&pmap_pmap_pool, PR_WAITOK);
	splx(s);
	pmap_pinit(pmap);
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

	/*
	 * reference count is zero, free pmap resources and free pmap.
	 */
	pmap_release(pm);
	s = splimp();
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
	int i, tblidx, tbloff;
	int s;

	pmap_vp_destroy(pm);
	i = (pm->pm_sr[0] & SR_VSID) >> 4;
	tblidx = i / (8  * sizeof usedsr[0]);
	tbloff = i % (8  * sizeof usedsr[0]);

	/* LOCK? */
	s = splimp();
	usedsr[tblidx] &= ~(1 << tbloff);
	splx(s);
}

void
pmap_vp_destroy(pmap_t pm)
{
	int i, j;
	int s;
	struct pmapvp *vp1;
	struct pmapvp *vp2;

	for (i = 0; i < VP_SR_SIZE; i++) {
		vp1 = pm->pm_vp[i];
		if (vp1 == NULL)
			continue;

		for (j = 0; j < VP_IDX1_SIZE; j++) {
			vp2 = vp1->vp[j];
			if (vp2 == NULL)
				continue;
			
			s = splimp();
			pool_put(&pmap_vp_pool, vp2);
			splx(s);
		}
		pm->pm_vp[i] = NULL;
		s = splimp();
		pool_put(&pmap_vp_pool, vp1);
		splx(s);
	}
}

void
pmap_avail_setup(void)
{
	struct mem_region *mp;

	(fw->mem_regions) (&pmap_mem, &pmap_avail);
	pmap_cnt_avail = 0;
	physmem = 0;

	for (mp = pmap_mem; mp->size !=0; mp++)
		physmem += btoc(mp->size);

	/* limit to 1GB available, for now -XXXGRR */
#define MEMMAX 0x40000000
	for (mp = pmap_avail; mp->size !=0 ; /* increment in loop */) {
		if (mp->start + mp->size > MEMMAX) {
			int rm_start;
			int rm_end;
			if (mp->start > MEMMAX) {
				rm_start = mp->start;
				rm_end = mp->start+mp->size;
			} else {
				rm_start = MEMMAX;
				rm_end = mp->start+mp->size;
			}
			pmap_remove_avail(rm_start, rm_end);

			/* whack physmem, since we ignore more than 256MB */
			physmem = btoc(MEMMAX);

			/* start over at top, make sure not to skip any */
			mp = pmap_avail;
			continue;
		}
		mp++;
	}
	for (mp = pmap_avail; mp->size !=0; mp++)
		pmap_cnt_avail += 1;
}

void
pmap_avail_fixup(void)
{
	struct mem_region *mp;
	u_int32_t align;
	u_int32_t end;

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
	int mpend;

	/* remove given region from available */
	for (mp = pmap_avail; mp->size; mp++) {
		/*
		 * Check if this region hold all of the region
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

void *
pmap_steal_avail(size_t size, int align)
{
	struct mem_region *mp;
	int start;
	int remsize;

	for (mp = pmap_avail; mp->size; mp++) {
		if (mp->size > size) {
			start = (mp->start + (align -1)) & ~(align -1);
			remsize = mp->size - (start - mp->start); 
			if (remsize >= 0) {
				pmap_remove_avail(start, start+size);
				return (void *)start;
			}
		}
	}
	panic ("unable to allocate region with size %x align %x",
	    size, align);
}

void *msgbuf_addr;

/*
 * Initialize pmap setup.
 * ALL of the code which deals with avail needs rewritten as an actual
 * memory allocation.
 */ 
void
pmap_bootstrap(u_int kernelstart, u_int kernelend)
{
	struct mem_region *mp;
	int i, k;
	struct pmapvp *vp1;
	struct pmapvp *vp2;

	/*
	 * Get memory.
	 */
	pmap_avail_setup();

	/*
	 * Page align all regions.
	 * Non-page memory isn't very interesting to us.
	 * Also, sort the entries for ascending addresses.
	 */
	kernelstart = trunc_page(kernelstart);
	kernelend = round_page(kernelend);
	pmap_remove_avail(kernelstart, kernelend);

	msgbuf_addr = pmap_steal_avail(MSGBUFSIZE,4);

	for (mp = pmap_avail; mp->size; mp++) {
		bzero((void *)mp->start, mp->size);
	}

#ifndef  HTABENTS
#define HTABENTS 1024
#endif
	pmap_ptab_cnt = HTABENTS;
	while ((HTABSIZE << 7) < ctob(physmem)) {
		pmap_ptab_cnt <<= 1;
	}
	/*
	 * allocate suitably aligned memory for HTAB
	 */
	pmap_ptable = pmap_steal_avail(HTABSIZE, HTABSIZE);
	bzero((void *)pmap_ptable, HTABSIZE);
	pmap_ptab_mask = pmap_ptab_cnt - 1;

	/* allocate v->p mappings for pmap_kernel() */
	for (i = 0; i < VP_SR_SIZE; i++) {
		pmap_kernel()->pm_vp[i] = NULL;
	}
	vp1 = pmap_steal_avail(sizeof (struct pmapvp), 4);
	bzero (vp1, sizeof(struct pmapvp));
	pmap_kernel()->pm_vp[KERNEL_SR] = vp1;

	for (i = 0; i < VP_IDX1_SIZE; i++) {
		vp2 = vp1->vp[i] = pmap_steal_avail(sizeof (struct pmapvp), 4);
		bzero (vp2, sizeof(struct pmapvp));
		for (k = 0; k < VP_IDX2_SIZE; k++) {
			struct pte_desc *pted;
			pted = pmap_steal_avail(sizeof (struct pte_desc), 4);
			bzero (pted, sizeof (struct pte_desc));
			vp2->vp[k] = pted;
		}
	}

	zero_page = VM_MIN_KERNEL_ADDRESS + ppc_kvm_stolen;
	ppc_kvm_stolen += PAGE_SIZE;
	copy_src_page = VM_MIN_KERNEL_ADDRESS + ppc_kvm_stolen;
	ppc_kvm_stolen += PAGE_SIZE;
	copy_dst_page = VM_MIN_KERNEL_ADDRESS + ppc_kvm_stolen;
	ppc_kvm_stolen += PAGE_SIZE;


	/*
	 * Initialize kernel pmap and hardware.
	 */
#if NPMAPS >= KERNEL_SEGMENT / 16
	usedsr[KERNEL_SEGMENT / 16 / (sizeof usedsr[0] * 8)]
		|= 1 << ((KERNEL_SEGMENT / 16) % (sizeof usedsr[0] * 8));
#endif
	for (i = 0; i < 16; i++) {
		pmap_kernel()->pm_sr[i] = (KERNEL_SEG0 + i) | SR_NOEXEC;
		asm volatile ("mtsrin %0,%1"
		    :: "r"( KERNEL_SEG0 + i), "r"(i << ADDR_SR_SHIFT) );
	}
	asm volatile ("sync; mtsdr1 %0; isync"
	    :: "r"((u_int)pmap_ptable | (pmap_ptab_mask >> 10)));

	pmap_avail_fixup();


	tlbia();

	npgs = 0;
	for (mp = pmap_avail; mp->size; mp++) {
		npgs += btoc(mp->size);
	}
	/* Ok we loose a few pages from this allocation, but hopefully
	 * not too many 
	 */
	pmap_pvh = pmap_steal_avail(sizeof(struct pted_pv_head *) * npgs, 4);
	pmap_attrib = pmap_steal_avail(sizeof(char) * npgs, 1);
	pmap_avail_fixup();
	for (mp = pmap_avail; mp->size; mp++) {
		uvm_page_physload(atop(mp->start), atop(mp->start+mp->size),
		    atop(mp->start), atop(mp->start+mp->size),
		    VM_FREELIST_DEFAULT);
	}
}

/*
 * activate a pmap entry
 * NOOP on powerpc, all PTE entries exist in the same hash table.
 * Segment registers are filled on exit to user mode.
 */
void
pmap_activate(struct proc *p)
{
}

/*
 * deactivate a pmap entry
 * NOOP on powerpc
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

	pted = pmap_vp_lookup(pm, va);
	if (pted == NULL || !PTED_VALID(pted)) {
		if (pm == pmap_kernel() && va < 0x80000000) {
			/* XXX - this is not true without BATs */
			/* if in kernel, va==pa for 0-0x80000000 */
			*pa = va;
			return TRUE;
		}
		return FALSE;
	}
	*pa = (pted->pted_pte.pte_lo & PTE_RPGN) | (va & ~PTE_RPGN);
	return TRUE;
}

u_int32_t
pmap_setusr(pmap_t pm, vaddr_t va)
{
	u_int32_t sr;
	u_int32_t oldsr;

	sr = pm->pm_sr[(u_int)va >> ADDR_SR_SHIFT];

	/* user address range lock?? */
	asm volatile ("mfsr %0,%1"
	    : "=r" (oldsr): "n"(USER_SR));
	asm volatile ("isync; mtsr %0,%1; isync"
	    :: "n"(USER_SR), "r"(sr));
	return oldsr;
}

void
pmap_popusr(u_int32_t sr)
{
	asm volatile ("isync; mtsr %0,%1; isync"
	    :: "n"(USER_SR), "r"(sr));
}

int
copyin(udaddr, kaddr, len)
	const void *udaddr;
	void *kaddr;
	size_t len;
{
	void *p;
	size_t l;
	u_int32_t oldsr;
	faultbuf env;
	void *oldh = curpcb->pcb_onfault;

	while (len > 0) {
		p = USER_ADDR + ((u_int)udaddr & ~SEGMENT_MASK);
		l = (USER_ADDR + SEGMENT_LENGTH) - p;
		if (l > len)
			l = len;
		oldsr = pmap_setusr(curpcb->pcb_pm, (vaddr_t)udaddr);
		if (setfault(env)) {
			pmap_popusr(oldsr);
			curpcb->pcb_onfault = oldh;
			return EFAULT;
		}
		bcopy(p, kaddr, l);
		pmap_popusr(oldsr);
		udaddr += l;
		kaddr += l;
		len -= l;
	}
	curpcb->pcb_onfault = oldh;
	return 0;
}

int
copyout(kaddr, udaddr, len)
	const void *kaddr;
	void *udaddr;
	size_t len;
{
	void *p;
	size_t l;
	u_int32_t oldsr;
	faultbuf env;
	void *oldh = curpcb->pcb_onfault;

	while (len > 0) {
		p = USER_ADDR + ((u_int)udaddr & ~SEGMENT_MASK);
		l = (USER_ADDR + SEGMENT_LENGTH) - p;
		if (l > len)
			l = len;
		oldsr = pmap_setusr(curpcb->pcb_pm, (vaddr_t)udaddr);
		if (setfault(env)) {
			pmap_popusr(oldsr);
			curpcb->pcb_onfault = oldh;
			return EFAULT;
		}

		bcopy(kaddr, p, l);
		pmap_popusr(oldsr);
		udaddr += l;
		kaddr += l;
		len -= l;
	}
	curpcb->pcb_onfault = oldh;
	return 0;
}

int
copyinstr(const void *udaddr, void *kaddr, size_t len, size_t *done)
{
	const u_char *uaddr = udaddr;
	u_char *kp    = kaddr;
	u_char *up;
	u_char c;
	void   *p;
	size_t	 l;
	u_int32_t oldsr;
	int cnt = 0;
	faultbuf env;
	void *oldh = curpcb->pcb_onfault;

	while (len > 0) {
		p = USER_ADDR + ((u_int)uaddr & ~SEGMENT_MASK);
		l = (USER_ADDR + SEGMENT_LENGTH) - p;
		up = p;
		if (l > len)
			l = len;
		len -= l;
		oldsr = pmap_setusr(curpcb->pcb_pm, (vaddr_t)uaddr);
		if (setfault(env)) {
			if (done != NULL)
				*done =  cnt;

			curpcb->pcb_onfault = oldh;
			pmap_popusr(oldsr);
			return EFAULT;
		}
		while (l > 0) {
			c = *up;
			*kp = c;
			if (c == 0) {
				if (done != NULL)
					*done = cnt + 1;

				curpcb->pcb_onfault = oldh;
				pmap_popusr(oldsr);
				return 0;
			} 
			up++;
			kp++;
			l--;
			cnt++;
			uaddr++;
		}
		pmap_popusr(oldsr);
	}
	curpcb->pcb_onfault = oldh;
	if (done != NULL)
		*done = cnt;

	return ENAMETOOLONG;
}

int
copyoutstr(const void *kaddr, void *udaddr, size_t len, size_t *done)
{
	u_char *uaddr = (void *)udaddr;
	const u_char *kp    = kaddr;
	u_char *up;
	u_char c;
	void   *p;
	size_t	 l;
	u_int32_t oldsr;
	int cnt = 0;
	faultbuf env;
	void *oldh = curpcb->pcb_onfault;

	while (len > 0) {
		p = USER_ADDR + ((u_int)uaddr & ~SEGMENT_MASK);
		l = (USER_ADDR + SEGMENT_LENGTH) - p;
		up = p;
		if (l > len)
			l = len;
		len -= l;
		oldsr = pmap_setusr(curpcb->pcb_pm, (vaddr_t)uaddr);
		if (setfault(env)) {
			if (done != NULL)
				*done =  cnt;

			curpcb->pcb_onfault = oldh;
			pmap_popusr(oldsr);
			return EFAULT;
		}
		while (l > 0) {
			c = *kp;
			*up = c;
			if (c == 0) {
				if (done != NULL)
					*done = cnt + 1;

				curpcb->pcb_onfault = oldh;
				pmap_popusr(oldsr);
				return 0;
			} 
			up++;
			kp++;
			l--;
			cnt++;
			uaddr++;
		}
		pmap_popusr(oldsr);
	}
	curpcb->pcb_onfault = oldh;
	if (done != NULL)
		*done = cnt;

	return ENAMETOOLONG;
}

/*
 * sync instruction cache for user virtual address.
 * The address WAS JUST MAPPED, so we have a VALID USERSPACE mapping
 */
#define CACHELINESIZE   32		/* For now XXX*/
void
pmap_syncicache_user_virt(pmap_t pm, vaddr_t va)
{
	vaddr_t p, start;
	int oldsr;
	int l;

	if (pm != pmap_kernel()) {
		start = ((u_int)USER_ADDR + ((u_int)va & ~SEGMENT_MASK));
		/* will only ever be page size, will not cross segments */

		/* USER SEGMENT LOCK - MPXXX */
		oldsr = pmap_setusr(pm, va);
	} else {
		start = va; /* flush mapped page */
	}
	p = start;
	l = PAGE_SIZE;
	do {
		__asm__ __volatile__ ("dcbst 0,%0" :: "r"(p));
		p += CACHELINESIZE;
	} while ((l -= CACHELINESIZE) > 0);
	p = start;
	l = PAGE_SIZE;
	do {
		__asm__ __volatile__ ("icbi 0,%0" :: "r"(p));
		p += CACHELINESIZE;
	} while ((l -= CACHELINESIZE) > 0);


	if (pm != pmap_kernel()) {
		pmap_popusr(oldsr);
		/* USER SEGMENT UNLOCK -MPXXX */
	}
}

/*
 * Change a page to readonly
 */
void
pmap_page_ro(pmap_t pm, vaddr_t va)
{
	struct pte *ptp;
	struct pte_desc *pted;
	int sr, idx;

	pted = pmap_vp_lookup(pm, va);
	if (pted == NULL || !PTED_VALID(pted))
		return;

	pted->pted_pte.pte_lo &= ~PTE_PP;
	pted->pted_pte.pte_lo |= PTE_RO;

	sr = ptesr(pm->pm_sr, va);
	idx = pteidx(sr, va);

	/* determine which pteg mapping is present in */
	ptp = pmap_ptable + (idx ^ (PTED_HID(pted) ? pmap_ptab_mask : 0)) * 8;
	ptp += PTED_PTEGIDX(pted); /* increment by index into pteg */

	/*
	 * We now have the pointer to where it will be, if it is
	 * currently mapped. If the mapping was thrown away in
	 * exchange for another page mapping, then this page is
	 * not currently in the HASH.
	 */
	if ((pted->pted_pte.pte_hi | (PTED_HID(pted) ? PTE_HID : 0))
	    == ptp->pte_hi) {
		ptp->pte_hi &= ~PTE_VALID;
		__asm__ volatile ("sync");
		tlbie(va);
		tlbsync();
		if (PTED_MANAGED(pted)) { /* XXX */
			pmap_attr_save(ptp->pte_lo & PTE_RPGN,
			    ptp->pte_lo & (PTE_REF|PTE_CHG));
		}
		ptp->pte_lo &= ~PTE_CHG;
		ptp->pte_lo &= ~PTE_PP;
		ptp->pte_lo |= PTE_RO;
		__asm__ volatile ("sync");
		ptp->pte_hi |= PTE_VALID;
	}
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
	paddr_t pa = VM_PAGE_TO_PHYS(pg);
	int s;
	struct pte_desc *pted;
	struct pted_pv_head *pvh;

	/* need to lock for this pv */
	s = splimp();
	pvh = pmap_find_pvh(pa);

	/* nothing to do if not a managed page */
	if (pvh == NULL) {
		splx(s);
		return;
	}

	if (prot == VM_PROT_NONE) {
		while (!LIST_EMPTY(pvh)) {
			pted = LIST_FIRST(pvh);
			pmap_remove_pg(pted->pted_pmap, pted->pted_va);
		}
		splx(s);
		return;
	}
	
	LIST_FOREACH(pted, pvh, pted_pv_list) {
		pmap_page_ro(pted->pted_pmap, pted->pted_va);
	}
	splx(s);
}

void
pmap_protect(pmap_t pm, vaddr_t sva, vaddr_t eva, vm_prot_t prot)
{
	int s;
	if (prot & VM_PROT_READ) {
		s = splimp();
		while (sva < eva) {
			pmap_page_ro(pm, sva);
			sva += PAGE_SIZE;
		}
		splx(s);
		return;
	}
	pmap_remove(pm, sva, eva);
}

/*
 * Restrict given range to physical memory
 */
void
pmap_real_memory(paddr_t *start, vm_size_t *size)
{
	struct mem_region *mp;

	for (mp = pmap_mem; mp->size; mp++) {
		if (((*start + *size) > mp->start)
			&& (*start < (mp->start + mp->size)))
		{
			if (*start < mp->start) {
				*size -= mp->start - *start;
				*start = mp->start;
			}
			if ((*start + *size) > (mp->start + mp->size))
				*size = mp->start + mp->size - *start;
			return;
		}
	}
	*size = 0;
}

/*
 * How much virtual space is available to the kernel?
 */
void
pmap_virtual_space(vaddr_t *start, vaddr_t *end)
{
	*start = VM_MIN_KERNEL_ADDRESS;
	*end = VM_MAX_KERNEL_ADDRESS;
}

void
pmap_init()
{
	vsize_t sz;
	struct pted_pv_head *pvh;
	char *attr;
	int i, bank;

	pool_init(&pmap_pmap_pool, sizeof(struct pmap), 0, 0, 20, "pmap", NULL);
	pool_setlowat(&pmap_pmap_pool, 2);
	pool_init(&pmap_vp_pool, sizeof(struct pmapvp), 0, 0, 150, "vp", NULL);
	pool_setlowat(&pmap_vp_pool, 10);
	pool_init(&pmap_pted_pool, sizeof(struct pte_desc), 0, 0, 150, "pted",
	    NULL);
	pool_setlowat(&pmap_pted_pool, 20);

	/* pmap_pvh and pmap_attr must be allocated 1-1 so that pmap_save_attr
	 * is callable from pte_spill_r (with vm disabled)
	 */
	pvh = (struct pted_pv_head *)pmap_pvh;
	for (i = npgs; i > 0; i--)
		LIST_INIT(pvh++);
	attr = pmap_attrib;
	bzero(pmap_attrib, npgs);
	pvh = (struct pted_pv_head *)pmap_pvh;
	for (bank = 0; bank < vm_nphysseg; bank++) {
		sz = vm_physmem[bank].end - vm_physmem[bank].start;
		vm_physmem[bank].pmseg.pvent = pvh;
		vm_physmem[bank].pmseg.attrs = attr;
		pvh += sz;
		attr += sz;
	}
	pmap_initialized = 1;
}

/* 
 * There are two routines, pte_spill_r and pte_spill_v
 * the _r version only handles kernel faults which are not user
 * accesses. The _v version handles all user faults and kernel copyin/copyout
 * "user" accesses.
 */
int
pte_spill_r(u_int32_t va, u_int32_t msr, u_int32_t dsisr, int exec_fault)
{
	pmap_t pm;
	struct pte_desc *pted;

	/* 
	 * This function only handles kernel faults, not supervisor copyins.
	 */
	if (!(msr & PSL_PR)) {
		/* lookup is done physical to prevent faults */
		if (VP_SR(va) == USER_SR) {
			return 0;
		} else {
			pm = pmap_kernel();
		}
	} else {
		return 0;
	}

	pted = pmap_vp_lookup(pm, va);
	if (pted == NULL) {
		return 0;
	}

	/* if the current mapping is RO and the access was a write
	 * we return 0
	 */
	if (!PTED_VALID(pted)) {
		return 0;
	} 
	if ((dsisr & (1 << (31-6))) && (pted->pted_pte.pte_lo & 0x1)) {
		/* write fault and we have a readonly mapping */
		return 0;
	}
	if ((exec_fault != 0)
	    && ((pted->pted_va & PTED_VA_EXEC_M) == 0)) {
		/* attempted to execute non-executeable page */
		return 0;
	}
	pte_insert(pted);

	return 1;
}

int
pte_spill_v(pmap_t pm, u_int32_t va, u_int32_t dsisr, int exec_fault)
{
	struct pte_desc *pted;

	pted = pmap_vp_lookup(pm, va);
	if (pted == NULL) {
		return 0;
	}

	/*
	 * if the current mapping is RO and the access was a write
	 * we return 0
	 */
	if (!PTED_VALID(pted)) {
		return 0;
	}
	if ((dsisr & (1 << (31-6))) && (pted->pted_pte.pte_lo & 0x1)) {
		/* write fault and we have a readonly mapping */
		return 0;
	}
	if ((exec_fault != 0)
	    && ((pted->pted_va & PTED_VA_EXEC_M) == 0)) {
		/* attempted to execute non-executeable page */
		return 0;
	}
	pte_insert(pted);
	return 1;
}


/*
 * should pte_insert code avoid wired mappings?
 * is the stack safe?
 * is the pted safe? (physical)
 * -ugh
 */

void
pte_insert(struct pte_desc *pted)
{
	int off;
	int secondary;
	struct pte *ptp;
	int sr, idx;
	int i;

	/* HASH lock? */

	sr = ptesr(pted->pted_pmap->pm_sr, pted->pted_va);
	idx = pteidx(sr, pted->pted_va);

	/* determine if ptp is already mapped */
	ptp = pmap_ptable + (idx ^ (PTED_HID(pted) ? pmap_ptab_mask : 0)) * 8;
	ptp += PTED_PTEGIDX(pted); /* increment by index into pteg */
	if ((pted->pted_pte.pte_hi | (PTED_HID(pted) ? PTE_HID : 0))
	    == ptp->pte_hi) {
		pte_zap(ptp,pted);
	}

	pted->pted_va &= ~(PTED_VA_HID_M|PTED_VA_PTEGIDX_M);

	/*
	 * instead of starting at the beginning of each pteg,
	 * the code should pick a random location with in the primary
	 * then search all of the entries, then if not yet found,
	 * do the same for the secondary.
	 * this would reduce the frontloading of the pteg.
	 */

	/* first just try fill of primary hash */
	ptp = pmap_ptable + (idx) * 8;
	for (i = 0; i < 8; i++) {
		if (ptp[i].pte_hi & PTE_VALID)
			continue;

		/* not valid, just load */
/* printf("inserting in primary idx %x, i %x\n", idx, i); */
		pted->pted_va |= i;
		ptp[i].pte_hi = pted->pted_pte.pte_hi & ~PTE_VALID;
		ptp[i].pte_lo = pted->pted_pte.pte_lo;
		__asm__ volatile ("sync");
		ptp[i].pte_hi |= PTE_VALID;
		__asm volatile ("sync");
		return;
	}
	/* first just try fill of secondary hash */
	ptp = pmap_ptable + (idx ^ pmap_ptab_mask) * 8;
	for (i = 0; i < 8; i++) {
		if (ptp[i].pte_hi & PTE_VALID)
			continue;

		pted->pted_va |= (i | PTED_VA_HID_M);
		ptp[i].pte_hi = (pted->pted_pte.pte_hi | PTE_HID) & ~PTE_VALID;
		ptp[i].pte_lo = pted->pted_pte.pte_lo;
		__asm__ volatile ("sync");
		ptp[i].pte_hi |= PTE_VALID;
		__asm volatile ("sync");
		return;
	}

	/* need decent replacement algorithm */
	__asm__ volatile ("mftb %0" : "=r"(off));
	secondary = off & 8;
	pted->pted_va |= off & (PTED_VA_PTEGIDX_M|PTED_VA_HID_M);

	idx =  (idx ^ (PTED_HID(pted) ? pmap_ptab_mask : 0));

	ptp = pmap_ptable + (idx * 8);
	ptp += PTED_PTEGIDX(pted); /* increment by index into pteg */
	if (ptp->pte_hi & PTE_VALID) {
		vaddr_t va;
		ptp->pte_hi &= ~PTE_VALID;
		__asm volatile ("sync");

		va = ((ptp->pte_hi & PTE_API) << ADDR_API_SHIFT) |
		     ((((ptp->pte_hi >> PTE_VSID_SHIFT) & SR_VSID)
			^(idx ^ ((ptp->pte_hi & PTE_HID) ? 0x3ff : 0)))
			    & 0x3ff) << PAGE_SHIFT;
		tlbie(va);

		tlbsync();
		pmap_attr_save(ptp->pte_lo & PTE_RPGN,
		    ptp->pte_lo & (PTE_REF|PTE_CHG));
	}

	if (secondary)
		ptp->pte_hi = (pted->pted_pte.pte_hi | PTE_HID) & ~PTE_VALID;
	else
		ptp->pte_hi = pted->pted_pte.pte_hi & ~PTE_VALID;

	ptp->pte_lo = pted->pted_pte.pte_lo;
	__asm__ volatile ("sync");
	ptp->pte_hi |= PTE_VALID;
}

#ifdef DEBUG_PMAP
void
print_pteg(pmap_t pm, vaddr_t va)
{
	int sr, idx;
	struct pte *ptp;

	sr = ptesr(pm->pm_sr, va);
	idx = pteidx(sr,  va);

	ptp = pmap_ptable + idx  * 8;
	db_printf("va %x, sr %x, idx %x\n", va, sr, idx);

	db_printf("%08x %08x %08x %08x  %08x %08x %08x %08x\n",
	    ptp[0].pte_hi, ptp[1].pte_hi, ptp[2].pte_hi, ptp[3].pte_hi,
	    ptp[4].pte_hi, ptp[5].pte_hi, ptp[6].pte_hi, ptp[7].pte_hi);
	db_printf("%08x %08x %08x %08x  %08x %08x %08x %08x\n",
	    ptp[0].pte_lo, ptp[1].pte_lo, ptp[2].pte_lo, ptp[3].pte_lo,
	    ptp[4].pte_lo, ptp[5].pte_lo, ptp[6].pte_lo, ptp[7].pte_lo);
	ptp = pmap_ptable + (idx ^ pmap_ptab_mask) * 8;
	db_printf("%08x %08x %08x %08x  %08x %08x %08x %08x\n",
	    ptp[0].pte_hi, ptp[1].pte_hi, ptp[2].pte_hi, ptp[3].pte_hi,
	    ptp[4].pte_hi, ptp[5].pte_hi, ptp[6].pte_hi, ptp[7].pte_hi);
	db_printf("%08x %08x %08x %08x  %08x %08x %08x %08x\n",
	    ptp[0].pte_lo, ptp[1].pte_lo, ptp[2].pte_lo, ptp[3].pte_lo,
	    ptp[4].pte_lo, ptp[5].pte_lo, ptp[6].pte_lo, ptp[7].pte_lo);
}


/* debugger assist function */
int pmap_prtrans(u_int pid, vaddr_t va);

void
pmap_print_pted(struct pte_desc *pted, int(*print)(const char *, ...))
{
	vaddr_t va;
	va = pted->pted_va & ~PAGE_MASK;
	print("\n pted %x", pted);
	if (PTED_VALID(pted)) {
		print(" va %x:", pted->pted_va & ~PAGE_MASK);
		print(" HID %d", PTED_HID(pted) ? 1: 0);
		print(" PTEGIDX %x", PTED_PTEGIDX(pted));
		print(" MANAGED %d", PTED_MANAGED(pted) ? 1: 0);
		print(" WIRED %d\n", PTED_WIRED(pted) ? 1: 0);
		print("ptehi %x ptelo %x ptp %x Aptp %x\n",
		    pted->pted_pte.pte_hi, pted->pted_pte.pte_lo,
		    pmap_ptable +
			8*pteidx(ptesr(pted->pted_pmap->pm_sr, va), va),
		    pmap_ptable +
			8*(pteidx(ptesr(pted->pted_pmap->pm_sr, va), va)
			    ^ pmap_ptab_mask)
		    );
	}
}

int pmap_user_read(int size, vaddr_t va);
int
pmap_user_read(int size, vaddr_t va)
{
	unsigned char  read1;
	unsigned short read2;
	unsigned int   read4;
	int err;

	if (size == 1) {
		err = copyin((void *)va, &read1, 1);
		if (err == 0) {
			db_printf("byte read %x\n", read1);
		}
	} else if (size == 2) {
		err = copyin((void *)va, &read2, 2);
		if (err == 0) {
			db_printf("short read %x\n", read2);
		}
	} else if (size == 4) {
		err = copyin((void *)va, &read4, 4);
		if (err == 0) {
			db_printf("int read %x\n", read4);
		}
	} else {
		return 1;
	}


	return 0;
}

int pmap_dump_pmap(u_int pid);
int
pmap_dump_pmap(u_int pid)
{
	pmap_t pm;
	struct proc *p;
	if (pid == 0) {
		pm = pmap_kernel();
	} else {
		p = pfind(pid);

		if (p == NULL) {
			db_printf("invalid pid %d", pid);
			return 1;
		}
		pm = p->p_vmspace->vm_map.pmap;
	}
	printf("pmap %x:\n", pm);
	printf("segid %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x",
	    pm->pm_sr[0], pm->pm_sr[1], pm->pm_sr[2], pm->pm_sr[3],
	    pm->pm_sr[4], pm->pm_sr[5], pm->pm_sr[6], pm->pm_sr[7],
	    pm->pm_sr[8], pm->pm_sr[9], pm->pm_sr[10], pm->pm_sr[11],
	    pm->pm_sr[12], pm->pm_sr[13], pm->pm_sr[14], pm->pm_sr[15]);

	return 0;
}

int
pmap_prtrans(u_int pid, vaddr_t va)
{
	struct proc *p;
	pmap_t pm;
	struct pmapvp *vp1;
	struct pmapvp *vp2;
	struct pte_desc *pted;

	if (pid == 0) {
		pm = pmap_kernel();
	} else {
		p = pfind(pid);

		if (p == NULL) {
			db_printf("invalid pid %d", pid);
			return 1;
		}
		pm = p->p_vmspace->vm_map.pmap;
	}

	db_printf(" pid %d, va 0x%x pmap %x\n", pid, va, pm);
	vp1 = pm->pm_vp[VP_SR(va)];
	db_printf("sr %x id %x vp1 %x", VP_SR(va), pm->pm_sr[VP_SR(va)],
	    vp1);

	if (vp1) {
		vp2 = vp1->vp[VP_IDX1(va)];
		db_printf(" vp2 %x", vp2);

		if (vp2) {
			pted = vp2->vp[VP_IDX2(va)];
			pmap_print_pted(pted, db_printf);

		}
	}
	print_pteg(pm, va);

	return 0;
}
int pmap_show_mappings(paddr_t pa);

int
pmap_show_mappings(paddr_t pa) 
{
	struct pted_pv_head *pvh;
	struct pte_desc *pted;
	pvh = pmap_find_pvh(pa);
	if (pvh == NULL) {
		db_printf("pa %x: unmanaged\n");
	} else {
		LIST_FOREACH(pted, pvh, pted_pv_list) {
			pmap_print_pted(pted, db_printf);
		}
	}
	return 0;
}
#endif
