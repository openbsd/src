/*	$NetBSD: uvm_km.c,v 1.18 1998/10/18 23:49:59 chs Exp $	*/

/*
 * XXXCDC: "ROUGH DRAFT" QUALITY UVM PRE-RELEASE FILE!
 *         >>>USE AT YOUR OWN RISK, WORK IS NOT FINISHED<<<
 */
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
 *	@(#)vm_kern.c   8.3 (Berkeley) 1/12/94
 * from: Id: uvm_km.c,v 1.1.2.14 1998/02/06 05:19:27 chs Exp
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
 * uvm_km.c: handle kernel memory allocation and management
 */

/*
 * overview of kernel memory management:
 *
 * the kernel virtual address space is mapped by "kernel_map."   kernel_map
 * starts at VM_MIN_KERNEL_ADDRESS and goes to VM_MAX_KERNEL_ADDRESS.
 * note that VM_MIN_KERNEL_ADDRESS is equal to vm_map_min(kernel_map).
 *
 * the kernel_map has several "submaps."   submaps can only appear in 
 * the kernel_map (user processes can't use them).   submaps "take over"
 * the management of a sub-range of the kernel's address space.  submaps
 * are typically allocated at boot time and are never released.   kernel
 * virtual address space that is mapped by a submap is locked by the 
 * submap's lock -- not the kernel_map's lock.
 *
 * thus, the useful feature of submaps is that they allow us to break
 * up the locking and protection of the kernel address space into smaller
 * chunks.
 *
 * the vm system has several standard kernel submaps, including:
 *   kmem_map => contains only wired kernel memory for the kernel
 *		malloc.   *** access to kmem_map must be protected
 *		by splimp() because we are allowed to call malloc()
 *		at interrupt time ***
 *   mb_map => memory for large mbufs,  *** protected by splimp ***
 *   pager_map => used to map "buf" structures into kernel space
 *   exec_map => used during exec to handle exec args
 *   etc...
 *
 * the kernel allocates its private memory out of special uvm_objects whose
 * reference count is set to UVM_OBJ_KERN (thus indicating that the objects
 * are "special" and never die).   all kernel objects should be thought of
 * as large, fixed-sized, sparsely populated uvm_objects.   each kernel 
 * object is equal to the size of kernel virtual address space (i.e. the
 * value "VM_MAX_KERNEL_ADDRESS - VM_MIN_KERNEL_ADDRESS").
 *
 * most kernel private memory lives in kernel_object.   the only exception
 * to this is for memory that belongs to submaps that must be protected
 * by splimp().    each of these submaps has their own private kernel 
 * object (e.g. kmem_object, mb_object).
 *
 * note that just because a kernel object spans the entire kernel virutal
 * address space doesn't mean that it has to be mapped into the entire space.
 * large chunks of a kernel object's space go unused either because 
 * that area of kernel VM is unmapped, or there is some other type of 
 * object mapped into that range (e.g. a vnode).    for submap's kernel
 * objects, the only part of the object that can ever be populated is the
 * offsets that are managed by the submap.
 *
 * note that the "offset" in a kernel object is always the kernel virtual
 * address minus the VM_MIN_KERNEL_ADDRESS (aka vm_map_min(kernel_map)).
 * example:
 *   suppose VM_MIN_KERNEL_ADDRESS is 0xf8000000 and the kernel does a
 *   uvm_km_alloc(kernel_map, PAGE_SIZE) [allocate 1 wired down page in the
 *   kernel map].    if uvm_km_alloc returns virtual address 0xf8235000,
 *   then that means that the page at offset 0x235000 in kernel_object is
 *   mapped at 0xf8235000.   
 *
 * note that the offsets in kmem_object and mb_object also follow this
 * rule.   this means that the offsets for kmem_object must fall in the
 * range of [vm_map_min(kmem_object) - vm_map_min(kernel_map)] to
 * [vm_map_max(kmem_object) - vm_map_min(kernel_map)], so the offsets
 * in those objects will typically not start at zero.
 *
 * kernel object have one other special property: when the kernel virtual
 * memory mapping them is unmapped, the backing memory in the object is
 * freed right away.   this is done with the uvm_km_pgremove() function.
 * this has to be done because there is no backing store for kernel pages
 * and no need to save them after they are no longer referenced.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>

#include <vm/vm.h>
#include <vm/vm_page.h>
#include <vm/vm_kern.h>

#include <uvm/uvm.h>

/*
 * global data structures
 */

vm_map_t kernel_map = NULL;

/*
 * local functions
 */

static int uvm_km_get __P((struct uvm_object *, vaddr_t, 
													 vm_page_t *, int *, int, vm_prot_t, int, int));
/*
 * local data structues
 */

static struct vm_map		kernel_map_store;
static struct uvm_object	kmem_object_store;
static struct uvm_object	mb_object_store;

static struct uvm_pagerops km_pager = {
	NULL,	/* init */
	NULL, /* attach */
	NULL, /* reference */
	NULL, /* detach */
	NULL, /* fault */
	NULL, /* flush */
	uvm_km_get, /* get */
	/* ... rest are NULL */
};

/*
 * uvm_km_get: pager get function for kernel objects
 *
 * => currently we do not support pageout to the swap area, so this
 *    pager is very simple.    eventually we may want an anonymous 
 *    object pager which will do paging.
 * => XXXCDC: this pager should be phased out in favor of the aobj pager
 */


static int
uvm_km_get(uobj, offset, pps, npagesp, centeridx, access_type, advice, flags)
	struct uvm_object *uobj;
	vaddr_t offset;
	struct vm_page **pps;
	int *npagesp;
	int centeridx, advice, flags;
	vm_prot_t access_type;
{
	vaddr_t current_offset;
	vm_page_t ptmp;
	int lcv, gotpages, maxpages;
	boolean_t done;
	UVMHIST_FUNC("uvm_km_get"); UVMHIST_CALLED(maphist);

	UVMHIST_LOG(maphist, "flags=%d", flags,0,0,0);
	
	/*
	 * get number of pages
	 */

	maxpages = *npagesp;

	/*
	 * step 1: handled the case where fault data structures are locked.
	 */

	if (flags & PGO_LOCKED) {

		/*
		 * step 1a: get pages that are already resident.   only do
		 * this if the data structures are locked (i.e. the first time
		 * through).
		 */

		done = TRUE;	/* be optimistic */
		gotpages = 0;	/* # of pages we got so far */

		for (lcv = 0, current_offset = offset ; 
		    lcv < maxpages ; lcv++, current_offset += PAGE_SIZE) {

			/* do we care about this page?  if not, skip it */
			if (pps[lcv] == PGO_DONTCARE)
				continue;

			/* lookup page */
			ptmp = uvm_pagelookup(uobj, current_offset);
			
			/* null?  attempt to allocate the page */
			if (ptmp == NULL) {
				ptmp = uvm_pagealloc(uobj, current_offset,
				    NULL);
				if (ptmp) {
					/* new page */
					ptmp->flags &= ~(PG_BUSY|PG_FAKE);
					UVM_PAGE_OWN(ptmp, NULL);
					uvm_pagezero(ptmp);
				}
			}

			/*
			 * to be useful must get a non-busy, non-released page
			 */
			if (ptmp == NULL ||
			    (ptmp->flags & (PG_BUSY|PG_RELEASED)) != 0) {
				if (lcv == centeridx ||
				    (flags & PGO_ALLPAGES) != 0)
					/* need to do a wait or I/O! */
					done = FALSE;
				continue;
			}

			/*
			 * useful page: busy/lock it and plug it in our
			 * result array
			 */

			/* caller must un-busy this page */
			ptmp->flags |= PG_BUSY;	
			UVM_PAGE_OWN(ptmp, "uvm_km_get1");
			pps[lcv] = ptmp;
			gotpages++;

		}	/* "for" lcv loop */

		/*
		 * step 1b: now we've either done everything needed or we
		 * to unlock and do some waiting or I/O.
		 */

		UVMHIST_LOG(maphist, "<- done (done=%d)", done, 0,0,0);

		*npagesp = gotpages;
		if (done)
			return(VM_PAGER_OK);		/* bingo! */
		else
			return(VM_PAGER_UNLOCK);	/* EEK!   Need to
							 * unlock and I/O */
	}

	/*
	 * step 2: get non-resident or busy pages.
	 * object is locked.   data structures are unlocked.
	 */

	for (lcv = 0, current_offset = offset ; 
	    lcv < maxpages ; lcv++, current_offset += PAGE_SIZE) {
		
		/* skip over pages we've already gotten or don't want */
		/* skip over pages we don't _have_ to get */
		if (pps[lcv] != NULL ||
		    (lcv != centeridx && (flags & PGO_ALLPAGES) == 0))
			continue;

		/*
		 * we have yet to locate the current page (pps[lcv]).   we
		 * first look for a page that is already at the current offset.
		 * if we find a page, we check to see if it is busy or
		 * released.  if that is the case, then we sleep on the page
		 * until it is no longer busy or released and repeat the
		 * lookup.    if the page we found is neither busy nor
		 * released, then we busy it (so we own it) and plug it into
		 * pps[lcv].   this 'break's the following while loop and
		 * indicates we are ready to move on to the next page in the
		 * "lcv" loop above.
		 *
		 * if we exit the while loop with pps[lcv] still set to NULL,
		 * then it means that we allocated a new busy/fake/clean page
		 * ptmp in the object and we need to do I/O to fill in the
		 * data.
		 */

		while (pps[lcv] == NULL) {	/* top of "pps" while loop */
			
			/* look for a current page */
			ptmp = uvm_pagelookup(uobj, current_offset);

			/* nope?   allocate one now (if we can) */
			if (ptmp == NULL) {

				ptmp = uvm_pagealloc(uobj, current_offset,
				    NULL);	/* alloc */

				/* out of RAM? */
				if (ptmp == NULL) {
					simple_unlock(&uobj->vmobjlock);
					uvm_wait("kmgetwait1");
					simple_lock(&uobj->vmobjlock);
					/* goto top of pps while loop */
					continue;
				}

				/* 
				 * got new page ready for I/O.  break pps
				 * while loop.  pps[lcv] is still NULL.
				 */
				break;		
			}

			/* page is there, see if we need to wait on it */
			if ((ptmp->flags & (PG_BUSY|PG_RELEASED)) != 0) {
				ptmp->flags |= PG_WANTED;
				UVM_UNLOCK_AND_WAIT(ptmp,&uobj->vmobjlock, 0,
				    "uvn_get",0);
				simple_lock(&uobj->vmobjlock);
				continue;	/* goto top of pps while loop */
			}
			
			/* 
			 * if we get here then the page has become resident
			 * and unbusy between steps 1 and 2.  we busy it now
			 * (so we own it) and set pps[lcv] (so that we exit
			 * the while loop).  caller must un-busy.
			 */
			ptmp->flags |= PG_BUSY;
			UVM_PAGE_OWN(ptmp, "uvm_km_get2");
			pps[lcv] = ptmp;
		}

		/*
		 * if we own the a valid page at the correct offset, pps[lcv]
		 * will point to it.   nothing more to do except go to the
		 * next page.
		 */

		if (pps[lcv])
			continue;			/* next lcv */

		/*
		 * we have a "fake/busy/clean" page that we just allocated.  
		 * do the needed "i/o" (in this case that means zero it).
		 */

		uvm_pagezero(ptmp);
		ptmp->flags &= ~(PG_FAKE);
		pps[lcv] = ptmp;

	}	/* lcv loop */

	/*
	 * finally, unlock object and return.
	 */

	simple_unlock(&uobj->vmobjlock);
	UVMHIST_LOG(maphist, "<- done (OK)",0,0,0,0);
	return(VM_PAGER_OK);
}

/*
 * uvm_km_init: init kernel maps and objects to reflect reality (i.e.
 * KVM already allocated for text, data, bss, and static data structures).
 *
 * => KVM is defined by VM_MIN_KERNEL_ADDRESS/VM_MAX_KERNEL_ADDRESS.
 *    we assume that [min -> start] has already been allocated and that
 *    "end" is the end.
 */

void
uvm_km_init(start, end)
	vaddr_t start, end;
{
	vaddr_t base = VM_MIN_KERNEL_ADDRESS;

	/*
	 * first, init kernel memory objects.
	 */

	/* kernel_object: for pageable anonymous kernel memory */
	uvm.kernel_object = uao_create(VM_MAX_KERNEL_ADDRESS -
				 VM_MIN_KERNEL_ADDRESS, UAO_FLAG_KERNOBJ);

	/* kmem_object: for malloc'd memory (wired, protected by splimp) */
	simple_lock_init(&kmem_object_store.vmobjlock);
	kmem_object_store.pgops = &km_pager;
	TAILQ_INIT(&kmem_object_store.memq);
	kmem_object_store.uo_npages = 0;
	/* we are special.  we never die */
	kmem_object_store.uo_refs = UVM_OBJ_KERN; 
	uvmexp.kmem_object = &kmem_object_store;

	/* mb_object: for mbuf memory (always wired, protected by splimp) */
	simple_lock_init(&mb_object_store.vmobjlock);
	mb_object_store.pgops = &km_pager;
	TAILQ_INIT(&mb_object_store.memq);
	mb_object_store.uo_npages = 0;
	/* we are special.  we never die */
	mb_object_store.uo_refs = UVM_OBJ_KERN; 
	uvmexp.mb_object = &mb_object_store;

	/*
	 * init the map and reserve allready allocated kernel space 
	 * before installing.
	 */

	uvm_map_setup(&kernel_map_store, base, end, FALSE);
	kernel_map_store.pmap = pmap_kernel();
	if (uvm_map(&kernel_map_store, &base, start - base, NULL,
	    UVM_UNKNOWN_OFFSET, UVM_MAPFLAG(UVM_PROT_ALL, UVM_PROT_ALL,
	    UVM_INH_NONE, UVM_ADV_RANDOM,UVM_FLAG_FIXED)) != KERN_SUCCESS)
		panic("uvm_km_init: could not reserve space for kernel");
	
	/*
	 * install!
	 */

	kernel_map = &kernel_map_store;
}

/*
 * uvm_km_suballoc: allocate a submap in the kernel map.   once a submap
 * is allocated all references to that area of VM must go through it.  this
 * allows the locking of VAs in kernel_map to be broken up into regions.
 *
 * => if `fixed' is true, *min specifies where the region described
 *      by the submap must start
 * => if submap is non NULL we use that as the submap, otherwise we
 *	alloc a new map
 */
struct vm_map *
uvm_km_suballoc(map, min, max, size, pageable, fixed, submap)
	struct vm_map *map;
	vaddr_t *min, *max;		/* OUT, OUT */
	vsize_t size;
	boolean_t pageable;
	boolean_t fixed;
	struct vm_map *submap;
{
	int mapflags = UVM_FLAG_NOMERGE | (fixed ? UVM_FLAG_FIXED : 0);

	size = round_page(size);	/* round up to pagesize */

	/*
	 * first allocate a blank spot in the parent map
	 */

	if (uvm_map(map, min, size, NULL, UVM_UNKNOWN_OFFSET, 
	    UVM_MAPFLAG(UVM_PROT_ALL, UVM_PROT_ALL, UVM_INH_NONE,
	    UVM_ADV_RANDOM, mapflags)) != KERN_SUCCESS) {
	       panic("uvm_km_suballoc: unable to allocate space in parent map");
	}

	/*
	 * set VM bounds (min is filled in by uvm_map)
	 */

	*max = *min + size;

	/*
	 * add references to pmap and create or init the submap
	 */

	pmap_reference(vm_map_pmap(map));
	if (submap == NULL) {
		submap = uvm_map_create(vm_map_pmap(map), *min, *max, pageable);
		if (submap == NULL)
			panic("uvm_km_suballoc: unable to create submap");
	} else {
		uvm_map_setup(submap, *min, *max, pageable);
		submap->pmap = vm_map_pmap(map);
	}

	/*
	 * now let uvm_map_submap plug in it...
	 */

	if (uvm_map_submap(map, *min, *max, submap) != KERN_SUCCESS)
		panic("uvm_km_suballoc: submap allocation failed");

	return(submap);
}

/*
 * uvm_km_pgremove: remove pages from a kernel uvm_object.
 *
 * => when you unmap a part of anonymous kernel memory you want to toss
 *    the pages right away.    (this gets called from uvm_unmap_...).
 */

#define UKM_HASH_PENALTY 4      /* a guess */

void
uvm_km_pgremove(uobj, start, end)
	struct uvm_object *uobj;
	vaddr_t start, end;
{
	boolean_t by_list, is_aobj;
	struct vm_page *pp, *ppnext;
	vaddr_t curoff;
	UVMHIST_FUNC("uvm_km_pgremove"); UVMHIST_CALLED(maphist);

	simple_lock(&uobj->vmobjlock);		/* lock object */

	/* is uobj an aobj? */
	is_aobj = uobj->pgops == &aobj_pager;

	/* choose cheapest traversal */
	by_list = (uobj->uo_npages <=
	     ((end - start) >> PAGE_SHIFT) * UKM_HASH_PENALTY);
 
	if (by_list)
		goto loop_by_list;

	/* by hash */

	for (curoff = start ; curoff < end ; curoff += PAGE_SIZE) {
		pp = uvm_pagelookup(uobj, curoff);
		if (pp == NULL)
			continue;

		UVMHIST_LOG(maphist,"  page 0x%x, busy=%d", pp,
		    pp->flags & PG_BUSY, 0, 0);
		/* now do the actual work */
		if (pp->flags & PG_BUSY)
			/* owner must check for this when done */
			pp->flags |= PG_RELEASED;
		else {
			pmap_page_protect(PMAP_PGARG(pp), VM_PROT_NONE);

			/*
			 * if this kernel object is an aobj, free the swap slot.
			 */
			if (is_aobj) {
				int slot = uao_set_swslot(uobj,
							  curoff >> PAGE_SHIFT,
							  0);

				if (slot)
					uvm_swap_free(slot, 1);
			}

			uvm_lock_pageq();
			uvm_pagefree(pp);
			uvm_unlock_pageq();
		}
		/* done */

	}
	simple_unlock(&uobj->vmobjlock);
	return;

loop_by_list:

	for (pp = uobj->memq.tqh_first ; pp != NULL ; pp = ppnext) {

		ppnext = pp->listq.tqe_next;
		if (pp->offset < start || pp->offset >= end) {
			continue;
		}

		UVMHIST_LOG(maphist,"  page 0x%x, busy=%d", pp,
		    pp->flags & PG_BUSY, 0, 0);
		/* now do the actual work */
		if (pp->flags & PG_BUSY)
			/* owner must check for this when done */
			pp->flags |= PG_RELEASED;
		else {
			pmap_page_protect(PMAP_PGARG(pp), VM_PROT_NONE);

			/*
			 * if this kernel object is an aobj, free the swap slot.
			 */
			if (is_aobj) {
				int slot = uao_set_swslot(uobj,
						pp->offset >> PAGE_SHIFT, 0);

				if (slot)
					uvm_swap_free(slot, 1);
			}

			uvm_lock_pageq();
			uvm_pagefree(pp);
			uvm_unlock_pageq();
		}
		/* done */

	}
	simple_unlock(&uobj->vmobjlock);
	return;
}


/*
 * uvm_km_kmemalloc: lower level kernel memory allocator for malloc()
 *
 * => we map wired memory into the specified map using the obj passed in
 * => NOTE: we can return NULL even if we can wait if there is not enough
 *	free VM space in the map... caller should be prepared to handle
 *	this case.
 * => we return KVA of memory allocated
 * => flags: NOWAIT, VALLOC - just allocate VA, TRYLOCK - fail if we can't
 *	lock the map
 */

vaddr_t
uvm_km_kmemalloc(map, obj, size, flags)
	vm_map_t map;
	struct uvm_object *obj;
	vsize_t size;
	int flags;
{
	vaddr_t kva, loopva;
	vaddr_t offset;
	struct vm_page *pg;
	UVMHIST_FUNC("uvm_km_kmemalloc"); UVMHIST_CALLED(maphist);


	UVMHIST_LOG(maphist,"  (map=0x%x, obj=0x%x, size=0x%x, flags=%d)",
	map, obj, size, flags);
#ifdef DIAGNOSTIC
	/* sanity check */
	if (vm_map_pmap(map) != pmap_kernel())
		panic("uvm_km_kmemalloc: invalid map");
#endif

	/*
	 * setup for call
	 */

	size = round_page(size);
	kva = vm_map_min(map);	/* hint */

	/*
	 * allocate some virtual space
	 */

	if (uvm_map(map, &kva, size, obj, UVM_UNKNOWN_OFFSET,
	      UVM_MAPFLAG(UVM_PROT_ALL, UVM_PROT_ALL, UVM_INH_NONE,
			  UVM_ADV_RANDOM, (flags & UVM_KMF_TRYLOCK))) 
			!= KERN_SUCCESS) {
		UVMHIST_LOG(maphist, "<- done (no VM)",0,0,0,0);
		return(0);
	}

	/*
	 * if all we wanted was VA, return now
	 */

	if (flags & UVM_KMF_VALLOC) {
		UVMHIST_LOG(maphist,"<- done valloc (kva=0x%x)", kva,0,0,0);
		return(kva);
	}
	/*
	 * recover object offset from virtual address
	 */

	offset = kva - vm_map_min(kernel_map);
	UVMHIST_LOG(maphist, "  kva=0x%x, offset=0x%x", kva, offset,0,0);

	/*
	 * now allocate and map in the memory... note that we are the only ones
	 * whom should ever get a handle on this area of VM.
	 */

	loopva = kva;
	while (size) {
		simple_lock(&obj->vmobjlock);
		pg = uvm_pagealloc(obj, offset, NULL);
		if (pg) {
			pg->flags &= ~PG_BUSY;	/* new page */
			UVM_PAGE_OWN(pg, NULL);
		}
		simple_unlock(&obj->vmobjlock);
		
		/*
		 * out of memory?
		 */

		if (pg == NULL) {
			if (flags & UVM_KMF_NOWAIT) {
				/* free everything! */
				uvm_unmap(map, kva, kva + size);
				return(0);
			} else {
				uvm_wait("km_getwait2");	/* sleep here */
				continue;
			}
		}
		
		/*
		 * map it in: note that we call pmap_enter with the map and
		 * object unlocked in case we are kmem_map/kmem_object
		 * (because if pmap_enter wants to allocate out of kmem_object
		 * it will need to lock it itself!)
		 */
#if defined(PMAP_NEW)
		pmap_kenter_pa(loopva, VM_PAGE_TO_PHYS(pg), VM_PROT_ALL);
#else
		pmap_enter(map->pmap, loopva, VM_PAGE_TO_PHYS(pg),
		    UVM_PROT_ALL, TRUE);
#endif
		loopva += PAGE_SIZE;
		offset += PAGE_SIZE;
		size -= PAGE_SIZE;
	}

	UVMHIST_LOG(maphist,"<- done (kva=0x%x)", kva,0,0,0);
	return(kva);
}

/*
 * uvm_km_free: free an area of kernel memory
 */

void
uvm_km_free(map, addr, size)
	vm_map_t map;
	vaddr_t addr;
	vsize_t size;
{

	uvm_unmap(map, trunc_page(addr), round_page(addr+size));
}

/*
 * uvm_km_free_wakeup: free an area of kernel memory and wake up
 * anyone waiting for vm space.
 *
 * => XXX: "wanted" bit + unlock&wait on other end?
 */

void
uvm_km_free_wakeup(map, addr, size)
	vm_map_t map;
	vaddr_t addr;
	vsize_t size;
{
	vm_map_entry_t dead_entries;

	vm_map_lock(map);
	(void)uvm_unmap_remove(map, trunc_page(addr), round_page(addr+size), 
			 &dead_entries);
	thread_wakeup(map);
	vm_map_unlock(map);

	if (dead_entries != NULL)
		uvm_unmap_detach(dead_entries, 0);
}

/*
 * uvm_km_alloc1: allocate wired down memory in the kernel map.
 *
 * => we can sleep if needed
 */

vaddr_t
uvm_km_alloc1(map, size, zeroit)
	vm_map_t map;
	vsize_t size;
	boolean_t zeroit;
{
	vaddr_t kva, loopva, offset;
	struct vm_page *pg;
	UVMHIST_FUNC("uvm_km_alloc1"); UVMHIST_CALLED(maphist);

	UVMHIST_LOG(maphist,"(map=0x%x, size=0x%x)", map, size,0,0);

#ifdef DIAGNOSTIC
	if (vm_map_pmap(map) != pmap_kernel())
		panic("uvm_km_alloc1");
#endif

	size = round_page(size);
	kva = vm_map_min(map);		/* hint */

	/*
	 * allocate some virtual space
	 */

	if (uvm_map(map, &kva, size, uvm.kernel_object, UVM_UNKNOWN_OFFSET,
	      UVM_MAPFLAG(UVM_PROT_ALL, UVM_PROT_ALL, UVM_INH_NONE,
			  UVM_ADV_RANDOM, 0)) != KERN_SUCCESS) {
		UVMHIST_LOG(maphist,"<- done (no VM)",0,0,0,0);
		return(0);
	}

	/*
	 * recover object offset from virtual address
	 */

	offset = kva - vm_map_min(kernel_map);
	UVMHIST_LOG(maphist,"  kva=0x%x, offset=0x%x", kva, offset,0,0);

	/*
	 * now allocate the memory.  we must be careful about released pages.
	 */

	loopva = kva;
	while (size) {
		simple_lock(&uvm.kernel_object->vmobjlock);
		pg = uvm_pagelookup(uvm.kernel_object, offset);

		/*
		 * if we found a page in an unallocated region, it must be
		 * released
		 */
		if (pg) {
			if ((pg->flags & PG_RELEASED) == 0)
				panic("uvm_km_alloc1: non-released page");
			pg->flags |= PG_WANTED;
			UVM_UNLOCK_AND_WAIT(pg, &uvm.kernel_object->vmobjlock,
			    0, "km_alloc", 0);
			continue;   /* retry */
		}
		
		/* allocate ram */
		pg = uvm_pagealloc(uvm.kernel_object, offset, NULL);
		if (pg) {
			pg->flags &= ~PG_BUSY;	/* new page */
			UVM_PAGE_OWN(pg, NULL);
		}
		simple_unlock(&uvm.kernel_object->vmobjlock);
		if (pg == NULL) {
			uvm_wait("km_alloc1w");	/* wait for memory */
			continue;
		}
		
		/* map it in */
#if defined(PMAP_NEW)
		pmap_kenter_pa(loopva, VM_PAGE_TO_PHYS(pg), UVM_PROT_ALL);
#else
		pmap_enter(map->pmap, loopva, VM_PAGE_TO_PHYS(pg),
		    UVM_PROT_ALL, TRUE);
#endif
		loopva += PAGE_SIZE;
		offset += PAGE_SIZE;
		size -= PAGE_SIZE;
	}
	
	/*
	 * zero on request (note that "size" is now zero due to the above loop
	 * so we need to subtract kva from loopva to reconstruct the size).
	 */

	if (zeroit)
		bzero((caddr_t)kva, loopva - kva);

	UVMHIST_LOG(maphist,"<- done (kva=0x%x)", kva,0,0,0);
	return(kva);
}

/*
 * uvm_km_valloc: allocate zero-fill memory in the kernel's address space
 *
 * => memory is not allocated until fault time
 */

vaddr_t
uvm_km_valloc(map, size)
	vm_map_t map;
	vsize_t size;
{
	vaddr_t kva;
	UVMHIST_FUNC("uvm_km_valloc"); UVMHIST_CALLED(maphist);

	UVMHIST_LOG(maphist, "(map=0x%x, size=0x%x)", map, size, 0,0);

#ifdef DIAGNOSTIC
	if (vm_map_pmap(map) != pmap_kernel())
		panic("uvm_km_valloc");
#endif

	size = round_page(size);
	kva = vm_map_min(map);		/* hint */

	/*
	 * allocate some virtual space.  will be demand filled by kernel_object.
	 */

	if (uvm_map(map, &kva, size, uvm.kernel_object, UVM_UNKNOWN_OFFSET,
	    UVM_MAPFLAG(UVM_PROT_ALL, UVM_PROT_ALL, UVM_INH_NONE,
	    UVM_ADV_RANDOM, 0)) != KERN_SUCCESS) {
		UVMHIST_LOG(maphist, "<- done (no VM)", 0,0,0,0);
		return(0);
	}

	UVMHIST_LOG(maphist, "<- done (kva=0x%x)", kva,0,0,0);
	return(kva);
}

/*
 * uvm_km_valloc_wait: allocate zero-fill memory in the kernel's address space
 *
 * => memory is not allocated until fault time
 * => if no room in map, wait for space to free, unless requested size
 *    is larger than map (in which case we return 0)
 */

vaddr_t
uvm_km_valloc_wait(map, size)
	vm_map_t map;
	vsize_t size;
{
	vaddr_t kva;
	UVMHIST_FUNC("uvm_km_valloc_wait"); UVMHIST_CALLED(maphist);

	UVMHIST_LOG(maphist, "(map=0x%x, size=0x%x)", map, size, 0,0);

#ifdef DIAGNOSTIC
	if (vm_map_pmap(map) != pmap_kernel())
		panic("uvm_km_valloc_wait");
#endif

	size = round_page(size);
	if (size > vm_map_max(map) - vm_map_min(map))
		return(0);

	while (1) {
		kva = vm_map_min(map);		/* hint */

		/*
		 * allocate some virtual space.   will be demand filled
		 * by kernel_object.
		 */

		if (uvm_map(map, &kva, size, uvm.kernel_object,
		    UVM_UNKNOWN_OFFSET, UVM_MAPFLAG(UVM_PROT_ALL,
		    UVM_PROT_ALL, UVM_INH_NONE, UVM_ADV_RANDOM, 0))
		    == KERN_SUCCESS) {
			UVMHIST_LOG(maphist,"<- done (kva=0x%x)", kva,0,0,0);
			return(kva);
		}

		/*
		 * failed.  sleep for a while (on map)
		 */

		UVMHIST_LOG(maphist,"<<<sleeping>>>",0,0,0,0);
		tsleep((caddr_t)map, PVM, "vallocwait", 0);
	}
	/*NOTREACHED*/
}

/* Sanity; must specify both or none. */
#if (defined(PMAP_MAP_POOLPAGE) || defined(PMAP_UNMAP_POOLPAGE)) && \
    (!defined(PMAP_MAP_POOLPAGE) || !defined(PMAP_UNMAP_POOLPAGE))
#error Must specify MAP and UNMAP together.
#endif

/*
 * uvm_km_alloc_poolpage: allocate a page for the pool allocator
 *
 * => if the pmap specifies an alternate mapping method, we use it.
 */

/* ARGSUSED */
vaddr_t
uvm_km_alloc_poolpage1(map, obj, waitok)
	vm_map_t map;
	struct uvm_object *obj;
	boolean_t waitok;
{
#if defined(PMAP_MAP_POOLPAGE)
	struct vm_page *pg;
	vaddr_t va;

 again:
	pg = uvm_pagealloc(NULL, 0, NULL);
	if (pg == NULL) {
		if (waitok) {
			uvm_wait("plpg");
			goto again;
		} else
			return (0);
	}
	va = PMAP_MAP_POOLPAGE(VM_PAGE_TO_PHYS(pg));
	if (va == 0)
		uvm_pagefree(pg);
	return (va);
#else
	vaddr_t va;
	int s;

	/*
	 * NOTE: We may be called with a map that doens't require splimp
	 * protection (e.g. kernel_map).  However, it does not hurt to
	 * go to splimp in this case (since unprocted maps will never be
	 * accessed in interrupt context).
	 *
	 * XXX We may want to consider changing the interface to this
	 * XXX function.
	 */

	s = splimp();
	va = uvm_km_kmemalloc(map, obj, PAGE_SIZE, waitok ? 0 : UVM_KMF_NOWAIT);
	splx(s);
	return (va);
#endif /* PMAP_MAP_POOLPAGE */
}

/*
 * uvm_km_free_poolpage: free a previously allocated pool page
 *
 * => if the pmap specifies an alternate unmapping method, we use it.
 */

/* ARGSUSED */
void
uvm_km_free_poolpage1(map, addr)
	vm_map_t map;
	vaddr_t addr;
{
#if defined(PMAP_UNMAP_POOLPAGE)
	paddr_t pa;

	pa = PMAP_UNMAP_POOLPAGE(addr);
	uvm_pagefree(PHYS_TO_VM_PAGE(pa));
#else
	int s;

	/*
	 * NOTE: We may be called with a map that doens't require splimp
	 * protection (e.g. kernel_map).  However, it does not hurt to
	 * go to splimp in this case (since unprocted maps will never be
	 * accessed in interrupt context).
	 *
	 * XXX We may want to consider changing the interface to this
	 * XXX function.
	 */

	s = splimp();
	uvm_km_free(map, addr, PAGE_SIZE);
	splx(s);
#endif /* PMAP_UNMAP_POOLPAGE */
}
