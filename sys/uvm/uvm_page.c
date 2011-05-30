/*	$OpenBSD: uvm_page.c,v 1.108 2011/05/30 22:25:24 oga Exp $	*/
/*	$NetBSD: uvm_page.c,v 1.44 2000/11/27 08:40:04 chs Exp $	*/

/*
 * Copyright (c) 1997 Charles D. Cranor and Washington University.
 * Copyright (c) 1991, 1993, The Regents of the University of California.
 *
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * The Mach Operating System project at Carnegie-Mellon University.
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
 *	This product includes software developed by Charles D. Cranor,
 *      Washington University, the University of California, Berkeley and
 *      its contributors.
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
 *	@(#)vm_page.c   8.3 (Berkeley) 3/21/94
 * from: Id: uvm_page.c,v 1.1.2.18 1998/02/06 05:24:42 chs Exp
 *
 *
 * Copyright (c) 1987, 1990 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

/*
 * uvm_page.c: page ops.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sched.h>
#include <sys/kernel.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/proc.h>

#include <uvm/uvm.h>

/*
 * for object trees
 */
RB_GENERATE(uvm_objtree, vm_page, objt, uvm_pagecmp);

int
uvm_pagecmp(struct vm_page *a, struct vm_page *b)
{
	return (a->offset < b->offset ? -1 : a->offset > b->offset);
}

/*
 * global vars... XXXCDC: move to uvm. structure.
 */

/*
 * physical memory config is stored in vm_physmem.
 */

struct vm_physseg vm_physmem[VM_PHYSSEG_MAX];	/* XXXCDC: uvm.physmem */
int vm_nphysseg = 0;				/* XXXCDC: uvm.nphysseg */

/*
 * Some supported CPUs in a given architecture don't support all
 * of the things necessary to do idle page zero'ing efficiently.
 * We therefore provide a way to disable it from machdep code here.
 */

/*
 * XXX disabled until we can find a way to do this without causing
 * problems for either cpu caches or DMA latency.
 */
boolean_t vm_page_zero_enable = FALSE;

/*
 * local variables
 */

/*
 * these variables record the values returned by vm_page_bootstrap,
 * for debugging purposes.  The implementation of uvm_pageboot_alloc
 * and pmap_startup here also uses them internally.
 */

static vaddr_t      virtual_space_start;
static vaddr_t      virtual_space_end;

/*
 * History
 */
UVMHIST_DECL(pghist);

/*
 * local prototypes
 */

static void uvm_pageinsert(struct vm_page *);
static void uvm_pageremove(struct vm_page *);

/*
 * inline functions
 */

/*
 * uvm_pageinsert: insert a page in the object
 *
 * => caller must lock object
 * => caller must lock page queues XXX questionable
 * => call should have already set pg's object and offset pointers
 *    and bumped the version counter
 */

__inline static void
uvm_pageinsert(struct vm_page *pg)
{
	struct vm_page	*dupe;
	UVMHIST_FUNC("uvm_pageinsert"); UVMHIST_CALLED(pghist);

	KASSERT((pg->pg_flags & PG_TABLED) == 0);
	dupe = RB_INSERT(uvm_objtree, &pg->uobject->memt, pg);
	/* not allowed to insert over another page */
	KASSERT(dupe == NULL);
	atomic_setbits_int(&pg->pg_flags, PG_TABLED);
	pg->uobject->uo_npages++;
}

/*
 * uvm_page_remove: remove page from object
 *
 * => caller must lock object
 * => caller must lock page queues
 */

static __inline void
uvm_pageremove(struct vm_page *pg)
{
	UVMHIST_FUNC("uvm_pageremove"); UVMHIST_CALLED(pghist);

	KASSERT(pg->pg_flags & PG_TABLED);
	RB_REMOVE(uvm_objtree, &pg->uobject->memt, pg);

	atomic_clearbits_int(&pg->pg_flags, PG_TABLED);
	pg->uobject->uo_npages--;
	pg->uobject = NULL;
	pg->pg_version++;
}

/*
 * uvm_page_init: init the page system.   called from uvm_init().
 *
 * => we return the range of kernel virtual memory in kvm_startp/kvm_endp
 */

