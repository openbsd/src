/*	$OpenBSD: pmap.c,v 1.51 2010/05/09 15:46:17 jasper Exp $	*/
/*
 * Copyright (c) 2001-2004, Miodrag Vallat
 * Copyright (c) 1998-2001 Steve Murphree, Jr.
 * Copyright (c) 1996 Nivas Madhur
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
 *      This product includes software developed by Nivas Madhur.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
 */
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/pool.h>
#include <sys/msgbuf.h>
#include <sys/user.h>

#include <machine/asm_macro.h>
#include <machine/cmmu.h>
#include <machine/cpu.h>
#include <machine/lock.h>
#include <machine/pmap_table.h>
#ifdef M88100
#include <machine/m8820x.h>
#endif

#include <uvm/uvm.h>

/*
 * VM externals
 */
extern vaddr_t	avail_start;
extern vaddr_t	virtual_avail, virtual_end;
extern vaddr_t	last_addr;

#ifdef	PMAPDEBUG
/*
 * Static variables, functions and variables for debugging
 */

/*
 * conditional debugging
 */
#define CD_FULL		0x02

#define CD_ACTIVATE	0x0000004	/* pmap_activate */
#define CD_KMAP		0x0000008	/* pmap_expand_kmap */
#define CD_MAP		0x0000010	/* pmap_map */
#define CD_CACHE	0x0000020	/* pmap_cache_ctrl */
#define CD_INIT		0x0000080	/* pmap_init */
#define CD_CREAT	0x0000100	/* pmap_create */
#define CD_FREE		0x0000200	/* pmap_release */
#define CD_DESTR	0x0000400	/* pmap_destroy */
#define CD_RM		0x0000800	/* pmap_remove */
#define CD_RMPG		0x0001000	/* pmap_remove_page */
#define CD_PROT		0x0002000	/* pmap_protect */
#define CD_EXP		0x0004000	/* pmap_expand */
#define CD_ENT		0x0008000	/* pmap_enter */
#define CD_UPD		0x0010000	/* pmap_update */
#define CD_COL		0x0020000	/* pmap_collect */
#define CD_CBIT		0x0040000	/* pmap_changebit */
#define CD_TBIT		0x0080000	/* pmap_testbit */
#define CD_USBIT	0x0100000	/* pmap_unsetbit */
#define	CD_COPY		0x0200000	/* pmap_copy_page */
#define	CD_ZERO		0x0400000	/* pmap_zero_page */
#define CD_ALL		0x0fffffc

int pmap_debug = 0;

#endif	/* PMAPDEBUG */

struct pool pmappool, pvpool;

caddr_t vmmap;
pt_entry_t *vmpte, *msgbufmap;

struct pmap kernel_pmap_store;
pmap_t kernel_pmap = &kernel_pmap_store;

apr_t	default_apr = CACHE_GLOBAL | APR_V;

typedef struct kpdt_entry *kpdt_entry_t;
struct kpdt_entry {
	kpdt_entry_t	next;
	paddr_t		phys;
};

kpdt_entry_t	kpdt_free;

/*
 * Two pages of scratch space per cpu.
 * Used in pmap_copy_page() and pmap_zero_page().
 */
vaddr_t phys_map_vaddr, phys_map_vaddr_end;

static pv_entry_t pg_to_pvh(struct vm_page *);

static __inline pv_entry_t
pg_to_pvh(struct vm_page *pg)
{
	return &pg->mdpage.pvent;
}

/*
 *	Locking primitives
 */

#if defined(MULTIPROCESSOR) && 0
#define	PMAP_LOCK(pmap)		__cpu_simple_lock(&(pmap)->pm_lock)
#define	PMAP_UNLOCK(pmap)	__cpu_simple_unlock(&(pmap)->pm_lock)
#else
#define	PMAP_LOCK(pmap)		do { /* nothing */ } while (0)
#define	PMAP_UNLOCK(pmap)	do { /* nothing */ } while (0)
#endif

vaddr_t kmapva = 0;

/*
 * Internal routines
 */
void	flush_atc_entry(pmap_t, vaddr_t);
pt_entry_t *pmap_expand_kmap(vaddr_t, vm_prot_t, int);
void	pmap_remove_pte(pmap_t, vaddr_t, pt_entry_t *, boolean_t);
void	pmap_remove_range(pmap_t, vaddr_t, vaddr_t);
void	pmap_expand(pmap_t, vaddr_t);
void	pmap_release(pmap_t);
vaddr_t	pmap_map(vaddr_t, paddr_t, paddr_t, vm_prot_t, u_int);
pt_entry_t *pmap_pte(pmap_t, vaddr_t);
void	pmap_remove_page(struct vm_page *);
void	pmap_changebit(struct vm_page *, int, int);
boolean_t pmap_testbit(struct vm_page *, int);

/*
 * quick PTE field checking macros
 */
#define	pmap_pte_w(pte)		(*(pte) & PG_W)
#define	pmap_pte_prot(pte)	(*(pte) & PG_PROT)

#define	pmap_pte_w_chg(pte, nw)		((nw) ^ pmap_pte_w(pte))
#define	pmap_pte_prot_chg(pte, np)	((np) ^ pmap_pte_prot(pte))

#define	m88k_protection(prot)	((prot) & VM_PROT_WRITE ? PG_RW : PG_RO)

#define SDTENT(map, va)		((sdt_entry_t *)((map)->pm_stab + SDTIDX(va)))

/*
 * Routine:	FLUSH_ATC_ENTRY
 *
 * Function:
 *	Flush atc (TLB) which maps given pmap and virtual address.
 *
 * Parameters:
 *	pmap	affected pmap
 *	va	virtual address that should be flushed
 */
void
flush_atc_entry(pmap_t pmap, vaddr_t va)
{
	struct cpu_info *ci;
	boolean_t kernel = pmap == kernel_pmap;

#ifdef MULTIPROCESSOR	/* { */
	CPU_INFO_ITERATOR cpu;

	/*
	 * On 88100, we take action immediately.
	 */
	if (CPU_IS88100) {
		CPU_INFO_FOREACH(cpu, ci) {
			if (kernel || pmap == ci->ci_curpmap)
				cmmu_flush_tlb(ci->ci_cpuid, kernel, va, 1);
		}
	}

	/*
	 * On 88110, we only remember which tlb need to be invalidated,
	 * and wait for pmap_update() to do it.
	 */
	if (CPU_IS88110) {
		CPU_INFO_FOREACH(cpu, ci) {
			if (kernel)
				ci->ci_pmap_ipi |= CI_IPI_TLB_FLUSH_KERNEL;
			else if (pmap == ci->ci_curpmap)
				ci->ci_pmap_ipi |= CI_IPI_TLB_FLUSH_USER;
		}
	}
#else	/* MULTIPROCESSOR */	/* } { */
	ci = curcpu();

	if (kernel || pmap == ci->ci_curpmap) {
		if (CPU_IS88100)
			cmmu_flush_tlb(ci->ci_cpuid, kernel, va, 1);
		if (CPU_IS88110)
			ci->ci_pmap_ipi |= kernel ?
			    CI_IPI_TLB_FLUSH_KERNEL : CI_IPI_TLB_FLUSH_USER;
	}
#endif	/* MULTIPROCESSOR */	/* } */
}

#ifdef M88110
void
pmap_update(pmap_t pm)
{
	/*
	 * Time to perform all necessary TLB invalidations.
	 */
	if (CPU_IS88110) {
		u_int ipi;
#ifdef MULTIPROCESSOR
		struct cpu_info *ci;
		CPU_INFO_ITERATOR cpu;
		
		CPU_INFO_FOREACH(cpu, ci)
#else
		struct cpu_info *ci = curcpu();
#endif
		/* CPU_INFO_FOREACH(cpu, ci) */ {
			ipi = atomic_clear_int(&ci->ci_pmap_ipi);
			if (ipi & CI_IPI_TLB_FLUSH_KERNEL)
				cmmu_flush_tlb(ci->ci_cpuid, TRUE, 0 ,0);
			if (ipi & CI_IPI_TLB_FLUSH_USER)
				cmmu_flush_tlb(ci->ci_cpuid, FALSE, 0 ,0);
		}
	}
}
#endif

/*
 * Routine:	PMAP_PTE
 *
 * Function:
 *	Given a map and a virtual address, compute a (virtual) pointer
 *	to the page table entry (PTE) which maps the address .
 *	If the page table associated with the address does not
 *	exist, NULL is returned (and the map may need to grow).
 *
 * Parameters:
 *	pmap	pointer to pmap structure
 *	virt	virtual address for which page table entry is desired
 *
 *    Otherwise the page table address is extracted from the segment table,
 *    the page table index is added, and the result is returned.
 */

static __inline__
pt_entry_t *
sdt_pte(sdt_entry_t *sdt, vaddr_t va)
{
	return ((pt_entry_t *)
	    (PG_PFNUM(*(sdt + SDT_ENTRIES)) << PDT_SHIFT) + PDTIDX(va));
}

pt_entry_t *
pmap_pte(pmap_t pmap, vaddr_t virt)
{
	sdt_entry_t *sdt;

	sdt = SDTENT(pmap, virt);
	/*
	 * Check whether page table exists.
	 */
	if (!SDT_VALID(sdt))
		return (NULL);

	return (sdt_pte(sdt, virt));
}

/*
 * Routine:	PMAP_EXPAND_KMAP (internal)
 *
 * Function:
 *    Allocate a page descriptor table (pte_table) and validate associated
 * segment table entry, returning pointer to page table entry. This is
 * much like 'pmap_expand', except that table space is acquired
 * from an area set up by pmap_bootstrap, instead of through
 * uvm_km_zalloc. (Obviously, because uvm_km_zalloc uses the kernel map
 * for allocation - which we can't do when trying to expand the
 * kernel map!) Note that segment tables for the kernel map were
 * all allocated at pmap_bootstrap time, so we only need to worry
 * about the page table here.
 *
 * Parameters:
 *	virt	VA for which translation tables are needed
 *	prot	protection attributes for segment entries
 *
 * Extern/Global:
 *	kpdt_free	kernel page table free queue
 *
 * This routine simply dequeues a table from the kpdt_free list,
 * initializes all its entries (invalidates them), and sets the
 * corresponding segment table entry to point to it. If the kpdt_free
 * list is empty - we panic (no other places to get memory, sorry). (Such
 * a panic indicates that pmap_bootstrap is not allocating enough table
 * space for the kernel virtual address space).
 *
 */
