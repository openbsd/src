/*	$OpenBSD: pmap.c,v 1.63 2002/03/19 00:33:18 mickey Exp $	*/

/*
 * Copyright (c) 1998-2002 Michael Shalayeff
 * All rights reserved.
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
 *	This product includes software developed by Michael Shalayeff.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * MOND, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * References:
 * 1. PA7100LC ERS, Hewlett-Packard, March 30 1999, Public version 1.0
 * 2. PA7300LC ERS, Hewlett-Packard, March 18 1996, Version 1.0
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/user.h>
#include <sys/proc.h>
#include <sys/pool.h>
#include <sys/malloc.h>

#include <uvm/uvm.h>

#include <machine/reg.h>
#include <machine/psl.h>
#include <machine/cpu.h>
#include <machine/pmap.h>
#include <machine/pte.h>
#include <machine/cpufunc.h>
#include <machine/pdc.h>
#include <machine/iomod.h>

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
/*	| PDB_FOLLOW */
/*	| PDB_VP */
/*	| PDB_PV */
/*	| PDB_INIT */
/*	| PDB_ENTER */
/*	| PDB_REMOVE */
/*	| PDB_STEAL */
/*	| PDB_PROTECT */
/*	| PDB_PHYS */
	;
#else
#define	DPRINTF(l,s)	/* */
#endif

vaddr_t	virtual_steal, virtual_avail;

#if defined(HP7100LC_CPU) || defined(HP7300LC_CPU)
int		pmap_hptsize = 256;	/* patchable */
#endif

struct pmap	kernel_pmap_store;
int		pmap_sid_counter, hppa_sid_max = HPPA_SID_MAX;
boolean_t	pmap_initialized = FALSE;
struct pool	pmap_pmap_pool;
struct pool	pmap_pv_pool;
struct simplelock pvalloc_lock;

void    *pmap_pv_page_alloc(struct pool *, int);
void    pmap_pv_page_free(struct pool *, void *);

struct pool_allocator pmap_allocator_pv = {
	pmap_pv_page_alloc, pmap_pv_page_free, 0
};

u_int	hppa_prot[8];

#define	pmap_sid(pmap, va) \
	(((va & 0xc0000000) != 0xc0000000)? pmap->pmap_space : HPPA_SID_KERNEL)

#define	pmap_pvh_attrs(a) \
	(((a) & PTE_PROT(TLB_DIRTY)) | ((a) ^ PTE_PROT(TLB_REFTRAP)))

#if defined(HP7100LC_CPU) || defined(HP7300LC_CPU)
/*
 * This hash function is the one used by the hardware TLB walker on the 7100LC.
 */
static __inline struct hpt_entry *
pmap_hash(pa_space_t sp, vaddr_t va)
{
	struct hpt_entry *hpt;
	__asm __volatile (
		"extru	%2, 23, 20, %%r22\n\t"	/* r22 = (va >> 8) */
		"zdep	%1, 22, 16, %%r23\n\t"	/* r23 = (sp << 9) */
		"xor	%%r22,%%r23, %%r23\n\t"	/* r23 ^= r22 */
		"mfctl	%%cr24, %%r22\n\t"	/* r22 = sizeof(HPT)-1 */
		"and	%%r22,%%r23, %%r23\n\t"	/* r23 &= r22 */
		"mfctl	%%cr25, %%r22\n\t"	/* r22 = addr of HPT table */
		"or	%%r23, %%r22, %0"	/* %0 = HPT entry */
		: "=r" (hpt) : "r" (sp), "r" (va) : "r22", "r23");
	return hpt;
}
#endif

static __inline void
pmap_sdir_set(pa_space_t space, paddr_t pa)
{
	paddr_t vtop;

	mfctl(CR_VTOP, vtop);
#ifdef PMAPDEBUG
	if (!vtop)
		panic("pmap_sdir_set: zero vtop");
#endif
	asm("stwas	%0, 0(%1)":: "r" (pa), "r" (vtop + (space << 2)));
}

static __inline paddr_t
pmap_sdir_get(pa_space_t space)
{
	paddr_t vtop, pa;

	mfctl(CR_VTOP, vtop);
	asm("ldwax,s	%2(%1), %0": "=&r" (pa) : "r" (vtop), "r" (space));

	return (pa);
}

static __inline pt_entry_t *
pmap_pde_get(paddr_t pa, vaddr_t va)
{
	pt_entry_t *pde;

	asm("ldwax,s	%2(%1), %0": "=&r" (pde) : "r" (pa), "r" (va >> 22));

	return (pde);
}

static __inline void
pmap_pde_set(struct pmap *pm, vaddr_t va, paddr_t ptp)
{
	asm("stwas	%0, 0(%1)"
	    :: "r" (ptp), "r" ((paddr_t)pm->pm_pdir + ((va >> 20) & 0xffc)));
}

static __inline pt_entry_t *
pmap_pde_alloc(struct pmap *pm, vaddr_t va, struct vm_page **pdep)
{
	struct vm_page *pg;
	paddr_t pa;

	DPRINTF(PDB_FOLLOW|PDB_VP,
	    ("pmap_pde_alloc(%p, 0x%x, %p)\n", pm, va, pdep));

	va &= PDE_MASK;
	pg = uvm_pagealloc(&pm->pm_obj, va, NULL,
	    UVM_PGA_USERESERVE|UVM_PGA_ZERO);
	if (pg == NULL) {
		/* try to steal somewhere */
		return (NULL);
	}

	pa = VM_PAGE_TO_PHYS(pg);

	DPRINTF(PDB_FOLLOW|PDB_VP, ("pmap_pde_alloc: pde %x\n", pa));

	pg->flags &= ~PG_BUSY;	/* never busy */
	pg->wire_count = 1;		/* no mappings yet */
	pmap_pde_set(pm, va, pa);
	pm->pm_stats.resident_count++;	/* count PTP as resident */
	pm->pm_ptphint = pg;
	if (pdep)
		*pdep = pg;
	return ((pt_entry_t *)pa);
}

