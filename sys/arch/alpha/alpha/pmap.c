/* $OpenBSD: pmap.c,v 1.37 2003/02/18 13:14:43 jmc Exp $ */
/* $NetBSD: pmap.c,v 1.154 2000/12/07 22:18:55 thorpej Exp $ */

/*-
 * Copyright (c) 1998, 1999, 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center and by Chris G. Demetriou.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

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
 * DEC Alpha physical map management code.
 *
 * History:
 *
 *	This pmap started life as a Motorola 68851/68030 pmap,
 *	written by Mike Hibler at the University of Utah.
 *
 *	It was modified for the DEC Alpha by Chris Demetriou
 *	at Carnegie Mellon University.
 *
 *	Support for non-contiguous physical memory was added by
 *	Jason R. Thorpe of the Numerical Aerospace Simulation
 *	Facility, NASA Ames Research Center and Chris Demetriou.
 *
 *	Page table management and a major cleanup were undertaken
 *	by Jason R. Thorpe, with lots of help from Ross Harvey of
 *	Avalon Computer Systems and from Chris Demetriou.
 *
 *	Support for the new UVM pmap interface was written by
 *	Jason R. Thorpe.
 *
 *	Support for ASNs was written by Jason R. Thorpe, again
 *	with help from Chris Demetriou and Ross Harvey.
 *
 *	The locking protocol was written by Jason R. Thorpe,
 *	using Chuck Cranor's i386 pmap for UVM as a model.
 *
 *	TLB shootdown code was written by Jason R. Thorpe.
 *
 * Notes:
 *
 *	All page table access is done via K0SEG.  The one exception
 *	to this is for kernel mappings.  Since all kernel page
 *	tables are pre-allocated, we can use the Virtual Page Table
 *	to access PTEs that map K1SEG addresses.
 *
 *	Kernel page table pages are statically allocated in
 *	pmap_bootstrap(), and are never freed.  In the future,
 *	support for dynamically adding additional kernel page
 *	table pages may be added.  User page table pages are
 *	dynamically allocated and freed.
 *
 *	This pmap implementation only supports NBPG == PAGE_SIZE.
 *	In practice, this is not a problem since PAGE_SIZE is
 *	initialized to the hardware page size in alpha_init().
 *
 * Bugs/misfeatures:
 *
 *	- Some things could be optimized.
 */

/*
 *	Manages physical address maps.
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
#include <sys/pool.h>
#include <sys/user.h>
#include <sys/buf.h>
#ifdef SYSVSHM
#include <sys/shm.h>
#endif

#include <uvm/uvm.h>

#include <machine/atomic.h>
#include <machine/cpu.h>
#if defined(_PMAP_MAY_USE_PROM_CONSOLE) || defined(MULTIPROCESSOR)
#include <machine/rpb.h>
#endif

#ifdef DEBUG
#define	PDB_FOLLOW	0x0001
#define	PDB_INIT	0x0002
#define	PDB_ENTER	0x0004
#define	PDB_REMOVE	0x0008
#define	PDB_CREATE	0x0010
#define	PDB_PTPAGE	0x0020
#define	PDB_ASN		0x0040
#define	PDB_BITS	0x0080
#define	PDB_COLLECT	0x0100
#define	PDB_PROTECT	0x0200
#define	PDB_BOOTSTRAP	0x1000
#define	PDB_PARANOIA	0x2000
#define	PDB_WIRING	0x4000
#define	PDB_PVDUMP	0x8000

int debugmap = 0;
int pmapdebug = PDB_PARANOIA|PDB_FOLLOW|PDB_ENTER;
#endif

/*
 * Given a map and a machine independent protection code,
 * convert to an alpha protection code.
 */
#define pte_prot(m, p)	(protection_codes[m == pmap_kernel() ? 0 : 1][p])
int	protection_codes[2][8];

/*
 * kernel_lev1map:
 *
 *	Kernel level 1 page table.  This maps all kernel level 2
 *	page table pages, and is used as a template for all user
 *	pmap level 1 page tables.  When a new user level 1 page
 *	table is allocated, all kernel_lev1map PTEs for kernel
 *	addresses are copied to the new map.
 *
 *	The kernel also has an initial set of kernel level 2 page
 *	table pages.  These map the kernel level 3 page table pages.
 *	As kernel level 3 page table pages are added, more level 2
 *	page table pages may be added to map them.  These pages are
 *	never freed.
 *
 *	Finally, the kernel also has an initial set of kernel level
 *	3 page table pages.  These map pages in K1SEG.  More level
 *	3 page table pages may be added at run-time if additional
 *	K1SEG address space is required.  These pages are never freed.
 *
 * NOTE: When mappings are inserted into the kernel pmap, all
 * level 2 and level 3 page table pages must already be allocated
 * and mapped into the parent page table.
 */
pt_entry_t	*kernel_lev1map;

/*
 * Virtual Page Table.
 */
pt_entry_t	*VPT;

struct pmap	kernel_pmap_store;
u_int		kernel_pmap_asn_store[ALPHA_MAXPROCS];
u_long		kernel_pmap_asngen_store[ALPHA_MAXPROCS];

paddr_t    	avail_start;	/* PA of first available physical page */
paddr_t		avail_end;	/* PA of last available physical page */
vaddr_t		virtual_avail;  /* VA of first avail page (after kernel bss)*/
vaddr_t		virtual_end;	/* VA of last avail page (end of kernel AS) */

boolean_t	pmap_initialized;	/* Has pmap_init completed? */

u_long		pmap_pages_stolen;	/* instrumentation */

/*
 * This variable contains the number of CPU IDs we need to allocate
 * space for when allocating the pmap structure.  It is used to
 * size a per-CPU array of ASN and ASN Generation number.
 */
u_long		pmap_ncpuids;

/*
 * Storage for physical->virtual entries and page attributes.
 */
struct pv_head	*pv_table;
int		pv_table_npages;

#ifndef PMAP_PV_LOWAT
#define	PMAP_PV_LOWAT	16
#endif
int		pmap_pv_lowat = PMAP_PV_LOWAT;

/*
 * List of all pmaps, used to update them when e.g. additional kernel
 * page tables are allocated.  This list is kept LRU-ordered by
 * pmap_activate().
 */
TAILQ_HEAD(, pmap) pmap_all_pmaps;

/*
 * The pools from which pmap structures and sub-structures are allocated.
 */
struct pool pmap_pmap_pool;
struct pool pmap_l1pt_pool;
struct pool_cache pmap_l1pt_cache;
struct pool pmap_asn_pool;
struct pool pmap_asngen_pool;
struct pool pmap_pv_pool;

/*
 * Canonical names for PGU_* constants.
 */
const char *pmap_pgu_strings[] = PGU_STRINGS;

/*
 * Address Space Numbers.
 *
 * On many implementations of the Alpha architecture, the TLB entries and
 * I-cache blocks are tagged with a unique number within an implementation-
 * specified range.  When a process context becomes active, the ASN is used
 * to match TLB entries; if a TLB entry for a particular VA does not match
 * the current ASN, it is ignored (one could think of the processor as
 * having a collection of <max ASN> separate TLBs).  This allows operating
 * system software to skip the TLB flush that would otherwise be necessary
 * at context switch time.
 *
 * Alpha PTEs have a bit in them (PG_ASM - Address Space Match) that
 * causes TLB entries to match any ASN.  The PALcode also provides
 * a TBI (Translation Buffer Invalidate) operation that flushes all
 * TLB entries that _do not_ have PG_ASM.  We use this bit for kernel
 * mappings, so that invalidation of all user mappings does not invalidate
 * kernel mappings (which are consistent across all processes).
 *
 * pmap_next_asn always indicates to the next ASN to use.  When
 * pmap_next_asn exceeds pmap_max_asn, we start a new ASN generation.
 *
 * When a new ASN generation is created, the per-process (i.e. non-PG_ASM)
 * TLB entries and the I-cache are flushed, the generation number is bumped,
 * and pmap_next_asn is changed to indicate the first non-reserved ASN.
 *
 * We reserve ASN #0 for pmaps that use the global kernel_lev1map.  This
 * prevents the following scenario:
 *
 *	* New ASN generation starts, and process A is given ASN #0.
 *
 *	* A new process B (and thus new pmap) is created.  The ASN,
 *	  for lack of a better value, is initialized to 0.
 *
 *	* Process B runs.  It is now using the TLB entries tagged
 *	  by process A.  *poof*
 *
 * In the scenario above, in addition to the processor using using incorrect
 * TLB entires, the PALcode might use incorrect information to service a
 * TLB miss.  (The PALcode uses the recursively mapped Virtual Page Table
 * to locate the PTE for a faulting address, and tagged TLB entires exist
 * for the Virtual Page Table addresses in order to speed up this procedure,
 * as well.)
 *
 * By reserving an ASN for kernel_lev1map users, we are guaranteeing that
 * new pmaps will initially run with no TLB entries for user addresses
 * or VPT mappings that map user page tables.  Since kernel_lev1map only
 * contains mappings for kernel addresses, and since those mappings
 * are always made with PG_ASM, sharing an ASN for kernel_lev1map users is
 * safe (since PG_ASM mappings match any ASN).
 *
 * On processors that do not support ASNs, the PALcode invalidates
 * the TLB and I-cache automatically on swpctx.  We still still go
 * through the motions of assigning an ASN (really, just refreshing
 * the ASN generation in this particular case) to keep the logic sane
 * in other parts of the code.
 */
u_int	pmap_max_asn;			/* max ASN supported by the system */
u_int	pmap_next_asn[ALPHA_MAXPROCS];	/* next free ASN to use */
u_long	pmap_asn_generation[ALPHA_MAXPROCS]; /* current ASN generation */

/*
 * Locking:
 *
 *	This pmap module uses two types of locks: `normal' (sleep)
 *	locks and `simple' (spin) locks.  They are used as follows:
 *
 *	READ/WRITE SPIN LOCKS
 *	---------------------
 *
 *	* pmap_main_lock - This lock is used to prevent deadlock and/or
 *	  provide mutex access to the pmap module.  Most operations lock
 *	  the pmap first, then PV lists as needed.  However, some operations,
 *	  such as pmap_page_protect(), lock the PV lists before locking
 *	  the pmaps.  To prevent deadlock, we require a mutex lock on the
 *	  pmap module if locking in the PV->pmap direction.  This is
 *	  implemented by acquiring a (shared) read lock on pmap_main_lock
 *	  if locking pmap->PV and a (exclusive) write lock if locking in
 *	  the PV->pmap direction.  Since only one thread can hold a write
 *	  lock at a time, this provides the mutex.
 *
 *	SIMPLE LOCKS
 *	------------
 *
 *	* pm_slock (per-pmap) - This lock protects all of the members
 *	  of the pmap structure itself.  This lock will be asserted
 *	  in pmap_activate() and pmap_deactivate() from a critical
 *	  section of cpu_switch(), and must never sleep.  Note that
 *	  in the case of the kernel pmap, interrupts which cause
 *	  memory allocation *must* be blocked while this lock is
 *	  asserted.
 *
 *	* pvh_slock (per-pv_head) - This lock protects the PV list
 *	  for a specified managed page.
 *
 *	* pmap_all_pmaps_slock - This lock protects the global list of
 *	  all pmaps.  Note that a pm_slock must never be held while this
 *	  lock is held.
 *
 *	* pmap_growkernel_slock - This lock protects pmap_growkernel()
 *	  and the virtual_end variable.
 *
 *	Address space number management (global ASN counters and per-pmap
 *	ASN state) are not locked; they use arrays of values indexed
 *	per-processor.
 *
 *	All internal functions which operate on a pmap are called
 *	with the pmap already locked by the caller (which will be
 *	an interface function).
 */
struct lock pmap_main_lock;
struct simplelock pmap_all_pmaps_slock;
struct simplelock pmap_growkernel_slock;

#ifdef __OpenBSD__
#define spinlockinit(lock, name, flags)  lockinit(lock, 0, name, 0, flags)
#define spinlockmgr(lock, flags, slock) lockmgr(lock, flags, slock, curproc)
#endif

#if defined(MULTIPROCESSOR) || defined(LOCKDEBUG)
#define	PMAP_MAP_TO_HEAD_LOCK() \
	spinlockmgr(&pmap_main_lock, LK_SHARED, NULL)
#define	PMAP_MAP_TO_HEAD_UNLOCK() \
	spinlockmgr(&pmap_main_lock, LK_RELEASE, NULL)
#define	PMAP_HEAD_TO_MAP_LOCK() \
	spinlockmgr(&pmap_main_lock, LK_EXCLUSIVE, NULL)
#define	PMAP_HEAD_TO_MAP_UNLOCK() \
	spinlockmgr(&pmap_main_lock, LK_RELEASE, NULL)
#else
#define	PMAP_MAP_TO_HEAD_LOCK()		/* nothing */
#define	PMAP_MAP_TO_HEAD_UNLOCK()	/* nothing */
#define	PMAP_HEAD_TO_MAP_LOCK()		/* nothing */
#define	PMAP_HEAD_TO_MAP_UNLOCK()	/* nothing */
#endif /* MULTIPROCESSOR || LOCKDEBUG */

#if defined(MULTIPROCESSOR)
/*
 * TLB Shootdown:
 *
 * When a mapping is changed in a pmap, the TLB entry corresponding to
 * the virtual address must be invalidated on all processors.  In order
 * to accomplish this on systems with multiple processors, messages are
 * sent from the processor which performs the mapping change to all
 * processors on which the pmap is active.  For other processors, the
 * ASN generation numbers for that processor is invalidated, so that
 * the next time the pmap is activated on that processor, a new ASN
 * will be allocated (which implicitly invalidates all TLB entries).
 *
 * Note, we can use the pool allocator to allocate job entries
 * since pool pages are mapped with K0SEG, not with the TLB.
 */
struct pmap_tlb_shootdown_job {
	TAILQ_ENTRY(pmap_tlb_shootdown_job) pj_list;
	vaddr_t pj_va;			/* virtual address */
	pmap_t pj_pmap;			/* the pmap which maps the address */
	pt_entry_t pj_pte;		/* the PTE bits */
};

struct pmap_tlb_shootdown_q {
	TAILQ_HEAD(, pmap_tlb_shootdown_job) pq_head;
	int pq_pte;			/* aggregate PTE bits */
	int pq_count;			/* number of pending requests */
	struct simplelock pq_slock;	/* spin lock on queue */
} pmap_tlb_shootdown_q[ALPHA_MAXPROCS];

#define	PSJQ_LOCK(pq, s)						\
do {									\
	s = splimp();							\
	simple_lock(&(pq)->pq_slock);					\
} while (0)

#define	PSJQ_UNLOCK(pq, s)						\
do {									\
	simple_unlock(&(pq)->pq_slock);					\
	splx(s);							\
} while (0)

/* If we have more pending jobs than this, we just nail the whole TLB. */
#define	PMAP_TLB_SHOOTDOWN_MAXJOBS	6

struct pool pmap_tlb_shootdown_job_pool;

struct pmap_tlb_shootdown_job *pmap_tlb_shootdown_job_get
	    (struct pmap_tlb_shootdown_q *);
void	pmap_tlb_shootdown_job_put(struct pmap_tlb_shootdown_q *,
	    struct pmap_tlb_shootdown_job *);
#endif /* MULTIPROCESSOR */

#define	PAGE_IS_MANAGED(pa)	(vm_physseg_find(atop(pa), NULL) != -1)

static __inline struct pv_head *
pa_to_pvh(paddr_t pa)
{
	int bank, pg;

	bank = vm_physseg_find(atop(pa), &pg);
	return (&vm_physmem[bank].pmseg.pvhead[pg]);
}

/*
 * Optional argument passed to pmap_remove_mapping() for stealing mapping
 * resources.
 */
struct prm_thief {
	int	prmt_flags;		/* flags; what to steal */
	struct pv_entry *prmt_pv;	/* the stolen PV entry */
	pt_entry_t *prmt_ptp;		/* the stolen PT page */
};

#define	PRMT_PV		0x0001		/* steal the PV entry */
#define	PRMT_PTP	0x0002		/* steal the PT page */

/*
 * Internal routines
 */
void	alpha_protection_init(void);
void	pmap_do_remove(pmap_t, vaddr_t, vaddr_t, boolean_t);
boolean_t pmap_remove_mapping(pmap_t, vaddr_t, pt_entry_t *,
	    boolean_t, long, struct prm_thief *);
void	pmap_changebit(paddr_t, pt_entry_t, pt_entry_t, long);

/*
 * PT page management functions.
 */
int	pmap_lev1map_create(pmap_t, long);
void	pmap_lev1map_destroy(pmap_t, long);
int	pmap_ptpage_alloc(pmap_t, pt_entry_t *, int);
boolean_t pmap_ptpage_steal(pmap_t, int, paddr_t *);
void	pmap_ptpage_free(pmap_t, pt_entry_t *, pt_entry_t **);
void	pmap_l3pt_delref(pmap_t, vaddr_t, pt_entry_t *, long,
	    pt_entry_t **);
void	pmap_l2pt_delref(pmap_t, pt_entry_t *, pt_entry_t *, long);
void	pmap_l1pt_delref(pmap_t, pt_entry_t *, long);

void	*pmap_l1pt_alloc(struct pool *, int);
void	pmap_l1pt_free(struct pool *, void *);

struct pool_allocator pmap_l1pt_allocator = {
	pmap_l1pt_alloc, pmap_l1pt_free, 0,
};

int	pmap_l1pt_ctor(void *, void *, int);

