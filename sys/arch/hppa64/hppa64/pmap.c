/*	$OpenBSD: pmap.c,v 1.18 2011/07/07 18:40:12 kettenis Exp $	*/

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

#define	PMAPDEBUG

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/pool.h>
#include <sys/extent.h>

#include <uvm/uvm.h>

#include <machine/iomod.h>

#include <dev/rndvar.h>

#ifdef PMAPDEBUG
#define	DPRINTF(l,s)	do {		\
	if ((pmapdebug & (l)) == (l))	\
		printf s;		\
} while(0)
#define	PDB_FOLLOW	0x00000001
#define	PDB_INIT	0x00000002
#define	PDB_ENTER	0x00000004
#define	PDB_REMOVE	0x00000008
#define	PDB_CREATE	0x00000010
#define	PDB_PTPAGE	0x00000020
#define	PDB_CACHE	0x00000040
#define	PDB_BITS	0x00000080
#define	PDB_COLLECT	0x00000100
#define	PDB_PROTECT	0x00000200
#define	PDB_EXTRACT	0x00000400
#define	PDB_VP		0x00000800
#define	PDB_PV		0x00001000
#define	PDB_PARANOIA	0x00002000
#define	PDB_WIRING	0x00004000
#define	PDB_PMAP	0x00008000
#define	PDB_STEAL	0x00010000
#define	PDB_PHYS	0x00020000
#define	PDB_POOL	0x00040000
int pmapdebug = 0
	| PDB_INIT
/*	| PDB_FOLLOW */
/*	| PDB_VP */
/*	| PDB_PV */
/*	| PDB_ENTER */
/*	| PDB_REMOVE */
/*	| PDB_STEAL */
/*	| PDB_PROTECT */
/*	| PDB_PHYS */
	;
#else
#define	DPRINTF(l,s)	/* */
#endif

paddr_t physical_steal, physical_end;

struct pmap	kernel_pmap_store;
struct pool	pmap_pmap_pool;
struct pool	pmap_pv_pool;
int		pmap_pvlowat = 252;
int 		pmap_initialized;
int		pmap_nkpdes = 32;

pt_entry_t	hppa_prot[8];
#define	pmap_prot(m,vp) (hppa_prot[(vp)] | ((m) == pmap_kernel()? 0 : PTE_USER))

pt_entry_t	kernel_ptes[] = {
	PTE_EXEC  | PTE_ORDER | PTE_PREDICT | PTE_WIRED |
	    TLB_PAGE(0x000000) | PTE_PG4M,
	PTE_WRITE | PTE_ORDER | PTE_DIRTY   | PTE_WIRED |
	    TLB_PAGE(0x400000) | PTE_PG4M,
	PTE_WRITE | PTE_ORDER | PTE_DIRTY   | PTE_WIRED |
	    TLB_PAGE(0x800000) | PTE_PG4M,
	PTE_WRITE | PTE_ORDER | PTE_DIRTY   | PTE_WIRED |
	    TLB_PAGE(0xc00000) | PTE_PG4M
};

#define	pmap_pvh_attrs(a) \
	(((a) & PTE_DIRTY) | ((a) ^ PTE_REFTRAP))

struct vm_page	*pmap_pagealloc(int wait);
volatile pt_entry_t *pmap_pde_get(volatile u_int32_t *pd, vaddr_t va);
void		 pmap_pde_set(struct pmap *pm, vaddr_t va, paddr_t ptp);
void		 pmap_pte_flush(struct pmap *pmap, vaddr_t va, pt_entry_t pte);
pt_entry_t *	 pmap_pde_alloc(struct pmap *pm, vaddr_t va,
		    struct vm_page **pdep);
#ifdef DDB
void		 pmap_dump_table(pa_space_t space, vaddr_t sva);
void		 pmap_dump_pv(paddr_t pa);
#endif
int		 pmap_check_alias(struct pv_entry *pve, vaddr_t va,
		    pt_entry_t pte);
void		 pmap_pv_free(struct pv_entry *pv);
void		 pmap_pv_enter(struct vm_page *pg, struct pv_entry *pve,
		    struct pmap *pm, vaddr_t va, struct vm_page *pdep);
struct pv_entry *pmap_pv_remove(struct vm_page *pg, struct pmap *pmap,
		    vaddr_t va);
void		 pmap_maphys(paddr_t spa, paddr_t epa);

struct vm_page *
pmap_pagealloc(int wait)
{
	struct vm_page *pg;

	if ((pg = uvm_pagealloc(NULL, 0, NULL,
	    UVM_PGA_USERESERVE | UVM_PGA_ZERO)) == NULL)
		printf("pmap_pagealloc fail\n");

	return (pg);
}

volatile pt_entry_t *
pmap_pde_get(volatile u_int32_t *pd, vaddr_t va)
{
	int i;

	DPRINTF(PDB_FOLLOW|PDB_VP,
	    ("pmap_pde_get(%p, 0x%lx)\n", pd, va));

	i = (va & PIE_MASK) >> PIE_SHIFT;
	if (i) {
		pd = (volatile u_int32_t *)((u_int64_t)pd[i] << PAGE_SHIFT);

		if (!pd)
			return (NULL);
	} else
		pd += PAGE_SIZE / sizeof(*pd);

	i = (va & PDE_MASK) >> PDE_SHIFT;
	return (pt_entry_t *)((u_int64_t)pd[i] << PAGE_SHIFT);
}

void
pmap_pde_set(struct pmap *pm, vaddr_t va, paddr_t ptp)
{
	volatile u_int32_t *pd = pm->pm_pdir;
	int i;

	DPRINTF(PDB_FOLLOW|PDB_VP,
	    ("pmap_pde_set(%p, 0x%lx, 0x%lx)\n", pm, va, ptp));

	i = (va & PIE_MASK) >> PIE_SHIFT;
	if (i)
		pd = (volatile u_int32_t *)((u_int64_t)pd[i] << PAGE_SHIFT);
	else
		pd += PAGE_SIZE / sizeof(*pd);

	i = (va & PDE_MASK) >> PDE_SHIFT;
	pd[i] = ptp >> PAGE_SHIFT;
}

