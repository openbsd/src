/*	$OpenBSD: uvm_page.c,v 1.45 2002/09/12 12:56:16 art Exp $	*/
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

#define UVM_PAGE                /* pull in uvm_page.h functions */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/sched.h>
#include <sys/kernel.h>
#include <sys/vnode.h>

#include <uvm/uvm.h>

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
 * we use a hash table with only one bucket during bootup.  we will
 * later rehash (resize) the hash table once the allocator is ready.
 * we static allocate the one bootstrap bucket below...
 */

static struct pglist uvm_bootbucket;

/*
 * local prototypes
 */

static void uvm_pageinsert(struct vm_page *);
static void uvm_pageremove(struct vm_page *);

/*
 * inline functions
 */

/*
 * uvm_pageinsert: insert a page in the object and the hash table
 *
 * => caller must lock object
 * => caller must lock page queues
 * => call should have already set pg's object and offset pointers
 *    and bumped the version counter
 */

__inline static void
uvm_pageinsert(pg)
	struct vm_page *pg;
{
	struct pglist *buck;
	int s;

	KASSERT((pg->flags & PG_TABLED) == 0);
	buck = &uvm.page_hash[uvm_pagehash(pg->uobject,pg->offset)];
	s = splvm();
	simple_lock(&uvm.hashlock);
	TAILQ_INSERT_TAIL(buck, pg, hashq);	/* put in hash */
	simple_unlock(&uvm.hashlock);
	splx(s);

	TAILQ_INSERT_TAIL(&pg->uobject->memq, pg, listq); /* put in object */
	pg->flags |= PG_TABLED;
	pg->uobject->uo_npages++;
}

/*
 * uvm_page_remove: remove page from object and hash
 *
 * => caller must lock object
 * => caller must lock page queues
 */

static __inline void
uvm_pageremove(pg)
	struct vm_page *pg;
{
	struct pglist *buck;
	int s;

	KASSERT(pg->flags & PG_TABLED);
	buck = &uvm.page_hash[uvm_pagehash(pg->uobject,pg->offset)];
	s = splvm();
	simple_lock(&uvm.hashlock);
	TAILQ_REMOVE(buck, pg, hashq);
	simple_unlock(&uvm.hashlock);
	splx(s);

#ifdef UBC
	if (pg->uobject->pgops == &uvm_vnodeops) {
		uvm_pgcnt_vnode--;
	}
#endif

	/* object should be locked */
	TAILQ_REMOVE(&pg->uobject->memq, pg, listq);

	pg->flags &= ~PG_TABLED;
	pg->uobject->uo_npages--;
	pg->uobject = NULL;
	pg->version++;
}

/*
 * uvm_page_init: init the page system.   called from uvm_init().
 * 
 * => we return the range of kernel virtual memory in kvm_startp/kvm_endp
 */

void
uvm_page_init(kvm_startp, kvm_endp)
	vaddr_t *kvm_startp, *kvm_endp;
{
	vsize_t freepages, pagecount, n;
	vm_page_t pagearray;
	int lcv, i;  
	paddr_t paddr;

	/*
	 * init the page queues and page queue locks
	 */

	for (lcv = 0; lcv < VM_NFREELIST; lcv++) {
		for (i = 0; i < PGFL_NQUEUES; i++)
			TAILQ_INIT(&uvm.page_free[lcv].pgfl_queues[i]);
	}
	TAILQ_INIT(&uvm.page_active);
	TAILQ_INIT(&uvm.page_inactive_swp);
	TAILQ_INIT(&uvm.page_inactive_obj);
	simple_lock_init(&uvm.pageqlock);
	simple_lock_init(&uvm.fpageqlock);

	/*
	 * init the <obj,offset> => <page> hash table.  for now
	 * we just have one bucket (the bootstrap bucket).  later on we
	 * will allocate new buckets as we dynamically resize the hash table.
	 */

	uvm.page_nhash = 1;			/* 1 bucket */
	uvm.page_hashmask = 0;			/* mask for hash function */
	uvm.page_hash = &uvm_bootbucket;	/* install bootstrap bucket */
	TAILQ_INIT(uvm.page_hash);		/* init hash table */
	simple_lock_init(&uvm.hashlock);	/* init hash table lock */

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
	 
	pagecount = ((freepages + 1) << PAGE_SHIFT) /
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
			printf("uvm_page_init: lost %ld page(s) in init\n",
			    (long)(n - pagecount));
			panic("uvm_page_init");  /* XXXCDC: shouldn't happen? */
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
				/* add page to free pool */
				uvm_pagefree(&vm_physmem[lcv].pgs[i]);
			}
		}
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

	simple_lock_init(&uvm.pagedaemon_lock);
	simple_lock_init(&uvm.aiodoned_lock);

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
uvm_setpagesize()
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
uvm_pageboot_alloc(size)
	vsize_t size;
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