pt_entry_t *
pmap_expand_kmap(vaddr_t virt, vm_prot_t prot, int canfail)
{
	sdt_entry_t template, *sdt;
	kpdt_entry_t kpdt_ent;

#ifdef PMAPDEBUG
	if ((pmap_debug & (CD_KMAP | CD_FULL)) == (CD_KMAP | CD_FULL))
		printf("pmap_expand_kmap(%p, %x, %d)\n", virt, prot, canfail);
#endif

	template = m88k_protection(prot) | PG_M | SG_V;

	/* segment table entry derivate from map and virt. */
	sdt = SDTENT(kernel_pmap, virt);
#ifdef PMAPDEBUG
	if (SDT_VALID(sdt))
		panic("pmap_expand_kmap: segment table entry VALID");
#endif

	kpdt_ent = kpdt_free;
	if (kpdt_ent == NULL) {
		if (canfail)
			return (NULL);
		else
			panic("pmap_expand_kmap: Ran out of kernel pte tables");
	}

	kpdt_free = kpdt_free->next;
	/* physical table */
	*sdt = kpdt_ent->phys | template;
	/* virtual table */
	*(sdt + SDT_ENTRIES) = (vaddr_t)kpdt_ent | template;

	/* Reinitialize this kpdt area to zero */
	bzero((void *)kpdt_ent, PDT_SIZE);

	return (pt_entry_t *)(kpdt_ent) + PDTIDX(virt);
}

/*
 * Routine:	PMAP_MAP
 *
 * Function:
 *    Map memory at initialization. The physical addresses being
 * mapped are not managed and are never unmapped.
 *
 * Parameters:
 *	virt	virtual address of range to map
 *	start	physical address of range to map
 *	end	physical address of end of range
 *	prot	protection attributes
 *	cmode	cache control attributes
 *
 * Calls:
 *	pmap_pte
 *	pmap_expand_kmap
 *
 * Special Assumptions
 *	For now, VM is already on, only need to map the specified
 * memory. Used only by pmap_bootstrap() and vm_page_startup().
 *
 * For each page that needs mapping:
 *	pmap_pte is called to obtain the address of the page table
 *	table entry (PTE). If the page table does not exist,
 *	pmap_expand_kmap is called to allocate it. Finally, the page table
 *	entry is set to point to the physical page.
 *
 *	initialize template with paddr, prot, dt
 *	look for number of phys pages in range
 *	{
 *		pmap_pte(virt)	- expand if necessary
 *		stuff pte from template
 *		increment virt one page
 *		increment template paddr one page
 *	}
 *
 */
vaddr_t
pmap_map(vaddr_t virt, paddr_t start, paddr_t end, vm_prot_t prot, u_int cmode)
{
	u_int npages;
	u_int num_phys_pages;
	pt_entry_t template, *pte;
	paddr_t	 page;

#ifdef PMAPDEBUG
	if (pmap_debug & CD_MAP)
		printf("pmap_map(%p, %p, %p, %x, %x)\n",
		    virt, start, end, prot, cmode);
#endif

#ifdef PMAPDEBUG
	/* Check for zero if we map the very end of the address space... */
	if (start > end && end != 0)
		panic("pmap_map: start greater than end address");
#endif

	template = m88k_protection(prot) | cmode | PG_V;
#ifdef M88110
	if (CPU_IS88110 && m88k_protection(prot) != PG_RO)
		template |= PG_M;
#endif

	page = trunc_page(start);
	npages = atop(round_page(end) - page);
	for (num_phys_pages = npages; num_phys_pages != 0; num_phys_pages--) {
		if ((pte = pmap_pte(kernel_pmap, virt)) == NULL)
			pte = pmap_expand_kmap(virt,
			    VM_PROT_READ | VM_PROT_WRITE, 0);

#ifdef PMAPDEBUG
		if ((pmap_debug & (CD_MAP | CD_FULL)) == (CD_MAP | CD_FULL))
			if (PDT_VALID(pte))
				printf("pmap_map: pte @%p already valid\n", pte);
#endif

		*pte = template | page;
		virt += PAGE_SIZE;
		page += PAGE_SIZE;
	}
	return virt;
}

/*
 * Routine:	PMAP_CACHE_CONTROL
 *
 * Function:
 *	Set the cache-control bits in the page table entries(PTE) which maps
 *	the specified virtual address range.
 *
 * Parameters:
 *	pmap_t		pmap
 *	vaddr_t		s
 *	vaddr_t		e
 *	u_int		mode
 *
 * Calls:
 *	pmap_pte
 *	invalidate_pte
 *	flush_atc_entry
 *
 *  This routine sequences through the pages of the specified range.
 * For each, it calls pmap_pte to acquire a pointer to the page table
 * entry (PTE). If the PTE is invalid, or non-existent, nothing is done.
 * Otherwise, the cache-control bits in the PTE's are adjusted as specified.
 *
 */
void
pmap_cache_ctrl(pmap_t pmap, vaddr_t s, vaddr_t e, u_int mode)
{
	int spl;
	pt_entry_t opte, *pte;
	vaddr_t va;
	paddr_t pa;
	cpuid_t cpu;

#ifdef PMAPDEBUG
	if (pmap_debug & CD_CACHE)
		printf("pmap_cache_ctrl(%p, %p, %p, %x)\n", pmap, s, e, mode);
#endif

	spl = splvm();
	PMAP_LOCK(pmap);

	for (va = s; va != e; va += PAGE_SIZE) {
		if ((pte = pmap_pte(pmap, va)) == NULL)
			continue;
#ifdef PMAPDEBUG
		if ((pmap_debug & (CD_CACHE | CD_FULL)) == (CD_CACHE | CD_FULL))
			printf("cache_ctrl: pte@%p\n", pte);
#endif
		/*
		 * Invalidate pte temporarily to avoid being written back
		 * the modified bit and/or the reference bit by any other cpu.
		 * XXX
		 */
		opte = invalidate_pte(pte);
		*pte = (opte & ~CACHE_MASK) | mode;
		flush_atc_entry(pmap, va);

		/*
		 * Data cache should be copied back and invalidated if
		 * the old mapping was cached.
		 */
		if ((opte & CACHE_INH) == 0) {
			pa = ptoa(PG_PFNUM(opte));
#ifdef MULTIPROCESSOR
			for (cpu = 0; cpu < MAX_CPUS; cpu++)
				if (ISSET(m88k_cpus[cpu].ci_flags, CIF_ALIVE))
#else
			cpu = cpu_number();
#endif
					cmmu_flush_cache(cpu, pa, PAGE_SIZE);
		}
	}
	PMAP_UNLOCK(pmap);
	splx(spl);
}

/*
 * Routine:	PMAP_BOOTSTRAP
 *
 * Function:
 *	Bootstrap the system enough to run with virtual memory.
 *	Map the kernel's code and data, allocate the kernel
 *	translation table space, and map control registers
 *	and other IO addresses.
 *
 * Parameters:
 *	load_start	PA where kernel was loaded
 *
 * Extern/Global:
 *
 *	PAGE_SIZE	VM (software) page size
 *	etext		end of kernel text
 *	phys_map_vaddr	VA of page mapped arbitrarily for debug/IO
 *
 * Calls:
 *	__cpu_simple_lock_init
 *	pmap_map
 *
 *    The physical address 'load_start' is mapped at
 * VM_MIN_KERNEL_ADDRESS, which maps the kernel code and data at the
 * virtual address for which it was (presumably) linked. Immediately
 * following the end of the kernel code/data, sufficient page of
 * physical memory are reserved to hold translation tables for the kernel
 * address space.
 *
 *    A pair of virtual pages per cpu are reserved for debugging and
 * IO purposes. They are arbitrarily mapped when needed. They are used,
 * for example, by pmap_copy_page and pmap_zero_page.
 *
 *    This implementation also assumes that the space below the kernel
 * is reserved (typically from PROM purposes). We should ideally map it
 * read only except when invoking its services...
 */