pt_entry_t *
pmap_pde_alloc(struct pmap *pm, vaddr_t va, struct vm_page **pdep)
{
	struct vm_page *pg;
	paddr_t pa;

	DPRINTF(PDB_FOLLOW|PDB_VP,
	    ("pmap_pde_alloc(%p, 0x%lx, %p)\n", pm, va, pdep));

	if ((pg = pmap_pagealloc(0)) == NULL)
		return (NULL);

	pa = VM_PAGE_TO_PHYS(pg);

	DPRINTF(PDB_FOLLOW|PDB_VP, ("pmap_pde_alloc: pde %lx\n", pa));

	atomic_clearbits_int(&pg->pg_flags, PG_BUSY);
	pg->wire_count = 1;		/* no mappings yet */
	pmap_pde_set(pm, va, pa);
	pm->pm_stats.resident_count++;	/* count PTP as resident */
	pm->pm_ptphint = pg;
	if (pdep)
		*pdep = pg;
	return ((pt_entry_t *)pa);
}

static __inline struct vm_page *
pmap_pde_ptp(struct pmap *pm, volatile pt_entry_t *pde)
{
	paddr_t pa = (paddr_t)pde;

	DPRINTF(PDB_FOLLOW|PDB_PV, ("pmap_pde_ptp(%p, %p)\n", pm, pde));

	if (pm->pm_ptphint && VM_PAGE_TO_PHYS(pm->pm_ptphint) == pa)
		return (pm->pm_ptphint);

	DPRINTF(PDB_FOLLOW|PDB_PV, ("pmap_pde_ptp: lookup 0x%lx\n", pa));

	return (PHYS_TO_VM_PAGE(pa));
}

static __inline void
pmap_pde_release(struct pmap *pmap, vaddr_t va, struct vm_page *ptp)
{
	DPRINTF(PDB_FOLLOW|PDB_PV,
	    ("pmap_pde_release(%p, 0x%lx, %p)\n", pmap, va, ptp));

	if (pmap != pmap_kernel() && --ptp->wire_count <= 1) {
		DPRINTF(PDB_FOLLOW|PDB_PV,
		    ("pmap_pde_release: disposing ptp %p\n", ptp));
		pmap_pde_set(pmap, va, 0);
		pmap->pm_stats.resident_count--;
		if (pmap->pm_ptphint == ptp)
			pmap->pm_ptphint = NULL;
		ptp->wire_count = 0;
#ifdef DIAGNOSTIC
		if (ptp->pg_flags & PG_BUSY)
			panic("pmap_pde_release: busy page table page");
#endif
		pdcache(HPPA_SID_KERNEL, (vaddr_t)ptp, PAGE_SIZE);
		pdtlb(HPPA_SID_KERNEL, (vaddr_t)ptp);
		uvm_pagefree(ptp);
	}
}

static __inline pt_entry_t
pmap_pte_get(volatile pt_entry_t *pde, vaddr_t va)
{
	DPRINTF(PDB_FOLLOW|PDB_VP,
	    ("pmap_pte_get(%p, 0x%lx)\n", pde, va));

	return (pde[(va & PTE_MASK) >> PTE_SHIFT]);
}

static __inline void
pmap_pte_set(volatile pt_entry_t *pde, vaddr_t va, pt_entry_t pte)
{
	DPRINTF(PDB_FOLLOW|PDB_VP,
	    ("pmap_pte_set(%p, 0x%lx, 0x%lx)\n", pde, va, pte));

	pde[(va & PTE_MASK) >> PTE_SHIFT] = pte;
}

void
pmap_pte_flush(struct pmap *pmap, vaddr_t va, pt_entry_t pte)
{
	fdcache(pmap->pm_space, va, PAGE_SIZE);
	if (pte & PTE_EXEC) {
		ficache(pmap->pm_space, va, PAGE_SIZE);
		pdtlb(pmap->pm_space, va);
		pitlb(pmap->pm_space, va);
	} else
		pdtlb(pmap->pm_space, va);
}

static __inline pt_entry_t
pmap_vp_find(struct pmap *pm, vaddr_t va)
{
	volatile pt_entry_t *pde;

	if (!(pde = pmap_pde_get(pm->pm_pdir, va)))
		return (0);

	return (pmap_pte_get(pde, va));
}

#ifdef DDB
void
pmap_dump_table(pa_space_t space, vaddr_t sva)
{
	pa_space_t sp;
	volatile pt_entry_t *pde;
	volatile u_int32_t *pd;
	pt_entry_t pte;
	vaddr_t va, pdemask;

	if (space)
		pd = (u_int32_t *)mfctl(CR_VTOP);
	else
		pd = pmap_kernel()->pm_pdir;

	for (pdemask = 1, va = sva ? sva : 0;
	    va < VM_MAX_ADDRESS; va += PAGE_SIZE) {
		if (pdemask != (va & (PDE_MASK|PIE_MASK))) {
			pdemask = va & (PDE_MASK|PIE_MASK);
			if (!(pde = pmap_pde_get(pd, va))) {
				va += ~PDE_MASK + 1 - PAGE_SIZE;
				continue;
			}
			printf("%x:%8p:\n", sp, pde);
		}

		if (!(pte = pmap_pte_get(pde, va)))
			continue;

		printf("0x%08lx-0x%08lx:%b\n",
		    va, PTE_PAGE(pte), PTE_GETBITS(pte), PTE_BITS);
	}
}

void
pmap_dump_pv(paddr_t pa)
{
	struct vm_page *pg;
	struct pv_entry *pve;

	pg = PHYS_TO_VM_PAGE(pa);
	simple_lock(&pg->mdpage.pvh_lock);
	for(pve = pg->mdpage.pvh_list; pve; pve = pve->pv_next)
		printf("%x:%lx\n", pve->pv_pmap->pm_space, pve->pv_va);
	simple_unlock(&pg->mdpage.pvh_lock);
}
#endif

