/*	$OpenBSD: vm_page.c,v 1.10 1998/02/06 08:32:47 niklas Exp $    */
/*	$NetBSD: vm_page.c,v 1.31 1997/06/06 23:10:23 thorpej Exp $	*/

#define	VM_PAGE_ALLOC_MEMORY_STATS

/*-
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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
 * @(#)vm_page.c	8.3 (Berkeley) 3/21/94
 *
 *
 * Copyright (c) 1987, 1990 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Authors: Avadis Tevanian, Jr., Michael Wayne Young
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
 * Resident memory management module.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>

#include <vm/vm.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_pageout.h>

#include <machine/cpu.h>

#ifdef MACHINE_NONCONTIG
/*
 * These variables record the values returned by vm_page_bootstrap,
 * for debugging purposes.  The implementation of pmap_steal_memory
 * and pmap_startup here also uses them internally.
 */
vm_offset_t	virtual_space_start;
vm_offset_t	virtual_space_end;
#endif /* MACHINE_NONCONTIG */

/*
 * Associated with page of user-allocatable memory is a
 * page structure.
 */

struct pglist	*vm_page_buckets;		/* Array of buckets */
int		vm_page_bucket_count = 0;	/* How big is array? */
int		vm_page_hash_mask;		/* Mask for hash function */
simple_lock_data_t	bucket_lock;		/* lock for all buckets XXX */

struct pglist	vm_page_queue_free;
struct pglist	vm_page_queue_active;
struct pglist	vm_page_queue_inactive;
simple_lock_data_t	vm_page_queue_lock;
simple_lock_data_t	vm_page_queue_free_lock;

/* has physical page allocation been initialized? */
boolean_t vm_page_startup_initialized;

vm_page_t	vm_page_array;
int		vm_page_count;
#ifndef MACHINE_NONCONTIG
long		first_page;
long		last_page;
vm_offset_t	first_phys_addr;
vm_offset_t	last_phys_addr;
#else
u_long		first_page;
#endif /* MACHINE_NONCONTIG */
vm_size_t	page_mask;
int		page_shift;

/*
 * vm_set_page_size:
 *
 * Sets the page size, perhaps based upon the memory
 * size.  Must be called before any use of page-size
 * dependent functions.
 *
 * Sets page_shift and page_mask from cnt.v_page_size.
 */
void
vm_set_page_size()
{

	if (cnt.v_page_size == 0)
		cnt.v_page_size = DEFAULT_PAGE_SIZE;
	page_mask = cnt.v_page_size - 1;
	if ((page_mask & cnt.v_page_size) != 0)
		panic("vm_set_page_size: page size not a power of two");
	for (page_shift = 0; ; page_shift++)
		if ((1 << page_shift) == cnt.v_page_size)
			break;
}


#ifdef	MACHINE_NONCONTIG
/*
 * vm_page_bootstrap:
 *
 * Initializes the resident memory module.
 *
 * Allocates memory for the page cells, and
 * for the object/offset-to-page hash table headers.
 * Each page cell is initialized and placed on the free list.
 * Returns the range of available kernel virtual memory.
 */