static __inline struct vm_page *
pmap_pde_ptp(struct pmap *pm, pt_entry_t *pde)
{
	paddr_t pa = (paddr_t)pde;

	DPRINTF(PDB_FOLLOW|PDB_PV, ("pmap_pde_ptp(%p, %p)\n", pm, pde));

	if (pm->pm_ptphint && VM_PAGE_TO_PHYS(pm->pm_ptphint) == pa)
		return (pm->pm_ptphint);

	DPRINTF(PDB_FOLLOW|PDB_PV, ("pmap_pde_ptp: lookup 0x%x\n", pa));

	return (PHYS_TO_VM_PAGE(pa));
}

static __inline void
pmap_pde_release(struct pmap *pmap, vaddr_t va, struct vm_page *ptp)
{
	ptp->wire_count--;
	if (ptp->wire_count <= 1) {
		pmap_pde_set(pmap, va, 0);
		pmap->pm_stats.resident_count--;
		if (pmap->pm_ptphint == ptp)
			pmap->pm_ptphint = TAILQ_FIRST(&pmap->pm_obj.memq);
		ptp->wire_count = 0;
		uvm_pagefree(ptp);
	}
}

static __inline pt_entry_t
pmap_pte_get(pt_entry_t *pde, vaddr_t va)
{
	pt_entry_t pte;

	asm("ldwax,s	%2(%1),%0" : "=&r" (pte)
	    : "r" (pde),  "r" ((va >> 12) & 0x3ff));

	return (pte);
}

static __inline void
pmap_pte_set(pt_entry_t *pde, vaddr_t va, pt_entry_t pte)
{
	DPRINTF(PDB_FOLLOW|PDB_VP, ("pmap_pte_set(%p, 0x%x, 0x%x)\n",
	    pde, va, pte));

#ifdef PMAPDEBUG
	if (!pde)
		panic("pmap_pte_set: zero pde");
	if (pte && pte < virtual_steal &&
	    hppa_trunc_page(pte) != (paddr_t)&gateway_page)
		panic("pmap_pte_set: invalid pte");
#endif
	if (!(pte & PTE_PROT(TLB_UNCACHABLE)))
		Debugger();
	asm("stwas	%0, 0(%1)"
	    :: "r" (pte), "r" ((paddr_t)pde + ((va >> 10) & 0xffc)));
}

static __inline pt_entry_t
pmap_vp_find(struct pmap *pm, vaddr_t va)
{
	pt_entry_t *pde;

	if (!(pde = pmap_pde_get(pm->pm_pdir, va)))
		return (NULL);

	return (pmap_pte_get(pde, va));
}

void
pmap_dump_table(pa_space_t space)
{
	pa_space_t sp;

	for (sp = 0; sp <= hppa_sid_max; sp++) {
		paddr_t pa;
		pt_entry_t *pde, pte;
		vaddr_t va, pdemask = virtual_avail + 1;

		if (((int)space >= 0 && sp != space) ||
		    !(pa = pmap_sdir_get(sp)))
			continue;

		for (va = virtual_avail; va < VM_MAX_KERNEL_ADDRESS;
		    va += PAGE_SIZE) {
			if (pdemask != (va & PDE_MASK)) {
				pdemask = va & PDE_MASK;
				if (!(pde = pmap_pde_get(pa, va))) {
					va += ~PDE_MASK + 1 - PAGE_SIZE;
					continue;
				}
				printf("%x:0x%08x:\n", sp, pde);
			}

			if (!(pte = pmap_pte_get(pde, va)))
				continue;

			printf("0x%08x-0x%08x\n", va, pte);
		}
	}
}

static __inline struct pv_entry *
pmap_pv_alloc(void)
{
	struct pv_entry *pv;

	DPRINTF(PDB_FOLLOW|PDB_PV, ("pmap_pv_alloc()\n"));

	simple_lock(&pvalloc_lock);

	pv = pool_get(&pmap_pv_pool, 0);

	simple_unlock(&pvalloc_lock);

	DPRINTF(PDB_FOLLOW|PDB_PV, ("pmap_pv_alloc: %p\n", pv));

	return (pv);
}

static __inline void
pmap_pv_free(struct pv_entry *pv)
{
	simple_lock(&pvalloc_lock);

	if (pv->pv_ptp)
		pmap_pde_release(pv->pv_pmap, pv->pv_va, pv->pv_ptp);

	pool_put(&pmap_pv_pool, pv);

	simple_unlock(&pvalloc_lock);
}

static __inline void
pmap_pv_enter(struct pv_head *pvh, struct pv_entry *pve, struct pmap *pm,
    vaddr_t va, struct vm_page *pdep)
{
	DPRINTF(PDB_FOLLOW|PDB_PV, ("pmap_pv_enter(%p, %p, %p, 0x%x, %p)\n",
	    pvh, pve, pm, va, pdep));

	pve->pv_pmap	= pm;
	pve->pv_va	= va;
	pve->pv_ptp	= pdep;
	simple_lock(&pvh->pvh_lock);		/* lock pv_head */
	pve->pv_next = pvh->pvh_list;
	pvh->pvh_list = pve;
	simple_unlock(&pvh->pvh_lock);		/* unlock, done! */
}