/*
 * PV table management functions.
 */
int	pmap_pv_enter(pmap_t, paddr_t, vaddr_t, pt_entry_t *, boolean_t);
void	pmap_pv_remove(pmap_t, paddr_t, vaddr_t, boolean_t,
	    struct pv_entry **);
struct	pv_entry *pmap_pv_alloc(void);
void	pmap_pv_free(struct pv_entry *);
void	*pmap_pv_page_alloc(struct pool *, int);
void	pmap_pv_page_free(struct pool *, void *);
struct pool_allocator pmap_pv_allocator = {
	pmap_pv_page_alloc, pmap_pv_page_free, 0,
};
#ifdef DEBUG
void	pmap_pv_dump(paddr_t);
#endif

/*
 * ASN management functions.
 */
void	pmap_asn_alloc(pmap_t, long);

/*
 * Misc. functions.
 */
boolean_t pmap_physpage_alloc(int, paddr_t *);
void	pmap_physpage_free(paddr_t);
int	pmap_physpage_addref(void *);
int	pmap_physpage_delref(void *);

/*
 * PMAP_ISACTIVE{,_TEST}:
 *
 *	Check to see if a pmap is active on the current processor.
 */
#define	PMAP_ISACTIVE_TEST(pm, cpu_id)					\
	(((pm)->pm_cpus & (1UL << (cpu_id))) != 0)

#if defined(DEBUG) && !defined(MULTIPROCESSOR)
#define	PMAP_ISACTIVE(pm, cpu_id)					\
({									\
	/*								\
	 * XXX This test is not MP-safe.				\
	 */								\
	int isactive_ = PMAP_ISACTIVE_TEST(pm, cpu_id);			\
									\
	if (curproc != NULL && curproc->p_vmspace != NULL &&		\
	    (pm) != pmap_kernel() &&					\
	    (isactive_ ^ ((pm) == curproc->p_vmspace->vm_map.pmap)))	\
		panic("PMAP_ISACTIVE, isa: %d pm: %p curpm:%p",		\
		    isactive_, (pm), curproc->p_vmspace->vm_map.pmap);	\
	(isactive_);							\
})
#else
#define	PMAP_ISACTIVE(pm, cpu_id)	PMAP_ISACTIVE_TEST(pm, cpu_id)
#endif /* DEBUG && !MULTIPROCESSOR */

/*
 * PMAP_ACTIVATE_ASN_SANITY:
 *
 *	DEBUG sanity checks for ASNs within PMAP_ACTIVATE.
 */
#ifdef DEBUG
#define	PMAP_ACTIVATE_ASN_SANITY(pmap, cpu_id)				\
do {									\
	if ((pmap)->pm_lev1map == kernel_lev1map) {			\
		/*							\
		 * This pmap implementation also ensures that pmaps	\
		 * referencing kernel_lev1map use a reserved ASN	\
		 * ASN to prevent the PALcode from servicing a TLB	\
		 * miss	with the wrong PTE.				\
		 */							\
		if ((pmap)->pm_asn[(cpu_id)] != PMAP_ASN_RESERVED) {	\
			printf("kernel_lev1map with non-reserved ASN "	\
			    "(line %d)\n", __LINE__);			\
			panic("PMAP_ACTIVATE_ASN_SANITY");		\
		}							\
	} else {							\
		if ((pmap)->pm_asngen[(cpu_id)] != 			\
		    pmap_asn_generation[(cpu_id)]) {			\
			/*						\
			 * ASN generation number isn't valid!		\
			 */						\
			printf("pmap asngen %lu, current %lu "		\
			    "(line %d)\n",				\
			    (pmap)->pm_asngen[(cpu_id)], 		\
			    pmap_asn_generation[(cpu_id)],		\
			    __LINE__);					\
			panic("PMAP_ACTIVATE_ASN_SANITY");		\
		}							\
		if ((pmap)->pm_asn[(cpu_id)] == PMAP_ASN_RESERVED) {	\
			/*						\
			 * DANGER WILL ROBINSON!  We're going to	\
			 * pollute the VPT TLB entries!			\
			 */						\
			printf("Using reserved ASN! (line %d)\n",	\
			    __LINE__);					\
			panic("PMAP_ACTIVATE_ASN_SANITY");		\
		}							\
	}								\
} while (0)
#else
#define	PMAP_ACTIVATE_ASN_SANITY(pmap, cpu_id)	/* nothing */
#endif

/*
 * PMAP_ACTIVATE:
 *
 *	This is essentially the guts of pmap_activate(), without
 *	ASN allocation.  This is used by pmap_activate(),
 *	pmap_lev1map_create(), and pmap_lev1map_destroy().
 *
 *	This is called only when it is known that a pmap is "active"
 *	on the current processor; the ASN must already be valid.
 */
#define	PMAP_ACTIVATE(pmap, p, cpu_id)					\
do {									\
	PMAP_ACTIVATE_ASN_SANITY(pmap, cpu_id);				\
									\
	(p)->p_addr->u_pcb.pcb_hw.apcb_ptbr =				\
	    ALPHA_K0SEG_TO_PHYS((vaddr_t)(pmap)->pm_lev1map) >> PGSHIFT; \
	(p)->p_addr->u_pcb.pcb_hw.apcb_asn = (pmap)->pm_asn[(cpu_id)];	\
									\
	if ((p) == curproc) {						\
		/*							\
		 * Page table base register has changed; switch to	\
		 * our own context again so that it will take effect.	\
		 */							\
		(void) alpha_pal_swpctx((u_long)p->p_md.md_pcbpaddr);	\
	}								\
} while (0)

/*
 * PMAP_SET_NEEDISYNC:
 *
 *	Mark that a user pmap needs an I-stream synch on its
 *	way back out to userspace.
 */
#define	PMAP_SET_NEEDISYNC(pmap)	(pmap)->pm_needisync = ~0UL

/*
 * PMAP_SYNC_ISTREAM:
 *
 *	Synchronize the I-stream for the specified pmap.  For user
 *	pmaps, this is deferred until a process using the pmap returns
 *	to userspace.
 */
#if defined(MULTIPROCESSOR)
#define	PMAP_SYNC_ISTREAM_KERNEL()					\
do {									\
	alpha_pal_imb();						\
	alpha_broadcast_ipi(ALPHA_IPI_IMB);				\
} while (0)

#define	PMAP_SYNC_ISTREAM_USER(pmap)					\
do {									\
	alpha_multicast_ipi((pmap)->pm_cpus, ALPHA_IPI_AST);		\
	/* for curcpu, will happen in userret() */			\
} while (0)
#else
#define	PMAP_SYNC_ISTREAM_KERNEL()	alpha_pal_imb()
#define	PMAP_SYNC_ISTREAM_USER(pmap)	/* will happen in userret() */
#endif /* MULTIPROCESSOR */

#define	PMAP_SYNC_ISTREAM(pmap)						\
do {									\
	if ((pmap) == pmap_kernel())					\
		PMAP_SYNC_ISTREAM_KERNEL();				\
	else								\
		PMAP_SYNC_ISTREAM_USER(pmap);				\
} while (0)

/*
 * PMAP_INVALIDATE_ASN:
 *
 *	Invalidate the specified pmap's ASN, so as to force allocation
 *	of a new one the next time pmap_asn_alloc() is called.
 *
 *	NOTE: THIS MUST ONLY BE CALLED IF AT LEAST ONE OF THE FOLLOWING
 *	CONDITIONS ARE TRUE:
 *
 *		(1) The pmap references the global kernel_lev1map.
 *
 *		(2) The pmap is not active on the current processor.
 */
#define	PMAP_INVALIDATE_ASN(pmap, cpu_id)				\
do {									\
	(pmap)->pm_asn[(cpu_id)] = PMAP_ASN_RESERVED;			\
} while (0)

/*
 * PMAP_INVALIDATE_TLB:
 *
 *	Invalidate the TLB entry for the pmap/va pair.
 */
#define	PMAP_INVALIDATE_TLB(pmap, va, hadasm, isactive, cpu_id)		\
do {									\
	if ((hadasm) || (isactive)) {					\
		/*							\
		 * Simply invalidating the TLB entry and I-cache	\
		 * works in this case.					\
		 */							\
		ALPHA_TBIS((va));					\
	} else if ((pmap)->pm_asngen[(cpu_id)] == 			\
	    pmap_asn_generation[(cpu_id)]) {				\
		/*							\
		 * We can't directly invalidate the TLB entry		\
		 * in this case, so we have to force allocation		\
		 * of a new ASN the next time this pmap becomes		\
		 * active.						\
		 */							\
		PMAP_INVALIDATE_ASN((pmap), (cpu_id));			\
	}								\
		/*							\
		 * Nothing to do in this case; the next time the	\
		 * pmap becomes active on this processor, a new		\
		 * ASN will be allocated anyway.			\
		 */							\
} while (0)

/*
 * PMAP_KERNEL_PTE:
 *
 *	Get a kernel PTE.
 *
 *	If debugging, do a table walk.  If not debugging, just use
 *	the Virtual Page Table, since all kernel page tables are
 *	pre-allocated and mapped in.
 */
#ifdef DEBUG
#define	PMAP_KERNEL_PTE(va)						\
({									\
	pt_entry_t *l1pte_, *l2pte_;					\
									\
	l1pte_ = pmap_l1pte(pmap_kernel(), va);				\
	if (pmap_pte_v(l1pte_) == 0) {					\
		printf("kernel level 1 PTE not valid, va 0x%lx "	\
		    "(line %d)\n", (va), __LINE__);			\
		panic("PMAP_KERNEL_PTE");				\
	}								\
	l2pte_ = pmap_l2pte(pmap_kernel(), va, l1pte_);			\
	if (pmap_pte_v(l2pte_) == 0) {					\
		printf("kernel level 2 PTE not valid, va 0x%lx "	\
		    "(line %d)\n", (va), __LINE__);			\
		panic("PMAP_KERNEL_PTE");				\
	}								\
	pmap_l3pte(pmap_kernel(), va, l2pte_);				\
})
#else
#define	PMAP_KERNEL_PTE(va)	(&VPT[VPT_INDEX((va))])
#endif

/*
 * PMAP_SET_PTE:
 *
 *	Set a PTE to a specified value.
 */
#define	PMAP_SET_PTE(ptep, val)	*(ptep) = (val)

/*
 * PMAP_STAT_{INCR,DECR}:
 *
 *	Increment or decrement a pmap statistic.
 */
#define	PMAP_STAT_INCR(s, v)	atomic_add_ulong((unsigned long *)(&(s)), (v))
#define	PMAP_STAT_DECR(s, v)	atomic_sub_ulong((unsigned long *)(&(s)), (v))

/*
 * pmap_bootstrap:
 *
 *	Bootstrap the system to run with virtual memory.
 *
 *	Note: no locking is necessary in this function.
 */
void
pmap_bootstrap(paddr_t ptaddr, u_int maxasn, u_long ncpuids)
{
	vsize_t lev2mapsize, lev3mapsize;
	pt_entry_t *lev2map, *lev3map;
	pt_entry_t pte;
	int i;

#ifdef DEBUG
	if (pmapdebug & (PDB_FOLLOW|PDB_BOOTSTRAP))
		printf("pmap_bootstrap(0x%lx, %u)\n", ptaddr, maxasn);
#endif

	/*
	 * Compute the number of pages kmem_map will have.
	 */
	kmeminit_nkmempages();

	/*
	 * Figure out how many PTE's are necessary to map the kernel.
	 */
	lev3mapsize = (VM_PHYS_SIZE +
		nbuf * MAXBSIZE + 16 * NCARGS + PAGER_MAP_SIZE) / NBPG +
		(maxproc * UPAGES) + nkmempages;

#ifdef SYSVSHM
	lev3mapsize += shminfo.shmall;
#endif
	lev3mapsize = roundup(lev3mapsize, NPTEPG);

	/*
	 * Allocate a level 1 PTE table for the kernel.
	 * This is always one page long.
	 * IF THIS IS NOT A MULTIPLE OF NBPG, ALL WILL GO TO HELL.
	 */
	kernel_lev1map = (pt_entry_t *)
	    pmap_steal_memory(sizeof(pt_entry_t) * NPTEPG, NULL, NULL);

	/*
	 * Allocate a level 2 PTE table for the kernel.
	 * These must map all of the level3 PTEs.
	 * IF THIS IS NOT A MULTIPLE OF NBPG, ALL WILL GO TO HELL.
	 */
	lev2mapsize = roundup(howmany(lev3mapsize, NPTEPG), NPTEPG);
	lev2map = (pt_entry_t *)
	    pmap_steal_memory(sizeof(pt_entry_t) * lev2mapsize, NULL, NULL);

	/*
	 * Allocate a level 3 PTE table for the kernel.
	 * Contains lev3mapsize PTEs.
	 */
	lev3map = (pt_entry_t *)
	    pmap_steal_memory(sizeof(pt_entry_t) * lev3mapsize, NULL, NULL);

	/*
	 * Allocate memory for the pv_heads.  (A few more of the latter
	 * are allocated than are needed.)
	 *
	 * We could do this in pmap_init when we know the actual
	 * managed page pool size, but its better to use kseg0
	 * addresses rather than kernel virtual addresses mapped
	 * through the TLB.
	 */
	pv_table_npages = physmem;
	pv_table = (struct pv_head *)
	    pmap_steal_memory(sizeof(struct pv_head) * pv_table_npages,
	    NULL, NULL);

	/*
	 * ...and initialize the pv_entry list headers.
	 */
	for (i = 0; i < pv_table_npages; i++) {
		LIST_INIT(&pv_table[i].pvh_list);
		simple_lock_init(&pv_table[i].pvh_slock);
	}

	/*
	 * Set up level 1 page table
	 */

	/* Map all of the level 2 pte pages */
	for (i = 0; i < howmany(lev2mapsize, NPTEPG); i++) {
		pte = (ALPHA_K0SEG_TO_PHYS(((vaddr_t)lev2map) +
		    (i*PAGE_SIZE)) >> PGSHIFT) << PG_SHIFT;
		pte |= PG_V | PG_ASM | PG_KRE | PG_KWE | PG_WIRED;
		kernel_lev1map[l1pte_index(VM_MIN_KERNEL_ADDRESS +
		    (i*PAGE_SIZE*NPTEPG*NPTEPG))] = pte;
	}

	/* Map the virtual page table */
	pte = (ALPHA_K0SEG_TO_PHYS((vaddr_t)kernel_lev1map) >> PGSHIFT)
	    << PG_SHIFT;
	pte |= PG_V | PG_KRE | PG_KWE; /* NOTE NO ASM */
	kernel_lev1map[l1pte_index(VPTBASE)] = pte;
	VPT = (pt_entry_t *)VPTBASE;

#ifdef _PMAP_MAY_USE_PROM_CONSOLE
    {
	extern pt_entry_t prom_pte;			/* XXX */
	extern int prom_mapped;				/* XXX */

	if (pmap_uses_prom_console()) {
		/*
		 * XXX Save old PTE so we can remap the PROM, if
		 * XXX necessary.
		 */
		prom_pte = *(pt_entry_t *)ptaddr & ~PG_ASM;
	}
	prom_mapped = 0;

	/*
	 * Actually, this code lies.  The prom is still mapped, and will
	 * remain so until the context switch after alpha_init() returns.
	 */
    }
#endif

	/*
	 * Set up level 2 page table.
	 */
	/* Map all of the level 3 pte pages */
	for (i = 0; i < howmany(lev3mapsize, NPTEPG); i++) {
		pte = (ALPHA_K0SEG_TO_PHYS(((vaddr_t)lev3map) +
		    (i*PAGE_SIZE)) >> PGSHIFT) << PG_SHIFT;
		pte |= PG_V | PG_ASM | PG_KRE | PG_KWE | PG_WIRED;
		lev2map[l2pte_index(VM_MIN_KERNEL_ADDRESS+
		    (i*PAGE_SIZE*NPTEPG))] = pte;
	}

	/* Initialize the pmap_growkernel_slock. */
	simple_lock_init(&pmap_growkernel_slock);

	/*
	 * Set up level three page table (lev3map)
	 */
	/* Nothing to do; it's already zero'd */

	/*
	 * Initialize `FYI' variables.  Note we're relying on
	 * the fact that BSEARCH sorts the vm_physmem[] array
	 * for us.
	 */
	avail_start = ptoa(vm_physmem[0].start);
	avail_end = ptoa(vm_physmem[vm_nphysseg - 1].end);
	virtual_avail = VM_MIN_KERNEL_ADDRESS;
	virtual_end = VM_MIN_KERNEL_ADDRESS + lev3mapsize * PAGE_SIZE;

#if 0
	printf("avail_start = 0x%lx\n", avail_start);
	printf("avail_end = 0x%lx\n", avail_end);
	printf("virtual_avail = 0x%lx\n", virtual_avail);
	printf("virtual_end = 0x%lx\n", virtual_end);
#endif

	/*
	 * Intialize the pmap pools and list.
	 */
	pmap_ncpuids = ncpuids;
	pool_init(&pmap_pmap_pool, sizeof(struct pmap), 0, 0, 0, "pmappl",
	    &pool_allocator_nointr);
	pool_init(&pmap_l1pt_pool, PAGE_SIZE, 0, 0, 0, "l1ptpl",
	    &pmap_l1pt_allocator);
	pool_cache_init(&pmap_l1pt_cache, &pmap_l1pt_pool, pmap_l1pt_ctor,
	    NULL, NULL);
	pool_init(&pmap_asn_pool, pmap_ncpuids * sizeof(u_int), 0, 0, 0,
	    "pmasnpl", &pool_allocator_nointr);
	pool_init(&pmap_asngen_pool, pmap_ncpuids * sizeof(u_long), 0, 0, 0,
	    "pmasngenpl", &pool_allocator_nointr);
	pool_init(&pmap_pv_pool, sizeof(struct pv_entry), 0, 0, 0, "pvpl",
	    &pmap_pv_allocator);

	TAILQ_INIT(&pmap_all_pmaps);

	/*
	 * Initialize the ASN logic.
	 */
	pmap_max_asn = maxasn;
	for (i = 0; i < ALPHA_MAXPROCS; i++) {
		pmap_next_asn[i] = 1;
		pmap_asn_generation[i] = 0;
	}

	/*
	 * Initialize the locks.
	 */
	spinlockinit(&pmap_main_lock, "pmaplk", 0);
	simple_lock_init(&pmap_all_pmaps_slock);

	/*
	 * Initialize kernel pmap.  Note that all kernel mappings
	 * have PG_ASM set, so the ASN doesn't really matter for
	 * the kernel pmap.  Also, since the kernel pmap always
	 * references kernel_lev1map, it always has an invalid ASN
	 * generation.
	 */
	memset(pmap_kernel(), 0, sizeof(struct pmap));
	pmap_kernel()->pm_lev1map = kernel_lev1map;
	pmap_kernel()->pm_count = 1;
	pmap_kernel()->pm_asn = kernel_pmap_asn_store;
	pmap_kernel()->pm_asngen = kernel_pmap_asngen_store;
	for (i = 0; i < ALPHA_MAXPROCS; i++) {
		pmap_kernel()->pm_asn[i] = PMAP_ASN_RESERVED;
		pmap_kernel()->pm_asngen[i] = pmap_asn_generation[i];
	}
	simple_lock_init(&pmap_kernel()->pm_slock);
	TAILQ_INSERT_TAIL(&pmap_all_pmaps, pmap_kernel(), pm_list);

#if defined(MULTIPROCESSOR)
	/*
	 * Initialize the TLB shootdown queues.
	 */
	pool_init(&pmap_tlb_shootdown_job_pool,
	    sizeof(struct pmap_tlb_shootdown_job), 0, 0, 0, "pmaptlbpl",
	    NULL);
	for (i = 0; i < ALPHA_MAXPROCS; i++) {
		TAILQ_INIT(&pmap_tlb_shootdown_q[i].pq_head);
		simple_lock_init(&pmap_tlb_shootdown_q[i].pq_slock);
	}
#endif

	/*
	 * Set up proc0's PCB such that the ptbr points to the right place
	 * and has the kernel pmap's (really unused) ASN.
	 */
	proc0.p_addr->u_pcb.pcb_hw.apcb_ptbr =
	    ALPHA_K0SEG_TO_PHYS((vaddr_t)kernel_lev1map) >> PGSHIFT;
	proc0.p_addr->u_pcb.pcb_hw.apcb_asn =
	    pmap_kernel()->pm_asn[cpu_number()];

	/*
	 * Mark the kernel pmap `active' on this processor.
	 */
	atomic_setbits_ulong(&pmap_kernel()->pm_cpus,
	    (1UL << cpu_number()));
}