void
vm_page_bootstrap(startp, endp)
	vm_offset_t	*startp;
	vm_offset_t	*endp;
{
	int			i;
	register struct pglist	*bucket;
	
	extern	vm_offset_t	kentry_data;
	extern	vm_size_t	kentry_data_size;

	/*
	 * Initialize the locks
	 */
	simple_lock_init(&vm_page_queue_free_lock);
	simple_lock_init(&vm_page_queue_lock);

	/*
	 * Initialize the queue headers for the free queue,
	 * the active queue and the inactive queue.
	 */
	TAILQ_INIT(&vm_page_queue_free);
	TAILQ_INIT(&vm_page_queue_active);
	TAILQ_INIT(&vm_page_queue_inactive);

	/*
	 * Pre-allocate maps and map entries that cannot be dynamically
	 * allocated via malloc().  The maps include the kernel_map and
	 * kmem_map which must be initialized before malloc() will
	 * work (obviously).  Also could include pager maps which would
	 * be allocated before kmeminit.
	 *
	 * Allow some kernel map entries... this should be plenty
	 * since people shouldn't be cluttering up the kernel
	 * map (they should use their own maps).
	 */
	kentry_data_size = round_page(MAX_KMAP*sizeof(struct vm_map) +
	    MAX_KMAPENT*sizeof(struct vm_map_entry));
	kentry_data = (vm_offset_t)pmap_steal_memory(kentry_data_size);
	
	/*
	 * Validate these zone addresses.
	 */
	bzero((caddr_t)kentry_data, kentry_data_size);

	/*
	 * Allocate (and initialize) the virtual-to-physical
	 * table hash buckets.
	 *
	 * The number of buckets MUST BE a power of 2, and
	 * the actual value is the next power of 2 greater
	 * than the number of physical pages in the system.
	 *
	 * Note:
	 * 	This computation can be tweaked if desired.
	 */
	if (vm_page_bucket_count == 0) {
		unsigned int npages = pmap_free_pages();
	    
		vm_page_bucket_count = 1;
		while (vm_page_bucket_count < npages)
			vm_page_bucket_count <<= 1;
	}

	vm_page_hash_mask = vm_page_bucket_count - 1;

	vm_page_buckets = (struct pglist *)
	    pmap_steal_memory(vm_page_bucket_count * sizeof(*vm_page_buckets));
	bucket = vm_page_buckets;

	for (i = vm_page_bucket_count; i--;) {
		TAILQ_INIT(bucket);
		bucket++;
	}

	simple_lock_init(&bucket_lock);

	/*
	 * Machine-dependent code allocates the resident page table.
	 * It uses VM_PAGE_INIT to initialize the page frames.
	 * The code also returns to us the virtual space available
	 * to the kernel.  We don't trust the pmap module
	 * to get the alignment right.
	 */
	pmap_startup(&virtual_space_start, &virtual_space_end);
	virtual_space_start = round_page(virtual_space_start);
	virtual_space_end = trunc_page(virtual_space_end);
	
	*startp = virtual_space_start;
	*endp = virtual_space_end;
	
	simple_lock_init(&vm_pages_needed_lock);
}

#else	/* MACHINE_NONCONTIG */

/*
 * vm_page_startup:
 *
 * Initializes the resident memory module.
 *
 * Allocates memory for the page cells, and
 * for the object/offset-to-page hash table headers.
 * Each page cell is initialized and placed on the free list.
 */
