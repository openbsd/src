/*	$OpenBSD: pmap.c,v 1.10 2015/05/02 14:33:19 jsg Exp $	*/
/*
 * Copyright (c) 2005, Miodrag Vallat
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
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * KAP physical memory management code.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/pool.h>
#include <sys/proc.h>

#include <uvm/uvm.h>

#include <machine/idt.h>
#include <machine/kap.h>
#include <machine/prom.h>

#include <sparc/sparc/asm.h>
#include <sparc/sparc/cache.h>
#include <sparc/sparc/cpuvar.h>

#include <sparc/dev/if_lereg.h>

#ifdef PMAPDEBUG
#define	PDB_ACTIVATE	0x000001
#define	PDB_CLEAR_M	0x000002
#define	PDB_CLEAR_U	0x000004
#define	PDB_COLLECT	0x000008
#define	PDB_COPY	0x000010
#define	PDB_CREATE	0x000020
#define	PDB_DESTROY	0x000040
#define	PDB_ENTER	0x000080
#define	PDB_EXTRACT	0x000100
#define	PDB_IS_M	0x000200
#define	PDB_IS_U	0x000400
#define	PDB_KENTER	0x000800
#define	PDB_KREMOVE	0x001000
#define	PDB_PROTECT	0x002000
#define	PDB_REFERENCE	0x004000
#define	PDB_RELEASE	0x008000
#define	PDB_REMOVE	0x010000
#define	PDB_UNWIRE	0x020000
#define	PDB_ZERO	0x040000

#define	DPRINTF(flg,stmt) \
do { \
	if (pmapdebug & (flg))	\
		printf stmt;	\
} while (0)

u_int	_pmapdebug_cold = 0;
u_int	_pmapdebug = -1;
#define	pmapdebug	((cold) ? _pmapdebug_cold : _pmapdebug)
#else
#define	DPRINTF(flg,stmt) do { } while (0)
#endif

/* pmap and pde/pte pool allocators */
struct pool pmappool, pvpool;

struct pmap kernel_pmap_store;

pt_entry_t *pmap_grow_pte(struct pmap *, vaddr_t);
static pd_entry_t *pmap_pde(pmap_t, vaddr_t);
static pt_entry_t *pde_pte(pd_entry_t *, vaddr_t);
pt_entry_t *pmap_pte(pmap_t, vaddr_t);

void	pg_flushcache(struct vm_page *);

void	tlb_flush(vaddr_t);
void	tlb_flush_all(void);

vaddr_t	virtual_avail;
vaddr_t	virtual_end;

vaddr_t	vreserve;	/* two reserved pages for copy and zero operations... */
pt_entry_t *ptereserve;	/* ...and their PTEs */

vaddr_t	lance_va;	/* a fixed buffer for the on-board lance */

/*
 * Attribute caching
 */

typedef struct pvlist *pv_entry_t;

static pv_entry_t pg_to_pvl(struct vm_page *);

static __inline__
pv_entry_t
pg_to_pvl(struct vm_page *pg)
{
	return (&pg->mdpage.pv_head);
}

/*
 * TLB operations
 */

void
tlb_flush(vaddr_t va)
{
#if 0
	u_int32_t fvar;

	fvar = lda(0, ASI_FVAR);
#endif

	sta(0, ASI_PID, 0);
	sta(0, ASI_FVAR, va);
	sta(0, ASI_GTLB_INVAL_ENTRY, 0);
#if 0
	sta(0, ASI_FVAR, fvar);
#endif
}

void
tlb_flush_all()
{
	/*
	 * Note that loaded TLB for PTEs with PG_G do NOT get invalidated
	 * by this command (because they are common to all PID), and need
	 * to be invalidated with ASI_GTLB_INVAL_ENTRY.
	 * This does not matter to us, as we don't use PG_G for now.
	 */
	sta(0, ASI_PID, 0);
	sta(0, ASI_GTLB_INVALIDATE, 0);
}

/*
 * Simple pde and pte access routines.
 */

#define	trunc_seg(va)		((va) & PDT_INDEX_MASK)

static __inline__
pd_entry_t *
pmap_pde(pmap_t pmap, vaddr_t va)
{
	return (&pmap->pm_segtab[va >> PDT_INDEX_SHIFT]);
}

static __inline__
pt_entry_t *
pde_pte(pd_entry_t *pde, vaddr_t va)
{
	pt_entry_t *pte;

	pte = (pt_entry_t *)pde->pde_va;
	pte += (va & PT_INDEX_MASK) >> PT_INDEX_SHIFT;

	return (pte);
}

pt_entry_t *
pmap_pte(pmap_t pmap, vaddr_t va)
{
	pd_entry_t *pde;

	pde = pmap_pde(pmap, va);
	if (pde->pde_va == NULL)
		return (NULL);

	return (pde_pte(pde, va));
}

/*
 * Setup virtual memory for the kernel. The new tables are not activated yet,
 * they will be in locore.s after bootstrap() returns.
 */