void
uvm_page_init(vaddr_t *kvm_startp, vaddr_t *kvm_endp)
{
	vsize_t freepages, pagecount, n;
	vm_page_t pagearray;
	int lcv, i;
	paddr_t paddr;
#if defined(UVMHIST)
	static struct uvm_history_ent pghistbuf[100];
#endif

	UVMHIST_FUNC("uvm_page_init");
	UVMHIST_INIT_STATIC(pghist, pghistbuf);
	UVMHIST_CALLED(pghist);

	/*
	 * init the page queues and page queue locks
	 */

	TAILQ_INIT(&uvm.page_active);
	TAILQ_INIT(&uvm.page_inactive_swp);
	TAILQ_INIT(&uvm.page_inactive_obj);
	simple_lock_init(&uvm.pageqlock);
	mtx_init(&uvm.fpageqlock, IPL_VM);
	uvm_pmr_init();

	/*
	 * allocate vm_page structures.
	 */

	/*
	 * sanity check:
	 * before calling this function the MD code is expected to register
	 * some free RAM with the uvm_page_physload() function.   our job
	 * now is to allocate vm_page structures for this memory.
	 */

	if (vm_nphysseg == 0)
		panic("uvm_page_bootstrap: no memory pre-allocated");

	/*
	 * first calculate the number of free pages...
	 *
	 * note that we use start/end rather than avail_start/avail_end.
	 * this allows us to allocate extra vm_page structures in case we
	 * want to return some memory to the pool after booting.
	 */

	freepages = 0;
	for (lcv = 0 ; lcv < vm_nphysseg ; lcv++)
		freepages += (vm_physmem[lcv].end - vm_physmem[lcv].start);

	/*
	 * we now know we have (PAGE_SIZE * freepages) bytes of memory we can
	 * use.   for each page of memory we use we need a vm_page structure.
	 * thus, the total number of pages we can use is the total size of
	 * the memory divided by the PAGE_SIZE plus the size of the vm_page
	 * structure.   we add one to freepages as a fudge factor to avoid
	 * truncation errors (since we can only allocate in terms of whole
	 * pages).
	 */

	pagecount = (((paddr_t)freepages + 1) << PAGE_SHIFT) /
	    (PAGE_SIZE + sizeof(struct vm_page));
	pagearray = (vm_page_t)uvm_pageboot_alloc(pagecount *
	    sizeof(struct vm_page));
	memset(pagearray, 0, pagecount * sizeof(struct vm_page));

	/*
	 * init the vm_page structures and put them in the correct place.
	 */

	for (lcv = 0 ; lcv < vm_nphysseg ; lcv++) {
		n = vm_physmem[lcv].end - vm_physmem[lcv].start;
		if (n > pagecount) {
			panic("uvm_page_init: lost %ld page(s) in init",
			    (long)(n - pagecount));
			    /* XXXCDC: shouldn't happen? */
			/* n = pagecount; */
		}

		/* set up page array pointers */
		vm_physmem[lcv].pgs = pagearray;
		pagearray += n;
		pagecount -= n;
		vm_physmem[lcv].lastpg = vm_physmem[lcv].pgs + (n - 1);

		/* init and free vm_pages (we've already zeroed them) */
		paddr = ptoa(vm_physmem[lcv].start);
		for (i = 0 ; i < n ; i++, paddr += PAGE_SIZE) {
			vm_physmem[lcv].pgs[i].phys_addr = paddr;
#ifdef __HAVE_VM_PAGE_MD
			VM_MDPAGE_INIT(&vm_physmem[lcv].pgs[i]);
#endif
			if (atop(paddr) >= vm_physmem[lcv].avail_start &&
			    atop(paddr) <= vm_physmem[lcv].avail_end) {
				uvmexp.npages++;
			}
		}

		/*
		 * Add pages to free pool.
		 */
		uvm_pmr_freepages(&vm_physmem[lcv].pgs[
		    vm_physmem[lcv].avail_start - vm_physmem[lcv].start],
		    vm_physmem[lcv].avail_end - vm_physmem[lcv].avail_start);
	}

	/*
	 * pass up the values of virtual_space_start and
	 * virtual_space_end (obtained by uvm_pageboot_alloc) to the upper
	 * layers of the VM.
	 */

	*kvm_startp = round_page(virtual_space_start);
	*kvm_endp = trunc_page(virtual_space_end);

	/*
	 * init locks for kernel threads
	 */
	mtx_init(&uvm.aiodoned_lock, IPL_BIO);

	/*
	 * init reserve thresholds
	 * XXXCDC - values may need adjusting
	 */
	uvmexp.reserve_pagedaemon = 4;
	uvmexp.reserve_kernel = 6;
	uvmexp.anonminpct = 10;
	uvmexp.vnodeminpct = 10;
	uvmexp.vtextminpct = 5;
	uvmexp.anonmin = uvmexp.anonminpct * 256 / 100;
	uvmexp.vnodemin = uvmexp.vnodeminpct * 256 / 100;
	uvmexp.vtextmin = uvmexp.vtextminpct * 256 / 100;

  	/*
	 * determine if we should zero pages in the idle loop.
	 */

	uvm.page_idle_zero = vm_page_zero_enable;

	/*
	 * done!
	 */

	uvm.page_init_done = TRUE;
}

/*
 * uvm_setpagesize: set the page size
 *
 * => sets page_shift and page_mask from uvmexp.pagesize.
 */

void
uvm_setpagesize(void)
{
	if (uvmexp.pagesize == 0)
		uvmexp.pagesize = DEFAULT_PAGE_SIZE;
	uvmexp.pagemask = uvmexp.pagesize - 1;
	if ((uvmexp.pagemask & uvmexp.pagesize) != 0)
		panic("uvm_setpagesize: page size not a power of two");
	for (uvmexp.pageshift = 0; ; uvmexp.pageshift++)
		if ((1 << uvmexp.pageshift) == uvmexp.pagesize)
			break;
}

/*
 * uvm_pageboot_alloc: steal memory from physmem for bootstrapping
 */

vaddr_t
uvm_pageboot_alloc(vsize_t size)
{
#if defined(PMAP_STEAL_MEMORY)
	vaddr_t addr;

	/*
	 * defer bootstrap allocation to MD code (it may want to allocate
	 * from a direct-mapped segment).  pmap_steal_memory should round
	 * off virtual_space_start/virtual_space_end.
	 */

	addr = pmap_steal_memory(size, &virtual_space_start,
	    &virtual_space_end);

	return(addr);

#else /* !PMAP_STEAL_MEMORY */

	static boolean_t initialized = FALSE;
	vaddr_t addr, vaddr;
	paddr_t paddr;

	/* round to page size */
	size = round_page(size);

	/*
	 * on first call to this function, initialize ourselves.
	 */
	if (initialized == FALSE) {
		pmap_virtual_space(&virtual_space_start, &virtual_space_end);

		/* round it the way we like it */
		virtual_space_start = round_page(virtual_space_start);
		virtual_space_end = trunc_page(virtual_space_end);

		initialized = TRUE;
	}

	/*
	 * allocate virtual memory for this request
	 */
	if (virtual_space_start == virtual_space_end ||
	    (virtual_space_end - virtual_space_start) < size)
		panic("uvm_pageboot_alloc: out of virtual space");

	addr = virtual_space_start;

#ifdef PMAP_GROWKERNEL
	/*
	 * If the kernel pmap can't map the requested space,
	 * then allocate more resources for it.
	 */
	if (uvm_maxkaddr < (addr + size)) {
		uvm_maxkaddr = pmap_growkernel(addr + size);
		if (uvm_maxkaddr < (addr + size))
			panic("uvm_pageboot_alloc: pmap_growkernel() failed");
	}
#endif

	virtual_space_start += size;

	/*
	 * allocate and mapin physical pages to back new virtual pages
	 */

	for (vaddr = round_page(addr) ; vaddr < addr + size ;
	    vaddr += PAGE_SIZE) {

		if (!uvm_page_physget(&paddr))
			panic("uvm_pageboot_alloc: out of memory");

		/*
		 * Note this memory is no longer managed, so using
		 * pmap_kenter is safe.
		 */
		pmap_kenter_pa(vaddr, paddr, VM_PROT_READ|VM_PROT_WRITE);
	}
	pmap_update(pmap_kernel());
	return(addr);
#endif	/* PMAP_STEAL_MEMORY */
}