#ifdef _PMAP_MAY_USE_PROM_CONSOLE
int
pmap_uses_prom_console(void)
{

#if defined(NEW_SCC_DRIVER)
	return (cputype == ST_DEC_21000);
#else
	return (cputype == ST_DEC_21000
	    || cputype == ST_DEC_3000_300
	    || cputype == ST_DEC_3000_500);
#endif /* NEW_SCC_DRIVER */
}
#endif /* _PMAP_MAY_USE_PROM_CONSOLE */

void
pmap_virtual_space(vaddr_t *vstartp, vaddr_t *vendp)
{
	*vstartp = VM_MIN_KERNEL_ADDRESS;
	*vendp = VM_MAX_KERNEL_ADDRESS;
}

/*
 * pmap_steal_memory:		[ INTERFACE ]
 *
 *	Bootstrap memory allocator (alternative to vm_bootstrap_steal_memory()).
 *	This function allows for early dynamic memory allocation until the
 *	virtual memory system has been bootstrapped.  After that point, either
 *	kmem_alloc or malloc should be used.  This function works by stealing
 *	pages from the (to be) managed page pool, then implicitly mapping the
 *	pages (by using their k0seg addresses) and zeroing them.
 *
 *	It may be used once the physical memory segments have been pre-loaded
 *	into the vm_physmem[] array.  Early memory allocation MUST use this
 *	interface!  This cannot be used after vm_page_startup(), and will
 *	generate a panic if tried.
 *
 *	Note that this memory will never be freed, and in essence it is wired
 *	down.
 *
 *	Note: no locking is necessary in this function.
 */
vaddr_t
pmap_steal_memory(vsize_t size, vaddr_t *vstartp, vaddr_t *vendp)
{
	int bank, npgs, x;
	vaddr_t va;
	paddr_t pa;

	size = round_page(size);
	npgs = atop(size);

#if 0
	printf("PSM: size 0x%lx (npgs 0x%x)\n", size, npgs);
#endif

	for (bank = 0; bank < vm_nphysseg; bank++) {
		if (uvm.page_init_done == TRUE)
			panic("pmap_steal_memory: called _after_ bootstrap");


#if 0
		printf("     bank %d: avail_start 0x%lx, start 0x%lx, "
		    "avail_end 0x%lx\n", bank, vm_physmem[bank].avail_start,
		    vm_physmem[bank].start, vm_physmem[bank].avail_end);
#endif

		if (vm_physmem[bank].avail_start != vm_physmem[bank].start ||
		    vm_physmem[bank].avail_start >= vm_physmem[bank].avail_end)
			continue;

#if 0
		printf("             avail_end - avail_start = 0x%lx\n",
		    vm_physmem[bank].avail_end - vm_physmem[bank].avail_start);
#endif

		if ((vm_physmem[bank].avail_end - vm_physmem[bank].avail_start)
		    < npgs)
			continue;

		/*
		 * There are enough pages here; steal them!
		 */
		pa = ptoa(vm_physmem[bank].avail_start);
		vm_physmem[bank].avail_start += npgs;
		vm_physmem[bank].start += npgs;

		/*
		 * Have we used up this segment?
		 */
		if (vm_physmem[bank].avail_start == vm_physmem[bank].end) {
			if (vm_nphysseg == 1)
				panic("pmap_steal_memory: out of memory!");

			/* Remove this segment from the list. */
			vm_nphysseg--;
			for (x = bank; x < vm_nphysseg; x++) {
				/* structure copy */
				vm_physmem[x] = vm_physmem[x + 1];
			}
		}

		/*
		 * Fill these in for the caller; we don't modify them,
		 * but the upper layers still want to know.
		 */
		if (vstartp)
			*vstartp = round_page(virtual_avail);
		if (vendp)
			*vendp = VM_MAX_KERNEL_ADDRESS;

		va = ALPHA_PHYS_TO_K0SEG(pa);
		memset((caddr_t)va, 0, size);
		pmap_pages_stolen += npgs;
		return (va);
	}

	/*
	 * If we got here, this was no memory left.
	 */
	panic("pmap_steal_memory: no memory to steal");
}

/*
 * pmap_init:			[ INTERFACE ]
 *
 *	Initialize the pmap module.  Called by vm_init(), to initialize any
 *	structures that the pmap system needs to map virtual memory.
 *
 *	Note: no locking is necessary in this function.
 */
void
pmap_init(void)
{
	vsize_t		s;
	int		bank;
	struct pv_head	*pvh;

#ifdef DEBUG
        if (pmapdebug & PDB_FOLLOW)
                printf("pmap_init()\n");
#endif

	/* initialize protection array */
	alpha_protection_init();

	/*
	 * Memory for the pv heads has already been allocated.
	 * Initialize the physical memory segments.
	 */
	pvh = pv_table;
	for (bank = 0; bank < vm_nphysseg; bank++) {
		s = vm_physmem[bank].end - vm_physmem[bank].start;
		vm_physmem[bank].pmseg.pvhead = pvh;
		pvh += s;
	}

	/*
	 * Set a low water mark on the pv_entry pool, so that we are
	 * more likely to have these around even in extreme memory
	 * starvation.
	 */
	pool_setlowat(&pmap_pv_pool, pmap_pv_lowat);

	/*
	 * Now it is safe to enable pv entry recording.
	 */
	pmap_initialized = TRUE;

#if 0
	for (bank = 0; bank < vm_nphysseg; bank++) {
		printf("bank %d\n", bank);
		printf("\tstart = 0x%x\n", ptoa(vm_physmem[bank].start));
		printf("\tend = 0x%x\n", ptoa(vm_physmem[bank].end));
		printf("\tavail_start = 0x%x\n",
		    ptoa(vm_physmem[bank].avail_start));
		printf("\tavail_end = 0x%x\n",
		    ptoa(vm_physmem[bank].avail_end));
	}
#endif
}

/*
 * pmap_create:			[ INTERFACE ]
 *
 *	Create and return a physical map.
 *
 *	Note: no locking is necessary in this function.
 */
pmap_t
pmap_create(void)
{
	pmap_t pmap;
	int i;

#ifdef DEBUG
	if (pmapdebug & (PDB_FOLLOW|PDB_CREATE))
		printf("pmap_create()\n");
#endif

	pmap = pool_get(&pmap_pmap_pool, PR_WAITOK);
	memset(pmap, 0, sizeof(*pmap));

	pmap->pm_asn = pool_get(&pmap_asn_pool, PR_WAITOK);
	pmap->pm_asngen = pool_get(&pmap_asngen_pool, PR_WAITOK);

	/*
	 * Defer allocation of a new level 1 page table until
	 * the first new mapping is entered; just take a reference
	 * to the kernel kernel_lev1map.
	 */
	pmap->pm_lev1map = kernel_lev1map;

	pmap->pm_count = 1;
	for (i = 0; i < pmap_ncpuids; i++) {
		pmap->pm_asn[i] = PMAP_ASN_RESERVED;
		/* XXX Locking? */
		pmap->pm_asngen[i] = pmap_asn_generation[i];
	}
	simple_lock_init(&pmap->pm_slock);

	simple_lock(&pmap_all_pmaps_slock);
	TAILQ_INSERT_TAIL(&pmap_all_pmaps, pmap, pm_list);
	simple_unlock(&pmap_all_pmaps_slock);

	return (pmap);
}

/*
 * pmap_destroy:		[ INTERFACE ]
 *
 *	Drop the reference count on the specified pmap, releasing
 *	all resources if the reference count drops to zero.
 */
void
pmap_destroy(pmap_t pmap)
{
	int refs;

#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_destroy(%p)\n", pmap);
#endif
	if (pmap == NULL)
		return;

	PMAP_LOCK(pmap);
	refs = --pmap->pm_count;
	PMAP_UNLOCK(pmap);

	if (refs > 0)
		return;

	/*
	 * Remove it from the global list of all pmaps.
	 */
	simple_lock(&pmap_all_pmaps_slock);
	TAILQ_REMOVE(&pmap_all_pmaps, pmap, pm_list);
	simple_unlock(&pmap_all_pmaps_slock);

#ifdef DIAGNOSTIC
	/*
	 * Since the pmap is supposed to contain no valid
	 * mappings at this point, this should never happen.
	 */
	if (pmap->pm_lev1map != kernel_lev1map) {
		printf("pmap_release: pmap still contains valid mappings!\n");
		if (pmap->pm_nlev2)
			printf("pmap_release: %ld level 2 tables left\n",
			    pmap->pm_nlev2);
		if (pmap->pm_nlev3)
			printf("pmap_release: %ld level 3 tables left\n",
			    pmap->pm_nlev3);
		pmap_remove(pmap, VM_MIN_ADDRESS, VM_MAX_ADDRESS);
		pmap_update(pmap);
		if (pmap->pm_lev1map != kernel_lev1map)
			panic("pmap_release: pmap_remove() didn't");
	}
#endif

	pool_put(&pmap_asn_pool, pmap->pm_asn);
	pool_put(&pmap_asngen_pool, pmap->pm_asngen);
	pool_put(&pmap_pmap_pool, pmap);
}

/*
 * pmap_reference:		[ INTERFACE ]
 *
 *	Add a reference to the specified pmap.
 */
void
pmap_reference(pmap_t pmap)
{

#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_reference(%p)\n", pmap);
#endif
	if (pmap != NULL) {
		PMAP_LOCK(pmap);
		pmap->pm_count++;
		PMAP_UNLOCK(pmap);
	}
}

/*
 * pmap_remove:			[ INTERFACE ]
 *
 *	Remove the given range of addresses from the specified map.
 *
 *	It is assumed that the start and end are properly
 *	rounded to the page size.
 */
void
pmap_remove(pmap_t pmap, vaddr_t sva, vaddr_t eva)
{

#ifdef DEBUG
	if (pmapdebug & (PDB_FOLLOW|PDB_REMOVE|PDB_PROTECT))
		printf("pmap_remove(%p, %lx, %lx)\n", pmap, sva, eva);
#endif

	pmap_do_remove(pmap, sva, eva, TRUE);
}

/*
 * pmap_do_remove:
 *
 *	This actually removes the range of addresses from the
 *	specified map.  It is used by pmap_collect() (does not
 *	want to remove wired mappings) and pmap_remove() (does
 *	want to remove wired mappings).
 */
void
pmap_do_remove(pmap_t pmap, vaddr_t sva, vaddr_t eva, boolean_t dowired)
{
	pt_entry_t *l1pte, *l2pte, *l3pte;
	pt_entry_t *saved_l1pte, *saved_l2pte, *saved_l3pte;
	vaddr_t l1eva, l2eva, vptva;
	boolean_t needisync = FALSE;
	long cpu_id = cpu_number();

#ifdef DEBUG
	if (pmapdebug & (PDB_FOLLOW|PDB_REMOVE|PDB_PROTECT))
		printf("pmap_remove(%p, %lx, %lx)\n", pmap, sva, eva);
#endif

	if (pmap == NULL)
		return;

	/*
	 * If this is the kernel pmap, we can use a faster method
	 * for accessing the PTEs (since the PT pages are always
	 * resident).
	 *
	 * Note that this routine should NEVER be called from an
	 * interrupt context; pmap_kremove() is used for that.
	 */
	if (pmap == pmap_kernel()) {
		PMAP_MAP_TO_HEAD_LOCK();
		PMAP_LOCK(pmap);

		KASSERT(dowired == TRUE);

		while (sva < eva) {
			l3pte = PMAP_KERNEL_PTE(sva);
			if (pmap_pte_v(l3pte)) {
#ifdef DIAGNOSTIC
				if (PAGE_IS_MANAGED(pmap_pte_pa(l3pte)) &&
				    pmap_pte_pv(l3pte) == 0)
					panic("pmap_remove: managed page "
					    "without PG_PVLIST for 0x%lx",
					    sva);
#endif
				needisync |= pmap_remove_mapping(pmap, sva,
				    l3pte, TRUE, cpu_id, NULL);
			}
			sva += PAGE_SIZE;
		}

		PMAP_UNLOCK(pmap);
		PMAP_MAP_TO_HEAD_UNLOCK();

		if (needisync)
			PMAP_SYNC_ISTREAM_KERNEL();
		return;
	}

#ifdef DIAGNOSTIC
	if (sva > VM_MAXUSER_ADDRESS || eva > VM_MAXUSER_ADDRESS)
		panic("pmap_remove: (0x%lx - 0x%lx) user pmap, kernel "
		    "address range", sva, eva);
#endif

	PMAP_MAP_TO_HEAD_LOCK();
	PMAP_LOCK(pmap);

	/*
	 * If we're already referencing the kernel_lev1map, there
	 * is no work for us to do.
	 */
	if (pmap->pm_lev1map == kernel_lev1map)
		goto out;

	saved_l1pte = l1pte = pmap_l1pte(pmap, sva);

	/*
	 * Add a reference to the L1 table to it won't get
	 * removed from under us.
	 */
	pmap_physpage_addref(saved_l1pte);

	for (; sva < eva; sva = l1eva, l1pte++) {
		l1eva = alpha_trunc_l1seg(sva) + ALPHA_L1SEG_SIZE;
		if (pmap_pte_v(l1pte)) {
			saved_l2pte = l2pte = pmap_l2pte(pmap, sva, l1pte);

			/*
			 * Add a reference to the L2 table so it won't
			 * get removed from under us.
			 */
			pmap_physpage_addref(saved_l2pte);

			for (; sva < l1eva && sva < eva; sva = l2eva, l2pte++) {
				l2eva =
				    alpha_trunc_l2seg(sva) + ALPHA_L2SEG_SIZE;
				if (pmap_pte_v(l2pte)) {
					saved_l3pte = l3pte =
					    pmap_l3pte(pmap, sva, l2pte);

					/*
					 * Add a reference to the L3 table so
					 * it won't get removed from under us.
					 */
					pmap_physpage_addref(saved_l3pte);

					/*
					 * Remember this sva; if the L3 table
					 * gets removed, we need to invalidate
					 * the VPT TLB entry for it.
					 */
					vptva = sva;

					for (; sva < l2eva && sva < eva;
					     sva += PAGE_SIZE, l3pte++) {
						if (pmap_pte_v(l3pte) &&
						    (dowired == TRUE ||
						     pmap_pte_w(l3pte) == 0)) {
							needisync |=
							    pmap_remove_mapping(
								pmap, sva,
								l3pte, TRUE,
								cpu_id, NULL);
						}
					}

					/*
					 * Remove the reference to the L3
					 * table that we added above.  This
					 * may free the L3 table.
					 */
					pmap_l3pt_delref(pmap, vptva,
					    saved_l3pte, cpu_id, NULL);
				}
			}

			/*
			 * Remove the reference to the L2 table that we
			 * added above.  This may free the L2 table.
			 */
			pmap_l2pt_delref(pmap, l1pte, saved_l2pte, cpu_id);
		}
	}

	/*
	 * Remove the reference to the L1 table that we added above.
	 * This may free the L1 table.
	 */
	pmap_l1pt_delref(pmap, saved_l1pte, cpu_id);

	if (needisync)
		PMAP_SYNC_ISTREAM_USER(pmap);

 out:
	PMAP_UNLOCK(pmap);
	PMAP_MAP_TO_HEAD_UNLOCK();
}

