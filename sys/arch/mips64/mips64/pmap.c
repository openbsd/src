/*	$OpenBSD: pmap.c,v 1.7 2004/09/09 22:11:38 pefo Exp $	*/

/*
 * Copyright (c) 2001-2004 Opsycon AB  (www.opsycon.se / www.opsycon.com)
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	XXX This code needs some major rewriting.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/user.h>
#include <sys/buf.h>
#include <sys/pool.h>
#ifdef SYSVSHM
#include <sys/shm.h>
#endif

#include <uvm/uvm.h>

#include <machine/pte.h>
#include <machine/cpu.h>
#include <machine/autoconf.h>
#include <machine/memconf.h>
#include <mips64/archtype.h>

extern void mem_zero_page (vaddr_t);

typedef struct pv_entry {
	struct pv_entry	*pv_next;	/* next pv_entry */
	struct pmap	*pv_pmap;	/* pmap where mapping lies */
	vaddr_t		pv_va;		/* virtual address for mapping */
	int		pv_flags;	/* Some flags for the mapping */
} *pv_entry_t;
#define	PV_UNCACHED	0x0001		/* Page is mapped unchached */
#define	PV_CACHED	0x0002		/* Page has been cached */

/*
 * Local pte bits used only here
 */
#define PG_RO		0x40000000

pv_entry_t pv_table;			/* array of entries, one per page */

struct pool pmap_pmap_pool;
struct pool pmap_pv_pool;

void    *pmap_pv_page_alloc(u_long, int, int);
void    pmap_pv_page_free(void *, u_long, int);

#define pmap_pv_alloc()		(pv_entry_t)pool_get(&pmap_pv_pool, PR_NOWAIT)
#define pmap_pv_free(pv)	pool_put(&pmap_pv_pool, (pv))

#ifndef PMAP_PV_LOWAT
#define PMAP_PV_LOWAT   16
#endif
int	pmap_pv_lowat = PMAP_PV_LOWAT;

void pmap_pinit __P((struct pmap *pmap));
void pmap_release __P((pmap_t pmap));
boolean_t pmap_physpage_alloc(paddr_t *);
void    pmap_physpage_free(paddr_t);

#ifdef DIAGNOSTIC
struct {
	int kernel;	/* entering kernel mapping */
	int user;	/* entering user mapping */
	int ptpneeded;	/* needed to allocate a PT page */
	int pwchange;	/* no mapping change, just wiring or protection */
	int wchange;	/* no mapping change, just wiring */
	int mchange;	/* was mapped but mapping to different page */
	int managed;	/* a managed page */
	int firstpv;	/* first mapping for this PA */
	int secondpv;	/* second mapping for this PA */
	int ci;		/* cache inhibited */
	int unmanaged;	/* not a managed page */
	int flushes;	/* cache flushes */
	int cachehit;	/* new entry forced valid entry out */
} enter_stats;
struct {
	int calls;
	int removes;
	int flushes;
	int pidflushes;	/* HW pid stolen */
	int pvfirst;
	int pvsearch;
} remove_stats;

#define stat_count(what)	(what)++

#define PDB_FOLLOW	0x0001
#define PDB_INIT	0x0002
#define PDB_ENTER	0x0004
#define PDB_REMOVE	0x0008
#define PDB_CREATE	0x0010
#define PDB_PTPAGE	0x0020
#define PDB_PVENTRY	0x0040
#define PDB_BITS	0x0080
#define PDB_COLLECT	0x0100
#define PDB_PROTECT	0x0200
#define PDB_TLBPID	0x0400
#define PDB_PARANOIA	0x2000
#define PDB_WIRING	0x4000
#define PDB_PVDUMP	0x8000

int pmapdebugflag = 0x0;
#define pmapdebug pmapdebugflag

#else

#define stat_count(what)
#define pmapdebug (0)

#endif /* DIAGNOSTIC */


struct pmap	kernel_pmap_store;

psize_t	mem_size;	/* memory size in bytes */
vaddr_t	virtual_start;  /* VA of first avail page (after kernel bss)*/
vaddr_t	virtual_end;	/* VA of last avail page (end of kernel AS) */

struct segtab	*free_segtab;		/* free list kept locally */
u_int		tlbpid_gen = 1;		/* TLB PID generation count */
int		tlbpid_cnt = 2;		/* next available TLB PID */

pt_entry_t	*Sysmap;		/* kernel pte table */
u_int		Sysmapsize;		/* number of pte's in Sysmap */


/*
 *	Bootstrap the system enough to run with virtual memory.
 */
void
pmap_bootstrap()
{
	int i;
	pt_entry_t *spte;
	int n;


	/*
	 * Create a mapping table for kernel virtual memory. This
	 * table is a linear table in contrast to the user process
	 * mapping tables which are built with segment/page tables.
	 * Create at least 256MB of map even if physmem is smaller.
	 */
	if (physmem < 65536)
		Sysmapsize = 65536;
	else
		Sysmapsize = physmem;

	virtual_start = VM_MIN_KERNEL_ADDRESS;
	virtual_end = VM_MIN_KERNEL_ADDRESS + Sysmapsize * NBPG;

	Sysmap = (pt_entry_t *)uvm_pageboot_alloc(sizeof(pt_entry_t) * Sysmapsize);

	/*
	 * Allocate memory for pv_table.
	 * This will allocate more entries than we really need.
	 * We could do this in pmap_init when we know the actual
	 * phys_start and phys_end but its better to use kseg0 addresses
	 * rather than kernel virtual addresses mapped through the TLB.
	 */
	i = 0;
	for( n = 0; n < MAXMEMSEGS; n++) {
		i += mem_layout[n].mem_last_page - mem_layout[n].mem_first_page + 1;
	}
	pv_table = (struct pv_entry *)uvm_pageboot_alloc(sizeof(struct pv_entry) * i);

	pool_init(&pmap_pmap_pool, sizeof(struct pmap), 0, 0, 0,"pmappl", NULL);
	pool_init(&pmap_pv_pool, sizeof(struct pv_entry), 0, 0, 0,"pvpl", NULL);

	/* XXX need to decide how to set cnt.v_page_size */

	simple_lock_init(&pmap_kernel()->pm_lock);
	pmap_kernel()->pm_count = 1;

	/*
	 * The R4?00 stores only one copy of the Global bit in the
	 * translation lookaside buffer for each 2 page entry.
	 * Thus invalid entrys must have the Global bit set so
	 * when Entry LO and Entry HI G bits are anded together
	 * they will produce a global bit to store in the tlb.
	 */
	for(i = 0, spte = Sysmap; i < Sysmapsize; i++, spte++)
		spte->pt_entry = PG_G;
}