#if !defined(PMAP_STEAL_MEMORY)
/*
 * uvm_page_physget: "steal" one page from the vm_physmem structure.
 *
 * => attempt to allocate it off the end of a segment in which the "avail"
 *    values match the start/end values.   if we can't do that, then we
 *    will advance both values (making them equal, and removing some
 *    vm_page structures from the non-avail area).
 * => return false if out of memory.
 */

boolean_t
uvm_page_physget(paddr_t *paddrp)
{
	int lcv, x;
	UVMHIST_FUNC("uvm_page_physget"); UVMHIST_CALLED(pghist);

	/* pass 1: try allocating from a matching end */
#if (VM_PHYSSEG_STRAT == VM_PSTRAT_BIGFIRST) || \
	(VM_PHYSSEG_STRAT == VM_PSTRAT_BSEARCH)
	for (lcv = vm_nphysseg - 1 ; lcv >= 0 ; lcv--)
#else
	for (lcv = 0 ; lcv < vm_nphysseg ; lcv++)
#endif
	{

		if (uvm.page_init_done == TRUE)
			panic("uvm_page_physget: called _after_ bootstrap");

		/* try from front */
		if (vm_physmem[lcv].avail_start == vm_physmem[lcv].start &&
		    vm_physmem[lcv].avail_start < vm_physmem[lcv].avail_end) {
			*paddrp = ptoa(vm_physmem[lcv].avail_start);
			vm_physmem[lcv].avail_start++;
			vm_physmem[lcv].start++;
			/* nothing left?   nuke it */
			if (vm_physmem[lcv].avail_start ==
			    vm_physmem[lcv].end) {
				if (vm_nphysseg == 1)
				    panic("uvm_page_physget: out of memory!");
				vm_nphysseg--;
				for (x = lcv ; x < vm_nphysseg ; x++)
					/* structure copy */
					vm_physmem[x] = vm_physmem[x+1];
			}
			return (TRUE);
		}

		/* try from rear */
		if (vm_physmem[lcv].avail_end == vm_physmem[lcv].end &&
		    vm_physmem[lcv].avail_start < vm_physmem[lcv].avail_end) {
			*paddrp = ptoa(vm_physmem[lcv].avail_end - 1);
			vm_physmem[lcv].avail_end--;
			vm_physmem[lcv].end--;
			/* nothing left?   nuke it */
			if (vm_physmem[lcv].avail_end ==
			    vm_physmem[lcv].start) {
				if (vm_nphysseg == 1)
				    panic("uvm_page_physget: out of memory!");
				vm_nphysseg--;
				for (x = lcv ; x < vm_nphysseg ; x++)
					/* structure copy */
					vm_physmem[x] = vm_physmem[x+1];
			}
			return (TRUE);
		}
	}

	/* pass2: forget about matching ends, just allocate something */
#if (VM_PHYSSEG_STRAT == VM_PSTRAT_BIGFIRST) || \
	(VM_PHYSSEG_STRAT == VM_PSTRAT_BSEARCH)
	for (lcv = vm_nphysseg - 1 ; lcv >= 0 ; lcv--)
#else
	for (lcv = 0 ; lcv < vm_nphysseg ; lcv++)
#endif
	{

		/* any room in this bank? */
		if (vm_physmem[lcv].avail_start >= vm_physmem[lcv].avail_end)
			continue;  /* nope */

		*paddrp = ptoa(vm_physmem[lcv].avail_start);
		vm_physmem[lcv].avail_start++;
		/* truncate! */
		vm_physmem[lcv].start = vm_physmem[lcv].avail_start;

		/* nothing left?   nuke it */
		if (vm_physmem[lcv].avail_start == vm_physmem[lcv].end) {
			if (vm_nphysseg == 1)
				panic("uvm_page_physget: out of memory!");
			vm_nphysseg--;
			for (x = lcv ; x < vm_nphysseg ; x++)
				/* structure copy */
				vm_physmem[x] = vm_physmem[x+1];
		}
		return (TRUE);
	}

	return (FALSE);        /* whoops! */
}

#endif /* PMAP_STEAL_MEMORY */

/*
 * uvm_page_physload: load physical memory into VM system
 *
 * => all args are PFs
 * => all pages in start/end get vm_page structures
 * => areas marked by avail_start/avail_end get added to the free page pool
 * => we are limited to VM_PHYSSEG_MAX physical memory segments
 */

