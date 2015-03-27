/*	$OpenBSD: pmap.c,v 1.177 2015/03/27 20:25:39 miod Exp $	*/
/*	$NetBSD: pmap.c,v 1.118 1998/05/19 19:00:18 thorpej Exp $ */

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
#include <sys/exec.h>
#include <sys/core.h>
#include <sys/kcore.h>
#include <sys/pool.h>

#include <uvm/uvm_extern.h>

#include <machine/autoconf.h>
#include <machine/bsd_openprom.h>
#include <machine/oldmon.h>
#include <machine/cpu.h>
#include <machine/ctlreg.h>
#include <machine/kcore.h>

#include <sparc/sparc/asm.h>
#include <sparc/sparc/cache.h>
#include <sparc/sparc/vaddrs.h>
#include <sparc/sparc/cpuvar.h>

#ifdef SMALL_KERNEL
/*
 * Force the SRMMU code to be limited to the Sun-4 compatible VM layout.
 * (this is done here to allow installation kernels to be loaded by older
 *  boot blocks which do not map enough data after the kernel image to
 *  cover pmap_bootstrap() needs.)
 */
#define	NKREG_OLD \
	((unsigned int)(-VM_MIN_KERNEL_ADDRESS_OLD / NBPRG))	/* 8 */
#define	NUREG_OLD	(256 - NKREG_OLD)			/* 248 */
#undef	NKREG_4C
#undef	NUREG_4C
#undef	NKREG_4M
#undef	NUREG_4M
#define	NKREG_4C	NKREG_OLD
#define	NUREG_4C	NUREG_OLD
#define	NKREG_4M	NKREG_OLD
#define	NUREG_4M	NUREG_OLD
#endif

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
	int	ps_alias_uncache;	/* # of uncaches due to bad aliases */
	int	ps_alias_recache;	/* # of recaches due to bad aliases */
	int	ps_unlink_pvfirst;	/* # of pv_unlinks on head */
	int	ps_unlink_pvsearch;	/* # of pv_unlink searches */
	int	ps_changeprots;		/* # of calls to changeprot */
	int	ps_useless_changeprots;	/* # of changeprots for wiring */
	int	ps_enter_firstpv;	/* pv heads entered */
	int	ps_enter_secondpv;	/* pv nonheads entered */
	int	ps_useless_changewire;	/* useless wiring changes */
	int	ps_npg_prot_all;	/* # of active pages protected */
	int	ps_npg_prot_actual;	/* # pages actually affected */
	int	ps_npmeg_free;		/* # of free pmegs */
	int	ps_npmeg_locked;	/* # of pmegs on locked list */
	int	ps_npmeg_lru;		/* # of pmegs on lru list */
} pmap_stats;

#ifdef DEBUG
#define	PDB_CREATE	0x0001
#define	PDB_DESTROY	0x0002
#define	PDB_REMOVE	0x0004
#define	PDB_CHANGEPROT	0x0008
#define	PDB_ENTER	0x0010
#define	PDB_FOLLOW	0x0020

#define	PDB_MMU_ALLOC	0x0100
#define	PDB_MMU_STEAL	0x0200
#define	PDB_CTX_ALLOC	0x0400
#define	PDB_CTX_STEAL	0x0800
#define	PDB_MMUREG_ALLOC	0x1000
#define	PDB_MMUREG_STEAL	0x2000
#define	PDB_CACHESTUFF	0x4000
#define	PDB_SWITCHMAP	0x8000
#define	PDB_SANITYCHK	0x10000
int	pmapdebug = 0;
#endif

/*
 * Internal helpers.
 */
static __inline struct pvlist *pvhead(int);

#if defined(SUN4M)
u_int	VA2PA(caddr_t);
#endif

/*
 * Given a page number, return the head of its pvlist.
 */
static __inline struct pvlist *
pvhead(int pnum)
{
	int bank, off;

	bank = vm_physseg_find(pnum, &off);
	if (bank == -1)
		return NULL;

	return &vm_physmem[bank].pgs[off].mdpage.pv_head;
}

struct pool pvpool;

unsigned int nureg, nkreg;
#if (defined(SUN4) || defined(SUN4C) || defined(SUN4E)) && \
    !(defined(SUN4D) || defined(SUN4M))
#define	NUREG	NUREG_4C
#define	NKREG	NKREG_4C
#elif (defined(SUN4D) || defined(SUN4M)) && \
      !(defined(SUN4) || defined(SUN4C) || defined(SUN4E))
#define	NUREG	NUREG_4M
#define	NKREG	NKREG_4M
#else
#define	NUREG	nureg
#define	NKREG	nkreg
#endif

#if defined(SUN4M)
/*
 * Memory pools and back-end supplier for SRMMU page tables.
 * Share a pool between the level 2 and level 3 page tables,
 * since these are equal in size.
 */
static struct pool L1_pool;
static struct pool L23_pool;
void	*pgt_page_alloc(struct pool *, int, int *);
void	 pgt_page_free(struct pool *, void *);

struct pool_allocator pgt_allocator = {
	pgt_page_alloc, pgt_page_free, 0,
};

void    pcache_flush(caddr_t, caddr_t, int);
void
pcache_flush(va, pa, n)
        caddr_t va, pa;
        int     n;
{
        void (*f)(int,int) = cpuinfo.pcache_flush_line;

        while ((n -= 4) >= 0)
                (*f)((u_int)va+n, (u_int)pa+n);
}

/*
 * Page table pool back-end.
 */
void *
pgt_page_alloc(struct pool *pp, int flags, int *slowdown)
{
	extern void	*pool_page_alloc(struct pool *, int, int *);
	void		*pga;

	if ((pga = pool_page_alloc(pp, flags, slowdown)) != NULL &&
	     (cpuinfo.flags & CPUFLG_CACHEPAGETABLES) == 0) {
		pcache_flush((caddr_t)pga, (caddr_t)VA2PA(pga), PAGE_SIZE);
		kvm_uncache((caddr_t)pga, 1);
	}

	return (pga);
}       

void
pgt_page_free(struct pool *pp, void *pga)
{
	extern void	*pool_page_free(struct pool *, void *);
	/*
	 * if we marked the page uncached, we must recache it to go back to
	 * the uvm_km_thread, so other pools don't get uncached pages from us.
	 */
	if ((cpuinfo.flags & CPUFLG_CACHEPAGETABLES) == 0)
		kvm_recache((caddr_t)pga, 1);
	pool_page_free(pp, pga);
	
}
#endif /* SUN4M */

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
 *
 * The Reference MMU generally uses a Translation Look-aside Buffer (TLB)
 * to cache the result of recently executed page table walks. When
 * manipulating page tables, we need to ensure consistency of the
 * in-memory and TLB copies of the page table entries. This is handled
 * by flushing (and invalidating) a TLB entry when appropriate before
 * altering an in-memory page table entry.
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

int	seginval;		/* [4/4c] the invalid segment number */
int	reginval;		/* [4/3mmu] the invalid region number */

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

#define ncontext	(cpuinfo.mmu_ncontext)
#define ctx_kick	(cpuinfo.ctx_kick)
#define ctx_kickdir	(cpuinfo.ctx_kickdir)
#define ctx_freelist	(cpuinfo.ctx_freelist)

#if 0
union ctxinfo *ctxinfo;		/* allocated at in pmap_bootstrap */

union	ctxinfo *ctx_freelist;	/* context free list */
int	ctx_kick;		/* allocation rover when none free */
int	ctx_kickdir;		/* ctx_kick roves both directions */

char	*ctxbusyvector;		/* [4m] tells what contexts are busy (XXX)*/
#endif

caddr_t	vpage[2];		/* two reserved MD virtual pages */

smeg_t		tregion;	/* [4/3mmu] Region for temporary mappings */

struct pmap	kernel_pmap_store;		/* the kernel's pmap */
struct regmap	kernel_regmap_store[NKREG_MAX];	/* the kernel's regmap */
struct segmap	kernel_segmap_store[NKREG_MAX*NSEGRG];/* the kernel's segmaps */

#if defined(SUN4M)
u_int 	*kernel_regtable_store;		/* 8k of storage to map the kernel */
u_int	*kernel_segtable_store;		/* 16k of storage to map the kernel */
u_int	*kernel_pagtable_store;		/* 1M of storage to map the kernel */
#endif

struct	memarr *pmemarr;		/* physical memory regions */
int	npmemarr;			/* number of entries in pmemarr */

vaddr_t avail_start;			/* first available physical page */
vaddr_t	virtual_avail;			/* first free virtual page number */
vaddr_t	virtual_end;			/* last free virtual page number */
paddr_t phys_avail;			/* first free physical page
					   XXX - pmap_pa_exists needs this */
vaddr_t pagetables_start, pagetables_end;

static void pmap_page_upload(void);
void pmap_release(pmap_t);

#if defined(SUN4) || defined(SUN4C) || defined(SUN4E)
int mmu_has_hole;
#endif

vaddr_t prom_vstart;	/* For /dev/kmem */
vaddr_t prom_vend;

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
#define getcontext4()		lduba(AC_CONTEXT, ASI_CONTROL)
#define getcontext4m()		lda(SRMMU_CXR, ASI_SRMMU)
#define getcontext()		(CPU_ISSUN4M \
					? getcontext4m() \
					: getcontext4()  )

#define setcontext4(c)		stba(AC_CONTEXT, ASI_CONTROL, c)
#define setcontext4m(c)		sta(SRMMU_CXR, ASI_SRMMU, c)
#define setcontext(c)		(CPU_ISSUN4M \
					? setcontext4m(c) \
					: setcontext4(c)  )

#define	getsegmap(va)		(CPU_ISSUN4 \
					? (lduha(va, ASI_SEGMAP) & segfixmask) \
					: lduba(va, ASI_SEGMAP))
#define	setsegmap(va, pmeg)	(CPU_ISSUN4 \
					? stha(va, ASI_SEGMAP, pmeg) \
					: stba(va, ASI_SEGMAP, pmeg))

/* 3-level sun4 MMU only: */
#define	getregmap(va)		((unsigned)lduha((va)+2, ASI_REGMAP) >> 8)
#define	setregmap(va, smeg)	stha((va)+2, ASI_REGMAP, (smeg << 8))

#if defined(SUN4M)
#define getpte4m(va)		lda((va & 0xFFFFF000) | ASI_SRMMUFP_L3, \
				    ASI_SRMMUFP)
u_int	*getptep4m(struct pmap *, vaddr_t);
static __inline void	setpgt4m(int *, int);
void	setpte4m(vaddr_t va, int pte);
#endif

#if defined(SUN4) || defined(SUN4C) || defined(SUN4E)
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
void mmu_setup4m_L1(int, struct pmap *);
void mmu_setup4m_L2(int, struct regmap *);
void  mmu_setup4m_L3(int, struct segmap *);
void	mmu_reservemon4m(struct pmap *);

void	pmap_rmk4m(struct pmap *, vaddr_t, vaddr_t, int, int);
void	pmap_rmu4m(struct pmap *, vaddr_t, vaddr_t, int, int);
int	pmap_enk4m(struct pmap *, vaddr_t, vm_prot_t,
			  int, struct pvlist *, int);
int	pmap_enu4m(struct pmap *, vaddr_t, vm_prot_t,
			  int, struct pvlist *, int);
void	pv_changepte4m(struct pvlist *, int, int);
int	pv_syncflags4m(struct pvlist *);
int	pv_link4m(struct pvlist *, struct pmap *, vaddr_t, int);
void	pv_unlink4m(struct pvlist *, struct pmap *, vaddr_t);
#endif

#if defined(SUN4) || defined(SUN4C) || defined(SUN4E)
void	mmu_reservemon4_4c(int *, int *);
void	pmap_rmk4_4c(struct pmap *, vaddr_t, vaddr_t, int, int);
void	pmap_rmu4_4c(struct pmap *, vaddr_t, vaddr_t, int, int);
int	pmap_enk4_4c(struct pmap *, vaddr_t, vm_prot_t,
			  int, struct pvlist *, int);
int	pmap_enu4_4c(struct pmap *, vaddr_t, vm_prot_t,
			  int, struct pvlist *, int);
void	pv_changepte4_4c(struct pvlist *, int, int);
int	pv_syncflags4_4c(struct pvlist *);
int	pv_link4_4c(struct pvlist *, struct pmap *, vaddr_t, int);
void	pv_unlink4_4c(struct pvlist *, struct pmap *, vaddr_t);
#endif

#if !(defined(SUN4D) || defined(SUN4M)) && (defined(SUN4) || defined(SUN4C) || defined(SUN4E))
#define		pmap_rmk	pmap_rmk4_4c
#define		pmap_rmu	pmap_rmu4_4c

#elif (defined(SUN4D) || defined(SUN4M)) && !(defined(SUN4) || defined(SUN4C) || defined(SUN4E))
#define		pmap_rmk	pmap_rmk4m
#define		pmap_rmu	pmap_rmu4m

#else  /* must use function pointers */

/* function pointer declarations */
/* from pmap.h: */
boolean_t	(*pmap_clear_modify_p)(struct vm_page *);
boolean_t	(*pmap_clear_reference_p)(struct vm_page *);
int		(*pmap_enter_p)(pmap_t, vaddr_t, paddr_t, vm_prot_t, int);
boolean_t	(*pmap_extract_p)(pmap_t, vaddr_t, paddr_t *);
boolean_t	(*pmap_is_modified_p)(struct vm_page *);
boolean_t	(*pmap_is_referenced_p)(struct vm_page *);
void		(*pmap_kenter_pa_p)(vaddr_t, paddr_t, vm_prot_t);
void		(*pmap_page_protect_p)(struct vm_page *, vm_prot_t);
void		(*pmap_protect_p)(pmap_t, vaddr_t, vaddr_t, vm_prot_t);
void		(*pmap_copy_page_p)(struct vm_page *, struct vm_page *);
void            (*pmap_zero_page_p)(struct vm_page *);
void	       	(*pmap_changeprot_p)(pmap_t, vaddr_t, vm_prot_t, int);
/* local: */
void 		(*pmap_rmk_p)(struct pmap *, vaddr_t, vaddr_t, int, int);
void 		(*pmap_rmu_p)(struct pmap *, vaddr_t, vaddr_t, int, int);

#define		pmap_rmk	(*pmap_rmk_p)
#define		pmap_rmu	(*pmap_rmu_p)

#endif

/* --------------------------------------------------------------*/

/*
 * Next we have some Sun4m-specific routines which have no 4/4c
 * counterparts, or which are 4/4c macros.
 */

#if defined(SUN4M)

/*
 * Macros which implement SRMMU TLB flushing/invalidation
 */

#define tlb_flush_page(va)	\
	sta(((vaddr_t)(va) & ~0xfff) | ASI_SRMMUFP_L3, ASI_SRMMUFP,0)
#define tlb_flush_segment(vr, vs)	\
	sta(((vr)<<RGSHIFT) | ((vs)<<SGSHIFT) | ASI_SRMMUFP_L2, ASI_SRMMUFP,0)
#define tlb_flush_context()   sta(ASI_SRMMUFP_L1, ASI_SRMMUFP, 0)
#define tlb_flush_all()	      sta(ASI_SRMMUFP_LN, ASI_SRMMUFP, 0)

/*
 * VA2PA(addr) -- converts a virtual address to a physical address using
 * the MMU's currently-installed page tables. As a side effect, the address
 * translation used may cause the associated pte to be encached. The correct
 * context for VA must be set before this is called.
 *
 * This routine should work with any level of mapping, as it is used
 * during bootup to interact with the ROM's initial L1 mapping of the kernel.
 */
u_int
VA2PA(addr)
	caddr_t addr;
{
	u_int pte;

	/* we'll use that handy SRMMU flush/probe! %%%: make consts below! */
	/* Try each level in turn until we find a valid pte. Otherwise panic */

	pte = lda(((u_int)addr & ~0xfff) | ASI_SRMMUFP_L3, ASI_SRMMUFP);
	/* Unlock fault status; required on Hypersparc modules */
	(void)lda(SRMMU_SFSR, ASI_SRMMU);
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
 * Get the pointer to the pte for the given (pmap, va).
 *
 * Assumes level 3 mapping (for now).
 */
u_int *
getptep4m(pm, va)
        struct pmap *pm;
        vaddr_t va;
{
        struct regmap *rm;
        struct segmap *sm;
        int vr, vs;
        vr = VA_VREG(va);
        vs = VA_VSEG(va);

        rm = &pm->pm_regmap[vr];
#ifdef notyet
        if ((rm->rg_seg_ptps[vs] & SRMMU_TETYPE) == SRMMU_TEPTE)
                return &rm->rg_seg_ptps[vs];
#endif
	if (rm->rg_segmap == NULL)
		return NULL;

        sm = &rm->rg_segmap[vs];

	if (sm->sg_pte == NULL)
		return NULL;

        return &sm->sg_pte[VA_SUN4M_VPG(va)];
}

/*
 * Set the pte at "ptep" to "pte".
 */
static __inline void
setpgt4m(ptep, pte)
	int *ptep;
	int pte;
{
	swap(ptep, pte);
}

/*
 * Set the page table entry for va to pte. Only legal for kernel mappings.
 */
void
setpte4m(va, pte)
	vaddr_t va;
	int pte;
{
	int *ptep;

	ptep = getptep4m(pmap_kernel(), va);
	tlb_flush_page(va);
	setpgt4m(ptep, pte);
}

/*
 * Translation table for kernel vs. PTE protection bits.
 */
u_int protection_codes[2][8];
#define pte_prot4m(pm, prot) (protection_codes[pm == pmap_kernel()?0:1][prot])

void
sparc_protection_init4m(void)
{
	u_int prot, *kp, *up;

	kp = protection_codes[0];
	up = protection_codes[1];

	for (prot = 0; prot < 8; prot++) {
		switch (prot) {
		case PROT_READ | PROT_WRITE | PROT_EXEC:
			kp[prot] = PPROT_N_RWX;
			up[prot] = PPROT_RWX_RWX;
			break;
		case PROT_READ | PROT_WRITE | PROT_NONE:
			kp[prot] = PPROT_N_RWX;
			up[prot] = PPROT_RW_RW;
			break;
		case PROT_READ | PROT_NONE  | PROT_EXEC:
			kp[prot] = PPROT_N_RX;
			up[prot] = PPROT_RX_RX;
			break;
		case PROT_READ | PROT_NONE  | PROT_NONE:
			kp[prot] = PPROT_N_RX;
			up[prot] = PPROT_R_R;
			break;
		case PROT_NONE | PROT_WRITE | PROT_EXEC:
			kp[prot] = PPROT_N_RWX;
			up[prot] = PPROT_RWX_RWX;
			break;
		case PROT_NONE | PROT_WRITE | PROT_NONE:
			kp[prot] = PPROT_N_RWX;
			up[prot] = PPROT_RW_RW;
			break;
		case PROT_NONE | PROT_NONE  | PROT_EXEC:
			kp[prot] = PPROT_N_RX;
			up[prot] = PPROT_X_X;
			break;
		case PROT_NONE | PROT_NONE  | PROT_NONE:
			kp[prot] = PPROT_N_RX;
			up[prot] = PPROT_R_R;
			break;
		}
	}
}

#endif /* 4m only */

/*----------------------------------------------------------------*/

/*
 * The following three macros are to be used in sun4/sun4c code only.
 */
#if defined(SUN4_MMU3L)
#define CTX_USABLE(pm,rp) (					\
		((pm)->pm_ctx != NULL &&			\
		 (!HASSUN4_MMU3L || (rp)->rg_smeg != reginval))	\
)
#else
#define CTX_USABLE(pm,rp)	((pm)->pm_ctx != NULL )
#endif

#define GAP_WIDEN(pm,vr) do if (CPU_ISSUN4OR4COR4E) {	\
	if (vr + 1 == pm->pm_gap_start)			\
		pm->pm_gap_start = vr;			\
	if (vr == pm->pm_gap_end)			\
		pm->pm_gap_end = vr + 1;		\
} while (0)

#define GAP_SHRINK(pm,vr) do if (CPU_ISSUN4OR4COR4E) {			\
	int x;							\
	x = pm->pm_gap_start + (pm->pm_gap_end - pm->pm_gap_start) / 2;	\
	if (vr > x) {							\
		if (vr < pm->pm_gap_end)				\
			pm->pm_gap_end = vr;				\
	} else {							\
		if (vr >= pm->pm_gap_start && x != pm->pm_gap_start)	\
			pm->pm_gap_start = vr + 1;			\
	}								\
} while (0)


void get_phys_mem(void **);
void	ctx_alloc(struct pmap *);
void	ctx_free(struct pmap *);
void	pg_flushcache(struct vm_page *);
#ifdef DEBUG
void	pm_check(char *, struct pmap *);
void	pm_check_k(char *, struct pmap *);
void	pm_check_u(char *, struct pmap *);
#endif

/*
 * During the PMAP bootstrap, we can use a simple translation to map a
 * kernel virtual address to a physical memory address (this is arranged
 * in locore).  Usually, KERNBASE maps to physical address 0. This is always
 * the case on sun4 and sun4c machines (unless the kernel is too large to fit
 * under the second stage bootloader in memory). On sun4m machines, if no
 * memory is installed in the bank corresponding to physical address 0, or
 * again if the kernel is large, the boot blocks may elect to load us at
 * some other address, presumably at the start of the first memory bank that
 * is large enough to hold the kernel image. We set the up the variable
 * `va2pa_offset' to hold the physical address corresponding to KERNBASE.
 */

