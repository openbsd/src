/*	$NetBSD: pmap.c,v 1.60.4.1 1996/06/12 20:36:30 pk Exp $ */

/*
 * Copyright (c) 1996
 * 	The President and Fellows of Harvard College. All rights reserved.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by Harvard University.
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Aaron Brown and
 *	Harvard University.
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
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
 *	@(#)pmap.c	8.4 (Berkeley) 2/5/94
 *
 */

/*
 * SPARC physical map management code.
 * Does not function on multiprocessors (yet).
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/malloc.h>
#include <sys/kcore.h>
#include <sys/core.h>
#include <sys/exec_aout.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_prot.h>
#include <vm/vm_page.h>

#include <machine/autoconf.h>
#include <machine/bsd_openprom.h>
#include <machine/oldmon.h>
#include <machine/cpu.h>
#include <machine/ctlreg.h>
#include <machine/kcore.h>

#include <sparc/sparc/asm.h>
#include <sparc/sparc/cache.h>
#include <sparc/sparc/vaddrs.h>

#ifdef DEBUG
#define PTE_BITS "\20\40V\37W\36S\35NC\33IO\32U\31M"
#define PTE_BITS4M "\20\10C\7M\6R\5ACC3\4ACC2\3ACC1\2TYP2\1TYP1"
#endif

/*
 * The SPARCstation offers us the following challenges:
 *
 *   1. A virtual address cache.  This is, strictly speaking, not
 *	part of the architecture, but the code below assumes one.
 *	This is a write-through cache on the 4c and a write-back cache
 *	on others.
 *
 *   2. (4/4c only) An MMU that acts like a cache.  There is not enough
 *	space in the MMU to map everything all the time.  Instead, we need
 *	to load MMU with the `working set' of translations for each
 *	process. The sun4m does not act like a cache; tables are maintained
 *	in physical memory.
 *
 *   3.	Segmented virtual and physical spaces.  The upper 12 bits of
 *	a virtual address (the virtual segment) index a segment table,
 *	giving a physical segment.  The physical segment selects a
 *	`Page Map Entry Group' (PMEG) and the virtual page number---the
 *	next 5 or 6 bits of the virtual address---select the particular
 *	`Page Map Entry' for the page.  We call the latter a PTE and
 *	call each Page Map Entry Group a pmeg (for want of a better name).
 *	Note that the sun4m has an unsegmented 36-bit physical space.
 *
 *	Since there are no valid bits in the segment table, the only way
 *	to have an invalid segment is to make one full pmeg of invalid PTEs.
 *	We use the last one (since the ROM does as well) (sun4/4c only)
 *
 *   4. Discontiguous physical pages.  The Mach VM expects physical pages
 *	to be in one sequential lump.
 *
 *   5. The MMU is always on: it is not possible to disable it.  This is
 *	mainly a startup hassle.
 */

struct pmap_stats {
	int	ps_unlink_pvfirst;	/* # of pv_unlinks on head */
	int	ps_unlink_pvsearch;	/* # of pv_unlink searches */
	int	ps_changeprots;		/* # of calls to changeprot */
	int	ps_useless_changeprots;	/* # of changeprots for wiring */
	int	ps_enter_firstpv;	/* pv heads entered */
	int	ps_enter_secondpv;	/* pv nonheads entered */
	int	ps_useless_changewire;	/* useless wiring changes */
	int	ps_npg_prot_all;	/* # of active pages protected */
	int	ps_npg_prot_actual;	/* # pages actually affected */
} pmap_stats;

#ifdef DEBUG
#define	PDB_CREATE	0x0001
#define	PDB_DESTROY	0x0002
#define	PDB_REMOVE	0x0004
#define	PDB_CHANGEPROT	0x0008
#define	PDB_ENTER	0x0010

#define	PDB_MMU_ALLOC	0x0100
#define	PDB_MMU_STEAL	0x0200
#define	PDB_CTX_ALLOC	0x0400
#define	PDB_CTX_STEAL	0x0800
#define	PDB_MMUREG_ALLOC	0x1000
#define	PDB_MMUREG_STEAL	0x2000
#define	PDB_CACHESTUFF	0x4000
int	pmapdebug = 0;
#endif

#if 0
#define	splpmap() splimp()
#endif

/*
 * First and last managed physical addresses.
 */
vm_offset_t	vm_first_phys, vm_num_phys;

/*
 * For each managed physical page, there is a list of all currently
 * valid virtual mappings of that page.  Since there is usually one
 * (or zero) mapping per page, the table begins with an initial entry,
 * rather than a pointer; this head entry is empty iff its pv_pmap
 * field is NULL.
 *
 * Note that these are per machine independent page (so there may be
 * only one for every two hardware pages, e.g.).  Since the virtual
 * address is aligned on a page boundary, the low order bits are free
 * for storing flags.  Only the head of each list has flags.
 *
 * THIS SHOULD BE PART OF THE CORE MAP
 */
struct pvlist {
	struct	pvlist *pv_next;	/* next pvlist, if any */
	struct	pmap *pv_pmap;		/* pmap of this va */
	vm_offset_t	pv_va;		/* virtual address */
	int	pv_flags;		/* flags (below) */
};

/*
 * Flags in pv_flags.  Note that PV_MOD must be 1 and PV_REF must be 2
 * since they must line up with the bits in the hardware PTEs (see pte.h).
 * Sun4M bits are different (reversed), and at a different location in the
 * pte. Now why did they do that?
 */
#define PV_MOD	1		/* page modified */
#define PV_REF	2		/* page referenced */
#define PV_NC	4		/* page cannot be cached */
#define PV_REF4M	1	/* page referenced on sun4m */
#define PV_MOD4M	2	/* page modified on 4m (why reversed?!?) */
#define PV_C4M		4	/* page _can_ be cached on 4m */
/*efine	PV_ALLF	7		** all of the above */

struct pvlist *pv_table;	/* array of entries, one per physical page */

#define pvhead(pa)	(&pv_table[atop((pa) - vm_first_phys)])

/*
 * Each virtual segment within each pmap is either valid or invalid.
 * It is valid if pm_npte[VA_VSEG(va)] is not 0.  This does not mean
 * it is in the MMU, however; that is true iff pm_segmap[VA_VSEG(va)]
 * does not point to the invalid PMEG.
 *
 * In the older SPARC architectures (pre-4m), page tables are cached in the
 * MMU. The following discussion applies to these architectures:
 *
 * If a virtual segment is valid and loaded, the correct PTEs appear
 * in the MMU only.  If it is valid and unloaded, the correct PTEs appear
 * in the pm_pte[VA_VSEG(va)] only.  However, some effort is made to keep
 * the software copies consistent enough with the MMU so that libkvm can
 * do user address translations.  In particular, pv_changepte() and
 * pmap_enu() maintain consistency, while less critical changes are
 * not maintained.  pm_pte[VA_VSEG(va)] always points to space for those
 * PTEs, unless this is the kernel pmap, in which case pm_pte[x] is not
 * used (sigh).
 *
 * Each PMEG in the MMU is either free or contains PTEs corresponding to
 * some pmap and virtual segment.  If it contains some PTEs, it also contains
 * reference and modify bits that belong in the pv_table.  If we need
 * to steal a PMEG from some process (if we need one and none are free)
 * we must copy the ref and mod bits, and update pm_segmap in the other
 * pmap to show that its virtual segment is no longer in the MMU.
 *
 * There are 128 PMEGs in a small Sun-4, of which only a few dozen are
 * tied down permanently, leaving `about' 100 to be spread among
 * running processes.  These are managed as an LRU cache.  Before
 * calling the VM paging code for a user page fault, the fault handler
 * calls mmu_load(pmap, va) to try to get a set of PTEs put into the
 * MMU.  mmu_load will check the validity of the segment and tell whether
 * it did something.
 *
 * Since I hate the name PMEG I call this data structure an `mmu entry'.
 * Each mmuentry is on exactly one of three `usage' lists: free, LRU,
 * or locked.  The LRU list is for user processes; the locked list is
 * for kernel entries; both are doubly linked queues headed by `mmuhd's.
 * The free list is a simple list, headed by a free list pointer.
 *
 * In the sun4m architecture using the SPARC Reference MMU (SRMMU), three
 * levels of page tables are maintained in physical memory. We use the same
 * structures as with the 3-level old-style MMU (pm_regmap, pm_segmap,
 * rg_segmap, sg_pte, etc) to maintain kernel-edible page tables; we also
 * build a parallel set of physical tables that can be used by the MMU.
 * (XXX: This seems redundant, but is it necessary for the unified kernel?)
 *
 * If a virtual segment is valid, its entries will be in both parallel lists.
 * If it is not valid, then its entry in the kernel tables will be zero, and
 * its entry in the MMU tables will either be nonexistent or zero as well.
 */
struct mmuentry {
	TAILQ_ENTRY(mmuentry)	me_list;	/* usage list link */
	TAILQ_ENTRY(mmuentry)	me_pmchain;	/* pmap owner link */
	struct	pmap *me_pmap;		/* pmap, if in use */
	u_short	me_vreg;		/* associated virtual region/segment */
	u_short	me_vseg;		/* associated virtual region/segment */
	u_short	me_cookie;		/* hardware SMEG/PMEG number */
};
struct mmuentry *mmusegments;	/* allocated in pmap_bootstrap */
struct mmuentry *mmuregions;	/* allocated in pmap_bootstrap */

struct mmuhd segm_freelist, segm_lru, segm_locked;
struct mmuhd region_freelist, region_lru, region_locked;

int	seginval;		/* the invalid segment number */
#ifdef MMU_3L
int	reginval;		/* the invalid region number */
#endif

/*
 * (sun4/4c)
 * A context is simply a small number that dictates which set of 4096
 * segment map entries the MMU uses.  The Sun 4c has eight such sets.
 * These are alloted in an `almost MRU' fashion.
 * (sun4m)
 * A context is simply a small number that indexes the context table, the
 * root-level page table mapping 4G areas. Each entry in this table points
 * to a 1st-level region table. A SPARC reference MMU will usually use 16
 * such contexts, but some offer as many as 64k contexts; the theoretical
 * maximum is 2^32 - 1, but this would create overlarge context tables.
 *
 * Each context is either free or attached to a pmap.
 *
 * Since the virtual address cache is tagged by context, when we steal
 * a context we have to flush (that part of) the cache.
 */
union ctxinfo {
	union	ctxinfo *c_nextfree;	/* free list (if free) */
	struct	pmap *c_pmap;		/* pmap (if busy) */
};
union ctxinfo *ctxinfo;		/* allocated at in pmap_bootstrap */
int	ncontext;

union	ctxinfo *ctx_freelist;	/* context free list */
int	ctx_kick;		/* allocation rover when none free */
int	ctx_kickdir;		/* ctx_kick roves both directions */

char	*ctxbusyvector;		/* [4m] tells what contexts are busy */
int	*ctx_phys_tbl;		/* [4m] SRMMU-edible context table */

caddr_t	vpage[2];		/* two reserved MD virtual pages */
caddr_t	vmmap;			/* one reserved MI vpage for /dev/mem */
caddr_t	vdumppages;		/* 32KB worth of reserved dump pages */

#ifdef MMU_3L
smeg_t		tregion;
#endif
struct pmap	kernel_pmap_store;		/* the kernel's pmap */
struct regmap	kernel_regmap_store[NKREG];	/* the kernel's regmap */
struct segmap	kernel_segmap_store[NKREG*NSEGRG];/* the kernel's segmaps */

#ifdef SUN4M
u_int 	*kernel_regtable_store;		/* 1k of storage to map the kernel */
u_int	*kernel_segtable_store;		/* 2k of storage to map the kernel */
u_int	*kernel_pagtable_store;		/* 128k of storage to map the kernel */

u_int	*kernel_iopte_table;		/* 64k of storage for iommu */
u_int 	kernel_iopte_table_pa;

static 	int cant_cache_pagetables = 0;	/* 1 if page tables are uncachable */
#endif

#define	MA_SIZE	32		/* size of memory descriptor arrays */
struct	memarr pmemarr[MA_SIZE];/* physical memory regions */
int	npmemarr;		/* number of entries in pmemarr */
int	cpmemarr;		/* pmap_next_page() state */
/*static*/ vm_offset_t	avail_start;	/* first free physical page */
/*static*/ vm_offset_t	avail_end;	/* last free physical page */
/*static*/ vm_offset_t	avail_next;	/* pmap_next_page() state:
					   next free physical page */
/*static*/ vm_offset_t	virtual_avail;	/* first free virtual page number */
/*static*/ vm_offset_t	virtual_end;	/* last free virtual page number */
#ifdef SUN4M
#if 0 /* not needed anymore */
static void * pmap_bootstrap_malloc __P((unsigned long, int, int));

caddr_t		minipool_start;
caddr_t		minipool_current;
#define MINIPOOL_SIZE	8192  /* number of bytes to reserve for early malloc */
                              /* (must be multiple of pagesize) */
#endif
#endif

int mmu_has_hole;

vm_offset_t prom_vstart;	/* For /dev/kmem */
vm_offset_t prom_vend;

#if defined(SUN4)
/*
 * [sun4]: segfixmask: on some systems (4/110) "getsegmap()" returns a
 * partly invalid value. getsegmap returns a 16 bit value on the sun4,
 * but only the first 8 or so bits are valid (the rest are *supposed* to
 * be zero. On the 4/110 the bits that are supposed to be zero are
 * all one instead. e.g. KERNBASE is usually mapped by pmeg number zero.
 * On a 4/300 getsegmap(KERNBASE) == 0x0000, but
 * on a 4/100 getsegmap(KERNBASE) == 0xff00
 *
 * This confuses mmu_reservemon() and causes it to not reserve the PROM's
 * pmegs. Then the PROM's pmegs get used during autoconfig and everything
 * falls apart!  (not very fun to debug, BTW.)
 *
 * solution: mask the invalid bits in the getsetmap macro.
 */

static u_long segfixmask = 0xffffffff; /* all bits valid to start */
#else
#define segfixmask 0xffffffff	/* It's in getsegmap's scope */
#endif

/*
 * pseudo-functions for mnemonic value
 */
#define getcontext()		(CPU_ISSUN4M \
					? lda(SRMMU_CXR, ASI_SRMMU) \
					: lduba(AC_CONTEXT, ASI_CONTROL))
#define setcontext(c)		(CPU_ISSUN4M \
					? sta(SRMMU_CXR, ASI_SRMMU, c) \
					: stba(AC_CONTEXT, ASI_CONTROL, c))

#define	getsegmap(va)		(CPU_ISSUN4C \
					? lduba(va, ASI_SEGMAP) \
					: (lduha(va, ASI_SEGMAP) & segfixmask))
#define	setsegmap(va, pmeg)	(CPU_ISSUN4C \
					? stba(va, ASI_SEGMAP, pmeg) \
					: stha(va, ASI_SEGMAP, pmeg))

/* 3-level sun4 MMU only: */
#define	getregmap(va)		((unsigned)lduha((va)+2, ASI_REGMAP) >> 8)
#define	setregmap(va, smeg)	stha((va)+2, ASI_REGMAP, (smeg << 8))

#if defined(SUN4M)
#define getpte4m(va)		lda((va & 0xFFFFF000) | ASI_SRMMUFP_L3, \
				    ASI_SRMMUFP)
void	setpte4m __P((vm_offset_t va, int pte));
void	setptesw4m __P((struct pmap *pm, vm_offset_t va, int pte));
u_int	getptesw4m __P((struct pmap *pm, vm_offset_t va));
#endif

#if defined(SUN4) || defined(SUN4C)
#define	getpte4(va)		lda(va, ASI_PTE)
#define	setpte4(va, pte)	sta(va, ASI_PTE, pte)
#endif

/* Function pointer messiness for supporting multiple sparc architectures
 * within a single kernel: notice that there are two versions of many of the
 * functions within this file/module, one for the sun4/sun4c and the other
 * for the sun4m. For performance reasons (since things like pte bits don't
 * map nicely between the two architectures), there are separate functions
 * rather than unified functions which test the cputyp variable. If only
 * one architecture is being used, then the non-suffixed function calls
 * are macro-translated into the appropriate xxx4_4c or xxx4m call. If
 * multiple architectures are defined, the calls translate to (*xxx_p),
 * i.e. they indirect through function pointers initialized as appropriate
 * to the run-time architecture in pmap_bootstrap. See also pmap.h.
 */

#if defined(SUN4M)
static void mmu_setup4m_L1 __P((int, caddr_t, struct pmap *, caddr_t *));
static void mmu_setup4m_L2 __P((int, caddr_t, struct regmap *, caddr_t *,
				struct segmap *));
static int  mmu_setup4m_L3 __P((int, caddr_t));
/*static*/ void	mmu_reservemon4m __P((struct pmap *, caddr_t *));

/*static*/ void pmap_rmk4m __P((struct pmap *, vm_offset_t, vm_offset_t,
                          int, int));
/*static*/ void pmap_rmu4m __P((struct pmap *, vm_offset_t, vm_offset_t,
                          int, int));
/*static*/ void pmap_enk4m __P((struct pmap *, vm_offset_t, vm_prot_t,
			  int, struct pvlist *, int));
/*static*/ void pmap_enu4m __P((struct pmap *, vm_offset_t, vm_prot_t,
			  int, struct pvlist *, int));
/*static*/ void pv_changepte4m __P((struct pvlist *, int, int));
/*static*/ int  pv_syncflags4m __P((struct pvlist *));
/*static*/ int  pv_link4m __P((struct pvlist *, struct pmap *, vm_offset_t));
/*static*/ void pv_unlink4m __P((struct pvlist *, struct pmap *, vm_offset_t));
#endif

#if defined(SUN4) || defined(SUN4C)
/*static*/ void	mmu_reservemon4_4c __P((int *, int *));
/*static*/ void pmap_rmk4_4c __P((struct pmap *, vm_offset_t, vm_offset_t,
                          int, int));
/*static*/ void pmap_rmu4_4c __P((struct pmap *, vm_offset_t, vm_offset_t,
                          int, int));
/*static*/ void pmap_enk4_4c __P((struct pmap *, vm_offset_t, vm_prot_t,
			  int, struct pvlist *, int));
/*static*/ void pmap_enu4_4c __P((struct pmap *, vm_offset_t, vm_prot_t,
			  int, struct pvlist *, int));
/*static*/ void pv_changepte4_4c __P((struct pvlist *, int, int));
/*static*/ int  pv_syncflags4_4c __P((struct pvlist *));
/*static*/ int  pv_link4_4c __P((struct pvlist *, struct pmap *, vm_offset_t));
/*static*/ void pv_unlink4_4c __P((struct pvlist *, struct pmap *, vm_offset_t));
#endif

#if !defined(SUN4M) && (defined(SUN4) || defined(SUN4C))
#define		pmap_rmk	pmap_rmk4_4c
#define		pmap_rmu	pmap_rmu4_4c

#elif defined(SUN4M) && !(defined(SUN4) || defined(SUN4C))
#define		pmap_rmk	pmap_rmk4m
#define		pmap_rmu	pmap_rmu4m

#else  /* must use function pointers */

/* function pointer declarations */
/* from pmap.h: */
void      	(*pmap_clear_modify_p) __P((vm_offset_t pa));
void            (*pmap_clear_reference_p) __P((vm_offset_t pa));
void            (*pmap_copy_page_p) __P((vm_offset_t, vm_offset_t));
void            (*pmap_enter_p) __P((pmap_t,
		     vm_offset_t, vm_offset_t, vm_prot_t, boolean_t));
vm_offset_t     (*pmap_extract_p) __P((pmap_t, vm_offset_t));
boolean_t       (*pmap_is_modified_p) __P((vm_offset_t pa));
boolean_t       (*pmap_is_referenced_p) __P((vm_offset_t pa));
void            (*pmap_page_protect_p) __P((vm_offset_t, vm_prot_t));
void            (*pmap_protect_p) __P((pmap_t,
		     vm_offset_t, vm_offset_t, vm_prot_t));
void            (*pmap_zero_page_p) __P((vm_offset_t));
void	       	(*pmap_changeprot_p) __P((pmap_t, vm_offset_t,
		     vm_prot_t, int));
/* local: */
void 		(*pmap_rmk_p) __P((struct pmap *, vm_offset_t, vm_offset_t,
                          int, int));
void 		(*pmap_rmu_p) __P((struct pmap *, vm_offset_t, vm_offset_t,
                          int, int));

#define		pmap_rmk	(*pmap_rmk_p)
#define		pmap_rmu	(*pmap_rmu_p)

#endif

/* --------------------------------------------------------------*/

/*
 * Next we have some Sun4m-specific routines which have no 4/4c
 * counterparts, or which are 4/4c macros.
 */

#if defined(SUN4M)

/* Macros which implement SRMMU TLB flushing/invalidation */

#define tlb_flush_page(va)    sta((va & ~0xfff) | ASI_SRMMUFP_L3, ASI_SRMMUFP,0)
#define tlb_flush_segment(vreg, vseg) sta((vreg << RGSHIFT) | (vseg << SGSHIFT)\
					  | ASI_SRMMUFP_L2, ASI_SRMMUFP,0)
#define tlb_flush_context()   sta(ASI_SRMMUFP_L1, ASI_SRMMUFP, 0)
#define tlb_flush_all()	      sta(ASI_SRMMUFP_LN, ASI_SRMMUFP, 0)

static u_int	VA2PA __P((caddr_t));

/*
 * VA2PA(addr) -- converts a virtual address to a physical address using
 * the MMU's currently-installed page tables. As a side effect, the address
 * translation used may cause the associated pte to be encached. The correct
 * context for VA must be set before this is called.
 *
 * This routine should work with any level of mapping, as it is used
 * during bootup to interact with the ROM's initial L1 mapping of the kernel.
 */
static __inline u_int
VA2PA(addr)
	register caddr_t addr;
{
	register u_int pte;

	/* we'll use that handy SRMMU flush/probe! %%%: make consts below! */
	/* Try each level in turn until we find a valid pte. Otherwise panic */

	pte = lda(((u_int)addr & ~0xfff) | ASI_SRMMUFP_L3, ASI_SRMMUFP);
	if ((pte & SRMMU_TETYPE) == SRMMU_TEPTE)
	    return (((pte & SRMMU_PPNMASK) << SRMMU_PPNPASHIFT) |
		    ((u_int)addr & 0xfff));

	/* A `TLB Flush Entire' is required before any L0, L1 or L2 probe */
	tlb_flush_all();

	pte = lda(((u_int)addr & ~0xfff) | ASI_SRMMUFP_L2, ASI_SRMMUFP);
	if ((pte & SRMMU_TETYPE) == SRMMU_TEPTE)
	    return (((pte & SRMMU_PPNMASK) << SRMMU_PPNPASHIFT) |
		    ((u_int)addr & 0x3ffff));
	pte = lda(((u_int)addr & ~0xfff) | ASI_SRMMUFP_L1, ASI_SRMMUFP);
	if ((pte & SRMMU_TETYPE) == SRMMU_TEPTE)
	    return (((pte & SRMMU_PPNMASK) << SRMMU_PPNPASHIFT) |
		    ((u_int)addr & 0xffffff));
	pte = lda(((u_int)addr & ~0xfff) | ASI_SRMMUFP_L0, ASI_SRMMUFP);
	if ((pte & SRMMU_TETYPE) == SRMMU_TEPTE)
	    return (((pte & SRMMU_PPNMASK) << SRMMU_PPNPASHIFT) |
		    ((u_int)addr & 0xffffffff));

	panic("VA2PA: Asked to translate unmapped VA %p", addr);
}

/*
 * Get the page table entry (PTE) for va by looking it up in the software
 * page tables. These are the same tables that are used by the MMU; this
 * routine allows easy access to the page tables even if the context
 * corresponding to the table is not loaded or selected.
 * This routine should NOT be used if there is any chance that the desired
 * pte is in the TLB cache, since it will return stale data in that case.
 * For that case, and for general use, use getpte4m, which is much faster
 * and avoids walking in-memory page tables if the page is in the cache.
 * Note also that this routine only works if a kernel mapping has been
 * installed for the given page!
 */
__inline u_int
getptesw4m(pm, va)		/* Assumes L3 mapping! */
	register struct pmap *pm;
	register vm_offset_t va;
{
	register struct regmap *rm;
	register struct segmap *sm;

	rm = &pm->pm_regmap[VA_VREG(va)];
#ifdef DEBUG
	if (rm == NULL)
		panic("getptesw4m: no regmap entry");
#endif
	sm = &rm->rg_segmap[VA_VSEG(va)];
#ifdef DEBUG
	if (sm == NULL)
		panic("getptesw4m: no segmap");
#endif
	return (sm->sg_pte[VA_SUN4M_VPG(va)]); 	/* return pte */
}

/*
 * Set the page table entry for va to pte. Only affects software MMU page-
 * tables (the in-core pagetables read by the MMU). Ignores TLB, and
 * thus should _not_ be called if the pte translation could be in the TLB.
 * In this case, use setpte4m().
 */
__inline void
setptesw4m(pm, va, pte)
	register struct pmap *pm;
	register vm_offset_t va;
	register int pte;
{
	register struct regmap *rm;
	register struct segmap *sm;

	rm = &pm->pm_regmap[VA_VREG(va)];

#ifdef DEBUG
	if (pm->pm_regmap == NULL || rm == NULL)
		panic("setpte4m: no regmap entry");
#endif
	sm = &rm->rg_segmap[VA_VSEG(va)];

#ifdef DEBUG
	if (rm->rg_segmap == NULL || sm == NULL || sm->sg_pte == NULL)
		panic("setpte4m: no segmap for va %p", (caddr_t)va);
#endif
	sm->sg_pte[VA_SUN4M_VPG(va)] = pte; /* set new pte */
}

/* Set the page table entry for va to pte. Flushes cache. */
__inline void
setpte4m(va, pte)
	register vm_offset_t va;
	register int pte;
{
	register struct pmap *pm;
	register struct regmap *rm;
	register struct segmap *sm;
	register union ctxinfo *c;

	if (vactype != VAC_NONE)
		cache_flush_page(va);

	/*
	 * Now walk tables to find pte. We use ctxinfo to locate the pmap
	 * from the current context
	 */
#ifdef DEBUG
	if (ctxbusyvector[getcontext()] == 0)
		panic("setpte4m: no pmap for current context (%d)",
		      getcontext());
#endif
	c = &ctxinfo[getcontext()];
	pm = c->c_pmap;

	/* Note: inline version of setptesw4m() */
#ifdef DEBUG
	if (pm->pm_regmap == NULL)
		panic("setpte4m: no regmap entry");
#endif
	rm = &pm->pm_regmap[VA_VREG(va)];
	sm = &rm->rg_segmap[VA_VSEG(va)];

#ifdef DEBUG
	if (rm->rg_segmap == NULL || sm == NULL || sm->sg_pte == NULL)
		panic("setpte4m: no segmap for va %p", (caddr_t)va);
#endif
	tlb_flush_page(va);
	sm->sg_pte[VA_SUN4M_VPG(va)] = pte; /* set new pte */
}
#endif /* 4m only */

/*----------------------------------------------------------------*/

#if defined(MMU_3L)
#define CTX_USABLE(pm,rp)	(CPU_ISSUN4M \
				    ? ((pm)->pm_ctx != NULL ) \
				    : ((pm)->pm_ctx != NULL && \
					(!mmu_3l || (rp)->rg_smeg != reginval)))
#else
#define CTX_USABLE(pm,rp)	((pm)->pm_ctx != NULL )
#endif

#define GAP_WIDEN(pm,vr) do if (CPU_ISSUN4OR4C) {	\
	if (vr + 1 == pm->pm_gap_start)			\
		pm->pm_gap_start = vr;			\
	if (vr == pm->pm_gap_end)			\
		pm->pm_gap_end = vr + 1;		\
} while (0)

#define GAP_SHRINK(pm,vr) do if (CPU_ISSUN4OR4C) {			\
	register int x;							\
	x = pm->pm_gap_start + (pm->pm_gap_end - pm->pm_gap_start) / 2;	\
	if (vr > x) {							\
		if (vr < pm->pm_gap_end)				\
			pm->pm_gap_end = vr;				\
	} else {							\
		if (vr >= pm->pm_gap_start && x != pm->pm_gap_start)	\
			pm->pm_gap_start = vr + 1;			\
	}								\
} while (0)

static void sortm __P((struct memarr *, int));
void	ctx_alloc __P((struct pmap *));
void	ctx_free __P((struct pmap *));
void	pv_flushcache __P((struct pvlist *));
void	kvm_iocache __P((caddr_t, int));
#ifdef DEBUG
void	pm_check __P((char *, struct pmap *));
void	pm_check_k __P((char *, struct pmap *));
void	pm_check_u __P((char *, struct pmap *));
#endif


/*
 * Sort a memory array by address.
 */
static void
sortm(mp, n)
	register struct memarr *mp;
	register int n;
{
	register struct memarr *mpj;
	register int i, j;
	register u_int addr, len;

	/* Insertion sort.  This is O(n^2), but so what? */
	for (i = 1; i < n; i++) {
		/* save i'th entry */
		addr = mp[i].addr;
		len = mp[i].len;
		/* find j such that i'th entry goes before j'th */
		for (j = 0, mpj = mp; j < i; j++, mpj++)
			if (addr < mpj->addr)
				break;
		/* slide up any additional entries */
		ovbcopy(mpj, mpj + 1, (i - j) * sizeof(*mp));
		mpj->addr = addr;
		mpj->len = len;
	}
}

/*
 * For our convenience, vm_page.c implements:
 *       pmap_startup(), pmap_steal_memory()
 * using the functions:
 *       pmap_virtual_space(), pmap_free_pages(), pmap_next_page(),
 * which are much simpler to implement.
 */