void
uvm_page_physload(paddr_t start, paddr_t end, paddr_t avail_start,
    paddr_t avail_end, int flags)
{
	int preload, lcv;
	psize_t npages;
	struct vm_page *pgs;
	struct vm_physseg *ps;

	if (uvmexp.pagesize == 0)
		panic("uvm_page_physload: page size not set!");

	if (start >= end)
		panic("uvm_page_physload: start >= end");

	/*
	 * do we have room?
	 */
	if (vm_nphysseg == VM_PHYSSEG_MAX) {
		printf("uvm_page_physload: unable to load physical memory "
		    "segment\n");
		printf("\t%d segments allocated, ignoring 0x%llx -> 0x%llx\n",
		    VM_PHYSSEG_MAX, (long long)start, (long long)end);
		printf("\tincrease VM_PHYSSEG_MAX\n");
		return;
	}

	/*
	 * check to see if this is a "preload" (i.e. uvm_mem_init hasn't been
	 * called yet, so malloc is not available).
	 */
	for (lcv = 0 ; lcv < vm_nphysseg ; lcv++) {
		if (vm_physmem[lcv].pgs)
			break;
	}
	preload = (lcv == vm_nphysseg);

	/*
	 * if VM is already running, attempt to malloc() vm_page structures
	 */
	if (!preload) {
		/*
		 * XXXCDC: need some sort of lockout for this case
		 * right now it is only used by devices so it should be alright.
		 */
 		paddr_t paddr;

 		npages = end - start;  /* # of pages */

		pgs = (struct vm_page *)uvm_km_zalloc(kernel_map,
		    npages * sizeof(*pgs));
		if (pgs == NULL) {
			printf("uvm_page_physload: can not malloc vm_page "
			    "structs for segment\n");
			printf("\tignoring 0x%lx -> 0x%lx\n", start, end);
			return;
		}
		/* init phys_addr and free pages, XXX uvmexp.npages */
		for (lcv = 0, paddr = ptoa(start); lcv < npages;
		    lcv++, paddr += PAGE_SIZE) {
			pgs[lcv].phys_addr = paddr;
#ifdef __HAVE_VM_PAGE_MD
			VM_MDPAGE_INIT(&pgs[lcv]);
#endif
			if (atop(paddr) >= avail_start &&
			    atop(paddr) <= avail_end) {
				if (flags & PHYSLOAD_DEVICE) {
					atomic_setbits_int(&pgs[lcv].pg_flags,
					    PG_DEV);
					pgs[lcv].wire_count = 1;
				} else {
#if defined(VM_PHYSSEG_NOADD)
		panic("uvm_page_physload: tried to add RAM after vm_mem_init");
#endif
				}
			}
		}

		/*
		 * Add pages to free pool.
		 */
		if ((flags & PHYSLOAD_DEVICE) == 0) {
			uvm_pmr_freepages(&pgs[avail_start - start],
			    avail_end - avail_start);
		}

		/* XXXCDC: need hook to tell pmap to rebuild pv_list, etc... */
	} else {

		/* gcc complains if these don't get init'd */
		pgs = NULL;
		npages = 0;

	}

	/*
	 * now insert us in the proper place in vm_physmem[]
	 */

#if (VM_PHYSSEG_STRAT == VM_PSTRAT_RANDOM)

	/* random: put it at the end (easy!) */
	ps = &vm_physmem[vm_nphysseg];

#elif (VM_PHYSSEG_STRAT == VM_PSTRAT_BSEARCH)

	{
		int x;
		/* sort by address for binary search */
		for (lcv = 0 ; lcv < vm_nphysseg ; lcv++)
			if (start < vm_physmem[lcv].start)
				break;
		ps = &vm_physmem[lcv];
		/* move back other entries, if necessary ... */
		for (x = vm_nphysseg ; x > lcv ; x--)
			/* structure copy */
			vm_physmem[x] = vm_physmem[x - 1];
	}

#elif (VM_PHYSSEG_STRAT == VM_PSTRAT_BIGFIRST)

	{
		int x;
		/* sort by largest segment first */
		for (lcv = 0 ; lcv < vm_nphysseg ; lcv++)
			if ((end - start) >
			    (vm_physmem[lcv].end - vm_physmem[lcv].start))
				break;
		ps = &vm_physmem[lcv];
		/* move back other entries, if necessary ... */
		for (x = vm_nphysseg ; x > lcv ; x--)
			/* structure copy */
			vm_physmem[x] = vm_physmem[x - 1];
	}

#else

	panic("uvm_page_physload: unknown physseg strategy selected!");

#endif

	ps->start = start;
	ps->end = end;
	ps->avail_start = avail_start;
	ps->avail_end = avail_end;
	if (preload) {
		ps->pgs = NULL;
	} else {
		ps->pgs = pgs;
		ps->lastpg = pgs + npages - 1;
	}
	vm_nphysseg++;

	/*
	 * done!
	 */

	return;
}

#ifdef DDB /* XXXCDC: TMP TMP TMP DEBUG DEBUG DEBUG */

void uvm_page_physdump(void); /* SHUT UP GCC */

/* call from DDB */
void
uvm_page_physdump(void)
{
	int lcv;

	printf("uvm_page_physdump: physical memory config [segs=%d of %d]:\n",
	    vm_nphysseg, VM_PHYSSEG_MAX);
	for (lcv = 0 ; lcv < vm_nphysseg ; lcv++)
		printf("0x%llx->0x%llx [0x%llx->0x%llx]\n",
		    (long long)vm_physmem[lcv].start,
		    (long long)vm_physmem[lcv].end,
		    (long long)vm_physmem[lcv].avail_start,
		    (long long)vm_physmem[lcv].avail_end);
	printf("STRATEGY = ");
	switch (VM_PHYSSEG_STRAT) {
	case VM_PSTRAT_RANDOM: printf("RANDOM\n"); break;
	case VM_PSTRAT_BSEARCH: printf("BSEARCH\n"); break;
	case VM_PSTRAT_BIGFIRST: printf("BIGFIRST\n"); break;
	default: printf("<<UNKNOWN>>!!!!\n");
	}
}
#endif

void
uvm_shutdown(void)
{
#ifdef UVM_SWAP_ENCRYPT
	uvm_swap_finicrypt_all();
#endif
}

/*
 * Perform insert of a given page in the specified anon of obj.
 * This is basically, uvm_pagealloc, but with the page already given.
 */
void
uvm_pagealloc_pg(struct vm_page *pg, struct uvm_object *obj, voff_t off,
    struct vm_anon *anon)
{
	int	flags;

	flags = PG_BUSY | PG_FAKE;
	pg->offset = off;
	pg->uobject = obj;
	pg->uanon = anon;

