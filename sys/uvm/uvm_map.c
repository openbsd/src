/*	$NetBSD: uvm_map.c,v 1.34 1999/01/24 23:53:15 chuck Exp $	*/

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
 *	@(#)vm_map.c    8.3 (Berkeley) 1/12/94
 * from: Id: uvm_map.c,v 1.1.2.27 1998/02/07 01:16:54 chs Exp
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
 * uvm_map.c: uvm map operations
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mman.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/pool.h>

#include <sys/user.h>
#include <machine/pcb.h>

#ifdef SYSVSHM
#include <sys/shm.h>
#endif

#include <vm/vm.h>
#include <vm/vm_page.h>
#include <vm/vm_kern.h>

#define UVM_MAP
#include <uvm/uvm.h>

#ifdef DDB
#include <uvm/uvm_ddb.h>
#endif


struct uvm_cnt uvm_map_call, map_backmerge, map_forwmerge;
struct uvm_cnt uvm_mlk_call, uvm_mlk_hint;

/*
 * pool for vmspace structures.
 */

struct pool uvm_vmspace_pool;

/*
 * pool for dynamically-allocated map entries.
 */

struct pool uvm_map_entry_pool;

/*
 * macros
 */

/*
 * uvm_map_entry_link: insert entry into a map
 *
 * => map must be locked
 */
#define uvm_map_entry_link(map, after_where, entry) do { \
	(map)->nentries++; \
	(entry)->prev = (after_where); \
	(entry)->next = (after_where)->next; \
	(entry)->prev->next = (entry); \
	(entry)->next->prev = (entry); \
} while (0)

/*
 * uvm_map_entry_unlink: remove entry from a map
 *
 * => map must be locked
 */
#define uvm_map_entry_unlink(map, entry) do { \
	(map)->nentries--; \
	(entry)->next->prev = (entry)->prev; \
	(entry)->prev->next = (entry)->next; \
} while (0)

/*
 * SAVE_HINT: saves the specified entry as the hint for future lookups.
 *
 * => map need not be locked (protected by hint_lock).
 */
#define SAVE_HINT(map,value) do { \
	simple_lock(&(map)->hint_lock); \
	(map)->hint = (value); \
	simple_unlock(&(map)->hint_lock); \
} while (0)

/*
 * VM_MAP_RANGE_CHECK: check and correct range
 *
 * => map must at least be read locked
 */

#define VM_MAP_RANGE_CHECK(map, start, end) do { \
	if (start < vm_map_min(map)) 		\
		start = vm_map_min(map);        \
	if (end > vm_map_max(map))              \
		end = vm_map_max(map);          \
	if (start > end)                        \
		start = end;                    \
} while (0)

/*
 * local prototypes
 */

static vm_map_entry_t	uvm_mapent_alloc __P((vm_map_t));
static void		uvm_mapent_copy __P((vm_map_entry_t,vm_map_entry_t));
static void		uvm_mapent_free __P((vm_map_entry_t));
static void		uvm_map_entry_unwire __P((vm_map_t, vm_map_entry_t));

/*
 * local inlines
 */

#undef UVM_MAP_INLINES

#ifdef UVM_MAP_INLINES
#define UVM_INLINE __inline
#else
#define UVM_INLINE
#endif

/*
 * uvm_mapent_alloc: allocate a map entry
 *
 * => XXX: static pool for kernel map?
 */

static UVM_INLINE vm_map_entry_t
uvm_mapent_alloc(map)
	vm_map_t map;
{
	vm_map_entry_t me;
	int s;
	UVMHIST_FUNC("uvm_mapent_alloc");
	UVMHIST_CALLED(maphist);

	if (map->entries_pageable) {
		me = pool_get(&uvm_map_entry_pool, PR_WAITOK);
		me->flags = 0;
		/* me can't be null, wait ok */

	} else {
		s = splimp();	/* protect kentry_free list with splimp */
		simple_lock(&uvm.kentry_lock);
		me = uvm.kentry_free;
		if (me) uvm.kentry_free = me->next;
		simple_unlock(&uvm.kentry_lock);
		splx(s);
		if (!me)
	panic("mapent_alloc: out of kernel map entries, check MAX_KMAPENT");
		me->flags = UVM_MAP_STATIC;
	}

	UVMHIST_LOG(maphist, "<- new entry=0x%x [pageable=%d]", 
		me, map->entries_pageable, 0, 0);
	return(me);

}

/*
 * uvm_mapent_free: free map entry
 *
 * => XXX: static pool for kernel map?
 */

static UVM_INLINE void
uvm_mapent_free(me)
	vm_map_entry_t me;
{
	int s;
	UVMHIST_FUNC("uvm_mapent_free");
	UVMHIST_CALLED(maphist);
	UVMHIST_LOG(maphist,"<- freeing map entry=0x%x [flags=%d]", 
		me, me->flags, 0, 0);
	if ((me->flags & UVM_MAP_STATIC) == 0) {
		pool_put(&uvm_map_entry_pool, me);
	} else {
		s = splimp();	/* protect kentry_free list with splimp */
		simple_lock(&uvm.kentry_lock);
		me->next = uvm.kentry_free;
		uvm.kentry_free = me;
		simple_unlock(&uvm.kentry_lock);
		splx(s);
	}
}

/*
 * uvm_mapent_copy: copy a map entry, preserving flags
 */

static UVM_INLINE void
uvm_mapent_copy(src, dst)
	vm_map_entry_t src;
	vm_map_entry_t dst;
{

	bcopy(src, dst, ((char *)&src->uvm_map_entry_stop_copy) - ((char*)src));
}

/*
 * uvm_map_entry_unwire: unwire a map entry
 *
 * => map should be locked by caller
 */

static UVM_INLINE void
uvm_map_entry_unwire(map, entry)
	vm_map_t map;
	vm_map_entry_t entry;
{

	uvm_fault_unwire(map->pmap, entry->start, entry->end);
	entry->wired_count = 0;
}

/*
 * uvm_map_init: init mapping system at boot time.   note that we allocate
 * and init the static pool of vm_map_entry_t's for the kernel here.
 */

void
uvm_map_init() 
{
	static struct vm_map_entry kernel_map_entry[MAX_KMAPENT];
#if defined(UVMHIST)
	static struct uvm_history_ent maphistbuf[100];
	static struct uvm_history_ent pdhistbuf[100];
#endif
	int lcv;

	/*
	 * first, init logging system.
	 */

	UVMHIST_FUNC("uvm_map_init");
	UVMHIST_INIT_STATIC(maphist, maphistbuf);
	UVMHIST_INIT_STATIC(pdhist, pdhistbuf);
	UVMHIST_CALLED(maphist);
	UVMHIST_LOG(maphist,"<starting uvm map system>", 0, 0, 0, 0);
	UVMCNT_INIT(uvm_map_call,  UVMCNT_CNT, 0,
	    "# uvm_map() successful calls", 0);
	UVMCNT_INIT(map_backmerge, UVMCNT_CNT, 0, "# uvm_map() back merges", 0);
	UVMCNT_INIT(map_forwmerge, UVMCNT_CNT, 0, "# uvm_map() missed forward",
	    0);
	UVMCNT_INIT(uvm_mlk_call,  UVMCNT_CNT, 0, "# map lookup calls", 0);
	UVMCNT_INIT(uvm_mlk_hint,  UVMCNT_CNT, 0, "# map lookup hint hits", 0);

	/*
	 * now set up static pool of kernel map entrys ...
	 */

	simple_lock_init(&uvm.kentry_lock);
	uvm.kentry_free = NULL;
	for (lcv = 0 ; lcv < MAX_KMAPENT ; lcv++) {
		kernel_map_entry[lcv].next = uvm.kentry_free;
		uvm.kentry_free = &kernel_map_entry[lcv];
	}

	/*
	 * initialize the map-related pools.
	 */
	pool_init(&uvm_vmspace_pool, sizeof(struct vmspace),
	    0, 0, 0, "vmsppl", 0,
	    pool_page_alloc_nointr, pool_page_free_nointr, M_VMMAP);
	pool_init(&uvm_map_entry_pool, sizeof(struct vm_map_entry),
	    0, 0, 0, "vmmpepl", 0,
	    pool_page_alloc_nointr, pool_page_free_nointr, M_VMMAP);
}

/*
 * clippers
 */

/*
 * uvm_map_clip_start: ensure that the entry begins at or after
 *	the starting address, if it doesn't we split the entry.
 * 
 * => caller should use UVM_MAP_CLIP_START macro rather than calling
 *    this directly
 * => map must be locked by caller
 */

void uvm_map_clip_start(map, entry, start)
	vm_map_t       map;
	vm_map_entry_t entry;
	vaddr_t    start;
{
	vm_map_entry_t new_entry;
	vaddr_t new_adj;

	/* uvm_map_simplify_entry(map, entry); */ /* XXX */

	/*
	 * Split off the front portion.  note that we must insert the new
	 * entry BEFORE this one, so that this entry has the specified
	 * starting address.
	 */

	new_entry = uvm_mapent_alloc(map);
	uvm_mapent_copy(entry, new_entry); /* entry -> new_entry */
				
	new_entry->end = start; 
	new_adj = start - new_entry->start;
	if (entry->object.uvm_obj)
		entry->offset += new_adj;	/* shift start over */
	entry->start = start;

	if (new_entry->aref.ar_amap) {
		amap_splitref(&new_entry->aref, &entry->aref, new_adj);
	}

	uvm_map_entry_link(map, entry->prev, new_entry);
				 
	if (UVM_ET_ISSUBMAP(entry)) {
		/* ... unlikely to happen, but play it safe */
		 uvm_map_reference(new_entry->object.sub_map);
	} else {
		if (UVM_ET_ISOBJ(entry) && 
		    entry->object.uvm_obj->pgops &&
		    entry->object.uvm_obj->pgops->pgo_reference)
			entry->object.uvm_obj->pgops->pgo_reference(
			    entry->object.uvm_obj);
	}
}

/*
 * uvm_map_clip_end: ensure that the entry ends at or before
 *	the ending address, if it does't we split the reference
 * 
 * => caller should use UVM_MAP_CLIP_END macro rather than calling
 *    this directly
 * => map must be locked by caller
 */

void
uvm_map_clip_end(map, entry, end)
	vm_map_t	map;
	vm_map_entry_t	entry;
	vaddr_t	end;
{
	vm_map_entry_t	new_entry;
	vaddr_t new_adj; /* #bytes we move start forward */

	/*
	 *	Create a new entry and insert it
	 *	AFTER the specified entry
	 */

	new_entry = uvm_mapent_alloc(map);
	uvm_mapent_copy(entry, new_entry); /* entry -> new_entry */

	new_entry->start = entry->end = end;
	new_adj = end - entry->start;
	if (new_entry->object.uvm_obj)
		new_entry->offset += new_adj;

	if (entry->aref.ar_amap)
		amap_splitref(&entry->aref, &new_entry->aref, new_adj);

	uvm_map_entry_link(map, entry, new_entry);

	if (UVM_ET_ISSUBMAP(entry)) {
		/* ... unlikely to happen, but play it safe */
	 	uvm_map_reference(new_entry->object.sub_map);
	} else {
		if (UVM_ET_ISOBJ(entry) &&
		    entry->object.uvm_obj->pgops &&
		    entry->object.uvm_obj->pgops->pgo_reference)
			entry->object.uvm_obj->pgops->pgo_reference(
			    entry->object.uvm_obj);
	}
}


