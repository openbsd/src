/*	$NetBSD: pmap.c,v 1.7 1995/11/23 02:34:26 cgd Exp $	*/

/* 
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)pmap.c	8.6 (Berkeley) 5/27/94
 */

#ifdef XXX
/*
 * HP9000/300 series physical map management code.
 *
 * Supports:
 *	68020 with HP MMU	models 320, 350
 *	68020 with 68551 MMU	models 318, 319, 330 (all untested)
 *	68030 with on-chip MMU	models 340, 360, 370, 345, 375, 400
 *	68040 with on-chip MMU	models 380, 425, 433
 *
 * Notes:
 *	Don't even pay lip service to multiprocessor support.
 *
 *	We assume TLB entries don't have process tags (except for the
 *	supervisor/user distinction) so we only invalidate TLB entries
 *	when changing mappings for the current (or kernel) pmap.  This is
 *	technically not true for the 68551 but we flush the TLB on every
 *	context switch, so it effectively winds up that way.
 *
 *	Bitwise and/or operations are significantly faster than bitfield
 *	references so we use them when accessing STE/PTEs in the pmap_pte_*
 *	macros.  Note also that the two are not always equivalent; e.g.:
 *		(*(int *)pte & PG_PROT) [4] != pte->pg_prot [1]
 *	and a couple of routines that deal with protection and wiring take
 *	some shortcuts that assume the and/or definitions.
 *
 *	This implementation will only work for PAGE_SIZE == NBPG
 *	(i.e. 4096 bytes).
 */
#endif

/*
 *	Manages physical address maps.
 *
 *	In addition to hardware address maps, this
 *	module is called upon to provide software-use-only
 *	maps which may or may not be stored in the same
 *	form as hardware maps.  These pseudo-maps are
 *	used to store intermediate results from copy
 *	operations to and from address spaces.
 *
 *	Since the information managed by this module is
 *	also stored by the logical address mapping module,
 *	this module may throw away valid virtual-to-physical
 *	mappings at almost any time.  However, invalidations
 *	of virtual-to-physical mappings must be done as
 *	requested.
 *
 *	In order to cope with hardware architectures which
 *	make virtual-to-physical map invalidates expensive,
 *	this module may delay invalidate or reduced protection
 *	operations until such time as they are actually
 *	necessary.  This module is given full information as
 *	to which processors are currently using which maps,
 *	and to when physical maps must be made correct.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/user.h>
#include <sys/buf.h>
#ifdef SYSVSHM
#include <sys/shm.h>
#endif

#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>

#include <machine/cpu.h>

#ifdef PMAPSTATS
struct {
	int collectscans;
	int collectpages;
	int kpttotal;
	int kptinuse;
	int kptmaxuse;
} kpt_stats;
struct {
	int kernel;	/* entering kernel mapping */
	int user;	/* entering user mapping */
	int ptpneeded;	/* needed to allocate a PT page */
	int nochange;	/* no change at all */
	int pwchange;	/* no mapping change, just wiring or protection */
	int wchange;	/* no mapping change, just wiring */
	int pchange;	/* no mapping change, just protection */
	int mchange;	/* was mapped but mapping to different page */
	int managed;	/* a managed page */
	int firstpv;	/* first mapping for this PA */
	int secondpv;	/* second mapping for this PA */
	int ci;		/* cache inhibited */
	int unmanaged;	/* not a managed page */
	int flushes;	/* cache flushes */
} enter_stats;
struct {
	int calls;
	int removes;
	int pvfirst;
	int pvsearch;
	int ptinvalid;
	int uflushes;
	int sflushes;
} remove_stats;
struct {
	int calls;
	int changed;
	int alreadyro;
	int alreadyrw;
} protect_stats;
struct chgstats {
	int setcalls;
	int sethits;
	int setmiss;
	int clrcalls;
	int clrhits;
	int clrmiss;
} changebit_stats[16];
#endif

#ifdef DEBUG
#define PDB_FOLLOW	0x0001
#define PDB_INIT	0x0002
#define PDB_ENTER	0x0004
#define PDB_REMOVE	0x0008
#define PDB_CREATE	0x0010
#define PDB_PTPAGE	0x0020
#define PDB_CACHE	0x0040
#define PDB_BITS	0x0080
#define PDB_COLLECT	0x0100
#define PDB_PROTECT	0x0200
#define PDB_SEGTAB	0x0400
#define PDB_MULTIMAP	0x0800
#define PDB_BOOTSTRAP	0x1000
#define PDB_PARANOIA	0x2000
#define PDB_WIRING	0x4000
#define PDB_PVDUMP	0x8000

int debugmap = 0;
int pmapdebug = PDB_PARANOIA;
extern vm_offset_t pager_sva, pager_eva;
#endif

/*
 * Get STEs and PTEs for user/kernel address space
 */
#define	pmap_ste(m, v)	 (&((m)->pm_stab[vatoste((vm_offset_t)(v))]))
#define pmap_ste_v(m, v) (*pmap_ste(m, v) & PG_V)

#define pmap_pte(m, v)							\
	(&((m)->pm_ptab[NPTEPG * vatoste((vm_offset_t)(v)) +		\
	    vatopte((vm_offset_t)(v))]))
#define pmap_pte_pa(pte)	(PG_PFNUM(*(pte)) << PGSHIFT)
#define pmap_pte_prot(pte)	(*(pte) & PG_PROT)
#define pmap_pte_w(pte)		(*(pte) & PG_WIRED)
#define pmap_pte_v(pte)		(*(pte) & PG_V)

#define pmap_pte_set_w(pte, v) \
	if (v) *(u_long *)(pte) |= PG_WIRED; else *(u_long *)(pte) &= ~PG_WIRED
#define pmap_pte_w_chg(pte, nw)		((nw) ^ pmap_pte_w(pte))

#define pmap_pte_set_prot(pte, np)	{ *(pte) &= ~PG_PROT ; *(pte) |= (np); }
#define pmap_pte_prot_chg(pte, np)	((np) ^ pmap_pte_prot(pte))
	

/*
 * Given a map and a machine independent protection code,
 * convert to an hp300 protection code.
 */
#define pte_prot(m, p)	(protection_codes[m == pmap_kernel() ? 0 : 1][p])
int	protection_codes[2][8];

/*
 * Kernel page table page management.
 */
struct kpt_page {
	struct kpt_page *kpt_next;	/* link on either used or free list */
	vm_offset_t	kpt_va;		/* always valid kernel VA */
	vm_offset_t	kpt_pa;		/* PA of this page (for speed) */
};
struct kpt_page *kpt_free_list, *kpt_used_list;
struct kpt_page *kpt_pages;

/*
 * The Alpha's level-1 page table.
 */
pt_entry_t	*Lev1map;

/*
 * Kernel segment/page table and page table map.
 * The page table map gives us a level of indirection we need to dynamically
 * expand the page table.  It is essentially a copy of the segment table
 * with PTEs instead of STEs.  All are initialized in locore at boot time.
 * Segtabzero is an empty segment table which all processes share til they
 * reference something.
 */
pt_entry_t	*Sysptmap;
pt_entry_t	*Sysmap;
vm_size_t	Sysptmapsize, Sysmapsize;
pt_entry_t	*Segtabzero, Segtabzeropte;

struct pmap	kernel_pmap_store;
vm_map_t	st_map, pt_map;

vm_offset_t    	avail_start;	/* PA of first available physical page */
vm_offset_t	avail_end;	/* PA of last available physical page */
vm_size_t	mem_size;	/* memory size in bytes */
vm_offset_t	virtual_avail;  /* VA of first avail page (after kernel bss)*/
vm_offset_t	virtual_end;	/* VA of last avail page (end of kernel AS) */
vm_offset_t	vm_first_phys;	/* PA of first managed page */
vm_offset_t	vm_last_phys;	/* PA just past last managed page */
boolean_t	pmap_initialized = FALSE;	/* Has pmap_init completed? */
char		*pmap_attributes; /* reference and modify bits */

/*
 * Internal routines
 */
void alpha_protection_init __P((void));
void pmap_remove_mapping __P((pmap_t, vm_offset_t, pt_entry_t *, int));
void pmap_changebit	__P((vm_offset_t, u_long, boolean_t));
void pmap_enter_ptpage	__P((pmap_t, vm_offset_t));
#ifdef DEBUG
void pmap_pvdump	__P((vm_offset_t));
void pmap_check_wiring	__P((char *, vm_offset_t));
#endif

/* pmap_remove_mapping flags */
#define	PRM_TFLUSH	1
#define	PRM_CFLUSH	2

/*
 * pmap_bootstrap:
 * Bootstrap the system to run with virtual memory.
 * firstaddr is the first unused kseg0 address (not page aligned).
 */
