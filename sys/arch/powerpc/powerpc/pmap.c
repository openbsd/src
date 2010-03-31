/*	$OpenBSD: pmap.c,v 1.109 2010/03/31 21:02:42 drahn Exp $ */

/*
 * Copyright (c) 2001, 2002, 2007 Dale Rahn.
 * All rights reserved.
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

/*
 * powerpc lazy icache managment.
 * The icache does not snoop dcache accesses. The icache also will not load
 * modified data from the dcache, but the unmodified data in ram.
 * Before the icache is loaded, the dcache must be synced to ram to prevent
 * the icache from loading stale data.
 * pg->pg_flags PG_PMAP_EXE bit is used to track if the dcache is clean
 * and the icache may have valid data in it.
 * if the PG_PMAP_EXE bit is set (and the page is not currently RWX)
 * the icache will only have valid code in it. If the bit is clear
 * memory may not match the dcache contents or the icache may contain
 * data from a previous page.
 *
 * pmap enter
 * !E  NONE 	-> R	no action
 * !E  NONE|R 	-> RW	no action
 * !E  NONE|R 	-> RX	flush dcache, inval icache (that page only), set E
 * !E  NONE|R 	-> RWX	flush dcache, inval icache (that page only), set E
 * !E  NONE|RW 	-> RWX	flush dcache, inval icache (that page only), set E
 *  E  NONE 	-> R	no action
 *  E  NONE|R 	-> RW	clear PG_PMAP_EXE bit
 *  E  NONE|R 	-> RX	no action
 *  E  NONE|R 	-> RWX	no action
 *  E  NONE|RW 	-> RWX	-invalid source state
 *
 * pamp_protect
 *  E RW -> R	- invalid source state
 * !E RW -> R	- no action
 *  * RX -> R	- no action
 *  * RWX -> R	- sync dcache, inval icache
 *  * RWX -> RW	- clear PG_PMAP_EXE
 *  * RWX -> RX	- sync dcache, inval icache
 *  * * -> NONE	- no action
 * 
 * pmap_page_protect (called with arg PROT_NONE if page is to be reused)
 *  * RW -> R	- as pmap_protect
 *  * RX -> R	- as pmap_protect
 *  * RWX -> R	- as pmap_protect
 *  * RWX -> RW	- as pmap_protect
 *  * RWX -> RX	- as pmap_protect
 *  * * -> NONE - clear PG_PMAP_EXE
 * 
 */

#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/queue.h>
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

#include <powerpc/lock.h>

struct pmap kernel_pmap_;
static struct mem_region *pmap_mem, *pmap_avail;
struct mem_region pmap_allocated[10];
int pmap_cnt_avail;
int pmap_cnt_allocated;

struct pte_64  *pmap_ptable64;
struct pte_32  *pmap_ptable32;
int	pmap_ptab_cnt;
u_int	pmap_ptab_mask;

#define HTABSIZE_32	(pmap_ptab_cnt * 64)
#define HTABMEMSZ_64	(pmap_ptab_cnt * 8 * sizeof(struct pte_64))
#define HTABSIZE_64	(ffs(pmap_ptab_cnt) - 12)

static u_int usedsr[NPMAPS / sizeof(u_int) / 8];
paddr_t zero_page;
paddr_t copy_src_page;
paddr_t copy_dst_page;

struct pte_desc {
	/* Linked list of phys -> virt entries */
	LIST_ENTRY(pte_desc) pted_pv_list;
	union {
	struct pte_32 pted_pte32;
	struct pte_64 pted_pte64;
	}p;
	pmap_t pted_pmap;
	vaddr_t pted_va;
};

void print_pteg(pmap_t pm, vaddr_t va);

static inline void tlbsync(void);
static inline void tlbie(vaddr_t ea);
void tlbia(void);

void pmap_attr_save(paddr_t pa, u_int32_t bits);
void pmap_page_ro64(pmap_t pm, vaddr_t va, vm_prot_t prot);
void pmap_page_ro32(pmap_t pm, vaddr_t va, vm_prot_t prot);

/*
 * LOCKING structures.
 * This may not be correct, and doesn't do anything yet.
 */
#define pmap_simplelock_pm(pm)
#define pmap_simpleunlock_pm(pm)
#define pmap_simplelock_pv(pm)
#define pmap_simpleunlock_pv(pm)


/* VP routines */
int pmap_vp_enter(pmap_t pm, vaddr_t va, struct pte_desc *pted, int flags);
struct pte_desc *pmap_vp_remove(pmap_t pm, vaddr_t va);
void pmap_vp_destroy(pmap_t pm);
struct pte_desc *pmap_vp_lookup(pmap_t pm, vaddr_t va);

/* PV routines */
void pmap_enter_pv(struct pte_desc *pted, struct vm_page *);
void pmap_remove_pv(struct pte_desc *pted);


/* pte hash table routines */
void pte_insert32(struct pte_desc *pted);
void pte_insert64(struct pte_desc *pted);
void pmap_hash_remove(struct pte_desc *pted);
void pmap_fill_pte64(pmap_t pm, vaddr_t va, paddr_t pa,
    struct pte_desc *pted, vm_prot_t prot, int flags, int cache);