/*
 * How much virtual space does this kernel have?
 * (After mapping kernel text, data, etc.)
 */
void
pmap_virtual_space(v_start, v_end)
        vm_offset_t *v_start;
        vm_offset_t *v_end;
{
        *v_start = virtual_avail;
        *v_end   = virtual_end;
}

/*
 * Return the number of page indices in the range of
 * possible return values for pmap_page_index() for
 * all addresses provided by pmap_next_page().  This
 * return value is used to allocate per-page data.
 *
 */
u_int
pmap_free_pages()
{
	int long bytes;
	int nmem;
	register struct memarr *mp;

	bytes = -avail_start;
	for (mp = pmemarr, nmem = npmemarr; --nmem >= 0; mp++)
		bytes += mp->len;

        return atop(bytes);
}

/*
 * If there are still physical pages available, put the address of
 * the next available one at paddr and return TRUE.  Otherwise,
 * return FALSE to indicate that there are no more free pages.
 * Note that avail_next is set to avail_start in pmap_bootstrap().
 *
 * Imporant:  The page indices of the pages returned here must be
 * in ascending order.
 */
int
pmap_next_page(paddr)
        vm_offset_t *paddr;
{

        /* Is it time to skip over a hole? */
	if (avail_next == pmemarr[cpmemarr].addr + pmemarr[cpmemarr].len) {
		if (++cpmemarr == npmemarr)
			return FALSE;
		avail_next = pmemarr[cpmemarr].addr;
	}

#ifdef DIAGNOSTIC
        /* Any available memory remaining? */
        if (avail_next >= avail_end) {
		panic("pmap_next_page: too much memory?!\n");
	}
#endif

        /* Have memory, will travel... */
        *paddr = avail_next;
        avail_next += NBPG;
        return TRUE;
}

/*
 * pmap_page_index()
 *
 * Given a physical address, return a page index.
 *
 * There can be some values that we never return (i.e. a hole)
 * as long as the range of indices returned by this function
 * is smaller than the value returned by pmap_free_pages().
 * The returned index does NOT need to start at zero.
 *
 */
int
pmap_page_index(pa)
	vm_offset_t pa;
{
	int idx;
	int nmem;
	register struct memarr *mp;

#ifdef  DIAGNOSTIC
	if (pa < avail_start || pa >= avail_end)
		panic("pmap_page_index: pa=0x%lx", pa);
#endif

	for (idx = 0, mp = pmemarr, nmem = npmemarr; --nmem >= 0; mp++) {
		if (pa >= mp->addr && pa < mp->addr + mp->len)
			break;
		idx += atop(mp->len);
	}

	return (idx + atop(pa - mp->addr));
}

int
pmap_pa_exists(pa)
	vm_offset_t pa;
{
	register int nmem;
	register struct memarr *mp;

	for (mp = pmemarr, nmem = npmemarr; --nmem >= 0; mp++) {
		if (pa >= mp->addr && pa < mp->addr + mp->len)
			return 1;
	}

	return 0;
}

/* update pv_flags given a valid pte */
#define	MR4_4C(pte) (((pte) >> PG_M_SHIFT) & (PV_MOD | PV_REF))
#define MR4M(pte) (((pte) >> PG_M_SHIFT4M) & (PV_MOD4M | PV_REF4M))

/*----------------------------------------------------------------*/

/*
 * Agree with the monitor ROM as to how many MMU entries are
 * to be reserved, and map all of its segments into all contexts.
 *
 * Unfortunately, while the Version 0 PROM had a nice linked list of
 * taken virtual memory, the Version 2 PROM provides instead a convoluted
 * description of *free* virtual memory.  Rather than invert this, we
 * resort to two magic constants from the PROM vector description file.
 */
#if defined(SUN4) || defined(SUN4C)
void
mmu_reservemon4_4c(nrp, nsp)
	register int *nrp, *nsp;
{
	register vm_offset_t va = 0, eva = 0;
	register int mmuseg, i, nr, ns, vr, lastvr;
#ifdef MMU_3L
	register int mmureg;
#endif
	register struct regmap *rp;

#if defined(SUN4M)
	if (CPU_ISSUN4M) {
		panic("mmu_reservemon called on Sun4M machine");
		return;
	}
#endif

#if defined(SUN4)
	if (CPU_ISSUN4) {
		prom_vstart = va = OLDMON_STARTVADDR;
		prom_vend = eva = OLDMON_ENDVADDR;
	}
#endif
#if defined(SUN4C)
	if (CPU_ISSUN4C) {
		prom_vstart = va = OPENPROM_STARTVADDR;
		prom_vend = eva = OPENPROM_ENDVADDR;
	}
#endif
	ns = *nsp;
	nr = *nrp;
	lastvr = 0;
	while (va < eva) {
		vr = VA_VREG(va);
		rp = &pmap_kernel()->pm_regmap[vr];

#ifdef MMU_3L
		if (mmu_3l && vr != lastvr) {
			lastvr = vr;
			mmureg = getregmap(va);
			if (mmureg < nr)
				rp->rg_smeg = nr = mmureg;
			/*
			 * On 3-level MMU machines, we distribute regions,
			 * rather than segments, amongst the contexts.
			 */
			for (i = ncontext; --i > 0;)
				(*promvec->pv_setctxt)(i, (caddr_t)va, mmureg);
		}
#endif
		mmuseg = getsegmap(va);
		if (mmuseg < ns)
			ns = mmuseg;
#ifdef MMU_3L
		if (!mmu_3l)
#endif
			for (i = ncontext; --i > 0;)
				(*promvec->pv_setctxt)(i, (caddr_t)va, mmuseg);

		if (mmuseg == seginval) {
			va += NBPSG;
			continue;
		}
		/*
		 * Another PROM segment. Enter into region map.
		 * Assume the entire segment is valid.
		 */
		rp->rg_nsegmap += 1;
		rp->rg_segmap[VA_VSEG(va)].sg_pmeg = mmuseg;
		rp->rg_segmap[VA_VSEG(va)].sg_npte = NPTESG;

		/* PROM maps its memory user-accessible: fix it. */
		for (i = NPTESG; --i >= 0; va += NBPG)
			setpte4(va, getpte4(va) | PG_S);
	}
	*nsp = ns;
	*nrp = nr;
	return;
}
#endif

#if defined(SUN4M) /* Sun4M versions of above */

/*
 * Take the monitor's initial page table layout, convert it to 3rd-level pte's
 * (it starts out as a L1 mapping), and install it along with a set of kernel
 * mapping tables as the kernel's initial page table setup. Also create and
 * enable a context table. I suppose we also want to block user-mode access
 * to the new kernel/ROM mappings.
 */

/*
 * mmu_reservemon4m(): Copies the existing (ROM) page tables to kernel space,
 * converting any L1/L2 PTEs to L3 PTEs. Does *not* copy the L1 entry mapping
 * the kernel at 0xf8000000 (KERNBASE) since we don't want to map 16M of
 * physical memory for the kernel. Thus the kernel must be installed later!
 * Also installs ROM mappings into the kernel pmap.
 * NOTE: This also revokes all user-mode access to the mapped regions.
 * NOTE ALSO: Must set up kernel pmap as all invalid before this is called.
 */
void
mmu_reservemon4m(kpmap, kmemtop)
	struct pmap *kpmap;
	register caddr_t *kmemtop; /* Note: this is a *virtual* address! */
{
	unsigned int ctxtblptr;
	int wierdviking = 0;
	register int te;
	register caddr_t tableptr;
	unsigned int mmupcrsav;

/*XXX-GCC!*/mmupcrsav = 0;

	/*
	 * XXX: although the Sun4M can handle 36 bits of physical
	 * address space, we assume that all these page tables, etc
	 * are in the lower 4G (32-bits) of address space, i.e. out of I/O
	 * space. Eventually this should be changed to support the 36 bit
	 * physical addressing, in case some crazed ROM designer decides to
	 * stick the pagetables up there. In that case, we should use MMU
	 * transparent mode, (i.e. ASI 0x20 to 0x2f) to access
	 * physical memory.
	 */

	ctxtblptr = (lda(SRMMU_CXTPTR,ASI_SRMMU) << SRMMU_PPNPASHIFT);

	/* We're going to have to use MMU passthrough. If we're on a
	 * Viking MicroSparc without an mbus, we need to turn off traps
	 * and set the AC bit at 0x8000 in the MMU's control register. Ugh.
	 * XXX: Once we've done this, can we still access kernel vm?
	 */
	if ((getpsr() & 0x40000000) && (!(lda(SRMMU_PCR,ASI_SRMMU) & 0x800))) {
		wierdviking = 1;
		sta(SRMMU_PCR, ASI_SRMMU, 	/* set MMU AC bit */
		    ((mmupcrsav = lda(SRMMU_PCR,ASI_SRMMU)) | SRMMU_PCR_AC));
	}
	if (ncontext < 1024) {
		/* to keep supersparc happy, still align on 4K boundary */
		ctx_phys_tbl = (int *)roundup((u_int) *kmemtop, 4096);
		*kmemtop = (caddr_t)((u_int)ctx_phys_tbl + 4096);
		qzero(ctx_phys_tbl, 4096);
	} else {
		ctx_phys_tbl = (int *)roundup((u_int) *kmemtop,
					      ncontext * sizeof(int));
		*kmemtop = (caddr_t)((u_int)ctx_phys_tbl +
				     (ncontext * sizeof(int)));
		qzero(ctx_phys_tbl, ncontext * sizeof(int));
	}

	te = lda(ctxtblptr, ASI_BYPASS);
	switch (te & SRMMU_TETYPE) {
	case SRMMU_TEINVALID:
		ctx_phys_tbl[0] = SRMMU_TEINVALID;
		panic("mmu_reservemon4m: no existing L0 mapping! (How are we running?");
		break;
	case SRMMU_TEPTE:
#ifdef DEBUG
		printf("mmu_reservemon4m: trying to remap 4G segment!\n");
#endif
		panic("mmu_reservemon4m: can't handle ROM 4G page size");
		/* XXX: Should make this work, however stupid it is */
		break;
	case SRMMU_TEPTD:
		tableptr = (caddr_t) roundup((u_int) *kmemtop,
					     SRMMU_L1SIZE * sizeof(long));
		*kmemtop = tableptr + (SRMMU_L1SIZE * sizeof(long));
		bzero(tableptr, *kmemtop - tableptr);
		ctx_phys_tbl[0] = (VA2PA(tableptr) >> SRMMU_PPNPASHIFT) |
		    SRMMU_TEPTD;
		kpmap->pm_reg_ptps = (int *)tableptr;
		kpmap->pm_reg_ptps_pa = VA2PA(tableptr);
		mmu_setup4m_L1(te, tableptr, kpmap, kmemtop);
		break;
	default:
		panic("mmu_reservemon4m: unknown pagetable entry type");
	}

	if (wierdviking == 1) {
		sta(SRMMU_PCR, ASI_SRMMU, mmupcrsav);
	}
}

void
mmu_setup4m_L1(regtblptd, newtableptr, kpmap, kmemtop)
	int regtblptd;		/* PTD for region table to be remapped */
	caddr_t newtableptr;	/* Virtual ptr to new region table */
	struct pmap *kpmap;
	register caddr_t *kmemtop;
{
	register unsigned int regtblrover;
	register int i;
	unsigned int te;
	caddr_t segtblptr;
	register caddr_t pagtblptr;
	struct regmap *thisregmap = NULL;
	struct segmap *segmaps;
	int j, k;

	/* Here we scan the region table to copy any entries which appear.
	 * We are only concerned with regions in kernel space and above
	 * (i.e. regions 0xf8 to 0xff). We also ignore region 0xf8, since
	 * that is the 16MB L1 mapping that the ROM used to map the kernel
	 * in initially. Later, we will rebuild a new L3 mapping for the
	 * kernel and install it before switching to the new pagetables.
	 */
	regtblrover = ((regtblptd & ~SRMMU_TETYPE) << SRMMU_PPNPASHIFT) +
	    (VA_VREG(KERNBASE)+1) * sizeof(long);      /* kernel only */

	for (i = VA_VREG(KERNBASE) + 1; i < SRMMU_L1SIZE;
	     i++, regtblrover += sizeof(long)) {
		te = lda(regtblrover, ASI_BYPASS);
		switch(te & SRMMU_TETYPE) {
		case SRMMU_TEINVALID:
			((int *)newtableptr)[i] = SRMMU_TEINVALID;
			break;
		case SRMMU_TEPTE:
#ifdef DEBUG
			printf("mmu_reservemon4m: converting region 0x%x from L1->L3\n",i);
#endif
			/* We must build segment and page tables, then
			 *  fill them in
			 */
			thisregmap = &kpmap->pm_regmap[i];
			bzero(thisregmap, sizeof(struct regmap));
			segmaps = &kernel_segmap_store[(i-VA_VREG(KERNBASE))*NSEGRG];
			segtblptr = (caddr_t) roundup((u_int) *kmemtop,
					    SRMMU_L2SIZE * sizeof(long));
			*kmemtop = segtblptr + (SRMMU_L2SIZE * sizeof(long));
			bzero(segtblptr, *kmemtop - segtblptr);
			bzero(segmaps, NSEGRG * sizeof(struct segmap));
			thisregmap->rg_segmap = segmaps;
			thisregmap->rg_seg_ptps = (int *)segtblptr;
			((int *)newtableptr)[i] = (VA2PA(segtblptr) >>
						   SRMMU_PPNPASHIFT) |
						       SRMMU_TEPTD;
			for (j = 0; j < SRMMU_L2SIZE; j++) {
				thisregmap->rg_nsegmap++;
				pagtblptr = (caddr_t) roundup((u_int) *kmemtop,
						    SRMMU_L3SIZE * sizeof(int));
				*kmemtop = segtblptr + (SRMMU_L3SIZE *
							sizeof(long));
				bzero(pagtblptr, *kmemtop - pagtblptr);
				((int *)segtblptr)[j] = (VA2PA(pagtblptr) >>
							 SRMMU_PPNPASHIFT) |
							     SRMMU_TEPTD;
				segmaps[j].sg_pte = (int *)pagtblptr;

				for (k = 0; k < SRMMU_L3SIZE; k++) {
					segmaps[j].sg_npte++;
					((int *)pagtblptr)[k] =
					    (te & SRMMU_L1PPNMASK) |
					    (j << SRMMU_L2PPNSHFT) |
					    (k << SRMMU_L3PPNSHFT) |
					    (te & SRMMU_PGBITSMSK) |
					    ((te & SRMMU_PROT_MASK) |
					     PPROT_U2S_OMASK) |
					    SRMMU_TEPTE;
				}
			}
			break;
		case SRMMU_TEPTD:
			thisregmap = &kpmap->pm_regmap[i];
			bzero(thisregmap, sizeof(struct regmap));
			segtblptr = (caddr_t) roundup((u_int)*kmemtop,
					    SRMMU_L2SIZE * sizeof(long));
			*kmemtop = segtblptr + (SRMMU_L2SIZE * sizeof(long));
			segmaps = &kernel_segmap_store[(i-(KERNBASE >> RGSHIFT))*NSEGRG];
			bzero(segtblptr, *kmemtop - segtblptr);
			bzero(segmaps, NSEGRG * sizeof(struct segmap));
			thisregmap->rg_segmap = segmaps;
			thisregmap->rg_seg_ptps = (int *)segtblptr;
			((int *)newtableptr)[i] = (VA2PA(segtblptr) >>
						   SRMMU_PPNPASHIFT) |
						       SRMMU_TEPTD;
			mmu_setup4m_L2(te, segtblptr, thisregmap, kmemtop, segmaps);
			break;
		default:
			panic("mmu_setup4m_L1: unknown pagetable entry type");
		}
	}
}

void
mmu_setup4m_L2(segtblptd, newtableptr, pregmap, kmemtop, segmaps)
	int segtblptd;
	caddr_t newtableptr;
	struct regmap *pregmap;
	register caddr_t *kmemtop;
	struct segmap *segmaps;
{
	register unsigned int segtblrover;
	register int i, k;
	unsigned int te;
	register caddr_t pagtblptr;
	struct segmap *thissegmap = NULL;

	segtblrover = (segtblptd & ~SRMMU_TETYPE) << SRMMU_PPNPASHIFT;
	for (i = 0; i < SRMMU_L2SIZE; i++, segtblrover += sizeof(long)) {
		te = lda(segtblrover, ASI_BYPASS);
		switch(te & SRMMU_TETYPE) {
		case SRMMU_TEINVALID:
			((int *)newtableptr)[i] = SRMMU_TEINVALID;
			break;
		case SRMMU_TEPTE:
#ifdef DEBUG
			printf("mmu_reservemon4m: converting L2 entry at segment 0x%x to L3\n",i);
#endif
			pregmap->rg_nsegmap++;
			/* We must build page tables and fill them in */
			pagtblptr = (caddr_t) roundup((u_int) *kmemtop,
						      SRMMU_L3SIZE*sizeof(int));
			*kmemtop = pagtblptr + (SRMMU_L3SIZE *
						sizeof(int));
			bzero(pagtblptr, *kmemtop - pagtblptr);
			((int *)newtableptr)[i] = (VA2PA(pagtblptr) >>
						 SRMMU_PPNPASHIFT) |
						     SRMMU_TEPTD;
			segmaps[i].sg_pte = (int *) pagtblptr;

			for (k = 0; k < SRMMU_L3SIZE; k++) {
				segmaps[i].sg_npte++;
				((int *)pagtblptr)[k] =
				    (te & SRMMU_L1PPNMASK) |
				    (te & SRMMU_L2PPNMASK) |
				    (k << SRMMU_L3PPNSHFT) |
				    (te & SRMMU_PGBITSMSK) |
				    ((te & SRMMU_PROT_MASK) |
				     PPROT_U2S_OMASK) |
				    SRMMU_TEPTE;
			}
			break;
		case SRMMU_TEPTD:
			pregmap->rg_nsegmap++;
			thissegmap = &segmaps[i];
			pagtblptr = (caddr_t) roundup((u_int) *kmemtop,
						      SRMMU_L3SIZE*sizeof(int));
			*kmemtop = pagtblptr + (SRMMU_L3SIZE * sizeof(int));
			bzero(pagtblptr, *kmemtop - pagtblptr);
			((int *)newtableptr)[i] = (VA2PA(pagtblptr) >>
						   SRMMU_PPNPASHIFT) |
						       SRMMU_TEPTD;
			thissegmap->sg_pte = (int *) pagtblptr;
			thissegmap->sg_npte += mmu_setup4m_L3(te, pagtblptr);
			break;
		default:
			panic("mmu_setup4m_L2: unknown pagetable entry type");
		}
	}
}

int
mmu_setup4m_L3(pagtblptd, newtableptr)
	register int pagtblptd;
	register caddr_t newtableptr;
{
	register unsigned int pagtblrover;
	register int i, n = 0;
	register unsigned int te;

	pagtblrover = (pagtblptd & ~SRMMU_TETYPE) << SRMMU_PPNPASHIFT;
	for (i = 0; i < SRMMU_L3SIZE; i++, pagtblrover += sizeof(long)) {
		te = lda(pagtblrover, ASI_BYPASS);
		switch(te & SRMMU_TETYPE) {
		case SRMMU_TEINVALID:
			((int *)newtableptr)[i] = SRMMU_TEINVALID;
			break;
		case SRMMU_TEPTE:
			((int *)newtableptr)[i] = te | PPROT_U2S_OMASK;
			n++;
			break;
		case SRMMU_TEPTD:
			panic("mmu_setup4m_L3: PTD found in L3 page table");
		default:
			panic("mmu_setup4m_L3: unknown pagetable entry type");
		}
	}
	return n;
}
#endif /* defined SUN4M */

/*
 * TODO: agree with the ROM on physical pages by taking them away
 * from the page list, rather than having a dinky BTSIZE above.
 */

/*----------------------------------------------------------------*/

/*
 * MMU management.
 */
struct mmuentry *me_alloc __P((struct mmuhd *, struct pmap *, int, int));
void		me_free __P((struct pmap *, u_int));
struct mmuentry	*region_alloc __P((struct mmuhd *, struct pmap *, int));
void		region_free __P((struct pmap *, u_int));

/*
 * Change contexts.  We need the old context number as well as the new
 * one.  If the context is changing, we must write all user windows
 * first, lest an interrupt cause them to be written to the (other)
 * user whose context we set here.
 */
#define	CHANGE_CONTEXTS(old, new) \
	if ((old) != (new)) { \
		write_user_windows(); \
		setcontext(new); \
	}

#if defined(SUN4) || defined(SUN4C) /* This is old sun MMU stuff */
/*
 * Allocate an MMU entry (i.e., a PMEG).
 * If necessary, steal one from someone else.
 * Put it on the tail of the given queue
 * (which is either the LRU list or the locked list).
 * The locked list is not actually ordered, but this is easiest.
 * Also put it on the given (new) pmap's chain,
 * enter its pmeg number into that pmap's segmap,
 * and store the pmeg's new virtual segment number (me->me_vseg).
 *
 * This routine is large and complicated, but it must be fast
 * since it implements the dynamic allocation of MMU entries.
 */
struct mmuentry *
me_alloc(mh, newpm, newvreg, newvseg)
	register struct mmuhd *mh;
	register struct pmap *newpm;
	register int newvreg, newvseg;
{
	register struct mmuentry *me;
	register struct pmap *pm;
	register int i, va, pa, *pte, tpte;
	int ctx;
	struct regmap *rp;
	struct segmap *sp;

	/* try free list first */
	if ((me = segm_freelist.tqh_first) != NULL) {
		TAILQ_REMOVE(&segm_freelist, me, me_list);
#ifdef DEBUG
		if (me->me_pmap != NULL)
			panic("me_alloc: freelist entry has pmap");
		if (pmapdebug & PDB_MMU_ALLOC)
			printf("me_alloc: got pmeg %d\n", me->me_cookie);
#endif
		TAILQ_INSERT_TAIL(mh, me, me_list);

		/* onto on pmap chain; pmap is already locked, if needed */
		TAILQ_INSERT_TAIL(&newpm->pm_seglist, me, me_pmchain);

		/* into pmap segment table, with backpointers */
		newpm->pm_regmap[newvreg].rg_segmap[newvseg].sg_pmeg = me->me_cookie;
		me->me_pmap = newpm;
		me->me_vseg = newvseg;
		me->me_vreg = newvreg;

		return (me);
	}

	/* no luck, take head of LRU list */
	if ((me = segm_lru.tqh_first) == NULL)
		panic("me_alloc: all pmegs gone");

	pm = me->me_pmap;
	if (pm == NULL)
		panic("me_alloc: LRU entry has no pmap");
	if (pm == pmap_kernel())
		panic("me_alloc: stealing from kernel");
#ifdef DEBUG
	if (pmapdebug & (PDB_MMU_ALLOC | PDB_MMU_STEAL))
		printf("me_alloc: stealing pmeg %x from pmap %p\n",
		    me->me_cookie, pm);
#endif
	/*
	 * Remove from LRU list, and insert at end of new list
	 * (probably the LRU list again, but so what?).
	 */
	TAILQ_REMOVE(&segm_lru, me, me_list);
	TAILQ_INSERT_TAIL(mh, me, me_list);

	rp = &pm->pm_regmap[me->me_vreg];
	if (rp->rg_segmap == NULL)
		panic("me_alloc: LRU entry's pmap has no segments");
	sp = &rp->rg_segmap[me->me_vseg];
	pte = sp->sg_pte;
	if (pte == NULL)
		panic("me_alloc: LRU entry's pmap has no ptes");

	/*
	 * The PMEG must be mapped into some context so that we can
	 * read its PTEs.  Use its current context if it has one;
	 * if not, and since context 0 is reserved for the kernel,
	 * the simplest method is to switch to 0 and map the PMEG
	 * to virtual address 0---which, being a user space address,
	 * is by definition not in use.
	 *
	 * XXX for ncpus>1 must use per-cpu VA?
	 * XXX do not have to flush cache immediately
	 */
	ctx = getcontext();
	if (CTX_USABLE(pm,rp)) {
		CHANGE_CONTEXTS(ctx, pm->pm_ctxnum);
		if (vactype != VAC_NONE)
			cache_flush_segment(me->me_vreg, me->me_vseg);
		va = VSTOVA(me->me_vreg,me->me_vseg);
	} else {
		CHANGE_CONTEXTS(ctx, 0);
#ifdef MMU_3L
		if (mmu_3l)
			setregmap(0, tregion);
#endif
		setsegmap(0, me->me_cookie);
		/*
		 * No cache flush needed: it happened earlier when
		 * the old context was taken.
		 */
		va = 0;
	}

	/*
	 * Record reference and modify bits for each page,
	 * and copy PTEs into kernel memory so that they can
	 * be reloaded later.
	 */
	i = NPTESG;
	do {
		tpte = getpte4(va);
		if ((tpte & (PG_V | PG_TYPE)) == (PG_V | PG_OBMEM)) {
			pa = ptoa(tpte & PG_PFNUM);
			if (managed(pa))
				pvhead(pa)->pv_flags |= MR4_4C(tpte);
		}
		*pte++ = tpte & ~(PG_U|PG_M);
		va += NBPG;
	} while (--i > 0);

	/* update segment tables */
	simple_lock(&pm->pm_lock); /* what if other cpu takes mmuentry ?? */
	if (CTX_USABLE(pm,rp))
		setsegmap(VSTOVA(me->me_vreg,me->me_vseg), seginval);
	sp->sg_pmeg = seginval;

	/* off old pmap chain */
	TAILQ_REMOVE(&pm->pm_seglist, me, me_pmchain);
	simple_unlock(&pm->pm_lock);
	setcontext(ctx);	/* done with old context */

	/* onto new pmap chain; new pmap is already locked, if needed */
	TAILQ_INSERT_TAIL(&newpm->pm_seglist, me, me_pmchain);

	/* into new segment table, with backpointers */
	newpm->pm_regmap[newvreg].rg_segmap[newvseg].sg_pmeg = me->me_cookie;
	me->me_pmap = newpm;
	me->me_vseg = newvseg;
	me->me_vreg = newvreg;

	return (me);
}

/*
 * Free an MMU entry.
 *
 * Assumes the corresponding pmap is already locked.
 * Does NOT flush cache, but does record ref and mod bits.
 * The rest of each PTE is discarded.
 * CALLER MUST SET CONTEXT to pm->pm_ctxnum (if pmap has
 * a context) or to 0 (if not).  Caller must also update
 * pm->pm_segmap and (possibly) the hardware.
 */
void
me_free(pm, pmeg)
	register struct pmap *pm;
	register u_int pmeg;
{
	register struct mmuentry *me = &mmusegments[pmeg];
	register int i, va, pa, tpte;
	register int vr;
	register struct regmap *rp;

	vr = me->me_vreg;

#ifdef DEBUG
	if (pmapdebug & PDB_MMU_ALLOC)
		printf("me_free: freeing pmeg %d from pmap %p\n",
		    me->me_cookie, pm);
	if (me->me_cookie != pmeg)
		panic("me_free: wrong mmuentry");
	if (pm != me->me_pmap)
		panic("me_free: pm != me_pmap");
#endif

	rp = &pm->pm_regmap[vr];

	/* just like me_alloc, but no cache flush, and context already set */
	if (CTX_USABLE(pm,rp)) {
		va = VSTOVA(vr,me->me_vseg);
	} else {
#ifdef DEBUG
if (getcontext() != 0) panic("me_free: ctx != 0");
#endif
#ifdef MMU_3L
		if (mmu_3l)
			setregmap(0, tregion);
#endif
		setsegmap(0, me->me_cookie);
		va = 0;
	}
	i = NPTESG;
	do {
		tpte = getpte4(va);
		if ((tpte & (PG_V | PG_TYPE)) == (PG_V | PG_OBMEM)) {
			pa = ptoa(tpte & PG_PFNUM);
			if (managed(pa))
				pvhead(pa)->pv_flags |= MR4_4C(tpte);
		}
		va += NBPG;
	} while (--i > 0);

	/* take mmu entry off pmap chain */
	TAILQ_REMOVE(&pm->pm_seglist, me, me_pmchain);
	/* ... and remove from segment map */
	if (rp->rg_segmap == NULL)
		panic("me_free: no segments in pmap");
	rp->rg_segmap[me->me_vseg].sg_pmeg = seginval;

	/* off LRU or lock chain */
	if (pm == pmap_kernel()) {
		TAILQ_REMOVE(&segm_locked, me, me_list);
	} else {
		TAILQ_REMOVE(&segm_lru, me, me_list);
	}

	/* no associated pmap; on free list */
	me->me_pmap = NULL;
	TAILQ_INSERT_TAIL(&segm_freelist, me, me_list);
}

#ifdef MMU_3L

/* XXX - Merge with segm_alloc/segm_free ? */