void
pmap_bootstrap(vaddr_t load_start)
{
	kpdt_entry_t kpdt_virt;
	sdt_entry_t *kmap;
	vaddr_t vaddr, virt;
	paddr_t s_text, e_text, kpdt_phys;
	unsigned int kernel_pmap_size, pdt_size;
	int i;
	pmap_table_t ptable;
	extern void *etext;

#ifdef MULTIPROCESSOR
	__cpu_simple_lock_init(&kernel_pmap->pm_lock);
#endif

	/*
	 * Allocate the kernel page table from the front of available
	 * physical memory, i.e. just after where the kernel image was loaded.
	 */
	/*
	 * The calling sequence is
	 *    ...
	 *  pmap_bootstrap(&kernelstart, ...);
	 * kernelstart being the first symbol in the load image.
	 */

	avail_start = round_page(avail_start);
	virtual_avail = avail_start;

	/*
	 * Initialize kernel_pmap structure
	 */
	kernel_pmap->pm_count = 1;
	kmap = (sdt_entry_t *)(avail_start);
	kernel_pmap->pm_stab = (sdt_entry_t *)virtual_avail;
	kmapva = virtual_avail;

	/*
	 * Reserve space for segment table entries.
	 * One for the regular segment table and one for the shadow table
	 * The shadow table keeps track of the virtual address of page
	 * tables. This is used in virtual-to-physical address translation
	 * functions. Remember, MMU cares only for physical addresses of
	 * segment and page table addresses. For kernel page tables, we
	 * really don't need this virtual stuff (since the kernel will
	 * be mapped 1-to-1) but for user page tables, this is required.
	 * Just to be consistent, we will maintain the shadow table for
	 * kernel pmap also.
	 */
	kernel_pmap_size = 2 * SDT_SIZE;

#ifdef PMAPDEBUG
	printf("kernel segment table size = %p\n", kernel_pmap_size);
#endif
	/* init all segment descriptors to zero */
	bzero(kernel_pmap->pm_stab, kernel_pmap_size);

	avail_start += kernel_pmap_size;
	virtual_avail += kernel_pmap_size;

	/* make sure page tables are page aligned!! XXX smurph */
	avail_start = round_page(avail_start);
	virtual_avail = round_page(virtual_avail);

	/* save pointers to where page table entries start in physical memory */
	kpdt_phys = avail_start;
	kpdt_virt = (kpdt_entry_t)virtual_avail;

	/* Compute how much space we need for the kernel page table */
	pdt_size = atop(VM_MAX_KERNEL_ADDRESS - VM_MIN_KERNEL_ADDRESS)
	    * sizeof(pt_entry_t);
	for (ptable = pmap_table_build(); ptable->size != (vsize_t)-1; ptable++)
		pdt_size += atop(ptable->size) * sizeof(pt_entry_t);
	pdt_size = round_page(pdt_size);
	kernel_pmap_size += pdt_size;
	avail_start += pdt_size;
	virtual_avail += pdt_size;

	/* init all page descriptors to zero */
	bzero((void *)kpdt_phys, pdt_size);
#ifdef PMAPDEBUG
	printf("kernel page start = %p\n", kpdt_phys);
	printf("kernel page table size = %p\n", pdt_size);
	printf("kernel page end = %p\n", avail_start);
	printf("kpdt_virt = %p\n", kpdt_virt);
#endif
	/*
	 * init the kpdt queue
	 */
	kpdt_free = kpdt_virt;
	for (i = pdt_size / PDT_SIZE; i != 0; i--) {
		kpdt_virt->next = (kpdt_entry_t)((vaddr_t)kpdt_virt + PDT_SIZE);
		kpdt_virt->phys = kpdt_phys;
		kpdt_virt = kpdt_virt->next;
		kpdt_phys += PDT_SIZE;
	}
	kpdt_virt->next = NULL; /* terminate the list */

	/*
	 * Map the kernel image into virtual space
	 */

	s_text = trunc_page(load_start);	/* paddr of text */
	e_text = round_page((vaddr_t)&etext);	/* paddr of end of text */

	/* map the PROM area */
	vaddr = pmap_map(0, 0, s_text, VM_PROT_WRITE | VM_PROT_READ, CACHE_INH);

	/* map the kernel text read only */
	vaddr = pmap_map(s_text, s_text, e_text, VM_PROT_READ, 0);

	vaddr = pmap_map(vaddr, e_text, (paddr_t)kmap,
	    VM_PROT_WRITE | VM_PROT_READ, 0);

	/*
	 * Map system segment & page tables - should be cache inhibited?
	 * 88200 manual says that CI bit is driven on the Mbus while accessing
	 * the translation tree. I don't think we need to map it CACHE_INH
	 * here...
	 */
	if (kmapva != vaddr) {
		while (vaddr < (virtual_avail - kernel_pmap_size))
			vaddr = round_page(vaddr + 1);
	}
	vaddr = pmap_map(vaddr, (paddr_t)kmap, avail_start,
	    VM_PROT_WRITE | VM_PROT_READ, CACHE_INH);

	vaddr = pmap_bootstrap_md(vaddr);

	virtual_avail = round_page(virtual_avail);
	virtual_end = VM_MAX_KERNEL_ADDRESS;

	/*
	 * Map two pages per cpu for copying/zeroing.
	 */

	phys_map_vaddr = virtual_avail;
	phys_map_vaddr_end = virtual_avail + 2 * (ncpusfound << PAGE_SHIFT);
	avail_start += 2 * (ncpusfound << PAGE_SHIFT);
	virtual_avail += 2 * (ncpusfound << PAGE_SHIFT);

	/*
	 * Create all the machine-specific mappings.
	 */

	for (ptable = pmap_table_build(); ptable->size != (vsize_t)-1; ptable++)
		if (ptable->size != 0) {
			pmap_map(ptable->virt_start, ptable->phys_start,
			    ptable->phys_start + ptable->size,
			    ptable->prot, ptable->cacheability);
		}

	/*
	 * Allocate all the submaps we need. Note that SYSMAP just allocates
	 * kernel virtual address with no physical backing memory. The idea
	 * is physical memory will be mapped at this va before using that va.
	 * This means that if different physical pages are going to be mapped
	 * at different times, we better do a tlb flush before using it -
	 * else we will be referencing the wrong page.
	 */

#define	SYSMAP(c, p, v, n)	\
({ \
	v = (c)virt; \
	if ((p = pmap_pte(kernel_pmap, virt)) == NULL) \
		pmap_expand_kmap(virt, VM_PROT_READ | VM_PROT_WRITE, 0); \
	virt += ((n) * PAGE_SIZE); \
})

	virt = virtual_avail;

	SYSMAP(caddr_t, vmpte, vmmap, 1);
	invalidate_pte(vmpte);

	SYSMAP(struct msgbuf *, msgbufmap, msgbufp, atop(MSGBUFSIZE));

	virtual_avail = virt;

	/*
	 * Switch to using new page tables
	 */

#if !defined(MULTIPROCESSOR) && defined(M88110)
	if (CPU_IS88110)
		default_apr &= ~CACHE_GLOBAL;
#endif
	kernel_pmap->pm_apr = (atop((paddr_t)kmap) << PG_SHIFT) | default_apr |
	    CACHE_WT;

	pmap_bootstrap_cpu(cpu_number());
}

void
pmap_bootstrap_cpu(cpuid_t cpu)
{
	/* Invalidate entire kernel TLB and get ready for address translation */
#ifdef MULTIPROCESSOR
	if (cpu != master_cpu)
		cmmu_initialize_cpu(cpu);
	else
#endif
		cmmu_flush_tlb(cpu, TRUE, 0, -1);

	/* Load supervisor pointer to segment table. */
	cmmu_set_sapr(kernel_pmap->pm_apr);
#ifdef PMAPDEBUG
	printf("cpu%d: running virtual\n", cpu);
#endif
	curcpu()->ci_curpmap = NULL;
}

/*
 * Routine:	PMAP_INIT
 *
 * Function:
 *	Initialize the pmap module. It is called by vm_init, to initialize
 *	any structures that the pmap system needs to map virtual memory.
 *
 * Calls:
 *	pool_init
 *
 *   This routine does not really have much to do. It initializes
 * pools for pmap structures and pv_entry structures.
 */
void
pmap_init(void)
{
#ifdef PMAPDEBUG
	if (pmap_debug & CD_INIT)
		printf("pmap_init()\n");
#endif

	pool_init(&pmappool, sizeof(struct pmap), 0, 0, 0, "pmappl",
	    &pool_allocator_nointr);
	pool_init(&pvpool, sizeof(pv_entry_t), 0, 0, 0, "pvpl", NULL);
} /* pmap_init() */

/*
 * Routine:	PMAP_ZERO_PAGE
 *
 * Function:
 *	Zeroes the specified page.
 *
 * Parameters:
 *	pg		page to zero
 *
 * Extern/Global:
 *	phys_map_vaddr
 *
 * Special Assumptions:
 *	no locking required
 *
 *	This routine maps the physical pages at the 'phys_map' virtual
 * address set up in pmap_bootstrap. It flushes the TLB to make the new
 * mappings effective, and zeros all the bits.
 */
void
pmap_zero_page(struct vm_page *pg)
{
	paddr_t pa = VM_PAGE_TO_PHYS(pg);
	vaddr_t va;
	int spl;
	int cpu = cpu_number();
	pt_entry_t *pte;
	extern void zeropage(vaddr_t);

#ifdef PMAPDEBUG
	if (pmap_debug & CD_ZERO)
		printf("pmap_zero_page(%p) pa %p\n", pg, pa);
#endif

	va = (vaddr_t)(phys_map_vaddr + 2 * (cpu << PAGE_SHIFT));
	pte = pmap_pte(kernel_pmap, va);

	spl = splvm();

	*pte = m88k_protection(VM_PROT_READ | VM_PROT_WRITE) |
	    PG_M /* 88110 */ | PG_U | PG_V | pa;

	/*
	 * We don't need the flush_atc_entry() dance, as these pages are
	 * bound to only one cpu.
	 */
	cmmu_flush_tlb(cpu, TRUE, va, 1);
	zeropage(va);
	splx(spl);
}

/*
 * Routine:	PMAP_CREATE
 *
 * Function:
 *	Create and return a physical map. If the size specified for the
 *	map is zero, the map is an actual physical map, and may be referenced
 *	by the hardware. If the size specified is non-zero, the map will be
 *	used in software only, and is bounded by that size.
 *
 *  This routines allocates a pmap structure.
 */
pmap_t
pmap_create(void)
{
	pmap_t pmap;
	sdt_entry_t *segdt;
	paddr_t stpa;
	u_int s;

	pmap = pool_get(&pmappool, PR_WAITOK | PR_ZERO);

	/*
	 * Allocate memory for *actual* segment table and *shadow* table.
	 */
	s = round_page(2 * SDT_SIZE);

	segdt = (sdt_entry_t *)uvm_km_zalloc(kernel_map, s);
	if (segdt == NULL)
		panic("pmap_create: uvm_km_zalloc failure");

	/*
	 * Initialize pointer to segment table both virtual and physical.
	 */
	pmap->pm_stab = segdt;
	if (pmap_extract(kernel_pmap, (vaddr_t)segdt,
	    (paddr_t *)&stpa) == FALSE)
		panic("pmap_create: pmap_extract failed!");
	pmap->pm_apr = (atop(stpa) << PG_SHIFT) | default_apr;
#if !defined(MULTIPROCESSOR) && defined(M88110)
	if (CPU_IS88110)
		pmap->pm_apr &= ~CACHE_GLOBAL;
#endif

#ifdef PMAPDEBUG
	if (pmap_debug & CD_CREAT)
		printf("pmap_create() -> pmap %p, pm_stab %p (pa %p)\n",
		    pmap, pmap->pm_stab, stpa);
#endif

	/* memory for page tables should not be writeback or local */
	pmap_cache_ctrl(kernel_pmap,
	    (vaddr_t)segdt, (vaddr_t)segdt + s, CACHE_WT);

	/*
	 * Initialize SDT_ENTRIES.
	 */
	/*
	 * There is no need to clear segment table, since uvm_km_zalloc
	 * provides us clean pages.
	 */

	/*
	 * Initialize pmap structure.
	 */
	pmap->pm_count = 1;
#ifdef MULTIPROCESSOR
	__cpu_simple_lock_init(&pmap->pm_lock);
#endif

	return pmap;
}

