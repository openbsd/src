/*	$NetBSD: pmap.c,v 1.47 1995/07/05 18:52:32 pk Exp $ */

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
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
 *	@(#)pmap.c	8.4 (Berkeley) 2/5/94
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

#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_prot.h>
#include <vm/vm_page.h>

#include <machine/autoconf.h>
#include <machine/bsd_openprom.h>
#include <machine/oldmon.h>
#include <machine/cpu.h>
#include <machine/ctlreg.h>

#include <sparc/sparc/asm.h>
#include <sparc/sparc/cache.h>
#include <sparc/sparc/vaddrs.h>

#ifdef DEBUG
#define PTE_BITS "\20\40V\37W\36S\35NC\33IO\32U\31M"
#endif

extern struct promvec *promvec;

/*
 * The SPARCstation offers us the following challenges:
 *
 *   1. A virtual address cache.  This is, strictly speaking, not
 *	part of the architecture, but the code below assumes one.
 *	This is a write-through cache on the 4c and a write-back cache
 *	on others.
 *
 *   2. An MMU that acts like a cache.  There is not enough space
 *	in the MMU to map everything all the time.  Instead, we need
 *	to load MMU with the `working set' of translations for each
 *	process.
 *
 *   3.	Segmented virtual and physical spaces.  The upper 12 bits of
 *	a virtual address (the virtual segment) index a segment table,
 *	giving a physical segment.  The physical segment selects a
 *	`Page Map Entry Group' (PMEG) and the virtual page number---the
 *	next 5 or 6 bits of the virtual address---select the particular
 *	`Page Map Entry' for the page.  We call the latter a PTE and
 *	call each Page Map Entry Group a pmeg (for want of a better name).
 *
 *	Since there are no valid bits in the segment table, the only way
 *	to have an invalid segment is to make one full pmeg of invalid PTEs.
 *	We use the last one (since the ROM does as well).
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
int	pmapdebug = 0x0;
#endif

#define	splpmap() splimp()

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
	int	pv_va;			/* virtual address */
	int	pv_flags;		/* flags (below) */
};

/*
 * Flags in pv_flags.  Note that PV_MOD must be 1 and PV_REF must be 2
 * since they must line up with the bits in the hardware PTEs (see pte.h).
 */
#define PV_MOD	1		/* page modified */
#define PV_REF	2		/* page referenced */
#define PV_NC	4		/* page cannot be cached */
/*efine	PV_ALLF	7		** all of the above */

struct pvlist *pv_table;	/* array of entries, one per physical page */

#define pvhead(pa)	(&pv_table[atop((pa) - vm_first_phys)])

/*
 * Each virtual segment within each pmap is either valid or invalid.
 * It is valid if pm_npte[VA_VSEG(va)] is not 0.  This does not mean
 * it is in the MMU, however; that is true iff pm_segmap[VA_VSEG(va)]
 * does not point to the invalid PMEG.
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
 * A context is simply a small number that dictates which set of 4096
 * segment map entries the MMU uses.  The Sun 4c has eight such sets.
 * These are alloted in an `almost MRU' fashion.
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

caddr_t	vpage[2];		/* two reserved MD virtual pages */
caddr_t	vmmap;			/* one reserved MI vpage for /dev/mem */
caddr_t vdumppages;		/* 32KB worth of reserved dump pages */

#ifdef MMU_3L
smeg_t		tregion;
#endif
struct pmap	kernel_pmap_store;		/* the kernel's pmap */
struct regmap	kernel_regmap_store[NKREG];	/* the kernel's regmap */
struct segmap	kernel_segmap_store[NKREG*NSEGRG];/* the kernel's segmaps */

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

int mmu_has_hole;

vm_offset_t prom_vstart;	/* For /dev/kmem */
vm_offset_t prom_vend;

#ifdef SUN4
/*
 * segfixmask: on some systems (4/110) "getsegmap()" returns a partly
 * invalid value.   getsegmap returns a 16 bit value on the sun4, but
 * only the first 8 or so bits are valid (the rest are *supposed* to
 * be zero.   on the 4/110 the bits that are supposed to be zero are
 * all one instead.   e.g. KERNBASE is usually mapped by pmeg number zero.
 * on a 4/300 getsegmap(KERNBASE) == 0x0000, but
 * on a 4/100 getsegmap(KERNBASE) == 0xff00
 *
 * this confuses mmu_reservemon() and causes it to not reserve the PROM's
 * pmegs.   then the PROM's pmegs get used during autoconfig and everything
 * falls apart!  (not very fun to debug, BTW.)
 *
 * solution: mask the invalid bits in the getsetmap macro.
 */

static u_long segfixmask = 0xffffffff; /* all bits valid to start */
#endif

/*
 * pseudo-functions for mnemonic value
 * NB: setsegmap should be stba for 4c, but stha works and makes the
 * code right for the Sun-4 as well.
 */