void
pmap_bootstrap(firstaddr, ptaddr)
	vm_offset_t firstaddr;
	vm_offset_t ptaddr;
{
	register int i;
	vm_offset_t start;
	pt_entry_t pte;
	extern int firstusablepage, lastusablepage;

#ifdef DEBUG
	if (pmapdebug & (PDB_FOLLOW|PDB_BOOTSTRAP))
		printf("pmap_bootstrap(0x%lx, 0x%lx)\n", firstaddr, ptaddr);
#endif

	/* must be page aligned */
	start = firstaddr = alpha_round_page(firstaddr);

#define valloc(name, type, num)					\
	    (name) = (type *)firstaddr;				\
	    firstaddr = ALIGN((vm_offset_t)((name)+(num)))

	/*
	 * Allocate an empty prototype segment map for processes.
	 * This will be used until processes get their own.
	 */
	valloc(Segtabzero, pt_entry_t, NPTEPG);
        Segtabzeropte = (k0segtophys(Segtabzero) >> PGSHIFT) << PG_SHIFT;
	Segtabzeropte |= PG_V | PG_KRE | PG_KWE | PG_WIRED;

	/*
	 * Figure out how many PTE's are necessary to map the kernel.
	 * The '512' comes from PAGER_MAP_SIZE in vm_pager_init().
	 * This should be kept in sync.
	 * We also reserve space for kmem_alloc_pageable() for vm_fork().
	 */
	Sysmapsize = (VM_KMEM_SIZE + VM_MBUF_SIZE + VM_PHYS_SIZE +
		nbuf * MAXBSIZE + 16 * NCARGS) / NBPG + 512 + 256;
        Sysmapsize += maxproc * (btoc(ALPHA_STSIZE) + btoc(ALPHA_MAX_PTSIZE));

#ifdef SYSVSHM
	Sysmapsize += shminfo.shmall;
#endif
	Sysmapsize = roundup(Sysmapsize, NPTEPG);

	/*
	 * Allocate a level 1 PTE table for the kernel.
	 * This is always one page long.
	 * IF THIS IS NOT A MULTIPLE OF NBPG, ALL WILL GO TO HELL.
	 */
	valloc(Lev1map, pt_entry_t, NPTEPG);

	/*
	 * Allocate a level 2 PTE table for the kernel.
	 * These must map all of the level3 PTEs.
	 * IF THIS IS NOT A MULTIPLE OF NBPG, ALL WILL GO TO HELL.
	 */
	Sysptmapsize = roundup(howmany(Sysmapsize, NPTEPG), NPTEPG);
	valloc(Sysptmap, pt_entry_t, Sysptmapsize);
	pmap_kernel()->pm_stab = Sysptmap;

	/*
	 * Allocate a level 3 PTE table for the kernel.
	 * Contains Sysmapsize PTEs.
	 */
	valloc(Sysmap, pt_entry_t, Sysmapsize);
	pmap_kernel()->pm_ptab = Sysmap;

	/*
	 * Allocate memory for page attributes.
	 * allocates a few more entries than we need, but that's safe.
	 */
	valloc(pmap_attributes, char, 1 + lastusablepage - firstusablepage);

	/*
	 * Allocate memory for pv_table.
	 * This will allocate more entries than we really need.
	 * We could do this in pmap_init when we know the actual
	 * phys_start and phys_end but its better to use kseg0 addresses
	 * rather than kernel virtual addresses mapped through the TLB.
	 */
	i = 1 + lastusablepage - alpha_btop(k0segtophys(firstaddr));
	valloc(pv_table, struct pv_entry, i);

	/*
	 * Clear allocated memory.
	 */
	firstaddr = alpha_round_page(firstaddr);
	bzero((caddr_t)start, firstaddr - start);

	/*
	 * Set up level 1 page table
	 */

	/* First, copy mappings for things below VM_MIN_KERNEL_ADDRESS */
	bcopy((caddr_t)ptaddr, Lev1map,
	    kvtol1pte(VM_MIN_KERNEL_ADDRESS) * sizeof Lev1map[0]);

	/* Second, map all of the level 2 pte pages */
	for (i = 0; i < howmany(Sysptmapsize, NPTEPG); i++) {
		pte = (k0segtophys(Sysptmap + (i*PAGE_SIZE)) >> PGSHIFT)
		    << PG_SHIFT;
		pte |= PG_V | PG_ASM | PG_KRE | PG_KWE | PG_WIRED;
		Lev1map[kvtol1pte(VM_MIN_KERNEL_ADDRESS +
		    (i*PAGE_SIZE*NPTEPG*NPTEPG))] = pte;
	}

	/* Finally, map the virtual page table */
	pte = (k0segtophys(Lev1map) >> PGSHIFT) << PG_SHIFT;
	pte |= PG_V | PG_KRE | PG_KWE; /* NOTE NO ASM */
	Lev1map[kvtol1pte(VPTBASE)] = pte;
	
	/*
	 * Set up level 2 page table.
	 */
	/* Map all of the level 3 pte pages */
	for (i = 0; i < howmany(Sysmapsize, NPTEPG); i++) {
		pte = (k0segtophys(((caddr_t)Sysmap)+(i*PAGE_SIZE)) >> PGSHIFT)
		    << PG_SHIFT;
		pte |= PG_V | PG_ASM | PG_KRE | PG_KWE | PG_WIRED;
		Sysptmap[vatoste(VM_MIN_KERNEL_ADDRESS+
		    (i*PAGE_SIZE*NPTEPG))] = pte;
	}

	/*
	 * Set up level three page table (Sysmap)
	 */
	/* Nothing to do; it's already zero'd */

	avail_start = k0segtophys(firstaddr);
	avail_end = alpha_ptob(1 + lastusablepage);
	mem_size = avail_end - avail_start;

	virtual_avail = VM_MIN_KERNEL_ADDRESS;
	virtual_end = VM_MIN_KERNEL_ADDRESS + Sysmapsize * NBPG;

	simple_lock_init(&pmap_kernel()->pm_lock);
	pmap_kernel()->pm_count = 1;

	/*
	 * Set up curproc's (i.e. proc 0's) PCB such that the ptbr
	 * points to the right place.
	 */
	curproc->p_addr->u_pcb.pcb_ptbr = k0segtophys(Lev1map) >> PGSHIFT;
}

/*
 * Unmap the PROM mappings.  PROM mappings are kept around
 * by pmap_bootstrap, so we can still use the prom's printf.
 * Basically, blow away all mappings in the level one PTE
 * table below VM_MIN_KERNEL_ADDRESS.  The Virtual Page Table
 * Is at the end of virtual space, so it's safe.
 */
void
pmap_unmap_prom()
{
	int i;
	extern int prom_mapped;
	extern pt_entry_t *rom_ptep, rom_pte;

#ifdef DEBUG
	if (pmapdebug & (PDB_FOLLOW|PDB_BOOTSTRAP))
		printf("pmap_unmap_prom\n");
#endif

	/* XXX save old pte so that we can remap prom if necessary */
	rom_ptep = &Lev1map[0];					/* XXX */
	rom_pte = *rom_ptep & ~PG_ASM;				/* XXX */

	/* Mark all mappings before VM_MIN_KERNEL_ADDRESS as invalid. */
	bzero(Lev1map, kvtol1pte(VM_MIN_KERNEL_ADDRESS) * sizeof Lev1map[0]);
	prom_mapped = 0;
	TBIA();
}

/*
 * Bootstrap memory allocator. This function allows for early dynamic
 * memory allocation until the virtual memory system has been bootstrapped.
 * After that point, either kmem_alloc or malloc should be used. This
 * function works by stealing pages from the (to be) managed page pool,
 * stealing virtual address space, then implicitly mapping the pages 
 * (by using their k0seg addresses) and zeroing them.
 *
 * It should be used from pmap_bootstrap till vm_page_startup, afterwards
 * it cannot be used, and will generate a panic if tried. Note that this
 * memory will never be freed, and in essence it is wired down.
 */
void *
pmap_bootstrap_alloc(size)
	int size;
{
	vm_offset_t val;
	extern boolean_t vm_page_startup_initialized;

#ifdef DEBUG
	if (pmapdebug & (PDB_FOLLOW|PDB_BOOTSTRAP))
		printf("pmap_bootstrap_alloc(%lx)\n", size);
#endif
	if (vm_page_startup_initialized)
		panic("pmap_bootstrap_alloc: called after startup initialized");

	val = phystok0seg(avail_start);
	size = round_page(size);
	avail_start += size;

	bzero((caddr_t)val, size);
	return ((void *)val);
}

/*
 *	Initialize the pmap module.
 *	Called by vm_init, to initialize any structures that the pmap
 *	system needs to map virtual memory.
 */