void
pmap_bootstrap(size_t promdata)
{
	extern caddr_t end;
	extern vaddr_t esym;
	u_int32_t icuconf;
	u_int8_t imcmcr;
	vaddr_t ekern;
	vaddr_t va, eva;
	paddr_t pa;
	unsigned int tabidx;
	pd_entry_t *pde;
	pt_entry_t *pte;
	struct sb_prom *sp;
	paddr_t prompa;
	psize_t promlen;
	extern vaddr_t prom_data;

	/*
	 * Compute memory size by checking the iCU for the number of iMC,
	 * then each iMC for its status.
	 */

	icuconf = lda(ICU_CONF, ASI_PHYS_IO);
	physmem = 0;

#if 0
	imcmcr = lduba(MC0_MCR, ASI_PHYS_IO);
#else
	imcmcr = *(u_int8_t *)MC0_MCR;
#endif
	if (imcmcr & MCR_BANK0_AVAIL)
		physmem += (imcmcr & MCR_BANK0_32M) ? 32 : 8;
	if (imcmcr & MCR_BANK1_AVAIL)
		physmem += (imcmcr & MCR_BANK1_32M) ? 32 : 8;

	if ((icuconf & CONF_NO_EXTRA_MEMORY) == 0) {
#if 0
		imcmcr = lduba(MC1_MCR, ASI_PHYS_IO);
#else
		imcmcr = *(u_int8_t *)MC1_MCR;
#endif
		if (imcmcr & MCR_BANK0_AVAIL)
			physmem += (imcmcr & MCR_BANK0_32M) ? 32 : 8;
		if (imcmcr & MCR_BANK1_AVAIL)
			physmem += (imcmcr & MCR_BANK1_32M) ? 32 : 8;
	}

	/* scale to pages */
	physmem <<= (20 - PAGE_SHIFT);

	/*
	 * Get a grip on the PROM communication area.
	 */
	sp = (struct sb_prom *)PROM_DATA_VA;

	/*
	 * Set virtual page size.
	 */
	uvmexp.pagesize = PAGE_SIZE;
	uvm_setpagesize();
	
	/*
	 * Initialize kernel pmap.
	 */
	pmap_kernel()->pm_refcount = 1;

	/*
	 * Compute kernel fixed memory usage.
	 */
	ekern = (vaddr_t)&end;
#if defined(DDB) || NKSYMS > 0
	if (esym != 0)
		ekern = esym;
#endif

	/*
	 * Reserve room for the prom data we're interested in.
	 */
	prom_data = ekern;
	ekern += promdata;
	
	/*
	 * From then on, all allocations will be multiples of the
	 * page size.
	 */
	ekern = round_page(ekern);

	/*
	 * Reserve buffers for the on-board Lance chip - the whole buffer
	 * must be in the same 128KB segment.
	 * This should disappear once iCU is tamed...
	 */
	if ((ekern >> 17) != ((ekern + MEMSIZE) >> 17))
		ekern = roundup(ekern, 1 << 17);
	lance_va = ekern;
	ekern += MEMSIZE;

	/*
	 * Initialize fixed mappings.
	 * We want to keep the PTW mapping the kernel for now, but all
	 * devices needed during early bootstrap needs to have their own
	 * mappings.
	 */

	/*
	 * Step 1: reserve memory for the kernel pde.
	 */

	bzero((caddr_t)ekern, PDT_SIZE);
	pmap_kernel()->pm_segtab = (pd_entry_t *)ekern;
	pmap_kernel()->pm_psegtab = PTW1_TO_PHYS(ekern);
	ekern += PDT_SIZE;	/* not rounded anymore ! */

	/*
	 * Step 2: create as many pages tables as necessary.
	 * We'll provide page tables for the kernel virtual memory range
	 * and the top of memory (i.e. PTW1, PTW2 and I/O maps), so that
	 * we can invoke mapdev() early in the boot process.
	 *
	 * For the early console, we will also provide an 1:1 mapping
	 * of the I/O space.
	 */

	tabidx = 0;

	va = VM_MIN_KERNEL_ADDRESS;
	while (va != 0) {
		pde = pmap_pde(pmap_kernel(), va);

		pde->pde_va = (pt_entry_t *)(ekern + tabidx * PT_SIZE);
		pde->pde_pa = PTW1_TO_PHYS((vaddr_t)pde->pde_va);

		tabidx++;
		va += NBSEG;
	}

	va = (vaddr_t)OBIO_PA_START;
	while (va < (vaddr_t)OBIO_PA_END) {
		pde = pmap_pde(pmap_kernel(), va);

		pde->pde_va = (pt_entry_t *)(ekern + tabidx * PT_SIZE);
		pde->pde_pa = PTW1_TO_PHYS((vaddr_t)pde->pde_va);

		tabidx++;
		va += NBSEG;
	}

	va = IOSPACE_BASE;
	while (va < IOSPACE_BASE + IOSPACE_LEN) {
		pde = pmap_pde(pmap_kernel(), va);

		pde->pde_va = (pt_entry_t *)(ekern + tabidx * PT_SIZE);
		pde->pde_pa = PTW1_TO_PHYS((vaddr_t)pde->pde_va);

		tabidx++;
		/* watch out for wraparound! */
		if ((va += NBSEG) == 0)
			break;
	}

	bzero((caddr_t)ekern, tabidx * PT_SIZE);
	ekern += tabidx * PT_SIZE;
	ekern = round_page(ekern);

	/*
	 * Step 3: fill them. We fill the page tables backing PTW1 and
	 * PTW2 to simplify pmap_extract(), by not having to check if
	 * the va is in a PTW.
	 */

	va = PTW1_BASE;
	pa = PHYSMEM_BASE;
	while (va < PTW1_BASE + PTW_WINDOW_SIZE) {
		pde = pmap_pde(pmap_kernel(), va);
		eva = trunc_seg(va) + NBSEG;
		if (eva > PTW1_BASE + PTW_WINDOW_SIZE)
			eva = PTW1_BASE + PTW_WINDOW_SIZE;
		pte = pde_pte(pde, va);
		while (va < eva) {
			*pte++ = pa | PG_V | PG_S | PG_U | PG_CACHE;
			va += PAGE_SIZE;
			pa += PAGE_SIZE;
		}
	}

	va = PTW2_BASE;
	pa = PHYSMEM_BASE;
	while (va < PTW2_BASE + PTW_WINDOW_SIZE) {
		pde = pmap_pde(pmap_kernel(), va);
		eva = trunc_seg(va) + NBSEG;
		if (eva > PTW2_BASE + PTW_WINDOW_SIZE)
			eva = PTW2_BASE + PTW_WINDOW_SIZE;
		pte = pde_pte(pde, va);
		while (va < eva) {
			*pte++ = pa | PG_V | PG_S | PG_U | PG_SHARED;
			va += PAGE_SIZE;
			pa += PAGE_SIZE;
		}
	}

	va = (vaddr_t)OBIO_PA_START;
	while (va < (vaddr_t)OBIO_PA_END) {
		pde = pmap_pde(pmap_kernel(), va);
		eva = trunc_seg(va) + NBSEG;
		if (eva > OBIO_PA_END)
			eva = OBIO_PA_END;
		pte = pde_pte(pde, va);
		for (; va < eva; va += PAGE_SIZE)
			*pte++ = va | PG_V | PG_S | PG_U | PG_IO;
	}

	/*
	 * Compute the virtual memory space.
	 * Note that the kernel is mapped by PTW1 and PTW2, and is outside
	 * this range.
	 */

	virtual_avail = VM_MIN_KERNEL_ADDRESS;
	virtual_end = VM_MAX_KERNEL_ADDRESS;

	/*
	 * Reserve two _virtual_ pages for copy and zero operations.
	 * Since we need to be able to tweak their PTE, they need to be
	 * outside PTW1 and PTW2. We'll steal them from the top of the
	 * virtual space; thus we are sure they will be in the same
	 * segment as well.
	 */

	virtual_end -= 2* PAGE_SIZE;
	vreserve = virtual_end;
	ptereserve = pmap_pte(pmap_kernel(), vreserve);

	/*
	 * Tell the VM system about the available memory.
	 * Physical memory starts at PHYSMEM_BASE; kernel uses space
	 * from PTW1_TO_PHYS(KERNBASE) to ekern at this point.
	 *
	 * The physical memory below the kernel is reserved for the PROM
	 * data and bss, and need to be left intact when invoking it, so
	 * we do not upload (manage) it.
	 *
	 * The PROM communication area may claim another area, way above
	 * the kernel (usually less than 200 KB, immediately under 8MB
	 * physical).
	 */

	if (sp->sp_interface >= PROM_INTERFACE) {
		prompa = atop(PHYSMEM_BASE) + sp->sp_reserve_start;
		promlen = sp->sp_reserve_len;
	} else
		promlen = 0;

	if (promlen != 0) {
#ifdef DIAGNOSTIC
		if (PTW1_TO_PHYS(ekern) > ptoa(prompa))
			panic("kernel overlaps PROM reserved area");
#endif
		uvm_page_physload(
		    atop(PTW1_TO_PHYS(ekern)), prompa,
		    atop(PTW1_TO_PHYS(ekern)), prompa, 0);
		uvm_page_physload(
		    prompa + promlen, atop(PHYSMEM_BASE) + physmem,
		    prompa + promlen, atop(PHYSMEM_BASE) + physmem, 0);
	} else {
		uvm_page_physload(
		    atop(PTW1_TO_PHYS(ekern)), atop(PHYSMEM_BASE) + physmem,
		    atop(PTW1_TO_PHYS(ekern)), atop(PHYSMEM_BASE) + physmem, 0);
	}
}