void pmap_fill_pte32(pmap_t pm, vaddr_t va, paddr_t pa,
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
void pte_zap(void *ptp, struct pte_desc *pted);

/* debugging */
void pmap_print_pted(struct pte_desc *pted, int(*print)(const char *, ...));

/* XXX - panic on pool get failures? */
struct pool pmap_pmap_pool;
struct pool pmap_vp_pool;
struct pool pmap_pted_pool;

int pmap_initialized = 0;
int physmem;
int physmaxaddr;

void pmap_hash_lock_init(void);
void pmap_hash_lock(int entry);
void pmap_hash_unlock(int entry);
int pmap_hash_lock_try(int entry);

volatile unsigned int pmap_hash_lock_word = 0;

void
pmap_hash_lock_init()
{
	pmap_hash_lock_word = 0;
}

int
pmap_hash_lock_try(int entry)
{
	int val = 1 << entry;
	int success, tmp;
	__asm volatile (
	    "1: lwarx	%0, 0, %3	\n"
	    "	and.	%1, %2, %0	\n"
	    "	li	%1, 0		\n"
	    "	bne 2f			\n"
	    "	or	%0, %2, %0	\n"
	    "	stwcx.  %0, 0, %3	\n"
	    "	li	%1, 1		\n"
	    "	bne-	1b		\n"
	    "2:				\n"
	    : "=&r" (tmp), "=&r" (success)
	    : "r" (val), "r" (&pmap_hash_lock_word)
	    : "memory");
	return success;
}


void
pmap_hash_lock(int entry)
{
	int attempt = 0;
	int locked = 0;
	do {
		if (pmap_hash_lock_word & (1 << entry)) {
			attempt++;
			if(attempt >0x20000000)
				panic("unable to obtain lock on entry %d\n",
				    entry);
			continue;
		}
		locked = pmap_hash_lock_try(entry);
	} while (locked == 0);
}

void
pmap_hash_unlock(int entry)
{
	atomic_clearbits_int(&pmap_hash_lock_word,  1 << entry);
}

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
 * Otherwise bad race conditions can appear.
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
 * Should this be called under splvm?
 */
int
pmap_vp_enter(pmap_t pm, vaddr_t va, struct pte_desc *pted, int flags)
{
	struct pmapvp *vp1;
	struct pmapvp *vp2;
	int s;

	pmap_simplelock_pm(pm);

	vp1 = pm->pm_vp[VP_SR(va)];
	if (vp1 == NULL) {
		s = splvm();
		vp1 = pool_get(&pmap_vp_pool, PR_NOWAIT | PR_ZERO);
		splx(s);
		if (vp1 == NULL) {
			if ((flags & PMAP_CANFAIL) == 0)
				panic("pmap_vp_enter: failed to allocate vp1");
			return ENOMEM;
		}
		pm->pm_vp[VP_SR(va)] = vp1;
	}

	vp2 = vp1->vp[VP_IDX1(va)];
	if (vp2 == NULL) {
		s = splvm();
		vp2 = pool_get(&pmap_vp_pool, PR_NOWAIT | PR_ZERO);
		splx(s);
		if (vp2 == NULL) {
			if ((flags & PMAP_CANFAIL) == 0)
				panic("pmap_vp_enter: failed to allocate vp2");
			return ENOMEM;
		}
		vp1->vp[VP_IDX1(va)] = vp2;
	}

	vp2->vp[VP_IDX2(va)] = pted;

	pmap_simpleunlock_pm(pm);

	return 0;
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

void
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
	if (ppc_proc_is_64b)
		return (pted->p.pted_pte64.pte_hi & PTE_VALID_64);
	else 
		return (pted->p.pted_pte32.pte_hi & PTE_VALID_32);
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


/* PTE_CHG_32 == PTE_CHG_64 */
/* PTE_REF_32 == PTE_REF_64 */
static __inline u_int
pmap_pte2flags(u_int32_t pte)
{
	return (((pte & PTE_REF_32) ? PG_PMAP_REF : 0) |
	    ((pte & PTE_CHG_32) ? PG_PMAP_MOD : 0));
}

static __inline u_int
pmap_flags2pte(u_int32_t flags)
{
	return (((flags & PG_PMAP_REF) ? PTE_REF_32 : 0) |
	    ((flags & PG_PMAP_MOD) ? PTE_CHG_32 : 0));
}

void
pmap_attr_save(paddr_t pa, u_int32_t bits)
{
	struct vm_page *pg;

	pg = PHYS_TO_VM_PAGE(pa);
	if (pg == NULL)
		return;

	atomic_setbits_int(&pg->pg_flags,  pmap_pte2flags(bits));
}

int
pmap_enter(pmap_t pm, vaddr_t va, paddr_t pa, vm_prot_t prot, int flags)
{
	struct pte_desc *pted;
	struct vm_page *pg;
	int s;
	int need_sync = 0;
	int cache;
	int error;

	/* MP - Acquire lock for this pmap */

	s = splvm();
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
		pted = pool_get(&pmap_pted_pool, PR_NOWAIT | PR_ZERO);	
		if (pted == NULL) {
			if ((flags & PMAP_CANFAIL) == 0)
				return ENOMEM;
			panic("pmap_enter: failed to allocate pted");
		}
		error = pmap_vp_enter(pm, va, pted, flags);
		if (error) {
			pool_put(&pmap_pted_pool, pted);
			return error;
		}
	}

	/* Calculate PTE */
	pg = PHYS_TO_VM_PAGE(pa);
	if (pg != NULL)
		cache = PMAP_CACHE_WB; /* managed memory is cacheable */
	else
		cache = PMAP_CACHE_CI;

	if (ppc_proc_is_64b)
		pmap_fill_pte64(pm, va, pa, pted, prot, flags, cache);
	else
		pmap_fill_pte32(pm, va, pa, pted, prot, flags, cache);

	if (pg != NULL) {
		pmap_enter_pv(pted, pg); /* only managed mem */
	}

	/*
	 * Insert into HTAB
	 * We were told to map the page, probably called from vm_fault,
	 * so map the page!
	 */
	if (ppc_proc_is_64b)
		pte_insert64(pted);
	else
		pte_insert32(pted);

        if (prot & VM_PROT_EXECUTE) {
		u_int sn = VP_SR(va);

        	pm->pm_exec[sn]++;
		if (pm->pm_sr[sn] & SR_NOEXEC)
			pm->pm_sr[sn] &= ~SR_NOEXEC;

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
		if ((prot & VM_PROT_WRITE) && (pg != NULL))
			atomic_clearbits_int(&pg->pg_flags, PG_PMAP_EXE);
	}

	splx(s);

	/* only instruction sync executable pages */
	if (need_sync)
		pmap_syncicache_user_virt(pm, va);

	/* MP - free pmap lock */
	return 0;
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
	s = splvm();
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
		if (pm->pm_exec[sn] == 0)
			pm->pm_sr[sn] |= SR_NOEXEC;
	}

	if (ppc_proc_is_64b)
		pted->p.pted_pte64.pte_hi &= ~PTE_VALID_64;
	else
		pted->p.pted_pte32.pte_hi &= ~PTE_VALID_32;

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

	pm = pmap_kernel();

	/* MP - lock pmap. */
	s = splvm();

	pted = pmap_vp_lookup(pm, va);
	if (pted && PTED_VALID(pted))
		pmap_kremove_pg(va); /* pted is reused */

	pm->pm_stats.resident_count++;

	/* Do not have pted for this, get one and put it in VP */
	if (pted == NULL) {
		panic("pted not preallocated in pmap_kernel() va %lx pa %lx\n",
		    va, pa);
	}

	if (cache == PMAP_CACHE_DEFAULT) {
		if (PHYS_TO_VM_PAGE(pa) != NULL)
			cache = PMAP_CACHE_WB; /* managed memory is cacheable */
		else
			cache = PMAP_CACHE_CI;
	}

	/* Calculate PTE */
	if (ppc_proc_is_64b)
		pmap_fill_pte64(pm, va, pa, pted, prot, flags, cache);
	else
		pmap_fill_pte32(pm, va, pa, pted, prot, flags, cache);

	/*
	 * Insert into HTAB
	 * We were told to map the page, probably called from vm_fault,
	 * so map the page!
	 */
	if (ppc_proc_is_64b)
		pte_insert64(pted);
	else
		pte_insert32(pted);

	pted->pted_va |= PTED_VA_WIRED_M;

        if (prot & VM_PROT_EXECUTE) {
		u_int sn = VP_SR(va);

        	pm->pm_exec[sn]++;
		if (pm->pm_sr[sn] & SR_NOEXEC)
			pm->pm_sr[sn] &= ~SR_NOEXEC;
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

	s = splvm();

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
		if (pm->pm_exec[sn] == 0)
			pm->pm_sr[sn] |= SR_NOEXEC;
	}

	if (PTED_MANAGED(pted))
		pmap_remove_pv(pted);

	/* invalidate pted; */
	if (ppc_proc_is_64b)
		pted->p.pted_pte64.pte_hi &= ~PTE_VALID_64;
	else
		pted->p.pted_pte32.pte_hi &= ~PTE_VALID_32;

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
pte_zap(void *ptp, struct pte_desc *pted)
{

	struct pte_64 *ptp64 = (void*) ptp;
	struct pte_32 *ptp32 = (void*) ptp;

	if (ppc_proc_is_64b)
		ptp64->pte_hi &= ~PTE_VALID_64;
	else 
		ptp32->pte_hi &= ~PTE_VALID_32;

	__asm volatile ("sync");
	tlbie(pted->pted_va);
	__asm volatile ("sync");
	tlbsync();
	__asm volatile ("sync");
	if (ppc_proc_is_64b) {
		if (PTED_MANAGED(pted))
			pmap_attr_save(pted->p.pted_pte64.pte_lo & PTE_RPGN_64,
			    ptp64->pte_lo & (PTE_REF_64|PTE_CHG_64));
	} else {
		if (PTED_MANAGED(pted))
			pmap_attr_save(pted->p.pted_pte32.pte_lo & PTE_RPGN_32,
			    ptp32->pte_lo & (PTE_REF_32|PTE_CHG_32));
	}
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
	struct pte_64 *ptp64;
	struct pte_32 *ptp32;
	int sr, idx;

	sr = ptesr(pm->pm_sr, va);
	idx = pteidx(sr, va);

	idx =  (idx ^ (PTED_HID(pted) ? pmap_ptab_mask : 0));
	/* determine which pteg mapping is present in */

	if (ppc_proc_is_64b) {
		int entry = PTED_PTEGIDX(pted); 
		ptp64 = pmap_ptable64 + (idx * 8);
		ptp64 += entry; /* increment by entry into pteg */
		pmap_hash_lock(entry);
		/*
		 * We now have the pointer to where it will be, if it is
		 * currently mapped. If the mapping was thrown away in
		 * exchange for another page mapping, then this page is not
		 * currently in the HASH.
		 */
		if ((pted->p.pted_pte64.pte_hi | 
		    (PTED_HID(pted) ? PTE_HID_64 : 0)) == ptp64->pte_hi) {
			pte_zap((void*)ptp64, pted);
		}
		pmap_hash_unlock(entry);
	} else {
		int entry = PTED_PTEGIDX(pted); 
		ptp32 = pmap_ptable32 + (idx * 8);
		ptp32 += entry; /* increment by entry into pteg */
		pmap_hash_lock(entry);
		/*
		 * We now have the pointer to where it will be, if it is
		 * currently mapped. If the mapping was thrown away in
		 * exchange for another page mapping, then this page is not
		 * currently in the HASH.
		 */
		if ((pted->p.pted_pte32.pte_hi |
		    (PTED_HID(pted) ? PTE_HID_32 : 0)) == ptp32->pte_hi) {
			pte_zap((void*)ptp32, pted);
		}
		pmap_hash_unlock(entry);
	}
}

/*
 * What about execution control? Even at only a segment granularity.
 */
void
pmap_fill_pte64(pmap_t pm, vaddr_t va, paddr_t pa, struct pte_desc *pted,
	vm_prot_t prot, int flags, int cache)
{
	sr_t sr;
	struct pte_64 *pte64;

	sr = ptesr(pm->pm_sr, va);
	pte64 = &pted->p.pted_pte64;

	pte64->pte_hi = (((u_int64_t)sr & SR_VSID) <<
	   PTE_VSID_SHIFT_64) |
	    ((va >> ADDR_API_SHIFT_64) & PTE_API_64) | PTE_VALID_64;
	pte64->pte_lo = (pa & PTE_RPGN_64);


	if ((cache == PMAP_CACHE_WB))
		pte64->pte_lo |= PTE_M_64;
	else if ((cache == PMAP_CACHE_WT))
		pte64->pte_lo |= (PTE_W_64 | PTE_M_64);
	else
		pte64->pte_lo |= (PTE_M_64 | PTE_I_64 | PTE_G_64);

	if (prot & VM_PROT_WRITE)
		pte64->pte_lo |= PTE_RW_64;
	else
		pte64->pte_lo |= PTE_RO_64;

	pted->pted_va = va & ~PAGE_MASK;

	if (prot & VM_PROT_EXECUTE)
		pted->pted_va  |= PTED_VA_EXEC_M;
	else
		pte64->pte_lo |= PTE_N_64;

	pted->pted_pmap = pm;
}
/*
 * What about execution control? Even at only a segment granularity.
 */
void
pmap_fill_pte32(pmap_t pm, vaddr_t va, paddr_t pa, struct pte_desc *pted,
	vm_prot_t prot, int flags, int cache)
{
	sr_t sr;
	struct pte_32 *pte32;

	sr = ptesr(pm->pm_sr, va);
	pte32 = &pted->p.pted_pte32;

	pte32->pte_hi = ((sr & SR_VSID) << PTE_VSID_SHIFT_32) |
	    ((va >> ADDR_API_SHIFT_32) & PTE_API_32) | PTE_VALID_32;
	pte32->pte_lo = (pa & PTE_RPGN_32);

	if ((cache == PMAP_CACHE_WB))
		pte32->pte_lo |= PTE_M_32;
	else if ((cache == PMAP_CACHE_WT))
		pte32->pte_lo |= (PTE_W_32 | PTE_M_32);
	else
		pte32->pte_lo |= (PTE_M_32 | PTE_I_32 | PTE_G_32);

	if (prot & VM_PROT_WRITE)
		pte32->pte_lo |= PTE_RW_32;
	else
		pte32->pte_lo |= PTE_RO_32;

	pted->pted_va = va & ~PAGE_MASK;

	/* XXX Per-page execution control. */
	if (prot & VM_PROT_EXECUTE)
		pted->pted_va  |= PTED_VA_EXEC_M;

	pted->pted_pmap = pm;
}

/*
 * read/clear bits from pte/attr cache, for reference/change
 * ack, copied code in the pte flush code....
 */
int
pteclrbits(struct vm_page *pg, u_int flagbit, u_int clear)
{
	u_int bits;
	int s;
	struct pte_desc *pted;
	u_int ptebit = pmap_flags2pte(flagbit);

	/* PTE_CHG_32 == PTE_CHG_64 */
	/* PTE_REF_32 == PTE_REF_64 */

	/*
	 *  First try the attribute cache
	 */
	bits = pg->pg_flags & flagbit;
	if ((bits == flagbit) && (clear == 0))
		return bits;

	/* cache did not contain all necessary bits,
	 * need to walk thru pv table to collect all mappings for this
	 * page, copying bits to the attribute cache 
	 * then reread the attribute cache.
	 */
	/* need lock for this pv */
	s = splvm();

	LIST_FOREACH(pted, &(pg->mdpage.pv_list), pted_pv_list) {
		vaddr_t va = pted->pted_va & PAGE_MASK;
		pmap_t pm = pted->pted_pmap;
		struct pte_64 *ptp64;
		struct pte_32 *ptp32;
		int sr, idx;

		sr = ptesr(pm->pm_sr, va);
		idx = pteidx(sr, va);

		/* determine which pteg mapping is present in */
		if (ppc_proc_is_64b) {
			ptp64 = pmap_ptable64 +
				(idx ^ (PTED_HID(pted) ? pmap_ptab_mask : 0)) * 8;
			ptp64 += PTED_PTEGIDX(pted); /* increment by index into pteg */

			/*
			 * We now have the pointer to where it will be, if it is
			 * currently mapped. If the mapping was thrown away in
			 * exchange for another page mapping, then this page is
			 * not currently in the HASH.
			 *
			 * if we are not clearing bits, and have found all of the
			 * bits we want, we can stop
			 */
			if ((pted->p.pted_pte64.pte_hi |
			    (PTED_HID(pted) ? PTE_HID_64 : 0)) == ptp64->pte_hi) {
				bits |=	pmap_pte2flags(ptp64->pte_lo & ptebit);
				if (clear) {
					ptp64->pte_hi &= ~PTE_VALID_64;
					__asm__ volatile ("sync");
					tlbie(va);
					tlbsync();
					ptp64->pte_lo &= ~ptebit;
					__asm__ volatile ("sync");
					ptp64->pte_hi |= PTE_VALID_64;
				} else if (bits == flagbit)
					break;
			}
		} else {
			ptp32 = pmap_ptable32 +
				(idx ^ (PTED_HID(pted) ? pmap_ptab_mask : 0)) * 8;
			ptp32 += PTED_PTEGIDX(pted); /* increment by index into pteg */

			/*
			 * We now have the pointer to where it will be, if it is
			 * currently mapped. If the mapping was thrown away in
			 * exchange for another page mapping, then this page is
			 * not currently in the HASH.
			 *
			 * if we are not clearing bits, and have found all of the
			 * bits we want, we can stop
			 */
			if ((pted->p.pted_pte32.pte_hi |
			    (PTED_HID(pted) ? PTE_HID_32 : 0)) == ptp32->pte_hi) {
				bits |=	pmap_pte2flags(ptp32->pte_lo & ptebit);
				if (clear) {
					ptp32->pte_hi &= ~PTE_VALID_32;
					__asm__ volatile ("sync");
					tlbie(va);
					tlbsync();
					ptp32->pte_lo &= ~ptebit;
					__asm__ volatile ("sync");
					ptp32->pte_hi |= PTE_VALID_32;
				} else if (bits == flagbit)
					break;
			}
		}
	}

	if (clear) {
		/*
		 * this is done a second time, because while walking the list
		 * a bit could have been promoted via pmap_attr_save()
		 */
		bits |= pg->pg_flags & flagbit;
		atomic_clearbits_int(&pg->pg_flags,  flagbit); 
	} else
		atomic_setbits_int(&pg->pg_flags,  bits);

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
	 * Try not to reuse pmap ids, to spread the hash table usage.
	 */
again:
	for (i = 0; i < NPMAPS; i++) {
		try = pmap_id_avail + i;
		try = try % NPMAPS; /* truncate back into bounds */
		tblidx = try / (8 * sizeof usedsr[0]);
		tbloff = try % (8 * sizeof usedsr[0]);
		if ((usedsr[tblidx] & (1 << tbloff)) == 0) {
			/* pmap create lock? */
			s = splvm();
			if ((usedsr[tblidx] & (1 << tbloff)) == 1) {
				/* entry was stolen out from under us, retry */
				splx(s); /* pmap create unlock */
				goto again;
			}
			usedsr[tblidx] |= (1 << tbloff); 
			pmap_id_avail = try + 1;
			splx(s); /* pmap create unlock */

			seg = try << 4;
			for (k = 0; k < 16; k++)
				pm->pm_sr[k] = (seg + k) | SR_NOEXEC;
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

	s = splvm();
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
	int i, tblidx, tbloff;
	int s;

	pmap_vp_destroy(pm);
	i = (pm->pm_sr[0] & SR_VSID) >> 4;
	tblidx = i / (8  * sizeof usedsr[0]);
	tbloff = i % (8  * sizeof usedsr[0]);

	/* LOCK? */
	s = splvm();
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
			
			s = splvm();
			pool_put(&pmap_vp_pool, vp2);
			splx(s);
		}
		pm->pm_vp[i] = NULL;
		s = splvm();
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

	ndumpmem = 0;
	for (mp = pmap_mem; mp->size !=0; mp++, ndumpmem++) {
		physmem += atop(mp->size);
		dumpmem[ndumpmem].start = atop(mp->start);
		dumpmem[ndumpmem].end = atop(mp->start + mp->size);
	}

	for (mp = pmap_avail; mp->size !=0 ; mp++) {
		if (physmaxaddr <  mp->start + mp->size)
			physmaxaddr = mp->start + mp->size;
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

	ppc_check_procid();

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

#define HTABENTS_32 1024
#define HTABENTS_64 2048

	if (ppc_proc_is_64b) { 
		pmap_ptab_cnt = HTABENTS_64;
		while (pmap_ptab_cnt * 2 < physmem)
			pmap_ptab_cnt <<= 1;
	} else {
		pmap_ptab_cnt = HTABENTS_32;
		while (HTABSIZE_32 < (ptoa(physmem) >> 7))
			pmap_ptab_cnt <<= 1;
	}
	/*
	 * allocate suitably aligned memory for HTAB
	 */
	if (ppc_proc_is_64b) {
		pmap_ptable64 = pmap_steal_avail(HTABMEMSZ_64, HTABMEMSZ_64);
		bzero((void *)pmap_ptable64, HTABMEMSZ_64);
		pmap_ptab_mask = pmap_ptab_cnt - 1;
	} else {
		pmap_ptable32 = pmap_steal_avail(HTABSIZE_32, HTABSIZE_32);
		bzero((void *)pmap_ptable32, HTABSIZE_32);
		pmap_ptab_mask = pmap_ptab_cnt - 1;
	}

	/* allocate v->p mappings for pmap_kernel() */
	for (i = 0; i < VP_SR_SIZE; i++) {
		pmap_kernel()->pm_vp[i] = NULL;
	}
	vp1 = pmap_steal_avail(sizeof (struct pmapvp), 4);
	bzero (vp1, sizeof(struct pmapvp));
	pmap_kernel()->pm_vp[PPC_KERNEL_SR] = vp1;
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

	if (ppc_proc_is_64b) {
		vp1 = pmap_steal_avail(sizeof (struct pmapvp), 4);
		bzero (vp1, sizeof(struct pmapvp));
		pmap_kernel()->pm_vp[0] = vp1;
		for (i = 0; i < VP_IDX1_SIZE; i++) {
			vp2 = vp1->vp[i] =
			    pmap_steal_avail(sizeof (struct pmapvp), 4);
			bzero (vp2, sizeof(struct pmapvp));
			for (k = 0; k < VP_IDX2_SIZE; k++) {
				struct pte_desc *pted;
				pted = pmap_steal_avail(sizeof (struct pte_desc), 4);
				bzero (pted, sizeof (struct pte_desc));
				vp2->vp[k] = pted;
			}
		}
	}

	zero_page = VM_MIN_KERNEL_ADDRESS + ppc_kvm_stolen;
	ppc_kvm_stolen += PAGE_SIZE;
	copy_src_page = VM_MIN_KERNEL_ADDRESS + ppc_kvm_stolen;
	ppc_kvm_stolen += PAGE_SIZE;
	copy_dst_page = VM_MIN_KERNEL_ADDRESS + ppc_kvm_stolen;
	ppc_kvm_stolen += PAGE_SIZE;
	ppc_kvm_stolen += reserve_dumppages( (caddr_t)(VM_MIN_KERNEL_ADDRESS +
	    ppc_kvm_stolen));


	/*
	 * Initialize kernel pmap and hardware.
	 */
#if NPMAPS >= PPC_KERNEL_SEGMENT / 16
	usedsr[PPC_KERNEL_SEGMENT / 16 / (sizeof usedsr[0] * 8)]
		|= 1 << ((PPC_KERNEL_SEGMENT / 16) % (sizeof usedsr[0] * 8));
#endif
	for (i = 0; i < 16; i++) {
		pmap_kernel()->pm_sr[i] = (PPC_KERNEL_SEG0 + i) | SR_NOEXEC;
		ppc_mtsrin(PPC_KERNEL_SEG0 + i, i << ADDR_SR_SHIFT);
	}

	if (ppc_proc_is_64b) {
		for(i = 0; i < 0x10000; i++)
			pmap_kenter_cache(ptoa(i), ptoa(i), VM_PROT_ALL,
			    PMAP_CACHE_WB);
		asm volatile ("sync; mtsdr1 %0; isync"
		    :: "r"((u_int)pmap_ptable64 | HTABSIZE_64));
	} else 
		asm volatile ("sync; mtsdr1 %0; isync"
		    :: "r"((u_int)pmap_ptable32 | (pmap_ptab_mask >> 10)));

	pmap_avail_fixup();


	tlbia();

	pmap_avail_fixup();
	for (mp = pmap_avail; mp->size; mp++) {
		if (mp->start > 0x80000000)
			continue;
		if (mp->start + mp->size > 0x80000000)
			mp->size = 0x80000000 - mp->start;
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
	if (ppc_proc_is_64b)
		*pa = (pted->p.pted_pte64.pte_lo & PTE_RPGN_64) |
		    (va & ~PTE_RPGN_64);
	else
		*pa = (pted->p.pted_pte32.pte_lo & PTE_RPGN_32) |
		    (va & ~PTE_RPGN_32);
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
	    : "=r" (oldsr): "n"(PPC_USER_SR));
	asm volatile ("isync; mtsr %0,%1; isync"
	    :: "n"(PPC_USER_SR), "r"(sr));
	return oldsr;
}

void
pmap_popusr(u_int32_t sr)
{
	asm volatile ("isync; mtsr %0,%1; isync"
	    :: "n"(PPC_USER_SR), "r"(sr));
}

int
copyin(const void *udaddr, void *kaddr, size_t len)
{
	void *p;
	size_t l;
	u_int32_t oldsr;
	faultbuf env;
	void *oldh = curpcb->pcb_onfault;

	while (len > 0) {
		p = PPC_USER_ADDR + ((u_int)udaddr & ~PPC_SEGMENT_MASK);
		l = (PPC_USER_ADDR + PPC_SEGMENT_LENGTH) - p;
		if (l > len)
			l = len;
		oldsr = pmap_setusr(curpcb->pcb_pm, (vaddr_t)udaddr);
		if (setfault(&env)) {
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
copyout(const void *kaddr, void *udaddr, size_t len)
{
	void *p;
	size_t l;
	u_int32_t oldsr;
	faultbuf env;
	void *oldh = curpcb->pcb_onfault;

	while (len > 0) {
		p = PPC_USER_ADDR + ((u_int)udaddr & ~PPC_SEGMENT_MASK);
		l = (PPC_USER_ADDR + PPC_SEGMENT_LENGTH) - p;
		if (l > len)
			l = len;
		oldsr = pmap_setusr(curpcb->pcb_pm, (vaddr_t)udaddr);
		if (setfault(&env)) {
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
		p = PPC_USER_ADDR + ((u_int)uaddr & ~PPC_SEGMENT_MASK);
		l = (PPC_USER_ADDR + PPC_SEGMENT_LENGTH) - p;
		up = p;
		if (l > len)
			l = len;
		len -= l;
		oldsr = pmap_setusr(curpcb->pcb_pm, (vaddr_t)uaddr);
		if (setfault(&env)) {
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
		p = PPC_USER_ADDR + ((u_int)uaddr & ~PPC_SEGMENT_MASK);
		l = (PPC_USER_ADDR + PPC_SEGMENT_LENGTH) - p;
		up = p;
		if (l > len)
			l = len;
		len -= l;
		oldsr = pmap_setusr(curpcb->pcb_pm, (vaddr_t)uaddr);
		if (setfault(&env)) {
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
		start = ((u_int)PPC_USER_ADDR + ((u_int)va &
		    ~PPC_SEGMENT_MASK));
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

void
pmap_page_ro64(pmap_t pm, vaddr_t va, vm_prot_t prot)
{
	struct pte_64 *ptp64;
	struct pte_desc *pted;
	struct vm_page *pg;
	int sr, idx;

	pted = pmap_vp_lookup(pm, va);
	if (pted == NULL || !PTED_VALID(pted))
		return;

	pg = PHYS_TO_VM_PAGE(pted->p.pted_pte64.pte_lo & PTE_RPGN_64);
	if (pg->pg_flags & PG_PMAP_EXE) {
		if ((prot & (VM_PROT_WRITE|VM_PROT_EXECUTE)) == VM_PROT_WRITE) {
			atomic_clearbits_int(&pg->pg_flags, PG_PMAP_EXE);
		} else {
			pmap_syncicache_user_virt(pm, va);
		}
	}

	pted->p.pted_pte64.pte_lo &= ~PTE_PP_64;
	pted->p.pted_pte64.pte_lo |= PTE_RO_64;

	if ((prot & VM_PROT_EXECUTE) == 0)
		pted->p.pted_pte64.pte_lo |= PTE_N_64;

	sr = ptesr(pm->pm_sr, va);
	idx = pteidx(sr, va);

	/* determine which pteg mapping is present in */
	ptp64 = pmap_ptable64 +
	    (idx ^ (PTED_HID(pted) ? pmap_ptab_mask : 0)) * 8;
	ptp64 += PTED_PTEGIDX(pted); /* increment by index into pteg */

	/*
	 * We now have the pointer to where it will be, if it is
	 * currently mapped. If the mapping was thrown away in
	 * exchange for another page mapping, then this page is
	 * not currently in the HASH.
	 */
	if ((pted->p.pted_pte64.pte_hi | (PTED_HID(pted) ? PTE_HID_64 : 0))
	    == ptp64->pte_hi) {
		ptp64->pte_hi &= ~PTE_VALID_64;
		__asm__ volatile ("sync");
		tlbie(va);
		tlbsync();
		if (PTED_MANAGED(pted)) { /* XXX */
			pmap_attr_save(ptp64->pte_lo & PTE_RPGN_64,
			    ptp64->pte_lo & (PTE_REF_64|PTE_CHG_64));
		}
		ptp64->pte_lo &= ~PTE_CHG_64;
		ptp64->pte_lo &= ~PTE_PP_64;
		ptp64->pte_lo |= PTE_RO_64;
		__asm__ volatile ("sync");
		ptp64->pte_hi |= PTE_VALID_64;
	}
}

void
pmap_page_ro32(pmap_t pm, vaddr_t va, vm_prot_t prot)
{
	struct pte_32 *ptp32;
	struct pte_desc *pted;
	struct vm_page *pg = NULL;
	int sr, idx;

	pted = pmap_vp_lookup(pm, va);
	if (pted == NULL || !PTED_VALID(pted))
		return;

	pg = PHYS_TO_VM_PAGE(pted->p.pted_pte32.pte_lo & PTE_RPGN_32);
	if (pg->pg_flags & PG_PMAP_EXE) {
		if ((prot & (VM_PROT_WRITE|VM_PROT_EXECUTE)) == VM_PROT_WRITE) {
			atomic_clearbits_int(&pg->pg_flags, PG_PMAP_EXE);
		} else {
			pmap_syncicache_user_virt(pm, va);
		}
	}

	pted->p.pted_pte32.pte_lo &= ~PTE_PP_32;
	pted->p.pted_pte32.pte_lo |= PTE_RO_32;

	sr = ptesr(pm->pm_sr, va);
	idx = pteidx(sr, va);

	/* determine which pteg mapping is present in */
	ptp32 = pmap_ptable32 +
	    (idx ^ (PTED_HID(pted) ? pmap_ptab_mask : 0)) * 8;
	ptp32 += PTED_PTEGIDX(pted); /* increment by index into pteg */

	/*
	 * We now have the pointer to where it will be, if it is
	 * currently mapped. If the mapping was thrown away in
	 * exchange for another page mapping, then this page is
	 * not currently in the HASH.
	 */
	if ((pted->p.pted_pte32.pte_hi | (PTED_HID(pted) ? PTE_HID_32 : 0))
	    == ptp32->pte_hi) {
		ptp32->pte_hi &= ~PTE_VALID_32;
		__asm__ volatile ("sync");
		tlbie(va);
		tlbsync();
		if (PTED_MANAGED(pted)) { /* XXX */
			pmap_attr_save(ptp32->pte_lo & PTE_RPGN_32,
			    ptp32->pte_lo & (PTE_REF_32|PTE_CHG_32));
		}
		ptp32->pte_lo &= ~PTE_CHG_32;
		ptp32->pte_lo &= ~PTE_PP_32;
		ptp32->pte_lo |= PTE_RO_32;
		__asm__ volatile ("sync");
		ptp32->pte_hi |= PTE_VALID_32;
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
	int s;
	struct pte_desc *pted;

	/* need to lock for this pv */
	s = splvm();

	if (prot == VM_PROT_NONE) {
		while (!LIST_EMPTY(&(pg->mdpage.pv_list))) {
			pted = LIST_FIRST(&(pg->mdpage.pv_list));
			pmap_remove_pg(pted->pted_pmap, pted->pted_va);
		}
		/* page is being reclaimed, sync icache next use */
		atomic_clearbits_int(&pg->pg_flags, PG_PMAP_EXE);
		splx(s);
		return;
	}
	
	LIST_FOREACH(pted, &(pg->mdpage.pv_list), pted_pv_list) {
		if (ppc_proc_is_64b)
			pmap_page_ro64(pted->pted_pmap, pted->pted_va, prot);
		else
			pmap_page_ro32(pted->pted_pmap, pted->pted_va, prot);
	}
	splx(s);
}

void
pmap_protect(pmap_t pm, vaddr_t sva, vaddr_t eva, vm_prot_t prot)
{
	int s;
	if (prot & (VM_PROT_READ | VM_PROT_EXECUTE)) {
		s = splvm();
		if (ppc_proc_is_64b) {
			while (sva < eva) {
				pmap_page_ro64(pm, sva, prot);
				sva += PAGE_SIZE;
			}
		} else {
			while (sva < eva) {
				pmap_page_ro32(pm, sva, prot);
				sva += PAGE_SIZE;
			}
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
pmap_real_memory(paddr_t *start, vsize_t *size)
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

void
pmap_init()
{
	pool_init(&pmap_pmap_pool, sizeof(struct pmap), 0, 0, 0, "pmap", NULL);
	pool_setlowat(&pmap_pmap_pool, 2);
	pool_init(&pmap_vp_pool, sizeof(struct pmapvp), 0, 0, 0, "vp", NULL);
	pool_setlowat(&pmap_vp_pool, 10);
	pool_init(&pmap_pted_pool, sizeof(struct pte_desc), 0, 0, 0, "pted",
	    NULL);
	pool_setlowat(&pmap_pted_pool, 20);

	pmap_initialized = 1;
}

void
pmap_proc_iflush(struct proc *p, vaddr_t addr, vsize_t len)
{
	paddr_t pa;
	vsize_t clen;

	while (len > 0) {
		/* add one to always round up to the next page */
		clen = round_page(addr + 1) - addr;
		if (clen > len)
			clen = len;

		if (pmap_extract(p->p_vmspace->vm_map.pmap, addr, &pa)) {
			syncicache((void *)pa, clen);
		}

		len -= clen;
		addr += clen;
	}
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
	struct pte_desc pted_store;

	/* lookup is done physical to prevent faults */

	/* 
	 * This function only handles kernel faults, not supervisor copyins.
	 */
	if (msr & PSL_PR)
		return 0;

	/* if copyin, throw to full excption handler */
	if (VP_SR(va) == PPC_USER_SR)
		return 0;

	pm = pmap_kernel();


	if (va < physmaxaddr) {
		u_int32_t aligned_va;
		pted =  &pted_store;
		/* 0 - physmaxaddr mapped 1-1 */
		/* XXX - no WRX control */

		aligned_va = trunc_page(va);
		if (ppc_proc_is_64b) {
			pmap_fill_pte64(pm, aligned_va, aligned_va,
			    pted, VM_PROT_READ | VM_PROT_WRITE |
			    VM_PROT_EXECUTE, 0, PMAP_CACHE_WB);
			pte_insert64(pted);
			return 1;
		} else {
			pmap_fill_pte32(pm, aligned_va, aligned_va,
			    &pted_store, VM_PROT_READ | VM_PROT_WRITE |
			    VM_PROT_EXECUTE, 0, PMAP_CACHE_WB);
			pte_insert32(pted);
			return 1;
		}
		/* NOTREACHED */
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

	if (ppc_proc_is_64b) {
		/* check write fault and we have a readonly mapping */
		if ((dsisr & (1 << (31-6))) &&
		    (pted->p.pted_pte64.pte_lo & 0x1))
			return 0;
		if ((exec_fault != 0)
		    && ((pted->pted_va & PTED_VA_EXEC_M) == 0)) {
			/* attempted to execute non-executable page */
			return 0;
		}
		pte_insert64(pted);
	} else {
		/* check write fault and we have a readonly mapping */
		if ((dsisr & (1 << (31-6))) &&
		    (pted->p.pted_pte32.pte_lo & 0x1))
			return 0;
		if ((exec_fault != 0)
		    && ((pted->pted_va & PTED_VA_EXEC_M) == 0)) {
			/* attempted to execute non-executable page */
			return 0;
		}
		pte_insert32(pted);
	}

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
	if (ppc_proc_is_64b) {
		/* check write fault and we have a readonly mapping */
		if ((dsisr & (1 << (31-6))) &&
		    (pted->p.pted_pte64.pte_lo & 0x1))
			return 0;
	} else {
		/* check write fault and we have a readonly mapping */
		if ((dsisr & (1 << (31-6))) &&
		    (pted->p.pted_pte32.pte_lo & 0x1))
			return 0;
	}
	if ((exec_fault != 0)
	    && ((pted->pted_va & PTED_VA_EXEC_M) == 0)) {
		/* attempted to execute non-executable page */
		return 0;
	}
	if (ppc_proc_is_64b)
		pte_insert64(pted);
	else
		pte_insert32(pted);
	return 1;
}


/*
 * should pte_insert code avoid wired mappings?
 * is the stack safe?
 * is the pted safe? (physical)
 * -ugh
 */
void
pte_insert64(struct pte_desc *pted)
{
	int off;
	int secondary;
	struct pte_64 *ptp64;
	int sr, idx;
	int i;


	sr = ptesr(pted->pted_pmap->pm_sr, pted->pted_va);
	idx = pteidx(sr, pted->pted_va);

	ptp64 = pmap_ptable64 +
	    (idx ^ (PTED_HID(pted) ? pmap_ptab_mask : 0)) * 8;
	ptp64 += PTED_PTEGIDX(pted); /* increment by index into pteg */
	if ((pted->p.pted_pte64.pte_hi |
	    (PTED_HID(pted) ? PTE_HID_64 : 0)) == ptp64->pte_hi)
		pte_zap(ptp64,pted);

	pted->pted_va &= ~(PTED_VA_HID_M|PTED_VA_PTEGIDX_M);

	/*
	 * instead of starting at the beginning of each pteg,
	 * the code should pick a random location with in the primary
	 * then search all of the entries, then if not yet found,
	 * do the same for the secondary.
	 * this would reduce the frontloading of the pteg.
	 */
	/* first just try fill of primary hash */
	ptp64 = pmap_ptable64 + (idx) * 8;
	for (i = 0; i < 8; i++) {
		if (ptp64[i].pte_hi & PTE_VALID_64)
			continue;
		if (pmap_hash_lock_try(i) == 0)
			continue;

		/* not valid, just load */
		pted->pted_va |= i;
		ptp64[i].pte_hi =
		    pted->p.pted_pte64.pte_hi & ~PTE_VALID_64;
		ptp64[i].pte_lo = pted->p.pted_pte64.pte_lo;
		__asm__ volatile ("sync");
		ptp64[i].pte_hi |= PTE_VALID_64;
		__asm volatile ("sync");

		pmap_hash_unlock(i);
		return;
	}
	/* try fill of secondary hash */
	ptp64 = pmap_ptable64 + (idx ^ pmap_ptab_mask) * 8;
	for (i = 0; i < 8; i++) {
		if (ptp64[i].pte_hi & PTE_VALID_64)
			continue;
		if (pmap_hash_lock_try(i) == 0)
			continue;

		pted->pted_va |= (i | PTED_VA_HID_M);
		ptp64[i].pte_hi =
		    (pted->p.pted_pte64.pte_hi | PTE_HID_64) & ~PTE_VALID_64;
		ptp64[i].pte_lo = pted->p.pted_pte64.pte_lo;
		__asm__ volatile ("sync");
		ptp64[i].pte_hi |= PTE_VALID_64;
		__asm volatile ("sync");

		pmap_hash_unlock(i);
		return;
	}

	/* need decent replacement algorithm */
busy:
	__asm__ volatile ("mftb %0" : "=r"(off));
	secondary = off & 8;

	if (pmap_hash_lock_try(off & 7) == 0)
		goto busy;

	pted->pted_va |= off & (PTED_VA_PTEGIDX_M|PTED_VA_HID_M);

	idx = (idx ^ (PTED_HID(pted) ? pmap_ptab_mask : 0));

	ptp64 = pmap_ptable64 + (idx * 8);
	ptp64 += PTED_PTEGIDX(pted); /* increment by index into pteg */
	if (ptp64->pte_hi & PTE_VALID_64) {
		vaddr_t va;
		ptp64->pte_hi &= ~PTE_VALID_64;
		__asm volatile ("sync");
		
		/* Bits 9-19 */
		idx = (idx ^ ((ptp64->pte_hi & PTE_HID_64) ?
		    pmap_ptab_mask : 0));
		va = (ptp64->pte_hi >> PTE_VSID_SHIFT_64) ^ idx;
		va <<= ADDR_PIDX_SHIFT;
		/* Bits 4-8 */
		va |= (ptp64->pte_hi & PTE_API_64) << ADDR_API_SHIFT_32;
		/* Bits 0-3 */
		va |= (ptp64->pte_hi >> PTE_VSID_SHIFT_64)
		    << ADDR_SR_SHIFT;
		tlbie(va);

		tlbsync();
		pmap_attr_save(ptp64->pte_lo & PTE_RPGN_64,
		    ptp64->pte_lo & (PTE_REF_64|PTE_CHG_64));
	}

	if (secondary)
		ptp64->pte_hi =
		    (pted->p.pted_pte64.pte_hi | PTE_HID_64) &
		    ~PTE_VALID_64;
	 else 
		ptp64->pte_hi = pted->p.pted_pte64.pte_hi & 
		    ~PTE_VALID_64;

	ptp64->pte_lo = pted->p.pted_pte64.pte_lo;
	__asm__ volatile ("sync");
	ptp64->pte_hi |= PTE_VALID_64;

	pmap_hash_unlock(off & 7);
}

void
pte_insert32(struct pte_desc *pted)
{
	int off;
	int secondary;
	struct pte_32 *ptp32;
	int sr, idx;
	int i;

	sr = ptesr(pted->pted_pmap->pm_sr, pted->pted_va);
	idx = pteidx(sr, pted->pted_va);

	/* determine if ptp is already mapped */
	ptp32 = pmap_ptable32 +
	    (idx ^ (PTED_HID(pted) ? pmap_ptab_mask : 0)) * 8;
	ptp32 += PTED_PTEGIDX(pted); /* increment by index into pteg */
	if ((pted->p.pted_pte32.pte_hi |
	    (PTED_HID(pted) ? PTE_HID_32 : 0)) == ptp32->pte_hi)
		pte_zap(ptp32,pted);

	pted->pted_va &= ~(PTED_VA_HID_M|PTED_VA_PTEGIDX_M);

	/*
	 * instead of starting at the beginning of each pteg,
	 * the code should pick a random location with in the primary
	 * then search all of the entries, then if not yet found,
	 * do the same for the secondary.
	 * this would reduce the frontloading of the pteg.
	 */

	/* first just try fill of primary hash */
	ptp32 = pmap_ptable32 + (idx) * 8;
	for (i = 0; i < 8; i++) {
		if (ptp32[i].pte_hi & PTE_VALID_32)
			continue;
		if (pmap_hash_lock_try(i) == 0)
			continue;

		/* not valid, just load */
		pted->pted_va |= i;
		ptp32[i].pte_hi = pted->p.pted_pte32.pte_hi & ~PTE_VALID_32;
		ptp32[i].pte_lo = pted->p.pted_pte32.pte_lo;
		__asm__ volatile ("sync");
		ptp32[i].pte_hi |= PTE_VALID_32;
		__asm volatile ("sync");

		pmap_hash_unlock(i);
		return;
	}
	/* try fill of secondary hash */
	ptp32 = pmap_ptable32 + (idx ^ pmap_ptab_mask) * 8;
	for (i = 0; i < 8; i++) {
		if (ptp32[i].pte_hi & PTE_VALID_32)
			continue;
		if (pmap_hash_lock_try(i) == 0)
			continue;

		pted->pted_va |= (i | PTED_VA_HID_M);
		ptp32[i].pte_hi =
		    (pted->p.pted_pte32.pte_hi | PTE_HID_32) & ~PTE_VALID_32;
		ptp32[i].pte_lo = pted->p.pted_pte32.pte_lo;
		__asm__ volatile ("sync");
		ptp32[i].pte_hi |= PTE_VALID_32;
		__asm volatile ("sync");

		pmap_hash_unlock(i);
		return;
	}

	/* need decent replacement algorithm */
busy:
	__asm__ volatile ("mftb %0" : "=r"(off));
	secondary = off & 8;
	if (pmap_hash_lock_try(off & 7) == 0)
		goto busy;

	pted->pted_va |= off & (PTED_VA_PTEGIDX_M|PTED_VA_HID_M);

	idx = (idx ^ (PTED_HID(pted) ? pmap_ptab_mask : 0));

	ptp32 = pmap_ptable32 + (idx * 8);
	ptp32 += PTED_PTEGIDX(pted); /* increment by index into pteg */
	if (ptp32->pte_hi & PTE_VALID_32) {
		vaddr_t va;
		ptp32->pte_hi &= ~PTE_VALID_32;
		__asm volatile ("sync");

		va = ((ptp32->pte_hi & PTE_API_32) << ADDR_API_SHIFT_32) |
		     ((((ptp32->pte_hi >> PTE_VSID_SHIFT_32) & SR_VSID)
			^(idx ^ ((ptp32->pte_hi & PTE_HID_32) ? 0x3ff : 0)))
			    & 0x3ff) << PAGE_SHIFT;
		tlbie(va);

		tlbsync();
		pmap_attr_save(ptp32->pte_lo & PTE_RPGN_32,
		    ptp32->pte_lo & (PTE_REF_32|PTE_CHG_32));
	}
	if (secondary)
		ptp32->pte_hi =
		    (pted->p.pted_pte32.pte_hi | PTE_HID_32) & ~PTE_VALID_32;
	else
		ptp32->pte_hi = pted->p.pted_pte32.pte_hi & ~PTE_VALID_32;
	ptp32->pte_lo = pted->p.pted_pte32.pte_lo;
	__asm__ volatile ("sync");
	ptp32->pte_hi |= PTE_VALID_32;

	pmap_hash_unlock(off & 7);
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
		if (ppc_proc_is_64b) {
			print("ptehi %x ptelo %x ptp %x Aptp %x\n",
			    pted->p.pted_pte64.pte_hi,
			    pted->p.pted_pte64.pte_lo,
			    pmap_ptable +
				8*pteidx(ptesr(pted->pted_pmap->pm_sr, va), va),
			    pmap_ptable +
				8*(pteidx(ptesr(pted->pted_pmap->pm_sr, va), va)
				    ^ pmap_ptab_mask)
			    );
		} else {
			print("ptehi %x ptelo %x ptp %x Aptp %x\n",
			    pted->p.pted_pte32.pte_hi,
			    pted->p.pted_pte32.pte_lo,
			    pmap_ptable +
				8*pteidx(ptesr(pted->pted_pmap->pm_sr, va), va),
			    pmap_ptable +
				8*(pteidx(ptesr(pted->pted_pmap->pm_sr, va), va)
				    ^ pmap_ptab_mask)
			    );
		}
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
	struct pte_desc *pted;
	struct vm_page *pg;

	pg = PHYS_TO_VM_PAGE(pa);
	if (pg == NULL) {
		db_printf("pa %x: unmanaged\n");
	} else {
		LIST_FOREACH(pted, &(pg->mdpage.pv_list), pted_pv_list) {
			pmap_print_pted(pted, db_printf);
		}
	}
	return 0;
}
#endif
