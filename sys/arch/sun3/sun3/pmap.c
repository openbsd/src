/*	$NetBSD: pmap.c,v 1.60 1996/02/28 22:51:05 gwr Exp $	*/

/*
 * Copyright (c) 1994, 1995 Gordon W. Ross
 * Copyright (c) 1993 Adam Glass
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
 *	This product includes software developed by:
 *	  Adam Glass, Gordon Ross
 * 4. The name of the Authors may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
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
 * Some notes:
 *
 * sun3s have contexts (8).  In our mapping of the world, the kernel is mapped
 * into all contexts.  Processes take up a known portion of the context,
 * and compete for the available contexts on a LRU basis.
 *
 * sun3s also have this evil "PMEG" crapola.  Essentially each "context"'s
 * address space is defined by the 2048 one-byte entries in the segment map.
 * Each of these 1-byte entries points to a "Page Map Entry Group" (PMEG)
 * which contains the mappings for that virtual segment.  (This strange
 * terminology invented by Sun and preserved here for consistency.)
 * Each PMEG maps a segment of 128Kb length, with 16 pages of 8Kb each.
 *
 * As you might guess, these PMEGs are in short supply and heavy demand.
 * PMEGs allocated to the kernel are "static" in the sense that they can't
 * be stolen from it.  PMEGs allocated to a particular segment of a
 * pmap's virtual space will be fought over by the other pmaps.
 */

/*
 * wanted attributes:
 *       pmegs that aren't needed by a pmap remain in the MMU.
 *       quick context switches between pmaps
 *       kernel is in all contexts
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/user.h>
#include <sys/queue.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>

#include <machine/pte.h>
#include <machine/control.h>

#include <machine/cpu.h>
#include <machine/mon.h>
#include <machine/vmparam.h>
#include <machine/dvma.h>
#include <machine/pmap.h>

#include "cache.h"

#if	(PMAP_OBIO << PG_MOD_SHIFT) != PGT_OBIO
#error	"PMAP_XXX definitions don't match pte.h!"
#endif

/*
 * Globals shared between here and sun3_startup:
 * For simplicity, this interface retains the variables
 * that were used in the old interface (without NONCONTIG).
 * These are set in pmap_bootstrap() and used
 * in pmap_next_page().
 */
/* Kernel virtual address space available: */
extern vm_offset_t virtual_avail, virtual_end;
/* Physical address space available: */
extern vm_offset_t avail_start, avail_end;
/* The "hole" (used to skip the Sun3/50 video RAM) */
extern vm_offset_t hole_start, hole_size;

/* statistics... */
struct pmap_stats {
	int	ps_enter_firstpv;	/* pv heads entered */
	int	ps_enter_secondpv;	/* pv nonheads entered */
	int	ps_unlink_pvfirst;	/* of pv_unlinks on head */
	int	ps_unlink_pvsearch;	/* of pv_unlink searches */
	int	ps_pmeg_faultin;	/* pmegs reloaded */
	int	ps_changeprots;		/* of calls to changeprot */
	int	ps_changewire;		/* useless wiring changes */
	int	ps_npg_prot_all;	/* of active pages protected */
	int	ps_npg_prot_actual;	/* pages actually affected */
	int	ps_vac_uncached;	/* non-cached due to bad alias */
	int	ps_vac_recached;	/* re-cached when bad alias gone */
} pmap_stats;

struct context_state {
	TAILQ_ENTRY(context_state) context_link;
	int            context_num;
	struct pmap   *context_upmap;
};

typedef struct context_state *context_t;


#define VA_SEGNUM(x)	((u_int)(x) >> SEGSHIFT)

/* This is for pmap_next_page() */
static vm_offset_t avail_next;

/* This is where we map a PMEG without a context. */
static vm_offset_t temp_seg_va;

/* XXX - Why do we need this? */
#define	managed(pa)	(((pa) >= avail_start) && ((pa) < avail_end))


#define	NKSEG	(NSEGMAP - (KERNBASE / NBSG)) /* is KERNBASE ok? */
#define	NUSEG	(NSEGMAP-NKSEG)

/*
 * locking issues:
 *
 */

/*
 * Note that PMAP_LOCK is used in routines called at splnet() and
 * MUST NOT lower the priority.  For this reason we arrange that:
 *    splimp = max(splnet,splbio)
 * Would splvm() be more natural here? (same level as splimp).
 */
#define splpmap splimp
#define PMAP_LOCK() s = splpmap()
#define PMAP_UNLOCK() splx(s)

#define TAILQ_EMPTY(headp) \
		!((headp)->tqh_first)

#define TAILQ_REMOVE_FIRST(result, headp, entries) \
{ \
	result = (headp)->tqh_first; \
	if (result) TAILQ_REMOVE(headp, result, entries); \
	}

/*
 * pv support, i.e stuff that maps from physical pages to virtual addresses
 *
 */

/*
 * XXX - Could eliminate this by causing managed() to return 0
 * ( avail_start = avail_end = 0 )
 */
int pv_initialized = 0;

struct pv_entry {
	struct pv_entry *pv_next;
	pmap_t	       pv_pmap;
	vm_offset_t      pv_va;
	/* XXX - put flags in low byte of pv_va */
	unsigned int     pv_flags;
};
typedef struct pv_entry *pv_entry_t;

pv_entry_t pv_head_table = NULL;
#ifdef	DIAGNOSTIC
static struct pv_entry *
pa_to_pvp(pa)
	vm_offset_t pa;
{
	struct pv_entry *pvp;
	if (pa < avail_start || pa >= avail_end) {
		panic("pmap:pa_to_pvp: bad pa=0x%x", pa);
	}
	pvp = &pv_head_table[PA_PGNUM(pa)];
	return pvp;
}
#else
#define pa_to_pvp(pa) &pv_head_table[PA_PGNUM(pa)]
#endif

/* These are as in the MMU but shifted by PV_SHIFT. */
#define PV_SHIFT	24
#define PV_VALID  0x80
#define PV_WRITE  0x40
#define PV_SYSTEM 0x20
#define PV_NC     0x10
#define PV_PERM   0xF0
#define PV_TYPE   0x0C
#define PV_REF    0x02
#define PV_MOD    0x01

#define MAKE_PV_REAL(pv_flags) ((pv_flags & PV_PERM) << PV_SHIFT)
#define PG_TO_PV_FLAGS(pte) (((PG_PERM) & pte) >> PV_SHIFT)

/*
 * Only "main memory" pages are registered in the pv_lists.
 * This macro is used to determine if a given pte refers to
 * "main memory" or not.  One slight hack here deserves more
 * explanation:  The Sun frame buffers all appear as PG_OBMEM
 * devices but way up near the end of the address space.
 * We do not want to consider these as "main memory" so the
 * macro below treats the high bits of the PFN as type bits.
 *
 * Note that on the 3/60 only 16 bits of PFN are stored in the
 * MMU and the top 3 bits read back as zero.  This means a
 * translation entered into the mmu for physical address
 * 0xFF000000 will look like 0x1F000000 after one reads back
 * the pte and converts the PFN to a physical address.
 */
#define MEM_BITS	(PG_TYPE | PA_PGNUM(0xF0000000))
#define	IS_MAIN_MEM(pte) (((pte) & MEM_BITS) == 0)


/*
 * cache support
 */
#define	BADALIAS(a1, a2)	(((int)(a1) ^ (int)(a2)) & SEGOFSET)

/*
 * Save the MOD bit from the given PTE using its PA
 */
static void
save_modref_bits(int pte)
{
	pv_entry_t pvhead;

	if (pv_initialized == 0)
		return;

	/* Only main memory is ever in the pv_lists */
	if (!IS_MAIN_MEM(pte))
		return;

	pvhead = pa_to_pvp(PG_PA(pte));
	pvhead->pv_flags |= ((pte & PG_MODREF) >> PV_SHIFT);
}

#define pmap_lock(pmap) simple_lock(&pmap->pm_lock)
#define pmap_unlock(pmap) simple_unlock(&pmap->pm_lock)
#define pmap_add_ref(pmap) ++pmap->pm_refcount
#define pmap_del_ref(pmap) --pmap->pm_refcount
#define pmap_refcount(pmap) pmap->pm_refcount

#define PM_UPDATE_CACHE 1

static int pmap_version = 1;
struct pmap 	kernel_pmap_store;
#define kernel_pmap (&kernel_pmap_store)

/* protection conversion */
static unsigned int protection_converter[8];
#define pmap_pte_prot(x) protection_converter[x&7]

/*
 * pmeg structures, queues, and macros
 */
#define PMEGQ_FREE     0
#define PMEGQ_INACTIVE 1
#define PMEGQ_ACTIVE   2
#define PMEGQ_KERNEL   3
#define PMEGQ_NONE     4

struct pmeg_state {
	TAILQ_ENTRY(pmeg_state) pmeg_link;
	int            pmeg_index;
	pmap_t         pmeg_owner;
	int            pmeg_version;
	vm_offset_t    pmeg_va;
	int            pmeg_wired;
	int            pmeg_reserved;
	int            pmeg_vpages;
	int            pmeg_qstate;
};

typedef struct pmeg_state *pmeg_t;

#define PMEG_INVAL (NPMEG-1)
#define PMEG_NULL (pmeg_t) NULL

/* XXX - Replace pmeg_kernel_queue with pmeg_wired_queue ? */
TAILQ_HEAD(pmeg_tailq, pmeg_state);
struct pmeg_tailq pmeg_free_queue, pmeg_inactive_queue,
	pmeg_active_queue, pmeg_kernel_queue;

static struct pmeg_state pmeg_array[NPMEG];

#ifdef	PMAP_DEBUG
static pmeg_t
pmeg_p(sme)
	int sme;
{
	if (sme < 0 || sme >= SEGINV)
		panic("pmeg_p: bad sme");
	return &pmeg_array[sme];
}
#else
#define pmeg_p(x) &pmeg_array[x]
#endif

#define is_pmeg_wired(pmegp) (pmegp->pmeg_wired != 0)

/*
 * context structures, and queues
 */
#define	CTXINVAL -1
#define	has_context(pmap)	(pmap->pm_ctxnum >= 0)

TAILQ_HEAD(context_tailq, context_state);
struct context_tailq context_free_queue, context_active_queue;

static struct context_state context_array[NCONTEXT];

/*
 * location to store virtual addresses
 * to be used in copy/zero operations
 * (set in sun3_startup.c)
 */
vm_offset_t tmp_vpages[2];
int tmp_vpages_inuse;


/*
 * prototypes
 */
static int get_pte_pmeg __P((int, int));
static void set_pte_pmeg __P((int, int, int));

static void context_allocate __P((pmap_t pmap));
static void context_free __P((pmap_t pmap));
static void context_init __P((void));

static void pmeg_flush __P((pmeg_t pmegp));
static pmeg_t pmeg_allocate __P((pmap_t pmap, vm_offset_t va));
static void pmeg_release __P((pmeg_t pmegp));
static void pmeg_free __P((pmeg_t pmegp, int segnum));
static pmeg_t pmeg_cache __P((pmap_t pmap, vm_offset_t va));
static void pmeg_set_wiring __P((pmeg_t pmegp, vm_offset_t va, int));

static int pv_compute_cache __P((pv_entry_t head));
static int pv_link __P((pmap_t pmap, vm_offset_t, vm_offset_t, u_int));
static void pv_unlink __P((pmap_t, vm_offset_t, vm_offset_t));
static void pv_remove_all __P(( vm_offset_t pa));
static void pv_changepte __P((pv_entry_t, int, int));
static void pv_syncflags __P((pv_entry_t head));
static void pv_init __P((void));

void sun3_pmeg_init __P((void));
void sun3_reserve_pmeg __P((int pmeg_num));
void sun3_protection_init __P((void));

static void pmap_common_init __P((pmap_t pmap));

static void pmap_user_pmap_init __P((pmap_t pmap));