void
vm_page_startup(start, end)
	vm_offset_t	*start;
	vm_offset_t	*end;
{
	register vm_page_t	m;
	register struct pglist	*bucket;
	int			npages;
	int			i;
	vm_offset_t		pa;
	extern	vm_offset_t	kentry_data;
	extern	vm_size_t	kentry_data_size;

	/*
	 * Initialize the locks
	 */
	simple_lock_init(&vm_page_queue_free_lock);
	simple_lock_init(&vm_page_queue_lock);

	/*
	 * Initialize the queue headers for the free queue,
	 * the active queue and the inactive queue.
	 */
	TAILQ_INIT(&vm_page_queue_free);
	TAILQ_INIT(&vm_page_queue_active);
	TAILQ_INIT(&vm_page_queue_inactive);

	/*
	 * Calculate the number of hash table buckets.
	 *
	 * The number of buckets MUST BE a power of 2, and
	 * the actual value is the next power of 2 greater
	 * than the number of physical pages in the system.
	 *
	 * Note:
	 * 	This computation can be tweaked if desired.
	 */
	if (vm_page_bucket_count == 0) {
		vm_page_bucket_count = 1;
		while (vm_page_bucket_count < atop(*end - *start))
			vm_page_bucket_count <<= 1;
	}

	vm_page_hash_mask = vm_page_bucket_count - 1;

	/*
	 * Allocate (and initialize) the hash table buckets.
	 */
	vm_page_buckets = (struct pglist *)
	    pmap_bootstrap_alloc(vm_page_bucket_count * sizeof(struct pglist));
	bucket = vm_page_buckets;

	for (i = vm_page_bucket_count; i--;) {
		TAILQ_INIT(bucket);
		bucket++;
	}

	simple_lock_init(&bucket_lock);

	/*
	 * Truncate the remainder of physical memory to our page size.
	 */
	*end = trunc_page(*end);

	/*
	 * Pre-allocate maps and map entries that cannot be dynamically
	 * allocated via malloc().  The maps include the kernel_map and
	 * kmem_map which must be initialized before malloc() will
	 * work (obviously).  Also could include pager maps which would
	 * be allocated before kmeminit.
	 *
	 * Allow some kernel map entries... this should be plenty
	 * since people shouldn't be cluttering up the kernel
	 * map (they should use their own maps).
	 */
	kentry_data_size = round_page(MAX_KMAP*sizeof(struct vm_map) +
	    MAX_KMAPENT*sizeof(struct vm_map_entry));
	kentry_data = (vm_offset_t) pmap_bootstrap_alloc(kentry_data_size);

	/*
 	 * Compute the number of pages of memory that will be
	 * available for use (taking into account the overhead
	 * of a page structure per page).
	 */
	cnt.v_free_count = vm_page_count = (*end - *start +
	     sizeof(struct vm_page)) / (PAGE_SIZE + sizeof(struct vm_page));

	/*
	 * Record the extent of physical memory that the
	 * virtual memory system manages.
	 */
	first_page = *start;
	first_page += vm_page_count * sizeof(struct vm_page);
	first_page = atop(round_page(first_page));
	last_page = first_page + vm_page_count - 1;

	first_phys_addr = ptoa(first_page);
	last_phys_addr = ptoa(last_page) + PAGE_MASK;

	/*
	 * Allocate and clear the mem entry structures.
	 */
	m = vm_page_array = (vm_page_t)
	    pmap_bootstrap_alloc(vm_page_count * sizeof(struct vm_page));
	bzero(vm_page_array, vm_page_count * sizeof(struct vm_page));

	/*
	 * Initialize the mem entry structures now, and
	 * put them in the free queue.
	 */
	pa = first_phys_addr;
	npages = vm_page_count;
	while (npages--) {
		m->flags = PG_FREE;
		m->object = NULL;
		m->phys_addr = pa;
		TAILQ_INSERT_TAIL(&vm_page_queue_free, m, pageq);
		m++;
		pa += PAGE_SIZE;
	}

	/*
	 * Initialize vm_pages_needed lock here - don't wait for pageout
	 * daemon	XXX
	 */
	simple_lock_init(&vm_pages_needed_lock);

	/* from now on, pmap_bootstrap_alloc can't be used */
	vm_page_startup_initialized = TRUE;
}
#endif /* MACHINE_NONCONTIG */

#if	defined(MACHINE_NONCONTIG) && !defined(MACHINE_PAGES)
/*
 * We implement pmap_steal_memory and pmap_startup with the help
 * of two simpler functions, pmap_virtual_space and pmap_next_page.
 */
vm_offset_t
pmap_steal_memory(size)
	vm_size_t	size;
{
	vm_offset_t	addr, vaddr, paddr;

#ifdef i386	/* XXX i386 calls pmap_steal_memory before vm_mem_init() */
	if (cnt.v_page_size == 0)		/* XXX */
		vm_set_page_size();
#endif

	/*
	 * We round the size to an integer multiple.
	 */
	size = (size + 3) &~ 3; /* XXX */
	
	/*
	 * If this is the first call to pmap_steal_memory,
	 * we have to initialize ourself.
	 */
	if (virtual_space_start == virtual_space_end) {
		pmap_virtual_space(&virtual_space_start, &virtual_space_end);
		
		/*
		 * The initial values must be aligned properly, and
		 * we don't trust the pmap module to do it right.
		 */
		virtual_space_start = round_page(virtual_space_start);
		virtual_space_end = trunc_page(virtual_space_end);
	}
	
	/*
	 * Allocate virtual memory for this request.
	 */
	addr = virtual_space_start;
	virtual_space_start += size;
	
	/*
	 * Allocate and map physical pages to back new virtual pages.
	 */
	for (vaddr = round_page(addr); vaddr < addr + size;
	    vaddr += PAGE_SIZE) {
		if (!pmap_next_page(&paddr))
			panic("pmap_steal_memory");
		
		/*
		 * XXX Logically, these mappings should be wired,
		 * but some pmap modules barf if they are.
		 */
		pmap_enter(pmap_kernel(), vaddr, paddr,
		    VM_PROT_READ|VM_PROT_WRITE, FALSE);
	}
	
	return addr;
}