static __inline struct pv_entry *
pmap_pv_remove(struct pv_head *pvh, struct pmap *pmap, vaddr_t va)
{
	struct pv_entry **pve, *pv;

	for(pv = *(pve = &pvh->pvh_list); pv; pv = *(pve = &(*pve)->pv_next))
		if (pv->pv_pmap == pmap && pv->pv_va == va) {
			*pve = pv->pv_next;
			break;
		}
	return (pv);
}

void
pmap_bootstrap(vstart)
	vaddr_t vstart;
{
	extern char etext;
	extern u_int totalphysmem, *ie_mem;
	vaddr_t addr = hppa_round_page(vstart), t;
	vsize_t size;
#if 0 && (defined(HP7100LC_CPU) || defined(HP7300LC_CPU))
	struct vp_entry *hptp;
#endif
	struct pmap *kpm;
	int i;

	DPRINTF(PDB_FOLLOW|PDB_INIT, ("pmap_bootstrap(0x%x)\n", vstart));

	uvm_setpagesize();

	hppa_prot[VM_PROT_NONE | VM_PROT_NONE  | VM_PROT_NONE]    =TLB_AR_NA;
	hppa_prot[VM_PROT_READ | VM_PROT_NONE  | VM_PROT_NONE]    =TLB_AR_R;
	hppa_prot[VM_PROT_NONE | VM_PROT_WRITE | VM_PROT_NONE]    =TLB_AR_RW;
	hppa_prot[VM_PROT_READ | VM_PROT_WRITE | VM_PROT_NONE]    =TLB_AR_RW;
	hppa_prot[VM_PROT_NONE | VM_PROT_NONE  | VM_PROT_EXECUTE] =TLB_AR_RX;
	hppa_prot[VM_PROT_READ | VM_PROT_NONE  | VM_PROT_EXECUTE] =TLB_AR_RX;
	hppa_prot[VM_PROT_NONE | VM_PROT_WRITE | VM_PROT_EXECUTE] =TLB_AR_RWX;
	hppa_prot[VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE] =TLB_AR_RWX;

	/*
	 * Initialize kernel pmap
	 */
	kpm = &kernel_pmap_store;
	bzero(kpm, sizeof(*kpm));
	simple_lock_init(&kpm->pm_obj.vmobjlock);
	kpm->pm_obj.pgops = NULL;
	TAILQ_INIT(&kpm->pm_obj.memq);
	kpm->pm_obj.uo_npages = 0;
	kpm->pm_obj.uo_refs = 1;
	kpm->pm_space = HPPA_SID_KERNEL;
	kpm->pm_pid = HPPA_PID_KERNEL;
	kpm->pm_pdir_pg = NULL;
	kpm->pm_pdir = addr;
	addr += PAGE_SIZE;
	bzero((void *)addr, PAGE_SIZE);
	fdcache(HPPA_SID_KERNEL, addr, PAGE_SIZE);

	/*
	 * Allocate various tables and structures.
	 */

	mtctl(addr, CR_VTOP);
	bzero((void *)addr, (hppa_sid_max + 1) * 4);
	fdcache(HPPA_SID_KERNEL, addr, (hppa_sid_max + 1) * 4);
	printf("vtop: 0x%x @ 0x%x\n", (hppa_sid_max + 1) * 4, addr);
	addr += (hppa_sid_max + 1) * 4;
	pmap_sdir_set(HPPA_SID_KERNEL, kpm->pm_pdir);

	ie_mem = (u_int *)addr;
	addr += 0x8000;

#if 0 && (defined(HP7100LC_CPU) || defined(HP7300LC_CPU))
	if (pmap_hptsize && (cpu_type == hpcxl || cpu_type == hpcxl2)) {
		int error;

		if (pmap_hptsize > pdc_hwtlb.max_size)
			pmap_hptsize = pdc_hwtlb.max_size;
		else if (pmap_hptsize < pdc_hwtlb.min_size)
			pmap_hptsize = pdc_hwtlb.min_size;

		size = pmap_hptsize * sizeof(*hptp);
		bzero((void *)addr, size);
		/* Allocate the HPT */
		for (hptp = (struct vp_entry *)addr, i = pmap_hptsize; i--;)
			hptp[i].vp_tag = 0xffff;

		DPRINTF(PDB_INIT, ("hpt_table: 0x%x @ %p\n", size, addr));

		if ((error = (cpu_hpt_init)(addr, size)) < 0) {
			printf("WARNING: HPT init error %d\n", error);
		} else {
			printf("HPT: %d entries @ 0x%x\n",
			    pmap_hptsize / sizeof(struct vp_entry), addr);
		}

		/* TODO find a way to avoid using cr*, use cpu regs instead */
		mtctl(addr, CR_VTOP);
		mtctl(size - 1, CR_HPTMASK);
		addr += size;
	}
#endif	/* HP7100LC_CPU | HP7300LC_CPU */

	/* map the kernel space, which will give us virtual_avail */
	vstart = hppa_round_page(addr + (totalphysmem - (atop(addr))) *
	    (16 + sizeof(struct pv_head) + sizeof(struct vm_page)));
	/* XXX PCXS needs two separate inserts in separate btlbs */
	t = (vaddr_t)&etext;
	if (btlb_insert(HPPA_SID_KERNEL, 0, 0, &t,
	    pmap_sid2pid(HPPA_SID_KERNEL) |
	    pmap_prot(pmap_kernel(), VM_PROT_READ|VM_PROT_EXECUTE)) < 0)
		panic("pmap_bootstrap: cannot block map kernel text");
	t = vstart - (vaddr_t)&etext;
	if (btlb_insert(HPPA_SID_KERNEL, (vaddr_t)&etext, (vaddr_t)&etext, &t,
	    pmap_sid2pid(HPPA_SID_KERNEL) | TLB_UNCACHABLE |
	    pmap_prot(pmap_kernel(), VM_PROT_ALL)) < 0)
		panic("pmap_bootstrap: cannot block map kernel");
	vstart = (vaddr_t)&etext + t;
	virtual_avail = vstart;
	kpm->pm_stats.wired_count = kpm->pm_stats.resident_count =
	    physmem = atop(vstart);

	/*
	 * NOTE: we no longer trash the BTLB w/ unused entries,
	 * lazy map only needed pieces (see bus_mem_add_mapping() for refs).
	 */

	addr = hppa_round_page(addr);
	size = hppa_round_page(sizeof(struct pv_head) * totalphysmem);
	bzero ((caddr_t)addr, size);

	DPRINTF(PDB_INIT, ("pmseg.pvent: 0x%x @ 0x%x\n", size, addr));

	/* XXX we might need to split this for isa */
	virtual_steal = addr + size;
	i = atop(virtual_avail - virtual_steal);
	uvm_page_physload(0, totalphysmem + i,
		atop(virtual_avail), totalphysmem + i, VM_FREELIST_DEFAULT);
	/* we have only one initial phys memory segment */
	vm_physmem[0].pmseg.pvhead = (struct pv_head *)addr;
}