struct mmuentry *
region_alloc(mh, newpm, newvr)
	register struct mmuhd *mh;
	register struct pmap *newpm;
	register int newvr;
{
	register struct mmuentry *me;
	register struct pmap *pm;
	int ctx;
	struct regmap *rp;

	/* try free list first */
	if ((me = region_freelist.tqh_first) != NULL) {
		TAILQ_REMOVE(&region_freelist, me, me_list);
#ifdef DEBUG
		if (me->me_pmap != NULL)
			panic("region_alloc: freelist entry has pmap");
		if (pmapdebug & PDB_MMUREG_ALLOC)
			printf("region_alloc: got smeg %x\n", me->me_cookie);
#endif
		TAILQ_INSERT_TAIL(mh, me, me_list);

		/* onto on pmap chain; pmap is already locked, if needed */
		TAILQ_INSERT_TAIL(&newpm->pm_reglist, me, me_pmchain);

		/* into pmap segment table, with backpointers */
		newpm->pm_regmap[newvr].rg_smeg = me->me_cookie;
		me->me_pmap = newpm;
		me->me_vreg = newvr;

		return (me);
	}

	/* no luck, take head of LRU list */
	if ((me = region_lru.tqh_first) == NULL)
		panic("region_alloc: all smegs gone");

	pm = me->me_pmap;
	if (pm == NULL)
		panic("region_alloc: LRU entry has no pmap");
	if (pm == pmap_kernel())
		panic("region_alloc: stealing from kernel");
#ifdef DEBUG
	if (pmapdebug & (PDB_MMUREG_ALLOC | PDB_MMUREG_STEAL))
		printf("region_alloc: stealing smeg %x from pmap %p\n",
		    me->me_cookie, pm);
#endif
	/*
	 * Remove from LRU list, and insert at end of new list
	 * (probably the LRU list again, but so what?).
	 */
	TAILQ_REMOVE(&region_lru, me, me_list);
	TAILQ_INSERT_TAIL(mh, me, me_list);

	rp = &pm->pm_regmap[me->me_vreg];
	ctx = getcontext();
	if (pm->pm_ctx) {
		CHANGE_CONTEXTS(ctx, pm->pm_ctxnum);
		if (vactype != VAC_NONE)
			cache_flush_region(me->me_vreg);
	}

	/* update region tables */
	simple_lock(&pm->pm_lock); /* what if other cpu takes mmuentry ?? */
	if (pm->pm_ctx)
		setregmap(VRTOVA(me->me_vreg), reginval);
	rp->rg_smeg = reginval;

	/* off old pmap chain */
	TAILQ_REMOVE(&pm->pm_reglist, me, me_pmchain);
	simple_unlock(&pm->pm_lock);
	setcontext(ctx);	/* done with old context */

	/* onto new pmap chain; new pmap is already locked, if needed */
	TAILQ_INSERT_TAIL(&newpm->pm_reglist, me, me_pmchain);

	/* into new segment table, with backpointers */
	newpm->pm_regmap[newvr].rg_smeg = me->me_cookie;
	me->me_pmap = newpm;
	me->me_vreg = newvr;

	return (me);
}

/*
 * Free an MMU entry.
 *
 * Assumes the corresponding pmap is already locked.
 * Does NOT flush cache. ???
 * CALLER MUST SET CONTEXT to pm->pm_ctxnum (if pmap has
 * a context) or to 0 (if not).  Caller must also update
 * pm->pm_regmap and (possibly) the hardware.
 */
void
region_free(pm, smeg)
	register struct pmap *pm;
	register u_int smeg;
{
	register struct mmuentry *me = &mmuregions[smeg];

#ifdef DEBUG
	if (pmapdebug & PDB_MMUREG_ALLOC)
		printf("region_free: freeing smeg %x from pmap %p\n",
		    me->me_cookie, pm);
	if (me->me_cookie != smeg)
		panic("region_free: wrong mmuentry");
	if (pm != me->me_pmap)
		panic("region_free: pm != me_pmap");
#endif

	if (pm->pm_ctx)
		if (vactype != VAC_NONE)
			cache_flush_region(me->me_vreg);

	/* take mmu entry off pmap chain */
	TAILQ_REMOVE(&pm->pm_reglist, me, me_pmchain);
	/* ... and remove from segment map */
	pm->pm_regmap[smeg].rg_smeg = reginval;

	/* off LRU or lock chain */
	if (pm == pmap_kernel()) {
		TAILQ_REMOVE(&region_locked, me, me_list);
	} else {
		TAILQ_REMOVE(&region_lru, me, me_list);
	}

	/* no associated pmap; on free list */
	me->me_pmap = NULL;
	TAILQ_INSERT_TAIL(&region_freelist, me, me_list);
}
#endif

/*
 * `Page in' (load or inspect) an MMU entry; called on page faults.
 * Returns 1 if we reloaded the segment, -1 if the segment was
 * already loaded and the page was marked valid (in which case the
 * fault must be a bus error or something), or 0 (segment loaded but
 * PTE not valid, or segment not loaded at all).
 */
int
mmu_pagein(pm, va, prot)
	register struct pmap *pm;
	register vm_offset_t va;
	vm_prot_t prot;
{
	register int *pte;
	register int vr, vs, pmeg, i, s, bits;
	struct regmap *rp;
	struct segmap *sp;

	if (prot != VM_PROT_NONE)
		bits = PG_V | ((prot & VM_PROT_WRITE) ? PG_W : 0);
	else
		bits = 0;

	vr = VA_VREG(va);
	vs = VA_VSEG(va);
	rp = &pm->pm_regmap[vr];
#ifdef DEBUG
if (pm == pmap_kernel())
printf("mmu_pagein: kernel wants map at va %x, vr %d, vs %d\n", va, vr, vs);
#endif

	/* return 0 if we have no PMEGs to load */
	if (rp->rg_segmap == NULL)
		return (0);
#ifdef MMU_3L
	if (mmu_3l && rp->rg_smeg == reginval) {
		smeg_t smeg;
		unsigned int tva = VA_ROUNDDOWNTOREG(va);
		struct segmap *sp = rp->rg_segmap;

		s = splpmap();		/* paranoid */
		smeg = region_alloc(&region_lru, pm, vr)->me_cookie;
		setregmap(tva, smeg);
		i = NSEGRG;
		do {
			setsegmap(tva, sp++->sg_pmeg);
			tva += NBPSG;
		} while (--i > 0);
		splx(s);
	}
#endif
	sp = &rp->rg_segmap[vs];

	/* return 0 if we have no PTEs to load */
	if ((pte = sp->sg_pte) == NULL)
		return (0);

	/* return -1 if the fault is `hard', 0 if not */
	if (sp->sg_pmeg != seginval)
		return (bits && (getpte4(va) & bits) == bits ? -1 : 0);

	/* reload segment: write PTEs into a new LRU entry */
	va = VA_ROUNDDOWNTOSEG(va);
	s = splpmap();		/* paranoid */
	pmeg = me_alloc(&segm_lru, pm, vr, vs)->me_cookie;
	setsegmap(va, pmeg);
	i = NPTESG;
	do {
		setpte4(va, *pte++);
		va += NBPG;
	} while (--i > 0);
	splx(s);
	return (1);
}
#endif /* defined SUN4 or SUN4C */

#if defined(DEBUG) && defined(SUN4M)
/*
 * `Page in' (load or inspect) an MMU entry; called on page faults.
 * Returns -1 if the desired page was marked valid (in which case the
 * fault must be a bus error or something), or 0 (segment loaded but
 * PTE not valid, or segment not loaded at all).
 *
 * The SRMMU does not have the concept of `loading PMEGs into the MMU'.
 * For now, we use it to debug certain sporadic and strange memory
 * fault traps.
 */
int
mmu_pagein4m(pm, va, prot)
	register struct pmap *pm;
	register vm_offset_t va;
	register int prot;
{
	register int pte;
	register int bits;

	pte = getpte4m(va);
	if ((pte & SRMMU_TETYPE) != SRMMU_TEPTE)
		/* pte not found == not valid */
		return 0;

	/*
	 * If pte has "correct" protection (according to bits), then
	 * something bizarre has happened, so we return -1 to generate
	 * a fault. Otherwise, we return 0 and let the VM code reload
	 * the page.
	 * XXX: Does this actually work, or will it cause a loop?
	 */

	if (prot != VM_PROT_NONE)
		bits = ((prot & VM_PROT_WRITE) ? PPROT_RWX_RWX : PPROT_RX_RX);
	else
		bits = 0;
	/* If we're in kernelland, mask out user RWX */
/*	if (getcontext() == 0)
		bits |= PPROT_S;
*/
	if (bits && (pte & bits) == bits) {
		printf("pagein4m(%s[%d]): OOPS: prot=%x, va=%x, pte=%x, bits=%x\n",
			curproc->p_comm, curproc->p_pid, prot, va, pte, bits);
		return -1;
	}
	return 0;
}
#endif

/*
 * Allocate a context.  If necessary, steal one from someone else.
 * Changes hardware context number and loads segment map.
 *
 * This routine is only ever called from locore.s just after it has
 * saved away the previous process, so there are no active user windows.
 */
void
ctx_alloc(pm)
	register struct pmap *pm;
{
	register union ctxinfo *c;
	register int s, cnum, i, doflush;
	register struct regmap *rp;
	register int gap_start, gap_end;
	register unsigned long va;

/*XXX-GCC!*/gap_start=gap_end=0;
#ifdef DEBUG
	if (pm->pm_ctx)
		panic("ctx_alloc pm_ctx");
	if (pmapdebug & PDB_CTX_ALLOC)
		printf("ctx_alloc(%p)\n", pm);
#endif
	if (CPU_ISSUN4OR4C) {
		gap_start = pm->pm_gap_start;
		gap_end = pm->pm_gap_end;
	}

	s = splpmap();
	if ((c = ctx_freelist) != NULL) {
		ctx_freelist = c->c_nextfree;
		cnum = c - ctxinfo;
		doflush = 0;
	} else {
		if ((ctx_kick += ctx_kickdir) >= ncontext) {
			ctx_kick = ncontext - 1;
			ctx_kickdir = -1;
		} else if (ctx_kick < 1) {
			ctx_kick = 1;
			ctx_kickdir = 1;
		}
		c = &ctxinfo[cnum = ctx_kick];
#ifdef DEBUG
		if (c->c_pmap == NULL)
			panic("ctx_alloc cu_pmap");
		if (pmapdebug & (PDB_CTX_ALLOC | PDB_CTX_STEAL))
			printf("ctx_alloc: steal context %d from %p\n",
			    cnum, c->c_pmap);
#endif
		c->c_pmap->pm_ctx = NULL;
		doflush = (vactype != VAC_NONE);
		if (CPU_ISSUN4OR4C) {
			if (gap_start < c->c_pmap->pm_gap_start)
				gap_start = c->c_pmap->pm_gap_start;
			if (gap_end > c->c_pmap->pm_gap_end)
				gap_end = c->c_pmap->pm_gap_end;
		}
	}

	c->c_pmap = pm;
	pm->pm_ctx = c;
	pm->pm_ctxnum = cnum;

	if (CPU_ISSUN4OR4C) {
		/*
		 * Write pmap's region (3-level MMU) or segment table into
		 * the MMU.
		 *
		 * Only write those entries that actually map something in
		 * this context by maintaining a pair of region numbers in
		 * between which the pmap has no valid mappings.
		 *
		 * If a context was just allocated from the free list, trust
		 * that all its pmeg numbers are `seginval'. We make sure this
		 * is the case initially in pmap_bootstrap(). Otherwise, the
		 * context was freed by calling ctx_free() in pmap_release(),
		 * which in turn is supposedly called only when all mappings
		 * have been removed.
		 *
		 * On the other hand, if the context had to be stolen from
		 * another pmap, we possibly shrink the gap to be the
		 * disjuction of the new and the previous map.
		 */

		setcontext(cnum);
		splx(s);
		if (doflush)
			cache_flush_context();

		rp = pm->pm_regmap;
		for (va = 0, i = NUREG; --i >= 0; ) {
			if (VA_VREG(va) >= gap_start) {
				va = VRTOVA(gap_end);
				i -= gap_end - gap_start;
				rp += gap_end - gap_start;
				if (i < 0)
					break;
				/* mustn't re-enter this branch */
				gap_start = NUREG;
			}
#ifdef MMU_3L
			if (mmu_3l) {
				setregmap(va, rp++->rg_smeg);
				va += NBPRG;
			} else
#endif
			{
				register int j;
				register struct segmap *sp = rp->rg_segmap;
				for (j = NSEGRG; --j >= 0; va += NBPSG)
					setsegmap(va,
						  sp?sp++->sg_pmeg:seginval);
				rp++;
			}
		}

	} else if (CPU_ISSUN4M) {

		/*
		 * Reload page and context tables to activate the page tables
		 * for this context.
		 *
		 * The gap stuff isn't really needed in the Sun4m architecture,
		 * since we don't have to worry about excessive mappings (all
		 * mappings exist since the page tables must be complete for
		 * the mmu to be happy).
		 *
		 * If a context was just allocated from the free list, trust
		 * that all of its mmu-edible page tables are zeroed out
		 * (except for those associated with the kernel). We make
		 * sure this is the case initially in pmap_bootstrap() and
		 * pmap_init() (?).
		 * Otherwise, the context was freed by calling ctx_free() in
		 * pmap_release(), which in turn is supposedly called only
		 * when all mappings have been removed.
		 *
		 * XXX: Do we have to flush cache after reloading ctx tbl?
		 */

		/*
		 * We install kernel mappings into the pmap here, since when
		 * the kernel expands it only propagates the expansion to pmaps
		 * corresponding to valid contexts. Thus it is possible (and
		 * it has happened!) that a pmap is created just before
		 * the kernel expands, but the pmap gets a context *after*
		 * the kernel expands, thus missing getting mappings.
		 */
		qcopy(&pmap_kernel()->pm_reg_ptps[VA_VREG(KERNBASE)],
		      &pm->pm_reg_ptps[VA_VREG(KERNBASE)],
		      NKREG * sizeof(int));
		/*
		 * We must also install the regmap/segmap/etc stuff for
		 * kernel maps.
		 */
		qcopy(&pmap_kernel()->pm_regmap[VA_VREG(KERNBASE)],
		       &pm->pm_regmap[VA_VREG(KERNBASE)],
		       NKREG * sizeof(struct regmap));

		ctxbusyvector[cnum] = 1; /* mark context as busy */
#ifdef DEBUG
		if (pm->pm_reg_ptps_pa == 0)
			panic("ctx_alloc: no region table in current pmap");
#endif
		/*setcontext(0); * paranoia? can we modify curr. ctx? */
		ctx_phys_tbl[cnum] =
			(pm->pm_reg_ptps_pa >> SRMMU_PPNPASHIFT) | SRMMU_TEPTD;

		setcontext(cnum);
		if (doflush)
			cache_flush_context();
#if defined(SUN4M)
		tlb_flush_context(); /* remove any remnant garbage from tlb */
#endif
		splx(s);
	}
}

/*
 * Give away a context.  Flushes cache and sets current context to 0.
 */
void
ctx_free(pm)
	struct pmap *pm;
{
	register union ctxinfo *c;
	register int newc, oldc;

	if ((c = pm->pm_ctx) == NULL)
		panic("ctx_free");
	pm->pm_ctx = NULL;
	oldc = getcontext();

	if (vactype != VAC_NONE) {
		newc = pm->pm_ctxnum;
		CHANGE_CONTEXTS(oldc, newc);
		cache_flush_context();
#if defined(SUN4M)
		if (CPU_ISSUN4M)
			tlb_flush_context();
#endif
		setcontext(0);
	} else {
#if defined(SUN4M)
		if (CPU_ISSUN4M)
			tlb_flush_context();
#endif
		CHANGE_CONTEXTS(oldc, 0);
	}
	c->c_nextfree = ctx_freelist;
	ctx_freelist = c;

#if defined(SUN4M)
	if (CPU_ISSUN4M) {
		/* Map kernel back into unused context */
		newc = pm->pm_ctxnum;
		ctx_phys_tbl[newc] = ctx_phys_tbl[0];
		if (newc)
			ctxbusyvector[newc] = 0; /* mark as free */
	}
#endif
}


/*----------------------------------------------------------------*/

/*
 * pvlist functions.
 */

/*
 * Walk the given pv list, and for each PTE, set or clear some bits
 * (e.g., PG_W or PG_NC).
 *
 * As a special case, this never clears PG_W on `pager' pages.
 * These, being kernel addresses, are always in hardware and have
 * a context.
 *
 * This routine flushes the cache for any page whose PTE changes,
 * as long as the process has a context; this is overly conservative.
 * It also copies ref and mod bits to the pvlist, on the theory that
 * this might save work later.  (XXX should test this theory)
 */

#if defined(SUN4) || defined(SUN4C)

void
pv_changepte4_4c(pv0, bis, bic)
	register struct pvlist *pv0;
	register int bis, bic;
{
	register int *pte;
	register struct pvlist *pv;
	register struct pmap *pm;
	register vm_offset_t va;
	register int vr, vs, flags;
	int ctx, s;
	struct regmap *rp;
	struct segmap *sp;

	write_user_windows();		/* paranoid? */

	s = splpmap();			/* paranoid? */
	if (pv0->pv_pmap == NULL) {
		splx(s);
		return;
	}
	ctx = getcontext();
	flags = pv0->pv_flags;
	for (pv = pv0; pv != NULL; pv = pv->pv_next) {
		pm = pv->pv_pmap;
if(pm==NULL)panic("pv_changepte 1");
		va = pv->pv_va;
		vr = VA_VREG(va);
		vs = VA_VSEG(va);
		rp = &pm->pm_regmap[vr];
		if (rp->rg_segmap == NULL)
			panic("pv_changepte: no segments");

		sp = &rp->rg_segmap[vs];
		pte = sp->sg_pte;

		if (sp->sg_pmeg == seginval) {
			/* not in hardware: just fix software copy */
			if (pte == NULL)
				panic("pv_changepte 2");
			pte += VA_VPG(va);
			*pte = (*pte | bis) & ~bic;
		} else {
			register int tpte;

			/* in hardware: fix hardware copy */
			if (CTX_USABLE(pm,rp)) {
				extern vm_offset_t pager_sva, pager_eva;

				/*
				 * Bizarreness:  we never clear PG_W on
				 * pager pages, nor PG_NC on DVMA pages.
				 */
				if (bic == PG_W &&
				    va >= pager_sva && va < pager_eva)
					continue;
				if (bic == PG_NC &&
				    va >= DVMA_BASE && va < DVMA_END)
					continue;
				setcontext(pm->pm_ctxnum);
				/* XXX should flush only when necessary */
				tpte = getpte4(va);
				if (vactype != VAC_NONE && (tpte & PG_M))
					cache_flush_page(va);
			} else {
				/* XXX per-cpu va? */
				setcontext(0);
#ifdef MMU_3L
				if (mmu_3l)
					setregmap(0, tregion);
#endif
				setsegmap(0, sp->sg_pmeg);
				va = VA_VPG(va) << PGSHIFT;
				tpte = getpte4(va);
			}
			if (tpte & PG_V)
				flags |= (tpte >> PG_M_SHIFT) & (PV_MOD|PV_REF);
			tpte = (tpte | bis) & ~bic;
			setpte4(va, tpte);
			if (pte != NULL)	/* update software copy */
				pte[VA_VPG(va)] = tpte;
		}
	}
	pv0->pv_flags = flags;
	setcontext(ctx);
	splx(s);
}

/*
 * Sync ref and mod bits in pvlist (turns off same in hardware PTEs).
 * Returns the new flags.
 *
 * This is just like pv_changepte, but we never add or remove bits,
 * hence never need to adjust software copies.
 */
int
pv_syncflags4_4c(pv0)
	register struct pvlist *pv0;
{
	register struct pvlist *pv;
	register struct pmap *pm;
	register int tpte, va, vr, vs, pmeg, flags;
	int ctx, s;
	struct regmap *rp;
	struct segmap *sp;

	write_user_windows();		/* paranoid? */

	s = splpmap();			/* paranoid? */
	if (pv0->pv_pmap == NULL) {	/* paranoid */
		splx(s);
		return (0);
	}
	ctx = getcontext();
	flags = pv0->pv_flags;
	for (pv = pv0; pv != NULL; pv = pv->pv_next) {
		pm = pv->pv_pmap;
		va = pv->pv_va;
		vr = VA_VREG(va);
		vs = VA_VSEG(va);
		rp = &pm->pm_regmap[vr];
		if (rp->rg_segmap == NULL)
			panic("pv_syncflags: no segments");
		sp = &rp->rg_segmap[vs];

		if ((pmeg = sp->sg_pmeg) == seginval)
			continue;

		if (CTX_USABLE(pm,rp)) {
			setcontext(pm->pm_ctxnum);
			/* XXX should flush only when necessary */
			tpte = getpte4(va);
			if (vactype != VAC_NONE && (tpte & PG_M))
				cache_flush_page(va);
		} else {
			/* XXX per-cpu va? */
			setcontext(0);
#ifdef MMU_3L
			if (mmu_3l)
				setregmap(0, tregion);
#endif
			setsegmap(0, pmeg);
			va = VA_VPG(va) << PGSHIFT;
			tpte = getpte4(va);
		}
		if (tpte & (PG_M|PG_U) && tpte & PG_V) {
			flags |= (tpte >> PG_M_SHIFT) &
			    (PV_MOD|PV_REF);
			tpte &= ~(PG_M|PG_U);
			setpte4(va, tpte);
		}
	}
	pv0->pv_flags = flags;
	setcontext(ctx);
	splx(s);
	return (flags);
}

/*
 * pv_unlink is a helper function for pmap_remove.
 * It takes a pointer to the pv_table head for some physical address
 * and removes the appropriate (pmap, va) entry.
 *
 * Once the entry is removed, if the pv_table head has the cache
 * inhibit bit set, see if we can turn that off; if so, walk the
 * pvlist and turn off PG_NC in each PTE.  (The pvlist is by
 * definition nonempty, since it must have at least two elements
 * in it to have PV_NC set, and we only remove one here.)
 */
/*static*/ void
pv_unlink4_4c(pv, pm, va)
	register struct pvlist *pv;
	register struct pmap *pm;
	register vm_offset_t va;
{
	register struct pvlist *npv;

#ifdef DIAGNOSTIC
	if (pv == NULL)
		panic("pv_unlink: pv==NULL");
	if (pv->pv_pmap == NULL)
		panic("pv_unlink: pv->pv_pmap==NULL");
#endif
	/*
	 * First entry is special (sigh).
	 */
	npv = pv->pv_next;
	if (pv->pv_pmap == pm && pv->pv_va == va) {
		pmap_stats.ps_unlink_pvfirst++;
		if (npv != NULL) {
			pv->pv_next = npv->pv_next;
			pv->pv_pmap = npv->pv_pmap;
			pv->pv_va = npv->pv_va;
			free(npv, M_VMPVENT);
		} else {
			pv->pv_pmap = NULL;
			pv->pv_va == NULL;
			return;
		}
	} else {
		register struct pvlist *prev;

		for (prev = pv;; prev = npv, npv = npv->pv_next) {
			pmap_stats.ps_unlink_pvsearch++;
			if (npv == NULL)
				panic("pv_unlink: npv==NULL");
			if (npv->pv_pmap == pm && npv->pv_va == va)
				break;
		}
		prev->pv_next = npv->pv_next;
		free(npv, M_VMPVENT);
	}
	if (pv->pv_flags & PV_NC) {
		/*
		 * Not cached: check to see if we can fix that now.
		 */
		va = pv->pv_va;
		for (npv = pv->pv_next; npv != NULL; npv = npv->pv_next)
			if (BADALIAS(va, npv->pv_va))
				return;
		pv->pv_flags &= ~PV_NC;
		pv_changepte4_4c(pv, 0, PG_NC);
	}
}

/*
 * pv_link is the inverse of pv_unlink, and is used in pmap_enter.
 * It returns PG_NC if the (new) pvlist says that the address cannot
 * be cached.
 */
/*static*/ int
pv_link4_4c(pv, pm, va)
	register struct pvlist *pv;
	register struct pmap *pm;
	register vm_offset_t va;
{
	register struct pvlist *npv;
	register int ret;

	if (pv->pv_pmap == NULL) {
		/* no pvlist entries yet */
		pmap_stats.ps_enter_firstpv++;
		pv->pv_next = NULL;
		pv->pv_pmap = pm;
		pv->pv_va = va;
		return (0);
	}
	/*
	 * Before entering the new mapping, see if
	 * it will cause old mappings to become aliased
	 * and thus need to be `discached'.
	 */
	ret = 0;
	pmap_stats.ps_enter_secondpv++;
	if (pv->pv_flags & PV_NC) {
		/* already uncached, just stay that way */
		ret = PG_NC;
	} else {
		/* MAY NEED TO DISCACHE ANYWAY IF va IS IN DVMA SPACE? */
		for (npv = pv; npv != NULL; npv = npv->pv_next) {
			if (BADALIAS(va, npv->pv_va)) {
#ifdef DEBUG
				if (pmapdebug) printf(
				"pv_link: badalias: pid %d, %lx<=>%x, pa %lx\n",
				curproc?curproc->p_pid:-1, va, npv->pv_va,
				vm_first_phys + (pv-pv_table)*NBPG);
#endif
				pv->pv_flags |= PV_NC;
				pv_changepte4_4c(pv, ret = PG_NC, 0);
				break;
			}
		}
	}
	npv = (struct pvlist *)malloc(sizeof *npv, M_VMPVENT, M_WAITOK);
	npv->pv_next = pv->pv_next;
	npv->pv_pmap = pm;
	npv->pv_va = va;
	pv->pv_next = npv;
	return (ret);
}

#endif /* sun4, sun4c code */

#if defined(SUN4M)		/* Sun4M versions of above */
/*
 * Walk the given pv list, and for each PTE, set or clear some bits
 * (e.g., PG_W or PG_NC).
 *
 * As a special case, this never clears PG_W on `pager' pages.
 * These, being kernel addresses, are always in hardware and have
 * a context.
 *
 * This routine flushes the cache for any page whose PTE changes,
 * as long as the process has a context; this is overly conservative.
 * It also copies ref and mod bits to the pvlist, on the theory that
 * this might save work later.  (XXX should test this theory)
 */
void
pv_changepte4m(pv0, bis, bic)
	register struct pvlist *pv0;
	register int bis, bic;
{
	register struct pvlist *pv;
	register struct pmap *pm;
	register vm_offset_t va;
	register int vr, flags;
	int ctx, s;
	struct regmap *rp;

	write_user_windows();		/* paranoid? */

	s = splpmap();			/* paranoid? */
	if (pv0->pv_pmap == NULL) {
		splx(s);
		return;
	}
	ctx = getcontext();
	flags = pv0->pv_flags;
	for (pv = pv0; pv != NULL; pv = pv->pv_next) {
		register int tpte;
		pm = pv->pv_pmap;
		if (pm == NULL)
			panic("pv_changepte 1");
		va = pv->pv_va;
		vr = VA_VREG(va);
		rp = &pm->pm_regmap[vr];
		if (rp->rg_segmap == NULL)
			panic("pv_changepte: no segments");

		if (CTX_USABLE(pm,rp)) {
			extern vm_offset_t pager_sva, pager_eva;

			/*
			 * Bizarreness:  we never clear PG_W on
			 * pager pages, nor set PG_C on DVMA pages.
			 */
			if ((bic & PPROT_WRITE) &&
			    va >= pager_sva && va < pager_eva)
				continue;
			if ((bis & SRMMU_PG_C) &&
			    va >= DVMA_BASE && va < DVMA_END)
				continue;
			setcontext(pm->pm_ctxnum);
			/* %%%: Do we need to always flush? */
			tpte = getpte4m(va);
			if (vactype != VAC_NONE && (tpte & SRMMU_PG_M))
				cache_flush_page(va);
		} else {
			/* PTE that we want has no context. Use sw getpte*/
			tpte = getptesw4m(pm, va);
		}
		if ((tpte & SRMMU_TETYPE) == SRMMU_TEPTE) /* i.e. if valid pte */
			flags |= (tpte >> PG_M_SHIFT4M) & (PV_MOD4M|PV_REF4M|PV_C4M);

		tpte = (tpte | bis) & ~bic;

		if (CTX_USABLE(pm,rp))
			setpte4m(va, tpte);
		else
			setptesw4m(pm, va, tpte);
	}
	pv0->pv_flags = flags;
	setcontext(ctx);
	splx(s);
}

/*
 * Sync ref and mod bits in pvlist. If page has been ref'd or modified,
 * update ref/mod bits in pvlist, and clear the hardware bits.
 *
 * Return the new flags.
 */
int
pv_syncflags4m(pv0)
	register struct pvlist *pv0;
{
	register struct pvlist *pv;
	register struct pmap *pm;
	register int tpte, va, vr, vs, flags;
	int ctx, s;
	struct regmap *rp;
	struct segmap *sp;

	write_user_windows();		/* paranoid? */

	s = splpmap();			/* paranoid? */
	if (pv0->pv_pmap == NULL) {	/* paranoid */
		splx(s);
		return (0);
	}
	ctx = getcontext();
	flags = pv0->pv_flags;
	for (pv = pv0; pv != NULL; pv = pv->pv_next) {
		pm = pv->pv_pmap;
		va = pv->pv_va;
		vr = VA_VREG(va);
		vs = VA_VSEG(va);
		rp = &pm->pm_regmap[vr];
		if (rp->rg_segmap == NULL)
			panic("pv_syncflags: no segments");
		sp = &rp->rg_segmap[vs];

		if (sp->sg_pte == NULL)	/* invalid */
			continue;

		/*
		 * We need the PTE from memory as the TLB version will
		 * always have the SRMMU_PG_R bit on.
		 */
		if (CTX_USABLE(pm,rp)) {
			setcontext(pm->pm_ctxnum);
			tlb_flush_page(va);
		}
		tpte = getptesw4m(pm, va);

		if ((tpte & SRMMU_TETYPE) == SRMMU_TEPTE && /* if valid pte */
		    (tpte & (SRMMU_PG_M|SRMMU_PG_R))) {	  /* and mod/refd */
			flags |= (tpte >> PG_M_SHIFT4M) &
				 (PV_MOD4M|PV_REF4M|PV_C4M);
			tpte &= ~(SRMMU_PG_M | SRMMU_PG_R);
			/* TLB has been invalidated, so just update memory */
			setptesw4m(pm, va, tpte);
			if (vactype != VAC_NONE &&
			    CTX_USABLE(pm,rp) && (tpte & SRMMU_PG_M))
				cache_flush_page(va); /* XXX: do we need this?*/
		}
	}
	pv0->pv_flags = flags;
	setcontext(ctx);
	splx(s);
	return (flags);
}