/* subroutine: try to allocate from memory chunks on the specified freelist */
static boolean_t uvm_page_physget_freelist(paddr_t *, int);

static boolean_t
uvm_page_physget_freelist(paddrp, freelist)
	paddr_t *paddrp;
	int freelist;
{
	int lcv, x;

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

		if (vm_physmem[lcv].free_list != freelist)
			continue;

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
				    panic("vum_page_physget: out of memory!");
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

boolean_t
uvm_page_physget(paddrp)
	paddr_t *paddrp;
{
	int i;

	/* try in the order of freelist preference */
	for (i = 0; i < VM_NFREELIST; i++)
		if (uvm_page_physget_freelist(paddrp, i) == TRUE)
			return (TRUE);
	return (FALSE);
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
uvm_page_physload(start, end, avail_start, avail_end, free_list)
	paddr_t start, end, avail_start, avail_end;
	int free_list;
{
	int preload, lcv;
	psize_t npages;
	struct vm_page *pgs;
	struct vm_physseg *ps;

	if (uvmexp.pagesize == 0)
		panic("uvm_page_physload: page size not set!");

	if (free_list >= VM_NFREELIST || free_list < VM_FREELIST_DEFAULT)
		panic("uvm_page_physload: bad free list %d\n", free_list);

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
#if defined(VM_PHYSSEG_NOADD)
		panic("uvm_page_physload: tried to add RAM after vm_mem_init");
#else
		/* XXXCDC: need some sort of lockout for this case */
		paddr_t paddr;
		npages = end - start;  /* # of pages */
		pgs = (vm_page *)uvm_km_alloc(kernel_map,
		    sizeof(struct vm_page) * npages);
		if (pgs == NULL) {
			printf("uvm_page_physload: can not malloc vm_page "
			    "structs for segment\n");
			printf("\tignoring 0x%lx -> 0x%lx\n", start, end);
			return;
		}
		/* zero data, init phys_addr and free_list, and free pages */
		memset(pgs, 0, sizeof(struct vm_page) * npages);
		for (lcv = 0, paddr = ptoa(start) ;
				 lcv < npages ; lcv++, paddr += PAGE_SIZE) {
			pgs[lcv].phys_addr = paddr;
			pgs[lcv].free_list = free_list;
			if (atop(paddr) >= avail_start &&
			    atop(paddr) <= avail_end)
				uvm_pagefree(&pgs[lcv]);
		}
		/* XXXCDC: incomplete: need to update uvmexp.free, what else? */
		/* XXXCDC: need hook to tell pmap to rebuild pv_list, etc... */
#endif
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
	ps->free_list = free_list;
	vm_nphysseg++;

	/*
	 * done!
	 */

	if (!preload)
		uvm_page_rehash();

	return;
}

/*
 * uvm_page_rehash: reallocate hash table based on number of free pages.
 */

void
uvm_page_rehash()
{
	int freepages, lcv, bucketcount, s, oldcount;
	struct pglist *newbuckets, *oldbuckets;
	struct vm_page *pg;
	size_t newsize, oldsize;

	/*
	 * compute number of pages that can go in the free pool
	 */

	freepages = 0;
	for (lcv = 0 ; lcv < vm_nphysseg ; lcv++)
		freepages +=
		    (vm_physmem[lcv].avail_end - vm_physmem[lcv].avail_start);

	/*
	 * compute number of buckets needed for this number of pages
	 */

	bucketcount = 1;
	while (bucketcount < freepages)
		bucketcount = bucketcount * 2;

	/*
	 * compute the size of the current table and new table.
	 */

	oldbuckets = uvm.page_hash;
	oldcount = uvm.page_nhash;
	oldsize = round_page(sizeof(struct pglist) * oldcount);
	newsize = round_page(sizeof(struct pglist) * bucketcount);

	/*
	 * allocate the new buckets
	 */

	newbuckets = (struct pglist *) uvm_km_alloc(kernel_map, newsize);
	if (newbuckets == NULL) {
		printf("uvm_page_physrehash: WARNING: could not grow page "
		    "hash table\n");
		return;
	}
	for (lcv = 0 ; lcv < bucketcount ; lcv++)
		TAILQ_INIT(&newbuckets[lcv]);

	/*
	 * now replace the old buckets with the new ones and rehash everything
	 */

	s = splvm();
	simple_lock(&uvm.hashlock);
	uvm.page_hash = newbuckets;
	uvm.page_nhash = bucketcount;
	uvm.page_hashmask = bucketcount - 1;  /* power of 2 */

	/* ... and rehash */
	for (lcv = 0 ; lcv < oldcount ; lcv++) {
		while ((pg = oldbuckets[lcv].tqh_first) != NULL) {
			TAILQ_REMOVE(&oldbuckets[lcv], pg, hashq);
			TAILQ_INSERT_TAIL(
			  &uvm.page_hash[uvm_pagehash(pg->uobject, pg->offset)],
			  pg, hashq);
		}
	}
	simple_unlock(&uvm.hashlock);
	splx(s);

	/*
	 * free old bucket array if is not the boot-time table
	 */

	if (oldbuckets != &uvm_bootbucket)
		uvm_km_free(kernel_map, (vaddr_t) oldbuckets, oldsize);

	/*
	 * done
	 */
	return;
}


#if 1 /* XXXCDC: TMP TMP TMP DEBUG DEBUG DEBUG */

void uvm_page_physdump(void); /* SHUT UP GCC */

/* call from DDB */
void
uvm_page_physdump()
{
	int lcv;

	printf("rehash: physical memory config [segs=%d of %d]:\n",
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
	printf("number of buckets = %d\n", uvm.page_nhash);
}
#endif

/*
 * uvm_pagealloc_strat: allocate vm_page from a particular free list.
 *
 * => return null if no pages free
 * => wake up pagedaemon if number of free pages drops below low water mark
 * => if obj != NULL, obj must be locked (to put in hash)
 * => if anon != NULL, anon must be locked (to put in anon)
 * => only one of obj or anon can be non-null
 * => caller must activate/deactivate page if it is not wired.
 * => free_list is ignored if strat == UVM_PGA_STRAT_NORMAL.
 * => policy decision: it is more important to pull a page off of the
 *	appropriate priority free list than it is to get a zero'd or
 *	unknown contents page.  This is because we live with the
 *	consequences of a bad free list decision for the entire
 *	lifetime of the page, e.g. if the page comes from memory that
 *	is slower to access.
 */

struct vm_page *
uvm_pagealloc_strat(obj, off, anon, flags, strat, free_list)
	struct uvm_object *obj;
	voff_t off;
	int flags;
	struct vm_anon *anon;
	int strat, free_list;
{
	int lcv, try1, try2, s, zeroit = 0;
	struct vm_page *pg;
	struct pglist *freeq;
	struct pgfreelist *pgfl;
	boolean_t use_reserve;

	KASSERT(obj == NULL || anon == NULL);
	KASSERT(off == trunc_page(off));
	s = uvm_lock_fpageq();

	/*
	 * check to see if we need to generate some free pages waking
	 * the pagedaemon.
	 */

#ifdef UBC
	if (uvmexp.free + uvmexp.paging < uvmexp.freemin ||
	    (uvmexp.free + uvmexp.paging < uvmexp.freetarg &&
	     uvmexp.inactive < uvmexp.inactarg)) {
		wakeup(&uvm.pagedaemon);
	}
#else
	if (uvmexp.free < uvmexp.freemin || (uvmexp.free < uvmexp.freetarg &&
	    uvmexp.inactive < uvmexp.inactarg))
		wakeup(&uvm.pagedaemon);
#endif

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
	     !(use_reserve && (curproc == uvm.pagedaemon_proc ||
				curproc == syncerproc))))
		goto fail;

#if PGFL_NQUEUES != 2
#error uvm_pagealloc_strat needs to be updated
#endif

	/*
	 * If we want a zero'd page, try the ZEROS queue first, otherwise
	 * we try the UNKNOWN queue first.
	 */
	if (flags & UVM_PGA_ZERO) {
		try1 = PGFL_ZEROS;
		try2 = PGFL_UNKNOWN;
	} else {
		try1 = PGFL_UNKNOWN;
		try2 = PGFL_ZEROS;
	}

 again:
	switch (strat) {
	case UVM_PGA_STRAT_NORMAL:
		/* Check all freelists in descending priority order. */
		for (lcv = 0; lcv < VM_NFREELIST; lcv++) {
			pgfl = &uvm.page_free[lcv];
			if ((pg = TAILQ_FIRST((freeq =
			      &pgfl->pgfl_queues[try1]))) != NULL ||
			    (pg = TAILQ_FIRST((freeq =
			      &pgfl->pgfl_queues[try2]))) != NULL)
				goto gotit;
		}

		/* No pages free! */
		goto fail;

	case UVM_PGA_STRAT_ONLY:
	case UVM_PGA_STRAT_FALLBACK:
		/* Attempt to allocate from the specified free list. */
		KASSERT(free_list >= 0 && free_list < VM_NFREELIST);
		pgfl = &uvm.page_free[free_list];
		if ((pg = TAILQ_FIRST((freeq =
		      &pgfl->pgfl_queues[try1]))) != NULL ||
		    (pg = TAILQ_FIRST((freeq =
		      &pgfl->pgfl_queues[try2]))) != NULL)
			goto gotit;

		/* Fall back, if possible. */
		if (strat == UVM_PGA_STRAT_FALLBACK) {
			strat = UVM_PGA_STRAT_NORMAL;
			goto again;
		}

		/* No pages free! */
		goto fail;

	default:
		panic("uvm_pagealloc_strat: bad strat %d", strat);
		/* NOTREACHED */
	}

 gotit:
	TAILQ_REMOVE(freeq, pg, pageq);
	uvmexp.free--;

	/* update zero'd page count */
	if (pg->flags & PG_ZERO)
		uvmexp.zeropages--;

	/*
	 * update allocation statistics and remember if we have to
	 * zero the page
	 */
	if (flags & UVM_PGA_ZERO) {
		if (pg->flags & PG_ZERO) {
			uvmexp.pga_zerohit++;
			zeroit = 0;
		} else {
			uvmexp.pga_zeromiss++;
			zeroit = 1;
		}
	}

	uvm_unlock_fpageq(s);		/* unlock free page queue */

	pg->offset = off;
	pg->uobject = obj;
	pg->uanon = anon;
	pg->flags = PG_BUSY|PG_CLEAN|PG_FAKE;
	pg->version++;
	if (anon) {
		anon->u.an_page = pg;
		pg->pqflags = PQ_ANON;
#ifdef UBC
		uvm_pgcnt_anon++;
#endif
	} else {
		if (obj)
			uvm_pageinsert(pg);
		pg->pqflags = 0;
	}
#if defined(UVM_PAGE_TRKOWN)
	pg->owner_tag = NULL;
#endif
	UVM_PAGE_OWN(pg, "new alloc");

	if (flags & UVM_PGA_ZERO) {
		/*
		 * A zero'd page is not clean.  If we got a page not already
		 * zero'd, then we have to zero it ourselves.
		 */
		pg->flags &= ~PG_CLEAN;
		if (zeroit)
			pmap_zero_page(pg);
	}

	return(pg);

 fail:
	uvm_unlock_fpageq(s);
	return (NULL);
}

/*
 * uvm_pagerealloc: reallocate a page from one object to another
 *
 * => both objects must be locked
 */

void
uvm_pagerealloc(pg, newobj, newoff)
	struct vm_page *pg;
	struct uvm_object *newobj;
	voff_t newoff;
{
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
		pg->version++;
		uvm_pageinsert(pg);
	}
}