void
pmap_startup(startp, endp)
	vm_offset_t	*startp;
	vm_offset_t	*endp;
{
	unsigned int	i, freepages;
	vm_offset_t	paddr;
	
	/*
	 * We calculate how many page frames we will have
	 * and then allocate the page structures in one chunk.
	 * The calculation is non-trivial.  We want:
	 *
	 * vmpages > (freepages - (vmpages / sizeof(vm_page_t)))
	 *
	 * which, with some algebra, becomes:
	 *
	 * vmpages > (freepages * sizeof(...) / (1 + sizeof(...)))
	 *
	 * The value of vm_page_count need not be exact, but must be
	 * large enough so vm_page_array handles the index range.
	 */
	freepages = pmap_free_pages();
	/* Fudge slightly to deal with truncation error. */
	freepages += 1;	/* fudge */

	vm_page_count = (PAGE_SIZE * freepages) /
	    (PAGE_SIZE + sizeof(*vm_page_array));

	vm_page_array = (vm_page_t)
	    pmap_steal_memory(vm_page_count * sizeof(*vm_page_array));
	bzero(vm_page_array, vm_page_count * sizeof(*vm_page_array));

#ifdef	DIAGNOSTIC
	/*
	 * Initialize everyting in case the holes are stepped in,
	 * and set PA to something that will cause a panic...
	 */
	for (i = 0; i < vm_page_count; i++)
		vm_page_array[i].phys_addr = 0xdeadbeef;
#endif

	/*
	 * Initialize the page frames.
	 * Note that some page indices may not be usable
	 * when pmap_free_pages() counts pages in a hole.
	 */
	if (!pmap_next_page(&paddr))
		panic("pmap_startup: can't get first page");
	first_page = pmap_page_index(paddr);
	i = 0;
	for (;;) {
		/* Initialize a page array element. */
		VM_PAGE_INIT(&vm_page_array[i], NULL, 0);
		vm_page_array[i].phys_addr = paddr;
		vm_page_free(&vm_page_array[i]);

		/* Are there more physical pages? */
		if (!pmap_next_page(&paddr))
			break;
		i = pmap_page_index(paddr) - first_page;

		/* Don't trust pmap_page_index()... */
		if (
#if 0
		    /* Cannot happen; i is unsigned */
		    i < 0 ||
#endif
		    i >= vm_page_count)
			panic("pmap_startup: bad i=0x%x", i);
	}

	*startp = virtual_space_start;
	*endp = virtual_space_end;
}
#endif /* MACHINE_NONCONTIG && !MACHINE_PAGES */

/*
 * vm_page_hash:
 *
 * Distributes the object/offset key pair among hash buckets.
 *
 * NOTE:  This macro depends on vm_page_bucket_count being a power of 2.
 */
#define vm_page_hash(object, offset) \
	(((unsigned long)object+(unsigned long)atop(offset))&vm_page_hash_mask)

/*
 * vm_page_insert:		[ internal use only ]
 *
 * Inserts the given mem entry into the object/object-page
 * table and object list.
 *
 * The object and page must be locked.
 */
void
vm_page_insert(mem, object, offset)
	register vm_page_t	mem;
	register vm_object_t	object;
	register vm_offset_t	offset;
{
	register struct pglist	*bucket;
	int			spl;

	VM_PAGE_CHECK(mem);

	if (mem->flags & PG_TABLED)
		panic("vm_page_insert: already inserted");

	/*
	 * Record the object/offset pair in this page
	 */
	mem->object = object;
	mem->offset = offset;

	/*
	 * Insert it into the object_object/offset hash table
	 */
	bucket = &vm_page_buckets[vm_page_hash(object, offset)];
	spl = splimp();
	simple_lock(&bucket_lock);
	TAILQ_INSERT_TAIL(bucket, mem, hashq);
	simple_unlock(&bucket_lock);
	(void)splx(spl);

	/*
	 * Now link into the object's list of backed pages.
	 */
	TAILQ_INSERT_TAIL(&object->memq, mem, listq);
	mem->flags |= PG_TABLED;

	/*
	 * And show that the object has one more resident
	 * page.
	 */
	object->resident_page_count++;
}

