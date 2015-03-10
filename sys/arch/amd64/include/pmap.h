/*	$OpenBSD: pmap.h,v 1.55 2015/03/10 20:12:39 kettenis Exp $	*/
/*	$NetBSD: pmap.h,v 1.1 2003/04/26 18:39:46 fvdl Exp $	*/

/*
 * Copyright (c) 1997 Charles D. Cranor and Washington University.
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
 */

/*
 * Copyright (c) 2001 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Frank van der Linden for Wasabi Systems, Inc.
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
 *      This product includes software developed for the NetBSD Project by
 *      Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * pmap.h: see pmap.c for the history of this pmap module.
 */

#ifndef	_MACHINE_PMAP_H_
#define	_MACHINE_PMAP_H_

#ifndef _LOCORE
#ifdef _KERNEL
#include <sys/mman.h>
#include <machine/cpufunc.h>
#include <machine/segments.h>
#endif /* _KERNEL */
#include <sys/mutex.h>
#include <uvm/uvm_object.h>
#include <machine/pte.h>
#endif

/*
 * The x86_64 pmap module closely resembles the i386 one. It uses
 * the same recursive entry scheme. See the i386 pmap.h for a
 * description. The alternate area trick for accessing non-current
 * pmaps has been removed, though, because it performs badly on SMP
 * systems.
 * The most obvious difference to i386 is that 2 extra levels of page
 * table need to be dealt with. The level 1 page table pages are at:
 *
 * l1: 0x00007f8000000000 - 0x00007fffffffffff     (39 bits, needs PML4 entry)
 *
 * The other levels are kept as physical pages in 3 UVM objects and are
 * temporarily mapped for virtual access when needed.
 *
 * The other obvious difference from i386 is that it has a direct map of all
 * physical memory in the VA range:
 *
 *     0xffffff0000000000 - 0xffffff7fffffffff
 *
 * The direct map is used in some cases to access PTEs of non-current pmaps.
 *
 * Note that address space is signed, so the layout for 48 bits is:
 *
 *  +---------------------------------+ 0xffffffffffffffff
 *  |         Kernel Image            |
 *  +---------------------------------+ 0xffffff8000000000
 *  |         Direct Map              |
 *  +---------------------------------+ 0xffffff0000000000
 *  ~                                 ~
 *  |                                 |
 *  |         Kernel Space            |
 *  |                                 |
 *  |                                 |
 *  +---------------------------------+ 0xffff800000000000 = 0x0000800000000000
 *  |    L1 table (PTE pages)         |
 *  +---------------------------------+ 0x00007f8000000000
 *  ~                                 ~
 *  |                                 |
 *  |         User Space              |
 *  |                                 |
 *  |                                 |
 *  +---------------------------------+ 0x0000000000000000
 *
 * In other words, there is a 'VA hole' at 0x0000800000000000 -
 * 0xffff800000000000 which will trap, just as on, for example,
 * sparcv9.
 *
 * The unused space can be used if needed, but it adds a little more
 * complexity to the calculations.
 */

/*
 * The first generation of Hammer processors can use 48 bits of
 * virtual memory, and 40 bits of physical memory. This will be
 * more for later generations. These defines can be changed to
 * variable names containing the # of bits, extracted from an
 * extended cpuid instruction (variables are harder to use during
 * bootstrap, though)
 */
#define VIRT_BITS	48
#define PHYS_BITS	40

/*
 * Mask to get rid of the sign-extended part of addresses.
 */
#define VA_SIGN_MASK		0xffff000000000000
#define VA_SIGN_NEG(va)		((va) | VA_SIGN_MASK)
/*
 * XXXfvdl this one's not right.
 */
#define VA_SIGN_POS(va)		((va) & ~VA_SIGN_MASK)

#define L4_SLOT_PTE		255
#define L4_SLOT_KERN		256
#define L4_SLOT_KERNBASE	511
#define L4_SLOT_DIRECT		510

#define PDIR_SLOT_KERN		L4_SLOT_KERN
#define PDIR_SLOT_PTE		L4_SLOT_PTE
#define PDIR_SLOT_DIRECT	L4_SLOT_DIRECT

/*
 * the following defines give the virtual addresses of various MMU
 * data structures:
 * PTE_BASE: the base VA of the linear PTE mappings
 * PTD_BASE: the base VA of the recursive mapping of the PTD
 * PDP_PDE: the VA of the PDE that points back to the PDP
 *
 */

#define PTE_BASE  ((pt_entry_t *) (L4_SLOT_PTE * NBPD_L4))
#define PMAP_DIRECT_BASE	(VA_SIGN_NEG((L4_SLOT_DIRECT * NBPD_L4)))
#define PMAP_DIRECT_END		(VA_SIGN_NEG(((L4_SLOT_DIRECT + 1) * NBPD_L4)))