	if (anon) {
		anon->an_page = pg;
		flags |= PQ_ANON;
	} else if (obj)
		uvm_pageinsert(pg);
	atomic_setbits_int(&pg->pg_flags, flags);
#if defined(UVM_PAGE_TRKOWN)
	pg->owner_tag = NULL;
#endif
	UVM_PAGE_OWN(pg, "new alloc");
}

/*
 * interface used by the buffer cache to allocate a buffer at a time.
 * The pages are allocated wired in DMA accessible memory
 */
void
uvm_pagealloc_multi(struct uvm_object *obj, voff_t off, vsize_t size, int flags)
{
	struct pglist    plist;
	struct vm_page  *pg;
	int              i;


	TAILQ_INIT(&plist);
	(void) uvm_pglistalloc(size, dma_constraint.ucr_low,
	    dma_constraint.ucr_high, 0, 0, &plist, atop(round_page(size)),
	    UVM_PLA_WAITOK);
	i = 0;
	while ((pg = TAILQ_FIRST(&plist)) != NULL) {
		pg->wire_count = 1;
		atomic_setbits_int(&pg->pg_flags, PG_CLEAN | PG_FAKE);
		KASSERT((pg->pg_flags & PG_DEV) == 0);
		TAILQ_REMOVE(&plist, pg, pageq);
		uvm_pagealloc_pg(pg, obj, off + ptoa(i++), NULL);
	}
}

/*
 * uvm_pagealloc_strat: allocate vm_page from a particular free list.
 *
 * => return null if no pages free
 * => wake up pagedaemon if number of free pages drops below low water mark
 * => if obj != NULL, obj must be locked (to put in tree)
 * => if anon != NULL, anon must be locked (to put in anon)
 * => only one of obj or anon can be non-null
 * => caller must activate/deactivate page if it is not wired.
 */

struct vm_page *
uvm_pagealloc(struct uvm_object *obj, voff_t off, struct vm_anon *anon,
    int flags)
{
	struct vm_page *pg;
	struct pglist pgl;
	int pmr_flags;
	boolean_t use_reserve;
	UVMHIST_FUNC("uvm_pagealloc"); UVMHIST_CALLED(pghist);

	KASSERT(obj == NULL || anon == NULL);
	KASSERT(off == trunc_page(off));

	/*
	 * check to see if we need to generate some free pages waking
	 * the pagedaemon.
	 */
	if ((uvmexp.free - BUFPAGES_DEFICIT) < uvmexp.freemin ||
	    ((uvmexp.free - BUFPAGES_DEFICIT) < uvmexp.freetarg &&
	    (uvmexp.inactive + BUFPAGES_INACT) < uvmexp.inactarg))
		wakeup(&uvm.pagedaemon);

	/*
	 * fail if any of these conditions is true:
	 * [1]  there really are no free pages, or
	 * [2]  only kernel "reserved" pages remain and
	 *        the page isn't being allocated to a kernel object.
	 * [3]  only pagedaemon "reserved" pages remain and
	 *        the requestor isn't the pagedaemon.
	 */

	use_reserve = (flags & UVM_PGA_USERESERVE) ||
		(obj && UVM_OBJ_IS_KERN_OBJECT(obj));
	if ((uvmexp.free <= uvmexp.reserve_kernel && !use_reserve) ||
	    (uvmexp.free <= uvmexp.reserve_pagedaemon &&
	     !((curproc == uvm.pagedaemon_proc) ||
	      (curproc == syncerproc))))
		goto fail;

	pmr_flags = UVM_PLA_NOWAIT;
	if (flags & UVM_PGA_ZERO)
		pmr_flags |= UVM_PLA_ZERO;
	TAILQ_INIT(&pgl);
	if (uvm_pmr_getpages(1, 0, 0, 1, 0, 1, pmr_flags, &pgl) != 0)
		goto fail;

	pg = TAILQ_FIRST(&pgl);
	KASSERT(pg != NULL && TAILQ_NEXT(pg, pageq) == NULL);

	uvm_pagealloc_pg(pg, obj, off, anon);
	KASSERT((pg->pg_flags & PG_DEV) == 0);
	atomic_setbits_int(&pg->pg_flags, PG_BUSY|PG_CLEAN|PG_FAKE);
	if (flags & UVM_PGA_ZERO)
		atomic_clearbits_int(&pg->pg_flags, PG_CLEAN);

	UVMHIST_LOG(pghist, "allocated pg %p/%lx", pg,
	    (u_long)VM_PAGE_TO_PHYS(pg), 0, 0);
	return(pg);

 fail:
	UVMHIST_LOG(pghist, "failed!", 0, 0, 0, 0);
	return (NULL);
}

/*
 * uvm_pagerealloc: reallocate a page from one object to another
 *
 * => both objects must be locked
 */

void
uvm_pagerealloc(struct vm_page *pg, struct uvm_object *newobj, voff_t newoff)
{

	UVMHIST_FUNC("uvm_pagerealloc"); UVMHIST_CALLED(pghist);

	/*
	 * remove it from the old object
	 */

	if (pg->uobject) {
		uvm_pageremove(pg);
	}

	/*
	 * put it in the new object
	 */

	if (newobj) {
		pg->uobject = newobj;
		pg->offset = newoff;
		pg->pg_version++;
		uvm_pageinsert(pg);
	}
}


/*
 * uvm_pagefree: free page
 *
 * => erase page's identity (i.e. remove from object)
 * => put page on free list
 * => caller must lock owning object (either anon or uvm_object)
 * => caller must lock page queues
 * => assumes all valid mappings of pg are gone
 */

