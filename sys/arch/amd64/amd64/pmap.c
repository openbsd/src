/*	$OpenBSD: pmap.c,v 1.82 2014/12/15 04:54:44 tedu Exp $	*/
/*	$NetBSD: pmap.c,v 1.3 2003/05/08 18:13:13 thorpej Exp $	*/

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
 * Copyright 2001 (c) Wasabi Systems, Inc.
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
 * This is the i386 pmap modified and generalized to support x86-64
 * as well. The idea is to hide the upper N levels of the page tables
 * inside pmap_get_ptp, pmap_free_ptp and pmap_growkernel. The rest
 * is mostly untouched, except that it uses some more generalized
 * macros and interfaces.
 *
 * This pmap has been tested on the i386 as well, and it can be easily
 * adapted to PAE.
 *
 * fvdl@wasabisystems.com 18-Jun-2001
 */

/*
 * pmap.c: i386 pmap module rewrite
 * Chuck Cranor <chuck@ccrc.wustl.edu>
 * 11-Aug-97
 *
 * history of this pmap module: in addition to my own input, i used
 *    the following references for this rewrite of the i386 pmap:
 *
 * [1] the NetBSD i386 pmap.   this pmap appears to be based on the
 *     BSD hp300 pmap done by Mike Hibler at University of Utah.
 *     it was then ported to the i386 by William Jolitz of UUNET
 *     Technologies, Inc.   Then Charles M. Hannum of the NetBSD
 *     project fixed some bugs and provided some speed ups.
 *
 * [2] the FreeBSD i386 pmap.   this pmap seems to be the
 *     Hibler/Jolitz pmap, as modified for FreeBSD by John S. Dyson
 *     and David Greenman.
 *
 * [3] the Mach pmap.   this pmap, from CMU, seems to have migrated
 *     between several processors.   the VAX version was done by
 *     Avadis Tevanian, Jr., and Michael Wayne Young.    the i386
 *     version was done by Lance Berc, Mike Kupfer, Bob Baron,
 *     David Golub, and Richard Draves.    the alpha version was
 *     done by Alessandro Forin (CMU/Mach) and Chris Demetriou
 *     (NetBSD/alpha).
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/pool.h>
#include <sys/user.h>
#include <sys/kernel.h>
#include <sys/mutex.h>
#include <sys/sched.h>

#include <uvm/uvm.h>

#include <machine/atomic.h>
#include <machine/lock.h>
#include <machine/cpu.h>
#include <machine/specialreg.h>

#include <dev/isa/isareg.h>
#include <machine/isa_machdep.h>

#include "acpi.h"

/*
 * general info:
 *
 *  - for an explanation of how the i386 MMU hardware works see
 *    the comments in <machine/pte.h>.
 *
 *  - for an explanation of the general memory structure used by
 *    this pmap (including the recursive mapping), see the comments
 *    in <machine/pmap.h>.
 *
 * this file contains the code for the "pmap module."   the module's
 * job is to manage the hardware's virtual to physical address mappings.
 * note that there are two levels of mapping in the VM system:
 *
 *  [1] the upper layer of the VM system uses vm_map's and vm_map_entry's
 *      to map ranges of virtual address space to objects/files.  for
 *      example, the vm_map may say: "map VA 0x1000 to 0x22000 read-only
 *      to the file /bin/ls starting at offset zero."   note that
 *      the upper layer mapping is not concerned with how individual
 *      vm_pages are mapped.
 *
 *  [2] the lower layer of the VM system (the pmap) maintains the mappings
 *      from virtual addresses.   it is concerned with which vm_page is
 *      mapped where.   for example, when you run /bin/ls and start
 *      at page 0x1000 the fault routine may lookup the correct page
 *      of the /bin/ls file and then ask the pmap layer to establish
 *      a mapping for it.
 *
 * note that information in the lower layer of the VM system can be
 * thrown away since it can easily be reconstructed from the info
 * in the upper layer.
 *
 * data structures we use include:
 *  - struct pmap: describes the address space of one process
 *  - struct pv_entry: describes one <PMAP,VA> mapping of a PA
 *  - struct pg_to_free: a list of virtual addresses whose mappings
 *	have been changed.   used for TLB flushing.
 */

/*
 * memory allocation
 *
 *  - there are three data structures that we must dynamically allocate:
 *
 * [A] new process' page directory page (PDP)
 *	- plan 1: done at pmap_create() we use
 *	  uvm_km_alloc(kernel_map, PAGE_SIZE)  [fka kmem_alloc] to do this
 *	  allocation.
 *
 * if we are low in free physical memory then we sleep in
 * uvm_km_alloc -- in this case this is ok since we are creating
 * a new pmap and should not be holding any locks.
 *
 * if the kernel is totally out of virtual space
 * (i.e. uvm_km_alloc returns NULL), then we panic.
 *
 * XXX: the fork code currently has no way to return an "out of
 * memory, try again" error code since uvm_fork [fka vm_fork]
 * is a void function.
 *
 * [B] new page tables pages (PTP)
 * 	call uvm_pagealloc()
 * 		=> success: zero page, add to pm_pdir
 * 		=> failure: we are out of free vm_pages, let pmap_enter()
 *		   tell UVM about it.
 *
 * note: for kernel PTPs, we start with NKPTP of them.   as we map
 * kernel memory (at uvm_map time) we check to see if we've grown
 * the kernel pmap.   if so, we call the optional function
 * pmap_growkernel() to grow the kernel PTPs in advance.
 *
 * [C] pv_entry structures
 *	- try to allocate one from the pool.
 *	If we fail, we simply let pmap_enter() tell UVM about it.
 */

vaddr_t ptp_masks[] = PTP_MASK_INITIALIZER;
int ptp_shifts[] = PTP_SHIFT_INITIALIZER;
long nkptp[] = NKPTP_INITIALIZER;
long nkptpmax[] = NKPTPMAX_INITIALIZER;
long nbpd[] = NBPD_INITIALIZER;
pd_entry_t *normal_pdes[] = PDES_INITIALIZER;

/* int nkpde = NKPTP; */

#define PMAP_MAP_TO_HEAD_LOCK()		/* null */
#define PMAP_MAP_TO_HEAD_UNLOCK()	/* null */

#define PMAP_HEAD_TO_MAP_LOCK()		/* null */
#define PMAP_HEAD_TO_MAP_UNLOCK()	/* null */

#define COUNT(x)	/* nothing */

#define pmap_pte_set(p, n)		x86_atomic_testset_u64(p, n)
#define pmap_pte_clearbits(p, b)	x86_atomic_clearbits_u64(p, b)
#define pmap_pte_setbits(p, b)		x86_atomic_setbits_u64(p, b)


/*
 * global data structures
 */

struct pmap kernel_pmap_store;	/* the kernel's pmap (proc0) */

/*
 * pmap_pg_wc: if our processor supports PAT then we set this
 * to be the pte bits for Write Combining. Else we fall back to
 * UC- so mtrrs can override the cacheability;
 */
int pmap_pg_wc = PG_UCMINUS;

/*
 * other data structures
 */

pt_entry_t protection_codes[8];     /* maps MI prot to i386 prot code */
boolean_t pmap_initialized = FALSE; /* pmap_init done yet? */

/*
 * pv management structures.
 */
struct pool pmap_pv_pool;

/*
 * linked list of all non-kernel pmaps
 */

struct pmap_head pmaps;

/*
 * pool that pmap structures are allocated from
 */

struct pool pmap_pmap_pool;

/*
 * When we're freeing a ptp, we need to delay the freeing until all
 * tlb shootdown has been done. This is the list of the to-be-freed pages.
 */
TAILQ_HEAD(pg_to_free, vm_page);

/*
 * pool that PDPs are allocated from
 */

struct pool pmap_pdp_pool;
void pmap_pdp_ctor(pd_entry_t *);

extern vaddr_t msgbuf_vaddr;
extern paddr_t msgbuf_paddr;

extern vaddr_t idt_vaddr;			/* we allocate IDT early */
extern paddr_t idt_paddr;

extern vaddr_t lo32_vaddr;
extern vaddr_t lo32_paddr;

vaddr_t virtual_avail;
extern int end;

/*
 * local prototypes
 */

void  pmap_enter_pv(struct vm_page *, struct pv_entry *, struct pmap *,
    vaddr_t, struct vm_page *);
struct vm_page *pmap_get_ptp(struct pmap *, vaddr_t, pd_entry_t **);
struct vm_page *pmap_find_ptp(struct pmap *, vaddr_t, paddr_t, int);
int pmap_find_pte_direct(struct pmap *pm, vaddr_t va, pt_entry_t **pd, int *offs);
void pmap_free_ptp(struct pmap *, struct vm_page *,
    vaddr_t, pt_entry_t *, pd_entry_t **, struct pg_to_free *);
void pmap_freepage(struct pmap *, struct vm_page *, int, struct pg_to_free *);
static boolean_t pmap_is_active(struct pmap *, int);
void pmap_map_ptes(struct pmap *, pt_entry_t **, pd_entry_t ***, paddr_t *);
struct pv_entry *pmap_remove_pv(struct vm_page *, struct pmap *, vaddr_t);
void pmap_do_remove(struct pmap *, vaddr_t, vaddr_t, int);
boolean_t pmap_remove_pte(struct pmap *, struct vm_page *, pt_entry_t *,
    vaddr_t, int);
void pmap_remove_ptes(struct pmap *, struct vm_page *, vaddr_t,
    vaddr_t, vaddr_t, int);
#define PMAP_REMOVE_ALL		0	/* remove all mappings */
#define PMAP_REMOVE_SKIPWIRED	1	/* skip wired mappings */

void pmap_unmap_ptes(struct pmap *, paddr_t);
boolean_t pmap_get_physpage(vaddr_t, int, paddr_t *);
boolean_t pmap_pdes_valid(vaddr_t, pd_entry_t **, pd_entry_t *);
void pmap_alloc_level(pd_entry_t **, vaddr_t, int, long *);

void pmap_sync_flags_pte(struct vm_page *, u_long);