void
pmap_init(phys_start, phys_end)
	vm_offset_t	phys_start, phys_end;
{
	vm_offset_t	addr, addr2;
        vm_size_t	s;

#ifdef DEBUG
        if (pmapdebug & PDB_FOLLOW)
                printf("pmap_init(%x, %x)\n", phys_start, phys_end);
#endif

	/* initialize protection array */
	alpha_protection_init();

	/*
	 * Allocate the segment table map
	 */
	s = maxproc * ALPHA_STSIZE;
	st_map = kmem_suballoc(kernel_map, &addr, &addr2, s, TRUE);

	/*
	 * Allocate the page table map
	 */
	s = maxproc * ALPHA_MAX_PTSIZE;			/* XXX limit it */
	pt_map = kmem_suballoc(kernel_map, &addr, &addr2, s, TRUE);
	
	/*
	 * Now it is safe to enable pv_table recording.
	 */
	vm_first_phys = phys_start;
	vm_last_phys = phys_end;
	pmap_initialized = TRUE;
}

/*
 *	Used to map a range of physical addresses into kernel
 *	virtual address space.
 *
 *	For now, VM is already on, we only need to map the
 *	specified memory.
 */
vm_offset_t
pmap_map(virt, start, end, prot)
	vm_offset_t	virt;
	vm_offset_t	start;
	vm_offset_t	end;
	int		prot;
{
#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_map(%lx, %lx, %lx, %lx)\n", virt, start, end, prot);
#endif
	while (start < end) {
		pmap_enter(pmap_kernel(), virt, start, prot, FALSE);
		virt += PAGE_SIZE;
		start += PAGE_SIZE;
	}
	return(virt);
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
pmap_create(size)
	vm_size_t	size;
{
	register pmap_t pmap;

#ifdef DEBUG
	if (pmapdebug & (PDB_FOLLOW|PDB_CREATE))
		printf("pmap_create(%lx)\n", size);
#endif
	/*
	 * Software use map does not need a pmap
	 */
	if (size)
		return(NULL);

	/* XXX: is it ok to wait here? */
	pmap = (pmap_t) malloc(sizeof *pmap, M_VMPMAP, M_WAITOK);
#ifdef notifwewait
	if (pmap == NULL)
		panic("pmap_create: cannot allocate a pmap");
#endif
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
	register struct pmap *pmap;
{

#ifdef DEBUG
	if (pmapdebug & (PDB_FOLLOW|PDB_CREATE))
		printf("pmap_pinit(%lx)\n", pmap);
#endif
	/*
	 * No need to allocate page table space yet but we do need a
	 * valid segment table.  Initially, we point everyone at the
	 * "null" segment table.  On the first pmap_enter, a real
	 * segment table will be allocated.
	 */
	pmap->pm_stab = Segtabzero;
	pmap->pm_stpte = Segtabzeropte;
	pmap->pm_stchanged = TRUE;
	pmap->pm_count = 1;
	simple_lock_init(&pmap->pm_lock);
}

/*
 *	Retire the given physical map from service.
 *	Should only be called if the map contains
 *	no valid mappings.
 */
void
pmap_destroy(pmap)
	register pmap_t pmap;
{
	int count;

#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_destroy(%lx)\n", pmap);
#endif
	if (pmap == NULL)
		return;

	simple_lock(&pmap->pm_lock);
	count = --pmap->pm_count;
	simple_unlock(&pmap->pm_lock);
	if (count == 0) {
		pmap_release(pmap);
		free((caddr_t)pmap, M_VMPMAP);
	}
}

/*
 * Release any resources held by the given physical map.
 * Called when a pmap initialized by pmap_pinit is being released.
 * Should only be called if the map contains no valid mappings.
 */
void
pmap_release(pmap)
	register struct pmap *pmap;
{

#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_release(%lx)\n", pmap);
#endif
#ifdef notdef /* DIAGNOSTIC */
	/* count would be 0 from pmap_destroy... */
	simple_lock(&pmap->pm_lock);
	if (pmap->pm_count != 1)
		panic("pmap_release count");
#endif
	if (pmap->pm_ptab)
		kmem_free_wakeup(pt_map, (vm_offset_t)pmap->pm_ptab,
				 ALPHA_MAX_PTSIZE);
	if (pmap->pm_stab != Segtabzero)
		kmem_free_wakeup(st_map, (vm_offset_t)pmap->pm_stab,
				 ALPHA_STSIZE);
}

/*
 *	Add a reference to the specified pmap.
 */
void
pmap_reference(pmap)
	pmap_t	pmap;
{
#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_reference(%lx)\n", pmap);
#endif
	if (pmap != NULL) {
		simple_lock(&pmap->pm_lock);
		pmap->pm_count++;
		simple_unlock(&pmap->pm_lock);
	}
}

/*
 *	Remove the given range of addresses from the specified map.
 *
 *	It is assumed that the start and end are properly
 *	rounded to the page size.
 */
void
pmap_remove(pmap, sva, eva)
	register pmap_t pmap;
	register vm_offset_t sva, eva;
{
	register vm_offset_t nssva;
	register pt_entry_t *pte;
	boolean_t firstpage, needcflush;
	int flags;

#ifdef DEBUG
	if (pmapdebug & (PDB_FOLLOW|PDB_REMOVE|PDB_PROTECT))
		printf("pmap_remove(%lx, %lx, %lx)\n", pmap, sva, eva);
#endif

	if (pmap == NULL)
		return;

#ifdef PMAPSTATS
	remove_stats.calls++;
#endif
	firstpage = TRUE;
	needcflush = FALSE;
	flags = active_pmap(pmap) ? PRM_TFLUSH : 0;
	while (sva < eva) {
		nssva = alpha_trunc_seg(sva) + ALPHA_SEG_SIZE;
		if (nssva == 0 || nssva > eva)
			nssva = eva;
		/*
		 * If VA belongs to an unallocated segment,
		 * skip to the next segment boundary.
		 */
		if (!pmap_ste_v(pmap, sva)) {
			sva = nssva;
			continue;
		}
		/*
		 * Invalidate every valid mapping within this segment.
		 */
		pte = pmap_pte(pmap, sva);
		while (sva < nssva) {
			if (pmap_pte_v(pte)) {
				pmap_remove_mapping(pmap, sva, pte, flags);
				firstpage = FALSE;
			}
			pte++;
			sva += PAGE_SIZE;
		}
	}
	/*
	 * Didn't do anything, no need for cache flushes
	 */
	if (firstpage)
		return;
}

/*
 *	pmap_page_protect:
 *
 *	Lower the permission for all mappings to a given page.
 */
void
pmap_page_protect(pa, prot)
	vm_offset_t	pa;
	vm_prot_t	prot;
{
	register pv_entry_t pv;
	int s;

#ifdef DEBUG
	if ((pmapdebug & (PDB_FOLLOW|PDB_PROTECT)) ||
	    prot == VM_PROT_NONE && (pmapdebug & PDB_REMOVE))
		printf("pmap_page_protect(%lx, %lx)\n", pa, prot);
#endif
	if (pa < vm_first_phys || pa >= vm_last_phys)
		return;

	switch (prot) {
	case VM_PROT_READ|VM_PROT_WRITE:
	case VM_PROT_ALL:
		return;
	/* copy_on_write */
	case VM_PROT_READ:
	case VM_PROT_READ|VM_PROT_EXECUTE:
/* XXX */	pmap_changebit(pa, PG_KWE | PG_UWE, FALSE);
		return;
	/* remove_all */
	default:
		break;
	}
	pv = pa_to_pvh(pa);
	s = splimp();
	while (pv->pv_pmap != NULL) {
		register pt_entry_t *pte;

		pte = pmap_pte(pv->pv_pmap, pv->pv_va);
#ifdef DEBUG
		if (!pmap_ste_v(pv->pv_pmap, pv->pv_va) ||
		    pmap_pte_pa(pte) != pa)
			panic("pmap_page_protect: bad mapping");
#endif
		if (!pmap_pte_w(pte))
			pmap_remove_mapping(pv->pv_pmap, pv->pv_va,
					    pte, PRM_TFLUSH|PRM_CFLUSH);
		else {
			pv = pv->pv_next;
#ifdef DEBUG
			if (pmapdebug & PDB_PARANOIA)
				printf("%s wired mapping for %lx not removed\n",
				       "pmap_page_protect:", pa);
#endif
		}
	}
	splx(s);
}

/*
 *	Set the physical protection on the
 *	specified range of this map as requested.
 */
void
pmap_protect(pmap, sva, eva, prot)
	register pmap_t	pmap;
	register vm_offset_t sva, eva;
	vm_prot_t prot;
{
	register vm_offset_t nssva;
	register pt_entry_t *pte, bits;
	boolean_t firstpage, needtflush;

#ifdef DEBUG
	if (pmapdebug & (PDB_FOLLOW|PDB_PROTECT))
		printf("pmap_protect(%lx, %lx, %lx, %lx)\n", pmap, sva, eva, prot);
#endif

	if (pmap == NULL)
		return;

#ifdef PMAPSTATS
	protect_stats.calls++;
#endif
	if ((prot & VM_PROT_READ) == VM_PROT_NONE) {
		pmap_remove(pmap, sva, eva);
		return;
	}
	if (prot & VM_PROT_WRITE)
		return;

	bits = pte_prot(pmap, prot);
	needtflush = active_pmap(pmap);
	firstpage = TRUE;
	while (sva < eva) {
		nssva = alpha_trunc_seg(sva) + ALPHA_SEG_SIZE;
		if (nssva == 0 || nssva > eva)
			nssva = eva;
		/*
		 * If VA belongs to an unallocated segment,
		 * skip to the next segment boundary.
		 */
		if (!pmap_ste_v(pmap, sva)) {
			sva = nssva;
			continue;
		}
		/*
		 * Change protection on mapping if it is valid and doesn't
		 * already have the correct protection.
		 */
		pte = pmap_pte(pmap, sva);
		while (sva < nssva) {
			if (pmap_pte_v(pte) && pmap_pte_prot_chg(pte, bits)) {
				pmap_pte_set_prot(pte, bits);
				if (needtflush)
					TBIS((caddr_t)sva);
#ifdef PMAPSTATS
				protect_stats.changed++;
#endif
				firstpage = FALSE;
			}
#ifdef PMAPSTATS
			else if (pmap_pte_v(pte)) {
				if (isro)
					protect_stats.alreadyro++;
				else
					protect_stats.alreadyrw++;
			}
#endif
			pte++;
			sva += PAGE_SIZE;
		}
	}
}

/*
 *	Insert the given physical page (p) at
 *	the specified virtual address (v) in the
 *	target physical map with the protection requested.
 *
 *	If specified, the page will be wired down, meaning
 *	that the related pte can not be reclaimed.
 *
 *	NB:  This is the only routine which MAY NOT lazy-evaluate
 *	or lose information.  That is, this routine must actually
 *	insert this page into the given map NOW.
 */
void
pmap_enter(pmap, va, pa, prot, wired)
	register pmap_t pmap;
	vm_offset_t va;
	register vm_offset_t pa;
	vm_prot_t prot;
	boolean_t wired;
{
	register pt_entry_t *pte;
	register pt_entry_t npte;
	vm_offset_t opa;
	boolean_t checkpv = TRUE;

#ifdef DEBUG
	if (pmapdebug & (PDB_FOLLOW|PDB_ENTER))
		printf("pmap_enter(%lx, %lx, %lx, %lx, %lx)\n",
		       pmap, va, pa, prot, wired);
#endif
	if (pmap == NULL)
		return;

#ifdef PMAPSTATS
	if (pmap == pmap_kernel())
		enter_stats.kernel++;
	else
		enter_stats.user++;
#endif
	/*
	 * For user mapping, allocate kernel VM resources if necessary.
	 */
	if (pmap->pm_ptab == NULL)
		pmap->pm_ptab = (pt_entry_t *)
			kmem_alloc_wait(pt_map, ALPHA_MAX_PTSIZE);
	/*
	 * Segment table entry not valid, we need a new PT page
	 */
	if (!pmap_ste_v(pmap, va))
		pmap_enter_ptpage(pmap, va);

	pa = alpha_trunc_page(pa);
	pte = pmap_pte(pmap, va);
	opa = pmap_pte_pa(pte);
#ifdef DEBUG
	if (pmapdebug & PDB_ENTER)
		printf("enter: pte %lx, *pte %lx\n", pte, *pte);
#endif

	/*
	 * Mapping has not changed, must be protection or wiring change.
	 */
	if (opa == pa) {
#ifdef PMAPSTATS
		enter_stats.pwchange++;
#endif
		/*
		 * Wiring change, just update stats.
		 * We don't worry about wiring PT pages as they remain
		 * resident as long as there are valid mappings in them.
		 * Hence, if a user page is wired, the PT page will be also.
		 */
		if (pmap_pte_w_chg(pte, wired ? PG_WIRED : 0)) {
#ifdef DEBUG
			if (pmapdebug & PDB_ENTER)
				printf("enter: wiring change -> %lx\n", wired);
#endif
			if (wired)
				pmap->pm_stats.wired_count++;
			else
				pmap->pm_stats.wired_count--;
#ifdef PMAPSTATS
			if (pmap_pte_prot(pte) == pte_prot(pmap, prot))
				enter_stats.wchange++;
#endif
		}
#ifdef PMAPSTATS
		else if (pmap_pte_prot(pte) != pte_prot(pmap, prot))
			enter_stats.pchange++;
		else
			enter_stats.nochange++;
#endif
		/*
		 * Retain cache inhibition status
		 */
		checkpv = FALSE;
		goto validate;
	}

	/*
	 * Mapping has changed, invalidate old range and fall through to
	 * handle validating new mapping.
	 */
	if (opa) {
#ifdef DEBUG
		if (pmapdebug & PDB_ENTER)
			printf("enter: removing old mapping %lx\n", va);
#endif
		pmap_remove_mapping(pmap, va, pte, PRM_TFLUSH|PRM_CFLUSH);
#ifdef PMAPSTATS
		enter_stats.mchange++;
#endif
	}

	/*
	 * If this is a new user mapping, increment the wiring count
	 * on this PT page.  PT pages are wired down as long as there
	 * is a valid mapping in the page.
	 */
	if (pmap != pmap_kernel())
		(void) vm_map_pageable(pt_map, trunc_page(pte),
				       round_page(pte+1), FALSE);

	/*
	 * Enter on the PV list if part of our managed memory
	 * Note that we raise IPL while manipulating pv_table
	 * since pmap_enter can be called at interrupt time.
	 */
	if (pa >= vm_first_phys && pa < vm_last_phys) {
		register pv_entry_t pv, npv;
		int s;

#ifdef PMAPSTATS
		enter_stats.managed++;
#endif
		pv = pa_to_pvh(pa);
		s = splimp();
#ifdef DEBUG
		if (pmapdebug & PDB_ENTER)
			printf("enter: pv at %lx: %lx/%lx/%lx\n",
			       pv, pv->pv_va, pv->pv_pmap, pv->pv_next);
#endif
		/*
		 * No entries yet, use header as the first entry
		 */
		if (pv->pv_pmap == NULL) {
#ifdef PMAPSTATS
			enter_stats.firstpv++;
#endif
			pv->pv_va = va;
			pv->pv_pmap = pmap;
			pv->pv_next = NULL;
			pv->pv_ptpte = NULL;
			pv->pv_ptpmap = NULL;
			pv->pv_flags = 0;
		}
		/*
		 * There is at least one other VA mapping this page.
		 * Place this entry after the header.
		 */
		else {
#ifdef DEBUG
			for (npv = pv; npv; npv = npv->pv_next)
				if (pmap == npv->pv_pmap && va == npv->pv_va)
					panic("pmap_enter: already in pv_tab");
#endif
			npv = (pv_entry_t)
				malloc(sizeof *npv, M_VMPVENT, M_NOWAIT);
			npv->pv_va = va;
			npv->pv_pmap = pmap;
			npv->pv_next = pv->pv_next;
			npv->pv_ptpte = NULL;
			npv->pv_ptpmap = NULL;
			npv->pv_flags = 0;
			pv->pv_next = npv;
#ifdef PMAPSTATS
			if (!npv->pv_next)
				enter_stats.secondpv++;
#endif
		}
		splx(s);
	}
	/*
	 * Assumption: if it is not part of our managed memory
	 * then it must be device memory which may be volitile.
	 */
	else if (pmap_initialized) {
		checkpv = FALSE;
#ifdef PMAPSTATS
		enter_stats.unmanaged++;
#endif
	}

	/*
	 * Increment counters
	 */
	pmap->pm_stats.resident_count++;
	if (wired)
		pmap->pm_stats.wired_count++;

validate:
	/*
	 * Build the new PTE.
	 */
	npte = ((pa >> PGSHIFT) << PG_SHIFT) | pte_prot(pmap, prot) | PG_V;
	if ((pmap_attributes[pa_index(pa)] & PMAP_ATTR_REF) == 0)
		npte |= PG_FOR | PG_FOW | PG_FOE;
	else if ((pmap_attributes[pa_index(pa)] & PMAP_ATTR_MOD) == 0)
		npte |= PG_FOW;
	if (wired)
		npte |= PG_WIRED;
#ifdef DEBUG
	if (pmapdebug & PDB_ENTER)
		printf("enter: new pte value %lx\n", npte);
#endif
	/*
	 * Remember if this was a wiring-only change.
	 * If so, we need not flush the TLB and caches.
	 */
	wired = ((*pte ^ npte) == PG_WIRED);
	*pte = npte;
	if (!wired && active_pmap(pmap))
		TBIS((caddr_t)va);
#ifdef DEBUG
	if ((pmapdebug & PDB_WIRING) && pmap != pmap_kernel())
		pmap_check_wiring("enter", trunc_page(pmap_pte(pmap, va)));
#endif
}

/*
 *	Routine:	pmap_change_wiring
 *	Function:	Change the wiring attribute for a map/virtual-address
 *			pair.
 *	In/out conditions:
 *			The mapping must already exist in the pmap.
 */
void
pmap_change_wiring(pmap, va, wired)
	register pmap_t	pmap;
	vm_offset_t	va;
	boolean_t	wired;
{
	register pt_entry_t *pte;

#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_change_wiring(%lx, %lx, %lx)\n", pmap, va, wired);
#endif
	if (pmap == NULL)
		return;

	pte = pmap_pte(pmap, va);
#ifdef DEBUG
	/*
	 * Page table page is not allocated.
	 * Should this ever happen?  Ignore it for now,
	 * we don't want to force allocation of unnecessary PTE pages.
	 */
	if (!pmap_ste_v(pmap, va)) {
		if (pmapdebug & PDB_PARANOIA)
			printf("pmap_change_wiring: invalid STE for %lx\n", va);
		return;
	}
	/*
	 * Page not valid.  Should this ever happen?
	 * Just continue and change wiring anyway.
	 */
	if (!pmap_pte_v(pte)) {
		if (pmapdebug & PDB_PARANOIA)
			printf("pmap_change_wiring: invalid PTE for %lx\n", va);
	}
#endif
	/*
	 * If wiring actually changed (always?) set the wire bit and
	 * update the wire count.  Note that wiring is not a hardware
	 * characteristic so there is no need to invalidate the TLB.
	 */
	if (pmap_pte_w_chg(pte, wired ? PG_WIRED : 0)) {
		pmap_pte_set_w(pte, wired);
		if (wired)
			pmap->pm_stats.wired_count++;
		else
			pmap->pm_stats.wired_count--;
	}
}

/*
 *	Routine:	pmap_extract
 *	Function:
 *		Extract the physical page address associated
 *		with the given map/virtual_address pair.
 */

vm_offset_t
pmap_extract(pmap, va)
	register pmap_t	pmap;
	vm_offset_t va;
{
	pt_entry_t pte;
	register vm_offset_t pa;

#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_extract(%lx, %lx) -> ", pmap, va);
#endif
	pa = 0;
	if (pmap && pmap_ste_v(pmap, va)) {
		pte = *pmap_pte(pmap, va);
		if (pte & PG_V)
			pa = ctob(PG_PFNUM(pte)) | (va & PGOFSET);
	}

#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("%lx\n", pa);
#endif
	return(pa);
}

/*
 *	Copy the range specified by src_addr/len
 *	from the source map to the range dst_addr/len
 *	in the destination map.
 *
 *	This routine is only advisory and need not do anything.
 */
void pmap_copy(dst_pmap, src_pmap, dst_addr, len, src_addr)
	pmap_t		dst_pmap;
	pmap_t		src_pmap;
	vm_offset_t	dst_addr;
	vm_size_t	len;
	vm_offset_t	src_addr;
{
#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_copy(%lx, %lx, %lx, %lx, %lx)\n",
		       dst_pmap, src_pmap, dst_addr, len, src_addr);
#endif
}

/*
 *	Require that all active physical maps contain no
 *	incorrect entries NOW.  [This update includes
 *	forcing updates of any address map caching.]
 *
 *	Generally used to insure that a thread about
 *	to run will see a semantically correct world.
 */
void pmap_update()
{
#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_update()\n");
#endif
	TBIA();
}

/*
 *	Routine:	pmap_collect
 *	Function:
 *		Garbage collects the physical map system for
 *		pages which are no longer used.
 *		Success need not be guaranteed -- that is, there
 *		may well be pages which are not referenced, but
 *		others may be collected.
 *	Usage:
 *		Called by the pageout daemon when pages are scarce.
 */
void
pmap_collect(pmap)
	pmap_t		pmap;
{
	register vm_offset_t pa;
	register pv_entry_t pv;
	register pt_entry_t *pte;
	vm_offset_t kpa;
	int s;

#ifdef DEBUG
	pt_entry_t *ste;
	int opmapdebug;
#endif
	if (pmap != pmap_kernel())
		return;

#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_collect(%lx)\n", pmap);
#endif
#ifdef PMAPSTATS
	kpt_stats.collectscans++;
#endif
	s = splimp();
	for (pa = vm_first_phys; pa < vm_last_phys; pa += PAGE_SIZE) {
		register struct kpt_page *kpt, **pkpt;

		/*
		 * Locate physical pages which are being used as kernel
		 * page table pages.
		 */
		pv = pa_to_pvh(pa);
		if (pv->pv_pmap != pmap_kernel() || !(pv->pv_flags & PV_PTPAGE))
			continue;
		do {
			if (pv->pv_ptpte && pv->pv_ptpmap == pmap_kernel())
				break;
		} while (pv = pv->pv_next);
		if (pv == NULL)
			continue;
#ifdef DEBUG
		if (pv->pv_va < (vm_offset_t)Sysmap ||
		    pv->pv_va >= (vm_offset_t)Sysmap + ALPHA_MAX_PTSIZE)
			printf("collect: kernel PT VA out of range\n");
		else
			goto ok;
		pmap_pvdump(pa);
		continue;
ok:
#endif
		pte = (pt_entry_t *)(pv->pv_va + ALPHA_PAGE_SIZE);
		while (--pte >= (pt_entry_t *)pv->pv_va && *pte == PG_NV)
			;
		if (pte >= (pt_entry_t *)pv->pv_va)
			continue;

#ifdef DEBUG
		if (pmapdebug & (PDB_PTPAGE|PDB_COLLECT)) {
			printf("collect: freeing KPT page at %lx (ste %lx@%lx)\n",
			       pv->pv_va, *pv->pv_ptpte, pv->pv_ptpte);
			opmapdebug = pmapdebug;
			pmapdebug |= PDB_PTPAGE;
		}

		ste = pv->pv_ptpte;
#endif
		/*
		 * If all entries were invalid we can remove the page.
		 * We call pmap_remove_entry to take care of invalidating
		 * ST and Sysptmap entries.
		 */
		kpa = pmap_extract(pmap, pv->pv_va);
		pmap_remove_mapping(pmap, pv->pv_va, PT_ENTRY_NULL,
				    PRM_TFLUSH|PRM_CFLUSH);
		/*
		 * Use the physical address to locate the original
		 * (kmem_alloc assigned) address for the page and put
		 * that page back on the free list.
		 */
		for (pkpt = &kpt_used_list, kpt = *pkpt;
		     kpt != (struct kpt_page *)0;
		     pkpt = &kpt->kpt_next, kpt = *pkpt)
			if (kpt->kpt_pa == kpa)
				break;
#ifdef DEBUG
		if (kpt == (struct kpt_page *)0)
			panic("pmap_collect: lost a KPT page");
		if (pmapdebug & (PDB_PTPAGE|PDB_COLLECT))
			printf("collect: %lx (%lx) to free list\n",
			       kpt->kpt_va, kpa);
#endif
		*pkpt = kpt->kpt_next;
		kpt->kpt_next = kpt_free_list;
		kpt_free_list = kpt;
#ifdef PMAPSTATS
		kpt_stats.kptinuse--;
		kpt_stats.collectpages++;
#endif
#ifdef DEBUG
		if (pmapdebug & (PDB_PTPAGE|PDB_COLLECT))
			pmapdebug = opmapdebug;

		if (*ste)
			printf("collect: kernel STE at %lx still valid (%lx)\n",
			       ste, *ste);
		ste = &Sysptmap[(pt_entry_t *)ste-pmap_ste(pmap_kernel(), 0)];
		if (*ste)
			printf("collect: kernel PTmap at %lx still valid (%lx)\n",
			       ste, *ste);
#endif
	}
	splx(s);
}

void
pmap_activate(pmap)
	register pmap_t pmap;
{
	int iscurproc;

#ifdef DEBUG
	if (pmapdebug & (PDB_FOLLOW|PDB_SEGTAB))
		printf("pmap_activate(%lx)\n", pmap);
#endif

	iscurproc = curproc != NULL && pmap == curproc->p_vmspace->vm_map.pmap;
	PMAP_ACTIVATE(pmap, iscurproc);
}

/*
 *	pmap_zero_page zeros the specified (machine independent)
 *	page by mapping the page into virtual memory and using
 *	bzero to clear its contents, one machine dependent page
 *	at a time.
 */
void
pmap_zero_page(phys)
	vm_offset_t phys;
{
	caddr_t p;

#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_zero_page(%lx)\n", phys);
#endif
	p = (caddr_t)phystok0seg(phys);
	bzero(p, PAGE_SIZE);
}

/*
 *	pmap_copy_page copies the specified (machine independent)
 *	page by mapping the page into virtual memory and using
 *	bcopy to copy the page, one machine dependent page at a
 *	time.
 */
void
pmap_copy_page(src, dst)
	vm_offset_t src, dst;
{
	caddr_t s, d;

#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_copy_page(%lx, %lx)\n", src, dst);
#endif
        s = (caddr_t)phystok0seg(src);
        d = (caddr_t)phystok0seg(dst);
	bcopy(s, d, PAGE_SIZE);
}

/*
 *	Routine:	pmap_pageable
 *	Function:
 *		Make the specified pages (by pmap, offset)
 *		pageable (or not) as requested.
 *
 *		A page which is not pageable may not take
 *		a fault; therefore, its page table entry
 *		must remain valid for the duration.
 *
 *		This routine is merely advisory; pmap_enter
 *		will specify that these pages are to be wired
 *		down (or not) as appropriate.
 */
void
pmap_pageable(pmap, sva, eva, pageable)
	pmap_t		pmap;
	vm_offset_t	sva, eva;
	boolean_t	pageable;
{
#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_pageable(%lx, %lx, %lx, %lx)\n",
		       pmap, sva, eva, pageable);
#endif
	/*
	 * If we are making a PT page pageable then all valid
	 * mappings must be gone from that page.  Hence it should
	 * be all zeros and there is no need to clean it.
	 * Assumptions:
	 *	- we are called with only one page at a time
	 *	- PT pages have only one pv_table entry
	 */
	if (pmap == pmap_kernel() && pageable && sva + PAGE_SIZE == eva) {
		register pv_entry_t pv;
		register vm_offset_t pa;

#ifdef DEBUG
		if ((pmapdebug & (PDB_FOLLOW|PDB_PTPAGE)) == PDB_PTPAGE)
			printf("pmap_pageable(%lx, %lx, %lx, %lx)\n",
			       pmap, sva, eva, pageable);
#endif
		if (!pmap_ste_v(pmap, sva))
			return;
		pa = pmap_pte_pa(pmap_pte(pmap, sva));
		if (pa < vm_first_phys || pa >= vm_last_phys)
			return;
		pv = pa_to_pvh(pa);
		if (pv->pv_ptpte == NULL)
			return;
#ifdef DEBUG
		if (pv->pv_va != sva || pv->pv_next) {
			printf("pmap_pageable: bad PT page va %lx next %lx\n",
			       pv->pv_va, pv->pv_next);
			return;
		}
#endif
		/*
		 * Mark it unmodified to avoid pageout
		 */
		pmap_clear_modify(pa);
#ifdef DEBUG
		if ((PHYS_TO_VM_PAGE(pa)->flags & PG_CLEAN) == 0) {
			printf("pa %lx: flags=%lx: not clean\n",
			       pa, PHYS_TO_VM_PAGE(pa)->flags);
			PHYS_TO_VM_PAGE(pa)->flags |= PG_CLEAN;
		}
		if (pmapdebug & PDB_PTPAGE)
			printf("pmap_pageable: PT page %lx(%lx) unmodified\n",
			       sva, *pmap_pte(pmap, sva));
		if (pmapdebug & PDB_WIRING)
			pmap_check_wiring("pageable", sva);
#endif
	}
}