void
pv_unlink4m(pv, pm, va)
	register struct pvlist *pv;
	register struct pmap *pm;
	register vm_offset_t va;
{
	register struct pvlist *npv;

#ifdef DIAGNOSTIC
	if (pv == NULL)
		panic("pv_unlink: pv==NULL");
	if (pv->pv_pmap == NULL)
		panic("pv_unlink: pv->pv_pmap==NULL");
#endif
	/*
	 * First entry is special (sigh).
	 */
	npv = pv->pv_next;
	if (pv->pv_pmap == pm && pv->pv_va == va) {
		pmap_stats.ps_unlink_pvfirst++;
		if (npv != NULL) {
			pv->pv_next = npv->pv_next;
			pv->pv_pmap = npv->pv_pmap;
			pv->pv_va = npv->pv_va;
			free(npv, M_VMPVENT);
		} else {
			pv->pv_pmap = NULL;
			pv->pv_va == NULL;
			return;
		}
	} else {
		register struct pvlist *prev;

		for (prev = pv;; prev = npv, npv = npv->pv_next) {
			pmap_stats.ps_unlink_pvsearch++;
			if (npv == NULL)
				panic("pv_unlink: npv==NULL");
			if (npv->pv_pmap == pm && npv->pv_va == va)
				break;
		}
		prev->pv_next = npv->pv_next;
		free(npv, M_VMPVENT);
	}
	if (!(pv->pv_flags & PV_C4M)) {
		/*
		 * Not cached: check to see if we can fix that now.
		 */
		va = pv->pv_va;
		for (npv = pv->pv_next; npv != NULL; npv = npv->pv_next)
			if (BADALIAS(va, npv->pv_va))
				return;
		pv->pv_flags |= PV_C4M;
		pv_changepte4m(pv, SRMMU_PG_C, 0);
	}
}

/*
 * pv_link is the inverse of pv_unlink, and is used in pmap_enter.
 * It returns SRMMU_PG_C if the (new) pvlist says that the address cannot
 * be cached (i.e. its results must be (& ~)'d in.
 */
/*static*/ int
pv_link4m(pv, pm, va)
	register struct pvlist *pv;
	register struct pmap *pm;
	register vm_offset_t va;
{
	register struct pvlist *npv;
	register int ret;

	if (pv->pv_pmap == NULL) {
		/* no pvlist entries yet */
		pmap_stats.ps_enter_firstpv++;
		pv->pv_next = NULL;
		pv->pv_pmap = pm;
		pv->pv_va = va;
		pv->pv_flags |= PV_C4M;
		return (0);
	}
	/*
	 * Before entering the new mapping, see if
	 * it will cause old mappings to become aliased
	 * and thus need to be `discached'.
	 */
	ret = 0;
	pmap_stats.ps_enter_secondpv++;
	if (!(pv->pv_flags & PV_C4M)) {
		/* already uncached, just stay that way */
		ret = SRMMU_PG_C;
	} else {
		for (npv = pv; npv != NULL; npv = npv->pv_next) {
			if (BADALIAS(va, npv->pv_va)) {
#ifdef DEBUG
				if (pmapdebug & PDB_CACHESTUFF) printf(
				"pv_link: badalias: pid %d, %lx<=>%x, pa %lx\n",
				curproc?curproc->p_pid:-1, va, npv->pv_va,
				vm_first_phys + (pv-pv_table)*NBPG);
#endif
				pv->pv_flags &= ~PV_C4M;
				pv_changepte4m(pv, 0, ret = SRMMU_PG_C);
				/* cache_flush_page(va); XXX: needed? */
				break;
			}
		}
	}
	npv = (struct pvlist *)malloc(sizeof *npv, M_VMPVENT, M_WAITOK);
	npv->pv_next = pv->pv_next;
	npv->pv_pmap = pm;
	npv->pv_va = va;
	npv->pv_flags |= (ret == SRMMU_PG_C ? 0 : PV_C4M);
	pv->pv_next = npv;
	return (ret);
}
#endif

/*
 * Walk the given list and flush the cache for each (MI) page that is
 * potentially in the cache. Called only if vactype != VAC_NONE.
 */
void
pv_flushcache(pv)
	register struct pvlist *pv;
{
	register struct pmap *pm;
	register int s, ctx;

	write_user_windows();	/* paranoia? */

	s = splpmap();		/* XXX extreme paranoia */
	if ((pm = pv->pv_pmap) != NULL) {
		ctx = getcontext();
		for (;;) {
			if (pm->pm_ctx) {
				setcontext(pm->pm_ctxnum);
				cache_flush_page(pv->pv_va);
			}
			pv = pv->pv_next;
			if (pv == NULL)
				break;
			pm = pv->pv_pmap;
		}
		setcontext(ctx);
	}
	splx(s);
}

/*----------------------------------------------------------------*/

/*
 * At last, pmap code.
 */

#if defined(SUN4) && defined(SUN4C)
int nptesg;
#endif

#if defined(SUN4M)
static void pmap_bootstrap4m __P((void));
#endif
#if defined(SUN4) || defined(SUN4C)
static void pmap_bootstrap4_4c __P((int, int, int));
#endif

/*
 * Bootstrap the system enough to run with VM enabled.
 *
 * nsegment is the number of mmu segment entries (``PMEGs'');
 * nregion is the number of mmu region entries (``SMEGs'');
 * nctx is the number of contexts.
 */
void
pmap_bootstrap(nctx, nregion, nsegment)
	int nsegment, nctx, nregion;
{

	cnt.v_page_size = NBPG;
	vm_set_page_size();

#if defined(SUN4) && (defined(SUN4C) || defined(SUN4M))
	/* In this case NPTESG is not a #define */
	nptesg = (NBPSG >> pgshift);
#endif

	ncontext = nctx;

#if defined(SUN4M)
	if (CPU_ISSUN4M) {
		pmap_bootstrap4m();
		return;
	}
#endif
#if defined(SUN4) || defined(SUN4C)
	if (CPU_ISSUN4OR4C) {
		pmap_bootstrap4_4c(nctx, nregion, nsegment);
		return;
	}
#endif
}

#if defined(SUN4) || defined(SUN4C)
void
pmap_bootstrap4_4c(nctx, nregion, nsegment)
	int nsegment, nctx, nregion;
{
	register union ctxinfo *ci;
	register struct mmuentry *mmuseg;
#ifdef MMU_3L
	register struct mmuentry *mmureg;
#endif
	struct   regmap *rp;
	register int i, j;
	register int npte, zseg, vr, vs;
	register int rcookie, scookie;
	register caddr_t p;
	register struct memarr *mp;
	register void (*rom_setmap)(int ctx, caddr_t va, int pmeg);
	int lastpage;
	extern char end[];
#ifdef DDB
	extern char *esym;
	char *theend = end;
#endif

	switch (cputyp) {
	case CPU_SUN4C:
		mmu_has_hole = 1;
		break;
	case CPU_SUN4:
		if (cpumod != SUN4_400) {
			mmu_has_hole = 1;
			break;
		}
	}

	cnt.v_page_size = NBPG;
	vm_set_page_size();

#if defined(SUN4)
	/*
	 * set up the segfixmask to mask off invalid bits
	 */
	segfixmask =  nsegment - 1; /* assume nsegment is a power of 2 */
#ifdef DIAGNOSTIC
	if (((nsegment & segfixmask) | (nsegment & ~segfixmask)) != nsegment) {
		printf("pmap_bootstrap: unsuitable number of segments (%d)\n",
			nsegment);
		callrom();
	}
#endif
#endif

#if defined(SUN4M) /* We're in a dual-arch kernel. Setup 4/4c fn. ptrs */
	pmap_clear_modify_p 	=	pmap_clear_modify4_4c;
	pmap_clear_reference_p 	= 	pmap_clear_reference4_4c;
	pmap_copy_page_p 	=	pmap_copy_page4_4c;
	pmap_enter_p 		=	pmap_enter4_4c;
	pmap_extract_p 		=	pmap_extract4_4c;
	pmap_is_modified_p 	=	pmap_is_modified4_4c;
	pmap_is_referenced_p	=	pmap_is_referenced4_4c;
	pmap_page_protect_p	=	pmap_page_protect4_4c;
	pmap_protect_p		=	pmap_protect4_4c;
	pmap_zero_page_p	=	pmap_zero_page4_4c;
	pmap_changeprot_p	=	pmap_changeprot4_4c;
	pmap_rmk_p		=	pmap_rmk4_4c;
	pmap_rmu_p		=	pmap_rmu4_4c;
#endif /* defined SUN4M */

	/*
	 * Last segment is the `invalid' one (one PMEG of pte's with !pg_v).
	 * It will never be used for anything else.
	 */
	seginval = --nsegment;

#ifdef MMU_3L
	if (mmu_3l)
		reginval = --nregion;
#endif

	/*
	 * Intialize the kernel pmap.
	 */
	/* kernel_pmap_store.pm_ctxnum = 0; */
	simple_lock_init(kernel_pmap_store.pm_lock);
	kernel_pmap_store.pm_refcount = 1;
#ifdef MMU_3L
	TAILQ_INIT(&kernel_pmap_store.pm_reglist);
#endif
	TAILQ_INIT(&kernel_pmap_store.pm_seglist);

	kernel_pmap_store.pm_regmap = &kernel_regmap_store[-NUREG];
	for (i = NKREG; --i >= 0;) {
#ifdef MMU_3L
		kernel_regmap_store[i].rg_smeg = reginval;
#endif
		kernel_regmap_store[i].rg_segmap =
			&kernel_segmap_store[i * NSEGRG];
		for (j = NSEGRG; --j >= 0;)
			kernel_segmap_store[i * NSEGRG + j].sg_pmeg = seginval;
	}

	/*
	 * Preserve the monitor ROM's reserved VM region, so that
	 * we can use L1-A or the monitor's debugger.  As a side
	 * effect we map the ROM's reserved VM into all contexts
	 * (otherwise L1-A crashes the machine!).
	 */

	mmu_reservemon4_4c(&nregion, &nsegment);

#ifdef MMU_3L
	/* Reserve one region for temporary mappings */
	tregion = --nregion;
#endif

	/*
	 * Allocate and clear mmu entries and context structures.
	 */
	p = end;
#ifdef DDB
	if (esym != 0)
		theend = p = esym;
#endif
#ifdef MMU_3L
	mmuregions = mmureg = (struct mmuentry *)p;
	p += nregion * sizeof(struct mmuentry);
#endif
	mmusegments = mmuseg = (struct mmuentry *)p;
	p += nsegment * sizeof(struct mmuentry);
	pmap_kernel()->pm_ctx = ctxinfo = ci = (union ctxinfo *)p;
	p += nctx * sizeof *ci;
#ifdef DDB
	bzero(theend, p - theend);
#else
	bzero(end, p - end);
#endif

	/* Initialize MMU resource queues */
#ifdef MMU_3L
	TAILQ_INIT(&region_freelist);
	TAILQ_INIT(&region_lru);
	TAILQ_INIT(&region_locked);
#endif
	TAILQ_INIT(&segm_freelist);
	TAILQ_INIT(&segm_lru);
	TAILQ_INIT(&segm_locked);

	/*
	 * Set up the `constants' for the call to vm_init()
	 * in main().  All pages beginning at p (rounded up to
	 * the next whole page) and continuing through the number
	 * of available pages are free, but they start at a higher
	 * virtual address.  This gives us two mappable MD pages
	 * for pmap_zero_page and pmap_copy_page, and one MI page
	 * for /dev/mem, all with no associated physical memory.
	 */
	p = (caddr_t)(((u_int)p + NBPG - 1) & ~PGOFSET);
	avail_start = (int)p - KERNBASE;

	/*
	 * Grab physical memory list, so pmap_next_page() can do its bit.
	 */
	npmemarr = makememarr(pmemarr, MA_SIZE, MEMARR_AVAILPHYS);
	sortm(pmemarr, npmemarr);
	if (pmemarr[0].addr != 0) {
		printf("pmap_bootstrap: no kernel memory?!\n");
		callrom();
	}
	avail_end = pmemarr[npmemarr-1].addr + pmemarr[npmemarr-1].len;
	avail_next = avail_start;
	for (physmem = 0, mp = pmemarr, j = npmemarr; --j >= 0; mp++)
		physmem += btoc(mp->len);

	i = (int)p;
	vpage[0] = p, p += NBPG;
	vpage[1] = p, p += NBPG;
	vmmap = p, p += NBPG;
	p = reserve_dumppages(p);

	/*
	 * Allocate virtual memory for pv_table[], which will be mapped
	 * sparsely in pmap_init().
	 */
	pv_table = (struct pvlist *)p;
	p += round_page(sizeof(struct pvlist) * atop(avail_end - avail_start));

	virtual_avail = (vm_offset_t)p;
	virtual_end = VM_MAX_KERNEL_ADDRESS;

	p = (caddr_t)i;			/* retract to first free phys */

	/*
	 * All contexts are free except the kernel's.
	 *
	 * XXX sun4c could use context 0 for users?
	 */
	ci->c_pmap = pmap_kernel();
	ctx_freelist = ci + 1;
	for (i = 1; i < ncontext; i++) {
		ci++;
		ci->c_nextfree = ci + 1;
	}
	ci->c_nextfree = NULL;
	ctx_kick = 0;
	ctx_kickdir = -1;

	/*
	 * Init mmu entries that map the kernel physical addresses.
	 *
	 * All the other MMU entries are free.
	 *
	 * THIS ASSUMES SEGMENT i IS MAPPED BY MMU ENTRY i DURING THE
	 * BOOT PROCESS
	 */

	rom_setmap = promvec->pv_setctxt;
	zseg = ((((u_int)p + NBPSG - 1) & ~SGOFSET) - KERNBASE) >> SGSHIFT;
	lastpage = VA_VPG(p);
	if (lastpage == 0)
		/*
		 * If the page bits in p are 0, we filled the last segment
		 * exactly (now how did that happen?); if not, it is
		 * the last page filled in the last segment.
		 */
		lastpage = NPTESG;

	p = (caddr_t)KERNBASE;		/* first va */
	vs = VA_VSEG(KERNBASE);		/* first virtual segment */
	vr = VA_VREG(KERNBASE);		/* first virtual region */
	rp = &pmap_kernel()->pm_regmap[vr];

	for (rcookie = 0, scookie = 0;;) {

		/*
		 * Distribute each kernel region/segment into all contexts.
		 * This is done through the monitor ROM, rather than
		 * directly here: if we do a setcontext we will fault,
		 * as we are not (yet) mapped in any other context.
		 */

		if ((vs % NSEGRG) == 0) {
			/* Entering a new region */
			if (VA_VREG(p) > vr) {
#ifdef DEBUG
				printf("note: giant kernel!\n");
#endif
				vr++, rp++;
			}
#ifdef MMU_3L
			if (mmu_3l) {
				for (i = 1; i < nctx; i++)
					rom_setmap(i, p, rcookie);

				TAILQ_INSERT_TAIL(&region_locked,
						  mmureg, me_list);
				TAILQ_INSERT_TAIL(&pmap_kernel()->pm_reglist,
						  mmureg, me_pmchain);
				mmureg->me_cookie = rcookie;
				mmureg->me_pmap = pmap_kernel();
				mmureg->me_vreg = vr;
				rp->rg_smeg = rcookie;
				mmureg++;
				rcookie++;
			}
#endif
		}

#ifdef MMU_3L
		if (!mmu_3l)
#endif
			for (i = 1; i < nctx; i++)
				rom_setmap(i, p, scookie);

		/* set up the mmu entry */
		TAILQ_INSERT_TAIL(&segm_locked, mmuseg, me_list);
		TAILQ_INSERT_TAIL(&pmap_kernel()->pm_seglist, mmuseg, me_pmchain);
		mmuseg->me_cookie = scookie;
		mmuseg->me_pmap = pmap_kernel();
		mmuseg->me_vreg = vr;
		mmuseg->me_vseg = vs % NSEGRG;
		rp->rg_segmap[vs % NSEGRG].sg_pmeg = scookie;
		npte = ++scookie < zseg ? NPTESG : lastpage;
		rp->rg_segmap[vs % NSEGRG].sg_npte = npte;
		rp->rg_nsegmap += 1;
		mmuseg++;
		vs++;
		if (scookie < zseg) {
			p += NBPSG;
			continue;
		}

		/*
		 * Unmap the pages, if any, that are not part of
		 * the final segment.
		 */
		for (p += npte << PGSHIFT; npte < NPTESG; npte++, p += NBPG)
			setpte4(p, 0);

#ifdef MMU_3L
		if (mmu_3l) {
			/*
			 * Unmap the segments, if any, that are not part of
			 * the final region.
			 */
			for (i = rp->rg_nsegmap; i < NSEGRG; i++, p += NBPSG)
				setsegmap(p, seginval);
		}
#endif
		break;
	}

#ifdef MMU_3L
	if (mmu_3l)
		for (; rcookie < nregion; rcookie++, mmureg++) {
			mmureg->me_cookie = rcookie;
			TAILQ_INSERT_TAIL(&region_freelist, mmureg, me_list);
		}
#endif

	for (; scookie < nsegment; scookie++, mmuseg++) {
		mmuseg->me_cookie = scookie;
		TAILQ_INSERT_TAIL(&segm_freelist, mmuseg, me_list);
	}

	/* Erase all spurious user-space segmaps */
	for (i = 1; i < ncontext; i++) {
		setcontext(i);
#ifdef MMU_3L
		if (mmu_3l)
			for (p = 0, j = NUREG; --j >= 0; p += NBPRG)
				setregmap(p, reginval);
		else
#endif
			for (p = 0, vr = 0; vr < NUREG; vr++) {
				if (VA_INHOLE(p)) {
					p = (caddr_t)MMU_HOLE_END;
					vr = VA_VREG(p);
				}
				for (j = NSEGRG; --j >= 0; p += NBPSG)
					setsegmap(p, seginval);
			}
	}
	setcontext(0);

	/*
	 * write protect & encache kernel text;
	 * set red zone at kernel base; enable cache on message buffer.
	 */
	{
		extern char etext[];
#ifdef KGDB
		register int mask = ~PG_NC;	/* XXX chgkprot is busted */
#else
		register int mask = ~(PG_W | PG_NC);
#endif

		for (p = (caddr_t)trapbase; p < etext; p += NBPG)
			setpte4(p, getpte4(p) & mask);
	}
}
#endif

#if defined(SUN4M)		/* Sun4M version of pmap_bootstrap */
/*
 * Bootstrap the system enough to run with VM enabled on a Sun4M machine.
 *
 * Switches from ROM to kernel page tables, and sets up initial mappings.
 */