/*
 * pmap_steal_memory(size, startp, endp)
 *	steals memory block of size `size' from directly mapped
 *	segment (mapped behind the scenes).
 *	directly mapped cannot be grown dynamically once allocated.
 */
vaddr_t
pmap_steal_memory(size, startp, endp)
	vsize_t size;
	vaddr_t *startp;
	vaddr_t *endp;
{
	vaddr_t va;

	DPRINTF(PDB_FOLLOW|PDB_STEAL,
	    ("pmap_steal_memory(%x, %x, %x)\n", size, startp, endp));

	if (startp)
		*startp = virtual_avail;
	if (endp)
		*endp = VM_MAX_KERNEL_ADDRESS;

	size = hppa_round_page(size);
	if (size <= virtual_avail - virtual_steal) {

		DPRINTF(PDB_STEAL,
		    ("pmap_steal_memory: steal %d bytes (%x+%x,%x)\n",
		    size, virtual_steal, size, virtual_avail));

		va = virtual_steal;
		virtual_steal += size;

		/* make seg0 smaller (reduce fake top border) */
		vm_physmem[0].end -= atop(size);
		vm_physmem[0].avail_end -= atop(size);
	} else
		va = NULL;

	return va;
}

void
pmap_init()
{
	DPRINTF(PDB_FOLLOW|PDB_INIT, ("pmap_init()\n"));

	pool_init(&pmap_pmap_pool, sizeof(struct pmap), 0, 0, 0, "pmappl",
	    &pool_allocator_nointr);
	pool_init(&pmap_pv_pool, sizeof(struct pv_entry), 0, 0, 0, "pmappv",
	    &pmap_allocator_pv);
	/* deplete the steal area */
	pool_prime(&pmap_pv_pool, (virtual_avail - virtual_steal) / PAGE_SIZE *
	    pmap_pv_pool.pr_itemsperpage);

	simple_lock_init(&pvalloc_lock);

	pmap_initialized = TRUE;

	/*
	 * map SysCall gateways page once for everybody
	 * NB: we'll have to remap the phys memory
	 *     if we have any at SYSCALLGATE address (;
	 *
	 * no spls since no interrupts.
	 */
	{
		pt_entry_t *pde;

		if (!(pde = pmap_pde_get(pmap_kernel()->pm_pdir, SYSCALLGATE)) &&
		    !(pde = pmap_pde_alloc(pmap_kernel(), SYSCALLGATE, NULL)))
			panic("pmap_init: cannot allocate pde");

		pmap_pte_set(pde, SYSCALLGATE, (paddr_t)&gateway_page |
		    PTE_PROT(TLB_UNCACHABLE|TLB_GATE_PROT));
	}
}

struct pmap *
pmap_create()
{
	struct pmap *pmap;
	pa_space_t space;

	DPRINTF(PDB_FOLLOW|PDB_PMAP, ("pmap_create()\n"));
	pmap = pool_get(&pmap_pmap_pool, PR_WAITOK);

	simple_lock_init(&pmap->pm_obj.vmobjlock);
	pmap->pm_obj.pgops = NULL;	/* currently not a mappable object */
	TAILQ_INIT(&pmap->pm_obj.memq);
	pmap->pm_obj.uo_npages = 0;
	pmap->pm_obj.uo_refs = 1;
	pmap->pm_stats.wired_count = 0;
	pmap->pm_stats.resident_count = 1;

	if (pmap_sid_counter >= hppa_sid_max) {
		/* collect some */
		panic("pmap_create: outer space");
	} else
		space = ++pmap_sid_counter;

	pmap->pm_space = space;
	pmap->pm_pid = (space + 1) << 1;
	pmap->pm_pdir_pg = uvm_pagealloc(NULL, 0, NULL,
	    UVM_PGA_USERESERVE|UVM_PGA_ZERO);
	if (!pmap->pm_pdir_pg)
		panic("pmap_create: no pages");
	pmap->pm_pdir = VM_PAGE_TO_PHYS(pmap->pm_pdir_pg);

	pmap_sdir_set(space, pmap->pm_pdir);

	return(pmap);
}

void
pmap_destroy(pmap)
	struct pmap *pmap;
{
	struct vm_page *pg;
	pa_space_t space;
	int refs;

	DPRINTF(PDB_FOLLOW|PDB_PMAP, ("pmap_destroy(%p)\n", pmap));