#ifdef PMAPDEBUG
int
pmap_check_alias(struct pv_entry *pve, vaddr_t va, pt_entry_t pte)
{
	int ret;

	/* check for non-equ aliased mappings */
	for (ret = 0; pve; pve = pve->pv_next) {
		pte |= pmap_vp_find(pve->pv_pmap, pve->pv_va);
		if ((va & HPPA_PGAOFF) != (pve->pv_va & HPPA_PGAOFF) &&
		    (pte & PTE_WRITE)) {
			printf("pmap_check_alias: "
			    "aliased writable mapping 0x%x:0x%lx\n",
			    pve->pv_pmap->pm_space, pve->pv_va);
			ret++;
		}
	}

	return (ret);
}
#endif

static __inline struct pv_entry *
pmap_pv_alloc(void)
{
	struct pv_entry *pv;

	DPRINTF(PDB_FOLLOW|PDB_PV, ("pmap_pv_alloc()\n"));

	pv = pool_get(&pmap_pv_pool, PR_NOWAIT);

	DPRINTF(PDB_FOLLOW|PDB_PV, ("pmap_pv_alloc: %p\n", pv));

	return (pv);
}

void
pmap_pv_free(struct pv_entry *pv)
{
	if (pv->pv_ptp)
		pmap_pde_release(pv->pv_pmap, pv->pv_va, pv->pv_ptp);

	pool_put(&pmap_pv_pool, pv);
}

void
pmap_pv_enter(struct vm_page *pg, struct pv_entry *pve, struct pmap *pm,
    vaddr_t va, struct vm_page *pdep)
{
	DPRINTF(PDB_FOLLOW|PDB_PV, ("pmap_pv_enter(%p, %p, %p, 0x%lx, %p)\n",
	    pg, pve, pm, va, pdep));
	pve->pv_pmap	= pm;
	pve->pv_va	= va;
	pve->pv_ptp	= pdep;
	pve->pv_next	= pg->mdpage.pvh_list;
	pg->mdpage.pvh_list = pve;
#ifdef PMAPDEBUG
	if (pmap_check_alias(pve, va, 0))
		Debugger();
#endif
}

struct pv_entry *
pmap_pv_remove(struct vm_page *pg, struct pmap *pmap, vaddr_t va)
{
	struct pv_entry **pve, *pv;

	DPRINTF(PDB_FOLLOW|PDB_PV, ("pmap_pv_remove(%p, %p, 0x%lx)\n",
	    pg, pmap, va));

	simple_lock(&pg->mdpage.pvh_lock);	/* lock pv_head */
	for(pv = *(pve = &pg->mdpage.pvh_list);
	    pv; pv = *(pve = &(*pve)->pv_next))
		if (pv->pv_pmap == pmap && pv->pv_va == va) {
			*pve = pv->pv_next;
			break;
		}
	simple_unlock(&pg->mdpage.pvh_lock);	/* unlock, done! */
	return (pv);
}

const pt_entry_t hppa_pgs[] = {
	PTE_PG4K,
	PTE_PG16K,
	PTE_PG64K,
	PTE_PG256K,
	PTE_PG1M,
	PTE_PG4M,
	PTE_PG16M,
	PTE_PG64M
};

void
pmap_maphys(paddr_t spa, paddr_t epa)
{
	volatile pt_entry_t *pde, *epde, pte;
	paddr_t pa, tpa;
	int s, e, i;

	DPRINTF(PDB_INIT, ("pmap_maphys: mapping 0x%lx - 0x%lx\n", spa, epa));

	s = ffs(spa) - 12;
	e = ffs(epa) - 12;

	if (s < e || (s == e && s / 2 < nitems(hppa_pgs))) {
		i = s / 2;
		if (i > nitems(hppa_pgs))
			i = nitems(hppa_pgs);
		pa = spa;
		spa = tpa = 0x1000 << ((i + 1) * 2);
	} else if (s > e) {
		i = e / 2;
		if (i > nitems(hppa_pgs))
			i = nitems(hppa_pgs);
		epa = pa = epa & (0xfffff000 << ((i + 1) * 2));
		tpa = epa;
	} else {
		i = s / 2;
		if (i > nitems(hppa_pgs))
			i = nitems(hppa_pgs);
		pa = spa;
		spa = tpa = epa;
	}

printf("pa 0x%lx tpa 0x%lx\n", pa, tpa);
	while (pa < tpa) {
		pte = TLB_PAGE(pa) | hppa_pgs[i - 1] |
		    PTE_WRITE | PTE_ORDER | PTE_DIRTY | PTE_WIRED;
		pde = pmap_pde_get(pmap_kernel()->pm_pdir, pa);
		epde = pde + (PTE_MASK >> PTE_SHIFT) + 1;
		if (pa + (PTE_MASK + (1 << PTE_SHIFT)) > tpa)
			epde = pde + ((tpa & PTE_MASK) >> PTE_SHIFT);
printf("pde %p epde %p pte 0x%lx\n", pde, epde, pte);
		for (pde += (pa & PTE_MASK) >> PTE_SHIFT; pde < epde;)
			*pde++ = pte;
		pa += PTE_MASK + (1 << PTE_SHIFT);
		pa &= ~(PTE_MASK | PAGE_MASK);
	}

	if (spa < epa)
		pmap_maphys(spa, epa);
}