#define L1_BASE		PTE_BASE

#define L2_BASE ((pd_entry_t *)((char *)L1_BASE + L4_SLOT_PTE * NBPD_L3))
#define L3_BASE ((pd_entry_t *)((char *)L2_BASE + L4_SLOT_PTE * NBPD_L2))
#define L4_BASE ((pd_entry_t *)((char *)L3_BASE + L4_SLOT_PTE * NBPD_L1))

#define PDP_PDE		(L4_BASE + PDIR_SLOT_PTE)

#define PDP_BASE	L4_BASE

#define NKL4_MAX_ENTRIES	(unsigned long)1
#define NKL3_MAX_ENTRIES	(unsigned long)(NKL4_MAX_ENTRIES * 512)
#define NKL2_MAX_ENTRIES	(unsigned long)(NKL3_MAX_ENTRIES * 512)
#define NKL1_MAX_ENTRIES	(unsigned long)(NKL2_MAX_ENTRIES * 512)

#define NKL4_KIMG_ENTRIES	1
#define NKL3_KIMG_ENTRIES	1
#define NKL2_KIMG_ENTRIES	16

#define NDML4_ENTRIES		1
#define NDML3_ENTRIES		1
#define NDML2_ENTRIES		4	/* 4GB */

/*
 * Since kva space is below the kernel in its entirety, we start off
 * with zero entries on each level.
 */
#define NKL4_START_ENTRIES	0
#define NKL3_START_ENTRIES	0
#define NKL2_START_ENTRIES	0
#define NKL1_START_ENTRIES	0	/* XXX */

#define NTOPLEVEL_PDES		(PAGE_SIZE / (sizeof (pd_entry_t)))

#define NPDPG			(PAGE_SIZE / sizeof (pd_entry_t))

/*
 * pl*_pi: index in the ptp page for a pde mapping a VA.
 * (pl*_i below is the index in the virtual array of all pdes per level)
 */
#define pl1_pi(VA)	(((VA_SIGN_POS(VA)) & L1_MASK) >> L1_SHIFT)
#define pl2_pi(VA)	(((VA_SIGN_POS(VA)) & L2_MASK) >> L2_SHIFT)
#define pl3_pi(VA)	(((VA_SIGN_POS(VA)) & L3_MASK) >> L3_SHIFT)
#define pl4_pi(VA)	(((VA_SIGN_POS(VA)) & L4_MASK) >> L4_SHIFT)

/*
 * pl*_i: generate index into pde/pte arrays in virtual space
 */
#define pl1_i(VA)	(((VA_SIGN_POS(VA)) & L1_FRAME) >> L1_SHIFT)
#define pl2_i(VA)	(((VA_SIGN_POS(VA)) & L2_FRAME) >> L2_SHIFT)
#define pl3_i(VA)	(((VA_SIGN_POS(VA)) & L3_FRAME) >> L3_SHIFT)
#define pl4_i(VA)	(((VA_SIGN_POS(VA)) & L4_FRAME) >> L4_SHIFT)
#define pl_i(va, lvl) \
        (((VA_SIGN_POS(va)) & ptp_masks[(lvl)-1]) >> ptp_shifts[(lvl)-1])

#define PTP_MASK_INITIALIZER	{ L1_FRAME, L2_FRAME, L3_FRAME, L4_FRAME }
#define PTP_SHIFT_INITIALIZER	{ L1_SHIFT, L2_SHIFT, L3_SHIFT, L4_SHIFT }
#define NKPTP_INITIALIZER	{ NKL1_START_ENTRIES, NKL2_START_ENTRIES, \
				  NKL3_START_ENTRIES, NKL4_START_ENTRIES }
#define NKPTPMAX_INITIALIZER	{ NKL1_MAX_ENTRIES, NKL2_MAX_ENTRIES, \
				  NKL3_MAX_ENTRIES, NKL4_MAX_ENTRIES }
#define NBPD_INITIALIZER	{ NBPD_L1, NBPD_L2, NBPD_L3, NBPD_L4 }
#define PDES_INITIALIZER	{ L2_BASE, L3_BASE, L4_BASE }

/*
 * PTP macros:
 *   a PTP's index is the PD index of the PDE that points to it
 *   a PTP's offset is the byte-offset in the PTE space that this PTP is at
 *   a PTP's VA is the first VA mapped by that PTP
 *
 * note that PAGE_SIZE == number of bytes in a PTP (4096 bytes == 1024 entries)
 *           NBPD == number of bytes a PTP can map (4MB)
 */

#define ptp_va2o(va, lvl)	(pl_i(va, (lvl)+1) * PAGE_SIZE)

#define PTP_LEVELS	4

/*
 * PG_AVAIL usage: we make use of the ignored bits of the PTE
 */