	simple_lock(&pmap->pm_obj.vmobjlock);
	refs = --pmap->pm_obj.uo_refs;
	simple_unlock(&pmap->pm_obj.vmobjlock);

	if (refs > 0)
		return;

	TAILQ_FOREACH(pg, &pmap->pm_obj.memq, listq) {
#ifdef DIAGNOSTIC
		if (pg->flags & PG_BUSY)
			panic("pmap_release: busy page table page");
#endif
		pg->wire_count = 0;
		uvm_pagefree(pg);
	}

	uvm_pagefree(pmap->pm_pdir_pg);
	pmap->pm_pdir_pg = NULL;	/* XXX cache it? */
	pmap_sdir_set(space, 0);
	pool_put(&pmap_pmap_pool, pmap);
}

/*
 * Add a reference to the specified pmap.
 */
void
pmap_reference(pmap)
	struct pmap *pmap;
{
	DPRINTF(PDB_FOLLOW|PDB_PMAP, ("pmap_reference(%p)\n", pmap));

	simple_lock(&pmap->pm_obj.vmobjlock);
	pmap->pm_obj.uo_refs++;
	simple_unlock(&pmap->pm_obj.vmobjlock);
}

void
pmap_collect(struct pmap *pmap)
{
	DPRINTF(PDB_FOLLOW|PDB_PMAP, ("pmap_collect(%p)\n", pmap));
	/* nothing yet */
}

int
pmap_enter(pmap, va, pa, prot, flags)
	struct pmap *pmap;
	vaddr_t va;
	paddr_t pa;
	vm_prot_t prot;
	int flags;
{
	pt_entry_t *pde, pte;
	struct vm_page *ptp = NULL;
	struct pv_head *pvh;
	struct pv_entry *pve;
	int bank, off;
	boolean_t wired = (flags & PMAP_WIRED) != 0;

	DPRINTF(PDB_FOLLOW|PDB_ENTER,
	    ("pmap_enter(%p, 0x%x, 0x%x, 0x%x, 0x%x)\n",
	    pmap, va, pa, prot, flags));

	simple_lock(&pmap->pm_obj.vmobjlock);

	if (!(pde = pmap_pde_get(pmap->pm_pdir, va)) &&
	    !(pde = pmap_pde_alloc(pmap, va, &ptp))) {
		if (flags & PMAP_CANFAIL)
			return (KERN_RESOURCE_SHORTAGE);

		panic("pmap_kenter: cannot allocate pde");
	}

	if (!ptp)
		ptp = pmap_pde_ptp(pmap, pde);

	if ((pte = pmap_pte_get(pde, va))) {

		DPRINTF(PDB_ENTER,
		    ("pmap_enter: remapping 0x%x -> 0x%x\n", pte, pa));

		if (pte & PTE_PROT(TLB_EXECUTE))
			ficache(pmap->pm_space, va, NBPG);
		pitlb(pmap->pm_space, va);
		fdcache(pmap->pm_space, va, NBPG);
		pdtlb(pmap->pm_space, va);

		if (wired && !(pte & PTE_PROT(TLB_WIRED)) == 0)
			pmap->pm_stats.wired_count++;
		else if (!wired && (pte & PTE_PROT(TLB_WIRED)) != 0)
			pmap->pm_stats.wired_count--;

		if (PTE_PAGE(pte) == pa) {
			DPRINTF(PDB_FOLLOW|PDB_ENTER,
			    ("pmap_enter: same page\n"));
			goto enter;
		}

		bank = vm_physseg_find(atop(PTE_PAGE(pte)), &off);
		if (bank != -1) {
			pvh = &vm_physmem[bank].pmseg.pvhead[off];
			simple_lock(&pvh->pvh_lock);
			pve = pmap_pv_remove(pvh, pmap, va);
			pvh->pvh_attrs |= pmap_pvh_attrs(pte);
			simple_unlock(&pvh->pvh_lock);
		} else
			pve = NULL;
	} else {
		DPRINTF(PDB_ENTER,
		    ("pmap_enter: new mapping 0x%x -> 0x%x\n", va, pa));
		pte = PTE_PROT(TLB_REFTRAP);
		pve = NULL;
		pmap->pm_stats.resident_count++;
		if (wired)
			pmap->pm_stats.wired_count++;
		if (ptp)
			ptp->wire_count++;
	}

	bank = vm_physseg_find(atop(pa), &off);
	if (pmap_initialized && bank != -1) {
		if (!pve && !(pve = pmap_pv_alloc())) {
			if (flags & PMAP_CANFAIL) {
				simple_unlock(&pmap->pm_obj.vmobjlock);
				return (KERN_RESOURCE_SHORTAGE);
			}
			panic("pmap_enter: no pv entries available");
		}
		pvh = &vm_physmem[bank].pmseg.pvhead[off];
		pmap_pv_enter(pvh, pve, pmap, va, ptp);
	} else {
		pvh = NULL;
		if (pve)
			pmap_pv_free(pve);
	}

enter:
	/* preserve old ref & mod */
	pte = pa | PTE_PROT(TLB_UNCACHABLE|pmap_prot(pmap, prot)) |
	    (pte & PTE_PROT(TLB_UNCACHABLE|TLB_DIRTY|TLB_REFTRAP));
	if (wired)
		pte |= PTE_PROT(TLB_WIRED);
	pmap_pte_set(pde, va, pte);

	simple_unlock(&pmap->pm_obj.vmobjlock);

	DPRINTF(PDB_FOLLOW, ("pmap_enter: leaving\n"));

	return (0);
}