/*
 * uvm_pagefree: free page
 *
 * => erase page's identity (i.e. remove from hash/object)
 * => put page on free list
 * => caller must lock owning object (either anon or uvm_object)
 * => caller must lock page queues
 * => assumes all valid mappings of pg are gone
 */

void
uvm_pagefree(pg)
	struct vm_page *pg;
{
	int s;
	int saved_loan_count = pg->loan_count;

#ifdef DEBUG
	if (pg->uobject == (void *)0xdeadbeef &&
	    pg->uanon == (void *)0xdeadbeef) {
		panic("uvm_pagefree: freeing free page %p\n", pg);
	}
#endif

	/*
	 * if the page was an object page (and thus "TABLED"), remove it
	 * from the object.
	 */

	if (pg->flags & PG_TABLED) {

		/*
		 * if the object page is on loan we are going to drop ownership.
		 * it is possible that an anon will take over as owner for this
		 * page later on.   the anon will want a !PG_CLEAN page so that
		 * it knows it needs to allocate swap if it wants to page the 
		 * page out. 
		 */

		if (saved_loan_count)
			pg->flags &= ~PG_CLEAN;	/* in case an anon takes over */
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
	} else if (saved_loan_count && (pg->pqflags & PQ_ANON)) {

		/*
		 * if our page is owned by an anon and is loaned out to the
		 * kernel then we just want to drop ownership and return.
		 * the kernel must free the page when all its loans clear ...
		 * note that the kernel can't change the loan status of our
		 * page as long as we are holding PQ lock.
		 */

		pg->pqflags &= ~PQ_ANON;
		pg->uanon = NULL;
		return;
	}
	KASSERT(saved_loan_count == 0);