void
pmap_bootstrap(vaddr_t vstart)
{
	extern int resvphysmem, __rodata_end, __data_start;
	vaddr_t va, eaddr, addr = round_page(vstart);
	struct pmap *kpm;

	DPRINTF(PDB_FOLLOW|PDB_INIT, ("pmap_bootstrap(0x%lx)\n", vstart));

	uvm_setpagesize();

	hppa_prot[UVM_PROT_NONE]  = PTE_ORDER|PTE_ACC_NONE;
	hppa_prot[UVM_PROT_READ]  = PTE_ORDER|PTE_READ;
	hppa_prot[UVM_PROT_WRITE] = PTE_ORDER|PTE_WRITE;
	hppa_prot[UVM_PROT_RW]    = PTE_ORDER|PTE_READ|PTE_WRITE;
	hppa_prot[UVM_PROT_EXEC]  = PTE_ORDER|PTE_EXEC;
	hppa_prot[UVM_PROT_RX]    = PTE_ORDER|PTE_READ|PTE_EXEC;
	hppa_prot[UVM_PROT_WX]    = PTE_ORDER|PTE_WRITE|PTE_EXEC;
	hppa_prot[UVM_PROT_RWX]   = PTE_ORDER|PTE_READ|PTE_WRITE|PTE_EXEC;

	/*
	 * Initialize kernel pmap
	 */
	kpm = &kernel_pmap_store;
	bzero(kpm, sizeof(*kpm));
	simple_lock_init(&kpm->pm_lock);
	kpm->pm_refcount = 1;
	kpm->pm_space = HPPA_SID_KERNEL;
	TAILQ_INIT(&kpm->pm_pglist);
	kpm->pm_pdir = (u_int32_t *)mfctl(CR_VTOP);

	/*
	 * Allocate various tables and structures.
	 */

	if (&__rodata_end < &__data_start) {
		physical_steal = (vaddr_t)&__rodata_end;
		physical_end = (vaddr_t)&__data_start;
		DPRINTF(PDB_INIT, ("physpool: 0x%lx @ 0x%lx\n",
		    physical_end - physical_steal, physical_steal));
	}

	/* map enough PDEs to map initial physmem */
	for (va = 0x1000000, eaddr = ptoa(physmem);
	    va < eaddr; addr += PAGE_SIZE, va += 1 << PDE_SHIFT) {
		bzero((void *)addr, PAGE_SIZE);
		pmap_pde_set(kpm, va, addr);
		kpm->pm_stats.resident_count++;	/* count PTP as resident */
	}

	/* map a little of initial kmem */
	for (va = VM_MIN_KERNEL_ADDRESS + ((pmap_nkpdes - 1) << PDE_SHIFT);
	    va >= VM_MIN_KERNEL_ADDRESS;
	    addr += PAGE_SIZE, va -= 1 << PDE_SHIFT) {
		bzero((void *)addr, PAGE_SIZE);
		pmap_pde_set(kpm, va, addr);
		kpm->pm_stats.resident_count++;	/* count PTP as resident */
	}

	pmap_maphys(0x1000000, ptoa(physmem));

	eaddr = physmem - atop(round_page(MSGBUFSIZE));
	resvphysmem = atop(addr);
	DPRINTF(PDB_INIT, ("physmem: 0x%lx - 0x%lx\n", resvphysmem, eaddr));
	uvm_page_physload(0, physmem, resvphysmem, eaddr, 0);
}

void
pmap_init(void)
{
	DPRINTF(PDB_FOLLOW|PDB_INIT, ("pmap_init()\n"));

	pool_init(&pmap_pmap_pool, sizeof(struct pmap), 0, 0, 0, "pmappl",
	    &pool_allocator_nointr);
	pool_init(&pmap_pv_pool, sizeof(struct pv_entry),0,0,0, "pmappv", NULL);
	pool_setlowat(&pmap_pv_pool, pmap_pvlowat);
	pool_sethiwat(&pmap_pv_pool, pmap_pvlowat * 32);

	pmap_initialized = 1;

	DPRINTF(PDB_FOLLOW|PDB_INIT, ("pmap_init(): done\n"));
}

#ifdef PMAP_STEAL_MEMORY
vaddr_t
pmap_steal_memory(vsize_t size, vaddr_t *vstartp, vaddr_t *vendp)
{
	vaddr_t va;
	int npg;

	DPRINTF(PDB_FOLLOW|PDB_PHYS,
	    ("pmap_steal_memory(0x%lx, %p, %p)\n", size, vstartp, vendp));

	size = round_page(size);
	npg = atop(size);

	if (vm_physmem[0].avail_end - vm_physmem[0].avail_start < npg)
		panic("pmap_steal_memory: no more");

	if (vstartp)
		*vstartp = VM_MIN_KERNEL_ADDRESS;
	if (vendp)
		*vendp = VM_MAX_KERNEL_ADDRESS;

	vm_physmem[0].end -= npg;
	vm_physmem[0].avail_end -= npg;
	va = ptoa(vm_physmem[0].avail_end);
	bzero((void *)va, size);

	DPRINTF(PDB_FOLLOW|PDB_PHYS, ("pmap_steal_memory: 0x%lx\n", va));

	return (va);
}
#else
void
pmap_virtual_space(vaddr_t *startp, vaddr_t *endp)
{
	*startp = VM_MIN_KERNEL_ADDRESS;
	*endp = VM_MAX_KERNEL_ADDRESS;
}
#endif /* PMAP_STEAL_MEMORY */

#ifdef PMAP_GROWKERNEL
vaddr_t
pmap_growkernel(vaddr_t kva)
{
	vaddr_t va;

	DPRINTF(PDB_FOLLOW|PDB_PHYS, ("pmap_growkernel(0x%lx)\n", kva));

	va = VM_MIN_KERNEL_ADDRESS + (pmap_nkpdes << PDE_SHIFT);
	DPRINTF(PDB_PHYS, ("pmap_growkernel: was va 0x%lx\n", va));
	if (va < kva) {
		simple_lock(&pmap_kernel()->pm_obj.vmobjlock);

		for ( ; va < kva ; pmap_nkpdes++, va += 1 << PDE_SHIFT)
			if (uvm.page_init_done) {
				if (!pmap_pde_alloc(pmap_kernel(), va, NULL))
					break;
			} else {
				paddr_t pa;

				pa = pmap_steal_memory(PAGE_SIZE, NULL, NULL);
				if (pa)
					panic("pmap_growkernel: out of memory");
				pmap_pde_set(pmap_kernel(), va, pa);
				pmap_kernel()->pm_stats.resident_count++;
			}

		simple_unlock(&pmap_kernel()->pm_obj.vmobjlock);
	}
	DPRINTF(PDB_PHYS|PDB_VP, ("pmap_growkernel: now va 0x%lx\n", va));
	return (va);
}
#endif /* PMAP_GROWKERNEL */