/*
 *  Page steal allocator used during bootup.
 */
vaddr_t
pmap_steal_memory(size, vstartp, vendp)
	vsize_t size;
	vaddr_t *vstartp, *vendp;
{
	int i, j, x;
	int npgs;
	vaddr_t va;
	paddr_t pa;

	if (uvm.page_init_done) {
		panic("pmap_steal_memory: to late, vm is running!");
	}

	size = round_page(size);
	npgs = atop(size);
	va = 0;

	for(i = 0; i < vm_nphysseg && va == 0; i++) {
		if (vm_physmem[i].avail_start != vm_physmem[i].start ||
		    vm_physmem[i].avail_start >= vm_physmem[i].avail_end) {
			continue;
		}

		if ((vm_physmem[i].avail_end - vm_physmem[i].avail_start) < npgs) {
			continue;
		}

		pa = ptoa(vm_physmem[i].avail_start);
		vm_physmem[i].avail_start += npgs;
		vm_physmem[i].start += npgs;

		if (vm_physmem[i].avail_start == vm_physmem[i].end) {
			if (vm_nphysseg == 1) {
				panic("pmap_steal_memory: out of memory!");
			}

			vm_nphysseg--;
			for(j = i; j < vm_nphysseg; x++) {
				vm_physmem[x] = vm_physmem[x + 1];
			}
		}
		if (vstartp) {
			*vstartp = round_page(virtual_start);
		}
		if (vendp) {
			*vendp = virtual_end;
		}
		va = PHYS_TO_KSEG0(pa);
		memset((caddr_t)va, 0, size);
	}

	if (va == 0) {
		panic("pmap_steal_memory: no memory to steal");
	}

	return(va);
}

/*
 *	Initialize the pmap module.
 *	Called by vm_init, to initialize any structures that the pmap
 *	system needs to map virtual memory.
 */
void
pmap_init()
{
	vsize_t s;
	int bank;
	pv_entry_t pv;

	if (pmapdebug & (PDB_FOLLOW|PDB_INIT)) {
		printf("pmap_init()\n");
	}

	pv = pv_table;
	for(bank = 0; bank < vm_nphysseg; bank++) {
		s = vm_physmem[bank].end - vm_physmem[bank].start;
		vm_physmem[bank].pmseg.pvent = pv;
		pv += s;
	}

#if 0 /* too early */
	pool_setlowat(&pmap_pv_pool, pmap_pv_lowat);
#endif
}

inline struct pv_entry *pa_to_pvh __P((paddr_t));
inline struct pv_entry *
pa_to_pvh(pa)
    paddr_t pa;
{
	int i, p;

	i = vm_physseg_find(atop((pa)), &p);
	return(&vm_physmem[i].pmseg.pvent[p]);
}

/*
 *	Create and return a physical map.
 *
 *	If the size specified for the map
 *	is zero, the map is an actual physical
 *	map, and may be referenced by the
 *	hardware.
 *
 *	If the size specified is non-zero,
 *	the map will be used in software only, and
 *	is bounded by that size.
 */
pmap_t
pmap_create()
{
	pmap_t pmap;

	if (pmapdebug & (PDB_FOLLOW|PDB_CREATE)) {
		printf("pmap_create()\n");
	}

	pmap = pool_get(&pmap_pmap_pool, PR_WAITOK);
	bzero(pmap, sizeof(*pmap));
	pmap_pinit(pmap);
	return (pmap);
}

/*
 * Initialize a preallocated and zeroed pmap structure,
 * such as one in a vmspace structure.
 */
void
pmap_pinit(pmap)
	struct pmap *pmap;
{
	int i;
	int s;
extern struct vmspace vmspace0;
extern struct user *proc0paddr;

	simple_lock_init(&pmap->pm_lock);
	pmap->pm_count = 1;
	if (free_segtab) {
		s = splimp();
		pmap->pm_segtab = free_segtab;
		free_segtab = *(struct segtab **)free_segtab;
		pmap->pm_segtab->seg_tab[0] = NULL;
		splx(s);
	}
	else {
		struct segtab *stp;
		vm_page_t mem;
		pv_entry_t pv;

		do {
			mem = uvm_pagealloc(NULL, 0, NULL, UVM_PGA_USERESERVE|UVM_PGA_ZERO);
			if (mem == NULL) {
				/* XXX What else can we do?  Deadlocks?  */
				uvm_wait("ppinit");
			}
		} while (mem == NULL);

		pv = pa_to_pvh(VM_PAGE_TO_PHYS(mem));
		if (pv->pv_flags & PV_CACHED &&
		   ((pv->pv_va ^ PHYS_TO_KSEG0(VM_PAGE_TO_PHYS(mem))) & CpuCacheAliasMask) != 0) {
			Mips_SyncDCachePage(pv->pv_va);
		}
		pv->pv_va = PHYS_TO_KSEG0(VM_PAGE_TO_PHYS(mem));
		pv->pv_flags = PV_CACHED;

		pmap->pm_segtab = stp = (struct segtab *)
			(long)PHYS_TO_KSEG0(VM_PAGE_TO_PHYS(mem));

		i = NBPG / sizeof(struct segtab);
		s = splimp();
		while (--i != 0) {
			stp++;
			*(struct segtab **)stp = free_segtab;
			free_segtab = stp;
		}
		splx(s);
	}
	if (pmap == vmspace0.vm_map.pmap) {
		/*
		 * The initial process has already been allocated a TLBPID
		 * in mach_init().
		 */
		pmap->pm_tlbpid = 1;
		pmap->pm_tlbgen = tlbpid_gen;
		proc0paddr->u_pcb.pcb_segtab = (void *)pmap->pm_segtab;
	}
	else {
		pmap->pm_tlbpid = 0;
		pmap->pm_tlbgen = 0;
	}
}

/*
 *	Retire the given physical map from service.
 *	Should only be called if the map contains
 *	no valid mappings.
 */