	/*
	 * now remove the page from the queues
	 */

	if (pg->pqflags & PQ_ACTIVE) {
		TAILQ_REMOVE(&uvm.page_active, pg, pageq);
		pg->pqflags &= ~PQ_ACTIVE;
		uvmexp.active--;
	}
	if (pg->pqflags & PQ_INACTIVE) {
		if (pg->pqflags & PQ_SWAPBACKED)
			TAILQ_REMOVE(&uvm.page_inactive_swp, pg, pageq);
		else
			TAILQ_REMOVE(&uvm.page_inactive_obj, pg, pageq);
		pg->pqflags &= ~PQ_INACTIVE;
		uvmexp.inactive--;
	}

	/*
	 * if the page was wired, unwire it now.
	 */

	if (pg->wire_count) {
		pg->wire_count = 0;
		uvmexp.wired--;
	}
#ifdef UBC
	if (pg->uanon) {
		uvm_pgcnt_anon--;
	}
#endif

	/*
	 * and put on free queue
	 */

	pg->flags &= ~PG_ZERO;

	s = uvm_lock_fpageq();
	TAILQ_INSERT_TAIL(&uvm.page_free[
	    uvm_page_lookup_freelist(pg)].pgfl_queues[PGFL_UNKNOWN], pg, pageq);
	pg->pqflags = PQ_FREE;
#ifdef DEBUG
	pg->uobject = (void *)0xdeadbeef;
	pg->offset = 0xdeadbeef;
	pg->uanon = (void *)0xdeadbeef;
#endif
	uvmexp.free++;