struct pmap *
pmap_create(void)
{
	struct pmap *pmap;
	struct vm_page *pg;
	static pa_space_t space = 0x200;
	paddr_t pa;

	DPRINTF(PDB_FOLLOW|PDB_PMAP, ("pmap_create()\n"));

	pmap = pool_get(&pmap_pmap_pool, PR_WAITOK);

	simple_lock_init(&pmap->pm_lock);
	pmap->pm_refcount = 1;
	pmap->pm_ptphint = NULL;

	TAILQ_INIT(&pmap->pm_pglist);
	if (uvm_pglistalloc(2 * PAGE_SIZE, 0, VM_MIN_KERNEL_ADDRESS - 1,
	    PAGE_SIZE, 2 * PAGE_SIZE, &pmap->pm_pglist, 1, UVM_PLA_WAITOK))
		panic("pmap_create: no pages");

	pg = TAILQ_FIRST(&pmap->pm_pglist);
	atomic_clearbits_int(&pg->pg_flags, PG_BUSY|PG_CLEAN);
	pmap->pm_pdir = (u_int32_t *)(pa = VM_PAGE_TO_PHYS(pg));
	bzero((void *)pa, PAGE_SIZE);

	/* set the first PIE that's covering low 2g of the address space */
	pg = TAILQ_LAST(&pmap->pm_pglist, pglist);
	atomic_clearbits_int(&pg->pg_flags, PG_BUSY|PG_CLEAN);
	*pmap->pm_pdir = (pa = VM_PAGE_TO_PHYS(pg)) >> PAGE_SHIFT;
	bzero((void *)pa, PAGE_SIZE);

/* TODO	for (space = 1 + (arc4random() & HPPA_SID_MAX);
	    pmap_sdir_get(space); space = (space + 1) % HPPA_SID_MAX); */
	pmap->pm_space = space;
	space += 0x200;

	pmap->pm_stats.resident_count = 2;
	pmap->pm_stats.wired_count = 0;

	return (pmap);
}

void
pmap_destroy(struct pmap *pmap)
{
	int refs;

	DPRINTF(PDB_FOLLOW|PDB_PMAP, ("pmap_destroy(%p)\n", pmap));

	simple_lock(&pmap->pm_lock);
	refs = --pmap->pm_refcount;
	simple_unlock(&pmap->pm_lock);

	if (refs > 0)
		return;

	uvm_pglistfree(&pmap->pm_pglist);
	TAILQ_INIT(&pmap->pm_pglist);
	pool_put(&pmap_pmap_pool, pmap);
}

/*
 * Add a reference to the specified pmap.
 */
void
pmap_reference(struct pmap *pmap)
{
	DPRINTF(PDB_FOLLOW|PDB_PMAP, ("pmap_reference(%p)\n", pmap));

	simple_lock(&pmap->pm_lock);
	pmap->pm_refcount++;
	simple_unlock(&pmap->pm_lock);
}

void
pmap_collect(struct pmap *pmap)
{
	DPRINTF(PDB_FOLLOW|PDB_PMAP, ("pmap_collect(%p)\n", pmap));
	/* nothing yet */
}

int
pmap_enter(struct pmap *pmap, vaddr_t va, paddr_t pa, vm_prot_t prot, int flags)
{
	volatile pt_entry_t *pde;
	pt_entry_t pte;
	struct vm_page *pg, *ptp = NULL;
	struct pv_entry *pve;
	boolean_t wired = (flags & PMAP_WIRED) != 0;

	DPRINTF(PDB_FOLLOW|PDB_ENTER,
	    ("pmap_enter(%p, 0x%lx, 0x%lx, 0x%x, 0x%x)\n",
	    pmap, va, pa, prot, flags));

	simple_lock(&pmap->pm_lock);

	if (!(pde = pmap_pde_get(pmap->pm_pdir, va)) &&
	    !(pde = pmap_pde_alloc(pmap, va, &ptp))) {
		if (flags & PMAP_CANFAIL) {
			simple_unlock(&pmap->pm_lock);
			return (ENOMEM);
		}

		panic("pmap_enter: cannot allocate pde");
	}

	if (!ptp)
		ptp = pmap_pde_ptp(pmap, pde);

	if ((pte = pmap_pte_get(pde, va))) {

		DPRINTF(PDB_ENTER,
		    ("pmap_enter: remapping 0x%lx -> 0x%lx\n", pte, pa));

		pmap_pte_flush(pmap, va, pte);
		if (wired && !(pte & PTE_WIRED))
			pmap->pm_stats.wired_count++;
		else if (!wired && (pte & PTE_WIRED))
			pmap->pm_stats.wired_count--;
		pte &= PTE_UNCACHABLE|PTE_DIRTY|PTE_REFTRAP;

		if (PTE_PAGE(pte) == pa) {
			DPRINTF(PDB_FOLLOW|PDB_ENTER,
			    ("pmap_enter: same page\n"));
			goto enter;
		}

		pg = PHYS_TO_VM_PAGE(PTE_PAGE(pte));
		simple_lock(&pg->mdpage.pvh_lock);
		pve = pmap_pv_remove(pg, pmap, va);
		pg->mdpage.pvh_attrs |= pmap_pvh_attrs(pte);
	} else {
		DPRINTF(PDB_ENTER,
		    ("pmap_enter: new mapping 0x%lx -> 0x%lx\n", va, pa));
		pte = PTE_REFTRAP;
		pve = NULL;
		pmap->pm_stats.resident_count++;
		if (wired)
			pmap->pm_stats.wired_count++;
		if (ptp)
			ptp->wire_count++;
		simple_lock(&pg->mdpage.pvh_lock);
	}

	if (pmap_initialized && (pg = PHYS_TO_VM_PAGE(pa))) {
		if (!pve && !(pve = pmap_pv_alloc())) {
			if (flags & PMAP_CANFAIL) {
				simple_unlock(&pg->mdpage.pvh_lock);
				simple_unlock(&pmap->pm_lock);
				return (ENOMEM);
			}
			panic("pmap_enter: no pv entries available");
		}
		pmap_pv_enter(pg, pve, pmap, va, ptp);
	} else if (pve)
		pmap_pv_free(pve);
	simple_unlock(&pg->mdpage.pvh_lock);

enter:
	/* preserve old ref & mod */
	pte = TLB_PAGE(pa) | pmap_prot(pmap, prot);
	if (wired)
		pte |= PTE_WIRED;
	pmap_pte_set(pde, va, pte);

	simple_unlock(&pmap->pm_lock);

	DPRINTF(PDB_FOLLOW|PDB_ENTER, ("pmap_enter: leaving\n"));

	return (0);
}