/*
 * pmap_page_protect:		[ INTERFACE ]
 *
 *	Lower the permission for all mappings to a given page to
 *	the permissions specified.
 */
void
pmap_page_protect(struct vm_page *pg, vm_prot_t prot)
{
	pmap_t pmap;
	struct pv_head *pvh;
	pv_entry_t pv, nextpv;
	boolean_t needkisync = FALSE;
	long cpu_id = cpu_number();
	paddr_t pa = VM_PAGE_TO_PHYS(pg);

#ifdef DEBUG
	if ((pmapdebug & (PDB_FOLLOW|PDB_PROTECT)) ||
	    (prot == VM_PROT_NONE && (pmapdebug & PDB_REMOVE)))
		printf("pmap_page_protect(%p, %x)\n", pg, prot);
#endif

	switch (prot) {
	case VM_PROT_READ|VM_PROT_WRITE|VM_PROT_EXECUTE:
	case VM_PROT_READ|VM_PROT_WRITE:
		return;
	/* copy_on_write */
	case VM_PROT_READ|VM_PROT_EXECUTE:
	case VM_PROT_READ:
		pvh = pa_to_pvh(pa);
		PMAP_HEAD_TO_MAP_LOCK();
		simple_lock(&pvh->pvh_slock);
/* XXX */	pmap_changebit(pa, 0, ~(PG_KWE | PG_UWE), cpu_id);
		simple_unlock(&pvh->pvh_slock);
		PMAP_HEAD_TO_MAP_UNLOCK();
		return;
	/* remove_all */
	default:
		break;
	}

	pvh = pa_to_pvh(pa);
	PMAP_HEAD_TO_MAP_LOCK();
	simple_lock(&pvh->pvh_slock);
	for (pv = LIST_FIRST(&pvh->pvh_list); pv != NULL; pv = nextpv) {
		nextpv = LIST_NEXT(pv, pv_list);
		pmap = pv->pv_pmap;

		PMAP_LOCK(pmap);
#ifdef DEBUG
		if (pmap_pte_v(pmap_l2pte(pv->pv_pmap, pv->pv_va, NULL)) == 0 ||
		    pmap_pte_pa(pv->pv_pte) != pa)
			panic("pmap_page_protect: bad mapping");
#endif
		if (pmap_pte_w(pv->pv_pte) == 0) {
			if (pmap_remove_mapping(pmap, pv->pv_va, pv->pv_pte,
			    FALSE, cpu_id, NULL) == TRUE) {
				if (pmap == pmap_kernel())
					needkisync |= TRUE;
				else
					PMAP_SYNC_ISTREAM_USER(pmap);
			}
		}
#ifdef DEBUG
		else {
			if (pmapdebug & PDB_PARANOIA) {
				printf("%s wired mapping for %lx not removed\n",
				       "pmap_page_protect:", pa);
				printf("vm wire count %d\n", 
					PHYS_TO_VM_PAGE(pa)->wire_count);
			}
		}
#endif
		PMAP_UNLOCK(pmap);
	}

	if (needkisync)
		PMAP_SYNC_ISTREAM_KERNEL();

	simple_unlock(&pvh->pvh_slock);
	PMAP_HEAD_TO_MAP_UNLOCK();
}

/*
 * pmap_protect:		[ INTERFACE ]
 *
 *	Set the physical protection on the specified range of this map
 *	as requested.
 */
void
pmap_protect(pmap_t pmap, vaddr_t sva, vaddr_t eva, vm_prot_t prot)
{
	pt_entry_t *l1pte, *l2pte, *l3pte, bits;
	boolean_t isactive;
	boolean_t hadasm;
	vaddr_t l1eva, l2eva;
	long cpu_id = cpu_number();

#ifdef DEBUG
	if (pmapdebug & (PDB_FOLLOW|PDB_PROTECT))
		printf("pmap_protect(%p, %lx, %lx, %x)\n",
		    pmap, sva, eva, prot);
#endif

	if (pmap == NULL)
		return;

	if ((prot & VM_PROT_READ) == VM_PROT_NONE) {
		pmap_remove(pmap, sva, eva);
		return;
	}

	if (prot & VM_PROT_WRITE)
		return;

	PMAP_LOCK(pmap);

	bits = pte_prot(pmap, prot);
	if (!pmap_pte_exec(&bits))
		bits |= PG_FOE;
	isactive = PMAP_ISACTIVE(pmap, cpu_id);

	l1pte = pmap_l1pte(pmap, sva);
	for (; sva < eva; sva = l1eva, l1pte++) {
		l1eva = alpha_trunc_l1seg(sva) + ALPHA_L1SEG_SIZE;
		if (pmap_pte_v(l1pte)) {
			l2pte = pmap_l2pte(pmap, sva, l1pte);
			for (; sva < l1eva && sva < eva; sva = l2eva, l2pte++) {
				l2eva =
				    alpha_trunc_l2seg(sva) + ALPHA_L2SEG_SIZE;
				if (pmap_pte_v(l2pte)) {
					l3pte = pmap_l3pte(pmap, sva, l2pte);
					for (; sva < l2eva && sva < eva;
					     sva += PAGE_SIZE, l3pte++) {
						if (pmap_pte_v(l3pte) &&
						    pmap_pte_prot_chg(l3pte,
						    bits)) {
							hadasm =
							   (pmap_pte_asm(l3pte)
							    != 0);
							pmap_pte_set_prot(l3pte,
							   bits);
							PMAP_INVALIDATE_TLB(
							   pmap, sva, hadasm,
							   isactive, cpu_id);
							PMAP_TLB_SHOOTDOWN(
							   pmap, sva,
							   hadasm ? PG_ASM : 0);
						}
					}
				}
			}
		}
	}

	if (prot & VM_PROT_EXECUTE)
		PMAP_SYNC_ISTREAM(pmap);

	PMAP_UNLOCK(pmap);
}

/*
 * pmap_enter:			[ INTERFACE ]
 *
 *	Insert the given physical page (p) at
 *	the specified virtual address (v) in the
 *	target physical map with the protection requested.
 *
 *	If specified, the page will be wired down, meaning
 *	that the related pte can not be reclaimed.
 *
 *	Note:  This is the only routine which MAY NOT lazy-evaluate
 *	or lose information.  That is, this routine must actually
 *	insert this page into the given map NOW.
 */
int
pmap_enter(pmap_t pmap, vaddr_t va, paddr_t pa, vm_prot_t prot, int flags)
{
	boolean_t managed;
	pt_entry_t *pte, npte, opte;
	paddr_t opa;
	boolean_t tflush = TRUE;
	boolean_t hadasm = FALSE;	/* XXX gcc -Wuninitialized */
	boolean_t needisync = FALSE;
	boolean_t setisync = FALSE;
	boolean_t isactive;
	boolean_t wired;
	long cpu_id = cpu_number();
	int error = 0;

#ifdef DEBUG
	if (pmapdebug & (PDB_FOLLOW|PDB_ENTER))
		printf("pmap_enter(%p, %lx, %lx, %x, %x)\n",
		       pmap, va, pa, prot, flags);
#endif
	managed = PAGE_IS_MANAGED(pa);
	isactive = PMAP_ISACTIVE(pmap, cpu_id);
	wired = (flags & PMAP_WIRED) != 0;

	/*
	 * Determine what we need to do about the I-stream.  If
	 * VM_PROT_EXECUTE is set, we mark a user pmap as needing
	 * an I-sync on the way back out to userspace.  We always
	 * need an immediate I-sync for the kernel pmap.
	 */
	if (prot & VM_PROT_EXECUTE) {
		if (pmap == pmap_kernel())
			needisync = TRUE;
		else {
			setisync = TRUE;
			needisync = (pmap->pm_cpus != 0);
		}
	}

	PMAP_MAP_TO_HEAD_LOCK();
	PMAP_LOCK(pmap);

	if (pmap == pmap_kernel()) {
#ifdef DIAGNOSTIC
		/*
		 * Sanity check the virtual address.
		 */
		if (va < VM_MIN_KERNEL_ADDRESS)
			panic("pmap_enter: kernel pmap, invalid va 0x%lx", va);
#endif
		pte = PMAP_KERNEL_PTE(va);
	} else {
		pt_entry_t *l1pte, *l2pte;

#ifdef DIAGNOSTIC
		/*
		 * Sanity check the virtual address.
		 */
		if (va >= VM_MAXUSER_ADDRESS)
			panic("pmap_enter: user pmap, invalid va 0x%lx", va);
#endif

		/*
		 * If we're still referencing the kernel kernel_lev1map,
		 * create a new level 1 page table.  A reference will be
		 * added to the level 1 table when the level 2 table is
		 * created.
		 */
		if (pmap->pm_lev1map == kernel_lev1map) {
			error = pmap_lev1map_create(pmap, cpu_id);
			if (error) {
				if (flags & PMAP_CANFAIL)
					goto out;
				panic("pmap_enter: unable to create lev1map");
			}
		}

		/*
		 * Check to see if the level 1 PTE is valid, and
		 * allocate a new level 2 page table page if it's not.
		 * A reference will be added to the level 2 table when
		 * the level 3 table is created.
		 */
		l1pte = pmap_l1pte(pmap, va);
		if (pmap_pte_v(l1pte) == 0) {
			pmap_physpage_addref(l1pte);
			error = pmap_ptpage_alloc(pmap, l1pte, PGU_L2PT);
			if (error) {
				pmap_l1pt_delref(pmap, l1pte, cpu_id);
				if (flags & PMAP_CANFAIL)
					goto out;
				panic("pmap_enter: unable to create L2 PT "
				    "page");
			}
			pmap->pm_nlev2++;
#ifdef DEBUG
			if (pmapdebug & PDB_PTPAGE)
				printf("pmap_enter: new level 2 table at "
				    "0x%lx\n", pmap_pte_pa(l1pte));
#endif
		}

		/*
		 * Check to see if the level 2 PTE is valid, and
		 * allocate a new level 3 page table page if it's not.
		 * A reference will be added to the level 3 table when
		 * the mapping is validated.
		 */
		l2pte = pmap_l2pte(pmap, va, l1pte);
		if (pmap_pte_v(l2pte) == 0) {
			pmap_physpage_addref(l2pte);
			error = pmap_ptpage_alloc(pmap, l2pte, PGU_L3PT);
			if (error) {
				pmap_l2pt_delref(pmap, l1pte, l2pte, cpu_id);
				if (flags & PMAP_CANFAIL)
					goto out;
				panic("pmap_enter: unable to create L3 PT "
				    "page");
			}
			pmap->pm_nlev3++;
#ifdef DEBUG
			if (pmapdebug & PDB_PTPAGE)
				printf("pmap_enter: new level 3 table at "
				    "0x%lx\n", pmap_pte_pa(l2pte));
#endif
		}

		/*
		 * Get the PTE that will map the page.
		 */
		pte = pmap_l3pte(pmap, va, l2pte);
	}

	/* Remember all of the old PTE; used for TBI check later. */
	opte = *pte;

	/*
	 * Check to see if the old mapping is valid.  If not, validate the
	 * new one immediately.
	 */
	if (pmap_pte_v(pte) == 0) {
		/*
		 * No need to invalidate the TLB in this case; an invalid
		 * mapping won't be in the TLB, and a previously valid
		 * mapping would have been flushed when it was invalidated.
		 */
		tflush = FALSE;

		/*
		 * No need to synchronize the I-stream, either, for basically
		 * the same reason.
		 */
		setisync = needisync = FALSE;

		if (pmap != pmap_kernel()) {
			/*
			 * New mappings gain a reference on the level 3
			 * table.
			 */
			pmap_physpage_addref(pte);
		}
		goto validate_enterpv;
	}

	opa = pmap_pte_pa(pte);
	hadasm = (pmap_pte_asm(pte) != 0);

	if (opa == pa) {
		/*
		 * Mapping has not changed; must be a protection or
		 * wiring change.
		 */
		if (pmap_pte_w_chg(pte, wired ? PG_WIRED : 0)) {
#ifdef DEBUG
			if (pmapdebug & PDB_ENTER)
				printf("pmap_enter: wiring change -> %d\n",
				    wired);
#endif
			/*
			 * Adjust the wiring count.
			 */
			if (wired)
				PMAP_STAT_INCR(pmap->pm_stats.wired_count, 1);
			else
				PMAP_STAT_DECR(pmap->pm_stats.wired_count, 1);
		}

		/*
		 * Set the PTE.
		 */
		goto validate;
	}

	/*
	 * The mapping has changed.  We need to invalidate the
	 * old mapping before creating the new one.
	 */
#ifdef DEBUG
	if (pmapdebug & PDB_ENTER)
		printf("pmap_enter: removing old mapping 0x%lx\n", va);
#endif
	if (pmap != pmap_kernel()) {
		/*
		 * Gain an extra reference on the level 3 table.
		 * pmap_remove_mapping() will delete a reference,
		 * and we don't want the table to be erroneously
		 * freed.
		 */
		pmap_physpage_addref(pte);
	}
	needisync |= pmap_remove_mapping(pmap, va, pte, TRUE, cpu_id, NULL);

 validate_enterpv:
	/*
	 * Enter the mapping into the pv_table if appropriate.
	 */
	if (managed) {
		error = pmap_pv_enter(pmap, pa, va, pte, TRUE);
		if (error) {
			pmap_l3pt_delref(pmap, va, pte, cpu_id, NULL);
			if (flags & PMAP_CANFAIL)
				goto out;
			panic("pmap_enter: unable to enter mapping in PV "
			    "table");
		}
	}

	/*
	 * Increment counters.
	 */
	PMAP_STAT_INCR(pmap->pm_stats.resident_count, 1);
	if (wired)
		PMAP_STAT_INCR(pmap->pm_stats.wired_count, 1);

 validate:
	/*
	 * Build the new PTE.
	 */
	npte = ((pa >> PGSHIFT) << PG_SHIFT) | pte_prot(pmap, prot) | PG_V;
	if (managed) {
		struct pv_head *pvh = pa_to_pvh(pa);
		int attrs;

#ifdef DIAGNOSTIC
		if ((flags & VM_PROT_ALL) & ~prot)
			panic("pmap_enter: access type exceeds prot");
#endif
		simple_lock(&pvh->pvh_slock);
		if (flags & VM_PROT_WRITE)
			pvh->pvh_attrs |= (PGA_REFERENCED|PGA_MODIFIED);
		else if (flags & VM_PROT_ALL)
			pvh->pvh_attrs |= PGA_REFERENCED;
		attrs = pvh->pvh_attrs;
		simple_unlock(&pvh->pvh_slock);

		/*
		 * Set up referenced/modified emulation for new mapping.
		 */
		if ((attrs & PGA_REFERENCED) == 0)
			npte |= PG_FOR | PG_FOW | PG_FOE;
		else if ((attrs & PGA_MODIFIED) == 0)
			npte |= PG_FOW;

		/* Always force FOE on non-exec mappings. */
		if (!pmap_pte_exec(pte))
			npte |= PG_FOE;

		/*
		 * Mapping was entered on PV list.
		 */
		npte |= PG_PVLIST;
	}
	if (wired)
		npte |= PG_WIRED;
#ifdef DEBUG
	if (pmapdebug & PDB_ENTER)
		printf("pmap_enter: new pte = 0x%lx\n", npte);
#endif

	/*
	 * If the PALcode portion of the new PTE is the same as the
	 * old PTE, no TBI is necessary.
	 */
	if (PG_PALCODE(opte) == PG_PALCODE(npte))
		tflush = FALSE;

	/*
	 * Set the new PTE.
	 */
	PMAP_SET_PTE(pte, npte);

	/*
	 * Invalidate the TLB entry for this VA and any appropriate
	 * caches.
	 */
	if (tflush) {
		PMAP_INVALIDATE_TLB(pmap, va, hadasm, isactive, cpu_id);
		PMAP_TLB_SHOOTDOWN(pmap, va, hadasm ? PG_ASM : 0);
	}
	if (setisync)
		PMAP_SET_NEEDISYNC(pmap);
	if (needisync)
		PMAP_SYNC_ISTREAM(pmap);

out:
	PMAP_UNLOCK(pmap);
	PMAP_MAP_TO_HEAD_UNLOCK();

	return error;
}

/*
 * pmap_kenter_pa:		[ INTERFACE ]
 *
 *	Enter a va -> pa mapping into the kernel pmap without any
 *	physical->virtual tracking.
 *
 *	Note: no locking is necessary in this function.
 */