static void
pmap_bootstrap4m(void)
{
	register int i, j;
	caddr_t p, p2;
	register caddr_t q;
	register union ctxinfo *ci;
	register struct memarr *mp;
	struct   regmap *rmapp = NULL;
	struct	 segmap *smapp = NULL;
	register int reg, seg;
#if 0
	int nkreg, nkseg, nkpag, kernsize, newpgs;
#endif
	caddr_t kphyssegtbl, kphyspagtbl = NULL;
	int deadfill, deadspace;
	extern char end[];
	extern char etext[];
#ifdef DDB
	extern char *esym;
	char *theend = end;
#endif
	extern caddr_t reserve_dumppages(caddr_t);

#if defined(SUN4) || defined(SUN4C) /* setup 4M fn. ptrs for dual-arch kernel */
	pmap_clear_modify_p 	=	pmap_clear_modify4m;
	pmap_clear_reference_p 	= 	pmap_clear_reference4m;
	pmap_copy_page_p 	=	pmap_copy_page4m;
	pmap_enter_p 		=	pmap_enter4m;
	pmap_extract_p 		=	pmap_extract4m;
	pmap_is_modified_p 	=	pmap_is_modified4m;
	pmap_is_referenced_p	=	pmap_is_referenced4m;
	pmap_page_protect_p	=	pmap_page_protect4m;
	pmap_protect_p		=	pmap_protect4m;
	pmap_zero_page_p	=	pmap_zero_page4m;
	pmap_changeprot_p	=	pmap_changeprot4m;
	pmap_rmk_p		=	pmap_rmk4m;
	pmap_rmu_p		=	pmap_rmu4m;
#endif /* defined Sun4/Sun4c */

	/*
	 * Intialize the kernel pmap.
	 */
	/* kernel_pmap_store.pm_ctxnum = 0; */
	simple_lock_init(kernel_pmap_store.pm_lock);
	kernel_pmap_store.pm_refcount = 1;
	/* Set up pm_regmap for kernel to point NUREG *below* the beginning
	 * of kernel regmap storage. Since the kernel only uses regions
	 * above NUREG, we save storage space and can index kernel and
	 * user regions in the same way
	 */
	kernel_pmap_store.pm_regmap = &kernel_regmap_store[-NUREG];
	kernel_pmap_store.pm_reg_ptps = NULL;
	kernel_pmap_store.pm_reg_ptps_pa = 0;
	bzero(kernel_regmap_store, NKREG * sizeof(struct regmap));
	bzero(kernel_segmap_store, NKREG * NSEGRG * sizeof(struct segmap));
	for (i = NKREG; --i >= 0;) {
		kernel_regmap_store[i].rg_segmap =
			&kernel_segmap_store[i * NSEGRG];
		kernel_regmap_store[i].rg_seg_ptps = NULL;
		for (j = NSEGRG; --j >= 0;)
			kernel_segmap_store[i * NSEGRG + j].sg_pte = NULL;
	}

	p = end;		/* p points to top of kernel mem */
#ifdef DDB
	if (esym != 0)
		theend = p = esym;
#endif

	/*
	 * Preserve the monitor ROM's reserved VM region, so that
	 * we can use L1-A or the monitor's debugger.
	 */

	mmu_reservemon4m(&kernel_pmap_store, &p);

	pmap_kernel()->pm_ctx = ctxinfo = ci = (union ctxinfo *)p;
	p += ncontext * sizeof *ci;
	bzero((caddr_t)ctxinfo, (u_int)p - (u_int)ctxinfo);
	ctxbusyvector = p;
	p += ncontext;
	bzero(ctxbusyvector, ncontext);
	ctxbusyvector[0] = 1;	/* context 0 is always in use */

	/*
	 * reserve memory for I/O pagetables. This takes 64k of memory
	 * since we want to have 64M of dvma space (this actually depends
	 * on the definition of DVMA4M_BASE...we may drop it back to 32M)
	 * but since the table must be aligned, we might end up using
	 * as much as 128K. (note 1024 = NBPG / sizeof(iopte_t))
	 *
	 * We optimize with some space saving song and dance to
	 * squeeze other pagetables in the dead space.
	 */
#ifdef DEBUG
	if ((0 - DVMA4M_BASE) % (16*1024*1024))
	    panic("pmap_bootstrap4m: invalid DVMA4M_BASE of 0x%x",DVMA4M_BASE);
#endif

	deadfill = 0;
	p = (caddr_t) roundup((u_int) p, max(SRMMU_L1SIZE * sizeof(long),
				   max(SRMMU_L2SIZE * sizeof(long),
				       SRMMU_L3SIZE * sizeof(long))));

	deadspace = (int) (((caddr_t)roundup((u_int)p,
					     (0 - DVMA4M_BASE) / 1024)) - p);

	if (deadspace >= SRMMU_L3SIZE * sizeof(long) * NKREG * NSEGRG) {
		p = (caddr_t) roundup((u_int)p, SRMMU_L3SIZE * sizeof(long));
		kernel_pagtable_store = (u_int *)p;
		p += ((SRMMU_L3SIZE * sizeof(long)) * NKREG) * NSEGRG;
		bzero(kernel_pagtable_store,
		      p - (caddr_t) kernel_pagtable_store);
		deadfill |= 4;
		deadspace -= (int)(p - (caddr_t) kernel_pagtable_store);
	}
	if (deadspace >= SRMMU_L2SIZE * sizeof(long) * NKREG) {
		p = (caddr_t) roundup((u_int)p, SRMMU_L2SIZE * sizeof(long));
		kernel_segtable_store = (u_int *)p;
		p += (SRMMU_L2SIZE * sizeof(long)) * NKREG;
		bzero(kernel_segtable_store,
		      p - (caddr_t) kernel_segtable_store);
		deadfill |= 2;
		deadspace -= (int)(p - (caddr_t) kernel_segtable_store);
	}
	if (deadspace >= SRMMU_L1SIZE * sizeof(long)) {
		p = (caddr_t) roundup((u_int)p, SRMMU_L1SIZE * sizeof(long));
		kernel_regtable_store = (u_int *)p;
		p += SRMMU_L1SIZE * sizeof(long);
		bzero(kernel_regtable_store,
		      p - (caddr_t) kernel_regtable_store);
		deadfill |= 1;
		deadspace -= (int)(p - (caddr_t) kernel_regtable_store);
	}
	if (deadspace < 0)
	    printf("pmap_bootstrap4m: botch in memory-saver\n");

	p = (caddr_t) roundup((u_int)p, (0 - DVMA4M_BASE) / 1024);
	kernel_iopte_table = (u_int *)p;
	kernel_iopte_table_pa = VA2PA((caddr_t)kernel_iopte_table);
	p += (0 - DVMA4M_BASE) / 1024;
	bzero(kernel_iopte_table, p - (caddr_t) kernel_iopte_table);

	/*
	 * reserve memory for segment and page tables needed to map the entire
	 * kernel (from regions 0xf8 -> 0xff). This takes 130k of space, but
	 * unfortunately is necessary since pmap_enk *must* be able to enter
	 * a kernel mapping without resorting to malloc, or else the
	 * possibility of deadlock arises (pmap_enk4m is called to enter a
	 * mapping; it needs to malloc a page table; malloc then calls
	 * pmap_enk4m to enter the new malloc'd page; pmap_enk4m needs to
	 * malloc a page table to enter _that_ mapping; malloc deadlocks since
	 * it is already allocating that object).
	 *
	 * We only do this if it wasn't done above...
	 */
	if (!(deadfill & 2)) {
		p = (caddr_t) roundup((u_int)p, SRMMU_L2SIZE * sizeof(long));
		kernel_segtable_store = (u_int *)p;
		p += (SRMMU_L2SIZE * sizeof(long)) * NKREG;
		bzero(kernel_segtable_store,
		      p - (caddr_t) kernel_segtable_store);
	}
	if (!(deadfill & 4)) {
		p = (caddr_t) roundup((u_int)p, SRMMU_L3SIZE * sizeof(long));
		kernel_pagtable_store = (u_int *)p;
		p += ((SRMMU_L3SIZE * sizeof(long)) * NKREG) * NSEGRG;
		bzero(kernel_pagtable_store,
		      p - (caddr_t) kernel_pagtable_store);
	}
	if (!(deadfill & 1)) {
		p = (caddr_t) roundup((u_int)p, SRMMU_L1SIZE * sizeof(long));
		kernel_regtable_store = (u_int *)p;
		p += SRMMU_L1SIZE * sizeof(long);
		bzero(kernel_regtable_store,
		      p - (caddr_t) kernel_regtable_store);
	}

#if 0
	/* At this point, "p" is the highest location of kernel memory that
	 * we need to lock in/map initially. We now need to calculate the
	 * size of this kernel image and allocate memory for its page tables,
	 * which we won't set up until the end of this function. After this
	 * point, it is NOT POSSIBLE to allocate kernel physical memory
	 * by bumping up p!!!
	 */
	p = (caddr_t) roundup((u_int)p,SRMMU_L1SIZE * sizeof(long));
	kernsize = (u_int)p - KERNBASE;

	/* We keep adding pages until the number of added pages is sufficient
	 * to hold the map for both the kernel and the new pages. This
	 * converges since it takes at most about 1152 bytes to map one page.
	 */
	newpgs = 0;
	do {
		newpgs++;
		nkreg = (kernsize + (newpgs * NBPG) + NBPRG - 1) / NBPRG;
		nkseg = (kernsize + (newpgs * NBPG) + NBPSG - 1) / NBPSG;
                nkpag = (kernsize + NBPG - 1) / NBPG + newpgs;
	} while (((SRMMU_L1SIZE + (nkreg * SRMMU_L2SIZE) +
		   (nkseg * SRMMU_L3SIZE) + (nkpag)) * sizeof(long)) >
		 newpgs * NBPG);

	kernsize += newpgs * NBPG;
	p2 = p;			/* Page tables go from p2 to p. */
	p += newpgs * NBPG;	/* p is now the _real_ top of kernel mem. */
#endif

	/*
	 * Set up the `constants' for the call to vm_init()
	 * in main().  All pages beginning at p (rounded up to
	 * the next whole page) and continuing through the number
	 * of available pages are free, but they start at a higher
	 * virtual address.  This gives us two mappable MD pages
	 * for pmap_zero_page and pmap_copy_page, and one MI page
	 * for /dev/mem, all with no associated physical memory.
	 */
	p = (caddr_t)(((u_int)p + NBPG - 1) & ~PGOFSET);
	avail_start = (int)p - KERNBASE;
	/*
	 * Grab physical memory list, so pmap_next_page() can do its bit.
	 */
	npmemarr = makememarr(pmemarr, MA_SIZE, MEMARR_AVAILPHYS);
	sortm(pmemarr, npmemarr);
	if (pmemarr[0].addr != 0) {
		printf("pmap_bootstrap: no kernel memory?!\n");
		callrom();
	}
	avail_end = pmemarr[npmemarr-1].addr + pmemarr[npmemarr-1].len;
	avail_next = avail_start;
	for (physmem = 0, mp = pmemarr, j = npmemarr; --j >= 0; mp++)
		physmem += btoc(mp->len);

	i = (int)p;
	vpage[0] = p, p += NBPG;
	vpage[1] = p, p += NBPG;
	vmmap = p, p += NBPG;
	p = reserve_dumppages(p);

	/*
	 * Allocate virtual memory for pv_table[], which will be mapped
	 * sparsely in pmap_init().
	 */
	pv_table = (struct pvlist *)p;
	p += round_page(sizeof(struct pvlist) * atop(avail_end - avail_start));

	virtual_avail = (vm_offset_t)p;
	virtual_end = VM_MAX_KERNEL_ADDRESS;

	p = (caddr_t)i;			/* retract to first free phys */

	/* Set up the ctxinfo structures (freelist of contexts)
	 */
	ci->c_pmap = pmap_kernel();
	ctx_freelist = ci + 1;
	for (i = 1; i < ncontext; i++) {
		ci++;
		ci->c_nextfree = ci + 1;
	}
	ci->c_nextfree = NULL;
	ctx_kick = 0;
	ctx_kickdir = -1;

	/* Now map the kernel into our new set of page tables, then
	 * (finally) switch over to our running page tables.
	 * We map from KERNBASE to p into context 0's page tables (and
	 * the kernel pmap).
	 */
#ifdef DEBUG			/* Sanity checks */
	if ((u_int)p % NBPG != 0)
	    panic("pmap_bootstrap4m: p misaligned?!?");
	if (KERNBASE % NBPRG != 0)
	    panic("pmap_bootstrap4m: KERNBASE not region-aligned");
#endif
	if (pmap_kernel()->pm_reg_ptps == NULL) {
#ifdef DEBUG
		printf("pmap_bootstrap4m: no kernel regiontable; creating.\n");
#endif
		pmap_kernel()->pm_reg_ptps = (int *) kernel_regtable_store;
		pmap_kernel()->pm_reg_ptps_pa =
			VA2PA((caddr_t)pmap_kernel()->pm_reg_ptps);
		p2 = (caddr_t) pmap_kernel()->pm_reg_ptps +
		    (SRMMU_L1SIZE * sizeof(int));
		bzero(pmap_kernel()->pm_reg_ptps, SRMMU_L1SIZE * sizeof(long));
	}
	reg = -1;
	seg = -1;
	for (q = (caddr_t) KERNBASE; q < p; q += NBPG) {
		if ((VA_VSEG(q) % NSEGRG == 0) && VA_VREG(q) != reg) {
			/* entering new region; install & build segtbl
			 * XXX: WE TRASH ANY EXISTING MAPPINGS IN THE KERNEL
			 *      REGION. SHOULD BE FIXED!
			 */
			reg = VA_VREG(q);
			rmapp = &(pmap_kernel()->pm_regmap[reg]);
			kphyssegtbl = (caddr_t)
			    &kernel_segtable_store[(reg - VA_VREG(KERNBASE)) *
						   SRMMU_L2SIZE];
			bzero(kphyssegtbl, SRMMU_L2SIZE * sizeof(long));

			((int *)pmap_kernel()->pm_reg_ptps)[VA_VREG(q)] =
			    (VA2PA(kphyssegtbl) >> SRMMU_PPNPASHIFT) |
				SRMMU_TEPTD;

			rmapp->rg_seg_ptps = (int *)kphyssegtbl;

			if (rmapp->rg_segmap == NULL)
			    rmapp->rg_segmap = &kernel_segmap_store[(reg -
			        VA_VREG(KERNBASE)) * NSEGRG];
		}
		if (((VA_VPG(q) % NPTESG) == 0) && VA_VSEG(q) != seg) {
			rmapp->rg_nsegmap++;
			/* Entering new segment. XXX: trashes existing maps */
			seg = VA_VSEG(q);
			smapp = &(rmapp->rg_segmap[seg]);
			kphyspagtbl = (caddr_t)
			    &kernel_pagtable_store[(((reg-VA_VREG(KERNBASE))*
						     NSEGRG) + seg)*SRMMU_L3SIZE];
			bzero(kphyspagtbl, SRMMU_L3SIZE * sizeof(long));

			rmapp->rg_seg_ptps[VA_VSEG(q)] =
			    (VA2PA(kphyspagtbl) >> SRMMU_PPNPASHIFT) |
				SRMMU_TEPTD;
			smapp->sg_pte = (int *) kphyspagtbl;
		}
		/* Now install entry for current page. Cache and write-protect
		 * kernel text.
		 */
		smapp->sg_npte++;
		if (q == (caddr_t) KERNBASE) {
			/* Must map in message buffer in low page. */
			((int *)kphyspagtbl)[VA_VPG(q)] = PPROT_N_RWX |
			    SRMMU_PG_C | SRMMU_TEPTE;
			continue;
		}
		else if (q < (caddr_t) (KERNBASE+0x4000)) {
			/* Special case: retain interrupt register mapping */
			((int *)kphyspagtbl)[VA_VPG(q)] =
			    getpte4m((vm_offset_t)q);
		}
		if (q >= (caddr_t) trapbase && q < etext)
			((int *)kphyspagtbl)[VA_VPG(q)] =
				(VA2PA(q) >> SRMMU_PPNPASHIFT) |
				PPROT_N_RX | SRMMU_PG_C | SRMMU_TEPTE;
		else
			((int *)kphyspagtbl)[VA_VPG(q)] =
				(VA2PA(q) >> SRMMU_PPNPASHIFT) |
				PPROT_N_RWX | SRMMU_PG_C | SRMMU_TEPTE;
	}

	/*
	 * We must also create a segment table for region 0xfe and any
	 * needed page tables necessary to map the special (important)
	 * devices that get mapped at the beginning of this I/O segment.
	 * This is necessary since we have to map the interrupt registers
	 * at the start of region 0xfe in bootstrap().
	 *
	 * Since we've statically allocated space to map the entire kernel,
	 * we might as well pre-wire the mappings to save time in pmap_enter.
	 * This also gets around nasty problems with caching of L1/L2 ptp's.
	 *
	 * XXX WHY DO WE HAVE THIS CACHING PROBLEM WITH L1/L2 PTPS????? %%%
	 */
	for (reg = VA_VREG(KERNBASE); reg < NKREG+VA_VREG(KERNBASE); reg++) {
		rmapp = &(pmap_kernel()->pm_regmap[reg]);

		if (((int *)pmap_kernel()->pm_reg_ptps)[reg] == NULL) {
			kphyssegtbl = (caddr_t)
			    &kernel_segtable_store[(reg - VA_VREG(KERNBASE)) *
						   SRMMU_L2SIZE];
			bzero(kphyssegtbl, SRMMU_L2SIZE * sizeof(long));
			((int *)pmap_kernel()->pm_reg_ptps)[reg] =
			    (VA2PA(kphyssegtbl) >> SRMMU_PPNPASHIFT) |
				SRMMU_TEPTD;

			rmapp->rg_seg_ptps = (int *)kphyssegtbl;

			if (rmapp->rg_segmap == NULL)
				rmapp->rg_segmap = &kernel_segmap_store
					[(reg - VA_VREG(KERNBASE)) * NSEGRG];
		}
		for (seg = 0; seg < NSEGRG; seg++) {
			if (rmapp->rg_seg_ptps[seg] == NULL) {
				rmapp->rg_nsegmap++;

				smapp = &(rmapp->rg_segmap[seg]);
				kphyspagtbl = (caddr_t)
				    &kernel_pagtable_store[(((reg-
							    VA_VREG(KERNBASE))*
							     NSEGRG) + seg)*
							   SRMMU_L3SIZE];
				bzero(kphyspagtbl,
				      SRMMU_L3SIZE * sizeof(long));

				rmapp->rg_seg_ptps[seg] =
				    (VA2PA(kphyspagtbl) >> SRMMU_PPNPASHIFT) |
					SRMMU_TEPTD;
				smapp->sg_pte = (int *) kphyspagtbl;
			}
		}
	}

	/*
	 * We also install the kernel mapping into all other contexts by
	 * copying the context 0 L1 PTP from ctx_phys_tbl[0] into the
	 * remainder of the context table (i.e. we share the kernel page-
	 * tables). Each user pmap automatically gets the kernel mapped
	 * into it when it is created, but we do this extra step early on
	 * in case some twit decides to switch to a context with no user
	 * pmap associated with it.
	 */
	for (i = 1; i < ncontext; i++)
	    ctx_phys_tbl[i] = ctx_phys_tbl[0];

	/*
	 * Now switch to kernel pagetables (finally!)
	 */
#ifdef DEBUG
	printf("pmap_bootstrap: installing kernel page tables...");
#endif
	setcontext(0);		/* paranoia? %%%: Make 0x3 a define! below */

	/* Enable MMU tablewalk caching, flush TLB */

	i = lda(SRMMU_PCR, ASI_SRMMU);
	switch(mmumod) {
/*	case SUN4M_MMU_MS: */  /* These have the same model # as SS */
	case SUN4M_MMU_SS:
		if ((cpumod & 0xf0) == (SUN4M_SS & 0xf0))
			sta(SRMMU_PCR, ASI_SRMMU, (i | SRMMU_PCR_TC));
		else	/* microsparc */
			printf("(if this doesn't work, "
			       "fix pmap_bootstrap4m in pmap.c)");
		break;
	case SUN4M_MMU_HS:
		printf("(if this doesn't work, fix pmap_bootstrap4m in pmap.c)");
		sta(SRMMU_PCR, ASI_SRMMU, (i | SRMMU_PCR_C) & ~SRMMU_PCR_CE);
		/* above: CHECKFIX %%% below: add microsparc*/
		break;
	case SUN4M_MMU_MS1:
		printf("(if this doesn't work, fix pmap_bootstrap4m in pmap.c)");
		break;
	default:
		panic("Unimplemented MMU architecture %d",mmumod);
	}

	tlb_flush_all();

	sta(SRMMU_CXTPTR, ASI_SRMMU,
	    (VA2PA((caddr_t)ctx_phys_tbl) >> SRMMU_PPNPASHIFT) & ~0x3);

	tlb_flush_all();

#ifdef DEBUG
	printf("done.\n");
#endif

	/*
	 * On SuperSPARC machines without a MXCC, we *cannot* cache the
	 * page tables.
	 * XXX: this needs to be cleaned up with the cpu_softc stuff...
	 */
	if (mmumod == SUN4M_MMU_SS &&
	    (lda(SRMMU_PCR, ASI_SRMMU) & SRMMU_PCR_MB)) {

		int bytes, numpages;
		cant_cache_pagetables = 1;

#define DO_THE_MATH(math)						\
	bytes = (math);							\
	numpages = (bytes >> PGSHIFT) + (bytes % NBPG ? 1 : 0);

		DO_THE_MATH(SRMMU_L3SIZE * sizeof(long) * NKREG * NSEGRG);
#ifdef DEBUG
		printf("pmap_bootstrap4m: uncaching %d PT pages at 0x%lx\n",
		    numpages, (long)kernel_pagtable_store);
#endif
		kvm_uncache((caddr_t)kernel_pagtable_store, numpages);

		DO_THE_MATH(SRMMU_L2SIZE * sizeof(long) * NKREG);
#ifdef DEBUG
		printf("pmap_bootstrap4m: uncaching %d ST pages at 0x%lx\n",
		    numpages, (long)kernel_segtable_store);
#endif
		kvm_uncache((caddr_t)kernel_segtable_store, numpages);

		DO_THE_MATH(SRMMU_L1SIZE * sizeof(long));
#ifdef DEBUG
		printf("pmap_bootstrap4m: uncaching %d RT pages at 0x%lx\n",
		    numpages, (long)kernel_regtable_store);
#endif
		kvm_uncache((caddr_t)kernel_regtable_store, numpages);

#undef DO_THE_MATH
	}

#ifdef DEBUG
	printf("\n");	/* Might as well make it pretty... */
#endif

	/* All done! */
}
#endif /* defined sun4m */

void
pmap_init()
{
	register vm_size_t s;
	int pass1, nmem;
	register struct memarr *mp;
	vm_offset_t sva, va, eva;
	vm_offset_t pa = 0;

	if (PAGE_SIZE != NBPG)
		panic("pmap_init: CLSIZE!=1");

	/*
	 * Map pv_table[] as a `sparse' array. This requires two passes
	 * over the `pmemarr': (1) to determine the number of physical
	 * pages needed, and (2), to map the correct pieces of virtual
	 * memory allocated to pv_table[].
	 */

	s = 0;
	pass1 = 1;

pass2:
	sva = eva = 0;
	for (mp = pmemarr, nmem = npmemarr; --nmem >= 0; mp++) {
		int len;
		vm_offset_t addr;

		len = mp->len;
		if ((addr = mp->addr) < avail_start) {
			/*
			 * pv_table[] covers everything above `avail_start'.
			 */
			addr = avail_start;
			len -= avail_start;
		}
		len = sizeof(struct pvlist) * atop(len);

		if (addr < avail_start || addr >= avail_end)
			panic("pmap_init: unmanaged address: 0x%lx", addr);

		va = (vm_offset_t)&pv_table[atop(addr - avail_start)];
		sva = trunc_page(va);

		if (sva < eva) {
#if defined(DEBUG) && !defined(SUN4M)
			/*
			 * crowded chunks are normal on SS20s; don't clutter
			 * screen with messages
			 */
			printf("note: crowded chunk at 0x%x\n", mp->addr);
#endif
			sva += PAGE_SIZE;
			if (sva < eva)
				panic("pmap_init: sva(%lx) < eva(%lx)",
				      sva, eva);
		}
		eva = round_page(va + len);
		if (pass1) {
			/* Just counting */
			s += eva - sva;
			continue;
		}

		/* Map this piece of pv_table[] */
		for (va = sva; va < eva; va += PAGE_SIZE) {
			pmap_enter(pmap_kernel(), va, pa,
				   VM_PROT_READ|VM_PROT_WRITE, 1);
			pa += PAGE_SIZE;
		}
		bzero((caddr_t)sva, eva - sva);
	}

	if (pass1) {
		pa = pmap_extract(pmap_kernel(), kmem_alloc(kernel_map, s));
		pass1 = 0;
		goto pass2;
	}

	vm_first_phys = avail_start;
	vm_num_phys = avail_end - avail_start;
}


/*
 * Map physical addresses into kernel VM.
 */
vm_offset_t
pmap_map(va, pa, endpa, prot)
	register vm_offset_t va, pa, endpa;
	register int prot;
{
	register int pgsize = PAGE_SIZE;

	while (pa < endpa) {
		pmap_enter(pmap_kernel(), va, pa, prot, 1);
		va += pgsize;
		pa += pgsize;
	}
	return (va);
}

/*
 * Create and return a physical map.
 *
 * If size is nonzero, the map is useless. (ick)
 */
struct pmap *
pmap_create(size)
	vm_size_t size;
{
	register struct pmap *pm;

	if (size)
		return (NULL);
	pm = (struct pmap *)malloc(sizeof *pm, M_VMPMAP, M_WAITOK);
#ifdef DEBUG
	if (pmapdebug & PDB_CREATE)
		printf("pmap_create: created %p\n", pm);
#endif
	bzero((caddr_t)pm, sizeof *pm);
	pmap_pinit(pm);
	return (pm);
}

/*
 * Initialize a preallocated and zeroed pmap structure,
 * such as one in a vmspace structure.
 */
void
pmap_pinit(pm)
	register struct pmap *pm;
{
	register int size;
	void *urp;

#ifdef DEBUG
	if (pmapdebug & PDB_CREATE)
		printf("pmap_pinit(%p)\n", pm);
#endif

	size = NUREG * sizeof(struct regmap);

	pm->pm_regstore = urp = malloc(size, M_VMPMAP, M_WAITOK);
	qzero((caddr_t)urp, size);
	/* pm->pm_ctx = NULL; */
	simple_lock_init(&pm->pm_lock);
	pm->pm_refcount = 1;
	pm->pm_regmap = urp;

	if (CPU_ISSUN4OR4C) {
		TAILQ_INIT(&pm->pm_seglist);
#ifdef MMU_3L
		TAILQ_INIT(&pm->pm_reglist);
		if (mmu_3l)
		    for (i = NUREG; --i >= 0;)
			pm->pm_regmap[i].rg_smeg = reginval;
#endif
	}
#if defined(SUN4M)
	else {
		/*
		 * We must allocate and initialize hardware-readable (MMU)
		 * pagetables. We must also map the kernel regions into this
		 * pmap's pagetables, so that we can access the kernel from
		 * user mode!
		 *
		 * Note: pm->pm_regmap's have been zeroed already, so we don't
		 * need to explicitly mark them as invalid (a null
		 * rg_seg_ptps pointer indicates invalid for the 4m)
		 */
		urp = malloc(SRMMU_L1SIZE * sizeof(int), M_VMPMAP, M_WAITOK);
		if (cant_cache_pagetables)
			kvm_uncache(urp,
				    ((SRMMU_L1SIZE*sizeof(int))+NBPG-1)/NBPG);

#ifdef DEBUG
		if ((u_int) urp % (SRMMU_L1SIZE * sizeof(int)))
			panic("pmap_pinit: malloc() not giving aligned memory");
#endif
		pm->pm_reg_ptps = urp;
		pm->pm_reg_ptps_pa = VA2PA(urp);
		qzero(urp, SRMMU_L1SIZE * sizeof(int));

	}
#endif

	pm->pm_gap_end = VA_VREG(VM_MAXUSER_ADDRESS);

	return;
}

/*
 * Retire the given pmap from service.
 * Should only be called if the map contains no valid mappings.
 */
void
pmap_destroy(pm)
	register struct pmap *pm;
{
	int count;

	if (pm == NULL)
		return;
#ifdef DEBUG
	if (pmapdebug & PDB_DESTROY)
		printf("pmap_destroy(%p)\n", pm);
#endif
	simple_lock(&pm->pm_lock);
	count = --pm->pm_refcount;
	simple_unlock(&pm->pm_lock);
	if (count == 0) {
		pmap_release(pm);
		free(pm, M_VMPMAP);
	}
}

/*
 * Release any resources held by the given physical map.
 * Called when a pmap initialized by pmap_pinit is being released.
 */
void
pmap_release(pm)
	register struct pmap *pm;
{
	register union ctxinfo *c;
	register int s = splpmap();	/* paranoia */

#ifdef DEBUG
	if (pmapdebug & PDB_DESTROY)
		printf("pmap_release(%p)\n", pm);
#endif

	if (CPU_ISSUN4OR4C) {
#ifdef MMU_3L
		if (pm->pm_reglist.tqh_first)
			panic("pmap_release: region list not empty");
#endif
		if (pm->pm_seglist.tqh_first)
			panic("pmap_release: segment list not empty");

		if ((c = pm->pm_ctx) != NULL) {
			if (pm->pm_ctxnum == 0)
				panic("pmap_release: releasing kernel");
			ctx_free(pm);
		}
	}
	splx(s);

#ifdef DEBUG
if (pmapdebug) {
	int vs, vr;
	for (vr = 0; vr < NUREG; vr++) {
		struct regmap *rp = &pm->pm_regmap[vr];
		if (rp->rg_nsegmap != 0)
			printf("pmap_release: %d segments remain in "
				"region %d\n", rp->rg_nsegmap, vr);
		if (rp->rg_segmap != NULL) {
			printf("pmap_release: segments still "
				"allocated in region %d\n", vr);
			for (vs = 0; vs < NSEGRG; vs++) {
				struct segmap *sp = &rp->rg_segmap[vs];
				if (sp->sg_npte != 0)
					printf("pmap_release: %d ptes "
					     "remain in segment %d\n",
						sp->sg_npte, vs);
				if (sp->sg_pte != NULL) {
					printf("pmap_release: ptes still "
					     "allocated in segment %d\n", vs);
				}
			}
		}
	}
}
#endif
	if (pm->pm_regstore)
		free(pm->pm_regstore, M_VMPMAP);

	if (CPU_ISSUN4M) {
		if ((c = pm->pm_ctx) != NULL) {
			if (pm->pm_ctxnum == 0)
				panic("pmap_release: releasing kernel");
			ctx_free(pm);
		}
		free(pm->pm_reg_ptps, M_VMPMAP);
		pm->pm_reg_ptps = NULL;
		pm->pm_reg_ptps_pa = 0;
	}
}

/*
 * Add a reference to the given pmap.
 */
void
pmap_reference(pm)
	struct pmap *pm;
{

	if (pm != NULL) {
		simple_lock(&pm->pm_lock);
		pm->pm_refcount++;
		simple_unlock(&pm->pm_lock);
	}
}

/*
 * Remove the given range of mapping entries.
 * The starting and ending addresses are already rounded to pages.
 * Sheer lunacy: pmap_remove is often asked to remove nonexistent
 * mappings.
 */
void
pmap_remove(pm, va, endva)
	register struct pmap *pm;
	register vm_offset_t va, endva;
{
	register vm_offset_t nva;
	register int vr, vs, s, ctx;
	register void (*rm)(struct pmap *, vm_offset_t, vm_offset_t, int, int);

	if (pm == NULL)
		return;

#ifdef DEBUG
	if (pmapdebug & PDB_REMOVE)
		printf("pmap_remove(%p, %lx, %lx)\n", pm, va, endva);
#endif

	if (pm == pmap_kernel()) {
		/*
		 * Removing from kernel address space.
		 */
		rm = pmap_rmk;
	} else {
		/*
		 * Removing from user address space.
		 */
		write_user_windows();
		rm = pmap_rmu;
	}

	ctx = getcontext();
	s = splpmap();		/* XXX conservative */
	simple_lock(&pm->pm_lock);
	for (; va < endva; va = nva) {
		/* do one virtual segment at a time */
		vr = VA_VREG(va);
		vs = VA_VSEG(va);
		nva = VSTOVA(vr, vs + 1);
		if (nva == 0 || nva > endva)
			nva = endva;
		(*rm)(pm, va, nva, vr, vs);
	}
	simple_unlock(&pm->pm_lock);
	splx(s);
	setcontext(ctx);
}

/*
 * The following magic number was chosen because:
 *	1. It is the same amount of work to cache_flush_page 4 pages
 *	   as to cache_flush_segment 1 segment (so at 4 the cost of
 *	   flush is the same).
 *	2. Flushing extra pages is bad (causes cache not to work).
 *	3. The current code, which malloc()s 5 pages for each process
 *	   for a user vmspace/pmap, almost never touches all 5 of those
 *	   pages.
 */
#if 0
#define	PMAP_RMK_MAGIC	(cacheinfo.c_hwflush?5:64)	/* if > magic, use cache_flush_segment */
#else
#define	PMAP_RMK_MAGIC	5	/* if > magic, use cache_flush_segment */
#endif

/*
 * Remove a range contained within a single segment.
 * These are egregiously complicated routines.
 */

#if defined(SUN4) || defined(SUN4C)

/* remove from kernel */
/*static*/ void
pmap_rmk4_4c(pm, va, endva, vr, vs)
	register struct pmap *pm;
	register vm_offset_t va, endva;
	register int vr, vs;
{
	register int i, tpte, perpage, npg;
	register struct pvlist *pv;
	register int nleft, pmeg;
	struct regmap *rp;
	struct segmap *sp;

	rp = &pm->pm_regmap[vr];
	sp = &rp->rg_segmap[vs];

	if (rp->rg_nsegmap == 0)
		return;

#ifdef DEBUG
	if (rp->rg_segmap == NULL)
		panic("pmap_rmk: no segments");
#endif

	if ((nleft = sp->sg_npte) == 0)
		return;

	pmeg = sp->sg_pmeg;

#ifdef DEBUG
	if (pmeg == seginval)
		panic("pmap_rmk: not loaded");
	if (pm->pm_ctx == NULL)
		panic("pmap_rmk: lost context");
#endif

	setcontext(0);
	/* decide how to flush cache */
	npg = (endva - va) >> PGSHIFT;
	if (npg > PMAP_RMK_MAGIC) {
		/* flush the whole segment */
		perpage = 0;
		if (vactype != VAC_NONE)
			cache_flush_segment(vr, vs);
	} else {
		/* flush each page individually; some never need flushing */
		perpage = (vactype != VAC_NONE);
	}
	while (va < endva) {
		tpte = getpte4(va);
		if ((tpte & PG_V) == 0) {
			va += NBPG;
			continue;
		}
		if ((tpte & PG_TYPE) == PG_OBMEM) {
			/* if cacheable, flush page as needed */
			if (perpage && (tpte & PG_NC) == 0)
				cache_flush_page(va);
			i = ptoa(tpte & PG_PFNUM);
			if (managed(i)) {
				pv = pvhead(i);
				pv->pv_flags |= MR4_4C(tpte);
				pv_unlink4_4c(pv, pm, va);
			}
		}
		nleft--;
		setpte4(va, 0);
		va += NBPG;
	}

	/*
	 * If the segment is all gone, remove it from everyone and
	 * free the MMU entry.
	 */
	if ((sp->sg_npte = nleft) == 0) {
		va = VSTOVA(vr,vs);		/* retract */
#ifdef MMU_3L
		if (mmu_3l)
			setsegmap(va, seginval);
		else
#endif
			for (i = ncontext; --i >= 0;) {
				setcontext(i);
				setsegmap(va, seginval);
			}
		me_free(pm, pmeg);
		if (--rp->rg_nsegmap == 0) {
#ifdef MMU_3L
			if (mmu_3l) {
				for (i = ncontext; --i >= 0;) {
					setcontext(i);
					setregmap(va, reginval);
				}
				/* note: context is 0 */
				region_free(pm, rp->rg_smeg);
			}
#endif
		}
	}
}

#endif /* sun4, sun4c */

#if defined(SUN4M)		/* 4M version of pmap_rmk */
/* remove from kernel (4m)*/
/*static*/ void
pmap_rmk4m(pm, va, endva, vr, vs)
	register struct pmap *pm;
	register vm_offset_t va, endva;
	register int vr, vs;
{
	register int i, tpte, perpage, npg;
	register struct pvlist *pv;
	register int nleft;
	struct regmap *rp;
	struct segmap *sp;

	rp = &pm->pm_regmap[vr];
	sp = &rp->rg_segmap[vs];

	if (rp->rg_nsegmap == 0)
		return;

#ifdef DEBUG
	if (rp->rg_segmap == NULL)
		panic("pmap_rmk: no segments");
#endif

	if ((nleft = sp->sg_npte) == 0)
		return;

#ifdef DEBUG
	if (sp->sg_pte == NULL || rp->rg_seg_ptps == NULL)
		panic("pmap_rmk: segment/region does not exist");
	if (pm->pm_ctx == NULL)
		panic("pmap_rmk: lost context");
#endif

	setcontext(0);
	/* decide how to flush cache */
	npg = (endva - va) >> PGSHIFT;
	if (npg > PMAP_RMK_MAGIC) {
		/* flush the whole segment */
		perpage = 0;
		if (vactype != VAC_NONE)
			cache_flush_segment(vr, vs);
	} else {
		/* flush each page individually; some never need flushing */
		perpage = (vactype != VAC_NONE);
	}
	while (va < endva) {
		tpte = getpte4m(va);
		if ((tpte & SRMMU_TETYPE) != SRMMU_TEPTE) {
			va += NBPG;
			continue;
		}
		if ((tpte & SRMMU_PGTYPE) == PG_SUN4M_OBMEM) {
			/* if cacheable, flush page as needed */
			if (perpage && (tpte & SRMMU_PG_C))
			    cache_flush_page(va);
			i = ptoa((tpte & SRMMU_PPNMASK) >> SRMMU_PPNSHIFT);
			if (managed(i)) {
				pv = pvhead(i);
				pv->pv_flags |= MR4M(tpte);
				pv_unlink4m(pv, pm, va);
			}
		}
		nleft--;
		setpte4m(va, SRMMU_TEINVALID);
		va += NBPG;
	}

	/*
	 * If the segment is all gone, remove it from everyone and
	 * flush the TLB.
	 */
	if ((sp->sg_npte = nleft) == 0) {
		va = VSTOVA(vr,vs);		/* retract */

		tlb_flush_segment(vr, vs); 	/* Paranoia? */

		/*
		 * We need to free the segment table. The problem is that
		 * we can't free the initial (bootstrap) mapping, so
		 * we have to explicitly check for this case (ugh).
		 */
		if (va < virtual_avail) {
#ifdef DEBUG
			printf("pmap_rmk4m: attempt to free base kernel alloc\n");
#endif
			/* sp->sg_pte = NULL; */
			sp->sg_npte = 0;
			return;
		}
		/* no need to free the table; it is statically allocated */
		qzero(sp->sg_pte, SRMMU_L3SIZE * sizeof(long));
	}
	/* if we're done with a region, leave it wired */
}
#endif /* sun4m */
/*
 * Just like pmap_rmk_magic, but we have a different threshold.
 * Note that this may well deserve further tuning work.
 */
#if 0
#define	PMAP_RMU_MAGIC	(cacheinfo.c_hwflush?4:64)	/* if > magic, use cache_flush_segment */
#else
#define	PMAP_RMU_MAGIC	4	/* if > magic, use cache_flush_segment */
#endif

#if defined(SUN4) || defined(SUN4C)