void
pmap_remove(struct pmap *pmap, vaddr_t sva, vaddr_t eva)
{
	struct pv_entry *pve;
	volatile pt_entry_t *pde;
	pt_entry_t pte;
	struct vm_page *pg;
	vaddr_t pdemask;
	int batch;

	DPRINTF(PDB_FOLLOW|PDB_REMOVE,
	    ("pmap_remove(%p, 0x%lx, 0x%lx)\n", pmap, sva, eva));

	simple_lock(&pmap->pm_lock);

	for (batch = 0, pdemask = 1; sva < eva; sva += PAGE_SIZE) {
		if (pdemask != (sva & PDE_MASK)) {
			pdemask = sva & PDE_MASK;
			if (!(pde = pmap_pde_get(pmap->pm_pdir, sva))) {
				sva += ~PDE_MASK + 1 - PAGE_SIZE;
				continue;
			}
			batch = pdemask == sva && sva + ~PDE_MASK + 1 <= eva;
		}

		if ((pte = pmap_pte_get(pde, sva))) {

			/* TODO measure here the speed tradeoff
			 * for flushing whole PT vs per-page
			 * in case of non-complete pde fill
			 */
			pmap_pte_flush(pmap, sva, pte);
			if (pte & PTE_WIRED)
				pmap->pm_stats.wired_count--;
			pmap->pm_stats.resident_count--;

			/* iff properly accounted pde will be dropped anyway */
			if (!batch)
				pmap_pte_set(pde, sva, 0);

			if (pmap_initialized &&
			    (pg = PHYS_TO_VM_PAGE(PTE_PAGE(pte)))) {

				simple_lock(&pg->mdpage.pvh_lock);
				pg->mdpage.pvh_attrs |= pmap_pvh_attrs(pte);
				if ((pve = pmap_pv_remove(pg, pmap, sva)))
					pmap_pv_free(pve);
				simple_unlock(&pg->mdpage.pvh_lock);
			}
		}
	}

	simple_unlock(&pmap->pm_lock);

	DPRINTF(PDB_FOLLOW|PDB_REMOVE, ("pmap_remove: leaving\n"));
}

void
pmap_write_protect(struct pmap *pmap, vaddr_t sva, vaddr_t eva, vm_prot_t prot)
{
	struct vm_page *pg;
	volatile pt_entry_t *pde;
	pt_entry_t pte;
	u_int tlbprot, pdemask;

	DPRINTF(PDB_FOLLOW|PDB_PMAP,
	    ("pmap_write_protect(%p, %lx, %lx, %x)\n", pmap, sva, eva, prot));

	sva = trunc_page(sva);
	tlbprot = pmap_prot(pmap, prot);

	simple_lock(&pmap->pm_lock);

	for (pdemask = 1; sva < eva; sva += PAGE_SIZE) {
		if (pdemask != (sva & PDE_MASK)) {
			pdemask = sva & PDE_MASK;
			if (!(pde = pmap_pde_get(pmap->pm_pdir, sva))) {
				sva += ~PDE_MASK + 1 - PAGE_SIZE;
				continue;
			}
		}
		if ((pte = pmap_pte_get(pde, sva))) {

			DPRINTF(PDB_PMAP,
			    ("pmap_write_protect: va=0x%lx pte=0x%lx\n",
			    sva,  pte));
			/*
			 * Determine if mapping is changing.
			 * If not, nothing to do.
			 */
			if ((pte & PTE_ACC_MASK) == tlbprot)
				continue;

			pg = PHYS_TO_VM_PAGE(PTE_PAGE(pte));
			simple_lock(&pg->mdpage.pvh_lock);
			pg->mdpage.pvh_attrs |= pmap_pvh_attrs(pte);
			simple_unlock(&pg->mdpage.pvh_lock);

			pmap_pte_flush(pmap, sva, pte);
			pte &= ~PTE_ACC_MASK;
			pte |= tlbprot;
			pmap_pte_set(pde, sva, pte);
		}
	}

	simple_unlock(&pmap->pm_lock);
}

void
pmap_page_remove(struct vm_page *pg)
{
	struct pv_entry *pve, *ppve;

	DPRINTF(PDB_FOLLOW|PDB_PV, ("pmap_page_remove(%p)\n", pg));

	if (pg->mdpage.pvh_list == NULL)
		return;

	simple_lock(&pg->mdpage.pvh_lock);
	for (pve = pg->mdpage.pvh_list; pve;
	     pve = (ppve = pve)->pv_next, pmap_pv_free(ppve)) {
		struct pmap *pmap = pve->pv_pmap;
		vaddr_t va = pve->pv_va;
		volatile pt_entry_t *pde;
		pt_entry_t pte;

		simple_lock(&pmap->pm_lock);

		pde = pmap_pde_get(pmap->pm_pdir, va);
		pte = pmap_pte_get(pde, va);
		pg->mdpage.pvh_attrs |= pmap_pvh_attrs(pte);

		pmap_pte_flush(pmap, va, pte);
		if (pte & PTE_WIRED)
			pmap->pm_stats.wired_count--;
		pmap->pm_stats.resident_count--;

		pmap_pte_set(pde, va, 0);
		simple_unlock(&pmap->pm_lock);
	}
	pg->mdpage.pvh_list = NULL;
	simple_unlock(&pg->mdpage.pvh_lock);

	DPRINTF(PDB_FOLLOW|PDB_PV, ("pmap_page_remove: leaving\n"));

}