void
pmap_kenter_pa(vaddr_t va, paddr_t pa, vm_prot_t prot)
{
	pt_entry_t *pte, npte;
	long cpu_id = cpu_number();
	boolean_t needisync = FALSE;
	pmap_t pmap = pmap_kernel();

#ifdef DEBUG
	if (pmapdebug & (PDB_FOLLOW|PDB_ENTER))
		printf("pmap_kenter_pa(%lx, %lx, %x)\n",
		    va, pa, prot);
#endif

#ifdef DIAGNOSTIC
	/*
	 * Sanity check the virtual address.
	 */
	if (va < VM_MIN_KERNEL_ADDRESS)
		panic("pmap_kenter_pa: kernel pmap, invalid va 0x%lx", va);
#endif

	pte = PMAP_KERNEL_PTE(va);

	if (pmap_pte_v(pte) == 0)
		PMAP_STAT_INCR(pmap->pm_stats.resident_count, 1);
	if (pmap_pte_w(pte) == 0)
		PMAP_STAT_DECR(pmap->pm_stats.wired_count, 1);

	if ((prot & VM_PROT_EXECUTE) != 0 || pmap_pte_exec(pte))
		needisync = TRUE;

	/*
	 * Build the new PTE.
	 */
	npte = ((pa >> PGSHIFT) << PG_SHIFT) | pte_prot(pmap_kernel(), prot) |
	    PG_V | PG_WIRED;

	/*
	 * Set the new PTE.
	 */
	PMAP_SET_PTE(pte, npte);
#if defined(MULTIPROCESSOR)
	alpha_mb();		/* XXX alpha_wmb()? */
#endif

	/*
	 * Invalidate the TLB entry for this VA and any appropriate
	 * caches.
	 */
	PMAP_INVALIDATE_TLB(pmap, va, TRUE, TRUE, cpu_id);
	PMAP_TLB_SHOOTDOWN(pmap, va, PG_ASM);

	if (needisync)
		PMAP_SYNC_ISTREAM_KERNEL();
}

/*
 * pmap_kremove:		[ INTERFACE ]
 *
 *	Remove a mapping entered with pmap_kenter_pa()
 *	starting at va, for size bytes (assumed to be page rounded).
 */
void
pmap_kremove(vaddr_t va, vsize_t size)
{
	pt_entry_t *pte;
	boolean_t needisync = FALSE;
	long cpu_id = cpu_number();
	pmap_t pmap = pmap_kernel();

#ifdef DEBUG
	if (pmapdebug & (PDB_FOLLOW|PDB_ENTER))
		printf("pmap_kremove(%lx, %lx)\n",
		    va, size);
#endif

#ifdef DIAGNOSTIC
	if (va < VM_MIN_KERNEL_ADDRESS)
		panic("pmap_kremove: user address");
#endif

	for (; size != 0; size -= PAGE_SIZE, va += PAGE_SIZE) {
		pte = PMAP_KERNEL_PTE(va);
		if (pmap_pte_v(pte)) {
#ifdef DIAGNOSTIC
			if (pmap_pte_pv(pte))
				panic("pmap_kremove: PG_PVLIST mapping for "
				    "0x%lx", va);
#endif
			if (pmap_pte_exec(pte))
				needisync = TRUE;

			/* Zap the mapping. */
			PMAP_SET_PTE(pte, PG_NV);
#if defined(MULTIPROCESSOR)
			alpha_mb();		/* XXX alpha_wmb()? */
#endif
			PMAP_INVALIDATE_TLB(pmap, va, TRUE, TRUE, cpu_id);
			PMAP_TLB_SHOOTDOWN(pmap, va, PG_ASM);

			/* Update stats. */
			PMAP_STAT_DECR(pmap->pm_stats.resident_count, 1);
			PMAP_STAT_DECR(pmap->pm_stats.wired_count, 1);
		}
	}

	if (needisync)
		PMAP_SYNC_ISTREAM_KERNEL();
}

/*
 * pmap_unwire:			[ INTERFACE ]
 *
 *	Clear the wired attribute for a map/virtual-address pair.
 *
 *	The mapping must already exist in the pmap.
 */
void
pmap_unwire(pmap_t pmap, vaddr_t va)
{
	pt_entry_t *pte;

#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_unwire(%p, %lx)\n", pmap, va);
#endif
	if (pmap == NULL)
		return;

	PMAP_LOCK(pmap);

	pte = pmap_l3pte(pmap, va, NULL);
#ifdef DIAGNOSTIC
	if (pte == NULL || pmap_pte_v(pte) == 0)
		panic("pmap_unwire");
#endif

	/*
	 * If wiring actually changed (always?) clear the wire bit and
	 * update the wire count.  Note that wiring is not a hardware
	 * characteristic so there is no need to invalidate the TLB.
	 */
	if (pmap_pte_w_chg(pte, 0)) {
		pmap_pte_set_w(pte, FALSE);
		PMAP_STAT_DECR(pmap->pm_stats.wired_count, 1);
	}
#ifdef DIAGNOSTIC
	else {
		printf("pmap_unwire: wiring for pmap %p va 0x%lx "
		    "didn't change!\n", pmap, va);
	}
#endif

	PMAP_UNLOCK(pmap);
}

/*
 * pmap_extract:		[ INTERFACE ]
 *
 *	Extract the physical address associated with the given
 *	pmap/virtual address pair.
 */
boolean_t
pmap_extract(pmap_t pmap, vaddr_t va, paddr_t *pap)
{
	pt_entry_t *l1pte, *l2pte, *l3pte;
	boolean_t rv = FALSE;
	paddr_t pa;

#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_extract(%p, %lx) -> ", pmap, va);
#endif
	PMAP_LOCK(pmap);

	l1pte = pmap_l1pte(pmap, va);
	if (pmap_pte_v(l1pte) == 0)
		goto out;

	l2pte = pmap_l2pte(pmap, va, l1pte);
	if (pmap_pte_v(l2pte) == 0)
		goto out;

	l3pte = pmap_l3pte(pmap, va, l2pte);
	if (pmap_pte_v(l3pte) == 0)
		goto out;

	pa = pmap_pte_pa(l3pte) | (va & PGOFSET);
	*pap = pa;
	rv = TRUE;
 out:
	PMAP_UNLOCK(pmap);
#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW) {
		if (rv)
			printf("0x%lx\n", pa);
		else
			printf("failed\n");
	}
#endif
	return (rv);
}

/*
 * pmap_copy:			[ INTERFACE ]
 *
 *	Copy the mapping range specified by src_addr/len
 *	from the source map to the range dst_addr/len
 *	in the destination map.
 *
 *	This routine is only advisory and need not do anything.
 */
void
pmap_copy(pmap_t dst_pmap, pmap_t src_pmap, vaddr_t dst_addr, vsize_t len,
    vaddr_t src_addr)
{
#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_copy(%p, %p, %lx, %lx, %lx)\n",
		       dst_pmap, src_pmap, dst_addr, len, src_addr);
#endif
}

/*
 * pmap_collect:		[ INTERFACE ]
 *
 *	Garbage collects the physical map system for pages which are no
 *	longer used.  Success need not be guaranteed -- that is, there
 *	may well be pages which are not referenced, but others may be
 *	collected.
 *
 *	Called by the pageout daemon when pages are scarce.
 */
void
pmap_collect(pmap_t pmap)
{

#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_collect(%p)\n", pmap);
#endif

	/*
	 * If called for the kernel pmap, just return.  We
	 * handle this case in the event that we ever want
	 * to have swappable kernel threads.
	 */
	if (pmap == pmap_kernel())
		return;

	/*
	 * This process is about to be swapped out; free all of
	 * the PT pages by removing the physical mappings for its
	 * entire address space.  Note: pmap_remove() performs
	 * all necessary locking.
	 */
	pmap_do_remove(pmap, VM_MIN_ADDRESS, VM_MAX_ADDRESS, FALSE);
}

/*
 * pmap_activate:		[ INTERFACE ]
 *
 *	Activate the pmap used by the specified process.  This includes
 *	reloading the MMU context if the current process, and marking
 *	the pmap in use by the processor.
 *
 *	Note: We may use only spin locks here, since we are called
 *	by a critical section in cpu_switch()!
 */
void
pmap_activate(struct proc *p)
{
	struct pmap *pmap = p->p_vmspace->vm_map.pmap;
	long cpu_id = cpu_number();

#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_activate(%p)\n", p);
#endif

	/*
	 * Mark the pmap in use by this processor.
	 */
	atomic_setbits_ulong(&pmap->pm_cpus, (1UL << cpu_id));

	/*
	 * Move the pmap to the end of the LRU list.
	 */
	simple_lock(&pmap_all_pmaps_slock);
	TAILQ_REMOVE(&pmap_all_pmaps, pmap, pm_list);
	TAILQ_INSERT_TAIL(&pmap_all_pmaps, pmap, pm_list);
	simple_unlock(&pmap_all_pmaps_slock);

	PMAP_LOCK(pmap);

	/*
	 * Allocate an ASN.
	 */
	pmap_asn_alloc(pmap, cpu_id);

	PMAP_ACTIVATE(pmap, p, cpu_id);

	PMAP_UNLOCK(pmap);
}

/*
 * pmap_deactivate:		[ INTERFACE ]
 *
 *	Mark that the pmap used by the specified process is no longer
 *	in use by the processor.
 *
 *	The comment above pmap_activate() wrt. locking applies here,
 *	as well.  Note that we use only a single `atomic' operation,
 *	so no locking is necessary.
 */
void
pmap_deactivate(struct proc *p)
{
	struct pmap *pmap = p->p_vmspace->vm_map.pmap;

#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_deactivate(%p)\n", p);
#endif

	/*
	 * Mark the pmap no longer in use by this processor.
	 */
	atomic_clearbits_ulong(&pmap->pm_cpus, (1UL << cpu_number()));
}

/*
 * pmap_zero_page:		[ INTERFACE ]
 *
 *	Zero the specified (machine independent) page by mapping the page
 *	into virtual memory and clear its contents, one machine dependent
 *	page at a time.
 *
 *	Note: no locking is necessary in this function.
 */
void
pmap_zero_page(struct vm_page *pg)
{
	paddr_t phys = VM_PAGE_TO_PHYS(pg);
	u_long *p0, *p1, *pend;

#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_zero_page(%lx)\n", phys);
#endif

	p0 = (u_long *)ALPHA_PHYS_TO_K0SEG(phys);
	pend = (u_long *)((u_long)p0 + PAGE_SIZE);

	/*
	 * Unroll the loop a bit, doing 16 quadwords per iteration.
	 * Do only 8 back-to-back stores, and alternate registers.
	 */
	do {
		__asm __volatile(
		"# BEGIN loop body\n"
		"	addq	%2, (8 * 8), %1		\n"
		"	stq	$31, (0 * 8)(%0)	\n"
		"	stq	$31, (1 * 8)(%0)	\n"
		"	stq	$31, (2 * 8)(%0)	\n"
		"	stq	$31, (3 * 8)(%0)	\n"
		"	stq	$31, (4 * 8)(%0)	\n"
		"	stq	$31, (5 * 8)(%0)	\n"
		"	stq	$31, (6 * 8)(%0)	\n"
		"	stq	$31, (7 * 8)(%0)	\n"
		"					\n"
		"	addq	%3, (8 * 8), %0		\n"
		"	stq	$31, (0 * 8)(%1)	\n"
		"	stq	$31, (1 * 8)(%1)	\n"
		"	stq	$31, (2 * 8)(%1)	\n"
		"	stq	$31, (3 * 8)(%1)	\n"
		"	stq	$31, (4 * 8)(%1)	\n"
		"	stq	$31, (5 * 8)(%1)	\n"
		"	stq	$31, (6 * 8)(%1)	\n"
		"	stq	$31, (7 * 8)(%1)	\n"
		"	# END loop body"
		: "=r" (p0), "=r" (p1)
		: "0" (p0), "1" (p1)
		: "memory");
	} while (p0 < pend);
}

/*
 * pmap_copy_page:		[ INTERFACE ]
 *
 *	Copy the specified (machine independent) page by mapping the page
 *	into virtual memory and using memcpy to copy the page, one machine
 *	dependent page at a time.
 *
 *	Note: no locking is necessary in this function.
 */
void
pmap_copy_page(struct vm_page *srcpg, struct vm_page *dstpg)
{
	paddr_t src = VM_PAGE_TO_PHYS(srcpg);
	paddr_t dst = VM_PAGE_TO_PHYS(dstpg);
	caddr_t s, d;

#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_copy_page(%lx, %lx)\n", src, dst);
#endif
        s = (caddr_t)ALPHA_PHYS_TO_K0SEG(src);
        d = (caddr_t)ALPHA_PHYS_TO_K0SEG(dst);
	memcpy(d, s, PAGE_SIZE);
}

/*
 * pmap_clear_modify:		[ INTERFACE ]
 *
 *	Clear the modify bits on the specified physical page.
 */
boolean_t
pmap_clear_modify(struct vm_page *pg)
{
	struct pv_head *pvh;
	paddr_t pa = VM_PAGE_TO_PHYS(pg);
	boolean_t rv = FALSE;
	long cpu_id = cpu_number();

#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_clear_modify(%p)\n", pg);
#endif

	pvh = pa_to_pvh(pa);

	PMAP_HEAD_TO_MAP_LOCK();
	simple_lock(&pvh->pvh_slock);

	if (pvh->pvh_attrs & PGA_MODIFIED) {
		rv = TRUE;
		pmap_changebit(pa, PG_FOW, ~0, cpu_id);
		pvh->pvh_attrs &= ~PGA_MODIFIED;
	}

	simple_unlock(&pvh->pvh_slock);
	PMAP_HEAD_TO_MAP_UNLOCK();

	return (rv);
}

/*
 * pmap_clear_reference:	[ INTERFACE ]
 *
 *	Clear the reference bit on the specified physical page.
 */
boolean_t
pmap_clear_reference(struct vm_page *pg)
{
	struct pv_head *pvh;
	paddr_t pa = VM_PAGE_TO_PHYS(pg);
	boolean_t rv = FALSE;
	long cpu_id = cpu_number();

#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_clear_reference(%p)\n", pg);
#endif

	pvh = pa_to_pvh(pa);

	PMAP_HEAD_TO_MAP_LOCK();
	simple_lock(&pvh->pvh_slock);

	if (pvh->pvh_attrs & PGA_REFERENCED) {
		rv = TRUE;
		pmap_changebit(pa, PG_FOR | PG_FOW | PG_FOE, ~0, cpu_id);
		pvh->pvh_attrs &= ~PGA_REFERENCED;
	}

	simple_unlock(&pvh->pvh_slock);
	PMAP_HEAD_TO_MAP_UNLOCK();

	return (rv);
}

/*
 * pmap_is_referenced:		[ INTERFACE ]
 *
 *	Return whether or not the specified physical page is referenced
 *	by any physical maps.
 */
boolean_t
pmap_is_referenced(struct vm_page *pg)
{
	struct pv_head *pvh;
	paddr_t pa = VM_PAGE_TO_PHYS(pg);
	boolean_t rv;

	pvh = pa_to_pvh(pa);
	rv = ((pvh->pvh_attrs & PGA_REFERENCED) != 0);
#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW) {
		printf("pmap_is_referenced(%p) -> %c\n", pg, "FT"[rv]);
	}
#endif
	return (rv);
}

/*
 * pmap_is_modified:		[ INTERFACE ]
 *
 *	Return whether or not the specified physical page is modified
 *	by any physical maps.
 */
boolean_t
pmap_is_modified(struct vm_page *pg)
{
	struct pv_head *pvh;
	paddr_t pa = VM_PAGE_TO_PHYS(pg);
	boolean_t rv;

	pvh = pa_to_pvh(pa);
	rv = ((pvh->pvh_attrs & PGA_MODIFIED) != 0);
#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW) {
		printf("pmap_is_modified(%p) -> %c\n", pg, "FT"[rv]);
	}
#endif
	return (rv);
}

/*
 * pmap_phys_address:		[ INTERFACE ]
 *
 *	Return the physical address corresponding to the specified
 *	cookie.  Used by the device pager to decode a device driver's
 *	mmap entry point return value.
 *
 *	Note: no locking is necessary in this function.
 */
paddr_t
pmap_phys_address(int ppn)
{

	return (alpha_ptob(ppn));
}

/*
 * Miscellaneous support routines follow
 */

/*
 * alpha_protection_init:
 *
 *	Initialize Alpha protection code array.
 *
 *	Note: no locking is necessary in this function.
 */
void
alpha_protection_init(void)
{
	int prot, *kp, *up;

	kp = protection_codes[0];
	up = protection_codes[1];

	for (prot = 0; prot < 8; prot++) {
		kp[prot] = 0; up[prot] = 0;
		switch (prot) {
		case VM_PROT_NONE | VM_PROT_NONE | VM_PROT_NONE:
			kp[prot] |= PG_ASM;
			up[prot] |= 0;
			break;

		case VM_PROT_READ | VM_PROT_NONE | VM_PROT_EXECUTE:
		case VM_PROT_NONE | VM_PROT_NONE | VM_PROT_EXECUTE:
			kp[prot] |= PG_EXEC;		/* software */
			up[prot] |= PG_EXEC;		/* software */
			/* FALLTHROUGH */

		case VM_PROT_READ | VM_PROT_NONE | VM_PROT_NONE:
			kp[prot] |= PG_ASM | PG_KRE;
			up[prot] |= PG_URE | PG_KRE;
			break;

		case VM_PROT_NONE | VM_PROT_WRITE | VM_PROT_NONE:
			kp[prot] |= PG_ASM | PG_KWE;
			up[prot] |= PG_UWE | PG_KWE;
			break;

		case VM_PROT_NONE | VM_PROT_WRITE | VM_PROT_EXECUTE:
		case VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE:
			kp[prot] |= PG_EXEC;		/* software */
			up[prot] |= PG_EXEC;		/* software */
			/* FALLTHROUGH */

		case VM_PROT_READ | VM_PROT_WRITE | VM_PROT_NONE:
			kp[prot] |= PG_ASM | PG_KWE | PG_KRE;
			up[prot] |= PG_UWE | PG_URE | PG_KWE | PG_KRE;
			break;
		}
	}
}