static u_long va2pa_offset;
#define PMAP_BOOTSTRAP_VA2PA(v) ((paddr_t)((u_long)(v) - va2pa_offset))
#define PMAP_BOOTSTRAP_PA2VA(p) ((vaddr_t)((u_long)(p) + va2pa_offset))

/*
 * Grab physical memory list.
 * While here, compute `physmem'.
 */
void
get_phys_mem(void **top)
{
	struct memarr *mp;
	char *p;
	int i;

	/* Load the memory descriptor array at the current kernel top */
	p = (void *)ALIGN(*top);
	pmemarr = (struct memarr *)p;
	npmemarr = makememarr(pmemarr, 1000, MEMARR_AVAILPHYS);

	/* Update kernel top */
	p += npmemarr * sizeof(struct memarr);
	*top = p;

	for (physmem = 0, mp = pmemarr, i = npmemarr; --i >= 0; mp++) {
#ifdef SUN4D
		if (CPU_ISSUN4D) {
			/*
			 * XXX Limit ourselves to 2GB of physical memory
			 * XXX for now.
			 */
			uint32_t addr, len;
			int skip = 0;

			addr = mp->addr_lo;
			len = mp->len;
			if (mp->addr_hi != 0 || addr >= 0x80000000)
				skip = 1;
			else {
				if (len >= 0x80000000)
					len = 0x80000000;
				if (addr + len > 0x80000000)
					len = 0x80000000 - addr;
			}
			if (skip)
				len = 0;	/* disable this entry */
			mp->len = len;
		}
#endif
		physmem += atop(mp->len);
	}
}

/*
 * Support functions for vm_page_bootstrap();
 */

/*
 * How much virtual space does this kernel have?
 * (After mapping kernel text, data, etc.)
 */
void
pmap_virtual_space(v_start, v_end)
        vaddr_t *v_start;
        vaddr_t *v_end;
{
        *v_start = virtual_avail;
        *v_end   = virtual_end;
}

/*
 * Helper routine that hands off available physical pages to the VM system.
 */
void
pmap_page_upload(void)
{
	int	n;
	paddr_t	start, end;

	for (n = 0; n < npmemarr; n++) {
		start = pmemarr[n].addr_lo;
		end = start + pmemarr[n].len;

		/*
		 * Exclude any memory allocated for the kernel as computed
		 * by pmap_bootstrap(), i.e. the range
		 *	[KERNBASE_PA, avail_start>.
		 */
		if (start < PMAP_BOOTSTRAP_VA2PA(KERNBASE)) {
			/*
			 * This segment starts below the kernel load address.
			 * Chop it off at the start of the kernel.
			 */
			paddr_t	chop = PMAP_BOOTSTRAP_VA2PA(KERNBASE);

			if (end < chop)
				chop = end;
#ifdef DEBUG
			printf("bootstrap gap: start %lx, chop %lx, end %lx\n",
				start, chop, end);
#endif
			uvm_page_physload(atop(start), atop(chop),
				atop(start), atop(chop), 0);

			/*
			 * Adjust the start address to reflect the
			 * uploaded portion of this segment.
			 */
			start = chop;
		}

		/* Skip the current kernel address range */
		if (start <= avail_start && avail_start < end)
			start = avail_start;

		if (start == end)
			continue;

		/* Upload (the rest of) this segment */
		uvm_page_physload(atop(start), atop(end),
			atop(start), atop(end), 0);
	}
}

/*
 * This routine is used by mmrw() to validate access to `/dev/mem'.
 */
