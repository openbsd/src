/*	$OpenBSD: pmap.c,v 1.63 2002/09/11 22:39:00 art Exp $	*/
/*	$NetBSD: pmap.c,v 1.91 2000/06/02 17:46:37 thorpej Exp $	*/

/*
 *
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Charles D. Cranor and
 *      Washington University.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

#include <uvm/uvm.h>

#include <machine/cpu.h>
#include <machine/specialreg.h>
#include <machine/gdt.h>

#include <dev/isa/isareg.h>
#ifdef __NetBSD__
#include <machine/isa_machdep.h>
#endif
#ifdef __OpenBSD__
#include <sys/msgbuf.h>
#include <stand/boot/bootarg.h>
#endif

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
 *
 *  - struct pmap: describes the address space of one thread
 *  - struct pv_entry: describes one <PMAP,VA> mapping of a PA
 *  - struct pv_head: there is one pv_head per managed page of
 *	physical memory.   the pv_head points to a list of pv_entry
 *	structures which describe all the <PMAP,VA> pairs that this
 *      page is mapped in.    this is critical for page based operations
 *      such as pmap_page_protect() [change protection on _all_ mappings
 *      of a page]
 *  - pv_page/pv_page_info: pv_entry's are allocated out of pv_page's.
 *      if we run out of pv_entry's we allocate a new pv_page and free
 *      its pv_entrys.
 * - pmap_remove_record: a list of virtual addresses whose mappings
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
 *	- plan 1: try to allocate one off the free list
 *		=> success: done!
 *		=> failure: no more free pv_entrys on the list
 *	- plan 2: try to allocate a new pv_page to add a chunk of
 *	pv_entrys to the free list
 *		[a] obtain a free, unmapped, VA in kmem_map.  either
 *		we have one saved from a previous call, or we allocate
 *		one now using a "vm_map_lock_try" in uvm_map
 *		=> success: we have an unmapped VA, continue to [b]
 *		=> failure: unable to lock kmem_map or out of VA in it.
 *			move on to plan 3.
 *		[b] allocate a page in kmem_object for the VA
 *		=> success: map it in, free the pv_entry's, DONE!
 *		=> failure: kmem_object locked, no free vm_pages, etc.
 *			save VA for later call to [a], go to plan 3.
 *	If we fail, we simply let pmap_enter() tell UVM about it.
 */

/*
 * locking
 *
 * we have the following locks that we must contend with:
 *
 * "normal" locks:
 *
 *  - pmap_main_lock
 *    this lock is used to prevent deadlock and/or provide mutex
 *    access to the pmap system.   most operations lock the pmap
 *    structure first, then they lock the pv_lists (if needed).
 *    however, some operations such as pmap_page_protect lock
 *    the pv_lists and then lock pmaps.   in order to prevent a
 *    cycle, we require a mutex lock when locking the pv_lists
 *    first.   thus, the "pmap = >pv_list" lockers must gain a
 *    read-lock on pmap_main_lock before locking the pmap.   and
 *    the "pv_list => pmap" lockers must gain a write-lock on
 *    pmap_main_lock before locking.    since only one thread
 *    can write-lock a lock at a time, this provides mutex.
 *
 * "simple" locks:
 *
 * - pmap lock (per pmap, part of uvm_object)
 *   this lock protects the fields in the pmap structure including
 *   the non-kernel PDEs in the PDP, and the PTEs.  it also locks
 *   in the alternate PTE space (since that is determined by the
 *   entry in the PDP).
 *
 * - pvh_lock (per pv_head)
 *   this lock protects the pv_entry list which is chained off the
 *   pv_head structure for a specific managed PA.   it is locked
 *   when traversing the list (e.g. adding/removing mappings,
 *   syncing R/M bits, etc.)
 *
 * - pvalloc_lock
 *   this lock protects the data structures which are used to manage
 *   the free list of pv_entry structures.
 *
 * - pmaps_lock
 *   this lock protects the list of active pmaps (headed by "pmaps").
 *   we lock it when adding or removing pmaps from this list.
 *
 * - pmap_copy_page_lock
 *   locks the tmp kernel PTE mappings we used to copy data
 *
 * - pmap_zero_page_lock
 *   locks the tmp kernel PTE mapping we use to zero a page
 *
 * - pmap_tmpptp_lock
 *   locks the tmp kernel PTE mapping we use to look at a PTP
 *   in another process
 *
 * XXX: would be nice to have per-CPU VAs for the above 4
 */

/*
 * locking data structures
 */

#ifdef __OpenBSD__
/* XXX */
#define spinlockinit(lock, name, flags) /* nada */
#define spinlockmgr(lock, flags, slock) /* nada */
#endif

struct lock pmap_main_lock;
struct simplelock pvalloc_lock;
struct simplelock pmaps_lock;
struct simplelock pmap_copy_page_lock;
struct simplelock pmap_zero_page_lock;
struct simplelock pmap_tmpptp_lock;

#define PMAP_MAP_TO_HEAD_LOCK() \
     spinlockmgr(&pmap_main_lock, LK_SHARED, (void *) 0)
#define PMAP_MAP_TO_HEAD_UNLOCK() \
     spinlockmgr(&pmap_main_lock, LK_RELEASE, (void *) 0)

#define PMAP_HEAD_TO_MAP_LOCK() \
     spinlockmgr(&pmap_main_lock, LK_EXCLUSIVE, (void *) 0)
#define PMAP_HEAD_TO_MAP_UNLOCK() \
     spinlockmgr(&pmap_main_lock, LK_RELEASE, (void *) 0)

/*
 * global data structures
 */

struct pmap kernel_pmap_store;	/* the kernel's pmap (proc0) */

/*
 * nkpde is the number of kernel PTPs allocated for the kernel at
 * boot time (NKPTP is a compile time override).   this number can
 * grow dynamically as needed (but once allocated, we never free
 * kernel PTPs).
 */

int nkpde = NKPTP;
#ifdef NKPDE
#error "obsolete NKPDE: use NKPTP"
#endif

/*
 * pmap_pg_g: if our processor supports PG_G in the PTE then we
 * set pmap_pg_g to PG_G (otherwise it is zero).
 */

int pmap_pg_g = 0;

/*
 * i386 physical memory comes in a big contig chunk with a small
 * hole toward the front of it...  the following 4 paddr_t's
 * (shared with machdep.c) describe the physical address space
 * of this machine.
 */
paddr_t avail_start;	/* PA of first available physical page */
paddr_t avail_end;	/* PA of last available physical page */
paddr_t hole_start;	/* PA of start of "hole" */
paddr_t hole_end;	/* PA of end of "hole" */

/*
 * other data structures
 */

static pt_entry_t protection_codes[8];     /* maps MI prot to i386 prot code */
static boolean_t pmap_initialized = FALSE; /* pmap_init done yet? */

/*
 * the following two vaddr_t's are used during system startup
 * to keep track of how much of the kernel's VM space we have used.
 * once the system is started, the management of the remaining kernel
 * VM space is turned over to the kernel_map vm_map.
 */

static vaddr_t virtual_avail;	/* VA of first free KVA */
static vaddr_t virtual_end;	/* VA of last free KVA */


/*
 * pv_page management structures: locked by pvalloc_lock
 */

TAILQ_HEAD(pv_pagelist, pv_page);
static struct pv_pagelist pv_freepages;	/* list of pv_pages with free entrys */
static struct pv_pagelist pv_unusedpgs; /* list of unused pv_pages */
static int pv_nfpvents;			/* # of free pv entries */
static struct pv_page *pv_initpage;	/* bootstrap page from kernel_map */
static vaddr_t pv_cachedva;		/* cached VA for later use */

#define PVE_LOWAT (PVE_PER_PVPAGE / 2)	/* free pv_entry low water mark */
#define PVE_HIWAT (PVE_LOWAT + (PVE_PER_PVPAGE * 2))
					/* high water mark */

/*
 * linked list of all non-kernel pmaps
 */

static struct pmap_head pmaps;
static struct pmap *pmaps_hand = NULL;	/* used by pmap_steal_ptp */

/*
 * pool that pmap structures are allocated from
 */

struct pool pmap_pmap_pool;

/*
 * special VAs and the PTEs that map them
 */

static pt_entry_t *csrc_pte, *cdst_pte, *zero_pte, *ptp_pte;
static caddr_t csrcp, cdstp, zerop, ptpp;
caddr_t vmmap; /* XXX: used by mem.c... it should really uvm_map_reserve it */

#ifdef __NetBSD__
extern vaddr_t msgbuf_vaddr;
extern paddr_t msgbuf_paddr;

extern vaddr_t idt_vaddr;			/* we allocate IDT early */
extern paddr_t idt_paddr;
#endif

#if defined(I586_CPU)
/* stuff to fix the pentium f00f bug */
extern vaddr_t pentium_idt_vaddr;
#endif


/*
 * local prototypes
 */

static struct pv_entry	*pmap_add_pvpage(struct pv_page *, boolean_t);
static struct vm_page	*pmap_alloc_ptp(struct pmap *, int, boolean_t);
static struct pv_entry	*pmap_alloc_pv(struct pmap *, int); /* see codes below */
#define ALLOCPV_NEED	0	/* need PV now */
#define ALLOCPV_TRY	1	/* just try to allocate, don't steal */
#define ALLOCPV_NONEED	2	/* don't need PV, just growing cache */
static struct pv_entry	*pmap_alloc_pvpage(struct pmap *, int);
static void		 pmap_enter_pv(struct pv_head *,
					    struct pv_entry *, struct pmap *,
					    vaddr_t, struct vm_page *);
static void		 pmap_free_pv(struct pmap *, struct pv_entry *);
static void		 pmap_free_pvs(struct pmap *, struct pv_entry *);
static void		 pmap_free_pv_doit(struct pv_entry *);
static void		 pmap_free_pvpage(void);
static struct vm_page	*pmap_get_ptp(struct pmap *, int, boolean_t);
static boolean_t	 pmap_is_curpmap(struct pmap *);
static pt_entry_t	*pmap_map_ptes(struct pmap *);
static struct pv_entry	*pmap_remove_pv(struct pv_head *, struct pmap *,
					     vaddr_t);
static boolean_t	 pmap_remove_pte(struct pmap *, struct vm_page *,
					      pt_entry_t *, vaddr_t);
static void		 pmap_remove_ptes(struct pmap *,
					       struct pmap_remove_record *,
					       struct vm_page *, vaddr_t,
					       vaddr_t, vaddr_t);
static struct vm_page	*pmap_steal_ptp(struct uvm_object *,
					     vaddr_t);
static vaddr_t		 pmap_tmpmap_pa(paddr_t);
static pt_entry_t	*pmap_tmpmap_pvepte(struct pv_entry *);
static void		 pmap_tmpunmap_pa(void);
static void		 pmap_tmpunmap_pvepte(struct pv_entry *);
static boolean_t	 pmap_transfer_ptes(struct pmap *,
					 struct pmap_transfer_location *,
					 struct pmap *,
					 struct pmap_transfer_location *,
					 int, boolean_t);
static boolean_t	 pmap_try_steal_pv(struct pv_head *,
						struct pv_entry *,
						struct pv_entry *);
static void		pmap_unmap_ptes(struct pmap *);

void			pmap_pinit(pmap_t);
void			pmap_release(pmap_t);

void			pmap_zero_phys(paddr_t);

/*
 * p m a p   i n l i n e   h e l p e r   f u n c t i o n s
 */

/*
 * pmap_is_curpmap: is this pmap the one currently loaded [in %cr3]?
 *		of course the kernel is always loaded
 */

__inline static boolean_t
pmap_is_curpmap(pmap)
	struct pmap *pmap;
{
	return((pmap == pmap_kernel()) ||
	       (pmap->pm_pdirpa == (paddr_t) rcr3()));
}

/*
 * pmap_tmpmap_pa: map a page in for tmp usage
 *
 * => returns with pmap_tmpptp_lock held
 */

__inline static vaddr_t
pmap_tmpmap_pa(pa)
	paddr_t pa;
{
	simple_lock(&pmap_tmpptp_lock);
#if defined(DIAGNOSTIC)
	if (*ptp_pte)
		panic("pmap_tmpmap_pa: ptp_pte in use?");
#endif
	*ptp_pte = PG_V | PG_RW | pa;		/* always a new mapping */
	return((vaddr_t)ptpp);
}

/*
 * pmap_tmpunmap_pa: unmap a tmp use page (undoes pmap_tmpmap_pa)
 *
 * => we release pmap_tmpptp_lock
 */

__inline static void
pmap_tmpunmap_pa()
{
#if defined(DIAGNOSTIC)
	if (!pmap_valid_entry(*ptp_pte))
		panic("pmap_tmpunmap_pa: our pte invalid?");
#endif
	*ptp_pte = 0;		/* zap! */
	pmap_update_pg((vaddr_t)ptpp);
	simple_unlock(&pmap_tmpptp_lock);
}

/*
 * pmap_tmpmap_pvepte: get a quick mapping of a PTE for a pv_entry
 *
 * => do NOT use this on kernel mappings [why?  because pv_ptp may be NULL]
 * => we may grab pmap_tmpptp_lock and return with it held
 */

__inline static pt_entry_t *
pmap_tmpmap_pvepte(pve)
	struct pv_entry *pve;
{
#ifdef DIAGNOSTIC
	if (pve->pv_pmap == pmap_kernel())
		panic("pmap_tmpmap_pvepte: attempt to map kernel");
#endif

	/* is it current pmap?  use direct mapping... */
	if (pmap_is_curpmap(pve->pv_pmap))
		return(vtopte(pve->pv_va));

	return(((pt_entry_t *)pmap_tmpmap_pa(VM_PAGE_TO_PHYS(pve->pv_ptp)))
	       + ptei((unsigned)pve->pv_va));
}

/*
 * pmap_tmpunmap_pvepte: release a mapping obtained with pmap_tmpmap_pvepte
 *
 * => we will release pmap_tmpptp_lock if we hold it
 */

__inline static void
pmap_tmpunmap_pvepte(pve)
	struct pv_entry *pve;
{
	/* was it current pmap?   if so, return */
	if (pmap_is_curpmap(pve->pv_pmap))
		return;

	pmap_tmpunmap_pa();
}

/*
 * pmap_map_ptes: map a pmap's PTEs into KVM and lock them in
 *
 * => we lock enough pmaps to keep things locked in
 * => must be undone with pmap_unmap_ptes before returning
 */

__inline static pt_entry_t *
pmap_map_ptes(pmap)
	struct pmap *pmap;
{
	pd_entry_t opde;