/*
 * Return the virtual area range available to the kernel.
 */
void
pmap_virtual_space(vaddr_t *v_start, vaddr_t *v_end)
{
	*v_start = virtual_avail;
	*v_end = virtual_end;
}

/*
 * Secondary initialization, at uvm_init() time.
 * We can now create the pools we'll use for pmap and pvlist allocations.
 */
void
pmap_init()
{
	pool_init(&pmappool, sizeof(struct pmap), 0, 0, 0, "pmappl",
	    &pool_allocator_nointr);
	pool_init(&pvpool, sizeof(struct pvlist), 0, 0, 0, "pvpl", NULL);
}

/*
 * Create a new pmap.
 *
 * We initialize pmaps with an empty pde, and a shadow of the kernel
 * space (VM_MIN_KERNEL_ADDRESS onwards).
 */
pmap_t
pmap_create()
{
	pmap_t pmap;
	u_int pde;

	DPRINTF(PDB_CREATE, ("pmap_create()"));

	pmap = pool_get(&pmappool, PR_WAITOK | PR_ZERO);

	pmap->pm_refcount = 1;

	/*
	 * Allocate the page directory.
	 */
	pmap->pm_segtab = (pd_entry_t *)uvm_km_zalloc(kernel_map, PDT_SIZE);
	if (pmap_extract(pmap_kernel(), (vaddr_t)pmap->pm_segtab,
	    &pmap->pm_psegtab) == FALSE)
		panic("pmap_create: pmap_extract failed!");

	/*
	 * Shadow the kernel map in all user pmaps.
	 */
	for (pde = (VM_MIN_KERNEL_ADDRESS >> PDT_INDEX_SHIFT);
	    pde < NBR_PDE; pde++) {
		pmap->pm_segtab[pde].pde_pa =
		    pmap_kernel()->pm_segtab[pde].pde_pa;
		pmap->pm_segtab[pde].pde_va =
		    pmap_kernel()->pm_segtab[pde].pde_va;
	}

	DPRINTF(PDB_CREATE, (" -> %p\n", pmap));

	return (pmap);
}

/*
 * Destroy a pmap.
 * Its mappings will not actually be removed until the reference count
 * drops to zero.
 */
void
pmap_destroy(struct pmap *pmap)
{
	int count;

	DPRINTF(PDB_DESTROY, ("pmap_destroy(%p)\n", pmap));

	count = --pmap->pm_refcount;
	if (count == 0) {
		pmap_release(pmap);
		pool_put(&pmappool, pmap);
	}
}

/*
 * Release all mappings and resources associated to a given pmap.
 */