/*
 *	Clear the modify bits on the specified physical page.
 */

void
pmap_clear_modify(pa)
	vm_offset_t	pa;
{
#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_clear_modify(%lx)\n", pa);
#endif
	if ((pmap_attributes[pa_index(pa)] & PMAP_ATTR_MOD) != 0) {
		pmap_changebit(pa, PG_FOW, TRUE);
		pmap_attributes[pa_index(pa)] &= ~PMAP_ATTR_MOD;
	}
}

/*
 *	pmap_clear_reference:
 *
 *	Clear the reference bit on the specified physical page.
 */

void pmap_clear_reference(pa)
	vm_offset_t	pa;
{
#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_clear_reference(%lx)\n", pa);
#endif
	if ((pmap_attributes[pa_index(pa)] & PMAP_ATTR_REF) != 0) {
		pmap_changebit(pa, PG_FOR | PG_FOW | PG_FOE, TRUE);
		pmap_attributes[pa_index(pa)] &= ~PMAP_ATTR_REF;
	}
}

/*
 *	pmap_is_referenced:
 *
 *	Return whether or not the specified physical page is referenced
 *	by any physical maps.
 */

boolean_t
pmap_is_referenced(pa)
	vm_offset_t	pa;
{
	boolean_t rv;

	rv = (pmap_attributes[pa_index(pa)] & PMAP_ATTR_REF) != 0;
#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW) {
		printf("pmap_is_referenced(%lx) -> %c\n", pa, "FT"[rv]);
	}