void
pmap_destroy(pmap)
	pmap_t pmap;
{
	int count;

	if (pmapdebug & (PDB_FOLLOW|PDB_CREATE)) {
		printf("pmap_destroy(%x)\n", pmap);
	}
	if (pmap) {
		simple_lock(&pmap->pm_lock);
		count = --pmap->pm_count;
		simple_unlock(&pmap->pm_lock);
		if (count == 0) {
			pmap_release(pmap);
			pool_put(&pmap_pmap_pool, pmap);
		}
	}
}

/*
 * Release any resources held by the given physical map.
 * Called when a pmap initialized by pmap_pinit is being released.
 * Should only be called if the map contains no valid mappings.
 */
void
pmap_release(pmap)
	pmap_t pmap;
{

	if (pmapdebug & (PDB_FOLLOW|PDB_CREATE)) {
		printf("pmap_release(%x)\n", pmap);
	}

	if (pmap->pm_segtab) {
		pt_entry_t *pte;
		int i;
		int s;
#ifdef PARANIOA
		int j;
#endif

		for (i = 0; i < PMAP_SEGTABSIZE; i++) {
			/* get pointer to segment map */
			pte = pmap->pm_segtab->seg_tab[i];
			if (!pte)
				continue;
#ifdef PARANOIA
			for (j = 0; j < NPTEPG; j++) {
				if ((pte+j)->pt_entry)
					panic("pmap_release: segmap not empty");
			}
#endif
			Mips_HitInvalidateDCache((vaddr_t)pte, PAGE_SIZE);
			uvm_pagefree(PHYS_TO_VM_PAGE(KSEG0_TO_PHYS(pte)));
			pmap->pm_segtab->seg_tab[i] = NULL;
		}
		s = splimp();
		*(struct segtab **)pmap->pm_segtab = free_segtab;
		free_segtab = pmap->pm_segtab;
		splx(s);
		pmap->pm_segtab = NULL;
	}
}

/*
 *	Add a reference to the specified pmap.
 */
void
pmap_reference(pmap)
	pmap_t pmap;
{

	if (pmapdebug & PDB_FOLLOW) {
		printf("pmap_reference(%x)\n", pmap);
	}
	if (pmap) {
		simple_lock(&pmap->pm_lock);
		pmap->pm_count++;
		simple_unlock(&pmap->pm_lock);
	}
}

/*
 *      Make a new pmap (vmspace) active for the given process.
 */
void
pmap_activate(p)
	struct proc *p;
{
	pmap_t pmap = p->p_vmspace->vm_map.pmap;

	p->p_addr->u_pcb.pcb_segtab = pmap->pm_segtab;

	pmap_alloc_tlbpid(p);
}

/*
 *      Make a previously active pmap (vmspace) inactive.
 */
void
pmap_deactivate(p)
	struct proc *p;
{
	/* Empty */
}

/*
 *	Remove the given range of addresses from the specified map.
 *
 *	It is assumed that the start and end are properly
 *	rounded to the page size.
 */
void
pmap_remove(pmap, sva, eva)
	pmap_t pmap;
	vaddr_t sva, eva;
{
	vaddr_t nssva;
	pt_entry_t *pte;
	unsigned entry;

	stat_count(remove_stats.calls);

	if (pmapdebug & (PDB_FOLLOW|PDB_REMOVE|PDB_PROTECT)) {
		printf("pmap_remove(%x, %x, %x)\n", pmap, sva, eva);
	}
	if (pmap == NULL) {
		return;
	}

	if (pmap == pmap_kernel()) {
		pt_entry_t *pte;

		/* remove entries from kernel pmap */
#ifdef DIAGNOSTIC
		if (sva < VM_MIN_KERNEL_ADDRESS || eva > virtual_end)
			panic("pmap_remove: kva not in range");
#endif
		pte = kvtopte(sva);
		for(; sva < eva; sva += NBPG, pte++) {
			entry = pte->pt_entry;
			if (!(entry & PG_V))
				continue;
			pmap->pm_stats.resident_count--;
			pmap_remove_pv(pmap, sva, pfn_to_pad(entry));
			/*
			 * Flush the TLB for the given address.
			 */
			pte->pt_entry = PG_NV | PG_G; /* See above about G bit */
			tlb_flush_addr(sva);
			stat_count(remove_stats.flushes);
		}
		return;
	}

#ifdef DIAGNOSTIC
	if (eva > VM_MAXUSER_ADDRESS)
		panic("pmap_remove: uva not in range");
#endif
	while (sva < eva) {
		nssva = mips_trunc_seg(sva) + NBSEG;
		if (nssva == 0 || nssva > eva)
			nssva = eva;
		/*
		 * If VA belongs to an unallocated segment,
		 * skip to the next segment boundary.
		 */
		if (!(pte = pmap_segmap(pmap, sva))) {
			sva = nssva;
			continue;
		}
		/*
		 * Invalidate every valid mapping within this segment.
		 */
		pte += uvtopte(sva);
		for (; sva < nssva; sva += NBPG, pte++) {
			entry = pte->pt_entry;
			if (!(entry & PG_V))
				continue;
			pmap->pm_stats.resident_count--;
			if (!pfn_is_ext(entry)) {/* padr > 32 bits */
				pmap_remove_pv(pmap, sva, pfn_to_pad(entry));
			}
			pte->pt_entry = PG_NV;
			/*
			 * Flush the TLB for the given address.
			 */
			if (pmap->pm_tlbgen == tlbpid_gen) {
				tlb_flush_addr(sva | (pmap->pm_tlbpid <<
					VMTLB_PID_SHIFT));
				stat_count(remove_stats.flushes);
			}
		}
	}
}

/*
 *	pmap_page_protect:
 *
 *	Lower the permission for all mappings to a given page.
 */