/* remove from user */
/*static*/ void
pmap_rmu4_4c(pm, va, endva, vr, vs)
	register struct pmap *pm;
	register vm_offset_t va, endva;
	register int vr, vs;
{
	register int *pte0, i, pteva, tpte, perpage, npg;
	register struct pvlist *pv;
	register int nleft, pmeg;
	struct regmap *rp;
	struct segmap *sp;

	rp = &pm->pm_regmap[vr];
	if (rp->rg_nsegmap == 0)
		return;
	if (rp->rg_segmap == NULL)
		panic("pmap_rmu: no segments");

	sp = &rp->rg_segmap[vs];
	if ((nleft = sp->sg_npte) == 0)
		return;
	if (sp->sg_pte == NULL)
		panic("pmap_rmu: no pages");


	pmeg = sp->sg_pmeg;
	pte0 = sp->sg_pte;

	if (pmeg == seginval) {
		register int *pte = pte0 + VA_VPG(va);

		/*
		 * PTEs are not in MMU.  Just invalidate software copies.
		 */
		for (; va < endva; pte++, va += NBPG) {
			tpte = *pte;
			if ((tpte & PG_V) == 0) {
				/* nothing to remove (braindead VM layer) */
				continue;
			}
			if ((tpte & PG_TYPE) == PG_OBMEM) {
				i = ptoa(tpte & PG_PFNUM);
				if (managed(i))
					pv_unlink4_4c(pvhead(i), pm, va);
			}
			nleft--;
			*pte = 0;
		}
		if ((sp->sg_npte = nleft) == 0) {
			free(pte0, M_VMPMAP);
			sp->sg_pte = NULL;
			if (--rp->rg_nsegmap == 0) {
				free(rp->rg_segmap, M_VMPMAP);
				rp->rg_segmap = NULL;
#ifdef MMU_3L
				if (mmu_3l && rp->rg_smeg != reginval) {
					if (pm->pm_ctx) {
						setcontext(pm->pm_ctxnum);
						setregmap(va, reginval);
					} else
						setcontext(0);
					region_free(pm, rp->rg_smeg);
				}
#endif
			}
		}
		return;
	}

	/*
	 * PTEs are in MMU.  Invalidate in hardware, update ref &
	 * mod bits, and flush cache if required.
	 */
	if (CTX_USABLE(pm,rp)) {
		/* process has a context, must flush cache */
		npg = (endva - va) >> PGSHIFT;
		setcontext(pm->pm_ctxnum);
		if (npg > PMAP_RMU_MAGIC) {
			perpage = 0; /* flush the whole segment */
			if (vactype != VAC_NONE)
				cache_flush_segment(vr, vs);
		} else
			perpage = (vactype != VAC_NONE);
		pteva = va;
	} else {
		/* no context, use context 0; cache flush unnecessary */
		setcontext(0);
#ifdef MMU_3L
		if (mmu_3l)
			setregmap(0, tregion);
#endif
		/* XXX use per-cpu pteva? */
		setsegmap(0, pmeg);
		pteva = VA_VPG(va) << PGSHIFT;
		perpage = 0;
	}
	for (; va < endva; pteva += NBPG, va += NBPG) {
		tpte = getpte4(pteva);
		if ((tpte & PG_V) == 0)
			continue;
		if ((tpte & PG_TYPE) == PG_OBMEM) {
			/* if cacheable, flush page as needed */
			if (perpage && (tpte & PG_NC) == 0)
				cache_flush_page(va);
			i = ptoa(tpte & PG_PFNUM);
			if (managed(i)) {
				pv = pvhead(i);
				pv->pv_flags |= MR4_4C(tpte);
				pv_unlink4_4c(pv, pm, va);
			}
		}
		nleft--;
		setpte4(pteva, 0);
#define PMAP_PTESYNC
#ifdef PMAP_PTESYNC
		pte0[VA_VPG(pteva)] = 0;
#endif
	}

	/*
	 * If the segment is all gone, and the context is loaded, give
	 * the segment back.
	 */
	if ((sp->sg_npte = nleft) == 0 /* ??? && pm->pm_ctx != NULL*/) {
#ifdef DEBUG
if (pm->pm_ctx == NULL) {
	printf("pmap_rmu: no context here...");
}
#endif
		va = VSTOVA(vr,vs);		/* retract */
		if (CTX_USABLE(pm,rp))
			setsegmap(va, seginval);
#ifdef MMU_3L
		else if (mmu_3l && rp->rg_smeg != reginval) {
			/* note: context already set earlier */
			setregmap(0, rp->rg_smeg);
			setsegmap(vs << SGSHIFT, seginval);
		}
#endif
		free(pte0, M_VMPMAP);
		sp->sg_pte = NULL;
		me_free(pm, pmeg);

		if (--rp->rg_nsegmap == 0) {
			free(rp->rg_segmap, M_VMPMAP);
			rp->rg_segmap = NULL;
			GAP_WIDEN(pm,vr);

#ifdef MMU_3L
			if (mmu_3l && rp->rg_smeg != reginval) {
				/* note: context already set */
				if (pm->pm_ctx)
					setregmap(va, reginval);
				region_free(pm, rp->rg_smeg);
			}
#endif
		}

	}
}

#endif /* sun4,4c */

#if defined(SUN4M)		/* 4M version of pmap_rmu */
/* remove from user */
/*static*/ void
pmap_rmu4m(pm, va, endva, vr, vs)
	register struct pmap *pm;
	register vm_offset_t va, endva;
	register int vr, vs;
{
	register int *pte0, i, tpte, perpage, npg;
	register struct pvlist *pv;
	register int nleft;
	struct regmap *rp;
	struct segmap *sp;

	rp = &pm->pm_regmap[vr];
	if (rp->rg_nsegmap == 0)
		return;
	if (rp->rg_segmap == NULL)
		panic("pmap_rmu: no segments");

	sp = &rp->rg_segmap[vs];
	if ((nleft = sp->sg_npte) == 0)
		return;
	if (sp->sg_pte == NULL)
		panic("pmap_rmu: no pages");

	pte0 = sp->sg_pte;

	/*
	 * Invalidate PTE in MMU pagetables. Flush cache if necessary.
	 */
	if (CTX_USABLE(pm,rp)) {
		/* process has a context, must flush cache */
		setcontext(pm->pm_ctxnum);
		if (vactype != VAC_NONE) {
			npg = (endva - va) >> PGSHIFT;
			if (npg > PMAP_RMU_MAGIC) {
				perpage = 0; /* flush the whole segment */
				cache_flush_segment(vr, vs);
			} else
				perpage = 1;
		} else
			perpage = 0;
	} else {
		/* no context; cache flush unnecessary */
		perpage = 0;
	}
	for (; va < endva; va += NBPG) {
		/* Note: we use sw pagetables here since pages have been
		 * flushed already. This avoids over-zealous cache flushing.
		 */
		if (CTX_USABLE(pm,rp)) {    /* %%% XXX: Performance hit? */
			tpte = getpte4m(va); /* should we just flush seg? */
			tlb_flush_page(va);
		} else
			tpte = getptesw4m(pm, va);
		if ((tpte & SRMMU_TETYPE) != SRMMU_TEPTE)
			continue;
		if ((tpte & SRMMU_PGTYPE) == PG_SUN4M_OBMEM) {
			/* if cacheable, flush page as needed */
			if (perpage && (tpte & SRMMU_PG_C))
				cache_flush_page(va);
			i = ptoa((tpte & SRMMU_PPNMASK) >> SRMMU_PPNSHIFT);
			if (managed(i)) {
				pv = pvhead(i);
				pv->pv_flags |= MR4M(tpte);
				pv_unlink4m(pv, pm, va);
			}
		}
		nleft--;
		setptesw4m(pm, va, SRMMU_TEINVALID);	/* Update pagetables */
	}

	/*
	 * If the segment is all gone, and the context is loaded, give
	 * the segment back.
	 */
	if ((sp->sg_npte = nleft) == 0 /* ??? && pm->pm_ctx != NULL*/) {
#ifdef DEBUG
		if (pm->pm_ctx == NULL) {
			printf("pmap_rmu: no context here...");
		}
#endif
		va = VSTOVA(vr,vs);		/* retract */

		tlb_flush_segment(vr, vs); 	/* Paranoia? */
		rp->rg_seg_ptps[vs] = SRMMU_TEINVALID;
		free(pte0, M_VMPMAP);
		sp->sg_pte = NULL;

		if (--rp->rg_nsegmap == 0) {
			free(rp->rg_segmap, M_VMPMAP);
			rp->rg_segmap = NULL;
			free(rp->rg_seg_ptps, M_VMPMAP);
			pm->pm_reg_ptps[vr] = SRMMU_TEINVALID;
			GAP_WIDEN(pm,vr);
		}
	}
}
#endif /* sun4m */

/*
 * Lower (make more strict) the protection on the specified
 * physical page.
 *
 * There are only two cases: either the protection is going to 0
 * (in which case we do the dirty work here), or it is going from
 * to read-only (in which case pv_changepte does the trick).
 */

#if defined(SUN4) || defined(SUN4C)
void
pmap_page_protect4_4c(pa, prot)
	vm_offset_t pa;
	vm_prot_t prot;
{
	register struct pvlist *pv, *pv0, *npv;
	register struct pmap *pm;
	register vm_offset_t va;
	register int vr, vs, pteva, tpte;
	register int flags, nleft, i, s, ctx;
	struct regmap *rp;
	struct segmap *sp;

#ifdef DEBUG
	if (!pmap_pa_exists(pa))
		panic("pmap_page_protect: no such address: %lx", pa);
	if ((pmapdebug & PDB_CHANGEPROT) ||
	    (pmapdebug & PDB_REMOVE && prot == VM_PROT_NONE))
		printf("pmap_page_protect(%lx, %x)\n", pa, prot);
#endif
	/*
	 * Skip unmanaged pages, or operations that do not take
	 * away write permission.
	 */
	if ((pa & (PMAP_TNC & ~PMAP_NC)) ||
	     !managed(pa) || prot & VM_PROT_WRITE)
		return;
	write_user_windows();	/* paranoia */
	if (prot & VM_PROT_READ) {
		pv_changepte4_4c(pvhead(pa), 0, PG_W);
		return;
	}

	/*
	 * Remove all access to all people talking to this page.
	 * Walk down PV list, removing all mappings.
	 * The logic is much like that for pmap_remove,
	 * but we know we are removing exactly one page.
	 */
	pv = pvhead(pa);
	s = splpmap();
	if ((pm = pv->pv_pmap) == NULL) {
		splx(s);
		return;
	}
	ctx = getcontext();
	pv0 = pv;
	flags = pv->pv_flags & ~PV_NC;
	for (;; pm = pv->pv_pmap) {
		va = pv->pv_va;
		vr = VA_VREG(va);
		vs = VA_VSEG(va);
		rp = &pm->pm_regmap[vr];
		if (rp->rg_nsegmap == 0)
			panic("pmap_remove_all: empty vreg");
		sp = &rp->rg_segmap[vs];
		if ((nleft = sp->sg_npte) == 0)
			panic("pmap_remove_all: empty vseg");
		nleft--;
		sp->sg_npte = nleft;

		if (sp->sg_pmeg == seginval) {
			/* Definitely not a kernel map */
			if (nleft) {
				sp->sg_pte[VA_VPG(va)] = 0;
			} else {
				free(sp->sg_pte, M_VMPMAP);
				sp->sg_pte = NULL;
				if (--rp->rg_nsegmap == 0) {
					free(rp->rg_segmap, M_VMPMAP);
					rp->rg_segmap = NULL;
					GAP_WIDEN(pm,vr);
#ifdef MMU_3L
					if (mmu_3l && rp->rg_smeg != reginval) {
						if (pm->pm_ctx) {
							setcontext(pm->pm_ctxnum);
							setregmap(va, reginval);
						} else
							setcontext(0);
						region_free(pm, rp->rg_smeg);
					}
#endif
				}
			}
			goto nextpv;
		}
		if (CTX_USABLE(pm,rp)) {
			setcontext(pm->pm_ctxnum);
			pteva = va;
			if (vactype != VAC_NONE)
				cache_flush_page(va);
		} else {
			setcontext(0);
			/* XXX use per-cpu pteva? */
#ifdef MMU_3L
			if (mmu_3l)
				setregmap(0, tregion);
#endif
			setsegmap(0, sp->sg_pmeg);
			pteva = VA_VPG(va) << PGSHIFT;
		}

		tpte = getpte4(pteva);
		if ((tpte & PG_V) == 0)
			panic("pmap_page_protect !PG_V");
		flags |= MR4_4C(tpte);

		if (nleft) {
			setpte4(pteva, 0);
#ifdef PMAP_PTESYNC
			if (sp->sg_pte != NULL)
				sp->sg_pte[VA_VPG(pteva)] = 0;
#endif
		} else {
			if (pm == pmap_kernel()) {
#ifdef MMU_3L
				if (!mmu_3l)
#endif
					for (i = ncontext; --i >= 0;) {
						setcontext(i);
						setsegmap(va, seginval);
					}
				me_free(pm, sp->sg_pmeg);
				if (--rp->rg_nsegmap == 0) {
#ifdef MMU_3L
					if (mmu_3l) {
						for (i = ncontext; --i >= 0;) {
							setcontext(i);
							setregmap(va, reginval);
						}
						region_free(pm, rp->rg_smeg);
					}
#endif
				}
			} else {
				if (CTX_USABLE(pm,rp))
					/* `pteva'; we might be using tregion */
					setsegmap(pteva, seginval);
#ifdef MMU_3L
				else if (mmu_3l && rp->rg_smeg != reginval) {
					/* note: context already set earlier */
					setregmap(0, rp->rg_smeg);
					setsegmap(vs << SGSHIFT, seginval);
				}
#endif
				free(sp->sg_pte, M_VMPMAP);
				sp->sg_pte = NULL;
				me_free(pm, sp->sg_pmeg);

				if (--rp->rg_nsegmap == 0) {
#ifdef MMU_3L
					if (mmu_3l && rp->rg_smeg != reginval) {
						if (pm->pm_ctx)
							setregmap(va, reginval);
						region_free(pm, rp->rg_smeg);
					}
#endif
					free(rp->rg_segmap, M_VMPMAP);
					rp->rg_segmap = NULL;
					GAP_WIDEN(pm,vr);
				}
			}
		}
	nextpv:
		npv = pv->pv_next;
		if (pv != pv0)
			free(pv, M_VMPVENT);
		if ((pv = npv) == NULL)
			break;
	}
	pv0->pv_pmap = NULL;
	pv0->pv_next = NULL; /* ? */
	pv0->pv_flags = flags;
	setcontext(ctx);
	splx(s);
}

/*
 * Lower (make more strict) the protection on the specified
 * range of this pmap.
 *
 * There are only two cases: either the protection is going to 0
 * (in which case we call pmap_remove to do the dirty work), or
 * it is going from read/write to read-only.  The latter is
 * fairly easy.
 */
void
pmap_protect4_4c(pm, sva, eva, prot)
	register struct pmap *pm;
	vm_offset_t sva, eva;
	vm_prot_t prot;
{
	register vm_offset_t va, nva;
	register int vr, vs;
	register int s, ctx;
	struct regmap *rp;
	struct segmap *sp;

	if (pm == NULL || prot & VM_PROT_WRITE)
		return;

	if ((prot & VM_PROT_READ) == 0) {
		pmap_remove(pm, sva, eva);
		return;
	}

	write_user_windows();
	ctx = getcontext();
	s = splpmap();
	simple_lock(&pm->pm_lock);

	for (va = sva; va < eva;) {
		vr = VA_VREG(va);
		vs = VA_VSEG(va);
		rp = &pm->pm_regmap[vr];
		nva = VSTOVA(vr,vs + 1);
if (nva == 0) panic("pmap_protect: last segment");	/* cannot happen */
		if (nva > eva)
			nva = eva;
		if (rp->rg_nsegmap == 0) {
			va = nva;
			continue;
		}
#ifdef DEBUG
		if (rp->rg_segmap == NULL)
			panic("pmap_protect: no segments");
#endif
		sp = &rp->rg_segmap[vs];
		if (sp->sg_npte == 0) {
			va = nva;
			continue;
		}
#ifdef DEBUG
		if (pm != pmap_kernel() && sp->sg_pte == NULL)
			panic("pmap_protect: no pages");
#endif
		if (sp->sg_pmeg == seginval) {
			register int *pte = &sp->sg_pte[VA_VPG(va)];

			/* not in MMU; just clear PG_W from core copies */
			for (; va < nva; va += NBPG)
				*pte++ &= ~PG_W;
		} else {
			/* in MMU: take away write bits from MMU PTEs */
			if (CTX_USABLE(pm,rp)) {
				register int tpte;

				/*
				 * Flush cache so that any existing cache
				 * tags are updated.  This is really only
				 * needed for PTEs that lose PG_W.
				 */
				setcontext(pm->pm_ctxnum);
				for (; va < nva; va += NBPG) {
					tpte = getpte4(va);
					pmap_stats.ps_npg_prot_all++;
					if ((tpte & (PG_W|PG_TYPE)) ==
					    (PG_W|PG_OBMEM)) {
						pmap_stats.ps_npg_prot_actual++;
						if (vactype != VAC_NONE)
							cache_flush_page(va);
						setpte4(va, tpte & ~PG_W);
					}
				}
			} else {
				register int pteva;

				/*
				 * No context, hence not cached;
				 * just update PTEs.
				 */
				setcontext(0);
				/* XXX use per-cpu pteva? */
#ifdef MMU_3L
				if (mmu_3l)
					setregmap(0, tregion);
#endif
				setsegmap(0, sp->sg_pmeg);
				pteva = VA_VPG(va) << PGSHIFT;
				for (; va < nva; pteva += NBPG, va += NBPG)
					setpte4(pteva, getpte4(pteva) & ~PG_W);
			}
		}
	}
	simple_unlock(&pm->pm_lock);
	splx(s);
	setcontext(ctx);
}

/*
 * Change the protection and/or wired status of the given (MI) virtual page.
 * XXX: should have separate function (or flag) telling whether only wiring
 * is changing.
 */
void
pmap_changeprot4_4c(pm, va, prot, wired)
	register struct pmap *pm;
	register vm_offset_t va;
	vm_prot_t prot;
	int wired;
{
	register int vr, vs, tpte, newprot, ctx, s;
	struct regmap *rp;
	struct segmap *sp;

#ifdef DEBUG
	if (pmapdebug & PDB_CHANGEPROT)
		printf("pmap_changeprot(%p, %lx, %x, %x)\n",
		    pm, va, prot, wired);
#endif

	write_user_windows();	/* paranoia */

	va &= ~(NBPG-1);
	if (pm == pmap_kernel())
		newprot = prot & VM_PROT_WRITE ? PG_S|PG_W : PG_S;
	else
		newprot = prot & VM_PROT_WRITE ? PG_W : 0;
	vr = VA_VREG(va);
	vs = VA_VSEG(va);
	s = splpmap();		/* conservative */
	rp = &pm->pm_regmap[vr];
	if (rp->rg_nsegmap == 0) {
		printf("pmap_changeprot: no segments in %d\n", vr);
		return;
	}
	if (rp->rg_segmap == NULL) {
		printf("pmap_changeprot: no segments in %d!\n", vr);
		return;
	}
	sp = &rp->rg_segmap[vs];

	pmap_stats.ps_changeprots++;

#ifdef DEBUG
	if (pm != pmap_kernel() && sp->sg_pte == NULL)
		panic("pmap_changeprot: no pages");
#endif

	/* update PTEs in software or hardware */
	if (sp->sg_pmeg == seginval) {
		register int *pte = &sp->sg_pte[VA_VPG(va)];

		/* update in software */
		if ((*pte & PG_PROT) == newprot)
			goto useless;
		*pte = (*pte & ~PG_PROT) | newprot;
	} else {
		/* update in hardware */
		ctx = getcontext();
		if (CTX_USABLE(pm,rp)) {
			/* use current context; flush writeback cache */
			setcontext(pm->pm_ctxnum);
			tpte = getpte4(va);
			if ((tpte & PG_PROT) == newprot) {
				setcontext(ctx);
				goto useless;
			}

			/*
			 * the latter check deals with a writethrough cache
			 * problem on the 4/300
			 */
			if ((vactype==VAC_WRITEBACK ||
			    (vactype==VAC_WRITETHROUGH && cputyp==CPU_SUN4)) &&
			    (tpte & (PG_U|PG_NC|PG_TYPE)) == (PG_U|PG_OBMEM))
				cache_flush_page((int)va);
		} else {
			setcontext(0);
			/* XXX use per-cpu va? */
#ifdef MMU_3L
			if (mmu_3l)
				setregmap(0, tregion);
#endif
			setsegmap(0, sp->sg_pmeg);
			va = VA_VPG(va) << PGSHIFT;
			tpte = getpte4(va);
			if ((tpte & PG_PROT) == newprot) {
				setcontext(ctx);
				goto useless;
			}
		}
		tpte = (tpte & ~PG_PROT) | newprot;
		setpte4(va, tpte);
		setcontext(ctx);
	}
	splx(s);
	return;

useless:
	/* only wiring changed, and we ignore wiring */
	pmap_stats.ps_useless_changeprots++;
	splx(s);
}

#endif /* sun4, 4c */

#if defined(SUN4M)		/* 4M version of protection routines above */
/*
 * Lower (make more strict) the protection on the specified
 * physical page.
 *
 * There are only two cases: either the protection is going to 0
 * (in which case we do the dirty work here), or it is going
 * to read-only (in which case pv_changepte does the trick).
 */
void
pmap_page_protect4m(pa, prot)
	vm_offset_t pa;
	vm_prot_t prot;
{
	register struct pvlist *pv, *pv0, *npv;
	register struct pmap *pm;
	register vm_offset_t va;
	register int vr, vs, tpte;
	register int flags, nleft, s, ctx;
	struct regmap *rp;
	struct segmap *sp;

#ifdef DEBUG
	if (!pmap_pa_exists(pa))
		panic("pmap_page_protect: no such address: 0x%lx", pa);
	if ((pmapdebug & PDB_CHANGEPROT) ||
	    (pmapdebug & PDB_REMOVE && prot == VM_PROT_NONE))
		printf("pmap_page_protect(%lx, %x)\n", pa, prot);
#endif
	/*
	 * Skip unmanaged pages, or operations that do not take
	 * away write permission.
	 */
	if (!managed(pa) || prot & VM_PROT_WRITE)
		return;
	write_user_windows();	/* paranoia */
	if (prot & VM_PROT_READ) {
		pv_changepte4m(pvhead(pa), 0, PPROT_WRITE);
		return;
	}

	/*
	 * Remove all access to all people talking to this page.
	 * Walk down PV list, removing all mappings.
	 * The logic is much like that for pmap_remove,
	 * but we know we are removing exactly one page.
	 */
	pv = pvhead(pa);
	s = splpmap();
	if ((pm = pv->pv_pmap) == NULL) {
		splx(s);
		return;
	}
	ctx = getcontext();
	pv0 = pv;
	flags = pv->pv_flags /*| PV_C4M*/;	/* %%%: ???? */
	for (;; pm = pv->pv_pmap) {
		va = pv->pv_va;
		vr = VA_VREG(va);
		vs = VA_VSEG(va);
		rp = &pm->pm_regmap[vr];
		if (rp->rg_nsegmap == 0)
			panic("pmap_remove_all: empty vreg");
		sp = &rp->rg_segmap[vs];
		if ((nleft = sp->sg_npte) == 0)
			panic("pmap_remove_all: empty vseg");
		nleft--;
		sp->sg_npte = nleft;

		/* Invalidate PTE in MMU pagetables. Flush cache if necessary */
		if (CTX_USABLE(pm,rp)) { 	/* Must flush */
			setcontext(pm->pm_ctxnum);
			tpte = getpte4m(va);
			if (vactype != VAC_NONE)
				cache_flush_page(va);
			tlb_flush_page(va);
		} else
			tpte = getptesw4m(pm, va);

		if ((tpte & SRMMU_TETYPE) != SRMMU_TEPTE)
			panic("pmap_page_protect !PG_V");
		flags |= MR4M(tpte);

		if (nleft)
			setptesw4m(pm, va, SRMMU_TEINVALID);
		else {
			if (pm == pmap_kernel()) {
				tlb_flush_segment(vr, vs); /* Paranoid? */
				if (va < virtual_avail) {
#ifdef DEBUG
					printf("pmap_rmk4m: attempt to free "
					       "base kernel allocation\n");
#endif
					goto nextpv;
				}
				/* no need to free the table; it is static */
				qzero(sp->sg_pte, SRMMU_L3SIZE * sizeof(int));

				/* if we're done with a region, leave it */

			} else { 	/* User mode mapping */
				if (CTX_USABLE(pm,rp))
					tlb_flush_segment(vr, vs);
				rp->rg_seg_ptps[vs] = SRMMU_TEINVALID;
				free(sp->sg_pte, M_VMPMAP);
				sp->sg_pte = NULL;

				if (--rp->rg_nsegmap == 0) {
					free(rp->rg_segmap, M_VMPMAP);
					rp->rg_segmap = NULL;
					free(rp->rg_seg_ptps, M_VMPMAP);
					pm->pm_reg_ptps[vr] = SRMMU_TEINVALID;
					GAP_WIDEN(pm,vr);
				}
			}
		}
	nextpv:
		npv = pv->pv_next;
		if (pv != pv0)
			free(pv, M_VMPVENT);
		if ((pv = npv) == NULL)
			break;
	}
	pv0->pv_pmap = NULL;
	pv0->pv_next = NULL; /* ? */
	pv0->pv_flags = flags;
	setcontext(ctx);
	splx(s);
}

/*
 * Lower (make more strict) the protection on the specified
 * range of this pmap.
 *
 * There are only two cases: either the protection is going to 0
 * (in which case we call pmap_remove to do the dirty work), or
 * it is going from read/write to read-only.  The latter is
 * fairly easy.
 */
void
pmap_protect4m(pm, sva, eva, prot)
	register struct pmap *pm;
	vm_offset_t sva, eva;
	vm_prot_t prot;
{
	register vm_offset_t va, nva;
	register int vr, vs;
	register int s, ctx;
	struct regmap *rp;
	struct segmap *sp;

	if (pm == NULL || prot & VM_PROT_WRITE)
		return;

	if ((prot & VM_PROT_READ) == 0) {
		pmap_remove(pm, sva, eva);
		return;
	}

	write_user_windows();
	ctx = getcontext();
	s = splpmap();
	simple_lock(&pm->pm_lock);

	for (va = sva; va < eva;) {
		vr = VA_VREG(va);
		vs = VA_VSEG(va);
		rp = &pm->pm_regmap[vr];
		nva = VSTOVA(vr,vs + 1);
		if (nva == 0)	/* XXX */
			panic("pmap_protect: last segment"); /* cannot happen(why?)*/
		if (nva > eva)
			nva = eva;
		if (rp->rg_nsegmap == 0) {
			va = nva;
			continue;
		}
#ifdef DEBUG
		if (rp->rg_segmap == NULL)
			panic("pmap_protect: no segments");
#endif
		sp = &rp->rg_segmap[vs];
		if (sp->sg_npte == 0) {
			va = nva;
			continue;
		}
#ifdef DEBUG
		if (sp->sg_pte == NULL)
			panic("pmap_protect: no pages");
#endif
		/* in MMU: take away write bits from MMU PTEs */
		if (CTX_USABLE(pm,rp)) {
			/*
			 * Flush cache so that any existing cache
			 * tags are updated.  This is really only
			 * needed for PTEs that lose PG_W.
			 */
			setcontext(pm->pm_ctxnum);
			for (; va < nva; va += NBPG) {
				register int tpte = getpte4m(va);
				pmap_stats.ps_npg_prot_all++;
				if ((tpte & (PPROT_WRITE|SRMMU_PGTYPE)) ==
				    (PPROT_WRITE|PG_SUN4M_OBMEM)) {
					pmap_stats.ps_npg_prot_actual++;
					if (vactype != VAC_NONE)
						cache_flush_page(va);
					setpte4m(va, tpte & ~PPROT_WRITE);
				}
			}
		} else {
			/*
			 * No context, hence not cached;
			 * just update PTEs.
			 */
			setcontext(0);
			for (; va < nva; va += NBPG) {
				register int tpte = getptesw4m(pm, va);
				pmap_stats.ps_npg_prot_all++;
				if ((tpte & (PPROT_WRITE)))
					pmap_stats.ps_npg_prot_actual++;
				setptesw4m(pm, va, tpte & ~PPROT_WRITE);
			}
		}
	}
	simple_unlock(&pm->pm_lock);
	splx(s);
	setcontext(ctx);
}

/*
 * Change the protection and/or wired status of the given (MI) virtual page.
 * XXX: should have separate function (or flag) telling whether only wiring
 * is changing.
 */
void
pmap_changeprot4m(pm, va, prot, wired)
	register struct pmap *pm;
	register vm_offset_t va;
	vm_prot_t prot;
	int wired;
{
	register int tpte, newprot, ctx, s;

#ifdef DEBUG
	if (pmapdebug & PDB_CHANGEPROT)
		printf("pmap_changeprot(%p, %lx, %x, %x)\n",
		    pm, va, prot, wired);
#endif

	write_user_windows();	/* paranoia */

	va &= ~(NBPG-1);
	if (pm == pmap_kernel())
		newprot = prot & VM_PROT_WRITE ? PPROT_N_RWX : PPROT_N_RX;
	else
		newprot = prot & VM_PROT_WRITE ? PPROT_RWX_RWX : PPROT_RX_RX;

	pmap_stats.ps_changeprots++;

	s = splpmap();		/* conservative */
	ctx = getcontext();
	if (pm->pm_ctx) {
		setcontext(pm->pm_ctxnum);
		tpte = getpte4m(va);
		if (vactype == VAC_WRITEBACK &&
		    (tpte & SRMMU_PGTYPE) == PG_SUN4M_OBMEM)
			cache_flush_page(va); /* XXX: paranoia? */
	} else {
		tpte = getptesw4m(pm, va);
	}
	if ((tpte & SRMMU_PROT_MASK) == newprot) {
		setcontext(ctx);
		goto useless;
	}
	if (pm->pm_ctx)
		setpte4m(va, (tpte & ~SRMMU_PROT_MASK) | newprot);
	else
		setptesw4m(pm, va, (tpte & ~SRMMU_PROT_MASK) | newprot);
	setcontext(ctx);
	splx(s);
	return;

useless:
	/* only wiring changed, and we ignore wiring */
	pmap_stats.ps_useless_changeprots++;
	splx(s);
}
#endif /* 4m */