/*
 *   M A P   -   m a i n   e n t r y   p o i n t
 */
/*
 * uvm_map: establish a valid mapping in a map
 *
 * => assume startp is page aligned.
 * => assume size is a multiple of PAGE_SIZE.
 * => assume sys_mmap provides enough of a "hint" to have us skip
 *	over text/data/bss area.
 * => map must be unlocked (we will lock it)
 * => <uobj,uoffset> value meanings (4 cases):
 *	 [1] <NULL,uoffset> 		== uoffset is a hint for PMAP_PREFER
 *	 [2] <NULL,UVM_UNKNOWN_OFFSET>	== don't PMAP_PREFER
 *	 [3] <uobj,uoffset>		== normal mapping
 *	 [4] <uobj,UVM_UNKNOWN_OFFSET>	== uvm_map finds offset based on VA
 *	
 *    case [4] is for kernel mappings where we don't know the offset until
 *    we've found a virtual address.   note that kernel object offsets are
 *    always relative to vm_map_min(kernel_map).
 * => XXXCDC: need way to map in external amap?
 */

int
uvm_map(map, startp, size, uobj, uoffset, flags)
	vm_map_t map;
	vaddr_t *startp;	/* IN/OUT */
	vsize_t size;
	struct uvm_object *uobj;
	vaddr_t uoffset;
	uvm_flag_t flags;
{
	vm_map_entry_t prev_entry, new_entry;
	vm_prot_t prot = UVM_PROTECTION(flags), maxprot =
	    UVM_MAXPROTECTION(flags);
	vm_inherit_t inherit = UVM_INHERIT(flags);
	int advice = UVM_ADVICE(flags);
	UVMHIST_FUNC("uvm_map");
	UVMHIST_CALLED(maphist);

	UVMHIST_LOG(maphist, "(map=0x%x, *startp=0x%x, size=%d, flags=0x%x)",
	    map, *startp, size, flags);
	UVMHIST_LOG(maphist, "  uobj/offset 0x%x/%d", uobj, uoffset,0,0);

	/*
	 * step 0: sanity check of protection code
	 */

	if ((prot & maxprot) != prot) {
		UVMHIST_LOG(maphist, "<- prot. failure:  prot=0x%x, max=0x%x", 
		prot, maxprot,0,0);
		return(KERN_PROTECTION_FAILURE);
	}

	/*
	 * step 1: figure out where to put new VM range
	 */

	if (vm_map_lock_try(map) == FALSE) {
		if (flags & UVM_FLAG_TRYLOCK)
			return(KERN_FAILURE);
		vm_map_lock(map); /* could sleep here */
	}
	if ((prev_entry = uvm_map_findspace(map, *startp, size, startp, 
	    uobj, uoffset, flags & UVM_FLAG_FIXED)) == NULL) {
		UVMHIST_LOG(maphist,"<- uvm_map_findspace failed!",0,0,0,0);
		vm_map_unlock(map);
		return (KERN_NO_SPACE);
	}

#if defined(PMAP_GROWKERNEL)	/* hack */
	{
		/* locked by kernel_map lock */
		static vaddr_t maxkaddr = 0;
		
		/*
		 * hack: grow kernel PTPs in advance.
		 */
		if (map == kernel_map && maxkaddr < (*startp + size)) {
			pmap_growkernel(*startp + size);
			maxkaddr = *startp + size;
		}
	}
#endif

	UVMCNT_INCR(uvm_map_call);

	/*
	 * if uobj is null, then uoffset is either a VAC hint for PMAP_PREFER
	 * [typically from uvm_map_reserve] or it is UVM_UNKNOWN_OFFSET.   in 
	 * either case we want to zero it  before storing it in the map entry 
	 * (because it looks strange and confusing when debugging...)
	 * 
	 * if uobj is not null 
	 *   if uoffset is not UVM_UNKNOWN_OFFSET then we have a normal mapping
	 *      and we do not need to change uoffset.
	 *   if uoffset is UVM_UNKNOWN_OFFSET then we need to find the offset
	 *      now (based on the starting address of the map).   this case is
	 *      for kernel object mappings where we don't know the offset until
	 *      the virtual address is found (with uvm_map_findspace).   the
	 *      offset is the distance we are from the start of the map.
	 */

	if (uobj == NULL) {
		uoffset = 0;
	} else {
		if (uoffset == UVM_UNKNOWN_OFFSET) {
#ifdef DIAGNOSTIC
			if (uobj->uo_refs != UVM_OBJ_KERN)
	panic("uvm_map: unknown offset with non-kernel object");
#endif
			uoffset = *startp - vm_map_min(kernel_map);
		}
	}

	/*
	 * step 2: try and insert in map by extending previous entry, if
	 * possible
	 * XXX: we don't try and pull back the next entry.   might be useful
	 * for a stack, but we are currently allocating our stack in advance.
	 */

	if ((flags & UVM_FLAG_NOMERGE) == 0 && 
	    prev_entry->end == *startp && prev_entry != &map->header &&
	    prev_entry->object.uvm_obj == uobj) {

		if (uobj && prev_entry->offset +
		    (prev_entry->end - prev_entry->start) != uoffset)
			goto step3;

		if (UVM_ET_ISSUBMAP(prev_entry))
			goto step3;

		if (prev_entry->protection != prot || 
		    prev_entry->max_protection != maxprot)
			goto step3;

		if (prev_entry->inheritance != inherit ||
		    prev_entry->advice != advice)
			goto step3;

		/* wired_count's must match (new area is unwired) */
		if (prev_entry->wired_count)
			goto step3; 

		/*
		 * can't extend a shared amap.  note: no need to lock amap to 
		 * look at refs since we don't care about its exact value.
		 * if it is one (i.e. we have only reference) it will stay there
		 */
		   
		if (prev_entry->aref.ar_amap &&
		    amap_refs(prev_entry->aref.ar_amap) != 1) {
			goto step3;
		}
		
		/* got it! */

		UVMCNT_INCR(map_backmerge);
		UVMHIST_LOG(maphist,"  starting back merge", 0, 0, 0, 0);

		/*
		 * drop our reference to uobj since we are extending a reference
		 * that we already have (the ref count can not drop to zero).
		 */
		if (uobj && uobj->pgops->pgo_detach)
			uobj->pgops->pgo_detach(uobj);

		if (prev_entry->aref.ar_amap) {
			amap_extend(prev_entry, size);
		}

		prev_entry->end += size;
		map->size += size;

		UVMHIST_LOG(maphist,"<- done (via backmerge)!", 0, 0, 0, 0);
		vm_map_unlock(map);
		return (KERN_SUCCESS);

	}
step3:
	UVMHIST_LOG(maphist,"  allocating new map entry", 0, 0, 0, 0);

	/*
	 * check for possible forward merge (which we don't do) and count
	 * the number of times we missed a *possible* chance to merge more 
	 */

	if ((flags & UVM_FLAG_NOMERGE) == 0 &&
	    prev_entry->next != &map->header && 
	    prev_entry->next->start == (*startp + size))
		UVMCNT_INCR(map_forwmerge);

	/*
	 * step 3: allocate new entry and link it in
	 */

	new_entry = uvm_mapent_alloc(map);
	new_entry->start = *startp;
	new_entry->end = new_entry->start + size;
	new_entry->object.uvm_obj = uobj;
	new_entry->offset = uoffset;

	if (uobj) 
		new_entry->etype = UVM_ET_OBJ;
	else
		new_entry->etype = 0;

	if (flags & UVM_FLAG_COPYONW) {
		new_entry->etype |= UVM_ET_COPYONWRITE;
		if ((flags & UVM_FLAG_OVERLAY) == 0)
			new_entry->etype |= UVM_ET_NEEDSCOPY;
	}

	new_entry->protection = prot;
	new_entry->max_protection = maxprot;
	new_entry->inheritance = inherit;
	new_entry->wired_count = 0;
	new_entry->advice = advice;
	if (flags & UVM_FLAG_OVERLAY) {
		/*
		 * to_add: for BSS we overallocate a little since we
		 * are likely to extend
		 */
		vaddr_t to_add = (flags & UVM_FLAG_AMAPPAD) ? 
			UVM_AMAP_CHUNK << PAGE_SHIFT : 0;
		struct vm_amap *amap = amap_alloc(size, to_add, M_WAITOK);
		new_entry->aref.ar_pageoff = 0;
		new_entry->aref.ar_amap = amap;
	} else {
		new_entry->aref.ar_amap = NULL;
	}

	uvm_map_entry_link(map, prev_entry, new_entry);

	map->size += size;

	/*
	 *      Update the free space hint
	 */

	if ((map->first_free == prev_entry) &&
	    (prev_entry->end >= new_entry->start))
		map->first_free = new_entry;

	UVMHIST_LOG(maphist,"<- done!", 0, 0, 0, 0);
	vm_map_unlock(map);
	return(KERN_SUCCESS);
}

/*
 * uvm_map_lookup_entry: find map entry at or before an address
 *
 * => map must at least be read-locked by caller
 * => entry is returned in "entry"
 * => return value is true if address is in the returned entry
 */

boolean_t
uvm_map_lookup_entry(map, address, entry)
	vm_map_t	map;
	vaddr_t	address;
	vm_map_entry_t		*entry;		/* OUT */
{
	vm_map_entry_t		cur;
	vm_map_entry_t		last;
	UVMHIST_FUNC("uvm_map_lookup_entry");
	UVMHIST_CALLED(maphist);

	UVMHIST_LOG(maphist,"(map=0x%x,addr=0x%x,ent=0x%x)",
	    map, address, entry, 0);

	/*
	 * start looking either from the head of the
	 * list, or from the hint.
	 */

	simple_lock(&map->hint_lock);
	cur = map->hint;
	simple_unlock(&map->hint_lock);

	if (cur == &map->header)
		cur = cur->next;

	UVMCNT_INCR(uvm_mlk_call);
	if (address >= cur->start) {
	    	/*
		 * go from hint to end of list.
		 *
		 * but first, make a quick check to see if
		 * we are already looking at the entry we
		 * want (which is usually the case).
		 * note also that we don't need to save the hint
		 * here... it is the same hint (unless we are
		 * at the header, in which case the hint didn't
		 * buy us anything anyway).
		 */
		last = &map->header;
		if ((cur != last) && (cur->end > address)) {
			UVMCNT_INCR(uvm_mlk_hint);
			*entry = cur;
			UVMHIST_LOG(maphist,"<- got it via hint (0x%x)",
			    cur, 0, 0, 0);
			return (TRUE);
		}
	} else {
	    	/*
		 * go from start to hint, *inclusively*
		 */
		last = cur->next;
		cur = map->header.next;
	}

	/*
	 * search linearly
	 */

	while (cur != last) {
		if (cur->end > address) {
			if (address >= cur->start) {
			    	/*
				 * save this lookup for future
				 * hints, and return
				 */

				*entry = cur;
				SAVE_HINT(map, cur);
				UVMHIST_LOG(maphist,"<- search got it (0x%x)",
					cur, 0, 0, 0);
				return (TRUE);
			}
			break;
		}
		cur = cur->next;
	}
	*entry = cur->prev;
	SAVE_HINT(map, *entry);
	UVMHIST_LOG(maphist,"<- failed!",0,0,0,0);
	return (FALSE);
}