void
pmap_page_protect(pg, prot)
	struct vm_page *pg;
	vm_prot_t prot;
{
	paddr_t pa = VM_PAGE_TO_PHYS(pg);
	pv_entry_t pv;
	vaddr_t va;
	int s, i;

	if ((pmapdebug & (PDB_FOLLOW|PDB_PROTECT)) ||
	    ((prot == VM_PROT_NONE) && (pmapdebug & PDB_REMOVE))) {
		printf("pmap_page_protect(%x, %x)\n", pa, prot);
	}
	if (!IS_VM_PHYSADDR(pa)) {
		return;
	}

	switch (prot) {
	case VM_PROT_READ|VM_PROT_WRITE:
	case VM_PROT_ALL:
		break;

	/* copy_on_write */
	case VM_PROT_READ:
	case VM_PROT_READ|VM_PROT_EXECUTE:
		pv = pa_to_pvh(pa);
		s = splimp();
		/*
		 * Loop over all current mappings setting/clearing as apropos.
		 */
		if (pv->pv_pmap != NULL) {
			for (; pv; pv = pv->pv_next) {
				va = pv->pv_va;

				/*
				 * XXX don't write protect pager mappings
				 */
				if (va >= uvm.pager_sva && va < uvm.pager_eva)
					continue;
				pmap_protect(pv->pv_pmap, va, va + PAGE_SIZE, prot);
			}
		}
		splx(s);
		break;

	/* remove_all */
	default:
		i = 0;
		pv = pa_to_pvh(pa);
		s = splimp();
		while (pv->pv_pmap != NULL && i < 10) {
			i++;
			pmap_remove(pv->pv_pmap, pv->pv_va,
				    pv->pv_va + PAGE_SIZE);
		}
		splx(s);
	}
}

/*
 *	Set the physical protection on the
 *	specified range of this map as requested.
 */
void
pmap_protect(pmap, sva, eva, prot)
	pmap_t pmap;
	vaddr_t sva, eva;
	vm_prot_t prot;
{
	vaddr_t nssva;
	pt_entry_t *pte;
	unsigned entry;
	u_int p;

	if (pmapdebug & (PDB_FOLLOW|PDB_PROTECT)) {
		printf("pmap_protect(%x, %x, %x, %x)\n", pmap, sva, eva, prot);
	}
	if (pmap == NULL)
		return;

	if ((prot & VM_PROT_READ) == VM_PROT_NONE) {
		pmap_remove(pmap, sva, eva);
		return;
	}

	p = (prot & VM_PROT_WRITE) ? PG_M : PG_RO;

	if (!pmap->pm_segtab) {
		/*
		 * Change entries in kernel pmap.
		 * This will trap if the page is writeable (in order to set
		 * the dirty bit) even if the dirty bit is already set. The
		 * optimization isn't worth the effort since this code isn't
		 * executed much. The common case is to make a user page
		 * read-only.
		 */
#ifdef DIAGNOSTIC
		if (sva < VM_MIN_KERNEL_ADDRESS || eva > virtual_end)
			panic("pmap_protect: kva not in range");
#endif
		pte = kvtopte(sva);
		for (; sva < eva; sva += NBPG, pte++) {
			entry = pte->pt_entry;
			if (!(entry & PG_V))
				continue;
			entry = (entry & ~(PG_M | PG_RO)) | p;
			pte->pt_entry = entry;
			/*
			 * Update the TLB if the given address is in the cache.
			 */
			tlb_update(sva, entry);
		}
		return;
	}

#ifdef DIAGNOSTIC
	if (eva > VM_MAXUSER_ADDRESS)
		panic("pmap_protect: uva not in range");
#endif
	while (sva < eva) {
		nssva = mips_trunc_seg(sva) + NBSEG;
		if (nssva == 0 || nssva > eva)
			nssva = eva;
		/*
		 * If VA belongs to an unallocated segment,
		 * skip to the next segment boundary.
		 */
		if (!(pte = pmap_segmap(pmap, sva))) {
			sva = nssva;
			continue;
		}
		/*
		 * Change protection on every valid mapping within this segment.
		 */
		pte += (sva >> PGSHIFT) & (NPTEPG - 1);
		for (; sva < nssva; sva += NBPG, pte++) {
			entry = pte->pt_entry;
			if (!(entry & PG_V))
				continue;
			entry = (entry & ~(PG_M | PG_RO)) | p;
			pte->pt_entry = entry;
			/*
			 * Update the TLB if the given address is in the cache.
			 */
			if (pmap->pm_tlbgen == tlbpid_gen)
				tlb_update(sva | (pmap->pm_tlbpid <<
					VMTLB_PID_SHIFT), entry);
		}
	}
}

/*
 *	Return RO protection of page.
 */
int
pmap_is_page_ro(pmap, va, entry)
	pmap_t	    pmap;
	vaddr_t va;
	int	entry;
{
	return(entry & PG_RO);
}

/*
 *	pmap_page_cache:
 *
 *	Change all mappings of a page to cached/uncached.
 */
void
pmap_page_cache(pa,mode)
	vaddr_t pa;
	int mode;
{
	pv_entry_t pv;
	pt_entry_t *pte;
	unsigned entry;
	unsigned newmode;
	int s;

	if (pmapdebug & (PDB_FOLLOW|PDB_ENTER)) {
		printf("pmap_page_uncache(%x)\n", pa);
	}
	if (!IS_VM_PHYSADDR(pa)) {
		return;
	}

	newmode = mode & PV_UNCACHED ? PG_UNCACHED : PG_CACHED;
	pv = pa_to_pvh(pa);
	s = splimp();
	while (pv) {
		pv->pv_flags = (pv->pv_flags & ~PV_UNCACHED) | mode;
		if (!pv->pv_pmap->pm_segtab) {
		/*
		 * Change entries in kernel pmap.
		 */
			pte = kvtopte(pv->pv_va);
			entry = pte->pt_entry;
			if (entry & PG_V) {
				entry = (entry & ~PG_CACHEMODE) | newmode;
				pte->pt_entry = entry;
				tlb_update(pv->pv_va, entry);
			}
		}
		else {
			if ((pte = pmap_segmap(pv->pv_pmap, pv->pv_va))) {
				pte += (pv->pv_va >> PGSHIFT) & (NPTEPG - 1);
				entry = pte->pt_entry;
				if (entry & PG_V) {
					entry = (entry & ~PG_CACHEMODE) | newmode;
					pte->pt_entry = entry;
					if (pv->pv_pmap->pm_tlbgen == tlbpid_gen)
						tlb_update(pv->pv_va | (pv->pv_pmap->pm_tlbpid <<
							VMTLB_PID_SHIFT), entry);
				}
			}
		}
		pv = pv->pv_next;
	}

	splx(s);
}