void
pmap_remove(pmap, sva, eva)
	struct pmap *pmap;
	vaddr_t sva;
	vaddr_t eva;
{
	struct pv_head *pvh;
	struct pv_entry *pve;
	pt_entry_t *pde, pte;
	int bank, off;
	u_int pdemask;

	DPRINTF(PDB_FOLLOW|PDB_REMOVE,
	    ("pmap_remove(%p, 0x%x, 0x%x\n", pmap, sva, eva));

	simple_lock(&pmap->pm_obj.vmobjlock);

	for (pdemask = sva + 1; sva < eva; sva += PAGE_SIZE) {
		if (pdemask != (sva & PDE_MASK)) {
			pdemask = sva & PDE_MASK;
			if (!(pde = pmap_pde_get(pmap->pm_pdir, sva))) {
				sva += ~PDE_MASK + 1 - PAGE_SIZE;
				continue;
			}
		}

		if ((pte = pmap_pte_get(pde, sva))) {

			if (pte & PTE_PROT(TLB_WIRED))
				pmap->pm_stats.wired_count--;
			pmap->pm_stats.resident_count--;

			if (pte & PTE_PROT(pte))
				ficache(pmap->pm_space, sva, PAGE_SIZE);
			pitlb(pmap->pm_space, sva);
			fdcache(pmap->pm_space, sva, PAGE_SIZE);
			pdtlb(pmap->pm_space, sva);

			pmap_pte_set(pde, sva, 0);

			bank = vm_physseg_find(atop(pte), &off);
			if (pmap_initialized && bank != -1) {
				pvh = &vm_physmem[bank].pmseg.pvhead[off];
				pve = pmap_pv_remove(pvh, pmap, sva);
				if (pve) {
					pvh->pvh_attrs |= pmap_pvh_attrs(pte);
					pmap_pv_free(pve);
				}
			}
		}
	}

	simple_unlock(&pmap->pm_obj.vmobjlock);

	DPRINTF(PDB_FOLLOW|PDB_REMOVE, ("pmap_remove: leaving\n"));
}

void
pmap_write_protect(pmap, sva, eva, prot)
	struct pmap *pmap;
	vaddr_t sva;
	vaddr_t eva;
	vm_prot_t prot;
{
	pt_entry_t *pde, pte;
	u_int tlbprot, pdemask;

	DPRINTF(PDB_FOLLOW|PDB_PMAP,
	    ("pmap_write_protect(%p, %x, %x, %x)\n", pmap, sva, eva, prot));

	sva = hppa_trunc_page(sva);
	tlbprot = PTE_PROT(pmap_prot(pmap, prot));

	simple_lock(&pmap->pm_obj.vmobjlock);

	for(pdemask = sva + 1; sva < eva; sva += PAGE_SIZE) {
		if (pdemask != (sva & PDE_MASK)) {
			pdemask = sva & PDE_MASK;
			if (!(pde = pmap_pde_get(pmap->pm_pdir, sva))) {
				sva += ~PDE_MASK + 1 - PAGE_SIZE;
				continue;
			}
		}
		if ((pte = pmap_pte_get(pde, sva))) {

			DPRINTF(PDB_PMAP,
			    ("pmap_write_protect: pte=0x%x\n", pte));
			/*
			 * Determine if mapping is changing.
			 * If not, nothing to do.
			 */
			if ((pte & PTE_PROT(TLB_AR_MASK)) == tlbprot)
				continue;

			if (pte & PTE_PROT(TLB_EXECUTE))
				ficache(pmap->pm_space, sva, PAGE_SIZE);
			pitlb(pmap->pm_space, sva);
			fdcache(pmap->pm_space, sva, PAGE_SIZE);
			pdtlb(pmap->pm_space, sva);

			pte &= ~PTE_PROT(TLB_AR_MASK);
			pte |= tlbprot;
			pmap_pte_set(pde, sva, pte);
		}
	}

	simple_unlock(&pmap->pm_obj.vmobjlock);
}

void
pmap_page_remove(pg)
	struct vm_page *pg;
{
	struct pv_head *pvh;
	struct pv_entry *pve, *ppve;
	pt_entry_t *pde, pte;
	int bank, off;

	DPRINTF(PDB_FOLLOW|PDB_PV, ("pmap_page_remove(%p)\n", pg));

	bank = vm_physseg_find(atop(VM_PAGE_TO_PHYS(pg)), &off);
	if (bank == -1) {
		printf("pmap_page_remove: unmanaged page?\n");
		return;
	}

	pvh = &vm_physmem[bank].pmseg.pvhead[off];
	if (pvh->pvh_list == NULL)
		return;

	simple_lock(&pvh->pvh_lock);

	for (pve = pvh->pvh_list; pve; ) {
		simple_lock(&pve->pv_pmap->pm_obj.vmobjlock);

		pde = pmap_pde_get(pve->pv_pmap->pm_pdir, pve->pv_va);
		pte = pmap_pte_get(pde, pve->pv_va);
		pmap_pte_set(pde, pve->pv_va, 0);

		if (pte & PTE_PROT(TLB_WIRED))
			pve->pv_pmap->pm_stats.wired_count--;
		pve->pv_pmap->pm_stats.resident_count--;

		simple_unlock(&pve->pmap->pm_obj.vmobjlock);

		pvh->pvh_attrs |= pmap_pvh_attrs(pte);
		ppve = pve;
		pve = pve->pv_next;
		pmap_pv_free(ppve);
	}
	simple_unlock(&pvh->pvh_lock);

	DPRINTF(PDB_FOLLOW|PDB_PV, ("pmap_page_remove: leaving\n"));

}