void
pmap_release(struct pmap *pmap)
{
	u_int pde;
	pt_entry_t *pdeva;
#ifdef DIAGNOSTIC
	u_int pte;
#endif

	DPRINTF(PDB_RELEASE, ("pmap_release(%p)\n", pmap));

	/*
	 * Free all page tables.
	 */
	for (pde = 0; pde < (VM_MIN_KERNEL_ADDRESS >> PDT_INDEX_SHIFT); pde++) {
		if ((pdeva = pmap->pm_segtab[pde].pde_va) != NULL) {
#ifdef DIAGNOSTIC
			for (pte = 0; pte < NBR_PTE; pte++)
				if (pdeva[pte] & PG_V) {
					DPRINTF(PDB_RELEASE,
					    ("pmap_release: unreleased pte "
					    "%p (%08x)\n",
					    pdeva + pte, pdeva[pte]));
				}
#endif
			uvm_km_free(kernel_map, (vaddr_t)pdeva, PT_SIZE);
		}
	}

	/*
	 * Free the page directory.
	 */
	uvm_km_free(kernel_map, (vaddr_t)pmap->pm_segtab, PDT_SIZE);
}

/*
 * Returns a preferred virtual address for the given address, which
 * does not cause a VAC aliasing situation.
 */
vaddr_t
pmap_prefer(vaddr_t foff, vaddr_t va)
{
	/* XXX assume no cache aliasing yet */
	return va;
}

/*
 * Activate the pmap associated to a given process.
 * Called from the scheduler.
 */
void
pmap_activate(struct proc *p)
{
	pmap_t pmap = p->p_vmspace->vm_map.pmap;
	int s;

	DPRINTF(PDB_ACTIVATE,
	    ("pmap_activate(%p/pmap %p/segtab pa %08x va %08x)\n",
	    p, pmap, pmap->pm_psegtab, (vaddr_t)pmap->pm_segtab));

	s = splvm();

	if (p == curproc) {
		write_user_windows();
		cache_flush_context();
		sta(0, ASI_PID, 0);
		sta(0, ASI_PDBR, pmap->pm_psegtab);
		tlb_flush_all();
	}

	splx(s);
}

/*
 * Increment the pmap reference counter.
 */
void
pmap_reference(struct pmap *pmap)
{
	DPRINTF(PDB_REFERENCE, ("pmap_reference(%p)\n", pmap));

	pmap->pm_refcount++;
}

/*
 * Remove a range of virtual addresses from the given pmap.
 * Addresses are expected to be page-aligned.
 */
void
pmap_remove(struct pmap *pmap, vaddr_t sva, vaddr_t e)
{
	vaddr_t va, eva;
	paddr_t pa;
	pd_entry_t *pde;
	pt_entry_t *pte, opte;
	struct vm_page *pg;
	struct pvlist *pvl, *prev, *cur;
	int s;

	s = splvm();

	DPRINTF(PDB_REMOVE, ("pmap_remove(%p,%08x,%08x)\n", pmap, sva, e));

	va = sva;
	while (va != e) {
		pde = pmap_pde(pmap, va);
		eva = trunc_seg(va) + NBSEG;
		if (eva > e || eva == 0)
			eva = e;

		if (pde == NULL) {
			va = eva;
			continue;
		}

		pte = pde_pte(pde, va);
		for (; va != eva; va += PAGE_SIZE, pte++) {
			opte = *pte;
			if ((opte & PG_V) == 0)
				continue;

			pmap->pm_stats.resident_count--;

			pa = opte & PG_FRAME;

#ifdef DIAGNOSTIC
			if (opte & PG_W) {
				printf("pmap_remove(%p): wired mapping for %08x",
				    pmap, va);
				pmap->pm_stats.wired_count--;
			}
#endif

			*pte = PG_NV;
			tlb_flush(va);

			pg = PHYS_TO_VM_PAGE(pa);
			if (pg == NULL)
				continue;

			/*
			 * Remove the mapping from the pvlist for this
			 * physical page.
			 */
			pvl = pg_to_pvl(pg);
#ifdef DIAGNOSTIC
			if (pvl->pv_pmap == NULL)
				panic("pmap_remove: NULL pmap in pvlist");
#endif
			prev = NULL;
			for (cur = pvl; cur != NULL; cur = cur->pv_next) {
				if (cur->pv_va == va && cur->pv_pmap == pmap)
					break;
				prev = cur;
			}
#ifdef DIAGNOSTIC
			if (cur == NULL) {
				panic("pmap_remove: va not in pvlist");
			}
#endif
			if (prev == NULL) {
				cur = cur->pv_next;
				if (cur != NULL) {
					cur->pv_flags = pvl->pv_flags;
					*pvl = *cur;
					pool_put(&pvpool, cur);
				} else {
					pvl->pv_pmap = NULL;
				}
			} else {
				prev->pv_next = cur->pv_next;
				pool_put(&pvpool, cur);
			}

			/* update saved attributes for managed page */
			pvl->pv_flags |= (opte & (PG_U | PG_M));
		}
	}

	splx(s);
}

/*
 * Release any unnecessary management resources for the given pmap,
 * before swapping it out.
 */
void
pmap_collect(struct pmap *pmap)
{
	u_int pde, pte;
	pt_entry_t *pdeva;
	int s;

	s = splvm();

	DPRINTF(PDB_COLLECT, ("pmap_collect(%p)\n", pmap));

	/*
	 * Free all empty page tables.
	 */
	for (pde = 0; pde < (VM_MIN_KERNEL_ADDRESS >> PDT_INDEX_SHIFT); pde++) {
		if ((pdeva = pmap->pm_segtab[pde].pde_va) == NULL)
			continue;
		for (pte = 0; pte < NBR_PTE; pte++)
			if (pdeva[pte] & PG_V)
				break;
		if (pte != NBR_PTE)
			continue;

		/*
		 * Free the unused page table.
		 */
		pmap->pm_segtab[pde].pde_va = NULL;
		pmap->pm_segtab[pde].pde_pa = 0;
		uvm_km_free(kernel_map, (vaddr_t)pdeva, PT_SIZE);
	}

	splx(s);
}