/*
 *	Insert the given physical page (p) at
 *	the specified virtual address (v) in the
 *	target physical map with the protection requested.
 *
 *	NB:  This is the only routine which MAY NOT lazy-evaluate
 *	or lose information.  That is, this routine must actually
 *	insert this page into the given map NOW.
 */
int
pmap_enter(pmap, va, pa, prot, stat)
	pmap_t pmap;
	vaddr_t va;
	paddr_t pa;
	vm_prot_t prot;
	int stat;
{
	pt_entry_t *pte;
	u_int npte;
	vm_page_t mem;
	int is_physaddr;

	if (pmapdebug & (PDB_FOLLOW|PDB_ENTER)) {
		printf("pmap_enter(%x, %x, %x, %x, %x)\n",
		       pmap, va, pa, prot, stat);
	}
#ifdef DIAGNOSTIC
	if (pmap == pmap_kernel()) {
		enter_stats.kernel++;
		if (va < VM_MIN_KERNEL_ADDRESS || va >= virtual_end)
			panic("pmap_enter: kva %p", va);
	} else {
		enter_stats.user++;
		if (va >= VM_MAXUSER_ADDRESS)
			panic("pmap_enter: uva %p", va);
	}
#endif

	is_physaddr = IS_VM_PHYSADDR(pa);

	if (is_physaddr) {
		pv_entry_t pv, npv;
		int s;

		if (!(prot & VM_PROT_WRITE)) {
			npte = PG_ROPAGE;
		} else {
			vm_page_t mem;

			mem = PHYS_TO_VM_PAGE(pa);
			if ((int)va < 0) {
				/*
				 * Don't bother to trap on kernel writes,
				 * just record page as dirty.
				 */
				npte = PG_RWPAGE;
#if 0 /*XXX*/
				mem->flags &= ~PG_CLEAN;
#endif
			} else {
				if (!(mem->flags & PG_CLEAN)) {
					npte = PG_RWPAGE;
				} else {
					npte = PG_CWPAGE;
				}
			}
		}

		stat_count(enter_stats.managed);
		/*
		 * Enter the pmap and virtual address into the
		 * physical to virtual map table.
		 */
		pv = pa_to_pvh(pa);
		s = splimp();

		if (pmapdebug & PDB_ENTER) {
			printf("pmap_enter: pv %x: was %x/%x/%x\n",
			       pv, pv->pv_va, pv->pv_pmap, pv->pv_next);
		}

		if (pv->pv_pmap == NULL) {
			/*
			 * No entries yet, use header as the first entry
			 */

			if (pmapdebug & PDB_PVENTRY) {
				printf("pmap_enter: first pv: pmap %x va %x pa %p\n",
					pmap, va, pa);
			}
			stat_count(enter_stats.firstpv);

			Mips_SyncDCachePage(pv->pv_va);

			pv->pv_va = va;
			pv->pv_flags = PV_CACHED;
			pv->pv_pmap = pmap;
			pv->pv_next = NULL;
		} else {
			if (pv->pv_flags & PV_UNCACHED) {
				npte = (npte & ~PG_CACHEMODE) | PG_UNCACHED;
			} else if (CpuCacheAliasMask != 0) {
			/*
			 * There is at least one other VA mapping this page.
			 * Check if they are cache index compatible. If not
			 * remove all mappings, flush the cache and set page
			 * to be mapped uncached. Caching will be restored
			 * when pages are mapped compatible again. NOT!
			 */
				for (npv = pv; npv; npv = npv->pv_next) {
					/*
					 * Check cache aliasing incompatibility
					 */
					if (((npv->pv_va ^ va) & CpuCacheAliasMask) != 0) {
						printf("pmap_enter: uncached mapping for pa %p, va %p !=  %p.\n", pa, npv->pv_va, va);
						pmap_page_cache(pa,PV_UNCACHED);
						Mips_SyncCache();
						pv->pv_flags &= ~PV_CACHED;
						npte = (npte & ~PG_CACHEMODE) | PG_UNCACHED;
						break;
					}
				}
			}

			/*
			 * There is at least one other VA mapping this page.
			 * Place this entry after the header.
			 *
			 * Note: the entry may already be in the table if
			 * we are only changing the protection bits.
			 */
			for (npv = pv; npv; npv = npv->pv_next) {
				if (pmap == npv->pv_pmap && va == npv->pv_va) {
					goto fnd;
				}
			}

			if (pmapdebug & PDB_PVENTRY) {
				printf("pmap_enter: new pv: pmap %x va %x pa %p\n",
					pmap, va, pa);
			}

			/* can this cause us to recurse forever? */
			npv = pmap_pv_alloc();
			if (npv == NULL) {
				panic("pmap_pv_alloc() failed");
			}
			npv->pv_va = va;
			npv->pv_pmap = pmap;
			npv->pv_next = pv->pv_next;
			npv->pv_flags = pv->pv_flags;
			pv->pv_next = npv;

			if (!npv->pv_next)
				stat_count(enter_stats.secondpv);
		fnd:
			;
		}
		splx(s);
	}
	else {
		/*
		 * Assumption: if it is not part of our managed memory
		 * then it must be device memory which may be volitile.
		 */
		stat_count(enter_stats.unmanaged);
		if (prot & VM_PROT_WRITE) {
			npte = PG_IOPAGE & ~PG_G;
		} else {
			npte = PG_IOPAGE & ~(PG_G | PG_M);
		}
	}

	if (pmap == pmap_kernel()) {
		pte = kvtopte(va);
		npte |= vad_to_pfn(pa) | PG_ROPAGE | PG_G;
		if (!(pte->pt_entry & PG_V)) {
			pmap->pm_stats.resident_count++;
		}
		if (pa != pfn_to_pad(pte->pt_entry)) {
			pmap_remove(pmap, va, va + NBPG);
			stat_count(enter_stats.mchange);
		}

		/*
		 * Update the same virtual address entry.
		 */
		pte->pt_entry = npte;
		tlb_update(va, npte);
		return (KERN_SUCCESS);
	}

	/*
	 *  User space mapping. Do table build.
	 */
	if (!(pte = pmap_segmap(pmap, va))) {
		pv_entry_t pv;
		do {
			mem = uvm_pagealloc(NULL, 0, NULL, UVM_PGA_USERESERVE|UVM_PGA_ZERO);
			if (mem == NULL) {
				/* XXX What else can we do?  Deadlocks?  */
				uvm_wait("penter");
			}
		} while (mem == NULL);

		pv = pa_to_pvh(VM_PAGE_TO_PHYS(mem));
		if (pv->pv_flags & PV_CACHED &&
		   ((pv->pv_va ^ PHYS_TO_KSEG0(VM_PAGE_TO_PHYS(mem))) & CpuCacheAliasMask) != 0) {
			Mips_SyncDCachePage(pv->pv_va);
		}
		pv->pv_va = PHYS_TO_KSEG0(VM_PAGE_TO_PHYS(mem));
		pv->pv_flags = PV_CACHED;

		pmap_segmap(pmap, va) = pte = (pt_entry_t *)
			(long)PHYS_TO_KSEG0(VM_PAGE_TO_PHYS(mem));
	}
	pte += (va >> PGSHIFT) & (NPTEPG - 1);

	/*
	 * Now validate mapping with desired protection/wiring.
	 * Assume uniform modified and referenced status for all
	 * MIPS pages in a OpenBSD page.
	 */
	if (is_physaddr) {
		npte |= vad_to_pfn(pa);
	}
	else {
		npte |= vad_to_pfn(pa);
	}
	if (pmapdebug & PDB_ENTER) {
		printf("pmap_enter: new pte %x", npte);
		if (pmap->pm_tlbgen == tlbpid_gen)
			printf(" tlbpid %d", pmap->pm_tlbpid);
		printf("\n");
	}

	if (pa != pfn_to_pad(pte->pt_entry)) {
		pmap_remove(pmap, va, va + NBPG);
		stat_count(enter_stats.mchange);
	}

	if (!(pte->pt_entry & PG_V)) {
		pmap->pm_stats.resident_count++;
	}
	pte->pt_entry = npte;
	if (pmap->pm_tlbgen == tlbpid_gen) {
		int s, i;
		s = splimp();
		i = tlb_update(va | (pmap->pm_tlbpid << VMTLB_PID_SHIFT), npte);

		/*
		 *  If mapping a memory space address invalidate ICache.
		 */
		if (is_physaddr) {
			Mips_InvalidateICachePage(va);
		}
		splx(s);
	}

	return (KERN_SUCCESS);
}