/*
 * Insert (MI) physical page pa at virtual address va in the given pmap.
 * NB: the pa parameter includes type bits PMAP_OBIO, PMAP_NC as necessary.
 *
 * If pa is not in the `managed' range it will not be `bank mapped'.
 * This works during bootstrap only because the first 4MB happens to
 * map one-to-one.
 *
 * There may already be something else there, or we might just be
 * changing protections and/or wiring on an existing mapping.
 *	XXX	should have different entry points for changing!
 */

#if defined(SUN4) || defined(SUN4C)

void
pmap_enter4_4c(pm, va, pa, prot, wired)
	register struct pmap *pm;
	vm_offset_t va, pa;
	vm_prot_t prot;
	int wired;
{
	register struct pvlist *pv;
	register int pteproto, ctx;

	if (pm == NULL)
		return;

	if (VA_INHOLE(va)) {
#ifdef DEBUG
		printf("pmap_enter: pm %p, va %lx, pa %lx: in MMU hole\n",
			pm, va, pa);
#endif
		return;
	}

#ifdef DEBUG
	if (pmapdebug & PDB_ENTER)
		printf("pmap_enter(%p, %lx, %lx, %x, %x)\n",
		    pm, va, pa, prot, wired);
#endif

	pteproto = PG_V | ((pa & PMAP_TNC) << PG_TNC_SHIFT);
	pa &= ~PMAP_TNC;
	/*
	 * Set up prototype for new PTE.  Cannot set PG_NC from PV_NC yet
	 * since the pvlist no-cache bit might change as a result of the
	 * new mapping.
	 */
	if ((pteproto & PG_TYPE) == PG_OBMEM && managed(pa)) {
#ifdef DIAGNOSTIC
		if (!pmap_pa_exists(pa))
			panic("pmap_enter: no such address: %lx", pa);
#endif
		pv = pvhead(pa);
	} else {
		pv = NULL;
	}
	pteproto |= atop(pa) & PG_PFNUM;
	if (prot & VM_PROT_WRITE)
		pteproto |= PG_W;

	ctx = getcontext();
	if (pm == pmap_kernel())
		pmap_enk4_4c(pm, va, prot, wired, pv, pteproto | PG_S);
	else
		pmap_enu4_4c(pm, va, prot, wired, pv, pteproto);
	setcontext(ctx);
}

/* enter new (or change existing) kernel mapping */
void
pmap_enk4_4c(pm, va, prot, wired, pv, pteproto)
	register struct pmap *pm;
	vm_offset_t va;
	vm_prot_t prot;
	int wired;
	register struct pvlist *pv;
	register int pteproto;
{
	register int vr, vs, tpte, i, s;
	struct regmap *rp;
	struct segmap *sp;

	vr = VA_VREG(va);
	vs = VA_VSEG(va);
	rp = &pm->pm_regmap[vr];
	sp = &rp->rg_segmap[vs];
	s = splpmap();		/* XXX way too conservative */

#ifdef MMU_3L
	if (mmu_3l && rp->rg_smeg == reginval) {
		vm_offset_t tva;
		rp->rg_smeg = region_alloc(&region_locked, pm, vr)->me_cookie;
		i = ncontext - 1;
		do {
			setcontext(i);
			setregmap(va, rp->rg_smeg);
		} while (--i >= 0);

		/* set all PTEs to invalid, then overwrite one PTE below */
		tva = VA_ROUNDDOWNTOREG(va);
		for (i = 0; i < NSEGRG; i++) {
			setsegmap(tva, rp->rg_segmap[i].sg_pmeg);
			tva += NBPSG;
		};
	}
#endif
	if (sp->sg_pmeg != seginval && (tpte = getpte4(va)) & PG_V) {
		register int addr;

		/* old mapping exists, and is of the same pa type */
		if ((tpte & (PG_PFNUM|PG_TYPE)) ==
		    (pteproto & (PG_PFNUM|PG_TYPE))) {
			/* just changing protection and/or wiring */
			splx(s);
			pmap_changeprot(pm, va, prot, wired);
			return;
		}

		if ((tpte & PG_TYPE) == PG_OBMEM) {
#ifdef DEBUG
printf("pmap_enk: changing existing va=>pa entry: va %lx, pteproto %x\n",
	va, pteproto);
#endif
			/*
			 * Switcheroo: changing pa for this va.
			 * If old pa was managed, remove from pvlist.
			 * If old page was cached, flush cache.
			 */
			addr = ptoa(tpte & PG_PFNUM);
			if (managed(addr))
				pv_unlink4_4c(pvhead(addr), pm, va);
			if ((tpte & PG_NC) == 0) {
				setcontext(0);	/* ??? */
				if (vactype != VAC_NONE)
					cache_flush_page((int)va);
			}
		}
	} else {
		/* adding new entry */
		sp->sg_npte++;
	}

	/*
	 * If the new mapping is for a managed PA, enter into pvlist.
	 * Note that the mapping for a malloc page will always be
	 * unique (hence will never cause a second call to malloc).
	 */
	if (pv != NULL)
		pteproto |= pv_link4_4c(pv, pm, va);

	if (sp->sg_pmeg == seginval) {
		register int tva;

		/*
		 * Allocate an MMU entry now (on locked list),
		 * and map it into every context.  Set all its
		 * PTEs invalid (we will then overwrite one, but
		 * this is more efficient than looping twice).
		 */
#ifdef DEBUG
		if (pm->pm_ctx == NULL || pm->pm_ctxnum != 0)
			panic("pmap_enk: kern seg but no kern ctx");
#endif
		sp->sg_pmeg = me_alloc(&segm_locked, pm, vr, vs)->me_cookie;
		rp->rg_nsegmap++;

#ifdef MMU_3L
		if (mmu_3l)
			setsegmap(va, sp->sg_pmeg);
		else
#endif
		{
			i = ncontext - 1;
			do {
				setcontext(i);
				setsegmap(va, sp->sg_pmeg);
			} while (--i >= 0);
		}

		/* set all PTEs to invalid, then overwrite one PTE below */
		tva = VA_ROUNDDOWNTOSEG(va);
		i = NPTESG;
		do {
			setpte4(tva, 0);
			tva += NBPG;
		} while (--i > 0);
	}

	/* ptes kept in hardware only */
	setpte4(va, pteproto);
	splx(s);
}

/* enter new (or change existing) user mapping */
void
pmap_enu4_4c(pm, va, prot, wired, pv, pteproto)
	register struct pmap *pm;
	vm_offset_t va;
	vm_prot_t prot;
	int wired;
	register struct pvlist *pv;
	register int pteproto;
{
	register int vr, vs, *pte, tpte, pmeg, s, doflush;
	struct regmap *rp;
	struct segmap *sp;

	write_user_windows();		/* XXX conservative */
	vr = VA_VREG(va);
	vs = VA_VSEG(va);
	rp = &pm->pm_regmap[vr];
	s = splpmap();			/* XXX conservative */

	/*
	 * If there is no space in which the PTEs can be written
	 * while they are not in the hardware, this must be a new
	 * virtual segment.  Get PTE space and count the segment.
	 *
	 * TO SPEED UP CTX ALLOC, PUT SEGMENT BOUNDS STUFF HERE
	 * AND IN pmap_rmu()
	 */

	GAP_SHRINK(pm,vr);

#ifdef DEBUG
	if (pm->pm_gap_end < pm->pm_gap_start) {
		printf("pmap_enu: gap_start %x, gap_end %x",
			pm->pm_gap_start, pm->pm_gap_end);
		panic("pmap_enu: gap botch");
	}
#endif

rretry:
	if (rp->rg_segmap == NULL) {
		/* definitely a new mapping */
		register int i;
		register int size = NSEGRG * sizeof (struct segmap);

		sp = (struct segmap *)malloc((u_long)size, M_VMPMAP, M_WAITOK);
		if (rp->rg_segmap != NULL) {
printf("pmap_enter: segment filled during sleep\n");	/* can this happen? */
			free(sp, M_VMPMAP);
			goto rretry;
		}
		qzero((caddr_t)sp, size);
		rp->rg_segmap = sp;
		rp->rg_nsegmap = 0;
		for (i = NSEGRG; --i >= 0;)
			sp++->sg_pmeg = seginval;
	}

	sp = &rp->rg_segmap[vs];

sretry:
	if ((pte = sp->sg_pte) == NULL) {
		/* definitely a new mapping */
		register int size = NPTESG * sizeof *pte;

		pte = (int *)malloc((u_long)size, M_VMPMAP, M_WAITOK);
		if (sp->sg_pte != NULL) {
printf("pmap_enter: pte filled during sleep\n");	/* can this happen? */
			free(pte, M_VMPMAP);
			goto sretry;
		}
#ifdef DEBUG
		if (sp->sg_pmeg != seginval)
			panic("pmap_enter: new ptes, but not seginval");
#endif
		qzero((caddr_t)pte, size);
		sp->sg_pte = pte;
		sp->sg_npte = 1;
		rp->rg_nsegmap++;
	} else {
		/* might be a change: fetch old pte */
		doflush = 0;
		if ((pmeg = sp->sg_pmeg) == seginval) {
			/* software pte */
			tpte = pte[VA_VPG(va)];
		} else {
			/* hardware pte */
			if (CTX_USABLE(pm,rp)) {
				setcontext(pm->pm_ctxnum);
				tpte = getpte4(va);
				doflush = 1;
			} else {
				setcontext(0);
				/* XXX use per-cpu pteva? */
#ifdef MMU_3L
				if (mmu_3l)
					setregmap(0, tregion);
#endif
				setsegmap(0, pmeg);
				tpte = getpte4(VA_VPG(va) << PGSHIFT);
			}
		}
		if (tpte & PG_V) {
			register int addr;

			/* old mapping exists, and is of the same pa type */
			if ((tpte & (PG_PFNUM|PG_TYPE)) ==
			    (pteproto & (PG_PFNUM|PG_TYPE))) {
				/* just changing prot and/or wiring */
				splx(s);
				/* caller should call this directly: */
				pmap_changeprot4_4c(pm, va, prot, wired);
				if (wired)
					pm->pm_stats.wired_count++;
				else
					pm->pm_stats.wired_count--;
				return;
			}
			/*
			 * Switcheroo: changing pa for this va.
			 * If old pa was managed, remove from pvlist.
			 * If old page was cached, flush cache.
			 */
/*printf("%s[%d]: pmap_enu: changing existing va(%x)=>pa entry\n",
curproc->p_comm, curproc->p_pid, va);*/
			if ((tpte & PG_TYPE) == PG_OBMEM) {
				addr = ptoa(tpte & PG_PFNUM);
				if (managed(addr))
					pv_unlink4_4c(pvhead(addr), pm, va);
				if (vactype != VAC_NONE &&
				    doflush && (tpte & PG_NC) == 0)
					cache_flush_page((int)va);
			}
		} else {
			/* adding new entry */
			sp->sg_npte++;

			/*
			 * Increment counters
			 */
			if (wired)
				pm->pm_stats.wired_count++;
		}
	}

	if (pv != NULL)
		pteproto |= pv_link4_4c(pv, pm, va);

	/*
	 * Update hardware & software PTEs.
	 */
	if ((pmeg = sp->sg_pmeg) != seginval) {
		/* ptes are in hardare */
		if (CTX_USABLE(pm,rp))
			setcontext(pm->pm_ctxnum);
		else {
			setcontext(0);
			/* XXX use per-cpu pteva? */
#ifdef MMU_3L
			if (mmu_3l)
				setregmap(0, tregion);
#endif
			setsegmap(0, pmeg);
			va = VA_VPG(va) << PGSHIFT;
		}
		setpte4(va, pteproto);
	}
	/* update software copy */
	pte += VA_VPG(va);
	*pte = pteproto;

	splx(s);
}

#endif /*sun4,4c*/

#if defined(SUN4M)		/* Sun4M versions of enter routines */
/*
 * Insert (MI) physical page pa at virtual address va in the given pmap.
 * NB: the pa parameter includes type bits PMAP_OBIO, PMAP_NC as necessary.
 *
 * If pa is not in the `managed' range it will not be `bank mapped'.
 * This works during bootstrap only because the first 4MB happens to
 * map one-to-one.
 *
 * There may already be something else there, or we might just be
 * changing protections and/or wiring on an existing mapping.
 *	XXX	should have different entry points for changing!
 */

void
pmap_enter4m(pm, va, pa, prot, wired)
	register struct pmap *pm;
	vm_offset_t va, pa;
	vm_prot_t prot;
	int wired;
{
	register struct pvlist *pv;
	register int pteproto, ctx;

	if (pm == NULL)
		return;

#ifdef DEBUG
	if (pmapdebug & PDB_ENTER)
		printf("pmap_enter(%p, %lx, %lx, %x, %x)\n",
		    pm, va, pa, prot, wired);
#endif

	/* Initialise pteproto with cache bit */
	pteproto = (pa & PMAP_NC) == 0 ? SRMMU_PG_C : 0;

	if (pa & PMAP_TYPE4M) {		/* this page goes in an iospace */
		if (cpumod == SUN4M_MS)
			panic("pmap_enter4m: attempt to use 36-bit iospace on"
			      " MicroSPARC");
		pteproto |= (pa & PMAP_TYPE4M) << PMAP_PTESHFT4M;
	}

	/* Make sure we get a pte with appropriate perms! */
	pteproto |= SRMMU_TEPTE | PPROT_RX_RX;

	pa &= ~PMAP_TNC;
	/*
	 * Set up prototype for new PTE.  Cannot set PG_NC from PV_NC yet
	 * since the pvlist no-cache bit might change as a result of the
	 * new mapping.
	 */
	if ((pteproto & SRMMU_PGTYPE) == PG_SUN4M_OBMEM && managed(pa)) {
#ifdef DIAGNOSTIC
		if (!pmap_pa_exists(pa))
			panic("pmap_enter: no such address: %lx", pa);
#endif
		pv = pvhead(pa);
	} else {
		pv = NULL;
	}
	pteproto |= (atop(pa) << SRMMU_PPNSHIFT);

	if (prot & VM_PROT_WRITE)
		pteproto |= PPROT_WRITE;

	ctx = getcontext();

	if (pm == pmap_kernel())
		pmap_enk4m(pm, va, prot, wired, pv, pteproto | PPROT_S);
	else
		pmap_enu4m(pm, va, prot, wired, pv, pteproto);

	setcontext(ctx);
}

/* enter new (or change existing) kernel mapping */
void
pmap_enk4m(pm, va, prot, wired, pv, pteproto)
	register struct pmap *pm;
	vm_offset_t va;
	vm_prot_t prot;
	int wired;
	register struct pvlist *pv;
	register int pteproto;
{
	register int vr, vs, tpte, s;
	struct regmap *rp;
	struct segmap *sp;

#ifdef DEBUG
	if (va < KERNBASE)
	    panic("pmap_enk4m: can't enter va 0x%lx below KERNBASE",va);
#endif
	vr = VA_VREG(va);
	vs = VA_VSEG(va);
	rp = &pm->pm_regmap[vr];
	sp = &rp->rg_segmap[vs];

	s = splpmap();		/* XXX way too conservative */

	if (rp->rg_seg_ptps == NULL) /* enter new region */
		panic("pmap_enk4m: missing kernel region table for va %lx",va);

	if (((tpte = getpte4m(va)) & SRMMU_TETYPE) == SRMMU_TEPTE) {
		register int addr;

		/* old mapping exists, and is of the same pa type */

		if ((tpte & SRMMU_PPNMASK) == (pteproto & SRMMU_PPNMASK)) {
			/* just changing protection and/or wiring */
			splx(s);
			pmap_changeprot(pm, va, prot, wired);
			return;
		}

		if ((tpte & SRMMU_PGTYPE) == PG_SUN4M_OBMEM) {
#ifdef DEBUG
printf("pmap_enk4m: changing existing va=>pa entry: va %lx, pteproto %x, "
       "oldpte %x\n", va, pteproto, tpte);
#endif
			/*
			 * Switcheroo: changing pa for this va.
			 * If old pa was managed, remove from pvlist.
			 * If old page was cached, flush cache.
			 */
			addr = ptoa((tpte & SRMMU_PPNMASK) >> SRMMU_PPNSHIFT);
			if (managed(addr))
				pv_unlink4m(pvhead(addr), pm, va);
			if (tpte & SRMMU_PG_C) {
				setcontext(0);	/* ??? */
				if (vactype != VAC_NONE)
					cache_flush_page((int)va);
			}
		}
	} else {
		/* adding new entry */
		sp->sg_npte++;
	}

	/*
	 * If the new mapping is for a managed PA, enter into pvlist.
	 * Note that the mapping for a malloc page will always be
	 * unique (hence will never cause a second call to malloc).
	 */
	if (pv != NULL)
	        pteproto &= ~(pv_link4m(pv, pm, va));

	if (sp->sg_pte == NULL) /* If no existing pagetable */
		panic("pmap_enk4m: missing segment table for va 0x%lx",va);

	/* ptes kept in hardware only */
	setpte4m(va, pteproto);

	splx(s);
}

/* enter new (or change existing) user mapping */
void
pmap_enu4m(pm, va, prot, wired, pv, pteproto)
	register struct pmap *pm;
	vm_offset_t va;
	vm_prot_t prot;
	int wired;
	register struct pvlist *pv;
	register int pteproto;
{
	register int vr, vs, *pte, tpte, s, doflush;
	struct regmap *rp;
	struct segmap *sp;

	write_user_windows();		/* XXX conservative */
	vr = VA_VREG(va);
	vs = VA_VSEG(va);
	rp = &pm->pm_regmap[vr];
	s = splpmap();			/* XXX conservative */

#ifdef DEBUG
	if (pm->pm_gap_end < pm->pm_gap_start) {
		printf("pmap_enu: gap_start %x, gap_end %x",
			pm->pm_gap_start, pm->pm_gap_end);
		panic("pmap_enu: gap botch");
	}
#endif

rretry:
	if (rp->rg_segmap == NULL) {
		/* definitely a new mapping */
		register int size = NSEGRG * sizeof (struct segmap);

		sp = (struct segmap *)malloc((u_long)size, M_VMPMAP, M_WAITOK);
		if (rp->rg_segmap != NULL) {
#ifdef DEBUG
printf("pmap_enu4m: segment filled during sleep\n");	/* can this happen? */
#endif
			free(sp, M_VMPMAP);
			goto rretry;
		}
		qzero((caddr_t)sp, size);
		rp->rg_segmap = sp;
		rp->rg_nsegmap = 0;
		rp->rg_seg_ptps = NULL;
	}
rgretry:
	if (rp->rg_seg_ptps == NULL) {
		/* Need a segment table */
		register int size;
		register caddr_t tblp;
		size = SRMMU_L2SIZE * sizeof(long);
		tblp = malloc(size, M_VMPMAP, M_WAITOK);
		if (rp->rg_seg_ptps != NULL) {
#ifdef DEBUG
printf("pmap_enu4m: bizarre segment table fill during sleep\n");
#endif
			free(tblp,M_VMPMAP);
			goto rgretry;
		}
		if (cant_cache_pagetables)
			kvm_uncache(tblp, (size+NBPG-1)/NBPG);

		rp->rg_seg_ptps = (int *)tblp;
		qzero(tblp, size);
		pm->pm_reg_ptps[vr] =
			(VA2PA(tblp) >> SRMMU_PPNPASHIFT) | SRMMU_TEPTD;
	}

	sp = &rp->rg_segmap[vs];

sretry:
	if ((pte = sp->sg_pte) == NULL) {
		/* definitely a new mapping */
		register int size = SRMMU_L3SIZE * sizeof(*pte);

		pte = (int *)malloc((u_long)size, M_VMPMAP, M_WAITOK);
		if (sp->sg_pte != NULL) {
printf("pmap_enter: pte filled during sleep\n");	/* can this happen? */
			free(pte, M_VMPMAP);
			goto sretry;
		}
		if (cant_cache_pagetables)
			kvm_uncache((caddr_t)pte, (size+NBPG-1)/NBPG);

		qzero((caddr_t)pte, size);
		sp->sg_pte = pte;
		sp->sg_npte = 1;
		rp->rg_nsegmap++;
		rp->rg_seg_ptps[vs] =
			(VA2PA((caddr_t)pte) >> SRMMU_PPNPASHIFT) | SRMMU_TEPTD;
	} else {
		/* might be a change: fetch old pte */
		doflush = 0;

		if (CTX_USABLE(pm,rp)) {
			setcontext(pm->pm_ctxnum);
			tpte = getpte4m(va);
			doflush = 1;
		} else {
			tpte = getptesw4m(pm, va);
		}
		if ((tpte & SRMMU_TETYPE) == SRMMU_TEPTE) {
			register int addr;

			/* old mapping exists, and is of the same pa type */
			if ((tpte & SRMMU_PPNMASK) ==
			    (pteproto & SRMMU_PPNMASK)) {
				/* just changing prot and/or wiring */
				splx(s);
				/* caller should call this directly: */
				pmap_changeprot4m(pm, va, prot, wired);
				if (wired)
					pm->pm_stats.wired_count++;
				else
					pm->pm_stats.wired_count--;
				return;
			}
			/*
			 * Switcheroo: changing pa for this va.
			 * If old pa was managed, remove from pvlist.
			 * If old page was cached, flush cache.
			 */
#ifdef DEBUG
if (pmapdebug & PDB_ENTER)
printf("%s[%d]: pmap_enu: changing existing va(%x)=>pa(pte=%x) entry\n",
curproc->p_comm, curproc->p_pid, (int)va, (int)pte);
#endif
			if ((tpte & SRMMU_PGTYPE) == PG_SUN4M_OBMEM) {
				addr = ptoa( (tpte & SRMMU_PPNMASK) >>
					     SRMMU_PPNSHIFT);
				if (managed(addr))
					pv_unlink4m(pvhead(addr), pm, va);
				if (vactype != VAC_NONE &&
				    doflush && (tpte & SRMMU_PG_C))
					cache_flush_page((int)va);
			}
		} else {
			/* adding new entry */
			sp->sg_npte++;

			/*
			 * Increment counters
			 */
			if (wired)
				pm->pm_stats.wired_count++;
		}
	}
	if (pv != NULL)
		pteproto &= ~(pv_link4m(pv, pm, va));

	/*
	 * Update hardware & software PTEs.
	 */
	if (CTX_USABLE(pm,rp)) {
		setcontext(pm->pm_ctxnum);
		setpte4m(va, pteproto);
	} else
		setptesw4m(pm, va, pteproto);
	/* XXX: restore previous context here? */

	splx(s);
}
#endif /* sun4m */

/*
 * Change the wiring attribute for a map/virtual-address pair.
 */
/* ARGSUSED */
void
pmap_change_wiring(pm, va, wired)
	struct pmap *pm;
	vm_offset_t va;
	int wired;
{

	pmap_stats.ps_useless_changewire++;
}

/*
 * Extract the physical page address associated
 * with the given map/virtual_address pair.
 * GRR, the vm code knows; we should not have to do this!
 */

#if defined(SUN4) || defined(SUN4C)
vm_offset_t
pmap_extract4_4c(pm, va)
	register struct pmap *pm;
	vm_offset_t va;
{
	register int tpte;
	register int vr, vs;
	struct regmap *rp;
	struct segmap *sp;

	if (pm == NULL) {
		printf("pmap_extract: null pmap\n");
		return (0);
	}
	vr = VA_VREG(va);
	vs = VA_VSEG(va);
	rp = &pm->pm_regmap[vr];
	if (rp->rg_segmap == NULL) {
		printf("pmap_extract: invalid segment (%d)\n", vr);
		return (0);
	}
	sp = &rp->rg_segmap[vs];

	if (sp->sg_pmeg != seginval) {
		register int ctx = getcontext();

		if (CTX_USABLE(pm,rp)) {
			CHANGE_CONTEXTS(ctx, pm->pm_ctxnum);
			tpte = getpte4(va);
		} else {
			CHANGE_CONTEXTS(ctx, 0);
#ifdef MMU_3L
			if (mmu_3l)
				setregmap(0, tregion);
#endif
			setsegmap(0, sp->sg_pmeg);
			tpte = getpte4(VA_VPG(va) << PGSHIFT);
		}
		setcontext(ctx);
	} else {
		register int *pte = sp->sg_pte;

		if (pte == NULL) {
			printf("pmap_extract: invalid segment\n");
			return (0);
		}
		tpte = pte[VA_VPG(va)];
	}
	if ((tpte & PG_V) == 0) {
		printf("pmap_extract: invalid pte\n");
		return (0);
	}
	tpte &= PG_PFNUM;
	tpte = tpte;
	return ((tpte << PGSHIFT) | (va & PGOFSET));
}
#endif /*4,4c*/

#if defined(SUN4M)		/* 4m version of pmap_extract */
/*
 * Extract the physical page address associated
 * with the given map/virtual_address pair.
 * GRR, the vm code knows; we should not have to do this!
 */
vm_offset_t
pmap_extract4m(pm, va)
	register struct pmap *pm;
	vm_offset_t va;
{
	register int tpte, ctx;

	if (pm == NULL) {
		printf("pmap_extract: null pmap\n");
		return (0);
	}

	if (pm->pm_ctx) {
		ctx = getcontext();
		CHANGE_CONTEXTS(ctx, pm->pm_ctxnum);
		tpte = getpte4m(va);
#ifdef DEBUG
		if ((tpte & SRMMU_TETYPE) != SRMMU_TEPTE) {
			printf("pmap_extract: invalid pte of type %d\n",
			       tpte & SRMMU_TETYPE);
			return (0);
		}
#endif
		setcontext(ctx);
	} else
		tpte = getptesw4m(pm, va);


	return (ptoa((tpte & SRMMU_PPNMASK) >> SRMMU_PPNSHIFT) | VA_OFF(va));
}
#endif /* sun4m */

/*
 * Copy the range specified by src_addr/len
 * from the source map to the range dst_addr/len
 * in the destination map.
 *
 * This routine is only advisory and need not do anything.
 */
/* ARGSUSED */
void
pmap_copy(dst_pmap, src_pmap, dst_addr, len, src_addr)
	struct pmap *dst_pmap, *src_pmap;
	vm_offset_t dst_addr;
	vm_size_t len;
	vm_offset_t src_addr;
{
#if 0
	if (CPU_ISSUN4M) {
		register int i, pte;
		for (i = 0; i < len/NBPG; i++) {
			pte = getptesw4m(src_pmap, src_addr);
			pmap_enter(dst_pmap, dst_addr,
				   ptoa((pte & SRMMU_PPNMASK) >>
					SRMMU_PPNSHIFT) |
				    VA_OFF(src_addr),
				   (pte & PPROT_WRITE)
					? VM_PROT_WRITE| VM_PROT_READ
					: VM_PROT_READ,
				   0);
			src_addr += NBPG;
			dst_addr += NBPG;
		}
	}
#endif
}

/*
 * Require that all active physical maps contain no
 * incorrect entries NOW.  [This update includes
 * forcing updates of any address map caching.]
 */
void
pmap_update()
{
#if defined(SUN4M)
	if (CPU_ISSUN4M)
		tlb_flush_all();	/* %%%: Extreme Paranoia?  */
#endif
}

/*
 * Garbage collects the physical map system for
 * pages which are no longer used.
 * Success need not be guaranteed -- that is, there
 * may well be pages which are not referenced, but
 * others may be collected.
 * Called by the pageout daemon when pages are scarce.
 */
/* ARGSUSED */
void
pmap_collect(pm)
	struct pmap *pm;
{
}

#if defined(SUN4) || defined(SUN4C)

/*
 * Clear the modify bit for the given physical page.
 */
void
pmap_clear_modify4_4c(pa)
	register vm_offset_t pa;
{
	register struct pvlist *pv;

	if ((pa & (PMAP_TNC & ~PMAP_NC)) == 0 && managed(pa)) {
		pv = pvhead(pa);
		(void) pv_syncflags4_4c(pv);
		pv->pv_flags &= ~PV_MOD;
	}
}

/*
 * Tell whether the given physical page has been modified.
 */
int
pmap_is_modified4_4c(pa)
	register vm_offset_t pa;
{
	register struct pvlist *pv;

	if ((pa & (PMAP_TNC & ~PMAP_NC)) == 0 && managed(pa)) {
		pv = pvhead(pa);
		if (pv->pv_flags & PV_MOD || pv_syncflags4_4c(pv) & PV_MOD)
			return (1);
	}
	return (0);
}

/*
 * Clear the reference bit for the given physical page.
 */
void
pmap_clear_reference4_4c(pa)
	vm_offset_t pa;
{
	register struct pvlist *pv;

	if ((pa & (PMAP_TNC & ~PMAP_NC)) == 0 && managed(pa)) {
		pv = pvhead(pa);
		(void) pv_syncflags4_4c(pv);
		pv->pv_flags &= ~PV_REF;
	}
}

/*
 * Tell whether the given physical page has been referenced.
 */
int
pmap_is_referenced4_4c(pa)
	vm_offset_t pa;
{
	register struct pvlist *pv;

	if ((pa & (PMAP_TNC & ~PMAP_NC)) == 0 && managed(pa)) {
		pv = pvhead(pa);
		if (pv->pv_flags & PV_REF || pv_syncflags4_4c(pv) & PV_REF)
			return (1);
	}
	return (0);
}
#endif /*4,4c*/

#if defined(SUN4M)