/*
 * Change the protection for a given vm_page. The protection can only
 * become more strict, i.e. protection rights get removed.
 *
 * Note that this pmap does not manage execution protection yet.
 */
void
pmap_page_protect(struct vm_page *pg, vm_prot_t prot)
{
	struct pvlist *pvl;
	int s;

	if ((prot & PROT_READ) == PROT_NONE) {	/* remove all */
		s = splvm();
		pvl = pg_to_pvl(pg);

		DPRINTF(PDB_REMOVE, ("pmap_page_protect(%p/pmap %p,%x)\n",
		    pg, pvl->pv_pmap, prot));

		while (pvl->pv_pmap != NULL) {
			pmap_remove(pvl->pv_pmap, pvl->pv_va,
			    pvl->pv_va + PAGE_SIZE);
		}

		splx(s);
	} else if ((prot & PROT_WRITE) == PROT_NONE) {
		s = splvm();
		pvl = pg_to_pvl(pg);

		DPRINTF(PDB_REMOVE, ("pmap_page_protect(%p/pmap %p,%x)\n",
		    pg, pvl->pv_pmap, prot));

		if (pvl->pv_pmap != NULL)
			for (; pvl != NULL; pvl = pvl->pv_next)
				pmap_protect(pvl->pv_pmap, pvl->pv_va,
				    pvl->pv_va + PAGE_SIZE, prot);

		splx(s);
	} else {
		DPRINTF(PDB_REMOVE, ("pmap_page_protect(%p,%x)\n", pg, prot));
	}
}

/*
 * Set the protection for a virtual address range in the given pmap.
 *
 * Note that this pmap does not manage execution protection yet.
 */
void
pmap_protect(struct pmap *pmap, vaddr_t sva, vaddr_t e, vm_prot_t prot)
{
	vaddr_t va, eva;
	pd_entry_t *pde;
	pt_entry_t *pte, opte, npte;
	int s;

	s = splvm();

	DPRINTF(PDB_PROTECT,
	    ("pmap_protect(%p,%08x,%08x,%x)\n", pmap, sva, e, prot));

	if ((prot & PROT_READ) == PROT_NONE) {
		pmap_remove(pmap, sva, e);
		splx(s);
		return;
	}

	va = sva;
	while (va != e) {
		pde = pmap_pde(pmap, va);
		eva = trunc_seg(va) + NBSEG;
		if (eva > e || eva == 0)
			eva = e;

		if (pde == NULL) {
			va = eva;
			continue;
		}

		pte = pde_pte(pde, va);
		for (; va != eva; va += PAGE_SIZE, pte++) {
			opte = *pte;
			if ((opte & PG_V) == 0)
				continue;

			npte = (opte & ~PG_RO) | 
			    (prot & PROT_WRITE) ? PG_RW : PG_RO;
			if (opte != npte) {
				*pte = npte;
				tlb_flush(va);
			}
		}
	}

	splx(s);
}

/*
 * Expand a pmap, if necessary, to include a pte.
 */
pt_entry_t *
pmap_grow_pte(struct pmap *pmap, vaddr_t va)
{
	pd_entry_t *pde;

	pde = pmap_pde(pmap, va);
	if (pde->pde_va == NULL) {
		pde->pde_va = (pt_entry_t *)uvm_km_zalloc(kernel_map, PT_SIZE);
		if (pde->pde_va == NULL)
			return (NULL);
		if (pmap_extract(pmap_kernel(), (vaddr_t)pde->pde_va,
		    (paddr_t *)&pde->pde_pa) == FALSE)
			panic("pmap_grow_pte: pmap_extract on PT failed!");
		tlb_flush((vaddr_t)pmap->pm_segtab);
	}

	return (pde_pte(pde, va));
}

/*
 * Create or update a mapping for the page at the given physical and
 * virtual addresses, for the given pmap.
 */
int
pmap_enter(struct pmap *pmap, vaddr_t va, paddr_t pa, vm_prot_t prot, int flags)
{
	pt_entry_t *pte, opte, npte;
	struct vm_page *pg;
	struct pvlist *pvl, *cur;
	int s;

	s = splvm();

	DPRINTF(PDB_ENTER,
	    ("pmap_enter(%p,%08x,%08x,%x,%x)", pmap, va, pa, prot, flags));

	if ((pte = pmap_grow_pte(pmap, va)) == NULL) {
		DPRINTF(PDB_ENTER, (" -> pmap_grow_pte failed\n"));
		if (flags & PMAP_CANFAIL) {
			splx(s);
			return (ENOMEM);
		} else
			panic("pmap_enter: unable to allocate PT");
	}

	opte = *pte;
	DPRINTF(PDB_ENTER, (" opte %08x", opte));

	/*
	 * Enable cache, by default, if on physical memory, unless
	 * PMAP_NC has been passed in pa.
	 */
	switch (pa & PAGE_MASK) {
	case PMAP_NC:
		npte = PG_IO;
		break;
	case PMAP_BWS:
		npte = PG_BYTE_SHARED;
		break;
	default:
		if (pa >= PHYSMEM_BASE && pa < PHYSMEM_BASE + ptoa(physmem))
			npte = PG_CACHE;
		else
			npte = PG_IO;
		break;
	}

	pa = trunc_page(pa);
	npte |= pa | PG_V | (prot & PROT_WRITE ? PG_RW : PG_RO);

	pg = PHYS_TO_VM_PAGE(pa);
	if (pg != NULL) {
		/*
		 * For a managed page, enter the mapping in the pvlist.
		 */
		pvl = pg_to_pvl(pg);

		if (pvl->pv_pmap == NULL) {
			/*
			 * We are the first mapping.
			 */
			pvl->pv_pmap = pmap;
			pvl->pv_va = va;
			pvl->pv_next = NULL;
		} else {
			/*
			 * Add ourselves to the list.
			 * Note that, if we are only changing attributes
			 * and/or protection, we are already in the list!
			 */
			for (cur = pvl; cur != NULL; cur = cur->pv_next) {
				if (pmap == cur->pv_pmap && va == cur->pv_va)
					break;
			}

			if (cur == NULL) {
				cur = pool_get(&pvpool, PR_NOWAIT);
				if (cur == NULL) {
					if (flags & PMAP_CANFAIL)
						return (ENOMEM);
					else
						panic("pmap_enter: "
						    "pvlist pool exhausted");
				}
				/*
				 * Add the new entry after the header.
				 */
				cur->pv_pmap = pmap;
				cur->pv_va = va;
				cur->pv_flags = 0;
				cur->pv_next = pvl->pv_next;
				pvl->pv_next = cur;
			}
		}
	}

	if (flags & PMAP_WIRED) {
		npte |= PG_W;
		if ((opte & PG_W) == 0)
			pmap->pm_stats.wired_count++;
	} else {
		if ((opte & PG_W) != 0)
			pmap->pm_stats.wired_count--;
	}
	if ((opte & PG_V) == 0)
		pmap->pm_stats.resident_count++;
	if (pa >= VM_MIN_KERNEL_ADDRESS)
		npte |= PG_S;

	/*
	 * Now update the pte.
	 */
	if (opte != npte) {
		DPRINTF(PDB_ENTER, (" -> npte %08x", npte));
		*pte = npte;
		tlb_flush(va);
	}

	DPRINTF(PDB_ENTER, ("\n"));

	splx(s);
	
	return (0);
}