void
pmap_unwire(pmap, va)
	struct pmap *pmap;
	vaddr_t	va;
{
	pt_entry_t *pde, pte = 0;

	DPRINTF(PDB_FOLLOW|PDB_PMAP, ("pmap_unwire(%p, 0x%x)\n", pmap, va));

	simple_lock(&pmap->pm_obj.vmobjlock);
	if ((pde = pmap_pde_get(pmap->pm_pdir, va))) {
		pte = pmap_pte_get(pde, va);

		if (pte & PTE_PROT(TLB_WIRED)) {
			pte &= ~PTE_PROT(TLB_WIRED);
			pmap->pm_stats.wired_count--;
			pmap_pte_set(pde, va, pte);
		}
	}
	simple_unlock(&pmap->pm_obj.vmobjlock);

	DPRINTF(PDB_FOLLOW|PDB_PMAP, ("pmap_unwire: leaving\n"));

#ifdef DIAGNOSTIC
	if (!pte)
		panic("pmap_unwire: invalid va 0x%x", va);
#endif
}

boolean_t
pmap_changebit(struct vm_page *pg, u_int set, u_int clear)
{
	struct pv_head *pvh;
	struct pv_entry *pve;
	pt_entry_t *pde, pte, res;
	int bank, off;

	DPRINTF(PDB_FOLLOW|PDB_BITS,
	    ("pmap_changebit(%p, %x, %x)\n", pg, set, clear));

	bank = vm_physseg_find(atop(VM_PAGE_TO_PHYS(pg)), &off);
	if (bank == -1) {
		printf("pmap_testbits: unmanaged page?\n");
		return(FALSE);
	}

	pvh = &vm_physmem[bank].pmseg.pvhead[off];
	res = pvh->pvh_attrs = 0;

	simple_lock(&pvh->pvh_lock);
	for(pve = pvh->pvh_list; pve; pve = pve->pv_next) {
		simple_lock(&pve->pv_pmap->pm_obj.vmobjlock);
		if ((pde = pmap_pde_get(pve->pv_pmap->pm_pdir, pve->pv_va))) {
			pte = pmap_pte_get(pde, pve->pv_va);
			res |= pmap_pvh_attrs(pte);
			pte &= ~clear;
			pte |= set;

			if (pte & PTE_PROT(TLB_EXECUTE))
				pitlb(pve->pv_pmap->pm_space, pve->pv_va);
			/* XXX flush only if there was mod ? */
			fdcache(pve->pv_pmap->pm_space, pve->pv_va, PAGE_SIZE);
			pdtlb(pve->pv_pmap->pm_space, pve->pv_va);

			pmap_pte_set(pde, pve->pv_va, pte);
		}
		simple_unlock(&pve->pv_pmap->pm_obj.vmobjlock);
	}
	pvh->pvh_attrs = res;
	simple_unlock(&pvh->pvh_lock);

	return ((res & clear) != 0);
}

boolean_t
pmap_testbit(struct vm_page *pg, u_int bits)
{
	struct pv_head *pvh;
	struct pv_entry *pve;
	pt_entry_t pte;
	int bank, off;

	DPRINTF(PDB_FOLLOW|PDB_BITS, ("pmap_testbit(%p, %x)\n", pg, bits));

	bank = vm_physseg_find(atop(VM_PAGE_TO_PHYS(pg)), &off);
	if (bank == -1) {
		printf("pmap_testbits: unmanaged page?\n");
		return(FALSE);
	}

	simple_lock(&pvh->pvh_lock);
	pvh = &vm_physmem[bank].pmseg.pvhead[off];
	for(pve = pvh->pvh_list; !(pvh->pvh_attrs & bits) && pve;
	    pve = pve->pv_next) {
		simple_lock(&pve->pv_pmap->pm_obj.vmobjlock);
		pte = pmap_vp_find(pve->pv_pmap, pve->pv_va);
		simple_unlock(&pve->pv_pmap->pm_obj.vmobjlock);
		pvh->pvh_attrs |= pmap_pvh_attrs(pte);
	}
	simple_unlock(&pvh->pvh_lock);

	return ((pvh->pvh_attrs & bits) != 0);
}

boolean_t
pmap_extract(pmap, va, pap)
	struct pmap *pmap;
	vaddr_t va;
	paddr_t *pap;
{
	pt_entry_t pte;

	DPRINTF(PDB_FOLLOW|PDB_EXTRACT, ("pmap_extract(%p, %x)\n", pmap, va));

	simple_lock(&pmap->pm_obj.vmobjlock);
	pte = pmap_vp_find(pmap, va);
	simple_unlock(&pmap->pm_obj.vmobjlock);

	if (pte) {
		if (pap)
			*pap = (pte & ~PGOFSET) | (va & PGOFSET);
		return (TRUE);
	}

	return (FALSE);
}

void
pmap_zero_page(pa)
	paddr_t pa;
{
	extern int dcache_line_mask;
	register paddr_t pe = pa + PAGE_SIZE;
	int s;

	DPRINTF(PDB_FOLLOW|PDB_PHYS, ("pmap_zero_page(%x)\n", pa));

	/*
	 * do not allow any ints to happen, since cache is in
	 * inconsistant state during the loop.
	 *
	 * do not rsm(PSW_I) since that will lose ints completely,
	 * instead, keep 'em pending (or verify by the book).
	 */
	s = splhigh();

	while (pa < pe) {
		__asm volatile(			/* can use ,bc */
		    "stwas,ma %%r0,4(%0)\n\t"
		    "stwas,ma %%r0,4(%0)\n\t"
		    "stwas,ma %%r0,4(%0)\n\t"
		    "stwas,ma %%r0,4(%0)"
		    : "+r" (pa) :: "memory");

		if (!(pa & dcache_line_mask))
			__asm volatile("rsm	%1, %%r0\n\t"
				       "fdc	%2(%0)\n\t"
				       "ssm	%1, %%r0"
			    :: "r" (pa), "i" (PSW_D), "r" (-4) : "memory");
	}

	sync_caches();
	splx(s);
}