void
pmap_unwire(struct pmap *pmap, vaddr_t	va)
{
	volatile pt_entry_t *pde;
	pt_entry_t pte = 0;

	DPRINTF(PDB_FOLLOW|PDB_PMAP, ("pmap_unwire(%p, 0x%lx)\n", pmap, va));

	simple_lock(&pmap->pm_lock);
	if ((pde = pmap_pde_get(pmap->pm_pdir, va))) {
		pte = pmap_pte_get(pde, va);

		if (pte & PTE_WIRED) {
			pte &= ~PTE_WIRED;
			pmap->pm_stats.wired_count--;
			pmap_pte_set(pde, va, pte);
		}
	}
	simple_unlock(&pmap->pm_lock);

	DPRINTF(PDB_FOLLOW|PDB_PMAP, ("pmap_unwire: leaving\n"));

#ifdef DIAGNOSTIC
	if (!pte)
		panic("pmap_unwire: invalid va 0x%lx", va);
#endif
}

boolean_t
pmap_changebit(struct vm_page *pg, pt_entry_t set, pt_entry_t clear)
{
	struct pv_entry *pve;
	pt_entry_t res;

	DPRINTF(PDB_FOLLOW|PDB_BITS,
	    ("pmap_changebit(%p, %lx, %lx)\n", pg, set, clear));

	simple_lock(&pg->mdpage.pvh_lock);
	res = pg->mdpage.pvh_attrs = 0;
	for(pve = pg->mdpage.pvh_list; pve; pve = pve->pv_next) {
		struct pmap *pmap = pve->pv_pmap;
		vaddr_t va = pve->pv_va;
		volatile pt_entry_t *pde;
		pt_entry_t opte, pte;

		simple_lock(&pmap->pm_lock);
		if ((pde = pmap_pde_get(pmap->pm_pdir, va))) {
			opte = pte = pmap_pte_get(pde, va);
#ifdef PMAPDEBUG
			if (!pte) {
				printf("pmap_changebit: zero pte for 0x%lx\n",
				    va);
				continue;
			}
#endif
			pte &= ~clear;
			pte |= set;
			pg->mdpage.pvh_attrs |= pmap_pvh_attrs(pte);
			res |= pmap_pvh_attrs(opte);

			if (opte != pte) {
				pmap_pte_flush(pmap, va, opte);
				pmap_pte_set(pde, va, pte);
			}
		}
		simple_unlock(&pmap->pm_lock);
	}
	simple_unlock(&pg->mdpage.pvh_lock);

	return ((res & (clear | set)) != 0);
}

boolean_t
pmap_testbit(struct vm_page *pg, pt_entry_t bit)
{
	struct pv_entry *pve;
	pt_entry_t pte;

	DPRINTF(PDB_FOLLOW|PDB_BITS, ("pmap_testbit(%p, %lx)\n", pg, bit));

	simple_lock(&pg->mdpage.pvh_lock);
	for(pve = pg->mdpage.pvh_list; !(pg->mdpage.pvh_attrs & bit) && pve;
	    pve = pve->pv_next) {
		simple_lock(&pve->pv_pmap->pm_lock);
		pte = pmap_vp_find(pve->pv_pmap, pve->pv_va);
		simple_unlock(&pve->pv_pmap->pm_lock);
		pg->mdpage.pvh_attrs |= pmap_pvh_attrs(pte);
	}
	simple_unlock(&pg->mdpage.pvh_lock);

	return ((pg->mdpage.pvh_attrs & bit) != 0);
}

boolean_t
pmap_extract(struct pmap *pmap, vaddr_t va, paddr_t *pap)
{
	pt_entry_t pte;
	vaddr_t mask;

	DPRINTF(PDB_FOLLOW|PDB_EXTRACT, ("pmap_extract(%p, %lx)\n", pmap, va));

	simple_lock(&pmap->pm_lock);
	pte = pmap_vp_find(pmap, va);
	simple_unlock(&pmap->pm_lock);

	if (pte) {
		if (pap) {
			mask = PTE_PAGE_SIZE(pte) - 1;
			*pap = PTE_PAGE(pte) | (va & mask);
		}
		return (TRUE);
	}

	return (FALSE);
}

void
pmap_activate(struct proc *p)
{
	struct pmap *pmap = p->p_vmspace->vm_map.pmap;
	struct pcb *pcb = &p->p_addr->u_pcb;

	pcb->pcb_space = pmap->pm_space;
}

void
pmap_deactivate(struct proc *p)
{

}

static __inline void
pmap_flush_page(struct vm_page *pg, int purge)
{
	struct pv_entry *pve;

	/* purge cache for all possible mappings for the pa */
	simple_lock(&pg->mdpage.pvh_lock);
	for (pve = pg->mdpage.pvh_list; pve; pve = pve->pv_next) {
		if (purge)
			pdcache(pve->pv_pmap->pm_space, pve->pv_va, PAGE_SIZE);
		else
			fdcache(pve->pv_pmap->pm_space, pve->pv_va, PAGE_SIZE);
		ficache(pve->pv_pmap->pm_space, pve->pv_va, PAGE_SIZE);
		pdtlb(pve->pv_pmap->pm_space, pve->pv_va);
		pitlb(pve->pv_pmap->pm_space, pve->pv_va);
}
	simple_unlock(&pg->mdpage.pvh_lock);
}