void	pmap_tlb_shootpage(struct pmap *, vaddr_t, int);
void	pmap_tlb_shootrange(struct pmap *, vaddr_t, vaddr_t, int);
void	pmap_tlb_shoottlb(struct pmap *, int);
#ifdef MULTIPROCESSOR
void	pmap_tlb_shootwait(void);
#else
#define	pmap_tlb_shootwait()
#endif


/*
 * p m a p   i n l i n e   h e l p e r   f u n c t i o n s
 */

/*
 * pmap_is_curpmap: is this pmap the one currently loaded [in %cr3]?
 *		of course the kernel is always loaded
 */

static __inline boolean_t
pmap_is_curpmap(struct pmap *pmap)
{
	return((pmap == pmap_kernel()) ||
	       (pmap->pm_pdirpa == (paddr_t) rcr3()));
}

/*
 * pmap_is_active: is this pmap loaded into the specified processor's %cr3?
 */

static __inline boolean_t
pmap_is_active(struct pmap *pmap, int cpu_id)
{
	return (pmap == pmap_kernel() ||
	    (pmap->pm_cpus & (1ULL << cpu_id)) != 0);
}

static __inline u_int
pmap_pte2flags(u_long pte)
{
	return (((pte & PG_U) ? PG_PMAP_REF : 0) |
	    ((pte & PG_M) ? PG_PMAP_MOD : 0));
}

void
pmap_sync_flags_pte(struct vm_page *pg, u_long pte)
{
	if (pte & (PG_U|PG_M)) {
		atomic_setbits_int(&pg->pg_flags, pmap_pte2flags(pte));
	}
}

/*
 * pmap_map_ptes: map a pmap's PTEs into KVM
 */

void
pmap_map_ptes(struct pmap *pmap, pt_entry_t **ptepp, pd_entry_t ***pdeppp, paddr_t *save_cr3)
{
	paddr_t cr3 = rcr3();

	/* the kernel's pmap is always accessible */
	if (pmap == pmap_kernel() || pmap->pm_pdirpa == cr3) {
		*save_cr3 = 0;
	} else {
		*save_cr3 = cr3;

		/*
		 * Not sure if we need this, but better be safe.
		 * We don't have the current pmap in order to unset its
		 * active bit, but this just means that we may receive
		 * an unneccessary cross-CPU TLB flush now and then.
		 */
		x86_atomic_setbits_u64(&pmap->pm_cpus, (1ULL << cpu_number()));

		lcr3(pmap->pm_pdirpa);
	}

	*ptepp = PTE_BASE;
	*pdeppp = normal_pdes;
	return;
}

void
pmap_unmap_ptes(struct pmap *pmap, paddr_t save_cr3)
{
	if (save_cr3 != 0) {
		x86_atomic_clearbits_u64(&pmap->pm_cpus, (1ULL << cpu_number()));
		lcr3(save_cr3);
	}
}

int
pmap_find_pte_direct(struct pmap *pm, vaddr_t va, pt_entry_t **pd, int *offs)
{
	u_long mask, shift;
	pd_entry_t pde;
	paddr_t pdpa;
	int lev;

	pdpa = pm->pm_pdirpa;
	shift = L4_SHIFT;
	mask = L4_MASK;
	for (lev = PTP_LEVELS; lev > 0; lev--) {
		*pd = (pd_entry_t *)PMAP_DIRECT_MAP(pdpa);
		*offs = (VA_SIGN_POS(va) & mask) >> shift;
		pde = (*pd)[*offs];

		/* Large pages are different, break early if we run into one. */
		if ((pde & (PG_PS|PG_V)) != PG_V)
			return (lev - 1);

		pdpa = ((*pd)[*offs] & PG_FRAME);
		/* 4096/8 == 512 == 2^9 entries per level */
		shift -= 9;
		mask >>= 9;
	}

	return (0);
}


/*
 * p m a p   k e n t e r   f u n c t i o n s
 *
 * functions to quickly enter/remove pages from the kernel address
 * space.   pmap_kremove is exported to MI kernel.  we make use of
 * the recursive PTE mappings.
 */

/*
 * pmap_kenter_pa: enter a kernel mapping without R/M (pv_entry) tracking
 *
 * => no need to lock anything, assume va is already allocated
 * => should be faster than normal pmap enter function
 */

void
pmap_kenter_pa(vaddr_t va, paddr_t pa, vm_prot_t prot)
{
	pt_entry_t *pte, opte, npte;

	pte = kvtopte(va);

	npte = (pa & PMAP_PA_MASK) | ((prot & PROT_WRITE) ? PG_RW : PG_RO) |
	    ((pa & PMAP_NOCACHE) ? PG_N : 0) |
	    ((pa & PMAP_WC) ? pmap_pg_wc : 0) | PG_V;

	/* special 1:1 mappings in the first 2MB must not be global */
	if (va >= (vaddr_t)NBPD_L2)
		npte |= PG_G;

	if (!(prot & PROT_EXEC))
		npte |= pg_nx;
	opte = pmap_pte_set(pte, npte);
#ifdef LARGEPAGES
	/* XXX For now... */
	if (opte & PG_PS)
		panic("%s: PG_PS", __func__);
#endif
	if (pmap_valid_entry(opte)) {
		if (pa & PMAP_NOCACHE && (opte & PG_N) == 0)
			wbinvd();
		/* This shouldn't happen */
		pmap_tlb_shootpage(pmap_kernel(), va, 1);
		pmap_tlb_shootwait();
	}
}

/*
 * pmap_kremove: remove a kernel mapping(s) without R/M (pv_entry) tracking
 *
 * => no need to lock anything
 * => caller must dispose of any vm_page mapped in the va range
 * => note: not an inline function
 * => we assume the va is page aligned and the len is a multiple of PAGE_SIZE
 * => we assume kernel only unmaps valid addresses and thus don't bother
 *    checking the valid bit before doing TLB flushing
 */

void
pmap_kremove(vaddr_t sva, vsize_t len)
{
	pt_entry_t *pte, opte;
	vaddr_t va, eva;

	eva = sva + len;

	for (va = sva; va != eva; va += PAGE_SIZE) {
		pte = kvtopte(va);

		opte = pmap_pte_set(pte, 0);
#ifdef LARGEPAGES
		KASSERT((opte & PG_PS) == 0);
#endif
		KASSERT((opte & PG_PVLIST) == 0);
	}

	pmap_tlb_shootrange(pmap_kernel(), sva, eva, 1);
	pmap_tlb_shootwait();
}

/*
 * p m a p   i n i t   f u n c t i o n s
 *
 * pmap_bootstrap and pmap_init are called during system startup
 * to init the pmap module.   pmap_bootstrap() does a low level
 * init just to get things rolling.   pmap_init() finishes the job.
 */

/*
 * pmap_bootstrap: get the system in a state where it can run with VM
 *	properly enabled (called before main()).   the VM system is
 *      fully init'd later...
 *
 * => on i386, locore.s has already enabled the MMU by allocating
 *	a PDP for the kernel, and nkpde PTP's for the kernel.
 * => kva_start is the first free virtual address in kernel space
 */