void
pmap_copy_page(spa, dpa)
	paddr_t spa;
	paddr_t dpa;
{
	extern int dcache_line_mask;
	register paddr_t spe = spa + PAGE_SIZE;
	int s;

	DPRINTF(PDB_FOLLOW|PDB_PHYS, ("pmap_copy_page(%x, %x)\n", spa, dpa));

	s = splhigh();
	/* XXX flush cache for the spa ??? */

	while (spa < spe) {
		__asm volatile(			/* can use ,bc */
		    "ldwas,ma 4(%0),%%r22\n\t"
		    "ldwas,ma 4(%0),%%r21\n\t"
		    "stwas,ma %%r22,4(%1)\n\t"
		    "stwas,ma %%r21,4(%1)\n\t"
		    "ldwas,ma 4(%0),%%r22\n\t"
		    "ldwas,ma 4(%0),%%r21\n\t"
		    "stwas,ma %%r22,4(%1)\n\t"
		    "stwas,ma %%r21,4(%1)\n\t"
		    : "+r" (spa), "+r" (dpa) :: "r22", "r21", "memory");

		if (!(spa & dcache_line_mask))
			__asm volatile(
			    "rsm	%2, %%r0\n\t"
			    "pdc	%3(%0)\n\t"
			    "fdc	%3(%1)\n\t"
			    "ssm	%2, %%r0"
			    :: "r" (spa), "r" (dpa), "i" (PSW_D), "r" (-4)
			    : "memory");
	}

	sync_caches();
	splx(s);
}

void
pmap_kenter_pa(va, pa, prot)
	vaddr_t va;
	paddr_t pa;
	vm_prot_t prot;
{
	pt_entry_t *pde, pte;

	DPRINTF(PDB_FOLLOW|PDB_ENTER,
	    ("pmap_kenter_pa(%x, %x, %x)\n", va, pa, prot));

	if (!(pde = pmap_pde_get(pmap_kernel()->pm_pdir, va)) &&
	    !(pde = pmap_pde_alloc(pmap_kernel(), va, NULL)))
		panic("pmap_kenter_pa: cannot allocate pde");
#ifdef DIAGNOSTIC
	if ((pte = pmap_pte_get(pde, va)))
		panic("pmap_kenter_pa: 0x%x is already mapped %p:0x%x",
		    va, pde, pte);
#endif

	pte = pa | PTE_PROT(TLB_UNCACHABLE|TLB_WIRED|TLB_DIRTY|pmap_prot(pmap_kernel(), prot));
	pmap_pte_set(pde, va, pte);

	DPRINTF(PDB_FOLLOW|PDB_ENTER, ("pmap_kenter_pa: leaving\n"));
}

void
pmap_kremove(va, size)
	vaddr_t va;
	vsize_t size;
{
	pt_entry_t *pde, pte;
	vaddr_t eva = va + size, pdemask;

	DPRINTF(PDB_FOLLOW|PDB_REMOVE,
	    ("pmap_kremove(%x, %x)\n", va, size));

	for (pdemask = va + 1; va < eva; va += PAGE_SIZE) {
		if (pdemask != (va & PDE_MASK)) {
			pdemask = va & PDE_MASK;
			if (!(pde = pmap_pde_get(pmap_kernel()->pm_pdir, va))) {
				va += ~PDE_MASK + 1 - PAGE_SIZE;
				continue;
			}
		}
		if (!(pte = pmap_pte_get(pde, va))) {
#ifdef DEBUG
			printf("pmap_kremove: unmapping unmapped 0x%x\n", va);
#endif
			continue;
		}

		if (pte & PTE_PROT(TLB_EXECUTE))
			ficache(HPPA_SID_KERNEL, va, NBPG);
		pitlb(HPPA_SID_KERNEL, va);
		fdcache(HPPA_SID_KERNEL, va, NBPG);
		pdtlb(HPPA_SID_KERNEL, va);

		pmap_pte_set(pde, va, 0);
	}

	DPRINTF(PDB_FOLLOW|PDB_REMOVE, ("pmap_kremove: leaving\n"));
}

void *
pmap_pv_page_alloc(struct pool *pp, int flags)
{
	vaddr_t va;

	DPRINTF(PDB_FOLLOW|PDB_POOL,
	    ("pmap_pv_page_alloc(%p, %x)\n", pp, flags));

	if ((va = pmap_steal_memory(PAGE_SIZE, NULL, NULL)))
		return (void *)va;

	/*
		TODO
	if (list not empty) {
		get from the list;
		return (va);
	}
	*/

	DPRINTF(PDB_FOLLOW|PDB_POOL,
	    ("pmap_pv_page_alloc: uvm_km_alloc_poolpage1\n"));

	return ((void *)uvm_km_alloc_poolpage1(kernel_map, uvm.kernel_object,
	    (flags & PR_WAITOK) ? TRUE : FALSE));
}

void
pmap_pv_page_free(struct pool *pp, void *v)
{
	vaddr_t va = (vaddr_t)v;

	DPRINTF(PDB_FOLLOW|PDB_POOL, ("pmap_pv_page_free(%p, %p)\n", pp, v));

	if (va < virtual_avail) {
		/* TODO save on list somehow */
	} else
		uvm_km_free_poolpage1(kernel_map, va);
}