/*
 * uvm_map_findspace: find "length" sized space in "map".
 *
 * => "hint" is a hint about where we want it, unless fixed is true
 *	(in which case we insist on using "hint").
 * => "result" is VA returned
 * => uobj/uoffset are to be used to handle VAC alignment, if required
 * => caller must at least have read-locked map
 * => returns NULL on failure, or pointer to prev. map entry if success
 * => note this is a cross between the old vm_map_findspace and vm_map_find
 */

vm_map_entry_t
uvm_map_findspace(map, hint, length, result, uobj, uoffset, fixed)
	vm_map_t map;
	vaddr_t hint;
	vsize_t length;
	vaddr_t *result; /* OUT */
	struct uvm_object *uobj;
	vaddr_t uoffset;
	boolean_t fixed;
{
	vm_map_entry_t entry, next, tmp;
	vaddr_t end;
	UVMHIST_FUNC("uvm_map_findspace");
	UVMHIST_CALLED(maphist);

	UVMHIST_LOG(maphist, "(map=0x%x, hint=0x%x, len=%d, fixed=%d)", 
		map, hint, length, fixed);

	if (hint < map->min_offset) {	/* check ranges ... */
		if (fixed) {
			UVMHIST_LOG(maphist,"<- VA below map range",0,0,0,0);
			return(NULL);
		}
		hint = map->min_offset;
	}
	if (hint > map->max_offset) {
		UVMHIST_LOG(maphist,"<- VA 0x%x > range [0x%x->0x%x]",
				hint, map->min_offset, map->max_offset, 0);
		return(NULL);
	}

	/*
	 * Look for the first possible address; if there's already
	 * something at this address, we have to start after it.
	 */

	if (!fixed && hint == map->min_offset) {
		if ((entry = map->first_free) != &map->header) 
			hint = entry->end;
	} else {
		if (uvm_map_lookup_entry(map, hint, &tmp)) {
			/* "hint" address already in use ... */
			if (fixed) {
				UVMHIST_LOG(maphist,"<- fixed & VA in use",
				    0, 0, 0, 0);
				return(NULL);
			}
			hint = tmp->end;
		}
		entry = tmp;
	}

	/*
	 * Look through the rest of the map, trying to fit a new region in
	 * the gap between existing regions, or after the very last region.
	 * note: entry->end   = base VA of current gap,
	 *	 next->start  = VA of end of current gap
	 */
	for (;; hint = (entry = next)->end) {
		/*
		 * Find the end of the proposed new region.  Be sure we didn't
		 * go beyond the end of the map, or wrap around the address;
		 * if so, we lose.  Otherwise, if this is the last entry, or
		 * if the proposed new region fits before the next entry, we
		 * win.
		 */

#ifdef PMAP_PREFER
		/*
		 * push hint forward as needed to avoid VAC alias problems.
		 * we only do this if a valid offset is specified.
		 */
		if (!fixed && uoffset != UVM_UNKNOWN_OFFSET)
		  PMAP_PREFER(uoffset, &hint);
#endif
		end = hint + length;
		if (end > map->max_offset || end < hint) {
			UVMHIST_LOG(maphist,"<- failed (off end)", 0,0,0,0);
			return (NULL);
		}
		next = entry->next;
		if (next == &map->header || next->start >= end)
			break;
		if (fixed) {
			UVMHIST_LOG(maphist,"<- fixed mapping failed", 0,0,0,0);
			return(NULL); /* only one shot at it ... */
		}
	}
	SAVE_HINT(map, entry);
	*result = hint;
	UVMHIST_LOG(maphist,"<- got it!  (result=0x%x)", hint, 0,0,0);
	return (entry);
}

/*
 *   U N M A P   -   m a i n   h e l p e r   f u n c t i o n s
 */

/*
 * uvm_unmap_remove: remove mappings from a vm_map (from "start" up to "stop")
 *
 * => caller must check alignment and size 
 * => map must be locked by caller
 * => we return a list of map entries that we've remove from the map
 *    in "entry_list"
 */

int
uvm_unmap_remove(map, start, end, entry_list)
	vm_map_t map;
	vaddr_t start,end;
	vm_map_entry_t *entry_list;	/* OUT */
{
	vm_map_entry_t entry, first_entry, next;
	vaddr_t len;
	UVMHIST_FUNC("uvm_unmap_remove");
	UVMHIST_CALLED(maphist);

	UVMHIST_LOG(maphist,"(map=0x%x, start=0x%x, end=0x%x)",
	    map, start, end, 0);

	VM_MAP_RANGE_CHECK(map, start, end);

	/*
	 * find first entry
	 */
	if (uvm_map_lookup_entry(map, start, &first_entry) == TRUE) {
		/* clip and go... */
		entry = first_entry;
		UVM_MAP_CLIP_START(map, entry, start);
		/* critical!  prevents stale hint */
		SAVE_HINT(map, entry->prev);

	} else {
		entry = first_entry->next;
	}

	/*
	 * Save the free space hint
	 */

	if (map->first_free->start >= start)
		map->first_free = entry->prev;

	/*
	 * note: we now re-use first_entry for a different task.  we remove
	 * a number of map entries from the map and save them in a linked
	 * list headed by "first_entry".  once we remove them from the map
	 * the caller should unlock the map and drop the references to the
	 * backing objects [c.f. uvm_unmap_detach].  the object is to
	 * seperate unmapping from reference dropping.  why?
	 *   [1] the map has to be locked for unmapping
	 *   [2] the map need not be locked for reference dropping
	 *   [3] dropping references may trigger pager I/O, and if we hit
	 *       a pager that does synchronous I/O we may have to wait for it.
	 *   [4] we would like all waiting for I/O to occur with maps unlocked
	 *       so that we don't block other threads.  
	 */
	first_entry = NULL;
	*entry_list = NULL;		/* to be safe */

	/*
	 * break up the area into map entry sized regions and unmap.  note 
	 * that all mappings have to be removed before we can even consider
	 * dropping references to amaps or VM objects (otherwise we could end
	 * up with a mapping to a page on the free list which would be very bad)
	 */

	while ((entry != &map->header) && (entry->start < end)) {

		UVM_MAP_CLIP_END(map, entry, end); 
		next = entry->next;
		len = entry->end - entry->start;
	
		/*
		 * unwire before removing addresses from the pmap; otherwise
		 * unwiring will put the entries back into the pmap (XXX).
		 */

		if (entry->wired_count)
			uvm_map_entry_unwire(map, entry);

		/*
		 * special case: handle mappings to anonymous kernel objects.
		 * we want to free these pages right away...
		 */
		if (UVM_ET_ISOBJ(entry) &&
		    entry->object.uvm_obj->uo_refs == UVM_OBJ_KERN) {

#ifdef DIAGNOSTIC
			if (vm_map_pmap(map) != pmap_kernel())
	panic("uvm_unmap_remove: kernel object mapped by non-kernel map");
#endif

			/*
			 * note: kernel object mappings are currently used in
			 * two ways:
			 *  [1] "normal" mappings of pages in the kernel object
			 *  [2] uvm_km_valloc'd allocations in which we
			 *      pmap_enter in some non-kernel-object page
			 *      (e.g. vmapbuf).
			 *
			 * for case [1], we need to remove the mapping from
			 * the pmap and then remove the page from the kernel
			 * object (because, once pages in a kernel object are
			 * unmapped they are no longer needed, unlike, say,
			 * a vnode where you might want the data to persist
			 * until flushed out of a queue).
			 *
			 * for case [2], we need to remove the mapping from
			 * the pmap.  there shouldn't be any pages at the
			 * specified offset in the kernel object [but it
			 * doesn't hurt to call uvm_km_pgremove just to be
			 * safe?]
			 *
			 * uvm_km_pgremove currently does the following: 
			 *   for pages in the kernel object in range: 
			 *     - pmap_page_protect them out of all pmaps
			 *     - uvm_pagefree the page
			 *
			 * note that in case [1] the pmap_page_protect call
			 * in uvm_km_pgremove may very well be redundant
			 * because we have already removed the mappings
			 * beforehand with pmap_remove (or pmap_kremove).
			 * in the PMAP_NEW case, the pmap_page_protect call
			 * may not do anything, since PMAP_NEW allows the
			 * kernel to enter/remove kernel mappings without
			 * bothing to keep track of the mappings (e.g. via
			 * pv_entry lists).    XXX: because of this, in the
			 * future we should consider removing the
			 * pmap_page_protect from uvm_km_pgremove some time
			 * in the future.
			 */

			/*
			 * remove mappings from pmap
			 */
#if defined(PMAP_NEW)
			pmap_kremove(entry->start, len);
#else
			pmap_remove(pmap_kernel(), entry->start,
			    entry->start+len);
#endif

			/*
			 * remove pages from a kernel object (offsets are
			 * always relative to vm_map_min(kernel_map)).
			 */
			uvm_km_pgremove(entry->object.uvm_obj, 
			entry->start - vm_map_min(kernel_map),
			entry->end - vm_map_min(kernel_map));

			/*
			 * null out kernel_object reference, we've just
			 * dropped it
			 */
			entry->etype &= ~UVM_ET_OBJ;
			entry->object.uvm_obj = NULL;	/* to be safe */

		} else {
			/*
		 	 * remove mappings the standard way.
		 	 */
			pmap_remove(map->pmap, entry->start, entry->end);
		}

		/*
		 * remove entry from map and put it on our list of entries 
		 * that we've nuked.  then go do next entry.
		 */
		UVMHIST_LOG(maphist, "  removed map entry 0x%x", entry, 0, 0,0);
		uvm_map_entry_unlink(map, entry);
		map->size -= len;
		entry->next = first_entry;
		first_entry = entry;
		entry = next;		/* next entry, please */
	}

	/*
	 * now we've cleaned up the map and are ready for the caller to drop
	 * references to the mapped objects.  
	 */

	*entry_list = first_entry;
	UVMHIST_LOG(maphist,"<- done!", 0, 0, 0, 0);
	return(KERN_SUCCESS);
}

/*
 * uvm_unmap_detach: drop references in a chain of map entries
 *
 * => we will free the map entries as we traverse the list.
 */

void
uvm_unmap_detach(first_entry, amap_unref_flags)
	vm_map_entry_t first_entry;
	int amap_unref_flags;
{
	vm_map_entry_t next_entry;
	UVMHIST_FUNC("uvm_unmap_detach"); UVMHIST_CALLED(maphist);

	while (first_entry) {

#ifdef DIAGNOSTIC
		/*
		 * sanity check
		 */
		/* was part of vm_map_entry_delete() */
		if (first_entry->wired_count)
			panic("unmap: still wired!");
#endif

		UVMHIST_LOG(maphist,
		    "  detach 0x%x: amap=0x%x, obj=0x%x, submap?=%d", 
		    first_entry, first_entry->aref.ar_amap, 
		    first_entry->object.uvm_obj,
		    UVM_ET_ISSUBMAP(first_entry));

		/*
		 * drop reference to amap, if we've got one
		 */

		if (first_entry->aref.ar_amap)
			amap_unref(first_entry, amap_unref_flags);

		/*
		 * drop reference to our backing object, if we've got one
		 */
		
		if (UVM_ET_ISSUBMAP(first_entry)) {
			/* ... unlikely to happen, but play it safe */
			uvm_map_deallocate(first_entry->object.sub_map);
		} else {
			if (UVM_ET_ISOBJ(first_entry) &&
			    first_entry->object.uvm_obj->pgops->pgo_detach)
				first_entry->object.uvm_obj->pgops->
				    pgo_detach(first_entry->object.uvm_obj);
		}

		/*
		 * next entry
		 */
		next_entry = first_entry->next;
		uvm_mapent_free(first_entry);
		first_entry = next_entry;
	}

	/*
	 * done!
	 */
	UVMHIST_LOG(maphist, "<- done", 0,0,0,0);
	return;
}