#endif
	return rv;
}

/*
 *	pmap_is_modified:
 *
 *	Return whether or not the specified physical page is modified
 *	by any physical maps.
 */

boolean_t
pmap_is_modified(pa)
	vm_offset_t	pa;
{
	boolean_t rv;

	rv = (pmap_attributes[pa_index(pa)] & PMAP_ATTR_MOD) != 0;
#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW) {
		printf("pmap_is_modified(%lx) -> %c\n", pa, "FT"[rv]);
	}
#endif
	return rv;
}

vm_offset_t
pmap_phys_address(ppn)
	int ppn;
{
	return(alpha_ptob(ppn));
}

/*
 * Miscellaneous support routines follow
 */

/*
 * Initialize Alpha protection code array.
 */
/* static */
void
alpha_protection_init()
{
	int prot, *kp, *up;

	kp = protection_codes[0];
	up = protection_codes[1];

	for (prot = 0; prot < 8; prot++) {
		switch (prot) {
		case VM_PROT_NONE | VM_PROT_NONE | VM_PROT_NONE:
			*kp++ = PG_ASM;
			*up++ = 0;
			break;
		case VM_PROT_READ | VM_PROT_NONE | VM_PROT_NONE:
		case VM_PROT_READ | VM_PROT_NONE | VM_PROT_EXECUTE:
		case VM_PROT_NONE | VM_PROT_NONE | VM_PROT_EXECUTE:
			*kp++ = PG_ASM | PG_KRE;
			*up++ = PG_URE | PG_KRE;
			break;
		case VM_PROT_NONE | VM_PROT_WRITE | VM_PROT_NONE:
			*kp++ = PG_ASM | PG_KWE;
			*up++ = PG_UWE | PG_KWE;
			break;
		case VM_PROT_NONE | VM_PROT_WRITE | VM_PROT_EXECUTE:
		case VM_PROT_READ | VM_PROT_WRITE | VM_PROT_NONE:
		case VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE:
			*kp++ = PG_ASM | PG_KWE | PG_KRE;
			*up++ = PG_UWE | PG_URE | PG_KWE | PG_KRE;
			break;
		}
	}
}