/*
 * Routine:	PMAP_RELEASE
 *
 *	Internal procedure used by pmap_destroy() to actualy deallocate
 *	the tables.
 *
 * Parameters:
 *	pmap		pointer to pmap structure
 *
 * Calls:
 *	pmap_pte
 *	uvm_km_free
 *
 * Special Assumptions:
 *	No locking is needed, since this is only called which the
 * 	pm_count field of the pmap structure goes to zero.
 *
 * This routine sequences of through the user address space, releasing
 * all translation table space back to the system using uvm_km_free.
 * The loops are indexed by the virtual address space
 * ranges represented by the table group sizes (1 << SDT_SHIFT).
 */
void
pmap_release(pmap_t pmap)
{
	u_int sdt;		/* outer loop index */
	sdt_entry_t *sdttbl;	/* ptr to first entry in the segment table */
	pt_entry_t *gdttbl;	/* ptr to first entry in a page table */

#ifdef PMAPDEBUG
	if (pmap_debug & CD_FREE)
		printf("pmap_release(%p)\n", pmap);
#endif

	/* segment table loop */
	for (sdt = VM_MIN_ADDRESS >> SDT_SHIFT;
	    sdt <= VM_MAX_ADDRESS >> SDT_SHIFT; sdt++) {
		if ((gdttbl = pmap_pte(pmap, sdt << SDT_SHIFT)) != NULL) {
#ifdef PMAPDEBUG
			if ((pmap_debug & (CD_FREE | CD_FULL)) == (CD_FREE | CD_FULL))
				printf("pmap_release(%p) free page table %p\n",
				    pmap, gdttbl);
#endif
			uvm_km_free(kernel_map, (vaddr_t)gdttbl, PAGE_SIZE);
		}
	}

	/*
	 * Freeing both *actual* and *shadow* segment tables
	 */
	sdttbl = pmap->pm_stab;		/* addr of segment table */
#ifdef PMAPDEBUG
	if ((pmap_debug & (CD_FREE | CD_FULL)) == (CD_FREE | CD_FULL))
		printf("(pmap_release(%p) free segment table %p\n",
		    pmap, sdttbl);
#endif
	uvm_km_free(kernel_map, (vaddr_t)sdttbl, round_page(2 * SDT_SIZE));
}

/*
 * Routine:	PMAP_DESTROY
 *
 * Function:
 *	Retire the given physical map from service. Should only be called
 *	if the map contains no valid mappings.
 *
 * Parameters:
 *	pmap		pointer to pmap structure
 *
 * Calls:
 *	pmap_release
 *	pool_put
 *
 * Special Assumptions:
 *	Map contains no valid mappings.
 *
 *  This routine decrements the reference count in the pmap
 * structure. If it goes to zero, pmap_release is called to release
 * the memory space to the system. Then, call pool_put to free the
 * pmap structure.
 */
void
pmap_destroy(pmap_t pmap)
{
	int count;

	PMAP_LOCK(pmap);
	count = --pmap->pm_count;
	PMAP_UNLOCK(pmap);
	if (count == 0) {
		pmap_release(pmap);
		pool_put(&pmappool, pmap);
	}
}


/*
 * Routine:	PMAP_REFERENCE
 *
 * Function:
 *	Add a reference to the specified pmap.
 *
 * Parameters:
 *	pmap		pointer to pmap structure
 *
 * Under a pmap read lock, the pm_count field of the pmap structure
 * is incremented. The function then returns.
 */
void
pmap_reference(pmap_t pmap)
{
	PMAP_LOCK(pmap);
	pmap->pm_count++;
	PMAP_UNLOCK(pmap);
}

/*
 * Routine:	PMAP_REMOVE_PTE (internal)
 *
 * Function:
 *	Invalidate a given page table entry associated with the
 *	given virtual address.
 *
 * Parameters:
 *	pmap		pointer to pmap structure
 *	va		virtual address of page to remove
 *	pte		existing pte
 *
 * External/Global:
 *	pv lists
 *
 * Calls:
 *	pool_put
 *	invalidate_pte
 *	flush_atc_entry
 *
 * Special Assumptions:
 *	The pmap must be locked.
 *
 *  If the PTE is valid, the routine must invalidate the entry. The
 * 'modified' bit, if on, is referenced to the VM, and into the appropriate
 * entry in the PV list entry. Next, the function must find the PV
 * list entry associated with this pmap/va (if it doesn't exist - the function
 * panics). The PV list entry is unlinked from the list, and returned to
 * its zone.
 */