static void pmap_remove_range_mmu __P((pmap_t, vm_offset_t, vm_offset_t));
static void pmap_remove_range_noctx __P((pmap_t, vm_offset_t, vm_offset_t));
static void pmap_remove_range __P((pmap_t pmap, vm_offset_t, vm_offset_t));

static void pmap_enter_kernel __P((vm_offset_t va, vm_offset_t pa,
	vm_prot_t prot, boolean_t wired, int pte_proto));
static void pmap_enter_user __P((pmap_t pmap, vm_offset_t va, vm_offset_t pa,
	vm_prot_t prot, boolean_t wired, int pte_proto));

static void pmap_protect_range_noctx __P((pmap_t, vm_offset_t, vm_offset_t));
static void pmap_protect_range_mmu __P((pmap_t, vm_offset_t, vm_offset_t));
static void pmap_protect_range __P((pmap_t, vm_offset_t, vm_offset_t));


/*
 * Debugging support.
 */
#define	PMD_ENTER	1
#define	PMD_LINK	2
#define	PMD_PROTECT	4
#define	PMD_SWITCH	8
#define PMD_COW		0x10
#define PMD_MODBIT	0x20
#define PMD_REFBIT	0x40
#define PMD_WIRING	0x80
#define PMD_CONTEXT	0x100
#define PMD_CREATE	0x200
#define PMD_SEGMAP	0x400
#define PMD_SETPTE	0x800

#define	PMD_REMOVE	PMD_ENTER
#define	PMD_UNLINK	PMD_LINK

#ifdef	PMAP_DEBUG	/* XXX */
int pmap_debug = 0;
int pmap_db_watchva = -1;
int pmap_db_watchpmeg = -1;
#endif	/* PMAP_DEBUG */

#ifdef	PMAP_DEBUG	/* XXX */
extern int getsr();
#define	CHECK_SPL() do { \
	if ((getsr() & PSL_IPL) < PSL_IPL3) \
		panic("pmap: bad spl, line %d", __LINE__); \
} while (0)
#else	/* PMAP_DEBUG */
#define	CHECK_SPL() (void)0
#endif	/* PMAP_DEBUG */

static void
context_allocate(pmap)
	pmap_t pmap;
{
	context_t context;
	int s;

	PMAP_LOCK();
#ifdef	PMAP_DEBUG
	if (pmap_debug & PMD_CONTEXT)
		printf("context_allocate: for pmap %x\n", pmap);
#endif
	if (pmap == kernel_pmap)
		panic("context_allocate: kernel_pmap");
	if (has_context(pmap))
		panic("pmap: pmap already has context allocated to it");
	if (TAILQ_EMPTY(&context_free_queue)) {
		/* Steal one from the active queue. */
		if (TAILQ_EMPTY(&context_active_queue))
			panic("pmap: no contexts to be found");
		context_free((&context_active_queue)->tqh_first->context_upmap);
#ifdef	PMAP_DEBUG
		if (pmap_debug & PMD_CONTEXT)
			printf("context_allocate: pmap %x, take context %x num %d\n",
				   pmap, context, context->context_num);
#endif
	}
	TAILQ_REMOVE_FIRST(context, &context_free_queue, context_link);
	TAILQ_INSERT_TAIL(&context_active_queue, context, context_link);
	if (context->context_upmap != NULL)
		panic("pmap: context in use???");
	pmap->pm_ctxnum = context->context_num;
	context->context_upmap = pmap;
#ifdef	PMAP_DEBUG
	if (pmap_debug & PMD_CONTEXT)
		printf("context_allocate: pmap %x given context %x num %d\n",
			   pmap, context, context->context_num);
#endif

	/*
	 * We could reload the MMU here, but that would
	 * artificially move PMEGs from the inactive queue
	 * to the active queue, so do lazy reloading.
	 * XXX - Need to reload wired pmegs though...
	 */

	PMAP_UNLOCK();
}

static void
context_free(pmap)		/* :) */
	pmap_t pmap;
{
	int saved_ctxnum, ctxnum;
	int i, s, sme;
	context_t contextp;
	vm_offset_t va;

	PMAP_LOCK();

	ctxnum = pmap->pm_ctxnum;
#ifdef	PMAP_DEBUG
	if (pmap_debug & PMD_CONTEXT)
		printf("context_free: freeing context num %d of pmap 0x%x\n",
			   ctxnum, pmap);
#endif

	if (ctxnum < 0 || ctxnum >= NCONTEXT)
		panic("pmap: can't free a non-existent context");
	contextp = &context_array[ctxnum];

	saved_ctxnum = get_context();
	set_context(ctxnum);

	/* Before unloading translations, flush cache. */
#ifdef	HAVECACHE
	if (cache_size)
		cache_flush_context();
#endif

	/* Unload MMU (but keep in SW segmap). */
	va = 0;
	for (i=0; i < NUSEG; i++) {
		if (pmap->pm_segmap[i] != SEGINV) {
			/* The MMU might have a valid sme. */
			sme = get_segmap(va);
			if (sme != SEGINV) {
#ifdef	PMAP_DEBUG
				/* Validate SME found in MMU. */
				if (sme != pmap->pm_segmap[i])
					panic("context_free: unknown sme at va=0x%x", va);
				if (pmap_debug & PMD_SEGMAP)
					printf("pmap: set_segmap ctx=%d v=%x old=%x new=ff (cf)\n",
						   ctxnum, sun3_trunc_seg(va), sme);
#endif
				/* Did cache flush above (whole context). */
				set_segmap(va, SEGINV);
				pmeg_release(pmeg_p(sme));
			}
		}
		va += NBSG;
	}
	set_context(saved_ctxnum);
	contextp->context_upmap = NULL;
	TAILQ_REMOVE(&context_active_queue, contextp, context_link);
	TAILQ_INSERT_TAIL(&context_free_queue, contextp,
					  context_link);/* active??? XXX */
	pmap->pm_ctxnum = CTXINVAL;
#ifdef	PMAP_DEBUG
	if (pmap_debug & PMD_CONTEXT)
		printf("context_free: pmap %x context removed\n", pmap);
#endif
	PMAP_UNLOCK();
}

static void
context_init()
{
	int i;

	TAILQ_INIT(&context_free_queue);
	TAILQ_INIT(&context_active_queue);

	for (i=0; i < NCONTEXT; i++) {
		context_array[i].context_num = i;
		context_array[i].context_upmap = NULL;
		TAILQ_INSERT_TAIL(&context_free_queue, &context_array[i],
						  context_link);
#ifdef	PMAP_DEBUG
		if (pmap_debug & PMD_CONTEXT)
			printf("context_init: context num %d is %x\n",
				   i, &context_array[i]);
#endif
	}
}

#ifdef	PMAP_DEBUG
void pmap_print(pmap)
	pmap_t pmap;
{
	printf(" pm_ctxnum=%d\n", pmap->pm_ctxnum);
	printf(" pm_version=0x%x\n", pmap->pm_version);
	printf(" pm_segmap=0x%x\n", pmap->pm_segmap);
}
#endif

/*
 * Reserve a pmeg (forever) for use by PROM, etc.
 * Contents are left as-is.  Called very early...
 */
void
sun3_reserve_pmeg(sme)
	int sme;
{
	pmeg_t pmegp;

	/* Can not use pmeg_p() because it fails on SEGINV. */
	pmegp = &pmeg_array[sme];

	if (pmegp->pmeg_reserved)
		mon_panic("sun3_reserve_pmeg: already reserved\n");
	if (pmegp->pmeg_owner)
		mon_panic("sun3_reserve_pmeg: already owned\n");

	/* XXX - Owned by kernel, but not really usable... */
	pmegp->pmeg_owner = kernel_pmap;
	pmegp->pmeg_reserved++;	/* keep count, just in case */
	TAILQ_REMOVE(&pmeg_free_queue, pmegp, pmeg_link);
	pmegp->pmeg_qstate = PMEGQ_NONE;
}

static void
pmeg_clean(pmegp)
	pmeg_t pmegp;
{
	int i;

	for (i = 0; i < NPAGSEG; i++)
		set_pte_pmeg(pmegp->pmeg_index, i, PG_INVAL);
}

/*
 * This routine makes sure that pmegs on the pmeg_free_queue contain
 * no valid ptes.  It pulls things off the queue, cleans them, and
 * puts them at the end.  Ending condition is finding the first queue element
 * at the head of the queue again.
 */

static void
pmeg_clean_free()
{
	pmeg_t pmegp, pmegp_first;

	CHECK_SPL();

	if (TAILQ_EMPTY(&pmeg_free_queue))
		panic("pmap: no free pmegs available to clean");

	pmegp_first = NULL;

	for (;;) {

		TAILQ_REMOVE_FIRST(pmegp, &pmeg_free_queue, pmeg_link);

		pmegp->pmeg_qstate = PMEGQ_NONE;

		pmeg_clean(pmegp);

		TAILQ_INSERT_TAIL(&pmeg_free_queue, pmegp, pmeg_link);
		pmegp->pmeg_qstate = PMEGQ_FREE;

		if (pmegp == pmegp_first)
			break;
		if (pmegp_first == NULL)
			pmegp_first = pmegp;

	}
}

#ifdef	PMAP_DEBUG
static void
pmeg_verify_empty(va)
	vm_offset_t va;
{
	vm_offset_t eva;
	int pte;

	for (eva = va + NBSG;  va < eva; va += NBPG) {
		pte = get_pte(va);
		if (pte & PG_VALID)
			panic("pmeg_verify_empty");
	}
}

static void
pmeg_print(pmegp)
	pmeg_t pmegp;
{
	printf("link_next=0x%x  link_prev=0x%x\n",
		   pmegp->pmeg_link.tqe_next,
		   pmegp->pmeg_link.tqe_prev);
	printf("index=0x%x owner=0x%x own_vers=0x%x\n",
		   pmegp->pmeg_index, pmegp->pmeg_owner,
		   pmegp->pmeg_version);
	printf("va=0x%x wired=0x%x reserved=0x%x vpgs=0x%x qstate=0x%x\n",
		   pmegp->pmeg_va, pmegp->pmeg_wired,
		   pmegp->pmeg_reserved, pmegp->pmeg_vpages,
		   pmegp->pmeg_qstate);
}
#endif

/*
 * Allocate a PMEG by whatever means necessary.
 * (May invalidate some mappings!)
 */
static pmeg_t
pmeg_allocate(pmap, va)
	pmap_t pmap;
	vm_offset_t va;
{
	pmeg_t pmegp;

	CHECK_SPL();

#ifdef	DIAGNOSTIC
	if (va & SEGOFSET) {
		panic("pmap:pmeg_allocate: va=0x%x", va);
	}
#endif

	/* Get one onto the free list if necessary. */
	pmegp = pmeg_free_queue.tqh_first;
	if (!pmegp) {
		/* Try inactive queue... */
		pmegp = pmeg_inactive_queue.tqh_first;
		if (!pmegp) {
			/* Try active queue... */
			pmegp = pmeg_active_queue.tqh_first;
		}
		if (!pmegp) {
			panic("pmeg_allocate: failed");
		}
		/*
		 * Remove mappings to free-up a pmeg
		 * (so it will go onto the free list).
		 * XXX - Should this call up into the VM layer
		 * to notify it when pages are deactivated?
		 * See: vm_page.c:vm_page_deactivate(vm_page_t)
		 * XXX - Skip this one if it is wired?
		 */
		pmap_remove_range(pmegp->pmeg_owner,
						  pmegp->pmeg_va,
						  pmegp->pmeg_va + NBSG);
	}

	/* OK, free list has something for us to take. */
	pmegp = pmeg_free_queue.tqh_first;
#ifdef	PMAP_DEBUG
	if (!pmegp) {
		panic("pmeg_allocagte: still none free?");
	}
	if (pmegp->pmeg_qstate != PMEGQ_FREE)
		panic("pmeg_allocate: bad on free queue: %x", pmegp);
#endif
	TAILQ_REMOVE(&pmeg_free_queue, pmegp, pmeg_link);

#ifdef	PMAP_DEBUG
	if (pmegp->pmeg_index == pmap_db_watchpmeg) {
		printf("pmeg_allocate: watch pmeg 0x%x\n", pmegp->pmeg_index);
		Debugger();
	}
#endif
#ifdef	DIAGNOSTIC
	if (pmegp->pmeg_index == SEGINV)
		panic("pmeg_allocate: pmeg_index=ff");
	if (pmegp->pmeg_vpages)
		panic("pmeg_allocate: vpages!=0, pmegp=%x", pmegp);
#endif

	/* Reassign this PMEG for the caller. */
	pmegp->pmeg_owner = pmap;
	pmegp->pmeg_version = pmap->pm_version;
	pmegp->pmeg_va = va;
	pmegp->pmeg_wired = 0;
	pmegp->pmeg_reserved  = 0;
	pmegp->pmeg_vpages  = 0;
	if (pmap == kernel_pmap) {
		TAILQ_INSERT_TAIL(&pmeg_kernel_queue, pmegp, pmeg_link);
		pmegp->pmeg_qstate = PMEGQ_KERNEL;
	} else {
		TAILQ_INSERT_TAIL(&pmeg_active_queue, pmegp, pmeg_link);
		pmegp->pmeg_qstate = PMEGQ_ACTIVE;
#ifdef	PMAP_DEBUG
		if (pmap_debug & PMD_SEGMAP) {
			printf("pm_segmap: pmap=%x i=%x old=%x new=%x (pget)\n",
				   pmap, VA_SEGNUM(va),
				   pmap->pm_segmap[VA_SEGNUM(va)],
				   pmegp->pmeg_index);
		}
#endif
	}
	/* Caller will verify that it's empty (if debugging). */
	return pmegp;
}