/*
 * Specific flavour of pmap_enter() for unmanaged wired mappings in the
 * kernel pmap.
 */
void
pmap_kenter_pa(vaddr_t va, paddr_t pa, vm_prot_t prot)
{
	pt_entry_t *pte, opte, npte;
	int s;

	s = splvm();

	DPRINTF(PDB_KENTER,
	    ("pmap_kenter_pa(%08x,%08x,%x)", va, pa, prot));

	if ((pte = pmap_grow_pte(pmap_kernel(), va)) == NULL) {
		DPRINTF(PDB_KENTER, (" -> pmap_grow_pte failed\n"));
		panic("pmap_kenter_pa: unable to allocate PT");
	}

	opte = *pte;
	DPRINTF(PDB_KENTER, (" opte %08x", opte));

	/*
	 * Enable cache, by default, if on physical memory, unless
	 * PMAP_NC has been passed in pa.
	 */
	switch (pa & PAGE_MASK) {
	case PMAP_NC:
		npte = PG_IO;
		break;
	case PMAP_BWS:
		npte = PG_BYTE_SHARED;
		break;
	default:
		if (pa >= PHYSMEM_BASE && pa < PHYSMEM_BASE + ptoa(physmem))
			npte = PG_CACHE;
		else
			npte = PG_IO;
		break;
	}

	pa = trunc_page(pa);
	npte |= pa | PG_V | PG_W | (prot & PROT_WRITE ? PG_RW : PG_RO);

	if ((opte & PG_W) == 0)
		pmap_kernel()->pm_stats.wired_count++;
	if ((opte & PG_V) == 0)
		pmap_kernel()->pm_stats.resident_count++;
	if (pa >= VM_MIN_KERNEL_ADDRESS)
		npte |= PG_S;

	/*
	 * Now update the pte.
	 */
	if (opte != npte) {
		DPRINTF(PDB_KENTER, (" -> npte %08x", npte));
		*pte = npte;
		tlb_flush(va);
	}

	DPRINTF(PDB_KENTER, ("\n"));

	splx(s);
}

/*
 * Specific flavour of pmap_remove for unmanaged wired mappings in the
 * kernel pmap.
 */
void
pmap_kremove(vaddr_t va, vsize_t len)
{
	vaddr_t e, eva;
	pd_entry_t *pde;
	pt_entry_t *pte, opte;
	int s;

	s = splvm();

	DPRINTF(PDB_KREMOVE, ("pmap_kremove(%08x,%08x)\n", va, len));

	e = va + len;
	while (va != e) {
		pde = pmap_pde(pmap_kernel(), va);
		eva = trunc_seg(va) + NBSEG;
		if (eva > e || eva == 0)
			eva = e;

		if (pde == NULL) {
			va = eva;
			continue;
		}

		pte = pde_pte(pde, va);
		for (; va != eva; va += PAGE_SIZE, pte++) {
			opte = *pte;
			if ((opte & PG_V) == 0)
				continue;

			pmap_kernel()->pm_stats.resident_count--;

#ifdef DIAGNOSTIC
			if (!(opte & PG_W)) {
				printf("pmap_kremove: non-wired mapping for %08x",
				    va);
			} else
#endif
			pmap_kernel()->pm_stats.wired_count--;

			*pte = PG_NV;
			tlb_flush(va);
		}
	}

	splx(s);
}

/*
 * Remove the wiring state of a page in the given pmap.
 */
void
pmap_unwire(struct pmap *pmap, vaddr_t va)
{
	pt_entry_t *pte;
	int s;

	s = splvm();

	DPRINTF(PDB_UNWIRE, ("pmap_unwire(%p,%08x)\n", pmap, va));

	pte = pmap_pte(pmap, va);

	if (*pte & PG_V)
		if (*pte & PG_W) {
			pmap->pm_stats.wired_count--;
			/* No need to flush TLB, it's a software flag */
			*pte &= ~PG_W;
		}

	splx(s);
}

/*
 * Compute the physical address of a given virtual address in the given pmap.
 * If the physical address is not mapped by this pmap, FALSE is returned.
 */