/*
 * 4m versions of bit test/set routines
 *
 * Note that the 4m-specific routines should eventually service these
 * requests from their page tables, and the whole pvlist bit mess should
 * be dropped for the 4m (unless this causes a performance hit from
 * tracing down pagetables/regmap/segmaps).
 */

/*
 * Clear the modify bit for the given physical page.
 */
void
pmap_clear_modify4m(pa)	   /* XXX %%%: Should service from swpagetbl for 4m */
	register vm_offset_t pa;
{
	register struct pvlist *pv;

	if ((pa & (PMAP_TNC & ~PMAP_NC)) == 0 && managed(pa)) {
		pv = pvhead(pa);
		(void) pv_syncflags4m(pv);
		pv->pv_flags &= ~PV_MOD4M;
	}
}

/*
 * Tell whether the given physical page has been modified.
 */
int
pmap_is_modified4m(pa) /* Test performance with SUN4M && SUN4/4C. XXX */
	register vm_offset_t pa;
{
	register struct pvlist *pv;

	if ((pa & (PMAP_TNC & ~PMAP_NC)) == 0 && managed(pa)) {
		pv = pvhead(pa);
		if (pv->pv_flags & PV_MOD4M || pv_syncflags4m(pv) & PV_MOD4M)
		        return(1);
	}
	return (0);
}

/*
 * Clear the reference bit for the given physical page.
 */
void
pmap_clear_reference4m(pa)
	vm_offset_t pa;
{
	register struct pvlist *pv;

	if ((pa & (PMAP_TNC & ~PMAP_NC)) == 0 && managed(pa)) {
		pv = pvhead(pa);
		(void) pv_syncflags4m(pv);
		pv->pv_flags &= ~PV_REF4M;
	}
}

/*
 * Tell whether the given physical page has been referenced.
 */
int
pmap_is_referenced4m(pa)
	vm_offset_t pa;
{
	register struct pvlist *pv;

	if ((pa & (PMAP_TNC & ~PMAP_NC)) == 0 && managed(pa)) {
		pv = pvhead(pa);
		if (pv->pv_flags & PV_REF4M || pv_syncflags4m(pv) & PV_REF4M)
		        return(1);
	}
	return (0);
}
#endif /* 4m */

/*
 * Make the specified pages (by pmap, offset) pageable (or not) as requested.
 *
 * A page which is not pageable may not take a fault; therefore, its page
 * table entry must remain valid for the duration (or at least, the trap
 * handler must not call vm_fault).
 *
 * This routine is merely advisory; pmap_enter will specify that these pages
 * are to be wired down (or not) as appropriate.
 */
/* ARGSUSED */
void
pmap_pageable(pm, start, end, pageable)
	struct pmap *pm;
	vm_offset_t start, end;
	int pageable;
{
}

/*
 * Fill the given MI physical page with zero bytes.
 *
 * We avoid stomping on the cache.
 * XXX	might be faster to use destination's context and allow cache to fill?
 */

#if defined(SUN4) || defined(SUN4C)

void
pmap_zero_page4_4c(pa)
	register vm_offset_t pa;
{
	register caddr_t va;
	register int pte;

	if (((pa & (PMAP_TNC & ~PMAP_NC)) == 0) && managed(pa)) {
		/*
		 * The following might not be necessary since the page
		 * is being cleared because it is about to be allocated,
		 * i.e., is in use by no one.
		 */
		if (vactype != VAC_NONE)
			pv_flushcache(pvhead(pa));
	}
	pte = PG_V | PG_S | PG_W | PG_NC | (atop(pa) & PG_PFNUM);

	va = vpage[0];
	setpte4(va, pte);
	qzero(va, NBPG);
	setpte4(va, 0);
}

/*
 * Copy the given MI physical source page to its destination.
 *
 * We avoid stomping on the cache as above (with same `XXX' note).
 * We must first flush any write-back cache for the source page.
 * We go ahead and stomp on the kernel's virtual cache for the
 * source page, since the cache can read memory MUCH faster than
 * the processor.
 */
void
pmap_copy_page4_4c(src, dst)
	vm_offset_t src, dst;
{
	register caddr_t sva, dva;
	register int spte, dpte;

	if (managed(src)) {
		if (vactype == VAC_WRITEBACK)
			pv_flushcache(pvhead(src));
	}
	spte = PG_V | PG_S | (atop(src) & PG_PFNUM);

	if (managed(dst)) {
		/* similar `might not be necessary' comment applies */
		if (vactype != VAC_NONE)
			pv_flushcache(pvhead(dst));
	}
	dpte = PG_V | PG_S | PG_W | PG_NC | (atop(dst) & PG_PFNUM);

	sva = vpage[0];
	dva = vpage[1];
	setpte4(sva, spte);
	setpte4(dva, dpte);
	qcopy(sva, dva, NBPG);	/* loads cache, so we must ... */
	if (vactype != VAC_NONE)
		cache_flush_page((int)sva);
	setpte4(sva, 0);
	setpte4(dva, 0);
}
#endif /* 4, 4c */

#if defined(SUN4M)		/* Sun4M version of copy/zero routines */
/*
 * Fill the given MI physical page with zero bytes.
 *
 * We avoid stomping on the cache.
 * XXX	might be faster to use destination's context and allow cache to fill?
 */
void
pmap_zero_page4m(pa)
	register vm_offset_t pa;
{
	register caddr_t va;
	register int pte;

	if (((pa & (PMAP_TNC & ~PMAP_NC)) == 0) && managed(pa)) {
		/*
		 * The following might not be necessary since the page
		 * is being cleared because it is about to be allocated,
		 * i.e., is in use by no one.
		 */
		if (vactype != VAC_NONE)
			pv_flushcache(pvhead(pa));
	}
	pte = ~SRMMU_PG_C & (SRMMU_TEPTE | PPROT_S | PPROT_WRITE |
	      (atop(pa) << SRMMU_PPNSHIFT));
	va = vpage[0];
	setpte4m((vm_offset_t) va, pte);
	qzero(va, NBPG);
	setpte4m((vm_offset_t) va, SRMMU_TEINVALID);
}

/*
 * Copy the given MI physical source page to its destination.
 *
 * We avoid stomping on the cache as above (with same `XXX' note).
 * We must first flush any write-back cache for the source page.
 * We go ahead and stomp on the kernel's virtual cache for the
 * source page, since the cache can read memory MUCH faster than
 * the processor.
 */
void
pmap_copy_page4m(src, dst)
	vm_offset_t src, dst;
{
	register caddr_t sva, dva;
	register int spte, dpte;

	if (managed(src)) {
		if (vactype == VAC_WRITEBACK)
			pv_flushcache(pvhead(src));
	}
	spte = SRMMU_TEPTE | SRMMU_PG_C | PPROT_S |
		(atop(src) << SRMMU_PPNSHIFT);

	if (managed(dst)) {
		/* similar `might not be necessary' comment applies */
		if (vactype != VAC_NONE)
			pv_flushcache(pvhead(dst));
	}
	dpte = ~SRMMU_PG_C & (SRMMU_TEPTE | PPROT_S | PPROT_WRITE |
		(atop(dst) << SRMMU_PPNSHIFT));

	sva = vpage[0];
	dva = vpage[1];
	setpte4m((vm_offset_t) sva, spte);
	setpte4m((vm_offset_t) dva, dpte);
	qcopy(sva, dva, NBPG);	/* loads cache, so we must ... */
	if (vactype != VAC_NONE)
		cache_flush_page((int)sva);
	setpte4m((vm_offset_t) sva, SRMMU_TEINVALID);
	setpte4m((vm_offset_t) dva, SRMMU_TEINVALID);
}
#endif /* Sun4M */

/*
 * Turn a cdevsw d_mmap value into a byte address for pmap_enter.
 * XXX	this should almost certainly be done differently, and
 *	elsewhere, or even not at all
 */
vm_offset_t
pmap_phys_address(x)
	int x;
{

	return (x);
}

/*
 * Turn off cache for a given (va, number of pages).
 *
 * We just assert PG_NC for each PTE; the addresses must reside
 * in locked kernel space.  A cache flush is also done.
 */
void
kvm_uncache(va, npages)
	register caddr_t va;
	register int npages;
{
	register int pte;
	if (CPU_ISSUN4M) {
#if defined(SUN4M)
		for (; --npages >= 0; va += NBPG) {
			pte = getpte4m((vm_offset_t) va);
			if ((pte & SRMMU_TETYPE) != SRMMU_TEPTE)
				panic("kvm_uncache: table entry not pte");
			pte &= ~SRMMU_PG_C;
			setpte4m((vm_offset_t) va, pte);
			if (vactype != VAC_NONE && (pte & PG_TYPE) == PG_OBMEM)
				cache_flush_page((int)va);
		}
#endif
	} else {
#if defined(SUN4) || defined(SUN4C)
		for (; --npages >= 0; va += NBPG) {
			pte = getpte4(va);
			if ((pte & PG_V) == 0)
				panic("kvm_uncache !pg_v");
			pte |= PG_NC;
			setpte4(va, pte);
			if (vactype != VAC_NONE && (pte & PG_TYPE) == PG_OBMEM)
				cache_flush_page((int)va);
		}
#endif
	}
}

/*
 * Turn on IO cache for a given (va, number of pages).
 *
 * We just assert PG_NC for each PTE; the addresses must reside
 * in locked kernel space.  A cache flush is also done.
 */
void
kvm_iocache(va, npages)
	register caddr_t va;
	register int npages;
{

#ifdef SUN4M
	if (CPU_ISSUN4M) /* %%%: Implement! */
		panic("kvm_iocache: 4m iocache not implemented");
#endif
#if defined(SUN4) || defined(SUN4C)
	for (; --npages >= 0; va += NBPG) {
		register int pte = getpte4(va);
		if ((pte & PG_V) == 0)
			panic("kvm_iocache !pg_v");
		pte |= PG_IOC;
		setpte4(va, pte);
	}
#endif
}

int
pmap_count_ptes(pm)
	register struct pmap *pm;
{
	register int idx, total;
	register struct regmap *rp;
	register struct segmap *sp;

	if (pm == pmap_kernel()) {
		rp = &pm->pm_regmap[NUREG];
		idx = NKREG;
	} else {
		rp = pm->pm_regmap;
		idx = NUREG;
	}
	for (total = 0; idx;)
		if ((sp = rp[--idx].rg_segmap) != NULL)
			total += sp->sg_npte;
	pm->pm_stats.resident_count = total;
	return (total);
}

/*
 * Find first virtual address >= *va that is
 * least likely to cause cache aliases.
 * (This will just seg-align mappings.)
 */
void
pmap_prefer(foff, vap)
	register vm_offset_t foff;
	register vm_offset_t *vap;
{
	register vm_offset_t va = *vap;
	register long d, m;

	if (VA_INHOLE(va))
		va = MMU_HOLE_END;

	m = CACHE_ALIAS_DIST;
	if (m == 0)		/* m=0 => no cache aliasing */
		return;

	d = foff - va;
	d &= (m - 1);
	*vap = va + d;
}

void
pmap_redzone()
{
#if defined(SUN4M)
	if (CPU_ISSUN4M) {
		setpte4m(KERNBASE, 0);
		return;
	}
#endif
#if defined(SUN4) || defined(SUN4C)
	if (CPU_ISSUN4OR4C) {
		setpte4(KERNBASE, 0);
		return;
	}
#endif
}

#ifdef DEBUG
/*
 * Check consistency of a pmap (time consuming!).
 */
void
pm_check(s, pm)
	char *s;
	struct pmap *pm;
{
	if (pm == pmap_kernel())
		pm_check_k(s, pm);
	else
		pm_check_u(s, pm);
}

void
pm_check_u(s, pm)
	char *s;
	struct pmap *pm;
{
	struct regmap *rp;
	struct segmap *sp;
	int n, vs, vr, j, m, *pte;

	if (pm->pm_regmap == NULL)
	    panic("%s: CHK(pmap %p): no region mapping", s, pm);

#if defined(SUN4M)
	if (CPU_ISSUN4M &&
	    (pm->pm_reg_ptps == NULL ||
	     pm->pm_reg_ptps_pa != VA2PA((caddr_t)pm->pm_reg_ptps)))
	    panic("%s: CHK(pmap %p): no SRMMU region table or bad pa: tblva=%p, tblpa=0x%x",
		  s, pm, pm->pm_reg_ptps, pm->pm_reg_ptps_pa);

	if (CPU_ISSUN4M && pm->pm_ctx != NULL &&
	    (ctx_phys_tbl[pm->pm_ctxnum] != ((VA2PA((caddr_t)pm->pm_reg_ptps)
					      >> SRMMU_PPNPASHIFT) |
					     SRMMU_TEPTD)))
	    panic("%s: CHK(pmap %p): SRMMU region table at %x not installed "
		  "for context %d", s, pm, pm->pm_reg_ptps_pa, pm->pm_ctxnum);
#endif

	for (vr = 0; vr < NUREG; vr++) {
		rp = &pm->pm_regmap[vr];
		if (rp->rg_nsegmap == 0)
			continue;
		if (rp->rg_segmap == NULL)
			panic("%s: CHK(vr %d): nsegmap = %d; sp==NULL",
				s, vr, rp->rg_nsegmap);
#if defined(SUN4M)
		if (CPU_ISSUN4M && rp->rg_seg_ptps == NULL)
		    panic("%s: CHK(vr %d): nsegmap=%d; no SRMMU segment table",
			  s, vr, rp->rg_nsegmap);
		if (CPU_ISSUN4M &&
		    pm->pm_reg_ptps[vr] != ((VA2PA((caddr_t)rp->rg_seg_ptps) >>
					    SRMMU_PPNPASHIFT) | SRMMU_TEPTD))
		    panic("%s: CHK(vr %d): SRMMU segtbl not installed",s,vr);
#endif
		if ((unsigned int)rp < KERNBASE)
			panic("%s: rp=%p", s, rp);
		n = 0;
		for (vs = 0; vs < NSEGRG; vs++) {
			sp = &rp->rg_segmap[vs];
			if ((unsigned int)sp < KERNBASE)
				panic("%s: sp=%p", s, sp);
			if (sp->sg_npte != 0) {
				n++;
				if (sp->sg_pte == NULL)
					panic("%s: CHK(vr %d, vs %d): npte=%d, "
					   "pte=NULL", s, vr, vs, sp->sg_npte);
#if defined(SUN4M)
				if (CPU_ISSUN4M &&
				    rp->rg_seg_ptps[vs] !=
				     ((VA2PA((caddr_t)sp->sg_pte)
					>> SRMMU_PPNPASHIFT) |
				       SRMMU_TEPTD))
				    panic("%s: CHK(vr %d, vs %d): SRMMU page "
					  "table not installed correctly",s,vr,
					  vs);
#endif
				pte=sp->sg_pte;
				m = 0;
				for (j=0; j<NPTESG; j++,pte++)
				    if ((CPU_ISSUN4M
					 ?((*pte & SRMMU_TETYPE) == SRMMU_TEPTE)
					 :(*pte & PG_V)))
					m++;
				if (m != sp->sg_npte)
				    /*if (pmapdebug & 0x10000)*/
					printf("%s: user CHK(vr %d, vs %d): "
					    "npte(%d) != # valid(%d)\n",
						s, vr, vs, sp->sg_npte, m);
			}
		}
		if (n != rp->rg_nsegmap)
			panic("%s: CHK(vr %d): inconsistent "
				"# of pte's: %d, should be %d",
				s, vr, rp->rg_nsegmap, n);
	}
	return;
}

void
pm_check_k(s, pm)		/* Note: not as extensive as pm_check_u. */
	char *s;
	struct pmap *pm;
{
	struct regmap *rp;
	int vr, vs, n;

	if (pm->pm_regmap == NULL)
	    panic("%s: CHK(pmap %p): no region mapping", s, pm);

#if defined(SUN4M)
	if (CPU_ISSUN4M &&
	    (pm->pm_reg_ptps == NULL ||
	     pm->pm_reg_ptps_pa != VA2PA((caddr_t)pm->pm_reg_ptps)))
	    panic("%s: CHK(pmap %p): no SRMMU region table or bad pa: tblva=%p, tblpa=%x",
		  s, pm, pm->pm_reg_ptps, pm->pm_reg_ptps_pa);

	if (CPU_ISSUN4M &&
	    (ctx_phys_tbl[0] != ((VA2PA((caddr_t)pm->pm_reg_ptps) >>
					     SRMMU_PPNPASHIFT) | SRMMU_TEPTD)))
	    panic("%s: CHK(pmap %p): SRMMU region table at %x not installed "
		  "for context %d", s, pm, pm->pm_reg_ptps_pa, 0);
#endif
	for (vr = NUREG; vr < NUREG+NKREG; vr++) {
		rp = &pm->pm_regmap[vr];
		if (rp->rg_segmap == NULL)
			panic("%s: CHK(vr %d): nsegmap = %d; sp==NULL",
				s, vr, rp->rg_nsegmap);
		if (rp->rg_nsegmap == 0)
			continue;
#if defined(SUN4M)
		if (CPU_ISSUN4M && rp->rg_seg_ptps == NULL)
		    panic("%s: CHK(vr %d): nsegmap=%d; no SRMMU segment table",
			  s, vr, rp->rg_nsegmap);
		if (CPU_ISSUN4M &&
		    pm->pm_reg_ptps[vr] != ((VA2PA((caddr_t)rp->rg_seg_ptps) >>
					    SRMMU_PPNPASHIFT) | SRMMU_TEPTD))
		    panic("%s: CHK(vr %d): SRMMU segtbl not installed",s,vr);
#endif
		n = 0;
		for (vs = 0; vs < NSEGRG; vs++) {
			if (rp->rg_segmap[vs].sg_npte)
				n++;
		}
		if (n != rp->rg_nsegmap)
			printf("%s: kernel CHK(vr %d): inconsistent "
				"# of pte's: %d, should be %d\n",
				s, vr, rp->rg_nsegmap, n);
	}
	return;
}
#endif

/*
 * Return the number bytes that pmap_dumpmmu() will dump.
 * For each pmeg in the MMU, we'll write NPTESG PTEs.
 * The last page or two contains stuff so libkvm can bootstrap.
 */
int
pmap_dumpsize()
{
	long	sz;

	sz = ALIGN(sizeof(kcore_seg_t)) + ALIGN(sizeof(cpu_kcore_hdr_t));
	sz += npmemarr * sizeof(phys_ram_seg_t);

	if (CPU_ISSUN4OR4C)
		sz += (seginval + 1) * NPTESG * sizeof(int);

	return (btoc(sz));
}

/*
 * Write the dump header and memory mapping information at the front
 * of the dumpfile to make life easy for savecore and libkvm.  Also
 * dump the MMU contents for Sun 4/4c systems since they don't have
 * a separate incore copy of the kernel memory mappings.
 */
int
pmap_dumpmmu(dump, blkno)
	register daddr_t blkno;
	register int (*dump)	__P((dev_t, daddr_t, caddr_t, size_t));
{
	kcore_seg_t	*ksegp;
	cpu_kcore_hdr_t	*kcpup;
	phys_ram_seg_t	memseg;
	register int error = 0;
	register int	i, memsegoffset, pmegoffset;
	int buffer[dbtob(1) / sizeof(int)];
	int		*bp, *ep;
#if defined(SUN4C) || defined(SUN4)
	register int	pmeg;
#endif

#define EXPEDITE(p,n) do {						\
	int *sp = (int *)(p);						\
	int sz = (n);							\
	while (sz > 0) {						\
		*bp++ = *sp++;						\
		if (bp >= ep) {						\
			error = (*dump)(dumpdev, blkno,			\
					(caddr_t)buffer, dbtob(1));	\
			if (error != 0)					\
				return (error);				\
			++blkno;					\
			bp = buffer;					\
		}							\
		sz -= sizeof *sp;					\
	}								\
} while (0)

	setcontext(0);

	/* Setup bookkeeping pointers */
	bp = buffer;
	ep = &buffer[sizeof(buffer) / sizeof(buffer[0])];

	/* Fill in MI segment header */
	ksegp = (kcore_seg_t *)bp;
	CORE_SETMAGIC(*ksegp, KCORE_MAGIC, MID_MACHINE, CORE_CPU);
	ksegp->c_size = ctob(pmap_dumpsize()) - ALIGN(sizeof(kcore_seg_t));

	/* Fill in MD segment header (interpreted by MD part of libkvm) */
	kcpup = (cpu_kcore_hdr_t *)((int)bp + ALIGN(sizeof(kcore_seg_t)));
	kcpup->cputype = cputyp;
	kcpup->nmemseg = npmemarr;
	kcpup->memsegoffset = memsegoffset = ALIGN(sizeof(cpu_kcore_hdr_t));
	kcpup->npmeg = (CPU_ISSUN4OR4C) ? seginval + 1 : 0; 
	kcpup->pmegoffset = pmegoffset =
		memsegoffset + npmemarr * sizeof(phys_ram_seg_t);

	/* Note: we have assumed everything fits in buffer[] so far... */
	bp = (int *)&kcpup->segmap_store;
	EXPEDITE(&kernel_segmap_store, sizeof(kernel_segmap_store));

	/* Align storage for upcoming quad-aligned segment array */
	while (bp != (int *)ALIGN(bp)) {
		int dummy = 0;
		EXPEDITE(&dummy, sizeof dummy);
	}
	for (i = 0; i < npmemarr; i++) {
		memseg.start = pmemarr[i].addr;
		memseg.size = pmemarr[i].len;
		EXPEDITE(&memseg, sizeof(phys_ram_seg_t));
	}

	if (CPU_ISSUN4M)
		goto out;

#if defined(SUN4C) || defined(SUN4)
	/*
	 * dump page table entries
	 *
	 * We dump each pmeg in order (by segment number).  Since the MMU
	 * automatically maps the given virtual segment to a pmeg we must
	 * iterate over the segments by incrementing an unused segment slot
	 * in the MMU.  This fixed segment number is used in the virtual
	 * address argument to getpte().
	 */

	/*
	 * Go through the pmegs and dump each one.
	 */
	for (pmeg = 0; pmeg <= seginval; ++pmeg) {
		register vm_offset_t va = 0;

		setsegmap(va, pmeg);
		i = NPTESG;
		do {
			int pte = getpte4(va);
			EXPEDITE(&pte, sizeof(pte));
			va += NBPG;
		} while (--i > 0);
	}
	setsegmap(0, seginval);
#endif

out:
	if (bp != buffer)
		error = (*dump)(dumpdev, blkno++, (caddr_t)buffer, dbtob(1));

	return (error);
}

#ifdef EXTREME_DEBUG

static void test_region __P((int, int, int));

void
debug_pagetables()
{
	register int i;
	register int *regtbl;
	register int te;

	printf("\nncontext=%d. ",ncontext);
	printf("Context table is at va 0x%x. Level 0 PTP: 0x%x\n",
	       ctx_phys_tbl, ctx_phys_tbl[0]);
	printf("Context 0 region table is at va 0x%x, pa 0x%x. Contents:\n",
	       pmap_kernel()->pm_reg_ptps, pmap_kernel()->pm_reg_ptps_pa);

	regtbl = pmap_kernel()->pm_reg_ptps;

	printf("PROM vector is at 0x%x\n",promvec);
	printf("PROM reboot routine is at 0x%x\n",promvec->pv_reboot);
	printf("PROM abort routine is at 0x%x\n",promvec->pv_abort);
	printf("PROM halt routine is at 0x%x\n",promvec->pv_halt);

	printf("Testing region 0xfe: ");
	test_region(0xfe,0,16*1024*1024);
	printf("Testing region 0xff: ");
	test_region(0xff,0,16*1024*1024);
	printf("Testing kernel region 0xf8: ");
	test_region(0xf8, 4096, avail_start);
	cngetc();

	for (i = 0; i < SRMMU_L1SIZE; i++) {
		te = regtbl[i];
		if ((te & SRMMU_TETYPE) == SRMMU_TEINVALID)
		    continue;
		printf("Region 0x%x: PTE=0x%x <%s> L2PA=0x%x kernL2VA=0x%x\n",
		       i, te, ((te & SRMMU_TETYPE) == SRMMU_TEPTE ? "pte" :
			       ((te & SRMMU_TETYPE) == SRMMU_TEPTD ? "ptd" :
				((te & SRMMU_TETYPE) == SRMMU_TEINVALID ?
				 "invalid" : "reserved"))),
		       (te & ~0x3) << SRMMU_PPNPASHIFT,
		       pmap_kernel()->pm_regmap[i].rg_seg_ptps);
	}
	printf("Press q to halt...\n");
	if (cngetc()=='q')
	    callrom();
}

static u_int
VA2PAsw(ctx, addr, pte)
	register int ctx;
	register caddr_t addr;
	int *pte;
{
	register int *curtbl;
	register int curpte;

#ifdef EXTREME_EXTREME_DEBUG
	printf("Looking up addr 0x%x in context 0x%x\n",addr,ctx);
#endif
	/* L0 */
	*pte = curpte = ctx_phys_tbl[ctx];
#ifdef EXTREME_EXTREME_DEBUG
	printf("Got L0 pte 0x%x\n",pte);
#endif
	if ((curpte & SRMMU_TETYPE) == SRMMU_TEPTE) {
		return (((curpte & SRMMU_PPNMASK) << SRMMU_PPNPASHIFT) |
			((u_int)addr & 0xffffffff));
	}
	if ((curpte & SRMMU_TETYPE) != SRMMU_TEPTD) {
		printf("Bad context table entry 0x%x for context 0x%x\n",
		       curpte, ctx);
		return 0;
	}
	/* L1 */
	curtbl = ((curpte & ~0x3) << 4) | (0xf8 << RGSHIFT); /* correct for krn*/
	*pte = curpte = curtbl[VA_VREG(addr)];
#ifdef EXTREME_EXTREME_DEBUG
	printf("L1 table at 0x%x.\nGot L1 pte 0x%x\n",curtbl,curpte);
#endif
	if ((curpte & SRMMU_TETYPE) == SRMMU_TEPTE)
	    return (((curpte & SRMMU_PPNMASK) << SRMMU_PPNPASHIFT) |
		    ((u_int)addr & 0xffffff));
	if ((curpte & SRMMU_TETYPE) != SRMMU_TEPTD) {
		printf("Bad region table entry 0x%x for region 0x%x\n",
		       curpte, VA_VREG(addr));
		return 0;
	}
	/* L2 */
	curtbl = ((curpte & ~0x3) << 4) | (0xf8 << RGSHIFT); /* correct for krn*/
	*pte = curpte = curtbl[VA_VSEG(addr)];
#ifdef EXTREME_EXTREME_DEBUG
	printf("L2 table at 0x%x.\nGot L2 pte 0x%x\n",curtbl,curpte);
#endif
	if ((curpte & SRMMU_TETYPE) == SRMMU_TEPTE)
	    return (((curpte & SRMMU_PPNMASK) << SRMMU_PPNPASHIFT) |
		    ((u_int)addr & 0x3ffff));
	if ((curpte & SRMMU_TETYPE) != SRMMU_TEPTD) {
		printf("Bad segment table entry 0x%x for reg 0x%x, seg 0x%x\n",
		       curpte, VA_VREG(addr), VA_VSEG(addr));
		return 0;
	}
	/* L3 */
	curtbl = ((curpte & ~0x3) << 4) | (0xf8 << RGSHIFT); /* correct for krn*/
	*pte = curpte = curtbl[VA_VPG(addr)];
#ifdef EXTREME_EXTREME_DEBUG
	printf("L3 table at 0x%x.\nGot L3 pte 0x%x\n",curtbl,curpte);
#endif
	if ((curpte & SRMMU_TETYPE) == SRMMU_TEPTE)
	    return (((curpte & SRMMU_PPNMASK) << SRMMU_PPNPASHIFT) |
		    ((u_int)addr & 0xfff));
	else {
		printf("Bad L3 pte 0x%x for reg 0x%x, seg 0x%x, pg 0x%x\n",
		       curpte, VA_VREG(addr), VA_VSEG(addr), VA_VPG(addr));
		return 0;
	}
	printf("Bizarreness with address 0x%x!\n",addr);
}

void test_region(reg, start, stop)
	register int reg;
	register int start, stop;
{
	register int i;
	register int addr;
	register int pte;
	int ptesw;
/*	int cnt=0;
*/

	for (i = start; i < stop; i+= NBPG) {
		addr = (reg << RGSHIFT) | i;
		pte=lda(((u_int)(addr)) | ASI_SRMMUFP_LN, ASI_SRMMUFP);
		if (pte) {
/*			printf("Valid address 0x%x\n",addr);
			if (++cnt == 20) {
				cngetc();
				cnt=0;
			}
*/
			if (VA2PA(addr) != VA2PAsw(0,addr,&ptesw)) {
				printf("Mismatch at address 0x%x.\n",addr);
				if (cngetc()=='q') break;
			}
			if (reg == 0xf8) /* kernel permissions are different */
			    continue;
			if ((pte&SRMMU_PROT_MASK)!=(ptesw&SRMMU_PROT_MASK)) {
				printf("Mismatched protections at address "
				       "0x%x; pte=0x%x, ptesw=0x%x\n",
				       addr,pte,ptesw);
				if (cngetc()=='q') break;
			}
		}
	}
	printf("done.\n");
}


void print_fe_map(void)
{
	u_int i, pte;

	printf("map of region 0xfe:\n");
	for (i = 0xfe000000; i < 0xff000000; i+=4096) {
		if (((pte = getpte4m(i)) & SRMMU_TETYPE) != SRMMU_TEPTE)
		    continue;
		printf("0x%x -> 0x%x%x (pte %x)\n", i, pte >> 28,
		       (pte & ~0xff) << 4, pte);
	}
	printf("done\n");
}

#endif