#define	getcontext()		lduba(AC_CONTEXT, ASI_CONTROL)
#define	setcontext(c)		stba(AC_CONTEXT, ASI_CONTROL, c)
#if defined(SUN4) && !defined(SUN4C)
#define	getsegmap(va)		(lduha(va, ASI_SEGMAP) & segfixmask)
#define	setsegmap(va, pmeg)	stha(va, ASI_SEGMAP, pmeg)
#endif
#if !defined(SUN4) && defined(SUN4C)
#define	getsegmap(va)		lduba(va, ASI_SEGMAP)
#define	setsegmap(va, pmeg)	stba(va, ASI_SEGMAP, pmeg)
#endif
#if defined(SUN4) && defined(SUN4C)
#define	getsegmap(va)		(cputyp==CPU_SUN4C ? lduba(va, ASI_SEGMAP) \
				    : (lduha(va, ASI_SEGMAP) & segfixmask))
#define	setsegmap(va, pmeg)	(cputyp==CPU_SUN4C ? stba(va, ASI_SEGMAP, pmeg) \
				    : stha(va, ASI_SEGMAP, pmeg))
#endif
#if defined(SUN4) && defined(MMU_3L)
#define	getregmap(va)		((unsigned)lduha(va+2, ASI_REGMAP) >> 8)
#define	setregmap(va, smeg)	stha(va+2, ASI_REGMAP, (smeg << 8))
#endif

#define	getpte(va)		lda(va, ASI_PTE)
#define	setpte(va, pte)		sta(va, ASI_PTE, pte)

/*----------------------------------------------------------------*/

#define	HWTOSW(pg) (pg)
#define	SWTOHW(pg) (pg)

#ifdef MMU_3L
#define CTX_USABLE(pm,rp)	((pm)->pm_ctx && \
				 (!mmu_3l || (rp)->rg_smeg != reginval))
#else
#define CTX_USABLE(pm,rp)	((pm)->pm_ctx)
#endif

#define GAP_WIDEN(pm,vr) do {		\
	if (vr + 1 == pm->pm_gap_start)	\
		pm->pm_gap_start = vr;	\
	if (vr == pm->pm_gap_end)	\
		pm->pm_gap_end = vr + 1;\
} while (0)