	if (uvmexp.zeropages < UVM_PAGEZERO_TARGET)
		uvm.page_idle_zero = vm_page_zero_enable;

	uvm_unlock_fpageq(s);
}

/*
 * uvm_page_unbusy: unbusy an array of pages.
 *
 * => pages must either all belong to the same object, or all belong to anons.
 * => if pages are object-owned, object must be locked.
 * => if pages are anon-owned, anons must be unlockd and have 0 refcount.
 */

void
uvm_page_unbusy(pgs, npgs)
	struct vm_page **pgs;
	int npgs;
{
	struct vm_page *pg;
	struct uvm_object *uobj;
	int i;
	UVMHIST_FUNC("uvm_page_unbusy"); UVMHIST_CALLED(ubchist);

	for (i = 0; i < npgs; i++) {
		pg = pgs[i];

		if (pg == NULL) {
			continue;
		}
		if (pg->flags & PG_WANTED) {
			wakeup(pg);
		}
		if (pg->flags & PG_RELEASED) {
			UVMHIST_LOG(ubchist, "releasing pg %p", pg,0,0,0);
			uobj = pg->uobject;
			if (uobj != NULL) {
				uobj->pgops->pgo_releasepg(pg, NULL);
			} else {
				pg->flags &= ~(PG_BUSY);
				UVM_PAGE_OWN(pg, NULL);
				uvm_anfree(pg->uanon);
			}
		} else {
			UVMHIST_LOG(ubchist, "unbusying pg %p", pg,0,0,0);
			pg->flags &= ~(PG_WANTED|PG_BUSY);
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
uvm_page_own(pg, tag)
	struct vm_page *pg;
	char *tag;
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
uvm_pageidlezero()
{
	struct vm_page *pg;
	struct pgfreelist *pgfl;
	int free_list, s;

	do {
		s = uvm_lock_fpageq();

		if (uvmexp.zeropages >= UVM_PAGEZERO_TARGET) {
			uvm.page_idle_zero = FALSE;
			uvm_unlock_fpageq(s);
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
			uvm_unlock_fpageq(s);
			return;
		}

		TAILQ_REMOVE(&pgfl->pgfl_queues[PGFL_UNKNOWN], pg, pageq);
		uvmexp.free--;
		uvm_unlock_fpageq(s);

#ifdef PMAP_PAGEIDLEZERO
		if (PMAP_PAGEIDLEZERO(pg) == FALSE) {
			/*
			 * The machine-dependent code detected some
			 * reason for us to abort zeroing pages,
			 * probably because there is a process now
			 * ready to run.
			 */
			s = uvm_lock_fpageq();
			TAILQ_INSERT_HEAD(&pgfl->pgfl_queues[PGFL_UNKNOWN],
			    pg, pageq);
			uvmexp.free++;
			uvmexp.zeroaborts++;
			uvm_unlock_fpageq(s);
			return;
		}
#else
		/*
		 * XXX This will toast the cache unless the pmap_zero_page()
		 * XXX implementation does uncached access.
		 */
		pmap_zero_page(pg);
#endif
		pg->flags |= PG_ZERO;

		s = uvm_lock_fpageq();
		TAILQ_INSERT_HEAD(&pgfl->pgfl_queues[PGFL_ZEROS], pg, pageq);
		uvmexp.free++;
		uvmexp.zeropages++;
		uvm_unlock_fpageq(s);
	} while (whichqs == 0);
}