	/* the kernel's pmap is always accessible */
	if (pmap == pmap_kernel()) {
		return(PTE_BASE);
	}

	/* if curpmap then we are always mapped */
	if (pmap_is_curpmap(pmap)) {
		simple_lock(&pmap->pm_obj.vmobjlock);
		return(PTE_BASE);
	}

	/* need to lock both curpmap and pmap: use ordered locking */
	if ((unsigned) pmap < (unsigned) curpcb->pcb_pmap) {
		simple_lock(&pmap->pm_obj.vmobjlock);
		simple_lock(&curpcb->pcb_pmap->pm_obj.vmobjlock);
	} else {
		simple_lock(&curpcb->pcb_pmap->pm_obj.vmobjlock);
		simple_lock(&pmap->pm_obj.vmobjlock);
	}

	/* need to load a new alternate pt space into curpmap? */
	opde = *APDP_PDE;
	if (!pmap_valid_entry(opde) || (opde & PG_FRAME) != pmap->pm_pdirpa) {
		*APDP_PDE = (pd_entry_t) (pmap->pm_pdirpa | PG_RW | PG_V);
		if (pmap_valid_entry(opde))
			tlbflush();
	}
	return(APTE_BASE);
}

/*
 * pmap_unmap_ptes: unlock the PTE mapping of "pmap"
 */

__inline static void
pmap_unmap_ptes(pmap)
	struct pmap *pmap;
{
	if (pmap == pmap_kernel()) {
		return;
	}
	if (pmap_is_curpmap(pmap)) {
		simple_unlock(&pmap->pm_obj.vmobjlock);
	} else {
		simple_unlock(&pmap->pm_obj.vmobjlock);
		simple_unlock(&curpcb->pcb_pmap->pm_obj.vmobjlock);
	}
}