/*
 * Put pmeg on the inactive queue, leaving its contents intact.
 * This happens when we loose our context.  We may reclaim
 * this pmeg later if it is still in the inactive queue.
 */
static void
pmeg_release(pmegp)
	pmeg_t pmegp;
{
	CHECK_SPL();

#ifdef	PMAP_DEBUG
	if (pmegp->pmeg_qstate == PMEGQ_INACTIVE) {
		printf("pmeg_release: already inactive\n");
		Debugger();
		return;
	}
#endif

#ifdef	DIAGNOSTIC
	if (pmegp->pmeg_owner == kernel_pmap)
		panic("pmeg_release: kernel_pmap");
	if (pmegp->pmeg_qstate != PMEGQ_ACTIVE)
		panic("pmeg_release: not q_active %x", pmegp);
#endif

	TAILQ_REMOVE(&pmeg_active_queue, pmegp, pmeg_link);
	pmegp->pmeg_qstate = PMEGQ_INACTIVE;
	TAILQ_INSERT_TAIL(&pmeg_inactive_queue, pmegp, pmeg_link);
}

/*
 * Move the pmeg to the free queue from wherever it is.
 * The pmeg will be clean.  It might be in kernel_pmap.
 */
static void
pmeg_free(pmegp, segnum)
	pmeg_t pmegp;
	int segnum;
{
	CHECK_SPL();

#ifdef	PMAP_DEBUG
	/* XXX - Caller should verify that it's empty. */
	if (pmegp->pmeg_vpages != 0)
		panic("pmeg_free: vpages");
#endif

	switch (pmegp->pmeg_qstate) {
	case PMEGQ_ACTIVE:
		TAILQ_REMOVE(&pmeg_active_queue, pmegp, pmeg_link);
		break;
	case PMEGQ_INACTIVE:
		TAILQ_REMOVE(&pmeg_inactive_queue, pmegp, pmeg_link);
		break;
	case PMEGQ_KERNEL:
		TAILQ_REMOVE(&pmeg_kernel_queue, pmegp, pmeg_link);
		break;
	default:
		panic("pmeg_free: releasing bad pmeg");
		break;
	}

#ifdef	PMAP_DEBUG
	if (pmegp->pmeg_index == pmap_db_watchpmeg) {
		printf("pmeg_free: watch pmeg 0x%x\n",
			   pmegp->pmeg_index);
		Debugger();
	}
#endif

	pmegp->pmeg_owner = NULL;
	pmegp->pmeg_qstate = PMEGQ_FREE;
	TAILQ_INSERT_TAIL(&pmeg_free_queue, pmegp, pmeg_link);
}

/*
 * Find a PMEG that was put on the inactive queue when we
 * had our context stolen.  If found, move to active queue.
 */
static pmeg_t
pmeg_cache(pmap, va)
	pmap_t pmap;
	vm_offset_t va;
{
	int sme, segnum;
	pmeg_t pmegp;

	CHECK_SPL();

#ifdef	PMAP_DEBUG
	if (pmap == kernel_pmap)
		panic("pmeg_cache: kernel_pmap");
#endif

#ifdef	DIAGNOSTIC
	if (va & SEGOFSET) {
		panic("pmap:pmeg_cache: va=0x%x", va);
	}
#endif

	if (pmap->pm_segmap == NULL)
		return PMEG_NULL;
	segnum = VA_SEGNUM(va);
	if (segnum > NUSEG)		/* out of range */
		return PMEG_NULL;
	sme = pmap->pm_segmap[segnum];
	if (sme == SEGINV)	/* nothing cached */
		return PMEG_NULL;

	pmegp = pmeg_p(sme);

#ifdef	PMAP_DEBUG
	if (pmegp->pmeg_index == pmap_db_watchpmeg) {
		printf("pmeg_cache: watch pmeg 0x%x\n", pmegp->pmeg_index);
		Debugger();
	}
#endif

	/*
	 * Our segmap named a PMEG.  If it is no longer ours,
	 * invalidate that entry in our segmap and return NULL.
	 */
	if ((pmegp->pmeg_owner != pmap) ||
		(pmegp->pmeg_version != pmap->pm_version) ||
		(pmegp->pmeg_va != va))
	{
#ifdef	PMAP_DEBUG
		printf("pmap:pmeg_cache: invalid pmeg: sme=0x%x\n", sme);
		pmeg_print(pmegp);
		Debugger(); /* XXX */
#endif
		pmap->pm_segmap[segnum] = SEGINV;
		return PMEG_NULL; /* cache lookup failed */
	}

#ifdef	PMAP_DEBUG
	/* Make sure it is on the inactive queue. */
	if (pmegp->pmeg_qstate != PMEGQ_INACTIVE)
		panic("pmeg_cache: pmeg was taken: %x", pmegp);
#endif

	TAILQ_REMOVE(&pmeg_inactive_queue, pmegp, pmeg_link);
	pmegp->pmeg_qstate = PMEGQ_ACTIVE;
	TAILQ_INSERT_TAIL(&pmeg_active_queue, pmegp, pmeg_link);

	return pmegp;
}

static void
pmeg_set_wiring(pmegp, va, flag)
	pmeg_t pmegp;
	vm_offset_t va;
	int flag;
{
	int idx, mask;

	CHECK_SPL();
	idx = VA_PTE_NUM(va);
	mask = 1 << idx;

	if (flag)
		pmegp->pmeg_wired |= mask;
	else
		pmegp->pmeg_wired &= ~mask;
}

void
sun3_pmeg_init()
{
	int x;

	/* clear pmeg array, put it all on the free pmeq queue */

	TAILQ_INIT(&pmeg_free_queue);
	TAILQ_INIT(&pmeg_inactive_queue);
	TAILQ_INIT(&pmeg_active_queue);
	TAILQ_INIT(&pmeg_kernel_queue);

	bzero(pmeg_array, NPMEG*sizeof(struct pmeg_state));
	for (x =0 ; x<NPMEG; x++) {
		TAILQ_INSERT_TAIL(&pmeg_free_queue, &pmeg_array[x],
				  pmeg_link);
		pmeg_array[x].pmeg_qstate = PMEGQ_FREE;
		pmeg_array[x].pmeg_index = x;
	}

	/* The last pmeg is not usable. */
	sun3_reserve_pmeg(SEGINV);
}

#ifdef	PMAP_DEBUG
static void
pv_print(pa)
	vm_offset_t pa;
{
	pv_entry_t pv;

	if (!pv_initialized)
		return;

	pv = pa_to_pvp(pa);
	printf("pv_list for pa %x: flags=%x\n", pa, pv->pv_flags);
	while (pv) {
		printf("pv_entry %x pmap %x va %x next %x\n",
			   pv, pv->pv_pmap, pv->pv_va, pv->pv_next);
		pv = pv->pv_next;
	}
}
#endif	/* PMAP_DEBUG */

/*
 * Set or clear bits in all PTEs mapping a page.
 * Also does syncflags work while we are there...
 */
static void
pv_changepte(head, set_bits, clear_bits)
	pv_entry_t head;
	int set_bits;
	int clear_bits;
{
	pv_entry_t pv;
	pmap_t pmap;
	vm_offset_t va;
	int pte, sme, s;
	int saved_ctx;
	boolean_t in_ctx;

	if (!pv_initialized)
		return;
	if (head->pv_pmap == NULL)
		return;
	if ((set_bits == 0) && (clear_bits == 0))
		return;
#ifdef	DIAGNOSTIC
	/* This function should only clear these bits: */
	if (clear_bits & ~(PG_WRITE | PG_NC | PG_REF | PG_MOD))
		panic("pv_changepte: clear=0x%x\n");
#endif

	s = splpmap();
	saved_ctx = get_context();

	for (pv = head; pv != NULL; pv = pv->pv_next) {
		pmap = pv->pv_pmap;
		va = pv->pv_va;
#ifdef	DIAGNOSTIC
		/*
		 * Only the head may have a null pmap, and
		 * we checked for that above.
		 */
		if (pmap == NULL)
			panic("pv_changepte: null pmap");
#endif

		/* Is the PTE currently accessable in some context? */
		in_ctx = FALSE;
		if (pmap == kernel_pmap)
			in_ctx = TRUE;
		else if (has_context(pmap)) {
			/* PMEG may be inactive. */
			set_context(pmap->pm_ctxnum);
			sme = get_segmap(va);
			if (sme != SEGINV)
				in_ctx = TRUE;
		}

		if (in_ctx == TRUE) {
			/*
			 * The PTE is in the current context.
			 * Make sure PTE is up-to-date with VAC.
			 */
#ifdef	HAVECACHE
			if (cache_size)
				cache_flush_page(va);
#endif
			pte = get_pte(va);
		} else {
			/*
			 * The PTE is not in any context.
			 */
			if (pmap->pm_segmap == NULL)
				panic("pv_changepte: null segmap");
			sme = pmap->pm_segmap[VA_SEGNUM(va)];
			if (sme == SEGINV)
				panic("pv_changepte: SEGINV");
			pte = get_pte_pmeg(sme, VA_PTE_NUM(va));
		}

#ifdef	DIAGNOSTIC
		/*
		 * PV entries point only to valid mappings.
		 * XXX - Make sure pv_unlink() was done...
		 */
		if ((pte & PG_VALID) == 0)
			panic("pv_changepte: not PG_VALID at va=0x%x\n", va);
#endif
		/* Get these while it's easy. */
		if (pte & PG_MODREF) {
			head->pv_flags |= ((pte & PG_MODREF) >> PV_SHIFT);
			pte &= ~PG_MODREF;
		}

		/* Finally, set and clear some bits. */
		pte |= set_bits;
		pte &= ~clear_bits;

		if (in_ctx == TRUE) {
			/* Did cache flush above. */
			set_pte(va, pte);
		} else {
			set_pte_pmeg(sme, VA_PTE_NUM(va), pte);
		}
	}

	set_context(saved_ctx);
	splx(s);
	return;
}

/*
 * Sync ref and mod bits in pvlist
 * (turns off same in hardware PTEs).
 */