/*
 *   E X T R A C T I O N   F U N C T I O N S
 */

/* 
 * uvm_map_reserve: reserve space in a vm_map for future use.
 *
 * => we reserve space in a map by putting a dummy map entry in the 
 *    map (dummy means obj=NULL, amap=NULL, prot=VM_PROT_NONE)
 * => map should be unlocked (we will write lock it)
 * => we return true if we were able to reserve space
 * => XXXCDC: should be inline?
 */

int
uvm_map_reserve(map, size, offset, raddr)
	vm_map_t map;
	vsize_t size;
	vaddr_t offset;    /* hint for pmap_prefer */
	vaddr_t *raddr;	/* OUT: reserved VA */
{
	UVMHIST_FUNC("uvm_map_reserve"); UVMHIST_CALLED(maphist); 
 
	UVMHIST_LOG(maphist, "(map=0x%x, size=0x%x, offset=0x%x,addr=0x%x)",
	      map,size,offset,raddr);
 
	size = round_page(size);
	if (*raddr < vm_map_min(map))
		*raddr = vm_map_min(map);                /* hint */
 
	/*
	 * reserve some virtual space.
	 */
 
	if (uvm_map(map, raddr, size, NULL, offset,
	    UVM_MAPFLAG(UVM_PROT_NONE, UVM_PROT_NONE, UVM_INH_NONE,
	    UVM_ADV_RANDOM, UVM_FLAG_NOMERGE)) != KERN_SUCCESS) {
	    UVMHIST_LOG(maphist, "<- done (no VM)", 0,0,0,0);
		return (FALSE);
	}     
	
	UVMHIST_LOG(maphist, "<- done (*raddr=0x%x)", *raddr,0,0,0);
	return (TRUE);
}

/*
 * uvm_map_replace: replace a reserved (blank) area of memory with 
 * real mappings.
 *
 * => caller must WRITE-LOCK the map 
 * => we return TRUE if replacement was a success
 * => we expect the newents chain to have nnewents entrys on it and
 *    we expect newents->prev to point to the last entry on the list
 * => note newents is allowed to be NULL
 */

int
uvm_map_replace(map, start, end, newents, nnewents)
	struct vm_map *map;
	vaddr_t start, end;
	vm_map_entry_t newents;
	int nnewents;
{
	vm_map_entry_t oldent, last;
	UVMHIST_FUNC("uvm_map_replace");
	UVMHIST_CALLED(maphist);

	/*
	 * first find the blank map entry at the specified address
	 */
	
	if (!uvm_map_lookup_entry(map, start, &oldent)) {
		return(FALSE);
	}
	
	/*
	 * check to make sure we have a proper blank entry
	 */

	if (oldent->start != start || oldent->end != end || 
	    oldent->object.uvm_obj != NULL || oldent->aref.ar_amap != NULL) {
		return (FALSE);
	}

#ifdef DIAGNOSTIC
	/*
	 * sanity check the newents chain
	 */
	{
		vm_map_entry_t tmpent = newents;
		int nent = 0;
		vaddr_t cur = start;

		while (tmpent) {
			nent++;
			if (tmpent->start < cur)
				panic("uvm_map_replace1");
			if (tmpent->start > tmpent->end || tmpent->end > end) {
		printf("tmpent->start=0x%lx, tmpent->end=0x%lx, end=0x%lx\n",
			    tmpent->start, tmpent->end, end);
				panic("uvm_map_replace2");
			}
			cur = tmpent->end;
			if (tmpent->next) {
				if (tmpent->next->prev != tmpent)
					panic("uvm_map_replace3");
			} else {
				if (newents->prev != tmpent)
					panic("uvm_map_replace4");
			}
			tmpent = tmpent->next;
		}
		if (nent != nnewents)
			panic("uvm_map_replace5");
	}
#endif

	/*
	 * map entry is a valid blank!   replace it.   (this does all the
	 * work of map entry link/unlink...).
	 */

	if (newents) {

		last = newents->prev;		/* we expect this */

		/* critical: flush stale hints out of map */
		SAVE_HINT(map, newents);
		if (map->first_free == oldent)
			map->first_free = last;

		last->next = oldent->next;
		last->next->prev = last;
		newents->prev = oldent->prev;
		newents->prev->next = newents;
		map->nentries = map->nentries + (nnewents - 1);

	} else {

		/* critical: flush stale hints out of map */
		SAVE_HINT(map, oldent->prev);
		if (map->first_free == oldent)
			map->first_free = oldent->prev;

		/* NULL list of new entries: just remove the old one */
		uvm_map_entry_unlink(map, oldent);
	}


	/*
	 * now we can free the old blank entry, unlock the map and return.
	 */

	uvm_mapent_free(oldent);
	return(TRUE);
}

/*
 * uvm_map_extract: extract a mapping from a map and put it somewhere
 *	(maybe removing the old mapping)
 *
 * => maps should be unlocked (we will write lock them)
 * => returns 0 on success, error code otherwise
 * => start must be page aligned
 * => len must be page sized
 * => flags:
 *      UVM_EXTRACT_REMOVE: remove mappings from srcmap
 *      UVM_EXTRACT_CONTIG: abort if unmapped area (advisory only)
 *      UVM_EXTRACT_QREF: for a temporary extraction do quick obj refs
 *      UVM_EXTRACT_FIXPROT: set prot to maxprot as we go
 *    >>>NOTE: if you set REMOVE, you are not allowed to use CONTIG or QREF!<<<
 *    >>>NOTE: QREF's must be unmapped via the QREF path, thus should only
 *             be used from within the kernel in a kernel level map <<<
 */

int
uvm_map_extract(srcmap, start, len, dstmap, dstaddrp, flags)
	vm_map_t srcmap, dstmap;
	vaddr_t start, *dstaddrp;
	vsize_t len;
	int flags;
{
	vaddr_t dstaddr, end, newend, oldoffset, fudge, orig_fudge,
	    oldstart;
	vm_map_entry_t chain, endchain, entry, orig_entry, newentry, deadentry;
	vm_map_entry_t oldentry;
	vsize_t elen;
	int nchain, error, copy_ok;
	UVMHIST_FUNC("uvm_map_extract"); UVMHIST_CALLED(maphist);
	UVMHIST_LOG(maphist,"(srcmap=0x%x,start=0x%x, len=0x%x", srcmap, start,
	    len,0);
	UVMHIST_LOG(maphist," ...,dstmap=0x%x, flags=0x%x)", dstmap,flags,0,0);

#ifdef DIAGNOSTIC
	/*
	 * step 0: sanity check: start must be on a page boundary, length
	 * must be page sized.  can't ask for CONTIG/QREF if you asked for
	 * REMOVE.
	 */
	if ((start & PAGE_MASK) || (len & PAGE_MASK))
		panic("uvm_map_extract1");
	if (flags & UVM_EXTRACT_REMOVE)
		if (flags & (UVM_EXTRACT_CONTIG|UVM_EXTRACT_QREF))
			panic("uvm_map_extract2");
#endif


	/*
	 * step 1: reserve space in the target map for the extracted area
	 */

	dstaddr = *dstaddrp;
	if (uvm_map_reserve(dstmap, len, start, &dstaddr) == FALSE)
		return(ENOMEM);
	*dstaddrp = dstaddr;	/* pass address back to caller */
	UVMHIST_LOG(maphist, "  dstaddr=0x%x", dstaddr,0,0,0);


	/*
	 * step 2: setup for the extraction process loop by init'ing the 
	 * map entry chain, locking src map, and looking up the first useful
	 * entry in the map.
	 */

	end = start + len;
	newend = dstaddr + len;
	chain = endchain = NULL;
	nchain = 0;
	vm_map_lock(srcmap);

	if (uvm_map_lookup_entry(srcmap, start, &entry)) {

		/* "start" is within an entry */
		if (flags & UVM_EXTRACT_QREF) {
			/*
			 * for quick references we don't clip the entry, so
			 * the entry may map space "before" the starting
			 * virtual address... this is the "fudge" factor
			 * (which can be non-zero only the first time
			 * through the "while" loop in step 3).
			 */
			fudge = start - entry->start;
		} else {
			/*
			 * normal reference: we clip the map to fit (thus
			 * fudge is zero)
			 */
			UVM_MAP_CLIP_START(srcmap, entry, start);
			SAVE_HINT(srcmap, entry->prev);
			fudge = 0;
		}

	} else {
		
		/* "start" is not within an entry ... skip to next entry */
		if (flags & UVM_EXTRACT_CONTIG) {
			error = EINVAL;
			goto bad;    /* definite hole here ... */
		}

		entry = entry->next;
		fudge = 0;
	}
	/* save values from srcmap for step 6 */
	orig_entry = entry;
	orig_fudge = fudge;


	/*
	 * step 3: now start looping through the map entries, extracting
	 * as we go.
	 */

	while (entry->start < end && entry != &srcmap->header) {
		
		/* if we are not doing a quick reference, clip it */
		if ((flags & UVM_EXTRACT_QREF) == 0)
			UVM_MAP_CLIP_END(srcmap, entry, end);

		/* clear needs_copy (allow chunking) */
		if (UVM_ET_ISNEEDSCOPY(entry)) {
			if (fudge)
				oldstart = entry->start;
			else
				oldstart = 0;	/* XXX: gcc */
			amap_copy(srcmap, entry, M_NOWAIT, TRUE, start, end);
			if (UVM_ET_ISNEEDSCOPY(entry)) {  /* failed? */
				error = ENOMEM;
				goto bad;
			}
			/* amap_copy could clip (during chunk)!  update fudge */
			if (fudge) {
				fudge = fudge - (entry->start - oldstart);
				orig_fudge = fudge;
			}
		}

		/* calculate the offset of this from "start" */
		oldoffset = (entry->start + fudge) - start;

		/* allocate a new map entry */
		newentry = uvm_mapent_alloc(dstmap);
		if (newentry == NULL) {
			error = ENOMEM;
			goto bad;
		}

		/* set up new map entry */
		newentry->next = NULL;
		newentry->prev = endchain;
		newentry->start = dstaddr + oldoffset;
		newentry->end =
		    newentry->start + (entry->end - (entry->start + fudge));
		if (newentry->end > newend)
			newentry->end = newend;
		newentry->object.uvm_obj = entry->object.uvm_obj;
		if (newentry->object.uvm_obj) {
			if (newentry->object.uvm_obj->pgops->pgo_reference)
				newentry->object.uvm_obj->pgops->
				    pgo_reference(newentry->object.uvm_obj);
				newentry->offset = entry->offset + fudge;
		} else {
			newentry->offset = 0;
		}
		newentry->etype = entry->etype;
		newentry->protection = (flags & UVM_EXTRACT_FIXPROT) ? 
			entry->max_protection : entry->protection; 
		newentry->max_protection = entry->max_protection;
		newentry->inheritance = entry->inheritance;
		newentry->wired_count = 0;
		newentry->aref.ar_amap = entry->aref.ar_amap;
		if (newentry->aref.ar_amap) {
			newentry->aref.ar_pageoff =
			    entry->aref.ar_pageoff + (fudge >> PAGE_SHIFT);
			amap_ref(newentry, AMAP_SHARED |
			    ((flags & UVM_EXTRACT_QREF) ? AMAP_REFALL : 0));
		} else {
			newentry->aref.ar_pageoff = 0;
		}
		newentry->advice = entry->advice;

		/* now link it on the chain */
		nchain++;
		if (endchain == NULL) {
			chain = endchain = newentry;
		} else {
			endchain->next = newentry;
			endchain = newentry;
		}

		/* end of 'while' loop! */
		if ((flags & UVM_EXTRACT_CONTIG) && entry->end < end && 
		    (entry->next == &srcmap->header ||
		    entry->next->start != entry->end)) {
			error = EINVAL;
			goto bad;
		}
		entry = entry->next;
		fudge = 0;
	}


	/*
	 * step 4: close off chain (in format expected by uvm_map_replace)
	 */

	if (chain)
		chain->prev = endchain;


	/*
	 * step 5: attempt to lock the dest map so we can pmap_copy.
	 * note usage of copy_ok: 
	 *   1 => dstmap locked, pmap_copy ok, and we "replace" here (step 5)
	 *   0 => dstmap unlocked, NO pmap_copy, and we will "replace" in step 7
	 */
	
	if (srcmap == dstmap || vm_map_lock_try(dstmap) == TRUE) {

		copy_ok = 1;
		if (!uvm_map_replace(dstmap, dstaddr, dstaddr+len, chain,
		    nchain)) {
			if (srcmap != dstmap)
				vm_map_unlock(dstmap);
			error = EIO;
			goto bad;
		}

	} else {

		copy_ok = 0;
		/* replace defered until step 7 */

	}

		
	/*
	 * step 6: traverse the srcmap a second time to do the following:
	 *  - if we got a lock on the dstmap do pmap_copy
	 *  - if UVM_EXTRACT_REMOVE remove the entries
	 * we make use of orig_entry and orig_fudge (saved in step 2)
	 */

	if (copy_ok || (flags & UVM_EXTRACT_REMOVE)) {

		/* purge possible stale hints from srcmap */
		if (flags & UVM_EXTRACT_REMOVE) {
			SAVE_HINT(srcmap, orig_entry->prev);
			if (srcmap->first_free->start >= start)
				srcmap->first_free = orig_entry->prev;
		}

		entry = orig_entry;
		fudge = orig_fudge;
		deadentry = NULL;	/* for UVM_EXTRACT_REMOVE */

		while (entry->start < end && entry != &srcmap->header) {

			if (copy_ok) {
	oldoffset = (entry->start + fudge) - start;
	elen = min(end, entry->end) - (entry->start + fudge);
	pmap_copy(dstmap->pmap, srcmap->pmap, dstaddr + oldoffset, 
		  elen, entry->start + fudge);
			}

      /* we advance "entry" in the following if statement */
			if (flags & UVM_EXTRACT_REMOVE) {
				pmap_remove(srcmap->pmap, entry->start, 
						entry->end);
        			oldentry = entry;	/* save entry */
        			entry = entry->next;	/* advance */
				uvm_map_entry_unlink(srcmap, oldentry);
							/* add to dead list */
				oldentry->next = deadentry;
				deadentry = oldentry;
      			} else {
        			entry = entry->next;		/* advance */
			}

			/* end of 'while' loop */
			fudge = 0;
		}

		/*
		 * unlock dstmap.  we will dispose of deadentry in
		 * step 7 if needed
		 */
		if (copy_ok && srcmap != dstmap)
			vm_map_unlock(dstmap);

	}
	else
		deadentry = NULL; /* XXX: gcc */

	/*
	 * step 7: we are done with the source map, unlock.   if copy_ok
	 * is 0 then we have not replaced the dummy mapping in dstmap yet
	 * and we need to do so now.
	 */

	vm_map_unlock(srcmap);
	if ((flags & UVM_EXTRACT_REMOVE) && deadentry)
		uvm_unmap_detach(deadentry, 0);   /* dispose of old entries */

	/* now do the replacement if we didn't do it in step 5 */
	if (copy_ok == 0) {
		vm_map_lock(dstmap);
		error = uvm_map_replace(dstmap, dstaddr, dstaddr+len, chain,
		    nchain);
		vm_map_unlock(dstmap);

		if (error == FALSE) {
			error = EIO;
			goto bad2;
		}
	}

	/*
	 * done!
	 */
	return(0);

	/*
	 * bad: failure recovery
	 */
bad:
	vm_map_unlock(srcmap);
bad2:			/* src already unlocked */
	if (chain)
		uvm_unmap_detach(chain,
		    (flags & UVM_EXTRACT_QREF) ? AMAP_REFALL : 0);
	uvm_unmap(dstmap, dstaddr, dstaddr+len);   /* ??? */
	return(error);
}