paddr_t
pmap_bootstrap(paddr_t first_avail, paddr_t max_pa)
{
	vaddr_t kva, kva_end, kva_start = VM_MIN_KERNEL_ADDRESS;
	struct pmap *kpm;
	int i;
	unsigned long p1i;
	long ndmpdp;
	paddr_t dmpd, dmpdp;

	/*
	 * define the boundaries of the managed kernel virtual address
	 * space.
	 */

	virtual_avail = kva_start;		/* first free KVA */

	/*
	 * set up protection_codes: we need to be able to convert from
	 * a MI protection code (some combo of VM_PROT...) to something
	 * we can jam into a i386 PTE.
	 */

	protection_codes[PROT_NONE] = pg_nx;			/* --- */
	protection_codes[PROT_EXEC] = PG_RO;			/* --x */
	protection_codes[PROT_READ] = PG_RO | pg_nx;		/* -r- */
	protection_codes[PROT_READ | PROT_EXEC] = PG_RO;	/* -rx */
	protection_codes[PROT_WRITE] = PG_RW | pg_nx;		/* w-- */
	protection_codes[PROT_WRITE | PROT_EXEC] = PG_RW;	/* w-x */
	protection_codes[PROT_WRITE | PROT_READ] = PG_RW | pg_nx; /* wr- */
	protection_codes[PROT_READ | PROT_WRITE | PROT_EXEC] = PG_RW;	/* wrx */

	/*
	 * now we init the kernel's pmap
	 *
	 * the kernel pmap's pm_obj is not used for much.   however, in
	 * user pmaps the pm_obj contains the list of active PTPs.
	 * the pm_obj currently does not have a pager.   it might be possible
	 * to add a pager that would allow a process to read-only mmap its
	 * own page tables (fast user level vtophys?).   this may or may not
	 * be useful.
	 */

	kpm = pmap_kernel();
	for (i = 0; i < PTP_LEVELS - 1; i++) {
		uvm_objinit(&kpm->pm_obj[i], NULL, 1);
		kpm->pm_ptphint[i] = NULL;
	}
	memset(&kpm->pm_list, 0, sizeof(kpm->pm_list));  /* pm_list not used */
	kpm->pm_pdir = (pd_entry_t *)(proc0.p_addr->u_pcb.pcb_cr3 + KERNBASE);
	kpm->pm_pdirpa = proc0.p_addr->u_pcb.pcb_cr3;
	kpm->pm_stats.wired_count = kpm->pm_stats.resident_count =
		atop(kva_start - VM_MIN_KERNEL_ADDRESS);

	/*
	 * the above is just a rough estimate and not critical to the proper
	 * operation of the system.
	 */

	curpcb->pcb_pmap = kpm;	/* proc0's pcb */

	/*
	 * enable global TLB entries.
	 */
	/* add PG_G attribute to already mapped kernel pages */
#if KERNBASE == VM_MIN_KERNEL_ADDRESS
	for (kva = VM_MIN_KERNEL_ADDRESS ; kva < virtual_avail ;
#else
	kva_end = roundup((vaddr_t)&end, PAGE_SIZE);
	for (kva = KERNBASE; kva < kva_end ;
#endif
	     kva += PAGE_SIZE) {
		p1i = pl1_i(kva);
		if (pmap_valid_entry(PTE_BASE[p1i]))
			PTE_BASE[p1i] |= PG_G;
	}

	/*
	 * Map the direct map. The first 4GB were mapped in locore, here
	 * we map the rest if it exists. We actually use the direct map
	 * here to set up the page tables, we're assuming that we're still
	 * operating in the lower 4GB of memory.
	 */
	ndmpdp = (max_pa + NBPD_L3 - 1) >> L3_SHIFT;
	if (ndmpdp < NDML2_ENTRIES)
		ndmpdp = NDML2_ENTRIES;		/* At least 4GB */

	dmpdp = kpm->pm_pdir[PDIR_SLOT_DIRECT] & PG_FRAME;

	dmpd = first_avail; first_avail += ndmpdp * PAGE_SIZE;

	for (i = NDML2_ENTRIES; i < NPDPG * ndmpdp; i++) {
		paddr_t pdp;
		vaddr_t va;

		pdp = (paddr_t)&(((pd_entry_t *)dmpd)[i]);
		va = PMAP_DIRECT_MAP(pdp);

		*((pd_entry_t *)va) = ((paddr_t)i << L2_SHIFT);
		*((pd_entry_t *)va) |= PG_RW | PG_V | PG_PS | PG_G | PG_U |
		    PG_M | pg_nx;
	}

	for (i = NDML2_ENTRIES; i < ndmpdp; i++) {
		paddr_t pdp;
		vaddr_t va;

		pdp = (paddr_t)&(((pd_entry_t *)dmpdp)[i]);
		va = PMAP_DIRECT_MAP(pdp);

		*((pd_entry_t *)va) = dmpd + (i << PAGE_SHIFT);
		*((pd_entry_t *)va) |= PG_RW | PG_V | PG_U | PG_M | pg_nx;
	}

	kpm->pm_pdir[PDIR_SLOT_DIRECT] = dmpdp | PG_V | PG_KW | PG_U |
	    PG_M | pg_nx;

	tlbflush();

	msgbuf_vaddr = virtual_avail;
	virtual_avail += round_page(MSGBUFSIZE);

	idt_vaddr = virtual_avail;
	virtual_avail += 2 * PAGE_SIZE;
	idt_paddr = first_avail;			/* steal a page */
	first_avail += 2 * PAGE_SIZE;

#if defined(MULTIPROCESSOR) || \
    (NACPI > 0 && !defined(SMALL_KERNEL))
	/*
	 * Grab a page below 4G for things that need it (i.e.
	 * having an initial %cr3 for the MP trampoline).
	 */
	lo32_vaddr = virtual_avail;
	virtual_avail += PAGE_SIZE;
	lo32_paddr = first_avail;
	first_avail += PAGE_SIZE;
#endif

	/*
	 * init the global lists.
	 */
	LIST_INIT(&pmaps);

	/*
	 * initialize the pmap pool.
	 */

	pool_init(&pmap_pmap_pool, sizeof(struct pmap), 0, 0, 0, "pmappl",
	    &pool_allocator_nointr);
	pool_init(&pmap_pv_pool, sizeof(struct pv_entry), 0, 0, 0, "pvpl",
	    &pool_allocator_nointr);
	pool_sethiwat(&pmap_pv_pool, 32 * 1024);

	/*
	 * initialize the PDE pool.
	 */

	pool_init(&pmap_pdp_pool, PAGE_SIZE, 0, 0, 0, "pdppl",
	    &pool_allocator_nointr);

	/*
	 * ensure the TLB is sync'd with reality by flushing it...
	 */

	tlbflush();

	return first_avail;
}

/*
 * Pre-allocate PTPs for low memory, so that 1:1 mappings for various
 * trampoline code can be entered.
 */
paddr_t
pmap_prealloc_lowmem_ptps(paddr_t first_avail)
{
	pd_entry_t *pdes;
	int level;
	paddr_t newp;

	pdes = pmap_kernel()->pm_pdir;
	level = PTP_LEVELS;
	for (;;) {
		newp = first_avail; first_avail += PAGE_SIZE;
		memset((void *)PMAP_DIRECT_MAP(newp), 0, PAGE_SIZE);
		pdes[pl_i(0, level)] = (newp & PG_FRAME) | PG_V | PG_RW;
		level--;
		if (level <= 1)
			break;
		pdes = normal_pdes[level - 2];
	}

	return first_avail;
}

/*
 * pmap_init: called from uvm_init, our job is to get the pmap
 * system ready to manage mappings... this mainly means initing
 * the pv_entry stuff.
 */

void
pmap_init(void)
{
	/*
	 * done: pmap module is up (and ready for business)
	 */

	pmap_initialized = TRUE;
}

/*
 * p v _ e n t r y   f u n c t i o n s
 */

/*
 * main pv_entry manipulation functions:
 *   pmap_enter_pv: enter a mapping onto a pv list
 *   pmap_remove_pv: remove a mapping from a pv list
 */

/*
 * pmap_enter_pv: enter a mapping onto a pv list
 *
 * => caller should adjust ptp's wire_count before calling
 *
 * pve: preallocated pve for us to use
 * ptp: PTP in pmap that maps this VA
 */

void
pmap_enter_pv(struct vm_page *pg, struct pv_entry *pve, struct pmap *pmap,
    vaddr_t va, struct vm_page *ptp)
{
	pve->pv_pmap = pmap;
	pve->pv_va = va;
	pve->pv_ptp = ptp;			/* NULL for kernel pmap */
	pve->pv_next = pg->mdpage.pv_list;	/* add to ... */
	pg->mdpage.pv_list = pve;		/* ... list */
}

/*
 * pmap_remove_pv: try to remove a mapping from a pv_list
 *
 * => caller should adjust ptp's wire_count and free PTP if needed
 * => we return the removed pve
 */

struct pv_entry *
pmap_remove_pv(struct vm_page *pg, struct pmap *pmap, vaddr_t va)
{
	struct pv_entry *pve, **prevptr;

	prevptr = &pg->mdpage.pv_list;
	while ((pve = *prevptr) != NULL) {
		if (pve->pv_pmap == pmap && pve->pv_va == va) {	/* match? */
			*prevptr = pve->pv_next;		/* remove it! */
			break;
		}
		prevptr = &pve->pv_next;		/* previous pointer */
	}
	return(pve);				/* return removed pve */
}

/*
 * p t p   f u n c t i o n s
 */

struct vm_page *
pmap_find_ptp(struct pmap *pmap, vaddr_t va, paddr_t pa, int level)
{
	int lidx = level - 1;
	struct vm_page *pg;

	if (pa != (paddr_t)-1 && pmap->pm_ptphint[lidx] &&
	    pa == VM_PAGE_TO_PHYS(pmap->pm_ptphint[lidx])) {
		return (pmap->pm_ptphint[lidx]);
	}
	if (lidx == 0)
		pg = uvm_pagelookup(&pmap->pm_obj[lidx], ptp_va2o(va, level));
	else {
		pg = uvm_pagelookup(&pmap->pm_obj[lidx], ptp_va2o(va, level));
	}
	return pg;
}

void
pmap_freepage(struct pmap *pmap, struct vm_page *ptp, int level,
    struct pg_to_free *pagelist)
{
	int lidx;
	struct uvm_object *obj;

	lidx = level - 1;

	obj = &pmap->pm_obj[lidx];
	pmap->pm_stats.resident_count--;
	if (pmap->pm_ptphint[lidx] == ptp)
		pmap->pm_ptphint[lidx] = RB_ROOT(&obj->memt);
	ptp->wire_count = 0;
	uvm_pagerealloc(ptp, NULL, 0);
	TAILQ_INSERT_TAIL(pagelist, ptp, pageq);
}

void
pmap_free_ptp(struct pmap *pmap, struct vm_page *ptp, vaddr_t va,
    pt_entry_t *ptes, pd_entry_t **pdes, struct pg_to_free *pagelist)
{
	unsigned long index;
	int level;
	vaddr_t invaladdr;
	pd_entry_t opde;

	level = 1;
	do {
		pmap_freepage(pmap, ptp, level, pagelist);
		index = pl_i(va, level + 1);
		opde = pmap_pte_set(&pdes[level - 1][index], 0);
		invaladdr = level == 1 ? (vaddr_t)ptes :
		    (vaddr_t)pdes[level - 2];
		pmap_tlb_shootpage(curpcb->pcb_pmap,
		    invaladdr + index * PAGE_SIZE,
		    pmap_is_curpmap(curpcb->pcb_pmap));
#if defined(MULTIPROCESSOR)
		invaladdr = level == 1 ? (vaddr_t)PTE_BASE :
		    (vaddr_t)normal_pdes[level - 2];
		pmap_tlb_shootpage(pmap, invaladdr + index * PAGE_SIZE,
		    pmap_is_curpmap(curpcb->pcb_pmap));
#endif
		if (level < PTP_LEVELS - 1) {
			ptp = pmap_find_ptp(pmap, va, (paddr_t)-1, level + 1);
			ptp->wire_count--;
			if (ptp->wire_count > 1)
				break;
		}
	} while (++level < PTP_LEVELS);
}

/*
 * pmap_get_ptp: get a PTP (if there isn't one, allocate a new one)
 *
 * => pmap should NOT be pmap_kernel()
 */


struct vm_page *
pmap_get_ptp(struct pmap *pmap, vaddr_t va, pd_entry_t **pdes)
{
	struct vm_page *ptp, *pptp;
	int i;
	unsigned long index;
	pd_entry_t *pva;
	paddr_t ppa, pa;
	struct uvm_object *obj;

	ptp = NULL;
	pa = (paddr_t)-1;

	/*
	 * Loop through all page table levels seeing if we need to
	 * add a new page to that level.
	 */
	for (i = PTP_LEVELS; i > 1; i--) {
		/*
		 * Save values from previous round.
		 */
		pptp = ptp;
		ppa = pa;

		index = pl_i(va, i);
		pva = pdes[i - 2];

		if (pmap_valid_entry(pva[index])) {
			ppa = pva[index] & PG_FRAME;
			ptp = NULL;
			continue;
		}

		obj = &pmap->pm_obj[i-2];
		ptp = uvm_pagealloc(obj, ptp_va2o(va, i - 1), NULL,
		    UVM_PGA_USERESERVE|UVM_PGA_ZERO);

		if (ptp == NULL)
			return NULL;

		atomic_clearbits_int(&ptp->pg_flags, PG_BUSY);
		ptp->wire_count = 1;
		pmap->pm_ptphint[i - 2] = ptp;
		pa = VM_PAGE_TO_PHYS(ptp);
		pva[index] = (pd_entry_t) (pa | PG_u | PG_RW | PG_V);
		pmap->pm_stats.resident_count++;
		/*
		 * If we're not in the top level, increase the
		 * wire count of the parent page.
		 */
		if (i < PTP_LEVELS) {
			if (pptp == NULL)
				pptp = pmap_find_ptp(pmap, va, ppa, i);
#ifdef DIAGNOSTIC
			if (pptp == NULL)
				panic("%s: pde page disappeared", __func__);
#endif
			pptp->wire_count++;
		}
	}

	/*
	 * ptp is not NULL if we just allocated a new ptp. If it's
	 * still NULL, we must look up the existing one.
	 */
	if (ptp == NULL) {
		ptp = pmap_find_ptp(pmap, va, ppa, 1);
#ifdef DIAGNOSTIC
		if (ptp == NULL) {
			printf("va %lx ppa %lx\n", (unsigned long)va,
			    (unsigned long)ppa);
			panic("%s: unmanaged user PTP", __func__);
		}
#endif
	}

	pmap->pm_ptphint[0] = ptp;
	return(ptp);
}

/*
 * p m a p  l i f e c y c l e   f u n c t i o n s
 */

/*
 * pmap_pdp_ctor: constructor for the PDP cache.
 */

void
pmap_pdp_ctor(pd_entry_t *pdir)
{
	paddr_t pdirpa;
	int npde;

	/* fetch the physical address of the page directory. */
	(void) pmap_extract(pmap_kernel(), (vaddr_t) pdir, &pdirpa);

	/* zero init area */
	memset(pdir, 0, PDIR_SLOT_PTE * sizeof(pd_entry_t));

	/* put in recursive PDE to map the PTEs */
	pdir[PDIR_SLOT_PTE] = pdirpa | PG_V | PG_KW | pg_nx;

	npde = nkptp[PTP_LEVELS - 1];

	/* put in kernel VM PDEs */
	memcpy(&pdir[PDIR_SLOT_KERN], &PDP_BASE[PDIR_SLOT_KERN],
	    npde * sizeof(pd_entry_t));

	/* zero the rest */
	memset(&pdir[PDIR_SLOT_KERN + npde], 0,
	    (NTOPLEVEL_PDES - (PDIR_SLOT_KERN + npde)) * sizeof(pd_entry_t));

	pdir[PDIR_SLOT_DIRECT] = pmap_kernel()->pm_pdir[PDIR_SLOT_DIRECT];

#if VM_MIN_KERNEL_ADDRESS != KERNBASE
	pdir[pl4_pi(KERNBASE)] = PDP_BASE[pl4_pi(KERNBASE)];
#endif
}

/*
 * pmap_create: create a pmap
 *
 * => note: old pmap interface took a "size" args which allowed for
 *	the creation of "software only" pmaps (not in bsd).
 */

struct pmap *
pmap_create(void)
{
	struct pmap *pmap;
	int i;

	pmap = pool_get(&pmap_pmap_pool, PR_WAITOK);

	/* init uvm_object */
	for (i = 0; i < PTP_LEVELS - 1; i++) {
		uvm_objinit(&pmap->pm_obj[i], NULL, 1);
		pmap->pm_ptphint[i] = NULL;
	}
	pmap->pm_stats.wired_count = 0;
	pmap->pm_stats.resident_count = 1;	/* count the PDP allocd below */
	pmap->pm_cpus = 0;

	/* allocate PDP */

	/*
	 * note that there is no need to splvm to protect us from
	 * malloc since malloc allocates out of a submap and we should
	 * have already allocated kernel PTPs to cover the range...
	 */

	pmap->pm_pdir = pool_get(&pmap_pdp_pool, PR_WAITOK);
	pmap_pdp_ctor(pmap->pm_pdir);

	pmap->pm_pdirpa = pmap->pm_pdir[PDIR_SLOT_PTE] & PG_FRAME;

	LIST_INSERT_HEAD(&pmaps, pmap, pm_list);
	return (pmap);
}

/*
 * pmap_destroy: drop reference count on pmap.   free pmap if
 *	reference count goes to zero.
 */

void
pmap_destroy(struct pmap *pmap)
{
	struct vm_page *pg;
	int refs;
	int i;

	/*
	 * drop reference count
	 */

	refs = --pmap->pm_obj[0].uo_refs;
	if (refs > 0) {
		return;
	}

	/*
	 * reference count is zero, free pmap resources and then free pmap.
	 */

#ifdef DIAGNOSTIC
	if (__predict_false(pmap->pm_cpus != 0))
		printf("%s: pmap %p cpus=0x%llx\n", __func__,
		    (void *)pmap, pmap->pm_cpus);
#endif

	/*
	 * remove it from global list of pmaps
	 */
	LIST_REMOVE(pmap, pm_list);

	/*
	 * free any remaining PTPs
	 */

	for (i = 0; i < PTP_LEVELS - 1; i++) {
		while ((pg = RB_ROOT(&pmap->pm_obj[i].memt)) != NULL) {
			KASSERT((pg->pg_flags & PG_BUSY) == 0);

			pg->wire_count = 0;
			uvm_pagefree(pg);
		}
	}

	/* XXX: need to flush it out of other processor's space? */
	pool_put(&pmap_pdp_pool, pmap->pm_pdir);

	pool_put(&pmap_pmap_pool, pmap);
}

/*
 *	Add a reference to the specified pmap.
 */

void
pmap_reference(struct pmap *pmap)
{
	pmap->pm_obj[0].uo_refs++;
}

/*
 * pmap_activate: activate a process' pmap (fill in %cr3)
 *
 * => called from cpu_fork() and when switching pmaps during exec
 * => if p is the curproc, then load it into the MMU
 */

void
pmap_activate(struct proc *p)
{
	struct pcb *pcb = &p->p_addr->u_pcb;
	struct pmap *pmap = p->p_vmspace->vm_map.pmap;

	pcb->pcb_pmap = pmap;
	pcb->pcb_cr3 = pmap->pm_pdirpa;
	if (p == curproc) {
		lcr3(pcb->pcb_cr3);

		/*
		 * mark the pmap in use by this processor.
		 */
		x86_atomic_setbits_u64(&pmap->pm_cpus, (1ULL << cpu_number()));
	}
}

/*
 * pmap_deactivate: deactivate a process' pmap
 */

void
pmap_deactivate(struct proc *p)
{
	struct pmap *pmap = p->p_vmspace->vm_map.pmap;

	/*
	 * mark the pmap no longer in use by this processor.
	 */
	x86_atomic_clearbits_u64(&pmap->pm_cpus, (1ULL << cpu_number()));
}

/*
 * end of lifecycle functions
 */

/*
 * some misc. functions
 */

boolean_t
pmap_pdes_valid(vaddr_t va, pd_entry_t **pdes, pd_entry_t *lastpde)
{
	int i;
	unsigned long index;
	pd_entry_t pde;

	for (i = PTP_LEVELS; i > 1; i--) {
		index = pl_i(va, i);
		pde = pdes[i - 2][index];
		if (!pmap_valid_entry(pde))
			return FALSE;
	}
	if (lastpde != NULL)
		*lastpde = pde;
	return TRUE;
}

/*
 * pmap_extract: extract a PA for the given VA
 */

boolean_t
pmap_extract(struct pmap *pmap, vaddr_t va, paddr_t *pap)
{
	pt_entry_t *ptes;
	int level, offs;

	if (pmap == pmap_kernel() && va >= PMAP_DIRECT_BASE &&
	    va < PMAP_DIRECT_END) {
		*pap = va - PMAP_DIRECT_BASE;
		return (TRUE);
	}

	level = pmap_find_pte_direct(pmap, va, &ptes, &offs);

	if (__predict_true(level == 0 && pmap_valid_entry(ptes[offs]))) {
		if (pap != NULL)
			*pap = (ptes[offs] & PG_FRAME) | (va & PAGE_MASK);
		return (TRUE);
	}
	if (level == 1 && (ptes[offs] & (PG_PS|PG_V)) == (PG_PS|PG_V)) {
		if (pap != NULL)
			*pap = (ptes[offs] & PG_LGFRAME) | (va & PAGE_MASK_L2);
		return (TRUE);
	}

	return FALSE;
}

/*
 * pmap_zero_page: zero a page
 */

void
pmap_zero_page(struct vm_page *pg)
{
	pagezero(pmap_map_direct(pg));
}

/*
 * pmap_flush_cache: flush the cache for a virtual address.
 */
void
pmap_flush_cache(vaddr_t addr, vsize_t len)
{
	vaddr_t	i;

	if (curcpu()->ci_cflushsz == 0) {
		wbinvd();
		return;
	}

	/* all cpus that have clflush also have mfence. */
	mfence();
	for (i = addr; i < addr + len; i += curcpu()->ci_cflushsz)
		clflush(i);
	mfence();
}

/*
 * pmap_pagezeroidle: the same, for the idle loop page zero'er.
 * Returns TRUE if the page was zero'd, FALSE if we aborted for
 * some reason.
 */

boolean_t
pmap_pageidlezero(struct vm_page *pg)
{
	vaddr_t va = pmap_map_direct(pg);
	boolean_t rv = TRUE;
	long *ptr;
	int i;

	/*
	 * XXX - We'd really like to do this uncached. But at this moment
 	 *       we're never called, so just pretend that this works.
	 *       It shouldn't be too hard to create a second direct map
	 *       with uncached mappings.
	 */
	for (i = 0, ptr = (long *) va; i < PAGE_SIZE / sizeof(long); i++) {
		if (!curcpu_is_idle()) {

			/*
			 * A process has become ready.  Abort now,
			 * so we don't keep it waiting while we
			 * do slow memory access to finish this
			 * page.
			 */

			rv = FALSE;
			break;
		}
		*ptr++ = 0;
	}

	return (rv);
}

/*
 * pmap_copy_page: copy a page
 */

void
pmap_copy_page(struct vm_page *srcpg, struct vm_page *dstpg)
{
	vaddr_t srcva = pmap_map_direct(srcpg);
	vaddr_t dstva = pmap_map_direct(dstpg);

	memcpy((void *)dstva, (void *)srcva, PAGE_SIZE);
}

/*
 * p m a p   r e m o v e   f u n c t i o n s
 *
 * functions that remove mappings
 */

/*
 * pmap_remove_ptes: remove PTEs from a PTP
 *
 * => must have proper locking on pmap_master_lock
 * => PTP must be mapped into KVA
 * => PTP should be null if pmap == pmap_kernel()
 */

void
pmap_remove_ptes(struct pmap *pmap, struct vm_page *ptp, vaddr_t ptpva,
    vaddr_t startva, vaddr_t endva, int flags)
{
	struct pv_entry *pve;
	pt_entry_t *pte = (pt_entry_t *) ptpva;
	struct vm_page *pg;
	pt_entry_t opte;

	/*
	 * note that ptpva points to the PTE that maps startva.   this may
	 * or may not be the first PTE in the PTP.
	 *
	 * we loop through the PTP while there are still PTEs to look at
	 * and the wire_count is greater than 1 (because we use the wire_count
	 * to keep track of the number of real PTEs in the PTP).
	 */

	for (/*null*/; startva < endva && (ptp == NULL || ptp->wire_count > 1)
			     ; pte++, startva += PAGE_SIZE) {
		if (!pmap_valid_entry(*pte))
			continue;			/* VA not mapped */
		if ((flags & PMAP_REMOVE_SKIPWIRED) && (*pte & PG_W)) {
			continue;
		}

		/* atomically save the old PTE and zap! it */
		opte = pmap_pte_set(pte, 0);

		if (opte & PG_W)
			pmap->pm_stats.wired_count--;
		pmap->pm_stats.resident_count--;

		if (ptp)
			ptp->wire_count--;		/* dropping a PTE */

		pg = PHYS_TO_VM_PAGE(opte & PG_FRAME);

		/*
		 * if we are not on a pv list we are done.
		 */

		if ((opte & PG_PVLIST) == 0) {
#ifdef DIAGNOSTIC
			if (pg != NULL)
				panic("%s: managed page without PG_PVLIST "
				      "for 0x%lx", __func__, startva);
#endif
			continue;
		}

#ifdef DIAGNOSTIC
		if (pg == NULL)
			panic("%s: unmanaged page marked PG_PVLIST, "
			      "va = 0x%lx, pa = 0x%lx", __func__,
			      startva, (u_long)(opte & PG_FRAME));
#endif

		/* sync R/M bits */
		pmap_sync_flags_pte(pg, opte);
		pve = pmap_remove_pv(pg, pmap, startva);

		if (pve) {
			pool_put(&pmap_pv_pool, pve);
		}

		/* end of "for" loop: time for next pte */
	}
}


/*
 * pmap_remove_pte: remove a single PTE from a PTP
 *
 * => must have proper locking on pmap_master_lock
 * => PTP must be mapped into KVA
 * => PTP should be null if pmap == pmap_kernel()
 * => returns true if we removed a mapping
 */

boolean_t
pmap_remove_pte(struct pmap *pmap, struct vm_page *ptp, pt_entry_t *pte,
    vaddr_t va, int flags)
{
	struct pv_entry *pve;
	struct vm_page *pg;
	pt_entry_t opte;

	if (!pmap_valid_entry(*pte))
		return(FALSE);		/* VA not mapped */
	if ((flags & PMAP_REMOVE_SKIPWIRED) && (*pte & PG_W)) {
		return(FALSE);
	}

	/* atomically save the old PTE and zap! it */
	opte = pmap_pte_set(pte, 0);

	if (opte & PG_W)
		pmap->pm_stats.wired_count--;
	pmap->pm_stats.resident_count--;

	if (ptp)
		ptp->wire_count--;		/* dropping a PTE */

	pg = PHYS_TO_VM_PAGE(opte & PG_FRAME);

	/*
	 * if we are not on a pv list we are done.
	 */
	if ((opte & PG_PVLIST) == 0) {
#ifdef DIAGNOSTIC
		if (pg != NULL)
			panic("%s: managed page without PG_PVLIST for 0x%lx",
			      __func__, va);
#endif
		return(TRUE);
	}

#ifdef DIAGNOSTIC
	if (pg == NULL)
		panic("%s: unmanaged page marked PG_PVLIST, va = 0x%lx, "
		      "pa = 0x%lx", __func__, va, (u_long)(opte & PG_FRAME));
#endif

	/* sync R/M bits */
	pmap_sync_flags_pte(pg, opte);
	pve = pmap_remove_pv(pg, pmap, va);
	if (pve)
		pool_put(&pmap_pv_pool, pve);
	return(TRUE);
}

/*
 * pmap_remove: top level mapping removal function
 *
 * => caller should not be holding any pmap locks
 */

void
pmap_remove(struct pmap *pmap, vaddr_t sva, vaddr_t eva)
{
	pmap_do_remove(pmap, sva, eva, PMAP_REMOVE_ALL);
}

/*
 * pmap_do_remove: mapping removal guts
 *
 * => caller should not be holding any pmap locks
 */

void
pmap_do_remove(struct pmap *pmap, vaddr_t sva, vaddr_t eva, int flags)
{
	pt_entry_t *ptes;
	pd_entry_t **pdes, pde;
	boolean_t result;
	paddr_t ptppa;
	vaddr_t blkendva;
	struct vm_page *ptp;
	vaddr_t va;
	int shootall = 0, shootself;
	struct pg_to_free empty_ptps;
	paddr_t scr3;

	TAILQ_INIT(&empty_ptps);

	PMAP_MAP_TO_HEAD_LOCK();
	pmap_map_ptes(pmap, &ptes, &pdes, &scr3);
	shootself = (scr3 == 0);


	/*
	 * removing one page?  take shortcut function.
	 */

	if (sva + PAGE_SIZE == eva) {
		if (pmap_pdes_valid(sva, pdes, &pde)) {

			/* PA of the PTP */
			ptppa = pde & PG_FRAME;

			/* get PTP if non-kernel mapping */

			if (pmap == pmap_kernel()) {
				/* we never free kernel PTPs */
				ptp = NULL;
			} else {
				ptp = pmap_find_ptp(pmap, sva, ptppa, 1);
#ifdef DIAGNOSTIC
				if (ptp == NULL)
					panic("%s: unmanaged PTP detected",
					      __func__);
#endif
			}

			/* do it! */
			result = pmap_remove_pte(pmap, ptp,
			    &ptes[pl1_i(sva)], sva, flags);

			/*
			 * if mapping removed and the PTP is no longer
			 * being used, free it!
			 */

			if (result && ptp && ptp->wire_count <= 1)
				pmap_free_ptp(pmap, ptp, sva, ptes, pdes,
				    &empty_ptps);
			pmap_tlb_shootpage(pmap, sva, shootself);
			pmap_unmap_ptes(pmap, scr3);
			pmap_tlb_shootwait();
		} else {
			pmap_unmap_ptes(pmap, scr3);
		}
		PMAP_MAP_TO_HEAD_UNLOCK();

		while ((ptp = TAILQ_FIRST(&empty_ptps)) != NULL) {
			TAILQ_REMOVE(&empty_ptps, ptp, pageq);
			uvm_pagefree(ptp);
                }

		return;
	}

	if ((eva - sva > 32 * PAGE_SIZE) && pmap != pmap_kernel())
		shootall = 1;

	for (va = sva; va < eva; va = blkendva) {
		/* determine range of block */
		blkendva = x86_round_pdr(va + 1);
		if (blkendva > eva)
			blkendva = eva;

		/*
		 * XXXCDC: our PTE mappings should never be removed
		 * with pmap_remove!  if we allow this (and why would
		 * we?) then we end up freeing the pmap's page
		 * directory page (PDP) before we are finished using
		 * it when we hit in in the recursive mapping.  this
		 * is BAD.
		 *
		 * long term solution is to move the PTEs out of user
		 * address space.  and into kernel address space (up
		 * with APTE).  then we can set VM_MAXUSER_ADDRESS to
		 * be VM_MAX_ADDRESS.
		 */

		if (pl_i(va, PTP_LEVELS) == PDIR_SLOT_PTE)
			/* XXXCDC: ugly hack to avoid freeing PDP here */
			continue;

		if (!pmap_pdes_valid(va, pdes, &pde))
			continue;

		/* PA of the PTP */
		ptppa = pde & PG_FRAME;

		/* get PTP if non-kernel mapping */
		if (pmap == pmap_kernel()) {
			/* we never free kernel PTPs */
			ptp = NULL;
		} else {
			ptp = pmap_find_ptp(pmap, va, ptppa, 1);
#ifdef DIAGNOSTIC
			if (ptp == NULL)
				panic("%s: unmanaged PTP detected", __func__);
#endif
		}
		pmap_remove_ptes(pmap, ptp,
		    (vaddr_t)&ptes[pl1_i(va)], va, blkendva, flags);

		/* if PTP is no longer being used, free it! */
		if (ptp && ptp->wire_count <= 1) {
			pmap_free_ptp(pmap, ptp, va, ptes, pdes, &empty_ptps);
		}
	}

	if (shootall)
		pmap_tlb_shoottlb(pmap, shootself);
	else
		pmap_tlb_shootrange(pmap, sva, eva, shootself);

	pmap_unmap_ptes(pmap, scr3);
	pmap_tlb_shootwait();
	PMAP_MAP_TO_HEAD_UNLOCK();

	while ((ptp = TAILQ_FIRST(&empty_ptps)) != NULL) {
		TAILQ_REMOVE(&empty_ptps, ptp, pageq);
		uvm_pagefree(ptp);
	}
}

/*
 * pmap_page_remove: remove a managed vm_page from all pmaps that map it
 *
 * => R/M bits are sync'd back to attrs
 */

void
pmap_page_remove(struct vm_page *pg)
{
	struct pv_entry *pve;
	pt_entry_t *ptes, opte;
	pd_entry_t **pdes;
#ifdef DIAGNOSTIC
	pd_entry_t pde;
#endif
	struct pg_to_free empty_ptps;
	struct vm_page *ptp;
	paddr_t scr3;
	int shootself;

	TAILQ_INIT(&empty_ptps);

	PMAP_HEAD_TO_MAP_LOCK();

	while ((pve = pg->mdpage.pv_list) != NULL) {
		pg->mdpage.pv_list = pve->pv_next;

		/* XXX use direct map? */
		pmap_map_ptes(pve->pv_pmap, &ptes, &pdes, &scr3);
		shootself = (scr3 == 0);

#ifdef DIAGNOSTIC
		if (pve->pv_ptp && pmap_pdes_valid(pve->pv_va, pdes, &pde) &&
		   (pde & PG_FRAME) != VM_PAGE_TO_PHYS(pve->pv_ptp)) {
			printf("%s: pg=%p: va=%lx, pv_ptp=%p\n", __func__,
			       pg, pve->pv_va, pve->pv_ptp);
			printf("%s: PTP's phys addr: "
			       "actual=%lx, recorded=%lx\n", __func__,
			       (unsigned long)(pde & PG_FRAME),
				VM_PAGE_TO_PHYS(pve->pv_ptp));
			panic("%s: mapped managed page has "
			      "invalid pv_ptp field", __func__);
		}
#endif

		/* atomically save the old PTE and zap it */
		opte = pmap_pte_set(&ptes[pl1_i(pve->pv_va)], 0);

		if (opte & PG_W)
			pve->pv_pmap->pm_stats.wired_count--;
		pve->pv_pmap->pm_stats.resident_count--;

		pmap_tlb_shootpage(pve->pv_pmap, pve->pv_va, shootself);

		pmap_sync_flags_pte(pg, opte);

		/* update the PTP reference count.  free if last reference. */
		if (pve->pv_ptp) {
			pve->pv_ptp->wire_count--;
			if (pve->pv_ptp->wire_count <= 1) {
				pmap_free_ptp(pve->pv_pmap, pve->pv_ptp,
				    pve->pv_va, ptes, pdes, &empty_ptps);
			}
		}
		pmap_unmap_ptes(pve->pv_pmap, scr3);
		pool_put(&pmap_pv_pool, pve);
	}

	PMAP_HEAD_TO_MAP_UNLOCK();
	pmap_tlb_shootwait();

	while ((ptp = TAILQ_FIRST(&empty_ptps)) != NULL) {
		TAILQ_REMOVE(&empty_ptps, ptp, pageq);
		uvm_pagefree(ptp);
	}
}

/*
 * p m a p   a t t r i b u t e  f u n c t i o n s
 * functions that test/change managed page's attributes
 * since a page can be mapped multiple times we must check each PTE that
 * maps it by going down the pv lists.
 */

/*
 * pmap_test_attrs: test a page's attributes
 */

boolean_t
pmap_test_attrs(struct vm_page *pg, unsigned int testbits)
{
	struct pv_entry *pve;
	pt_entry_t *ptes;
	int level, offs;
	u_long mybits, testflags;

	testflags = pmap_pte2flags(testbits);

	if (pg->pg_flags & testflags)
		return (TRUE);

	PMAP_HEAD_TO_MAP_LOCK();
	mybits = 0;
	for (pve = pg->mdpage.pv_list; pve != NULL && mybits == 0;
	    pve = pve->pv_next) {
		level = pmap_find_pte_direct(pve->pv_pmap, pve->pv_va, &ptes,
		    &offs);
		mybits |= (ptes[offs] & testbits);
	}
	PMAP_HEAD_TO_MAP_UNLOCK();

	if (mybits == 0)
		return (FALSE);

	atomic_setbits_int(&pg->pg_flags, pmap_pte2flags(mybits));

	return (TRUE);
}

/*
 * pmap_clear_attrs: change a page's attributes
 *
 * => we return TRUE if we cleared one of the bits we were asked to
 */

boolean_t
pmap_clear_attrs(struct vm_page *pg, unsigned long clearbits)
{
	struct pv_entry *pve;
	pt_entry_t *ptes, opte;
	u_long clearflags;
	int result, level, offs;

	clearflags = pmap_pte2flags(clearbits);

	PMAP_HEAD_TO_MAP_LOCK();

	result = pg->pg_flags & clearflags;
	if (result)
		atomic_clearbits_int(&pg->pg_flags, clearflags);

	for (pve = pg->mdpage.pv_list; pve != NULL; pve = pve->pv_next) {
		level = pmap_find_pte_direct(pve->pv_pmap, pve->pv_va, &ptes,
		    &offs);
		opte = ptes[offs];
		if (opte & clearbits) {
			result = 1;
			pmap_pte_clearbits(&ptes[offs], (opte & clearbits));
			pmap_tlb_shootpage(pve->pv_pmap, pve->pv_va,
				pmap_is_curpmap(pve->pv_pmap));
		}
	}

	PMAP_HEAD_TO_MAP_UNLOCK();

	pmap_tlb_shootwait();

	return (result != 0);
}

/*
 * p m a p   p r o t e c t i o n   f u n c t i o n s
 */

/*
 * pmap_page_protect: change the protection of all recorded mappings
 *	of a managed page
 *
 * => NOTE: this is an inline function in pmap.h
 */

/* see pmap.h */

/*
 * pmap_protect: set the protection in of the pages in a pmap
 *
 * => NOTE: this is an inline function in pmap.h
 */

/* see pmap.h */

/*
 * pmap_write_protect: write-protect pages in a pmap
 */

void
pmap_write_protect(struct pmap *pmap, vaddr_t sva, vaddr_t eva, vm_prot_t prot)
{
	pt_entry_t nx, *ptes, *spte, *epte;
	pd_entry_t **pdes;
	vaddr_t blockend;
	int shootall = 0, shootself;
	vaddr_t va;
	paddr_t scr3;

	pmap_map_ptes(pmap, &ptes, &pdes, &scr3);
	shootself = (scr3 == 0);

	/* should be ok, but just in case ... */
	sva &= PG_FRAME;
	eva &= PG_FRAME;

	nx = 0;
	if (!(prot & PROT_EXEC))
		nx = pg_nx;

	if ((eva - sva > 32 * PAGE_SIZE) && pmap != pmap_kernel())
		shootall = 1;

	for (va = sva; va < eva ; va = blockend) {
		blockend = (va & L2_FRAME) + NBPD_L2;
		if (blockend > eva)
			blockend = eva;

		/*
		 * XXXCDC: our PTE mappings should never be write-protected!
		 *
		 * long term solution is to move the PTEs out of user
		 * address space.  and into kernel address space (up
		 * with APTE).  then we can set VM_MAXUSER_ADDRESS to
		 * be VM_MAX_ADDRESS.
		 */

		/* XXXCDC: ugly hack to avoid freeing PDP here */
		if (pl_i(va, PTP_LEVELS) == PDIR_SLOT_PTE)
			continue;

		/* empty block? */
		if (!pmap_pdes_valid(va, pdes, NULL))
			continue;

#ifdef DIAGNOSTIC
		if (va >= VM_MAXUSER_ADDRESS && va < VM_MAX_ADDRESS)
			panic("%s: PTE space", __func__);
#endif

		spte = &ptes[pl1_i(va)];
		epte = &ptes[pl1_i(blockend)];

		for (/*null */; spte < epte ; spte++) {
			if (!pmap_valid_entry(*spte))
				continue;
			pmap_pte_clearbits(spte, PG_RW);
			pmap_pte_setbits(spte, nx);
		}
	}

	if (shootall)
		pmap_tlb_shoottlb(pmap, shootself);
	else
		pmap_tlb_shootrange(pmap, sva, eva, shootself);

	pmap_unmap_ptes(pmap, scr3);

	pmap_tlb_shootwait();
}

/*
 * end of protection functions
 */

/*
 * pmap_unwire: clear the wired bit in the PTE
 *
 * => mapping should already be in map
 */

void
pmap_unwire(struct pmap *pmap, vaddr_t va)
{
	pt_entry_t *ptes;
	int level, offs;

	level = pmap_find_pte_direct(pmap, va, &ptes, &offs);

	if (level == 0) {

#ifdef DIAGNOSTIC
		if (!pmap_valid_entry(ptes[offs]))
			panic("%s: invalid (unmapped) va 0x%lx", __func__, va);
#endif
		if (__predict_true((ptes[offs] & PG_W) != 0)) {
			pmap_pte_clearbits(&ptes[offs], PG_W);
			pmap->pm_stats.wired_count--;
		}
#ifdef DIAGNOSTIC
		else {
			printf("%s: wiring for pmap %p va 0x%lx "
			       "didn't change!\n", __func__, pmap, va);
		}
#endif
	}
#ifdef DIAGNOSTIC
	else {
		panic("%s: invalid PDE", __func__);
	}
#endif
}

/*
 * pmap_collect: free resources held by a pmap
 *
 * => optional function.
 * => called when a process is swapped out to free memory.
 */

void
pmap_collect(struct pmap *pmap)
{
	/*
	 * free all of the pt pages by removing the physical mappings
	 * for its entire address space.
	 */

/*	pmap_do_remove(pmap, VM_MIN_ADDRESS, VM_MAX_ADDRESS,
	    PMAP_REMOVE_SKIPWIRED);
*/
}

/*
 * pmap_copy: copy mappings from one pmap to another
 *
 * => optional function
 * void pmap_copy(dst_pmap, src_pmap, dst_addr, len, src_addr)
 */

/*
 * defined as macro in pmap.h
 */

/*
 * pmap_enter: enter a mapping into a pmap
 *
 * => must be done "now" ... no lazy-evaluation
 */

int
pmap_enter(struct pmap *pmap, vaddr_t va, paddr_t pa, vm_prot_t prot, int flags)
{
	pt_entry_t *ptes, opte, npte;
	pd_entry_t **pdes;
	struct vm_page *ptp, *pg = NULL;
	struct pv_entry *pve = NULL;
	int ptpdelta, wireddelta, resdelta;
	boolean_t wired = (flags & PMAP_WIRED) != 0;
	boolean_t nocache = (pa & PMAP_NOCACHE) != 0;
	boolean_t wc = (pa & PMAP_WC) != 0;
	int error, shootself;
	paddr_t scr3;

	KASSERT(!(wc && nocache));
	pa &= PMAP_PA_MASK;

#ifdef DIAGNOSTIC
	if (va == (vaddr_t) PDP_BASE)
		panic("%s: trying to map over PDP!", __func__);

	/* sanity check: kernel PTPs should already have been pre-allocated */
	if (va >= VM_MIN_KERNEL_ADDRESS &&
	    !pmap_valid_entry(pmap->pm_pdir[pl_i(va, PTP_LEVELS)]))
		panic("%s: missing kernel PTP for va %lx!", __func__, va);

#endif

	/* get lock */
	PMAP_MAP_TO_HEAD_LOCK();

	/*
	 * map in ptes and get a pointer to our PTP (unless we are the kernel)
	 */

	pmap_map_ptes(pmap, &ptes, &pdes, &scr3);
	shootself = (scr3 == 0);
	if (pmap == pmap_kernel()) {
		ptp = NULL;
	} else {
		ptp = pmap_get_ptp(pmap, va, pdes);
		if (ptp == NULL) {
			if (flags & PMAP_CANFAIL) {
				error = ENOMEM;
				goto out;
			}
			panic("%s: get ptp failed", __func__);
		}
	}
	opte = ptes[pl1_i(va)];		/* old PTE */

	/*
	 * is there currently a valid mapping at our VA?
	 */

	if (pmap_valid_entry(opte)) {
		/*
		 * first, calculate pm_stats updates.  resident count will not
		 * change since we are replacing/changing a valid mapping.
		 * wired count might change...
		 */

		resdelta = 0;
		if (wired && (opte & PG_W) == 0)
			wireddelta = 1;
		else if (!wired && (opte & PG_W) != 0)
			wireddelta = -1;
		else
			wireddelta = 0;
		ptpdelta = 0;

		/*
		 * is the currently mapped PA the same as the one we
		 * want to map?
		 */

		if ((opte & PG_FRAME) == pa) {

			/* if this is on the PVLIST, sync R/M bit */
			if (opte & PG_PVLIST) {
				pg = PHYS_TO_VM_PAGE(pa);
#ifdef DIAGNOSTIC
				if (pg == NULL)
					panic("%s: same pa PG_PVLIST "
					      "mapping with unmanaged page "
					      "pa = 0x%lx (0x%lx)", __func__,
					      pa, atop(pa));
#endif
				pmap_sync_flags_pte(pg, opte);
			} else {
#ifdef DIAGNOSTIC
				if (PHYS_TO_VM_PAGE(pa) != NULL)
					panic("%s: same pa, managed "
					    "page, no PG_VLIST pa: 0x%lx\n",
					    __func__, pa);
#endif
			}
			goto enter_now;
		}

		/*
		 * changing PAs: we must remove the old one first
		 */

		/*
		 * if current mapping is on a pvlist,
		 * remove it (sync R/M bits)
		 */

		if (opte & PG_PVLIST) {
			pg = PHYS_TO_VM_PAGE(opte & PG_FRAME);
#ifdef DIAGNOSTIC
			if (pg == NULL)
				panic("%s: PG_PVLIST mapping with unmanaged "
				      "page pa = 0x%lx (0x%lx)",
				      __func__, pa, atop(pa));
#endif
			pmap_sync_flags_pte(pg, opte);
			pve = pmap_remove_pv(pg, pmap, va);
			pg = NULL; /* This is not the page we are looking for */
		}
	} else {	/* opte not valid */
		pve = NULL;
		resdelta = 1;
		if (wired)
			wireddelta = 1;
		else
			wireddelta = 0;
		if (ptp)
			ptpdelta = 1;
		else
			ptpdelta = 0;
	}

	/*
	 * pve is either NULL or points to a now-free pv_entry structure
	 * (the latter case is if we called pmap_remove_pv above).
	 *
	 * if this entry is to be on a pvlist, enter it now.
	 */

	if (pmap_initialized)
		pg = PHYS_TO_VM_PAGE(pa);

	if (pg != NULL) {
		if (pve == NULL) {
			pve = pool_get(&pmap_pv_pool, PR_NOWAIT);
			if (pve == NULL) {
				if (flags & PMAP_CANFAIL) {
					error = ENOMEM;
					goto out;
				}
				panic("%s: no pv entries available", __func__);
			}
		}
		pmap_enter_pv(pg, pve, pmap, va, ptp);
	} else {
		/* new mapping is not PG_PVLIST.   free pve if we've got one */
		if (pve)
			pool_put(&pmap_pv_pool, pve);
	}

enter_now:
	/*
	 * at this point pg is !NULL if we want the PG_PVLIST bit set
	 */

	pmap->pm_stats.resident_count += resdelta;
	pmap->pm_stats.wired_count += wireddelta;
	if (ptp)
		ptp->wire_count += ptpdelta;

	KASSERT(pg == PHYS_TO_VM_PAGE(pa));

	npte = pa | protection_codes[prot] | PG_V;
	if (pg != NULL) {
		npte |= PG_PVLIST;
		/*
		 * make sure that if the page is write combined all
		 * instances of pmap_enter make it so.
		 */
		if (pg->pg_flags & PG_PMAP_WC) {
			KASSERT(nocache == 0);
			wc = TRUE;
		}
	}
	if (wc)
		npte |= pmap_pg_wc;
	if (wired)
		npte |= PG_W;
	if (nocache)
		npte |= PG_N;
	if (va < VM_MAXUSER_ADDRESS)
		npte |= PG_u;
	else if (va < VM_MAX_ADDRESS)
		npte |= (PG_u | PG_RW);	/* XXXCDC: no longer needed? */
	if (pmap == pmap_kernel())
		npte |= PG_G;

	ptes[pl1_i(va)] = npte;		/* zap! */

	/*
	 * If we changed anything other than modified/used bits,
	 * flush the TLB.  (is this overkill?)
	 */
	if (pmap_valid_entry(opte)) {
		if (nocache && (opte & PG_N) == 0)
			wbinvd();
		pmap_tlb_shootpage(pmap, va, shootself);
		pmap_tlb_shootwait();
	}

	error = 0;

out:
	pmap_unmap_ptes(pmap, scr3);
	PMAP_MAP_TO_HEAD_UNLOCK();

	return error;
}

boolean_t
pmap_get_physpage(vaddr_t va, int level, paddr_t *paddrp)
{
	struct vm_page *ptp;
	struct pmap *kpm = pmap_kernel();

	if (uvm.page_init_done == FALSE) {
		vaddr_t va;

		/*
		 * we're growing the kernel pmap early (from
		 * uvm_pageboot_alloc()).  this case must be
		 * handled a little differently.
		 */

		va = pmap_steal_memory(PAGE_SIZE, NULL, NULL);
		*paddrp = PMAP_DIRECT_UNMAP(va);
	} else {
		ptp = uvm_pagealloc(&kpm->pm_obj[level - 1],
				    ptp_va2o(va, level), NULL,
				    UVM_PGA_USERESERVE|UVM_PGA_ZERO);
		if (ptp == NULL)
			panic("%s: out of memory", __func__);
		atomic_clearbits_int(&ptp->pg_flags, PG_BUSY);
		ptp->wire_count = 1;
		*paddrp = VM_PAGE_TO_PHYS(ptp);
	}
	kpm->pm_stats.resident_count++;
	return TRUE;
}

/*
 * Allocate the amount of specified ptps for a ptp level, and populate
 * all levels below accordingly, mapping virtual addresses starting at
 * kva.
 *
 * Used by pmap_growkernel.
 */
void
pmap_alloc_level(pd_entry_t **pdes, vaddr_t kva, int lvl, long *needed_ptps)
{
	unsigned long i;
	vaddr_t va;
	paddr_t pa;
	unsigned long index, endindex;
	int level;
	pd_entry_t *pdep;

	for (level = lvl; level > 1; level--) {
		if (level == PTP_LEVELS)
			pdep = pmap_kernel()->pm_pdir;
		else
			pdep = pdes[level - 2];
		va = kva;
		index = pl_i(kva, level);
		endindex = index + needed_ptps[level - 1];
		/*
		 * XXX special case for first time call.
		 */
		if (nkptp[level - 1] != 0)
			index++;
		else
			endindex--;

		for (i = index; i <= endindex; i++) {
			pmap_get_physpage(va, level - 1, &pa);
			pdep[i] = pa | PG_RW | PG_V | pg_nx;
			nkptp[level - 1]++;
			va += nbpd[level - 1];
		}
	}
}

/*
 * pmap_growkernel: increase usage of KVM space
 *
 * => we allocate new PTPs for the kernel and install them in all
 *	the pmaps on the system.
 */

static vaddr_t pmap_maxkvaddr = VM_MIN_KERNEL_ADDRESS;

vaddr_t
pmap_growkernel(vaddr_t maxkvaddr)
{
	struct pmap *kpm = pmap_kernel(), *pm;
	int s, i;
	unsigned newpdes;
	long needed_kptp[PTP_LEVELS], target_nptp, old;

	if (maxkvaddr <= pmap_maxkvaddr)
		return pmap_maxkvaddr;

	maxkvaddr = x86_round_pdr(maxkvaddr);
	old = nkptp[PTP_LEVELS - 1];
	/*
	 * This loop could be optimized more, but pmap_growkernel()
	 * is called infrequently.
	 */
	for (i = PTP_LEVELS - 1; i >= 1; i--) {
		target_nptp = pl_i(maxkvaddr, i + 1) -
		    pl_i(VM_MIN_KERNEL_ADDRESS, i + 1);
		/*
		 * XXX only need to check toplevel.
		 */
		if (target_nptp > nkptpmax[i])
			panic("%s: out of KVA space", __func__);
		needed_kptp[i] = target_nptp - nkptp[i] + 1;
	}


	s = splhigh();	/* to be safe */
	pmap_alloc_level(normal_pdes, pmap_maxkvaddr, PTP_LEVELS,
	    needed_kptp);

	/*
	 * If the number of top level entries changed, update all
	 * pmaps.
	 */
	if (needed_kptp[PTP_LEVELS - 1] != 0) {
		newpdes = nkptp[PTP_LEVELS - 1] - old;
		LIST_FOREACH(pm, &pmaps, pm_list) {
			memcpy(&pm->pm_pdir[PDIR_SLOT_KERN + old],
			       &kpm->pm_pdir[PDIR_SLOT_KERN + old],
			       newpdes * sizeof (pd_entry_t));
		}

		/* Invalidate the PDP cache. */
#if 0
		pool_cache_invalidate(&pmap_pdp_cache);
#endif
	}
	pmap_maxkvaddr = maxkvaddr;
	splx(s);

	return maxkvaddr;
}

vaddr_t
pmap_steal_memory(vsize_t size, vaddr_t *start, vaddr_t *end)
{
	int segno;
	u_int npg;
	vaddr_t va;
	paddr_t pa;
	struct vm_physseg *seg;

	size = round_page(size);
	npg = atop(size);

	for (segno = 0, seg = vm_physmem; segno < vm_nphysseg; segno++, seg++) {
		if (seg->avail_end - seg->avail_start < npg)
			continue;
		/*
		 * We can only steal at an ``unused'' segment boundary,
		 * i.e. either at the start or at the end.
		 */
		if (seg->avail_start == seg->start ||
		    seg->avail_end == seg->end)
			break;
	}
	if (segno == vm_nphysseg) {
		panic("%s: out of memory", __func__);
	} else {
		if (seg->avail_start == seg->start) {
			pa = ptoa(seg->avail_start);
			seg->avail_start += npg;
			seg->start += npg;
		} else {
			pa = ptoa(seg->avail_end) - size;
			seg->avail_end -= npg;
			seg->end -= npg;
		}
		/*
		 * If all the segment has been consumed now, remove it.
		 * Note that the crash dump code still knows about it
		 * and will dump it correctly.
		 */
		if (seg->start == seg->end) {
			if (vm_nphysseg-- == 1)
				panic("%s: out of memory", __func__);
			while (segno < vm_nphysseg) {
				seg[0] = seg[1]; /* struct copy */
				seg++;
				segno++;
			}
		}

		va = PMAP_DIRECT_MAP(pa);
		memset((void *)va, 0, size);
	}

	if (start != NULL)
		*start = virtual_avail;
	if (end != NULL)
		*end = VM_MAX_KERNEL_ADDRESS;

	return (va);
}

#ifdef DEBUG
void pmap_dump(struct pmap *, vaddr_t, vaddr_t);

/*
 * pmap_dump: dump all the mappings from a pmap
 *
 * => caller should not be holding any pmap locks
 */

void
pmap_dump(struct pmap *pmap, vaddr_t sva, vaddr_t eva)
{
	pt_entry_t *ptes, *pte;
	pd_entry_t **pdes;
	vaddr_t blkendva;
	paddr_t scr3;

	/*
	 * if end is out of range truncate.
	 * if (end == start) update to max.
	 */

	if (eva > VM_MAXUSER_ADDRESS || eva <= sva)
		eva = VM_MAXUSER_ADDRESS;


	PMAP_MAP_TO_HEAD_LOCK();
	pmap_map_ptes(pmap, &ptes, &pdes, &scr3);

	/*
	 * dumping a range of pages: we dump in PTP sized blocks (4MB)
	 */

	for (/* null */ ; sva < eva ; sva = blkendva) {

		/* determine range of block */
		blkendva = x86_round_pdr(sva+1);
		if (blkendva > eva)
			blkendva = eva;

		/* valid block? */
		if (!pmap_pdes_valid(sva, pdes, NULL))
			continue;

		pte = &ptes[pl1_i(sva)];
		for (/* null */; sva < blkendva ; sva += PAGE_SIZE, pte++) {
			if (!pmap_valid_entry(*pte))
				continue;
			printf("va %#lx -> pa %#llx (pte=%#llx)\n",
			       sva, *pte, *pte & PG_FRAME);
		}
	}
	pmap_unmap_ptes(pmap, scr3);
	PMAP_MAP_TO_HEAD_UNLOCK();
}
#endif

void
pmap_virtual_space(vaddr_t *vstartp, vaddr_t *vendp)
{
	*vstartp = virtual_avail;
	*vendp = VM_MAX_KERNEL_ADDRESS;
}

#ifdef MULTIPROCESSOR
/*
 * Locking for tlb shootdown.
 *
 * We lock by setting tlb_shoot_wait to the number of cpus that will
 * receive our tlb shootdown. After sending the IPIs, we don't need to
 * worry about locking order or interrupts spinning for the lock because
 * the call that grabs the "lock" isn't the one that releases it. And
 * there is nothing that can block the IPI that releases the lock.
 *
 * The functions are organized so that we first count the number of
 * cpus we need to send the IPI to, then we grab the counter, then
 * we send the IPIs, then we finally do our own shootdown.
 *
 * Our shootdown is last to make it parallel with the other cpus
 * to shorten the spin time.
 *
 * Notice that we depend on failures to send IPIs only being able to
 * happen during boot. If they happen later, the above assumption
 * doesn't hold since we can end up in situations where noone will
 * release the lock if we get an interrupt in a bad moment.
 */

volatile long tlb_shoot_wait;

volatile vaddr_t tlb_shoot_addr1;
volatile vaddr_t tlb_shoot_addr2;

void
pmap_tlb_shootpage(struct pmap *pm, vaddr_t va, int shootself)
{
	struct cpu_info *ci, *self = curcpu();
	CPU_INFO_ITERATOR cii;
	long wait = 0;
	u_int64_t mask = 0;

	CPU_INFO_FOREACH(cii, ci) {
		if (ci == self || !pmap_is_active(pm, ci->ci_cpuid) ||
		    !(ci->ci_flags & CPUF_RUNNING))
			continue;
		mask |= (1ULL << ci->ci_cpuid);
		wait++;
	}

	if (wait > 0) {
		int s = splvm();

		while (atomic_cas_ulong(&tlb_shoot_wait, 0, wait) != 0) {
			while (tlb_shoot_wait != 0)
				SPINLOCK_SPIN_HOOK;
		}
		tlb_shoot_addr1 = va;
		CPU_INFO_FOREACH(cii, ci) {
			if ((mask & (1ULL << ci->ci_cpuid)) == 0)
				continue;
			if (x86_fast_ipi(ci, LAPIC_IPI_INVLPG) != 0)
				panic("%s: ipi failed", __func__);
		}
		splx(s);
	}

	if (shootself)
		pmap_update_pg(va);
}

void
pmap_tlb_shootrange(struct pmap *pm, vaddr_t sva, vaddr_t eva, int shootself)
{
	struct cpu_info *ci, *self = curcpu();
	CPU_INFO_ITERATOR cii;
	long wait = 0;
	u_int64_t mask = 0;
	vaddr_t va;

	CPU_INFO_FOREACH(cii, ci) {
		if (ci == self || !pmap_is_active(pm, ci->ci_cpuid) ||
		    !(ci->ci_flags & CPUF_RUNNING))
			continue;
		mask |= (1ULL << ci->ci_cpuid);
		wait++;
	}

	if (wait > 0) {
		int s = splvm();

		while (atomic_cas_ulong(&tlb_shoot_wait, 0, wait) != 0) {
			while (tlb_shoot_wait != 0)
				SPINLOCK_SPIN_HOOK;
		}
		tlb_shoot_addr1 = sva;
		tlb_shoot_addr2 = eva;
		CPU_INFO_FOREACH(cii, ci) {
			if ((mask & (1ULL << ci->ci_cpuid)) == 0)
				continue;
			if (x86_fast_ipi(ci, LAPIC_IPI_INVLRANGE) != 0)
				panic("%s: ipi failed", __func__);
		}
		splx(s);
	}

	if (shootself)
		for (va = sva; va < eva; va += PAGE_SIZE)
			pmap_update_pg(va);
}

void
pmap_tlb_shoottlb(struct pmap *pm, int shootself)
{
	struct cpu_info *ci, *self = curcpu();
	CPU_INFO_ITERATOR cii;
	long wait = 0;
	u_int64_t mask = 0;

	CPU_INFO_FOREACH(cii, ci) {
		if (ci == self || !pmap_is_active(pm, ci->ci_cpuid) || !(ci->ci_flags & CPUF_RUNNING))
			continue;
		mask |= (1ULL << ci->ci_cpuid);
		wait++;
	}

	if (wait) {
		int s = splvm();

		while (atomic_cas_ulong(&tlb_shoot_wait, 0, wait) != 0) {
			while (tlb_shoot_wait != 0)
				SPINLOCK_SPIN_HOOK;
		}

		CPU_INFO_FOREACH(cii, ci) {
			if ((mask & (1ULL << ci->ci_cpuid)) == 0)
				continue;
			if (x86_fast_ipi(ci, LAPIC_IPI_INVLTLB) != 0)
				panic("%s: ipi failed", __func__);
		}
		splx(s);
	}

	if (shootself)
		tlbflush();
}

void
pmap_tlb_shootwait(void)
{
	while (tlb_shoot_wait != 0)
		SPINLOCK_SPIN_HOOK;
}

#else

void
pmap_tlb_shootpage(struct pmap *pm, vaddr_t va, int shootself)
{
	if (shootself)
		pmap_update_pg(va);

}

void
pmap_tlb_shootrange(struct pmap *pm, vaddr_t sva, vaddr_t eva, int shootself)
{
	vaddr_t va;

	if (!shootself)
		return;

	for (va = sva; va < eva; va += PAGE_SIZE)
		pmap_update_pg(va);

}

void
pmap_tlb_shoottlb(struct pmap *pm, int shootself)
{
	if (shootself)
		tlbflush();
}
#endif /* MULTIPROCESSOR */