__inline static void
pmap_nxstack_account(struct pmap *pmap, vaddr_t va,
    pt_entry_t opte, pt_entry_t npte)
{
	if (((opte ^ npte) & PG_X) &&
	    va < VM_MAXUSER_ADDRESS && va >= VM_MAXUSER_ADDRESS - MAXSSIZ) {
		struct trapframe *tf = curproc->p_md.md_regs;
		struct vm_map *map = &curproc->p_vmspace->vm_map;

		if (npte & PG_X && !(opte & PG_X)) {
			if (++pmap->pm_nxpages == 1 &&
			    pmap == vm_map_pmap(map)) {
				tf->tf_cs = GSEL(GUCODE1_SEL, SEL_UPL);
				pmap_update_pg(va);
			}
		} else {
			if (!--pmap->pm_nxpages &&
			    pmap == vm_map_pmap(map)) {
				tf->tf_cs = GSEL(GUCODE_SEL, SEL_UPL);
				pmap_update_pg(va);
			}
		}
	}
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
pmap_kenter_pa(va, pa, prot)
	vaddr_t va;
	paddr_t pa;
	vm_prot_t prot;
{
	pt_entry_t *pte, opte;

	pte = vtopte(va);
	opte = *pte;
	*pte = pa | ((prot & VM_PROT_WRITE)? PG_RW : PG_RO) |
		PG_V | pmap_pg_g;	/* zap! */
	if (pmap_valid_entry(opte))
		pmap_update_pg(va);
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
pmap_kremove(va, len)
	vaddr_t va;
	vsize_t len;
{
	pt_entry_t *pte;

	len >>= PAGE_SHIFT;
	for ( /* null */ ; len ; len--, va += NBPG) {
		pte = vtopte(va);
#ifdef DIAGNOSTIC
		if (*pte & PG_PVLIST)
			panic("pmap_kremove: PG_PVLIST mapping for 0x%lx\n",
			      va);
#endif
		*pte = 0;		/* zap! */
#if defined(I386_CPU)
		if (cpu_class != CPUCLASS_386)
#endif
			pmap_update_pg(va);
	}
#if defined(I386_CPU)
	if (cpu_class == CPUCLASS_386)
		tlbflush();
#endif
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

void
pmap_bootstrap(kva_start)
	vaddr_t kva_start;
{
	struct pmap *kpm;
	vaddr_t kva;
	pt_entry_t *pte;

	/*
	 * set the page size (default value is 4K which is ok)
	 */

	uvm_setpagesize();

	/*
	 * a quick sanity check
	 */

	if (PAGE_SIZE != NBPG)
		panic("pmap_bootstrap: PAGE_SIZE != NBPG");

	/*
	 * use the very last page of physical memory for the message buffer
	 */

	avail_end -= i386_round_page(MSGBUFSIZE);
	/*
	 * The arguments passed in from /boot needs space too.
	 */
	avail_end -= i386_round_page(bootargc);

	/*
	 * set up our local static global vars that keep track of the
	 * usage of KVM before kernel_map is set up
	 */

	virtual_avail = kva_start;		/* first free KVA */
	virtual_end = VM_MAX_KERNEL_ADDRESS;	/* last KVA */

	/*
	 * set up protection_codes: we need to be able to convert from
	 * a MI protection code (some combo of VM_PROT...) to something
	 * we can jam into a i386 PTE.
	 */

	protection_codes[UVM_PROT_NONE] = 0;  			/* --- */
	protection_codes[UVM_PROT_EXEC] = PG_X;			/* --x */
	protection_codes[UVM_PROT_READ] = PG_RO;		/* -r- */
	protection_codes[UVM_PROT_RX] = PG_X;			/* -rx */
	protection_codes[UVM_PROT_WRITE] = PG_RW;		/* w-- */
	protection_codes[UVM_PROT_WX] = PG_RW|PG_X;		/* w-x */
	protection_codes[UVM_PROT_RW] = PG_RW;			/* wr- */
	protection_codes[UVM_PROT_RWX] = PG_RW|PG_X;		/* wrx */

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
	simple_lock_init(&kpm->pm_obj.vmobjlock);
	kpm->pm_obj.pgops = NULL;
	TAILQ_INIT(&kpm->pm_obj.memq);
	kpm->pm_obj.uo_npages = 0;
	kpm->pm_obj.uo_refs = 1;
	bzero(&kpm->pm_list, sizeof(kpm->pm_list));  /* pm_list not used */
	kpm->pm_pdir = (pd_entry_t *)(proc0.p_addr->u_pcb.pcb_cr3 + KERNBASE);
	kpm->pm_pdirpa = (u_int32_t) proc0.p_addr->u_pcb.pcb_cr3;
	kpm->pm_stats.wired_count = kpm->pm_stats.resident_count =
		i386_btop(kva_start - VM_MIN_KERNEL_ADDRESS);

	/*
	 * the above is just a rough estimate and not critical to the proper
	 * operation of the system.
	 */

	curpcb->pcb_pmap = kpm;	/* proc0's pcb */

	/*
	 * enable global TLB entries if they are supported
	 */

	if (cpu_feature & CPUID_PGE) {
		lcr4(rcr4() | CR4_PGE);	/* enable hardware (via %cr4) */
		pmap_pg_g = PG_G;		/* enable software */

		/* add PG_G attribute to already mapped kernel pages */
		for (kva = VM_MIN_KERNEL_ADDRESS ; kva < virtual_avail ;
		     kva += PAGE_SIZE)
			if (pmap_valid_entry(PTE_BASE[i386_btop(kva)]))
				PTE_BASE[i386_btop(kva)] |= PG_G;
	}

	/*
	 * now we allocate the "special" VAs which are used for tmp mappings
	 * by the pmap (and other modules).    we allocate the VAs by advancing
	 * virtual_avail (note that there are no pages mapped at these VAs).
	 * we find the PTE that maps the allocated VA via the linear PTE
	 * mapping.
	 */

	pte = PTE_BASE + i386_btop(virtual_avail);

	csrcp = (caddr_t) virtual_avail;  csrc_pte = pte;	/* allocate */
	virtual_avail += PAGE_SIZE; pte++;			/* advance */

	cdstp = (caddr_t) virtual_avail;  cdst_pte = pte;
	virtual_avail += PAGE_SIZE; pte++;

	zerop = (caddr_t) virtual_avail;  zero_pte = pte;
	virtual_avail += PAGE_SIZE; pte++;

	ptpp = (caddr_t) virtual_avail;  ptp_pte = pte;
	virtual_avail += PAGE_SIZE; pte++;

	/* XXX: vmmap used by mem.c... should be uvm_map_reserve */
	vmmap = (char *)virtual_avail;			/* don't need pte */
	virtual_avail += PAGE_SIZE; pte++;

#ifdef __NetBSD
	msgbuf_vaddr = virtual_avail;			/* don't need pte */
#endif
#ifdef __OpenBSD__
	msgbufp = (struct msgbuf *)virtual_avail;	/* don't need pte */
#endif
	virtual_avail += round_page(MSGBUFSIZE); pte++;

#ifdef __NetBSD__
	idt_vaddr = virtual_avail;			/* don't need pte */
	virtual_avail += PAGE_SIZE; pte++;
	idt_paddr = avail_start;			/* steal a page */
	avail_start += PAGE_SIZE;

#if defined(I586_CPU)
	/* pentium f00f bug stuff */
	pentium_idt_vaddr = virtual_avail;		/* don't need pte */
	virtual_avail += PAGE_SIZE; pte++;
#endif
#endif

#ifdef __OpenBSD__
	bootargp = (bootarg_t *)virtual_avail;
	virtual_avail += round_page(bootargc); pte++;
#endif

	/*
	 * now we reserve some VM for mapping pages when doing a crash dump
	 */

	virtual_avail = reserve_dumppages(virtual_avail);

	/*
	 * init the static-global locks and global lists.
	 */

	spinlockinit(&pmap_main_lock, "pmaplk", 0);
	simple_lock_init(&pvalloc_lock);
	simple_lock_init(&pmaps_lock);
	simple_lock_init(&pmap_copy_page_lock);
	simple_lock_init(&pmap_zero_page_lock);
	simple_lock_init(&pmap_tmpptp_lock);
	LIST_INIT(&pmaps);
	TAILQ_INIT(&pv_freepages);
	TAILQ_INIT(&pv_unusedpgs);

	/*
	 * initialize the pmap pool.
	 */

	pool_init(&pmap_pmap_pool, sizeof(struct pmap), 0, 0, 0, "pmappl",
	    &pool_allocator_nointr);

#ifdef __NetBSD__
	/*
	 * we must call uvm_page_physload() after we are done playing with
	 * virtual_avail but before we call pmap_steal_memory.  [i.e. here]
	 * this call tells the VM system how much physical memory it
	 * controls.  If we have 16M of RAM or less, just put it all on
	 * the default free list.  Otherwise, put the first 16M of RAM
	 * on a lower priority free list (so that all of the ISA DMA'able
	 * memory won't be eaten up first-off).
	 */

	if (avail_end <= (16 * 1024 * 1024))
		first16q = VM_FREELIST_DEFAULT;
	else
		first16q = VM_FREELIST_FIRST16;

	if (avail_start < hole_start)   /* any free memory before the hole? */
		uvm_page_physload(atop(avail_start), atop(hole_start),
				  atop(avail_start), atop(hole_start),
				  first16q);

	if (first16q != VM_FREELIST_DEFAULT &&
	    hole_end < 16 * 1024 * 1024) {
		uvm_page_physload(atop(hole_end), atop(16 * 1024 * 1024),
				  atop(hole_end), atop(16 * 1024 * 1024),
				  first16q);
		uvm_page_physload(atop(16 * 1024 * 1024), atop(avail_end),
				  atop(16 * 1024 * 1024), atop(avail_end),
				  VM_FREELIST_DEFAULT);
	} else {
		uvm_page_physload(atop(hole_end), atop(avail_end),
				  atop(hole_end), atop(avail_end),
				  VM_FREELIST_DEFAULT);
	}
#endif

	/*
	 * ensure the TLB is sync'd with reality by flushing it...
	 */

	tlbflush();
}

/*
 * pmap_init: called from uvm_init, our job is to get the pmap
 * system ready to manage mappings... this mainly means initing
 * the pv_entry stuff.
 */

void
pmap_init()
{
	int npages, lcv, i;
	vaddr_t addr;
	vsize_t s;

	/*
	 * compute the number of pages we have and then allocate RAM
	 * for each pages' pv_head and saved attributes.
	 */

	npages = 0;
	for (lcv = 0 ; lcv < vm_nphysseg ; lcv++)
		npages += (vm_physmem[lcv].end - vm_physmem[lcv].start);
	s = (vsize_t) (sizeof(struct pv_head) * npages +
		       sizeof(char) * npages);
	s = round_page(s); /* round up */
	addr = (vaddr_t) uvm_km_zalloc(kernel_map, s);
	if (addr == 0)
		panic("pmap_init: unable to allocate pv_heads");

	/*
	 * init all pv_head's and attrs in one bzero
	 */

	/* allocate pv_head stuff first */
	for (lcv = 0 ; lcv < vm_nphysseg ; lcv++) {
		vm_physmem[lcv].pmseg.pvhead = (struct pv_head *) addr;
		addr = (vaddr_t)(vm_physmem[lcv].pmseg.pvhead +
				 (vm_physmem[lcv].end - vm_physmem[lcv].start));
		for (i = 0;
		     i < (vm_physmem[lcv].end - vm_physmem[lcv].start); i++) {
			simple_lock_init(
			    &vm_physmem[lcv].pmseg.pvhead[i].pvh_lock);
		}
	}

	/* now allocate attrs */
	for (lcv = 0 ; lcv < vm_nphysseg ; lcv++) {
		vm_physmem[lcv].pmseg.attrs = (char *) addr;
		addr = (vaddr_t)(vm_physmem[lcv].pmseg.attrs +
				 (vm_physmem[lcv].end - vm_physmem[lcv].start));
	}

	/*
	 * now we need to free enough pv_entry structures to allow us to get
	 * the kmem_map/kmem_object allocated and inited (done after this
	 * function is finished).  to do this we allocate one bootstrap page out
	 * of kernel_map and use it to provide an initial pool of pv_entry
	 * structures.   we never free this page.
	 */

	pv_initpage = (struct pv_page *) uvm_km_alloc(kernel_map, PAGE_SIZE);
	if (pv_initpage == NULL)
		panic("pmap_init: pv_initpage");
	pv_cachedva = 0;   /* a VA we have allocated but not used yet */
	pv_nfpvents = 0;
	(void) pmap_add_pvpage(pv_initpage, FALSE);

	/*
	 * done: pmap module is up (and ready for business)
	 */

	pmap_initialized = TRUE;
}

/*
 * p v _ e n t r y   f u n c t i o n s
 */

/*
 * pv_entry allocation functions:
 *   the main pv_entry allocation functions are:
 *     pmap_alloc_pv: allocate a pv_entry structure
 *     pmap_free_pv: free one pv_entry
 *     pmap_free_pvs: free a list of pv_entrys
 *
 * the rest are helper functions
 */

/*
 * pmap_alloc_pv: inline function to allocate a pv_entry structure
 * => we lock pvalloc_lock
 * => if we fail, we call out to pmap_alloc_pvpage
 * => 3 modes:
 *    ALLOCPV_NEED   = we really need a pv_entry, even if we have to steal it
 *    ALLOCPV_TRY    = we want a pv_entry, but not enough to steal
 *    ALLOCPV_NONEED = we are trying to grow our free list, don't really need
 *			one now
 *
 * "try" is for optional functions like pmap_copy().
 */

__inline static struct pv_entry *
pmap_alloc_pv(pmap, mode)
	struct pmap *pmap;
	int mode;
{
	struct pv_page *pvpage;
	struct pv_entry *pv;

	simple_lock(&pvalloc_lock);

	if (pv_freepages.tqh_first != NULL) {
		pvpage = pv_freepages.tqh_first;
		pvpage->pvinfo.pvpi_nfree--;
		if (pvpage->pvinfo.pvpi_nfree == 0) {
			/* nothing left in this one? */
			TAILQ_REMOVE(&pv_freepages, pvpage, pvinfo.pvpi_list);
		}
		pv = pvpage->pvinfo.pvpi_pvfree;
#ifdef DIAGNOSTIC
		if (pv == NULL)
			panic("pmap_alloc_pv: pvpi_nfree off");
#endif
		pvpage->pvinfo.pvpi_pvfree = pv->pv_next;
		pv_nfpvents--;  /* took one from pool */
	} else {
		pv = NULL;		/* need more of them */
	}

	/*
	 * if below low water mark or we didn't get a pv_entry we try and
	 * create more pv_entrys ...
	 */

	if (pv_nfpvents < PVE_LOWAT || pv == NULL) {
		if (pv == NULL)
			pv = pmap_alloc_pvpage(pmap, (mode == ALLOCPV_TRY) ?
					       mode : ALLOCPV_NEED);
		else
			(void) pmap_alloc_pvpage(pmap, ALLOCPV_NONEED);
	}

	simple_unlock(&pvalloc_lock);
	return(pv);
}

/*
 * pmap_alloc_pvpage: maybe allocate a new pvpage
 *
 * if need_entry is false: try and allocate a new pv_page
 * if need_entry is true: try and allocate a new pv_page and return a
 *	new pv_entry from it.   if we are unable to allocate a pv_page
 *	we make a last ditch effort to steal a pv_page from some other
 *	mapping.    if that fails, we panic...
 *
 * => we assume that the caller holds pvalloc_lock
 */

static struct pv_entry *
pmap_alloc_pvpage(pmap, mode)
	struct pmap *pmap;
	int mode;
{
	struct vm_page *pg;
	struct pv_page *pvpage;
	int lcv, idx, npg, s;
	struct pv_entry *pv, *cpv, *prevpv;

	/*
	 * if we need_entry and we've got unused pv_pages, allocate from there
	 */

	if (mode != ALLOCPV_NONEED && pv_unusedpgs.tqh_first != NULL) {

		/* move it to pv_freepages list */
		pvpage = pv_unusedpgs.tqh_first;
		TAILQ_REMOVE(&pv_unusedpgs, pvpage, pvinfo.pvpi_list);
		TAILQ_INSERT_HEAD(&pv_freepages, pvpage, pvinfo.pvpi_list);

		/* allocate a pv_entry */
		pvpage->pvinfo.pvpi_nfree--;	/* can't go to zero */
		pv = pvpage->pvinfo.pvpi_pvfree;
#ifdef DIAGNOSTIC
		if (pv == NULL)
			panic("pmap_alloc_pvpage: pvpi_nfree off");
#endif
		pvpage->pvinfo.pvpi_pvfree = pv->pv_next;

		pv_nfpvents--;  /* took one from pool */
		return(pv);
	}

	/*
	 *  see if we've got a cached unmapped VA that we can map a page in.
	 * if not, try to allocate one.
	 */

	s = splimp();   /* must protect kmem_map/kmem_object with splimp! */
	if (pv_cachedva == 0) {
		pv_cachedva = uvm_km_kmemalloc(kmem_map, uvmexp.kmem_object,
		    NBPG, UVM_KMF_TRYLOCK|UVM_KMF_VALLOC);
		if (pv_cachedva == 0) {
			splx(s);
			goto steal_one;
		}
	}

	/*
	 * we have a VA, now let's try and allocate a page in the object
	 * note: we are still holding splimp to protect kmem_object
	 */

	if (!simple_lock_try(&uvmexp.kmem_object->vmobjlock)) {
		splx(s);
		goto steal_one;
	}

	pg = uvm_pagealloc(uvmexp.kmem_object, pv_cachedva -
			   vm_map_min(kernel_map),
			   NULL, UVM_PGA_USERESERVE);
	if (pg)
		pg->flags &= ~PG_BUSY;	/* never busy */

	simple_unlock(&uvmexp.kmem_object->vmobjlock);
	splx(s);
	/* splimp now dropped */

	if (pg == NULL)
		goto steal_one;

	/*
	 * add a mapping for our new pv_page and free its entrys (save one!)
	 *
	 * NOTE: If we are allocating a PV page for the kernel pmap, the
	 * pmap is already locked!  (...but entering the mapping is safe...)
	 */

	pmap_kenter_pa(pv_cachedva, VM_PAGE_TO_PHYS(pg), VM_PROT_ALL);
	pvpage = (struct pv_page *) pv_cachedva;
	pv_cachedva = 0;
	return(pmap_add_pvpage(pvpage, mode != ALLOCPV_NONEED));

steal_one:
	/*
	 * if we don't really need a pv_entry right now, we can just return.
	 */

	if (mode != ALLOCPV_NEED)
		return(NULL);

	/*
	 * last ditch effort!   we couldn't allocate a free page to make
	 * more pv_entrys so we try and steal one from someone else.
	 */

	pv = NULL;
	for (lcv = 0 ; pv == NULL && lcv < vm_nphysseg ; lcv++) {
		npg = vm_physmem[lcv].end - vm_physmem[lcv].start;
		for (idx = 0 ; idx < npg ; idx++) {
			struct pv_head *pvhead = vm_physmem[lcv].pmseg.pvhead;

			if (pvhead->pvh_list == NULL)
				continue;	/* spot check */
			if (!simple_lock_try(&pvhead->pvh_lock))
				continue;
			cpv = prevpv = pvhead->pvh_list;
			while (cpv) {
				if (pmap_try_steal_pv(pvhead, cpv, prevpv))
					break;
				prevpv = cpv;
				cpv = cpv->pv_next;
			}
			simple_unlock(&pvhead->pvh_lock);
			/* got one?  break out of the loop! */
			if (cpv) {
				pv = cpv;
				break;
			}
		}
	}

	return(pv);
}

/*
 * pmap_try_steal_pv: try and steal a pv_entry from a pmap
 *
 * => return true if we did it!
 */

static boolean_t
pmap_try_steal_pv(pvh, cpv, prevpv)
	struct pv_head *pvh;
	struct pv_entry *cpv, *prevpv;
{
	pt_entry_t *ptep;	/* pointer to a PTE */

	/*
	 * we never steal kernel mappings or mappings from pmaps we can't lock
	 */

	if (cpv->pv_pmap == pmap_kernel() ||
	    !simple_lock_try(&cpv->pv_pmap->pm_obj.vmobjlock))
		return(FALSE);

	/*
	 * yes, we can try and steal it.   first we need to remove the
	 * mapping from the pmap.
	 */

	ptep = pmap_tmpmap_pvepte(cpv);
	if (*ptep & PG_W) {
		ptep = NULL;	/* wired page, avoid stealing this one */
	} else {
		*ptep = 0;		/* zap! */
		if (pmap_is_curpmap(cpv->pv_pmap))
			pmap_update_pg(cpv->pv_va);
		pmap_tmpunmap_pvepte(cpv);
	}
	if (ptep == NULL) {
		simple_unlock(&cpv->pv_pmap->pm_obj.vmobjlock);
		return(FALSE);	/* wired page, abort! */
	}
	cpv->pv_pmap->pm_stats.resident_count--;
	if (cpv->pv_ptp && cpv->pv_ptp->wire_count)
		/* drop PTP's wired count */
		cpv->pv_ptp->wire_count--;

	/*
	 * XXX: if wire_count goes to one the PTP could be freed, however,
	 * we'd have to lock the page queues (etc.) to do that and it could
	 * cause deadlock headaches.   besides, the pmap we just stole from
	 * may want the mapping back anyway, so leave the PTP around.
	 */

	/*
	 * now we need to remove the entry from the pvlist
	 */

	if (cpv == pvh->pvh_list)
		pvh->pvh_list = cpv->pv_next;
	else
		prevpv->pv_next = cpv->pv_next;
	return(TRUE);
}

/*
 * pmap_add_pvpage: add a pv_page's pv_entrys to the free list
 *
 * => caller must hold pvalloc_lock
 * => if need_entry is true, we allocate and return one pv_entry
 */

static struct pv_entry *
pmap_add_pvpage(pvp, need_entry)
	struct pv_page *pvp;
	boolean_t need_entry;
{
	int tofree, lcv;

	/* do we need to return one? */
	tofree = (need_entry) ? PVE_PER_PVPAGE - 1 : PVE_PER_PVPAGE;

	pvp->pvinfo.pvpi_pvfree = NULL;
	pvp->pvinfo.pvpi_nfree = tofree;
	for (lcv = 0 ; lcv < tofree ; lcv++) {
		pvp->pvents[lcv].pv_next = pvp->pvinfo.pvpi_pvfree;
		pvp->pvinfo.pvpi_pvfree = &pvp->pvents[lcv];
	}
	if (need_entry)
		TAILQ_INSERT_TAIL(&pv_freepages, pvp, pvinfo.pvpi_list);
	else
		TAILQ_INSERT_TAIL(&pv_unusedpgs, pvp, pvinfo.pvpi_list);
	pv_nfpvents += tofree;
	return((need_entry) ? &pvp->pvents[lcv] : NULL);
}

/*
 * pmap_free_pv_doit: actually free a pv_entry
 *
 * => do not call this directly!  instead use either
 *    1. pmap_free_pv ==> free a single pv_entry
 *    2. pmap_free_pvs => free a list of pv_entrys
 * => we must be holding pvalloc_lock
 */

__inline static void
pmap_free_pv_doit(pv)
	struct pv_entry *pv;
{
	struct pv_page *pvp;

	pvp = (struct pv_page *) i386_trunc_page(pv);
	pv_nfpvents++;
	pvp->pvinfo.pvpi_nfree++;

	/* nfree == 1 => fully allocated page just became partly allocated */
	if (pvp->pvinfo.pvpi_nfree == 1) {
		TAILQ_INSERT_HEAD(&pv_freepages, pvp, pvinfo.pvpi_list);
	}

	/* free it */
	pv->pv_next = pvp->pvinfo.pvpi_pvfree;
	pvp->pvinfo.pvpi_pvfree = pv;

	/*
	 * are all pv_page's pv_entry's free?  move it to unused queue.
	 */

	if (pvp->pvinfo.pvpi_nfree == PVE_PER_PVPAGE) {
		TAILQ_REMOVE(&pv_freepages, pvp, pvinfo.pvpi_list);
		TAILQ_INSERT_HEAD(&pv_unusedpgs, pvp, pvinfo.pvpi_list);
	}
}

/*
 * pmap_free_pv: free a single pv_entry
 *
 * => we gain the pvalloc_lock
 */

__inline static void
pmap_free_pv(pmap, pv)
	struct pmap *pmap;
	struct pv_entry *pv;
{
	simple_lock(&pvalloc_lock);
	pmap_free_pv_doit(pv);

	/*
	 * Can't free the PV page if the PV entries were associated with
	 * the kernel pmap; the pmap is already locked.
	 */
	if (pv_nfpvents > PVE_HIWAT && TAILQ_FIRST(&pv_unusedpgs) != NULL &&
	    pmap != pmap_kernel())
		pmap_free_pvpage();

	simple_unlock(&pvalloc_lock);
}

/*
 * pmap_free_pvs: free a list of pv_entrys
 *
 * => we gain the pvalloc_lock
 */

__inline static void
pmap_free_pvs(pmap, pvs)
	struct pmap *pmap;
	struct pv_entry *pvs;
{
	struct pv_entry *nextpv;

	simple_lock(&pvalloc_lock);

	for ( /* null */ ; pvs != NULL ; pvs = nextpv) {
		nextpv = pvs->pv_next;
		pmap_free_pv_doit(pvs);
	}

	/*
	 * Can't free the PV page if the PV entries were associated with
	 * the kernel pmap; the pmap is already locked.
	 */
	if (pv_nfpvents > PVE_HIWAT && TAILQ_FIRST(&pv_unusedpgs) != NULL &&
	    pmap != pmap_kernel())
		pmap_free_pvpage();

	simple_unlock(&pvalloc_lock);
}


/*
 * pmap_free_pvpage: try and free an unused pv_page structure
 *
 * => assume caller is holding the pvalloc_lock and that
 *	there is a page on the pv_unusedpgs list
 * => if we can't get a lock on the kmem_map we try again later
 * => note: analysis of MI kmem_map usage [i.e. malloc/free] shows
 *	that if we can lock the kmem_map then we are not already
 *	holding kmem_object's lock.
 */

static void
pmap_free_pvpage()
{
	int s;
	struct vm_map *map;
	struct vm_map_entry *dead_entries;
	struct pv_page *pvp;

	s = splvm(); /* protect kmem_map */
	pvp = TAILQ_FIRST(&pv_unusedpgs);

	/*
	 * note: watch out for pv_initpage which is allocated out of
	 * kernel_map rather than kmem_map.
	 */

	if (pvp == pv_initpage)
		map = kernel_map;
	else
		map = kmem_map;
	if (vm_map_lock_try(map)) {

		/* remove pvp from pv_unusedpgs */
		TAILQ_REMOVE(&pv_unusedpgs, pvp, pvinfo.pvpi_list);

		/* unmap the page */
		dead_entries = NULL;
		uvm_unmap_remove(map, (vaddr_t)pvp, ((vaddr_t)pvp) + PAGE_SIZE,
		    &dead_entries);
		vm_map_unlock(map);

		if (dead_entries != NULL)
			uvm_unmap_detach(dead_entries, 0);

		pv_nfpvents -= PVE_PER_PVPAGE;  /* update free count */
	}

	if (pvp == pv_initpage)
		/* no more initpage, we've freed it */
		pv_initpage = NULL;

	splx(s);
}

/*
 * main pv_entry manipulation functions:
 *   pmap_enter_pv: enter a mapping onto a pv_head list
 *   pmap_remove_pv: remove a mappiing from a pv_head list
 *
 * NOTE: pmap_enter_pv expects to lock the pvh itself
 *       pmap_remove_pv expects te caller to lock the pvh before calling
 */

/*
 * pmap_enter_pv: enter a mapping onto a pv_head lst
 *
 * => caller should hold the proper lock on pmap_main_lock
 * => caller should have pmap locked
 * => we will gain the lock on the pv_head and allocate the new pv_entry
 * => caller should adjust ptp's wire_count before calling
 */

__inline static void
pmap_enter_pv(pvh, pve, pmap, va, ptp)
	struct pv_head *pvh;
	struct pv_entry *pve;	/* preallocated pve for us to use */
	struct pmap *pmap;
	vaddr_t va;
	struct vm_page *ptp;	/* PTP in pmap that maps this VA */
{
	pve->pv_pmap = pmap;
	pve->pv_va = va;
	pve->pv_ptp = ptp;			/* NULL for kernel pmap */
	simple_lock(&pvh->pvh_lock);		/* lock pv_head */
	pve->pv_next = pvh->pvh_list;		/* add to ... */
	pvh->pvh_list = pve;			/* ... locked list */
	simple_unlock(&pvh->pvh_lock);		/* unlock, done! */
}

/*
 * pmap_remove_pv: try to remove a mapping from a pv_list
 *
 * => caller should hold proper lock on pmap_main_lock
 * => pmap should be locked
 * => caller should hold lock on pv_head [so that attrs can be adjusted]
 * => caller should adjust ptp's wire_count and free PTP if needed
 * => we return the removed pve
 */

__inline static struct pv_entry *
pmap_remove_pv(pvh, pmap, va)
	struct pv_head *pvh;
	struct pmap *pmap;
	vaddr_t va;
{
	struct pv_entry *pve, **prevptr;

	prevptr = &pvh->pvh_list;		/* previous pv_entry pointer */
	pve = *prevptr;
	while (pve) {
		if (pve->pv_pmap == pmap && pve->pv_va == va) {	/* match? */
			*prevptr = pve->pv_next;		/* remove it! */
			break;
		}
		prevptr = &pve->pv_next;		/* previous pointer */
		pve = pve->pv_next;			/* advance */
	}
	return(pve);				/* return removed pve */
}

/*
 * p t p   f u n c t i o n s
 */

/*
 * pmap_alloc_ptp: allocate a PTP for a PMAP
 *
 * => pmap should already be locked by caller
 * => we use the ptp's wire_count to count the number of active mappings
 *	in the PTP (we start it at one to prevent any chance this PTP
 *	will ever leak onto the active/inactive queues)
 * => we should not be holding any pv_head locks (in case we are forced
 *	to call pmap_steal_ptp())
 * => we may need to lock pv_head's if we have to steal a PTP
 * => just_try: true if we want a PTP, but not enough to steal one
 * 	from another pmap (e.g. during optional functions like pmap_copy)
 */

__inline static struct vm_page *
pmap_alloc_ptp(pmap, pde_index, just_try)
	struct pmap *pmap;
	int pde_index;
	boolean_t just_try;
{
	struct vm_page *ptp;

	ptp = uvm_pagealloc(&pmap->pm_obj, ptp_i2o(pde_index), NULL,
			    UVM_PGA_USERESERVE|UVM_PGA_ZERO);
	if (ptp == NULL) {
		if (just_try)
			return(NULL);
		ptp = pmap_steal_ptp(&pmap->pm_obj, ptp_i2o(pde_index));
		if (ptp == NULL) {
			return (NULL);
		}
		/* stole one; zero it. */
		pmap_zero_page(ptp);
	}

	/* got one! */
	ptp->flags &= ~PG_BUSY;	/* never busy */
	ptp->wire_count = 1;	/* no mappings yet */
	pmap->pm_pdir[pde_index] =
		(pd_entry_t) (VM_PAGE_TO_PHYS(ptp) | PG_u | PG_RW | PG_V);
	pmap->pm_stats.resident_count++;	/* count PTP as resident */
	pmap->pm_ptphint = ptp;
	return(ptp);
}

/*
 * pmap_steal_ptp: steal a PTP from any pmap that we can access
 *
 * => obj is locked by caller.
 * => we can throw away mappings at this level (except in the kernel's pmap)
 * => stolen PTP is placed in <obj,offset> pmap
 * => we lock pv_head's
 * => hopefully, this function will be seldom used [much better to have
 *	enough free pages around for us to allocate off the free page list]
 */

static struct vm_page *
pmap_steal_ptp(obj, offset)
	struct uvm_object *obj;
	vaddr_t offset;
{
	struct vm_page *ptp = NULL;
	struct pmap *firstpmap;
	struct uvm_object *curobj;
	pt_entry_t *ptes;
	int idx, lcv;
	boolean_t caller_locked, we_locked;

	simple_lock(&pmaps_lock);
	if (pmaps_hand == NULL)
		pmaps_hand = LIST_FIRST(&pmaps);
	firstpmap = pmaps_hand;

	do { /* while we haven't looped back around to firstpmap */

		curobj = &pmaps_hand->pm_obj;
		we_locked = FALSE;
		caller_locked = (curobj == obj);
		if (!caller_locked) {
			we_locked = simple_lock_try(&curobj->vmobjlock);
		}
		if (caller_locked || we_locked) {
			ptp = curobj->memq.tqh_first;
			for (/*null*/; ptp != NULL; ptp = ptp->listq.tqe_next) {

				/*
				 * might have found a PTP we can steal
				 * (unless it has wired pages).
				 */

				idx = ptp_o2i(ptp->offset);
#ifdef DIAGNOSTIC
				if (VM_PAGE_TO_PHYS(ptp) !=
				    (pmaps_hand->pm_pdir[idx] & PG_FRAME))
					panic("pmap_steal_ptp: PTP mismatch!");
#endif

				ptes = (pt_entry_t *)
					pmap_tmpmap_pa(VM_PAGE_TO_PHYS(ptp));
				for (lcv = 0 ; lcv < PTES_PER_PTP ; lcv++)
					if ((ptes[lcv] & (PG_V|PG_W)) ==
					    (PG_V|PG_W))
						break;
				if (lcv == PTES_PER_PTP)
					pmap_remove_ptes(pmaps_hand, NULL, ptp,
							 (vaddr_t)ptes,
							 ptp_i2v(idx),
							 ptp_i2v(idx+1));
				pmap_tmpunmap_pa();

				if (lcv != PTES_PER_PTP)
					/* wired, try next PTP */
					continue;

				/*
				 * got it!!!
				 */

				pmaps_hand->pm_pdir[idx] = 0;	/* zap! */
				pmaps_hand->pm_stats.resident_count--;
				if (pmap_is_curpmap(pmaps_hand))
					tlbflush();
				else if (pmap_valid_entry(*APDP_PDE) &&
					 (*APDP_PDE & PG_FRAME) ==
					 pmaps_hand->pm_pdirpa) {
					pmap_update_pg(((vaddr_t)APTE_BASE) +
						       ptp->offset);
				}

				/* put it in our pmap! */
				uvm_pagerealloc(ptp, obj, offset);
				break;	/* break out of "for" loop */
			}
			if (we_locked) {
				simple_unlock(&curobj->vmobjlock);
			}
		}

		/* advance the pmaps_hand */
		pmaps_hand = LIST_NEXT(pmaps_hand, pm_list);
		if (pmaps_hand == NULL) {
			pmaps_hand = LIST_FIRST(&pmaps);
		}

	} while (ptp == NULL && pmaps_hand != firstpmap);

	simple_unlock(&pmaps_lock);
	return(ptp);
}

/*
 * pmap_get_ptp: get a PTP (if there isn't one, allocate a new one)
 *
 * => pmap should NOT be pmap_kernel()
 * => pmap should be locked
 */

static struct vm_page *
pmap_get_ptp(pmap, pde_index, just_try)
	struct pmap *pmap;
	int pde_index;
	boolean_t just_try;
{
	struct vm_page *ptp;

	if (pmap_valid_entry(pmap->pm_pdir[pde_index])) {

		/* valid... check hint (saves us a PA->PG lookup) */
		if (pmap->pm_ptphint &&
		    (pmap->pm_pdir[pde_index] & PG_FRAME) ==
		    VM_PAGE_TO_PHYS(pmap->pm_ptphint))
			return(pmap->pm_ptphint);

		ptp = uvm_pagelookup(&pmap->pm_obj, ptp_i2o(pde_index));
#ifdef DIAGNOSTIC
		if (ptp == NULL)
			panic("pmap_get_ptp: unmanaged user PTP");
#endif
		pmap->pm_ptphint = ptp;
		return(ptp);
	}

	/* allocate a new PTP (updates ptphint) */
	return(pmap_alloc_ptp(pmap, pde_index, just_try));
}

/*
 * p m a p  l i f e c y c l e   f u n c t i o n s
 */

/*
 * pmap_create: create a pmap
 *
 * => note: old pmap interface took a "size" args which allowed for
 *	the creation of "software only" pmaps (not in bsd).
 */

struct pmap *
pmap_create()
{
	struct pmap *pmap;

	pmap = pool_get(&pmap_pmap_pool, PR_WAITOK);
	pmap_pinit(pmap);
	return(pmap);
}

/*
 * pmap_pinit: given a zero'd pmap structure, init it.
 */

void
pmap_pinit(pmap)
	struct pmap *pmap;
{
	/* init uvm_object */
	simple_lock_init(&pmap->pm_obj.vmobjlock);
	pmap->pm_obj.pgops = NULL;	/* currently not a mappable object */
	TAILQ_INIT(&pmap->pm_obj.memq);
	pmap->pm_obj.uo_npages = 0;
	pmap->pm_obj.uo_refs = 1;
	pmap->pm_stats.wired_count = 0;
	pmap->pm_stats.resident_count = 1;	/* count the PDP allocd below */
	pmap->pm_ptphint = NULL;
	pmap->pm_nxpages = 0;
	pmap->pm_flags = 0;

	/* allocate PDP */
	pmap->pm_pdir = (pd_entry_t *) uvm_km_alloc(kernel_map, NBPG);
	if (pmap->pm_pdir == NULL)
		panic("pmap_pinit: kernel_map out of virtual space!");
	(void) pmap_extract(pmap_kernel(), (vaddr_t)pmap->pm_pdir,
			    (paddr_t *)&pmap->pm_pdirpa);

	/* init PDP */
	/* zero init area */
	bzero(pmap->pm_pdir, PDSLOT_PTE * sizeof(pd_entry_t));
	/* put in recursive PDE to map the PTEs */
	pmap->pm_pdir[PDSLOT_PTE] = pmap->pm_pdirpa | PG_V | PG_KW;

	/* init the LDT */
	pmap->pm_ldt = NULL;
	pmap->pm_ldt_len = 0;
	pmap->pm_ldt_sel = GSEL(GLDT_SEL, SEL_KPL);

	/*
	 * we need to lock pmaps_lock to prevent nkpde from changing on
	 * us.   note that there is no need to splimp to protect us from
	 * malloc since malloc allocates out of a submap and we should have
	 * already allocated kernel PTPs to cover the range...
	 */
	simple_lock(&pmaps_lock);
	/* put in kernel VM PDEs */
	bcopy(&PDP_BASE[PDSLOT_KERN], &pmap->pm_pdir[PDSLOT_KERN],
	       nkpde * sizeof(pd_entry_t));
	/* zero the rest */
	bzero(&pmap->pm_pdir[PDSLOT_KERN + nkpde],
	       NBPG - ((PDSLOT_KERN + nkpde) * sizeof(pd_entry_t)));
	LIST_INSERT_HEAD(&pmaps, pmap, pm_list);
	simple_unlock(&pmaps_lock);
}

/*
 * pmap_destroy: drop reference count on pmap.   free pmap if
 *	reference count goes to zero.
 */

void
pmap_destroy(pmap)
	struct pmap *pmap;
{
	int refs;

	/*
	 * drop reference count
	 */

	simple_lock(&pmap->pm_obj.vmobjlock);
	refs = --pmap->pm_obj.uo_refs;
	simple_unlock(&pmap->pm_obj.vmobjlock);
	if (refs > 0) {
		return;
	}

	/*
	 * reference count is zero, free pmap resources and then free pmap.
	 */

	pmap_release(pmap);
	pool_put(&pmap_pmap_pool, pmap);
}

/*
 * pmap_release: release all resources held by a pmap
 *
 * => if pmap is still referenced it should be locked
 * => XXX: we currently don't expect any busy PTPs because we don't
 *    allow anything to map them (except for the kernel's private
 *    recursive mapping) or make them busy.
 */

void
pmap_release(pmap)
	struct pmap *pmap;
{
	struct vm_page *pg;

	/*
	 * remove it from global list of pmaps
	 */

	simple_lock(&pmaps_lock);
	if (pmap == pmaps_hand)
		pmaps_hand = LIST_NEXT(pmaps_hand, pm_list);
	LIST_REMOVE(pmap, pm_list);
	simple_unlock(&pmaps_lock);

	/*
	 * free any remaining PTPs
	 */

	while (pmap->pm_obj.memq.tqh_first != NULL) {
		pg = pmap->pm_obj.memq.tqh_first;
#ifdef DIAGNOSTIC
		if (pg->flags & PG_BUSY)
			panic("pmap_release: busy page table page");
#endif
		/* pmap_page_protect?  currently no need for it. */

		pg->wire_count = 0;
		uvm_pagefree(pg);
	}

	/* XXX: need to flush it out of other processor's APTE space? */
	uvm_km_free(kernel_map, (vaddr_t)pmap->pm_pdir, NBPG);

#ifdef USER_LDT
	if (pmap->pm_flags & PMF_USER_LDT) {
		/*
		 * no need to switch the LDT; this address space is gone,
		 * nothing is using it.
		 */
		ldt_free(pmap);
		uvm_km_free(kernel_map, (vaddr_t)pmap->pm_ldt,
			    pmap->pm_ldt_len * sizeof(union descriptor));
	}
#endif
}

/*
 *	Add a reference to the specified pmap.
 */

void
pmap_reference(pmap)
	struct pmap *pmap;
{
	simple_lock(&pmap->pm_obj.vmobjlock);
	pmap->pm_obj.uo_refs++;
	simple_unlock(&pmap->pm_obj.vmobjlock);
}

#if defined(PMAP_FORK)
/*
 * pmap_fork: perform any necessary data structure manipulation when
 * a VM space is forked.
 */

void
pmap_fork(pmap1, pmap2)
	struct pmap *pmap1, *pmap2;
{
	simple_lock(&pmap1->pm_obj.vmobjlock);
	simple_lock(&pmap2->pm_obj.vmobjlock);

#ifdef USER_LDT
	/* Copy the LDT, if necessary. */
	if (pmap1->pm_flags & PMF_USER_LDT) {
		union descriptor *new_ldt;
		size_t len;

		len = pmap1->pm_ldt_len * sizeof(union descriptor);
		new_ldt = (union descriptor *)uvm_km_alloc(kernel_map, len);
		bcopy(pmap1->pm_ldt, new_ldt, len);
		pmap2->pm_ldt = new_ldt;
		pmap2->pm_ldt_len = pmap1->pm_ldt_len;
		pmap2->pm_flags |= PMF_USER_LDT;
		ldt_alloc(pmap2, new_ldt, len);
	}
#endif /* USER_LDT */

	simple_unlock(&pmap2->pm_obj.vmobjlock);
	simple_unlock(&pmap1->pm_obj.vmobjlock);
}
#endif /* PMAP_FORK */

#ifdef USER_LDT
/*
 * pmap_ldt_cleanup: if the pmap has a local LDT, deallocate it, and
 * restore the default.
 */

void
pmap_ldt_cleanup(p)
	struct proc *p;
{
	struct pcb *pcb = &p->p_addr->u_pcb;
	pmap_t pmap = p->p_vmspace->vm_map.pmap;
	union descriptor *old_ldt = NULL;
	size_t len = 0;

	simple_lock(&pmap->pm_obj.vmobjlock);

	if (pmap->pm_flags & PMF_USER_LDT) {
		ldt_free(pmap);
		pmap->pm_ldt_sel = GSEL(GLDT_SEL, SEL_KPL);
		pcb->pcb_ldt_sel = pmap->pm_ldt_sel;
		if (pcb == curpcb)
			lldt(pcb->pcb_ldt_sel);
		old_ldt = pmap->pm_ldt;
		len = pmap->pm_ldt_len * sizeof(union descriptor);
		pmap->pm_ldt = NULL;
		pmap->pm_ldt_len = 0;
		pmap->pm_flags &= ~PMF_USER_LDT;
	}

	simple_unlock(&pmap->pm_obj.vmobjlock);

	if (old_ldt != NULL)
		uvm_km_free(kernel_map, (vaddr_t)old_ldt, len);
}
#endif /* USER_LDT */

/*
 * pmap_activate: activate a process' pmap (fill in %cr3 and LDT info)
 *
 * => called from cpu_switch()
 * => if proc is the curproc, then load it into the MMU
 */

void
pmap_activate(p)
	struct proc *p;
{
	struct pcb *pcb = &p->p_addr->u_pcb;
	struct pmap *pmap = p->p_vmspace->vm_map.pmap;

	pcb->pcb_pmap = pmap;
	pcb->pcb_ldt_sel = pmap->pm_ldt_sel;
	pcb->pcb_cr3 = pmap->pm_pdirpa;
	if (p == curproc)
		lcr3(pcb->pcb_cr3);
	if (pcb == curpcb)
		lldt(pcb->pcb_ldt_sel);
}

/*
 * pmap_deactivate: deactivate a process' pmap
 *
 * => XXX: what should this do, if anything?
 */

void
pmap_deactivate(p)
	struct proc *p;
{
}

/*
 * end of lifecycle functions
 */

/*
 * some misc. functions
 */

/*
 * pmap_extract: extract a PA for the given VA
 */

boolean_t
pmap_extract(pmap, va, pap)
	struct pmap *pmap;
	vaddr_t va;
	paddr_t *pap;
{
	paddr_t retval;
	pt_entry_t *ptes;

	if (pmap->pm_pdir[pdei(va)]) {
		ptes = pmap_map_ptes(pmap);
		retval = (paddr_t)(ptes[i386_btop(va)] & PG_FRAME);
		pmap_unmap_ptes(pmap);
		if (pap != NULL)
			*pap = retval | (va & ~PG_FRAME);
		return (TRUE);
	}
	return (FALSE);
}

/*
 * pmap_virtual_space: used during bootup [pmap_steal_memory] to
 *	determine the bounds of the kernel virtual addess space.
 */

void
pmap_virtual_space(startp, endp)
	vaddr_t *startp;
	vaddr_t *endp;
{
	*startp = virtual_avail;
	*endp = virtual_end;
}

/*
 * pmap_zero_page: zero a page
 */

void
pmap_zero_page(struct vm_page *pg)
{
	paddr_t pa = VM_PAGE_TO_PHYS(pg);

	simple_lock(&pmap_zero_page_lock);
#ifdef DIAGNOSTIC
	if (*zero_pte)
		panic("pmap_zero_page: lock botch");
#endif

	*zero_pte = (pa & PG_FRAME) | PG_V | PG_RW;	/* map in */
	bzero(zerop, NBPG);				/* zero */
	*zero_pte = 0;				/* zap! */
	pmap_update_pg((vaddr_t)zerop);		/* flush TLB */
	simple_unlock(&pmap_zero_page_lock);
}

/*
 * pmap_zero_phys: same as pmap_zero_page, but for use before vm_pages are
 * initialized.
 */
void
pmap_zero_phys(paddr_t pa)
{
	simple_lock(&pmap_zero_page_lock);
#ifdef DIAGNOSTIC
	if (*zero_pte)
		panic("pmap_zero_page: lock botch");
#endif

	*zero_pte = (pa & PG_FRAME) | PG_V | PG_RW;	/* map in */
	bzero(zerop, NBPG);				/* zero */
	*zero_pte = 0;				/* zap! */
	pmap_update_pg((vaddr_t)zerop);		/* flush TLB */
	simple_unlock(&pmap_zero_page_lock);
}

/*
 * pmap_zero_page_uncached: the same, except uncached.
 */

boolean_t
pmap_zero_page_uncached(pa)
	paddr_t pa;
{
	simple_lock(&pmap_zero_page_lock);
#ifdef DIAGNOSTIC
	if (*zero_pte)
		panic("pmap_zero_page_uncached: lock botch");
#endif

	*zero_pte = (pa & PG_FRAME) | PG_V | PG_RW |	/* map in */
	    ((cpu_class != CPUCLASS_386) ? PG_N : 0);
	memset(zerop, 0, NBPG);				/* zero */
	*zero_pte = 0;					/* zap! */
	pmap_update_pg((vaddr_t)zerop);			/* flush TLB */
	simple_unlock(&pmap_zero_page_lock);

	return (TRUE);
}

/*
 * pmap_copy_page: copy a page
 */

void
pmap_copy_page(struct vm_page *srcpg, struct vm_page *dstpg)
{
	paddr_t srcpa = VM_PAGE_TO_PHYS(srcpg);
	paddr_t dstpa = VM_PAGE_TO_PHYS(dstpg);

	simple_lock(&pmap_copy_page_lock);
#ifdef DIAGNOSTIC
	if (*csrc_pte || *cdst_pte)
		panic("pmap_copy_page: lock botch");
#endif

	*csrc_pte = (srcpa & PG_FRAME) | PG_V | PG_RW;
	*cdst_pte = (dstpa & PG_FRAME) | PG_V | PG_RW;
	bcopy(csrcp, cdstp, PAGE_SIZE);
	*csrc_pte = *cdst_pte = 0;			/* zap! */
	pmap_update_2pg((vaddr_t)csrcp, (vaddr_t)cdstp);
	simple_unlock(&pmap_copy_page_lock);
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
 * => caller must hold pmap's lock
 * => PTP must be mapped into KVA
 * => PTP should be null if pmap == pmap_kernel()
 */

static void
pmap_remove_ptes(pmap, pmap_rr, ptp, ptpva, startva, endva)
	struct pmap *pmap;
	struct pmap_remove_record *pmap_rr;
	struct vm_page *ptp;
	vaddr_t ptpva;
	vaddr_t startva, endva;
{
	struct pv_entry *pv_tofree = NULL;	/* list of pv_entrys to free */
	struct pv_entry *pve;
	pt_entry_t *pte = (pt_entry_t *) ptpva;
	pt_entry_t opte;
	int bank, off;

	/*
	 * note that ptpva points to the PTE that maps startva.   this may
	 * or may not be the first PTE in the PTP.
	 *
	 * we loop through the PTP while there are still PTEs to look at
	 * and the wire_count is greater than 1 (because we use the wire_count
	 * to keep track of the number of real PTEs in the PTP).
	 */

	for (/*null*/; startva < endva && (ptp == NULL || ptp->wire_count > 1)
			     ; pte++, startva += NBPG) {
		if (!pmap_valid_entry(*pte))
			continue;			/* VA not mapped */

		opte = *pte;		/* save the old PTE */
		*pte = 0;			/* zap! */
		if (opte & PG_W)
			pmap->pm_stats.wired_count--;
		pmap->pm_stats.resident_count--;

		if (pmap_rr) {		/* worried about tlb flushing? */
			if (opte & PG_G) {
				/* PG_G requires this */
				pmap_update_pg(startva);
			} else {
				if (pmap_rr->prr_npages < PMAP_RR_MAX) {
					pmap_rr->prr_vas[pmap_rr->prr_npages++]
						= startva;
				} else {
					if (pmap_rr->prr_npages == PMAP_RR_MAX)
						/* signal an overflow */
						pmap_rr->prr_npages++;
				}
			}
		}
		if (ptp)
			ptp->wire_count--;		/* dropping a PTE */

		/*
		 * if we are not on a pv_head list we are done.
		 */

		if ((opte & PG_PVLIST) == 0) {
#ifdef DIAGNOSTIC
			if (vm_physseg_find(i386_btop(opte & PG_FRAME), &off)
			    != -1)
				panic("pmap_remove_ptes: managed page without "
				      "PG_PVLIST for 0x%lx", startva);
#endif
			continue;
		}

		bank = vm_physseg_find(i386_btop(opte & PG_FRAME), &off);
#ifdef DIAGNOSTIC
		if (bank == -1)
			panic("pmap_remove_ptes: unmanaged page marked "
			      "PG_PVLIST, va = 0x%lx, pa = 0x%lx",
			      startva, (u_long)(opte & PG_FRAME));
#endif

		/* sync R/M bits */
		simple_lock(&vm_physmem[bank].pmseg.pvhead[off].pvh_lock);
		vm_physmem[bank].pmseg.attrs[off] |= (opte & (PG_U|PG_M));
		pve = pmap_remove_pv(&vm_physmem[bank].pmseg.pvhead[off], pmap,
				     startva);
		simple_unlock(&vm_physmem[bank].pmseg.pvhead[off].pvh_lock);

		if (pve) {
			pve->pv_next = pv_tofree;
			pv_tofree = pve;
		}

		/* end of "for" loop: time for next pte */
	}
	if (pv_tofree)
		pmap_free_pvs(pmap, pv_tofree);
}


/*
 * pmap_remove_pte: remove a single PTE from a PTP
 *
 * => must have proper locking on pmap_master_lock
 * => caller must hold pmap's lock
 * => PTP must be mapped into KVA
 * => PTP should be null if pmap == pmap_kernel()
 * => returns true if we removed a mapping
 */

static boolean_t
pmap_remove_pte(pmap, ptp, pte, va)
	struct pmap *pmap;
	struct vm_page *ptp;
	pt_entry_t *pte;
	vaddr_t va;
{
	pt_entry_t opte;
	int bank, off;
	struct pv_entry *pve;

	if (!pmap_valid_entry(*pte))
		return(FALSE);		/* VA not mapped */

	opte = *pte;			/* save the old PTE */
	*pte = 0;			/* zap! */

	pmap_nxstack_account(pmap, va, opte, 0);

	if (opte & PG_W)
		pmap->pm_stats.wired_count--;
	pmap->pm_stats.resident_count--;

	if (ptp)
		ptp->wire_count--;		/* dropping a PTE */

	if (pmap_is_curpmap(pmap))
		pmap_update_pg(va);		/* flush TLB */

	/*
	 * if we are not on a pv_head list we are done.
	 */

	if ((opte & PG_PVLIST) == 0) {
#ifdef DIAGNOSTIC
		if (vm_physseg_find(i386_btop(opte & PG_FRAME), &off) != -1)
			panic("pmap_remove_pte: managed page without "
			      "PG_PVLIST for 0x%lx", va);
#endif
		return(TRUE);
	}

	bank = vm_physseg_find(i386_btop(opte & PG_FRAME), &off);
#ifdef DIAGNOSTIC
	if (bank == -1)
		panic("pmap_remove_pte: unmanaged page marked "
		    "PG_PVLIST, va = 0x%lx, pa = 0x%lx", va,
		    (u_long)(opte & PG_FRAME));
#endif

	/* sync R/M bits */
	simple_lock(&vm_physmem[bank].pmseg.pvhead[off].pvh_lock);
	vm_physmem[bank].pmseg.attrs[off] |= (opte & (PG_U|PG_M));
	pve = pmap_remove_pv(&vm_physmem[bank].pmseg.pvhead[off], pmap, va);
	simple_unlock(&vm_physmem[bank].pmseg.pvhead[off].pvh_lock);

	if (pve)
		pmap_free_pv(pmap, pve);
	return(TRUE);
}

/*
 * pmap_remove: top level mapping removal function
 *
 * => caller should not be holding any pmap locks
 */

void
pmap_remove(pmap, sva, eva)
	struct pmap *pmap;
	vaddr_t sva, eva;
{
	pt_entry_t *ptes;
	boolean_t result;
	paddr_t ptppa;
	vaddr_t blkendva;
	struct vm_page *ptp;
	struct pmap_remove_record pmap_rr, *prr;

	/*
	 * we lock in the pmap => pv_head direction
	 */

	PMAP_MAP_TO_HEAD_LOCK();
	ptes = pmap_map_ptes(pmap);	/* locks pmap */

	/*
	 * removing one page?  take shortcut function.
	 */

	if (sva + PAGE_SIZE == eva) {

		if (pmap_valid_entry(pmap->pm_pdir[pdei(sva)])) {

			/* PA of the PTP */
			ptppa = pmap->pm_pdir[pdei(sva)] & PG_FRAME;

			/* get PTP if non-kernel mapping */

			if (pmap == pmap_kernel()) {
				/* we never free kernel PTPs */
				ptp = NULL;
			} else {
				if (pmap->pm_ptphint &&
				    VM_PAGE_TO_PHYS(pmap->pm_ptphint) ==
				    ptppa) {
					ptp = pmap->pm_ptphint;
				} else {
					ptp = PHYS_TO_VM_PAGE(ptppa);
#ifdef DIAGNOSTIC
					if (ptp == NULL)
						panic("pmap_remove: unmanaged "
						      "PTP detected");
#endif
				}
			}

			/* do it! */
			result = pmap_remove_pte(pmap, ptp,
						 &ptes[i386_btop(sva)], sva);

			/*
			 * if mapping removed and the PTP is no longer
			 * being used, free it!
			 */

			if (result && ptp && ptp->wire_count <= 1) {
				pmap->pm_pdir[pdei(sva)] = 0;	/* zap! */
#if defined(I386_CPU)
				/* already dumped whole TLB on i386 */
				if (cpu_class != CPUCLASS_386)
#endif
				{
					pmap_update_pg(((vaddr_t) ptes) +
						       ptp->offset);
				}
				pmap->pm_stats.resident_count--;
				if (pmap->pm_ptphint == ptp)
					pmap->pm_ptphint =
					    TAILQ_FIRST(&pmap->pm_obj.memq);
				ptp->wire_count = 0;
				uvm_pagefree(ptp);
			}
		}

		pmap_unmap_ptes(pmap);		/* unlock pmap */
		PMAP_MAP_TO_HEAD_UNLOCK();
		return;
	}

	/*
	 * removing a range of pages: we unmap in PTP sized blocks (4MB)
	 *
	 * if we are the currently loaded pmap, we use prr to keep track
	 * of the VAs we unload so that we can flush them out of the tlb.
	 */

	if (pmap_is_curpmap(pmap)) {
		prr = &pmap_rr;
		prr->prr_npages = 0;
	} else {
		prr = NULL;
	}

	for (/* null */ ; sva < eva ; sva = blkendva) {

		/* determine range of block */
		blkendva = i386_round_pdr(sva+1);
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

		if (pdei(sva) == PDSLOT_PTE)
			/* XXXCDC: ugly hack to avoid freeing PDP here */
			continue;

		if (!pmap_valid_entry(pmap->pm_pdir[pdei(sva)]))
			/* valid block? */
			continue;

		/* PA of the PTP */
		ptppa = (pmap->pm_pdir[pdei(sva)] & PG_FRAME);

		/* get PTP if non-kernel mapping */
		if (pmap == pmap_kernel()) {
			/* we never free kernel PTPs */
			ptp = NULL;
		} else {
			if (pmap->pm_ptphint &&
			    VM_PAGE_TO_PHYS(pmap->pm_ptphint) == ptppa) {
				ptp = pmap->pm_ptphint;
			} else {
				ptp = PHYS_TO_VM_PAGE(ptppa);
#ifdef DIAGNOSTIC
				if (ptp == NULL)
					panic("pmap_remove: unmanaged PTP "
					      "detected");
#endif
			}
		}
		pmap_remove_ptes(pmap, prr, ptp,
				 (vaddr_t)&ptes[i386_btop(sva)], sva, blkendva);

		/* if PTP is no longer being used, free it! */
		if (ptp && ptp->wire_count <= 1) {
			pmap->pm_pdir[pdei(sva)] = 0;	/* zap! */
			pmap_update_pg( ((vaddr_t) ptes) + ptp->offset);
#if defined(I386_CPU)
			/* cancel possible pending pmap update on i386 */
			if (cpu_class == CPUCLASS_386 && prr)
				prr->prr_npages = 0;
#endif
			pmap->pm_stats.resident_count--;
			if (pmap->pm_ptphint == ptp)	/* update hint? */
				pmap->pm_ptphint =
				    TAILQ_FIRST(&pmap->pm_obj.memq);
			ptp->wire_count = 0;
			uvm_pagefree(ptp);
		}
	}

	/*
	 * if we kept a removal record and removed some pages update the TLB
	 */

	if (prr && prr->prr_npages) {
#if defined(I386_CPU)
		if (cpu_class == CPUCLASS_386) {
			tlbflush();
		} else
#endif
		{ /* not I386 */
			if (prr->prr_npages > PMAP_RR_MAX) {
				tlbflush();
			} else {
				while (prr->prr_npages) {
					pmap_update_pg(
					    prr->prr_vas[--prr->prr_npages]);
				}
			}
		} /* not I386 */
	}
	pmap_unmap_ptes(pmap);
	PMAP_MAP_TO_HEAD_UNLOCK();
}

/*
 * pmap_page_remove: remove a managed vm_page from all pmaps that map it
 *
 * => we set pv_head => pmap locking
 * => R/M bits are sync'd back to attrs
 */

void
pmap_page_remove(pg)
	struct vm_page *pg;
{
	int bank, off;
	struct pv_head *pvh;
	struct pv_entry *pve;
	pt_entry_t *ptes, opte;
#if defined(I386_CPU)
	boolean_t needs_update = FALSE;
#endif

	/* XXX: vm_page should either contain pv_head or have a pointer to it */
	bank = vm_physseg_find(atop(VM_PAGE_TO_PHYS(pg)), &off);
	if (bank == -1) {
		printf("pmap_page_remove: unmanaged page?\n");
		return;
	}

	pvh = &vm_physmem[bank].pmseg.pvhead[off];
	if (pvh->pvh_list == NULL) {
		return;
	}

	/* set pv_head => pmap locking */
	PMAP_HEAD_TO_MAP_LOCK();

	/* XXX: needed if we hold head->map lock? */
	simple_lock(&pvh->pvh_lock);

	for (pve = pvh->pvh_list ; pve != NULL ; pve = pve->pv_next) {
		ptes = pmap_map_ptes(pve->pv_pmap);		/* locks pmap */

#ifdef DIAGNOSTIC
		if (pve->pv_va >= uvm.pager_sva && pve->pv_va < uvm.pager_eva) {
			printf("pmap_page_remove: found pager VA on pv_list\n");
		}
		if (pve->pv_ptp && (pve->pv_pmap->pm_pdir[pdei(pve->pv_va)] &
				    PG_FRAME)
		    != VM_PAGE_TO_PHYS(pve->pv_ptp)) {
			printf("pmap_page_remove: pg=%p: va=%lx, pv_ptp=%p\n",
			       pg, pve->pv_va, pve->pv_ptp);
			printf("pmap_page_remove: PTP's phys addr: "
			       "actual=%x, recorded=%lx\n",
			       (pve->pv_pmap->pm_pdir[pdei(pve->pv_va)] &
				PG_FRAME), VM_PAGE_TO_PHYS(pve->pv_ptp));
			panic("pmap_page_remove: mapped managed page has "
			      "invalid pv_ptp field");
		}
#endif

		opte = ptes[i386_btop(pve->pv_va)];
		ptes[i386_btop(pve->pv_va)] = 0;		/* zap! */

		if (opte & PG_W)
			pve->pv_pmap->pm_stats.wired_count--;
		pve->pv_pmap->pm_stats.resident_count--;

		if (pmap_is_curpmap(pve->pv_pmap)) {
#if defined(I386_CPU)
			if (cpu_class == CPUCLASS_386)
				needs_update = TRUE;
			else
#endif
				pmap_update_pg(pve->pv_va);
		}

		/* sync R/M bits */
		vm_physmem[bank].pmseg.attrs[off] |= (opte & (PG_U|PG_M));

		/* update the PTP reference count.  free if last reference. */
		if (pve->pv_ptp) {
			pve->pv_ptp->wire_count--;
			if (pve->pv_ptp->wire_count <= 1) {
				/* zap! */
				pve->pv_pmap->pm_pdir[pdei(pve->pv_va)] = 0;
				pmap_update_pg(((vaddr_t)ptes) +
					       pve->pv_ptp->offset);
#if defined(I386_CPU)
				needs_update = FALSE;
#endif
				pve->pv_pmap->pm_stats.resident_count--;
				/* update hint? */
				if (pve->pv_pmap->pm_ptphint == pve->pv_ptp)
					pve->pv_pmap->pm_ptphint =
					    TAILQ_FIRST(&pve->pv_pmap->pm_obj.memq);
				pve->pv_ptp->wire_count = 0;
				uvm_pagefree(pve->pv_ptp);
			}
		}
		pmap_unmap_ptes(pve->pv_pmap);		/* unlocks pmap */
	}
	pmap_free_pvs(NULL, pvh->pvh_list);
	pvh->pvh_list = NULL;
	simple_unlock(&pvh->pvh_lock);
	PMAP_HEAD_TO_MAP_UNLOCK();
#if defined(I386_CPU)
	if (needs_update)
		tlbflush();
#endif
}

/*
 * p m a p   a t t r i b u t e  f u n c t i o n s
 * functions that test/change managed page's attributes
 * since a page can be mapped multiple times we must check each PTE that
 * maps it by going down the pv lists.
 */

/*
 * pmap_test_attrs: test a page's attributes
 *
 * => we set pv_head => pmap locking
 */

boolean_t
pmap_test_attrs(pg, testbits)
	struct vm_page *pg;
	int testbits;
{
	int bank, off;
	char *myattrs;
	struct pv_head *pvh;
	struct pv_entry *pve;
	pt_entry_t *ptes, pte;

	/* XXX: vm_page should either contain pv_head or have a pointer to it */
	bank = vm_physseg_find(atop(VM_PAGE_TO_PHYS(pg)), &off);
	if (bank == -1) {
		printf("pmap_test_attrs: unmanaged page?\n");
		return(FALSE);
	}

	/*
	 * before locking: see if attributes are already set and if so,
	 * return!
	 */

	myattrs = &vm_physmem[bank].pmseg.attrs[off];
	if (*myattrs & testbits)
		return(TRUE);

	/* test to see if there is a list before bothering to lock */
	pvh = &vm_physmem[bank].pmseg.pvhead[off];
	if (pvh->pvh_list == NULL) {
		return(FALSE);
	}

	/* nope, gonna have to do it the hard way */
	PMAP_HEAD_TO_MAP_LOCK();
	/* XXX: needed if we hold head->map lock? */
	simple_lock(&pvh->pvh_lock);

	for (pve = pvh->pvh_list; pve != NULL && (*myattrs & testbits) == 0;
	     pve = pve->pv_next) {
		ptes = pmap_map_ptes(pve->pv_pmap);
		pte = ptes[i386_btop(pve->pv_va)];
		pmap_unmap_ptes(pve->pv_pmap);
		*myattrs |= pte;
	}

	/*
	 * note that we will exit the for loop with a non-null pve if
	 * we have found the bits we are testing for.
	 */

	simple_unlock(&pvh->pvh_lock);
	PMAP_HEAD_TO_MAP_UNLOCK();
	return((*myattrs & testbits) != 0);
}

/*
 * pmap_change_attrs: change a page's attributes
 *
 * => we set pv_head => pmap locking
 * => we return TRUE if we cleared one of the bits we were asked to
 */

boolean_t
pmap_change_attrs(pg, setbits, clearbits)
	struct vm_page *pg;
	int setbits, clearbits;
{
	u_int32_t result;
	int bank, off;
	struct pv_head *pvh;
	struct pv_entry *pve;
	pt_entry_t *ptes, npte;
	char *myattrs;
#if defined(I386_CPU)
	boolean_t needs_update = FALSE;
#endif

	/* XXX: vm_page should either contain pv_head or have a pointer to it */
	bank = vm_physseg_find(atop(VM_PAGE_TO_PHYS(pg)), &off);
	if (bank == -1) {
		printf("pmap_change_attrs: unmanaged page?\n");
		return(FALSE);
	}

	PMAP_HEAD_TO_MAP_LOCK();
	pvh = &vm_physmem[bank].pmseg.pvhead[off];
	/* XXX: needed if we hold head->map lock? */
	simple_lock(&pvh->pvh_lock);

	myattrs = &vm_physmem[bank].pmseg.attrs[off];
	result = *myattrs & clearbits;
	*myattrs = (*myattrs | setbits) & ~clearbits;

	for (pve = pvh->pvh_list; pve != NULL; pve = pve->pv_next) {
#ifdef DIAGNOSTIC
		if (!pmap_valid_entry(pve->pv_pmap->pm_pdir[pdei(pve->pv_va)]))
			panic("pmap_change_attrs: mapping without PTP "
			      "detected");
#endif

		ptes = pmap_map_ptes(pve->pv_pmap);		/* locks pmap */
		npte = ptes[i386_btop(pve->pv_va)];
		result |= (npte & clearbits);
		npte = (npte | setbits) & ~clearbits;
		if (ptes[i386_btop(pve->pv_va)] != npte) {
			ptes[i386_btop(pve->pv_va)] = npte;	/* zap! */

			if (pmap_is_curpmap(pve->pv_pmap)) {
#if defined(I386_CPU)
				if (cpu_class == CPUCLASS_386)
					needs_update = TRUE;
				else
#endif
					pmap_update_pg(pve->pv_va);
			}
		}
		pmap_unmap_ptes(pve->pv_pmap);		/* unlocks pmap */
	}

	simple_unlock(&pvh->pvh_lock);
	PMAP_HEAD_TO_MAP_UNLOCK();

#if defined(I386_CPU)
	if (needs_update)
		tlbflush();
#endif
	return(result != 0);
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
pmap_write_protect(pmap, sva, eva, prot)
	struct pmap *pmap;
	vaddr_t sva, eva;
	vm_prot_t prot;
{
	pt_entry_t *ptes, *spte, *epte, npte;
	struct pmap_remove_record pmap_rr, *prr;
	vaddr_t blockend, va;
	u_int32_t md_prot;

	ptes = pmap_map_ptes(pmap);		/* locks pmap */

	/* need to worry about TLB? [TLB stores protection bits] */
	if (pmap_is_curpmap(pmap)) {
		prr = &pmap_rr;
		prr->prr_npages = 0;
	} else {
		prr = NULL;
	}

	/* should be ok, but just in case ... */
	sva &= PG_FRAME;
	eva &= PG_FRAME;

	for (/* null */ ; sva < eva ; sva = blockend) {

		blockend = (sva & PD_MASK) + NBPD;
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
		if (pdei(sva) == PDSLOT_PTE)
			continue;

		/* empty block? */
		if (!pmap_valid_entry(pmap->pm_pdir[pdei(sva)]))
			continue;

		md_prot = protection_codes[prot];
		if (sva < VM_MAXUSER_ADDRESS)
			md_prot |= PG_u;
		else if (sva < VM_MAX_ADDRESS)
			/* XXX: write-prot our PTES? never! */
			md_prot |= (PG_u | PG_RW);

		spte = &ptes[i386_btop(sva)];
		epte = &ptes[i386_btop(blockend)];

		for (/*null */; spte < epte ; spte++, sva += PAGE_SIZE) {

			if (!pmap_valid_entry(*spte))	/* no mapping? */
				continue;

			npte = (*spte & ~PG_PROT) | md_prot;

			if (npte != *spte) {
				/* account for executable pages on the stack */
				pmap_nxstack_account(pmap, sva, *spte, npte);

				*spte = npte;		/* zap! */

				if (prr) {    /* worried about tlb flushing? */
					va = i386_ptob(spte - ptes);
					if (npte & PG_G) {
						/* PG_G requires this */
						pmap_update_pg(va);
					} else {
						if (prr->prr_npages <
						    PMAP_RR_MAX) {
							prr->prr_vas[
							    prr->prr_npages++] =
								va;
						} else {
						    if (prr->prr_npages ==
							PMAP_RR_MAX)
							/* signal an overflow */
							    prr->prr_npages++;
						}
					}
				}	/* if (prr) */
			}	/* npte != *spte */
		}	/* for loop */
	}

	/*
	 * if we kept a removal record and removed some pages update the TLB
	 */

	if (prr && prr->prr_npages) {
#if defined(I386_CPU)
		if (cpu_class == CPUCLASS_386) {
			tlbflush();
		} else
#endif
		{ /* not I386 */
			if (prr->prr_npages > PMAP_RR_MAX) {
				tlbflush();
			} else {
				while (prr->prr_npages) {
					pmap_update_pg(prr->prr_vas[
						       --prr->prr_npages]);
				}
			}
		} /* not I386 */
	}
	pmap_unmap_ptes(pmap);		/* unlocks pmap */
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
pmap_unwire(pmap, va)
	struct pmap *pmap;
	vaddr_t va;
{
	pt_entry_t *ptes;

	if (pmap_valid_entry(pmap->pm_pdir[pdei(va)])) {
		ptes = pmap_map_ptes(pmap);		/* locks pmap */

#ifdef DIAGNOSTIC
		if (!pmap_valid_entry(ptes[i386_btop(va)]))
			panic("pmap_unwire: invalid (unmapped) va 0x%lx", va);
#endif
		if ((ptes[i386_btop(va)] & PG_W) != 0) {
			ptes[i386_btop(va)] &= ~PG_W;
			pmap->pm_stats.wired_count--;
		}
#ifdef DIAGNOSTIC
		else {
			printf("pmap_unwire: wiring for pmap %p va 0x%lx "
			       "didn't change!\n", pmap, va);
		}
#endif
		pmap_unmap_ptes(pmap);		/* unlocks map */
	}
#ifdef DIAGNOSTIC
	else {
		panic("pmap_unwire: invalid PDE");
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
pmap_collect(pmap)
	struct pmap *pmap;
{
	/*
	 * free all of the pt pages by removing the physical mappings
	 * for its entire address space.
	 */

	pmap_remove(pmap, VM_MIN_ADDRESS, VM_MAX_ADDRESS);
}

/*
 * pmap_transfer: transfer (move or copy) mapping from one pmap
 * 	to another.
 *
 * => this function is optional, it doesn't have to do anything
 * => we assume that the mapping in the src pmap is valid (i.e. that
 *    it doesn't run off the end of the map's virtual space).
 * => we assume saddr, daddr, and len are page aligned/lengthed
 */

void
pmap_transfer(dstpmap, srcpmap, daddr, len, saddr, move)
	struct pmap *dstpmap, *srcpmap;
	vaddr_t daddr, saddr;
	vsize_t len;
	boolean_t move;
{
	/* base address of PTEs, dst could be NULL */
	pt_entry_t *srcptes, *dstptes;

	struct pmap_transfer_location srcl, dstl;
	int dstvalid;		  /* # of PTEs left in dst's current PTP */
	struct pmap *mapped_pmap; /* the pmap we passed to pmap_map_ptes */
	vsize_t blklen;
	int blkpgs, toxfer;
	boolean_t ok;

#ifdef DIAGNOSTIC
	/*
	 * sanity check: let's make sure our len doesn't overflow our dst
	 * space.
	 */

	if (daddr < VM_MAXUSER_ADDRESS) {
		if (VM_MAXUSER_ADDRESS - daddr < len) {
			printf("pmap_transfer: no room in user pmap "
			       "(addr=0x%lx, len=0x%lx)\n", daddr, len);
			return;
		}
	} else if (daddr < VM_MIN_KERNEL_ADDRESS ||
		   daddr >= VM_MAX_KERNEL_ADDRESS) {
		printf("pmap_transfer: invalid transfer address 0x%lx\n",
		       daddr);
	} else {
		if (VM_MAX_KERNEL_ADDRESS - daddr < len) {
			printf("pmap_transfer: no room in kernel pmap "
			       "(addr=0x%lx, len=0x%lx)\n", daddr, len);
			return;
		}
	}
#endif

	/*
	 * ideally we would like to have either src or dst pmap's be the
	 * current pmap so that we can map the other one in APTE space
	 * (if needed... one of the maps could be the kernel's pmap).
	 *
	 * however, if we can't get this, then we have to use the tmpmap
	 * (alternately we could punt).
	 */

	if (!pmap_is_curpmap(dstpmap) && !pmap_is_curpmap(srcpmap)) {
		dstptes = NULL;			/* dstptes NOT mapped */
		srcptes = pmap_map_ptes(srcpmap);   /* let's map the source */
		mapped_pmap = srcpmap;
	} else {
		if (!pmap_is_curpmap(srcpmap)) {
			srcptes = pmap_map_ptes(srcpmap);   /* possible APTE */
			dstptes = PTE_BASE;
			mapped_pmap = srcpmap;
		} else {
			dstptes = pmap_map_ptes(dstpmap);   /* possible APTE */
			srcptes = PTE_BASE;
			mapped_pmap = dstpmap;
		}
	}

	/*
	 * at this point we know that the srcptes are mapped.   the dstptes
	 * are mapped if (dstptes != NULL).    if (dstptes == NULL) then we
	 * will have to map the dst PTPs page at a time using the tmpmap.
	 * [XXX: is it worth the effort, or should we just punt?]
	 */

	srcl.addr = saddr;
	srcl.pte = &srcptes[i386_btop(srcl.addr)];
	srcl.ptp = NULL;
	dstl.addr = daddr;
	if (dstptes)
		dstl.pte = &dstptes[i386_btop(dstl.addr)];
	else
		dstl.pte  = NULL;		/* we map page at a time */
	dstl.ptp = NULL;
	dstvalid = 0;		/* force us to load a new dst PTP to start */

	while (len) {

		/*
		 * compute the size of this block.
		 */

		/* length in bytes */
		blklen = i386_round_pdr(srcl.addr+1) - srcl.addr;
		if (blklen > len)
			blklen = len;
		blkpgs = i386_btop(blklen);

		/*
		 * if the block is not valid in the src pmap,
		 * then we can skip it!
		 */

		if (!pmap_valid_entry(srcpmap->pm_pdir[pdei(srcl.addr)])) {
			len = len - blklen;
			srcl.pte  = srcl.pte + blkpgs;
			srcl.addr += blklen;
			dstl.addr += blklen;
			if (blkpgs > dstvalid) {
				dstvalid = 0;
				dstl.ptp = NULL;
			} else {
				dstvalid = dstvalid - blkpgs;
			}
			if (dstptes == NULL && (len == 0 || dstvalid == 0)) {
				if (dstl.pte) {
					pmap_tmpunmap_pa();
					dstl.pte = NULL;
				}
			} else {
				dstl.pte += blkpgs;
			}
			continue;
		}

		/*
		 * we have a valid source block of "blkpgs" PTEs to transfer.
		 * if we don't have any dst PTEs ready, then get some.
		 */

		if (dstvalid == 0) {
			if (!pmap_valid_entry(dstpmap->
					      pm_pdir[pdei(dstl.addr)])) {
#ifdef DIAGNOSTIC
				if (dstl.addr >= VM_MIN_KERNEL_ADDRESS)
					panic("pmap_transfer: missing kernel "
					      "PTP at 0x%lx", dstl.addr);
#endif
				dstl.ptp = pmap_get_ptp(dstpmap,
							pdei(dstl.addr), TRUE);
				if (dstl.ptp == NULL)	/* out of RAM?  punt. */
					break;
			} else {
				dstl.ptp = NULL;
			}
			dstvalid = i386_btop(i386_round_pdr(dstl.addr+1) -
					     dstl.addr);
			if (dstptes == NULL) {
				dstl.pte = (pt_entry_t *)
					pmap_tmpmap_pa(dstpmap->
						       pm_pdir[pdei(dstl.addr)]
						       & PG_FRAME);
				dstl.pte = dstl.pte + (PTES_PER_PTP - dstvalid);
			}
		}

		/*
		 * we have a valid source block of "blkpgs" PTEs to transfer.
		 * we have a valid dst block of "dstvalid" PTEs ready.
		 * thus we can transfer min(blkpgs, dstvalid) PTEs now.
		 */

		srcl.ptp = NULL;	/* don't know source PTP yet */
		if (dstvalid < blkpgs)
			toxfer = dstvalid;
		else
			toxfer = blkpgs;

		if (toxfer > 0) {
			ok = pmap_transfer_ptes(srcpmap, &srcl, dstpmap, &dstl,
						toxfer, move);

			if (!ok)		/* memory shortage?  punt. */
				break;

			dstvalid -= toxfer;
			blkpgs -= toxfer;
			len -= i386_ptob(toxfer);
			if (blkpgs == 0)	/* out of src PTEs?  restart */
				continue;
		}

		/*
		 * we have a valid source block of "blkpgs" PTEs left
		 * to transfer.  we have just used up our "dstvalid"
		 * PTEs, and thus must obtain more dst PTEs to finish
		 * off the src block.  since we are now going to
		 * obtain a brand new dst PTP, we know we can finish
		 * the src block in one more transfer.
		 */

#ifdef DIAGNOSTIC
		if (dstvalid)
			panic("pmap_transfer: dstvalid non-zero after drain");
		if ((dstl.addr & (NBPD-1)) != 0)
			panic("pmap_transfer: dstaddr not on PD boundary "
			      "(0x%lx)\n", dstl.addr);
#endif

		if (dstptes == NULL && dstl.pte != NULL) {
			/* dispose of old PT mapping */
			pmap_tmpunmap_pa();
			dstl.pte = NULL;
		}

		/*
		 * get new dst PTP
		 */
		if (!pmap_valid_entry(dstpmap->pm_pdir[pdei(dstl.addr)])) {
#ifdef DIAGNOSTIC
			if (dstl.addr >= VM_MIN_KERNEL_ADDRESS)
				panic("pmap_transfer: missing kernel PTP at "
				      "0x%lx", dstl.addr);
#endif
			dstl.ptp = pmap_get_ptp(dstpmap, pdei(dstl.addr), TRUE);
			if (dstl.ptp == NULL)	/* out of free RAM?  punt. */
				break;
		} else {
			dstl.ptp = NULL;
		}

		dstvalid = PTES_PER_PTP;	/* new PTP */

		/*
		 * if the dstptes are un-mapped, then we need to tmpmap in the
		 * dstl.ptp.
		 */

		if (dstptes == NULL) {
			dstl.pte = (pt_entry_t *)
				pmap_tmpmap_pa(dstpmap->pm_pdir[pdei(dstl.addr)]
					       & PG_FRAME);
		}

		/*
		 * we have a valid source block of "blkpgs" PTEs left
		 * to transfer.  we just got a brand new dst PTP to
		 * receive these PTEs.
		 */

#ifdef DIAGNOSTIC
		if (dstvalid < blkpgs)
			panic("pmap_transfer: too many blkpgs?");
#endif
		toxfer = blkpgs;
		ok = pmap_transfer_ptes(srcpmap, &srcl, dstpmap, &dstl, toxfer,
					move);

		if (!ok)		/* memory shortage?   punt. */
			break;

		dstvalid -= toxfer;
		blkpgs -= toxfer;
		len -= i386_ptob(toxfer);

		/*
		 * done src pte block
		 */
	}
	if (dstptes == NULL && dstl.pte != NULL)
		pmap_tmpunmap_pa();		/* dst PTP still mapped? */
	pmap_unmap_ptes(mapped_pmap);
}

/*
 * pmap_transfer_ptes: transfer PTEs from one pmap to another
 *
 * => we assume that the needed PTPs are mapped and that we will
 *	not cross a block boundary.
 * => we return TRUE if we transfered all PTEs, FALSE if we were
 *	unable to allocate a pv_entry
 */

static boolean_t
pmap_transfer_ptes(srcpmap, srcl, dstpmap, dstl, toxfer, move)
	struct pmap *srcpmap, *dstpmap;
	struct pmap_transfer_location *srcl, *dstl;
	int toxfer;
	boolean_t move;
{
	pt_entry_t dstproto, opte;
	int bank, off;
	struct pv_head *pvh;
	struct pv_entry *pve, *lpve;

	/*
	 * generate "prototype" dst PTE
	 */

	if (dstl->addr < VM_MAX_ADDRESS)
		dstproto = PG_u;		/* "user" page */
	else
		dstproto = pmap_pg_g;	/* kernel page */

	/*
	 * ensure we have dst PTP for user addresses.
	 */

	if (dstl->ptp == NULL && dstl->addr < VM_MAXUSER_ADDRESS)
		dstl->ptp = PHYS_TO_VM_PAGE(dstpmap->pm_pdir[pdei(dstl->addr)] &
					    PG_FRAME);

	/*
	 * main loop over range
	 */

	for (/*null*/; toxfer > 0 ; toxfer--,
			     srcl->addr += NBPG, dstl->addr += NBPG,
			     srcl->pte++, dstl->pte++) {

		if (!pmap_valid_entry(*srcl->pte))  /* skip invalid entrys */
			continue;

#ifdef DIAGNOSTIC
		if (pmap_valid_entry(*dstl->pte))
			panic("pmap_transfer_ptes: attempt to overwrite "
			      "active entry");
#endif

		/*
		 * let's not worry about non-pvlist mappings (typically device
		 * pager mappings).
		 */

		opte = *srcl->pte;

		if ((opte & PG_PVLIST) == 0)
			continue;

		/*
		 * if we are moving the mapping, then we can just adjust the
		 * current pv_entry.    if we are copying the mapping, then we
		 * need to allocate a new pv_entry to account for it.
		 */

		if (move == FALSE) {
			pve = pmap_alloc_pv(dstpmap, ALLOCPV_TRY);
			if (pve == NULL)
				return(FALSE); 		/* punt! */
		} else {
			pve = NULL;  /* XXX: quiet gcc warning */
		}

		/*
		 * find the pv_head for this mapping.  since our mapping is
		 * on the pvlist (PG_PVLIST), there must be a pv_head.
		 */

		bank = vm_physseg_find(atop(opte & PG_FRAME), &off);
#ifdef DIAGNOSTIC
		if (bank == -1)
			panic("pmap_transfer_ptes: PG_PVLIST PTE and "
			      "no pv_head!");
#endif
		pvh = &vm_physmem[bank].pmseg.pvhead[off];

		/*
		 * now lock down the pvhead and find the current entry (there
		 * must be one).
		 */

		simple_lock(&pvh->pvh_lock);
		for (lpve = pvh->pvh_list ; lpve ; lpve = lpve->pv_next)
			if (lpve->pv_pmap == srcpmap &&
			    lpve->pv_va == srcl->addr)
				break;
#ifdef DIAGNOSTIC
		if (lpve == NULL)
			panic("pmap_transfer_ptes: PG_PVLIST PTE, but "
			      "entry not found");
#endif

		/*
		 * update src ptp.   if the ptp is null in the pventry, then
		 * we are not counting valid entrys for this ptp (this is only
		 * true for kernel PTPs).
		 */

		if (srcl->ptp == NULL)
			srcl->ptp = lpve->pv_ptp;
#ifdef DIAGNOSTIC
		if (srcl->ptp &&
		    (srcpmap->pm_pdir[pdei(srcl->addr)] & PG_FRAME) !=
		    VM_PAGE_TO_PHYS(srcl->ptp))
			panic("pmap_transfer_ptes: pm_pdir - pv_ptp mismatch!");
#endif

		/*
		 * for move, update the pve we just found (lpve) to
		 * point to its new mapping.  for copy, init the new
		 * pve and put it in the list.
		 */

		if (move == TRUE) {
			pve = lpve;
		}
		pve->pv_pmap = dstpmap;
		pve->pv_va = dstl->addr;
		pve->pv_ptp = dstl->ptp;
		if (move == FALSE) {		/* link in copy */
			pve->pv_next = lpve->pv_next;
			lpve->pv_next = pve;
		}

		/*
		 * sync the R/M bits while we are here.
		 */

		vm_physmem[bank].pmseg.attrs[off] |= (opte & (PG_U|PG_M));

		/*
		 * now actually update the ptes and unlock the pvlist.
		 */

		if (move) {
			*srcl->pte = 0;		/* zap! */
			if (pmap_is_curpmap(srcpmap))
				pmap_update_pg(srcl->addr);
			if (srcl->ptp)
				/* don't bother trying to free PTP */
				srcl->ptp->wire_count--;
			srcpmap->pm_stats.resident_count--;
			if (opte & PG_W)
				srcpmap->pm_stats.wired_count--;
		}
		*dstl->pte = (opte & ~(PG_u|PG_U|PG_M|PG_G|PG_W)) | dstproto;
		dstpmap->pm_stats.resident_count++;
		if (dstl->ptp)
			dstl->ptp->wire_count++;
		simple_unlock(&pvh->pvh_lock);
	}
	return(TRUE);
}

/*
 * pmap_copy: copy mappings from one pmap to another
 *
 * => optional function
 * void pmap_copy(dst_pmap, src_pmap, dst_addr, len, src_addr)
 */

/*
 * defined as macro call in pmap.h
 */

/*
 * pmap_enter: enter a mapping into a pmap
 *
 * => must be done "now" ... no lazy-evaluation
 * => we set pmap => pv_head locking
 */

int
pmap_enter(pmap, va, pa, prot, flags)
	struct pmap *pmap;
	vaddr_t va;
	paddr_t pa;
	vm_prot_t prot;
	int flags;
{
	pt_entry_t *ptes, opte, npte;
	struct vm_page *ptp;
	struct pv_head *pvh;
	struct pv_entry *pve;
	int bank, off, error;
	boolean_t wired = (flags & PMAP_WIRED) != 0;

#ifdef DIAGNOSTIC
	/* sanity check: totally out of range? */
	if (va >= VM_MAX_KERNEL_ADDRESS)
		panic("pmap_enter: too big");

	if (va == (vaddr_t) PDP_BASE || va == (vaddr_t) APDP_BASE)
		panic("pmap_enter: trying to map over PDP/APDP!");

	/* sanity check: kernel PTPs should already have been pre-allocated */
	if (va >= VM_MIN_KERNEL_ADDRESS &&
	    !pmap_valid_entry(pmap->pm_pdir[pdei(va)]))
		panic("pmap_enter: missing kernel PTP!");
#endif

	/* get lock */
	PMAP_MAP_TO_HEAD_LOCK();

	/*
	 * map in ptes and get a pointer to our PTP (unless we are the kernel)
	 */

	ptes = pmap_map_ptes(pmap);		/* locks pmap */
	if (pmap == pmap_kernel()) {
		ptp = NULL;
	} else {
		ptp = pmap_get_ptp(pmap, pdei(va), FALSE);
		if (ptp == NULL) {
			if (flags & PMAP_CANFAIL) {
				return (KERN_RESOURCE_SHORTAGE);
			}
			panic("pmap_enter: get ptp failed");
		}
	}
	opte = ptes[i386_btop(va)];		/* old PTE */

	/*
	 * is there currently a valid mapping at our VA?
	 */

	if (pmap_valid_entry(opte)) {

		/*
		 * first, update pm_stats.  resident count will not
		 * change since we are replacing/changing a valid
		 * mapping.  wired count might change...
		 */

		if (wired && (opte & PG_W) == 0)
			pmap->pm_stats.wired_count++;
		else if (!wired && (opte & PG_W) != 0)
			pmap->pm_stats.wired_count--;

		/*
		 * is the currently mapped PA the same as the one we
		 * want to map?
		 */

		if ((opte & PG_FRAME) == pa) {

			/* if this is on the PVLIST, sync R/M bit */
			if (opte & PG_PVLIST) {
				bank = vm_physseg_find(atop(pa), &off);
#ifdef DIAGNOSTIC
				if (bank == -1)
					panic("pmap_enter: same pa PG_PVLIST "
					      "mapping with unmanaged page "
					      "pa = 0x%lx (0x%lx)", pa,
					      atop(pa));
#endif
				pvh = &vm_physmem[bank].pmseg.pvhead[off];
				simple_lock(&pvh->pvh_lock);
				vm_physmem[bank].pmseg.attrs[off] |= opte;
				simple_unlock(&pvh->pvh_lock);
			} else {
				pvh = NULL;	/* ensure !PG_PVLIST */
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
			bank = vm_physseg_find(atop(opte & PG_FRAME), &off);
#ifdef DIAGNOSTIC
			if (bank == -1)
				panic("pmap_enter: PG_PVLIST mapping with "
				      "unmanaged page "
				      "pa = 0x%lx (0x%lx)", pa, atop(pa));
#endif
			pvh = &vm_physmem[bank].pmseg.pvhead[off];
			simple_lock(&pvh->pvh_lock);
			pve = pmap_remove_pv(pvh, pmap, va);
			vm_physmem[bank].pmseg.attrs[off] |= opte;
			simple_unlock(&pvh->pvh_lock);
		} else {
			pve = NULL;
		}
	} else {	/* opte not valid */
		pve = NULL;
		pmap->pm_stats.resident_count++;
		if (wired)
			pmap->pm_stats.wired_count++;
		if (ptp)
			ptp->wire_count++;      /* count # of valid entrys */
	}

	/*
	 * at this point pm_stats has been updated.   pve is either NULL
	 * or points to a now-free pv_entry structure (the latter case is
	 * if we called pmap_remove_pv above).
	 *
	 * if this entry is to be on a pvlist, enter it now.
	 */

	bank = vm_physseg_find(atop(pa), &off);
	if (pmap_initialized && bank != -1) {
		pvh = &vm_physmem[bank].pmseg.pvhead[off];
		if (pve == NULL) {
			pve = pmap_alloc_pv(pmap, ALLOCPV_NEED);
			if (pve == NULL) {
				if (flags & PMAP_CANFAIL) {
					error = KERN_RESOURCE_SHORTAGE;
					goto out;
				}
				panic("pmap_enter: no pv entries available");
			}
		}
		/* lock pvh when adding */
		pmap_enter_pv(pvh, pve, pmap, va, ptp);
	} else {

		/* new mapping is not PG_PVLIST.   free pve if we've got one */
		pvh = NULL;		/* ensure !PG_PVLIST */
		if (pve)
			pmap_free_pv(pmap, pve);
	}

enter_now:
	/*
	 * at this point pvh is !NULL if we want the PG_PVLIST bit set
	 */

	npte = pa | protection_codes[prot] | PG_V;
	pmap_nxstack_account(pmap, va, opte, npte);
	if (pvh)
		npte |= PG_PVLIST;
	if (wired)
		npte |= PG_W;
	if (va < VM_MAXUSER_ADDRESS)
		npte |= PG_u;
	else if (va < VM_MAX_ADDRESS)
		npte |= (PG_u | PG_RW);	/* XXXCDC: no longer needed? */
	if (pmap == pmap_kernel())
		npte |= pmap_pg_g;

	ptes[i386_btop(va)] = npte;		/* zap! */

	if ((opte & ~(PG_M|PG_U)) != npte && pmap_is_curpmap(pmap))
		pmap_update_pg(va);

	error = 0;

out:
	pmap_unmap_ptes(pmap);
	PMAP_MAP_TO_HEAD_UNLOCK();

	return error;
}

/*
 * pmap_growkernel: increase usage of KVM space
 *
 * => we allocate new PTPs for the kernel and install them in all
 *	the pmaps on the system.
 */

vaddr_t
pmap_growkernel(maxkvaddr)
	vaddr_t maxkvaddr;
{
	struct pmap *kpm = pmap_kernel(), *pm;
	int needed_kpde;   /* needed number of kernel PTPs */
	int s;
	paddr_t ptaddr;

	needed_kpde = (int)(maxkvaddr - VM_MIN_KERNEL_ADDRESS + (NBPD-1))
		/ NBPD;
	if (needed_kpde <= nkpde)
		goto out;		/* we are OK */

	/*
	 * whoops!   we need to add kernel PTPs
	 */

	s = splhigh();	/* to be safe */
	simple_lock(&kpm->pm_obj.vmobjlock);

	for (/*null*/ ; nkpde < needed_kpde ; nkpde++) {

		if (uvm.page_init_done == FALSE) {

			/*
			 * we're growing the kernel pmap early (from
			 * uvm_pageboot_alloc()).  this case must be
			 * handled a little differently.
			 */

			if (uvm_page_physget(&ptaddr) == FALSE)
				panic("pmap_growkernel: out of memory");
			pmap_zero_phys(ptaddr);

			kpm->pm_pdir[PDSLOT_KERN + nkpde] =
				ptaddr | PG_RW | PG_V;

			/* count PTP as resident */
			kpm->pm_stats.resident_count++;
			continue;
		}

		/*
		 * THIS *MUST* BE CODED SO AS TO WORK IN THE
		 * pmap_initialized == FALSE CASE!  WE MAY BE
		 * INVOKED WHILE pmap_init() IS RUNNING!
		 */

		/*
		 * THIS *MUST* BE CODED SO AS TO WORK IN THE
		 * pmap_initialized == FALSE CASE!  WE MAY BE
		 * INVOKED WHILE pmap_init() IS RUNNING!
		 */

		if (pmap_alloc_ptp(kpm, PDSLOT_KERN + nkpde, FALSE) == NULL) {
			panic("pmap_growkernel: alloc ptp failed");
		}

		/* PG_u not for kernel */
		kpm->pm_pdir[PDSLOT_KERN + nkpde] &= ~PG_u;

		/* distribute new kernel PTP to all active pmaps */
		simple_lock(&pmaps_lock);
		for (pm = pmaps.lh_first; pm != NULL;
		     pm = pm->pm_list.le_next) {
			pm->pm_pdir[PDSLOT_KERN + nkpde] =
				kpm->pm_pdir[PDSLOT_KERN + nkpde];
		}
		simple_unlock(&pmaps_lock);
	}

	simple_unlock(&kpm->pm_obj.vmobjlock);
	splx(s);

out:
	return (VM_MIN_KERNEL_ADDRESS + (nkpde * NBPD));
}

#ifdef DEBUG
void pmap_dump(struct pmap *, vaddr_t, vaddr_t);

/*
 * pmap_dump: dump all the mappings from a pmap
 *
 * => caller should not be holding any pmap locks
 */

void
pmap_dump(pmap, sva, eva)
	struct pmap *pmap;
	vaddr_t sva, eva;
{
	pt_entry_t *ptes, *pte;
	vaddr_t blkendva;

	/*
	 * if end is out of range truncate.
	 * if (end == start) update to max.
	 */

	if (eva > VM_MAXUSER_ADDRESS || eva <= sva)
		eva = VM_MAXUSER_ADDRESS;

	/*
	 * we lock in the pmap => pv_head direction
	 */

	PMAP_MAP_TO_HEAD_LOCK();
	ptes = pmap_map_ptes(pmap);	/* locks pmap */

	/*
	 * dumping a range of pages: we dump in PTP sized blocks (4MB)
	 */

	for (/* null */ ; sva < eva ; sva = blkendva) {

		/* determine range of block */
		blkendva = i386_round_pdr(sva+1);
		if (blkendva > eva)
			blkendva = eva;

		/* valid block? */
		if (!pmap_valid_entry(pmap->pm_pdir[pdei(sva)]))
			continue;

		pte = &ptes[i386_btop(sva)];
		for (/* null */; sva < blkendva ; sva += NBPG, pte++) {
			if (!pmap_valid_entry(*pte))
				continue;
			printf("va %#lx -> pa %#x (pte=%#x)\n",
			       sva, *pte, *pte & PG_FRAME);
		}
	}
	pmap_unmap_ptes(pmap);
	PMAP_MAP_TO_HEAD_UNLOCK();
}
#endif