/* end of extraction functions */

/*
 * uvm_map_submap: punch down part of a map into a submap
 *
 * => only the kernel_map is allowed to be submapped
 * => the purpose of submapping is to break up the locking granularity
 *	of a larger map
 * => the range specified must have been mapped previously with a uvm_map()
 *	call [with uobj==NULL] to create a blank map entry in the main map.
 *	[And it had better still be blank!]
 * => maps which contain submaps should never be copied or forked.
 * => to remove a submap, use uvm_unmap() on the main map 
 *	and then uvm_map_deallocate() the submap.
 * => main map must be unlocked.
 * => submap must have been init'd and have a zero reference count.
 *	[need not be locked as we don't actually reference it]
 */
	 
int
uvm_map_submap(map, start, end, submap)
	vm_map_t map, submap;
	vaddr_t start, end;
{
	vm_map_entry_t entry;
	int result;
	UVMHIST_FUNC("uvm_map_submap"); UVMHIST_CALLED(maphist);

	vm_map_lock(map);

	VM_MAP_RANGE_CHECK(map, start, end);
 
	if (uvm_map_lookup_entry(map, start, &entry)) {
		UVM_MAP_CLIP_START(map, entry, start);
		UVM_MAP_CLIP_END(map, entry, end);		/* to be safe */
	}             
	else {
		entry = NULL;
	}

	if (entry != NULL && 
	    entry->start == start && entry->end == end &&
	    entry->object.uvm_obj == NULL && entry->aref.ar_amap == NULL &&
	    !UVM_ET_ISCOPYONWRITE(entry) && !UVM_ET_ISNEEDSCOPY(entry)) {
		
		/*
		 * doit!
		 */
		entry->etype |= UVM_ET_SUBMAP;
		entry->object.sub_map = submap;
		entry->offset = 0;
		uvm_map_reference(submap);
		result = KERN_SUCCESS;
	} else {
		result = KERN_INVALID_ARGUMENT;
	}
	vm_map_unlock(map);

	return(result);
}


/*
 * uvm_map_protect: change map protection
 *
 * => set_max means set max_protection.
 * => map must be unlocked.
 * => XXXCDC: does not work properly with share maps.  rethink.
 */

#define MASK(entry)     ( UVM_ET_ISCOPYONWRITE(entry) ? \
	~VM_PROT_WRITE : VM_PROT_ALL)
#define max(a,b)        ((a) > (b) ? (a) : (b))

int
uvm_map_protect(map, start, end, new_prot, set_max)
	vm_map_t map;
	vaddr_t start, end;
	vm_prot_t new_prot;
	boolean_t set_max;
{
	vm_map_entry_t current, entry;
	UVMHIST_FUNC("uvm_map_protect"); UVMHIST_CALLED(maphist);
	UVMHIST_LOG(maphist,"(map=0x%x,start=0x%x,end=0x%x,new_prot=0x%x)",
	map, start, end, new_prot);
	
	vm_map_lock(map);

	VM_MAP_RANGE_CHECK(map, start, end);
	
	if (uvm_map_lookup_entry(map, start, &entry)) {
		UVM_MAP_CLIP_START(map, entry, start);
	} else {
		entry = entry->next;
	}

	/*
	 * make a first pass to check for protection violations.
	 */

	current = entry;
	while ((current != &map->header) && (current->start < end)) {
		if (UVM_ET_ISSUBMAP(current))
			return(KERN_INVALID_ARGUMENT);
		if ((new_prot & current->max_protection) != new_prot) {
			vm_map_unlock(map);
			return(KERN_PROTECTION_FAILURE);
		}
			current = current->next;
	}

	/* go back and fix up protections (no need to clip this time). */

	current = entry;

	while ((current != &map->header) && (current->start < end)) {
		vm_prot_t old_prot;
		
		UVM_MAP_CLIP_END(map, current, end);

		old_prot = current->protection;
		if (set_max)
			current->protection =
			    (current->max_protection = new_prot) & old_prot;
		else
			current->protection = new_prot;

		/*
		 * update physical map if necessary.  worry about copy-on-write 
		 * here -- CHECK THIS XXX
		 */

		if (current->protection != old_prot) {

			/* update pmap! */
			pmap_protect(map->pmap, current->start, current->end,
			    current->protection & MASK(entry));

		}
		current = current->next;
	}
	
	vm_map_unlock(map);
	UVMHIST_LOG(maphist, "<- done",0,0,0,0);
	return(KERN_SUCCESS);
}

#undef  max
#undef  MASK

/* 
 * uvm_map_inherit: set inheritance code for range of addrs in map.
 *
 * => map must be unlocked
 * => note that the inherit code is used during a "fork".  see fork
 *	code for details.
 * => XXXCDC: currently only works in main map.  what about share map?
 */

int
uvm_map_inherit(map, start, end, new_inheritance)
	vm_map_t map;
	vaddr_t start;
	vaddr_t end;
	vm_inherit_t new_inheritance;
{
	vm_map_entry_t entry, temp_entry;
	UVMHIST_FUNC("uvm_map_inherit"); UVMHIST_CALLED(maphist);
	UVMHIST_LOG(maphist,"(map=0x%x,start=0x%x,end=0x%x,new_inh=0x%x)",
	    map, start, end, new_inheritance);

	switch (new_inheritance) {
	case VM_INHERIT_NONE:
	case VM_INHERIT_COPY:
	case VM_INHERIT_SHARE:
		break;
	default:
		UVMHIST_LOG(maphist,"<- done (INVALID ARG)",0,0,0,0);
		return(KERN_INVALID_ARGUMENT);
	}

	vm_map_lock(map);
	
	VM_MAP_RANGE_CHECK(map, start, end);
	
	if (uvm_map_lookup_entry(map, start, &temp_entry)) {
		entry = temp_entry;
		UVM_MAP_CLIP_START(map, entry, start);
	}  else {
		entry = temp_entry->next;
	}
	
	while ((entry != &map->header) && (entry->start < end)) {
		UVM_MAP_CLIP_END(map, entry, end);

		entry->inheritance = new_inheritance;
		
		entry = entry->next;
	}

	vm_map_unlock(map);
	UVMHIST_LOG(maphist,"<- done (OK)",0,0,0,0);
	return(KERN_SUCCESS);
}

/*
 * uvm_map_pageable: sets the pageability of a range in a map.
 *
 * => regions sepcified as not pageable require lock-down (wired) memory
 *	and page tables.
 * => map must not be locked.
 * => XXXCDC: check this and try and clean it up.
 */