/*
 * vm_page_remove:		[ internal use only ]
 * XXX: used by device pager as well
 *
 * Removes the given mem entry from the object/offset-page
 * table and the object page list.
 *
 * The object and page must be locked.
 */
void
vm_page_remove(mem)
	register vm_page_t	mem;
{
	register struct pglist	*bucket;
	int			spl;

	VM_PAGE_CHECK(mem);

#ifdef DIAGNOSTIC
	if (mem->flags & PG_FAULTING)
		panic("vm_page_remove: page is faulting");
#endif

	if (!(mem->flags & PG_TABLED))
		return;

	/*
	 * Remove from the object_object/offset hash table
	 */
	bucket = &vm_page_buckets[vm_page_hash(mem->object, mem->offset)];
	spl = splimp();
	simple_lock(&bucket_lock);
	TAILQ_REMOVE(bucket, mem, hashq);
	simple_unlock(&bucket_lock);
	(void) splx(spl);

	/*
	 * Now remove from the object's list of backed pages.
	 */
	TAILQ_REMOVE(&mem->object->memq, mem, listq);

	/*
	 * And show that the object has one fewer resident
	 * page.
	 */
	mem->object->resident_page_count--;

	mem->flags &= ~PG_TABLED;
}

/*
 * vm_page_lookup:
 *
 * Returns the page associated with the object/offset
 * pair specified; if none is found, NULL is returned.
 *
 * The object must be locked.  No side effects.
 */
vm_page_t
vm_page_lookup(object, offset)
	register vm_object_t	object;
	register vm_offset_t	offset;
{
	register vm_page_t	mem;
	register struct pglist	*bucket;
	int			spl;

	/*
	 * Search the hash table for this object/offset pair
	 */
	bucket = &vm_page_buckets[vm_page_hash(object, offset)];

	spl = splimp();
	simple_lock(&bucket_lock);
	for (mem = bucket->tqh_first; mem != NULL; mem = mem->hashq.tqe_next) {
		VM_PAGE_CHECK(mem);
		if ((mem->object == object) && (mem->offset == offset)) {
			simple_unlock(&bucket_lock);
			splx(spl);
			return(mem);
		}
	}

	simple_unlock(&bucket_lock);
	splx(spl);
	return(NULL);
}

/*
 * vm_page_rename:
 *
 * Move the given memory entry from its
 * current object to the specified target object/offset.
 *
 * The object must be locked.
 */
void
vm_page_rename(mem, new_object, new_offset)
	register vm_page_t	mem;
	register vm_object_t	new_object;
	vm_offset_t		new_offset;
{
	if (mem->object == new_object)
		return;

	/* Keep page from moving out from under pageout daemon */
	vm_page_lock_queues();

	vm_page_remove(mem);
	vm_page_insert(mem, new_object, new_offset);
	vm_page_unlock_queues();
}

/*
 * vm_page_alloc:
 *
 * Allocate and return a memory cell associated
 * with this VM object/offset pair.
 *
 * Object must be locked.
 */
vm_page_t
vm_page_alloc(object, offset)
	vm_object_t	object;
	vm_offset_t	offset;
{
	register vm_page_t	mem;
	int		spl;

	spl = splimp();				/* XXX */
	simple_lock(&vm_page_queue_free_lock);
	if (vm_page_queue_free.tqh_first == NULL) {
		simple_unlock(&vm_page_queue_free_lock);
		splx(spl);
		return(NULL);
	}

	mem = vm_page_queue_free.tqh_first;
	TAILQ_REMOVE(&vm_page_queue_free, mem, pageq);

	cnt.v_free_count--;
	simple_unlock(&vm_page_queue_free_lock);
	splx(spl);

	VM_PAGE_INIT(mem, object, offset);

	/*
	 * Decide if we should poke the pageout daemon.
	 * We do this if the free count is less than the low
	 * water mark, or if the free count is less than the high
	 * water mark (but above the low water mark) and the inactive
	 * count is less than its target.
	 *
	 * We don't have the counts locked ... if they change a little,
	 * it doesn't really matter.
	 */
	if (cnt.v_free_count < cnt.v_free_min ||
	    (cnt.v_free_count < cnt.v_free_target &&
	    cnt.v_inactive_count < cnt.v_inactive_target))
		thread_wakeup(&vm_pages_needed);
	return (mem);
}