/*
 * Invalidate a single page denoted by pmap/va.
 * If (pte != NULL), it is the already computed PTE for the page.
 * If (flags & PRM_TFLUSH), we must invalidate any TLB information.
 * If (flags & PRM_CFLUSH), we must flush/invalidate any cache information.
 */
/* static */
void
pmap_remove_mapping(pmap, va, pte, flags)
	register pmap_t pmap;
	register vm_offset_t va;
	register pt_entry_t *pte;
	int flags;
{
	register vm_offset_t pa;
	register pv_entry_t pv, npv;
	pmap_t ptpmap;
	pt_entry_t *ste;
	int s;
#ifdef DEBUG
	pt_entry_t opte;

	if (pmapdebug & (PDB_FOLLOW|PDB_REMOVE|PDB_PROTECT))
		printf("pmap_remove_mapping(%lx, %lx, %lx, %lx)\n",
		       pmap, va, pte, flags);
#endif

	/*
	 * PTE not provided, compute it from pmap and va.
	 */
	if (pte == PT_ENTRY_NULL) {
		pte = pmap_pte(pmap, va);
		if (*pte == PG_NV)
			return;
	}
	pa = pmap_pte_pa(pte);
#ifdef DEBUG
	opte = *pte;
#endif
#ifdef PMAPSTATS
	remove_stats.removes++;
#endif
	/*
	 * Update statistics
	 */
	if (pmap_pte_w(pte))
		pmap->pm_stats.wired_count--;
	pmap->pm_stats.resident_count--;

	/*
	 * Invalidate the PTE after saving the reference modify info.
	 */
#ifdef DEBUG
	if (pmapdebug & PDB_REMOVE)
		printf("remove: invalidating pte at %lx\n", pte);
#endif
	*pte = PG_NV;
	if ((flags & PRM_TFLUSH) && active_pmap(pmap))
		TBIS((caddr_t)va);
	/*
	 * For user mappings decrement the wiring count on
	 * the PT page.  We do this after the PTE has been
	 * invalidated because vm_map_pageable winds up in
	 * pmap_pageable which clears the modify bit for the
	 * PT page.
	 */
	if (pmap != pmap_kernel()) {
		(void) vm_map_pageable(pt_map, trunc_page(pte),
				       round_page(pte+1), TRUE);
#ifdef DEBUG
		if (pmapdebug & PDB_WIRING)
			pmap_check_wiring("remove", trunc_page(pte));
#endif
	}
	/*
	 * If this isn't a managed page, we are all done.
	 */
	if (pa < vm_first_phys || pa >= vm_last_phys)
		return;
	/*
	 * Otherwise remove it from the PV table
	 * (raise IPL since we may be called at interrupt time).
	 */
	pv = pa_to_pvh(pa);
	ste = NULL;
	s = splimp();
	/*
	 * If it is the first entry on the list, it is actually
	 * in the header and we must copy the following entry up
	 * to the header.  Otherwise we must search the list for
	 * the entry.  In either case we free the now unused entry.
	 */
	if (pmap == pv->pv_pmap && va == pv->pv_va) {
		ste = pv->pv_ptpte;
		ptpmap = pv->pv_ptpmap;
		npv = pv->pv_next;
		if (npv) {
			npv->pv_flags = pv->pv_flags;
			*pv = *npv;
			free((caddr_t)npv, M_VMPVENT);
		} else
			pv->pv_pmap = NULL;
#ifdef PMAPSTATS
		remove_stats.pvfirst++;
#endif
	} else {
		for (npv = pv->pv_next; npv; npv = npv->pv_next) {
#ifdef PMAPSTATS
			remove_stats.pvsearch++;
#endif
			if (pmap == npv->pv_pmap && va == npv->pv_va)
				break;
			pv = npv;
		}
#ifdef DEBUG
		if (npv == NULL)
			panic("pmap_remove: PA not in pv_tab");
#endif
		ste = npv->pv_ptpte;
		ptpmap = npv->pv_ptpmap;
		pv->pv_next = npv->pv_next;
		free((caddr_t)npv, M_VMPVENT);
		pv = pa_to_pvh(pa);
	}
	/*
	 * If this was a PT page we must also remove the
	 * mapping from the associated segment table.
	 */
	if (ste) {
#ifdef PMAPSTATS
		remove_stats.ptinvalid++;
#endif
#ifdef DEBUG
		if (pmapdebug & (PDB_REMOVE|PDB_PTPAGE))
			printf("remove: ste was %lx@%lx pte was %lx@%lx\n",
			       *ste, ste, opte, pmap_pte(pmap, va));
#endif
		*ste = PG_NV;
		/*
		 * If it was a user PT page, we decrement the
		 * reference count on the segment table as well,
		 * freeing it if it is now empty.
		 */
		if (ptpmap != pmap_kernel()) {
#ifdef DEBUG
			if (pmapdebug & (PDB_REMOVE|PDB_SEGTAB))
				printf("remove: stab %lx, refcnt %d\n",
				       ptpmap->pm_stab, ptpmap->pm_sref - 1);
			if ((pmapdebug & PDB_PARANOIA) &&
			    ptpmap->pm_stab != (pt_entry_t *)trunc_page(ste))
				panic("remove: bogus ste");
#endif
			if (--(ptpmap->pm_sref) == 0) {
#ifdef DEBUG
				if (pmapdebug&(PDB_REMOVE|PDB_SEGTAB))
					printf("remove: free stab %lx\n",
					       ptpmap->pm_stab);
#endif
				kmem_free_wakeup(st_map,
						 (vm_offset_t)ptpmap->pm_stab,
						 ALPHA_STSIZE);
				ptpmap->pm_stab = Segtabzero;
				ptpmap->pm_stpte = Segtabzeropte;
				ptpmap->pm_stchanged = TRUE;
				/*
				 * XXX may have changed segment table
				 * pointer for current process so
				 * update now to reload hardware.
				 * (curproc may be NULL if exiting.)
				 */
				if (curproc != NULL &&
				    ptpmap == curproc->p_vmspace->vm_map.pmap)
					PMAP_ACTIVATE(ptpmap, 1);
			}
#ifdef DEBUG
			else if (ptpmap->pm_sref < 0)
				panic("remove: sref < 0");
#endif
		}
#if 0
		/*
		 * XXX this should be unnecessary as we have been
		 * flushing individual mappings as we go.
		 */
		if (ptpmap == pmap_kernel())
			TBIAS();
		else
			TBIAU();
#endif
		pv->pv_flags &= ~PV_PTPAGE;
		ptpmap->pm_ptpages--;
	}
	splx(s);
}