/*
 * pmap_remove_mapping:
 *
 *	Invalidate a single page denoted by pmap/va.
 *
 *	If (pte != NULL), it is the already computed PTE for the page.
 *
 *	Note: locking in this function is complicated by the fact
 *	that we can be called when the PV list is already locked.
 *	(pmap_page_protect()).  In this case, the caller must be
 *	careful to get the next PV entry while we remove this entry
 *	from beneath it.  We assume that the pmap itself is already
 *	locked; dolock applies only to the PV list.
 *
 *	Returns TRUE or FALSE, indicating if an I-stream sync needs
 *	to be initiated (for this CPU or for other CPUs).
 */
boolean_t
pmap_remove_mapping(pmap_t pmap, vaddr_t va, pt_entry_t *pte,
    boolean_t dolock, long cpu_id, struct prm_thief *prmt)
{
	paddr_t pa;
	boolean_t onpv;
	boolean_t hadasm;
	boolean_t isactive;
	boolean_t needisync = FALSE;
	struct pv_entry **pvp;
	pt_entry_t **ptp;

#ifdef DEBUG
	if (pmapdebug & (PDB_FOLLOW|PDB_REMOVE|PDB_PROTECT))
		printf("pmap_remove_mapping(%p, %lx, %p, %d, %ld, %p)\n",
		       pmap, va, pte, dolock, cpu_id, pvp);
#endif

	if (prmt != NULL) {
		if (prmt->prmt_flags & PRMT_PV)
			pvp = &prmt->prmt_pv;
		else
			pvp = NULL;
		if (prmt->prmt_flags & PRMT_PTP)
			ptp = &prmt->prmt_ptp;
		else
			ptp = NULL;
	} else {
		pvp = NULL;
		ptp = NULL;
	}

	/*
	 * PTE not provided, compute it from pmap and va.
	 */
	if (pte == PT_ENTRY_NULL) {
		pte = pmap_l3pte(pmap, va, NULL);
		if (pmap_pte_v(pte) == 0)
			return (FALSE);
	}

	pa = pmap_pte_pa(pte);
	onpv = (pmap_pte_pv(pte) != 0);
	hadasm = (pmap_pte_asm(pte) != 0);
	isactive = PMAP_ISACTIVE(pmap, cpu_id);

	/*
	 * Determine what we need to do about the I-stream.  If
	 * PG_EXEC was set, we mark a user pmap as needing an
	 * I-sync on the way out to userspace.  We always need
	 * an immediate I-sync for the kernel pmap.
	 */
	if (pmap_pte_exec(pte)) {
		if (pmap == pmap_kernel())
			needisync = TRUE;
		else {
			PMAP_SET_NEEDISYNC(pmap);
			needisync = (pmap->pm_cpus != 0);
		}
	}

	/*
	 * Update statistics
	 */
	if (pmap_pte_w(pte))
		PMAP_STAT_DECR(pmap->pm_stats.wired_count, 1);
	PMAP_STAT_DECR(pmap->pm_stats.resident_count, 1);

	/*
	 * Invalidate the PTE after saving the reference modify info.
	 */
#ifdef DEBUG
	if (pmapdebug & PDB_REMOVE)
		printf("remove: invalidating pte at %p\n", pte);
#endif
	PMAP_SET_PTE(pte, PG_NV);

	PMAP_INVALIDATE_TLB(pmap, va, hadasm, isactive, cpu_id);
	PMAP_TLB_SHOOTDOWN(pmap, va, hadasm ? PG_ASM : 0);

	/*
	 * If we're removing a user mapping, check to see if we
	 * can free page table pages.
	 */
	if (pmap != pmap_kernel()) {
		/*
		 * Delete the reference on the level 3 table.  It will
		 * delete references on the level 2 and 1 tables as
		 * appropriate.
		 */
		pmap_l3pt_delref(pmap, va, pte, cpu_id, ptp);
	}

	/*
	 * If the mapping wasn't enterd on the PV list, we're all done.
	 */
	if (onpv == FALSE) {
#ifdef DIAGNOSTIC
		if (pvp != NULL)
			panic("pmap_removing_mapping: onpv / pvp inconsistent");
#endif
		return (needisync);
	}

	/*
	 * Remove it from the PV table.
	 */
	pmap_pv_remove(pmap, pa, va, dolock, pvp);

	return (needisync);
}

/*
 * pmap_changebit:
 *
 *	Set or clear the specified PTE bits for all mappings on the
 *	specified page.
 *
 *	Note: we assume that the pv_head is already locked, and that
 *	the caller has acquired a PV->pmap mutex so that we can lock
 *	the pmaps as we encounter them.
 *
 *	XXX This routine could stand to have some I-stream
 *	XXX optimization done.
 */
void
pmap_changebit(paddr_t pa, u_long set, u_long mask, long cpu_id)
{
	struct pv_head *pvh;
	pv_entry_t pv;
	pt_entry_t *pte, npte;
	vaddr_t va;
	boolean_t hadasm, isactive;
	boolean_t needisync, needkisync = FALSE;

#ifdef DEBUG
	if (pmapdebug & PDB_BITS)
		printf("pmap_changebit(0x%lx, 0x%lx, 0x%lx)\n",
		    pa, set, mask);
#endif
	if (!PAGE_IS_MANAGED(pa))
		return;

	pvh = pa_to_pvh(pa);
	/*
	 * Loop over all current mappings setting/clearing as appropos.
	 */
	for (pv = LIST_FIRST(&pvh->pvh_list); pv != NULL;
	     pv = LIST_NEXT(pv, pv_list)) {
		va = pv->pv_va;

		/*
		 * XXX don't write protect pager mappings
		 */
		if (pv->pv_pmap == pmap_kernel() &&
/* XXX */	    mask == ~(PG_KWE | PG_UWE)) {
			if (va >= uvm.pager_sva && va < uvm.pager_eva)
				continue;
		}

		PMAP_LOCK(pv->pv_pmap);

		pte = pv->pv_pte;
		npte = (*pte | set) & mask;
		if (*pte != npte) {
			hadasm = (pmap_pte_asm(pte) != 0);
			isactive = PMAP_ISACTIVE(pv->pv_pmap, cpu_id);
			/*
			 * Determine what we need to do about the I-stream.
			 * If PG_EXEC was set, we mark a user pmap as needing
			 * an I-sync on the way out to userspace.  We always
			 * need an immediate I-sync for the kernel pmap.
			 */
			needisync = FALSE;
			if (pmap_pte_exec(pte)) {
				if (pv->pv_pmap == pmap_kernel())
					needkisync = TRUE;
				else {
					PMAP_SET_NEEDISYNC(pv->pv_pmap);
					if (pv->pv_pmap->pm_cpus != 0)
						needisync = TRUE;
				}
			} else {
				/* Never clear FOE on non-exec mappings. */
				npte |= PG_FOE;
			}
			PMAP_SET_PTE(pte, npte);
			if (needisync)
				PMAP_SYNC_ISTREAM_USER(pv->pv_pmap);
			PMAP_INVALIDATE_TLB(pv->pv_pmap, va, hadasm, isactive,
			    cpu_id);
			PMAP_TLB_SHOOTDOWN(pv->pv_pmap, va,
			    hadasm ? PG_ASM : 0);
		}
		PMAP_UNLOCK(pv->pv_pmap);
	}

	if (needkisync)
		PMAP_SYNC_ISTREAM_KERNEL();
}

/*
 * pmap_emulate_reference:
 *
 *	Emulate reference and/or modified bit hits.
 *
 *	return non-zero if this was a FOE fault and the pte is not
 *	executable.
 */
int
pmap_emulate_reference(struct proc *p, vaddr_t v, int user, int type)
{
	pt_entry_t faultoff, *pte;
	paddr_t pa;
	struct pv_head *pvh;
	boolean_t didlock = FALSE;
	long cpu_id = cpu_number();

#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_emulate_reference: %p, 0x%lx, %d, %d\n",
		    p, v, user, type);
#endif

	/*
	 * Convert process and virtual address to physical address.
	 */
	if (v >= VM_MIN_KERNEL_ADDRESS) {
		if (user)
			panic("pmap_emulate_reference: user ref to kernel");
		/*
		 * No need to lock here; kernel PT pages never go away.
		 */
		pte = PMAP_KERNEL_PTE(v);
	} else {
#ifdef DIAGNOSTIC
		if (p == NULL)
			panic("pmap_emulate_reference: bad proc");
		if (p->p_vmspace == NULL)
			panic("pmap_emulate_reference: bad p_vmspace");
#endif
		PMAP_LOCK(p->p_vmspace->vm_map.pmap);
		didlock = TRUE;
		pte = pmap_l3pte(p->p_vmspace->vm_map.pmap, v, NULL);
		/*
		 * We'll unlock below where we're done with the PTE.
		 */
	}
	if (!pmap_pte_exec(pte) && type == ALPHA_MMCSR_FOE) {
		if (didlock)
			PMAP_UNLOCK(p->p_vmspace->vm_map.pmap);
		return (1);
	}
#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW) {
		printf("\tpte = %p, ", pte);
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
	if (type == ALPHA_MMCSR_FOW) {
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

	/*
	 * We're now done with the PTE.  If it was a user pmap, unlock
	 * it now.
	 */
	if (didlock)
		PMAP_UNLOCK(p->p_vmspace->vm_map.pmap);

#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("\tpa = 0x%lx\n", pa);
#endif
#ifdef DIAGNOSTIC
	if (!PAGE_IS_MANAGED(pa))
		panic("pmap_emulate_reference(%p, 0x%lx, %d, %d): pa 0x%lx not managed", p, v, user, type, pa);
#endif

	/*
	 * Twiddle the appropriate bits to reflect the reference
	 * and/or modification..
	 *
	 * The rules:
	 * 	(1) always mark page as used, and
	 *	(2) if it was a write fault, mark page as modified.
	 */
	pvh = pa_to_pvh(pa);

	PMAP_HEAD_TO_MAP_LOCK();
	simple_lock(&pvh->pvh_slock);

	if (type == ALPHA_MMCSR_FOW) {
		pvh->pvh_attrs |= (PGA_REFERENCED|PGA_MODIFIED);
		faultoff = PG_FOR | PG_FOW | PG_FOE;
	} else {
		pvh->pvh_attrs |= PGA_REFERENCED;
		faultoff = PG_FOR | PG_FOE;
	}
	/*
	 * If the page is not PG_EXEC, pmap_changebit will automagically
	 * set PG_FOE (gross, but necessary if I don't want to change the
	 * whole API).
	 */
	pmap_changebit(pa, 0, ~faultoff, cpu_id);

	simple_unlock(&pvh->pvh_slock);
	PMAP_HEAD_TO_MAP_UNLOCK();

	return (0);
}

#ifdef DEBUG
/*
 * pmap_pv_dump:
 *
 *	Dump the physical->virtual data for the specified page.
 */
void
pmap_pv_dump(paddr_t pa)
{
	struct pv_head *pvh;
	pv_entry_t pv;
	static const char *usage[] = {
		"normal", "pvent", "l1pt", "l2pt", "l3pt",
	};

	pvh = pa_to_pvh(pa);

	simple_lock(&pvh->pvh_slock);

	printf("pa 0x%lx (attrs = 0x%x, usage = " /* ) */, pa, pvh->pvh_attrs);
	if (pvh->pvh_usage < PGU_NORMAL || pvh->pvh_usage > PGU_L3PT)
/* ( */		printf("??? %d):\n", pvh->pvh_usage);
	else
/* ( */		printf("%s):\n", usage[pvh->pvh_usage]);

	for (pv = LIST_FIRST(&pvh->pvh_list); pv != NULL;
	     pv = LIST_NEXT(pv, pv_list))
		printf("     pmap %p, va 0x%lx\n",
		    pv->pv_pmap, pv->pv_va);
	printf("\n");

	simple_unlock(&pvh->pvh_slock);
}
#endif
 
/*
 * vtophys:
 *
 *	Return the physical address corresponding to the K0SEG or
 *	K1SEG address provided.
 *
 *	Note: no locking is necessary in this function.
 */
paddr_t
vtophys(vaddr_t vaddr)
{
	pt_entry_t *pte;
	paddr_t paddr = 0;

	if (vaddr < ALPHA_K0SEG_BASE)
		printf("vtophys: invalid vaddr 0x%lx", vaddr);
	else if (vaddr <= ALPHA_K0SEG_END)
		paddr = ALPHA_K0SEG_TO_PHYS(vaddr);
	else {
		pte = PMAP_KERNEL_PTE(vaddr);
		if (pmap_pte_v(pte))
			paddr = pmap_pte_pa(pte) | (vaddr & PGOFSET);
	}

#if 0
	printf("vtophys(0x%lx) -> 0x%lx\n", vaddr, paddr);
#endif

	return (paddr);
}

/******************** pv_entry management ********************/

/*
 * pmap_pv_enter:
 *
 *	Add a physical->virtual entry to the pv_table.
 */
int
pmap_pv_enter(pmap_t pmap, paddr_t pa, vaddr_t va, pt_entry_t *pte,
    boolean_t dolock)
{
	struct pv_head *pvh;
	pv_entry_t newpv;

	/*
	 * Allocate and fill in the new pv_entry.
	 */
	newpv = pmap_pv_alloc();
	if (newpv == NULL)
		return (ENOMEM);
	newpv->pv_va = va;
	newpv->pv_pmap = pmap;
	newpv->pv_pte = pte;

	pvh = pa_to_pvh(pa);

	if (dolock)
		simple_lock(&pvh->pvh_slock);

#ifdef DEBUG
	{
	pv_entry_t pv;
	/*
	 * Make sure the entry doesn't already exist.
	 */
	for (pv = LIST_FIRST(&pvh->pvh_list); pv != NULL;
	     pv = LIST_NEXT(pv, pv_list))
		if (pmap == pv->pv_pmap && va == pv->pv_va) {
			printf("pmap = %p, va = 0x%lx\n", pmap, va);
			panic("pmap_pv_enter: already in pv table");
		}
	}
#endif

	/*
	 * ...and put it in the list.
	 */
	LIST_INSERT_HEAD(&pvh->pvh_list, newpv, pv_list);

	if (dolock)
		simple_unlock(&pvh->pvh_slock);

	return (0);
}

/*
 * pmap_pv_remove:
 *
 *	Remove a physical->virtual entry from the pv_table.
 */
void
pmap_pv_remove(pmap_t pmap, paddr_t pa, vaddr_t va, boolean_t dolock,
    struct pv_entry **pvp)
{
	struct pv_head *pvh;
	pv_entry_t pv;

	pvh = pa_to_pvh(pa);

	if (dolock)
		simple_lock(&pvh->pvh_slock);

	/*
	 * Find the entry to remove.
	 */
	for (pv = LIST_FIRST(&pvh->pvh_list); pv != NULL;
	     pv = LIST_NEXT(pv, pv_list))
		if (pmap == pv->pv_pmap && va == pv->pv_va)
			break;

#ifdef DEBUG
	if (pv == NULL)
		panic("pmap_pv_remove: not in pv table");
#endif

	LIST_REMOVE(pv, pv_list);

	if (dolock)
		simple_unlock(&pvh->pvh_slock);

	/*
	 * If pvp is not NULL, this is pmap_pv_alloc() stealing an
	 * entry from another mapping, and we return the now unused
	 * entry in it.  Otherwise, free the pv_entry.
	 */
	if (pvp != NULL)
		*pvp = pv;
	else
		pmap_pv_free(pv);
}

/*
 * pmap_pv_alloc:
 *
 *	Allocate a pv_entry.
 */
struct pv_entry *
pmap_pv_alloc(void)
{
	struct pv_head *pvh;
	struct pv_entry *pv;
	int bank, npg, pg;
	pt_entry_t *pte;
	pmap_t pvpmap;
	u_long cpu_id;
	struct prm_thief prmt;

	pv = pool_get(&pmap_pv_pool, PR_NOWAIT);
	if (pv != NULL)
		return (pv);

	prmt.prmt_flags = PRMT_PV;

	/*
	 * We were unable to allocate one from the pool.  Try to
	 * steal one from another mapping.  At this point we know that:
	 *
	 *	(1) We have not locked the pv table, and we already have
	 *	    the map-to-head lock, so it is safe for us to do so here.
	 *
	 *	(2) The pmap that wants this entry *is* locked.  We must
	 *	    use simple_lock_try() to prevent deadlock from occurring.
	 *
	 * XXX Note that in case #2, there is an exception; it *is* safe to
	 * steal a mapping from the pmap that wants this entry!  We may want
	 * to consider passing the pmap to this function so that we can take
	 * advantage of this.
	 */

	/* XXX This search could probably be improved. */
	for (bank = 0; bank < vm_nphysseg; bank++) {
		npg = vm_physmem[bank].end - vm_physmem[bank].start;
		for (pg = 0; pg < npg; pg++) {
			pvh = &vm_physmem[bank].pmseg.pvhead[pg];
			simple_lock(&pvh->pvh_slock);
			for (pv = LIST_FIRST(&pvh->pvh_list);
			     pv != NULL; pv = LIST_NEXT(pv, pv_list)) {
				pvpmap = pv->pv_pmap;

				/* Don't steal from kernel pmap. */
				if (pvpmap == pmap_kernel())
					continue;

				if (simple_lock_try(&pvpmap->pm_slock) == 0)
					continue;

				pte = pv->pv_pte;

				/* Don't steal wired mappings. */
				if (pmap_pte_w(pte)) {
					simple_unlock(&pvpmap->pm_slock);
					continue;
				}

				cpu_id = cpu_number();

				/*
				 * Okay!  We have a mapping we can steal;
				 * remove it and grab the pv_entry.
				 */
				if (pmap_remove_mapping(pvpmap, pv->pv_va,
				    pte, FALSE, cpu_id, &prmt))
					PMAP_SYNC_ISTREAM(pvpmap);

				/* Unlock everything and return. */
				simple_unlock(&pvpmap->pm_slock);
				simple_unlock(&pvh->pvh_slock);
				return (prmt.prmt_pv);
			}
			simple_unlock(&pvh->pvh_slock);
		}
	}

	return (NULL);
}