/*
 * vm_page_free:
 *
 * Returns the given page to the free list,
 * disassociating it with any VM object.
 *
 * Object and page must be locked prior to entry.
 */
void
vm_page_free(mem)
	register vm_page_t	mem;
{
	vm_page_remove(mem);
	if (mem->flags & PG_ACTIVE) {
		TAILQ_REMOVE(&vm_page_queue_active, mem, pageq);
		mem->flags &= ~PG_ACTIVE;
		cnt.v_active_count--;
	}

	if (mem->flags & PG_INACTIVE) {
		TAILQ_REMOVE(&vm_page_queue_inactive, mem, pageq);
		mem->flags &= ~PG_INACTIVE;
		cnt.v_inactive_count--;
	}

	if (!(mem->flags & PG_FICTITIOUS)) {
		int	spl;

		spl = splimp();
		simple_lock(&vm_page_queue_free_lock);
		mem->flags |= PG_FREE;
		TAILQ_INSERT_TAIL(&vm_page_queue_free, mem, pageq);

		cnt.v_free_count++;
		simple_unlock(&vm_page_queue_free_lock);
		splx(spl);
	}
}

/*
 * vm_page_wire:
 *
 * Mark this page as wired down by yet
 * another map, removing it from paging queues
 * as necessary.
 *
 * The page queues must be locked.
 */
void
vm_page_wire(mem)
	register vm_page_t	mem;
{
	VM_PAGE_CHECK(mem);

	if (mem->wire_count == 0) {
		if (mem->flags & PG_ACTIVE) {
			TAILQ_REMOVE(&vm_page_queue_active, mem, pageq);
			cnt.v_active_count--;
			mem->flags &= ~PG_ACTIVE;
		}
		if (mem->flags & PG_INACTIVE) {
			TAILQ_REMOVE(&vm_page_queue_inactive, mem, pageq);
			cnt.v_inactive_count--;
			mem->flags &= ~PG_INACTIVE;
		}
		cnt.v_wire_count++;
	}
	mem->wire_count++;
}

/*
 * vm_page_unwire:
 *
 * Release one wiring of this page, potentially
 * enabling it to be paged again.
 *
 * The page queues must be locked.
 */
void
vm_page_unwire(mem)
	register vm_page_t	mem;
{
	VM_PAGE_CHECK(mem);

	mem->wire_count--;
	if (mem->wire_count == 0) {
		TAILQ_INSERT_TAIL(&vm_page_queue_active, mem, pageq);
		cnt.v_active_count++;
		mem->flags |= PG_ACTIVE;
		cnt.v_wire_count--;
	}
}

/*
 * vm_page_deactivate:
 *
 * Returns the given page to the inactive list,
 * indicating that no physical maps have access
 * to this page.  [Used by the physical mapping system.]
 *
 * The page queues must be locked.
 */
void
vm_page_deactivate(m)
	register vm_page_t	m;
{
	VM_PAGE_CHECK(m);

	/*
	 * Only move active pages -- ignore locked or already
	 * inactive ones.
	 */
	if (m->flags & PG_ACTIVE) {
		TAILQ_REMOVE(&vm_page_queue_active, m, pageq);
		m->flags &= ~PG_ACTIVE;
		cnt.v_active_count--;
		goto deact;
	}
	if ((m->flags & PG_INACTIVE) == 0) {
	deact:
		TAILQ_INSERT_TAIL(&vm_page_queue_inactive, m, pageq);
		m->flags |= PG_INACTIVE;
		cnt.v_inactive_count++;
		pmap_clear_reference(VM_PAGE_TO_PHYS(m));
		if (pmap_is_modified(VM_PAGE_TO_PHYS(m)))
			m->flags &= ~PG_CLEAN;
		if (m->flags & PG_CLEAN)
			m->flags &= ~PG_LAUNDRY;
		else
			m->flags |= PG_LAUNDRY;
	}
}