int
uvm_map_pageable(map, start, end, new_pageable)
	vm_map_t map;
	vaddr_t start, end;
	boolean_t new_pageable;
{
	vm_map_entry_t entry, start_entry;
	vaddr_t failed = 0;
	int rv;
	UVMHIST_FUNC("uvm_map_pageable"); UVMHIST_CALLED(maphist);
	UVMHIST_LOG(maphist,"(map=0x%x,start=0x%x,end=0x%x,new_pageable=0x%x)",
	map, start, end, new_pageable);

	vm_map_lock(map);
	VM_MAP_RANGE_CHECK(map, start, end);

	/* 
	 * only one pageability change may take place at one time, since
	 * uvm_fault_wire assumes it will be called only once for each
	 * wiring/unwiring.  therefore, we have to make sure we're actually
	 * changing the pageability for the entire region.  we do so before
	 * making any changes.  
	 */

	if (uvm_map_lookup_entry(map, start, &start_entry) == FALSE) {
		vm_map_unlock(map);
	 
		UVMHIST_LOG(maphist,"<- done (INVALID ARG)",0,0,0,0);
		return (KERN_INVALID_ADDRESS);
	}
	entry = start_entry;

	/* 
	 * handle wiring and unwiring seperately.
	 */

	if (new_pageable) {               /* unwire */

		UVM_MAP_CLIP_START(map, entry, start);

		/*
		 * unwiring.  first ensure that the range to be unwired is
		 * really wired down and that there are no holes.  
		 */
		while ((entry != &map->header) && (entry->start < end)) {
			
			if (entry->wired_count == 0 ||
			    (entry->end < end &&
			    (entry->next == &map->header ||
			    entry->next->start > entry->end))) {
				vm_map_unlock(map);
				UVMHIST_LOG(maphist,
				    "<- done (INVALID UNWIRE ARG)",0,0,0,0);
				return (KERN_INVALID_ARGUMENT);
			}
			entry = entry->next;
		}

		/* 
		 * now decrement the wiring count for each region.  if a region
		 * becomes completely unwired, unwire its physical pages and
		 * mappings.
		 */
#if 0		/* not necessary: uvm_fault_unwire does not lock */
		lock_set_recursive(&map->lock);
#endif  /* XXXCDC */

		entry = start_entry;
		while ((entry != &map->header) && (entry->start < end)) {
			UVM_MAP_CLIP_END(map, entry, end);
			
			entry->wired_count--;
			if (entry->wired_count == 0)
				uvm_map_entry_unwire(map, entry);
			
			entry = entry->next;
		}
#if 0 /* XXXCDC: not necessary, see above */
		lock_clear_recursive(&map->lock);
#endif
		vm_map_unlock(map);
		UVMHIST_LOG(maphist,"<- done (OK UNWIRE)",0,0,0,0);
		return(KERN_SUCCESS);

		/*
		 * end of unwire case!
		 */
	}

	/*
	 * wire case: in two passes [XXXCDC: ugly block of code here]
	 *
	 * 1: holding the write lock, we create any anonymous maps that need
	 *    to be created.  then we clip each map entry to the region to
	 *    be wired and increment its wiring count.  
	 *
	 * 2: we downgrade to a read lock, and call uvm_fault_wire to fault
	 *    in the pages for any newly wired area (wired_count is 1).
	 *
	 *    downgrading to a read lock for uvm_fault_wire avoids a possible
	 *    deadlock with another thread that may have faulted on one of
	 *    the pages to be wired (it would mark the page busy, blocking
	 *    us, then in turn block on the map lock that we hold).  because
	 *    of problems in the recursive lock package, we cannot upgrade
	 *    to a write lock in vm_map_lookup.  thus, any actions that
	 *    require the write lock must be done beforehand.  because we
	 *    keep the read lock on the map, the copy-on-write status of the
	 *    entries we modify here cannot change.
	 */

	while ((entry != &map->header) && (entry->start < end)) {

		if (entry->wired_count == 0) {  /* not already wired? */
			
			/* 
			 * perform actions of vm_map_lookup that need the
			 * write lock on the map: create an anonymous map
			 * for a copy-on-write region, or an anonymous map
			 * for a zero-fill region.  (XXXCDC: submap case
			 * ok?)
			 */
			
			if (!UVM_ET_ISSUBMAP(entry)) {  /* not submap */
				/*
				 * XXXCDC: protection vs. max_protection??
				 * (wirefault uses max?)
				 * XXXCDC: used to do it always if
				 * uvm_obj == NULL (wrong?)
				 */
				if ( UVM_ET_ISNEEDSCOPY(entry) && 
				    (entry->protection & VM_PROT_WRITE) != 0) {
					amap_copy(map, entry, M_WAITOK, TRUE,
					    start, end); 
					/* XXXCDC: wait OK? */
				}
			}
		}     /* wired_count == 0 */
		UVM_MAP_CLIP_START(map, entry, start);
		UVM_MAP_CLIP_END(map, entry, end);
		entry->wired_count++;

		/*
		 * Check for holes 
		 */
		if (entry->end < end && (entry->next == &map->header ||
			     entry->next->start > entry->end)) {
			/*
			 * found one.  amap creation actions do not need to
			 * be undone, but the wired counts need to be restored. 
			 */
			while (entry != &map->header && entry->end > start) {
				entry->wired_count--;
				entry = entry->prev;
			}
			vm_map_unlock(map);
			UVMHIST_LOG(maphist,"<- done (INVALID WIRE)",0,0,0,0);
			return(KERN_INVALID_ARGUMENT);
		}
		entry = entry->next;
	}

	/*
	 * Pass 2.
	 */
	/*
	 * HACK HACK HACK HACK
	 *
	 * if we are wiring in the kernel map or a submap of it, unlock the
	 * map to avoid deadlocks.  we trust that the kernel threads are
	 * well-behaved, and therefore will not do anything destructive to
	 * this region of the map while we have it unlocked.  we cannot
	 * trust user threads to do the same.
	 *
	 * HACK HACK HACK HACK 
	 */
	if (vm_map_pmap(map) == pmap_kernel()) {
		vm_map_unlock(map);         /* trust me ... */
	} else {
		vm_map_set_recursive(&map->lock);
		lockmgr(&map->lock, LK_DOWNGRADE, (void *)0, curproc /*XXX*/);
	}

	rv = 0;
	entry = start_entry;
	while (entry != &map->header && entry->start < end) {
		/*
		 * if uvm_fault_wire fails for any page we need to undo what has
		 * been done.  we decrement the wiring count for those pages
		 * which have not yet been wired (now) and unwire those that
		 * have * (later).
		 *
		 * XXX this violates the locking protocol on the map, needs to
		 * be fixed.  [because we only have a read lock on map we 
		 * shouldn't be changing wired_count?]
		 */
		if (rv) {
			entry->wired_count--;
		} else if (entry->wired_count == 1) {
			rv = uvm_fault_wire(map, entry->start, entry->end);
			if (rv) {
				failed = entry->start;
				entry->wired_count--;
			}
		}
		entry = entry->next;
	}

	if (vm_map_pmap(map) == pmap_kernel()) {
		vm_map_lock(map);     /* relock */
	} else {
		vm_map_clear_recursive(&map->lock);
	} 

	if (rv) {        /* failed? */
		vm_map_unlock(map);
		(void) uvm_map_pageable(map, start, failed, TRUE);
		UVMHIST_LOG(maphist, "<- done (RV=%d)", rv,0,0,0);
		return(rv);
	}
	vm_map_unlock(map);
	
	UVMHIST_LOG(maphist,"<- done (OK WIRE)",0,0,0,0);
	return(KERN_SUCCESS);
}

/*
 * uvm_map_clean: push dirty pages off to backing store.
 *
 * => valid flags:
 *   if (flags & PGO_SYNCIO): dirty pages are written synchronously
 *   if (flags & PGO_DEACTIVATE): any cached pages are deactivated after clean
 *   if (flags & PGO_FREE): any cached pages are freed after clean
 * => returns an error if any part of the specified range isn't mapped
 * => never a need to flush amap layer since the anonymous memory has 
 *	no permanent home...
 * => called from sys_msync()
 * => caller must not write-lock map (read OK).
 * => we may sleep while cleaning if SYNCIO [with map read-locked]
 * => XXX: does this handle share maps properly?
 */

int
uvm_map_clean(map, start, end, flags)
	vm_map_t map;
	vaddr_t start, end;
	int flags;
{
	vm_map_entry_t current;
	vm_map_entry_t entry;
	vsize_t size;
	struct uvm_object *object;
	vaddr_t offset;
	UVMHIST_FUNC("uvm_map_clean"); UVMHIST_CALLED(maphist);
	UVMHIST_LOG(maphist,"(map=0x%x,start=0x%x,end=0x%x,flags=0x%x)",
	map, start, end, flags);

	vm_map_lock_read(map);
	VM_MAP_RANGE_CHECK(map, start, end);
	if (!uvm_map_lookup_entry(map, start, &entry)) {
		vm_map_unlock_read(map);
		return(KERN_INVALID_ADDRESS);
	}

	/*
	 * Make a first pass to check for holes.
	 */
	for (current = entry; current->start < end; current = current->next) {
		if (UVM_ET_ISSUBMAP(current)) {
			vm_map_unlock_read(map);
			return(KERN_INVALID_ARGUMENT);
		}
		if (end > current->end && (current->next == &map->header ||
		    current->end != current->next->start)) {
			vm_map_unlock_read(map);
			return(KERN_INVALID_ADDRESS);
		}
	}

	/* 
	 * add "cleanit" flag to flags (for generic flush routine).  
	 * then make a second pass, cleaning/uncaching pages from 
	 * the indicated objects as we go.  
	 */
	flags = flags | PGO_CLEANIT;
	for (current = entry; current->start < end; current = current->next) {
		offset = current->offset + (start - current->start);
		size = (end <= current->end ? end : current->end) - start;

		/*
		 * get object/offset.  can't be submap (checked above).
		 */
		object = current->object.uvm_obj;
		simple_lock(&object->vmobjlock);

		/*
		 * flush pages if we've got a valid backing object.
		 * note that object is locked.
		 * XXX should we continue on an error?
		 */

		if (object && object->pgops) {
			if (!object->pgops->pgo_flush(object, offset,
			    offset+size, flags)) {
				simple_unlock(&object->vmobjlock);
				vm_map_unlock_read(map);
				return (KERN_FAILURE);
			}
		}
		simple_unlock(&object->vmobjlock);
		start += size;
	}
	vm_map_unlock_read(map);
	return(KERN_SUCCESS); 
}


/*
 * uvm_map_checkprot: check protection in map
 *
 * => must allow specified protection in a fully allocated region.
 * => map must be read or write locked by caller.
 */

boolean_t
uvm_map_checkprot(map, start, end, protection)
	vm_map_t       map;
	vaddr_t    start, end;
	vm_prot_t      protection;
{
	 vm_map_entry_t entry;
	 vm_map_entry_t tmp_entry;

	 if (!uvm_map_lookup_entry(map, start, &tmp_entry)) {
		 return(FALSE);
	 }

	 entry = tmp_entry;
	 
	 while (start < end) {
		 if (entry == &map->header) {
			 return(FALSE);
		 }
		 
		/*
		 * no holes allowed
		 */

		 if (start < entry->start) {
			 return(FALSE);
		 }

		/*
		 * check protection associated with entry
		 */

		 if ((entry->protection & protection) != protection) {
			 return(FALSE);
		 }

		 /* go to next entry */
		 
		 start = entry->end;
		 entry = entry->next;
	 }
	 return(TRUE);
}

/*
 * uvmspace_alloc: allocate a vmspace structure.
 *
 * - structure includes vm_map and pmap
 * - XXX: no locking on this structure
 * - refcnt set to 1, rest must be init'd by caller
 */