static void
pv_syncflags(head)
	pv_entry_t head;
{
	pv_entry_t pv;
	pmap_t pmap;
	vm_offset_t va;
	int pte, sme, s;
	int saved_ctx;
	boolean_t in_ctx;

	if (!pv_initialized)
		return;
	if (head->pv_pmap == NULL)
		return;

	PMAP_LOCK();
	saved_ctx = get_context();

	for (pv = head; pv != NULL; pv = pv->pv_next) {
		pmap = pv->pv_pmap;
		va = pv->pv_va;
#ifdef	DIAGNOSTIC
		/*
		 * Only the head may have a null pmap, and
		 * we checked for that above.
		 */
		if (pmap == NULL)
			panic("pv_syncflags: null pmap");
#endif

		/* Is the PTE currently accessable in some context? */
		in_ctx = FALSE;
		if (pmap == kernel_pmap)
			in_ctx = TRUE;
		else if (has_context(pmap)) {
			/* PMEG may be inactive. */
			set_context(pmap->pm_ctxnum);
			sme = get_segmap(va);
			if (sme != SEGINV)
				in_ctx = TRUE;
		}

		if (in_ctx == TRUE) {
			/*
			 * The PTE is in the current context.
			 * Make sure PTE is up-to-date with VAC.
			 */
#ifdef	HAVECACHE
			if (cache_size)
				cache_flush_page(va);
#endif
			pte = get_pte(va);
		} else {
			/*
			 * The PTE is not in any context.
			 * XXX - Consider syncing MODREF bits
			 * when the PMEG looses its context?
			 */
			if (pmap->pm_segmap == NULL)
				panic("pv_syncflags: null segmap");
			sme = pmap->pm_segmap[VA_SEGNUM(va)];
			if (sme == SEGINV)
				panic("pv_syncflags: SEGINV");
			pte = get_pte_pmeg(sme, VA_PTE_NUM(va));
		}

#ifdef	DIAGNOSTIC
		/*
		 * PV entries point only to valid mappings.
		 * XXX - Make sure pv_unlink() was done...
		 */
		if ((pte & PG_VALID) == 0)
			panic("pv_syncflags: not PG_VALID at va=0x%x\n", va);
#endif
		/* OK, do what we came here for... */
		if (pte & PG_MODREF) {
			head->pv_flags |= ((pte & PG_MODREF) >> PV_SHIFT);
			pte &= ~PG_MODREF;
		}

		if (in_ctx == TRUE) {
			/* Did cache flush above. */
			set_pte(va, pte);
		} else {
			set_pte_pmeg(sme, VA_PTE_NUM(va), pte);
		}
	}

	set_context(saved_ctx);

	PMAP_UNLOCK();
	return;
}


/* pv_entry support routines */
static void
pv_remove_all(pa)
	vm_offset_t pa;
{
	pv_entry_t pv;
	pmap_t pmap;
	vm_offset_t va;

#ifdef PMAP_DEBUG
	if (pmap_debug & PMD_REMOVE)
		printf("pv_remove_all(%x)\n", pa);
#endif
	if (!pv_initialized)
		return;

	/* The PV head never moves. */
	pv = pa_to_pvp(pa);
	while (pv->pv_pmap != NULL) {
		pmap = pv->pv_pmap;
		va   = pv->pv_va;
		pmap_remove_range(pmap, va, va + NBPG);
#ifdef PMAP_DEBUG
		/* Make sure it went away. */
		if ((pv->pv_pmap == pmap) && (pv->pv_va == va))
		{
			printf("pv_remove_all: head unchanged for pa=0x%x\n", pa);
			Debugger();
		}
#endif
	}
}

/*
 * The pmap system is asked to lookup all mappings that point to a
 * given physical memory address.  This function adds a new element
 * to the list of mappings maintained for the given physical address.
 * Returns PV_NC if the (new) pvlist says that the address cannot
 * be cached.
 */
static int
pv_link(pmap, pa, va, flags)
	pmap_t pmap;
	vm_offset_t pa, va;
	u_int flags;
{
	pv_entry_t head, npv;

	if (!pv_initialized)
		return 0;

#ifdef PMAP_DEBUG
	if ((pmap_debug & PMD_LINK) || (va == pmap_db_watchva)) {
		printf("pv_link(%x, %x, %x, %x)\n", pmap, pa, va, flags);
		/* pv_print(pa); */
	}
#endif

	CHECK_SPL();

	/* Only the non-cached bit is of interest. */
	flags = flags & PV_NC;

	head = pa_to_pvp(pa);
	if (head->pv_pmap == NULL) {
		/* not currently mapped anywhere */
		pmap_stats.ps_enter_firstpv++;
		head->pv_va = va;
		head->pv_pmap = pmap,
		head->pv_next = NULL;
		head->pv_flags = flags;
		return (flags);
	}

#ifdef PMAP_DEBUG
	/* XXX - See if this mapping is already in the list? */
	for (npv = head; npv != NULL; npv = npv->pv_next) {
		if ((npv->pv_pmap == pmap) && (npv->pv_va == va))
			panic("pv_link: duplicate entry for PA=0x%x", pa);
	}
#endif

	/*
	 * Before entering the new mapping, see if it will cause
	 * old mappings to become aliased (need cache inhibit).
	 */
	pmap_stats.ps_enter_secondpv++;

	head->pv_flags |= flags;
	if ((head->pv_flags & PV_NC) == 0) {
		for (npv = head; npv != NULL; npv = npv->pv_next) {
			if (BADALIAS(va, npv->pv_va)) {
				head->pv_flags |= PV_NC;
				pv_changepte(head, PG_NC, 0);
				pmap_stats.ps_vac_uncached++;
				break;
			}
		}
	}
	npv = (pv_entry_t) malloc(sizeof(*npv), M_VMPVENT, M_WAITOK);
	npv->pv_next = head->pv_next;
	npv->pv_pmap = pmap;
	npv->pv_va   = va;
	head->pv_next = npv;

	return (head->pv_flags & PV_NC);
}

/*
 * pv_unlink is a helper function for pmap_remove.
 * It removes the appropriate (pmap, pa, va) entry.
 *
 * Once the entry is removed, if the pv_table head has the cache
 * inhibit bit set, see if we can turn that off; if so, walk the
 * pvlist and turn off PG_NC in each PTE.  (The pvlist is by
 * definition nonempty, since it must have at least two elements
 * in it to have PV_NC set, and we only remove one here.)
 */
static void
pv_unlink(pmap, pa, va)
	pmap_t pmap;
	vm_offset_t pa, va;
{
	pv_entry_t head, npv;

	if (!pv_initialized)
		return;

#ifdef PMAP_DEBUG
	if ((pmap_debug & PMD_UNLINK) ||
		(va == pmap_db_watchva))
	{
		printf("pv_unlink(%x, %x, %x)\n", pmap, pa, va);
	}
#endif
	head = pa_to_pvp(pa);

#ifdef DIAGNOSTIC
	if (head->pv_pmap == NULL)
		panic("pv_unlink: empty list");
#endif

	/*
	 * First entry is special (sigh).
	 */
	npv = head->pv_next;
	if (head->pv_pmap == pmap && head->pv_va == va) {
		pmap_stats.ps_unlink_pvfirst++;
		if (npv != NULL) {
			/* Copy next entry into (fixed) head. */
			head->pv_next = npv->pv_next;
			head->pv_pmap = npv->pv_pmap;
			head->pv_va   = npv->pv_va;
			free((caddr_t)npv, M_VMPVENT);
		} else {
			/* No next entry, list is now empty. */
			head->pv_pmap = NULL;
		}
	} else {
		register pv_entry_t prev;

		for (prev = head;; prev = npv, npv = npv->pv_next) {
			pmap_stats.ps_unlink_pvsearch++;
			if (npv == NULL) {
				printf("pv_unlink: not on list (pa=%x,va=%x)\n",
					   pa, va);
				Debugger();	/* XXX */
				return;
			}
			if (npv->pv_pmap == pmap && npv->pv_va == va)
				break;
		}
		prev->pv_next = npv->pv_next;
		free((caddr_t)npv, M_VMPVENT);
	}

	if (head->pv_flags & PV_NC) {
		/*
		 * Not cached: check to see if we can fix that now.
		 */
		va = head->pv_va;
		for (npv = head->pv_next; npv != NULL; npv = npv->pv_next)
			if (BADALIAS(va, npv->pv_va))
				return;
		head->pv_flags &= ~PV_NC;
		pv_changepte(head, 0, PG_NC);
		pmap_stats.ps_vac_recached++;
	}
}

static void
pv_init()
{
	int sz;

	sz = PA_PGNUM(avail_end);
	sz *= sizeof(struct pv_entry);

	pv_head_table = (pv_entry_t) kmem_alloc(kernel_map, sz);
	if (!pv_head_table)
		mon_panic("pmap: kmem_alloc() of pv table failed");
	bzero((caddr_t) pv_head_table, sz);

	pv_initialized++;
}

void
sun3_protection_init()
{
	unsigned int *kp, prot;

	kp = protection_converter;
	for (prot = 0; prot < 8; prot++) {
		switch (prot) {
			/* READ WRITE EXECUTE */
		case VM_PROT_NONE |VM_PROT_NONE |VM_PROT_NONE:
			*kp++ = PG_INVAL;
			break;
		case VM_PROT_NONE |VM_PROT_NONE |VM_PROT_EXECUTE:
		case VM_PROT_READ |VM_PROT_NONE |VM_PROT_NONE:
		case VM_PROT_READ |VM_PROT_NONE |VM_PROT_EXECUTE:
			*kp++ = PG_VALID;
			break;
		case VM_PROT_NONE |VM_PROT_WRITE |VM_PROT_NONE:
		case VM_PROT_NONE |VM_PROT_WRITE |VM_PROT_EXECUTE:
		case VM_PROT_READ |VM_PROT_WRITE |VM_PROT_NONE:
		case VM_PROT_READ |VM_PROT_WRITE |VM_PROT_EXECUTE:
			*kp++ = PG_VALID|PG_WRITE;
			break;
		}
	}
}
/* pmap maintenance routines */

void
pmap_common_init(pmap)
	pmap_t pmap;
{
	bzero(pmap, sizeof(struct pmap));
	pmap->pm_refcount=1;
	pmap->pm_version = pmap_version++;
	pmap->pm_ctxnum = CTXINVAL;
	simple_lock_init(&pmap->pm_lock);
}

/*
 * Prepare the kernel for VM operations.
 * This is called by sun3_startup:sun3_bootstrap()
 * after the "start/end" globals are set.
 */
void
pmap_bootstrap()
{
	extern void vm_set_page_size();

	/*
	 * Reserve a segment for the kernel to use to access a pmeg
	 * that is not currently mapped into any context/segmap.
	 * The kernel temporarily maps such a pmeg into this segment.
	 */
	temp_seg_va = virtual_avail;
	virtual_avail += NBSG;
#ifdef	DEBUG
	if (temp_seg_va & SEGOFSET)
		mon_panic("pmap_bootstrap: temp_seg_va");
#endif

	/* Initialization for pmap_next_page() */
	avail_next = avail_start;

	PAGE_SIZE = NBPG;
	vm_set_page_size();

	sun3_protection_init();

	/* after setting up some structures */

	pmap_common_init(kernel_pmap);

	context_init();

	pmeg_clean_free();
}

/*
 * For our convenience, vm_page.c implements:
 *	 pmap_startup(), pmap_steal_memory()
 * using the functions:
 *	 pmap_virtual_space(), pmap_free_pages(), pmap_next_page(),
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
 * Note that a machine with a "hole" in physical memory
 * may include the pages in the hole in this count, and
 * skip the pages in the hole in pmap_next_page().
 */
