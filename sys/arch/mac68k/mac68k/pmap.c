/*	$OpenBSD: pmap.c,v 1.11 1999/01/11 05:11:36 millert Exp $	*/
/*	$NetBSD: pmap.c,v 1.28 1996/10/21 05:42:27 scottr Exp $	*/

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

/*
 * Derived from HP9000/300 series physical map management code.
 *
 * Supports:
 *	68020 with 68851 MMU	Mac II
 *	68030 with on-chip MMU	IIcx, etc.
 *	68040 with on-chip MMU	Quadras, etc.
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
 *		(*pte & PG_PROT) [4] != pte->pg_prot [1]
 *	and a couple of routines that deal with protection and wiring take
 *	some shortcuts that assume the and/or definitions.
 *
 *	This implementation will only work for PAGE_SIZE == NBPG
 *	(i.e. 4096 bytes).
 */

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

#include <machine/pte.h>

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
int debugmap = 0;
int pmapdebug = 0x2000;
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
#define PDB_PARANOIA	0x2000
#define PDB_WIRING	0x4000
#define PDB_PVDUMP	0x8000

#if defined(M68040)
int dowriteback = 1;	/* 68040: enable writeback caching */
int dokwriteback = 1;	/* 68040: enable writeback caching of kernel AS */
#endif

extern vm_offset_t pager_sva, pager_eva;
#endif

/*
 * Get STEs and PTEs for user/kernel address space
 */
#if defined(M68040)
#define pmap_ste1(m, v) \
	(&((m)->pm_stab[(vm_offset_t)(v) >> SG4_SHIFT1]))
/* XXX assumes physically contiguous ST pages (if more than one) */
#define pmap_ste2(m, v) \
	(&((m)->pm_stab[(st_entry_t *)(*(u_int *)pmap_ste1(m, v) & SG4_ADDR1) \
			- (m)->pm_stpa + (((v) & SG4_MASK2) >> SG4_SHIFT2)]))
#define pmap_ste(m, v) \
	(&((m)->pm_stab[(vm_offset_t)(v) \
			>> (mmutype == MMU_68040 ? SG4_SHIFT1 : SG_ISHIFT)]))
#define pmap_ste_v(m, v) \
	(mmutype == MMU_68040			\
	 ? ((*pmap_ste1(m, v) & SG_V) &&	\
	    (*pmap_ste2(m, v) & SG_V))  	\
	 : (*pmap_ste(m, v) & SG_V))
#else
#define	pmap_ste(m, v)   (&((m)->pm_stab[(vm_offset_t)(v) >> SG_ISHIFT]))
#define	pmap_ste_v(m, v) (*pmap_ste(m, v) & SG_V)
#endif

#define pmap_pte(m, v)	(&((m)->pm_ptab[(vm_offset_t)(v) >> PG_SHIFT]))
#define pmap_pte_pa(pte)	(*(pte) & PG_FRAME)
#define pmap_pte_w(pte)		(*(pte) & PG_W)
#define pmap_pte_ci(pte)	(*(pte) & PG_CI)
#define pmap_pte_m(pte)		(*(pte) & PG_M)
#define pmap_pte_u(pte)		(*(pte) & PG_U)
#define pmap_pte_prot(pte)	(*(pte) & PG_PROT)
#define pmap_pte_v(pte)		(*(pte) & PG_V)

#define pmap_pte_set_w(pte, v)	  \
	if (v) *(pte) |= PG_W; else *(pte) &= ~PG_W
#define pmap_pte_set_prot(pte, v) \
	if (v) *(pte) |= PG_PROT; else *(pte) &= ~PG_PROT
#define pmap_pte_w_chg(pte, nw)		((nw) ^ pmap_pte_w(pte))
#define pmap_pte_prot_chg(pte, np)	((np) ^ pmap_pte_prot(pte))

#define pmap_valid_page(pa)	(pmap_initialized && pmap_page_index(pa) >= 0)

int pmap_page_index(vm_offset_t pa);

/*
 * Given a map and a machine independent protection code,
 * convert to a m68k protection code.
 */
#define pte_prot(m, p)	(protection_codes[p])
int	protection_codes[8];

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
 * Kernel segment/page table and page table map.
 * The page table map gives us a level of indirection we need to dynamically
 * expand the page table.  It is essentially a copy of the segment table
 * with PTEs instead of STEs.  All are initialized in locore at boot time.
 * Sysmap will initially contain VM_KERNEL_PT_PAGES pages of PTEs.
 * Segtabzero is an empty segment table which all processes share til they
 * reference something.
 */
st_entry_t	*Sysseg;
pt_entry_t	*Sysmap, *Sysptmap;
st_entry_t	*Segtabzero, *Segtabzeropa;
vm_size_t	Sysptsize = VM_KERNEL_PT_PAGES;

struct pmap	kernel_pmap_store;
vm_map_t	st_map, pt_map;

vm_offset_t    	avail_start;	/* PA of first available physical page */
vm_offset_t	avail_next;	/* Next available physical page		*/
int		avail_remaining;/* Number of physical free pages left	*/
int		avail_range;	/* Range avail_next is in		*/
vm_offset_t	avail_end;	/* Set for ps and friends as		*/
				/*       avail_start + avail_remaining. */
vm_size_t	mem_size;	/* memory size in bytes */
vm_offset_t	virtual_avail;  /* VA of first avail page (after kernel bss)*/
vm_offset_t	virtual_end;	/* VA of last avail page (end of kernel AS) */
int		npages;

boolean_t	pmap_initialized = FALSE;	/* Has pmap_init completed? */
struct pv_entry	*pv_table;
char		*pmap_attributes;	/* reference and modify bits */
TAILQ_HEAD(pv_page_list, pv_page) pv_page_freelist;
int		pv_nfree;

/* The following three variables are defined in pmap_bootstrap.c */
extern int		numranges;
extern unsigned long	low[8];
extern unsigned long	high[8];

#if defined(M68040)
int		protostfree;	/* prototype (default) free ST map */
#endif

/*
 * Internal routines
 */