/* static */
void
pmap_changebit(pa, bit, setem)
	register vm_offset_t pa;
	u_long bit;
	boolean_t setem;
{
	register pv_entry_t pv;
	register pt_entry_t *pte, npte;
	vm_offset_t va;
	int s;
	boolean_t firstpage = TRUE;
#ifdef PMAPSTATS
	struct chgstats *chgp;
#endif

#ifdef DEBUG
	if (pmapdebug & PDB_BITS)
		printf("pmap_changebit(%lx, %lx, %s)\n",
		       pa, bit, setem ? "set" : "clear");
#endif
	if (pa < vm_first_phys || pa >= vm_last_phys)
		return;

#ifdef PMAPSTATS
	chgp = &changebit_stats[(bit>>2)-1];
	if (setem)
		chgp->setcalls++;
	else
		chgp->clrcalls++;
#endif
	pv = pa_to_pvh(pa);
	s = splimp();
	/*
	 * Loop over all current mappings setting/clearing as appropos
	 * If setting RO do we need to clear the VAC?
	 */
	if (pv->pv_pmap != NULL) {
#ifdef DEBUG
		int toflush = 0;
#endif
		for (; pv; pv = pv->pv_next) {
#ifdef DEBUG
			toflush |= (pv->pv_pmap == pmap_kernel()) ? 2 : 1;
#endif
			va = pv->pv_va;

			/*
			 * XXX don't write protect pager mappings
			 */
/* XXX */		if (bit == (PG_UWE | PG_KWE)) {
				extern vm_offset_t pager_sva, pager_eva;

				if (va >= pager_sva && va < pager_eva)
					continue;
			}

			pte = pmap_pte(pv->pv_pmap, va);
			if (setem)
				npte = *pte | bit;
			else
				npte = *pte & ~bit;
			if (*pte != npte) {
				*pte = npte;
				if (active_pmap(pv->pv_pmap))
					TBIS((caddr_t)va);
#ifdef PMAPSTATS
				if (setem)
					chgp->sethits++;
				else
					chgp->clrhits++;
#endif
			}
#ifdef PMAPSTATS
			else {
				if (setem)
					chgp->setmiss++;
				else
					chgp->clrmiss++;
			}
#endif
		}
	}
	splx(s);
}