/*
 * vm_page_activate:
 *
 * Put the specified page on the active list (if appropriate).
 *
 * The page queues must be locked.
 */
void
vm_page_activate(m)
	register vm_page_t	m;
{
	VM_PAGE_CHECK(m);

	if (m->flags & PG_INACTIVE) {
		TAILQ_REMOVE(&vm_page_queue_inactive, m, pageq);
		m->flags &= ~PG_INACTIVE;
		cnt.v_inactive_count--;
	}
	if (m->wire_count == 0) {
		if (m->flags & PG_ACTIVE)
			panic("vm_page_activate: already active");

		TAILQ_INSERT_TAIL(&vm_page_queue_active, m, pageq);
		m->flags |= PG_ACTIVE;
		cnt.v_active_count++;
	}
}

/*
 * vm_page_zero_fill:
 *
 * Zero-fill the specified page.
 * Written as a standard pagein routine, to
 * be used by the zero-fill object.
 */
boolean_t
vm_page_zero_fill(m)
	vm_page_t	m;
{
	VM_PAGE_CHECK(m);

	m->flags &= ~PG_CLEAN;
	pmap_zero_page(VM_PAGE_TO_PHYS(m));
	return(TRUE);
}

/*
 * vm_page_copy:
 *
 * Copy one page to another
 */
void
vm_page_copy(src_m, dest_m)
	vm_page_t	src_m;
	vm_page_t	dest_m;
{
	VM_PAGE_CHECK(src_m);
	VM_PAGE_CHECK(dest_m);

	dest_m->flags &= ~PG_CLEAN;
	pmap_copy_page(VM_PAGE_TO_PHYS(src_m), VM_PAGE_TO_PHYS(dest_m));
}

#ifdef VM_PAGE_ALLOC_MEMORY_STATS
#define	STAT_INCR(v)	(v)++
#define	STAT_DECR(v)	do { \
		if ((v) == 0) \
			printf("%s:%d -- Already 0!\n", __FILE__, __LINE__); \
		else \
			(v)--; \
	} while (0)
u_long	vm_page_alloc_memory_npages;
#else
#define	STAT_INCR(v)
#define	STAT_DECR(v)
#endif

/*
 *	vm_page_alloc_memory:
 *
 *	Allocate physical pages conforming to the restrictions
 *	provided:
 *
 *		size		The size of the allocation,
 *				rounded to page size.
 *
 *		low		The low address of the allowed
 *				allocation range.
 *
 *		high		The high address of the allowed
 *				allocation range.
 *
 *		alignment	Allocation must be aligned to this
 *				power-of-two boundary.
 *
 *		boundary	No segment in the allocation may
 *				cross this power-of-two boundary
 *				(relative to zero).
 *
 *	The allocated pages are placed at the tail of `rlist'; `rlist'
 *	is assumed to be properly initialized by the caller.  The
 *	number of memory segments that the allocated memory may
 *	occupy is specified in the `nsegs' arguement.
 *
 *	Returns 0 on success or an errno value to indicate mode
 *	of failure.
 *
 *	XXX This implementation could be improved.  It only
 *	XXX allocates a single segment.
 */