boolean_t
pmap_extract(struct pmap *pmap, vaddr_t va, paddr_t *pap)
{
	pt_entry_t *pte;
	paddr_t pa;
	boolean_t rv;
	int s;

	DPRINTF(PDB_EXTRACT, ("pmap_extract(%p,%08x)", pmap, va));

	s = splvm();

	pte = pmap_pte(pmap, va);
	if (pte != NULL && (*pte & PG_V) != 0) {
		rv = TRUE;
		pa = (*pte & PG_FRAME) | (va & PAGE_MASK);
		DPRINTF(PDB_EXTRACT, (" -> %08x\n", pa));
		if (pap != NULL)
			*pap = pa;
	} else {
		DPRINTF(PDB_EXTRACT, (" -> FALSE\n"));
		rv = FALSE;
	}

	splx(s);

	return (rv);
}

/*
 * Walk a vm_page and flush all existing mappings.
 */
void
pg_flushcache(struct vm_page *pg)
{
	struct pvlist *pvl;
	int s;

	s = splvm();

	pvl = pg_to_pvl(pg);
	if (pvl->pv_pmap == NULL) {
		splx(s);
		return;
	}

	/*
	 * Since cache_flush_page() causes the whole cache to be flushed,
	 * there is no need to loop - flush once.
	 */
	/* for (; pvl != NULL; pvl = pvl->pv_next) */
		cache_flush_page(pvl->pv_va);

	splx(s);
}

/*
 * Fill a vm_page with zeroes.
 */
void
pmap_zero_page(struct vm_page *pg)
{
	paddr_t pa;
	vaddr_t va;
	pt_entry_t *pte;
	int s;

	s = splvm();

	pa = VM_PAGE_TO_PHYS(pg);
	va = vreserve;
	pte = ptereserve;

	DPRINTF(PDB_ZERO, ("pmap_zero_page(%p/pa %x) pte %p\n", pg, pa, pte));

	pg_flushcache(pg);

	*pte = PG_V | PG_S | (pa & PG_FRAME);
	tlb_flush(va);

	qzero((caddr_t)va, PAGE_SIZE);
	cache_flush_page(va);

	/* paranoia */
	*pte = PG_NV;
	tlb_flush(va);

	splx(s);
}

/*
 * Copy the contents of a vm_page to another.
 */
void
pmap_copy_page(struct vm_page *srcpg, struct vm_page *dstpg)
{
	paddr_t srcpa, dstpa;
	vaddr_t srcva, dstva;
	pt_entry_t *srcpte, *dstpte;
	int s;

	s = splvm();

	DPRINTF(PDB_COPY, ("pmap_copy_page(%p,%p)\n", srcpg, dstpg));

	srcpa = VM_PAGE_TO_PHYS(srcpg);
	dstpa = VM_PAGE_TO_PHYS(dstpg);
	srcva = vreserve;
	dstva = srcva + PAGE_SIZE;

	dstpte = ptereserve;
	srcpte = dstpte++;

	pg_flushcache(srcpg);
	/*
	 * Since pg_flushcache() causes the whole cache to be flushed,
	 * there is no need flush dstpg.
	 */
	/* pg_flushcache(dstpg); */

	*srcpte = PG_V | PG_S | PG_RO | (srcpa & PG_FRAME);
	*dstpte = PG_V | PG_S | (dstpa & PG_FRAME);

	tlb_flush(srcva);
	tlb_flush(dstva);

	qcopy((caddr_t)srcva, (caddr_t)dstva, PAGE_SIZE);
	cache_flush_page(srcva);

	*srcpte = *dstpte = PG_NV;
	tlb_flush(srcva);
	tlb_flush(dstva);

	splx(s);
}

/*
 * Clear the modify bits on all mappings associated to the given vm_page.
 */
boolean_t
pmap_clear_modify(struct vm_page *pg)
{
	struct pvlist *pvl;
	pt_entry_t *pte;
	boolean_t rv;
	int s;
	int flushed;

	s = splvm();

	pvl = pg_to_pvl(pg);

	DPRINTF(PDB_CLEAR_M,
	    ("pmap_clear_modify(%p/pmap %p)\n", pg, pvl->pv_pmap));

	if (pvl->pv_flags & PG_M) {
		pvl->pv_flags &= ~PG_M;
		rv = TRUE;
	}

	if (pvl->pv_pmap != NULL) {
		flushed = 0;
		for (; pvl != NULL; pvl = pvl->pv_next) {
			pte = pmap_pte(pvl->pv_pmap, pvl->pv_va);
			if ((*pte & PG_V) != 0 && (*pte & PG_M) != 0) {
				/*
				 * Since cache_flush_page() causes the whole
				 * cache to be flushed, only flush once.
				 */
				if (flushed == 0) {
					cache_flush_page(pvl->pv_va);
					flushed = 1;
				}

				rv = TRUE;
				/* No need to flush TLB, it's a software flag */
				*pte &= ~PG_M;
			}
		}
	}

	splx(s);

	return (rv);
}

/*
 * Clear the reference bits on all mappings associated to the given vm_page.
 */
boolean_t
pmap_clear_reference(struct vm_page *pg)
{
	struct pvlist *pvl;
	pt_entry_t *pte;
	boolean_t rv;
	int s;

	s = splvm();

	pvl = pg_to_pvl(pg);

	DPRINTF(PDB_CLEAR_U,
	    ("pmap_clear_reference(%p/pmap %p)\n", pg, pvl->pv_pmap));

	if (pvl->pv_flags & PG_U) {
		pvl->pv_flags &= ~PG_U;
		rv = TRUE;
	}

	if (pvl->pv_pmap != NULL)
	for (; pvl != NULL; pvl = pvl->pv_next) {
		pte = pmap_pte(pvl->pv_pmap, pvl->pv_va);
		if ((*pte & PG_V) != 0 && (*pte & PG_U) != 0) {
			rv = TRUE;
			/* No need to flush TLB, it's a software flag */
			*pte &= ~PG_U;
		}
	}

	splx(s);

	return (rv);
}