void
pmap_zero_page(struct vm_page *pg)
{
	paddr_t pa = VM_PAGE_TO_PHYS(pg);

	DPRINTF(PDB_FOLLOW|PDB_PHYS, ("pmap_zero_page(%lx)\n", pa));

	pmap_flush_page(pg, 1);
	bzero((void *)pa, PAGE_SIZE);
	fdcache(HPPA_SID_KERNEL, pa, PAGE_SIZE);
	pdtlb(HPPA_SID_KERNEL, pa);
}

void
pmap_copy_page(struct vm_page *srcpg, struct vm_page *dstpg)
{
	paddr_t spa = VM_PAGE_TO_PHYS(srcpg);
	paddr_t dpa = VM_PAGE_TO_PHYS(dstpg);
	DPRINTF(PDB_FOLLOW|PDB_PHYS, ("pmap_copy_page(%lx, %lx)\n", spa, dpa));

	pmap_flush_page(srcpg, 0);
	pmap_flush_page(dstpg, 1);
	bcopy((void *)spa, (void *)dpa, PAGE_SIZE);
	pdcache(HPPA_SID_KERNEL, spa, PAGE_SIZE);
	fdcache(HPPA_SID_KERNEL, dpa, PAGE_SIZE);
	pdtlb(HPPA_SID_KERNEL, spa);
	pdtlb(HPPA_SID_KERNEL, dpa);
}

void
pmap_kenter_pa(vaddr_t va, paddr_t pa, vm_prot_t prot)
{
	volatile pt_entry_t *pde;
	pt_entry_t pte, opte;

	DPRINTF(PDB_FOLLOW|PDB_ENTER,
	    ("pmap_kenter_pa(%lx, %lx, %x)\n", va, pa, prot));

	simple_lock(&pmap->pm_lock);

	if (!(pde = pmap_pde_get(pmap_kernel()->pm_pdir, va)) &&
	    !(pde = pmap_pde_alloc(pmap_kernel(), va, NULL)))
		panic("pmap_kenter_pa: cannot allocate pde for va=0x%lx", va);
	opte = pmap_pte_get(pde, va);
	pte = TLB_PAGE(pa) | PTE_WIRED | PTE_REFTRAP |
	    pmap_prot(pmap_kernel(), prot);
	if (pa >= 0xf0000000ULL /* TODO (HPPA_IOBEGIN & HPPA_PHYSMAP) */)
		pte |= PTE_UNCACHABLE | PTE_ORDER;
	DPRINTF(PDB_ENTER, ("pmap_kenter_pa: pde %p va %lx pte %lx\n",
	    pde, va, pte));
	pmap_pte_set(pde, va, pte);
	pmap_kernel()->pm_stats.wired_count++;
	pmap_kernel()->pm_stats.resident_count++;
	if (opte)
		pmap_pte_flush(pmap_kernel(), va, opte);

#ifdef PMAPDEBUG
	{
		struct vm_page *pg;

		if (pmap_initialized && (pg = PHYS_TO_VM_PAGE(PTE_PAGE(pte)))) {

			simple_lock(&pg->mdpage.pvh_lock);
			if (pmap_check_alias(pg->mdpage.pvh_list, va, pte))
				Debugger();
			simple_unlock(&pg->mdpage.pvh_lock);
		}
	}
#endif
	simple_unlock(&pmap->pm_lock);

	DPRINTF(PDB_FOLLOW|PDB_ENTER, ("pmap_kenter_pa: leaving\n"));
}

void
pmap_kremove(vaddr_t va, vsize_t size)
{
	struct pv_entry *pve;
	vaddr_t eva, pdemask;
	volatile pt_entry_t *pde;
	pt_entry_t pte;
	struct vm_page *pg;

	DPRINTF(PDB_FOLLOW|PDB_REMOVE,
	    ("pmap_kremove(%lx, %lx)\n", va, size));
#ifdef PMAPDEBUG
	if (va < ptoa(physmem)) {
		printf("pmap_kremove(%lx, %lx): unmapping physmem\n", va, size);
		return;
	}
#endif

	simple_lock(&pmap->pm_lock);

	for (pdemask = 1, eva = va + size; va < eva; va += PAGE_SIZE) {
		if (pdemask != (va & PDE_MASK)) {
			pdemask = va & PDE_MASK;
			if (!(pde = pmap_pde_get(pmap_kernel()->pm_pdir, va))) {
				va += ~PDE_MASK + 1 - PAGE_SIZE;
				continue;
			}
		}
		if (!(pte = pmap_pte_get(pde, va))) {
#ifdef DEBUG
			printf("pmap_kremove: unmapping unmapped 0x%lx\n", va);
#endif
			continue;
		}

		pmap_pte_flush(pmap_kernel(), va, pte);
		pmap_pte_set(pde, va, 0);
		if (pmap_initialized && (pg = PHYS_TO_VM_PAGE(PTE_PAGE(pte)))) {

			simple_lock(&pg->mdpage.pvh_lock);
			pg->mdpage.pvh_attrs |= pmap_pvh_attrs(pte);
			/* just in case we have enter/kenter mismatch */
			if ((pve = pmap_pv_remove(pg, pmap_kernel(), va)))
				pmap_pv_free(pve);
			simple_unlock(&pg->mdpage.pvh_lock);
		}
	}

	simple_unlock(&pmap->pm_lock);

	DPRINTF(PDB_FOLLOW|PDB_REMOVE, ("pmap_kremove: leaving\n"));
}

void
pmap_proc_iflush(struct proc *p, vaddr_t va, vsize_t len)
{
	struct pmap *pmap = p->p_vmspace->vm_map.pmap;

	fdcache(pmap->pm_space, va, len);
	sync_caches();
	ficache(pmap->pm_space, va, len);
	sync_caches();
}

struct vm_page *
pmap_unmap_direct(vaddr_t va)
{
	fdcache(HPPA_SID_KERNEL, va, PAGE_SIZE);
	pdtlb(HPPA_SID_KERNEL, va);
	return (PHYS_TO_VM_PAGE(va));
}