int
vm_page_alloc_memory(size, low, high, alignment, boundary, rlist, nsegs,
    waitok)
	vm_size_t size;
	vm_offset_t low, high, alignment, boundary;
	struct pglist *rlist;
	int nsegs, waitok;
{
	vm_offset_t try, idxpa, lastidxpa;
	int s, tryidx, idx, end, error;
	vm_page_t m;
	u_long pagemask;
#ifdef DEBUG
	vm_page_t tp;
#endif

#ifdef DIAGNOSTIC
	if ((alignment & (alignment - 1)) != 0)
		panic("vm_page_alloc_memory: alignment must be power of 2");

	if ((boundary & (boundary - 1)) != 0)
		panic("vm_page_alloc_memory: boundary must be power of 2");
#endif

	/*
	 * Our allocations are always page granularity, so our alignment
	 * must be, too.
	 */
	if (alignment < PAGE_SIZE)
		alignment = PAGE_SIZE;

	size = round_page(size);
	try = roundup(low, alignment);

	if (boundary != 0 && boundary < size)
		return (EINVAL);

	pagemask = ~(boundary - 1);

	/* Default to "lose". */
	error = ENOMEM;

	/*
	 * Block all memory allocation and lock the free list.
	 */
	s = splimp();
	simple_lock(&vm_page_queue_free_lock);

	/* Are there even any free pages? */
	if (vm_page_queue_free.tqh_first == NULL)
		goto out;

	for (;; try += alignment) {
		if (try + size > high) {
			/*
			 * We've run past the allowable range.
			 */
			goto out;
		}

		/*
		 * Make sure this is a managed physical page.
		 */
		if (IS_VM_PHYSADDR(try) == 0)
			continue;

		tryidx = idx = VM_PAGE_INDEX(try);
		end = idx + (size / PAGE_SIZE);
		if (end > vm_page_count) {
			/*
			 * No more physical memory.
			 */
			goto out;
		}

		/*
		 * Found a suitable starting page.  See of the range
		 * is free.
		 */
		for (; idx < end; idx++) {
			if (VM_PAGE_IS_FREE(&vm_page_array[idx]) == 0) {
				/*
				 * Page not available.
				 */
				break;
			}

			idxpa = VM_PAGE_TO_PHYS(&vm_page_array[idx]);

			/*
			 * Make sure this is a managed physical page.
			 * XXX Necessary?  I guess only if there
			 * XXX are holes in the vm_page_array[].
			 */
			if (IS_VM_PHYSADDR(idxpa) == 0)
				break;

			if (idx > tryidx) {
				lastidxpa =
				    VM_PAGE_TO_PHYS(&vm_page_array[idx - 1]);

				if ((lastidxpa + PAGE_SIZE) != idxpa) {
					/*
					 * Region not contiguous.
					 */
					break;
				}
				if (boundary != 0 &&
				    ((lastidxpa ^ idxpa) & pagemask) != 0) {
					/*
					 * Region crosses boundary.
					 */
					break;
				}
			}
		}

		if (idx == end) {
			/*
			 * Woo hoo!  Found one.
			 */
			break;
		}
	}

	/*
	 * Okay, we have a chunk of memory that conforms to
	 * the requested constraints.
	 */
	idx = tryidx;
	while (idx < end) {
		m = &vm_page_array[idx];
#ifdef DEBUG
		for (tp = vm_page_queue_free.tqh_first; tp != NULL;
		    tp = tp->pageq.tqe_next) {
			if (tp == m)
				break;
		}
		if (tp == NULL)
			panic("vm_page_alloc_memory: page not on freelist");
#endif
		TAILQ_REMOVE(&vm_page_queue_free, m, pageq);
		cnt.v_free_count--;
		m->flags = PG_CLEAN;
		m->object = NULL;
		m->wire_count = 0;
		TAILQ_INSERT_TAIL(rlist, m, pageq);
		idx++;
		STAT_INCR(vm_page_alloc_memory_npages);
	}
	error = 0;

 out:
	simple_unlock(&vm_page_queue_free_lock);
	splx(s);
	return (error);
}

/*
 *	vm_page_free_memory:
 *
 *	Free a list of pages previously allocated by vm_page_alloc_memory().
 *	The pages are assumed to have no mappings.
 */
void
vm_page_free_memory(list)
	struct pglist *list;
{
	vm_page_t m;
	int s;

	/*
	 * Block all memory allocation and lock the free list.
	 */
	s = splimp();
	simple_lock(&vm_page_queue_free_lock);

	while ((m = list->tqh_first) != NULL) {
		TAILQ_REMOVE(list, m, pageq);
		m->flags = PG_FREE;
		TAILQ_INSERT_TAIL(&vm_page_queue_free, m, pageq);
		cnt.v_free_count++;
		STAT_DECR(vm_page_alloc_memory_npages);
	}

	simple_unlock(&vm_page_queue_free_lock);
	splx(s);
}