void
uvm_pagefree(struct vm_page *pg)
{
	int saved_loan_count = pg->loan_count;
	UVMHIST_FUNC("uvm_pagefree"); UVMHIST_CALLED(pghist);

#ifdef DEBUG
	if (pg->uobject == (void *)0xdeadbeef &&
	    pg->uanon == (void *)0xdeadbeef) {
		panic("uvm_pagefree: freeing free page %p", pg);
	}
#endif

	UVMHIST_LOG(pghist, "freeing pg %p/%lx", pg,
	    (u_long)VM_PAGE_TO_PHYS(pg), 0, 0);
	KASSERT((pg->pg_flags & PG_DEV) == 0);

	/*
	 * if the page was an object page (and thus "TABLED"), remove it
	 * from the object.
	 */

	if (pg->pg_flags & PG_TABLED) {

		/*
		 * if the object page is on loan we are going to drop ownership.
		 * it is possible that an anon will take over as owner for this
		 * page later on.   the anon will want a !PG_CLEAN page so that
		 * it knows it needs to allocate swap if it wants to page the
		 * page out.
		 */

		/* in case an anon takes over */
		if (saved_loan_count)
			atomic_clearbits_int(&pg->pg_flags, PG_CLEAN);
		uvm_pageremove(pg);

		/*
		 * if our page was on loan, then we just lost control over it
		 * (in fact, if it was loaned to an anon, the anon may have
		 * already taken over ownership of the page by now and thus
		 * changed the loan_count [e.g. in uvmfault_anonget()]) we just
		 * return (when the last loan is dropped, then the page can be
		 * freed by whatever was holding the last loan).
		 */

		if (saved_loan_count)
			return;
	} else if (saved_loan_count && pg->uanon) {
		/*
		 * if our page is owned by an anon and is loaned out to the
		 * kernel then we just want to drop ownership and return.
		 * the kernel must free the page when all its loans clear ...
		 * note that the kernel can't change the loan status of our
		 * page as long as we are holding PQ lock.
		 */
		atomic_clearbits_int(&pg->pg_flags, PQ_ANON);
		pg->uanon->an_page = NULL;
		pg->uanon = NULL;
		return;
	}
	KASSERT(saved_loan_count == 0);

	/*
	 * now remove the page from the queues
	 */

	if (pg->pg_flags & PQ_ACTIVE) {
		TAILQ_REMOVE(&uvm.page_active, pg, pageq);
		atomic_clearbits_int(&pg->pg_flags, PQ_ACTIVE);
		uvmexp.active--;
	}
	if (pg->pg_flags & PQ_INACTIVE) {
		if (pg->pg_flags & PQ_SWAPBACKED)
			TAILQ_REMOVE(&uvm.page_inactive_swp, pg, pageq);
		else
			TAILQ_REMOVE(&uvm.page_inactive_obj, pg, pageq);
		atomic_clearbits_int(&pg->pg_flags, PQ_INACTIVE);
		uvmexp.inactive--;
	}

	/*
	 * if the page was wired, unwire it now.
	 */

	if (pg->wire_count) {
		pg->wire_count = 0;
		uvmexp.wired--;
	}
	if (pg->uanon) {
		pg->uanon->an_page = NULL;
		pg->uanon = NULL;
		atomic_clearbits_int(&pg->pg_flags, PQ_ANON);
	}

	/*
	 * Clean page state bits.
	 */
	atomic_clearbits_int(&pg->pg_flags, PQ_AOBJ); /* XXX: find culprit */
	atomic_clearbits_int(&pg->pg_flags, PQ_ENCRYPT|
	    PG_ZERO|PG_FAKE|PG_BUSY|PG_RELEASED|PG_CLEAN|PG_CLEANCHK);

	/*
	 * and put on free queue
	 */

#ifdef DEBUG
	pg->uobject = (void *)0xdeadbeef;
	pg->offset = 0xdeadbeef;
	pg->uanon = (void *)0xdeadbeef;
#endif

	uvm_pmr_freepages(pg, 1);

	if (uvmexp.zeropages < UVM_PAGEZERO_TARGET)
		uvm.page_idle_zero = vm_page_zero_enable;
}

/*
 * uvm_page_unbusy: unbusy an array of pages.
 *
 * => pages must either all belong to the same object, or all belong to anons.
 * => if pages are object-owned, object must be locked.
 * => if pages are anon-owned, anons must be unlockd and have 0 refcount.
 */

void
uvm_page_unbusy(struct vm_page **pgs, int npgs)
{
	struct vm_page *pg;
	struct uvm_object *uobj;
	int i;
	UVMHIST_FUNC("uvm_page_unbusy"); UVMHIST_CALLED(pdhist);

	for (i = 0; i < npgs; i++) {
		pg = pgs[i];

		if (pg == NULL || pg == PGO_DONTCARE) {
			continue;
		}
		if (pg->pg_flags & PG_WANTED) {
			wakeup(pg);
		}
		if (pg->pg_flags & PG_RELEASED) {
			UVMHIST_LOG(pdhist, "releasing pg %p", pg,0,0,0);
			uobj = pg->uobject;
			if (uobj != NULL) {
				uvm_lock_pageq();
				pmap_page_protect(pg, VM_PROT_NONE);
				/* XXX won't happen right now */
				if (pg->pg_flags & PQ_ANON)
					uao_dropswap(uobj,
					    pg->offset >> PAGE_SHIFT);
				uvm_pagefree(pg);
				uvm_unlock_pageq();
			} else {
				atomic_clearbits_int(&pg->pg_flags, PG_BUSY);
				UVM_PAGE_OWN(pg, NULL);
				uvm_anfree(pg->uanon);
			}
		} else {
			UVMHIST_LOG(pdhist, "unbusying pg %p", pg,0,0,0);
			atomic_clearbits_int(&pg->pg_flags, PG_WANTED|PG_BUSY);
			UVM_PAGE_OWN(pg, NULL);
		}
	}
}

#if defined(UVM_PAGE_TRKOWN)
/*
 * uvm_page_own: set or release page ownership
 *
 * => this is a debugging function that keeps track of who sets PG_BUSY
 *	and where they do it.   it can be used to track down problems
 *	such a process setting "PG_BUSY" and never releasing it.
 * => page's object [if any] must be locked
 * => if "tag" is NULL then we are releasing page ownership
 */