void
pmap_kenter_pa(va, pa, prot)
	vaddr_t va;
	paddr_t pa;
	vm_prot_t prot;
{
	pt_entry_t *pte;
	u_int npte;

	if (pmapdebug & (PDB_FOLLOW|PDB_ENTER)) {
		printf("pmap_kenter_pa(%lx, %lx, %x)\n", va, pa, prot);
	}


	npte = vad_to_pfn(pa) | PG_G;
	if (prot & VM_PROT_WRITE) {
		npte |= PG_RWPAGE;
	}
	else {
		npte |= PG_ROPAGE;
	}
	pte = kvtopte(va);
	pte->pt_entry = npte;
	tlb_update(va, npte);
}

void
pmap_kenter_cache(va, pa, prot, cache)
	vaddr_t va;
	paddr_t pa;
	vm_prot_t prot;
	int cache;
{
	pt_entry_t *pte;
	u_int npte;

	if (pmapdebug & (PDB_FOLLOW|PDB_ENTER)) {
		printf("pmap_kenter_cache(%lx, %lx, %x)\n", va, pa, prot);
	}


	npte = vad_to_pfn(pa) | PG_G;
	if (prot & VM_PROT_WRITE) {
		npte |= PG_M | cache;
	}
	else {
		npte |= PG_RO | cache;
	}
	pte = kvtopte(va);
	pte->pt_entry = npte;
	tlb_update(va, npte);
}

void
pmap_kremove(va, len)
	vaddr_t va;
	vsize_t len;
{
	pt_entry_t *pte;
	vaddr_t eva;
	u_int entry;

	if (pmapdebug & (PDB_FOLLOW|PDB_REMOVE)) {
		printf("pmap_kremove(%lx, %lx)\n", va, len);
	}

	pte = kvtopte(va);
	eva = va + len;
	for (; va < eva; va += PAGE_SIZE, pte++) {
		entry = pte->pt_entry;
		if (entry & PG_V) {
			continue;
		}
		Mips_SyncDCachePage(va);
		pte->pt_entry = PG_NV | PG_G;
		tlb_flush_addr(va);
	}
}

void
pmap_unwire(pmap, va)
	pmap_t pmap;
	vaddr_t va;
{
}

/*
 *	Routine:	pmap_extract
 *	Function:
 *		Extract the physical page address associated
 *		with the given map/virtual_address pair.
 */
boolean_t
pmap_extract(pmap, va, pa)
	pmap_t	pmap;
	vaddr_t va;
	paddr_t *pa;
{
	if (pmapdebug & PDB_FOLLOW) {
		printf("pmap_extract(%x, %x) -> ", pmap, va);
	}

	if (!pmap->pm_segtab) {
		if (va >= (long)KSEG0_BASE && va < (long)(KSEG0_BASE + KSEG_SIZE)) {
			*pa = (long)KSEG0_TO_PHYS(va);
		}
		else {
#ifdef DIAGNOSTIC
			if (va < VM_MIN_KERNEL_ADDRESS || va >= virtual_end) {
				printf("pmap_extract(%x, %x) -> ", pmap, va);
				panic("pmap_extract");
			}
#endif
			*pa = pfn_to_pad(kvtopte(va)->pt_entry);
		}
	}
	else {
		pt_entry_t *pte;

		if (!(pte = pmap_segmap(pmap, va))) {
			*pa = 0;
		}
		else {
			pte += (va >> PGSHIFT) & (NPTEPG - 1);
			*pa = pfn_to_pad(pte->pt_entry);
		}
	}
	if (*pa)
		*pa |= va & PGOFSET;

	if (pmapdebug & PDB_FOLLOW) {
		printf("pmap_extract: pa %x\n", *pa);
	}

	return (TRUE);
}

/*
 * Find first virtual address >= *vap that
 * will not cause cache aliases.
 */
void
pmap_prefer(foff, vap)
	paddr_t foff;
	vaddr_t *vap;
{
#if 1
	vaddr_t va = *vap;
	long d, m;

	m = CpuCacheAliasMask;
	if (m == 0)		/* m=0 => no cache aliasing */
		return;

	m = (m | (m - 1)) + 1;	/* Value from mask */
	d = foff - va;
	d &= (m - 1);
	*vap = va + d;
#else
	*vap += (*vap ^ foff) & CpuCacheAliasMask;
#endif
}