/*
 * Check the reference bit attribute for the given vm_page.
 */
boolean_t
pmap_is_referenced(struct vm_page *pg)
{
	struct pvlist *pvl;
	boolean_t rv;
	int s;

	s = splvm();

	pvl = pg_to_pvl(pg);
	rv = (pvl->pv_flags & PG_U) != 0;

	DPRINTF(PDB_IS_U,
	    ("pmap_is_referenced(%p/pmap %p) -> %d\n", pg, pvl->pv_pmap, rv));

	splx(s);

	return (rv);
}

/*
 * Check the modify bit attribute for the given vm_page.
 */
boolean_t
pmap_is_modified(struct vm_page *pg)
{
	struct pvlist *pvl;
	boolean_t rv;
	int s;

	s = splvm();

	pvl = pg_to_pvl(pg);
	rv = (pvl->pv_flags & PG_M) != 0;

	DPRINTF(PDB_IS_M,
	    ("pmap_is_modified(%p/pmap %p) -> %d\n", pg, pvl->pv_pmap, rv));

	splx(s);

	return (rv);
}

/*
 * Flush instruction cache on the given dirty area.
 *
 * The KAP is the only sparc implementation OpenBSD runs on with independent
 * instruction and data caches; for now, we won't add a function pointer
 * to the cpu structure, but will directly invoke the necessary operation.
 */
void
pmap_proc_iflush(struct proc *p, vaddr_t va, vsize_t len)
{
	/* There is no way to invalidate a subset of the icache */
	sta(0, ASI_ICACHE_INVAL, 0);
}

/*
 * The following routines are not part of the MI pmap API, but are
 * necessary to use the common sparc code.
 */

/*
 * Enable caching of the page tables if necessary.
 */
void
pmap_cache_enable()
{
	/* nothing to do */
}

/*
 * Change the protection for a specific kernel mapping.
 * Used by machdep.c only.
 */
void
pmap_changeprot(struct pmap *pmap, vaddr_t va, vm_prot_t prot, int wired)
{
	pt_entry_t *pte, npte;
	int s;

	s = splvm();

	npte = PG_S | (prot & PROT_WRITE ? PG_RW : PG_RO);

	pte = pmap_pte(pmap, va);
	if ((*pte & PG_PROT) != npte) {
		*pte = (*pte & ~PG_PROT) | npte;
		tlb_flush(va);
	}

	splx(s);
}

/*
 * Set a ``red zone'' below the kernel.
 */
void
pmap_redzone()
{
}

/*
 * Write a given byte in a protected page; used by the ddb breakpoints.
 */
void
pmap_writetext(unsigned char *dst, int ch)
{
	pt_entry_t *pte, opte;
	int s;

	/*
	 * Check for a PTW hit first.
	 */
	switch ((vaddr_t)dst >> PTW_WINDOW_SHIFT) {
	case PTW1_WINDOW:
	case PTW2_WINDOW:
		*dst = (unsigned char)ch;
		cpuinfo.cache_flush(dst, 1);
		return;
	}
		
	s = splvm();

	pte = pmap_pte(pmap_kernel(), (vaddr_t)dst);
	if (pte != NULL) {
		opte = *pte;
		if ((opte & PG_V) != 0) {
			cpuinfo.cache_flush(dst, 1);

			if ((opte & PG_RO) != 0) {
				*pte &= ~PG_RO;
				tlb_flush(trunc_page((vaddr_t)dst));
			}

			*dst = (unsigned char)ch;

			if ((opte & PG_RO) != 0) {
				*pte = opte;
				tlb_flush(trunc_page((vaddr_t)dst));
			}

			cpuinfo.cache_flush(dst, 1);
		}
	}
	
	splx(s);
}

/*
 * Enable or disable cache for the given number of pages at the given
 * virtual address.
 */
void
kvm_setcache(caddr_t addr, int npages, int cached)
{
	pt_entry_t *pte, opte;
	vaddr_t va = (vaddr_t)addr;
	int s;
	int flushed;

#ifdef DIAGNOSTIC
	if (va & PAGE_MASK) {
		printf("kvm_setcache: unaligned va %08x\n", va);
		va = trunc_page(va);
	}
#endif

#ifdef DIAGNOSTIC
	/*
	 * Check for a PTW hit first.
	 */
	switch (va >> PTW_WINDOW_SHIFT) {
	case PTW1_WINDOW:
	case PTW2_WINDOW:
		printf("kvm_setcache(%08x, %08x, %d) in a PTW\n",
		    va, npages << PAGE_SHIFT, cached);
		return;
	}
#endif
		
	s = splvm();

	pte = pmap_pte(pmap_kernel(), va);
	flushed = 0;
	for (; --npages >= 0; va += PAGE_SIZE, pte++) {
		opte = *pte & ~PG_MA;

		if (cached)
			opte |= PG_CACHE;
		else
			opte |= PG_IO;

		*pte = opte;
		tlb_flush(va);

		/*
		 * Since cache_flush_page() causes the whole
		 * cache to be flushed, only flush once.
		 */
		if (flushed == 0) {
			cache_flush_page(va);
			flushed = 1;
		}
	}

	splx(s);
}

/*
 * Simple wrapper around pmap_kenter_pa() for multiple pages.
 */
vaddr_t
pmap_map(vaddr_t va, paddr_t pa, paddr_t epa, int prot)
{
	while (pa < epa) {
		pmap_kenter_pa(va, pa, (vm_prot_t)prot);
		va += PAGE_SIZE;
		pa += PAGE_SIZE;
	}
	return (va);
}

/*
 * Checks whether a given physical address is in physical memory or
 * in device space.
 * Used by mem.c.
 */
int
pmap_pa_exists(paddr_t pa)
{
	return (pa >= PHYSMEM_BASE && pa < PHYSMEM_BASE + ptoa(physmem));
}