void
uvm_page_own(struct vm_page *pg, char *tag)
{
	/* gain ownership? */
	if (tag) {
		if (pg->owner_tag) {
			printf("uvm_page_own: page %p already owned "
			    "by proc %d [%s]\n", pg,
			     pg->owner, pg->owner_tag);
			panic("uvm_page_own");
		}
		pg->owner = (curproc) ? curproc->p_pid :  (pid_t) -1;
		pg->owner_tag = tag;
		return;
	}

	/* drop ownership */
	if (pg->owner_tag == NULL) {
		printf("uvm_page_own: dropping ownership of an non-owned "
		    "page (%p)\n", pg);
		panic("uvm_page_own");
	}
	pg->owner_tag = NULL;
	return;
}
#endif

/*
 * uvm_pageidlezero: zero free pages while the system is idle.
 *
 * => we do at least one iteration per call, if we are below the target.
 * => we loop until we either reach the target or whichqs indicates that
 *	there is a process ready to run.
 */
void
uvm_pageidlezero(void)
{
#if 0 /* disabled: need new code */
	struct vm_page *pg;
	struct pgfreelist *pgfl;
	int free_list;
	UVMHIST_FUNC("uvm_pageidlezero"); UVMHIST_CALLED(pghist);

	do {
		uvm_lock_fpageq();

		if (uvmexp.zeropages >= UVM_PAGEZERO_TARGET) {
			uvm.page_idle_zero = FALSE;
			uvm_unlock_fpageq();
			return;
		}

		for (free_list = 0; free_list < VM_NFREELIST; free_list++) {
			pgfl = &uvm.page_free[free_list];
			if ((pg = TAILQ_FIRST(&pgfl->pgfl_queues[
			    PGFL_UNKNOWN])) != NULL)
				break;
		}

		if (pg == NULL) {
			/*
			 * No non-zero'd pages; don't bother trying again
			 * until we know we have non-zero'd pages free.
			 */
			uvm.page_idle_zero = FALSE;
			uvm_unlock_fpageq();
			return;
		}

		TAILQ_REMOVE(&pgfl->pgfl_queues[PGFL_UNKNOWN], pg, pageq);
		uvmexp.free--;
		uvm_unlock_fpageq();

#ifdef PMAP_PAGEIDLEZERO
		if (PMAP_PAGEIDLEZERO(pg) == FALSE) {
			/*
			 * The machine-dependent code detected some
			 * reason for us to abort zeroing pages,
			 * probably because there is a process now
			 * ready to run.
			 */
			uvm_lock_fpageq();
			TAILQ_INSERT_HEAD(&pgfl->pgfl_queues[PGFL_UNKNOWN],
			    pg, pageq);
			uvmexp.free++;
			uvmexp.zeroaborts++;
			uvm_unlock_fpageq();
			return;
		}
#else
		/*
		 * XXX This will toast the cache unless the pmap_zero_page()
		 * XXX implementation does uncached access.
		 */
		pmap_zero_page(pg);
#endif
		atomic_setbits_int(&pg->pg_flags, PG_ZERO);

		uvm_lock_fpageq();
		TAILQ_INSERT_HEAD(&pgfl->pgfl_queues[PGFL_ZEROS], pg, pageq);
		uvmexp.free++;
		uvmexp.zeropages++;
		uvm_unlock_fpageq();
	} while (curcpu_is_idle());
#endif /* 0 */
}

/*
 * when VM_PHYSSEG_MAX is 1, we can simplify these functions
 */

#if VM_PHYSSEG_MAX > 1
/*
 * vm_physseg_find: find vm_physseg structure that belongs to a PA
 */
int
vm_physseg_find(paddr_t pframe, int *offp)
{

#if (VM_PHYSSEG_STRAT == VM_PSTRAT_BSEARCH)
	/* binary search for it */
	int	start, len, try;

	/*
	 * if try is too large (thus target is less than than try) we reduce
	 * the length to trunc(len/2) [i.e. everything smaller than "try"]
	 *
	 * if the try is too small (thus target is greater than try) then
	 * we set the new start to be (try + 1).   this means we need to
	 * reduce the length to (round(len/2) - 1).
	 *
	 * note "adjust" below which takes advantage of the fact that
	 *  (round(len/2) - 1) == trunc((len - 1) / 2)
	 * for any value of len we may have
	 */

	for (start = 0, len = vm_nphysseg ; len != 0 ; len = len / 2) {
		try = start + (len / 2);	/* try in the middle */

		/* start past our try? */
		if (pframe >= vm_physmem[try].start) {
			/* was try correct? */
			if (pframe < vm_physmem[try].end) {
				if (offp)
					*offp = pframe - vm_physmem[try].start;
				return(try);            /* got it */
			}
			start = try + 1;	/* next time, start here */
			len--;			/* "adjust" */
		} else {
			/*
			 * pframe before try, just reduce length of
			 * region, done in "for" loop
			 */
		}
	}
	return(-1);

#else
	/* linear search for it */
	int	lcv;

	for (lcv = 0; lcv < vm_nphysseg; lcv++) {
		if (pframe >= vm_physmem[lcv].start &&
		    pframe < vm_physmem[lcv].end) {
			if (offp)
				*offp = pframe - vm_physmem[lcv].start;
			return(lcv);		   /* got it */
		}
	}
	return(-1);

#endif
}

/*
 * PHYS_TO_VM_PAGE: find vm_page for a PA.   used by MI code to get vm_pages
 * back from an I/O mapping (ugh!).   used in some MD code as well.
 */
struct vm_page *
PHYS_TO_VM_PAGE(paddr_t pa)
{
	paddr_t pf = atop(pa);
	int	off;
	int	psi;

	psi = vm_physseg_find(pf, &off);

	return ((psi == -1) ? NULL : &vm_physmem[psi].pgs[off]);
}
#endif /* VM_PHYSSEG_MAX > 1 */