/*
 *	Copy the range specified by src_addr/len
 *	from the source map to the range dst_addr/len
 *	in the destination map.
 *
 *	This routine is only advisory and need not do anything.
 */
void
pmap_copy(dst_pmap, src_pmap, dst_addr, len, src_addr)
	pmap_t dst_pmap;
	pmap_t src_pmap;
	vaddr_t dst_addr;
	vsize_t len;
	vaddr_t src_addr;
{

	if (pmapdebug & PDB_FOLLOW) {
		printf("pmap_copy(%x, %x, %x, %x, %x)\n",
		       dst_pmap, src_pmap, dst_addr, len, src_addr);
	}
}

#ifndef pmap_update
/*
 *	Require that all active physical maps contain no
 *	incorrect entries NOW.  [This update includes
 *	forcing updates of any address map caching.]
 *
 *	Generally used to insure that a thread about
 *	to run will see a semantically correct world.
 */
void
pmap_update(pmap)
	pmap_t pmap;
{
	if (pmapdebug & PDB_FOLLOW) {
		printf("pmap_update()\n");
	}
}
#endif

/*
 *	pmap_zero_page zeros the specified (machine independent) page.
 */
void
pmap_zero_page(struct vm_page *pg)
{
	paddr_t phys = VM_PAGE_TO_PHYS(pg);
	vaddr_t p;
	pv_entry_t pv;

	if (pmapdebug & PDB_FOLLOW) {
		printf("pmap_zero_page(%x)\n", phys);
	}

	p = (vaddr_t)PHYS_TO_KSEG0(phys);
	pv = pa_to_pvh(phys);
	if (pv->pv_flags & PV_CACHED &&
		   ((pv->pv_va ^ (int)p) & CpuCacheAliasMask) != 0) {
		Mips_SyncDCachePage(pv->pv_va);
	}
	mem_zero_page(p);
	Mips_HitSyncDCache(p, PAGE_SIZE);
}

/*
 *	pmap_copy_page copies the specified (machine independent) page.
 *
 *	We do the copy phys to phys and need to check if there may be
 *	a viritual coherence problem. If so flush the cache for the
 *	areas before copying, and flush afterwards.
 */
void
pmap_copy_page(struct vm_page *srcpg, struct vm_page *dstpg)
{
	paddr_t src = VM_PAGE_TO_PHYS(srcpg);
	paddr_t dst = VM_PAGE_TO_PHYS(dstpg);
#if 0
	int *s, *d;
/*	if (CpuCacheAliasMask == 0) {  XXX */
	s = (int *)PHYS_TO_KSEG0(src);
	d = (int *)PHYS_TO_KSEG0(dst);

	memcpy(d, s, PAGE_SIZE);

#else
	int *s, *d, *end;
	int df = 1;
	int sf = 1;
	int tmp0, tmp1, tmp2, tmp3;
	pv_entry_t pv;

	if (pmapdebug & PDB_FOLLOW) {
		printf("pmap_copy_page(%x, %x)\n", src, dst);
	}
	s = (int *)(long)PHYS_TO_KSEG0(src);
	d = (int *)(long)PHYS_TO_KSEG0(dst);

	pv = pa_to_pvh(src);
	if (pv->pv_flags & PV_CACHED &&
		   (sf = ((pv->pv_va ^ (long)s) & CpuCacheAliasMask) != 0)) {
		Mips_SyncDCachePage(pv->pv_va);
	}
	pv = pa_to_pvh(dst);
	if (pv->pv_flags & PV_CACHED &&
		   (df = ((pv->pv_va ^ (long)d) & CpuCacheAliasMask) != 0)) {
		Mips_SyncDCachePage(pv->pv_va);
	}

	end = s + PAGE_SIZE / sizeof(int);
	do {
		tmp0 = s[0]; tmp1 = s[1]; tmp2 = s[2]; tmp3 = s[3];
		d[0] = tmp0; d[1] = tmp1; d[2] = tmp2; d[3] = tmp3;
		s += 4;
		d += 4;
	} while (s != end);

	if (sf) {
		Mips_HitSyncDCache((vaddr_t)PHYS_TO_KSEG0(src), PAGE_SIZE);
	}
#if 0	/* XXX TODO: Why can't we trust the following? */
	if (df || (pv->pv_pmap == NULL) || (pv->pv_flags & PV_EXEC)) {
		Mips_HitSyncDCachePage(dst);
	}
#else
	Mips_HitSyncDCache((vaddr_t)PHYS_TO_KSEG0(dst), PAGE_SIZE);
#endif
#endif
}

/*
 *	Clear the modify bits on the specified physical page.
 */
boolean_t
pmap_clear_modify(pg)
	struct vm_page *pg;
{
	paddr_t pa = VM_PAGE_TO_PHYS(pg);
	boolean_t rv = FALSE;

	if (pmapdebug & PDB_FOLLOW) {
		printf("pmap_clear_modify(%x)\n", pa);
	}
	return(rv);
}

/*
 *	pmap_clear_reference:
 *
 *	Clear the reference bit on the specified physical page.
 */
boolean_t
pmap_clear_reference(pg)
	struct vm_page *pg;
{
	paddr_t pa = VM_PAGE_TO_PHYS(pg);

	if (pmapdebug & PDB_FOLLOW) {
		printf("pmap_clear_reference(%x)\n", pa);
	}
	return(FALSE);
}

/*
 *	pmap_is_referenced:
 *
 *	Return whether or not the specified physical page is referenced
 *	by any physical maps.
 */
boolean_t
pmap_is_referenced(pg)
	struct vm_page *pg;
{
	return (FALSE);
}

/*
 *	pmap_is_modified:
 *
 *	Return whether or not the specified physical page is modified
 *	by any physical maps.
 */
boolean_t
pmap_is_modified(pg)
	struct vm_page *pg;
{
	return (FALSE);
}

paddr_t
pmap_phys_address(ppn)
	int ppn;
{

	if (pmapdebug & PDB_FOLLOW) {
		printf("pmap_phys_address(%x)\n", ppn);
	}
	return (ptoa(ppn));
}

