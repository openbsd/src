/*	$OpenBSD: uvm_km.c,v 1.38 2004/04/28 02:20:58 markus Exp $	*/
/*	$NetBSD: uvm_km.c,v 1.42 2001/01/14 02:10:01 thorpej Exp $	*/

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
 *		by splvm() because we are allowed to call malloc()
 *		at interrupt time ***
 *   mb_map => memory for large mbufs,  *** protected by splvm ***
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
 * by splvm().    each of these submaps has their own private kernel 
 * object (e.g. kmem_object).
 *
 * note that just because a kernel object spans the entire kernel virtual
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
 * note that the offsets in kmem_object also follow this rule.
 * this means that the offsets for kmem_object must fall in the
 * range of [vm_map_min(kmem_object) - vm_map_min(kernel_map)] to
 * [vm_map_max(kmem_object) - vm_map_min(kernel_map)], so the offsets
 * in those objects will typically not start at zero.
 *
 * kernel objects have one other special property: when the kernel virtual
 * memory mapping them is unmapped, the backing memory in the object is
 * freed right away.   this is done with the uvm_km_pgremove() function.
 * this has to be done because there is no backing store for kernel pages
 * and no need to save them after they are no longer referenced.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/kthread.h>

#include <uvm/uvm.h>

/*
 * global data structures
 */

vm_map_t kernel_map = NULL;

struct vmi_list vmi_list;
simple_lock_data_t vmi_list_slock;

/*
 * local data structues
 */

static struct vm_map		kernel_map_store;
static struct uvm_object	kmem_object_store;

/*
 * All pager operations here are NULL, but the object must have
 * a pager ops vector associated with it; various places assume
 * it to be so.
 */