/*
 * uvm_pagelookup: look up a page
 *
 * => caller should lock object to keep someone from pulling the page
 *	out from under it
 */
struct vm_page *
uvm_pagelookup(struct uvm_object *obj, voff_t off)
{
	/* XXX if stack is too much, handroll */
	struct vm_page pg;

	pg.offset = off;
	return (RB_FIND(uvm_objtree, &obj->memt, &pg));
}

/*
 * uvm_pagewire: wire the page, thus removing it from the daemon's grasp
 *
 * => caller must lock page queues
 */
void
uvm_pagewire(struct vm_page *pg)
{
	if (pg->wire_count == 0) {
		if (pg->pg_flags & PQ_ACTIVE) {
			TAILQ_REMOVE(&uvm.page_active, pg, pageq);
			atomic_clearbits_int(&pg->pg_flags, PQ_ACTIVE);
			uvmexp.active--;
		}
		if (pg->pg_flags & PQ_INACTIVE) {
			if (pg->pg_flags & PQ_SWAPBACKED)
				TAILQ_REMOVE(&uvm.page_inactive_swp, pg, pageq);
			else
				TAILQ_REMOVE(&uvm.page_inactive_obj, pg, pageq);
			atomic_clearbits_int(&pg->pg_flags, PQ_INACTIVE);
			uvmexp.inactive--;
		}
		uvmexp.wired++;
	}
	pg->wire_count++;
}

/*
 * uvm_pageunwire: unwire the page.
 *
 * => activate if wire count goes to zero.
 * => caller must lock page queues
 */
void
uvm_pageunwire(struct vm_page *pg)
{
	pg->wire_count--;
	if (pg->wire_count == 0) {
		TAILQ_INSERT_TAIL(&uvm.page_active, pg, pageq);
		uvmexp.active++;
		atomic_setbits_int(&pg->pg_flags, PQ_ACTIVE);
		uvmexp.wired--;
	}
}

/*
 * uvm_pagedeactivate: deactivate page -- no pmaps have access to page
 *
 * => caller must lock page queues
 * => caller must check to make sure page is not wired
 * => object that page belongs to must be locked (so we can adjust pg->flags)
 */
void
uvm_pagedeactivate(struct vm_page *pg)
{
	if (pg->pg_flags & PQ_ACTIVE) {
		TAILQ_REMOVE(&uvm.page_active, pg, pageq);
		atomic_clearbits_int(&pg->pg_flags, PQ_ACTIVE);
		uvmexp.active--;
	}
	if ((pg->pg_flags & PQ_INACTIVE) == 0) {
		KASSERT(pg->wire_count == 0);
		if (pg->pg_flags & PQ_SWAPBACKED)
			TAILQ_INSERT_TAIL(&uvm.page_inactive_swp, pg, pageq);
		else
			TAILQ_INSERT_TAIL(&uvm.page_inactive_obj, pg, pageq);
		atomic_setbits_int(&pg->pg_flags, PQ_INACTIVE);
		uvmexp.inactive++;
		pmap_clear_reference(pg);
		/*
		 * update the "clean" bit.  this isn't 100%
		 * accurate, and doesn't have to be.  we'll
		 * re-sync it after we zap all mappings when
		 * scanning the inactive list.
		 */
		if ((pg->pg_flags & PG_CLEAN) != 0 &&
		    pmap_is_modified(pg))
			atomic_clearbits_int(&pg->pg_flags, PG_CLEAN);
	}
}

/*
 * uvm_pageactivate: activate page
 *
 * => caller must lock page queues
 */
void
uvm_pageactivate(struct vm_page *pg)
{
	if (pg->pg_flags & PQ_INACTIVE) {
		if (pg->pg_flags & PQ_SWAPBACKED)
			TAILQ_REMOVE(&uvm.page_inactive_swp, pg, pageq);
		else
			TAILQ_REMOVE(&uvm.page_inactive_obj, pg, pageq);
		atomic_clearbits_int(&pg->pg_flags, PQ_INACTIVE);
		uvmexp.inactive--;
	}
	if (pg->wire_count == 0) {

		/*
		 * if page is already active, remove it from list so we
		 * can put it at tail.  if it wasn't active, then mark
		 * it active and bump active count
		 */
		if (pg->pg_flags & PQ_ACTIVE)
			TAILQ_REMOVE(&uvm.page_active, pg, pageq);
		else {
			atomic_setbits_int(&pg->pg_flags, PQ_ACTIVE);
			uvmexp.active++;
		}

		TAILQ_INSERT_TAIL(&uvm.page_active, pg, pageq);
	}
}

/*
 * uvm_pagezero: zero fill a page
 *
 * => if page is part of an object then the object should be locked
 *	to protect pg->flags.
 */
void
uvm_pagezero(struct vm_page *pg)
{
	atomic_clearbits_int(&pg->pg_flags, PG_CLEAN);
	pmap_zero_page(pg);
}

/*
 * uvm_pagecopy: copy a page
 *
 * => if page is part of an object then the object should be locked
 *	to protect pg->flags.
 */
void
uvm_pagecopy(struct vm_page *src, struct vm_page *dst)
{
	atomic_clearbits_int(&dst->pg_flags, PG_CLEAN);
	pmap_copy_page(src, dst);
}

/*
 * uvm_pagecount: count the number of physical pages in the address range.
 */
psize_t
uvm_pagecount(struct uvm_constraint_range* constraint)
{
	int lcv;
	psize_t sz;
	paddr_t low, high;
	paddr_t ps_low, ps_high;

	/* Algorithm uses page numbers. */
	low = atop(constraint->ucr_low);
	high = atop(constraint->ucr_high);

	sz = 0;
	for (lcv = 0; lcv < vm_nphysseg; lcv++) {
		ps_low = MAX(low, vm_physmem[lcv].avail_start);
		ps_high = MIN(high, vm_physmem[lcv].avail_end);
		if (ps_low < ps_high)
			sz += ps_high - ps_low;
	}
	return sz;
}