/*
 * Miscellaneous support routines
 */

/*
 * Allocate a hardware PID and return it.
 * It takes almost as much or more time to search the TLB for a
 * specific PID and flush those entries as it does to flush the entire TLB.
 * Therefore, when we allocate a new PID, we just take the next number. When
 * we run out of numbers, we flush the TLB, increment the generation count
 * and start over. PID zero is reserved for kernel use.
 * This is called only by switch().
 */
int
pmap_alloc_tlbpid(p)
	struct proc *p;
{
	pmap_t pmap;
	int id;

	pmap = p->p_vmspace->vm_map.pmap;
	if (pmap->pm_tlbgen != tlbpid_gen) {
		id = tlbpid_cnt;
		if (id >= VMNUM_PIDS) {
			tlb_flush(sys_config.cpu[0].tlbsize);
			/* reserve tlbpid_gen == 0 to alway mean invalid */
			if (++tlbpid_gen == 0)
				tlbpid_gen = 1;
			id = 1;
		}
		tlbpid_cnt = id + 1;
		pmap->pm_tlbpid = id;
		pmap->pm_tlbgen = tlbpid_gen;
	}
	else {
		id = pmap->pm_tlbpid;
	}

	if (pmapdebug & (PDB_FOLLOW|PDB_TLBPID)) {
		if (curproc) {
			printf("pmap_alloc_tlbpid: curproc %d '%s' ",
				curproc->p_pid, curproc->p_comm);
		}
		else {
			printf("pmap_alloc_tlbpid: curproc <none> ");
		}
		printf("segtab %x tlbpid %d pid %d '%s'\n",
			pmap->pm_segtab, id, p->p_pid, p->p_comm);
	}

	return (id);
}

/*
 * Remove a physical to virtual address translation.
 * Returns TRUE if it was the last mapping and cached, else FALSE.
 */
void
pmap_remove_pv(pmap, va, pa)
	pmap_t pmap;
	vaddr_t va;
	paddr_t pa;
{
	pv_entry_t pv, npv;
	int s;

	if (pmapdebug & (PDB_FOLLOW|PDB_PVENTRY)) {
		printf("pmap_remove_pv(%x, %x, %x)\n", pmap, va, pa);
	}

	/*
	 * Remove page from the PV table (raise IPL since we
	 * may be called at interrupt time).
	 */
	if (!IS_VM_PHYSADDR(pa)) {
		return;
	}

	pv = pa_to_pvh(pa);
	s = splimp();			/* XXX not in nbsd */
	/*
	 * If it is the first entry on the list, it is actually
	 * in the header and we must copy the following entry up
	 * to the header.  Otherwise we must search the list for
	 * the entry. In either case we free the now unused entry.
	 */
	if (pmap == pv->pv_pmap && va == pv->pv_va) {
		npv = pv->pv_next;
		if (npv) {
			*pv = *npv;
			pmap_pv_free(npv);
		}
		else {
			pv->pv_pmap = NULL;
		}
		stat_count(remove_stats.pvfirst);
	}
	else {
		for (npv = pv->pv_next; npv; pv = npv, npv = npv->pv_next) {
			stat_count(remove_stats.pvsearch);
			if (pmap == npv->pv_pmap && va == npv->pv_va)
				goto fnd;
		}
#ifdef DIAGNOSTIC
		printf("pmap_remove_pv(%x, %x, %x) not found\n", pmap, va, pa);
		panic("pmap_remove_pv");
#endif
	fnd:
		pv->pv_next = npv->pv_next;
		pmap_pv_free(npv);
	}
	splx(s);
	return;
}

/*
 * pmap_pv_page_alloc:
 *
 *      Allocate a page for the pv_entry pool.
 */
void *
pmap_pv_page_alloc(u_long size, int flags, int mtype)
{
	paddr_t pg;

	if (pmap_physpage_alloc(&pg))
		return ((void *)(long)PHYS_TO_KSEG0(pg));
	return (NULL);
}

/*
 * pmap_pv_page_free:
 *
 *      Free a pv_entry pool page.
 */
void
pmap_pv_page_free(void *v, u_long size, int mtype)
{
	pmap_physpage_free(KSEG0_TO_PHYS((vaddr_t)v));
}

/*
 * pmap_physpage_alloc:
 *
 *	Allocate a single page from the VM system and return the
 *	physical address for that page.
 */
boolean_t
pmap_physpage_alloc(paddr_t *pap)
{
	struct vm_page *pg;
	paddr_t pa;

	pg = uvm_pagealloc(NULL, 0, NULL, UVM_PGA_USERESERVE);
	if (pg != NULL) {
		pa = VM_PAGE_TO_PHYS(pg);
		*pap = pa;
		return (TRUE);
	}
	return (FALSE);
}

/*
 * pmap_physpage_free:
 *
 *	Free a single pmap metadate page at the specified physical address.
 */
void
pmap_physpage_free(paddr_t pa)
{
	struct vm_page *pg;

	if ((pg = PHYS_TO_VM_PAGE(pa)) == NULL) {
		panic("pmap_physpage_free: bogus physical page address");
	}
	uvm_pagefree(pg);
}

/*==================================================================*/
/* Bus space map utility functions */

int
bus_mem_add_mapping(bus_addr_t bpa, bus_size_t size, int cacheable,
			bus_space_handle_t *bshp)
{
	bus_addr_t vaddr;
	bus_addr_t spa, epa;
	bus_size_t off;
	int len;

	spa = trunc_page(bpa);
	epa = bpa + size;
	off = bpa - spa;
	len = size+off;

	if (phys_map == NULL) {
		printf("ouch, add mapping when phys map not ready!\n");
	} else {
		vaddr = uvm_km_valloc_wait(kernel_map, len);
	}
	*bshp = vaddr + off;
#ifdef DEBUG_BUS_MEM_ADD_MAPPING
	printf("map bus %x size %x to %x vbase %x\n", bpa, size, *bshp, spa);
#endif
	for (; len > 0; len -= NBPG) {
		pmap_kenter_cache(vaddr, spa,
			VM_PROT_READ | VM_PROT_WRITE,
			cacheable ? PG_IOPAGE : PG_IOPAGE); /* XXX */
		spa += NBPG;
		vaddr += NBPG;
	}
	return 0;
}