#define PG_W		PG_AVAIL1	/* "wired" mapping */
#define PG_PVLIST	PG_AVAIL2	/* mapping has entry on pvlist */
/* PG_AVAIL3 not used */

/*
 * Number of PTE's per cache line.  8 byte pte, 64-byte cache line
 * Used to avoid false sharing of cache lines.
 */
#define NPTECL		8


#if defined(_KERNEL) && !defined(_LOCORE)
/*
 * pmap data structures: see pmap.c for details of locking.
 */

struct pmap;
typedef struct pmap *pmap_t;

/*
 * we maintain a list of all non-kernel pmaps
 */

LIST_HEAD(pmap_head, pmap); /* struct pmap_head: head of a pmap list */

/*
 * the pmap structure
 *
 * note that the pm_obj contains the reference count,
 * page list, and number of PTPs within the pmap.
 */

struct pmap {
	struct mutex pm_mtx;
	struct uvm_object pm_obj[PTP_LEVELS-1]; /* objects for lvl >= 1) */
#define pm_obj_l1 pm_obj[0]
#define pm_obj_l2 pm_obj[1]
#define pm_obj_l3 pm_obj[2]
	LIST_ENTRY(pmap) pm_list;	/* list (lck by pm_list lock) */
	pd_entry_t *pm_pdir;		/* VA of PD (lck by object lock) */
	paddr_t pm_pdirpa;		/* PA of PD (read-only after create) */
	struct vm_page *pm_ptphint[PTP_LEVELS-1];
					/* pointer to a PTP in our pmap */
	struct pmap_statistics pm_stats;  /* pmap stats (lck by object lock) */

	u_int64_t pm_cpus;		/* mask of CPUs using pmap */
};

/*
 * MD flags that we use for pmap_enter (in the pa):
 */
#define PMAP_PA_MASK	~((paddr_t)PAGE_MASK) /* to remove the flags */
#define	PMAP_NOCACHE	0x1 /* set the non-cacheable bit. */
#define	PMAP_WC		0x2 /* set page write combining. */

/*
 * We keep mod/ref flags in struct vm_page->pg_flags.
 */
#define	PG_PMAP_MOD	PG_PMAP0
#define	PG_PMAP_REF	PG_PMAP1
#define	PG_PMAP_WC      PG_PMAP2

/*
 * for each managed physical page we maintain a list of <PMAP,VA>'s
 * which it is mapped at.
 */
struct pv_entry {			/* locked by its list's pvh_lock */
	struct pv_entry *pv_next;	/* next entry */
	struct pmap *pv_pmap;		/* the pmap */
	vaddr_t pv_va;			/* the virtual address */
	struct vm_page *pv_ptp;		/* the vm_page of the PTP */
};

/*
 * global kernel variables
 */

/* PTDpaddr: is the physical address of the kernel's PDP */
extern u_long PTDpaddr;

extern struct pmap kernel_pmap_store;	/* kernel pmap */

extern paddr_t ptp_masks[];
extern int ptp_shifts[];
extern long nkptp[], nbpd[], nkptpmax[];

/*
 * macros
 */

#define	pmap_kernel()			(&kernel_pmap_store)
#define	pmap_resident_count(pmap)	((pmap)->pm_stats.resident_count)
#define	pmap_wired_count(pmap)		((pmap)->pm_stats.wired_count)
#define	pmap_update(pmap)		/* nothing (yet) */

#define pmap_clear_modify(pg)		pmap_clear_attrs(pg, PG_M)
#define pmap_clear_reference(pg)	pmap_clear_attrs(pg, PG_U)
#define pmap_copy(DP,SP,D,L,S)		
#define pmap_is_modified(pg)		pmap_test_attrs(pg, PG_M)
#define pmap_is_referenced(pg)		pmap_test_attrs(pg, PG_U)
#define pmap_move(DP,SP,D,L,S)		
#define pmap_valid_entry(E) 		((E) & PG_V) /* is PDE or PTE valid? */

#define pmap_proc_iflush(p,va,len)	/* nothing */
#define pmap_unuse_final(p)		/* nothing */
#define	pmap_remove_holes(vm)		do { /* nothing */ } while (0)


/*
 * prototypes
 */

paddr_t		pmap_bootstrap(paddr_t, paddr_t);
boolean_t	pmap_clear_attrs(struct vm_page *, unsigned long);
static void	pmap_page_protect(struct vm_page *, vm_prot_t);
void		pmap_page_remove (struct vm_page *);
static void	pmap_protect(struct pmap *, vaddr_t,
				vaddr_t, vm_prot_t);