int
pmap_pa_exists(paddr_t pa)
{
	int nmem;
	struct memarr *mp;

	for (mp = pmemarr, nmem = npmemarr; --nmem >= 0; mp++) {
#ifdef SUN4D
		if (mp->len == 0)
			continue;
#endif
		if (pa >= mp->addr_lo && pa < mp->addr_lo + mp->len)
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
#if defined(SUN4) || defined(SUN4C) || defined(SUN4E)
void
mmu_reservemon4_4c(nrp, nsp)
	int *nrp, *nsp;
{
	u_int va = 0, eva = 0;
	int mmuseg, i, nr, ns, vr, lastvr;
#if defined(SUN4_MMU3L)
	int mmureg;
#endif
	struct regmap *rp;

#if defined(SUN4)
	if (CPU_ISSUN4) {
		prom_vstart = va = OLDMON_STARTVADDR;
		prom_vend = eva = OLDMON_ENDVADDR;
	}
#endif
#if defined(SUN4C) || defined(SUN4E)
	if (CPU_ISSUN4C || CPU_ISSUN4E) {
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

#if defined(SUN4_MMU3L)
		if (HASSUN4_MMU3L && vr != lastvr) {
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

		if (!HASSUN4_MMU3L)
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
 * the kernel at KERNBASE since we don't want to map 16M of physical
 * memory for the kernel. Thus the kernel must be installed later!
 * Also installs ROM mappings into the kernel pmap.
 * NOTE: This also revokes all user-mode access to the mapped regions.
 */
void
mmu_reservemon4m(kpmap)
	struct pmap *kpmap;
{
	unsigned int rom_ctxtbl;
	int te;
	unsigned int mmupcrsave;

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

	rom_ctxtbl = (lda(SRMMU_CXTPTR,ASI_SRMMU) << SRMMU_PPNPASHIFT);

	/* We're going to have to use MMU passthrough. If we're on a
	 * Viking MicroSparc without an mbus, we need to turn off traps
	 * and set the AC bit at 0x8000 in the MMU's control register. Ugh.
	 * XXX: Once we've done this, can we still access kernel vm?
	 */
	if (cpuinfo.cpu_vers == 4 && cpuinfo.mxcc) {
		sta(SRMMU_PCR, ASI_SRMMU, 	/* set MMU AC bit */
		    ((mmupcrsave = lda(SRMMU_PCR,ASI_SRMMU)) | VIKING_PCR_AC));
	}

	te = lda(rom_ctxtbl, ASI_BYPASS);	/* i.e. context 0 */
	switch (te & SRMMU_TETYPE) {
	case SRMMU_TEINVALID:
		cpuinfo.ctx_tbl[0] = SRMMU_TEINVALID;
		panic("mmu_reservemon4m: no existing L0 mapping! "
		      "(How are we running?");
		break;
	case SRMMU_TEPTE:
#ifdef DEBUG
		printf("mmu_reservemon4m: trying to remap 4G segment!\n");
#endif
		panic("mmu_reservemon4m: can't handle ROM 4G page size");
		/* XXX: Should make this work, however stupid it is */
		break;
	case SRMMU_TEPTD:
		mmu_setup4m_L1(te, kpmap);
		break;
	default:
		panic("mmu_reservemon4m: unknown pagetable entry type");
	}

	if (cpuinfo.cpu_vers == 4 && cpuinfo.mxcc) {
		sta(SRMMU_PCR, ASI_SRMMU, mmupcrsave);
	}
}

void
mmu_setup4m_L1(regtblptd, kpmap)
	int regtblptd;		/* PTD for region table to be remapped */
	struct pmap *kpmap;
{
	unsigned int regtblrover;
	int i;
	unsigned int te;
	struct regmap *rp;
	int j, k;

	/*
	 * Here we scan the region table to copy any entries which appear.
	 * We are only concerned with regions in kernel space and above
	 * (i.e. regions VA_VREG(VM_MIN_KERNEL_ADDRESS_SRMMU) == NUREG to 0xff).
	 */
	regtblrover = ((regtblptd & ~SRMMU_TETYPE) << SRMMU_PPNPASHIFT) +
	    NUREG_4M * sizeof(long); /* kernel only */

	for (i = NUREG_4M; i < SRMMU_L1SIZE; i++, regtblrover += sizeof(long)) {
		/*
		 * Ignore the region spanning the area where the kernel has
		 * been loaded, since this is the 16MB L1 mapping that the ROM
		 * used to map the kernel in initially.
		 * Later, we will rebuild a new L3 mapping for the kernel
		 * and install it before switching to the new pagetables.
		 */
		if (i == VA_VREG(KERNBASE))
			continue;

		/* The region we're dealing with */
		rp = &kpmap->pm_regmap[i];

		te = lda(regtblrover, ASI_BYPASS);
		switch(te & SRMMU_TETYPE) {
		case SRMMU_TEINVALID:
			break;

		case SRMMU_TEPTE:
#ifdef DEBUG
			printf("mmu_setup4m_L1: "
			       "converting region 0x%x from L1->L3\n", i);
#endif
			/*
			 * This region entry covers 64MB of memory -- or
			 * (NSEGRG * NPTESG) pages -- which we must convert
			 * into a 3-level description.
			 */

			for (j = 0; j < SRMMU_L2SIZE; j++) {
				struct segmap *sp = &rp->rg_segmap[j];

				for (k = 0; k < SRMMU_L3SIZE; k++) {
					sp->sg_npte++;
					setpgt4m(&sp->sg_pte[k],
					    (te & SRMMU_L1PPNMASK) |
					    (j << SRMMU_L2PPNSHFT) |
					    (k << SRMMU_L3PPNSHFT) |
					    (te & SRMMU_PGBITSMSK) |
					    ((te & SRMMU_PROT_MASK) |
					     PPROT_U2S_OMASK) |
					    SRMMU_TEPTE);
				}
			}
			break;

		case SRMMU_TEPTD:
			mmu_setup4m_L2(te, rp);
			break;

		default:
			panic("mmu_setup4m_L1: unknown pagetable entry type");
		}
	}
}

void
mmu_setup4m_L2(segtblptd, rp)
	int segtblptd;
	struct regmap *rp;
{
	unsigned int segtblrover;
	int i, k;
	unsigned int te;
	struct segmap *sp;

	segtblrover = (segtblptd & ~SRMMU_TETYPE) << SRMMU_PPNPASHIFT;
	for (i = 0; i < SRMMU_L2SIZE; i++, segtblrover += sizeof(long)) {

		sp = &rp->rg_segmap[i];

		te = lda(segtblrover, ASI_BYPASS);
		switch(te & SRMMU_TETYPE) {
		case SRMMU_TEINVALID:
			break;

		case SRMMU_TEPTE:
#ifdef DEBUG
			printf("mmu_setup4m_L2: converting L2 entry at segment 0x%x to L3\n",i);
#endif
			/*
			 * This segment entry covers 256KB of memory -- or
			 * (NPTESG) pages -- which we must convert
			 * into a 3-level description.
			 */
			for (k = 0; k < SRMMU_L3SIZE; k++) {
				sp->sg_npte++;
				setpgt4m(&sp->sg_pte[k],
				    (te & SRMMU_L1PPNMASK) |
				    (te & SRMMU_L2PPNMASK) |
				    (k << SRMMU_L3PPNSHFT) |
				    (te & SRMMU_PGBITSMSK) |
				    ((te & SRMMU_PROT_MASK) |
				     PPROT_U2S_OMASK) |
				    SRMMU_TEPTE);
			}
			break;

		case SRMMU_TEPTD:
			mmu_setup4m_L3(te, sp);
			break;

		default:
			panic("mmu_setup4m_L2: unknown pagetable entry type");
		}
	}
}

void
mmu_setup4m_L3(pagtblptd, sp)
	int pagtblptd;
	struct segmap *sp;
{
	unsigned int pagtblrover;
	int i;
	unsigned int te;

	pagtblrover = (pagtblptd & ~SRMMU_TETYPE) << SRMMU_PPNPASHIFT;
	for (i = 0; i < SRMMU_L3SIZE; i++, pagtblrover += sizeof(long)) {
		te = lda(pagtblrover, ASI_BYPASS);
		switch(te & SRMMU_TETYPE) {
		case SRMMU_TEINVALID:
			break;
		case SRMMU_TEPTE:
			sp->sg_npte++;
			setpgt4m(&sp->sg_pte[i], te | PPROT_U2S_OMASK);
			pmap_kernel()->pm_stats.resident_count++;
			break;
		case SRMMU_TEPTD:
			panic("mmu_setup4m_L3: PTD found in L3 page table");
		default:
			panic("mmu_setup4m_L3: unknown pagetable entry type");
		}
	}
}
#endif /* defined SUN4M */

/*----------------------------------------------------------------*/

/*
 * MMU management.
 */

#if defined(SUN4) || defined(SUN4C) || defined(SUN4E) /* This is old sun MMU stuff */

/*
 * Change contexts.  We need the old context number as well as the new
 * one.  If the context is changing, we must write all user windows
 * first, lest an interrupt cause them to be written to the (other)
 * user whose context we set here.
 */
#define	CHANGE_CONTEXTS(old, new) \
	if ((old) != (new)) { \
		write_user_windows(); \
		setcontext4(new); \
	}

struct mmuentry *me_alloc(struct mmuhd *, struct pmap *, int, int);
void		me_free(struct pmap *, u_int);
struct mmuentry	*region_alloc(struct mmuhd *, struct pmap *, int);
void		region_free(struct pmap *, u_int);

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
	struct mmuhd *mh;
	struct pmap *newpm;
	int newvreg, newvseg;
{
	struct mmuentry *me;
	struct pmap *pm;
	int i, va, *pte, tpte;
	int ctx;
	struct regmap *rp;
	struct segmap *sp;

	/* try free list first */
	if (!TAILQ_EMPTY(&segm_freelist)) {
		me = TAILQ_FIRST(&segm_freelist);
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
#ifdef DIAGNOSTIC
		pmap_stats.ps_npmeg_free--;
		if (mh == &segm_locked)
			pmap_stats.ps_npmeg_locked++;
		else
			pmap_stats.ps_npmeg_lru++;
#endif

		/* into pmap segment table, with backpointers */
		newpm->pm_regmap[newvreg].rg_segmap[newvseg].sg_pmeg = me->me_cookie;
		me->me_pmap = newpm;
		me->me_vseg = newvseg;
		me->me_vreg = newvreg;

		return (me);
	}

	/* no luck, take head of LRU list */
	if ((me = TAILQ_FIRST(&segm_lru)) == NULL)
		panic("me_alloc: all pmegs gone");

	pm = me->me_pmap;
	if (pm == NULL)
		panic("me_alloc: LRU entry has no pmap");
	if (pm == pmap_kernel())
		panic("me_alloc: stealing from kernel");
#ifdef DEBUG
	if (pmapdebug & (PDB_MMU_ALLOC | PDB_MMU_STEAL))
		printf("me_alloc: stealing pmeg 0x%x from pmap %p\n",
		    me->me_cookie, pm);
#endif
	/*
	 * Remove from LRU list, and insert at end of new list
	 * (probably the LRU list again, but so what?).
	 */
	TAILQ_REMOVE(&segm_lru, me, me_list);
	TAILQ_INSERT_TAIL(mh, me, me_list);

#ifdef DIAGNOSTIC
	if (mh == &segm_locked) {
		pmap_stats.ps_npmeg_lru--;
		pmap_stats.ps_npmeg_locked++;
	}
#endif

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
	ctx = getcontext4();
	if (CTX_USABLE(pm,rp)) {
		CHANGE_CONTEXTS(ctx, pm->pm_ctxnum);
		cache_flush_segment(me->me_vreg, me->me_vseg);
		va = VSTOVA(me->me_vreg,me->me_vseg);
	} else {
		CHANGE_CONTEXTS(ctx, 0);
		if (HASSUN4_MMU3L)
			setregmap(0, tregion);
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
			struct pvlist *pv;

			pv = pvhead(tpte & PG_PFNUM);
			if (pv)
				pv->pv_flags |= MR4_4C(tpte);
		}
		*pte++ = tpte & ~(PG_U|PG_M);
		va += NBPG;
	} while (--i > 0);

	/* update segment tables */
	if (CTX_USABLE(pm,rp))
		setsegmap(VSTOVA(me->me_vreg,me->me_vseg), seginval);
	sp->sg_pmeg = seginval;

	/* off old pmap chain */
	TAILQ_REMOVE(&pm->pm_seglist, me, me_pmchain);
	setcontext4(ctx);	/* done with old context */

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
	struct pmap *pm;
	u_int pmeg;
{
	struct mmuentry *me = &mmusegments[pmeg];
	int i, va, tpte;
	int vr;
	struct regmap *rp;

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
if (getcontext4() != 0) panic("me_free: ctx != 0");
#endif
		if (HASSUN4_MMU3L)
			setregmap(0, tregion);
		setsegmap(0, me->me_cookie);
		va = 0;
	}
	i = NPTESG;
	do {
		tpte = getpte4(va);
		if ((tpte & (PG_V | PG_TYPE)) == (PG_V | PG_OBMEM)) {
			struct pvlist *pv;

			pv = pvhead(tpte & PG_PFNUM);
			if (pv)
				pv->pv_flags |= MR4_4C(tpte);
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
#ifdef DIAGNOSTIC
		pmap_stats.ps_npmeg_locked--;
#endif
	} else {
		TAILQ_REMOVE(&segm_lru, me, me_list);
#ifdef DIAGNOSTIC
		pmap_stats.ps_npmeg_lru--;
#endif
	}

	/* no associated pmap; on free list */
	me->me_pmap = NULL;
	TAILQ_INSERT_TAIL(&segm_freelist, me, me_list);
#ifdef DIAGNOSTIC
	pmap_stats.ps_npmeg_free++;
#endif
}

#if defined(SUN4_MMU3L)

/* XXX - Merge with segm_alloc/segm_free ? */

struct mmuentry *
region_alloc(mh, newpm, newvr)
	struct mmuhd *mh;
	struct pmap *newpm;
	int newvr;
{
	struct mmuentry *me;
	struct pmap *pm;
	int ctx;
	struct regmap *rp;

	/* try free list first */
	if (!TAILQ_EMPTY(&region_freelist)) {
		me = TAILQ_FIRST(&region_freelist);
		TAILQ_REMOVE(&region_freelist, me, me_list);
#ifdef DEBUG
		if (me->me_pmap != NULL)
			panic("region_alloc: freelist entry has pmap");
		if (pmapdebug & PDB_MMUREG_ALLOC)
			printf("region_alloc: got smeg 0x%x\n", me->me_cookie);
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
	if ((me = TAILQ_FIRST(&region_lru)) == NULL)
		panic("region_alloc: all smegs gone");

	pm = me->me_pmap;
	if (pm == NULL)
		panic("region_alloc: LRU entry has no pmap");
	if (pm == pmap_kernel())
		panic("region_alloc: stealing from kernel");
#ifdef DEBUG
	if (pmapdebug & (PDB_MMUREG_ALLOC | PDB_MMUREG_STEAL))
		printf("region_alloc: stealing smeg 0x%x from pmap %p\n",
		    me->me_cookie, pm);
#endif
	/*
	 * Remove from LRU list, and insert at end of new list
	 * (probably the LRU list again, but so what?).
	 */
	TAILQ_REMOVE(&region_lru, me, me_list);
	TAILQ_INSERT_TAIL(mh, me, me_list);

	rp = &pm->pm_regmap[me->me_vreg];
	ctx = getcontext4();
	if (pm->pm_ctx) {
		CHANGE_CONTEXTS(ctx, pm->pm_ctxnum);
		cache_flush_region(me->me_vreg);
	}

	/* update region tables */
	if (pm->pm_ctx)
		setregmap(VRTOVA(me->me_vreg), reginval);
	rp->rg_smeg = reginval;

	/* off old pmap chain */
	TAILQ_REMOVE(&pm->pm_reglist, me, me_pmchain);
	setcontext4(ctx);	/* done with old context */

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
	struct pmap *pm;
	u_int smeg;
{
	struct mmuentry *me = &mmuregions[smeg];

#ifdef DEBUG
	if (pmapdebug & PDB_MMUREG_ALLOC)
		printf("region_free: freeing smeg 0x%x from pmap %p\n",
		    me->me_cookie, pm);
	if (me->me_cookie != smeg)
		panic("region_free: wrong mmuentry");
	if (pm != me->me_pmap)
		panic("region_free: pm != me_pmap");
#endif

	if (pm->pm_ctx)
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
	struct pmap *pm;
	vaddr_t va;
	int prot;
{
	int *pte;
	int vr, vs, pmeg, i, s, bits;
	struct regmap *rp;
	struct segmap *sp;

	if (prot != PROT_NONE)
		bits = PG_V | ((prot & PROT_WRITE) ? PG_W : 0);
	else
		bits = 0;

	vr = VA_VREG(va);
	vs = VA_VSEG(va);
	rp = &pm->pm_regmap[vr];
#ifdef DEBUG
if (pm == pmap_kernel())
printf("mmu_pagein: kernel wants map at va 0x%lx, vr %d, vs %d\n", va, vr, vs);
#endif

	/* return 0 if we have no PMEGs to load */
	if (rp->rg_segmap == NULL)
		return (0);

#if defined(SUN4_MMU3L)
	if (HASSUN4_MMU3L && rp->rg_smeg == reginval) {
		smeg_t smeg;
		unsigned int tva = VA_ROUNDDOWNTOREG(va);
		struct segmap *sp = rp->rg_segmap;

		s = splvm();		/* paranoid */
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
	s = splvm();		/* paranoid */
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
#endif /* SUN4 || SUN4C || SUN4E */

/*
 * Allocate a context.  If necessary, steal one from someone else.
 * Changes hardware context number and loads segment map.
 *
 * This routine is only ever called from locore.s just after it has
 * saved away the previous process, so there are no active user windows.
 */
void
ctx_alloc(pm)
	struct pmap *pm;
{
	union ctxinfo *c;
	int s, cnum, i, doflush;
	struct regmap *rp;
	int gap_start, gap_end;
	unsigned long va;

#ifdef DEBUG
	if (pm->pm_ctx)
		panic("ctx_alloc pm_ctx");
	if (pmapdebug & PDB_CTX_ALLOC)
		printf("ctx_alloc(%p)\n", pm);
#endif
	if (CPU_ISSUN4OR4COR4E) {
		gap_start = pm->pm_gap_start;
		gap_end = pm->pm_gap_end;
	}

	s = splvm();
	if ((c = ctx_freelist) != NULL) {
		ctx_freelist = c->c_nextfree;
		cnum = c - cpuinfo.ctxinfo;
		doflush = 0;
	} else {
		if ((ctx_kick += ctx_kickdir) >= ncontext) {
			ctx_kick = ncontext - 1;
			ctx_kickdir = -1;
		} else if (ctx_kick < 1) {
			ctx_kick = 1;
			ctx_kickdir = 1;
		}
		c = &cpuinfo.ctxinfo[cnum = ctx_kick];
#ifdef DEBUG
		if (c->c_pmap == NULL)
			panic("ctx_alloc cu_pmap");
		if (pmapdebug & (PDB_CTX_ALLOC | PDB_CTX_STEAL))
			printf("ctx_alloc: steal context %d from %p\n",
			    cnum, c->c_pmap);
#endif
		c->c_pmap->pm_ctx = NULL;
		doflush = (CACHEINFO.c_vactype != VAC_NONE);
		if (CPU_ISSUN4OR4COR4E) {
			if (gap_start < c->c_pmap->pm_gap_start)
				gap_start = c->c_pmap->pm_gap_start;
			if (gap_end > c->c_pmap->pm_gap_end)
				gap_end = c->c_pmap->pm_gap_end;
		}
	}

	c->c_pmap = pm;
	pm->pm_ctx = c;
	pm->pm_ctxnum = cnum;

	if (CPU_ISSUN4OR4COR4E) {
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

		setcontext4(cnum);
		if (doflush)
			cache_flush_context();

		rp = pm->pm_regmap;
		for (va = 0, i = NUREG_4C; --i >= 0; ) {
			if (VA_VREG(va) >= gap_start) {
				va = VRTOVA(gap_end);
				i -= gap_end - gap_start;
				rp += gap_end - gap_start;
				if (i < 0)
					break;
				/* mustn't re-enter this branch */
				gap_start = NUREG_4C;
			}
			if (HASSUN4_MMU3L) {
				setregmap(va, rp++->rg_smeg);
				va += NBPRG;
			} else {
				int j;
				struct segmap *sp = rp->rg_segmap;
				for (j = NSEGRG; --j >= 0; va += NBPSG)
					setsegmap(va,
						  sp?sp++->sg_pmeg:seginval);
				rp++;
			}
		}
		splx(s);

	} else if (CPU_ISSUN4M) {

#if defined(SUN4M)
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

		/* Do any cache flush needed on context switch */
		(*cpuinfo.pure_vcache_flush)();
#ifdef DEBUG
#if 0
		ctxbusyvector[cnum] = 1; /* mark context as busy */
#endif
		if (pm->pm_reg_ptps_pa == 0)
			panic("ctx_alloc: no region table in current pmap");
#endif
		/*setcontext(0); * paranoia? can we modify curr. ctx? */
		setpgt4m(&cpuinfo.ctx_tbl[cnum],
			(pm->pm_reg_ptps_pa >> SRMMU_PPNPASHIFT) | SRMMU_TEPTD);

		setcontext4m(cnum);
		if (doflush)
			cache_flush_context();
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
	union ctxinfo *c;
	int newc, oldc;

	if ((c = pm->pm_ctx) == NULL)
		panic("ctx_free");
	pm->pm_ctx = NULL;

	if (CPU_ISSUN4M) {
#if defined(SUN4M)
		oldc = getcontext4m();
		/* Do any cache flush needed on context switch */
		(*cpuinfo.pure_vcache_flush)();
		newc = pm->pm_ctxnum;
		if (oldc != newc) {
			write_user_windows();
			setcontext4m(newc);
		}
		cache_flush_context();
		tlb_flush_context();
		setcontext4m(0);
#endif
	} else {
#if defined(SUN4) || defined(SUN4C) || defined(SUN4E)
		oldc = getcontext4();
		if (CACHEINFO.c_vactype != VAC_NONE) {
			newc = pm->pm_ctxnum;
			CHANGE_CONTEXTS(oldc, newc);
			cache_flush_context();
			setcontext4(0);
		} else {
			CHANGE_CONTEXTS(oldc, 0);
		}
#endif
	}

	c->c_nextfree = ctx_freelist;
	ctx_freelist = c;

#if 0
#if defined(SUN4M)
	if (CPU_ISSUN4M) {
		/* Map kernel back into unused context */
		newc = pm->pm_ctxnum;
		cpuinfo.ctx_tbl[newc] = cpuinfo.ctx_tbl[0];
		if (newc)
			ctxbusyvector[newc] = 0; /* mark as free */
	}
#endif
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
 *
 * In addition, if the cacheable bit (PG_NC) is updated in the PTE
 * the corresponding PV_NC flag is also updated in each pv entry. This
 * is done so kvm_uncache() can use this routine and have the uncached
 * status stick.
 */

#if defined(SUN4) || defined(SUN4C) || defined(SUN4E)

void
pv_changepte4_4c(pv0, bis, bic)
	struct pvlist *pv0;
	int bis, bic;
{
	int *pte;
	struct pvlist *pv;
	struct pmap *pm;
	int va, vr, vs;
	int ctx, s;
	struct regmap *rp;
	struct segmap *sp;

	write_user_windows();		/* paranoid? */

	s = splvm();			/* paranoid? */
	if (pv0->pv_pmap == NULL) {
		splx(s);
		return;
	}
	ctx = getcontext4();
	for (pv = pv0; pv != NULL; pv = pv->pv_next) {
		pm = pv->pv_pmap;
#ifdef DIAGNOSTIC
		if(pm == NULL)
			panic("pv_changepte: pm == NULL");
#endif
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
				panic("pv_changepte: pte == NULL");
			pte += VA_VPG(va);
			*pte = (*pte | bis) & ~bic;
		} else {
			int tpte;

			/* in hardware: fix hardware copy */
			if (CTX_USABLE(pm,rp)) {
				/*
				 * Bizarreness: we never clear PG_NC on
				 * DVMA pages.
				 * XXX should we ever get invoked on such
				 * XXX pages?
				 */
				if (bic == PG_NC &&
				    va >= DVMA_BASE && va < DVMA_END)
					continue;
				setcontext4(pm->pm_ctxnum);
				/* XXX should flush only when necessary */
				tpte = getpte4(va);
				/*
				 * XXX: always flush cache; conservative, but
				 * needed to invalidate cache tag protection
				 * bits and when disabling caching.
				 */
				cache_flush_page(va);
			} else {
				/* XXX per-cpu va? */
				setcontext4(0);
				if (HASSUN4_MMU3L)
					setregmap(0, tregion);
				setsegmap(0, sp->sg_pmeg);
				va = VA_VPG(va) << PGSHIFT;
				tpte = getpte4(va);
			}
			if (tpte & PG_V)
				pv0->pv_flags |= MR4_4C(tpte);
			tpte = (tpte | bis) & ~bic;
			setpte4(va, tpte);
			if (pte != NULL)	/* update software copy */
				pte[VA_VPG(va)] = tpte;

			/* Update PV_NC flag if required */
			if (bis & PG_NC)
				pv->pv_flags |= PV_NC;
			if (bic & PG_NC)
				pv->pv_flags &= ~PV_NC;
		}
	}
	setcontext4(ctx);
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
	struct pvlist *pv0;
{
	struct pvlist *pv;
	struct pmap *pm;
	int tpte, va, vr, vs, pmeg, flags;
	int ctx, s;
	struct regmap *rp;
	struct segmap *sp;

	write_user_windows();		/* paranoid? */

	s = splvm();			/* paranoid? */
	if (pv0->pv_pmap == NULL) {	/* paranoid */
		splx(s);
		return (0);
	}
	ctx = getcontext4();
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
			setcontext4(pm->pm_ctxnum);
			/* XXX should flush only when necessary */
			tpte = getpte4(va);
			if (tpte & PG_M)
				cache_flush_page(va);
		} else {
			/* XXX per-cpu va? */
			setcontext4(0);
			if (HASSUN4_MMU3L)
				setregmap(0, tregion);
			setsegmap(0, pmeg);
			va = VA_VPG(va) << PGSHIFT;
			tpte = getpte4(va);
		}
		if (tpte & (PG_M|PG_U) && tpte & PG_V) {
			flags |= MR4_4C(tpte);
			tpte &= ~(PG_M|PG_U);
			setpte4(va, tpte);
		}
	}
	pv0->pv_flags = flags;
	setcontext4(ctx);
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
void
pv_unlink4_4c(pv, pm, va)
	struct pvlist *pv;
	struct pmap *pm;
	vaddr_t va;
{
	struct pvlist *npv;

#ifdef DIAGNOSTIC
	if (pv->pv_pmap == NULL)
		panic("pv_unlink0");
#endif
	/*
	 * First entry is special (sigh).
	 */
	npv = pv->pv_next;
	if (pv->pv_pmap == pm && pv->pv_va == va) {
		pmap_stats.ps_unlink_pvfirst++;
		if (npv != NULL) {
			/*
			 * Shift next entry into the head.
			 * Make sure to retain the REF, MOD and ANC flags.
			 */
			pv->pv_next = npv->pv_next;
			pv->pv_pmap = npv->pv_pmap;
			pv->pv_va = npv->pv_va;
			pv->pv_flags &= ~PV_NC;
			pv->pv_flags |= npv->pv_flags & PV_NC;
			pool_put(&pvpool, npv);
		} else {
			/*
			 * No mappings left; we still need to maintain
			 * the REF and MOD flags. since pmap_is_modified()
			 * can still be called for this page.
			 */
			if (pv->pv_flags & PV_ANC)
				pmap_stats.ps_alias_recache++;
			pv->pv_pmap = NULL;
			pv->pv_flags &= ~(PV_NC|PV_ANC);
			return;
		}
	} else {
		struct pvlist *prev;

		for (prev = pv;; prev = npv, npv = npv->pv_next) {
			pmap_stats.ps_unlink_pvsearch++;
			if (npv == NULL)
				panic("pv_unlink");
			if (npv->pv_pmap == pm && npv->pv_va == va)
				break;
		}
		prev->pv_next = npv->pv_next;
		pool_put(&pvpool, npv);
	}
	if (pv->pv_flags & PV_ANC && (pv->pv_flags & PV_NC) == 0) {
		/*
		 * Not cached: check to see if we can fix that now.
		 */
		va = pv->pv_va;
		for (npv = pv->pv_next; npv != NULL; npv = npv->pv_next)
			if (BADALIAS(va, npv->pv_va) || (npv->pv_flags & PV_NC))
				return;
		pmap_stats.ps_alias_recache++;
		pv->pv_flags &= ~PV_ANC;
		pv_changepte4_4c(pv, 0, PG_NC);
	}
}

/*
 * pv_link is the inverse of pv_unlink, and is used in pmap_enter.
 * It returns PG_NC if the (new) pvlist says that the address cannot
 * be cached.
 */
int
pv_link4_4c(pv, pm, va, nc)
	struct pvlist *pv;
	struct pmap *pm;
	vaddr_t va;
	int nc;
{
	struct pvlist *npv;
	int ret;

	ret = nc ? PG_NC : 0;

	if (pv->pv_pmap == NULL) {
		/* no pvlist entries yet */
		pmap_stats.ps_enter_firstpv++;
		pv->pv_next = NULL;
		pv->pv_pmap = pm;
		pv->pv_va = va;
		pv->pv_flags |= nc ? PV_NC : 0;
		return (ret);
	}

	/*
	 * Before entering the new mapping, see if
	 * it will cause old mappings to become aliased
	 * and thus need to be `discached'.
	 */
	pmap_stats.ps_enter_secondpv++;
	if (pv->pv_flags & (PV_NC|PV_ANC)) {
		/* already uncached, just stay that way */
		ret = PG_NC;
	} else {
		for (npv = pv; npv != NULL; npv = npv->pv_next) {
			if (npv->pv_flags & PV_NC) {
				ret = PG_NC;
				break;
			}
			if (BADALIAS(va, npv->pv_va)) {
#ifdef DEBUG
				if (pmapdebug & PDB_CACHESTUFF)
					printf(
			"pv_link: badalias: pid %d, 0x%lx<=>0x%lx, pa 0x%lx\n",
					curproc ? curproc->p_pid : -1,
					va, npv->pv_va, (vaddr_t)-1); /* XXX -1 */
#endif
				/* Mark list head `uncached due to aliases' */
				pmap_stats.ps_alias_uncache++;
				pv->pv_flags |= PV_ANC;
				pv_changepte4_4c(pv, ret = PG_NC, 0);
				break;
			}
		}
	}

	npv = pool_get(&pvpool, PR_NOWAIT);
	if (npv == NULL)
		panic("pv_link_4_4c: allocation failed");
	npv->pv_next = pv->pv_next;
	npv->pv_pmap = pm;
	npv->pv_va = va;
	npv->pv_flags = nc ? PV_NC : 0;
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
 *
 * In addition, if the cacheable bit (SRMMU_PG_C) is updated in the PTE
 * the corresponding PV_C4M flag is also updated in each pv entry. This
 * is done so kvm_uncache() can use this routine and have the uncached
 * status stick.
 */
void
pv_changepte4m(pv0, bis, bic)
	struct pvlist *pv0;
	int bis, bic;
{
	struct pvlist *pv;
	struct pmap *pm;
	int ctx, s;
	vaddr_t va;

	write_user_windows();		/* paranoid? */

	s = splvm();			/* paranoid? */
	if (pv0->pv_pmap == NULL) {
		splx(s);
		return;
	}
	ctx = getcontext4m();
	for (pv = pv0; pv != NULL; pv = pv->pv_next) {
		int tpte;
		int *ptep;

		pm = pv->pv_pmap;
		va = pv->pv_va;
#ifdef DIAGNOSTIC
		if (pm == NULL)
			panic("pv_changepte4m: pmap == NULL");
#endif

		ptep = getptep4m(pm, va);

		if (pm->pm_ctx) {
			setcontext4m(pm->pm_ctxnum);

			/*
			 * XXX: always flush cache; conservative, but
			 * needed to invalidate cache tag protection
			 * bits and when disabling caching.
			 */
			cache_flush_page(va);

			tlb_flush_page(va);

		}

		tpte = *ptep;
#ifdef DIAGNOSTIC
		if ((tpte & SRMMU_TETYPE) != SRMMU_TEPTE)
			panic("pv_changepte: invalid PTE for 0x%lx", va);
#endif

		pv0->pv_flags |= MR4M(tpte);
		tpte = (tpte | bis) & ~bic;
		setpgt4m(ptep, tpte);

		/* Update PV_C4M flag if required */
		/*
		 * XXX - this is incorrect. The PV_C4M means that _this_
		 *       mapping should be kept uncached. This way we
		 *       effectively uncache this pa until all mappings
		 *       to it are gone (see also the XXX in pv_link4m and
		 *       pv_unlink4m).
		 */
		if (bis & SRMMU_PG_C)
			pv->pv_flags |= PV_C4M;
		if (bic & SRMMU_PG_C)
			pv->pv_flags &= ~PV_C4M;
	}
	setcontext4m(ctx);
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
	struct pvlist *pv0;
{
	struct pvlist *pv;
	struct pmap *pm;
	int tpte, va, flags;
	int ctx, s;

	write_user_windows();		/* paranoid? */

	s = splvm();			/* paranoid? */
	if (pv0->pv_pmap == NULL) {	/* paranoid */
		splx(s);
		return (0);
	}
	ctx = getcontext4m();
	flags = pv0->pv_flags;
	for (pv = pv0; pv != NULL; pv = pv->pv_next) {
		int *ptep;

		pm = pv->pv_pmap;
		va = pv->pv_va;

		ptep = getptep4m(pm, va);

		/*
		 * XXX - This can't happen?!?
		 */
		if (ptep == NULL) {	/* invalid */
			printf("pv_syncflags4m: no pte pmap: %p, va: 0x%x\n",
			    pm, va);
			continue;
		}

		/*
		 * We need the PTE from memory as the TLB version will
		 * always have the SRMMU_PG_R bit on.
		 */
		if (pm->pm_ctx) {
			setcontext4m(pm->pm_ctxnum);
			tlb_flush_page(va);
		}
			
		tpte = *ptep;

		if ((tpte & SRMMU_TETYPE) == SRMMU_TEPTE && /* if valid pte */
		    (tpte & (SRMMU_PG_M|SRMMU_PG_R))) {	  /* and mod/refd */

			flags |= MR4M(tpte);

			if (pm->pm_ctx && (tpte & SRMMU_PG_M)) {
				cache_flush_page(va); /* XXX:do we need this?*/
				tlb_flush_page(va);
			}

			/* Clear mod/ref bits from PTE and write it back */
			tpte &= ~(SRMMU_PG_M | SRMMU_PG_R);
			setpgt4m(ptep, tpte);
		}
	}
	pv0->pv_flags = flags;
	setcontext4m(ctx);
	splx(s);
	return (flags);
}

void
pv_unlink4m(pv, pm, va)
	struct pvlist *pv;
	struct pmap *pm;
	vaddr_t va;
{
	struct pvlist *npv;

#ifdef DIAGNOSTIC
	if (pv->pv_pmap == NULL)
		panic("pv_unlink0");
#endif
	/*
	 * First entry is special (sigh).
	 */
	npv = pv->pv_next;
	if (pv->pv_pmap == pm && pv->pv_va == va) {
		pmap_stats.ps_unlink_pvfirst++;
		if (npv != NULL) {
			/*
			 * Shift next entry into the head.
			 * Make sure to retain the REF, MOD and ANC flags.
			 */
			pv->pv_next = npv->pv_next;
			pv->pv_pmap = npv->pv_pmap;
			pv->pv_va = npv->pv_va;
			pv->pv_flags &= ~PV_C4M;
			pv->pv_flags |= (npv->pv_flags & PV_C4M);
			pool_put(&pvpool, npv);
		} else {
			/*
			 * No mappings left; we still need to maintain
			 * the REF and MOD flags. since pmap_is_modified()
			 * can still be called for this page.
			 */
			if (pv->pv_flags & PV_ANC)
				pmap_stats.ps_alias_recache++;
			pv->pv_pmap = NULL;
			pv->pv_flags &= ~(PV_C4M|PV_ANC);
			return;
		}
	} else {
		struct pvlist *prev;

		for (prev = pv;; prev = npv, npv = npv->pv_next) {
			pmap_stats.ps_unlink_pvsearch++;
			if (npv == NULL)
				panic("pv_unlink");
			if (npv->pv_pmap == pm && npv->pv_va == va)
				break;
		}
		prev->pv_next = npv->pv_next;
		pool_put(&pvpool, npv);
	}
	if ((pv->pv_flags & (PV_C4M|PV_ANC)) == (PV_C4M|PV_ANC)) {
		/*
		 * Not cached: check to see if we can fix that now.
		 */
		/*
		 * XXX - This code is incorrect. Even if the bad alias
		 *       has disappeared we keep the PV_ANC flag because
		 *       one of the mappings is not PV_C4M.
		 */
		va = pv->pv_va;
		for (npv = pv->pv_next; npv != NULL; npv = npv->pv_next)
			if (BADALIAS(va, npv->pv_va) ||
			    (npv->pv_flags & PV_C4M) == 0)
				return;
		pmap_stats.ps_alias_recache++;
		pv->pv_flags &= ~PV_ANC;
		pv_changepte4m(pv, SRMMU_PG_C, 0);
	}
}

/*
 * pv_link is the inverse of pv_unlink, and is used in pmap_enter.
 * It returns SRMMU_PG_C if the (new) pvlist says that the address cannot
 * be cached (i.e. its results must be (& ~)'d in.
 */
int
pv_link4m(pv, pm, va, nc)
	struct pvlist *pv;
	struct pmap *pm;
	vaddr_t va;
	int nc;
{
	struct pvlist *npv, *mpv;
	int ret;

	ret = nc ? SRMMU_PG_C : 0;

	if (pv->pv_pmap == NULL) {
		/* no pvlist entries yet */
		pmap_stats.ps_enter_firstpv++;
		pv->pv_next = NULL;
		pv->pv_pmap = pm;
		pv->pv_va = va;
		/*
		 * XXX - should we really keep the MOD/REF flags?
		 */
		pv->pv_flags |= nc ? 0 : PV_C4M;
		return (ret);
	}

	/*
	 * We do the malloc early so that we catch all changes that happen
	 * during the (possible) sleep.
	 */
	mpv = pool_get(&pvpool, PR_NOWAIT);
	if (mpv == NULL)
		panic("pv_link4m: allocation failed");

	/*
	 * Before entering the new mapping, see if
	 * it will cause old mappings to become aliased
	 * and thus need to be `discached'.
	 */
	pmap_stats.ps_enter_secondpv++;
	if ((pv->pv_flags & PV_ANC) != 0 || (pv->pv_flags & PV_C4M) == 0) {
		/* already uncached, just stay that way */
		ret = SRMMU_PG_C;
	} else {
		for (npv = pv; npv != NULL; npv = npv->pv_next) {
			/*
			 * XXX - This code is incorrect. Even when we have
			 *       a bad alias we can fail to set PV_ANC because
			 *       one of the mappings doesn't have PV_C4M set.
			 */
			if ((npv->pv_flags & PV_C4M) == 0) {
				ret = SRMMU_PG_C;
				break;
			}
			if (BADALIAS(va, npv->pv_va)) {
#ifdef DEBUG
				if (pmapdebug & PDB_CACHESTUFF)
					printf(
			"pv_link: badalias: pid %d, 0x%lx<=>0x%lx, pa 0x%lx\n",
					curproc ? curproc->p_pid : -1,
					va, npv->pv_va, (vaddr_t)-1); /* XXX -1 */
#endif
				/* Mark list head `uncached due to aliases' */
				pmap_stats.ps_alias_uncache++;
				pv->pv_flags |= PV_ANC;
				pv_changepte4m(pv, 0, ret = SRMMU_PG_C);
				/* cache_flush_page(va); XXX: needed? */
				break;
			}
		}
	}

	mpv->pv_next = pv->pv_next;
	mpv->pv_pmap = pm;
	mpv->pv_va = va;
	mpv->pv_flags = nc ? 0 : PV_C4M;
	pv->pv_next = mpv;
	return (ret);
}
#endif

/*
 * Walk the given list and flush the cache for each (MI) page that is
 * potentially in the cache. Called only if vactype != VAC_NONE.
 */
void
pg_flushcache(struct vm_page *pg)
{
	struct pvlist *pv = &pg->mdpage.pv_head;
	struct pmap *pm;
	int s, ctx;

	write_user_windows();	/* paranoia? */

	s = splvm();		/* XXX extreme paranoia */
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

#if (defined(SUN4) || defined(SUN4E)) && defined(SUN4C)
int nptesg;
#endif

#if defined(SUN4M)
void pmap_bootstrap4m(void *);
#endif
#if defined(SUN4) || defined(SUN4C) || defined(SUN4E)
void pmap_bootstrap4_4c(void *, int, int, int);
#endif

/*
 * Bootstrap the system enough to run with VM enabled.
 *
 * nsegment is the number of mmu segment entries (``PMEGs'');
 * nregion is the number of mmu region entries (``SMEGs'');
 * nctx is the number of contexts.
 */
void
pmap_bootstrap(int nctx, int nregion, int nsegment)
{
	void *p;
	extern char end[];
#ifdef DDB
	extern char *esym;
#endif
	extern int nbpg;	/* locore.s */

	uvmexp.pagesize = nbpg;
	uvm_setpagesize();

#if (defined(SUN4) || defined(SUN4E)) && (defined(SUN4C) || defined(SUN4D) || defined(SUN4M))
	/* In this case NPTESG is not a #define */
	nptesg = (NBPSG >> uvmexp.pageshift);
#endif

	/*
	 * Grab physical memory list.
	 */
	p = end;
#ifdef DDB
	if (esym != 0)
		p = esym;
#endif
	get_phys_mem(&p);

	if (CPU_ISSUN4M) {
#if defined(SUN4M)
		pmap_bootstrap4m(p);
#endif
	} else if (CPU_ISSUN4OR4COR4E) {
#if defined(SUN4) || defined(SUN4C) || defined(SUN4E)
		pmap_bootstrap4_4c(p, nctx, nregion, nsegment);
#endif
	}

	pmap_page_upload();
}

#if defined(SUN4) || defined(SUN4C) || defined(SUN4E)
void
pmap_bootstrap4_4c(void *top, int nctx, int nregion, int nsegment)
{
	union ctxinfo *ci;
	struct mmuentry *mmuseg;
#if defined(SUN4_MMU3L)
	struct mmuentry *mmureg;
#endif
	struct regmap *rp;
	int i, j;
	int npte, zseg, vr, vs;
	int startscookie, scookie;
#if defined(SUN4_MMU3L)
	int startrcookie, rcookie;
#endif
	caddr_t p;
	vaddr_t va;
	void (*rom_setmap)(int ctx, caddr_t va, int pmeg);
	int lastpage;
	extern char kernel_text[];

	nureg = NUREG_4C;
	nkreg = NKREG_4C;

	/*
	 * Compute `va2pa_offset'.
	 * Use `kernel_text' to probe the MMU translation since
	 * the pages at KERNBASE might not be mapped.
	 */
	va2pa_offset = (vaddr_t)kernel_text -
	    ((getpte4(kernel_text) & PG_PFNUM) << PGSHIFT);

	switch (cputyp) {
	default:
	case CPU_SUN4C:
	case CPU_SUN4E:
		mmu_has_hole = 1;
		break;
	case CPU_SUN4:
		if (cpuinfo.cpu_type != CPUTYP_4_400) {
			mmu_has_hole = 1;
			break;
		}
	}

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

#if defined(SUN4D) || defined(SUN4M) /* We're in a dual-arch kernel. Setup 4/4c fn. ptrs */
	pmap_clear_modify_p 	=	pmap_clear_modify4_4c;
	pmap_clear_reference_p 	= 	pmap_clear_reference4_4c;
	pmap_copy_page_p 	=	pmap_copy_page4_4c;
	pmap_enter_p 		=	pmap_enter4_4c;
	pmap_extract_p 		=	pmap_extract4_4c;
	pmap_is_modified_p 	=	pmap_is_modified4_4c;
	pmap_is_referenced_p	=	pmap_is_referenced4_4c;
	pmap_kenter_pa_p	=	pmap_kenter_pa4_4c;
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

#if defined(SUN4_MMU3L)
	if (HASSUN4_MMU3L)
		reginval = --nregion;
#endif

	/*
	 * Initialize the kernel pmap.
	 */
	/* kernel_pmap_store.pm_ctxnum = 0; */
	kernel_pmap_store.pm_refcount = 1;
#if defined(SUN4_MMU3L)
	TAILQ_INIT(&kernel_pmap_store.pm_reglist);
#endif
	TAILQ_INIT(&kernel_pmap_store.pm_seglist);

	kernel_pmap_store.pm_regmap = &kernel_regmap_store[-NUREG_4C];
	for (i = NKREG_4C; --i >= 0;) {
#if defined(SUN4_MMU3L)
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

#if defined(SUN4_MMU3L)
	/* Reserve one region for temporary mappings */
	tregion = --nregion;
#endif

	/*
	 * Allocate and clear mmu entries and context structures.
	 */
	p = top;
#if defined(SUN4_MMU3L)
	mmuregions = (struct mmuentry *)p;
	p += nregion * sizeof(struct mmuentry);
	bzero(mmuregions, nregion * sizeof(struct mmuentry));
#endif
	mmusegments = (struct mmuentry *)p;
	p += nsegment * sizeof(struct mmuentry);
	bzero(mmusegments, nsegment * sizeof(struct mmuentry));

	pmap_kernel()->pm_ctx = cpuinfo.ctxinfo = ci = (union ctxinfo *)p;
	p += nctx * sizeof *ci;

	/* Initialize MMU resource queues */
#if defined(SUN4_MMU3L)
	TAILQ_INIT(&region_freelist);
	TAILQ_INIT(&region_lru);
	TAILQ_INIT(&region_locked);
#endif
	TAILQ_INIT(&segm_freelist);
	TAILQ_INIT(&segm_lru);
	TAILQ_INIT(&segm_locked);

	/*
	 * Set up the `constants' for the call to uvm_init()
	 * in main().  All pages beginning at p (rounded up to
	 * the next whole page) and continuing through the number
	 * of available pages are free, but they start at a higher
	 * virtual address.  This gives us two mappable MD pages
	 * for pmap_zero_page and pmap_copy_page, and some pages
	 * for dumpsys(), all with no associated physical memory.
	 */
	p = (caddr_t)round_page((vaddr_t)p);
	avail_start = PMAP_BOOTSTRAP_VA2PA(p);

	i = (int)p;
	vpage[0] = p, p += NBPG;
	vpage[1] = p, p += NBPG;
	p = reserve_dumppages(p);

	virtual_avail = (vaddr_t)p;
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
	 * THIS ASSUMES THE KERNEL IS MAPPED BY A CONTIGUOUS RANGE OF
	 * MMU SEGMENTS/REGIONS DURING THE BOOT PROCESS
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

	p = (caddr_t)KERNBASE;	/* first kernel va */
	vs = VA_VSEG((vaddr_t)p);/* first virtual segment */
	vr = VA_VREG((vaddr_t)p);/* first virtual region */
	rp = &pmap_kernel()->pm_regmap[vr];

	/* Get region/segment where kernel addresses start */
#if defined(SUN4_MMU3L)
	if (HASSUN4_MMU3L)
		startrcookie = rcookie = getregmap(p);
	mmureg = &mmuregions[rcookie];
#endif
	startscookie = scookie = getsegmap(p);
	mmuseg = &mmusegments[scookie];
	zseg += scookie;	/* First free segment */

	for (;;) {

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
#if defined(SUN4_MMU3L)
			if (HASSUN4_MMU3L) {
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

#if defined(SUN4_MMU3L)
		if (!HASSUN4_MMU3L)
#endif
			for (i = 1; i < nctx; i++)
				rom_setmap(i, p, scookie);

		/* set up the mmu entry */
		TAILQ_INSERT_TAIL(&segm_locked, mmuseg, me_list);
		TAILQ_INSERT_TAIL(&pmap_kernel()->pm_seglist, mmuseg, me_pmchain);
		pmap_stats.ps_npmeg_locked++;
		mmuseg->me_cookie = scookie;
		mmuseg->me_pmap = pmap_kernel();
		mmuseg->me_vreg = vr;
		mmuseg->me_vseg = vs % NSEGRG;
		rp->rg_segmap[vs % NSEGRG].sg_pmeg = scookie;
		npte = ++scookie < zseg ? NPTESG : lastpage;
		rp->rg_segmap[vs % NSEGRG].sg_npte = npte;
		pmap_kernel()->pm_stats.resident_count += npte;
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

#if defined(SUN4_MMU3L)
		if (HASSUN4_MMU3L) {
			/*
			 * Unmap the segments, if any, that are not part of
			 * the final region.
			 */
			for (i = rp->rg_nsegmap; i < NSEGRG; i++, p += NBPSG)
				setsegmap(p, seginval);

			/*
			 * Unmap any kernel regions that we aren't using.
			 */
			for (i = 0; i < nctx; i++) {
				setcontext4(i);
				for (va = (vaddr_t)p;
				    va < (OPENPROM_STARTVADDR & ~(NBPRG - 1));
				    va += NBPRG)
					setregmap(va, reginval);
			}
		} else
#endif
		{
			/*
			 * Unmap any kernel regions that we aren't using.
			 */
			for (i = 0; i < nctx; i++) {
				setcontext4(i);
				for (va = (vaddr_t)p;
				    va < (OPENPROM_STARTVADDR & ~(NBPSG - 1));
				    va += NBPSG)
					setsegmap(va, seginval);
			}
		}
		break;
	}

#if defined(SUN4_MMU3L)
	if (HASSUN4_MMU3L)
		for (rcookie = 0; rcookie < nregion; rcookie++) {
			if (rcookie == startrcookie)
				/* Kernel must fit in one region! */
				rcookie++;
			mmureg = &mmuregions[rcookie];
			mmureg->me_cookie = rcookie;
			TAILQ_INSERT_TAIL(&region_freelist, mmureg, me_list);
		}
#endif

	for (scookie = 0; scookie < nsegment; scookie++) {
		if (scookie == startscookie)
			scookie = zseg;
		mmuseg = &mmusegments[scookie];
		mmuseg->me_cookie = scookie;
		TAILQ_INSERT_TAIL(&segm_freelist, mmuseg, me_list);
		pmap_stats.ps_npmeg_free++;
	}

	/* Erase all spurious user-space segmaps */
	for (i = 1; i < ncontext; i++) {
		setcontext4(i);
		if (HASSUN4_MMU3L)
			for (p = 0, j = NUREG_4C; --j >= 0; p += NBPRG)
				setregmap(p, reginval);
		else
			for (p = 0, vr = 0; vr < NUREG_4C; vr++) {
				if (VA_INHOLE(p)) {
					p = (caddr_t)MMU_HOLE_END;
					vr = VA_VREG(p);
				}
				for (j = NSEGRG; --j >= 0; p += NBPSG)
					setsegmap(p, seginval);
			}
	}
	setcontext4(0);

	/*
	 * write protect & encache kernel text;
	 * set red zone at kernel base; enable cache on message buffer.
	 */
	{
		extern char __data_start[];
		caddr_t sdata = (caddr_t)trunc_page((vaddr_t)__data_start);

		/* Enable cache on message buffer */
		for (p = (caddr_t)KERNBASE; p < (caddr_t)trapbase; p += NBPG)
			setpte4(p, getpte4(p) & ~PG_NC);

		/* Enable cache and write protext kernel text and rodata */
		for (; p < sdata; p += NBPG)
			setpte4(p, getpte4(p) & ~(PG_W | PG_NC));

		/* Enable cache on data & bss */
		for (; p < (caddr_t)virtual_avail; p += NBPG)
			setpte4(p, getpte4(p) & ~PG_NC);
	}
}
#endif

#if defined(SUN4M)		/* Sun4M version of pmap_bootstrap */
/*
 * Bootstrap the system enough to run with VM enabled on a Sun4M machine.
 *
 * Switches from ROM to kernel page tables, and sets up initial mappings.
 */
void
pmap_bootstrap4m(void *top)
{
	int i, j;
	caddr_t p;
	caddr_t q;
	union ctxinfo *ci;
	int reg, seg;
	unsigned int ctxtblsize;
	paddr_t pagetables_start_pa;
	extern char kernel_text[];
	extern caddr_t reserve_dumppages(caddr_t);

	nureg = NUREG_4M;
	nkreg = NKREG_4M;

	/*
	 * Compute `va2pa_offset'.
	 * Use `kernel_text' to probe the MMU translation since
	 * the pages at KERNBASE might not be mapped.
	 */
	va2pa_offset = (vaddr_t)kernel_text - VA2PA(kernel_text);

#if defined(SUN4) || defined(SUN4C) || defined(SUN4E) /* setup 4M fn. ptrs for dual-arch kernel */
	pmap_clear_modify_p 	=	pmap_clear_modify4m;
	pmap_clear_reference_p 	= 	pmap_clear_reference4m;
	pmap_copy_page_p 	=	pmap_copy_page4m;
	pmap_enter_p 		=	pmap_enter4m;
	pmap_extract_p 		=	pmap_extract4m;
	pmap_is_modified_p 	=	pmap_is_modified4m;
	pmap_is_referenced_p	=	pmap_is_referenced4m;
	pmap_kenter_pa_p	=	pmap_kenter_pa4m;
	pmap_page_protect_p	=	pmap_page_protect4m;
	pmap_protect_p		=	pmap_protect4m;
	pmap_zero_page_p	=	pmap_zero_page4m;
	pmap_changeprot_p	=	pmap_changeprot4m;
	pmap_rmk_p		=	pmap_rmk4m;
	pmap_rmu_p		=	pmap_rmu4m;
#endif /* SUN4 || SUN4C || SUN4E */

	/*
	 * Initialize the kernel pmap.
	 */
	/* kernel_pmap_store.pm_ctxnum = 0; */
	kernel_pmap_store.pm_refcount = 1;

	/*
	 * Set up pm_regmap for kernel to point NUREG_4M *below* the beginning
	 * of kernel regmap storage. Since the kernel only uses regions
	 * above NUREG_4M, we save storage space and can index kernel and
	 * user regions in the same way
	 */
	kernel_pmap_store.pm_regmap = &kernel_regmap_store[-NUREG_4M];
	kernel_pmap_store.pm_reg_ptps = NULL;
	kernel_pmap_store.pm_reg_ptps_pa = 0;
	bzero(kernel_regmap_store, NKREG_4M * sizeof(struct regmap));
	bzero(kernel_segmap_store, NKREG_4M * NSEGRG * sizeof(struct segmap));
	for (i = NKREG_4M; --i >= 0;) {
		kernel_regmap_store[i].rg_segmap =
			&kernel_segmap_store[i * NSEGRG];
		kernel_regmap_store[i].rg_seg_ptps = NULL;
		for (j = NSEGRG; --j >= 0;)
			kernel_segmap_store[i * NSEGRG + j].sg_pte = NULL;
	}

	p = top;		/* p points to top of kernel mem */
	p = (caddr_t)round_page((vaddr_t)p);

	/* Allocate context administration */
	pmap_kernel()->pm_ctx = cpuinfo.ctxinfo = ci = (union ctxinfo *)p;
	p += ncontext * sizeof *ci;
	bzero((caddr_t)ci, (u_int)p - (u_int)ci);
#if 0
	ctxbusyvector = p;
	p += ncontext;
	bzero(ctxbusyvector, ncontext);
	ctxbusyvector[0] = 1;	/* context 0 is always in use */
#endif

	/*
	 * Set up the `constants' for the call to uvm_init()
	 * in main().  All pages beginning at p (rounded up to
	 * the next whole page) and continuing through the number
	 * of available pages are free.
	 */
	p = (caddr_t)round_page((vaddr_t)p);

	/*
	 * Reserve memory for MMU pagetables. Some of these have severe
	 * alignment restrictions.
	 */
	pagetables_start = (vaddr_t)p;
	pagetables_start_pa = PMAP_BOOTSTRAP_VA2PA(p);

	/*
	 * Allocate context table.
	 * To keep supersparc happy, minimum alignment is on a 4K boundary.
	 */
	ctxtblsize = max(ncontext, 1024) * sizeof(int);
	cpuinfo.ctx_tbl = (int *)roundup((u_int)p, ctxtblsize);
	p = (caddr_t)((u_int)cpuinfo.ctx_tbl + ctxtblsize);
	qzero(cpuinfo.ctx_tbl, ctxtblsize);

	/*
	 * Reserve memory for segment and page tables needed to map the entire
	 * kernel. This takes (2k + NKREG_4M * 16k) of space, but
	 * unfortunately is necessary since pmap_enk *must* be able to enter
	 * a kernel mapping without resorting to malloc, or else the
	 * possibility of deadlock arises (pmap_enk4m is called to enter a
	 * mapping; it needs to malloc a page table; malloc then calls
	 * pmap_enk4m to enter the new malloc'd page; pmap_enk4m needs to
	 * malloc a page table to enter _that_ mapping; malloc deadlocks since
	 * it is already allocating that object).
	 */
	p = (caddr_t) roundup((u_int)p, SRMMU_L1SIZE * sizeof(long));
	kernel_regtable_store = (u_int *)p;
	p += SRMMU_L1SIZE * sizeof(long);
	bzero(kernel_regtable_store, p - (caddr_t)kernel_regtable_store);

	p = (caddr_t) roundup((u_int)p, SRMMU_L2SIZE * sizeof(long));
	kernel_segtable_store = (u_int *)p;
	p += (SRMMU_L2SIZE * sizeof(long)) * NKREG_4M;
	bzero(kernel_segtable_store, p - (caddr_t)kernel_segtable_store);

	p = (caddr_t) roundup((u_int)p, SRMMU_L3SIZE * sizeof(long));
	kernel_pagtable_store = (u_int *)p;
	p += ((SRMMU_L3SIZE * sizeof(long)) * NKREG_4M) * NSEGRG;
	bzero(kernel_pagtable_store, p - (caddr_t)kernel_pagtable_store);

	/* Round to next page and mark end of stolen pages */
	p = (caddr_t)round_page((vaddr_t)p);
	pagetables_end = (vaddr_t)p;

	avail_start = PMAP_BOOTSTRAP_VA2PA(p);

	/*
	 * Since we've statically allocated space to map the entire kernel,
	 * we might as well pre-wire the mappings to save time in pmap_enter.
	 * This also gets around nasty problems with caching of L1/L2 ptp's.
	 *
	 * XXX WHY DO WE HAVE THIS CACHING PROBLEM WITH L1/L2 PTPS????? %%%
	 */

	pmap_kernel()->pm_reg_ptps = (int *)kernel_regtable_store;
	pmap_kernel()->pm_reg_ptps_pa =
	    PMAP_BOOTSTRAP_VA2PA(kernel_regtable_store);

	/* Install L1 table in context 0 */
	setpgt4m(&cpuinfo.ctx_tbl[0],
	    (pmap_kernel()->pm_reg_ptps_pa >> SRMMU_PPNPASHIFT) | SRMMU_TEPTD);

	/* XXX:rethink - Store pointer to region table address */
	cpuinfo.L1_ptps = pmap_kernel()->pm_reg_ptps;

	for (reg = 0; reg < NKREG_4M; reg++) {
		struct regmap *rp;
		caddr_t kphyssegtbl;

		/*
		 * Entering new region; install & build segtbl
		 */

		rp = &pmap_kernel()->pm_regmap[reg + NUREG];

		kphyssegtbl = (caddr_t)
		    &kernel_segtable_store[reg * SRMMU_L2SIZE];

		setpgt4m(&pmap_kernel()->pm_reg_ptps[reg + NUREG],
		    (PMAP_BOOTSTRAP_VA2PA(kphyssegtbl) >> SRMMU_PPNPASHIFT) |
		    SRMMU_TEPTD);

		rp->rg_seg_ptps = (int *)kphyssegtbl;

		for (seg = 0; seg < NSEGRG; seg++) {
			struct segmap *sp;
			caddr_t kphyspagtbl;

			rp->rg_nsegmap++;

			sp = &rp->rg_segmap[seg];
			kphyspagtbl = (caddr_t)
			    &kernel_pagtable_store
				[((reg * NSEGRG) + seg) * SRMMU_L3SIZE];

			setpgt4m(&rp->rg_seg_ptps[seg],
				 (PMAP_BOOTSTRAP_VA2PA(kphyspagtbl) >> SRMMU_PPNPASHIFT) |
				 SRMMU_TEPTD);
			sp->sg_pte = (int *) kphyspagtbl;
		}
	}

	/*
	 * Preserve the monitor ROM's reserved VM region, so that
	 * we can use L1-A or the monitor's debugger.
	 */
	mmu_reservemon4m(&kernel_pmap_store);

	/*
	 * Reserve virtual address space for two mappable MD pages
	 * for pmap_zero_page and pmap_copy_page, and some more for
	 * dumpsys().
	 */
	q = p;
	vpage[0] = p, p += NBPG;
	vpage[1] = p, p += NBPG;
	p = reserve_dumppages(p);

	virtual_avail = (vaddr_t)p;
	virtual_end = VM_MAX_KERNEL_ADDRESS;

	p = q;			/* retract to first free phys */

	/*
	 * Set up the ctxinfo structures (freelist of contexts)
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
	 * Now map the kernel into our new set of page tables, then
	 * (finally) switch over to our running page tables.
	 * We map from VM_MIN_KERNEL_ADDRESS to p into context 0's
	 * page tables (and the kernel pmap).
	 */
#ifdef DEBUG			/* Sanity checks */
	if ((u_int)p % NBPG != 0)
		panic("pmap_bootstrap4m: p misaligned?!?");
	if (VM_MIN_KERNEL_ADDRESS_SRMMU % NBPRG != 0)
		panic("pmap_bootstrap4m: VM_MIN_KERNEL_ADDRESS not region-aligned");
#endif

	for (q = (caddr_t) KERNBASE; q < p; q += NBPG) {
		struct regmap *rp;
		struct segmap *sp;
		int pte, *ptep;
		extern char __data_start[];
		caddr_t sdata = (caddr_t)trunc_page((vaddr_t)__data_start);

		/*
		 * Now install entry for current page.
		 */
		rp = &pmap_kernel()->pm_regmap[VA_VREG(q)];
		sp = &rp->rg_segmap[VA_VSEG(q)];
		ptep = &sp->sg_pte[VA_VPG(q)];

		pte = PMAP_BOOTSTRAP_VA2PA(q) >> SRMMU_PPNPASHIFT;
		pte |= PPROT_N_RX | SRMMU_TEPTE;

		/* Deal with the cacheable bit for pagetable memory */
		if ((cpuinfo.flags & CPUFLG_CACHEPAGETABLES) != 0 ||
		    q < (caddr_t)pagetables_start ||
		    q >= (caddr_t)pagetables_end)
			pte |= SRMMU_PG_C;

		/* write-protect kernel text */
		if (q < (caddr_t)trapbase || q >= sdata)
			pte |= PPROT_WRITE;

		setpgt4m(ptep, pte);
		sp->sg_npte++;
		pmap_kernel()->pm_stats.resident_count++;
	}

#if 0
	/*
	 * We also install the kernel mapping into all other contexts by
	 * copying the context 0 L1 PTP from cpuinfo.ctx_tbl[0] into the
	 * remainder of the context table (i.e. we share the kernel page-
	 * tables). Each user pmap automatically gets the kernel mapped
	 * into it when it is created, but we do this extra step early on
	 * in case some twit decides to switch to a context with no user
	 * pmap associated with it.
	 */
	for (i = 1; i < ncontext; i++)
		cpuinfo.ctx_tbl[i] = cpuinfo.ctx_tbl[0];
#endif

	if ((cpuinfo.flags & CPUFLG_CACHEPAGETABLES) == 0)
		/* Flush page tables from cache */
		pcache_flush((caddr_t)pagetables_start,
			     (caddr_t)pagetables_start_pa,
			     pagetables_end - pagetables_start);

	/*
	 * Now switch to kernel pagetables (finally!)
	 */
	mmu_install_tables(&cpuinfo);

	sparc_protection_init4m();
}

void
mmu_install_tables(sc)
	struct cpu_softc *sc;
{

#ifdef DEBUG
	printf("pmap_bootstrap: installing kernel page tables...");
#endif
	setcontext4m(0);	/* paranoia? %%%: Make 0x3 a define! below */

	/* Enable MMU tablewalk caching, flush TLB */
	if (sc->mmu_enable != 0)
		sc->mmu_enable();

	tlb_flush_all();

	sta(SRMMU_CXTPTR, ASI_SRMMU,
	    (VA2PA((caddr_t)sc->ctx_tbl) >> SRMMU_PPNPASHIFT) & ~0x3);

	tlb_flush_all();

#ifdef DEBUG
	printf("done.\n");
#endif
}

#ifdef MULTIPROCESSOR
/*
 * Allocate per-CPU page tables.
 * Note: this routine is called in the context of the boot CPU
 * during autoconfig.
 */
void
pmap_alloc_cpu(sc)
	struct cpu_softc *sc;
{
	caddr_t cpustore;
	int *ctxtable;
	int *regtable;
	int *segtable;
	int *pagtable;
	int vr, vs, vpg;
	struct regmap *rp;
	struct segmap *sp;

	/* Allocate properly aligned and physically contiguous memory here */
	cpustore = 0;
	ctxtable = 0;
	regtable = 0;
	segtable = 0;
	pagtable = 0;

	vr = VA_VREG(CPUINFO_VA);
	vs = VA_VSEG(CPUINFO_VA);
	vpg = VA_VPG(CPUINFO_VA);
	rp = &pmap_kernel()->pm_regmap[vr];
	sp = &rp->rg_segmap[vs];

	/*
	 * Copy page tables, then modify entry for CPUINFO_VA so that
	 * it points at the per-CPU pages.
	 */
	bcopy(cpuinfo.L1_ptps, regtable, SRMMU_L1SIZE * sizeof(int));
	regtable[vr] =
		(VA2PA((caddr_t)segtable) >> SRMMU_PPNPASHIFT) | SRMMU_TEPTD;

	bcopy(rp->rg_seg_ptps, segtable, SRMMU_L2SIZE * sizeof(int));
	segtable[vs] =
		(VA2PA((caddr_t)pagtable) >> SRMMU_PPNPASHIFT) | SRMMU_TEPTD;

	bcopy(sp->sg_pte, pagtable, SRMMU_L3SIZE * sizeof(int));
	pagtable[vpg] =
		(VA2PA((caddr_t)cpustore) >> SRMMU_PPNPASHIFT) |
		(SRMMU_TEPTE | PPROT_RW_RW | SRMMU_PG_C);

	/* Install L1 table in context 0 */
	ctxtable[0] = ((u_int)regtable >> SRMMU_PPNPASHIFT) | SRMMU_TEPTD;

	sc->ctx_tbl = ctxtable;
	sc->L1_ptps = regtable;

#if 0
	if ((sc->flags & CPUFLG_CACHEPAGETABLES) == 0) {
		kvm_uncache((caddr_t)0, 1);
	}
#endif
}
#endif /* MULTIPROCESSOR */

#endif /* defined sun4m */


void
pmap_init()
{
	pool_init(&pvpool, sizeof(struct pvlist), 0, 0, 0, "pvpl", NULL);

#if defined(SUN4M)
        if (CPU_ISSUN4M) {
                /*
                 * The SRMMU only ever needs chunks in one of two sizes:
                 * 1024 (for region level tables) and 256 (for segment
                 * and page level tables).
                 */
                int n;

                n = SRMMU_L1SIZE * sizeof(int);
                pool_init(&L1_pool, n, n, 0, 0, "L1 pagetable",
		    &pgt_allocator);

                n = SRMMU_L2SIZE * sizeof(int);
                pool_init(&L23_pool, n, n, 0, 0, "L2/L3 pagetable",
                    &pgt_allocator);
        }
#endif
}

/*
 * Called just after enabling cache (so that CPUFLG_CACHEPAGETABLES is
 * set correctly).
 */
void
pmap_cache_enable()
{
#ifdef SUN4M
	if (CPU_ISSUN4M) {
		int pte;

		/*
		 * Deal with changed CPUFLG_CACHEPAGETABLES.
		 *
		 * If the tables were uncached during the initial mapping
		 * and cache_enable set the flag we recache the tables.
		 */

		pte = getpte4m(pagetables_start);

		if ((cpuinfo.flags & CPUFLG_CACHEPAGETABLES) != 0 &&
		    (pte & SRMMU_PG_C) == 0)
			kvm_recache((caddr_t)pagetables_start,
				    atop(pagetables_end - pagetables_start));
	}
#endif
}


/*
 * Map physical addresses into kernel VM.
 */
vaddr_t
pmap_map(va, pa, endpa, prot)
	vaddr_t va;
	paddr_t pa, endpa;
	int prot;
{
	int pgsize = PAGE_SIZE;

	while (pa < endpa) {
		pmap_kenter_pa(va, pa, prot);
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
pmap_create()
{
	struct pmap *pm;
	int size;
	void *urp;

	pm = (struct pmap *)malloc(sizeof *pm, M_VMPMAP, M_WAITOK);
#ifdef DEBUG
	if (pmapdebug & PDB_CREATE)
		printf("pmap_create: created %p\n", pm);
#endif
	bzero((caddr_t)pm, sizeof *pm);

	size = NUREG * sizeof(struct regmap);

	pm->pm_regstore = urp = malloc(size, M_VMPMAP, M_WAITOK);
	qzero((caddr_t)urp, size);
	/* pm->pm_ctx = NULL; */
	pm->pm_refcount = 1;
	pm->pm_regmap = urp;

	if (CPU_ISSUN4OR4COR4E) {
		TAILQ_INIT(&pm->pm_seglist);
#if defined(SUN4_MMU3L)
		TAILQ_INIT(&pm->pm_reglist);
		if (HASSUN4_MMU3L) {
			int i;
			for (i = NUREG_4C; --i >= 0;)
				pm->pm_regmap[i].rg_smeg = reginval;
		}
#endif
	}
#if defined(SUN4M)
	else {
		int i;

		/*
		 * We must allocate and initialize hardware-readable (MMU)
		 * pagetables. We must also map the kernel regions into this
		 * pmap's pagetables, so that we can access the kernel from
		 * this user context.
		 *
		 * Note: pm->pm_regmap's have been zeroed already, so we don't
		 * need to explicitly mark them as invalid (a null
		 * rg_seg_ptps pointer indicates invalid for the 4m)
		 */
		urp = pool_get(&L1_pool, PR_WAITOK);
		pm->pm_reg_ptps = urp;
		pm->pm_reg_ptps_pa = VA2PA(urp);

		/* Invalidate user mappings */
		for (i = 0; i < NUREG_4M; i++)
			setpgt4m(&pm->pm_reg_ptps[i], SRMMU_TEINVALID);

		/* Copy kernel regions */
		for (i = 0; i < NKREG_4M; i++) {
			setpgt4m(&pm->pm_reg_ptps[NUREG_4M + i],
				 cpuinfo.L1_ptps[NUREG_4M + i]);
		}
	}
#endif

	pm->pm_gap_end = VA_VREG(VM_MAXUSER_ADDRESS);

	return (pm);
}

/*
 * Retire the given pmap from service.
 * Should only be called if the map contains no valid mappings.
 */
void
pmap_destroy(pm)
	struct pmap *pm;
{
	int count;

	if (pm == NULL)
		return;
#ifdef DEBUG
	if (pmapdebug & PDB_DESTROY)
		printf("pmap_destroy(%p)\n", pm);
#endif
	count = --pm->pm_refcount;
	if (count == 0) {
		pmap_release(pm);
		free(pm, M_VMPMAP, 0);
	}
}

/*
 * Release any resources held by the given physical map.
 */
void
pmap_release(pm)
	struct pmap *pm;
{
	union ctxinfo *c;
	int s = splvm();	/* paranoia */

#ifdef DEBUG
	if (pmapdebug & PDB_DESTROY)
		printf("pmap_release(%p)\n", pm);
#endif

	if (CPU_ISSUN4OR4COR4E) {
#if defined(SUN4_MMU3L)
		if (!TAILQ_EMPTY(&pm->pm_reglist))
			panic("pmap_release: region list not empty");
#endif
		if (!TAILQ_EMPTY(&pm->pm_seglist))
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
		free(pm->pm_regstore, M_VMPMAP, 0);

#if defined(SUN4M)
	if (CPU_ISSUN4M) {
		if ((c = pm->pm_ctx) != NULL) {
			if (pm->pm_ctxnum == 0)
				panic("pmap_release: releasing kernel");
			ctx_free(pm);
		}
		pool_put(&L1_pool, pm->pm_reg_ptps);
		pm->pm_reg_ptps = NULL;
		pm->pm_reg_ptps_pa = 0;
	}
#endif
}

/*
 * Add a reference to the given pmap.
 */
void
pmap_reference(pm)
	struct pmap *pm;
{
	if (pm != NULL)
		pm->pm_refcount++;
}

/*
 * Remove the given range of mapping entries.
 * The starting and ending addresses are already rounded to pages.
 * Sheer lunacy: pmap_remove is often asked to remove nonexistent
 * mappings.
 */
void
pmap_remove(pm, va, endva)
	struct pmap *pm;
	vaddr_t va, endva;
{
	vaddr_t nva;
	int vr, vs, s, ctx;
	void (*rm)(struct pmap *, vaddr_t, vaddr_t, int, int);

	if (pm == NULL)
		return;

#ifdef DEBUG
	if (pmapdebug & PDB_REMOVE)
		printf("pmap_remove(%p, 0x%lx, 0x%lx)\n", pm, va, endva);
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
	s = splvm();		/* XXX conservative */
	for (; va < endva; va = nva) {
		/* do one virtual segment at a time */
		vr = VA_VREG(va);
		vs = VA_VSEG(va);
		nva = VSTOVA(vr, vs + 1);
		if (nva == 0 || nva > endva)
			nva = endva;
		if (pm->pm_regmap[vr].rg_nsegmap != 0)
			(*rm)(pm, va, nva, vr, vs);
	}
	splx(s);
	setcontext(ctx);
}

void
pmap_kremove(va, len)
	vaddr_t va;
	vsize_t len;
{
	struct pmap *pm = pmap_kernel();
	vaddr_t nva, endva = va + len;
	int vr, vs, s, ctx;

#ifdef DEBUG
	if (pmapdebug & PDB_REMOVE)
		printf("pmap_kremove(0x%lx, 0x%lx)\n", va, len);
#endif

	ctx = getcontext();
	s = splvm();		/* XXX conservative */

	for (; va < endva; va = nva) {
		/* do one virtual segment at a time */
		vr = VA_VREG(va);
		vs = VA_VSEG(va);
		nva = VSTOVA(vr, vs + 1);
		if (nva == 0 || nva > endva)
			nva = endva;
		if (pm->pm_regmap[vr].rg_nsegmap != 0)
			pmap_rmk(pm, va, nva, vr, vs);
	}

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

#if defined(SUN4) || defined(SUN4C) || defined(SUN4E)

/* remove from kernel */
void
pmap_rmk4_4c(pm, va, endva, vr, vs)
	struct pmap *pm;
	vaddr_t va, endva;
	int vr, vs;
{
	int i, tpte, perpage, npg;
	struct pvlist *pv;
	int nleft, pmeg;
	struct regmap *rp;
	struct segmap *sp;
	int s;

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

	setcontext4(0);
	/* decide how to flush cache */
	npg = (endva - va) >> PGSHIFT;
	if (npg > PMAP_RMK_MAGIC) {
		/* flush the whole segment */
		perpage = 0;
		cache_flush_segment(vr, vs);
	} else {
		/* flush each page individually; some never need flushing */
		perpage = (CACHEINFO.c_vactype != VAC_NONE);
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
			pv = pvhead(tpte & PG_PFNUM);
			if (pv) {
				pv->pv_flags |= MR4_4C(tpte);
				s = splvm();
				pv_unlink4_4c(pv, pm, va);
				splx(s);
			}
		}
		nleft--;
		setpte4(va, 0);
		pm->pm_stats.resident_count--;
		va += NBPG;
	}

	/*
	 * If the segment is all gone, remove it from everyone and
	 * free the MMU entry.
	 */
	if ((sp->sg_npte = nleft) == 0) {
		va = VSTOVA(vr,vs);		/* retract */
#if defined(SUN4_MMU3L)
		if (HASSUN4_MMU3L)
			setsegmap(va, seginval);
		else
#endif
			for (i = ncontext; --i >= 0;) {
				setcontext4(i);
				setsegmap(va, seginval);
			}
		me_free(pm, pmeg);
		if (--rp->rg_nsegmap == 0) {
#if defined(SUN4_MMU3L)
			if (HASSUN4_MMU3L) {
				for (i = ncontext; --i >= 0;) {
					setcontext4(i);
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
void
pmap_rmk4m(pm, va, endva, vr, vs)
	struct pmap *pm;
	vaddr_t va, endva;
	int vr, vs;
{
	int tpte, perpage, npg;
	struct pvlist *pv;
	int nleft;
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

	setcontext4m(0);
	/* decide how to flush cache */
	npg = (endva - va) >> PGSHIFT;
	if (npg > PMAP_RMK_MAGIC) {
		/* flush the whole segment */
		perpage = 0;
		if (CACHEINFO.c_vactype != VAC_NONE)
			cache_flush_segment(vr, vs);
	} else {
		/* flush each page individually; some never need flushing */
		perpage = (CACHEINFO.c_vactype != VAC_NONE);
	}
	while (va < endva) {
		tpte = sp->sg_pte[VA_SUN4M_VPG(va)];
		if ((tpte & SRMMU_TETYPE) != SRMMU_TEPTE) {
#ifdef DEBUG
			if ((pmapdebug & PDB_SANITYCHK) &&
			    (getpte4m(va) & SRMMU_TETYPE) == SRMMU_TEPTE)
				panic("pmap_rmk: Spurious kTLB entry for 0x%lx",
				      va);
#endif
			va += NBPG;
			continue;
		}
		if ((tpte & SRMMU_PGTYPE) == PG_SUN4M_OBMEM) {
			/* if cacheable, flush page as needed */
			if (perpage && (tpte & SRMMU_PG_C))
				cache_flush_page(va);
			pv = pvhead((tpte & SRMMU_PPNMASK) >> SRMMU_PPNSHIFT);
			if (pv) {
				pv->pv_flags |= MR4M(tpte);
				pv_unlink4m(pv, pm, va);
			}
		}
		nleft--;
		tlb_flush_page(va);
		setpgt4m(&sp->sg_pte[VA_SUN4M_VPG(va)], SRMMU_TEINVALID);
		pm->pm_stats.resident_count--;
		va += NBPG;
	}

	sp->sg_npte = nleft;
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

#if defined(SUN4) || defined(SUN4C) || defined(SUN4E)

/* remove from user */
void
pmap_rmu4_4c(pm, va, endva, vr, vs)
	struct pmap *pm;
	vaddr_t va, endva;
	int vr, vs;
{
	int *pte0, pteva, tpte, perpage, npg;
	struct pvlist *pv;
	int nleft, pmeg;
	struct regmap *rp;
	struct segmap *sp;
	int s;

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
		int *pte = pte0 + VA_VPG(va);

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
				struct pvlist *pv;

				pv = pvhead(tpte & PG_PFNUM);
				if (pv) {
					s = splvm();
					pv_unlink4_4c(pv, pm, va);
					splx(s);
				}
			}
			nleft--;
			*pte = 0;
			pm->pm_stats.resident_count--;
		}
		if ((sp->sg_npte = nleft) == 0) {
			free(pte0, M_VMPMAP, 0);
			sp->sg_pte = NULL;
			if (--rp->rg_nsegmap == 0) {
				free(rp->rg_segmap, M_VMPMAP, 0);
				rp->rg_segmap = NULL;
#if defined(SUN4_MMU3L)
				if (HASSUN4_MMU3L && rp->rg_smeg != reginval) {
					if (pm->pm_ctx) {
						setcontext4(pm->pm_ctxnum);
						setregmap(va, reginval);
					} else
						setcontext4(0);
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
		setcontext4(pm->pm_ctxnum);
		npg = (endva - va) >> PGSHIFT;
		if (npg > PMAP_RMU_MAGIC) {
			perpage = 0; /* flush the whole segment */
			cache_flush_segment(vr, vs);
		} else
			perpage = (CACHEINFO.c_vactype != VAC_NONE);
		pteva = va;
	} else {
		/* no context, use context 0; cache flush unnecessary */
		setcontext4(0);
		if (HASSUN4_MMU3L)
			setregmap(0, tregion);
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
			pv = pvhead(tpte & PG_PFNUM);
			if (pv) {
				pv->pv_flags |= MR4_4C(tpte);
				s = splvm();
				pv_unlink4_4c(pv, pm, va);
				splx(s);
			}
		}
		nleft--;
		setpte4(pteva, 0);
		pte0[VA_VPG(pteva)] = 0;
		pm->pm_stats.resident_count--;
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
		else if (HASSUN4_MMU3L && rp->rg_smeg != reginval) {
			/* note: context already set earlier */
			setregmap(0, rp->rg_smeg);
			setsegmap(vs << SGSHIFT, seginval);
		}
		free(pte0, M_VMPMAP, 0);
		sp->sg_pte = NULL;
		me_free(pm, pmeg);

		if (--rp->rg_nsegmap == 0) {
			free(rp->rg_segmap, M_VMPMAP, 0);
			rp->rg_segmap = NULL;
			GAP_WIDEN(pm,vr);

#if defined(SUN4_MMU3L)
			if (HASSUN4_MMU3L && rp->rg_smeg != reginval) {
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
void
pmap_rmu4m(pm, va, endva, vr, vs)
	struct pmap *pm;
	vaddr_t va, endva;
	int vr, vs;
{
	int *pte0, perpage, npg;
	struct pvlist *pv;
	int nleft;
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
	if (pm->pm_ctx) {
		/* process has a context, must flush cache */
		setcontext4m(pm->pm_ctxnum);
		if (CACHEINFO.c_vactype != VAC_NONE) {
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

		int tpte = pte0[VA_SUN4M_VPG(va)];

		if ((tpte & SRMMU_TETYPE) != SRMMU_TEPTE) {
#ifdef DEBUG
			if ((pmapdebug & PDB_SANITYCHK) &&
			    pm->pm_ctx &&
			    (getpte4m(va) & SRMMU_TEPTE) == SRMMU_TEPTE)
				panic("pmap_rmu: Spurious uTLB entry for 0x%lx",
				      va);
#endif
			continue;
		}

		if ((tpte & SRMMU_PGTYPE) == PG_SUN4M_OBMEM) {
			/* if cacheable, flush page as needed */
			if (perpage && (tpte & SRMMU_PG_C))
				cache_flush_page(va);
			pv = pvhead((tpte & SRMMU_PPNMASK) >> SRMMU_PPNSHIFT);
			if (pv) {
				pv->pv_flags |= MR4M(tpte);
				pv_unlink4m(pv, pm, va);
			}
		}
		nleft--;
		if (pm->pm_ctx)
			tlb_flush_page(va);
		setpgt4m(&pte0[VA_SUN4M_VPG(va)], SRMMU_TEINVALID);
		pm->pm_stats.resident_count--;
	}

	/*
	 * If the segment is all gone, and the context is loaded, give
	 * the segment back.
	 */
	if ((sp->sg_npte = nleft) == 0) {
#ifdef DEBUG
		if (pm->pm_ctx == NULL) {
			printf("pmap_rmu: no context here...");
		}
#endif
		va = VSTOVA(vr,vs);		/* retract */

		if (pm->pm_ctx)
			tlb_flush_segment(vr, vs); 	/* Paranoia? */
		setpgt4m(&rp->rg_seg_ptps[vs], SRMMU_TEINVALID);
		pool_put(&L23_pool, pte0);
		sp->sg_pte = NULL;

		if (--rp->rg_nsegmap == 0) {
			if (pm->pm_ctx)
				tlb_flush_context(); 	/* Paranoia? */
			setpgt4m(&pm->pm_reg_ptps[vr], SRMMU_TEINVALID);
			free(rp->rg_segmap, M_VMPMAP, 0);
			rp->rg_segmap = NULL;
			pool_put(&L23_pool, rp->rg_seg_ptps);
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

#if defined(SUN4) || defined(SUN4C) || defined(SUN4E)
void
pmap_page_protect4_4c(struct vm_page *pg, vm_prot_t prot)
{
	struct pvlist *pv, *pv0, *npv;
	struct pmap *pm;
	int va, vr, vs, pteva, tpte;
	int flags, nleft, i, s, ctx;
	struct regmap *rp;
	struct segmap *sp;

#ifdef DEBUG
	if ((pmapdebug & PDB_CHANGEPROT) ||
	    (pmapdebug & PDB_REMOVE && prot == PROT_NONE))
		printf("pmap_page_protect(%p, 0x%x)\n", pg, prot);
#endif
	pv = &pg->mdpage.pv_head;
	/*
	 * Skip operations that do not take away write permission.
	 */
	if (prot & PROT_WRITE)
		return;
	write_user_windows();	/* paranoia */
	if (prot & PROT_READ) {
		pv_changepte4_4c(pv, 0, PG_W);
		return;
	}

	/*
	 * Remove all access to all people talking to this page.
	 * Walk down PV list, removing all mappings.
	 * The logic is much like that for pmap_remove,
	 * but we know we are removing exactly one page.
	 */
	s = splvm();
	if ((pm = pv->pv_pmap) == NULL) {
		splx(s);
		return;
	}
	ctx = getcontext4();
	pv0 = pv;
	flags = pv->pv_flags & ~PV_NC;
	while (pv != NULL) {
		pm = pv->pv_pmap;
		va = pv->pv_va;
		vr = VA_VREG(va);
		vs = VA_VSEG(va);
		rp = &pm->pm_regmap[vr];
#ifdef DIAGNOSTIC
		if (rp->rg_nsegmap == 0)
			panic("pmap_remove_all: empty vreg");
#endif
		sp = &rp->rg_segmap[vs];
#ifdef DIAGNOSTIC
		if (sp->sg_npte == 0)
			panic("pmap_remove_all: empty vseg");
#endif
		nleft = --sp->sg_npte;

		if (sp->sg_pmeg == seginval) {
			/* Definitely not a kernel map */
			if (nleft) {
				sp->sg_pte[VA_VPG(va)] = 0;
			} else {
				free(sp->sg_pte, M_VMPMAP, 0);
				sp->sg_pte = NULL;
				if (--rp->rg_nsegmap == 0) {
					free(rp->rg_segmap, M_VMPMAP, 0);
					rp->rg_segmap = NULL;
					GAP_WIDEN(pm,vr);
#if defined(SUN4_MMU3L)
					if (HASSUN4_MMU3L && rp->rg_smeg != reginval) {
						if (pm->pm_ctx) {
							setcontext4(pm->pm_ctxnum);
							setregmap(va, reginval);
						} else
							setcontext4(0);
						region_free(pm, rp->rg_smeg);
					}
#endif
				}
			}
			goto nextpv;
		}

		if (CTX_USABLE(pm,rp)) {
			setcontext4(pm->pm_ctxnum);
			pteva = va;
			cache_flush_page(va);
		} else {
			setcontext4(0);
			/* XXX use per-cpu pteva? */
			if (HASSUN4_MMU3L)
				setregmap(0, tregion);
			setsegmap(0, sp->sg_pmeg);
			pteva = VA_VPG(va) << PGSHIFT;
		}

		tpte = getpte4(pteva);
#ifdef DIAGNOSTIC
		if ((tpte & PG_V) == 0)
			panic("pmap_page_protect !PG_V: ctx %d, va 0x%x, pte 0x%x",
			      pm->pm_ctxnum, va, tpte);
#endif
		flags |= MR4_4C(tpte);

		if (nleft) {
			setpte4(pteva, 0);
			if (sp->sg_pte != NULL)
				sp->sg_pte[VA_VPG(pteva)] = 0;
			goto nextpv;
		}

		/* Entire segment is gone */
		if (pm == pmap_kernel()) {
#if defined(SUN4_MMU3L)
			if (!HASSUN4_MMU3L)
#endif
				for (i = ncontext; --i >= 0;) {
					setcontext4(i);
					setsegmap(va, seginval);
				}
			me_free(pm, sp->sg_pmeg);
			if (--rp->rg_nsegmap == 0) {
#if defined(SUN4_MMU3L)
				if (HASSUN4_MMU3L) {
					for (i = ncontext; --i >= 0;) {
						setcontext4(i);
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
#if defined(SUN4_MMU3L)
			else if (HASSUN4_MMU3L &&
				 rp->rg_smeg != reginval) {
				/* note: context already set earlier */
				setregmap(0, rp->rg_smeg);
				setsegmap(vs << SGSHIFT, seginval);
			}
#endif
			free(sp->sg_pte, M_VMPMAP, 0);
			sp->sg_pte = NULL;
			me_free(pm, sp->sg_pmeg);

			if (--rp->rg_nsegmap == 0) {
#if defined(SUN4_MMU3L)
				if (HASSUN4_MMU3L &&
				    rp->rg_smeg != reginval) {
					if (pm->pm_ctx)
						setregmap(va, reginval);
					region_free(pm, rp->rg_smeg);
				}
#endif
				free(rp->rg_segmap, M_VMPMAP, 0);
				rp->rg_segmap = NULL;
				GAP_WIDEN(pm,vr);
			}
		}

	nextpv:
		pm->pm_stats.resident_count--;
		npv = pv->pv_next;
		if (pv != pv0)
			pool_put(&pvpool, pv);
		pv = npv;
	}
	pv0->pv_pmap = NULL;
	pv0->pv_next = NULL;
	pv0->pv_flags = flags;
	setcontext4(ctx);
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
pmap_protect4_4c(struct pmap *pm, vaddr_t sva, vaddr_t eva, vm_prot_t prot)
{
	int va, nva, vr, vs;
	int s, ctx;
	struct regmap *rp;
	struct segmap *sp;

	if (pm == NULL || prot & PROT_WRITE)
		return;

	if ((prot & PROT_READ) == 0) {
		pmap_remove(pm, sva, eva);
		return;
	}

	write_user_windows();
	ctx = getcontext4();
	s = splvm();

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
		if (sp->sg_pte == NULL)
			panic("pmap_protect: no pages");
#endif
		if (sp->sg_pmeg == seginval) {
			int *pte = &sp->sg_pte[VA_VPG(va)];

			/* not in MMU; just clear PG_W from core copies */
			for (; va < nva; va += NBPG)
				*pte++ &= ~PG_W;
		} else {
			/* in MMU: take away write bits from MMU PTEs */
			if (CTX_USABLE(pm,rp)) {
				int tpte;

				/*
				 * Flush cache so that any existing cache
				 * tags are updated.  This is really only
				 * needed for PTEs that lose PG_W.
				 */
				setcontext4(pm->pm_ctxnum);
				for (; va < nva; va += NBPG) {
					tpte = getpte4(va);
					pmap_stats.ps_npg_prot_all++;
					if ((tpte & (PG_W|PG_TYPE)) ==
					    (PG_W|PG_OBMEM)) {
						pmap_stats.ps_npg_prot_actual++;
						cache_flush_page(va);
						setpte4(va, tpte & ~PG_W);
					}
				}
			} else {
				int pteva;

				/*
				 * No context, hence not cached;
				 * just update PTEs.
				 */
				setcontext4(0);
				/* XXX use per-cpu pteva? */
				if (HASSUN4_MMU3L)
					setregmap(0, tregion);
				setsegmap(0, sp->sg_pmeg);
				pteva = VA_VPG(va) << PGSHIFT;
				for (; va < nva; pteva += NBPG, va += NBPG)
					setpte4(pteva, getpte4(pteva) & ~PG_W);
			}
		}
	}
	splx(s);
	setcontext4(ctx);
}

/*
 * Change the protection and/or wired status of the given (MI) virtual page.
 * XXX: should have separate function (or flag) telling whether only wiring
 * is changing.
 */
void
pmap_changeprot4_4c(pm, va, prot, wired)
	struct pmap *pm;
	vaddr_t va;
	vm_prot_t prot;
	int wired;
{
	int vr, vs, tpte, newprot, ctx, s;
	struct regmap *rp;
	struct segmap *sp;

#ifdef DEBUG
	if (pmapdebug & PDB_CHANGEPROT)
		printf("pmap_changeprot(%p, 0x%lx, 0x%x, 0x%x)\n",
		    pm, va, prot, wired);
#endif

	write_user_windows();	/* paranoia */

	va = trunc_page(va);
	if (pm == pmap_kernel())
		newprot = prot & PROT_WRITE ? PG_S|PG_W : PG_S;
	else
		newprot = prot & PROT_WRITE ? PG_W : 0;
	vr = VA_VREG(va);
	vs = VA_VSEG(va);
	s = splvm();		/* conservative */
	rp = &pm->pm_regmap[vr];
	if (rp->rg_nsegmap == 0) {
		printf("pmap_changeprot: no segments in %d\n", vr);
		splx(s);
		return;
	}
	if (rp->rg_segmap == NULL) {
		printf("pmap_changeprot: no segments in %d!\n", vr);
		splx(s);
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
		int *pte = &sp->sg_pte[VA_VPG(va)];

		/* update in software */
		if ((*pte & PG_PROT) == newprot)
			goto useless;
		*pte = (*pte & ~PG_PROT) | newprot;
	} else {
		/* update in hardware */
		ctx = getcontext4();
		if (CTX_USABLE(pm,rp)) {
			/*
			 * Use current context.
			 * Flush cache if page has been referenced to
			 * avoid stale protection bits in the cache tags.
			 */
			setcontext4(pm->pm_ctxnum);
			tpte = getpte4(va);
			if ((tpte & PG_PROT) == newprot) {
				setcontext4(ctx);
				goto useless;
			}
			if ((tpte & (PG_U|PG_NC|PG_TYPE)) == (PG_U|PG_OBMEM))
				cache_flush_page((int)va);
		} else {
			setcontext4(0);
			/* XXX use per-cpu va? */
			if (HASSUN4_MMU3L)
				setregmap(0, tregion);
			setsegmap(0, sp->sg_pmeg);
			va = VA_VPG(va) << PGSHIFT;
			tpte = getpte4(va);
			if ((tpte & PG_PROT) == newprot) {
				setcontext4(ctx);
				goto useless;
			}
		}
		tpte = (tpte & ~PG_PROT) | newprot;
		setpte4(va, tpte);
		setcontext4(ctx);
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
pmap_page_protect4m(struct vm_page *pg, vm_prot_t prot)
{
	struct pvlist *pv, *pv0, *npv;
	struct pmap *pm;
	int va, vr, vs, tpte;
	int flags, s, ctx;
	struct regmap *rp;
	struct segmap *sp;

#ifdef DEBUG
	if ((pmapdebug & PDB_CHANGEPROT) ||
	    (pmapdebug & PDB_REMOVE && prot == PROT_NONE))
		printf("pmap_page_protect(%p, 0x%x)\n", pg, prot);
#endif
	pv = &pg->mdpage.pv_head;
	/*
	 * Skip operations that do not take away write permission.
	 */
	if (prot & PROT_WRITE)
		return;
	write_user_windows();	/* paranoia */
	if (prot & PROT_READ) {
		pv_changepte4m(pv, 0, PPROT_WRITE);
		return;
	}

	/*
	 * Remove all access to all people talking to this page.
	 * Walk down PV list, removing all mappings.
	 * The logic is much like that for pmap_remove,
	 * but we know we are removing exactly one page.
	 */
	s = splvm();
	if ((pm = pv->pv_pmap) == NULL) {
		splx(s);
		return;
	}
	ctx = getcontext4m();
	pv0 = pv;
	flags = pv->pv_flags;
	while (pv != NULL) {
		pm = pv->pv_pmap;
		va = pv->pv_va;
		vr = VA_VREG(va);
		vs = VA_VSEG(va);
		rp = &pm->pm_regmap[vr];
#ifdef DIAGNOSTIC
		if (rp->rg_nsegmap == 0)
			panic("pmap_page_protect4m: empty vreg");
#endif
		sp = &rp->rg_segmap[vs];
#ifdef DIAGNOSTIC
		if (sp->sg_npte == 0)
			panic("pmap_page_protect4m: empty vseg");
#endif
		sp->sg_npte--;

		/* Invalidate PTE in pagetables. Flush cache if necessary */
		if (pm->pm_ctx) {
			setcontext4m(pm->pm_ctxnum);
			cache_flush_page(va);
			tlb_flush_page(va);
		}

		tpte = sp->sg_pte[VA_SUN4M_VPG(va)];

#ifdef DIAGNOSTIC
		if ((tpte & SRMMU_TETYPE) != SRMMU_TEPTE)
			panic("pmap_page_protect4m: !TEPTE");
#endif

		flags |= MR4M(tpte);

		setpgt4m(&sp->sg_pte[VA_SUN4M_VPG(va)], SRMMU_TEINVALID);
		pm->pm_stats.resident_count--;

		/* Entire segment is gone */
		if (sp->sg_npte == 0 && pm != pmap_kernel()) {
			if (pm->pm_ctx)
				tlb_flush_segment(vr, vs);
			setpgt4m(&rp->rg_seg_ptps[vs], SRMMU_TEINVALID);
			pool_put(&L23_pool, sp->sg_pte);
			sp->sg_pte = NULL;

			if (--rp->rg_nsegmap == 0) {
				if (pm->pm_ctx)
					tlb_flush_context();
				setpgt4m(&pm->pm_reg_ptps[vr], SRMMU_TEINVALID);
				free(rp->rg_segmap, M_VMPMAP, 0);
				rp->rg_segmap = NULL;
				pool_put(&L23_pool, rp->rg_seg_ptps);
			}
		}

		npv = pv->pv_next;
		if (pv != pv0)
			pool_put(&pvpool, pv);
		pv = npv;
	}
	pv0->pv_pmap = NULL;
	pv0->pv_next = NULL;
	pv0->pv_flags = (flags | PV_C4M) & ~PV_ANC;
	setcontext4m(ctx);
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
pmap_protect4m(struct pmap *pm, vaddr_t sva, vaddr_t eva, vm_prot_t prot)
{
	int va, nva, vr, vs;
	int s, ctx;
	struct regmap *rp;
	struct segmap *sp;
	int newprot;

	if ((prot & PROT_READ) == 0) {
		pmap_remove(pm, sva, eva);
		return;
	}

	/*
	 * Since the caller might request either a removal of PROT_EXEC
	 * or PROT_WRITE, we don't attempt to guess what to do, just lower
	 * to read-only and let the real protection be faulted in.
	 */
	newprot = pte_prot4m(pm, PROT_READ);

	write_user_windows();
	ctx = getcontext4m();
	s = splvm();

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
		/* pages loaded: take away write bits from MMU PTEs */
		if (pm->pm_ctx)
			setcontext4m(pm->pm_ctxnum);

		pmap_stats.ps_npg_prot_all += (nva - va) >> PGSHIFT;
		for (; va < nva; va += NBPG) {
			int tpte, npte;

			tpte = sp->sg_pte[VA_SUN4M_VPG(va)];
			npte = (tpte & ~SRMMU_PROT_MASK) | newprot;

			/* Only do work when needed. */
			if (npte == tpte)
				continue;

			pmap_stats.ps_npg_prot_actual++;
			/*
			 * Flush cache so that any existing cache
			 * tags are updated.
			 */
			if (pm->pm_ctx) {
				if ((tpte & SRMMU_PGTYPE) == PG_SUN4M_OBMEM) {
					cache_flush_page(va);
				}
				tlb_flush_page(va);
			}
			setpgt4m(&sp->sg_pte[VA_SUN4M_VPG(va)], npte);
		}
	}
	splx(s);
	setcontext4m(ctx);
}

/*
 * Change the protection and/or wired status of the given (MI) virtual page.
 * XXX: should have separate function (or flag) telling whether only wiring
 * is changing.
 */
void
pmap_changeprot4m(pm, va, prot, wired)
	struct pmap *pm;
	vaddr_t va;
	vm_prot_t prot;
	int wired;
{
	int tpte, newprot, ctx, s;
	int *ptep;

#ifdef DEBUG
	if (pmapdebug & PDB_CHANGEPROT)
		printf("pmap_changeprot(%p, 0x%lx, 0x%x, 0x%x)\n",
		    pm, va, prot, wired);
#endif

	write_user_windows();	/* paranoia */

	va = trunc_page(va);
	newprot = pte_prot4m(pm, prot);

	pmap_stats.ps_changeprots++;

	s = splvm();		/* conservative */
	ptep = getptep4m(pm, va);
	if (pm->pm_ctx) {
		ctx = getcontext4m();
		setcontext4m(pm->pm_ctxnum);
		/*
		 * Use current context.
		 * Flush cache if page has been referenced to
		 * avoid stale protection bits in the cache tags.
		 */
		tpte = *ptep;
		if ((tpte & (SRMMU_PG_C|SRMMU_PGTYPE)) ==
		    (SRMMU_PG_C|PG_SUN4M_OBMEM))
			cache_flush_page(va);
		tlb_flush_page(va);
		setcontext4m(ctx);
	} else {
		tpte = *ptep;
	}
	if ((tpte & SRMMU_PROT_MASK) == newprot) {
		/* only wiring changed, and we ignore wiring */
		pmap_stats.ps_useless_changeprots++;
		goto out;
	}
	setpgt4m(ptep, (tpte & ~SRMMU_PROT_MASK) | newprot);

out:
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

#if defined(SUN4) || defined(SUN4C) || defined(SUN4E)

int
pmap_enter4_4c(pm, va, pa, prot, flags)
	struct pmap *pm;
	vaddr_t va;
	paddr_t pa;
	vm_prot_t prot;
	int flags;
{
	struct pvlist *pv;
	int pteproto, ctx;
	int ret;

	if (VA_INHOLE(va)) {
#ifdef DEBUG
		printf("pmap_enter: pm %p, va 0x%lx, pa 0x%lx: in MMU hole\n",
			pm, va, pa);
#endif
		return (EINVAL);
	}

#ifdef DEBUG
	if (pmapdebug & PDB_ENTER)
		printf("pmap_enter(%p, 0x%lx, 0x%lx, 0x%x, 0x%x)\n",
		    pm, va, pa, prot, flags);
#endif

	pteproto = PG_V | PMAP_T2PTE_4(pa);
	pa &= ~PMAP_TNC_4;
	/*
	 * Set up prototype for new PTE.  Cannot set PG_NC from PV_NC yet
	 * since the pvlist no-cache bit might change as a result of the
	 * new mapping.
	 */
	if ((pteproto & PG_TYPE) == PG_OBMEM)
		pv = pvhead(atop(pa));
	else
		pv = NULL;

	pteproto |= atop(pa) & PG_PFNUM;
	if (prot & PROT_WRITE)
		pteproto |= PG_W;

	ctx = getcontext4();
	if (pm == pmap_kernel())
		ret = pmap_enk4_4c(pm, va, prot, flags, pv, pteproto | PG_S);
	else
		ret = pmap_enu4_4c(pm, va, prot, flags, pv, pteproto);
#ifdef DIAGNOSTIC
	if ((flags & PMAP_CANFAIL) == 0 && ret != 0)
		panic("pmap_enter4_4c: can't fail, but did");
#endif
	setcontext4(ctx);

	return (ret);
}

/* enter new (or change existing) kernel mapping */
int
pmap_enk4_4c(pm, va, prot, flags, pv, pteproto)
	struct pmap *pm;
	vaddr_t va;
	vm_prot_t prot;
	int flags;
	struct pvlist *pv;
	int pteproto;
{
	int vr, vs, tpte, i, s;
	struct regmap *rp;
	struct segmap *sp;
	int wired = (flags & PMAP_WIRED) != 0;

	vr = VA_VREG(va);
	vs = VA_VSEG(va);
	rp = &pm->pm_regmap[vr];
	sp = &rp->rg_segmap[vs];
	s = splvm();		/* XXX way too conservative */

#if defined(SUN4_MMU3L)
	if (HASSUN4_MMU3L && rp->rg_smeg == reginval) {
		vaddr_t tva;
		rp->rg_smeg = region_alloc(&region_locked, pm, vr)->me_cookie;
		i = ncontext - 1;
		do {
			setcontext4(i);
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
		/* old mapping exists, and is of the same pa type */
		if ((tpte & (PG_PFNUM|PG_TYPE)) ==
		    (pteproto & (PG_PFNUM|PG_TYPE))) {
			/* just changing protection and/or wiring */
			splx(s);
			pmap_changeprot4_4c(pm, va, prot, wired);
			return (0);
		}

		if ((tpte & PG_TYPE) == PG_OBMEM) {
			struct pvlist *pv1;

			/*
			 * Switcheroo: changing pa for this va.
			 * If old pa was managed, remove from pvlist.
			 * If old page was cached, flush cache.
			 */
			pv1 = pvhead(tpte & PG_PFNUM);
			if (pv1)
				pv_unlink4_4c(pv1, pm, va);
			if ((tpte & PG_NC) == 0) {
				setcontext4(0);	/* ??? */
				cache_flush_page((int)va);
			}
		}
		pm->pm_stats.resident_count--;
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
		pteproto |= pv_link4_4c(pv, pm, va, pteproto & PG_NC);

	if (sp->sg_pmeg == seginval) {
		int tva;

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

#if defined(SUN4_MMU3L)
		if (HASSUN4_MMU3L)
			setsegmap(va, sp->sg_pmeg);
		else
#endif
		{
			i = ncontext - 1;
			do {
				setcontext4(i);
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
	pm->pm_stats.resident_count++;
	splx(s);

	return (0);
}

/* enter new (or change existing) user mapping */
int
pmap_enu4_4c(pm, va, prot, flags, pv, pteproto)
	struct pmap *pm;
	vaddr_t va;
	vm_prot_t prot;
	int flags;
	struct pvlist *pv;
	int pteproto;
{
	int vr, vs, *pte, tpte, pmeg, s, doflush;
	struct regmap *rp;
	struct segmap *sp;
	int wired = (flags & PMAP_WIRED) != 0;

	write_user_windows();		/* XXX conservative */
	vr = VA_VREG(va);
	vs = VA_VSEG(va);
	rp = &pm->pm_regmap[vr];
	s = splvm();			/* XXX conservative */

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
		printf("pmap_enu: gap_start 0x%x, gap_end 0x%x",
			pm->pm_gap_start, pm->pm_gap_end);
		panic("pmap_enu: gap botch");
	}
#endif

	if (rp->rg_segmap == NULL) {
		/* definitely a new mapping */
		int i;
		int size = NSEGRG * sizeof (struct segmap);

		sp = malloc((u_long)size, M_VMPMAP, M_NOWAIT);
		if (sp == NULL) {
			splx(s);
			return (ENOMEM);
		}
		qzero((caddr_t)sp, size);
		rp->rg_segmap = sp;
		rp->rg_nsegmap = 0;
		for (i = NSEGRG; --i >= 0;)
			sp++->sg_pmeg = seginval;
	}

	sp = &rp->rg_segmap[vs];
	if ((pte = sp->sg_pte) == NULL) {
		/* definitely a new mapping */
		int size = NPTESG * sizeof *pte;

		pte = malloc((u_long)size, M_VMPMAP, M_NOWAIT);
		if (pte == NULL) {
			splx(s);
			return (ENOMEM);
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
				setcontext4(pm->pm_ctxnum);
				tpte = getpte4(va);
				doflush = CACHEINFO.c_vactype != VAC_NONE;
			} else {
				setcontext4(0);
				/* XXX use per-cpu pteva? */
				if (HASSUN4_MMU3L)
					setregmap(0, tregion);
				setsegmap(0, pmeg);
				tpte = getpte4(VA_VPG(va) << PGSHIFT);
			}
		}
		if (tpte & PG_V) {
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
				return (0);
			}
			/*
			 * Switcheroo: changing pa for this va.
			 * If old pa was managed, remove from pvlist.
			 * If old page was cached, flush cache.
			 */
			if ((tpte & PG_TYPE) == PG_OBMEM) {
				struct pvlist *pv1;

				pv1 = pvhead(tpte & PG_PFNUM);
				if (pv1)
					pv_unlink4_4c(pv1, pm, va);
				if (doflush && (tpte & PG_NC) == 0)
					cache_flush_page((int)va);
			}
			pm->pm_stats.resident_count--;
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
		pteproto |= pv_link4_4c(pv, pm, va, pteproto & PG_NC);

	/*
	 * Update hardware & software PTEs.
	 */
	if ((pmeg = sp->sg_pmeg) != seginval) {
		/* ptes are in hardware */
		if (CTX_USABLE(pm,rp))
			setcontext4(pm->pm_ctxnum);
		else {
			setcontext4(0);
			/* XXX use per-cpu pteva? */
			if (HASSUN4_MMU3L)
				setregmap(0, tregion);
			setsegmap(0, pmeg);
			va = VA_VPG(va) << PGSHIFT;
		}
		setpte4(va, pteproto);
	}
	/* update software copy */
	pte += VA_VPG(va);
	*pte = pteproto;
	pm->pm_stats.resident_count++;

	splx(s);

	return (0);
}

void
pmap_kenter_pa4_4c(va, pa, prot)
	vaddr_t va;
	paddr_t pa;
	vm_prot_t prot;
{
	struct pvlist *pv;
	int pteproto, ctx;

	pteproto = PG_S | PG_V | PMAP_T2PTE_4(pa);
	if (prot & PROT_WRITE)
		pteproto |= PG_W;

	pa &= ~PMAP_TNC_4;

	if ((pteproto & PG_TYPE) == PG_OBMEM)
		pv = pvhead(atop(pa));
	else
		pv = NULL;

	pteproto |= atop(pa) & PG_PFNUM;

	ctx = getcontext4();
	pmap_enk4_4c(pmap_kernel(), va, prot, PMAP_WIRED, pv, pteproto);
	setcontext4(ctx);
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
 */

int
pmap_enter4m(pm, va, pa, prot, flags)
	struct pmap *pm;
	vaddr_t va;
	paddr_t pa;
	vm_prot_t prot;
	int flags;
{
	struct pvlist *pv;
	int pteproto, ctx;
	int ret;

#ifdef DEBUG
	if (pmapdebug & PDB_ENTER)
		printf("pmap_enter(%p, 0x%lx, 0x%lx, 0x%x, 0x%x)\n",
		    pm, va, pa, prot, flags);
#endif

	/* Initialise pteproto with cache bit */
	pteproto = (pa & PMAP_NC) == 0 ? SRMMU_PG_C : 0;

#ifdef DEBUG
	if (pa & PMAP_TYPE_SRMMU) {	/* this page goes in an iospace */
		if (cpuinfo.cpu_type == CPUTYP_MS1)
			panic("pmap_enter4m: attempt to use 36-bit iospace on"
			      " MicroSPARC");
	}
#endif
	pteproto |= PMAP_T2PTE_SRMMU(pa);

	pteproto |= SRMMU_TEPTE;

	pa &= ~PMAP_TNC_SRMMU;
	/*
	 * Set up prototype for new PTE.  Cannot set PG_NC from PV_NC yet
	 * since the pvlist no-cache bit might change as a result of the
	 * new mapping.
	 */
	if ((pteproto & SRMMU_PGTYPE) == PG_SUN4M_OBMEM)
		pv = pvhead(atop(pa));
	else
		pv = NULL;

	pteproto |= (atop(pa) << SRMMU_PPNSHIFT);

	/* correct protections */
	pteproto |= pte_prot4m(pm, prot);

	ctx = getcontext4m();
	if (pm == pmap_kernel())
		ret = pmap_enk4m(pm, va, prot, flags, pv, pteproto);
	else
		ret = pmap_enu4m(pm, va, prot, flags, pv, pteproto);
#ifdef DIAGNOSTIC
	if ((flags & PMAP_CANFAIL) == 0 && ret != 0)
		panic("pmap_enter4m: can't fail, but did");
#endif
	if (pv) {
		if (flags & PROT_WRITE)
			pv->pv_flags |= PV_MOD4M;
		if (flags & PROT_READ)
			pv->pv_flags |= PV_REF4M;
	}
	setcontext4m(ctx);

	return (ret);
}

/* enter new (or change existing) kernel mapping */
int
pmap_enk4m(pm, va, prot, flags, pv, pteproto)
	struct pmap *pm;
	vaddr_t va;
	vm_prot_t prot;
	int flags;
	struct pvlist *pv;
	int pteproto;
{
	int tpte, s;
	struct regmap *rp;
	struct segmap *sp;
	int wired = (flags & PMAP_WIRED) != 0;

#ifdef DIAGNOSTIC
	if (VA_VREG(va) < NUREG_4M)
		panic("pmap_enk4m: can't enter va 0x%lx below VM_MIN_KERNEL_ADDRESS", va);
#endif
	rp = &pm->pm_regmap[VA_VREG(va)];
	sp = &rp->rg_segmap[VA_VSEG(va)];

	s = splvm();		/* XXX way too conservative */

#ifdef DEBUG
	if (rp->rg_seg_ptps == NULL) /* enter new region */
		panic("pmap_enk4m: missing region table for va 0x%lx", va);
	if (sp->sg_pte == NULL) /* If no existing pagetable */
		panic("pmap_enk4m: missing segment table for va 0x%lx", va);
#endif

	tpte = sp->sg_pte[VA_SUN4M_VPG(va)];
	if ((tpte & SRMMU_TETYPE) == SRMMU_TEPTE) {
		/* old mapping exists, and is of the same pa type */

		if ((tpte & SRMMU_PPNMASK) == (pteproto & SRMMU_PPNMASK)) {
			/* just changing protection and/or wiring */
			splx(s);
			pmap_changeprot4m(pm, va, prot, wired);
			return (0);
		}

		if ((tpte & SRMMU_PGTYPE) == PG_SUN4M_OBMEM) {
			struct pvlist *pv1;

			/*
			 * Switcheroo: changing pa for this va.
			 * If old pa was managed, remove from pvlist.
			 * If old page was cached, flush cache.
			 */
			pv1 = pvhead((tpte & SRMMU_PPNMASK) >> SRMMU_PPNSHIFT);
			if (pv1)
				pv_unlink4m(pv1, pm, va);
			if (tpte & SRMMU_PG_C) {
				setcontext4m(0);	/* ??? */
				cache_flush_page((int)va);
			}
		}
		pm->pm_stats.resident_count--;
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
	        pteproto &= ~(pv_link4m(pv, pm, va, (pteproto & SRMMU_PG_C) == 0));

	tlb_flush_page(va);
	setpgt4m(&sp->sg_pte[VA_SUN4M_VPG(va)], pteproto);
	pm->pm_stats.resident_count++;

	splx(s);

	return (0);
}

/* enter new (or change existing) user mapping */
int
pmap_enu4m(pm, va, prot, flags, pv, pteproto)
	struct pmap *pm;
	vaddr_t va;
	vm_prot_t prot;
	int flags;
	struct pvlist *pv;
	int pteproto;
{
	int vr, vs, *pte, tpte, s;
	struct regmap *rp;
	struct segmap *sp;
	int wired = (flags & PMAP_WIRED) != 0;

#ifdef DEBUG
	if (VA_VREG(va) >= NUREG_4M)
		panic("pmap_enu4m: can't enter va 0x%lx above VM_MIN_KERNEL_ADDRESS", va);
#endif

	write_user_windows();		/* XXX conservative */
	vr = VA_VREG(va);
	vs = VA_VSEG(va);
	rp = &pm->pm_regmap[vr];
	s = splvm();			/* XXX conservative */
	if (rp->rg_segmap == NULL) {
		/* definitely a new mapping */
		int size = NSEGRG * sizeof (struct segmap);

		sp = malloc((u_long)size, M_VMPMAP, M_NOWAIT);
		if (sp == NULL) {
			splx(s);
			return (ENOMEM);
		}
		qzero((caddr_t)sp, size);
		rp->rg_segmap = sp;
		rp->rg_nsegmap = 0;
		rp->rg_seg_ptps = NULL;
	}

	if (rp->rg_seg_ptps == NULL) {
		/* Need a segment table */
		int i, *ptd;

		ptd = pool_get(&L23_pool, PR_NOWAIT);
		if (ptd == NULL) {
			splx(s);
			return (ENOMEM);
		}

		rp->rg_seg_ptps = ptd;
		for (i = 0; i < SRMMU_L2SIZE; i++)
			setpgt4m(&ptd[i], SRMMU_TEINVALID);
		setpgt4m(&pm->pm_reg_ptps[vr],
			 (VA2PA((caddr_t)ptd) >> SRMMU_PPNPASHIFT) | SRMMU_TEPTD);
	}

	sp = &rp->rg_segmap[vs];
	if ((pte = sp->sg_pte) == NULL) {
		/* definitely a new mapping */
		int i;

		pte = pool_get(&L23_pool, PR_NOWAIT);
		if (pte == NULL) {
			splx(s);
			return (ENOMEM);
		}

		sp->sg_pte = pte;
		sp->sg_npte = 1;
		rp->rg_nsegmap++;
		for (i = 0; i < SRMMU_L3SIZE; i++)
			setpgt4m(&pte[i], SRMMU_TEINVALID);
		setpgt4m(&rp->rg_seg_ptps[vs],
			(VA2PA((caddr_t)pte) >> SRMMU_PPNPASHIFT) | SRMMU_TEPTD);
	} else {
		/*
		 * Might be a change: fetch old pte
		 * Note we're only interested in the PTE's page frame
		 * number and type bits, so the memory copy will do.
		 */
		tpte = pte[VA_SUN4M_VPG(va)];

		if ((tpte & SRMMU_TETYPE) == SRMMU_TEPTE) {
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
				return (0);
			}
			/*
			 * Switcheroo: changing pa for this va.
			 * If old pa was managed, remove from pvlist.
			 * If old page was cached, flush cache.
			 */
			if ((tpte & SRMMU_PGTYPE) == PG_SUN4M_OBMEM) {
				struct pvlist *pv1;

				pv1 = pvhead((tpte & SRMMU_PPNMASK) >>
					     SRMMU_PPNSHIFT);
				if (pv1)
					pv_unlink4m(pv1, pm, va);
				if (pm->pm_ctx && (tpte & SRMMU_PG_C))
					cache_flush_page((int)va);
			}
			pm->pm_stats.resident_count--;
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
	        pteproto &= ~(pv_link4m(pv, pm, va, (pteproto & SRMMU_PG_C) == 0));

	/*
	 * Update PTEs, flush TLB as necessary.
	 */
	if (pm->pm_ctx) {
		setcontext4m(pm->pm_ctxnum);
		tlb_flush_page(va);
	}
	setpgt4m(&sp->sg_pte[VA_SUN4M_VPG(va)], pteproto);
	pm->pm_stats.resident_count++;

	splx(s);

	return (0);
}

void
pmap_kenter_pa4m(va, pa, prot)
	vaddr_t va;
	paddr_t pa;
	vm_prot_t prot;
{
	struct pvlist *pv;
	int pteproto, ctx;

	pteproto = ((pa & PMAP_NC) == 0 ? SRMMU_PG_C : 0) |
		PMAP_T2PTE_SRMMU(pa) | SRMMU_TEPTE |
		((prot & PROT_WRITE) ? PPROT_N_RWX : PPROT_N_RX);

	pa &= ~PMAP_TNC_SRMMU;

	pteproto |= atop(pa) << SRMMU_PPNSHIFT;

	pv = pvhead(atop(pa));

	ctx = getcontext4m();
	pmap_enk4m(pmap_kernel(), va, prot, PMAP_WIRED, pv, pteproto);
	setcontext4m(ctx);
}

#endif /* sun4m */

/*
 * Change the wiring attribute for a map/virtual-address pair.
 */
/* ARGSUSED */
void
pmap_unwire(pm, va)
	struct pmap *pm;
	vaddr_t va;
{

	pmap_stats.ps_useless_changewire++;
}

/*
 * Extract the physical page address associated
 * with the given map/virtual_address pair.
 * GRR, the vm code knows; we should not have to do this!
 */

#if defined(SUN4) || defined(SUN4C) || defined(SUN4E)
boolean_t
pmap_extract4_4c(pm, va, pa)
	struct pmap *pm;
	vaddr_t va;
	paddr_t *pa;
{
	int tpte;
	int vr, vs;
	struct regmap *rp;
	struct segmap *sp;

	if (pm == NULL) {
#ifdef DEBUG
		if (pmapdebug & PDB_FOLLOW)
			printf("pmap_extract: null pmap\n");
#endif
		return (FALSE);
	}
	vr = VA_VREG(va);
	vs = VA_VSEG(va);
	rp = &pm->pm_regmap[vr];
	if (rp->rg_segmap == NULL) {
#ifdef DEBUG
		if (pmapdebug & PDB_FOLLOW)
			printf("pmap_extract: invalid segment (%d)\n", vr);
#endif
		return (FALSE);
	}
	sp = &rp->rg_segmap[vs];

	if (sp->sg_pmeg != seginval) {
		int ctx = getcontext4();

		if (CTX_USABLE(pm,rp)) {
			CHANGE_CONTEXTS(ctx, pm->pm_ctxnum);
			tpte = getpte4(va);
		} else {
			CHANGE_CONTEXTS(ctx, 0);
			if (HASSUN4_MMU3L)
				setregmap(0, tregion);
			setsegmap(0, sp->sg_pmeg);
			tpte = getpte4(VA_VPG(va) << PGSHIFT);
		}
		setcontext4(ctx);
	} else {
		int *pte = sp->sg_pte;

		if (pte == NULL) {
#ifdef DEBUG
			if (pmapdebug & PDB_FOLLOW)
				printf("pmap_extract: invalid segment\n");
#endif
			return (FALSE);
		}
		tpte = pte[VA_VPG(va)];
	}
	if ((tpte & PG_V) == 0) {
#ifdef DEBUG
		if (pmapdebug & PDB_FOLLOW)
			printf("pmap_extract: invalid pte\n");
#endif
		return (FALSE);
	}
	tpte &= PG_PFNUM;
	tpte = tpte;
	*pa = ((tpte << PGSHIFT) | (va & PGOFSET));
	return (TRUE);
}
#endif /*4,4c*/

#if defined(SUN4M)		/* 4m version of pmap_extract */
/*
 * Extract the physical page address associated
 * with the given map/virtual_address pair.
 * GRR, the vm code knows; we should not have to do this!
 */
boolean_t
pmap_extract4m(pm, va, pa)
	struct pmap *pm;
	vaddr_t va;
	paddr_t *pa;
{
	struct regmap *rm;
	struct segmap *sm;
	int pte;

	if (pm == NULL) {
#ifdef DEBUG
		if (pmapdebug & PDB_FOLLOW)
			printf("pmap_extract: null pmap\n");
#endif
		return (FALSE);
	}

	if ((rm = pm->pm_regmap) == NULL) {
#ifdef DEBUG
		if (pmapdebug & PDB_FOLLOW)
			printf("pmap_extract: no regmap entry");
#endif
		return (FALSE);
	}

	rm += VA_VREG(va);
	if ((sm = rm->rg_segmap) == NULL) {
#ifdef DEBUG
		if (pmapdebug & PDB_FOLLOW)
			printf("pmap_extract: no segmap");
#endif
		return (FALSE);
	}

	sm += VA_VSEG(va);
	if (sm->sg_pte == NULL) {
#ifdef DEBUG
		if (pmapdebug & PDB_FOLLOW)
			panic("pmap_extract: no ptes");
#endif
		return FALSE;
	}

	pte = sm->sg_pte[VA_SUN4M_VPG(va)];
	if ((pte & SRMMU_TETYPE) != SRMMU_TEPTE) {
#ifdef DEBUG
		if (pmapdebug & PDB_FOLLOW)
			printf("pmap_extract: invalid pte of type %d\n",
			       pte & SRMMU_TETYPE);
#endif
		return (FALSE);
	}

	*pa = (ptoa((pte & SRMMU_PPNMASK) >> SRMMU_PPNSHIFT) | VA_OFF(va));
	return (TRUE);
}
#endif /* sun4m */

#if defined(SUN4) || defined(SUN4C) || defined(SUN4E)

/*
 * Clear the modify bit for the given physical page.
 */
boolean_t
pmap_clear_modify4_4c(struct vm_page *pg)
{
	struct pvlist *pv;
	boolean_t ret;

	pv = &pg->mdpage.pv_head;	

	(void) pv_syncflags4_4c(pv);
	ret = pv->pv_flags & PV_MOD;
	pv->pv_flags &= ~PV_MOD;

	return ret;
}

/*
 * Tell whether the given physical page has been modified.
 */
boolean_t
pmap_is_modified4_4c(struct vm_page *pg)
{
	struct pvlist *pv;

	pv = &pg->mdpage.pv_head;

	return (pv->pv_flags & PV_MOD || pv_syncflags4_4c(pv) & PV_MOD);
}

/*
 * Clear the reference bit for the given physical page.
 */
boolean_t
pmap_clear_reference4_4c(struct vm_page *pg)
{
	struct pvlist *pv;
	boolean_t ret;

	pv = &pg->mdpage.pv_head;

	(void) pv_syncflags4_4c(pv);
	ret = pv->pv_flags & PV_REF;
	pv->pv_flags &= ~PV_REF;

	return ret;
}

/*
 * Tell whether the given physical page has been referenced.
 */
boolean_t
pmap_is_referenced4_4c(struct vm_page *pg)
{
	struct pvlist *pv;

	pv = &pg->mdpage.pv_head;

	return (pv->pv_flags & PV_REF || pv_syncflags4_4c(pv) & PV_REF);
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
boolean_t
pmap_clear_modify4m(struct vm_page *pg)
{
	struct pvlist *pv;
	boolean_t ret;

	pv = &pg->mdpage.pv_head;

	(void) pv_syncflags4m(pv);
	ret = pv->pv_flags & PV_MOD4M;
	pv->pv_flags &= ~PV_MOD4M;

	return ret;
}

/*
 * Tell whether the given physical page has been modified.
 */
boolean_t
pmap_is_modified4m(struct vm_page *pg)
{
	struct pvlist *pv;

	pv = &pg->mdpage.pv_head;

	return (pv->pv_flags & PV_MOD4M || pv_syncflags4m(pv) & PV_MOD4M);
}

/*
 * Clear the reference bit for the given physical page.
 */
boolean_t
pmap_clear_reference4m(struct vm_page *pg)
{
	struct pvlist *pv;
	boolean_t ret;

	pv = &pg->mdpage.pv_head;

	(void) pv_syncflags4m(pv);
	ret = pv->pv_flags & PV_REF4M;
	pv->pv_flags &= ~PV_REF4M;

	return ret;
}

/*
 * Tell whether the given physical page has been referenced.
 */
boolean_t
pmap_is_referenced4m(struct vm_page *pg)
{
	struct pvlist *pv;

	pv = &pg->mdpage.pv_head;

	return (pv->pv_flags & PV_REF4M || pv_syncflags4m(pv) & PV_REF4M);
}
#endif /* 4m */

/*
 * Fill the given MI physical page with zero bytes.
 *
 * We avoid stomping on the cache.
 * XXX	might be faster to use destination's context and allow cache to fill?
 */

#if defined(SUN4) || defined(SUN4C) || defined(SUN4E)

void
pmap_zero_page4_4c(struct vm_page *pg)
{
	paddr_t pa = VM_PAGE_TO_PHYS(pg);
	caddr_t va;
	int pte;

	/*
	 * The following might not be necessary since the page
	 * is being cleared because it is about to be allocated,
	 * i.e., is in use by no one.
	 */
	pg_flushcache(pg);

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
pmap_copy_page4_4c(struct vm_page *srcpg, struct vm_page *dstpg)
{
	paddr_t src = VM_PAGE_TO_PHYS(srcpg);
	paddr_t dst = VM_PAGE_TO_PHYS(dstpg);
	caddr_t sva, dva;
	int spte, dpte;

	if (CACHEINFO.c_vactype == VAC_WRITEBACK)
		pg_flushcache(srcpg);

	spte = PG_V | PG_S | (atop(src) & PG_PFNUM);

	if (CACHEINFO.c_vactype != VAC_NONE)
		pg_flushcache(dstpg);

	dpte = PG_V | PG_S | PG_W | PG_NC | (atop(dst) & PG_PFNUM);

	sva = vpage[0];
	dva = vpage[1];
	setpte4(sva, spte);
	setpte4(dva, dpte);
	qcopy(sva, dva, NBPG);	/* loads cache, so we must ... */
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
pmap_zero_page4m(struct vm_page *pg)
{
	paddr_t pa = VM_PAGE_TO_PHYS(pg);
	static vaddr_t va;
	static int *ptep;
	int pte;

	if (ptep == NULL)
		ptep = getptep4m(pmap_kernel(), (va = (vaddr_t)vpage[0]));

	if (CACHEINFO.c_vactype != VAC_NONE) {
		/*
		 * The following might not be necessary since the page
		 * is being cleared because it is about to be allocated,
		 * i.e., is in use by no one.
		 */
		pg_flushcache(pg);
	}

	pte = (SRMMU_TEPTE | (atop(pa) << SRMMU_PPNSHIFT) | PPROT_N_RWX);

	if (cpuinfo.flags & CPUFLG_CACHE_MANDATORY)
		pte |= SRMMU_PG_C;
	else
		pte &= ~SRMMU_PG_C;

	tlb_flush_page(va);
	setpgt4m(ptep, pte);
	qzero((caddr_t)va, PAGE_SIZE);
	tlb_flush_page(va);
	setpgt4m(ptep, SRMMU_TEINVALID);
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
pmap_copy_page4m(struct vm_page *srcpg, struct vm_page *dstpg)
{
	paddr_t src = VM_PAGE_TO_PHYS(srcpg);
	paddr_t dst = VM_PAGE_TO_PHYS(dstpg);
	static int *sptep, *dptep;
	static vaddr_t sva, dva;
	int spte, dpte;

	if (sptep == NULL) {
		sptep = getptep4m(pmap_kernel(), (sva = (vaddr_t)vpage[0]));
		dptep = getptep4m(pmap_kernel(), (dva = (vaddr_t)vpage[1]));
	}

	if (CACHEINFO.c_vactype == VAC_WRITEBACK)
		pg_flushcache(srcpg);

	spte = SRMMU_TEPTE | SRMMU_PG_C | (atop(src) << SRMMU_PPNSHIFT) |
	    PPROT_N_RX;

	if (CACHEINFO.c_vactype != VAC_NONE)
		pg_flushcache(dstpg);

	dpte = (SRMMU_TEPTE | (atop(dst) << SRMMU_PPNSHIFT) | PPROT_N_RWX);
	if (cpuinfo.flags & CPUFLG_CACHE_MANDATORY)
		dpte |= SRMMU_PG_C;
	else
		dpte &= ~SRMMU_PG_C;

	tlb_flush_page(sva);
	setpgt4m(sptep, spte);
	tlb_flush_page(dva);
	setpgt4m(dptep, dpte);
	qcopy((caddr_t)sva, (caddr_t)dva, PAGE_SIZE);
	cache_flush_page((int)sva);
	tlb_flush_page(sva);
	setpgt4m(sptep, SRMMU_TEINVALID);
	tlb_flush_page(dva);
	setpgt4m(dptep, SRMMU_TEINVALID);
}
#endif /* Sun4M */

/*
 * Turn on/off cache for a given (va, number of pages).
 *
 * We just assert PG_NC for each PTE; the addresses must reside
 * in locked kernel space.  A cache flush is also done.
 */
void
kvm_setcache(va, npages, cached)
	caddr_t va;
	int npages;
	int cached;
{
	int pte, ctx;
	struct pvlist *pv;

	if (CPU_ISSUN4M) {
#if defined(SUN4M)
		ctx = getcontext4m();
		setcontext4m(0);
		for (; --npages >= 0; va += NBPG) {
			int *ptep;

			ptep = getptep4m(pmap_kernel(), (vaddr_t)va);
			pte = *ptep;
#ifdef DIAGNOSTIC
			if ((pte & SRMMU_TETYPE) != SRMMU_TEPTE)
				panic("kvm_uncache: table entry not pte");
#endif
			pv = pvhead((pte & SRMMU_PPNMASK) >> SRMMU_PPNSHIFT);
			if (pv) {
				if (cached)
					pv_changepte4m(pv, SRMMU_PG_C, 0);
				else
					pv_changepte4m(pv, 0, SRMMU_PG_C);
			}
			if (cached)
				pte |= SRMMU_PG_C;
			else
				pte &= ~SRMMU_PG_C;
			tlb_flush_page((vaddr_t)va);
			setpgt4m(ptep, pte);

			if ((pte & SRMMU_PGTYPE) == PG_SUN4M_OBMEM)
				cache_flush_page((int)va);

		}
		setcontext4m(ctx);

#endif
	} else {
#if defined(SUN4) || defined(SUN4C) || defined(SUN4E)
		ctx = getcontext4();
		setcontext4(0);
		for (; --npages >= 0; va += NBPG) {
			pte = getpte4(va);
			if ((pte & PG_V) == 0)
				panic("kvm_uncache !pg_v");

			pv = pvhead(pte & PG_PFNUM);
			/* XXX - we probably don't need to check for OBMEM */
			if ((pte & PG_TYPE) == PG_OBMEM && pv) {
				if (cached)
					pv_changepte4_4c(pv, 0, PG_NC);
				else
					pv_changepte4_4c(pv, PG_NC, 0);
			}
			if (cached)
				pte &= ~PG_NC;
			else
				pte |= PG_NC;
			setpte4(va, pte);

			if ((pte & PG_TYPE) == PG_OBMEM)
				cache_flush_page((int)va);
		}
		setcontext4(ctx);
#endif
	}
}

/*
 * Find first virtual address >= *va that is
 * least likely to cause cache aliases.
 * (This will just seg-align mappings.)
 */
vaddr_t
pmap_prefer(vaddr_t foff, vaddr_t va)
{
	vaddr_t d, m;

#if defined(SUN4) || defined(SUN4C) || defined(SUN4E)
	if (VA_INHOLE(va))
		va = MMU_HOLE_END;
#endif

	m = CACHE_ALIAS_DIST;
	if (m != 0) {		/* m=0 => no cache aliasing */
		d = foff - va;
		d &= (m - 1);
		va += d;
	}

	return va;
}

void
pmap_remove_holes(struct vmspace *vm)
{
#if defined(SUN4) || defined(SUN4C) || defined(SUN4E)
	if (mmu_has_hole) {
		struct vm_map *map = &vm->vm_map;
		vaddr_t shole, ehole;

		shole = max(vm_map_min(map), (vaddr_t)MMU_HOLE_START);
		ehole = min(vm_map_max(map), (vaddr_t)MMU_HOLE_END);

		if (ehole <= shole)
			return;

		(void)uvm_map(map, &shole, ehole - shole, NULL,
		    UVM_UNKNOWN_OFFSET, 0,
		    UVM_MAPFLAG(PROT_NONE, PROT_NONE, MAP_INHERIT_SHARE,
		      MADV_RANDOM,
		      UVM_FLAG_NOMERGE | UVM_FLAG_HOLE | UVM_FLAG_FIXED));
	}
#endif
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
#if defined(SUN4) || defined(SUN4C) || defined(SUN4E)
	if (CPU_ISSUN4OR4COR4E) {
		setpte4(KERNBASE, 0);
		return;
	}
#endif
}

/*
 * Activate the address space for the specified process.  If the
 * process is the current process, load the new MMU context.
 */
void
pmap_activate(p)
	struct proc *p;
{
	pmap_t pmap = p->p_vmspace->vm_map.pmap;
	int s;

	/*
	 * This is essentially the same thing that happens in cpu_switch()
	 * when the newly selected process is about to run, except that we
	 * have to make sure to clean the windows before we set
	 * the new context.
	 */

	s = splvm();
	if (p == curproc) {
		write_user_windows();
		if (pmap->pm_ctx == NULL) {
			ctx_alloc(pmap);	/* performs setcontext() */
		} else {
			/* Do any cache flush needed on context switch */
			(*cpuinfo.pure_vcache_flush)();
			setcontext(pmap->pm_ctxnum);
		}
	}
	splx(s);
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
		panic("%s: CHK(pmap %p): no SRMMU region table or bad pa: "
		      "tblva=%p, tblpa=0x%x",
			s, pm, pm->pm_reg_ptps, pm->pm_reg_ptps_pa);

	if (CPU_ISSUN4M && pm->pm_ctx != NULL &&
	    (cpuinfo.ctx_tbl[pm->pm_ctxnum] != ((VA2PA((caddr_t)pm->pm_reg_ptps)
					      >> SRMMU_PPNPASHIFT) |
					     SRMMU_TEPTD)))
	    panic("%s: CHK(pmap %p): SRMMU region table at 0x%x not installed "
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
		if ((unsigned int)rp < vm_min_kernel_address)
			panic("%s: rp=%p", s, rp);
		n = 0;
		for (vs = 0; vs < NSEGRG; vs++) {
			sp = &rp->rg_segmap[vs];
			if ((unsigned int)sp < vm_min_kernel_address)
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
	    panic("%s: CHK(pmap %p): no SRMMU region table or bad pa: tblva=%p, tblpa=0x%x",
		  s, pm, pm->pm_reg_ptps, pm->pm_reg_ptps_pa);

	if (CPU_ISSUN4M &&
	    (cpuinfo.ctx_tbl[0] != ((VA2PA((caddr_t)pm->pm_reg_ptps) >>
					     SRMMU_PPNPASHIFT) | SRMMU_TEPTD)))
	    panic("%s: CHK(pmap %p): SRMMU region table at 0x%x not installed "
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
		if (CPU_ISSUN4M) {
			n = NSEGRG;
		} else {
			for (n = 0, vs = 0; vs < NSEGRG; vs++) {
				if (rp->rg_segmap[vs].sg_npte)
					n++;
			}
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

	if (CPU_ISSUN4OR4COR4E)
		sz += (seginval + 1) * NPTESG * sizeof(int);

	return (atop(sz));
}

/*
 * Write the mmu contents to the dump device.
 * This gets appended to the end of a crash dump since
 * there is no in-core copy of kernel memory mappings on a 4/4c machine.
 */
int
pmap_dumpmmu(dump, blkno)
	daddr_t blkno;
	int (*dump)(dev_t, daddr_t, caddr_t, size_t);
{
	kcore_seg_t	*ksegp;
	cpu_kcore_hdr_t	*kcpup;
	phys_ram_seg_t	memseg;
	int	error = 0;
	int	i, memsegoffset, pmegoffset;
	int		buffer[dbtob(1) / sizeof(int)];
	int		*bp, *ep;
#if defined(SUN4) || defined(SUN4C) || defined(SUN4E)
	int	pmeg;
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
		sz -= 4;						\
	}								\
} while (0)

	setcontext(0);

	/* Setup bookkeeping pointers */
	bp = buffer;
	ep = &buffer[sizeof(buffer) / sizeof(buffer[0])];

	/* Fill in MI segment header */
	ksegp = (kcore_seg_t *)bp;
	CORE_SETMAGIC(*ksegp, KCORE_MAGIC, MID_MACHINE, CORE_CPU);
	ksegp->c_size = ptoa(pmap_dumpsize()) - ALIGN(sizeof(kcore_seg_t));

	/* Fill in MD segment header (interpreted by MD part of libkvm) */
	kcpup = (cpu_kcore_hdr_t *)((int)bp + ALIGN(sizeof(kcore_seg_t)));
	kcpup->cputype = cputyp;
	kcpup->nmemseg = npmemarr;
	kcpup->memsegoffset = memsegoffset = ALIGN(sizeof(cpu_kcore_hdr_t));
	kcpup->npmeg = (CPU_ISSUN4OR4COR4E) ? seginval + 1 : 0; 
	kcpup->pmegoffset = pmegoffset =
		memsegoffset + npmemarr * sizeof(phys_ram_seg_t);

	/* Note: we have assumed everything fits in buffer[] so far... */
	bp = (int *)&kcpup->segmap_store;
	EXPEDITE(&kernel_segmap_store, sizeof(kernel_segmap_store));

	/* Align storage for upcoming quad-aligned segment array */
	while (bp != (int *)ALIGN(bp)) {
		int dummy = 0;
		EXPEDITE(&dummy, 4);
	}
	for (i = 0; i < npmemarr; i++) {
		memseg.start = pmemarr[i].addr_lo;
		memseg.size = pmemarr[i].len;
		EXPEDITE(&memseg, sizeof(phys_ram_seg_t));
	}

	if (CPU_ISSUN4M)
		goto out;

#if defined(SUN4) || defined(SUN4C) || defined(SUN4E)
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
		int va = 0;

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

/*
 * Helper function for debuggers.
 */
void
pmap_writetext(dst, ch)
	unsigned char *dst;
	int ch;
{
	int s, pte0, pte, ctx;
	vaddr_t va;

	s = splvm();
	va = (unsigned long)dst & (~PGOFSET);
	cpuinfo.cache_flush(dst, 1);

	ctx = getcontext();
	setcontext(0);

#if defined(SUN4M)
	if (CPU_ISSUN4M) {
		int *ptep;

		ptep = getptep4m(pmap_kernel(), va);
		pte0 = *ptep;
		if ((pte0 & SRMMU_TETYPE) != SRMMU_TEPTE) {
			splx(s);
			return;
		}
		pte = pte0 | PPROT_WRITE;
		tlb_flush_page((vaddr_t)va);
		setpgt4m(ptep, pte);
		*dst = (unsigned char)ch;
		tlb_flush_page((vaddr_t)va);
		setpgt4m(ptep, pte0);
	}
#endif
#if defined(SUN4) || defined(SUN4C) || defined(SUN4E)
	if (CPU_ISSUN4OR4COR4E) {
		pte0 = getpte4(va);
		if ((pte0 & PG_V) == 0) {
			splx(s);
			return;
		}
		pte = pte0 | PG_W;
		setpte4(va, pte);
		*dst = (unsigned char)ch;
		setpte4(va, pte0);
	}
#endif
	cpuinfo.cache_flush(dst, 1);
	setcontext(ctx);
	splx(s);
}

#ifdef EXTREME_DEBUG

static void test_region(int, int, int);

void
debug_pagetables()
{
	int i;
	int *regtbl;
	int te;

	printf("\nncontext=%d. ",ncontext);
	printf("Context table is at va 0x%x. Level 0 PTP: 0x%x\n",
	       cpuinfo.ctx_tbl, cpuinfo.ctx_tbl[0]);
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
#if 0	/* XXX avail_start */
	printf("Testing kernel region 0x%x: ", VA_VREG(vm_min_kernel_address));
	test_region(VA_VREG(vm_min_kernel_address), 4096, avail_start);
#endif
	cnpollc(1);
	cngetc();
	cnpollc(0);

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
	cnpollc(1);
	if (cngetc()=='q')
	    callrom();
	cnpollc(0);
}

static u_int
VA2PAsw(ctx, addr, pte)
	int ctx;
	caddr_t addr;
	int *pte;
{
	int *curtbl;
	int curpte;

#ifdef EXTREME_EXTREME_DEBUG
	printf("Looking up addr 0x%x in context 0x%x\n",addr,ctx);
#endif
	/* L0 */
	*pte = curpte = cpuinfo.ctx_tbl[ctx];
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
	curtbl = ((curpte & ~0x3) << 4) | vm_min_kernel_address; /* correct for krn*/
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
	curtbl = ((curpte & ~0x3) << 4) | vm_min_kernel_address; /* correct for krn*/
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
	curtbl = ((curpte & ~0x3) << 4) | vm_min_kernel_address; /* correct for krn*/
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

void
test_region(reg, start, stop)
	int reg;
	int start, stop;
{
	int i;
	int addr;
	int pte;
	int ptesw;
/*	int cnt=0;
*/

	cnpollc(1);
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
			if (reg == VA_VREG(vm_min_kernel_address))
				/* kernel permissions are different */
				continue;
			if ((pte&SRMMU_PROT_MASK)!=(ptesw&SRMMU_PROT_MASK)) {
				printf("Mismatched protections at address "
				       "0x%x; pte=0x%x, ptesw=0x%x\n",
				       addr,pte,ptesw);
				if (cngetc()=='q') break;
			}
		}
	}
	cnpollc(0);
	printf("done.\n");
}


void print_fe_map(void)
{
	u_int i, pte;

	printf("map of region 0xfe:\n");
	for (i = 0xfe000000; i < 0xff000000; i+=4096) {
		if (((pte = getpte4m(i)) & SRMMU_TETYPE) != SRMMU_TEPTE)
		    continue;
		printf("0x%x -> 0x%x%x (pte 0x%x)\n", i, pte >> 28,
		       (pte & ~0xff) << 4, pte);
	}
	printf("done\n");
}

#endif