#define GAP_SHRINK(pm,vr) do {						\
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
u_long
pmap_page_index(pa)
	vm_offset_t pa;
{
	int idx;
	int nmem;
	register struct memarr *mp;

#ifdef  DIAGNOSTIC
	if (pa < avail_start || pa >= avail_end)
		panic("pmap_page_index: pa=0x%x", pa);
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
#define	MR(pte) (((pte) >> PG_M_SHIFT) & (PV_MOD | PV_REF))

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
void
mmu_reservemon(nrp, nsp)
	register int *nrp, *nsp;
{
	register u_int va, eva;
	register int mmureg, mmuseg, i, nr, ns, vr, lastvr;
	register struct regmap *rp;

#if defined(SUN4)
	if (cputyp == CPU_SUN4) {
		prom_vstart = va = OLDMON_STARTVADDR;
		prom_vend = eva = OLDMON_ENDVADDR;
	}
#endif
#if defined(SUN4C)
	if (cputyp == CPU_SUN4C) {
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
			setpte(va, getpte(va) | PG_S);
	}
	*nsp = ns;
	*nrp = nr;
	return;
}

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
		printf("me_alloc: stealing pmeg %x from pmap %x\n",
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
		tpte = getpte(va);
		if ((tpte & (PG_V | PG_TYPE)) == (PG_V | PG_OBMEM)) {
			pa = ptoa(HWTOSW(tpte & PG_PFNUM));
			if (managed(pa))
				pvhead(pa)->pv_flags |= MR(tpte);
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
		printf("me_free: freeing pmeg %d from pmap %x\n",
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
		tpte = getpte(va);
		if ((tpte & (PG_V | PG_TYPE)) == (PG_V | PG_OBMEM)) {
			pa = ptoa(HWTOSW(tpte & PG_PFNUM));
			if (managed(pa))
				pvhead(pa)->pv_flags |= MR(tpte);
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
	register int i, va, pa;
	int ctx;
	struct regmap *rp;
	struct segmap *sp;

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
		printf("region_alloc: stealing smeg %x from pmap %x\n",
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
		printf("region_free: freeing smeg %x from pmap %x\n",
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
	register int va;
	vm_prot_t prot;
{
	register int *pte;
	register struct mmuentry *me;
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
		return (bits && (getpte(va) & bits) == bits ? -1 : 0);

	/* reload segment: write PTEs into a new LRU entry */
	va = VA_ROUNDDOWNTOSEG(va);
	s = splpmap();		/* paranoid */
	pmeg = me_alloc(&segm_lru, pm, vr, vs)->me_cookie;
	setsegmap(va, pmeg);
	i = NPTESG;
	do {
		setpte(va, *pte++);
		va += NBPG;
	} while (--i > 0);
	splx(s);
	return (1);
}

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
	register int cnum, i;
	register struct regmap *rp;
	register int gap_start, gap_end;
	register unsigned long va;

#ifdef DEBUG
	if (pm->pm_ctx)
		panic("ctx_alloc pm_ctx");
	if (pmapdebug & PDB_CTX_ALLOC)
		printf("ctx_alloc(%x)\n", pm);
#endif
	gap_start = pm->pm_gap_start;
	gap_end = pm->pm_gap_end;

	if ((c = ctx_freelist) != NULL) {
		ctx_freelist = c->c_nextfree;
		cnum = c - ctxinfo;
		setcontext(cnum);
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
			printf("ctx_alloc: steal context %x from %x\n",
			    cnum, c->c_pmap);
#endif
		c->c_pmap->pm_ctx = NULL;
		setcontext(cnum);
		if (vactype != VAC_NONE)
			cache_flush_context();
		if (gap_start < c->c_pmap->pm_gap_start)
			gap_start = c->c_pmap->pm_gap_start;
		if (gap_end > c->c_pmap->pm_gap_end)
			gap_end = c->c_pmap->pm_gap_end;
	}
	c->c_pmap = pm;
	pm->pm_ctx = c;
	pm->pm_ctxnum = cnum;

	/*
	 * Write pmap's region (3-level MMU) or segment table into the MMU.
	 *
	 * Only write those entries that actually map something in this
	 * context by maintaining a pair of region numbers in between
	 * which the pmap has no valid mappings.
	 *
	 * If a context was just allocated from the free list, trust that
	 * all its pmeg numbers are `seginval'. We make sure this is the
	 * case initially in pmap_bootstrap(). Otherwise, the context was
	 * freed by calling ctx_free() in pmap_release(), which in turn is
	 * supposedly called only when all mappings have been removed.
	 *
	 * On the other hand, if the context had to be stolen from another
	 * pmap, we possibly shrink the gap to be the disjuction of the new
	 * and the previous map.
	 */


	rp = pm->pm_regmap;
	for (va = 0, i = NUREG; --i >= 0; ) {
		if (VA_VREG(va) >= gap_start) {
			va = VRTOVA(gap_end);
			i -= gap_end - gap_start;
			rp += gap_end - gap_start;
			if (i < 0)
				break;
			gap_start = NUREG; /* mustn't re-enter this branch */
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
				setsegmap(va, sp?sp++->sg_pmeg:seginval);
			rp++;
		}
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
		setcontext(0);
	} else {
		CHANGE_CONTEXTS(oldc, 0);
	}
	c->c_nextfree = ctx_freelist;
	ctx_freelist = c;
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
void
pv_changepte(pv0, bis, bic)
	register struct pvlist *pv0;
	register int bis, bic;
{
	register int *pte;
	register struct pvlist *pv;
	register struct pmap *pm;
	register int va, vr, vs, i, flags;
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
				tpte = getpte(va);
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
				tpte = getpte(va);
			}
			if (tpte & PG_V)
				flags |= (tpte >> PG_M_SHIFT) &
				    (PV_MOD|PV_REF);
			tpte = (tpte | bis) & ~bic;
			setpte(va, tpte);
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
pv_syncflags(pv0)
	register struct pvlist *pv0;
{
	register struct pvlist *pv;
	register struct pmap *pm;
	register int tpte, va, vr, vs, pmeg, i, flags;
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
			tpte = getpte(va);
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
			tpte = getpte(va);
		}
		if (tpte & (PG_M|PG_U) && tpte & PG_V) {
			flags |= (tpte >> PG_M_SHIFT) &
			    (PV_MOD|PV_REF);
			tpte &= ~(PG_M|PG_U);
			setpte(va, tpte);
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
pv_unlink(pv, pm, va)
	register struct pvlist *pv;
	register struct pmap *pm;
	register vm_offset_t va;
{
	register struct pvlist *npv;

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
			pv->pv_next = npv->pv_next;
			pv->pv_pmap = npv->pv_pmap;
			pv->pv_va = npv->pv_va;
			free((caddr_t)npv, M_VMPVENT);
		} else
			pv->pv_pmap = NULL;
	} else {
		register struct pvlist *prev;

		for (prev = pv;; prev = npv, npv = npv->pv_next) {
			pmap_stats.ps_unlink_pvsearch++;
			if (npv == NULL)
				panic("pv_unlink");
			if (npv->pv_pmap == pm && npv->pv_va == va)
				break;
		}
		prev->pv_next = npv->pv_next;
		free((caddr_t)npv, M_VMPVENT);
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
		pv_changepte(pv, 0, PG_NC);
	}
}

/*
 * pv_link is the inverse of pv_unlink, and is used in pmap_enter.
 * It returns PG_NC if the (new) pvlist says that the address cannot
 * be cached.
 */
/*static*/ int
pv_link(pv, pm, va)
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
				"pv_link: badalias: pid %d, %x<=>%x, pa %x\n",
				curproc?curproc->p_pid:-1, va, npv->pv_va,
				vm_first_phys + (pv-pv_table)*NBPG);
#endif
				pv->pv_flags |= PV_NC;
				pv_changepte(pv, ret = PG_NC, 0);
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

/*
 * Walk the given list and flush the cache for each (MI) page that is
 * potentially in the cache. Called only if vactype != VAC_NONE.
 */
pv_flushcache(pv)
	register struct pvlist *pv;
{
	register struct pmap *pm;
	register int i, s, ctx;

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
	register union ctxinfo *ci;
	register struct mmuentry *mmuseg, *mmureg;
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
	extern caddr_t reserve_dumppages(caddr_t);

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

#if defined(SUN4) && defined(SUN4C)
	/* In this case NPTESG is not a #define */
	nptesg = (NBPSG >> pgshift);
#endif

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

	ncontext = nctx;

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

	mmu_reservemon(&nregion, &nsegment);

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
			setpte(p, 0);

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
			setpte(p, getpte(p) & mask);
	}
}

void
pmap_init()
{
	register vm_size_t s;
	int pass1, nmem;
	register struct memarr *mp;
	vm_offset_t sva, va, eva;
	vm_offset_t pa;

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
			panic("pmap_init: unmanaged address: 0x%x", addr);

		va = (vm_offset_t)&pv_table[atop(addr - avail_start)];
		sva = trunc_page(va);
		if (sva < eva) {
#ifdef DEBUG
			printf("note: crowded chunk at 0x%x\n", mp->addr);
#endif
			sva += PAGE_SIZE;
			if (sva < eva)
				panic("pmap_init: sva(%x) < eva(%x)", sva, eva);
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
		printf("pmap_create: created %x\n", pm);
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
	register int i, size;
	void *urp;

#ifdef DEBUG
	if (pmapdebug & PDB_CREATE)
		printf("pmap_pinit(%x)\n", pm);
#endif

	size = NUREG * sizeof(struct regmap);
	pm->pm_regstore = urp = malloc(size, M_VMPMAP, M_WAITOK);
	bzero((caddr_t)urp, size);
	/* pm->pm_ctx = NULL; */
	simple_lock_init(&pm->pm_lock);
	pm->pm_refcount = 1;
#ifdef MMU_3L
	TAILQ_INIT(&pm->pm_reglist);
#endif
	TAILQ_INIT(&pm->pm_seglist);
	pm->pm_regmap = urp;
#ifdef MMU_3L
	if (mmu_3l)
		for (i = NUREG; --i >= 0;)
			pm->pm_regmap[i].rg_smeg = reginval;
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
		printf("pmap_destroy(%x)\n", pm);
#endif
	simple_lock(&pm->pm_lock);
	count = --pm->pm_refcount;
	simple_unlock(&pm->pm_lock);
	if (count == 0) {
		pmap_release(pm);
		free((caddr_t)pm, M_VMPMAP);
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
		printf("pmap_release(%x)\n", pm);
#endif
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
	splx(s);
#ifdef DEBUG
{
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
		free((caddr_t)pm->pm_regstore, M_VMPMAP);
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

static void pmap_rmk __P((struct pmap *, vm_offset_t, vm_offset_t,
			  int, int));
static void pmap_rmu __P((struct pmap *, vm_offset_t, vm_offset_t,
			  int, int));

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
		printf("pmap_remove(%x, %x, %x)\n", pm, va, endva);
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

/* remove from kernel */
static void
pmap_rmk(pm, va, endva, vr, vs)
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
		tpte = getpte(va);
		if ((tpte & PG_V) == 0) {
			va += PAGE_SIZE;
			continue;
		}
		if ((tpte & PG_TYPE) == PG_OBMEM) {
			/* if cacheable, flush page as needed */
			if (perpage && (tpte & PG_NC) == 0)
				cache_flush_page(va);
			i = ptoa(HWTOSW(tpte & PG_PFNUM));
			if (managed(i)) {
				pv = pvhead(i);
				pv->pv_flags |= MR(tpte);
				pv_unlink(pv, pm, va);
			}
		}
		nleft--;
		setpte(va, 0);
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

/*
 * Just like pmap_rmk_magic, but we have a different threshold.
 * Note that this may well deserve further tuning work.
 */
#if 0
#define	PMAP_RMU_MAGIC	(cacheinfo.c_hwflush?4:64)	/* if > magic, use cache_flush_segment */
#else
#define	PMAP_RMU_MAGIC	4	/* if > magic, use cache_flush_segment */
#endif

/* remove from user */
static void
pmap_rmu(pm, va, endva, vr, vs)
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
		for (; va < endva; pte++, va += PAGE_SIZE) {
			tpte = *pte;
			if ((tpte & PG_V) == 0) {
				/* nothing to remove (braindead VM layer) */
				continue;
			}
			if ((tpte & PG_TYPE) == PG_OBMEM) {
				i = ptoa(HWTOSW(tpte & PG_PFNUM));
				if (managed(i))
					pv_unlink(pvhead(i), pm, va);
			}
			nleft--;
			*pte = 0;
		}
		if ((sp->sg_npte = nleft) == 0) {
			free((caddr_t)pte0, M_VMPMAP);
			sp->sg_pte = NULL;
			if (--rp->rg_nsegmap == 0) {
				free((caddr_t)rp->rg_segmap, M_VMPMAP);
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
	for (; va < endva; pteva += PAGE_SIZE, va += PAGE_SIZE) {
		tpte = getpte(pteva);
		if ((tpte & PG_V) == 0)
			continue;
		if ((tpte & PG_TYPE) == PG_OBMEM) {
			/* if cacheable, flush page as needed */
			if (perpage && (tpte & PG_NC) == 0)
				cache_flush_page(va);
			i = ptoa(HWTOSW(tpte & PG_PFNUM));
			if (managed(i)) {
				pv = pvhead(i);
				pv->pv_flags |= MR(tpte);
				pv_unlink(pv, pm, va);
			}
		}
		nleft--;
		setpte(pteva, 0);
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
		free((caddr_t)pte0, M_VMPMAP);
		sp->sg_pte = NULL;
		me_free(pm, pmeg);

		if (--rp->rg_nsegmap == 0) {
			free((caddr_t)rp->rg_segmap, M_VMPMAP);
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

/*
 * Lower (make more strict) the protection on the specified
 * physical page.
 *
 * There are only two cases: either the protection is going to 0
 * (in which case we do the dirty work here), or it is going from
 * to read-only (in which case pv_changepte does the trick).
 */
void
pmap_page_protect(pa, prot)
	vm_offset_t pa;
	vm_prot_t prot;
{
	register struct pvlist *pv, *pv0, *npv;
	register struct pmap *pm;
	register int va, vr, vs, pteva, tpte;
	register int flags, nleft, i, s, ctx, doflush;
	struct regmap *rp;
	struct segmap *sp;

#ifdef DEBUG
	if (!pmap_pa_exists(pa))
		panic("pmap_page_protect: no such address: %x", pa);
	if ((pmapdebug & PDB_CHANGEPROT) ||
	    (pmapdebug & PDB_REMOVE && prot == VM_PROT_NONE))
		printf("pmap_page_protect(%x, %x)\n", pa, prot);
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
		pv_changepte(pvhead(pa), 0, PG_W);
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
				free((caddr_t)sp->sg_pte, M_VMPMAP);
				sp->sg_pte = NULL;
				if (--rp->rg_nsegmap == 0) {
					free((caddr_t)rp->rg_segmap, M_VMPMAP);
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

		tpte = getpte(pteva);
		if ((tpte & PG_V) == 0)
			panic("pmap_page_protect !PG_V");
		flags |= MR(tpte);

		if (nleft) {
			setpte(pteva, 0);
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
				free((caddr_t)sp->sg_pte, M_VMPMAP);
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
					free((caddr_t)rp->rg_segmap, M_VMPMAP);
					rp->rg_segmap = NULL;
					GAP_WIDEN(pm,vr);
				}
			}
		}
	nextpv:
		npv = pv->pv_next;
		if (pv != pv0)
			free((caddr_t)pv, M_VMPVENT);
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
pmap_protect(pm, sva, eva, prot)
	register struct pmap *pm;
	vm_offset_t sva, eva;
	vm_prot_t prot;
{
	register int va, nva, vr, vs, pteva;
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
					tpte = getpte(va);
					pmap_stats.ps_npg_prot_all++;
					if ((tpte & (PG_W|PG_TYPE)) ==
					    (PG_W|PG_OBMEM)) {
						pmap_stats.ps_npg_prot_actual++;
						if (vactype != VAC_NONE)
							cache_flush_page(va);
						setpte(va, tpte & ~PG_W);
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
					setpte(pteva, getpte(pteva) & ~PG_W);
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
pmap_changeprot(pm, va, prot, wired)
	register struct pmap *pm;
	register vm_offset_t va;
	vm_prot_t prot;
	int wired;
{
	register int vr, vs, tpte, newprot, ctx, i, s;
	struct regmap *rp;
	struct segmap *sp;

#ifdef DEBUG
	if (pmapdebug & PDB_CHANGEPROT)
		printf("pmap_changeprot(%x, %x, %x, %x)\n",
		    pm, va, prot, wired);
#endif

	write_user_windows();	/* paranoia */

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
			tpte = getpte(va);
			if ((tpte & PG_PROT) == newprot) {
				setcontext(ctx);
				goto useless;
			}
			if (vactype == VAC_WRITEBACK &&
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
			tpte = getpte(va);
			if ((tpte & PG_PROT) == newprot) {
				setcontext(ctx);
				goto useless;
			}
		}
		tpte = (tpte & ~PG_PROT) | newprot;
		setpte(va, tpte);
		setcontext(ctx);
	}
	splx(s);
	return;

useless:
	/* only wiring changed, and we ignore wiring */
	pmap_stats.ps_useless_changeprots++;
	splx(s);
}

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
pmap_enter(pm, va, pa, prot, wired)
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
		printf("pmap_enter: pm %x, va %x, pa %x: in MMU hole\n",
			pm, va, pa);
#endif
		return;
	}

#ifdef DEBUG
	if (pmapdebug & PDB_ENTER)
		printf("pmap_enter(%x, %x, %x, %x, %x)\n",
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
			panic("pmap_enter: no such address: %x", pa);
#endif
		pteproto |= SWTOHW(atop(pa));
		pv = pvhead(pa);
	} else {
		pteproto |= atop(pa) & PG_PFNUM;
		pv = NULL;
	}
	if (prot & VM_PROT_WRITE)
		pteproto |= PG_W;

	ctx = getcontext();
	if (pm == pmap_kernel())
		pmap_enk(pm, va, prot, wired, pv, pteproto | PG_S);
	else
		pmap_enu(pm, va, prot, wired, pv, pteproto);
	setcontext(ctx);
}

/* enter new (or change existing) kernel mapping */
pmap_enk(pm, va, prot, wired, pv, pteproto)
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
	if (sp->sg_pmeg != seginval && (tpte = getpte(va)) & PG_V) {
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
printf("pmap_enk: changing existing va=>pa entry: va %x, pteproto %x\n",
	va, pteproto);
#endif
			/*
			 * Switcheroo: changing pa for this va.
			 * If old pa was managed, remove from pvlist.
			 * If old page was cached, flush cache.
			 */
			addr = ptoa(HWTOSW(tpte & PG_PFNUM));
			if (managed(addr))
				pv_unlink(pvhead(addr), pm, va);
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
		pteproto |= pv_link(pv, pm, va);

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
			setpte(tva, 0);
			tva += NBPG;
		} while (--i > 0);
	}

	/* ptes kept in hardware only */
	setpte(va, pteproto);
	splx(s);
}

/* enter new (or change existing) user mapping */
pmap_enu(pm, va, prot, wired, pv, pteproto)
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
			free((caddr_t)sp, M_VMPMAP);
			goto rretry;
		}
		bzero((caddr_t)sp, size);
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
			free((caddr_t)pte, M_VMPMAP);
			goto sretry;
		}
#ifdef DEBUG
		if (sp->sg_pmeg != seginval)
			panic("pmap_enter: new ptes, but not seginval");
#endif
		bzero((caddr_t)pte, size);
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
				tpte = getpte(va);
				doflush = 1;
			} else {
				setcontext(0);
				/* XXX use per-cpu pteva? */
#ifdef MMU_3L
				if (mmu_3l)
					setregmap(0, tregion);
#endif
				setsegmap(0, pmeg);
				tpte = getpte(VA_VPG(va) << PGSHIFT);
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
				pmap_changeprot(pm, va, prot, wired);
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
				addr = ptoa(HWTOSW(tpte & PG_PFNUM));
				if (managed(addr))
					pv_unlink(pvhead(addr), pm, va);
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
		pteproto |= pv_link(pv, pm, va);

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
		setpte(va, pteproto);
	}
	/* update software copy */
	pte += VA_VPG(va);
	*pte = pteproto;

	splx(s);
}

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
vm_offset_t
pmap_extract(pm, va)
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
			setcontext(pm->pm_ctxnum);
			tpte = getpte(va);
		} else {
			setcontext(0);
#ifdef MMU_3L
			if (mmu_3l)
				setregmap(0, tregion);
#endif
			setsegmap(0, sp->sg_pmeg);
			tpte = getpte(VA_VPG(va) << PGSHIFT);
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
	tpte = HWTOSW(tpte);
	return ((tpte << PGSHIFT) | (va & PGOFSET));
}

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
}

/*
 * Require that all active physical maps contain no
 * incorrect entries NOW.  [This update includes
 * forcing updates of any address map caching.]
 */
void
pmap_update()
{
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

/*
 * Clear the modify bit for the given physical page.
 */
void
pmap_clear_modify(pa)
	register vm_offset_t pa;
{
	register struct pvlist *pv;

	if ((pa & (PMAP_TNC & ~PMAP_NC)) == 0 && managed(pa)) {
		pv = pvhead(pa);
		(void) pv_syncflags(pv);
		pv->pv_flags &= ~PV_MOD;
	}
}

/*
 * Tell whether the given physical page has been modified.
 */
int
pmap_is_modified(pa)
	register vm_offset_t pa;
{
	register struct pvlist *pv;

	if ((pa & (PMAP_TNC & ~PMAP_NC)) == 0 && managed(pa)) {
		pv = pvhead(pa);
		if (pv->pv_flags & PV_MOD || pv_syncflags(pv) & PV_MOD)
			return (1);
	}
	return (0);
}

/*
 * Clear the reference bit for the given physical page.
 */
void
pmap_clear_reference(pa)
	vm_offset_t pa;
{
	register struct pvlist *pv;

	if ((pa & (PMAP_TNC & ~PMAP_NC)) == 0 && managed(pa)) {
		pv = pvhead(pa);
		(void) pv_syncflags(pv);
		pv->pv_flags &= ~PV_REF;
	}
}

/*
 * Tell whether the given physical page has been referenced.
 */
int
pmap_is_referenced(pa)
	vm_offset_t pa;
{
	register struct pvlist *pv;

	if ((pa & (PMAP_TNC & ~PMAP_NC)) == 0 && managed(pa)) {
		pv = pvhead(pa);
		if (pv->pv_flags & PV_REF || pv_syncflags(pv) & PV_REF)
			return (1);
	}
	return (0);
}

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
void
pmap_zero_page(pa)
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
		pte = PG_V | PG_S | PG_W | PG_NC | SWTOHW(atop(pa));
	} else
		pte = PG_V | PG_S | PG_W | PG_NC | (atop(pa) & PG_PFNUM);

	va = vpage[0];
	setpte(va, pte);
	qzero(va, NBPG);
	setpte(va, 0);
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
pmap_copy_page(src, dst)
	vm_offset_t src, dst;
{
	register caddr_t sva, dva;
	register int spte, dpte;

	if (managed(src)) {
		if (vactype == VAC_WRITEBACK)
			pv_flushcache(pvhead(src));
		spte = PG_V | PG_S | SWTOHW(atop(src));
	} else
		spte = PG_V | PG_S | (atop(src) & PG_PFNUM);

	if (managed(dst)) {
		/* similar `might not be necessary' comment applies */
		if (vactype != VAC_NONE)
			pv_flushcache(pvhead(dst));
		dpte = PG_V | PG_S | PG_W | PG_NC | SWTOHW(atop(dst));
	} else
		dpte = PG_V | PG_S | PG_W | PG_NC | (atop(dst) & PG_PFNUM);

	sva = vpage[0];
	dva = vpage[1];
	setpte(sva, spte);
	setpte(dva, dpte);
	qcopy(sva, dva, NBPG);	/* loads cache, so we must ... */
	if (vactype != VAC_NONE)
		cache_flush_page((int)sva);
	setpte(sva, 0);
	setpte(dva, 0);
}

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
kvm_uncache(va, npages)
	register caddr_t va;
	register int npages;
{
	register int pte;

	for (; --npages >= 0; va += NBPG) {
		pte = getpte(va);
		if ((pte & PG_V) == 0)
			panic("kvm_uncache !pg_v");
		pte |= PG_NC;
		setpte(va, pte);
		if (vactype != VAC_NONE && (pte & PG_TYPE) == PG_OBMEM)
			cache_flush_page((int)va);
	}
}

/*
 * Turn on IO cache for a given (va, number of pages).
 *
 * We just assert PG_NC for each PTE; the addresses must reside
 * in locked kernel space.  A cache flush is also done.
 */
kvm_iocache(va, npages)
	register caddr_t va;
	register int npages;
{
	register int pte;

	for (; --npages >= 0; va += NBPG) {
		pte = getpte(va);
		if ((pte & PG_V) == 0)
			panic("kvm_iocache !pg_v");
		pte |= PG_IOC;
		setpte(va, pte);
	}
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
 * Find first virtual address >= va that doesn't cause
 * a cache alias on physical address pa.
 */
vm_offset_t
pmap_prefer(pa, va)
	register vm_offset_t pa;
	register vm_offset_t va;
{
	register struct pvlist	*pv;
	register long		m, d;

	if (cputyp == CPU_SUN4M)
		/* does the sun4m have the cache alias problem? */
		return va;

	m = CACHE_ALIAS_DIST;

	if ((pa & (PMAP_TNC & ~PMAP_NC)) || !managed(pa))
		return va;

	pv = pvhead(pa);
	if (pv->pv_pmap == NULL) {
#if 0
		return ((va + m - 1) & ~(m - 1));
#else
		/* Unusable, tell caller to try another one */
		return (vm_offset_t)-1;
#endif
	}

	d = (long)(pv->pv_va & (m - 1)) - (long)(va & (m - 1));
	if (d < 0)
		va += m;
	va += d;

	return va;
}

pmap_redzone()
{
	setpte(KERNBASE, 0);
}

#ifdef DEBUG
/*
 * Check consistency of a pmap (time consuming!).
 */
int
pm_check(s, pm)
	char *s;
	struct pmap *pm;
{
	if (pm == pmap_kernel())
		pm_check_k(s, pm);
	else
		pm_check_u(s, pm);
}

int
pm_check_u(s, pm)
	char *s;
	struct pmap *pm;
{
	struct regmap *rp;
	struct segmap *sp;
	int n, vs, vr, j, m, *pte;

	for (vr = 0; vr < NUREG; vr++) {
		rp = &pm->pm_regmap[vr];
		if (rp->rg_nsegmap == 0)
			continue;
		if (rp->rg_segmap == NULL)
			panic("%s: CHK(vr %d): nsegmap = %d; sp==NULL",
				s, vr, rp->rg_nsegmap);
		if ((unsigned int)rp < KERNBASE)
			panic("%s: rp=%x", s, rp);
		n = 0;
		for (vs = 0; vs < NSEGRG; vs++) {
			sp = &rp->rg_segmap[vs];
			if ((unsigned int)sp < KERNBASE)
				panic("%s: sp=%x", s, sp);
			if (sp->sg_npte != 0) {
				n++;
				if (sp->sg_pte == NULL)
					panic("%s: CHK(vr %d, vs %d): npte=%d, "
					   "pte=NULL", s, vr, vs, sp->sg_npte);

				pte=sp->sg_pte;
				m = 0;
				for (j=0; j<NPTESG; j++,pte++)
					if (*pte & PG_V)
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
	return 0;
}

int
pm_check_k(s, pm)
	char *s;
	struct pmap *pm;
{
	struct regmap *rp;
	struct segmap *sp;
	int vr, vs, n;

	for (vr = NUREG; vr < NUREG+NKREG; vr++) {
		rp = &pm->pm_regmap[vr];
		if (rp->rg_segmap == NULL)
			panic("%s: CHK(vr %d): nsegmap = %d; sp==NULL",
				s, vr, rp->rg_nsegmap);
		if (rp->rg_nsegmap == 0)
			continue;
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
	return 0;
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
	return btoc(((seginval + 1) * NPTESG * sizeof(int)) +
		    sizeof(seginval) +
		    sizeof(pmemarr) +
		    sizeof(kernel_segmap_store));
}

/*
 * Write the mmu contents to the dump device.
 * This gets appended to the end of a crash dump since
 * there is no in-core copy of kernel memory mappings.
 */
int
pmap_dumpmmu(dump, blkno)
	register daddr_t blkno;
	register int (*dump)	__P((dev_t, daddr_t, caddr_t, size_t));
{
	register int pmeg;
	register int addr;	/* unused kernel virtual address */
	register int i;
	register int *pte, *ptend;
	register int error;
	register int *kp;
	int buffer[dbtob(1) / sizeof(int)];

	/*
	 * dump page table entries
	 *
	 * We dump each pmeg in order (by segment number).  Since the MMU
	 * automatically maps the given virtual segment to a pmeg we must
	 * iterate over the segments by incrementing an unused segment slot
	 * in the MMU.  This fixed segment number is used in the virtual
	 * address argument to getpte().
	 */
	setcontext(0);

	/*
	 * Go through the pmegs and dump each one.
	 */
	pte = buffer;
	ptend = &buffer[sizeof(buffer) / sizeof(buffer[0])];
	for (pmeg = 0; pmeg <= seginval; ++pmeg) {
		register int va = 0;

		setsegmap(va, pmeg);
		i = NPTESG;
		do {
			*pte++ = getpte(va);
			if (pte >= ptend) {
				/*
				 * Note that we'll dump the last block
				 * the last time through the loops because
				 * all the PMEGs occupy 32KB which is 
				 * a multiple of the block size.
				 */
				error = (*dump)(dumpdev, blkno,
						(caddr_t)buffer,
						dbtob(1));
				if (error != 0)
					return (error);
				++blkno;
				pte = buffer;
			}
			va += NBPG;
		} while (--i > 0);
	}
	setsegmap(0, seginval);

	/*
	 * Next, dump # of pmegs, the physical memory table and the
	 * kernel's segment map.
	 */
	pte = buffer;
	*pte++ = seginval;
	*pte++ = npmemarr;
	bcopy((char *)pmemarr, (char *)pte, sizeof(pmemarr));
	pte = (int *)((int)pte + sizeof(pmemarr));
	kp = (int *)kernel_segmap_store;
	i = sizeof(kernel_segmap_store) / sizeof(int);
	do {
		*pte++ = *kp++;
		if (pte >= ptend) {
			error = (*dump)(dumpdev, blkno, (caddr_t)buffer,
					dbtob(1));
			if (error != 0)
				return (error);
			++blkno;
			pte = buffer;
		}
	} while (--i > 0);
	if (pte != buffer)
		error = (*dump)(dumpdev, blkno++, (caddr_t)buffer, dbtob(1));

	return (error);
}