void
pmap_remove_pte(pmap_t pmap, vaddr_t va, pt_entry_t *pte, boolean_t flush)
{
	pt_entry_t opte;
	pv_entry_t prev, cur, pvl;
	struct vm_page *pg;
	paddr_t pa;

#ifdef PMAPDEBUG
	if (pmap_debug & CD_RM)
		printf("pmap_remove_pte(%p, %p, %d)\n", pmap, va, flush);
#endif

	if (pte == NULL || !PDT_VALID(pte)) {
		return;	 	/* no page mapping, nothing to do! */
	}

	/*
	 * Update statistics.
	 */
	pmap->pm_stats.resident_count--;
	if (pmap_pte_w(pte))
		pmap->pm_stats.wired_count--;

	pa = ptoa(PG_PFNUM(*pte));

	/*
	 * Invalidate the pte.
	 */

	opte = invalidate_pte(pte) & PG_M_U;
	if (flush)
		flush_atc_entry(pmap, va);

	pg = PHYS_TO_VM_PAGE(pa);

	/* If this isn't a managed page, just return. */
	if (pg == NULL)
		return;

	/*
	 * Remove the mapping from the pvlist for
	 * this physical page.
	 */
	pvl = pg_to_pvh(pg);

#ifdef DIAGNOSTIC
	if (pvl->pv_pmap == NULL)
		panic("pmap_remove_pte: null pv_list");
#endif

	prev = NULL;
	for (cur = pvl; cur != NULL; cur = cur->pv_next) {
		if (cur->pv_va == va && cur->pv_pmap == pmap)
			break;
		prev = cur;
	}
	if (cur == NULL) {
		panic("pmap_remove_pte: mapping for va "
		    "0x%lx (pa 0x%lx) not in pv list at %p",
		    va, pa, pvl);
	}

	if (prev == NULL) {
		/*
		 * Handler is the pv_entry. Copy the next one
		 * to handler and free the next one (we can't
		 * free the handler)
		 */
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

	/* Update saved attributes for managed page */
	pvl->pv_flags |= opte;
}

/*
 * Routine:	PMAP_REMOVE_RANGE (internal)
 *
 * Function:
 *	Invalidate page table entries associated with the
 *	given virtual address range. The entries given are the first
 *	(inclusive) and last (exclusive) entries for the VM pages.
 *
 * Parameters:
 *	pmap		pointer to pmap structure
 *	s		virtual address of start of range to remove
 *	e		virtual address of end of range to remove
 *
 * External/Global:
 *	pv lists
 *
 * Calls:
 *	pmap_pte
 *	pmap_remove_pte
 *
 * Special Assumptions:
 *	The pmap must be locked.
 *
 *   This routine sequences through the pages defined by the given
 * range. For each page, the associated page table entry (PTE) is
 * invalidated via pmap_remove_pte().
 *
 * Empty segments are skipped for performance.
 */
void
pmap_remove_range(pmap_t pmap, vaddr_t s, vaddr_t e)
{
	vaddr_t va, eseg;

#ifdef PMAPDEBUG
	if (pmap_debug & CD_RM)
		printf("pmap_remove_range(%p, %p, %p)\n", pmap, s, e);
#endif

	/*
	 * Loop through the range in PAGE_SIZE increments.
	 */
	va = s;
	while (va != e) {
		sdt_entry_t *sdt;

		eseg = (va & SDT_MASK) + (1 << SDT_SHIFT);
		if (eseg > e || eseg == 0)
			eseg = e;

		sdt = SDTENT(pmap, va);

		/* If no segment table, skip a whole segment */
		if (!SDT_VALID(sdt))
			va = eseg;
		else {
			while (va != eseg) {
				pmap_remove_pte(pmap, va, sdt_pte(sdt, va),
				    TRUE);
				va += PAGE_SIZE;
			}
		}
	}
}

/*
 * Routine:	PMAP_REMOVE
 *
 * Function:
 *	Remove the given range of addresses from the specified map.
 *	It is assumed that start and end are properly rounded to the VM page
 *	size.
 *
 * Parameters:
 *	pmap		pointer to pmap structure
 *	s
 *	e
 *
 * Special Assumptions:
 *	Assumes not all entries must be valid in specified range.
 *
 * Calls:
 *	pmap_remove_range
 *
 *  After taking pmap read lock, pmap_remove_range is called to do the
 * real work.
 */
void
pmap_remove(pmap_t pmap, vaddr_t s, vaddr_t e)
{
	int spl;

	if (pmap == NULL)
		return;

	spl = splvm();
	PMAP_LOCK(pmap);
	pmap_remove_range(pmap, s, e);
	PMAP_UNLOCK(pmap);
	splx(spl);
}

/*
 * Routine:	PMAP_REMOVE_ALL
 *
 * Function:
 *	Removes this physical page from all physical maps in which it
 *	resides. Reflects back modify bits to the pager.
 *
 * Parameters:
 *	pg		physical pages which is to
 *			be removed from all maps
 *
 * Extern/Global:
 *	pv lists
 *
 * Calls:
 *	pmap_pte
 *	pool_put
 *
 *  If the page specified by the given address is not a managed page,
 * this routine simply returns. Otherwise, the PV list associated with
 * that page is traversed. For each pmap/va pair pmap_pte is called to
 * obtain a pointer to the page table entry (PTE) associated with the
 * va (the PTE must exist and be valid, otherwise the routine panics).
 * The hardware 'modified' bit in the PTE is examined. If it is on, the
 * corresponding bit in the PV list entry corresponding
 * to the physical page is set to 1.
 * Then, the PTE is invalidated, and the PV list entry is unlinked and
 * freed.
 *
 *  At the end of this function, the PV list for the specified page
 * will be null.
 */
void
pmap_remove_page(struct vm_page *pg)
{
	pt_entry_t *pte;
	pv_entry_t pvl;
	vaddr_t va;
	pmap_t pmap;
	int spl;

	if (pg == NULL) {
		/* not a managed page. */
#ifdef PMAPDEBUG
		if (pmap_debug & CD_RMPG)
			printf("pmap_remove_page(%p): not a managed page\n", pg);
#endif
		return;
	}

#ifdef PMAPDEBUG
	if (pmap_debug & CD_RMPG)
		printf("pmap_remove_page(%p)\n", pg);
#endif

	spl = splvm();
	/*
	 * Walk down PV list, removing all mappings.
	 * We don't have to lock the pv list, since we have the entire pmap
	 * system.
	 */
#if defined(MULTIPROCESSOR) && 0
remove_all_Retry:
#endif

	pvl = pg_to_pvh(pg);

	/*
	 * Loop for each entry on the pv list
	 */
	while (pvl != NULL && (pmap = pvl->pv_pmap) != NULL) {
#if defined(MULTIPROCESSOR) && 0
		if (!__cpu_simple_lock_try(&pmap->pm_lock))
			goto remove_all_Retry;
#endif

		va = pvl->pv_va;
		pte = pmap_pte(pmap, va);

		if (pte == NULL || !PDT_VALID(pte)) {
			pvl = pvl->pv_next;
			goto next;	/* no page mapping */
		}
		if (pmap_pte_w(pte)) {
#ifdef PMAPDEBUG
			if (pmap_debug & CD_RMPG)
				printf("pmap_remove_page(%p): wired mapping not removed\n",
				    pg);
#endif
			pvl = pvl->pv_next;
			goto next;
		}

		pmap_remove_pte(pmap, va, pte, TRUE);

		/*
		 * Do not free any page tables,
		 * leaves that for when VM calls pmap_collect().
		 */
next:
		PMAP_UNLOCK(pmap);
	}
	splx(spl);
}

/*
 * Routine:	PMAP_PROTECT
 *
 * Function:
 *	Sets the physical protection on the specified range of this map
 *	as requested.
 *
 * Parameters:
 *	pmap		pointer to pmap structure
 *	s		start address of start of range
 *	e		end address of end of range
 *	prot		desired protection attributes
 *
 *	Calls:
 *		PMAP_LOCK, PMAP_UNLOCK
 *		pmap_pte
 *		PDT_VALID
 *
 *  This routine sequences through the pages of the specified range.
 * For each, it calls pmap_pte to acquire a pointer to the page table
 * entry (PTE). If the PTE is invalid, or non-existent, nothing is done.
 * Otherwise, the PTE's protection attributes are adjusted as specified.
 */
void
pmap_protect(pmap_t pmap, vaddr_t s, vaddr_t e, vm_prot_t prot)
{
	int spl;
	pt_entry_t *pte, ap;
	vaddr_t va, eseg;

	if ((prot & VM_PROT_READ) == 0) {
		pmap_remove(pmap, s, e);
		return;
	}

	ap = m88k_protection(prot);

	spl = splvm();
	PMAP_LOCK(pmap);

	/*
	 * Loop through the range in PAGE_SIZE increments.
	 */
	va = s;
	while (va != e) {
		sdt_entry_t *sdt;

		eseg = (va & SDT_MASK) + (1 << SDT_SHIFT);
		if (eseg > e || eseg == 0)
			eseg = e;

		sdt = SDTENT(pmap, va);

		/* If no segment table, skip a whole segment */
		if (!SDT_VALID(sdt))
			va = eseg;
		else {
			while (va != eseg) {
				pte = sdt_pte(sdt, va);
				if (pte != NULL && PDT_VALID(pte)) {
					/*
					 * Invalidate pte temporarily to avoid
					 * the modified bit and/or the
					 * reference bit being written back by
					 * any other cpu.
					 */
					*pte = ap |
					    (invalidate_pte(pte) & ~PG_PROT);
					flush_atc_entry(pmap, va);
				}
				va += PAGE_SIZE;
			}
		}
	}
	PMAP_UNLOCK(pmap);
	splx(spl);
}

/*
 * Routine:	PMAP_EXPAND
 *
 * Function:
 *	Expands a pmap to be able to map the specified virtual address.
 *	New kernel virtual memory is allocated for a page table.
 *
 *	Must be called with the pmap system and the pmap unlocked, since
 *	these must be unlocked to use vm_allocate or vm_deallocate (via
 *	uvm_km_zalloc). Thus it must be called in a unlock/lock loop
 *	that checks whether the map has been expanded enough. (We won't loop
 *	forever, since page table aren't shrunk.)
 *
 * Parameters:
 *	pmap	point to pmap structure
 *	v	VA indicating which tables are needed
 *
 * Extern/Global:
 *	user_pt_map
 *	kernel_pmap
 *
 * Calls:
 *	pmap_pte
 *	uvm_km_free
 *	uvm_km_zalloc
 *	pmap_extract
 *
 * Special Assumptions
 *	no pmap locks held
 *	pmap != kernel_pmap
 *
 * 1:	This routine immediately allocates space for a page table.
 *
 * 2:	The page table entries (PTEs) are initialized (set invalid), and
 *	the corresponding segment table entry is set to point to the new
 *	page table.
 */
void
pmap_expand(pmap_t pmap, vaddr_t v)
{
	int spl;
	vaddr_t pdt_vaddr;
	paddr_t pdt_paddr;
	sdt_entry_t *sdt;
	pt_entry_t *pte;

#ifdef PMAPDEBUG
	if (pmap_debug & CD_EXP)
		printf ("pmap_expand(%p, %p)\n", pmap, v);
#endif

	/* XXX */
	pdt_vaddr = uvm_km_zalloc(kernel_map, PAGE_SIZE);
	if (pmap_extract(kernel_pmap, pdt_vaddr, &pdt_paddr) == FALSE)
		panic("pmap_expand: pmap_extract failed");

	/* memory for page tables should not be writeback or local */
	pmap_cache_ctrl(kernel_pmap,
	    pdt_vaddr, pdt_vaddr + PAGE_SIZE, CACHE_WT);

	spl = splvm();
	PMAP_LOCK(pmap);

	if ((pte = pmap_pte(pmap, v)) != NULL) {
		/*
		 * Someone else caused us to expand
		 * during our vm_allocate.
		 */
		PMAP_UNLOCK(pmap);
		uvm_km_free(kernel_map, pdt_vaddr, PAGE_SIZE);

#ifdef PMAPDEBUG
		if (pmap_debug & CD_EXP)
			printf("pmap_expand(%p, %p): table has already been allocated\n",
			    pmap, v);
#endif
		splx(spl);
		return;
	}
	/*
	 * Apply a mask to V to obtain the vaddr of the beginning of
	 * its containing page 'table group', i.e. the group of
	 * page tables that fit eithin a single VM page.
	 * Using that, obtain the segment table pointer that references the
	 * first page table in the group, and initialize all the
	 * segment table descriptions for the page 'table group'.
	 */
	v &= ~((1 << (PDT_BITS + PG_BITS)) - 1);

	sdt = SDTENT(pmap, v);

	/*
	 * Init each of the segment entries to point the freshly allocated
	 * page tables.
	 */
	*((sdt_entry_t *)sdt) = pdt_paddr | SG_RW | SG_V;
	*((sdt_entry_t *)(sdt + SDT_ENTRIES)) = pdt_vaddr | SG_RW | SG_V;

	PMAP_UNLOCK(pmap);
	splx(spl);
}

/*
 * Routine:	PMAP_ENTER
 *
 * Function:
 *	Insert the given physical page (p) at the specified virtual
 *	address (v) in the target phisical map with the protecton requested.
 *	If specified, the page will be wired down, meaning that the
 *	related pte can not be reclaimed.
 *
 * N.B.: This is the only routine which MAY NOT lazy-evaluation or lose
 *	information. That is, this routine must actually insert this page
 *	into the given map NOW.
 *
 * Parameters:
 *	pmap	pointer to pmap structure
 *	va	VA of page to be mapped
 *	pa	PA of page to be mapped
 *	prot	protection attributes for page
 *	wired	wired attribute for page
 *
 * Extern/Global:
 *	pv lists
 *
 * Calls:
 *	pmap_pte
 *	pmap_expand
 *	pmap_remove_pte
 *
 *	This routine starts off by calling pmap_pte to obtain a (virtual)
 * pointer to the page table entry corresponding to given virtual
 * address. If the page table itself does not exist, pmap_expand is
 * called to allocate it.
 *
 *	If the page table entry (PTE) already maps the given physical page,
 * all that is needed is to set the protection and wired attributes as
 * given. TLB entries are flushed and pmap_enter returns.
 *
 *	If the page table entry (PTE) maps a different physical page than
 * that given, the old mapping is removed by a call to map_remove_range.
 * And execution of pmap_enter continues.
 *
 *	To map the new physical page, the routine first inserts a new
 * entry in the PV list exhibiting the given pmap and virtual address.
 * It then inserts the physical page address, protection attributes, and
 * wired attributes into the page table entry (PTE).
 *
 *
 *	get machine-dependent prot code
 *	get the pte for this page
 *	if necessary pmap_expand(pmap, v)
 *	if (changing wired attribute or protection) {
 * 		flush entry from TLB
 *		update template
 *		for (ptes per vm page)
 *			stuff pte
 *	} else if (mapped at wrong addr)
 *		flush entry from TLB
 *		pmap_remove_pte
 *	} else {
 *		enter mapping in pv_list
 *		setup template and stuff ptes
 *	}
 *
 */
int
pmap_enter(pmap_t pmap, vaddr_t va, paddr_t pa, vm_prot_t prot, int flags)
{
	int spl;
	pt_entry_t *pte, template;
	paddr_t old_pa;
	pv_entry_t pv_e, pvl;
	boolean_t wired = (flags & PMAP_WIRED) != 0;
	struct vm_page *pg;

#ifdef PMAPDEBUG
	if (pmap_debug & CD_ENT)
		printf("pmap_enter(%p, %p, %p, %x, %x)\n",
		    pmap, va, pa, prot, flags);
#endif

	template = m88k_protection(prot);

	spl = splvm();
	PMAP_LOCK(pmap);

	/*
	 * Expand pmap to include this pte.
	 */
	while ((pte = pmap_pte(pmap, va)) == NULL) {
		if (pmap == kernel_pmap) {
			/* will only return NULL if PMAP_CANFAIL is set */
			if (pmap_expand_kmap(va, VM_PROT_READ | VM_PROT_WRITE,
			    flags & PMAP_CANFAIL) == NULL) {
#ifdef PMAPDEBUG
				if (pmap_debug & CD_ENT)
					printf("failed (ENOMEM)\n");
#endif
				return (ENOMEM);
			}
		} else {
			/*
			 * Must unlock to expand the pmap.
			 */
			PMAP_UNLOCK(pmap);
			pmap_expand(pmap, va);
			PMAP_LOCK(pmap);
		}
	}
	/*
	 * Special case if the physical page is already mapped at this address.
	 */
	old_pa = ptoa(PG_PFNUM(*pte));
#ifdef PMAPDEBUG
	if (pmap_debug & CD_ENT)
		printf("pmap_enter: old_pa %p pte %p\n", old_pa, *pte);
#endif

	pg = PHYS_TO_VM_PAGE(pa);
	if (pg != NULL)
		pvl = pg_to_pvh(pg);
	else
		pvl = NULL;

	if (old_pa == pa) {
		/* May be changing its wired attributes or protection */
		if (wired && !(pmap_pte_w(pte)))
			pmap->pm_stats.wired_count++;
		else if (!wired && pmap_pte_w(pte))
			pmap->pm_stats.wired_count--;
	} else {
		/* Remove old mapping from the PV list if necessary. */
		pmap_remove_pte(pmap, va, pte, FALSE);

		if (pvl != NULL) {
			/*
			 * Enter the mapping in the PV list for this
			 * managed page.
			 */
			if (pvl->pv_pmap == NULL) {
				/*
				 * No mappings yet.
				 */
				pvl->pv_va = va;
				pvl->pv_pmap = pmap;
				pvl->pv_next = NULL;
				pvl->pv_flags = 0;
			} else {
				/*
				 * Add new pv_entry after header.
				 */
				pv_e = pool_get(&pvpool, PR_NOWAIT);
				if (pv_e == NULL) {
					/* Invalidate the old pte anyway */
					flush_atc_entry(pmap, va);

					if (flags & PMAP_CANFAIL) {
						PMAP_UNLOCK(pmap);
						splx(spl);
						return (ENOMEM);
					} else
						panic("pmap_enter: "
						    "pvpool exhausted");
				}
				pv_e->pv_va = va;
				pv_e->pv_pmap = pmap;
				pv_e->pv_next = pvl->pv_next;
				pv_e->pv_flags = 0;
				pvl->pv_next = pv_e;
			}
		}

		/*
		 * And count the mapping.
		 */
		pmap->pm_stats.resident_count++;
		if (wired)
			pmap->pm_stats.wired_count++;
	} /* if (pa == old_pa) ... else */

	template |= PG_V;
	if (wired)
		template |= PG_W;

	if (prot & VM_PROT_WRITE) {
		/*
		 * On 88110, do not mark writable mappings as dirty unless we
		 * know the page is dirty, or we are using the kernel pmap.
		 */
		if (CPU_IS88110 && pmap != kernel_pmap &&
		    pg != NULL && (pvl->pv_flags & PG_M) == 0)
			template |= PG_U;
		else
			template |= PG_M_U;
	} else if (prot & VM_PROT_ALL)
		template |= PG_U;

	/*
	 * If outside physical memory, disable cache on this (I/O) page.
	 */
	if ((unsigned long)pa >= last_addr)
		template |= CACHE_INH;

	/*
	 * Invalidate pte temporarily to avoid being written
	 * back the modified bit and/or the reference bit by
	 * any other cpu.
	 */
	template |= invalidate_pte(pte) & PG_M_U;
	*pte = template | pa;
	flush_atc_entry(pmap, va);
#ifdef PMAPDEBUG
	if (pmap_debug & CD_ENT)
		printf("pmap_enter: new pte %p\n", *pte);
#endif

	/*
	 * Cache attribute flags
	 */
	if (pvl != NULL) {
		if (flags & VM_PROT_WRITE) {
			if (CPU_IS88110 && pmap != kernel_pmap)
				pvl->pv_flags |= PG_U;
			else
				pvl->pv_flags |= PG_M_U;
		} else if (flags & VM_PROT_ALL)
			pvl->pv_flags |= PG_U;
	}

	PMAP_UNLOCK(pmap);
	splx(spl);

	return 0;
}

/*
 * Routine:	pmap_unwire
 *
 * Function:	Change the wiring attributes for a map/virtual-address pair.
 *
 * Parameters:
 *	pmap	pointer to pmap structure
 *	v	virtual address of page to be unwired
 *
 * Calls:
 *	pmap_pte
 *
 * Special Assumptions:
 *	The mapping must already exist in the pmap.
 */
void
pmap_unwire(pmap_t pmap, vaddr_t v)
{
	pt_entry_t *pte;
	int spl;

	spl = splvm();
	PMAP_LOCK(pmap);

	if ((pte = pmap_pte(pmap, v)) == NULL)
		panic("pmap_unwire: pte missing");

	if (pmap_pte_w(pte)) {
		/* unwired mapping */
		pmap->pm_stats.wired_count--;
		*pte &= ~PG_W;
	}

	PMAP_UNLOCK(pmap);
	splx(spl);
}

/*
 * Routine:	PMAP_EXTRACT
 *
 * Function:
 *	Extract the physical page address associoated
 *	with the given map/virtual_address pair.
 *
 * Parameters:
 *	pmap		pointer to pmap structure
 *	va		virtual address
 *	pap		storage for result.
 *
 * Calls:
 *	PMAP_LOCK, PMAP_UNLOCK
 *	pmap_pte
 *
 * The routine calls pmap_pte to get a (virtual) pointer to
 * the page table entry (PTE) associated with the given virtual
 * address. If the page table does not exist, or if the PTE is not valid,
 * then 0 address is returned. Otherwise, the physical page address from
 * the PTE is returned.
 */
boolean_t
pmap_extract(pmap_t pmap, vaddr_t va, paddr_t *pap)
{
	pt_entry_t *pte;
	paddr_t pa;
	int spl;
	boolean_t rv = FALSE;

#ifdef DIAGNOSTIC
	if (pmap == NULL)
		panic("pmap_extract: pmap is NULL");
#endif

#ifdef M88100
	/*
	 * 88100-based designs have two hardwired BATC entries which map
	 * the upper 1MB 1:1 in supervisor space.
	 */
	if (CPU_IS88100) {
		if (va >= BATC8_VA && pmap == kernel_pmap) {
			*pap = va;
			return (TRUE);
		}
	}
#endif

	spl = splvm();
	PMAP_LOCK(pmap);

	pte = pmap_pte(pmap, va);
	if (pte != NULL && PDT_VALID(pte)) {
		rv = TRUE;
		if (pap != NULL) {
			pa = ptoa(PG_PFNUM(*pte));
			pa |= (va & PAGE_MASK); /* offset within page */
			*pap = pa;
		}
	}

	PMAP_UNLOCK(pmap);
	splx(spl);
	return rv;
}

/*
 * Routine:	PMAP_COLLECT
 *
 * Runction:
 *	Garbage collects the physical map system for pages which are
 *	no longer used. there may well be pages which are not
 *	referenced, but others may be collected as well.
 *	Called by the pageout daemon when pages are scarce.
 *
 * Parameters:
 *	pmap		pointer to pmap structure
 *
 * Calls:
 *	pmap_pte
 *	pmap_remove_range
 *	uvm_km_free
 *
 *	The intent of this routine is to release memory pages being used
 * by translation tables. They can be release only if they contain no
 * valid mappings, and their parent table entry has been invalidated.
 *
 *	The routine sequences through the entries user address space,
 * inspecting page-sized groups of page tables for wired entries. If
 * a full page of tables has no wired enties, any otherwise valid
 * entries are invalidated (via pmap_remove_range). Then, the segment
 * table entries corresponding to this group of page tables are
 * invalidated. Finally, uvm_km_free is called to return the page to the
 * system.
 *
 *	If all entries in a segment table are invalidated, it too can
 * be returned to the system.
 */
void
pmap_collect(pmap_t pmap)
{
	u_int sdt;		/* outer loop index */
	vaddr_t sdt_va;
	sdt_entry_t *sdtp;	/* ptr to index into segment table */
	pt_entry_t *gdttbl;	/* ptr to first entry in a page table */
	pt_entry_t *gdttblend;	/* ptr to byte after last entry in
				   table group */
	pt_entry_t *gdtp;	/* ptr to index into a page table */
	boolean_t found_gdt_wired; /* flag indicating a wired page exists
				   in a page table's address range */
	int spl;

#ifdef PMAPDEBUG
	if (pmap_debug & CD_COL)
		printf ("pmap_collect(%p)\n", pmap);
#endif

	spl = splvm();
	PMAP_LOCK(pmap);

	sdtp = pmap->pm_stab; /* addr of segment table */

	/* segment table loop */
	for (sdt = VM_MIN_ADDRESS >> SDT_SHIFT;
	    sdt <= VM_MAX_ADDRESS >> SDT_SHIFT; sdt++, sdtp++) {
		sdt_va = sdt << SDT_SHIFT;
		gdttbl = pmap_pte(pmap, sdt_va);
		if (gdttbl == NULL)
			continue; /* no maps in this range */

		gdttblend = gdttbl + PDT_ENTRIES;

		/* scan page maps for wired pages */
		found_gdt_wired = FALSE;
		for (gdtp = gdttbl; gdtp < gdttblend; gdtp++) {
			if (pmap_pte_w(gdtp)) {
				found_gdt_wired = TRUE;
				break;
			}
		}

		if (found_gdt_wired)
			continue; /* can't free this range */

		/* invalidate all maps in this range */
		pmap_remove_range(pmap, sdt_va, sdt_va + (1 << SDT_SHIFT));

		/*
		 * we can safely deallocate the page map(s)
		 */
		*((sdt_entry_t *) sdtp) = 0;
		*((sdt_entry_t *)(sdtp + SDT_ENTRIES)) = 0;

		/*
		 * we have to unlock before freeing the table, since
		 * uvm_km_free will invoke another pmap routine
		 */
		PMAP_UNLOCK(pmap);
		uvm_km_free(kernel_map, (vaddr_t)gdttbl, PAGE_SIZE);
		PMAP_LOCK(pmap);
	}

	PMAP_UNLOCK(pmap);
	splx(spl);

#ifdef PMAPDEBUG
	if (pmap_debug & CD_COL)
		printf("pmap_collect(%p) done\n", pmap);
#endif
}

/*
 * Routine:	PMAP_ACTIVATE
 *
 * Function:
 * 	Binds the pmap associated to the process to the current processor.
 *
 * Parameters:
 * 	p	pointer to proc structure
 *
 * Notes:
 *	If the specified pmap is not kernel_pmap, this routine stores its
 *	apr template into UAPR (user area pointer register) in the
 *	CMMUs connected to the specified CPU.
 *
 *	Then it flushes the TLBs mapping user virtual space, in the CMMUs
 *	connected to the specified CPU.
 */
void
pmap_activate(struct proc *p)
{
	pmap_t pmap = vm_map_pmap(&p->p_vmspace->vm_map);
	struct cpu_info *ci = curcpu();

#ifdef PMAPDEBUG
	if (pmap_debug & CD_ACTIVATE)
		printf("pmap_activate(%p) pmap %p\n", p, pmap);
#endif

	if (pmap == kernel_pmap) {
		ci->ci_curpmap = NULL;
	} else {
		if (pmap != ci->ci_curpmap) {
			cmmu_set_uapr(pmap->pm_apr);
			cmmu_flush_tlb(ci->ci_cpuid, FALSE, 0, -1);
			ci->ci_curpmap = pmap;
		}
	}
}

/*
 * Routine:	PMAP_DEACTIVATE
 *
 * Function:
 *	Unbinds the pmap associated to the process from the current processor.
 *
 * Parameters:
 *	p		pointer to proc structure
 */
void
pmap_deactivate(struct proc *p)
{
	struct cpu_info *ci = curcpu();

	ci->ci_curpmap = NULL;
}

/*
 * Routine:	PMAP_COPY_PAGE
 *
 * Function:
 *	Copies the specified pages.
 *
 * Parameters:
 *	src	PA of source page
 *	dst	PA of destination page
 *
 * Extern/Global:
 *	phys_map_vaddr
 *
 * Special Assumptions:
 *	no locking required
 *
 * This routine maps the physical pages at the 'phys_map' virtual
 * addresses set up in pmap_bootstrap. It flushes the TLB to make the
 * new mappings effective, and performs the copy.
 */
void
pmap_copy_page(struct vm_page *srcpg, struct vm_page *dstpg)
{
	paddr_t src = VM_PAGE_TO_PHYS(srcpg);
	paddr_t dst = VM_PAGE_TO_PHYS(dstpg);
	vaddr_t dstva, srcva;
	int spl;
	pt_entry_t *dstpte, *srcpte;
	int cpu = cpu_number();
	extern void copypage(vaddr_t, vaddr_t);

#ifdef PMAPDEBUG
	if (pmap_debug & CD_COPY)
		printf("pmap_copy_page(%p,%p) pa %p %p\n",
		    srcpg, dstpg, src, dst);
#endif

	dstva = (vaddr_t)(phys_map_vaddr + 2 * (cpu << PAGE_SHIFT));
	srcva = dstva + PAGE_SIZE;
	dstpte = pmap_pte(kernel_pmap, dstva);
	srcpte = pmap_pte(kernel_pmap, srcva);

	spl = splvm();

	*dstpte = m88k_protection(VM_PROT_READ | VM_PROT_WRITE) |
	    PG_M /* 88110 */ | PG_U | PG_V | dst;
	*srcpte = m88k_protection(VM_PROT_READ) |
	    PG_U | PG_V | src;

	/*
	 * We don't need the flush_atc_entry() dance, as these pages are
	 * bound to only one cpu.
	 */
	cmmu_flush_tlb(cpu, TRUE, dstva, 2);
	cmmu_flush_cache(cpu, src, PAGE_SIZE);
	copypage(srcva, dstva);

	splx(spl);
}

/*
 * Routine:	PMAP_CHANGEBIT
 *
 * Function:
 *	Update the pte bits on the specified physical page.
 *
 * Parameters:
 *	pg	physical page
 *	set	bits to set
 *	mask	bits to mask
 *
 * Extern/Global:
 *	pv_lists
 *
 * Calls:
 *	pmap_pte
 *
 * The pte bits corresponding to the page's frame index will be changed as
 * requested. The PV list will be traversed.
 * For each pmap/va the hardware the necessary bits in the page descriptor
 * table entry will be altered as well if necessary. If any bits were changed,
 * a TLB flush will be performed.
 */
void
pmap_changebit(struct vm_page *pg, int set, int mask)
{
	pv_entry_t pvl, pvep;
	pt_entry_t *pte, npte, opte;
	pmap_t pmap;
	int spl;
	vaddr_t va;

#ifdef PMAPDEBUG
	if (pmap_debug & CD_CBIT)
		printf("pmap_changebit(%p, %x, %x)\n", pg, set, mask);
#endif

	spl = splvm();

#if defined(MULTIPROCESSOR) && 0
changebit_Retry:
#endif
	pvl = pg_to_pvh(pg);

	/*
	 * Clear saved attributes (modify, reference)
	 */
	pvl->pv_flags &= mask;

	if (pvl->pv_pmap == NULL)
		goto done;

	/* for each listed pmap, update the affected bits */
	for (pvep = pvl; pvep != NULL; pvep = pvep->pv_next) {
		pmap = pvep->pv_pmap;
#if defined(MULTIPROCESSOR) && 0
		if (!__cpu_simple_lock_try(&pmap->pm_lock)) {
			goto changebit_Retry;
		}
#endif
		va = pvep->pv_va;
		pte = pmap_pte(pmap, va);

		/*
		 * Check for existing and valid pte
		 */
		if (pte == NULL || !PDT_VALID(pte)) {
			goto next;	 /* no page mapping */
		}
#ifdef PMAPDEBUG
		if (ptoa(PG_PFNUM(*pte)) != VM_PAGE_TO_PHYS(pg))
			panic("pmap_changebit: pte %x in pmap %p doesn't point to page %p %lx",
			    *pte, pmap, pg, VM_PAGE_TO_PHYS(pg));
#endif

		/*
		 * Update bits
		 */
		opte = *pte;
		npte = (opte | set) & mask;

		/*
		 * Flush TLB of which cpus using pmap.
		 *
		 * Invalidate pte temporarily to avoid the modified bit
		 * and/or the reference being written back by any other cpu.
		 */
		if (npte != opte) {
			invalidate_pte(pte);
			*pte = npte;
			flush_atc_entry(pmap, va);
		}
next:
		PMAP_UNLOCK(pmap);
	}

done:
	splx(spl);
}

/*
 * Routine:	PMAP_TESTBIT
 *
 * Function:
 *	Test the modified/referenced bits of a physical page.
 *
 * Parameters:
 *	pg	physical page
 *	bit	bit to test
 *
 * Extern/Global:
 *	pv lists
 *
 * Calls:
 *	pmap_pte
 *
 * If the attribute list for the given page has the bit, this routine
 * returns TRUE.
 *
 * Otherwise, this routine walks the PV list corresponding to the
 * given page. For each pmap/va pair, the page descriptor table entry is
 * examined. If the selected bit is found on, the function returns TRUE
 * immediately (doesn't need to walk remainder of list), and updates the
 * attribute list.
 */
boolean_t
pmap_testbit(struct vm_page *pg, int bit)
{
	pv_entry_t pvl, pvep;
	pt_entry_t *pte;
	pmap_t pmap;
	int spl;

#ifdef PMAPDEBUG
	if (pmap_debug & CD_CBIT)
		printf("pmap_testbit(%p, %x): ", pg, bit);
#endif

	spl = splvm();

#if defined(MULTIPROCESSOR) && 0
testbit_Retry:
#endif
	pvl = pg_to_pvh(pg);

	if (pvl->pv_flags & bit) {
		/* we've already cached this flag for this page,
		   no use looking further... */
#ifdef PMAPDEBUG
		if (pmap_debug & CD_TBIT)
			printf("cached\n");
#endif
		splx(spl);
		return (TRUE);
	}

	if (pvl->pv_pmap == NULL)
		goto done;

	/* for each listed pmap, check modified bit for given page */
	for (pvep = pvl; pvep != NULL; pvep = pvep->pv_next) {
		pmap = pvep->pv_pmap;
#if defined(MULTIPROCESSOR) && 0
		if (!__cpu_simple_lock_try(&pmap->pm_lock)) {
			goto testbit_Retry;
		}
#endif

		pte = pmap_pte(pmap, pvep->pv_va);
		if (pte == NULL || !PDT_VALID(pte)) {
			goto next;
		}

#ifdef PMAPDEBUG
		if (ptoa(PG_PFNUM(*pte)) != VM_PAGE_TO_PHYS(pg))
			panic("pmap_testbit: pte %x in pmap %p %d doesn't point to page %p %lx",
			    *pte, pmap, pmap == kernel_pmap ? 1 : 0, pg, VM_PAGE_TO_PHYS(pg));
#endif

		if ((*pte & bit) != 0) {
			PMAP_UNLOCK(pmap);
			pvl->pv_flags |= bit;
#ifdef PMAPDEBUG
			if ((pmap_debug & (CD_TBIT | CD_FULL)) == (CD_TBIT | CD_FULL))
				printf("found in pte %p @%p\n",
				    *pte, pte);
			else if (pmap_debug & CD_TBIT)
				printf("found\n");
#endif
			splx(spl);
			return (TRUE);
		}
next:
		PMAP_UNLOCK(pmap);
	}

done:
#ifdef PMAPDEBUG
		if (pmap_debug & CD_TBIT)
			printf("not found\n");
#endif
	splx(spl);
	return (FALSE);
}

/*
 * Routine:	PMAP_UNSETBIT
 *
 * Function:
 *	Clears a pte bit and returns its previous state, for the
 *	specified physical page.
 *	This is an optimized version of:
 *		rv = pmap_testbit(pg, bit);
 *		pmap_changebit(pg, 0, ~bit);
 *		return rv;
 */
boolean_t
pmap_unsetbit(struct vm_page *pg, int bit)
{
	boolean_t rv = FALSE;
	pv_entry_t pvl, pvep;
	pt_entry_t *pte, opte;
	pmap_t pmap;
	int spl;
	vaddr_t va;

#ifdef PMAPDEBUG
	if (pmap_debug & CD_USBIT)
		printf("pmap_unsetbit(%p, %x): ", pg, bit);
#endif

	spl = splvm();

#if defined(MULTIPROCESSOR) && 0
unsetbit_Retry:
#endif
	pvl = pg_to_pvh(pg);

	/*
	 * Clear saved attributes
	 */
	if (pvl->pv_flags & bit) {
		pvl->pv_flags ^= bit;
		rv = TRUE;
	}

	if (pvl->pv_pmap == NULL)
		goto done;

	/* for each listed pmap, update the specified bit */
	for (pvep = pvl; pvep != NULL; pvep = pvep->pv_next) {
		pmap = pvep->pv_pmap;
#if defined(MULTIPROCESSOR) && 0
		if (!__cpu_simple_lock_try(&pmap->pm_lock)) {
			goto unsetbit_Retry;
		}
#endif
		va = pvep->pv_va;
		pte = pmap_pte(pmap, va);

		/*
		 * Check for existing and valid pte
		 */
		if (pte == NULL || !PDT_VALID(pte)) {
			goto next;	 /* no page mapping */
		}
#ifdef PMAPDEBUG
		if (ptoa(PG_PFNUM(*pte)) != VM_PAGE_TO_PHYS(pg))
			panic("pmap_unsetbit: pte %x in pmap %p doesn't point to page %p %lx",
			    *pte, pmap, pg, VM_PAGE_TO_PHYS(pg));
#endif

		/*
		 * Update bits
		 */
		opte = *pte;
		if (opte & bit) {
			/*
			 * Flush TLB of which cpus using pmap.
			 *
			 * Invalidate pte temporarily to avoid the specified
			 * bit being written back by any other cpu.
			 */
			invalidate_pte(pte);
			*pte = opte ^ bit;
			flush_atc_entry(pmap, va);
			rv = TRUE;
		}
next:
		PMAP_UNLOCK(pmap);
	}
	splx(spl);

done:
#ifdef PMAPDEBUG
	if (pmap_debug & CD_USBIT)
		printf(rv ? "TRUE\n" : "FALSE\n");
#endif
	return (rv);
}

/*
 * Routine:	PMAP_IS_MODIFIED
 *
 * Function:
 *	Return whether or not the specified physical page is modified
 *	by any physical maps.
 */
boolean_t
pmap_is_modified(struct vm_page *pg)
{
#ifdef M88110
	/*
	 * Since on 88110 PG_M bit tracking is done in software, we can
	 * trust the page flags without having to walk the individual
	 * ptes in case the page flags are behind actual usage.
	 */
	if (CPU_IS88110) {
		pv_entry_t pvl;
		boolean_t rc = FALSE;

		pvl = pg_to_pvh(pg);
		if (pvl->pv_flags & PG_M)
			rc = TRUE;
#ifdef PMAPDEBUG
		if (pmap_debug & CD_TBIT)
			printf("pmap_is_modified(%p) -> %x\n", pg, rc);
#endif
		return (rc);
	}
#endif

	return pmap_testbit(pg, PG_M);
}

/*
 * Routine:	PMAP_IS_REFERENCED
 *
 * Function:
 *	Return whether or not the specified physical page is referenced by
 *	any physical maps.
 */
boolean_t
pmap_is_referenced(struct vm_page *pg)
{
	return pmap_testbit(pg, PG_U);
}

/*
 * Routine:	PMAP_PAGE_PROTECT
 *
 * Calls:
 *	pmap_changebit
 *	pmap_remove_page
 *
 *	Lower the permission for all mappings to a given page.
 */
void
pmap_page_protect(struct vm_page *pg, vm_prot_t prot)
{
	if ((prot & VM_PROT_READ) == VM_PROT_NONE)
		pmap_remove_page(pg);
	else if ((prot & VM_PROT_WRITE) == VM_PROT_NONE)
		pmap_changebit(pg, PG_RO, ~0);
}

void
pmap_virtual_space(vaddr_t *startp, vaddr_t *endp)
{
	*startp = virtual_avail;
	*endp = virtual_end;
}

void
pmap_kenter_pa(vaddr_t va, paddr_t pa, vm_prot_t prot)
{
	int spl;
	pt_entry_t template, *pte;

#ifdef PMAPDEBUG
	if (pmap_debug & CD_ENT)
		printf ("pmap_kenter_pa(%p, %p, %x)\n", va, pa, prot);
#endif

	spl = splvm();
	PMAP_LOCK(kernel_pmap);

	template = m88k_protection(prot);
#ifdef M88110
	if (CPU_IS88110 && m88k_protection(prot) != PG_RO)
		template |= PG_M;
#endif

	/*
	 * Expand pmap to include this pte.
	 */
	while ((pte = pmap_pte(kernel_pmap, va)) == NULL)
		pmap_expand_kmap(va, VM_PROT_READ | VM_PROT_WRITE, 0);

	/*
	 * And count the mapping.
	 */
	kernel_pmap->pm_stats.resident_count++;
	kernel_pmap->pm_stats.wired_count++;

	invalidate_pte(pte);

	/*
	 * If outside physical memory, disable cache on this (I/O) page.
	 */
	if ((unsigned long)pa >= last_addr)
		template |= CACHE_INH | PG_V | PG_W;
	else
		template |= PG_V | PG_W;
	*pte = template | pa;
	flush_atc_entry(kernel_pmap, va);

	PMAP_UNLOCK(kernel_pmap);
	splx(spl);
}

void
pmap_kremove(vaddr_t va, vsize_t len)
{
	int spl;
	vaddr_t e, eseg;

#ifdef PMAPDEBUG
	if (pmap_debug & CD_RM)
		printf("pmap_kremove(%p, %x)\n", va, len);
#endif

	spl = splvm();
	PMAP_LOCK(kernel_pmap);

	e = va + len;
	while (va != e) {
		sdt_entry_t *sdt;
		pt_entry_t *pte;

		eseg = (va & SDT_MASK) + (1 << SDT_SHIFT);
		if (eseg > e || eseg == 0)
			eseg = e;

		sdt = SDTENT(kernel_pmap, va);

		/* If no segment table, skip a whole segment */
		if (!SDT_VALID(sdt))
			va = eseg;
		else {
			while (va != eseg) {
				pte = sdt_pte(sdt, va);
				if (pte != NULL && PDT_VALID(pte)) {
					/* Update the counts */
					kernel_pmap->pm_stats.resident_count--;
					kernel_pmap->pm_stats.wired_count--;

					invalidate_pte(pte);
					flush_atc_entry(kernel_pmap, va);
				}
				va += PAGE_SIZE;
			}
		}
	}
	PMAP_UNLOCK(kernel_pmap);
	splx(spl);
}

void
pmap_proc_iflush(struct proc *p, vaddr_t va, vsize_t len)
{
	pmap_t pmap = vm_map_pmap(&p->p_vmspace->vm_map);
	paddr_t pa;
	vsize_t count;
	struct cpu_info *ci;

	while (len != 0) {
		count = min(len, PAGE_SIZE - (va & PAGE_MASK));
		if (pmap_extract(pmap, va, &pa)) {
#ifdef MULTIPROCESSOR
			CPU_INFO_ITERATOR cpu;

			CPU_INFO_FOREACH(cpu, ci)
#else
			ci = curcpu();
#endif
			/* CPU_INFO_FOREACH(cpu, ci) */ {
				cmmu_flush_inst_cache(ci->ci_cpuid, pa, count);
			}
		}
		va += count;
		len -= count;
	}
}

#ifdef M88110
#include <machine/m88110.h>
int
pmap_set_modify(pmap_t pmap, vaddr_t va)
{
	pt_entry_t *pte;
	paddr_t pa;
	vm_page_t pg;
	pv_entry_t pvl;

	pte = pmap_pte(pmap, va);
#ifdef DEBUG
	if (pte == NULL)
		panic("NULL pte on write fault??");
#endif

	/* Not a first write to a writable page */
	if ((*pte & (PG_M | PG_RO)) != 0)
		return (FALSE);

	/* Mark the page as dirty */
	*pte |= PG_M;
	pa = *pte & PG_FRAME;
	pg = PHYS_TO_VM_PAGE(pa);
#ifdef DIAGNOSTIC
	if (pg == NULL)
		panic("Write fault to unmanaged page %p", pa);
#endif

	pvl = pg_to_pvh(pg);
	pvl->pv_flags |= PG_M_U;

	if (pmap == kernel_pmap)
		set_dcmd(CMMU_DCMD_INV_SATC);
	else
		set_dcmd(CMMU_DCMD_INV_UATC);

	return (TRUE);
}
#endif