struct vmspace *
uvmspace_alloc(min, max, pageable)
	vaddr_t min, max;
	int pageable;
{
	struct vmspace *vm;
	UVMHIST_FUNC("uvmspace_alloc"); UVMHIST_CALLED(maphist);

	vm = pool_get(&uvm_vmspace_pool, PR_WAITOK);
	uvmspace_init(vm, NULL, min, max, pageable);
	UVMHIST_LOG(maphist,"<- done (vm=0x%x)", vm,0,0,0);
	return (vm);
}

/*
 * uvmspace_init: initialize a vmspace structure.
 *
 * - XXX: no locking on this structure
 * - refcnt set to 1, rest must me init'd by caller
 */
void
uvmspace_init(vm, pmap, min, max, pageable)
	struct vmspace *vm;
	struct pmap *pmap;
	vaddr_t min, max;
	boolean_t pageable;
{
	UVMHIST_FUNC("uvmspace_init"); UVMHIST_CALLED(maphist);

	bzero(vm, sizeof(*vm));

	uvm_map_setup(&vm->vm_map, min, max, pageable);

	if (pmap)
		pmap_reference(pmap);
	else
#if defined(PMAP_NEW)
		pmap = pmap_create();
#else
		pmap = pmap_create(0);
#endif
	vm->vm_map.pmap = pmap;

	vm->vm_refcnt = 1;
	UVMHIST_LOG(maphist,"<- done",0,0,0,0);
}

/*
 * uvmspace_share: share a vmspace between two proceses
 *
 * - XXX: no locking on vmspace
 * - used for vfork, threads(?)
 */

void
uvmspace_share(p1, p2)
	struct proc *p1, *p2;
{
	p2->p_vmspace = p1->p_vmspace;
	p1->p_vmspace->vm_refcnt++;
}

/*
 * uvmspace_unshare: ensure that process "p" has its own, unshared, vmspace
 *
 * - XXX: no locking on vmspace
 */

void
uvmspace_unshare(p)
	struct proc *p; 
{
	struct vmspace *nvm, *ovm = p->p_vmspace;
	int s;
 
	if (ovm->vm_refcnt == 1)
		/* nothing to do: vmspace isn't shared in the first place */
		return;
 
	/* make a new vmspace, still holding old one */
	nvm = uvmspace_fork(ovm);

	s = splhigh();			/* make this `atomic' */
	pmap_deactivate(p);
					/* unbind old vmspace */
	p->p_vmspace = nvm; 
	pmap_activate(p);
					/* switch to new vmspace */
	splx(s);			/* end of critical section */

	uvmspace_free(ovm);		/* drop reference to old vmspace */
}

/*
 * uvmspace_exec: the process wants to exec a new program
 *
 * - XXX: no locking on vmspace
 */

void
uvmspace_exec(p)
	struct proc *p;
{
	struct vmspace *nvm, *ovm = p->p_vmspace;
	vm_map_t map = &ovm->vm_map;
	int s;

#ifdef sparc
	/* XXX cgd 960926: the sparc #ifdef should be a MD hook */
	kill_user_windows(p);   /* before stack addresses go away */
#endif

	/*
	 * see if more than one process is using this vmspace...
	 */

	if (ovm->vm_refcnt == 1) {

		/*
		 * if p is the only process using its vmspace then we can safely
		 * recycle that vmspace for the program that is being exec'd.
		 */

#ifdef SYSVSHM
		/*
		 * SYSV SHM semantics require us to kill all segments on an exec
		 */
		if (ovm->vm_shm)
			shmexit(ovm);
#endif

		/*
		 * now unmap the old program
		 */
		uvm_unmap(map, VM_MIN_ADDRESS, VM_MAXUSER_ADDRESS);

	} else {

		/*
		 * p's vmspace is being shared, so we can't reuse it for p since
		 * it is still being used for others.   allocate a new vmspace
		 * for p
		 */
		nvm = uvmspace_alloc(map->min_offset, map->max_offset, 
			 map->entries_pageable);

#if (defined(i386) || defined(pc532)) && !defined(PMAP_NEW)
		/* 
		 * allocate zero fill area in the new vmspace's map for user
		 * page tables for ports that have old style pmaps that keep
		 * user page tables in the top part of the process' address
		 * space.
		 *
		 * XXXCDC: this should go away once all pmaps are fixed
		 */
		{ 
			vaddr_t addr = VM_MAXUSER_ADDRESS;
			if (uvm_map(&nvm->vm_map, &addr, VM_MAX_ADDRESS - addr,
			    NULL, UVM_UNKNOWN_OFFSET, UVM_MAPFLAG(UVM_PROT_ALL,
			    UVM_PROT_ALL, UVM_INH_NONE, UVM_ADV_NORMAL,
			    UVM_FLAG_FIXED|UVM_FLAG_COPYONW)) != KERN_SUCCESS)
				panic("vm_allocate of PT page area failed");
		}
#endif

		/*
		 * install new vmspace and drop our ref to the old one.
		 */

		s = splhigh();
		pmap_deactivate(p);
		p->p_vmspace = nvm;
		pmap_activate(p);
		splx(s);

		uvmspace_free(ovm);
	}
}

/*
 * uvmspace_free: free a vmspace data structure
 *
 * - XXX: no locking on vmspace
 */

void
uvmspace_free(vm)
	struct vmspace *vm;
{
	vm_map_entry_t dead_entries;
	UVMHIST_FUNC("uvmspace_free"); UVMHIST_CALLED(maphist);

	UVMHIST_LOG(maphist,"(vm=0x%x) ref=%d", vm, vm->vm_refcnt,0,0);
	if (--vm->vm_refcnt == 0) {
		/*
		 * lock the map, to wait out all other references to it.  delete
		 * all of the mappings and pages they hold, then call the pmap
		 * module to reclaim anything left.
		 */
		vm_map_lock(&vm->vm_map);
		if (vm->vm_map.nentries) {
			(void)uvm_unmap_remove(&vm->vm_map,
			    vm->vm_map.min_offset, vm->vm_map.max_offset,
			    &dead_entries);
			if (dead_entries != NULL)
				uvm_unmap_detach(dead_entries, 0);
		}
		pmap_destroy(vm->vm_map.pmap);
		vm->vm_map.pmap = NULL;
		pool_put(&uvm_vmspace_pool, vm);
	}
	UVMHIST_LOG(maphist,"<- done", 0,0,0,0);
}

/*
 *   F O R K   -   m a i n   e n t r y   p o i n t
 */
/*
 * uvmspace_fork: fork a process' main map
 *
 * => create a new vmspace for child process from parent.
 * => parent's map must not be locked.
 */

struct vmspace *
uvmspace_fork(vm1)
	struct vmspace *vm1;
{
	struct vmspace *vm2;
	vm_map_t        old_map = &vm1->vm_map;
	vm_map_t        new_map;
	vm_map_entry_t  old_entry;
	vm_map_entry_t  new_entry;
	pmap_t          new_pmap;
	boolean_t	protect_child;
	UVMHIST_FUNC("uvmspace_fork"); UVMHIST_CALLED(maphist);

#if (defined(i386) || defined(pc532)) && !defined(PMAP_NEW)
	/*    
	 * avoid copying any of the parent's pagetables or other per-process
	 * objects that reside in the map by marking all of them non-inheritable
	 * XXXCDC: should go away
	 */
	(void) uvm_map_inherit(old_map, VM_MAXUSER_ADDRESS, VM_MAX_ADDRESS, 
			 VM_INHERIT_NONE);
#endif

	vm_map_lock(old_map);

	vm2 = uvmspace_alloc(old_map->min_offset, old_map->max_offset,
		      old_map->entries_pageable);
	bcopy(&vm1->vm_startcopy, &vm2->vm_startcopy,
	(caddr_t) (vm1 + 1) - (caddr_t) &vm1->vm_startcopy);
	new_map = &vm2->vm_map;		  /* XXX */
	new_pmap = new_map->pmap;

	old_entry = old_map->header.next;

	/*
	 * go entry-by-entry
	 */