/*
 * pmap_pv_free:
 *
 *	Free a pv_entry.
 */
void
pmap_pv_free(struct pv_entry *pv)
{

	pool_put(&pmap_pv_pool, pv);
}

/*
 * pmap_pv_page_alloc:
 *
 *	Allocate a page for the pv_entry pool.
 */
void *
pmap_pv_page_alloc(struct pool *pp, int flags)
{
	paddr_t pg;

	if (pmap_physpage_alloc(PGU_PVENT, &pg))
		return ((void *)ALPHA_PHYS_TO_K0SEG(pg));
	return (NULL);
}

/*
 * pmap_pv_page_free:
 *
 *	Free a pv_entry pool page.
 */
void
pmap_pv_page_free(struct pool *pp, void *v)
{

	pmap_physpage_free(ALPHA_K0SEG_TO_PHYS((vaddr_t)v));
}

/******************** misc. functions ********************/

/*
 * pmap_physpage_alloc:
 *
 *	Allocate a single page from the VM system and return the
 *	physical address for that page.
 */
boolean_t
pmap_physpage_alloc(int usage, paddr_t *pap)
{
	struct vm_page *pg;
	struct pv_head *pvh;
	paddr_t pa;

	/*
	 * Don't ask for a zero'd page in the L1PT case -- we will
	 * properly initialize it in the constructor.
	 */

	pg = uvm_pagealloc(NULL, 0, NULL, usage == PGU_L1PT ?
	    UVM_PGA_USERESERVE : UVM_PGA_USERESERVE|UVM_PGA_ZERO);
	if (pg != NULL) {
		pa = VM_PAGE_TO_PHYS(pg);

		pvh = pa_to_pvh(pa);
		simple_lock(&pvh->pvh_slock);
#ifdef DIAGNOSTIC
		if (pvh->pvh_usage != PGU_NORMAL) {
			printf("pmap_physpage_alloc: page 0x%lx is "
			    "in use (%s)\n", pa,
			    pmap_pgu_strings[pvh->pvh_usage]);
			panic("pmap_physpage_alloc");
		}
		if (pvh->pvh_refcnt != 0) {
			printf("pmap_physpage_alloc: page 0x%lx has "
			    "%d references\n", pa, pvh->pvh_refcnt);
			panic("pmap_physpage_alloc");
		}
#endif
		pvh->pvh_usage = usage;
		simple_unlock(&pvh->pvh_slock);
		*pap = pa;
		return (TRUE);
	}
	return (FALSE);
}

/*
 * pmap_physpage_free:
 *
 *	Free the single page table page at the specified physical address.
 */
void
pmap_physpage_free(paddr_t pa)
{
	struct pv_head *pvh;
	struct vm_page *pg;

	if ((pg = PHYS_TO_VM_PAGE(pa)) == NULL)
		panic("pmap_physpage_free: bogus physical page address");

	pvh = pa_to_pvh(pa);

	simple_lock(&pvh->pvh_slock);
#ifdef DIAGNOSTIC
	if (pvh->pvh_usage == PGU_NORMAL)
		panic("pmap_physpage_free: not in use?!");
	if (pvh->pvh_refcnt != 0)
		panic("pmap_physpage_free: page still has references");
#endif
	pvh->pvh_usage = PGU_NORMAL;
	simple_unlock(&pvh->pvh_slock);

	uvm_pagefree(pg);
}

/*
 * pmap_physpage_addref:
 *
 *	Add a reference to the specified special use page.
 */
int
pmap_physpage_addref(void *kva)
{
	struct pv_head *pvh;
	paddr_t pa;
	int rval;

	pa = ALPHA_K0SEG_TO_PHYS(trunc_page((vaddr_t)kva));
	pvh = pa_to_pvh(pa);

	simple_lock(&pvh->pvh_slock);
#ifdef DIAGNOSTIC
	if (pvh->pvh_usage == PGU_NORMAL)
		panic("pmap_physpage_addref: not a special use page");
#endif

	rval = ++pvh->pvh_refcnt;
	simple_unlock(&pvh->pvh_slock);

	return (rval);
}

/*
 * pmap_physpage_delref:
 *
 *	Delete a reference to the specified special use page.
 */
int
pmap_physpage_delref(void *kva)
{
	struct pv_head *pvh;
	paddr_t pa;
	int rval;

	pa = ALPHA_K0SEG_TO_PHYS(trunc_page((vaddr_t)kva));
	pvh = pa_to_pvh(pa);

	simple_lock(&pvh->pvh_slock);
#ifdef DIAGNOSTIC
	if (pvh->pvh_usage == PGU_NORMAL)
		panic("pmap_physpage_delref: not a special use page");
#endif

	rval = --pvh->pvh_refcnt;

#ifdef DIAGNOSTIC
	/*
	 * Make sure we never have a negative reference count.
	 */
	if (pvh->pvh_refcnt < 0)
		panic("pmap_physpage_delref: negative reference count");
#endif
	simple_unlock(&pvh->pvh_slock);

	return (rval);
}

/******************** page table page management ********************/

/*
 * pmap_growkernel:		[ INTERFACE ]
 *
 *	Grow the kernel address space.  This is a hint from the
 *	upper layer to pre-allocate more kernel PT pages.
 */
vaddr_t
pmap_growkernel(vaddr_t maxkvaddr)
{
	struct pmap *kpm = pmap_kernel(), *pm;
	paddr_t ptaddr;
	pt_entry_t *l1pte, *l2pte, pte;
	vaddr_t va;
	int s, l1idx;

	if (maxkvaddr <= virtual_end)
		goto out;		/* we are OK */

	s = splhigh();			/* to be safe */
	simple_lock(&pmap_growkernel_slock);

	va = virtual_end;

	while (va < maxkvaddr) {
		/*
		 * If there is no valid L1 PTE (i.e. no L2 PT page),
		 * allocate a new L2 PT page and insert it into the
		 * L1 map.
		 */
		l1pte = pmap_l1pte(kpm, va);
		if (pmap_pte_v(l1pte) == 0) {
			/*
			 * XXX PGU_NORMAL?  It's not a "traditional" PT page.
			 */
			if (uvm.page_init_done == FALSE) {
				/*
				 * We're growing the kernel pmap early (from
				 * uvm_pageboot_alloc()).  This case must
				 * be handled a little differently.
				 */
				ptaddr = ALPHA_K0SEG_TO_PHYS(
				    pmap_steal_memory(PAGE_SIZE, NULL, NULL));
			} else if (pmap_physpage_alloc(PGU_NORMAL,
				   &ptaddr) == FALSE)
				goto die;
			pte = (atop(ptaddr) << PG_SHIFT) |
			    PG_V | PG_ASM | PG_KRE | PG_KWE | PG_WIRED;
			*l1pte = pte;

			l1idx = l1pte_index(va);

			/* Update all the user pmaps. */
			simple_lock(&pmap_all_pmaps_slock);
			for (pm = TAILQ_FIRST(&pmap_all_pmaps);
			     pm != NULL; pm = TAILQ_NEXT(pm, pm_list)) {
				/* Skip the kernel pmap. */
				if (pm == pmap_kernel())
					continue;

				PMAP_LOCK(pm);
				if (pm->pm_lev1map == kernel_lev1map) {
					PMAP_UNLOCK(pm);
					continue;
				}
				pm->pm_lev1map[l1idx] = pte;
				PMAP_UNLOCK(pm);
			}
			simple_unlock(&pmap_all_pmaps_slock);
		}

		/*
		 * Have an L2 PT page now, add the L3 PT page.
		 */
		l2pte = pmap_l2pte(kpm, va, l1pte);
		KASSERT(pmap_pte_v(l2pte) == 0);
		if (uvm.page_init_done == FALSE) {
			/*
			 * See above.
			 */
			ptaddr = ALPHA_K0SEG_TO_PHYS(
			    pmap_steal_memory(PAGE_SIZE, NULL, NULL));
		} else if (pmap_physpage_alloc(PGU_NORMAL, &ptaddr) == FALSE)
			goto die;
		*l2pte = (atop(ptaddr) << PG_SHIFT) |
		    PG_V | PG_ASM | PG_KRE | PG_KWE | PG_WIRED;
		va += ALPHA_L2SEG_SIZE;
	}

	/* Invalidate the L1 PT cache. */
	pool_cache_invalidate(&pmap_l1pt_cache);

	virtual_end = va;

	simple_unlock(&pmap_growkernel_slock);
	splx(s);

 out:
	return (virtual_end);

 die:
	panic("pmap_growkernel: out of memory");
}

/*
 * pmap_lev1map_create:
 *
 *	Create a new level 1 page table for the specified pmap.
 *
 *	Note: the pmap must already be locked.
 */
int
pmap_lev1map_create(pmap_t pmap, long cpu_id)
{
	pt_entry_t *l1pt;

#ifdef DIAGNOSTIC
	if (pmap == pmap_kernel())
		panic("pmap_lev1map_create: got kernel pmap");

	if (pmap->pm_asn[cpu_id] != PMAP_ASN_RESERVED)
		panic("pmap_lev1map_create: pmap uses non-reserved ASN");
#endif

	simple_lock(&pmap_growkernel_slock);

	l1pt = pool_cache_get(&pmap_l1pt_cache, PR_NOWAIT);
	if (l1pt == NULL) {
		simple_unlock(&pmap_growkernel_slock);
		return (ENOMEM);
	}

	pmap->pm_lev1map = l1pt;

	simple_unlock(&pmap_growkernel_slock);

	/*
	 * The page table base has changed; if the pmap was active,
	 * reactivate it.
	 */
	if (PMAP_ISACTIVE(pmap, cpu_id)) {
		pmap_asn_alloc(pmap, cpu_id);
		PMAP_ACTIVATE(pmap, curproc, cpu_id);
	}
	return (0);
}

/*
 * pmap_lev1map_destroy:
 *
 *	Destroy the level 1 page table for the specified pmap.
 *
 *	Note: the pmap must already be locked.
 */
void
pmap_lev1map_destroy(pmap_t pmap, long cpu_id)
{
	pt_entry_t *l1pt = pmap->pm_lev1map;

#ifdef DIAGNOSTIC
	if (pmap == pmap_kernel())
		panic("pmap_lev1map_destroy: got kernel pmap");
#endif

	/*
	 * Go back to referencing the global kernel_lev1map.
	 */
	pmap->pm_lev1map = kernel_lev1map;

	/*
	 * The page table base has changed; if the pmap was active,
	 * reactivate it.  Note that allocation of a new ASN is
	 * not necessary here:
	 *
	 *	(1) We've gotten here because we've deleted all
	 *	    user mappings in the pmap, invalidating the
	 *	    TLB entries for them as we go.
	 *
	 *	(2) kernel_lev1map contains only kernel mappings, which
	 *	    were identical in the user pmap, and all of
	 *	    those mappings have PG_ASM, so the ASN doesn't
	 *	    matter.
	 *
	 * We do, however, ensure that the pmap is using the
	 * reserved ASN, to ensure that no two pmaps never have
	 * clashing TLB entries.
	 */
	PMAP_INVALIDATE_ASN(pmap, cpu_id);
	if (PMAP_ISACTIVE(pmap, cpu_id))
		PMAP_ACTIVATE(pmap, curproc, cpu_id);

	/*
	 * Free the old level 1 page table page.
	 */
	pool_cache_put(&pmap_l1pt_cache, l1pt);
}

/*
 * pmap_l1pt_ctor:
 *
 *	Pool cache constructor for L1 PT pages.
 */
int
pmap_l1pt_ctor(void *arg, void *object, int flags)
{
	pt_entry_t *l1pt = object, pte;
	int i;

	/*
	 * Initialize the new level 1 table by zeroing the
	 * user portion and copying the kernel mappings into
	 * the kernel portion.
	 */
	for (i = 0; i < l1pte_index(VM_MIN_KERNEL_ADDRESS); i++)
		l1pt[i] = 0;

	for (i = l1pte_index(VM_MIN_KERNEL_ADDRESS);
	     i <= l1pte_index(VM_MAX_KERNEL_ADDRESS); i++)
		l1pt[i] = kernel_lev1map[i];

	/*
	 * Now, map the new virtual page table.  NOTE: NO ASM!
	 */
	pte = ((ALPHA_K0SEG_TO_PHYS((vaddr_t) l1pt) >> PGSHIFT) << PG_SHIFT) |
	    PG_V | PG_KRE | PG_KWE;
	l1pt[l1pte_index(VPTBASE)] = pte;

	return (0);
}

/*
 * pmap_l1pt_alloc:
 *
 *	Page alloctor for L1 PT pages.
 */
void *
pmap_l1pt_alloc(struct pool *pp, int flags)
{
	paddr_t ptpa;

	/*
	 * Attempt to allocate a free page.
	 */
	if (pmap_physpage_alloc(PGU_L1PT, &ptpa) == FALSE) {
#if 0
		/*
		 * Yow!  No free pages!  Try to steal a PT page from
		 * another pmap!
		 */
		if (pmap_ptpage_steal(pmap, PGU_L1PT, &ptpa) == FALSE)
#endif
			return (NULL);
	}

	return ((void *) ALPHA_PHYS_TO_K0SEG(ptpa));
}

/*
 * pmap_l1pt_free:
 *
 *	Page freer for L1 PT pages.
 */
void
pmap_l1pt_free(struct pool *pp, void *v)
{

	pmap_physpage_free(ALPHA_K0SEG_TO_PHYS((vaddr_t) v));
}

/*
 * pmap_ptpage_alloc:
 *
 *	Allocate a level 2 or level 3 page table page, and
 *	initialize the PTE that references it.
 *
 *	Note: the pmap must already be locked.
 */
int
pmap_ptpage_alloc(pmap_t pmap, pt_entry_t *pte, int usage)
{
	paddr_t ptpa;

	/*
	 * Allocate the page table page.
	 */
	if (pmap_physpage_alloc(usage, &ptpa) == FALSE) {
		/*
		 * Yow!  No free pages!  Try to steal a PT page from
		 * another pmap!
		 */
		if (pmap_ptpage_steal(pmap, usage, &ptpa) == FALSE)
			return (ENOMEM);
	}

	/*
	 * Initialize the referencing PTE.
	 */
	PMAP_SET_PTE(pte, ((ptpa >> PGSHIFT) << PG_SHIFT) |
	    PG_V | PG_KRE | PG_KWE | PG_WIRED |
	    (pmap == pmap_kernel() ? PG_ASM : 0));

	return (0);
}

/*
 * pmap_ptpage_free:
 *
 *	Free the level 2 or level 3 page table page referenced
 *	be the provided PTE.
 *
 *	Note: the pmap must already be locked.
 */
void
pmap_ptpage_free(pmap_t pmap, pt_entry_t *pte, pt_entry_t **ptp)
{
	paddr_t ptpa;

	/*
	 * Extract the physical address of the page from the PTE
	 * and clear the entry.
	 */
	ptpa = pmap_pte_pa(pte);
	PMAP_SET_PTE(pte, PG_NV);

	/*
	 * Check to see if we're stealing the PT page.  If we are,
	 * zero it, and return the KSEG address of the page.
	 */
	if (ptp != NULL) {
		pmap_zero_page(PHYS_TO_VM_PAGE(ptpa));
		*ptp = (pt_entry_t *)ALPHA_PHYS_TO_K0SEG(ptpa);
	} else {
#ifdef DEBUG
		pmap_zero_page(PHYS_TO_VM_PAGE(ptpa));
#endif
		pmap_physpage_free(ptpa);
	}
}

/*
 * pmap_ptpage_steal:
 *
 *	Steal a PT page from a pmap.
 */