void	pmap_remove_mapping  __P((pmap_t, vm_offset_t, pt_entry_t *, int));
boolean_t	pmap_testbit __P((vm_offset_t, int));
void	pmap_changebit       __P((vm_offset_t, int, boolean_t));
void	pmap_enter_ptpage    __P((pmap_t, vm_offset_t));
#ifdef DEBUG
void	pmap_pvdump          __P((vm_offset_t));
void	pmap_check_wiring    __P((char *, vm_offset_t));
#endif

/* pmap_remove_mapping flags */
#define	PRM_TFLUSH	1
#define	PRM_CFLUSH	2

/*
 * Bootstrap memory allocator. This function allows for early dynamic
 * memory allocation until the virtual memory system has been bootstrapped.
 * After that point, either kmem_alloc or malloc should be used. This
 * function works by stealing pages from the (to be) managed page pool,
 * stealing virtual address space, then mapping the pages and zeroing them.
 *
 * It should be used from pmap_bootstrap till vm_page_startup, afterwards
 * it cannot be used, and will generate a panic if tried. Note that this
 * memory will never be freed, and in essence it is wired down.
 */
void *
pmap_bootstrap_alloc(size)
	int size;
{
	extern boolean_t vm_page_startup_initialized;
	vm_offset_t val;
	
	if (vm_page_startup_initialized)
		panic("pmap_bootstrap_alloc: called after startup initialized");
	size = round_page(size);
	val = virtual_avail;

	virtual_avail = pmap_map(virtual_avail, avail_start,
		avail_start + size, VM_PROT_READ|VM_PROT_WRITE);
	avail_start += size;

	avail_remaining -= mac68k_btop (size);
	/* XXX hope this doesn't pop it into the next range: */
	avail_next += size;

	bzero ((caddr_t) val, size);
	return ((void *) val);
}

/*
 *	Initialize the pmap module.
 *	Called by vm_init, to initialize any structures that the pmap
 *	system needs to map virtual memory.
 */
void
pmap_init()
{
	vm_offset_t	addr, addr2;
	vm_size_t	s;
	int		rv;

#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_init()\n");
#endif
	/*
	 * Now that kernel map has been allocated, we can mark as
	 * unavailable regions which we have mapped in pmap_bootstrap.
	 */
	addr = (vm_offset_t) IOBase;
	(void) vm_map_find(kernel_map, NULL, (vm_offset_t) 0, &addr,
			   mac68k_ptob(IIOMAPSIZE + ROMMAPSIZE + NBMAPSIZE),
			   FALSE);
	if (addr != (vm_offset_t)IOBase)
		panic("pmap_init: I/O space not mapped!");

	addr = (vm_offset_t) Sysmap;
	vm_object_reference(kernel_object);
	(void) vm_map_find(kernel_map, kernel_object, addr,
			   &addr, MAC_MAX_PTSIZE, FALSE);
	/*
	 * If this fails it is probably because the static portion of
	 * the kernel page table isn't big enough and we overran the
	 * page table map.   Need to adjust pmap_size() in mac68k_init.c.
	 */
	if (addr != (vm_offset_t)Sysmap)
		panic("pmap_init: bogons in the VM system!");

#ifdef DEBUG
	if (pmapdebug & PDB_INIT) {
		printf("pmap_init: Sysseg %p, Sysmap %p, Sysptmap %p\n",
		       Sysseg, Sysmap, Sysptmap);
		printf("  pstart %lx, plast %x, vstart %lx, vend %lx\n",
		    avail_start, avail_remaining, virtual_avail, virtual_end);
	}
#endif

	/*
	 * Allocate memory for random pmap data structures.  Includes the
	 * initial segment table, pv_head_table and pmap_attributes.
	 */
	/*
	 * This is wasteful on MACHINE_NONCONTIG.  Is it avoidable?
	 */
	npages = atop(high[numranges - 1] - 1);
	s = (vm_size_t) (MAC_STSIZE + sizeof(struct pv_entry)*npages + npages);
	s = round_page(s);
	addr = (vm_offset_t) kmem_alloc(kernel_map, s);
	Segtabzero = (st_entry_t *) addr;
	Segtabzeropa = (st_entry_t *) pmap_extract(pmap_kernel(), addr);
	addr += MAC_STSIZE;
	pv_table = (struct pv_entry *) addr;
	addr += sizeof(struct pv_entry) * npages;
	pmap_attributes = (char *) addr;
#ifdef DEBUG
	if (pmapdebug & PDB_INIT)
		printf("pmap_init: %lx bytes: npages %x s0 %p(%p) tbl %p atr %p\n",
		       s, npages, Segtabzero, Segtabzeropa,
		       pv_table, pmap_attributes);
#endif

	/*
	 * Allocate physical memory for kernel PT pages and their management.
	 * We need 1 PT page per possible task plus some slop.
	 */
	npages = min(atop(MAC_MAX_KPTSIZE), maxproc+16);
	s = ptoa(npages) + round_page(npages * sizeof(struct kpt_page));

	/*
	 * Verify that space will be allocated in region for which
	 * we already have kernel PT pages.
	 */
	addr = 0;
	rv = vm_map_find(kernel_map, NULL, 0, &addr, s, TRUE);
	if (rv != KERN_SUCCESS || addr + s >= (vm_offset_t)Sysmap)
		panic("pmap_init: kernel PT too small");
	vm_map_remove(kernel_map, addr, addr + s);

	/*
	 * Now allocate the space and link the pages together to
	 * form the KPT free list.
	 */
	addr = (vm_offset_t) kmem_alloc(kernel_map, s);
	s = ptoa(npages);
	addr2 = addr + s;
	kpt_pages = &((struct kpt_page *)addr2)[npages];
	kpt_free_list = (struct kpt_page *) 0;
	do {
		addr2 -= NBPG;
		(--kpt_pages)->kpt_next = kpt_free_list;
		kpt_free_list = kpt_pages;
		kpt_pages->kpt_va = addr2;
		kpt_pages->kpt_pa = pmap_extract(pmap_kernel(), addr2);
	} while (addr != addr2);
#ifdef PMAPSTATS
	kpt_stats.kpttotal = atop(s);
#endif
#ifdef DEBUG
	if (pmapdebug & PDB_INIT)
		printf("pmap_init: KPT: %ld pages from %lx to %lx\n",
		       atop(s), addr, addr + s);
#endif

	/*
	 * Allocate the segment table map
	 */
	s = maxproc * MAC_STSIZE;
	st_map = kmem_suballoc(kernel_map, &addr, &addr2, s, TRUE);

	/*
	 * Slightly modified version of kmem_suballoc() to get page table
	 * map where we want it.
	 */
	addr = MAC_PTBASE;
	s = (MAC_PTMAXSIZE / MAC_MAX_PTSIZE < maxproc) ?
		MAC_PTMAXSIZE : (maxproc * MAC_MAX_PTSIZE);
	addr2 = addr + s;
	rv = vm_map_find(kernel_map, NULL, 0, &addr, s, TRUE);
	if (rv != KERN_SUCCESS)
		panic("pmap_init: cannot allocate space for PT map");
	pmap_reference(vm_map_pmap(kernel_map));
	pt_map = vm_map_create(vm_map_pmap(kernel_map), addr, addr2, TRUE);
	if (pt_map == NULL)
		panic("pmap_init: cannot create pt_map");
	rv = vm_map_submap(kernel_map, addr, addr2, pt_map);
	if (rv != KERN_SUCCESS)
		panic("pmap_init: cannot map range to pt_map");
#ifdef DEBUG
	if (pmapdebug & PDB_INIT)
		printf("pmap_init: pt_map [%lx - %lx)\n", addr, addr2);
#endif

#if defined(M68040)
	if (mmutype == MMU_68040) {
		protostfree = ~l2tobm(0);
		for (rv = MAXUL2SIZE; rv < sizeof(protostfree)*NBBY; rv++)
			protostfree &= ~l2tobm(rv);
	}
#endif

	/*
	 * Now it is safe to enable pv_table recording.
	 */
	pmap_initialized = TRUE;
}