u_int
pmap_free_pages()
{
	int bytes;

	bytes = avail_end - avail_start;
	return(sun3_btop(bytes));
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
	/* Is it time to skip over the hole? */
	if (avail_next == hole_start)
		avail_next += sun3_round_page(hole_size);

	/* Any available memory remaining? */
	if (avail_next >= avail_end)
		return FALSE;

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
 * XXX - Should make this a macro in pmap.h
 */
int
pmap_page_index(pa)
	vm_offset_t pa;
{
	u_long idx;

#ifdef	DIAGNOSTIC
	if (pa < avail_start || pa >= avail_end)
		panic("pmap_page_index: pa=0x%x", pa);
	if (hole_start && pa >= hole_start) {
		/* Make sure pa is not in the hole. */
		if (pa < (hole_start + hole_size))
			panic("pmap_page_index: pa=0x%x", pa);
	}
#endif

	return (sun3_btop(pa));
}


/*
 *	Initialize the pmap module.
 *	Called by vm_init, to initialize any structures that the pmap
 *	system needs to map virtual memory.
 */
void
pmap_init()
{
	extern int physmem;

	pv_init();
	physmem = btoc((u_int)avail_end);
}

/*
 * Map a range of kernel virtual address space.
 * This might be used for device mappings, or to
 * record the mapping for kernel text/data/bss.
 */
vm_offset_t
pmap_map(virt, start, end, prot)
	vm_offset_t	virt;
	vm_offset_t	start;	/* physical */
	vm_offset_t	end;	/* physical */
	int		prot;
{
	while (start < end) {
		pmap_enter(kernel_pmap, virt, start, prot, FALSE);
		virt += NBPG;
		start += NBPG;
	}
	return(virt);
}

void
pmap_user_pmap_init(pmap)
	pmap_t pmap;
{
	int i;
	pmap->pm_segmap = malloc(sizeof(char)*NUSEG, M_VMPMAP, M_WAITOK);
	for (i=0; i < NUSEG; i++) {
		pmap->pm_segmap[i] = SEGINV;
	}
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
	pmap_t pmap;

	if (size)
		return NULL;

	pmap = (pmap_t) malloc(sizeof(struct pmap), M_VMPMAP, M_WAITOK);
	pmap_common_init(pmap);
	pmap_user_pmap_init(pmap);
	return pmap;
}

/*
 * Release any resources held by the given physical map.
 * Called when a pmap initialized by pmap_pinit is being released.
 * Should only be called if the map contains no valid mappings.
 */
void
pmap_release(pmap)
	struct pmap *pmap;
{

	if (pmap == kernel_pmap)
		panic("pmap_release: kernel_pmap!");

	if (has_context(pmap))
		context_free(pmap);
	free(pmap->pm_segmap, M_VMPMAP);
	pmap->pm_segmap = NULL;
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

	if (pmap == NULL)
		return;	/* XXX - Duh! */

#ifdef PMAP_DEBUG
	if (pmap_debug & PMD_CREATE)
		printf("pmap_destroy(%x)\n", pmap);
#endif
	if (pmap == kernel_pmap)
		panic("pmap_destroy: kernel_pmap!");
	pmap_lock(pmap);
	count = pmap_del_ref(pmap);
	pmap_unlock(pmap);
	if (count == 0) {
		pmap_release(pmap);
		free((caddr_t)pmap, M_VMPMAP);
	}
}

/*
 *	  pmap_page_protect:
 *
 *	  Lower the permission for all mappings to a given page.
 */
void
pmap_page_protect(pa, prot)
	vm_offset_t	 pa;
	vm_prot_t	   prot;
{
	int s;

	PMAP_LOCK();

#ifdef PMAP_DEBUG
	if (pmap_debug & PMD_PROTECT)
		printf("pmap_page_protect(%x, %x)\n", pa, prot);
#endif
	switch (prot) {
	case VM_PROT_ALL:
		break;
	case VM_PROT_READ:
	case VM_PROT_READ|VM_PROT_EXECUTE:
		pv_changepte(pa_to_pvp(pa), 0, PG_WRITE);
		break;
	default:
		/* remove mapping for all pmaps that have it:
		 *
		 * follow pv trail to pmaps and temporarily delete it that way.
		 * keep looping till all mappings go away
		 */
		pv_remove_all(pa);
	}

	PMAP_UNLOCK();
}

/*
 *	Add a reference to the specified pmap.
 */
void
pmap_reference(pmap)
	pmap_t	pmap;
{
	if (pmap != NULL) {
		pmap_lock(pmap);
		pmap_add_ref(pmap);
		pmap_unlock(pmap);
	}
}

/*
 * Remove some mappings, all in one PMEG,
 * where that PMEG is currently in the MMU.
 * The current context is already correct.
 * If no PTEs remain valid in the PMEG, free it.
 */
void
pmap_remove_range_mmu(pmap, sva, eva)
	pmap_t pmap;
	vm_offset_t sva, eva;
{
	int old_ctx, i;
	pmeg_t pmegp;
	vm_offset_t va;
	int pte, sme;
	int flush_by_page = 0;

#ifdef	PMAP_DEBUG
	if ((pmap_debug & PMD_REMOVE) ||
		((sva <= pmap_db_watchva && eva > pmap_db_watchva)))
		printf("pmap_remove_range_mmu(%x, %x, %x)\n", pmap, sva, eva);
#endif

	/* Interrupt level handled by caller. */
	CHECK_SPL();

#ifdef	DIAGNOSTIC
	if (pmap != kernel_pmap) {
		if (pmap->pm_ctxnum != get_context())
			panic("pmap_remove_range_mmu: wrong context");
	}
#endif

	va = sun3_trunc_seg(sva);
	sme = get_segmap(va);
#ifdef	DIAGNOSTIC
	/* Make sure it is valid and known. */
	if (sme == SEGINV)
		panic("pmap_remove_range_mmu: SEGINV");
	if (pmap->pm_segmap && (pmap->pm_segmap[VA_SEGNUM(sva)] != sme))
		panic("pmap_remove_range_mmu: incorrect sme, va=0x%x", va);
#endif
	pmegp = pmeg_p(sme);

	/* have pmeg, will travel */

#ifdef	DIAGNOSTIC
	/* Make sure we own the pmeg, right va, etc. */
	if ((pmegp->pmeg_va != va) ||
		(pmegp->pmeg_owner != pmap) ||
		(pmegp->pmeg_version != pmap->pm_version))
	{
		printf("pmap_remove_range_mmu: wrong pmeg, sme=0x%x\n", sme);
#ifdef	PMAP_DEBUG
		pmeg_print(pmegp);
		Debugger();	/* XXX */
#endif
		panic("bye");
	}
	if (pmegp->pmeg_vpages <= 0)
		panic("pmap_remove_range_mmu: no valid pages?");
#endif

#ifdef	HAVECACHE
	if (cache_size) {
		/*
		 * If the range to be removed is larger than the cache,
		 * it will be cheaper to flush this segment entirely.
		 */
		if (cache_size < (eva - sva)) {
			/* cheaper to flush whole segment */
			cache_flush_segment(va);
		} else {
			flush_by_page = 1;
		}
	}
#endif

	/* Invalidate the PTEs in the given range. */
	for (va = sva; va < eva; va += NBPG) {
		pte = get_pte(va);
		if (pte & PG_VALID) {
#ifdef	HAVECACHE
			if (flush_by_page) {
				cache_flush_page(va);
				/* Get fresh mod/ref bits from write-back. */
				pte = get_pte(va);
			}
#endif
			if (IS_MAIN_MEM(pte)) {
				save_modref_bits(pte);
				pv_unlink(pmap, PG_PA(pte), va);
			}
#ifdef	PMAP_DEBUG
			if ((pmap_debug & PMD_SETPTE) || (va == pmap_db_watchva)) {
				printf("pmap: set_pte pmap=%x va=%x old=%x new=%x (rrmmu)\n",
					   pmap, va, pte, PG_INVAL);
			}
#endif
			set_pte(va, PG_INVAL);
			pmegp->pmeg_vpages--;
		}
	}

	if (pmegp->pmeg_vpages <= 0) {
		/* We are done with this pmeg. */
		if (is_pmeg_wired(pmegp)) {
#ifdef	PMAP_DEBUG
			if (pmap_debug & PMD_WIRING) {
				printf("pmap: removing wired pmeg: 0x%x\n", pmegp);
				Debugger(); /* XXX */
			}
#endif	/* PMAP_DEBUG */
		}

		/* First, remove it from the MMU. */
		if (kernel_pmap == pmap) {
			/* Did cache flush above. */
			set_segmap_allctx(sva, SEGINV);
		} else {
#ifdef	PMAP_DEBUG
			if (pmap_debug & PMD_SEGMAP) {
				printf("pmap: set_segmap ctx=%d v=%x old=%x new=ff (rm2)\n",
					   get_context(), sun3_trunc_seg(sva),
					   pmegp->pmeg_index);
			}
#endif
			/* Did cache flush above. */
			set_segmap(sva, SEGINV);
			pmap->pm_segmap[VA_SEGNUM(sva)] = SEGINV;
		}
		/* Now, put it on the free list. */
		pmeg_free(pmegp, VA_SEGNUM(sva));
	}
}

void
pmap_remove_range_noctx(pmap, sva, eva)
	pmap_t pmap;
	vm_offset_t sva, eva;
{
	int pte, sme, ptenum, segnum;
	vm_offset_t va;
	pmeg_t pmegp;

#ifdef	PMAP_DEBUG
	if ((pmap_debug & PMD_REMOVE) ||
		((sva <= pmap_db_watchva && eva > pmap_db_watchva)))
		printf("pmap_remove_range_noctx(%x, %x, %x)\n", pmap, sva, eva);
#endif

	/* Interrupt level handled by caller. */
	CHECK_SPL();

#ifdef	PMAP_DEBUG
	/* Kernel always in a context (actually, in all contexts). */
	if (pmap == kernel_pmap)
		panic("pmap_remove_range_noctx: kernel_pmap");
	if (pmap->pm_segmap == NULL)
		panic("pmap_remove_range_noctx: null segmap");
#endif

	segnum = VA_SEGNUM(sva);
	sme = pmap->pm_segmap[segnum];
	pmegp = pmeg_p(sme);

	for (va = sva; va < eva; va += NBPG) {
		ptenum = VA_PTE_NUM(va);
		pte = get_pte_pmeg(pmegp->pmeg_index, ptenum);
		if (pte & PG_VALID) {
			if (IS_MAIN_MEM(pte)) {
				save_modref_bits(pte);
				pv_unlink(pmap, PG_PA(pte), va);
			}
#ifdef	PMAP_DEBUG
			if ((pmap_debug & PMD_SETPTE) || (va == pmap_db_watchva)) {
				printf("pmap: set_pte pmap=%x va=%x old=%x new=%x (rrncx)\n",
					   pmap, va, pte, PG_INVAL);
			}
#endif
			set_pte_pmeg(pmegp->pmeg_index, ptenum, PG_INVAL);
			pmegp->pmeg_vpages--;
		}
	}
	if (pmegp->pmeg_vpages <= 0) {
		if (is_pmeg_wired(pmegp))
			panic("pmap: removing wired");

		pmap->pm_segmap[segnum] = SEGINV;
		pmeg_free(pmegp, segnum);
	}
}

/*
 * guaraunteed to be within one segment
 */
void
pmap_remove_range(pmap, sva, eva)
	pmap_t pmap;
	vm_offset_t sva, eva;
{
	int c, s, sme;
	int old_ctx;
	boolean_t in_ctx;

	PMAP_LOCK();

#ifdef	DIAGNOSTIC
	if (sun3_trunc_seg(sva) != sun3_trunc_seg(eva-NBPG))
		panic("pmap_remove_range: bad range!");
#endif

	/* cases: kernel: always has context, always available
	 *
	 *		user: has context, is available
	 *		user: has no context, is available
	 *		user: has no context, is not available (NOTHING) |_ together
	 *		user: has context, isn't available (NOTHING)	 |
	 */

	if (pmap == kernel_pmap) {
		sme = get_segmap(sva);
		if (sme != SEGINV)
			pmap_remove_range_mmu(pmap, sva, eva);
	} else {
		/* It is a user pmap. */
		if (pmap->pm_segmap[VA_SEGNUM(sva)] != SEGINV) {
			/* There is a PMEG, but maybe not active. */
			old_ctx = CTXINVAL;
			in_ctx = FALSE;
			if (has_context(pmap)) {
				old_ctx = get_context();
				set_context(pmap->pm_ctxnum);
				sme = get_segmap(sva);
				if (sme != SEGINV)
					in_ctx = TRUE;
			}
			if (in_ctx == TRUE) {
				/*
				 * The PMEG is in the current context.
				 */
				pmap_remove_range_mmu(pmap, sva, eva);
			} else {
				/*
				 * There is a PMEG to deal with,
				 * but it is not active.
				 */
				pmap_remove_range_noctx(pmap, sva, eva);
			}
			if (old_ctx != CTXINVAL) {
				set_context(old_ctx);
			}
		}
	}

	PMAP_UNLOCK();
	return;
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
	vm_offset_t sva, eva;
{
	register vm_offset_t va, neva;

	if (pmap == NULL)
		return;

	if (pmap == kernel_pmap) {
		if (sva < VM_MIN_KERNEL_ADDRESS)
			sva = VM_MIN_KERNEL_ADDRESS;
		if (eva > DVMA_SPACE_END) {
#ifdef	PMAP_DEBUG
			printf("pmap_remove: eva=0x%x\n", eva);
			Debugger();
#endif
			eva = DVMA_SPACE_END;
		}
	} else {
		if (eva > VM_MAXUSER_ADDRESS)
			eva = VM_MAXUSER_ADDRESS;
	}

	va = sva;
	while (va < eva) {
		neva = sun3_trunc_seg(va) + NBSG;
		if (neva > eva)
			neva = eva;
		pmap_remove_range(pmap, va, neva);
		va = neva;
	}
}

static void
pmap_enter_kernel(va, pa, prot, wired, new_pte)
	vm_offset_t va;
	vm_offset_t pa;
	vm_prot_t prot;
	boolean_t wired;
	int new_pte;
{
	int s, i, c;
	int sme, old_pte;
	int nflags, do_pv;
	vm_offset_t seg_va;
	pmeg_t pmegp;

	/*
	  keep in hardware only, since its mapped into all contexts anyway;
	  need to handle possibly allocating additional pmegs
	  need to make sure they cant be stolen from the kernel;
	  map any new pmegs into all contexts, make sure rest of pmeg is null;
	  deal with pv_stuff; possibly caching problems;
	  must also deal with changes too.
	  */

	/*
	 * In detail:
	 *
	 * (a) lock pmap
	 * (b) Is the VA in a already mapped segment, if so
	 *	 look to see if that VA address is "valid".  If it is, then
	 *	 action is a change to an existing pte
	 * (c) if not mapped segment, need to allocate pmeg
	 * (d) if adding pte entry or changing physaddr of existing one,
	 *		use pv_stuff, for change, pmap_remove() possibly.
	 * (e) change/add pte
	 */

#ifdef PMAP_DEBUG
	if (va < virtual_avail) {
		printf("pmap_enter_kernel: va=0x%x < virtual_avail\n", va);
		Debugger();
	}
#endif
#ifdef	DIAGNOSTIC
	if ((va < VM_MIN_KERNEL_ADDRESS) || (va >= DVMA_SPACE_END))
		panic("pmap_enter_kernel: bad va=0x%x", va);
	if ((new_pte & (PG_VALID | PG_SYSTEM)) != (PG_VALID | PG_SYSTEM))
		panic("pmap_enter_kernel: bad pte");
#endif

	if (va >= DVMA_SPACE_START) {
		/* This is DVMA space.  Always want it non-cached. */
		new_pte |= PG_NC;
	}

	seg_va = sun3_trunc_seg(va);
	do_pv = TRUE;

	PMAP_LOCK();

	sme = get_segmap(va);
	if (sme == SEGINV) {
		pmegp = pmeg_allocate(kernel_pmap, sun3_trunc_seg(va));
		sme = pmegp->pmeg_index;
		set_segmap_allctx(va, sme);
#ifdef PMAP_DEBUG
		if (pmap_debug & PMD_SEGMAP) {
			printf("pmap: set_segmap pmap=%x va=%x sme=%x (ek1)\n",
				   kernel_pmap, seg_va, sme);
		}
		pmeg_verify_empty(sun3_trunc_seg(va));
#endif
		/* There are no existing mappings to deal with. */
		goto add_pte;
	}

	/* Found an existing pmeg.  Modify it... */
	pmegp = pmeg_p(sme);
#ifdef	DIAGNOSTIC
	/* Make sure it is ours. */
	if (pmegp->pmeg_owner != kernel_pmap)
		panic("pmap_enter_kernel: MMU has bad pmeg %x", sme);
#endif

	/*
	 * Found existing PMEG.  Does mapping already exist?
	 *	(a) if so, is it same pa then really a protection change
	 *	(b) if not same, pa then we have to unlink from old pa
	 *	(c)
	 */
	old_pte = get_pte(va);
	if ((old_pte & PG_VALID) == 0)
		goto add_pte;

	/* Have valid translation.  Flush cache before changing it. */
#ifdef	HAVECACHE
	if (cache_size) {
		cache_flush_page(va);
		/* Get fresh mod/ref bits from write-back. */
		old_pte = get_pte(va);
	}
#endif

	/* XXX - removing valid page here, way lame... -glass */
	pmegp->pmeg_vpages--;

	if (!IS_MAIN_MEM(old_pte)) {
		/* Was not main memory, so no pv_entry for it. */
		goto add_pte;
	}

	/* Old mapping was main memory.  Save mod/ref bits. */
	save_modref_bits(old_pte);

	/*
	 * If not changing the type or pfnum then re-use pv_entry.
	 * Note we get here only with old_pte having PGT_OBMEM.
	 */
	if ((old_pte & (PG_TYPE|PG_FRAME)) ==
		(new_pte & (PG_TYPE|PG_FRAME)) )
	{
		do_pv = FALSE;		/* re-use pv_entry */
		new_pte |= (old_pte & PG_NC);
		goto add_pte;
	}

	/* OK, different type or PA, have to kill old pv_entry. */
	pv_unlink(kernel_pmap, PG_PA(old_pte), va);

 add_pte:	/* can be destructive */
	pmeg_set_wiring(pmegp, va, wired);

	/* Anything but MAIN_MEM is mapped non-cached. */
	if (!IS_MAIN_MEM(new_pte)) {
		new_pte |= PG_NC;
		do_pv = FALSE;
	}
	if (do_pv) {
		nflags = pv_link(kernel_pmap, pa, va,
						 PG_TO_PV_FLAGS(new_pte & PG_NC));
		if (nflags & PV_NC)
			new_pte |= PG_NC;
	}
#ifdef	PMAP_DEBUG
	if ((pmap_debug & PMD_SETPTE) || (va == pmap_db_watchva)) {
		printf("pmap: set_pte pmap=%x va=%x old=%x new=%x (ek)\n",
			   kernel_pmap, va, old_pte, new_pte);
	}
#endif
	/* cache flush done above */
	set_pte(va, new_pte);
	pmegp->pmeg_vpages++;

	PMAP_UNLOCK();
}


void
pmap_enter_user(pmap, va, pa, prot, wired, new_pte)
	pmap_t pmap;
	vm_offset_t va;
	vm_offset_t pa;
	vm_prot_t prot;
	boolean_t wired;
	int new_pte;
{
	int s, old_ctx;
	int sme, segnum, old_pte;
	int nflags, do_pv;
	vm_offset_t seg_va;
	pmeg_t pmegp;

#ifdef	PMAP_DEBUG
	/*
	 * Some user pages are wired here, and a later
	 * call to pmap_change_wiring() will unwire them.
	 * XXX - Need a separate list for wired user pmegs
	 * so they can not be stolen from the active list.
	 * XXX - Note: vm_fault.c assumes pmap_extract will
	 * work on wired mappings, so must preserve them...
	 */
	if (wired && (pmap_debug & PMD_WIRING)) {
		printf("pmap_enter_user: attempt to wire user page, ignored\n");
		printf("pmap=0x%x va=0x%x pa=0x%x\n", pmap, va, pa);
	}
#endif
#ifdef	DIAGNOSTIC
	if (va >= VM_MAXUSER_ADDRESS)
		panic("pmap_enter_user: bad va=0x%x", va);
	if ((new_pte & (PG_VALID | PG_SYSTEM)) != PG_VALID)
		panic("pmap_enter_user: bad pte");
#endif

	segnum = VA_SEGNUM(va);
	seg_va = sun3_trunc_seg(va);
	do_pv = TRUE;

	PMAP_LOCK();

	/*
	 * Make sure the current context is correct.
	 * Even though we call pmap_activate when we switch
	 * to a new process, the VM system occasionally will
	 * activate enough other pmaps that we can loose our
	 * own context and have to reallocate one here.
	 */
	old_ctx = get_context();
	if (old_ctx != pmap->pm_ctxnum) {
#ifdef	PMAP_DEBUG
		if (pmap_debug & PMD_SWITCH) {
			printf("pmap_enter_user: wrong context\n");
			Debugger();
		}
#endif
		if (!has_context(pmap)) {
#ifdef	PMAP_DEBUG
			if (pmap_debug & PMD_SWITCH) {
				printf("pmap_enter_user: pmap without context\n");
				Debugger();
			}
#endif
			context_allocate(pmap);
		}
		set_context(pmap->pm_ctxnum);
	}

	/*
	 * Have correct context, now get a PMEG.
	 */
	if (pmap->pm_segmap[segnum] == SEGINV) {
		/* Not in cache; need to allocate a new PMEG. */
#ifdef	PMAP_DEBUG
		/* should not be in hardware */
		sme = get_segmap(va);
		if (sme != SEGINV)
			panic("pmap_enter_user: unknown sme at VA=0x%x", va);
#endif
		/* This will get us an "active" PMEG */
		pmegp = pmeg_allocate(pmap, seg_va);
		sme = pmegp->pmeg_index;
		pmap->pm_segmap[segnum] = sme;
		set_segmap(va, sme);
#ifdef	PMAP_DEBUG
		if (pmap_debug & PMD_SEGMAP) {
			printf("pmap: set_segmap pmap=%x va=%x sme=%x (eu1)\n",
				   pmap, seg_va, sme);
		}
		pmeg_verify_empty(seg_va);
#endif
		/* There are no existing mappings to deal with. */
		goto add_pte;
	}

	/* There is a PMEG but it might be inactive. */
	sme = get_segmap(va);
	if (sme != SEGINV) {
		/* It is already active. */
		pmegp = pmeg_p(sme);
#ifdef	PMAP_DEBUG
		/* Make sure it is the right PMEG. */
		if (sme != pmap->pm_segmap[segnum])
			panic("pmap_enter_user: wrong sme at VA=0x%x", seg_va);
#endif
	} else {
		/* Make the PMEG active. */
		pmegp = pmeg_cache(pmap, seg_va);
		if (pmegp == NULL)
			panic("pmap_enter_user: pmeg_cache lost");
		/* found cached pmeg - just reinstall in segmap */
		sme = pmegp->pmeg_index;
		set_segmap(va, sme);
#ifdef	PMAP_DEBUG
		if (pmap_debug & PMD_SEGMAP) {
			printf("pmap: set_segmap pmap=%x va=%x sme=%x (eu2)\n",
				   pmap, seg_va, sme);
		}
#endif
	}

	/*
	 * Found existing PMEG.  Does mapping already exist?
	 *	(a) if so, is it same pa then really a protection change
	 *	(b) if not same, pa then we have to unlink from old pa
	 *	(c)
	 */
	old_pte = get_pte(va);
	if ((old_pte & PG_VALID) == 0)
		goto add_pte;

	/* Have valid translation.  Flush cache before changing it. */
#ifdef	HAVECACHE
	if (cache_size) {
		cache_flush_page(va);
		/* Get fresh mod/ref bits from write-back. */
		old_pte = get_pte(va);
	}
#endif

	/* XXX - removing valid page here, way lame... -glass */
	pmegp->pmeg_vpages--;

	if (!IS_MAIN_MEM(old_pte)) {
		/* Was not main memory, so no pv_entry for it. */
		goto add_pte;
	}

	/* Old mapping was main memory.  Save mod/ref bits. */
	save_modref_bits(old_pte);

	/*
	 * If not changing the type or pfnum then re-use pv_entry.
	 * Note we get here only with old_pte having PGT_OBMEM.
	 */
	if ((old_pte & (PG_TYPE|PG_FRAME)) ==
		(new_pte & (PG_TYPE|PG_FRAME)) )
	{
		do_pv = FALSE;		/* re-use pv_entry */
		new_pte |= (old_pte & PG_NC);
		goto add_pte;
	}

	/* OK, different type or PA, have to kill old pv_entry. */
	pv_unlink(pmap, PG_PA(old_pte), va);

 add_pte:
	/* XXX - Wiring changes on user pmaps? */
	/* pmeg_set_wiring(pmegp, va, wired); */

	/* Anything but RAM is mapped non-cached. */
	if (!IS_MAIN_MEM(new_pte)) {
		new_pte |= PG_NC;
		do_pv = FALSE;
	}
	if (do_pv) {
		nflags = pv_link(pmap, pa, va,
						 PG_TO_PV_FLAGS(new_pte & PG_NC));
		if (nflags & PV_NC)
			new_pte |= PG_NC;
	}
#ifdef	PMAP_DEBUG
	if ((pmap_debug & PMD_SETPTE) || (va == pmap_db_watchva)) {
		printf("pmap: set_pte pmap=%x va=%x old=%x new=%x (eu)\n",
			   pmap, va, old_pte, new_pte);
	}
#endif
	/* cache flush done above */
	set_pte(va, new_pte);
	pmegp->pmeg_vpages++;

	set_context(old_ctx);
	PMAP_UNLOCK();
}

/*
 *	Insert the given physical page (p) at
 *	the specified virtual address (v) in the
 *	target physical map with the protection requested.
 *
 *	The physical address is page aligned, but may have some
 *	low bits set indicating an OBIO or VME bus page, or just
 *	that the non-cache bit should be set (i.e PMAP_NC).
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
	pmap_t pmap;
	vm_offset_t va;
	vm_offset_t pa;
	vm_prot_t prot;
	boolean_t wired;
{
	int pte_proto;
	int s;

	if (pmap == NULL)
		return;
#ifdef	PMAP_DEBUG
	if ((pmap_debug & PMD_ENTER) ||
		(va == pmap_db_watchva))
		printf("pmap_enter(%x, %x, %x, %x, %x)\n",
			   pmap, va, pa, prot, wired);
#endif

	/* Get page-type bits from low part of the PA... */
	pte_proto = (pa & PMAP_SPEC) << PG_MOD_SHIFT;

	/* ...now the valid and writable bits... */
	pte_proto |= (PG_VALID | pmap_pte_prot(prot));

	/* ...and finally the page-frame number. */
	pte_proto |= PA_PGNUM(pa);

	/* Remove spec bits from pa (now in pte_proto) */
	pa &= ~PMAP_SPEC;

	/*
	 * treatment varies significantly:
	 *  kernel ptes are in all contexts, and are always in the mmu
	 *  user ptes may not necessarily? be in the mmu.  pmap may not
	 *   be in the mmu either.
	 *
	 */
	PMAP_LOCK();
	if (pmap == kernel_pmap) {
		/* This can be called recursively through malloc. */
		pte_proto |= PG_SYSTEM;
		pmap_enter_kernel(va, pa, prot, wired, pte_proto);
	} else {
		pmap_enter_user(pmap, va, pa, prot, wired, pte_proto);
	}
	PMAP_UNLOCK();
}

/*
 * This is a shortcut used by the trap handler to
 * reload PMEGs into a user segmap without calling
 * the actual VM fault handler.  Returns TRUE if:
 *	the PMEG was reloaded, and
 *	it has a valid PTE at va.
 * Otherwise return zero and let VM code handle it.
 */
int pmap_fault_reload(pmap, va, ftype)
	pmap_t pmap;
	vm_offset_t va;
	vm_prot_t ftype;
{
	int rv, s, pte, chkpte, sme, segnum, ctx;
	vm_offset_t seg_va;
	pmeg_t pmegp;

#ifdef	PMAP_DEBUG
	if (pmap == kernel_pmap)
		panic("pmap_fault_reload: kernel_pmap");
#endif
	if (pmap->pm_segmap == NULL) {
#ifdef	PMAP_DEBUG
		printf("pmap_fault_reload: null segmap\n");
		Debugger();
#endif
		return (0);
	}
	if (va >= VM_MAXUSER_ADDRESS)
		return (0);
	seg_va = sun3_trunc_seg(va);

	/* Make sure context is correct. */
	ctx = get_context();
	if (ctx != pmap->pm_ctxnum) {
#ifdef	PMAP_DEBUG
		printf("pmap_fault_reload: wrong context\n");
		Debugger();
#endif
		/* XXX - What to do... */
		return (0);
	}

	chkpte = PG_VALID;
	if (ftype & VM_PROT_WRITE)
		chkpte |= PG_WRITE;

	PMAP_LOCK();

	rv = 0;
	sme = get_segmap(seg_va);
	if (sme == SEGINV) {
		/* See if there is something to reload. */
		pmegp = pmeg_cache(pmap, seg_va);
		if (pmegp) {
			/* Found one!  OK, reload it. */
			pmap_stats.ps_pmeg_faultin++;
			sme = pmegp->pmeg_index;
			set_segmap(seg_va, sme);
			pte = get_pte(va);
			if (pte & chkpte)
				rv = 1;
		}
	}
	
	PMAP_UNLOCK();
	return (0);
}


/*
 * Clear the modify bit for the given physical page.
 */
void
pmap_clear_modify(pa)
	register vm_offset_t pa;
{
	register pv_entry_t	pvhead;

	if (!pv_initialized)
		return;
	if (!managed(pa))
		return;

	pvhead = pa_to_pvp(pa);
	pv_syncflags(pvhead);
	pvhead->pv_flags &= ~PV_MOD;
}

/*
 * Tell whether the given physical page has been modified.
 */
int
pmap_is_modified(pa)
	register vm_offset_t pa;
{
	register pv_entry_t	pvhead;

	if (!pv_initialized)
		return (0);
	if (!managed(pa))
		return (0);
	pvhead = pa_to_pvp(pa);
	if ((pvhead->pv_flags & PV_MOD) == 0)
		pv_syncflags(pvhead);
	return (pvhead->pv_flags & PV_MOD);
}

/*
 * Clear the reference bit for the given physical page.
 * It's OK to just remove mappings if that's easier.
 */
void
pmap_clear_reference(pa)
	register vm_offset_t pa;
{
	register pv_entry_t	pvhead;

	if (!pv_initialized)
		return;
	if (!managed(pa))
		return;

	pvhead = pa_to_pvp(pa);
	pv_syncflags(pvhead);
	pvhead->pv_flags &= ~PV_REF;
}

/*
 * Tell whether the given physical page has been referenced.
 * It's OK to just return FALSE if page is not mapped.
 */
int
pmap_is_referenced(pa)
	vm_offset_t	pa;
{
	register pv_entry_t	pvhead;

	if (!pv_initialized)
		return (0);
	if (!managed(pa))
		return (0);
	pvhead = pa_to_pvp(pa);
	if ((pvhead->pv_flags & PV_REF) == 0)
		pv_syncflags(pvhead);
	return (pvhead->pv_flags & PV_REF);
}


void
pmap_activate(pmap, pcbp)
	pmap_t pmap;
	struct pcb *pcbp;
{
	CHECK_SPL();

	if (pmap == kernel_pmap)
		panic("pmap_activate: kernel_pmap");

	if (!has_context(pmap)) {
		context_allocate(pmap);
#ifdef PMAP_DEBUG
		if (pmap_debug & PMD_SWITCH)
			printf("pmap_activate(%x) takes context %d\n",
				   pmap, pmap->pm_ctxnum);
#endif
	}

#ifdef	PMAP_DEBUG
	if (pmap_debug & PMD_SWITCH) {
		int old_ctx = get_context();
		if (old_ctx != pmap->pm_ctxnum) {
			printf("pmap_activate(%x) old_ctx=%d new_ctx=%d\n",
				   pmap, old_ctx, pmap->pm_ctxnum);
		}
	}
#endif

	set_context(pmap->pm_ctxnum);
}

void
pmap_deactivate(pmap, pcbp)
	pmap_t pmap;
	struct pcb *pcbp;
{
#ifdef PMAP_DEBUG
	if (pmap_debug & PMD_SWITCH)
		printf("pmap_deactivate(%x, %x)\n", pmap, pcbp);
#endif
	/* Nothing to do really, and not called anyway... */
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
	pmap_t	pmap;
	vm_offset_t	va;
	boolean_t	wired;
{
	int s, sme;
	int wiremask, ptenum;
	pmeg_t pmegp;

	if (pmap == NULL)
		return;
#ifdef PMAP_DEBUG
	if (pmap_debug & PMD_WIRING)
		printf("pmap_change_wiring(pmap=0x%x, va=0x%x, wire=%d)\n",
			   pmap, va, wired);
#endif
	/*
	 * We are asked to unwire pages that were wired when
	 * pmap_enter() was called and we ignored wiring.
	 * (VM code appears to wire a stack page during fork.)
	 */
	if (pmap != kernel_pmap) {
#ifdef PMAP_DEBUG
		if (pmap_debug & PMD_WIRING)
			printf("  (user pmap -- ignored)\n");
#endif
		return;
	}

	ptenum = VA_PTE_NUM(va);
	wiremask = 1 << ptenum;

	PMAP_LOCK();

	sme = get_segmap(va);
	if (sme == SEGINV)
		panic("pmap_change_wiring: invalid va=0x%x", va);
	pmegp = pmeg_p(sme);
	if (wired)
		pmegp->pmeg_wired |= wiremask;
	else
		pmegp->pmeg_wired &= ~wiremask;
	PMAP_UNLOCK();
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
	pmap_t		dst_pmap;
	pmap_t		src_pmap;
	vm_offset_t	dst_addr;
	vm_size_t	len;
	vm_offset_t	src_addr;
{
}

/*
 *	Routine:	pmap_extract
 *	Function:
 *		Extract the physical page address associated
 *		with the given map/virtual_address pair.
 *	Returns zero if VA not valid.
 */
vm_offset_t
pmap_extract(pmap, va)
	pmap_t	pmap;
	vm_offset_t va;
{
	int s, sme, segnum, ptenum, pte;
	vm_offset_t pa;

	pte = 0;
	PMAP_LOCK();
	if (pmap == kernel_pmap) {
		sme = get_segmap(va);
		if (sme != SEGINV)
			pte = get_pte(va);
	} else {
		segnum = VA_SEGNUM(va);
		sme = pmap->pm_segmap[segnum];
		if (sme != SEGINV) {
			ptenum = VA_PTE_NUM(va);
			pte = get_pte_pmeg(sme, ptenum);
		}
	}
	PMAP_UNLOCK();
	if ((pte & PG_VALID) == 0) {
		printf("pmap_extract: invalid va=0x%x\n", va);
		Debugger();
		pte = 0;
	}
	pa = PG_PA(pte);
#ifdef	DIAGNOSTIC
	if (pte & PG_TYPE) {
		panic("pmap_extract: not main mem, va=0x%x\n", va);
	}
#endif
	return (pa);
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
	/* not implemented, hopefully not needed */
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
 * Initialize a preallocated and zeroed pmap structure,
 * such as one in a vmspace structure.
 */
void
pmap_pinit(pmap)
	pmap_t pmap;
{
	pmap_common_init(pmap);
	pmap_user_pmap_init(pmap);
}

/*
 * Remove write permissions, all in one PMEG,
 * where that PMEG is currently in the MMU.
 * The current context is already correct.
 */
void
pmap_protect_range_mmu(pmap, sva, eva)
	pmap_t pmap;
	vm_offset_t sva, eva;
{
	int old_ctx, i;
	pmeg_t pmegp;
	vm_offset_t va;
	int pte, sme;
	int nflags;
	int flush_by_page = 0;

	/* Interrupt level handled by caller. */
	CHECK_SPL();

#ifdef	DIAGNOSTIC
	if (pmap != kernel_pmap) {
		if (pmap->pm_ctxnum != get_context())
			panic("pmap_protect_range_mmu: wrong context");
	}
#endif

	va = sun3_trunc_seg(sva);
	sme = get_segmap(va);
#ifdef	DIAGNOSTIC
	/* Make sure it is valid and known. */
	if (sme == SEGINV)
		panic("pmap_protect_range_mmu: SEGINV");
	if (pmap->pm_segmap && (pmap->pm_segmap[VA_SEGNUM(sva)] != sme))
		panic("pmap_protect_range_mmu: incorrect sme, va=0x%x", va);
#endif
	pmegp = pmeg_p(sme);

	/* have pmeg, will travel */

#ifdef	DIAGNOSTIC
	/* Make sure we own the pmeg, right va, etc. */
	if ((pmegp->pmeg_va != va) ||
		(pmegp->pmeg_owner != pmap) ||
		(pmegp->pmeg_version != pmap->pm_version))
	{
		printf("pmap_protect_range_mmu: wrong pmeg, sme=0x%x\n", sme);
#ifdef	PMAP_DEBUG
		pmeg_print(pmegp);
		Debugger();	/* XXX */
#endif
		panic("bye");
	}
	if (pmegp->pmeg_vpages <= 0)
		panic("pmap_protect_range_mmu: no valid pages?");
#endif

#ifdef	HAVECACHE
	if (cache_size) {
		/*
		 * If the range to be removed is larger than the cache,
		 * it will be cheaper to flush this segment entirely.
		 */
		if (cache_size < (eva - sva)) {
			/* cheaper to flush whole segment */
			cache_flush_segment(va);
		} else {
			flush_by_page = 1;
		}
	}
#endif

	/* Remove write permission on PTEs in the given range. */
	for (va = sva; va < eva; va += NBPG) {
		pte = get_pte(va);
		if (pte & PG_VALID) {
#ifdef	HAVECACHE
			if (flush_by_page) {
				cache_flush_page(va);
				/* Get fresh mod/ref bits from write-back. */
				pte = get_pte(va);
			}
#endif
			if (IS_MAIN_MEM(pte)) {
				save_modref_bits(pte);
			}
			pte &= ~(PG_WRITE | PG_MODREF);
			set_pte(va, pte);
		}
	}
}

/*
 * Remove write permissions, all in one PMEG,
 * where it is not currently in any context.
 */
void
pmap_protect_range_noctx(pmap, sva, eva)
	pmap_t pmap;
	vm_offset_t sva, eva;
{
	int pte, sme, ptenum, segnum;
	vm_offset_t va;
	pmeg_t pmegp;
	int nflags;

	/* Interrupt level handled by caller. */
	CHECK_SPL();

#ifdef	PMAP_DEBUG
	/* Kernel always in a context (actually, in all contexts). */
	if (pmap == kernel_pmap)
		panic("pmap_protect_range_noctx: kernel_pmap");
	if (pmap->pm_segmap == NULL)
		panic("pmap_protect_range_noctx: null segmap");
#endif

	segnum = VA_SEGNUM(sva);
	sme = pmap->pm_segmap[segnum];
	pmegp = pmeg_p(sme);

	/* Remove write permission on PTEs in the given range. */
	for (va = sva; va < eva; va += NBPG) {
		ptenum = VA_PTE_NUM(va);
		pte = get_pte_pmeg(sme, ptenum);
		if (pte & PG_VALID) {
			if (IS_MAIN_MEM(pte)) {
				save_modref_bits(pte);
			}
			pte &= ~(PG_WRITE | PG_MODREF);
			set_pte_pmeg(sme, ptenum, pte);
		}
	}
}

/*
 * Remove write permissions in given range.
 * (guaranteed to be within one segment)
 * similar to pmap_remove_range()
 */
static void
pmap_protect_range(pmap, sva, eva)
	pmap_t pmap;
	vm_offset_t sva, eva;
{
	int c, s, sme;
	int old_ctx;
	boolean_t in_ctx;

	PMAP_LOCK();

#ifdef	PMAP_DEBUG
	if ((pmap_debug & PMD_PROTECT) ||
		((sva <= pmap_db_watchva && eva > pmap_db_watchva)))
		printf("pmap_protect_range(%x, %x, %x)\n", pmap, sva, eva);
#endif
#ifdef	DIAGNOSTIC
	if (sun3_trunc_seg(sva) != sun3_trunc_seg(eva-NBPG))
		panic("pmap_protect_range: bad range!");
#endif

	if (pmap == kernel_pmap) {
		sme = get_segmap(sva);
		if (sme != SEGINV)
			pmap_protect_range_mmu(pmap, sva, eva);
	} else {
		/* It is a user pmap. */
		if (pmap->pm_segmap[VA_SEGNUM(sva)] != SEGINV) {
			/* There is a PMEG, but maybe not active. */
			old_ctx = CTXINVAL;
			in_ctx = FALSE;
			if (has_context(pmap)) {
				old_ctx = get_context();
				set_context(pmap->pm_ctxnum);
				sme = get_segmap(sva);
				if (sme != SEGINV)
					in_ctx = TRUE;
			}
			if (in_ctx == TRUE) {
				/*
				 * The PMEG is in the current context.
				 */
				pmap_protect_range_mmu(pmap, sva, eva);
			} else {
				/*
				 * There is a PMEG to deal with,
				 * but it is not active.
				 */
				pmap_protect_range_noctx(pmap, sva, eva);
			}
			if (old_ctx != CTXINVAL) {
				set_context(old_ctx);
			}
		}
	}

	PMAP_UNLOCK();
	return;
}

/*
 *	Reduce the permissions on the specified
 *	range of this map as requested.
 *	(Make pages read-only.)
 */
void
pmap_protect(pmap, sva, eva, prot)
	pmap_t	pmap;
	vm_offset_t	sva, eva;
	vm_prot_t	prot;
{
	vm_offset_t va, neva;

#ifdef	PMAP_DEBUG
	if (pmap_debug & PMD_PROTECT)
		printf("pmap_protect(%x, %x, %x, %x)\n", pmap, sva, eva, prot);
#endif

	if (pmap == NULL)
		return;

	/* If removing all permissions, just unmap. */
	if ((prot & VM_PROT_READ) == 0) {
		pmap_remove(pmap, sva, eva);
		return;
	}

	/* If leaving writable, nothing to do. */
	if (prot & VM_PROT_WRITE) {
		return;
	}

	if (pmap == kernel_pmap) {
		if (sva < VM_MIN_KERNEL_ADDRESS)
			sva = VM_MIN_KERNEL_ADDRESS;
		if (eva > DVMA_SPACE_END) {
#ifdef	PMAP_DEBUG
			printf("pmap_protect: eva=0x%x\n", eva);
			Debugger();
#endif
			eva = DVMA_SPACE_END;
		}
	}
	else {
		if (eva > VM_MAX_ADDRESS)
			eva = VM_MAX_ADDRESS;
	}

	va = sva;
	while (va < eva) {
		neva = sun3_trunc_seg(va) + NBSG;
		if (neva > eva)
			neva = eva;
		pmap_protect_range(pmap, va, neva);
		va = neva;
	}
}

/*
 * Count pages resident in this pmap.
 * XXX - Should be called: pmap_resident_count()
 * but that has to be a macro (see kern_sysctl.c)
 */
segsz_t
pmap_resident_pages(pmap)
	pmap_t pmap;
{
	int i, sme, pages;
	pmeg_t pmeg;

	pages = 0;
	if (pmap->pm_segmap) {
		for (i = 0; i < NUSEG; i++) {
			sme = pmap->pm_segmap[i];
			if (sme != SEGINV) {
				pmeg = pmeg_p(sme);
				pages += pmeg->pmeg_vpages;
			}
		}
	}
	return (pages);
}


/*
 *	Require that all active physical maps contain no
 *	incorrect entries NOW.  [This update includes
 *	forcing updates of any address map caching.]
 *
 *	Generally used to insure that a thread about
 *	to run will see a semantically correct world.
 */
void
pmap_update()
{
}

/*
 *	pmap_copy_page copies the specified (machine independent)
 *	page by mapping the page into virtual memory and using
 *	bcopy to copy the page, one machine dependent page at a
 *	time.
 */
void
pmap_copy_page(src, dst)
	vm_offset_t	src, dst;
{
	int pte;
	int s;

#ifdef	PMAP_DEBUG
	if (pmap_debug & PMD_COW)
		printf("pmap_copy_page: %x -> %x\n", src, dst);
#endif
	PMAP_LOCK();

	if (tmp_vpages_inuse)
		panic("pmap_copy_page: vpages inuse");
	tmp_vpages_inuse++;

	/* PG_PERM is short for (PG_VALID|PG_WRITE|PG_SYSTEM|PG_NC) */
	/* All mappings to vmp_vpages are non-cached, so no flush. */
	pte = PG_PERM | PA_PGNUM(src);
	set_pte(tmp_vpages[0], pte);
	pte = PG_PERM | PA_PGNUM(dst);
	set_pte(tmp_vpages[1], pte);
	bcopy((char *) tmp_vpages[0], (char *) tmp_vpages[1], NBPG);
	set_pte(tmp_vpages[0], PG_INVAL);
	set_pte(tmp_vpages[0], PG_INVAL);

	tmp_vpages_inuse--;
	PMAP_UNLOCK();
}

/*
 *	pmap_zero_page zeros the specified (machine independent)
 *	page by mapping the page into virtual memory and using
 *	bzero to clear its contents, one machine dependent page
 *	at a time.
 */
void
pmap_zero_page(pa)
	vm_offset_t	pa;
{
	int pte;
	int s;

#ifdef	PMAP_DEBUG
	if (pmap_debug & PMD_COW)
		printf("pmap_zero_page: %x\n", pa);
#endif
	PMAP_LOCK();

	if (tmp_vpages_inuse)
		panic("pmap_zero_page: vpages inuse");
	tmp_vpages_inuse++;

	/* PG_PERM is short for (PG_VALID|PG_WRITE|PG_SYSTEM|PG_NC) */
	/* All mappings to vmp_vpages are non-cached, so no flush. */
	pte = PG_PERM | PA_PGNUM(pa);
	set_pte(tmp_vpages[0], pte);
	bzero((char *) tmp_vpages[0], NBPG);
	set_pte(tmp_vpages[0], PG_INVAL);

	tmp_vpages_inuse--;
	PMAP_UNLOCK();
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
	pmap_t pmap;
{
}

/*
 * Find first virtual address >= *va that is
 * least likely to cause cache aliases.
 * (This will just seg-align mappings.)
 */
void
pmap_prefer(fo, va)
	register vm_offset_t fo;
	register vm_offset_t *va;
{
	register long	d;

	d = fo - *va;
	d &= SEGOFSET;
	*va += d;
}

/*
 * Helper functions for changing unloaded PMEGs
 */
static int temp_seg_inuse;

static int
get_pte_pmeg(int pmeg_num, int page_num)
{
	vm_offset_t va;
	int pte;

	CHECK_SPL();
	if (temp_seg_inuse)
		panic("get_pte_pmeg: temp_seg_inuse");
	temp_seg_inuse++;

	va = temp_seg_va;
	set_segmap(temp_seg_va, pmeg_num);
	va += NBPG*page_num;
	pte = get_pte(va);
	set_segmap(temp_seg_va, SEGINV);

	temp_seg_inuse--;
	return pte;
}

static void
set_pte_pmeg(int pmeg_num, int page_num, int pte)
{
	vm_offset_t va;

	CHECK_SPL();
	if (temp_seg_inuse)
		panic("set_pte_pmeg: temp_seg_inuse");
	temp_seg_inuse++;

	/* We never access data in temp_seg_va so no need to flush. */
	va = temp_seg_va;
	set_segmap(temp_seg_va, pmeg_num);
	va += NBPG*page_num;
	set_pte(va, pte);
	set_segmap(temp_seg_va, SEGINV);

	temp_seg_inuse--;
}

/*
 * Local Variables:
 * tab-width: 4
 * End:
 */