/* static */
void
pmap_enter_ptpage(pmap, va)
	register pmap_t pmap;
	register vm_offset_t va;
{
	register vm_offset_t ptpa;
	register pv_entry_t pv;
	pt_entry_t *ste;
	int s;

#ifdef DEBUG
	if (pmapdebug & (PDB_FOLLOW|PDB_ENTER|PDB_PTPAGE))
		printf("pmap_enter_ptpage: pmap %lx, va %lx\n", pmap, va);
#endif
#ifdef PMAPSTATS
	enter_stats.ptpneeded++;
#endif
	/*
	 * Allocate a segment table if necessary.  Note that it is allocated
	 * from a private map and not pt_map.  This keeps user page tables
	 * aligned on segment boundaries in the kernel address space.
	 * The segment table is wired down.  It will be freed whenever the
	 * reference count drops to zero.
	 */
	if (pmap->pm_stab == Segtabzero) {
		pmap->pm_stab = (pt_entry_t *)
			kmem_alloc(st_map, ALPHA_STSIZE);
		pmap->pm_stpte = *kvtopte(pmap->pm_stab);
		pmap->pm_stchanged = TRUE;
		/*
		 * XXX may have changed segment table pointer for current
		 * process so update now to reload hardware.
		 */
		if (pmap == curproc->p_vmspace->vm_map.pmap)
			PMAP_ACTIVATE(pmap, 1);
#ifdef DEBUG
		if (pmapdebug & (PDB_ENTER|PDB_PTPAGE|PDB_SEGTAB))
			printf("enter: pmap %lx stab %lx(%lx)\n",
			       pmap, pmap->pm_stab, pmap->pm_stpte);
#endif
	}

	ste = pmap_ste(pmap, va);
	va = trunc_page((vm_offset_t)pmap_pte(pmap, va));

	/*
	 * In the kernel we allocate a page from the kernel PT page
	 * free list and map it into the kernel page table map (via
	 * pmap_enter).
	 */
	if (pmap == pmap_kernel()) {
		register struct kpt_page *kpt;

		s = splimp();
		if ((kpt = kpt_free_list) == (struct kpt_page *)0) {
			/*
			 * No PT pages available.
			 * Try once to free up unused ones.
			 */
#ifdef DEBUG
			if (pmapdebug & PDB_COLLECT)
				printf("enter: no KPT pages, collecting...\n");
#endif
			pmap_collect(pmap_kernel());
			if ((kpt = kpt_free_list) == (struct kpt_page *)0)
				panic("pmap_enter_ptpage: can't get KPT page");
		}
#ifdef PMAPSTATS
		if (++kpt_stats.kptinuse > kpt_stats.kptmaxuse)
			kpt_stats.kptmaxuse = kpt_stats.kptinuse;
#endif
		kpt_free_list = kpt->kpt_next;
		kpt->kpt_next = kpt_used_list;
		kpt_used_list = kpt;
		ptpa = kpt->kpt_pa;
		bzero((caddr_t)kpt->kpt_va, ALPHA_PAGE_SIZE);
		pmap_enter(pmap, va, ptpa, VM_PROT_DEFAULT, TRUE);
#ifdef DEBUG
		if (pmapdebug & (PDB_ENTER|PDB_PTPAGE)) {
			int ix = pmap_ste(pmap, va) - pmap_ste(pmap, 0);

			printf("enter: add &Sysptmap[%d]: %lx (KPT page %lx)\n",
			       ix, Sysptmap[ix], kpt->kpt_va);
		}
#endif
		splx(s);
	}
	/*
	 * For user processes we just simulate a fault on that location
	 * letting the VM system allocate a zero-filled page.
	 */
	else {
		/*
		 * Count the segment table reference now so that we won't
		 * lose the segment table when low on memory.
		 */
		pmap->pm_sref++;
#ifdef DEBUG
		if (pmapdebug & (PDB_ENTER|PDB_PTPAGE))
			printf("enter: about to fault UPT pg at %lx\n", va);
#endif
		s = vm_fault(pt_map, va, VM_PROT_READ|VM_PROT_WRITE, FALSE);
		if (s != KERN_SUCCESS) {
			printf("vm_fault(pt_map, %lx, RW, 0) -> %d\n", va, s);
			panic("pmap_enter: vm_fault failed");
		}
		ptpa = pmap_extract(pmap_kernel(), va);
		/*
		 * Mark the page clean now to avoid its pageout (and
		 * hence creation of a pager) between now and when it
		 * is wired; i.e. while it is on a paging queue.
		 */
		PHYS_TO_VM_PAGE(ptpa)->flags |= PG_CLEAN;
#ifdef DEBUG
		PHYS_TO_VM_PAGE(ptpa)->flags |= PG_PTPAGE;
#endif
	}
	/*
	 * Locate the PV entry in the kernel for this PT page and
	 * record the STE address.  This is so that we can invalidate
	 * the STE when we remove the mapping for the page.
	 */
	pv = pa_to_pvh(ptpa);
	s = splimp();
	if (pv) {
		pv->pv_flags |= PV_PTPAGE;
		do {
			if (pv->pv_pmap == pmap_kernel() && pv->pv_va == va)
				break;
		} while (pv = pv->pv_next);
	}
#ifdef DEBUG
	if (pv == NULL)
		panic("pmap_enter_ptpage: PT page not entered");
#endif
	pv->pv_ptpte = ste;
	pv->pv_ptpmap = pmap;
#ifdef DEBUG
	if (pmapdebug & (PDB_ENTER|PDB_PTPAGE))
		printf("enter: new PT page at PA %lx, ste at %lx\n", ptpa, ste);
#endif

	/*
	 * Map the new PT page into the segment table.
	 * Reference count on the user segment tables incremented above
	 * to prevent race conditions.  Note that we don't use vm_map_pageable
	 * to keep the count like we do for PT pages, this is mostly because
	 * it would be difficult to identify ST pages in pmap_pageable to
	 * release them.  We also avoid the overhead of vm_map_pageable.
	 */
	*ste = ((ptpa >> PGSHIFT) << PG_SHIFT) | PG_KRE | PG_KWE | PG_V |
	    (pmap == pmap_kernel() ? PG_ASM : 0);
	if (pmap != pmap_kernel()) {
#ifdef DEBUG
		if (pmapdebug & (PDB_ENTER|PDB_PTPAGE|PDB_SEGTAB))
			printf("enter: stab %lx refcnt %d\n",
			       pmap->pm_stab, pmap->pm_sref);
#endif
	}
#if 0
	/*
	 * Flush stale TLB info.
	 */
	if (pmap == pmap_kernel())
		TBIAS();
	else
		TBIAU();
#endif
	pmap->pm_ptpages++;
	splx(s);
}

/*
 * Emulate reference and/or modified bit hits.
 */
void
pmap_emulate_reference(p, v, user, write)
	struct proc *p;
	vm_offset_t v;
	int user;
	int write;
{
	pt_entry_t faultoff, *pte;
	vm_offset_t pa;
	char attr;

#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_emulate_reference: 0x%lx, 0x%lx, %d, %d\n",
		    p, v, user, write);
#endif

	/*
	 * Convert process and virtual address to physical address.
	 */
	if (v >= VM_MIN_KERNEL_ADDRESS) {
		if (user)
			panic("pmap_emulate_reference: user ref to kernel");
		pte = kvtopte(v);
	} else {
#ifdef DIAGNOSTIC
		if (p == NULL)
			panic("pmap_emulate_reference: bad proc");
		if (p->p_vmspace == NULL)
			panic("pmap_emulate_reference: bad p_vmspace");
#endif
		pte = pmap_pte(&p->p_vmspace->vm_pmap, v);
	}
#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW) {
		printf("\tpte = 0x%lx, ", pte);
		printf("*pte = 0x%lx\n", *pte);
	}
#endif
#ifdef DEBUG				/* These checks are more expensive */
	if (!pmap_pte_v(pte))
		panic("pmap_emulate_reference: invalid pte");
#if 0
	/*
	 * Can't do these, because cpu_fork and cpu_swapin call
	 * pmap_emulate_reference(), and the bits aren't guaranteed,
	 * for them...
	 */
	if (write) {
		if (!(*pte & (user ? PG_UWE : PG_UWE | PG_KWE)))
			panic("pmap_emulate_reference: write but unwritable");
		if (!(*pte & PG_FOW))
			panic("pmap_emulate_reference: write but not FOW");
	} else {
		if (!(*pte & (user ? PG_URE : PG_URE | PG_KRE)))
			panic("pmap_emulate_reference: !write but unreadable");
		if (!(*pte & (PG_FOR | PG_FOE)))
			panic("pmap_emulate_reference: !write but not FOR|FOE");
	}
#endif
	/* Other diagnostics? */
#endif
	pa = pmap_pte_pa(pte);
#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("\tpa = 0x%lx\n", pa);
#endif

	/*
	 * Twiddle the appropriate bits to reflect the reference
	 * and/or modification..
	 *
	 * The rules:
	 * 	(1) always mark page as used, and
	 *	(2) if it was a write fault, mark page as modified.
	 */
	attr = PMAP_ATTR_REF;
	faultoff = PG_FOR | PG_FOE;
	if (write) {
		attr |= PMAP_ATTR_MOD;
		faultoff |= PG_FOW;
	}
	pmap_attributes[pa_index(pa)] |= attr;
	pmap_changebit(pa, faultoff, FALSE);
	if ((*pte & faultoff) != 0) {
#if 0
		/*
		 * This is apparently normal.  Why? -- cgd
		 */
		printf("warning: pmap_changebit didn't.");
#endif
		*pte &= ~faultoff;
		TBIS((caddr_t)v);
	}
}

#ifdef DEBUG
/* static */
void
pmap_pvdump(pa)
	vm_offset_t pa;
{
	register pv_entry_t pv;

	printf("pa %lx", pa);
	for (pv = pa_to_pvh(pa); pv; pv = pv->pv_next)
		printf(" -> pmap %lx, va %lx, stpte %lx, ptpmap %lx, flags %lx",
		       pv->pv_pmap, pv->pv_va, pv->pv_ptpte, pv->pv_ptpmap,
		       pv->pv_flags);
	printf("\n");
}

/* static */
void
pmap_check_wiring(str, va)
	char *str;
	vm_offset_t va;
{
	vm_map_entry_t entry;
	pt_entry_t *pte;
	register int count;

	va = trunc_page(va);
	if (!pmap_ste_v(pmap_kernel(), va) ||
	    !pmap_pte_v(pmap_pte(pmap_kernel(), va)))
		return;

	if (!vm_map_lookup_entry(pt_map, va, &entry)) {
		printf("wired_check: entry for %lx not found\n", va);
		return;
	}
	count = 0;
	for (pte = (pt_entry_t *)va; pte < (pt_entry_t *)(va+PAGE_SIZE); pte++)
		if (*pte)
			count++;
	if (entry->wired_count != count)
		printf("*%s*: %lx: w%d/a%d\n",
		       str, va, entry->wired_count, count);
}
#endif