static struct pv_entry *pmap_alloc_pv __P((void));
static void	pmap_free_pv __P((struct pv_entry *));

static struct pv_entry *
pmap_alloc_pv()
{
	struct pv_page	*pvp;
	struct pv_entry	*pv;
	int		i;

	if (pv_nfree == 0) {
		pvp = (struct pv_page *)kmem_alloc(kernel_map, NBPG);
		if (pvp == 0)
			panic("pmap_alloc_pv: kmem_alloc() failed");
		pvp->pvp_pgi.pgi_freelist = pv = &pvp->pvp_pv[1];
		for (i = NPVPPG - 2; i; i--, pv++)
			pv->pv_next = pv + 1;
		pv->pv_next = 0;
		pv_nfree += pvp->pvp_pgi.pgi_nfree = NPVPPG - 1;
		TAILQ_INSERT_HEAD(&pv_page_freelist, pvp, pvp_pgi.pgi_list);
		pv = &pvp->pvp_pv[0];
	} else {
		--pv_nfree;
		pvp = pv_page_freelist.tqh_first;
		if (--pvp->pvp_pgi.pgi_nfree == 0) {
			TAILQ_REMOVE(&pv_page_freelist, pvp, pvp_pgi.pgi_list);
		}
		pv = pvp->pvp_pgi.pgi_freelist;
#ifdef DIAGNOSTIC
		if (pv == 0)
			panic("pmap_alloc_pv: pgi_nfree inconsistent");
#endif
		pvp->pvp_pgi.pgi_freelist = pv->pv_next;
	}
	return pv;
}

static void
pmap_free_pv(pv)
	struct pv_entry *pv;
{
	register struct pv_page *pvp;

	pvp = (struct pv_page *) trunc_page(pv);
	switch (++pvp->pvp_pgi.pgi_nfree) {
	case 1:
		TAILQ_INSERT_TAIL(&pv_page_freelist, pvp, pvp_pgi.pgi_list);
	default:
		pv->pv_next = pvp->pvp_pgi.pgi_freelist;
		pvp->pvp_pgi.pgi_freelist = pv;
		++pv_nfree;
		break;
	case NPVPPG:
		pv_nfree -= NPVPPG - 1;
		TAILQ_REMOVE(&pv_page_freelist, pvp, pvp_pgi.pgi_list);
		kmem_free(kernel_map, (vm_offset_t) pvp, NBPG);
		break;
	}
}

void	pmap_collect_pv __P((void));

void
pmap_collect_pv()
{
	struct pv_page_list pv_page_collectlist;
	struct pv_page *pvp, *npvp;
	struct pv_entry *ph, *ppv, *pv, *npv;
	int s;

	TAILQ_INIT(&pv_page_collectlist);

	for (pvp = pv_page_freelist.tqh_first; pvp; pvp = npvp) {
		if (pv_nfree < NPVPPG)
			break;
		npvp = pvp->pvp_pgi.pgi_list.tqe_next;
		if (pvp->pvp_pgi.pgi_nfree > NPVPPG / 3) {
			TAILQ_REMOVE(&pv_page_freelist, pvp, pvp_pgi.pgi_list);
			TAILQ_INSERT_TAIL(&pv_page_collectlist, pvp, pvp_pgi.pgi_list);
			pv_nfree -= pvp->pvp_pgi.pgi_nfree;
			pvp->pvp_pgi.pgi_nfree = -1;
		}
	}

	if (pv_page_collectlist.tqh_first == 0)
		return;

	for (ph = &pv_table[npages - 1]; ph >= &pv_table[0]; ph--) {
		if (ph->pv_pmap == 0)
			continue;
		s = splimp();
		for (ppv = ph; (pv = ppv->pv_next) != 0; ) {
			pvp = (struct pv_page *) trunc_page(pv);
			if (pvp->pvp_pgi.pgi_nfree == -1) {
				pvp = pv_page_freelist.tqh_first;
				if (--pvp->pvp_pgi.pgi_nfree == 0) {
					TAILQ_REMOVE(&pv_page_freelist, pvp, pvp_pgi.pgi_list);
				}
				npv = pvp->pvp_pgi.pgi_freelist;
#ifdef DIAGNOSTIC
				if (npv == 0)
					panic("pmap_collect_pv: pgi_nfree inconsistent");
#endif
				pvp->pvp_pgi.pgi_freelist = npv->pv_next;
				*npv = *pv;
				ppv->pv_next = npv;
				ppv = npv;
			} else
				ppv = pv;
		}
		splx(s);
	}

	for (pvp = pv_page_collectlist.tqh_first; pvp; pvp = npvp) {
		npvp = pvp->pvp_pgi.pgi_list.tqe_next;
		kmem_free(kernel_map, (vm_offset_t)pvp, NBPG);
	}
}