void		pmap_remove(struct pmap *, vaddr_t, vaddr_t);
boolean_t	pmap_test_attrs(struct vm_page *, unsigned);
static void	pmap_update_pg(vaddr_t);
static void	pmap_update_2pg(vaddr_t,vaddr_t);
void		pmap_write_protect(struct pmap *, vaddr_t,
				vaddr_t, vm_prot_t);

vaddr_t reserve_dumppages(vaddr_t); /* XXX: not a pmap fn */

paddr_t	pmap_prealloc_lowmem_ptps(paddr_t);

void	pagezero(vaddr_t);

/* 
 * functions for flushing the cache for vaddrs and pages.
 * these functions are not part of the MI pmap interface and thus
 * should not be used as such.
 */
void	pmap_flush_cache(vaddr_t, vsize_t);
#define pmap_flush_page(paddr) do {					\
	KDASSERT(PHYS_TO_VM_PAGE(paddr) != NULL);			\
	pmap_flush_cache(PMAP_DIRECT_MAP(paddr), PAGE_SIZE);		\
} while (/* CONSTCOND */ 0)

#define	PMAP_STEAL_MEMORY	/* enable pmap_steal_memory() */
#define PMAP_GROWKERNEL		/* turn on pmap_growkernel interface */

/*
 * inline functions
 */

static __inline void
pmap_remove_all(struct pmap *pmap)
{
	/* Nothing. */
}

/*
 * pmap_update_pg: flush one page from the TLB (or flush the whole thing
 *	if hardware doesn't support one-page flushing)
 */

__inline static void
pmap_update_pg(vaddr_t va)
{
	invlpg(va);
}

/*
 * pmap_update_2pg: flush two pages from the TLB
 */

__inline static void
pmap_update_2pg(vaddr_t va, vaddr_t vb)
{
	invlpg(va);
	invlpg(vb);
}

/*
 * pmap_page_protect: change the protection of all recorded mappings
 *	of a managed page
 *
 * => this function is a frontend for pmap_page_remove/pmap_clear_attrs
 * => we only have to worry about making the page more protected.
 *	unprotecting a page is done on-demand at fault time.
 */

__inline static void
pmap_page_protect(struct vm_page *pg, vm_prot_t prot)
{
	if ((prot & PROT_WRITE) == 0) {
		if (prot & (PROT_READ | PROT_EXEC)) {
			(void) pmap_clear_attrs(pg, PG_RW);
		} else {
			pmap_page_remove(pg);
		}
	}
}

/*
 * pmap_protect: change the protection of pages in a pmap
 *
 * => this function is a frontend for pmap_remove/pmap_write_protect
 * => we only have to worry about making the page more protected.
 *	unprotecting a page is done on-demand at fault time.
 */

__inline static void
pmap_protect(struct pmap *pmap, vaddr_t sva, vaddr_t eva, vm_prot_t prot)
{
	if ((prot & PROT_WRITE) == 0) {
		if (prot & (PROT_READ| PROT_EXEC)) {
			pmap_write_protect(pmap, sva, eva, prot);
		} else {
			pmap_remove(pmap, sva, eva);
		}
	}
}

/*
 * various address inlines
 *
 *  vtopte: return a pointer to the PTE mapping a VA, works only for
 *  user and PT addresses
 *
 *  kvtopte: return a pointer to the PTE mapping a kernel VA
 */

static __inline pt_entry_t *
vtopte(vaddr_t va)
{
	return (PTE_BASE + pl1_i(va));
}

static __inline pt_entry_t *
kvtopte(vaddr_t va)
{
#ifdef LARGEPAGES
	{
		pd_entry_t *pde;

		pde = L1_BASE + pl2_i(va);
		if (*pde & PG_PS)
			return ((pt_entry_t *)pde);
	}
#endif

	return (PTE_BASE + pl1_i(va));
}

#define PMAP_DIRECT_MAP(pa)	((vaddr_t)PMAP_DIRECT_BASE + (pa))
#define PMAP_DIRECT_UNMAP(va)	((paddr_t)(va) - PMAP_DIRECT_BASE)
#define pmap_map_direct(pg)	PMAP_DIRECT_MAP(VM_PAGE_TO_PHYS(pg))
#define pmap_unmap_direct(va)	PHYS_TO_VM_PAGE(PMAP_DIRECT_UNMAP(va))

#define __HAVE_PMAP_DIRECT

#endif /* _KERNEL && !_LOCORE */

#ifndef _LOCORE
struct pv_entry;
struct vm_page_md {
	struct mutex pv_mtx;
	struct pv_entry *pv_list;
};

#define VM_MDPAGE_INIT(pg) do {		\
	mtx_init(&(pg)->mdpage.pv_mtx, IPL_VM); \
	(pg)->mdpage.pv_list = NULL;	\
} while (0)
#endif	/* !_LOCORE */

#endif	/* _MACHINE_PMAP_H_ */