	while (old_entry != &old_map->header) {

		/*
		 * first, some sanity checks on the old entry
		 */
		if (UVM_ET_ISSUBMAP(old_entry))
		    panic("fork: encountered a submap during fork (illegal)");

		if (!UVM_ET_ISCOPYONWRITE(old_entry) &&
			    UVM_ET_ISNEEDSCOPY(old_entry))
	panic("fork: non-copy_on_write map entry marked needs_copy (illegal)");


		switch (old_entry->inheritance) {
		case VM_INHERIT_NONE:
			/*
			 * drop the mapping
			 */
			break;

		case VM_INHERIT_SHARE:
			/*
			 * share the mapping: this means we want the old and
			 * new entries to share amaps and backing objects.
			 */

			/*
			 * if the old_entry needs a new amap (due to prev fork)
			 * then we need to allocate it now so that we have
			 * something we own to share with the new_entry.   [in
			 * other words, we need to clear needs_copy]
			 */

			if (UVM_ET_ISNEEDSCOPY(old_entry)) {
				/* get our own amap, clears needs_copy */
				amap_copy(old_map, old_entry, M_WAITOK, FALSE,
				    0, 0); 
				/* XXXCDC: WAITOK??? */
			}

			new_entry = uvm_mapent_alloc(new_map);
			/* old_entry -> new_entry */
			uvm_mapent_copy(old_entry, new_entry);

			/* new pmap has nothing wired in it */
			new_entry->wired_count = 0;

			/*
			 * gain reference to object backing the map (can't
			 * be a submap, already checked this case).
			 */
			if (new_entry->aref.ar_amap)
				/* share reference */
				amap_ref(new_entry, AMAP_SHARED);

			if (new_entry->object.uvm_obj &&
			    new_entry->object.uvm_obj->pgops->pgo_reference)
				new_entry->object.uvm_obj->
				    pgops->pgo_reference(
				        new_entry->object.uvm_obj);

			/* insert entry at end of new_map's entry list */
			uvm_map_entry_link(new_map, new_map->header.prev,
			    new_entry);

			/* 
			 * pmap_copy the mappings: this routine is optional
			 * but if it is there it will reduce the number of
			 * page faults in the new proc.
			 */

			pmap_copy(new_pmap, old_map->pmap, new_entry->start,
			    (old_entry->end - old_entry->start),
			    old_entry->start);

			break;

		case VM_INHERIT_COPY:

			/*
			 * copy-on-write the mapping (using mmap's
			 * MAP_PRIVATE semantics)
			 *
			 * allocate new_entry, adjust reference counts.  
			 * (note that new references are read-only).
			 */

			new_entry = uvm_mapent_alloc(new_map);
			/* old_entry -> new_entry */
			uvm_mapent_copy(old_entry, new_entry);

			if (new_entry->aref.ar_amap)
				amap_ref(new_entry, 0);

			if (new_entry->object.uvm_obj &&
			    new_entry->object.uvm_obj->pgops->pgo_reference)
				new_entry->object.uvm_obj->pgops->pgo_reference
				    (new_entry->object.uvm_obj);

			/* new pmap has nothing wired in it */
			new_entry->wired_count = 0;

			new_entry->etype |=
			    (UVM_ET_COPYONWRITE|UVM_ET_NEEDSCOPY);
			uvm_map_entry_link(new_map, new_map->header.prev,
			    new_entry);
			
			/*
			 * the new entry will need an amap.  it will either
			 * need to be copied from the old entry or created
			 * from scratch (if the old entry does not have an
			 * amap).  can we defer this process until later
			 * (by setting "needs_copy") or do we need to copy
			 * the amap now?
			 *
			 * we must copy the amap now if any of the following
			 * conditions hold:
			 * 1. the old entry has an amap and that amap is
			 *    being shared.  this means that the old (parent)
			 *    process is sharing the amap with another 
			 *    process.  if we do not clear needs_copy here
			 *    we will end up in a situation where both the
			 *    parent and child process are refering to the
			 *    same amap with "needs_copy" set.  if the 
			 *    parent write-faults, the fault routine will
			 *    clear "needs_copy" in the parent by allocating
			 *    a new amap.   this is wrong because the 
			 *    parent is supposed to be sharing the old amap
			 *    and the new amap will break that.
			 *
			 * 2. if the old entry has an amap and a non-zero
			 *    wire count then we are going to have to call
			 *    amap_cow_now to avoid page faults in the 
			 *    parent process.   since amap_cow_now requires
			 *    "needs_copy" to be clear we might as well
			 *    clear it here as well.
			 *
			 */

			if (old_entry->aref.ar_amap != NULL) {

			  if ((amap_flags(old_entry->aref.ar_amap) & 
			       AMAP_SHARED) != 0 ||
			      old_entry->wired_count != 0) {

			    amap_copy(new_map, new_entry, M_WAITOK, FALSE,
				      0, 0);
			    /* XXXCDC: M_WAITOK ... ok? */
			  }
			}
			
			/*
			 * if the parent's entry is wired down, then the
			 * parent process does not want page faults on
			 * access to that memory.  this means that we
			 * cannot do copy-on-write because we can't write
			 * protect the old entry.   in this case we
			 * resolve all copy-on-write faults now, using
			 * amap_cow_now.   note that we have already
			 * allocated any needed amap (above).
			 */

			if (old_entry->wired_count != 0) {

			  /* 
			   * resolve all copy-on-write faults now
			   * (note that there is nothing to do if 
			   * the old mapping does not have an amap).
			   * XXX: is it worthwhile to bother with pmap_copy
			   * in this case?
			   */
			  if (old_entry->aref.ar_amap)
			    amap_cow_now(new_map, new_entry);

			} else { 

			  /*
			   * setup mappings to trigger copy-on-write faults
			   * we must write-protect the parent if it has
			   * an amap and it is not already "needs_copy"...
			   * if it is already "needs_copy" then the parent
			   * has already been write-protected by a previous
			   * fork operation.
			   *
			   * if we do not write-protect the parent, then
			   * we must be sure to write-protect the child
			   * after the pmap_copy() operation.
			   *
			   * XXX: pmap_copy should have some way of telling
			   * us that it didn't do anything so we can avoid
			   * calling pmap_protect needlessly.
			   */

			  if (old_entry->aref.ar_amap) {

			    if (!UVM_ET_ISNEEDSCOPY(old_entry)) {
			      if (old_entry->max_protection & VM_PROT_WRITE) {
				pmap_protect(old_map->pmap,
					     old_entry->start,
					     old_entry->end,
					     old_entry->protection &
					     ~VM_PROT_WRITE);
			      }
			      old_entry->etype |= UVM_ET_NEEDSCOPY;
			    }

			    /*
			     * parent must now be write-protected
			     */
			    protect_child = FALSE;
			  } else {

			    /*
			     * we only need to protect the child if the 
			     * parent has write access.
			     */
			    if (old_entry->max_protection & VM_PROT_WRITE)
			      protect_child = TRUE;
			    else
			      protect_child = FALSE;

			  }

			  /*
			   * copy the mappings
			   * XXX: need a way to tell if this does anything
			   */

			  pmap_copy(new_pmap, old_map->pmap,
				    new_entry->start,
				    (old_entry->end - old_entry->start),
				    old_entry->start);
			  
			  /*
			   * protect the child's mappings if necessary
			   */
			  if (protect_child) {
			    pmap_protect(new_pmap, new_entry->start,
					 new_entry->end, 
					 new_entry->protection & 
					          ~VM_PROT_WRITE);
			  }

			}
			break;
		}  /* end of switch statement */
		old_entry = old_entry->next;
	}

	new_map->size = old_map->size;
	vm_map_unlock(old_map); 

#if (defined(i386) || defined(pc532)) && !defined(PMAP_NEW)
	/* 
	 * allocate zero fill area in the new vmspace's map for user
	 * page tables for ports that have old style pmaps that keep
	 * user page tables in the top part of the process' address
	 * space.
	 *
	 * XXXCDC: this should go away once all pmaps are fixed
	 */
	{
		vaddr_t addr = VM_MAXUSER_ADDRESS;
		if (uvm_map(new_map, &addr, VM_MAX_ADDRESS - addr, NULL,
		    UVM_UNKNOWN_OFFSET, UVM_MAPFLAG(UVM_PROT_ALL,
		    UVM_PROT_ALL, UVM_INH_NONE, UVM_ADV_NORMAL,
		    UVM_FLAG_FIXED|UVM_FLAG_COPYONW)) != KERN_SUCCESS)
			panic("vm_allocate of PT page area failed");
	}
#endif

#ifdef SYSVSHM
	if (vm1->vm_shm)
		shmfork(vm1, vm2);
#endif

	UVMHIST_LOG(maphist,"<- done",0,0,0,0);
	return(vm2);    
}


#if defined(DDB)

/*
 * DDB hooks
 */

/*
 * uvm_map_print: print out a map 
 */

void
uvm_map_print(map, full)
	vm_map_t map;
	boolean_t full;
{

	uvm_map_printit(map, full, printf);
}

/*
 * uvm_map_printit: actually prints the map
 */

void
uvm_map_printit(map, full, pr)
	vm_map_t map;
	boolean_t full;
	int (*pr) __P((const char *, ...));
{
	vm_map_entry_t entry;

	(*pr)("MAP %p: [0x%lx->0x%lx]\n", map, map->min_offset,map->max_offset);
	(*pr)("\t#ent=%d, sz=%d, ref=%d, version=%d\n",
	    map->nentries, map->size, map->ref_count, map->timestamp);
#ifdef pmap_resident_count
	(*pr)("\tpmap=%p(resident=%d)\n", map->pmap, 
	    pmap_resident_count(map->pmap));
#else
	/* XXXCDC: this should be required ... */
	(*pr)("\tpmap=%p(resident=<<NOT SUPPORTED!!!>>)\n", map->pmap);
#endif
	if (!full)
		return;
	for (entry = map->header.next; entry != &map->header;
	    entry = entry->next) {
		(*pr)(" - %p: 0x%lx->0x%lx: obj=%p/0x%x, amap=%p/%d\n",
		    entry, entry->start, entry->end, entry->object.uvm_obj,
		    entry->offset, entry->aref.ar_amap, entry->aref.ar_pageoff);
		(*pr)(
"\tsubmap=%c, cow=%c, nc=%c, prot(max)=%d/%d, inh=%d, wc=%d, adv=%d\n",
		    (entry->etype & UVM_ET_SUBMAP) ? 'T' : 'F',
		    (entry->etype & UVM_ET_COPYONWRITE) ? 'T' : 'F', 
		    (entry->etype & UVM_ET_NEEDSCOPY) ? 'T' : 'F',
		    entry->protection, entry->max_protection,
		    entry->inheritance, entry->wired_count, entry->advice);
	}
} 

/*
 * uvm_object_print: print out an object 
 */

void
uvm_object_print(uobj, full)
	struct uvm_object *uobj;
	boolean_t full;
{

	uvm_object_printit(uobj, full, printf);
}

/*
 * uvm_object_printit: actually prints the object
 */

void
uvm_object_printit(uobj, full, pr)
	struct uvm_object *uobj;
	boolean_t full;
	int (*pr) __P((const char *, ...));
{
	struct vm_page *pg;
	int cnt = 0;

	(*pr)("OBJECT %p: pgops=%p, npages=%d, ", uobj, uobj->pgops,
	    uobj->uo_npages);
	if (uobj->uo_refs == UVM_OBJ_KERN)
		(*pr)("refs=<SYSTEM>\n");
	else
		(*pr)("refs=%d\n", uobj->uo_refs);

	if (!full) return;
	(*pr)("  PAGES <pg,offset>:\n  ");
	for (pg = uobj->memq.tqh_first ; pg ; pg = pg->listq.tqe_next, cnt++) {
		(*pr)("<%p,0x%lx> ", pg, pg->offset);
		if ((cnt % 3) == 2) (*pr)("\n  ");
	}
	if ((cnt % 3) != 2) (*pr)("\n");
} 

/*
 * uvm_page_print: print out a page
 */

void
uvm_page_print(pg, full)
	struct vm_page *pg;
	boolean_t full;
{

	uvm_page_printit(pg, full, printf);
}

/*
 * uvm_page_printit: actually print the page
 */

void
uvm_page_printit(pg, full, pr)
	struct vm_page *pg;
	boolean_t full;
	int (*pr) __P((const char *, ...));
{
	struct vm_page *lcv;
	struct uvm_object *uobj;
	struct pglist *pgl;

	(*pr)("PAGE %p:\n", pg);
	(*pr)("  flags=0x%x, pqflags=0x%x, vers=%d, wire_count=%d, pa=0x%lx\n", 
	pg->flags, pg->pqflags, pg->version, pg->wire_count, (long)pg->phys_addr);
	(*pr)("  uobject=%p, uanon=%p, offset=0x%lx loan_count=%d\n", 
	pg->uobject, pg->uanon, pg->offset, pg->loan_count);
#if defined(UVM_PAGE_TRKOWN)
	if (pg->flags & PG_BUSY)
		(*pr)("  owning process = %d, tag=%s\n",
		    pg->owner, pg->owner_tag);
	else
		(*pr)("  page not busy, no owner\n");
#else
	(*pr)("  [page ownership tracking disabled]\n");
#endif

	if (!full)
		return;

	/* cross-verify object/anon */
	if ((pg->pqflags & PQ_FREE) == 0) {
		if (pg->pqflags & PQ_ANON) {
			if (pg->uanon == NULL || pg->uanon->u.an_page != pg)
			    (*pr)("  >>> ANON DOES NOT POINT HERE <<< (%p)\n", 
				(pg->uanon) ? pg->uanon->u.an_page : NULL);
			else
				(*pr)("  anon backpointer is OK\n");
		} else {
			uobj = pg->uobject;
			if (uobj) {
				(*pr)("  checking object list\n");
				for (lcv = uobj->memq.tqh_first ; lcv ;
				    lcv = lcv->listq.tqe_next) {
					if (lcv == pg) break;
				}
				if (lcv)
					(*pr)("  page found on object list\n");
				else
			(*pr)("  >>> PAGE NOT FOUND ON OBJECT LIST! <<<\n");
			}
		}
	}

	/* cross-verify page queue */
	if (pg->pqflags & PQ_FREE)
		pgl = &uvm.page_free[uvm_page_lookup_freelist(pg)];
	else if (pg->pqflags & PQ_INACTIVE)
		pgl = (pg->pqflags & PQ_SWAPBACKED) ? 
		    &uvm.page_inactive_swp : &uvm.page_inactive_obj;
	else if (pg->pqflags & PQ_ACTIVE)
		pgl = &uvm.page_active;
	else
		pgl = NULL;

	if (pgl) {
		(*pr)("  checking pageq list\n");
		for (lcv = pgl->tqh_first ; lcv ; lcv = lcv->pageq.tqe_next) {
			if (lcv == pg) break;
		}
		if (lcv)
			(*pr)("  page found on pageq list\n");
		else
			(*pr)("  >>> PAGE NOT FOUND ON PAGEQ LIST! <<<\n");
	}
}
#endif