/*
 *	Used to map a range of physical addresses into kernel
 *	virtual address space.
 *
 *	For now, VM is already on, we only need to map the
 *	specified memory.
 */
vm_offset_t
pmap_map(va, spa, epa, prot)
	vm_offset_t	va, spa, epa;
	int		prot;
{

#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_map(%lx, %lx, %lx, %x)\n", va, spa, epa, prot);
#endif

	while (spa < epa) {
		pmap_enter(pmap_kernel(), va, spa, prot, FALSE);
		va += NBPG;
		spa += NBPG;
	}
	return(va);
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
		printf("pmap_pinit(%p)\n", pmap);
#endif

	/*
	 * No need to allocate page table space yet but we do need a
	 * valid segment table.  Initially, we point everyone at the
	 * "null" segment table.  On the first pmap_enter, a real
	 * segment table will be allocated.
	 */
	pmap->pm_stab = Segtabzero;
	pmap->pm_stpa = Segtabzeropa;
#if defined(M68040)
	if (mmutype == MMU_68040)
		pmap->pm_stfree = protostfree;
#endif
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

	if (pmap == NULL)
		return;

#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_destroy(%p)\n", pmap);
#endif

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
		printf("pmap_release(%p)\n", pmap);
#endif

#ifdef notdef /* DIAGNOSTIC */
	/* count would be 0 from pmap_destroy... */
	simple_lock(&pmap->pm_lock);
	if (pmap->pm_count != 1)
		panic("pmap_release count");
#endif

	if (pmap->pm_ptab)
		kmem_free_wakeup(pt_map, (vm_offset_t)pmap->pm_ptab,
				 MAC_MAX_PTSIZE);
	if (pmap->pm_stab != Segtabzero)
		kmem_free_wakeup(st_map, (vm_offset_t)pmap->pm_stab,
				MAC_STSIZE);
}

/*
 *	Add a reference to the specified pmap.
 */
void
pmap_reference(pmap)
	pmap_t	pmap;
{

	if (pmap == NULL)
		return;

#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_reference(%p)\n", pmap);
#endif

	simple_lock(&pmap->pm_lock);
	pmap->pm_count++;
	simple_unlock(&pmap->pm_lock);
}

void	loadustp __P((vm_offset_t));
void	pmap_activate __P((register pmap_t, struct pcb *));

void
pmap_activate(pmap, pcbp)
	register pmap_t pmap;
	struct pcb *pcbp;
{

	if (pmap == NULL)
		return;

#ifdef DEBUG
	if (pmapdebug & (PDB_FOLLOW|PDB_SEGTAB))
		printf("pmap_activate(%p, %p)\n", pmap, pcbp);
#endif

	PMAP_ACTIVATE(pmap, pcbp, pmap == curproc->p_vmspace->vm_map.pmap);
}

void	pmap_deactivate __P((register pmap_t, struct pcb *));