boolean_t
pmap_ptpage_steal(pmap_t pmap, int usage, paddr_t *pap)
{
	struct pv_head *pvh;
	pmap_t spmap;
	int l1idx, l2idx, l3idx;
	pt_entry_t *lev2map, *lev3map;
	vaddr_t va;
	paddr_t pa;
	struct prm_thief prmt;
	u_long cpu_id = cpu_number();
	boolean_t needisync = FALSE;

	prmt.prmt_flags = PRMT_PTP;
	prmt.prmt_ptp = NULL;

	/*
	 * We look for pmaps which do not reference kernel_lev1map (which
	 * would indicate that they are either the kernel pmap, or a user
	 * pmap with no valid mappings).  Since the list of all pmaps is
	 * maintained in an LRU fashion, we should get a pmap that is
	 * `more inactive' than our current pmap (although this may not
	 * always be the case).
	 *
	 * We start looking for valid L1 PTEs at the lowest address,
	 * go to that L2, look for the first valid L2 PTE, and steal
	 * that L3 PT page.
	 */
	simple_lock(&pmap_all_pmaps_slock);
	for (spmap = TAILQ_FIRST(&pmap_all_pmaps);
	     spmap != NULL; spmap = TAILQ_NEXT(spmap, pm_list)) {
		/*
		 * Skip the kernel pmap and ourselves.
		 */
		if (spmap == pmap_kernel() || spmap == pmap)
			continue;

		PMAP_LOCK(spmap);
		if (spmap->pm_lev1map == kernel_lev1map) {
			PMAP_UNLOCK(spmap);
			continue;
		}

		/*
		 * Have a candidate pmap.  Loop through the PT pages looking
		 * for one we can steal.
		 */
		for (l1idx = 0;
		     l1idx < l1pte_index(VM_MAXUSER_ADDRESS); l1idx++) {
			if (pmap_pte_v(&spmap->pm_lev1map[l1idx]) == 0)
				continue;

			lev2map = (pt_entry_t *)ALPHA_PHYS_TO_K0SEG(
			    pmap_pte_pa(&spmap->pm_lev1map[l1idx]));
			for (l2idx = 0; l2idx < NPTEPG; l2idx++) {
				if (pmap_pte_v(&lev2map[l2idx]) == 0)
					continue;
				lev3map = (pt_entry_t *)ALPHA_PHYS_TO_K0SEG(
				    pmap_pte_pa(&lev2map[l2idx]));
				for (l3idx = 0; l3idx < NPTEPG; l3idx++) {
					/*
					 * If the entry is valid and wired,
					 * we cannot steal this page.
					 */
					if (pmap_pte_v(&lev3map[l3idx]) &&
					    pmap_pte_w(&lev3map[l3idx]))
						break;
				}
				
				/*
				 * If we scanned all of the current L3 table
				 * without finding a wired entry, we can
				 * steal this page!
				 */
				if (l3idx == NPTEPG)
					goto found_one;
			}
		}

		/*
		 * Didn't find something we could steal in this
		 * pmap, try the next one.
		 */
		PMAP_UNLOCK(spmap);
		continue;

 found_one:
		/* ...don't need this anymore. */
		simple_unlock(&pmap_all_pmaps_slock);

		/*
		 * Okay!  We have a PT page we can steal.  l1idx and
		 * l2idx indicate which L1 PTP and L2 PTP we should
		 * use to compute the virtual addresses the L3 PTP
		 * maps.  Loop through all the L3 PTEs in this range
		 * and nuke the mappings for them.  When we're through,
		 * we'll have a PT page pointed to by prmt.prmt_ptp!
		 */
		for (l3idx = 0,
		     va = (l1idx * ALPHA_L1SEG_SIZE) +
		          (l2idx * ALPHA_L2SEG_SIZE);
		     l3idx < NPTEPG && prmt.prmt_ptp == NULL;
		     l3idx++, va += PAGE_SIZE) {
			if (pmap_pte_v(&lev3map[l3idx])) {
				needisync |= pmap_remove_mapping(spmap, va,
				    &lev3map[l3idx], TRUE, cpu_id, &prmt);
			}
		}

		if (needisync)
			PMAP_SYNC_ISTREAM(pmap);

		PMAP_UNLOCK(spmap);

#ifdef DIAGNOSTIC
		if (prmt.prmt_ptp == NULL)
			panic("pmap_ptptage_steal: failed");
		if (prmt.prmt_ptp != lev3map)
			panic("pmap_ptpage_steal: inconsistent");
#endif
		pa = ALPHA_K0SEG_TO_PHYS((vaddr_t)prmt.prmt_ptp);

		/*
		 * Don't bother locking here; the assignment is atomic.
		 */
		pvh = pa_to_pvh(pa);
		pvh->pvh_usage = usage;

		*pap = pa;
		return (TRUE);
	}
	simple_unlock(&pmap_all_pmaps_slock);
	return (FALSE);
}

/*
 * pmap_l3pt_delref:
 *
 *	Delete a reference on a level 3 PT page.  If the reference drops
 *	to zero, free it.
 *
 *	Note: the pmap must already be locked.
 */
void
pmap_l3pt_delref(pmap_t pmap, vaddr_t va, pt_entry_t *l3pte, long cpu_id,
    pt_entry_t **ptp)
{
	pt_entry_t *l1pte, *l2pte;

	l1pte = pmap_l1pte(pmap, va);
	l2pte = pmap_l2pte(pmap, va, l1pte);

#ifdef DIAGNOSTIC
	if (pmap == pmap_kernel())
		panic("pmap_l3pt_delref: kernel pmap");
#endif

	if (pmap_physpage_delref(l3pte) == 0) {
		/*
		 * No more mappings; we can free the level 3 table.
		 */
#ifdef DEBUG
		if (pmapdebug & PDB_PTPAGE)
			printf("pmap_l3pt_delref: freeing level 3 table at "
			    "0x%lx\n", pmap_pte_pa(l2pte));
#endif
		pmap_ptpage_free(pmap, l2pte, ptp);
		pmap->pm_nlev3--;

		/*
		 * We've freed a level 3 table, so we must
		 * invalidate the TLB entry for that PT page
		 * in the Virtual Page Table VA range, because
		 * otherwise the PALcode will service a TLB
		 * miss using the stale VPT TLB entry it entered
		 * behind our back to shortcut to the VA's PTE.
		 */
		PMAP_INVALIDATE_TLB(pmap,
		    (vaddr_t)(&VPT[VPT_INDEX(va)]), FALSE,
		    PMAP_ISACTIVE(pmap, cpu_id), cpu_id);
		PMAP_TLB_SHOOTDOWN(pmap,
		    (vaddr_t)(&VPT[VPT_INDEX(va)]), 0);

		/*
		 * We've freed a level 3 table, so delete the reference
		 * on the level 2 table.
		 */
		pmap_l2pt_delref(pmap, l1pte, l2pte, cpu_id);
	}
}

/*
 * pmap_l2pt_delref:
 *
 *	Delete a reference on a level 2 PT page.  If the reference drops
 *	to zero, free it.
 *
 *	Note: the pmap must already be locked.
 */
void
pmap_l2pt_delref(pmap_t pmap, pt_entry_t *l1pte, pt_entry_t *l2pte,
    long cpu_id)
{

#ifdef DIAGNOSTIC
	if (pmap == pmap_kernel())
		panic("pmap_l2pt_delref: kernel pmap");
#endif

	if (pmap_physpage_delref(l2pte) == 0) {
		/*
		 * No more mappings in this segment; we can free the
		 * level 2 table.
		 */
#ifdef DEBUG
		if (pmapdebug & PDB_PTPAGE)
			printf("pmap_l2pt_delref: freeing level 2 table at "
			    "0x%lx\n", pmap_pte_pa(l1pte));
#endif
		pmap_ptpage_free(pmap, l1pte, NULL);
		pmap->pm_nlev2--;

		/*
		 * We've freed a level 2 table, so delete the reference
		 * on the level 1 table.
		 */
		pmap_l1pt_delref(pmap, l1pte, cpu_id);
	}
}

/*
 * pmap_l1pt_delref:
 *
 *	Delete a reference on a level 1 PT page.  If the reference drops
 *	to zero, free it.
 *
 *	Note: the pmap must already be locked.
 */
void
pmap_l1pt_delref(pmap_t pmap, pt_entry_t *l1pte, long cpu_id)
{

#ifdef DIAGNOSTIC
	if (pmap == pmap_kernel())
		panic("pmap_l1pt_delref: kernel pmap");
#endif

	if (pmap_physpage_delref(l1pte) == 0) {
		/*
		 * No more level 2 tables left, go back to the global
		 * kernel_lev1map.
		 */
		pmap_lev1map_destroy(pmap, cpu_id);
	}
}

/******************** Address Space Number management ********************/

/*
 * pmap_asn_alloc:
 *
 *	Allocate and assign an ASN to the specified pmap.
 *
 *	Note: the pmap must already be locked.
 */
void
pmap_asn_alloc(pmap_t pmap, long cpu_id)
{

#ifdef DEBUG
	if (pmapdebug & (PDB_FOLLOW|PDB_ASN))
		printf("pmap_asn_alloc(%p)\n", pmap);
#endif

	/*
	 * If the pmap is still using the global kernel_lev1map, there
	 * is no need to assign an ASN at this time, because only
	 * kernel mappings exist in that map, and all kernel mappings
	 * have PG_ASM set.  If the pmap eventually gets its own
	 * lev1map, an ASN will be allocated at that time.
	 */
	if (pmap->pm_lev1map == kernel_lev1map) {
#ifdef DEBUG
		if (pmapdebug & PDB_ASN)
			printf("pmap_asn_alloc: still references "
			    "kernel_lev1map\n");
#endif
#ifdef DIAGNOSTIC
		if (pmap->pm_asn[cpu_id] != PMAP_ASN_RESERVED)
			panic("pmap_asn_alloc: kernel_lev1map without "
			    "PMAP_ASN_RESERVED");
#endif
		return;
	}

	/*
	 * On processors which do not implement ASNs, the swpctx PALcode
	 * operation will automatically invalidate the TLB and I-cache,
	 * so we don't need to do that here.
	 */
	if (pmap_max_asn == 0) {
		/*
		 * Refresh the pmap's generation number, to
		 * simplify logic elsewhere.
		 */
		pmap->pm_asngen[cpu_id] = pmap_asn_generation[cpu_id];
#ifdef DEBUG
		if (pmapdebug & PDB_ASN)
			printf("pmap_asn_alloc: no ASNs, using asngen %lu\n",
			    pmap->pm_asngen[cpu_id]);
#endif
		return;
	}

	/*
	 * Hopefully, we can continue using the one we have...
	 */
	if (pmap->pm_asn[cpu_id] != PMAP_ASN_RESERVED &&
	    pmap->pm_asngen[cpu_id] == pmap_asn_generation[cpu_id]) {
		/*
		 * ASN is still in the current generation; keep on using it.
		 */
#ifdef DEBUG
		if (pmapdebug & PDB_ASN) 
			printf("pmap_asn_alloc: same generation, keeping %u\n",
			    pmap->pm_asn[cpu_id]);
#endif
		return;
	}

	/*
	 * Need to assign a new ASN.  Grab the next one, incrementing
	 * the generation number if we have to.
	 */
	if (pmap_next_asn[cpu_id] > pmap_max_asn) {
		/*
		 * Invalidate all non-PG_ASM TLB entries and the
		 * I-cache, and bump the generation number.
		 */
		ALPHA_TBIAP();
		alpha_pal_imb();

		pmap_next_asn[cpu_id] = 1;

		pmap_asn_generation[cpu_id]++;
#ifdef DIAGNOSTIC
		if (pmap_asn_generation[cpu_id] == 0) {
			/*
			 * The generation number has wrapped.  We could
			 * handle this scenario by traversing all of
			 * the pmaps, and invaldating the generation
			 * number on those which are not currently
			 * in use by this processor.
			 *
			 * However... considering that we're using
			 * an unsigned 64-bit integer for generation
			 * numbers, on non-ASN CPUs, we won't wrap
			 * for approx. 585 million years, or 75 billion
			 * years on a 128-ASN CPU (assuming 1000 switch
			 * operations per second).
			 *
			 * So, we don't bother.
			 */
			panic("pmap_asn_alloc: too much uptime");
		}
#endif
#ifdef DEBUG
		if (pmapdebug & PDB_ASN)
			printf("pmap_asn_alloc: generation bumped to %lu\n",
			    pmap_asn_generation[cpu_id]);
#endif
	}

	/*
	 * Assign the new ASN and validate the generation number.
	 */
	pmap->pm_asn[cpu_id] = pmap_next_asn[cpu_id]++;
	pmap->pm_asngen[cpu_id] = pmap_asn_generation[cpu_id];

#ifdef DEBUG
	if (pmapdebug & PDB_ASN)
		printf("pmap_asn_alloc: assigning %u to pmap %p\n",
		    pmap->pm_asn[cpu_id], pmap);
#endif

	/*
	 * Have a new ASN, so there's no need to sync the I-stream
	 * on the way back out to userspace.
	 */
	atomic_clearbits_ulong(&pmap->pm_needisync, (1UL << cpu_id));
}

#if defined(MULTIPROCESSOR)
/******************** TLB shootdown code ********************/

/*
 * pmap_tlb_shootdown:
 *
 *	Cause the TLB entry for pmap/va to be shot down.
 */
void
pmap_tlb_shootdown(pmap_t pmap, vaddr_t va, pt_entry_t pte)
{
	u_long i, ipinum, cpu_id = cpu_number();
	struct pmap_tlb_shootdown_q *pq;
	struct pmap_tlb_shootdown_job *pj;
	int s;

	for (i = 0; i < hwrpb->rpb_pcs_cnt; i++) {
		if (i == cpu_id || (cpus_running & (1UL << i)) == 0)
			continue;

		pq = &pmap_tlb_shootdown_q[i];

		PSJQ_LOCK(pq, s);

		pj = pmap_tlb_shootdown_job_get(pq);
		pq->pq_pte |= pte;
		if (pj == NULL) {
			/*
			 * Couldn't allocate a job entry.  Just do a
			 * TBIA[P].
			 */
			if (pq->pq_pte & PG_ASM)
				ipinum = ALPHA_IPI_TBIA;
			else
				ipinum = ALPHA_IPI_TBIAP;
			alpha_send_ipi(i, ipinum);
		} else {
			pj->pj_pmap = pmap;
			pj->pj_va = va;
			pj->pj_pte = pte;
			TAILQ_INSERT_TAIL(&pq->pq_head, pj, pj_list);
			ipinum = ALPHA_IPI_SHOOTDOWN;
		}

		alpha_send_ipi(i, ipinum);

		PSJQ_UNLOCK(pq, s);
	}
}

/*
 * pmap_do_tlb_shootdown:
 *
 *	Process pending TLB shootdown operations for this processor.
 */
void
pmap_do_tlb_shootdown(struct cpu_info *ci, struct trapframe *framep)
{
	u_long cpu_id = ci->ci_cpuid;
	u_long cpu_mask = (1UL << cpu_id);
	struct pmap_tlb_shootdown_q *pq = &pmap_tlb_shootdown_q[cpu_id];
	struct pmap_tlb_shootdown_job *pj;
	int s;

	PSJQ_LOCK(pq, s);

	while ((pj = TAILQ_FIRST(&pq->pq_head)) != NULL) {
		TAILQ_REMOVE(&pq->pq_head, pj, pj_list);
		PMAP_INVALIDATE_TLB(pj->pj_pmap, pj->pj_va,
		    pj->pj_pte & PG_ASM, pj->pj_pmap->pm_cpus & cpu_mask,
		    cpu_id);
		pmap_tlb_shootdown_job_put(pq, pj);
	}
	pq->pq_pte = 0;

	PSJQ_UNLOCK(pq, s);
}

/*
 * pmap_tlb_shootdown_q_drain:
 *
 *	Drain a processor's TLB shootdown queue.  We do not perform
 *	the shootdown operations.  This is merely a convenience
 *	function.
 */
void
pmap_tlb_shootdown_q_drain(u_long cpu_id, boolean_t all)
{
	struct pmap_tlb_shootdown_q *pq = &pmap_tlb_shootdown_q[cpu_id];
	struct pmap_tlb_shootdown_job *pj, *npj;
	pt_entry_t npte = 0;
	int s;

	PSJQ_LOCK(pq, s);

	for (pj = TAILQ_FIRST(&pq->pq_head); pj != NULL; pj = npj) {
		npj = TAILQ_NEXT(pj, pj_list);
		if (all || (pj->pj_pte & PG_ASM) == 0) {
			TAILQ_REMOVE(&pq->pq_head, pj, pj_list);
			pmap_tlb_shootdown_job_put(pq, pj);
		} else
			npte |= pj->pj_pte;
	}
	pq->pq_pte = npte;

	PSJQ_UNLOCK(pq, s);
}

/*
 * pmap_tlb_shootdown_job_get:
 *
 *	Get a TLB shootdown job queue entry.  This places a limit on
 *	the number of outstanding jobs a processor may have.
 *
 *	Note: We expect the queue to be locked.
 */
struct pmap_tlb_shootdown_job *
pmap_tlb_shootdown_job_get(struct pmap_tlb_shootdown_q *pq)
{
	struct pmap_tlb_shootdown_job *pj;

	if (pq->pq_count >= PMAP_TLB_SHOOTDOWN_MAXJOBS)
		return (NULL);
	pj = pool_get(&pmap_tlb_shootdown_job_pool, PR_NOWAIT);
	if (pj != NULL)
		pq->pq_count++;
	return (pj);
}

/*
 * pmap_tlb_shootdown_job_put:
 *
 *	Put a TLB shootdown job queue entry onto the free list.
 *
 *	Note: We expect the queue to be locked.
 */
void
pmap_tlb_shootdown_job_put(struct pmap_tlb_shootdown_q *pq,
    struct pmap_tlb_shootdown_job *pj)
{

#ifdef DIAGNOSTIC
	if (pq->pq_count == 0)
		panic("pmap_tlb_shootdown_job_put: queue length inconsistency");
#endif
	pool_put(&pmap_tlb_shootdown_job_pool, pj);
	pq->pq_count--;
}
#endif /* MULTIPROCESSOR */