static struct uvm_pagerops	km_pager;

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
	 * first, initialize the interrupt-safe map list.
	 */
	LIST_INIT(&vmi_list);
	simple_lock_init(&vmi_list_slock);

	/*
	 * next, init kernel memory objects.
	 */

	/* kernel_object: for pageable anonymous kernel memory */
	uao_init();
	uvm.kernel_object = uao_create(VM_MAX_KERNEL_ADDRESS -
				 VM_MIN_KERNEL_ADDRESS, UAO_FLAG_KERNOBJ);

	/*
	 * kmem_object: for use by the kernel malloc().  Memory is always
	 * wired, and this object (and the kmem_map) can be accessed at
	 * interrupt time.
	 */
	simple_lock_init(&kmem_object_store.vmobjlock);
	kmem_object_store.pgops = &km_pager;
	TAILQ_INIT(&kmem_object_store.memq);
	kmem_object_store.uo_npages = 0;
	/* we are special.  we never die */
	kmem_object_store.uo_refs = UVM_OBJ_KERN_INTRSAFE; 
	uvmexp.kmem_object = &kmem_object_store;

	/*
	 * init the map and reserve already allocated kernel space 
	 * before installing.
	 */

	uvm_map_setup(&kernel_map_store, base, end, VM_MAP_PAGEABLE);
	kernel_map_store.pmap = pmap_kernel();
	if (base != start && uvm_map(&kernel_map_store, &base, start - base,
	    NULL, UVM_UNKNOWN_OFFSET, 0, UVM_MAPFLAG(UVM_PROT_ALL, UVM_PROT_ALL,
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
uvm_km_suballoc(map, min, max, size, flags, fixed, submap)
	struct vm_map *map;
	vaddr_t *min, *max;		/* OUT, OUT */
	vsize_t size;
	int flags;
	boolean_t fixed;
	struct vm_map *submap;
{
	int mapflags = UVM_FLAG_NOMERGE | (fixed ? UVM_FLAG_FIXED : 0);

	size = round_page(size);	/* round up to pagesize */

	/*
	 * first allocate a blank spot in the parent map
	 */

	if (uvm_map(map, min, size, NULL, UVM_UNKNOWN_OFFSET, 0,
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
		submap = uvm_map_create(vm_map_pmap(map), *min, *max, flags);
		if (submap == NULL)
			panic("uvm_km_suballoc: unable to create submap");
	} else {
		uvm_map_setup(submap, *min, *max, flags);
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
	boolean_t by_list;
	struct vm_page *pp, *ppnext;
	vaddr_t curoff;
	UVMHIST_FUNC("uvm_km_pgremove"); UVMHIST_CALLED(maphist);

	KASSERT(uobj->pgops == &aobj_pager);
	simple_lock(&uobj->vmobjlock);

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
		if (pp->flags & PG_BUSY) {
			/* owner must check for this when done */
			pp->flags |= PG_RELEASED;
		} else {
			/* free the swap slot... */
			uao_dropswap(uobj, curoff >> PAGE_SHIFT);

			/*
			 * ...and free the page; note it may be on the
			 * active or inactive queues.
			 */
			uvm_lock_pageq();
			uvm_pagefree(pp);
			uvm_unlock_pageq();
		}
	}
	simple_unlock(&uobj->vmobjlock);
	return;

loop_by_list:

	for (pp = TAILQ_FIRST(&uobj->memq); pp != NULL; pp = ppnext) {
		ppnext = TAILQ_NEXT(pp, listq);
		if (pp->offset < start || pp->offset >= end) {
			continue;
		}

		UVMHIST_LOG(maphist,"  page 0x%x, busy=%d", pp,
		    pp->flags & PG_BUSY, 0, 0);

		if (pp->flags & PG_BUSY) {
			/* owner must check for this when done */
			pp->flags |= PG_RELEASED;
		} else {
			/* free the swap slot... */
			uao_dropswap(uobj, pp->offset >> PAGE_SHIFT);

			/*
			 * ...and free the page; note it may be on the
			 * active or inactive queues.
			 */
			uvm_lock_pageq();
			uvm_pagefree(pp);
			uvm_unlock_pageq();
		}
	}
	simple_unlock(&uobj->vmobjlock);
}


/*
 * uvm_km_pgremove_intrsafe: like uvm_km_pgremove(), but for "intrsafe"
 *    objects
 *
 * => when you unmap a part of anonymous kernel memory you want to toss
 *    the pages right away.    (this gets called from uvm_unmap_...).
 * => none of the pages will ever be busy, and none of them will ever
 *    be on the active or inactive queues (because these objects are
 *    never allowed to "page").
 */

void
uvm_km_pgremove_intrsafe(uobj, start, end)
	struct uvm_object *uobj;
	vaddr_t start, end;
{
	boolean_t by_list;
	struct vm_page *pp, *ppnext;
	vaddr_t curoff;
	UVMHIST_FUNC("uvm_km_pgremove_intrsafe"); UVMHIST_CALLED(maphist);

	KASSERT(UVM_OBJ_IS_INTRSAFE_OBJECT(uobj));
	simple_lock(&uobj->vmobjlock);		/* lock object */

	/* choose cheapest traversal */
	by_list = (uobj->uo_npages <=
	     ((end - start) >> PAGE_SHIFT) * UKM_HASH_PENALTY);
 
	if (by_list)
		goto loop_by_list;

	/* by hash */

	for (curoff = start ; curoff < end ; curoff += PAGE_SIZE) {
		pp = uvm_pagelookup(uobj, curoff);
		if (pp == NULL) {
			continue;
		}

		UVMHIST_LOG(maphist,"  page 0x%x, busy=%d", pp,
		    pp->flags & PG_BUSY, 0, 0);
		KASSERT((pp->flags & PG_BUSY) == 0);
		KASSERT((pp->pqflags & PQ_ACTIVE) == 0);
		KASSERT((pp->pqflags & PQ_INACTIVE) == 0);
		uvm_pagefree(pp);
	}
	simple_unlock(&uobj->vmobjlock);
	return;

loop_by_list:

	for (pp = TAILQ_FIRST(&uobj->memq); pp != NULL; pp = ppnext) {
		ppnext = TAILQ_NEXT(pp, listq);
		if (pp->offset < start || pp->offset >= end) {
			continue;
		}

		UVMHIST_LOG(maphist,"  page 0x%x, busy=%d", pp,
		    pp->flags & PG_BUSY, 0, 0);
		KASSERT((pp->flags & PG_BUSY) == 0);
		KASSERT((pp->pqflags & PQ_ACTIVE) == 0);
		KASSERT((pp->pqflags & PQ_INACTIVE) == 0);
		uvm_pagefree(pp);
	}
	simple_unlock(&uobj->vmobjlock);
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
	KASSERT(vm_map_pmap(map) == pmap_kernel());

	/*
	 * setup for call
	 */

	size = round_page(size);
	kva = vm_map_min(map);	/* hint */

	/*
	 * allocate some virtual space
	 */

	if (__predict_false(uvm_map(map, &kva, size, obj, UVM_UNKNOWN_OFFSET,
	      0, UVM_MAPFLAG(UVM_PROT_RW, UVM_PROT_RW, UVM_INH_NONE,
			  UVM_ADV_RANDOM, (flags & UVM_KMF_TRYLOCK))) 
			!= KERN_SUCCESS)) {
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
		pg = uvm_pagealloc(obj, offset, NULL, 0);
		if (pg) {
			pg->flags &= ~PG_BUSY;	/* new page */
			UVM_PAGE_OWN(pg, NULL);
		}
		simple_unlock(&obj->vmobjlock);
		
		/*
		 * out of memory?
		 */

		if (__predict_false(pg == NULL)) {
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

		if (UVM_OBJ_IS_INTRSAFE_OBJECT(obj)) {
			pmap_kenter_pa(loopva, VM_PAGE_TO_PHYS(pg),
			    UVM_PROT_RW);
		} else {
			pmap_enter(map->pmap, loopva, VM_PAGE_TO_PHYS(pg),
			    UVM_PROT_RW,
			    PMAP_WIRED | VM_PROT_READ | VM_PROT_WRITE);
		}
		loopva += PAGE_SIZE;
		offset += PAGE_SIZE;
		size -= PAGE_SIZE;
	}
	pmap_update(pmap_kernel());

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
	uvm_unmap_remove(map, trunc_page(addr), round_page(addr+size), 
			 &dead_entries);
	wakeup(map);
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
	KASSERT(vm_map_pmap(map) == pmap_kernel());

	size = round_page(size);
	kva = vm_map_min(map);		/* hint */

	/*
	 * allocate some virtual space
	 */

	if (__predict_false(uvm_map(map, &kva, size, uvm.kernel_object,
	      UVM_UNKNOWN_OFFSET, 0, UVM_MAPFLAG(UVM_PROT_ALL, UVM_PROT_ALL,
					      UVM_INH_NONE, UVM_ADV_RANDOM,
					      0)) != KERN_SUCCESS)) {
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
			    FALSE, "km_alloc", 0);
			continue;   /* retry */
		}
		
		/* allocate ram */
		pg = uvm_pagealloc(uvm.kernel_object, offset, NULL, 0);
		if (pg) {
			pg->flags &= ~PG_BUSY;	/* new page */
			UVM_PAGE_OWN(pg, NULL);
		}
		simple_unlock(&uvm.kernel_object->vmobjlock);
		if (__predict_false(pg == NULL)) {
			uvm_wait("km_alloc1w");	/* wait for memory */
			continue;
		}
		
		/*
		 * map it in; note we're never called with an intrsafe
		 * object, so we always use regular old pmap_enter().
		 */
		pmap_enter(map->pmap, loopva, VM_PAGE_TO_PHYS(pg),
		    UVM_PROT_ALL, PMAP_WIRED | VM_PROT_READ | VM_PROT_WRITE);

		loopva += PAGE_SIZE;
		offset += PAGE_SIZE;
		size -= PAGE_SIZE;
	}
	pmap_update(map->pmap);
	
	/*
	 * zero on request (note that "size" is now zero due to the above loop
	 * so we need to subtract kva from loopva to reconstruct the size).
	 */

	if (zeroit)
		memset((caddr_t)kva, 0, loopva - kva);

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
	return(uvm_km_valloc_align(map, size, 0));
}

vaddr_t
uvm_km_valloc_align(map, size, align)
	vm_map_t map;
	vsize_t size;
	vsize_t align;
{
	vaddr_t kva;
	UVMHIST_FUNC("uvm_km_valloc"); UVMHIST_CALLED(maphist);

	UVMHIST_LOG(maphist, "(map=0x%x, size=0x%x)", map, size, 0,0);
	KASSERT(vm_map_pmap(map) == pmap_kernel());

	size = round_page(size);
	kva = vm_map_min(map);		/* hint */

	/*
	 * allocate some virtual space.  will be demand filled by kernel_object.
	 */

	if (__predict_false(uvm_map(map, &kva, size, uvm.kernel_object,
	    UVM_UNKNOWN_OFFSET, align, UVM_MAPFLAG(UVM_PROT_ALL, UVM_PROT_ALL,
					    UVM_INH_NONE, UVM_ADV_RANDOM,
					    0)) != KERN_SUCCESS)) {
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
uvm_km_valloc_prefer_wait(map, size, prefer)
	vm_map_t map;
	vsize_t size;
	voff_t prefer;
{
	vaddr_t kva;
	UVMHIST_FUNC("uvm_km_valloc_prefer_wait"); UVMHIST_CALLED(maphist);

	UVMHIST_LOG(maphist, "(map=0x%x, size=0x%x)", map, size, 0,0);
	KASSERT(vm_map_pmap(map) == pmap_kernel());

	size = round_page(size);
	if (size > vm_map_max(map) - vm_map_min(map))
		return(0);

	while (1) {
		kva = vm_map_min(map);		/* hint */

		/*
		 * allocate some virtual space.   will be demand filled
		 * by kernel_object.
		 */

		if (__predict_true(uvm_map(map, &kva, size, uvm.kernel_object,
		    prefer, 0, UVM_MAPFLAG(UVM_PROT_ALL,
		    UVM_PROT_ALL, UVM_INH_NONE, UVM_ADV_RANDOM, 0))
		    == KERN_SUCCESS)) {
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

vaddr_t
uvm_km_valloc_wait(map, size)
	vm_map_t map;
	vsize_t size;
{
	return uvm_km_valloc_prefer_wait(map, size, UVM_UNKNOWN_OFFSET);
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
	pg = uvm_pagealloc(NULL, 0, NULL, UVM_PGA_USERESERVE);
	if (__predict_false(pg == NULL)) {
		if (waitok) {
			uvm_wait("plpg");
			goto again;
		} else
			return (0);
	}
	va = PMAP_MAP_POOLPAGE(pg);
	if (__predict_false(va == 0))
		uvm_pagefree(pg);
	return (va);
#else
	vaddr_t va;
	int s;

	/*
	 * NOTE: We may be called with a map that doens't require splvm
	 * protection (e.g. kernel_map).  However, it does not hurt to
	 * go to splvm in this case (since unprocted maps will never be
	 * accessed in interrupt context).
	 *
	 * XXX We may want to consider changing the interface to this
	 * XXX function.
	 */

	s = splvm();
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
	uvm_pagefree(PMAP_UNMAP_POOLPAGE(addr));
#else
	int s;

	/*
	 * NOTE: We may be called with a map that doens't require splvm
	 * protection (e.g. kernel_map).  However, it does not hurt to
	 * go to splvm in this case (since unprocted maps will never be
	 * accessed in interrupt context).
	 *
	 * XXX We may want to consider changing the interface to this
	 * XXX function.
	 */

	s = splvm();
	uvm_km_free(map, addr, PAGE_SIZE);
	splx(s);
#endif /* PMAP_UNMAP_POOLPAGE */
}

int uvm_km_pages_lowat = 128;
int uvm_km_pages_free;
struct km_page {
	struct km_page *next;
} *uvm_km_pages_head;

void uvm_km_createthread(void *);
void uvm_km_thread(void *);

void
uvm_km_page_init(void)
{
	struct km_page *head, *page;
	int i;


	head = NULL;
	for (i = 0; i < uvm_km_pages_lowat * 4; i++) {
#if defined(PMAP_MAP_POOLPAGE)
		struct vm_page *pg;
		vaddr_t va;

		pg = uvm_pagealloc(NULL, 0, NULL, 0);
		if (__predict_false(pg == NULL))
			break;

		va = PMAP_MAP_POOLPAGE(pg);
		if (__predict_false(va == 0)) {
			uvm_pagefree(pg);
			break;
		}
		page = (void *)va;
#else
		page = (void *)uvm_km_alloc(kernel_map, PAGE_SIZE);
#endif
		page->next = head;
		head = page;
	}
	uvm_km_pages_head = head;
	uvm_km_pages_free = i;

	kthread_create_deferred(uvm_km_createthread, NULL);
}

void
uvm_km_createthread(void *arg)
{
	kthread_create(uvm_km_thread, NULL, NULL, "kmthread");
}

void
uvm_km_thread(void *arg)
{
	struct km_page *head, *tail, *page;
	int i, s, want;

	for (;;) {
		if (uvm_km_pages_free >= uvm_km_pages_lowat)
			tsleep(&uvm_km_pages_head, PVM, "kmalloc", 0);
		want = uvm_km_pages_lowat - uvm_km_pages_free;
		if (want < 16)
			want = 16;
		for (i = 0; i < want; i++) {
#if defined(PMAP_MAP_POOLPAGE)
			struct vm_page *pg;
			vaddr_t va;
	
			pg = uvm_pagealloc(NULL, 0, NULL, UVM_PGA_USERESERVE);
			if (__predict_false(pg == NULL))
				break;
			va = PMAP_MAP_POOLPAGE(pg);
			if (__predict_false(va == 0)) {
				uvm_pagefree(pg);
				break;
			}
			page = (void *)va;
#else
			page = (void *)uvm_km_alloc(kernel_map, PAGE_SIZE);
#endif
			if (i == 0)
				head = tail = page;
			page->next = head;
			head = page;
		}
		s = splvm();
		tail->next = uvm_km_pages_head;
		uvm_km_pages_head = head;
		uvm_km_pages_free += i;
		splx(s);
	}
}


void *
uvm_km_getpage(void)
{
	struct km_page *page;
	int s;

	s = splvm();
	page = uvm_km_pages_head;
	if (page) {
		uvm_km_pages_head = page->next;
		uvm_km_pages_free--;
	}
	splx(s);
	if (uvm_km_pages_free < uvm_km_pages_lowat)
		wakeup(&uvm_km_pages_head);
	return (page);
}

void
uvm_km_putpage(void *v)
{
	struct km_page *page = v;
	int s;

	s = splvm();
	page->next = uvm_km_pages_head;
	uvm_km_pages_head = page;
	uvm_km_pages_free++;
	splx(s);
}