void
pmap_deactivate(pmap, pcb)
	register pmap_t pmap;
	struct pcb *pcb;
{
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
	vm_offset_t sva, eva;
{
	register vm_offset_t nssva;
	register pt_entry_t *pte;
	boolean_t firstpage, needcflush;
	int flags;

#ifdef DEBUG
	if (pmapdebug & (PDB_FOLLOW|PDB_REMOVE|PDB_PROTECT))
		printf("pmap_remove(%p, %lx, %lx)\n", pmap, sva, eva);
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
		nssva = mac68k_trunc_seg(sva) + MAC_SEG_SIZE;
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
			sva += NBPG;
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
	register struct pv_entry *pv;
	int s;

#ifdef DEBUG
	if ((pmapdebug & (PDB_FOLLOW|PDB_PROTECT)) ||
	    (prot == VM_PROT_NONE && (pmapdebug & PDB_REMOVE)))
		printf("pmap_page_protect(%lx, %x)\n", pa, prot);
#endif
	if (!pmap_valid_page (pa))
		return;

	switch (prot) {
	case VM_PROT_READ|VM_PROT_WRITE:
	case VM_PROT_ALL:
		return;
	/* copy_on_write */
	case VM_PROT_READ:
	case VM_PROT_READ|VM_PROT_EXECUTE:
		pmap_changebit(pa, PG_RO, TRUE);
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
	register pmap_t		pmap;
	register vm_offset_t	sva, eva;
	vm_prot_t		prot;
{
	register vm_offset_t nssva;
	register pt_entry_t *pte;
	boolean_t firstpage, needtflush;
	int isro;

#ifdef DEBUG
	if (pmapdebug & (PDB_FOLLOW|PDB_PROTECT))
		printf("pmap_protect(%p, %lx, %lx, %x)\n", pmap, sva, eva, prot);
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

	isro = pte_prot(pmap, prot);
	needtflush = active_pmap(pmap);
	firstpage = TRUE;
	while (sva < eva) {
		nssva = mac68k_trunc_seg(sva) + MAC_SEG_SIZE;
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
			if (pmap_pte_v(pte) && pmap_pte_prot_chg(pte, isro)) {
#if defined(M68040)
				/*
				 * Clear caches if making RO (see section
				 * "7.3 Cache Coherency" in the manual).
				 */
				if (isro && mmutype == MMU_68040) {
					vm_offset_t pa = pmap_pte_pa(pte);

					DCFP(pa);
					ICPP(pa);
				}
#endif
				pmap_pte_set_prot(pte, isro);
				if (needtflush)
					TBIS(sva);
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
			sva += NBPG;
		}
	}
}

void
mac68k_set_pte(va, pge)
	vm_offset_t va;
	vm_offset_t pge;
{
extern	vm_offset_t tmp_vpages[];
	register pt_entry_t *pte;

	if (va != tmp_vpages[0])
		return;

	pte = pmap_pte(pmap_kernel(), va);
	*pte = (pt_entry_t) pge;
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
	register int npte;
	vm_offset_t opa;
	boolean_t cacheable = TRUE;
	boolean_t checkpv = TRUE;

#ifdef DEBUG
	if (pmapdebug & (PDB_FOLLOW|PDB_ENTER))
		printf("pmap_enter(%p, %lx, %lx, %x, %x)\n",
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
	if (pmap->pm_ptab == NULL) {
		pmap->pm_ptab = (pt_entry_t *)
			kmem_alloc_wait(pt_map, MAC_MAX_PTSIZE);
	}

	/*
	 * Segment table entry not valid, we need a new PT page
	 */
	if (!pmap_ste_v(pmap, va))
		pmap_enter_ptpage(pmap, va);

	pa = mac68k_trunc_page(pa);
	pte = pmap_pte(pmap, va);
	opa = pmap_pte_pa(pte);
#ifdef DEBUG
	if (pmapdebug & PDB_ENTER)
		printf("enter: pte %p, *pte %x\n", pte, *pte);
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
		if (pmap_pte_w_chg(pte, wired ? PG_W : 0)) {
#ifdef DEBUG
			if (pmapdebug & PDB_ENTER)
				printf("enter: wiring change -> %x\n", wired);
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
		if (pmap_pte_ci(pte))
			cacheable = FALSE;
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
	if (pmap_valid_page (pa)) {
		register struct pv_entry *pv, *npv;
		int s;

#ifdef PMAPSTATS
		enter_stats.managed++;
#endif
		pv = pa_to_pvh(pa);
		s = splimp();
#ifdef DEBUG
		if (pmapdebug & PDB_ENTER)
			printf("enter: pv at %p: %lx/%p/%p\n",
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
			pv->pv_ptste = NULL;
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
			npv = pmap_alloc_pv();
			npv->pv_va = va;
			npv->pv_pmap = pmap;
			npv->pv_next = pv->pv_next;
			npv->pv_ptste = NULL;
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
		checkpv = cacheable = FALSE;
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
	npte = pa | pte_prot(pmap, prot) | (*pte & (PG_M|PG_U)) | PG_V;
	if (wired)
		npte |= PG_W;

	/* Don't cache if process can't take it, like SunOS ones.  */
	if (mmutype == MMU_68040 && pmap != pmap_kernel() &&
	    (curproc->p_md.md_flags & MDP_UNCACHE_WX) &&
	    (prot & VM_PROT_EXECUTE) && (prot & VM_PROT_WRITE))
	        checkpv = cacheable = FALSE;

	if (!checkpv && !cacheable)
		npte |= PG_CI;
#if defined(M68040)
	if (mmutype == MMU_68040 && (npte & (PG_PROT|PG_CI)) == PG_RW)
#ifdef DEBUG
		if (dowriteback && (dokwriteback || pmap != pmap_kernel()))
#endif
		npte |= PG_CCB;
#endif
#ifdef DEBUG
	if (pmapdebug & PDB_ENTER)
		printf("enter: new pte value %x\n", npte);
#endif
	/*
	 * Remember if this was a wiring-only change.
	 * If so, we need not flush the TLB and caches.
	 */
	wired = ((*pte ^ npte) == PG_W);
#if defined(M68040)
	if (mmutype == MMU_68040 && !wired) {
		DCFP(pa);
		ICPP(pa);
	}
#endif
	*pte = npte;
	if (!wired && active_pmap(pmap))
		TBIS(va);
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
		printf("pmap_change_wiring(%p, %lx, %x)\n", pmap, va, wired);
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
	 * If wiring actually changes (always?) set the wire bit and
	 * update the wire count.  Note that wiring is not a hardware
	 * characteristic, so there is no need to invalidate the TLB.
	 */
	if (pmap_pte_w_chg(pte, wired ? PG_W : 0)) {
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
	register vm_offset_t pa;

#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_extract(%p, %lx) -> ", pmap, va);
#endif
	pa = 0;
	if (pmap && pmap_ste_v(pmap, va))
		pa = *pmap_pte(pmap, va);
	if (pa)
		pa = (pa & PG_FRAME) | (va & ~PG_FRAME);
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
		printf("pmap_copy(%p, %p, %lx, %lx, %lx)\n",
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
	return; /* XXX -- do we need to do anything here? */
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
	register vm_offset_t kva;
	extern caddr_t CADDR1;

#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_zero_page(%lx)\n", phys);
#endif
	kva = (vm_offset_t) CADDR1;
	pmap_enter(pmap_kernel(), kva, phys, VM_PROT_READ|VM_PROT_WRITE, TRUE);
	zeropage((caddr_t)kva);
	pmap_remove_mapping(pmap_kernel(), kva, PT_ENTRY_NULL,
			    PRM_TFLUSH|PRM_CFLUSH);
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
	register vm_offset_t skva, dkva;
	extern caddr_t CADDR1, CADDR2;

#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_copy_page(%lx, %lx)\n", src, dst);
#endif
	skva = (vm_offset_t) CADDR1;
	dkva = (vm_offset_t) CADDR2;
	pmap_enter(pmap_kernel(), skva, src, VM_PROT_READ, TRUE);
	pmap_enter(pmap_kernel(), dkva, dst, VM_PROT_READ|VM_PROT_WRITE, TRUE);
	copypage((caddr_t)skva, (caddr_t)dkva);
	/* CADDR1 and CADDR2 are virtually contiguous */
	pmap_remove(pmap_kernel(), skva, skva + (2 * NBPG));
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
		printf("pmap_pageable(%p, %lx, %lx, %x)\n",
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
	if (pmap == pmap_kernel() && pageable && sva + NBPG == eva) {
		register struct pv_entry *pv;
		register vm_offset_t pa;

#ifdef DEBUG
		if ((pmapdebug & (PDB_FOLLOW|PDB_PTPAGE)) == PDB_PTPAGE)
			printf("pmap_pageable(%p, %lx, %lx, %x)\n",
			       pmap, sva, eva, pageable);
#endif
		if (!pmap_ste_v(pmap, sva))
			return;
		pa = pmap_pte_pa(pmap_pte(pmap, sva));
		if (!pmap_valid_page (pa))
			return;
		pv = pa_to_pvh(pa);
		if (pv->pv_ptste == NULL)
			return;
#ifdef DEBUG
		if (pv->pv_va != sva || pv->pv_next) {
			printf("pmap_pageable: bad PT page va %lx next %p\n",
			       pv->pv_va, pv->pv_next);
			return;
		}
#endif
		/*
		 * Mark it unmodified to avoid pageout
		 */
		pmap_changebit(pa, PG_M, FALSE);
#ifdef DEBUG
		if ((PHYS_TO_VM_PAGE(pa)->flags & PG_CLEAN) == 0) {
			printf("pa %lx: flags=%x: not clean\n",
				pa, PHYS_TO_VM_PAGE(pa)->flags);
			PHYS_TO_VM_PAGE(pa)->flags |= PG_CLEAN;
		}
		if (pmapdebug & PDB_PTPAGE)
			printf("pmap_pageable: PT page %lx(%x) unmodified\n",
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
	pmap_changebit(pa, PG_M, FALSE);
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
	pmap_changebit(pa, PG_U, FALSE);
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
#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW) {
		boolean_t rv = pmap_testbit(pa, PG_U);
		printf("pmap_is_referenced(%lx) -> %c\n", pa, "FT"[rv]);
		return(rv);
	}
#endif
	return(pmap_testbit(pa, PG_U));
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
#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW) {
		boolean_t rv = pmap_testbit(pa, PG_M);
		printf("pmap_is_modified(%lx) -> %c\n", pa, "FT"[rv]);
		return(rv);
	}
#endif
	return(pmap_testbit(pa, PG_M));
}

vm_offset_t
pmap_phys_address(ppn)
	int ppn;
{
	return(mac68k_ptob(ppn));
}

/*
 * Miscellaneous support routines follow
 */

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
	register struct pv_entry *pv, *npv;
	pmap_t ptpmap;
	st_entry_t *ste;
	int s, bits;
#ifdef DEBUG
	pt_entry_t opte;

	if (pmapdebug & (PDB_FOLLOW|PDB_REMOVE|PDB_PROTECT))
		printf("pmap_remove_mapping(%p, %lx, %p, %x)\n",
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
		printf("remove: invalidating pte at %p\n", pte);
#endif
	bits = *pte & (PG_U|PG_M);
	*pte = PG_NV;
	if ((flags & PRM_TFLUSH) && active_pmap(pmap))
		TBIS(va);
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
	if (!pmap_valid_page (pa))
		return;
	/*
	 * Otherwise remove it from the PV table
	 * (raise IPL since we may be called at interrupt time).
	 */
	pv = pa_to_pvh(pa);
	ste = ST_ENTRY_NULL;
	s = splimp();
	/*
	 * If it is the first entry on the list, it is actually
	 * in the header and we must copy the following entry up
	 * to the header.  Otherwise we must search the list for
	 * the entry.  In either case we free the now unused entry.
	 */
	if (pmap == pv->pv_pmap && va == pv->pv_va) {
		ste = pv->pv_ptste;
		ptpmap = pv->pv_ptpmap;
		npv = pv->pv_next;
		if (npv) {
			npv->pv_flags = pv->pv_flags;
			*pv = *npv;
			pmap_free_pv(npv);
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
		ste = npv->pv_ptste;
		ptpmap = npv->pv_ptpmap;
		pv->pv_next = npv->pv_next;
		pmap_free_pv(npv);
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
			printf("remove: ste was %x@%p pte was %x@%p\n",
			       *ste, ste, opte, pmap_pte(pmap, va));
#endif
#if defined(M68040)
		if (mmutype == MMU_68040) {
			st_entry_t *este = &ste[NPTEPG/SG4_LEV3SIZE];

			while (ste < este)
				*ste++ = SG_NV;
#ifdef DEBUG
			ste -= NPTEPG/SG4_LEV3SIZE;
#endif
		} else
#endif
		*ste = SG_NV;
		/*
		 * If it was a user PT page, we decrement the
		 * reference count on the segment table as well,
		 * freeing it if it is now empty.
		 */
		if (ptpmap != pmap_kernel()) {
#ifdef DEBUG
			if (pmapdebug & (PDB_REMOVE|PDB_SEGTAB))
				printf("remove: stab %p, refcnt %d\n",
				       ptpmap->pm_stab, ptpmap->pm_sref - 1);
			if ((pmapdebug & PDB_PARANOIA) &&
			    ptpmap->pm_stab != (st_entry_t *)trunc_page(ste))
				panic("remove: bogus ste");
#endif
			if (--(ptpmap->pm_sref) == 0) {
#ifdef DEBUG
				if (pmapdebug&(PDB_REMOVE|PDB_SEGTAB))
					printf("remove: free stab %p\n",
					       ptpmap->pm_stab);
#endif
				kmem_free_wakeup(st_map,
						 (vm_offset_t)ptpmap->pm_stab,
						 MAC_STSIZE);
				ptpmap->pm_stab = Segtabzero;
				ptpmap->pm_stpa = Segtabzeropa;
#if defined(M68040)
				if (mmutype == MMU_68040)
					ptpmap->pm_stfree = protostfree;
#endif
				ptpmap->pm_stchanged = TRUE;
				/*
				 * XXX may have changed segment table
				 * pointer for current process so
				 * update now to reload hardware.
				 */
				if (curproc != NULL &&
				    ptpmap == curproc->p_vmspace->vm_map.pmap)
					PMAP_ACTIVATE(ptpmap,
					    &curproc->p_addr->u_pcb, 1);
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
	/*
	 * Update saved attributes for managed page
	 */
	pmap_attributes[pmap_page_index(pa)] |= bits;
	splx(s);
}

/* static */
boolean_t
pmap_testbit(pa, bit)
	register vm_offset_t pa;
	int bit;
{
	register struct pv_entry *pv;
	register pt_entry_t *pte;
	int s;

	if (!pmap_valid_page (pa))
		return FALSE;

	pv = pa_to_pvh(pa);
	s = splimp();
	/*
	 * Check saved info first
	 */
	if (pmap_attributes[pmap_page_index(pa)] & bit) {
		splx(s);
		return(TRUE);
	}
	/*
	 * Not found, check current mappings returning
	 * immediately if found.
	 */
	if (pv->pv_pmap != NULL) {
		for (; pv; pv = pv->pv_next) {
			pte = pmap_pte(pv->pv_pmap, pv->pv_va);
			if (*pte & bit) {
				splx(s);
				return(TRUE);
			}
		}
	}
	splx(s);
	return(FALSE);
}

/* static */
void
pmap_changebit(pa, bit, setem)
	register vm_offset_t pa;
	int bit;
	boolean_t setem;
{
	register struct pv_entry *pv;
	register pt_entry_t *pte, npte;
	vm_offset_t va;
	int s;
#if defined(M68040)
	boolean_t firstpage = TRUE;
#endif
#ifdef PMAPSTATS
	struct chgstats *chgp;
#endif

#ifdef DEBUG
	if (pmapdebug & PDB_BITS)
		printf("pmap_changebit(%lx, %x, %s)\n",
		       pa, bit, setem ? "set" : "clear");
#endif
	if (!pmap_valid_page (pa))
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
	 * Clear saved attributes (modify, reference)
	 */
	if (!setem)
		pmap_attributes[pmap_page_index(pa)] &= ~bit;
	/*
	 * Loop over all current mappings setting/clearing as appropos
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
			if (bit == PG_RO) {
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
#if defined(M68040)
				/*
				 * If we are changing caching status or
				 * protection make sure the caches are
				 * flushed (but only once).
				 */
				if (firstpage && mmutype == MMU_68040 &&
				    ((bit == PG_RO && setem) ||
				     (bit & PG_CMASK))) {
					firstpage = FALSE;
					DCFP(pa);
					ICPP(pa);
				}
#endif
				*pte = npte;
				if (active_pmap(pv->pv_pmap))
					TBIS(va);
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
	register struct pv_entry *pv;
	st_entry_t *ste;
	int s;

#ifdef DEBUG
	if (pmapdebug & (PDB_FOLLOW|PDB_ENTER|PDB_PTPAGE))
		printf("pmap_enter_ptpage: pmap %p, va %lx\n", pmap, va);
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
		pmap->pm_stab = (st_entry_t *)
				kmem_alloc(st_map, MAC_STSIZE);
		pmap->pm_stpa = (st_entry_t *)
			pmap_extract(pmap_kernel(), (vm_offset_t)pmap->pm_stab);
#if defined(M68040)
		if (mmutype == MMU_68040) {
#ifdef DEBUG
			if (dowriteback && dokwriteback)
#endif
			pmap_changebit((vm_offset_t)pmap->pm_stpa, PG_CCB, 0);
			pmap->pm_stfree = protostfree;
		}
#endif
		pmap->pm_stchanged = TRUE;
		/*
		 * XXX may have changed segment table pointer for current
		 * process so update now to reload hardware.
		 */
		if (pmap == curproc->p_vmspace->vm_map.pmap)
			PMAP_ACTIVATE(pmap, &curproc->p_addr->u_pcb, 1);
#ifdef DEBUG
		if (pmapdebug & (PDB_ENTER|PDB_PTPAGE|PDB_SEGTAB))
			printf("enter: pmap %p stab %p(%p)\n",
			       pmap, pmap->pm_stab, pmap->pm_stpa);
#endif
	}

	ste = pmap_ste(pmap, va);
#if defined(M68040)
	/*
	 * Allocate level 2 descriptor block if necessary
	 */
	if (mmutype == MMU_68040) {
		if (*ste == SG_NV) {
			int ix;
			caddr_t addr;

			ix = bmtol2(pmap->pm_stfree);
			if (ix == -1)
				panic("enter: out of address space"); /* XXX */
			pmap->pm_stfree &= ~l2tobm(ix);
			addr = (caddr_t)&pmap->pm_stab[ix*SG4_LEV2SIZE];
			bzero(addr, SG4_LEV2SIZE*sizeof(st_entry_t));
			addr = (caddr_t)&pmap->pm_stpa[ix*SG4_LEV2SIZE];
			*ste = (u_int)addr | SG_RW | SG_U | SG_V;
#ifdef DEBUG
			if (pmapdebug & (PDB_ENTER|PDB_PTPAGE|PDB_SEGTAB))
				printf("enter: alloc ste2 %d(%p)\n", ix, addr);
#endif
		}
		ste = pmap_ste2(pmap, va);
		/*
		 * Since a level 2 descriptor maps a block of SG4_LEV3SIZE
		 * level 3 descriptors, we need a chunk of NPTEPG/SG4_LEV3SIZE
		 * (16) such descriptors (NBPG/SG4_LEV3SIZE bytes) to map a
		 * PT page--the unit of allocation.  We set `ste' to point
		 * to the first entry of that chunk which is validated in its
		 * entirety below.
		 */
		ste = (st_entry_t *)((int)ste & ~(NBPG/SG4_LEV3SIZE-1));
#ifdef DEBUG
		if (pmapdebug & (PDB_ENTER|PDB_PTPAGE|PDB_SEGTAB))
			printf("enter: ste2 %p (%p)\n",
				pmap_ste2(pmap, va), ste);
#endif
	}
#endif
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
		bzero((caddr_t)kpt->kpt_va, NBPG);
		pmap_enter(pmap, va, ptpa, VM_PROT_DEFAULT, TRUE);
#ifdef DEBUG
		if (pmapdebug & (PDB_ENTER|PDB_PTPAGE)) {
			int ix = pmap_ste(pmap, va) - pmap_ste(pmap, 0);

			printf("enter: add &Sysptmap[%d]: %x (KPT page %lx)\n",
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
			printf("vm_fault(pt_map, 0x%lx, RW, 0) -> %d\n",va,s);
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
#if defined(M68040)
	/*
	 * Turn off copyback caching of page table pages,
	 * could get ugly otherwise.
	 */
#ifdef DEBUG
	if (dowriteback && dokwriteback)
#endif
	if (mmutype == MMU_68040) {
#ifdef DEBUG
		pt_entry_t *pte = pmap_pte(pmap_kernel(), va);
		if ((pmapdebug & PDB_PARANOIA) && (*pte & PG_CCB) == 0)
			printf("%s PT no CCB: kva=%lx ptpa=%lx pte@%p=%x\n",
				pmap == pmap_kernel() ? "Kernel" : "User",
				va, ptpa, pte, *pte);
#endif
		pmap_changebit(ptpa, PG_CCB, 0);
	}
#endif
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
		} while ((pv = pv->pv_next) != NULL);
	}
#ifdef DEBUG
	if (pv == NULL)
		panic("pmap_enter_ptpage: PT page not entered");
#endif
	pv->pv_ptste = ste;
	pv->pv_ptpmap = pmap;
#ifdef DEBUG
	if (pmapdebug & (PDB_ENTER|PDB_PTPAGE))
		printf("enter: new PT page at PA %lx, ste at %p\n", ptpa, ste);
#endif

	/*
	 * Map the new PT page into the segment table.
	 * Also increment the reference count on the segment table if this
	 * was a user page table page.  Note that we don't use vm_map_pageable
	 * to keep the count like we do for PT pages, this is mostly because
	 * it would be difficult to identify ST pages in pmap_pageable to
	 * release them.  We also avoid the overhead of vm_map_pageable.
	 */
#if defined(M68040)
	if (mmutype == MMU_68040) {
		st_entry_t *este;

		for (este = &ste[NPTEPG/SG4_LEV3SIZE]; ste < este; ste++) {
			*ste = ptpa | SG_U | SG_RW | SG_V;
			ptpa += SG4_LEV3SIZE * sizeof(st_entry_t);
		}
	} else
#endif
	*ste = (ptpa & SG_FRAME) | SG_RW | SG_V;
	if (pmap != pmap_kernel()) {
#ifdef DEBUG
		if (pmapdebug & (PDB_ENTER|PDB_PTPAGE|PDB_SEGTAB))
			printf("enter: stab %p refcnt %d\n",
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

#ifdef DEBUG
/* static */
void
pmap_pvdump(pa)
	vm_offset_t pa;
{
	register struct pv_entry *pv;

	printf("pa %lx", pa);
	for (pv = pa_to_pvh(pa); pv; pv = pv->pv_next)
		printf(" -> pmap %p, va %lx, ptste %p, ptpmap %p, flags %x",
		       pv->pv_pmap, pv->pv_va, pv->pv_ptste, pv->pv_ptpmap,
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
	register int count;
	register pt_entry_t *pte;

	va = trunc_page(va);
	if (!pmap_ste_v(pmap_kernel(), va) ||
	    !pmap_pte_v(pmap_pte(pmap_kernel(), va)))
		return;

	if (!vm_map_lookup_entry(pt_map, va, &entry)) {
		printf("wired_check: entry for %lx not found\n", va);
		return;
	}
	count = 0;
	for (pte = (pt_entry_t *)va; pte < (pt_entry_t *)(va + NBPG); pte++)
		if (*pte)
			count++;
	if (entry->wired_count != count)
		printf("*%s*: %lx: w%d/a%d\n",
		       str, va, entry->wired_count, count);
}
#endif

/*
 * LAK: These functions are from NetBSD/i386 and are used for
 *  the non-contiguous memory machines, such as the IIci, IIsi, and IIvx.
 *  See the functions in sys/vm that #ifdef MACHINE_NONCONTIG.
 */

/*
 * pmap_free_pages()
 *
 *   Returns the number of free physical pages left.
 */

unsigned int
pmap_free_pages()
{
	/* printf ("pmap_free_pages(): returning %d\n", avail_remaining); */
	return avail_remaining;
}

/*
 * pmap_next_page()
 *
 *   Stores in *addrp the next available page, skipping the hole between
 *   bank A and bank B.
 */

int
pmap_next_page(addrp)
	vm_offset_t *addrp;
{
	if (avail_next == high[avail_range]) {
		avail_range++;
		if (avail_range >= numranges) {
			/* printf ("pmap_next_page(): returning FALSE\n"); */
			return FALSE;
		}
		avail_next = low[avail_range];
	}

	*addrp = avail_next;
	/* printf ("pmap_next_page(): returning 0x%x\n", avail_next); */
	avail_next += NBPG;
	avail_remaining--;
	return TRUE;
}

/*
 * pmap_page_index()
 *
 *   Given a physical address, return the page number that it is in
 *   the block of free memory.
 */

int
pmap_page_index(pa)
	vm_offset_t pa;
{
	/*
	 * XXX LAK: This routine is called quite a bit.  We should go
	 *  back and try to optimize it a bit.
	 */

	int	i, index;

	index = 0;
	for (i = 0; i < numranges; i++) {
		if (pa >= low[i] && pa < high[i]) {
			index += mac68k_btop (pa - low[i]);
			/* printf ("pmap_page_index(0x%x): returning %d\n", */
				/* (int)pa, index); */
			return index;
		}
		index += mac68k_btop (high[i] - low[i]);
	}

	return -1;
}

void
pmap_virtual_space(startp, endp)
	vm_offset_t *startp, *endp;
{
	/* printf ("pmap_virtual_space(): returning 0x%x and 0x%x\n", */
		/* virtual_avail, virtual_end); */
	*startp = virtual_avail;
	*endp = virtual_end;
}
