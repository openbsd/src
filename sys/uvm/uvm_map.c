/*	$OpenBSD: uvm_map.c,v 1.192 2015/07/17 21:56:14 kettenis Exp $	*/
/*	$NetBSD: uvm_map.c,v 1.86 2000/11/27 08:40:03 chs Exp $	*/

/*
 * Copyright (c) 2011 Ariane van der Steldt <ariane@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * 
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
 * 3. Neither the name of the University nor the names of its contributors
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

/* #define DEBUG */
/* #define VMMAP_DEBUG */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mman.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/pool.h>
#include <sys/sysctl.h>

#ifdef SYSVSHM
#include <sys/shm.h>
#endif

#include <uvm/uvm.h>

#ifdef DDB
#include <uvm/uvm_ddb.h>
#endif

#include <uvm/uvm_addr.h>


vsize_t			 uvmspace_dused(struct vm_map*, vaddr_t, vaddr_t);
int			 uvm_mapent_isjoinable(struct vm_map*,
			    struct vm_map_entry*, struct vm_map_entry*);
struct vm_map_entry	*uvm_mapent_merge(struct vm_map*, struct vm_map_entry*,
			    struct vm_map_entry*, struct uvm_map_deadq*);
struct vm_map_entry	*uvm_mapent_tryjoin(struct vm_map*,
			    struct vm_map_entry*, struct uvm_map_deadq*);
struct vm_map_entry	*uvm_map_mkentry(struct vm_map*, struct vm_map_entry*,
			    struct vm_map_entry*, vaddr_t, vsize_t, int,
			    struct uvm_map_deadq*, struct vm_map_entry*);
struct vm_map_entry	*uvm_mapent_alloc(struct vm_map*, int);
void			 uvm_mapent_free(struct vm_map_entry*);
void			 uvm_unmap_kill_entry(struct vm_map*,
			    struct vm_map_entry*);
void			 uvm_mapent_mkfree(struct vm_map*,
			    struct vm_map_entry*, struct vm_map_entry**,
			    struct uvm_map_deadq*, boolean_t);
void			 uvm_map_pageable_pgon(struct vm_map*,
			    struct vm_map_entry*, struct vm_map_entry*,
			    vaddr_t, vaddr_t);
int			 uvm_map_pageable_wire(struct vm_map*,
			    struct vm_map_entry*, struct vm_map_entry*,
			    vaddr_t, vaddr_t, int);
void			 uvm_map_setup_entries(struct vm_map*);
void			 uvm_map_setup_md(struct vm_map*);
void			 uvm_map_teardown(struct vm_map*);
void			 uvm_map_vmspace_update(struct vm_map*,
			    struct uvm_map_deadq*, int);
void			 uvm_map_kmem_grow(struct vm_map*,
			    struct uvm_map_deadq*, vsize_t, int);
void			 uvm_map_freelist_update_clear(struct vm_map*,
			    struct uvm_map_deadq*);
void			 uvm_map_freelist_update_refill(struct vm_map *, int);
void			 uvm_map_freelist_update(struct vm_map*,
			    struct uvm_map_deadq*, vaddr_t, vaddr_t,
			    vaddr_t, vaddr_t, int);
struct vm_map_entry	*uvm_map_fix_space(struct vm_map*, struct vm_map_entry*,
			    vaddr_t, vaddr_t, int);
int			 uvm_map_sel_limits(vaddr_t*, vaddr_t*, vsize_t, int,
			    struct vm_map_entry*, vaddr_t, vaddr_t, vaddr_t,
			    int);
int			 uvm_map_findspace(struct vm_map*,
			    struct vm_map_entry**, struct vm_map_entry**,
			    vaddr_t*, vsize_t, vaddr_t, vaddr_t, vm_prot_t,
			    vaddr_t);
vsize_t			 uvm_map_addr_augment_get(struct vm_map_entry*);
void			 uvm_map_addr_augment(struct vm_map_entry*);

/*
 * Tree management functions.
 */

static __inline void	 uvm_mapent_copy(struct vm_map_entry*,
			    struct vm_map_entry*);
static int		 uvm_mapentry_addrcmp(struct vm_map_entry*,
			    struct vm_map_entry*);
static int		 uvm_mapentry_freecmp(struct vm_map_entry*,
			    struct vm_map_entry*);
void			 uvm_mapent_free_insert(struct vm_map*,
			    struct uvm_addr_state*, struct vm_map_entry*);
void			 uvm_mapent_free_remove(struct vm_map*,
			    struct uvm_addr_state*, struct vm_map_entry*);
void			 uvm_mapent_addr_insert(struct vm_map*,
			    struct vm_map_entry*);
void			 uvm_mapent_addr_remove(struct vm_map*,
			    struct vm_map_entry*);
void			 uvm_map_splitentry(struct vm_map*,
			    struct vm_map_entry*, struct vm_map_entry*,
			    vaddr_t);
vsize_t			 uvm_map_boundary(struct vm_map*, vaddr_t, vaddr_t);
int			 uvm_mapent_bias(struct vm_map*, struct vm_map_entry*);

/*
 * uvm_vmspace_fork helper functions.
 */
struct vm_map_entry	*uvm_mapent_clone(struct vm_map*, vaddr_t, vsize_t,
			    vsize_t, struct vm_map_entry*,
			    struct uvm_map_deadq*, int, int);
struct vm_map_entry	*uvm_mapent_forkshared(struct vmspace*, struct vm_map*,
			    struct vm_map*, struct vm_map_entry*,
			    struct uvm_map_deadq*);
struct vm_map_entry	*uvm_mapent_forkcopy(struct vmspace*, struct vm_map*,
			    struct vm_map*, struct vm_map_entry*,
			    struct uvm_map_deadq*);
struct vm_map_entry	*uvm_mapent_forkzero(struct vmspace*, struct vm_map*,
			    struct vm_map*, struct vm_map_entry*,
			    struct uvm_map_deadq*);

/*
 * Tree validation.
 */
#ifdef VMMAP_DEBUG
void			 uvm_tree_assert(struct vm_map*, int, char*,
			    char*, int);
#define UVM_ASSERT(map, cond, file, line)				\
	uvm_tree_assert((map), (cond), #cond, (file), (line))
void			 uvm_tree_sanity(struct vm_map*, char*, int);
void			 uvm_tree_size_chk(struct vm_map*, char*, int);
void			 vmspace_validate(struct vm_map*);
#else
#define uvm_tree_sanity(_map, _file, _line)		do {} while (0)
#define uvm_tree_size_chk(_map, _file, _line)		do {} while (0)
#define vmspace_validate(_map)				do {} while (0)
#endif

/*
 * All architectures will have pmap_prefer.
 */
#ifndef PMAP_PREFER
#define PMAP_PREFER_ALIGN()	(vaddr_t)PAGE_SIZE
#define PMAP_PREFER_OFFSET(off)	0
#define PMAP_PREFER(addr, off)	(addr)
#endif


/*
 * The kernel map will initially be VM_MAP_KSIZE_INIT bytes.
 * Every time that gets cramped, we grow by at least VM_MAP_KSIZE_DELTA bytes.
 *
 * We attempt to grow by UVM_MAP_KSIZE_ALLOCMUL times the allocation size
 * each time.
 */
#define VM_MAP_KSIZE_INIT	(512 * (vaddr_t)PAGE_SIZE)
#define VM_MAP_KSIZE_DELTA	(256 * (vaddr_t)PAGE_SIZE)
#define VM_MAP_KSIZE_ALLOCMUL	4
/*
 * When selecting a random free-space block, look at most FSPACE_DELTA blocks
 * ahead.
 */
#define FSPACE_DELTA		8
/*
 * Put allocations adjecent to previous allocations when the free-space tree
 * is larger than FSPACE_COMPACT entries.
 *
 * Alignment and PMAP_PREFER may still cause the entry to not be fully
 * adjecent. Note that this strategy reduces memory fragmentation (by leaving
 * a large space before or after the allocation).
 */
#define FSPACE_COMPACT		128
/*
 * Make the address selection skip at most this many bytes from the start of
 * the free space in which the allocation takes place.
 *
 * The main idea behind a randomized address space is that an attacker cannot
 * know where to target his attack. Therefore, the location of objects must be
 * as random as possible. However, the goal is not to create the most sparse
 * map that is possible.
 * FSPACE_MAXOFF pushes the considered range in bytes down to less insane
 * sizes, thereby reducing the sparseness. The biggest randomization comes
 * from fragmentation, i.e. FSPACE_COMPACT.
 */
#define FSPACE_MAXOFF		((vaddr_t)32 * 1024 * 1024)
/*
 * Allow for small gaps in the overflow areas.
 * Gap size is in bytes and does not have to be a multiple of page-size.
 */
#define FSPACE_BIASGAP		((vaddr_t)32 * 1024)

/* auto-allocate address lower bound */
#define VMMAP_MIN_ADDR		PAGE_SIZE


#ifdef DEADBEEF0
#define UVMMAP_DEADBEEF		((void*)DEADBEEF0)
#else
#define UVMMAP_DEADBEEF		((void*)0xdeadd0d0)
#endif

#ifdef DEBUG
int uvm_map_printlocks = 0;

#define LPRINTF(_args)							\
	do {								\
		if (uvm_map_printlocks)					\
			printf _args;					\
	} while (0)
#else
#define LPRINTF(_args)	do {} while (0)
#endif

static struct timeval uvm_kmapent_last_warn_time;
static struct timeval uvm_kmapent_warn_rate = { 10, 0 };

const char vmmapbsy[] = "vmmapbsy";

/*
 * pool for vmspace structures.
 */
struct pool uvm_vmspace_pool;

/*
 * pool for dynamically-allocated map entries.
 */
struct pool uvm_map_entry_pool;
struct pool uvm_map_entry_kmem_pool;

/*
 * This global represents the end of the kernel virtual address
 * space. If we want to exceed this, we must grow the kernel
 * virtual address space dynamically.
 *
 * Note, this variable is locked by kernel_map's lock.
 */
vaddr_t uvm_maxkaddr;

/*
 * Locking predicate.
 */
#define UVM_MAP_REQ_WRITE(_map)						\
	do {								\
		if (((_map)->flags & VM_MAP_INTRSAFE) == 0)		\
			rw_assert_wrlock(&(_map)->lock);		\
	} while (0)

/*
 * Tree describing entries by address.
 *
 * Addresses are unique.
 * Entries with start == end may only exist if they are the first entry
 * (sorted by address) within a free-memory tree.
 */

static __inline int
uvm_mapentry_addrcmp(struct vm_map_entry *e1, struct vm_map_entry *e2)
{
	return e1->start < e2->start ? -1 : e1->start > e2->start;
}

/*
 * Tree describing free memory.
 *
 * Free memory is indexed (so we can use array semantics in O(log N).
 * Free memory is ordered by size (so we can reduce fragmentation).
 *
 * The address range in the tree can be limited, having part of the
 * free memory not in the free-memory tree. Only free memory in the
 * tree will be considered during 'any address' allocations.
 */

static __inline int
uvm_mapentry_freecmp(struct vm_map_entry *e1, struct vm_map_entry *e2)
{
	int cmp = e1->fspace < e2->fspace ? -1 : e1->fspace > e2->fspace;
	return cmp ? cmp : uvm_mapentry_addrcmp(e1, e2);
}

/*
 * Copy mapentry.
 */
static __inline void
uvm_mapent_copy(struct vm_map_entry *src, struct vm_map_entry *dst)
{
	caddr_t csrc, cdst;
	size_t sz;

	csrc = (caddr_t)src;
	cdst = (caddr_t)dst;
	csrc += offsetof(struct vm_map_entry, uvm_map_entry_start_copy);
	cdst += offsetof(struct vm_map_entry, uvm_map_entry_start_copy);

	sz = offsetof(struct vm_map_entry, uvm_map_entry_stop_copy) -
	    offsetof(struct vm_map_entry, uvm_map_entry_start_copy);
	memcpy(cdst, csrc, sz);
}

/*
 * Handle free-list insertion.
 */
void
uvm_mapent_free_insert(struct vm_map *map, struct uvm_addr_state *uaddr,
    struct vm_map_entry *entry)
{
	const struct uvm_addr_functions *fun;
#ifdef VMMAP_DEBUG
	vaddr_t min, max, bound;
#endif

#ifdef VMMAP_DEBUG
	/*
	 * Boundary check.
	 * Boundaries are folded if they go on the same free list.
	 */
	min = VMMAP_FREE_START(entry);
	max = VMMAP_FREE_END(entry);

	while (min < max) {
		bound = uvm_map_boundary(map, min, max);
		KASSERT(uvm_map_uaddr(map, min) == uaddr);
		min = bound;
	}
#endif
	KDASSERT((entry->fspace & (vaddr_t)PAGE_MASK) == 0);
	KASSERT((entry->etype & UVM_ET_FREEMAPPED) == 0);

	UVM_MAP_REQ_WRITE(map);

	/* Actual insert: forward to uaddr pointer. */
	if (uaddr != NULL) {
		fun = uaddr->uaddr_functions;
		KDASSERT(fun != NULL);
		if (fun->uaddr_free_insert != NULL)
			(*fun->uaddr_free_insert)(map, uaddr, entry);
		entry->etype |= UVM_ET_FREEMAPPED;
	}

	/* Update fspace augmentation. */
	uvm_map_addr_augment(entry);
}

/*
 * Handle free-list removal.
 */
void
uvm_mapent_free_remove(struct vm_map *map, struct uvm_addr_state *uaddr,
    struct vm_map_entry *entry)
{
	const struct uvm_addr_functions *fun;

	KASSERT((entry->etype & UVM_ET_FREEMAPPED) != 0 || uaddr == NULL);
	KASSERT(uvm_map_uaddr_e(map, entry) == uaddr);
	UVM_MAP_REQ_WRITE(map);

	if (uaddr != NULL) {
		fun = uaddr->uaddr_functions;
		if (fun->uaddr_free_remove != NULL)
			(*fun->uaddr_free_remove)(map, uaddr, entry);
		entry->etype &= ~UVM_ET_FREEMAPPED;
	}
}

/*
 * Handle address tree insertion.
 */
void
uvm_mapent_addr_insert(struct vm_map *map, struct vm_map_entry *entry)
{
	struct vm_map_entry *res;

	if (RB_LEFT(entry, daddrs.addr_entry) != UVMMAP_DEADBEEF ||
	    RB_RIGHT(entry, daddrs.addr_entry) != UVMMAP_DEADBEEF ||
	    RB_PARENT(entry, daddrs.addr_entry) != UVMMAP_DEADBEEF)
		panic("uvm_mapent_addr_insert: entry still in addr list");
	KDASSERT(entry->start <= entry->end);
	KDASSERT((entry->start & (vaddr_t)PAGE_MASK) == 0 &&
	    (entry->end & (vaddr_t)PAGE_MASK) == 0);

	UVM_MAP_REQ_WRITE(map);
	res = RB_INSERT(uvm_map_addr, &map->addr, entry);
	if (res != NULL) {
		panic("uvm_mapent_addr_insert: map %p entry %p "
		    "(0x%lx-0x%lx G=0x%lx F=0x%lx) insert collision "
		    "with entry %p (0x%lx-0x%lx G=0x%lx F=0x%lx)",
		    map, entry,
		    entry->start, entry->end, entry->guard, entry->fspace,
		    res, res->start, res->end, res->guard, res->fspace);
	}
}

/*
 * Handle address tree removal.
 */
void
uvm_mapent_addr_remove(struct vm_map *map, struct vm_map_entry *entry)
{
	struct vm_map_entry *res;

	UVM_MAP_REQ_WRITE(map);
	res = RB_REMOVE(uvm_map_addr, &map->addr, entry);
	if (res != entry)
		panic("uvm_mapent_addr_remove");
	RB_LEFT(entry, daddrs.addr_entry) = RB_RIGHT(entry, daddrs.addr_entry) =
	    RB_PARENT(entry, daddrs.addr_entry) = UVMMAP_DEADBEEF;
}

/*
 * uvm_map_reference: add reference to a map
 *
 * XXX check map reference counter lock
 */
#define uvm_map_reference(_map)						\
	do {								\
		map->ref_count++;					\
	} while (0)

/*
 * Calculate the dused delta.
 */
vsize_t
uvmspace_dused(struct vm_map *map, vaddr_t min, vaddr_t max)
{
	struct vmspace *vm;
	vsize_t sz;
	vaddr_t lmax;
	vaddr_t stack_begin, stack_end; /* Position of stack. */

	KASSERT(map->flags & VM_MAP_ISVMSPACE);
	vm = (struct vmspace *)map;
	stack_begin = MIN((vaddr_t)vm->vm_maxsaddr, (vaddr_t)vm->vm_minsaddr);
	stack_end = MAX((vaddr_t)vm->vm_maxsaddr, (vaddr_t)vm->vm_minsaddr);

	sz = 0;
	while (min != max) {
		lmax = max;
		if (min < stack_begin && lmax > stack_begin)
			lmax = stack_begin;
		else if (min < stack_end && lmax > stack_end)
			lmax = stack_end;

		if (min >= stack_begin && min < stack_end) {
			/* nothing */
		} else
			sz += lmax - min;
		min = lmax;
	}

	return sz >> PAGE_SHIFT;
}

/*
 * Find the entry describing the given address.
 */
struct vm_map_entry*
uvm_map_entrybyaddr(struct uvm_map_addr *atree, vaddr_t addr)
{
	struct vm_map_entry *iter;

	iter = RB_ROOT(atree);
	while (iter != NULL) {
		if (iter->start > addr)
			iter = RB_LEFT(iter, daddrs.addr_entry);
		else if (VMMAP_FREE_END(iter) <= addr)
			iter = RB_RIGHT(iter, daddrs.addr_entry);
		else
			return iter;
	}
	return NULL;
}

/*
 * DEAD_ENTRY_PUSH(struct vm_map_deadq *deadq, struct vm_map_entry *entry)
 *
 * Push dead entries into a linked list.
 * Since the linked list abuses the address tree for storage, the entry
 * may not be linked in a map.
 *
 * *head must be initialized to NULL before the first call to this macro.
 * uvm_unmap_detach(*head, 0) will remove dead entries.
 */
static __inline void
dead_entry_push(struct uvm_map_deadq *deadq, struct vm_map_entry *entry)
{
	TAILQ_INSERT_TAIL(deadq, entry, dfree.deadq);
}
#define DEAD_ENTRY_PUSH(_headptr, _entry)				\
	dead_entry_push((_headptr), (_entry))

/*
 * Helper function for uvm_map_findspace_tree.
 *
 * Given allocation constraints and pmap constraints, finds the
 * lowest and highest address in a range that can be used for the
 * allocation.
 *
 * pmap_align and pmap_off are ignored on non-PMAP_PREFER archs.
 *
 *
 * Big chunk of math with a seasoning of dragons.
 */
int
uvm_map_sel_limits(vaddr_t *min, vaddr_t *max, vsize_t sz, int guardpg,
    struct vm_map_entry *sel, vaddr_t align,
    vaddr_t pmap_align, vaddr_t pmap_off, int bias)
{
	vaddr_t sel_min, sel_max;
#ifdef PMAP_PREFER
	vaddr_t pmap_min, pmap_max;
#endif /* PMAP_PREFER */
#ifdef DIAGNOSTIC
	int bad;
#endif /* DIAGNOSTIC */

	sel_min = VMMAP_FREE_START(sel);
	sel_max = VMMAP_FREE_END(sel) - sz - (guardpg ? PAGE_SIZE : 0);

#ifdef PMAP_PREFER

	/*
	 * There are two special cases, in which we can satisfy the align
	 * requirement and the pmap_prefer requirement.
	 * - when pmap_off == 0, we always select the largest of the two
	 * - when pmap_off % align == 0 and pmap_align > align, we simply
	 *   satisfy the pmap_align requirement and automatically
	 *   satisfy the align requirement.
	 */
	if (align > PAGE_SIZE &&
	    !(pmap_align > align && (pmap_off & (align - 1)) == 0)) {
		/*
		 * Simple case: only use align.
		 */
		sel_min = roundup(sel_min, align);
		sel_max &= ~(align - 1);

		if (sel_min > sel_max)
			return ENOMEM;

		/* Correct for bias. */
		if (sel_max - sel_min > FSPACE_BIASGAP) {
			if (bias > 0) {
				sel_min = sel_max - FSPACE_BIASGAP;
				sel_min = roundup(sel_min, align);
			} else if (bias < 0) {
				sel_max = sel_min + FSPACE_BIASGAP;
				sel_max &= ~(align - 1);
			}
		}
	} else if (pmap_align != 0) {
		/*
		 * Special case: satisfy both pmap_prefer and
		 * align argument.
		 */
		pmap_max = sel_max & ~(pmap_align - 1);
		pmap_min = sel_min;
		if (pmap_max < sel_min)
			return ENOMEM;

		/* Adjust pmap_min for BIASGAP for top-addr bias. */
		if (bias > 0 && pmap_max - pmap_min > FSPACE_BIASGAP)
			pmap_min = pmap_max - FSPACE_BIASGAP;
		/* Align pmap_min. */
		pmap_min &= ~(pmap_align - 1);
		if (pmap_min < sel_min)
			pmap_min += pmap_align;
		if (pmap_min > pmap_max)
			return ENOMEM;

		/* Adjust pmap_max for BIASGAP for bottom-addr bias. */
		if (bias < 0 && pmap_max - pmap_min > FSPACE_BIASGAP) {
			pmap_max = (pmap_min + FSPACE_BIASGAP) &
			    ~(pmap_align - 1);
		}
		if (pmap_min > pmap_max)
			return ENOMEM;

		/* Apply pmap prefer offset. */
		pmap_max |= pmap_off;
		if (pmap_max > sel_max)
			pmap_max -= pmap_align;
		pmap_min |= pmap_off;
		if (pmap_min < sel_min)
			pmap_min += pmap_align;

		/*
		 * Fixup: it's possible that pmap_min and pmap_max
		 * cross eachother. In this case, try to find one
		 * address that is allowed.
		 * (This usually happens in biased case.)
		 */
		if (pmap_min > pmap_max) {
			if (pmap_min < sel_max)
				pmap_max = pmap_min;
			else if (pmap_max > sel_min)
				pmap_min = pmap_max;
			else
				return ENOMEM;
		}

		/* Internal validation. */
		KDASSERT(pmap_min <= pmap_max);

		sel_min = pmap_min;
		sel_max = pmap_max;
	} else if (bias > 0 && sel_max - sel_min > FSPACE_BIASGAP)
		sel_min = sel_max - FSPACE_BIASGAP;
	else if (bias < 0 && sel_max - sel_min > FSPACE_BIASGAP)
		sel_max = sel_min + FSPACE_BIASGAP;

#else

	if (align > PAGE_SIZE) {
		sel_min = roundup(sel_min, align);
		sel_max &= ~(align - 1);
		if (sel_min > sel_max)
			return ENOMEM;

		if (bias != 0 && sel_max - sel_min > FSPACE_BIASGAP) {
			if (bias > 0) {
				sel_min = roundup(sel_max - FSPACE_BIASGAP,
				    align);
			} else {
				sel_max = (sel_min + FSPACE_BIASGAP) &
				    ~(align - 1);
			}
		}
	} else if (bias > 0 && sel_max - sel_min > FSPACE_BIASGAP)
		sel_min = sel_max - FSPACE_BIASGAP;
	else if (bias < 0 && sel_max - sel_min > FSPACE_BIASGAP)
		sel_max = sel_min + FSPACE_BIASGAP;

#endif

	if (sel_min > sel_max)
		return ENOMEM;

#ifdef DIAGNOSTIC
	bad = 0;
	/* Lower boundary check. */
	if (sel_min < VMMAP_FREE_START(sel)) {
		printf("sel_min: 0x%lx, but should be at least 0x%lx\n",
		    sel_min, VMMAP_FREE_START(sel));
		bad++;
	}
	/* Upper boundary check. */
	if (sel_max > VMMAP_FREE_END(sel) - sz - (guardpg ? PAGE_SIZE : 0)) {
		printf("sel_max: 0x%lx, but should be at most 0x%lx\n",
		    sel_max,
		    VMMAP_FREE_END(sel) - sz - (guardpg ? PAGE_SIZE : 0));
		bad++;
	}
	/* Lower boundary alignment. */
	if (align != 0 && (sel_min & (align - 1)) != 0) {
		printf("sel_min: 0x%lx, not aligned to 0x%lx\n",
		    sel_min, align);
		bad++;
	}
	/* Upper boundary alignment. */
	if (align != 0 && (sel_max & (align - 1)) != 0) {
		printf("sel_max: 0x%lx, not aligned to 0x%lx\n",
		    sel_max, align);
		bad++;
	}
	/* Lower boundary PMAP_PREFER check. */
	if (pmap_align != 0 && align == 0 &&
	    (sel_min & (pmap_align - 1)) != pmap_off) {
		printf("sel_min: 0x%lx, aligned to 0x%lx, expected 0x%lx\n",
		    sel_min, sel_min & (pmap_align - 1), pmap_off);
		bad++;
	}
	/* Upper boundary PMAP_PREFER check. */
	if (pmap_align != 0 && align == 0 &&
	    (sel_max & (pmap_align - 1)) != pmap_off) {
		printf("sel_max: 0x%lx, aligned to 0x%lx, expected 0x%lx\n",
		    sel_max, sel_max & (pmap_align - 1), pmap_off);
		bad++;
	}

	if (bad) {
		panic("uvm_map_sel_limits(sz = %lu, guardpg = %c, "
		    "align = 0x%lx, pmap_align = 0x%lx, pmap_off = 0x%lx, "
		    "bias = %d, "
		    "FREE_START(sel) = 0x%lx, FREE_END(sel) = 0x%lx)",
		    sz, (guardpg ? 'T' : 'F'), align, pmap_align, pmap_off,
		    bias, VMMAP_FREE_START(sel), VMMAP_FREE_END(sel));
	}
#endif /* DIAGNOSTIC */

	*min = sel_min;
	*max = sel_max;
	return 0;
}

/*
 * Test if memory starting at addr with sz bytes is free.
 *
 * Fills in *start_ptr and *end_ptr to be the first and last entry describing
 * the space.
 * If called with prefilled *start_ptr and *end_ptr, they are to be correct.
 */
int
uvm_map_isavail(struct vm_map *map, struct uvm_addr_state *uaddr,
    struct vm_map_entry **start_ptr, struct vm_map_entry **end_ptr,
    vaddr_t addr, vsize_t sz)
{
	struct uvm_addr_state *free;
	struct uvm_map_addr *atree;
	struct vm_map_entry *i, *i_end;

	/*
	 * Kernel memory above uvm_maxkaddr is considered unavailable.
	 */
	if ((map->flags & VM_MAP_ISVMSPACE) == 0) {
		if (addr + sz > uvm_maxkaddr)
			return 0;
	}

	atree = &map->addr;

	/*
	 * Fill in first, last, so they point at the entries containing the
	 * first and last address of the range.
	 * Note that if they are not NULL, we don't perform the lookup.
	 */
	KDASSERT(atree != NULL && start_ptr != NULL && end_ptr != NULL);
	if (*start_ptr == NULL) {
		*start_ptr = uvm_map_entrybyaddr(atree, addr);
		if (*start_ptr == NULL)
			return 0;
	} else
		KASSERT(*start_ptr == uvm_map_entrybyaddr(atree, addr));
	if (*end_ptr == NULL) {
		if (VMMAP_FREE_END(*start_ptr) >= addr + sz)
			*end_ptr = *start_ptr;
		else {
			*end_ptr = uvm_map_entrybyaddr(atree, addr + sz - 1);
			if (*end_ptr == NULL)
				return 0;
		}
	} else
		KASSERT(*end_ptr == uvm_map_entrybyaddr(atree, addr + sz - 1));

	/* Validation. */
	KDASSERT(*start_ptr != NULL && *end_ptr != NULL);
	KDASSERT((*start_ptr)->start <= addr &&
	    VMMAP_FREE_END(*start_ptr) > addr &&
	    (*end_ptr)->start < addr + sz &&
	    VMMAP_FREE_END(*end_ptr) >= addr + sz);

	/*
	 * Check the none of the entries intersects with <addr, addr+sz>.
	 * Also, if the entry belong to uaddr_exe or uaddr_brk_stack, it is
	 * considered unavailable unless called by those allocators.
	 */
	i = *start_ptr;
	i_end = RB_NEXT(uvm_map_addr, atree, *end_ptr);
	for (; i != i_end;
	    i = RB_NEXT(uvm_map_addr, atree, i)) {
		if (i->start != i->end && i->end > addr)
			return 0;

		/*
		 * uaddr_exe and uaddr_brk_stack may only be used
		 * by these allocators and the NULL uaddr (i.e. no
		 * uaddr).
		 * Reject if this requirement is not met.
		 */
		if (uaddr != NULL) {
			free = uvm_map_uaddr_e(map, i);

			if (uaddr != free && free != NULL &&
			    (free == map->uaddr_exe ||
			     free == map->uaddr_brk_stack))
				return 0;
		}
	}

	return -1;
}

/*
 * Invoke each address selector until an address is found.
 * Will not invoke uaddr_exe.
 */
int
uvm_map_findspace(struct vm_map *map, struct vm_map_entry**first,
    struct vm_map_entry**last, vaddr_t *addr, vsize_t sz,
    vaddr_t pmap_align, vaddr_t pmap_offset, vm_prot_t prot, vaddr_t hint)
{
	struct uvm_addr_state *uaddr;
	int i;

	/*
	 * Allocation for sz bytes at any address,
	 * using the addr selectors in order.
	 */
	for (i = 0; i < nitems(map->uaddr_any); i++) {
		uaddr = map->uaddr_any[i];

		if (uvm_addr_invoke(map, uaddr, first, last,
		    addr, sz, pmap_align, pmap_offset, prot, hint) == 0)
			return 0;
	}

	/* Fall back to brk() and stack() address selectors. */
	uaddr = map->uaddr_brk_stack;
	if (uvm_addr_invoke(map, uaddr, first, last,
	    addr, sz, pmap_align, pmap_offset, prot, hint) == 0)
		return 0;

	return ENOMEM;
}

/* Calculate entry augmentation value. */
vsize_t
uvm_map_addr_augment_get(struct vm_map_entry *entry)
{
	vsize_t			 augment;
	struct vm_map_entry	*left, *right;

	augment = entry->fspace;
	if ((left = RB_LEFT(entry, daddrs.addr_entry)) != NULL)
		augment = MAX(augment, left->fspace_augment);
	if ((right = RB_RIGHT(entry, daddrs.addr_entry)) != NULL)
		augment = MAX(augment, right->fspace_augment);
	return augment;
}

/*
 * Update augmentation data in entry.
 */
void
uvm_map_addr_augment(struct vm_map_entry *entry)
{
	vsize_t			 augment;

	while (entry != NULL) {
		/* Calculate value for augmentation. */
		augment = uvm_map_addr_augment_get(entry);

		/*
		 * Descend update.
		 * Once we find an entry that already has the correct value,
		 * stop, since it means all its parents will use the correct
		 * value too.
		 */
		if (entry->fspace_augment == augment)
			return;
		entry->fspace_augment = augment;
		entry = RB_PARENT(entry, daddrs.addr_entry);
	}
}

/*
 * uvm_mapanon: establish a valid mapping in map for an anon
 *
 * => *addr and sz must be a multiple of PAGE_SIZE.
 * => *addr is ignored, except if flags contains UVM_FLAG_FIXED.
 * => map must be unlocked.
 *
 * => align: align vaddr, must be a power-of-2.
 *    Align is only a hint and will be ignored if the alignment fails.
 */
int
uvm_mapanon(struct vm_map *map, vaddr_t *addr, vsize_t sz,
    vsize_t align, uvm_flag_t flags)
{
	struct vm_map_entry	*first, *last, *entry, *new;
	struct uvm_map_deadq	 dead;
	vm_prot_t		 prot;
	vm_prot_t		 maxprot;
	vm_inherit_t		 inherit;
	int			 advice;
	int			 error;
	vaddr_t			 pmap_align, pmap_offset;
	vaddr_t			 hint;

	KASSERT((map->flags & VM_MAP_ISVMSPACE) == VM_MAP_ISVMSPACE);
	KASSERT(map != kernel_map);
	KASSERT((map->flags & UVM_FLAG_HOLE) == 0);

	KASSERT((map->flags & VM_MAP_INTRSAFE) == 0);
	splassert(IPL_NONE);

	/*
	 * We use pmap_align and pmap_offset as alignment and offset variables.
	 *
	 * Because the align parameter takes precedence over pmap prefer,
	 * the pmap_align will need to be set to align, with pmap_offset = 0,
	 * if pmap_prefer will not align.
	 */
	pmap_align = MAX(align, PAGE_SIZE);
	pmap_offset = 0;

	/* Decode parameters. */
	prot = UVM_PROTECTION(flags);
	maxprot = UVM_MAXPROTECTION(flags);
	advice = UVM_ADVICE(flags);
	inherit = UVM_INHERIT(flags);
	error = 0;
	hint = trunc_page(*addr);
	TAILQ_INIT(&dead);
	KASSERT((sz & (vaddr_t)PAGE_MASK) == 0);
	KASSERT((align & (align - 1)) == 0);

	/* Check protection. */
	if ((prot & maxprot) != prot)
		return EACCES;

	/*
	 * Before grabbing the lock, allocate a map entry for later
	 * use to ensure we don't wait for memory while holding the
	 * vm_map_lock.
	 */
	new = uvm_mapent_alloc(map, flags);
	if (new == NULL)
		return(ENOMEM);

	if (flags & UVM_FLAG_TRYLOCK) {
		if (vm_map_lock_try(map) == FALSE) {
			error = EFAULT;
			goto out;
		}
	} else
		vm_map_lock(map);

	first = last = NULL;
	if (flags & UVM_FLAG_FIXED) {
		/*
		 * Fixed location.
		 *
		 * Note: we ignore align, pmap_prefer.
		 * Fill in first, last and *addr.
		 */
		KASSERT((*addr & PAGE_MASK) == 0);

		/* Check that the space is available. */
		if (!uvm_map_isavail(map, NULL, &first, &last, *addr, sz)) {
			error = ENOMEM;
			goto unlock;
		}
	} else if (*addr != 0 && (*addr & PAGE_MASK) == 0 &&
	    (align == 0 || (*addr & (align - 1)) == 0) &&
	    uvm_map_isavail(map, NULL, &first, &last, *addr, sz)) {
		/*
		 * Address used as hint.
		 *
		 * Note: we enforce the alignment restriction,
		 * but ignore pmap_prefer.
		 */
	} else if ((maxprot & PROT_EXEC) != 0 &&
	    map->uaddr_exe != NULL) {
		/* Run selection algorithm for executables. */
		error = uvm_addr_invoke(map, map->uaddr_exe, &first, &last,
		    addr, sz, pmap_align, pmap_offset, prot, hint);

		if (error != 0)
			goto unlock;
	} else {
		/* Update freelists from vmspace. */
		uvm_map_vmspace_update(map, &dead, flags);

		error = uvm_map_findspace(map, &first, &last, addr, sz,
		    pmap_align, pmap_offset, prot, hint);

		if (error != 0)
			goto unlock;
	}

	/* If we only want a query, return now. */
	if (flags & UVM_FLAG_QUERY) {
		error = 0;
		goto unlock;
	}

	/*
	 * Create new entry.
	 * first and last may be invalidated after this call.
	 */
	entry = uvm_map_mkentry(map, first, last, *addr, sz, flags, &dead,
	    new);
	if (entry == NULL) {
		error = ENOMEM;
		goto unlock;
	}
	new = NULL;
	KDASSERT(entry->start == *addr && entry->end == *addr + sz);
	entry->object.uvm_obj = NULL;
	entry->offset = 0;
	entry->protection = prot;
	entry->max_protection = maxprot;
	entry->inheritance = inherit;
	entry->wired_count = 0;
	entry->advice = advice;
	if (flags & UVM_FLAG_NOFAULT)
		entry->etype |= UVM_ET_NOFAULT;
	if (flags & UVM_FLAG_COPYONW) {
		entry->etype |= UVM_ET_COPYONWRITE;
		if ((flags & UVM_FLAG_OVERLAY) == 0)
			entry->etype |= UVM_ET_NEEDSCOPY;
	}
	if (flags & UVM_FLAG_OVERLAY) {
		KERNEL_LOCK();
		entry->aref.ar_pageoff = 0;
		entry->aref.ar_amap = amap_alloc(sz,
		    ptoa(flags & UVM_FLAG_AMAPPAD ? UVM_AMAP_CHUNK : 0),
		    M_WAITOK);
		KERNEL_UNLOCK();
	}

	/* Update map and process statistics. */
	map->size += sz;
	((struct vmspace *)map)->vm_dused += uvmspace_dused(map, *addr, *addr + sz);

unlock:
	vm_map_unlock(map);

	/*
	 * Remove dead entries.
	 *
	 * Dead entries may be the result of merging.
	 * uvm_map_mkentry may also create dead entries, when it attempts to
	 * destroy free-space entries.
	 */
	uvm_unmap_detach(&dead, 0);
out:
	if (new)
		uvm_mapent_free(new);
	return error;
}

/*
 * uvm_map: establish a valid mapping in map
 *
 * => *addr and sz must be a multiple of PAGE_SIZE.
 * => map must be unlocked.
 * => <uobj,uoffset> value meanings (4 cases):
 *	[1] <NULL,uoffset>		== uoffset is a hint for PMAP_PREFER
 *	[2] <NULL,UVM_UNKNOWN_OFFSET>	== don't PMAP_PREFER
 *	[3] <uobj,uoffset>		== normal mapping
 *	[4] <uobj,UVM_UNKNOWN_OFFSET>	== uvm_map finds offset based on VA
 *
 *   case [4] is for kernel mappings where we don't know the offset until
 *   we've found a virtual address.   note that kernel object offsets are
 *   always relative to vm_map_min(kernel_map).
 *
 * => align: align vaddr, must be a power-of-2.
 *    Align is only a hint and will be ignored if the alignment fails.
 */
int
uvm_map(struct vm_map *map, vaddr_t *addr, vsize_t sz,
    struct uvm_object *uobj, voff_t uoffset, vsize_t align, uvm_flag_t flags)
{
	struct vm_map_entry	*first, *last, *entry, *new;
	struct uvm_map_deadq	 dead;
	vm_prot_t		 prot;
	vm_prot_t		 maxprot;
	vm_inherit_t		 inherit;
	int			 advice;
	int			 error;
	vaddr_t			 pmap_align, pmap_offset;
	vaddr_t			 hint;

	if ((map->flags & VM_MAP_INTRSAFE) == 0)
		splassert(IPL_NONE);
	else
		splassert(IPL_VM);

	/*
	 * We use pmap_align and pmap_offset as alignment and offset variables.
	 *
	 * Because the align parameter takes precedence over pmap prefer,
	 * the pmap_align will need to be set to align, with pmap_offset = 0,
	 * if pmap_prefer will not align.
	 */
	if (uoffset == UVM_UNKNOWN_OFFSET) {
		pmap_align = MAX(align, PAGE_SIZE);
		pmap_offset = 0;
	} else {
		pmap_align = MAX(PMAP_PREFER_ALIGN(), PAGE_SIZE);
		pmap_offset = PMAP_PREFER_OFFSET(uoffset);

		if (align == 0 ||
		    (align <= pmap_align && (pmap_offset & (align - 1)) == 0)) {
			/* pmap_offset satisfies align, no change. */
		} else {
			/* Align takes precedence over pmap prefer. */
			pmap_align = align;
			pmap_offset = 0;
		}
	}

	/* Decode parameters. */
	prot = UVM_PROTECTION(flags);
	maxprot = UVM_MAXPROTECTION(flags);
	advice = UVM_ADVICE(flags);
	inherit = UVM_INHERIT(flags);
	error = 0;
	hint = trunc_page(*addr);
	TAILQ_INIT(&dead);
	KASSERT((sz & (vaddr_t)PAGE_MASK) == 0);
	KASSERT((align & (align - 1)) == 0);

	/* Holes are incompatible with other types of mappings. */
	if (flags & UVM_FLAG_HOLE) {
		KASSERT(uobj == NULL && (flags & UVM_FLAG_FIXED) &&
		    (flags & (UVM_FLAG_OVERLAY | UVM_FLAG_COPYONW)) == 0);
	}

	/* Unset hint for kernel_map non-fixed allocations. */
	if (!(map->flags & VM_MAP_ISVMSPACE) && !(flags & UVM_FLAG_FIXED))
		hint = 0;

	/* Check protection. */
	if ((prot & maxprot) != prot)
		return EACCES;

	if (map == kernel_map &&
	    (prot & (PROT_WRITE | PROT_EXEC)) == (PROT_WRITE | PROT_EXEC))
		panic("uvm_map: kernel map W^X violation requested");

	/*
	 * Before grabbing the lock, allocate a map entry for later
	 * use to ensure we don't wait for memory while holding the
	 * vm_map_lock.
	 */
	new = uvm_mapent_alloc(map, flags);
	if (new == NULL)
		return(ENOMEM);

	if (flags & UVM_FLAG_TRYLOCK) {
		if (vm_map_lock_try(map) == FALSE) {
			error = EFAULT;
			goto out;
		}
	} else
		vm_map_lock(map);

	first = last = NULL;
	if (flags & UVM_FLAG_FIXED) {
		/*
		 * Fixed location.
		 *
		 * Note: we ignore align, pmap_prefer.
		 * Fill in first, last and *addr.
		 */
		KASSERT((*addr & PAGE_MASK) == 0);

		/*
		 * Grow pmap to include allocated address.
		 * If the growth fails, the allocation will fail too.
		 */
		if ((map->flags & VM_MAP_ISVMSPACE) == 0 &&
		    uvm_maxkaddr < (*addr + sz)) {
			uvm_map_kmem_grow(map, &dead,
			    *addr + sz - uvm_maxkaddr, flags);
		}

		/* Check that the space is available. */
		if (!uvm_map_isavail(map, NULL, &first, &last, *addr, sz)) {
			error = ENOMEM;
			goto unlock;
		}
	} else if (*addr != 0 && (*addr & PAGE_MASK) == 0 &&
	    (map->flags & VM_MAP_ISVMSPACE) == VM_MAP_ISVMSPACE &&
	    (align == 0 || (*addr & (align - 1)) == 0) &&
	    uvm_map_isavail(map, NULL, &first, &last, *addr, sz)) {
		/*
		 * Address used as hint.
		 *
		 * Note: we enforce the alignment restriction,
		 * but ignore pmap_prefer.
		 */
	} else if ((maxprot & PROT_EXEC) != 0 &&
	    map->uaddr_exe != NULL) {
		/* Run selection algorithm for executables. */
		error = uvm_addr_invoke(map, map->uaddr_exe, &first, &last,
		    addr, sz, pmap_align, pmap_offset, prot, hint);

		/* Grow kernel memory and try again. */
		if (error != 0 && (map->flags & VM_MAP_ISVMSPACE) == 0) {
			uvm_map_kmem_grow(map, &dead, sz, flags);

			error = uvm_addr_invoke(map, map->uaddr_exe,
			    &first, &last, addr, sz,
			    pmap_align, pmap_offset, prot, hint);
		}

		if (error != 0)
			goto unlock;
	} else {
		/* Update freelists from vmspace. */
		if (map->flags & VM_MAP_ISVMSPACE)
			uvm_map_vmspace_update(map, &dead, flags);

		error = uvm_map_findspace(map, &first, &last, addr, sz,
		    pmap_align, pmap_offset, prot, hint);

		/* Grow kernel memory and try again. */
		if (error != 0 && (map->flags & VM_MAP_ISVMSPACE) == 0) {
			uvm_map_kmem_grow(map, &dead, sz, flags);

			error = uvm_map_findspace(map, &first, &last, addr, sz,
			    pmap_align, pmap_offset, prot, hint);
		}

		if (error != 0)
			goto unlock;
	}

	KASSERT((map->flags & VM_MAP_ISVMSPACE) == VM_MAP_ISVMSPACE ||
	    uvm_maxkaddr >= *addr + sz);

	/* If we only want a query, return now. */
	if (flags & UVM_FLAG_QUERY) {
		error = 0;
		goto unlock;
	}

	if (uobj == NULL)
		uoffset = 0;
	else if (uoffset == UVM_UNKNOWN_OFFSET) {
		KASSERT(UVM_OBJ_IS_KERN_OBJECT(uobj));
		uoffset = *addr - vm_map_min(kernel_map);
	}

	/*
	 * Create new entry.
	 * first and last may be invalidated after this call.
	 */
	entry = uvm_map_mkentry(map, first, last, *addr, sz, flags, &dead,
	    new);
	if (entry == NULL) {
		error = ENOMEM;
		goto unlock;
	}
	new = NULL;
	KDASSERT(entry->start == *addr && entry->end == *addr + sz);
	entry->object.uvm_obj = uobj;
	entry->offset = uoffset;
	entry->protection = prot;
	entry->max_protection = maxprot;
	entry->inheritance = inherit;
	entry->wired_count = 0;
	entry->advice = advice;
	if (uobj)
		entry->etype |= UVM_ET_OBJ;
	else if (flags & UVM_FLAG_HOLE)
		entry->etype |= UVM_ET_HOLE;
	if (flags & UVM_FLAG_NOFAULT)
		entry->etype |= UVM_ET_NOFAULT;
	if (flags & UVM_FLAG_COPYONW) {
		entry->etype |= UVM_ET_COPYONWRITE;
		if ((flags & UVM_FLAG_OVERLAY) == 0)
			entry->etype |= UVM_ET_NEEDSCOPY;
	}
	if (flags & UVM_FLAG_OVERLAY) {
		entry->aref.ar_pageoff = 0;
		entry->aref.ar_amap = amap_alloc(sz,
		    ptoa(flags & UVM_FLAG_AMAPPAD ? UVM_AMAP_CHUNK : 0),
		    M_WAITOK);
	}

	/* Update map and process statistics. */
	if (!(flags & UVM_FLAG_HOLE)) {
		map->size += sz;
		if ((map->flags & VM_MAP_ISVMSPACE) && uobj == NULL) {
			((struct vmspace *)map)->vm_dused +=
			    uvmspace_dused(map, *addr, *addr + sz);
		}
	}

	/*
	 * Try to merge entry.
	 *
	 * Userland allocations are kept separated most of the time.
	 * Forego the effort of merging what most of the time can't be merged
	 * and only try the merge if it concerns a kernel entry.
	 */
	if ((flags & UVM_FLAG_NOMERGE) == 0 &&
	    (map->flags & VM_MAP_ISVMSPACE) == 0)
		uvm_mapent_tryjoin(map, entry, &dead);

unlock:
	vm_map_unlock(map);

	/*
	 * Remove dead entries.
	 *
	 * Dead entries may be the result of merging.
	 * uvm_map_mkentry may also create dead entries, when it attempts to
	 * destroy free-space entries.
	 */
	uvm_unmap_detach(&dead, 0);
out:
	if (new)
		uvm_mapent_free(new);
	return error;
}

/*
 * True iff e1 and e2 can be joined together.
 */
int
uvm_mapent_isjoinable(struct vm_map *map, struct vm_map_entry *e1,
    struct vm_map_entry *e2)
{
	KDASSERT(e1 != NULL && e2 != NULL);

	/* Must be the same entry type and not have free memory between. */
	if (e1->etype != e2->etype || e1->end != e2->start)
		return 0;

	/* Submaps are never joined. */
	if (UVM_ET_ISSUBMAP(e1))
		return 0;

	/* Never merge wired memory. */
	if (VM_MAPENT_ISWIRED(e1) || VM_MAPENT_ISWIRED(e2))
		return 0;

	/* Protection, inheritance and advice must be equal. */
	if (e1->protection != e2->protection ||
	    e1->max_protection != e2->max_protection ||
	    e1->inheritance != e2->inheritance ||
	    e1->advice != e2->advice)
		return 0;

	/* If uvm_object: object itself and offsets within object must match. */
	if (UVM_ET_ISOBJ(e1)) {
		if (e1->object.uvm_obj != e2->object.uvm_obj)
			return 0;
		if (e1->offset + (e1->end - e1->start) != e2->offset)
			return 0;
	}

	/*
	 * Cannot join shared amaps.
	 * Note: no need to lock amap to look at refs, since we don't care
	 * about its exact value.
	 * If it is 1 (i.e. we have the only reference) it will stay there.
	 */
	if (e1->aref.ar_amap && amap_refs(e1->aref.ar_amap) != 1)
		return 0;
	if (e2->aref.ar_amap && amap_refs(e2->aref.ar_amap) != 1)
		return 0;

	/* Apprently, e1 and e2 match. */
	return 1;
}

/*
 * Join support function.
 *
 * Returns the merged entry on succes.
 * Returns NULL if the merge failed.
 */
struct vm_map_entry*
uvm_mapent_merge(struct vm_map *map, struct vm_map_entry *e1,
    struct vm_map_entry *e2, struct uvm_map_deadq *dead)
{
	struct uvm_addr_state *free;

	/*
	 * Amap of e1 must be extended to include e2.
	 * e2 contains no real information in its amap,
	 * so it can be erased immediately.
	 */
	if (e1->aref.ar_amap) {
		if (amap_extend(e1, e2->end - e2->start))
			return NULL;
	}

	/*
	 * Don't drop obj reference:
	 * uvm_unmap_detach will do this for us.
	 */
	free = uvm_map_uaddr_e(map, e1);
	uvm_mapent_free_remove(map, free, e1);

	free = uvm_map_uaddr_e(map, e2);
	uvm_mapent_free_remove(map, free, e2);
	uvm_mapent_addr_remove(map, e2);
	e1->end = e2->end;
	e1->guard = e2->guard;
	e1->fspace = e2->fspace;
	uvm_mapent_free_insert(map, free, e1);

	DEAD_ENTRY_PUSH(dead, e2);
	return e1;
}

/*
 * Attempt forward and backward joining of entry.
 *
 * Returns entry after joins.
 * We are guaranteed that the amap of entry is either non-existant or
 * has never been used.
 */
struct vm_map_entry*
uvm_mapent_tryjoin(struct vm_map *map, struct vm_map_entry *entry,
    struct uvm_map_deadq *dead)
{
	struct vm_map_entry *other;
	struct vm_map_entry *merged;

	/* Merge with previous entry. */
	other = RB_PREV(uvm_map_addr, &map->addr, entry);
	if (other && uvm_mapent_isjoinable(map, other, entry)) {
		merged = uvm_mapent_merge(map, other, entry, dead);
		if (merged)
			entry = merged;
	}

	/*
	 * Merge with next entry.
	 *
	 * Because amap can only extend forward and the next entry
	 * probably contains sensible info, only perform forward merging
	 * in the absence of an amap.
	 */
	other = RB_NEXT(uvm_map_addr, &map->addr, entry);
	if (other && entry->aref.ar_amap == NULL &&
	    other->aref.ar_amap == NULL &&
	    uvm_mapent_isjoinable(map, entry, other)) {
		merged = uvm_mapent_merge(map, entry, other, dead);
		if (merged)
			entry = merged;
	}

	return entry;
}

/*
 * Kill entries that are no longer in a map.
 */
void
uvm_unmap_detach(struct uvm_map_deadq *deadq, int flags)
{
	struct vm_map_entry *entry;
	int waitok = flags & UVM_PLA_WAITOK;

	if (TAILQ_EMPTY(deadq))
		return;

	KERNEL_LOCK();
	while ((entry = TAILQ_FIRST(deadq)) != NULL) {
		if (waitok)
			uvm_pause();
		/* Drop reference to amap, if we've got one. */
		if (entry->aref.ar_amap)
			amap_unref(entry->aref.ar_amap,
			    entry->aref.ar_pageoff,
			    atop(entry->end - entry->start),
			    flags & AMAP_REFALL);

		/* Drop reference to our backing object, if we've got one. */
		if (UVM_ET_ISSUBMAP(entry)) {
			/* ... unlikely to happen, but play it safe */
			uvm_map_deallocate(entry->object.sub_map);
		} else if (UVM_ET_ISOBJ(entry) &&
		    entry->object.uvm_obj->pgops->pgo_detach) {
			entry->object.uvm_obj->pgops->pgo_detach(
			    entry->object.uvm_obj);
		}

		/* Step to next. */
		TAILQ_REMOVE(deadq, entry, dfree.deadq);
		uvm_mapent_free(entry);
	}
	KERNEL_UNLOCK();
}

/*
 * Create and insert new entry.
 *
 * Returned entry contains new addresses and is inserted properly in the tree.
 * first and last are (probably) no longer valid.
 */
struct vm_map_entry*
uvm_map_mkentry(struct vm_map *map, struct vm_map_entry *first,
    struct vm_map_entry *last, vaddr_t addr, vsize_t sz, int flags,
    struct uvm_map_deadq *dead, struct vm_map_entry *new)
{
	struct vm_map_entry *entry, *prev;
	struct uvm_addr_state *free;
	vaddr_t min, max;	/* free space boundaries for new entry */

	KDASSERT(map != NULL);
	KDASSERT(first != NULL);
	KDASSERT(last != NULL);
	KDASSERT(dead != NULL);
	KDASSERT(sz > 0);
	KDASSERT(addr + sz > addr);
	KDASSERT(first->end <= addr && VMMAP_FREE_END(first) > addr);
	KDASSERT(last->start < addr + sz && VMMAP_FREE_END(last) >= addr + sz);
	KDASSERT(uvm_map_isavail(map, NULL, &first, &last, addr, sz));
	uvm_tree_sanity(map, __FILE__, __LINE__);

	min = addr + sz;
	max = VMMAP_FREE_END(last);

	/* Initialize new entry. */
	if (new == NULL)
		entry = uvm_mapent_alloc(map, flags);
	else
		entry = new;
	if (entry == NULL)
		return NULL;
	entry->offset = 0;
	entry->etype = 0;
	entry->wired_count = 0;
	entry->aref.ar_pageoff = 0;
	entry->aref.ar_amap = NULL;

	entry->start = addr;
	entry->end = min;
	entry->guard = 0;
	entry->fspace = 0;

	/* Reset free space in first. */
	free = uvm_map_uaddr_e(map, first);
	uvm_mapent_free_remove(map, free, first);
	first->guard = 0;
	first->fspace = 0;

	/*
	 * Remove all entries that are fully replaced.
	 * We are iterating using last in reverse order.
	 */
	for (; first != last; last = prev) {
		prev = RB_PREV(uvm_map_addr, &map->addr, last);

		KDASSERT(last->start == last->end);
		free = uvm_map_uaddr_e(map, last);
		uvm_mapent_free_remove(map, free, last);
		uvm_mapent_addr_remove(map, last);
		DEAD_ENTRY_PUSH(dead, last);
	}
	/* Remove first if it is entirely inside <addr, addr+sz>.  */
	if (first->start == addr) {
		uvm_mapent_addr_remove(map, first);
		DEAD_ENTRY_PUSH(dead, first);
	} else {
		uvm_map_fix_space(map, first, VMMAP_FREE_START(first),
		    addr, flags);
	}

	/* Finally, link in entry. */
	uvm_mapent_addr_insert(map, entry);
	uvm_map_fix_space(map, entry, min, max, flags);

	uvm_tree_sanity(map, __FILE__, __LINE__);
	return entry;
}


/*
 * uvm_mapent_alloc: allocate a map entry
 */
struct vm_map_entry *
uvm_mapent_alloc(struct vm_map *map, int flags)
{
	struct vm_map_entry *me, *ne;
	int s, i;
	int pool_flags;

	pool_flags = PR_WAITOK;
	if (flags & UVM_FLAG_TRYLOCK)
		pool_flags = PR_NOWAIT;

	if (map->flags & VM_MAP_INTRSAFE || cold) {
		s = splvm();
		me = uvm.kentry_free;
		if (me == NULL) {
			ne = km_alloc(PAGE_SIZE, &kv_page, &kp_dirty,
			    &kd_nowait);
			if (ne == NULL)
				panic("uvm_mapent_alloc: cannot allocate map "
				    "entry");
			for (i = 0;
			    i < PAGE_SIZE / sizeof(struct vm_map_entry) - 1;
			    i++)
				RB_LEFT(&ne[i], daddrs.addr_entry) = &ne[i + 1];
			RB_LEFT(&ne[i], daddrs.addr_entry) = NULL;
			me = ne;
			if (ratecheck(&uvm_kmapent_last_warn_time,
			    &uvm_kmapent_warn_rate))
				printf("uvm_mapent_alloc: out of static "
				    "map entries\n");
		}
		uvm.kentry_free = RB_LEFT(me, daddrs.addr_entry);
		uvmexp.kmapent++;
		splx(s);
		me->flags = UVM_MAP_STATIC;
	} else if (map == kernel_map) {
		splassert(IPL_NONE);
		me = pool_get(&uvm_map_entry_kmem_pool, pool_flags);
		if (me == NULL)
			goto out;
		me->flags = UVM_MAP_KMEM;
	} else {
		splassert(IPL_NONE);
		me = pool_get(&uvm_map_entry_pool, pool_flags);
		if (me == NULL)
			goto out;
		me->flags = 0;
	}

	if (me != NULL) {
		RB_LEFT(me, daddrs.addr_entry) =
		    RB_RIGHT(me, daddrs.addr_entry) =
		    RB_PARENT(me, daddrs.addr_entry) = UVMMAP_DEADBEEF;
	}

out:
	return(me);
}

/*
 * uvm_mapent_free: free map entry
 *
 * => XXX: static pool for kernel map?
 */
void
uvm_mapent_free(struct vm_map_entry *me)
{
	int s;

	if (me->flags & UVM_MAP_STATIC) {
		s = splvm();
		RB_LEFT(me, daddrs.addr_entry) = uvm.kentry_free;
		uvm.kentry_free = me;
		uvmexp.kmapent--;
		splx(s);
	} else if (me->flags & UVM_MAP_KMEM) {
		splassert(IPL_NONE);
		pool_put(&uvm_map_entry_kmem_pool, me);
	} else {
		splassert(IPL_NONE);
		pool_put(&uvm_map_entry_pool, me);
	}
}

/*
 * uvm_map_lookup_entry: find map entry at or before an address.
 *
 * => map must at least be read-locked by caller
 * => entry is returned in "entry"
 * => return value is true if address is in the returned entry
 * ET_HOLE entries are considered to not contain a mapping, ergo FALSE is
 * returned for those mappings.
 */
boolean_t
uvm_map_lookup_entry(struct vm_map *map, vaddr_t address,
    struct vm_map_entry **entry)
{
	*entry = uvm_map_entrybyaddr(&map->addr, address);
	return *entry != NULL && !UVM_ET_ISHOLE(*entry) &&
	    (*entry)->start <= address && (*entry)->end > address;
}

/*
 * uvm_map_pie: return a random load address for a PIE executable
 * properly aligned.
 */
#ifndef VM_PIE_MAX_ADDR
#define VM_PIE_MAX_ADDR (VM_MAXUSER_ADDRESS / 4)
#endif

#ifndef VM_PIE_MIN_ADDR
#define VM_PIE_MIN_ADDR VM_MIN_ADDRESS
#endif

#ifndef VM_PIE_MIN_ALIGN
#define VM_PIE_MIN_ALIGN PAGE_SIZE
#endif

vaddr_t
uvm_map_pie(vaddr_t align)
{
	vaddr_t addr, space, min;

	align = MAX(align, VM_PIE_MIN_ALIGN);

	/* round up to next alignment */
	min = (VM_PIE_MIN_ADDR + align - 1) & ~(align - 1);

	if (align >= VM_PIE_MAX_ADDR || min >= VM_PIE_MAX_ADDR)
		return (align);

	space = (VM_PIE_MAX_ADDR - min) / align;
	space = MIN(space, (u_int32_t)-1);

	addr = (vaddr_t)arc4random_uniform((u_int32_t)space) * align;
	addr += min;

	return (addr);
}

void
uvm_unmap(struct vm_map *map, vaddr_t start, vaddr_t end)
{
	struct uvm_map_deadq dead;

	KASSERT((start & (vaddr_t)PAGE_MASK) == 0 &&
	    (end & (vaddr_t)PAGE_MASK) == 0);
	TAILQ_INIT(&dead);
	vm_map_lock(map);
	uvm_unmap_remove(map, start, end, &dead, FALSE, TRUE);
	vm_map_unlock(map);

	uvm_unmap_detach(&dead, 0);
}

/*
 * Mark entry as free.
 *
 * entry will be put on the dead list.
 * The free space will be merged into the previous or a new entry,
 * unless markfree is false.
 */
void
uvm_mapent_mkfree(struct vm_map *map, struct vm_map_entry *entry,
    struct vm_map_entry **prev_ptr, struct uvm_map_deadq *dead,
    boolean_t markfree)
{
	struct uvm_addr_state	*free;
	struct vm_map_entry	*prev;
	vaddr_t			 addr;	/* Start of freed range. */
	vaddr_t			 end;	/* End of freed range. */

	prev = *prev_ptr;
	if (prev == entry)
		*prev_ptr = prev = NULL;

	if (prev == NULL ||
	    VMMAP_FREE_END(prev) != entry->start)
		prev = RB_PREV(uvm_map_addr, &map->addr, entry);

	/* Entry is describing only free memory and has nothing to drain into. */
	if (prev == NULL && entry->start == entry->end && markfree) {
		*prev_ptr = entry;
		return;
	}

	addr = entry->start;
	end = VMMAP_FREE_END(entry);
	free = uvm_map_uaddr_e(map, entry);
	uvm_mapent_free_remove(map, free, entry);
	uvm_mapent_addr_remove(map, entry);
	DEAD_ENTRY_PUSH(dead, entry);

	if (markfree) {
		if (prev) {
			free = uvm_map_uaddr_e(map, prev);
			uvm_mapent_free_remove(map, free, prev);
		}
		*prev_ptr = uvm_map_fix_space(map, prev, addr, end, 0);
	}
}

/*
 * Unwire and release referenced amap and object from map entry.
 */
void
uvm_unmap_kill_entry(struct vm_map *map, struct vm_map_entry *entry)
{
	/* Unwire removed map entry. */
	if (VM_MAPENT_ISWIRED(entry)) {
		KERNEL_LOCK();
		entry->wired_count = 0;
		uvm_fault_unwire_locked(map, entry->start, entry->end);
		KERNEL_UNLOCK();
	}

	/* Entry-type specific code. */
	if (UVM_ET_ISHOLE(entry)) {
		/* Nothing to be done for holes. */
	} else if (map->flags & VM_MAP_INTRSAFE) {
		KASSERT(vm_map_pmap(map) == pmap_kernel());
		uvm_km_pgremove_intrsafe(entry->start, entry->end);
		pmap_kremove(entry->start, entry->end - entry->start);
	} else if (UVM_ET_ISOBJ(entry) &&
	    UVM_OBJ_IS_KERN_OBJECT(entry->object.uvm_obj)) {
		KASSERT(vm_map_pmap(map) == pmap_kernel());
		/*
		 * Note: kernel object mappings are currently used in
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
		 *   for pages in the kernel object range:
		 *     - drops the swap slot
		 *     - uvm_pagefree the page
		 *
		 * note there is version of uvm_km_pgremove() that
		 * is used for "intrsafe" objects.
		 */
		/*
		 * remove mappings from pmap and drop the pages
		 * from the object.  offsets are always relative
		 * to vm_map_min(kernel_map).
		 */
		pmap_remove(pmap_kernel(), entry->start, entry->end);
		uvm_km_pgremove(entry->object.uvm_obj,
		    entry->start - vm_map_min(kernel_map),
		    entry->end - vm_map_min(kernel_map));

		/*
		 * null out kernel_object reference, we've just
		 * dropped it
		 */
		entry->etype &= ~UVM_ET_OBJ;
		entry->object.uvm_obj = NULL;  /* to be safe */
	} else {
		/* remove mappings the standard way. */
		pmap_remove(map->pmap, entry->start, entry->end);
	}
}

/*
 * Remove all entries from start to end.
 *
 * If remove_holes, then remove ET_HOLE entries as well.
 * If markfree, entry will be properly marked free, otherwise, no replacement
 * entry will be put in the tree (corrupting the tree).
 */
void
uvm_unmap_remove(struct vm_map *map, vaddr_t start, vaddr_t end,
    struct uvm_map_deadq *dead, boolean_t remove_holes,
    boolean_t markfree)
{
	struct vm_map_entry *prev_hint, *next, *entry;

	start = MAX(start, map->min_offset);
	end = MIN(end, map->max_offset);
	if (start >= end)
		return;

	if ((map->flags & VM_MAP_INTRSAFE) == 0)
		splassert(IPL_NONE);
	else
		splassert(IPL_VM);

	/* Find first affected entry. */
	entry = uvm_map_entrybyaddr(&map->addr, start);
	KDASSERT(entry != NULL && entry->start <= start);
	if (entry->end <= start && markfree)
		entry = RB_NEXT(uvm_map_addr, &map->addr, entry);
	else
		UVM_MAP_CLIP_START(map, entry, start);

	/*
	 * Iterate entries until we reach end address.
	 * prev_hint hints where the freed space can be appended to.
	 */
	prev_hint = NULL;
	for (; entry != NULL && entry->start < end; entry = next) {
		KDASSERT(entry->start >= start);
		if (entry->end > end || !markfree)
			UVM_MAP_CLIP_END(map, entry, end);
		KDASSERT(entry->start >= start && entry->end <= end);
		next = RB_NEXT(uvm_map_addr, &map->addr, entry);

		/* Don't remove holes unless asked to do so. */
		if (UVM_ET_ISHOLE(entry)) {
			if (!remove_holes) {
				prev_hint = entry;
				continue;
			}
		}

		/* Kill entry. */
		uvm_unmap_kill_entry(map, entry);

		/* Update space usage. */
		if ((map->flags & VM_MAP_ISVMSPACE) &&
		    entry->object.uvm_obj == NULL &&
		    !UVM_ET_ISHOLE(entry)) {
			((struct vmspace *)map)->vm_dused -=
			    uvmspace_dused(map, entry->start, entry->end);
		}
		if (!UVM_ET_ISHOLE(entry))
			map->size -= entry->end - entry->start;

		/* Actual removal of entry. */
		uvm_mapent_mkfree(map, entry, &prev_hint, dead, markfree);
	}

	pmap_update(vm_map_pmap(map));

#ifdef VMMAP_DEBUG
	if (markfree) {
		for (entry = uvm_map_entrybyaddr(&map->addr, start);
		    entry != NULL && entry->start < end;
		    entry = RB_NEXT(uvm_map_addr, &map->addr, entry)) {
			KDASSERT(entry->end <= start ||
			    entry->start == entry->end ||
			    UVM_ET_ISHOLE(entry));
		}
	} else {
		vaddr_t a;
		for (a = start; a < end; a += PAGE_SIZE)
			KDASSERT(uvm_map_entrybyaddr(&map->addr, a) == NULL);
	}
#endif
}

/*
 * Mark all entries from first until end (exclusive) as pageable.
 *
 * Lock must be exclusive on entry and will not be touched.
 */
void
uvm_map_pageable_pgon(struct vm_map *map, struct vm_map_entry *first,
    struct vm_map_entry *end, vaddr_t start_addr, vaddr_t end_addr)
{
	struct vm_map_entry *iter;

	for (iter = first; iter != end;
	    iter = RB_NEXT(uvm_map_addr, &map->addr, iter)) {
		KDASSERT(iter->start >= start_addr && iter->end <= end_addr);
		if (!VM_MAPENT_ISWIRED(iter) || UVM_ET_ISHOLE(iter))
			continue;

		iter->wired_count = 0;
		uvm_fault_unwire_locked(map, iter->start, iter->end);
	}
}

/*
 * Mark all entries from first until end (exclusive) as wired.
 *
 * Lockflags determines the lock state on return from this function.
 * Lock must be exclusive on entry.
 */
int
uvm_map_pageable_wire(struct vm_map *map, struct vm_map_entry *first,
    struct vm_map_entry *end, vaddr_t start_addr, vaddr_t end_addr,
    int lockflags)
{
	struct vm_map_entry *iter;
#ifdef DIAGNOSTIC
	unsigned int timestamp_save;
#endif
	int error;

	/*
	 * Wire pages in two passes:
	 *
	 * 1: holding the write lock, we create any anonymous maps that need
	 *    to be created.  then we clip each map entry to the region to
	 *    be wired and increment its wiring count.
	 *
	 * 2: we downgrade to a read lock, and call uvm_fault_wire to fault
	 *    in the pages for any newly wired area (wired_count == 1).
	 *
	 *    downgrading to a read lock for uvm_fault_wire avoids a possible
	 *    deadlock with another thread that may have faulted on one of
	 *    the pages to be wired (it would mark the page busy, blocking
	 *    us, then in turn block on the map lock that we hold).
	 *    because we keep the read lock on the map, the copy-on-write
	 *    status of the entries we modify here cannot change.
	 */
	for (iter = first; iter != end;
	    iter = RB_NEXT(uvm_map_addr, &map->addr, iter)) {
		KDASSERT(iter->start >= start_addr && iter->end <= end_addr);
		if (UVM_ET_ISHOLE(iter) || iter->start == iter->end ||
		    iter->protection == PROT_NONE)
			continue;

		/*
		 * Perform actions of vm_map_lookup that need the write lock.
		 * - create an anonymous map for copy-on-write
		 * - anonymous map for zero-fill
		 * Skip submaps.
		 */
		if (!VM_MAPENT_ISWIRED(iter) && !UVM_ET_ISSUBMAP(iter) &&
		    UVM_ET_ISNEEDSCOPY(iter) &&
		    ((iter->protection & PROT_WRITE) ||
		    iter->object.uvm_obj == NULL)) {
			amap_copy(map, iter, M_WAITOK, TRUE,
			    iter->start, iter->end);
		}
		iter->wired_count++;
	}

	/*
	 * Pass 2.
	 */
#ifdef DIAGNOSTIC
	timestamp_save = map->timestamp;
#endif
	vm_map_busy(map);
	vm_map_downgrade(map);

	error = 0;
	for (iter = first; error == 0 && iter != end;
	    iter = RB_NEXT(uvm_map_addr, &map->addr, iter)) {
		if (UVM_ET_ISHOLE(iter) || iter->start == iter->end ||
		    iter->protection == PROT_NONE)
			continue;

		error = uvm_fault_wire(map, iter->start, iter->end,
		    iter->protection);
	}

	if (error) {
		/*
		 * uvm_fault_wire failure
		 *
		 * Reacquire lock and undo our work.
		 */
		vm_map_upgrade(map);
		vm_map_unbusy(map);
#ifdef DIAGNOSTIC
		if (timestamp_save != map->timestamp)
			panic("uvm_map_pageable_wire: stale map");
#endif

		/*
		 * first is no longer needed to restart loops.
		 * Use it as iterator to unmap successful mappings.
		 */
		for (; first != iter;
		    first = RB_NEXT(uvm_map_addr, &map->addr, first)) {
			if (UVM_ET_ISHOLE(first) ||
			    first->start == first->end ||
			    first->protection == PROT_NONE)
				continue;

			first->wired_count--;
			if (!VM_MAPENT_ISWIRED(first)) {
				uvm_fault_unwire_locked(map,
				    iter->start, iter->end);
			}
		}

		/* decrease counter in the rest of the entries */
		for (; iter != end;
		    iter = RB_NEXT(uvm_map_addr, &map->addr, iter)) {
			if (UVM_ET_ISHOLE(iter) || iter->start == iter->end ||
			    iter->protection == PROT_NONE)
				continue;

			iter->wired_count--;
		}

		if ((lockflags & UVM_LK_EXIT) == 0)
			vm_map_unlock(map);
		return error;
	}

	/* We are currently holding a read lock. */
	if ((lockflags & UVM_LK_EXIT) == 0) {
		vm_map_unbusy(map);
		vm_map_unlock_read(map);
	} else {
		vm_map_upgrade(map);
		vm_map_unbusy(map);
#ifdef DIAGNOSTIC
		if (timestamp_save != map->timestamp)
			panic("uvm_map_pageable_wire: stale map");
#endif
	}
	return 0;
}

/*
 * uvm_map_pageable: set pageability of a range in a map.
 *
 * Flags:
 * UVM_LK_ENTER: map is already locked by caller
 * UVM_LK_EXIT:  don't unlock map on exit
 *
 * The full range must be in use (entries may not have fspace != 0).
 * UVM_ET_HOLE counts as unmapped.
 */
int
uvm_map_pageable(struct vm_map *map, vaddr_t start, vaddr_t end,
    boolean_t new_pageable, int lockflags)
{
	struct vm_map_entry *first, *last, *tmp;
	int error;

	start = trunc_page(start);
	end = round_page(end);

	if (start > end)
		return EINVAL;
	if (start == end)
		return 0;	/* nothing to do */
	if (start < map->min_offset)
		return EFAULT; /* why? see first XXX below */
	if (end > map->max_offset)
		return EINVAL; /* why? see second XXX below */

	KASSERT(map->flags & VM_MAP_PAGEABLE);
	if ((lockflags & UVM_LK_ENTER) == 0)
		vm_map_lock(map);

	/*
	 * Find first entry.
	 *
	 * Initial test on start is different, because of the different
	 * error returned. Rest is tested further down.
	 */
	first = uvm_map_entrybyaddr(&map->addr, start);
	if (first->end <= start || UVM_ET_ISHOLE(first)) {
		/*
		 * XXX if the first address is not mapped, it is EFAULT?
		 */
		error = EFAULT;
		goto out;
	}

	/* Check that the range has no holes. */
	for (last = first; last != NULL && last->start < end;
	    last = RB_NEXT(uvm_map_addr, &map->addr, last)) {
		if (UVM_ET_ISHOLE(last) ||
		    (last->end < end && VMMAP_FREE_END(last) != last->end)) {
			/*
			 * XXX unmapped memory in range, why is it EINVAL
			 * instead of EFAULT?
			 */
			error = EINVAL;
			goto out;
		}
	}

	/*
	 * Last ended at the first entry after the range.
	 * Move back one step.
	 *
	 * Note that last may be NULL.
	 */
	if (last == NULL) {
		last = RB_MAX(uvm_map_addr, &map->addr);
		if (last->end < end) {
			error = EINVAL;
			goto out;
		}
	} else {
		KASSERT(last != first);
		last = RB_PREV(uvm_map_addr, &map->addr, last);
	}

	/* Wire/unwire pages here. */
	if (new_pageable) {
		/*
		 * Mark pageable.
		 * entries that are not wired are untouched.
		 */
		if (VM_MAPENT_ISWIRED(first))
			UVM_MAP_CLIP_START(map, first, start);
		/*
		 * Split last at end.
		 * Make tmp be the first entry after what is to be touched.
		 * If last is not wired, don't touch it.
		 */
		if (VM_MAPENT_ISWIRED(last)) {
			UVM_MAP_CLIP_END(map, last, end);
			tmp = RB_NEXT(uvm_map_addr, &map->addr, last);
		} else
			tmp = last;

		uvm_map_pageable_pgon(map, first, tmp, start, end);
		error = 0;

out:
		if ((lockflags & UVM_LK_EXIT) == 0)
			vm_map_unlock(map);
		return error;
	} else {
		/*
		 * Mark entries wired.
		 * entries are always touched (because recovery needs this).
		 */
		if (!VM_MAPENT_ISWIRED(first))
			UVM_MAP_CLIP_START(map, first, start);
		/*
		 * Split last at end.
		 * Make tmp be the first entry after what is to be touched.
		 * If last is not wired, don't touch it.
		 */
		if (!VM_MAPENT_ISWIRED(last)) {
			UVM_MAP_CLIP_END(map, last, end);
			tmp = RB_NEXT(uvm_map_addr, &map->addr, last);
		} else
			tmp = last;

		return uvm_map_pageable_wire(map, first, tmp, start, end,
		    lockflags);
	}
}

/*
 * uvm_map_pageable_all: special case of uvm_map_pageable - affects
 * all mapped regions.
 *
 * Map must not be locked.
 * If no flags are specified, all ragions are unwired.
 */
int
uvm_map_pageable_all(struct vm_map *map, int flags, vsize_t limit)
{
	vsize_t size;
	struct vm_map_entry *iter;

	KASSERT(map->flags & VM_MAP_PAGEABLE);
	vm_map_lock(map);

	if (flags == 0) {
		uvm_map_pageable_pgon(map, RB_MIN(uvm_map_addr, &map->addr),
		    NULL, map->min_offset, map->max_offset);

		vm_map_modflags(map, 0, VM_MAP_WIREFUTURE);
		vm_map_unlock(map);
		return 0;
	}

	if (flags & MCL_FUTURE)
		vm_map_modflags(map, VM_MAP_WIREFUTURE, 0);
	if (!(flags & MCL_CURRENT)) {
		vm_map_unlock(map);
		return 0;
	}

	/*
	 * Count number of pages in all non-wired entries.
	 * If the number exceeds the limit, abort.
	 */
	size = 0;
	RB_FOREACH(iter, uvm_map_addr, &map->addr) {
		if (VM_MAPENT_ISWIRED(iter) || UVM_ET_ISHOLE(iter))
			continue;

		size += iter->end - iter->start;
	}

	if (atop(size) + uvmexp.wired > uvmexp.wiredmax) {
		vm_map_unlock(map);
		return ENOMEM;
	}

	/* XXX non-pmap_wired_count case must be handled by caller */
#ifdef pmap_wired_count
	if (limit != 0 &&
	    size + ptoa(pmap_wired_count(vm_map_pmap(map))) > limit) {
		vm_map_unlock(map);
		return ENOMEM;
	}
#endif

	/*
	 * uvm_map_pageable_wire will release lcok
	 */
	return uvm_map_pageable_wire(map, RB_MIN(uvm_map_addr, &map->addr),
	    NULL, map->min_offset, map->max_offset, 0);
}

/*
 * Initialize map.
 *
 * Allocates sufficient entries to describe the free memory in the map.
 */
void
uvm_map_setup(struct vm_map *map, vaddr_t min, vaddr_t max, int flags)
{
	int i;

	KASSERT((min & (vaddr_t)PAGE_MASK) == 0);
	KASSERT((max & (vaddr_t)PAGE_MASK) == 0 ||
	    (max & (vaddr_t)PAGE_MASK) == (vaddr_t)PAGE_MASK);

	/*
	 * Update parameters.
	 *
	 * This code handles (vaddr_t)-1 and other page mask ending addresses
	 * properly.
	 * We lose the top page if the full virtual address space is used.
	 */
	if (max & (vaddr_t)PAGE_MASK) {
		max += 1;
		if (max == 0) /* overflow */
			max -= PAGE_SIZE;
	}

	RB_INIT(&map->addr);
	map->uaddr_exe = NULL;
	for (i = 0; i < nitems(map->uaddr_any); ++i)
		map->uaddr_any[i] = NULL;
	map->uaddr_brk_stack = NULL;

	map->size = 0;
	map->ref_count = 1;
	map->min_offset = min;
	map->max_offset = max;
	map->b_start = map->b_end = 0; /* Empty brk() area by default. */
	map->s_start = map->s_end = 0; /* Empty stack area by default. */
	map->flags = flags;
	map->timestamp = 0;
	rw_init(&map->lock, "vmmaplk");
	mtx_init(&map->flags_lock, IPL_VM);

	/* Configure the allocators. */
	if (flags & VM_MAP_ISVMSPACE)
		uvm_map_setup_md(map);
	else
		map->uaddr_any[3] = &uaddr_kbootstrap;

	/*
	 * Fill map entries.
	 * This requires a write-locked map (because of diagnostic assertions
	 * in insert code).
	 */
	if ((map->flags & VM_MAP_INTRSAFE) == 0) {
		if (rw_enter(&map->lock, RW_NOSLEEP|RW_WRITE) != 0)
			panic("uvm_map_setup: rw_enter failed on new map");
	}
	uvm_map_setup_entries(map);
	uvm_tree_sanity(map, __FILE__, __LINE__);
	if ((map->flags & VM_MAP_INTRSAFE) == 0)
		rw_exit(&map->lock);
}

/*
 * Destroy the map.
 *
 * This is the inverse operation to uvm_map_setup.
 */
void
uvm_map_teardown(struct vm_map *map)
{
	struct uvm_map_deadq	 dead_entries;
	struct vm_map_entry	*entry, *tmp;
#ifdef VMMAP_DEBUG
	size_t			 numq, numt;
#endif
	int			 i;

	KERNEL_ASSERT_LOCKED();
	KERNEL_UNLOCK();
	KERNEL_ASSERT_UNLOCKED();

	KASSERT((map->flags & VM_MAP_INTRSAFE) == 0);

	if (rw_enter(&map->lock, RW_NOSLEEP | RW_WRITE) != 0)
		panic("uvm_map_teardown: rw_enter failed on free map");

	/* Remove address selectors. */
	uvm_addr_destroy(map->uaddr_exe);
	map->uaddr_exe = NULL;
	for (i = 0; i < nitems(map->uaddr_any); i++) {
		uvm_addr_destroy(map->uaddr_any[i]);
		map->uaddr_any[i] = NULL;
	}
	uvm_addr_destroy(map->uaddr_brk_stack);
	map->uaddr_brk_stack = NULL;

	/*
	 * Remove entries.
	 *
	 * The following is based on graph breadth-first search.
	 *
	 * In color terms:
	 * - the dead_entries set contains all nodes that are reachable
	 *   (i.e. both the black and the grey nodes)
	 * - any entry not in dead_entries is white
	 * - any entry that appears in dead_entries before entry,
	 *   is black, the rest is grey.
	 * The set [entry, end] is also referred to as the wavefront.
	 *
	 * Since the tree is always a fully connected graph, the breadth-first
	 * search guarantees that each vmmap_entry is visited exactly once.
	 * The vm_map is broken down in linear time.
	 */
	TAILQ_INIT(&dead_entries);
	if ((entry = RB_ROOT(&map->addr)) != NULL)
		DEAD_ENTRY_PUSH(&dead_entries, entry);
	while (entry != NULL) {
		sched_pause();
		uvm_unmap_kill_entry(map, entry);
		if ((tmp = RB_LEFT(entry, daddrs.addr_entry)) != NULL)
			DEAD_ENTRY_PUSH(&dead_entries, tmp);
		if ((tmp = RB_RIGHT(entry, daddrs.addr_entry)) != NULL)
			DEAD_ENTRY_PUSH(&dead_entries, tmp);
		/* Update wave-front. */
		entry = TAILQ_NEXT(entry, dfree.deadq);
	}

	rw_exit(&map->lock);

#ifdef VMMAP_DEBUG
	numt = numq = 0;
	RB_FOREACH(entry, uvm_map_addr, &map->addr)
		numt++;
	TAILQ_FOREACH(entry, &dead_entries, dfree.deadq)
		numq++;
	KASSERT(numt == numq);
#endif
	uvm_unmap_detach(&dead_entries, UVM_PLA_WAITOK);

	KERNEL_LOCK();

	pmap_destroy(map->pmap);
	map->pmap = NULL;
}

/*
 * Populate map with free-memory entries.
 *
 * Map must be initialized and empty.
 */
void
uvm_map_setup_entries(struct vm_map *map)
{
	KDASSERT(RB_EMPTY(&map->addr));

	uvm_map_fix_space(map, NULL, map->min_offset, map->max_offset, 0);
}

/*
 * Split entry at given address.
 *
 * orig:  entry that is to be split.
 * next:  a newly allocated map entry that is not linked.
 * split: address at which the split is done.
 */
void
uvm_map_splitentry(struct vm_map *map, struct vm_map_entry *orig,
    struct vm_map_entry *next, vaddr_t split)
{
	struct uvm_addr_state *free, *free_before;
	vsize_t adj;

	if ((split & PAGE_MASK) != 0) {
		panic("uvm_map_splitentry: split address 0x%lx "
		    "not on page boundary!", split);
	}
	KDASSERT(map != NULL && orig != NULL && next != NULL);
	uvm_tree_sanity(map, __FILE__, __LINE__);
	KASSERT(orig->start < split && VMMAP_FREE_END(orig) > split);

#ifdef VMMAP_DEBUG
	KDASSERT(RB_FIND(uvm_map_addr, &map->addr, orig) == orig);
	KDASSERT(RB_FIND(uvm_map_addr, &map->addr, next) != next);
#endif /* VMMAP_DEBUG */

	/*
	 * Free space will change, unlink from free space tree.
	 */
	free = uvm_map_uaddr_e(map, orig);
	uvm_mapent_free_remove(map, free, orig);

	adj = split - orig->start;

	uvm_mapent_copy(orig, next);
	if (split >= orig->end) {
		next->etype = 0;
		next->offset = 0;
		next->wired_count = 0;
		next->start = next->end = split;
		next->guard = 0;
		next->fspace = VMMAP_FREE_END(orig) - split;
		next->aref.ar_amap = NULL;
		next->aref.ar_pageoff = 0;
		orig->guard = MIN(orig->guard, split - orig->end);
		orig->fspace = split - VMMAP_FREE_START(orig);
	} else {
		orig->fspace = 0;
		orig->guard = 0;
		orig->end = next->start = split;

		if (next->aref.ar_amap)
			amap_splitref(&orig->aref, &next->aref, adj);
		if (UVM_ET_ISSUBMAP(orig)) {
			uvm_map_reference(next->object.sub_map);
			next->offset += adj;
		} else if (UVM_ET_ISOBJ(orig)) {
			if (next->object.uvm_obj->pgops &&
			    next->object.uvm_obj->pgops->pgo_reference) {
				next->object.uvm_obj->pgops->pgo_reference(
				    next->object.uvm_obj);
			}
			next->offset += adj;
		}
	}

	/*
	 * Link next into address tree.
	 * Link orig and next into free-space tree.
	 *
	 * Don't insert 'next' into the addr tree until orig has been linked,
	 * in case the free-list looks at adjecent entries in the addr tree
	 * for its decisions.
	 */
	if (orig->fspace > 0)
		free_before = free;
	else
		free_before = uvm_map_uaddr_e(map, orig);
	uvm_mapent_free_insert(map, free_before, orig);
	uvm_mapent_addr_insert(map, next);
	uvm_mapent_free_insert(map, free, next);

	uvm_tree_sanity(map, __FILE__, __LINE__);
}


#ifdef VMMAP_DEBUG

void
uvm_tree_assert(struct vm_map *map, int test, char *test_str,
    char *file, int line)
{
	char* map_special;

	if (test)
		return;

	if (map == kernel_map)
		map_special = " (kernel_map)";
	else if (map == kmem_map)
		map_special = " (kmem_map)";
	else
		map_special = "";
	panic("uvm_tree_sanity %p%s (%s %d): %s", map, map_special, file,
	    line, test_str);
}

/*
 * Check that map is sane.
 */
void
uvm_tree_sanity(struct vm_map *map, char *file, int line)
{
	struct vm_map_entry	*iter;
	vaddr_t			 addr;
	vaddr_t			 min, max, bound; /* Bounds checker. */
	struct uvm_addr_state	*free;

	addr = vm_map_min(map);
	RB_FOREACH(iter, uvm_map_addr, &map->addr) {
		/*
		 * Valid start, end.
		 * Catch overflow for end+fspace.
		 */
		UVM_ASSERT(map, iter->end >= iter->start, file, line);
		UVM_ASSERT(map, VMMAP_FREE_END(iter) >= iter->end, file, line);

		/* May not be empty. */
		UVM_ASSERT(map, iter->start < VMMAP_FREE_END(iter),
		    file, line);

		/* Addresses for entry must lie within map boundaries. */
		UVM_ASSERT(map, iter->start >= vm_map_min(map) &&
		    VMMAP_FREE_END(iter) <= vm_map_max(map), file, line);

		/* Tree may not have gaps. */
		UVM_ASSERT(map, iter->start == addr, file, line);
		addr = VMMAP_FREE_END(iter);

		/*
		 * Free space may not cross boundaries, unless the same
		 * free list is used on both sides of the border.
		 */
		min = VMMAP_FREE_START(iter);
		max = VMMAP_FREE_END(iter);

		while (min < max &&
		    (bound = uvm_map_boundary(map, min, max)) != max) {
			UVM_ASSERT(map,
			    uvm_map_uaddr(map, bound - 1) ==
			    uvm_map_uaddr(map, bound),
			    file, line);
			min = bound;
		}

		free = uvm_map_uaddr_e(map, iter);
		if (free) {
			UVM_ASSERT(map, (iter->etype & UVM_ET_FREEMAPPED) != 0,
			    file, line);
		} else {
			UVM_ASSERT(map, (iter->etype & UVM_ET_FREEMAPPED) == 0,
			    file, line);
		}
	}
	UVM_ASSERT(map, addr == vm_map_max(map), file, line);
}

void
uvm_tree_size_chk(struct vm_map *map, char *file, int line)
{
	struct vm_map_entry *iter;
	vsize_t size;

	size = 0;
	RB_FOREACH(iter, uvm_map_addr, &map->addr) {
		if (!UVM_ET_ISHOLE(iter))
			size += iter->end - iter->start;
	}

	if (map->size != size)
		printf("map size = 0x%lx, should be 0x%lx\n", map->size, size);
	UVM_ASSERT(map, map->size == size, file, line);

	vmspace_validate(map);
}

/*
 * This function validates the statistics on vmspace.
 */
void
vmspace_validate(struct vm_map *map)
{
	struct vmspace *vm;
	struct vm_map_entry *iter;
	vaddr_t imin, imax;
	vaddr_t stack_begin, stack_end; /* Position of stack. */
	vsize_t stack, heap; /* Measured sizes. */

	if (!(map->flags & VM_MAP_ISVMSPACE))
		return;

	vm = (struct vmspace *)map;
	stack_begin = MIN((vaddr_t)vm->vm_maxsaddr, (vaddr_t)vm->vm_minsaddr);
	stack_end = MAX((vaddr_t)vm->vm_maxsaddr, (vaddr_t)vm->vm_minsaddr);

	stack = heap = 0;
	RB_FOREACH(iter, uvm_map_addr, &map->addr) {
		imin = imax = iter->start;

		if (UVM_ET_ISHOLE(iter) || iter->object.uvm_obj != NULL)
			continue;

		/*
		 * Update stack, heap.
		 * Keep in mind that (theoretically) the entries of
		 * userspace and stack may be joined.
		 */
		while (imin != iter->end) {
			/*
			 * Set imax to the first boundary crossed between
			 * imin and stack addresses.
			 */
			imax = iter->end;
			if (imin < stack_begin && imax > stack_begin)
				imax = stack_begin;
			else if (imin < stack_end && imax > stack_end)
				imax = stack_end;

			if (imin >= stack_begin && imin < stack_end)
				stack += imax - imin;
			else
				heap += imax - imin;
			imin = imax;
		}
	}

	heap >>= PAGE_SHIFT;
	if (heap != vm->vm_dused) {
		printf("vmspace stack range: 0x%lx-0x%lx\n",
		    stack_begin, stack_end);
		panic("vmspace_validate: vmspace.vm_dused invalid, "
		    "expected %ld pgs, got %ld pgs in map %p",
		    heap, vm->vm_dused,
		    map);
	}
}

#endif /* VMMAP_DEBUG */

/*
 * uvm_map_init: init mapping system at boot time.   note that we allocate
 * and init the static pool of structs vm_map_entry for the kernel here.
 */
void
uvm_map_init(void)
{
	static struct vm_map_entry kernel_map_entry[MAX_KMAPENT];
	int lcv;

	/* now set up static pool of kernel map entries ... */
	uvm.kentry_free = NULL;
	for (lcv = 0 ; lcv < MAX_KMAPENT ; lcv++) {
		RB_LEFT(&kernel_map_entry[lcv], daddrs.addr_entry) =
		    uvm.kentry_free;
		uvm.kentry_free = &kernel_map_entry[lcv];
	}

	/* initialize the map-related pools. */
	pool_init(&uvm_vmspace_pool, sizeof(struct vmspace),
	    0, 0, PR_WAITOK, "vmsppl", NULL);
	pool_init(&uvm_map_entry_pool, sizeof(struct vm_map_entry),
	    0, 0, PR_WAITOK, "vmmpepl", NULL);
	pool_setipl(&uvm_map_entry_pool, IPL_VM);
	pool_init(&uvm_map_entry_kmem_pool, sizeof(struct vm_map_entry),
	    0, 0, 0, "vmmpekpl", NULL);
	pool_sethiwat(&uvm_map_entry_pool, 8192);

	uvm_addr_init();
}

#if defined(DDB)

/*
 * DDB hooks
 */

/*
 * uvm_map_printit: actually prints the map
 */
void
uvm_map_printit(struct vm_map *map, boolean_t full,
    int (*pr)(const char *, ...))
{
	struct vmspace			*vm;
	struct vm_map_entry		*entry;
	struct uvm_addr_state		*free;
	int				 in_free, i;
	char				 buf[8];

	(*pr)("MAP %p: [0x%lx->0x%lx]\n", map, map->min_offset,map->max_offset);
	(*pr)("\tbrk() allocate range: 0x%lx-0x%lx\n",
	    map->b_start, map->b_end);
	(*pr)("\tstack allocate range: 0x%lx-0x%lx\n",
	    map->s_start, map->s_end);
	(*pr)("\tsz=%u, ref=%d, version=%u, flags=0x%x\n",
	    map->size, map->ref_count, map->timestamp,
	    map->flags);
#ifdef pmap_resident_count
	(*pr)("\tpmap=%p(resident=%d)\n", map->pmap, 
	    pmap_resident_count(map->pmap));
#else
	/* XXXCDC: this should be required ... */
	(*pr)("\tpmap=%p(resident=<<NOT SUPPORTED!!!>>)\n", map->pmap);
#endif

	/* struct vmspace handling. */
	if (map->flags & VM_MAP_ISVMSPACE) {
		vm = (struct vmspace *)map;

		(*pr)("\tvm_refcnt=%d vm_shm=%p vm_rssize=%u vm_swrss=%u\n",
		    vm->vm_refcnt, vm->vm_shm, vm->vm_rssize, vm->vm_swrss);
		(*pr)("\tvm_tsize=%u vm_dsize=%u\n",
		    vm->vm_tsize, vm->vm_dsize);
		(*pr)("\tvm_taddr=%p vm_daddr=%p\n",
		    vm->vm_taddr, vm->vm_daddr);
		(*pr)("\tvm_maxsaddr=%p vm_minsaddr=%p\n",
		    vm->vm_maxsaddr, vm->vm_minsaddr);
	}

	if (!full)
		goto print_uaddr;
	RB_FOREACH(entry, uvm_map_addr, &map->addr) {
		(*pr)(" - %p: 0x%lx->0x%lx: obj=%p/0x%llx, amap=%p/%d\n",
		    entry, entry->start, entry->end, entry->object.uvm_obj,
		    (long long)entry->offset, entry->aref.ar_amap,
		    entry->aref.ar_pageoff);
		(*pr)("\tsubmap=%c, cow=%c, nc=%c, prot(max)=%d/%d, inh=%d, "
		    "wc=%d, adv=%d\n",
		    (entry->etype & UVM_ET_SUBMAP) ? 'T' : 'F',
		    (entry->etype & UVM_ET_COPYONWRITE) ? 'T' : 'F', 
		    (entry->etype & UVM_ET_NEEDSCOPY) ? 'T' : 'F',
		    entry->protection, entry->max_protection,
		    entry->inheritance, entry->wired_count, entry->advice);

		free = uvm_map_uaddr_e(map, entry);
		in_free = (free != NULL);
		(*pr)("\thole=%c, free=%c, guard=0x%lx, "
		    "free=0x%lx-0x%lx\n",
		    (entry->etype & UVM_ET_HOLE) ? 'T' : 'F',
		    in_free ? 'T' : 'F',
		    entry->guard,
		    VMMAP_FREE_START(entry), VMMAP_FREE_END(entry));
		(*pr)("\tfspace_augment=%lu\n", entry->fspace_augment);
		(*pr)("\tfreemapped=%c, uaddr=%p\n",
		    (entry->etype & UVM_ET_FREEMAPPED) ? 'T' : 'F', free);
		if (free) {
			(*pr)("\t\t(0x%lx-0x%lx %s)\n",
			    free->uaddr_minaddr, free->uaddr_maxaddr,
			    free->uaddr_functions->uaddr_name);
		}
	}

print_uaddr:
	uvm_addr_print(map->uaddr_exe, "exe", full, pr);
	for (i = 0; i < nitems(map->uaddr_any); i++) {
		snprintf(&buf[0], sizeof(buf), "any[%d]", i);
		uvm_addr_print(map->uaddr_any[i], &buf[0], full, pr);
	}
	uvm_addr_print(map->uaddr_brk_stack, "brk/stack", full, pr);
}

/*
 * uvm_object_printit: actually prints the object
 */
void
uvm_object_printit(uobj, full, pr)
	struct uvm_object *uobj;
	boolean_t full;
	int (*pr)(const char *, ...);
{
	struct vm_page *pg;
	int cnt = 0;

	(*pr)("OBJECT %p: pgops=%p, npages=%d, ",
	    uobj, uobj->pgops, uobj->uo_npages);
	if (UVM_OBJ_IS_KERN_OBJECT(uobj))
		(*pr)("refs=<SYSTEM>\n");
	else
		(*pr)("refs=%d\n", uobj->uo_refs);

	if (!full) {
		return;
	}
	(*pr)("  PAGES <pg,offset>:\n  ");
	RB_FOREACH(pg, uvm_objtree, &uobj->memt) {
		(*pr)("<%p,0x%llx> ", pg, (long long)pg->offset);
		if ((cnt % 3) == 2) {
			(*pr)("\n  ");
		}
		cnt++;
	}
	if ((cnt % 3) != 2) {
		(*pr)("\n");
	}
} 

/*
 * uvm_page_printit: actually print the page
 */
static const char page_flagbits[] =
	"\20\1BUSY\2WANTED\3TABLED\4CLEAN\5CLEANCHK\6RELEASED\7FAKE\10RDONLY"
	"\11ZERO\15PAGER1\20FREE\21INACTIVE\22ACTIVE\24ENCRYPT\30PMAP0"
	"\31PMAP1\32PMAP2\33PMAP3";

void
uvm_page_printit(pg, full, pr)
	struct vm_page *pg;
	boolean_t full;
	int (*pr)(const char *, ...);
{
	struct vm_page *tpg;
	struct uvm_object *uobj;
	struct pglist *pgl;

	(*pr)("PAGE %p:\n", pg);
	(*pr)("  flags=%b, vers=%d, wire_count=%d, pa=0x%llx\n",
	    pg->pg_flags, page_flagbits, pg->pg_version, pg->wire_count,
	    (long long)pg->phys_addr);
	(*pr)("  uobject=%p, uanon=%p, offset=0x%llx loan_count=%d\n",
	    pg->uobject, pg->uanon, (long long)pg->offset, pg->loan_count);
#if defined(UVM_PAGE_TRKOWN)
	if (pg->pg_flags & PG_BUSY)
		(*pr)("  owning process = %d, tag=%s",
		    pg->owner, pg->owner_tag);
	else
		(*pr)("  page not busy, no owner");
#else
	(*pr)("  [page ownership tracking disabled]");
#endif
	(*pr)("\tvm_page_md %p\n", &pg->mdpage);

	if (!full)
		return;

	/* cross-verify object/anon */
	if ((pg->pg_flags & PQ_FREE) == 0) {
		if (pg->pg_flags & PQ_ANON) {
			if (pg->uanon == NULL || pg->uanon->an_page != pg)
			    (*pr)("  >>> ANON DOES NOT POINT HERE <<< (%p)\n",
				(pg->uanon) ? pg->uanon->an_page : NULL);
			else
				(*pr)("  anon backpointer is OK\n");
		} else {
			uobj = pg->uobject;
			if (uobj) {
				(*pr)("  checking object list\n");
				RB_FOREACH(tpg, uvm_objtree, &uobj->memt) {
					if (tpg == pg) {
						break;
					}
				}
				if (tpg)
					(*pr)("  page found on object list\n");
				else
					(*pr)("  >>> PAGE NOT FOUND "
					    "ON OBJECT LIST! <<<\n");
			}
		}
	}

	/* cross-verify page queue */
	if (pg->pg_flags & PQ_FREE) {
		if (uvm_pmr_isfree(pg))
			(*pr)("  page found in uvm_pmemrange\n");
		else
			(*pr)("  >>> page not found in uvm_pmemrange <<<\n");
		pgl = NULL;
	} else if (pg->pg_flags & PQ_INACTIVE) {
		pgl = (pg->pg_flags & PQ_SWAPBACKED) ?
		    &uvm.page_inactive_swp : &uvm.page_inactive_obj;
	} else if (pg->pg_flags & PQ_ACTIVE) {
		pgl = &uvm.page_active;
 	} else {
		pgl = NULL;
	}

	if (pgl) {
		(*pr)("  checking pageq list\n");
		TAILQ_FOREACH(tpg, pgl, pageq) {
			if (tpg == pg) {
				break;
			}
		}
		if (tpg)
			(*pr)("  page found on pageq list\n");
		else
			(*pr)("  >>> PAGE NOT FOUND ON PAGEQ LIST! <<<\n");
	}
}
#endif

/*
 * uvm_map_protect: change map protection
 *
 * => set_max means set max_protection.
 * => map must be unlocked.
 */
int
uvm_map_protect(struct vm_map *map, vaddr_t start, vaddr_t end,
    vm_prot_t new_prot, boolean_t set_max)
{
	struct vm_map_entry *first, *iter;
	vm_prot_t old_prot;
	vm_prot_t mask;
	int error;

	if (start > end)
		return EINVAL;
	start = MAX(start, map->min_offset);
	end = MIN(end, map->max_offset);
	if (start >= end)
		return 0;

	error = 0;
	vm_map_lock(map);

	/*
	 * Set up first and last.
	 * - first will contain first entry at or after start.
	 */
	first = uvm_map_entrybyaddr(&map->addr, start);
	KDASSERT(first != NULL);
	if (first->end < start)
		first = RB_NEXT(uvm_map_addr, &map->addr, first);

	/* First, check for protection violations. */
	for (iter = first; iter != NULL && iter->start < end;
	    iter = RB_NEXT(uvm_map_addr, &map->addr, iter)) {
		/* Treat memory holes as free space. */
		if (iter->start == iter->end || UVM_ET_ISHOLE(iter))
			continue;

		if (UVM_ET_ISSUBMAP(iter)) {
			error = EINVAL;
			goto out;
		}
		if ((new_prot & iter->max_protection) != new_prot) {
			error = EACCES;
			goto out;
		}
		if (map == kernel_map &&
		    (new_prot & (PROT_WRITE | PROT_EXEC)) == (PROT_WRITE | PROT_EXEC))
			panic("uvm_map_protect: kernel map W^X violation requested");
	}

	/* Fix protections.  */
	for (iter = first; iter != NULL && iter->start < end;
	    iter = RB_NEXT(uvm_map_addr, &map->addr, iter)) {
		/* Treat memory holes as free space. */
		if (iter->start == iter->end || UVM_ET_ISHOLE(iter))
			continue;

		old_prot = iter->protection;

		/*
		 * Skip adapting protection iff old and new protection
		 * are equal.
		 */
		if (set_max) {
			if (old_prot == (new_prot & old_prot) &&
			    iter->max_protection == new_prot)
				continue;
		} else {
			if (old_prot == new_prot)
				continue;
		}

		UVM_MAP_CLIP_START(map, iter, start);
		UVM_MAP_CLIP_END(map, iter, end);

		if (set_max) {
			iter->max_protection = new_prot;
			iter->protection &= new_prot;
		} else
			iter->protection = new_prot;

		/*
		 * update physical map if necessary.  worry about copy-on-write
		 * here -- CHECK THIS XXX
		 */
		if (iter->protection != old_prot) {
			mask = UVM_ET_ISCOPYONWRITE(iter) ?
			    ~PROT_WRITE : PROT_MASK;

			/* update pmap */
			if ((iter->protection & mask) == PROT_NONE &&
			    VM_MAPENT_ISWIRED(iter)) {
				/*
				 * TODO(ariane) this is stupid. wired_count
				 * is 0 if not wired, otherwise anything
				 * larger than 0 (incremented once each time
				 * wire is called).
				 * Mostly to be able to undo the damage on
				 * failure. Not the actually be a wired
				 * refcounter...
				 * Originally: iter->wired_count--;
				 * (don't we have to unwire this in the pmap
				 * as well?)
				 */
				iter->wired_count = 0;
			}
			pmap_protect(map->pmap, iter->start, iter->end,
			    iter->protection & mask);
		}

		/*
		 * If the map is configured to lock any future mappings,
		 * wire this entry now if the old protection was PROT_NONE
		 * and the new protection is not PROT_NONE.
		 */
		if ((map->flags & VM_MAP_WIREFUTURE) != 0 &&
		    VM_MAPENT_ISWIRED(iter) == 0 &&
		    old_prot == PROT_NONE &&
		    new_prot != PROT_NONE) {
			if (uvm_map_pageable(map, iter->start, iter->end,
			    FALSE, UVM_LK_ENTER | UVM_LK_EXIT) != 0) {
				/*
				 * If locking the entry fails, remember the
				 * error if it's the first one.  Note we
				 * still continue setting the protection in
				 * the map, but it will return the resource
				 * storage condition regardless.
				 *
				 * XXX Ignore what the actual error is,
				 * XXX just call it a resource shortage
				 * XXX so that it doesn't get confused
				 * XXX what uvm_map_protect() itself would
				 * XXX normally return.
				 */
				error = ENOMEM;
			}
		}
	}
	pmap_update(map->pmap);

out:
	vm_map_unlock(map);
	return error;
}

/*
 * uvmspace_alloc: allocate a vmspace structure.
 *
 * - structure includes vm_map and pmap
 * - XXX: no locking on this structure
 * - refcnt set to 1, rest must be init'd by caller
 */
struct vmspace *
uvmspace_alloc(vaddr_t min, vaddr_t max, boolean_t pageable,
    boolean_t remove_holes)
{
	struct vmspace *vm;

	vm = pool_get(&uvm_vmspace_pool, PR_WAITOK | PR_ZERO);
	uvmspace_init(vm, NULL, min, max, pageable, remove_holes);
	return (vm);
}

/*
 * uvmspace_init: initialize a vmspace structure.
 *
 * - XXX: no locking on this structure
 * - refcnt set to 1, rest must be init'd by caller
 */
void
uvmspace_init(struct vmspace *vm, struct pmap *pmap, vaddr_t min, vaddr_t max,
    boolean_t pageable, boolean_t remove_holes)
{
	KASSERT(pmap == NULL || pmap == pmap_kernel());

	if (pmap)
		pmap_reference(pmap);
	else
		pmap = pmap_create();
	vm->vm_map.pmap = pmap;

	uvm_map_setup(&vm->vm_map, min, max,
	    (pageable ? VM_MAP_PAGEABLE : 0) | VM_MAP_ISVMSPACE);

	vm->vm_refcnt = 1;

	if (remove_holes)
		pmap_remove_holes(vm);
}

/*
 * uvmspace_share: share a vmspace between two processes
 *
 * - XXX: no locking on vmspace
 * - used for vfork
 */

struct vmspace *
uvmspace_share(struct process *pr)
{
	struct vmspace *vm = pr->ps_vmspace;

	vm->vm_refcnt++;
	return vm;
}

/*
 * uvmspace_exec: the process wants to exec a new program
 *
 * - XXX: no locking on vmspace
 */

void
uvmspace_exec(struct proc *p, vaddr_t start, vaddr_t end)
{
	struct process *pr = p->p_p;
	struct vmspace *nvm, *ovm = pr->ps_vmspace;
	struct vm_map *map = &ovm->vm_map;
	struct uvm_map_deadq dead_entries;

	KASSERT((start & (vaddr_t)PAGE_MASK) == 0);
	KASSERT((end & (vaddr_t)PAGE_MASK) == 0 ||
	    (end & (vaddr_t)PAGE_MASK) == (vaddr_t)PAGE_MASK);

	pmap_unuse_final(p);   /* before stack addresses go away */
	TAILQ_INIT(&dead_entries);

	/* see if more than one process is using this vmspace...  */
	if (ovm->vm_refcnt == 1) {
		/*
		 * If pr is the only process using its vmspace then
		 * we can safely recycle that vmspace for the program
		 * that is being exec'd.
		 */

#ifdef SYSVSHM
		/*
		 * SYSV SHM semantics require us to kill all segments on an exec
		 */
		if (ovm->vm_shm)
			shmexit(ovm);
#endif

		/*
		 * POSIX 1003.1b -- "lock future mappings" is revoked
		 * when a process execs another program image.
		 */
		vm_map_lock(map);
		vm_map_modflags(map, 0, VM_MAP_WIREFUTURE);

		/*
		 * now unmap the old program
		 *
		 * Instead of attempting to keep the map valid, we simply
		 * nuke all entries and ask uvm_map_setup to reinitialize
		 * the map to the new boundaries.
		 *
		 * uvm_unmap_remove will actually nuke all entries for us
		 * (as in, not replace them with free-memory entries).
		 */
		uvm_unmap_remove(map, map->min_offset, map->max_offset,
		    &dead_entries, TRUE, FALSE);

		KDASSERT(RB_EMPTY(&map->addr));

		/* Nuke statistics and boundaries. */
		memset(&ovm->vm_startcopy, 0,
		    (caddr_t) (ovm + 1) - (caddr_t) &ovm->vm_startcopy);


		if (end & (vaddr_t)PAGE_MASK) {
			end += 1;
			if (end == 0) /* overflow */
				end -= PAGE_SIZE;
		}

		/* Setup new boundaries and populate map with entries. */
		map->min_offset = start;
		map->max_offset = end;
		uvm_map_setup_entries(map);
		vm_map_unlock(map);

		/* but keep MMU holes unavailable */
		pmap_remove_holes(ovm);
	} else {
		/*
		 * pr's vmspace is being shared, so we can't reuse
		 * it for pr since it is still being used for others.
		 * allocate a new vmspace for pr
		 */
		nvm = uvmspace_alloc(start, end,
		    (map->flags & VM_MAP_PAGEABLE) ? TRUE : FALSE, TRUE);

		/* install new vmspace and drop our ref to the old one. */
		pmap_deactivate(p);
		p->p_vmspace = pr->ps_vmspace = nvm;
		pmap_activate(p);

		uvmspace_free(ovm);
	}

	/* Release dead entries */
	uvm_unmap_detach(&dead_entries, 0);
}

/*
 * uvmspace_free: free a vmspace data structure
 *
 * - XXX: no locking on vmspace
 */
void
uvmspace_free(struct vmspace *vm)
{
	if (--vm->vm_refcnt == 0) {
		/*
		 * lock the map, to wait out all other references to it.  delete
		 * all of the mappings and pages they hold, then call the pmap
		 * module to reclaim anything left.
		 */
#ifdef SYSVSHM
		/* Get rid of any SYSV shared memory segments. */
		if (vm->vm_shm != NULL)
			shmexit(vm);
#endif

		uvm_map_teardown(&vm->vm_map);
		pool_put(&uvm_vmspace_pool, vm);
	}
}

/*
 * Clone map entry into other map.
 *
 * Mapping will be placed at dstaddr, for the same length.
 * Space must be available.
 * Reference counters are incremented.
 */
struct vm_map_entry *
uvm_mapent_clone(struct vm_map *dstmap, vaddr_t dstaddr, vsize_t dstlen,
    vsize_t off, struct vm_map_entry *old_entry, struct uvm_map_deadq *dead,
    int mapent_flags, int amap_share_flags)
{
	struct vm_map_entry *new_entry, *first, *last;

	KDASSERT(!UVM_ET_ISSUBMAP(old_entry));

	/* Create new entry (linked in on creation). Fill in first, last. */
	first = last = NULL;
	if (!uvm_map_isavail(dstmap, NULL, &first, &last, dstaddr, dstlen)) {
		panic("uvmspace_fork: no space in map for "
		    "entry in empty map");
	}
	new_entry = uvm_map_mkentry(dstmap, first, last,
	    dstaddr, dstlen, mapent_flags, dead, NULL);
	if (new_entry == NULL)
		return NULL;
	/* old_entry -> new_entry */
	new_entry->object = old_entry->object;
	new_entry->offset = old_entry->offset;
	new_entry->aref = old_entry->aref;
	new_entry->etype |= old_entry->etype & ~UVM_ET_FREEMAPPED;
	new_entry->protection = old_entry->protection;
	new_entry->max_protection = old_entry->max_protection;
	new_entry->inheritance = old_entry->inheritance;
	new_entry->advice = old_entry->advice;

	/* gain reference to object backing the map (can't be a submap). */
	if (new_entry->aref.ar_amap) {
		new_entry->aref.ar_pageoff += off >> PAGE_SHIFT;
		amap_ref(new_entry->aref.ar_amap, new_entry->aref.ar_pageoff,
		    (new_entry->end - new_entry->start) >> PAGE_SHIFT,
		    amap_share_flags);
	}

	if (UVM_ET_ISOBJ(new_entry) &&
	    new_entry->object.uvm_obj->pgops->pgo_reference) {
		new_entry->offset += off;
		new_entry->object.uvm_obj->pgops->pgo_reference
		    (new_entry->object.uvm_obj);
	}

	return new_entry;
}

/*
 * share the mapping: this means we want the old and
 * new entries to share amaps and backing objects.
 */
struct vm_map_entry *
uvm_mapent_forkshared(struct vmspace *new_vm, struct vm_map *new_map,
    struct vm_map *old_map,
    struct vm_map_entry *old_entry, struct uvm_map_deadq *dead)
{
	struct vm_map_entry *new_entry;

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

	new_entry = uvm_mapent_clone(new_map, old_entry->start,
	    old_entry->end - old_entry->start, 0, old_entry,
	    dead, 0, AMAP_SHARED);

	/* 
	 * pmap_copy the mappings: this routine is optional
	 * but if it is there it will reduce the number of
	 * page faults in the new proc.
	 */
	if (!UVM_ET_ISHOLE(new_entry))
		pmap_copy(new_map->pmap, old_map->pmap, new_entry->start,
		    (new_entry->end - new_entry->start), new_entry->start);

	return (new_entry);
}

/*
 * copy-on-write the mapping (using mmap's
 * MAP_PRIVATE semantics)
 *
 * allocate new_entry, adjust reference counts.  
 * (note that new references are read-only).
 */
struct vm_map_entry *
uvm_mapent_forkcopy(struct vmspace *new_vm, struct vm_map *new_map,
    struct vm_map *old_map,
    struct vm_map_entry *old_entry, struct uvm_map_deadq *dead)
{
	struct vm_map_entry	*new_entry;
	boolean_t		 protect_child;

	new_entry = uvm_mapent_clone(new_map, old_entry->start,
	    old_entry->end - old_entry->start, 0, old_entry,
	    dead, 0, 0);

	new_entry->etype |=
	    (UVM_ET_COPYONWRITE|UVM_ET_NEEDSCOPY);

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
	 *    parent and child process are referring to the
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
	if (old_entry->aref.ar_amap != NULL &&
	    ((amap_flags(old_entry->aref.ar_amap) &
	    AMAP_SHARED) != 0 ||
	    VM_MAPENT_ISWIRED(old_entry))) {
		amap_copy(new_map, new_entry, M_WAITOK, FALSE,
		    0, 0);
		/* XXXCDC: M_WAITOK ... ok? */
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
	if (VM_MAPENT_ISWIRED(old_entry)) {
		/* 
		 * resolve all copy-on-write faults now
		 * (note that there is nothing to do if 
		 * the old mapping does not have an amap).
		 * XXX: is it worthwhile to bother with
		 * pmap_copy in this case?
		 */
		if (old_entry->aref.ar_amap)
			amap_cow_now(new_map, new_entry);
	} else {
		if (old_entry->aref.ar_amap) {
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
			if (!UVM_ET_ISNEEDSCOPY(old_entry)) {
				if (old_entry->max_protection & PROT_WRITE) {
					pmap_protect(old_map->pmap,
					    old_entry->start,
					    old_entry->end,
					    old_entry->protection &
					    ~PROT_WRITE);
					pmap_update(old_map->pmap);
				}
				old_entry->etype |= UVM_ET_NEEDSCOPY;
			}

	  		/* parent must now be write-protected */
	  		protect_child = FALSE;
		} else {
			/*
			 * we only need to protect the child if the 
			 * parent has write access.
			 */
			if (old_entry->max_protection & PROT_WRITE)
				protect_child = TRUE;
			else
				protect_child = FALSE;
		}
		/*
		 * copy the mappings
		 * XXX: need a way to tell if this does anything
		 */
		if (!UVM_ET_ISHOLE(new_entry))
			pmap_copy(new_map->pmap, old_map->pmap,
			    new_entry->start,
			    (old_entry->end - old_entry->start),
			    old_entry->start);

		/* protect the child's mappings if necessary */
		if (protect_child) {
			pmap_protect(new_map->pmap, new_entry->start,
			    new_entry->end,
			    new_entry->protection &
			    ~PROT_WRITE);
		}
	}

	return (new_entry);
}

/*
 * zero the mapping: the new entry will be zero initialized
 */
struct vm_map_entry *
uvm_mapent_forkzero(struct vmspace *new_vm, struct vm_map *new_map,
    struct vm_map *old_map,
    struct vm_map_entry *old_entry, struct uvm_map_deadq *dead)
{
	struct vm_map_entry *new_entry;

	new_entry = uvm_mapent_clone(new_map, old_entry->start,
	    old_entry->end - old_entry->start, 0, old_entry,
	    dead, 0, 0);

	new_entry->etype |=
	    (UVM_ET_COPYONWRITE|UVM_ET_NEEDSCOPY);

	if (new_entry->aref.ar_amap) {
		amap_unref(new_entry->aref.ar_amap, new_entry->aref.ar_pageoff,
		    atop(new_entry->end - new_entry->start), 0);
		new_entry->aref.ar_amap = NULL;
		new_entry->aref.ar_pageoff = 0;
	}

	if (UVM_ET_ISOBJ(new_entry)) {
		if (new_entry->object.uvm_obj->pgops->pgo_detach)
			new_entry->object.uvm_obj->pgops->pgo_detach(
			    new_entry->object.uvm_obj);
		new_entry->object.uvm_obj = NULL;
		new_entry->etype &= ~UVM_ET_OBJ;
	}

	return (new_entry);
}

/*
 * uvmspace_fork: fork a process' main map
 *
 * => create a new vmspace for child process from parent.
 * => parent's map must not be locked.
 */
struct vmspace *
uvmspace_fork(struct process *pr)
{
	struct vmspace *vm1 = pr->ps_vmspace;
	struct vmspace *vm2;
	struct vm_map *old_map = &vm1->vm_map;
	struct vm_map *new_map;
	struct vm_map_entry *old_entry, *new_entry;
	struct uvm_map_deadq dead;

	vm_map_lock(old_map);

	vm2 = uvmspace_alloc(old_map->min_offset, old_map->max_offset,
	    (old_map->flags & VM_MAP_PAGEABLE) ? TRUE : FALSE, FALSE);
	memcpy(&vm2->vm_startcopy, &vm1->vm_startcopy,
	    (caddr_t) (vm1 + 1) - (caddr_t) &vm1->vm_startcopy);
	vm2->vm_dused = 0; /* Statistic managed by us. */
	new_map = &vm2->vm_map;
	vm_map_lock(new_map);

	/* go entry-by-entry */
	TAILQ_INIT(&dead);
	RB_FOREACH(old_entry, uvm_map_addr, &old_map->addr) {
		if (old_entry->start == old_entry->end)
			continue;

		/* first, some sanity checks on the old entry */
		if (UVM_ET_ISSUBMAP(old_entry)) {
			panic("fork: encountered a submap during fork "
			    "(illegal)");
		}

		if (!UVM_ET_ISCOPYONWRITE(old_entry) &&
		    UVM_ET_ISNEEDSCOPY(old_entry)) {
			panic("fork: non-copy_on_write map entry marked "
			    "needs_copy (illegal)");
		}

		/* Apply inheritance. */
		switch (old_entry->inheritance) {
		case MAP_INHERIT_SHARE:
			new_entry = uvm_mapent_forkshared(vm2, new_map,
			    old_map, old_entry, &dead);
			break;
		case MAP_INHERIT_COPY:
			new_entry = uvm_mapent_forkcopy(vm2, new_map,
			    old_map, old_entry, &dead);
			break;
		case MAP_INHERIT_ZERO:
			new_entry = uvm_mapent_forkzero(vm2, new_map,
			    old_map, old_entry, &dead);
			break;
		default:
			continue;
		}

	 	/* Update process statistics. */
		if (!UVM_ET_ISHOLE(new_entry))
			new_map->size += new_entry->end - new_entry->start;
		if (!UVM_ET_ISOBJ(new_entry) && !UVM_ET_ISHOLE(new_entry)) {
			vm2->vm_dused += uvmspace_dused(
			    new_map, new_entry->start, new_entry->end);
		}
	}

	vm_map_unlock(old_map); 
	vm_map_unlock(new_map); 

	/*
	 * This can actually happen, if multiple entries described a
	 * space in which an entry was inherited.
	 */
	uvm_unmap_detach(&dead, 0);

#ifdef SYSVSHM
	if (vm1->vm_shm)
		shmfork(vm1, vm2);
#endif

#ifdef PMAP_FORK
	pmap_fork(vm1->vm_map.pmap, vm2->vm_map.pmap);
#endif

	return vm2;    
}

/*
 * uvm_map_hint: return the beginning of the best area suitable for
 * creating a new mapping with "prot" protection.
 */
vaddr_t
uvm_map_hint(struct vmspace *vm, vm_prot_t prot, vaddr_t minaddr,
    vaddr_t maxaddr)
{
	vaddr_t addr;
	vaddr_t spacing;

#ifdef __i386__
	/*
	 * If executable skip first two pages, otherwise start
	 * after data + heap region.
	 */
	if ((prot & PROT_EXEC) != 0 &&
	    (vaddr_t)vm->vm_daddr >= I386_MAX_EXE_ADDR) {
		addr = (PAGE_SIZE*2) +
		    (arc4random() & (I386_MAX_EXE_ADDR / 2 - 1));
		return (round_page(addr));
	}
#endif

#if defined (__LP64__)
	spacing = (MIN((4UL * 1024 * 1024 * 1024), BRKSIZ) - 1);
#else
	spacing = (MIN((256 * 1024 * 1024), BRKSIZ) - 1);
#endif

	addr = (vaddr_t)vm->vm_daddr;
	/*
	 * Start malloc/mmap after the brk.
	 * If the random spacing area has been used up,
	 * the brk area becomes fair game for mmap as well.
	 */
	if (vm->vm_dused < spacing >> PAGE_SHIFT)
		addr += BRKSIZ;
#if !defined(__vax__)
	if (addr < maxaddr) {
		while (spacing > maxaddr - addr)
			spacing >>= 1;
	}
	addr += arc4random() & spacing;
#endif
	return (round_page(addr));
}

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
uvm_map_submap(struct vm_map *map, vaddr_t start, vaddr_t end,
    struct vm_map *submap)
{
	struct vm_map_entry *entry;
	int result;

	if (start > map->max_offset || end > map->max_offset ||
	    start < map->min_offset || end < map->min_offset)
		return EINVAL;

	vm_map_lock(map);

	if (uvm_map_lookup_entry(map, start, &entry)) {
		UVM_MAP_CLIP_START(map, entry, start);
		UVM_MAP_CLIP_END(map, entry, end);
	} else
		entry = NULL;

	if (entry != NULL && 
	    entry->start == start && entry->end == end &&
	    entry->object.uvm_obj == NULL && entry->aref.ar_amap == NULL &&
	    !UVM_ET_ISCOPYONWRITE(entry) && !UVM_ET_ISNEEDSCOPY(entry)) {
		entry->etype |= UVM_ET_SUBMAP;
		entry->object.sub_map = submap;
		entry->offset = 0;
		uvm_map_reference(submap);
		result = 0;
	} else
		result = EINVAL;

	vm_map_unlock(map);
	return(result);
}

/*
 * uvm_map_checkprot: check protection in map
 *
 * => must allow specific protection in a fully allocated region.
 * => map mut be read or write locked by caller.
 */
boolean_t
uvm_map_checkprot(struct vm_map *map, vaddr_t start, vaddr_t end,
    vm_prot_t protection)
{
	struct vm_map_entry *entry;

	if (start < map->min_offset || end > map->max_offset || start > end)
		return FALSE;
	if (start == end)
		return TRUE;

	/*
	 * Iterate entries.
	 */
	for (entry = uvm_map_entrybyaddr(&map->addr, start);
	    entry != NULL && entry->start < end;
	    entry = RB_NEXT(uvm_map_addr, &map->addr, entry)) {
		/* Fail if a hole is found. */
		if (UVM_ET_ISHOLE(entry) ||
		    (entry->end < end && entry->end != VMMAP_FREE_END(entry)))
			return FALSE;

		/* Check protection. */
		if ((entry->protection & protection) != protection)
			return FALSE;
	}
	return TRUE;
}

/*
 * uvm_map_create: create map
 */
vm_map_t
uvm_map_create(pmap_t pmap, vaddr_t min, vaddr_t max, int flags)
{
	vm_map_t result;

	result = malloc(sizeof(struct vm_map), M_VMMAP, M_WAITOK);
	result->pmap = pmap;
	uvm_map_setup(result, min, max, flags);
	return(result);
}

/*
 * uvm_map_deallocate: drop reference to a map
 *
 * => caller must not lock map
 * => we will zap map if ref count goes to zero
 */
void
uvm_map_deallocate(vm_map_t map)
{
	int c;
	struct uvm_map_deadq dead;

	c = --map->ref_count;
	if (c > 0) {
		return;
	}

	/*
	 * all references gone.   unmap and free.
	 *
	 * No lock required: we are only one to access this map.
	 */
	TAILQ_INIT(&dead);
	uvm_tree_sanity(map, __FILE__, __LINE__);
	uvm_unmap_remove(map, map->min_offset, map->max_offset, &dead,
	    TRUE, FALSE);
	pmap_destroy(map->pmap);
	KASSERT(RB_EMPTY(&map->addr));
	free(map, M_VMMAP, 0);

	uvm_unmap_detach(&dead, 0);
}

/* 
 * uvm_map_inherit: set inheritance code for range of addrs in map.
 *
 * => map must be unlocked
 * => note that the inherit code is used during a "fork".  see fork
 *	code for details.
 */
int
uvm_map_inherit(struct vm_map *map, vaddr_t start, vaddr_t end,
    vm_inherit_t new_inheritance)
{
	struct vm_map_entry *entry;

	switch (new_inheritance) {
	case MAP_INHERIT_NONE:
	case MAP_INHERIT_COPY:
	case MAP_INHERIT_SHARE:
	case MAP_INHERIT_ZERO:
		break;
	default:
		return (EINVAL);
	}

	if (start > end)
		return EINVAL;
	start = MAX(start, map->min_offset);
	end = MIN(end, map->max_offset);
	if (start >= end)
		return 0;

	vm_map_lock(map);

	entry = uvm_map_entrybyaddr(&map->addr, start);
	if (entry->end > start)
		UVM_MAP_CLIP_START(map, entry, start);
	else
		entry = RB_NEXT(uvm_map_addr, &map->addr, entry);

	while (entry != NULL && entry->start < end) {
		UVM_MAP_CLIP_END(map, entry, end);
		entry->inheritance = new_inheritance;
		entry = RB_NEXT(uvm_map_addr, &map->addr, entry);
	}

	vm_map_unlock(map);
	return (0);
}

/* 
 * uvm_map_advice: set advice code for range of addrs in map.
 *
 * => map must be unlocked
 */
int
uvm_map_advice(struct vm_map *map, vaddr_t start, vaddr_t end, int new_advice)
{
	struct vm_map_entry *entry;

	switch (new_advice) {
	case MADV_NORMAL:
	case MADV_RANDOM:
	case MADV_SEQUENTIAL:
		break;
	default:
		return (EINVAL);
	}

	if (start > end)
		return EINVAL;
	start = MAX(start, map->min_offset);
	end = MIN(end, map->max_offset);
	if (start >= end)
		return 0;

	vm_map_lock(map);

	entry = uvm_map_entrybyaddr(&map->addr, start);
	if (entry != NULL && entry->end > start)
		UVM_MAP_CLIP_START(map, entry, start);
	else if (entry!= NULL)
		entry = RB_NEXT(uvm_map_addr, &map->addr, entry);

	/*
	 * XXXJRT: disallow holes?
	 */
	while (entry != NULL && entry->start < end) {
		UVM_MAP_CLIP_END(map, entry, end);
		entry->advice = new_advice;
		entry = RB_NEXT(uvm_map_addr, &map->addr, entry);
	}

	vm_map_unlock(map);
	return (0);
}

/*
 * uvm_map_extract: extract a mapping from a map and put it somewhere
 * in the kernel_map, setting protection to max_prot.
 *
 * => map should be unlocked (we will write lock it and kernel_map)
 * => returns 0 on success, error code otherwise
 * => start must be page aligned
 * => len must be page sized
 * => flags:
 *      UVM_EXTRACT_FIXPROT: set prot to maxprot as we go
 * Mappings are QREF's.
 */
int
uvm_map_extract(struct vm_map *srcmap, vaddr_t start, vsize_t len,
    vaddr_t *dstaddrp, int flags)
{
	struct uvm_map_deadq dead;
	struct vm_map_entry *first, *entry, *newentry, *tmp1, *tmp2;
	vaddr_t dstaddr;
	vaddr_t end;
	vaddr_t cp_start;
	vsize_t cp_len, cp_off;
	int error;

	TAILQ_INIT(&dead);
	end = start + len;

	/*
	 * Sanity check on the parameters.
	 * Also, since the mapping may not contain gaps, error out if the
	 * mapped area is not in source map.
	 */
	if ((start & (vaddr_t)PAGE_MASK) != 0 ||
	    (end & (vaddr_t)PAGE_MASK) != 0 || end < start)
		return EINVAL;
	if (start < srcmap->min_offset || end > srcmap->max_offset)
		return EINVAL;

	/* Initialize dead entries. Handle len == 0 case. */
	if (len == 0)
		return 0;

	/* Acquire lock on srcmap. */
	vm_map_lock(srcmap);

	/* Lock srcmap, lookup first and last entry in <start,len>. */
	first = uvm_map_entrybyaddr(&srcmap->addr, start);

	/* Check that the range is contiguous. */
	for (entry = first; entry != NULL && entry->end < end;
	    entry = RB_NEXT(uvm_map_addr, &map->addr, entry)) {
		if (VMMAP_FREE_END(entry) != entry->end ||
		    UVM_ET_ISHOLE(entry)) {
			error = EINVAL;
			goto fail;
		}
	}
	if (entry == NULL || UVM_ET_ISHOLE(entry)) {
		error = EINVAL;
		goto fail;
	}

	/*
	 * Handle need-copy flag.
	 * This may invalidate last, hence the re-initialization during the
	 * loop.
	 *
	 * Also, perform clipping of last if not UVM_EXTRACT_QREF.
	 */
	for (entry = first; entry != NULL && entry->start < end;
	    entry = RB_NEXT(uvm_map_addr, &map->addr, entry)) {
		if (UVM_ET_ISNEEDSCOPY(entry))
			amap_copy(srcmap, entry, M_NOWAIT, TRUE, start, end);
		if (UVM_ET_ISNEEDSCOPY(entry)) {
			/*
			 * amap_copy failure
			 */
			error = ENOMEM;
			goto fail;
		}
	}

	/* Lock destination map (kernel_map). */
	vm_map_lock(kernel_map);

	if (uvm_map_findspace(kernel_map, &tmp1, &tmp2, &dstaddr, len,
	    MAX(PAGE_SIZE, PMAP_PREFER_ALIGN()), PMAP_PREFER_OFFSET(start),
	    PROT_NONE, 0) != 0) {
		error = ENOMEM;
		goto fail2;
	}
	*dstaddrp = dstaddr;

	/*
	 * We now have srcmap and kernel_map locked.
	 * dstaddr contains the destination offset in dstmap.
	 */
	/* step 1: start looping through map entries, performing extraction. */
	for (entry = first; entry != NULL && entry->start < end;
	    entry = RB_NEXT(uvm_map_addr, &map->addr, entry)) {
		KDASSERT(!UVM_ET_ISNEEDSCOPY(entry));
		if (UVM_ET_ISHOLE(entry))
			continue;

		/* Calculate uvm_mapent_clone parameters. */
		cp_start = entry->start;
		if (cp_start < start) {
			cp_off = start - cp_start;
			cp_start = start;
		} else
			cp_off = 0;
		cp_len = MIN(entry->end, end) - cp_start;

		newentry = uvm_mapent_clone(kernel_map,
		    cp_start - start + dstaddr, cp_len, cp_off,
		    entry, &dead, flags, AMAP_SHARED | AMAP_REFALL);
		if (newentry == NULL) {
			error = ENOMEM;
			goto fail2_unmap;
		}
		kernel_map->size += cp_len;
		if (flags & UVM_EXTRACT_FIXPROT)
			newentry->protection = newentry->max_protection;

		/*
		 * Step 2: perform pmap copy.
		 * (Doing this in the loop saves one RB traversal.)
		 */
		pmap_copy(kernel_map->pmap, srcmap->pmap,
		    cp_start - start + dstaddr, cp_len, cp_start);
	}
	pmap_update(kernel_map->pmap);

	error = 0;

	/* Unmap copied entries on failure. */
fail2_unmap:
	if (error) {
		uvm_unmap_remove(kernel_map, dstaddr, dstaddr + len, &dead,
		    FALSE, TRUE);
	}

	/* Release maps, release dead entries. */
fail2:
	vm_map_unlock(kernel_map);

fail:
	vm_map_unlock(srcmap);

	uvm_unmap_detach(&dead, 0);

	return error;
}

/*
 * uvm_map_clean: clean out a map range
 *
 * => valid flags:
 *   if (flags & PGO_CLEANIT): dirty pages are cleaned first
 *   if (flags & PGO_SYNCIO): dirty pages are written synchronously
 *   if (flags & PGO_DEACTIVATE): any cached pages are deactivated after clean
 *   if (flags & PGO_FREE): any cached pages are freed after clean
 * => returns an error if any part of the specified range isn't mapped
 * => never a need to flush amap layer since the anonymous memory has 
 *	no permanent home, but may deactivate pages there
 * => called from sys_msync() and sys_madvise()
 * => caller must not write-lock map (read OK).
 * => we may sleep while cleaning if SYNCIO [with map read-locked]
 */
int	amap_clean_works = 1;	/* XXX for now, just in case... */

int
uvm_map_clean(struct vm_map *map, vaddr_t start, vaddr_t end, int flags)
{
	struct vm_map_entry *first, *entry;
	struct vm_amap *amap;
	struct vm_anon *anon;
	struct vm_page *pg;
	struct uvm_object *uobj;
	vaddr_t cp_start, cp_end;
	int refs;
	int error;
	boolean_t rv;

	KASSERT((flags & (PGO_FREE|PGO_DEACTIVATE)) !=
	    (PGO_FREE|PGO_DEACTIVATE));

	if (start > end || start < map->min_offset || end > map->max_offset)
		return EINVAL;

	vm_map_lock_read(map);
	first = uvm_map_entrybyaddr(&map->addr, start);

	/* Make a first pass to check for holes. */
	for (entry = first; entry != NULL && entry->start < end;
	    entry = RB_NEXT(uvm_map_addr, &map->addr, entry)) {
		if (UVM_ET_ISSUBMAP(entry)) {
			vm_map_unlock_read(map);
			return EINVAL;
		}
		if (UVM_ET_ISSUBMAP(entry) ||
		    UVM_ET_ISHOLE(entry) ||
		    (entry->end < end &&
		    VMMAP_FREE_END(entry) != entry->end)) {
			vm_map_unlock_read(map);
			return EFAULT;
		}
	}

	error = 0;
	for (entry = first; entry != NULL && entry->start < end;
	    entry = RB_NEXT(uvm_map_addr, &map->addr, entry)) {
		amap = entry->aref.ar_amap;	/* top layer */
		if (UVM_ET_ISOBJ(entry))
			uobj = entry->object.uvm_obj;
		else
			uobj = NULL;

		/*
		 * No amap cleaning necessary if:
		 *  - there's no amap
		 *  - we're not deactivating or freeing pages.
		 */
		if (amap == NULL || (flags & (PGO_DEACTIVATE|PGO_FREE)) == 0)
			goto flush_object;
		if (!amap_clean_works)
			goto flush_object;

		cp_start = MAX(entry->start, start);
		cp_end = MIN(entry->end, end);

		for (; cp_start != cp_end; cp_start += PAGE_SIZE) {
			anon = amap_lookup(&entry->aref,
			    cp_start - entry->start);
			if (anon == NULL)
				continue;

			pg = anon->an_page;
			if (pg == NULL) {
				continue;
			}

			switch (flags & (PGO_CLEANIT|PGO_FREE|PGO_DEACTIVATE)) {
			/*
			 * XXX In these first 3 cases, we always just
			 * XXX deactivate the page.  We may want to
			 * XXX handle the different cases more
			 * XXX specifically, in the future.
			 */
			case PGO_CLEANIT|PGO_FREE:
			case PGO_CLEANIT|PGO_DEACTIVATE:
			case PGO_DEACTIVATE:
deactivate_it:
				/* skip the page if it's loaned or wired */
				if (pg->loan_count != 0 ||
				    pg->wire_count != 0) {
					break;
				}

				uvm_lock_pageq();

				/*
				 * skip the page if it's not actually owned
				 * by the anon (may simply be loaned to the
				 * anon).
				 */
				if ((pg->pg_flags & PQ_ANON) == 0) {
					KASSERT(pg->uobject == NULL);
					uvm_unlock_pageq();
					break;
				}
				KASSERT(pg->uanon == anon);

				/* zap all mappings for the page. */
				pmap_page_protect(pg, PROT_NONE);

				/* ...and deactivate the page. */
				uvm_pagedeactivate(pg);

				uvm_unlock_pageq();
				break;
			case PGO_FREE:
				/*
				 * If there are mutliple references to
				 * the amap, just deactivate the page.
				 */
				if (amap_refs(amap) > 1)
					goto deactivate_it;

				/* XXX skip the page if it's wired */
				if (pg->wire_count != 0) {
					break;
				}
				amap_unadd(&entry->aref,
				    cp_start - entry->start);
				refs = --anon->an_ref;
				if (refs == 0)
					uvm_anfree(anon);
				break;
			default:
				panic("uvm_map_clean: weird flags");
			}
		}

flush_object:
		cp_start = MAX(entry->start, start);
		cp_end = MIN(entry->end, end);

		/*
		 * flush pages if we've got a valid backing object.
		 *
		 * Don't PGO_FREE if we don't have write permission
		 * and don't flush if this is a copy-on-write object
		 * since we can't know our permissions on it.
		 */
		if (uobj != NULL &&
		    ((flags & PGO_FREE) == 0 ||
		     ((entry->max_protection & PROT_WRITE) != 0 &&
		      (entry->etype & UVM_ET_COPYONWRITE) == 0))) {
			rv = uobj->pgops->pgo_flush(uobj,
			    cp_start - entry->start + entry->offset,
			    cp_end - entry->start + entry->offset, flags);

			if (rv == FALSE)
				error = EFAULT;
		}
	}

	vm_map_unlock_read(map);
	return error;
}

/*
 * UVM_MAP_CLIP_END implementation
 */
void
uvm_map_clip_end(struct vm_map *map, struct vm_map_entry *entry, vaddr_t addr)
{
	struct vm_map_entry *tmp;

	KASSERT(entry->start < addr && VMMAP_FREE_END(entry) > addr);
	tmp = uvm_mapent_alloc(map, 0);

	/* Invoke splitentry. */
	uvm_map_splitentry(map, entry, tmp, addr);
}

/*
 * UVM_MAP_CLIP_START implementation
 *
 * Clippers are required to not change the pointers to the entry they are
 * clipping on.
 * Since uvm_map_splitentry turns the original entry into the lowest
 * entry (address wise) we do a swap between the new entry and the original
 * entry, prior to calling uvm_map_splitentry.
 */
void
uvm_map_clip_start(struct vm_map *map, struct vm_map_entry *entry, vaddr_t addr)
{
	struct vm_map_entry *tmp;
	struct uvm_addr_state *free;

	/* Unlink original. */
	free = uvm_map_uaddr_e(map, entry);
	uvm_mapent_free_remove(map, free, entry);
	uvm_mapent_addr_remove(map, entry);

	/* Copy entry. */
	KASSERT(entry->start < addr && VMMAP_FREE_END(entry) > addr);
	tmp = uvm_mapent_alloc(map, 0);
	uvm_mapent_copy(entry, tmp);

	/* Put new entry in place of original entry. */
	uvm_mapent_addr_insert(map, tmp);
	uvm_mapent_free_insert(map, free, tmp);

	/* Invoke splitentry. */
	uvm_map_splitentry(map, tmp, entry, addr);
}

/*
 * Boundary fixer.
 */
static __inline vaddr_t uvm_map_boundfix(vaddr_t, vaddr_t, vaddr_t);
static __inline vaddr_t
uvm_map_boundfix(vaddr_t min, vaddr_t max, vaddr_t bound)
{
	return (min < bound && max > bound) ? bound : max;
}

/*
 * Choose free list based on address at start of free space.
 *
 * The uvm_addr_state returned contains addr and is the first of:
 * - uaddr_exe
 * - uaddr_brk_stack
 * - uaddr_any
 */
struct uvm_addr_state*
uvm_map_uaddr(struct vm_map *map, vaddr_t addr)
{
	struct uvm_addr_state *uaddr;
	int i;

	/* Special case the first page, to prevent mmap from returning 0. */
	if (addr < VMMAP_MIN_ADDR)
		return NULL;

	/* Upper bound for kernel maps at uvm_maxkaddr. */
	if ((map->flags & VM_MAP_ISVMSPACE) == 0) {
		if (addr >= uvm_maxkaddr)
			return NULL;
	}

	/* Is the address inside the exe-only map? */
	if (map->uaddr_exe != NULL && addr >= map->uaddr_exe->uaddr_minaddr &&
	    addr < map->uaddr_exe->uaddr_maxaddr)
		return map->uaddr_exe;

	/* Check if the space falls inside brk/stack area. */
	if ((addr >= map->b_start && addr < map->b_end) ||
	    (addr >= map->s_start && addr < map->s_end)) {
		if (map->uaddr_brk_stack != NULL &&
		    addr >= map->uaddr_brk_stack->uaddr_minaddr &&
		    addr < map->uaddr_brk_stack->uaddr_maxaddr) {
			return map->uaddr_brk_stack;
		} else
			return NULL;
	}

	/*
	 * Check the other selectors.
	 *
	 * These selectors are only marked as the owner, if they have insert
	 * functions.
	 */
	for (i = 0; i < nitems(map->uaddr_any); i++) {
		uaddr = map->uaddr_any[i];
		if (uaddr == NULL)
			continue;
		if (uaddr->uaddr_functions->uaddr_free_insert == NULL)
			continue;

		if (addr >= uaddr->uaddr_minaddr &&
		    addr < uaddr->uaddr_maxaddr)
			return uaddr;
	}

	return NULL;
}

/*
 * Choose free list based on address at start of free space.
 *
 * The uvm_addr_state returned contains addr and is the first of:
 * - uaddr_exe
 * - uaddr_brk_stack
 * - uaddr_any
 */
struct uvm_addr_state*
uvm_map_uaddr_e(struct vm_map *map, struct vm_map_entry *entry)
{
	return uvm_map_uaddr(map, VMMAP_FREE_START(entry));
}

/*
 * Returns the first free-memory boundary that is crossed by [min-max].
 */
vsize_t
uvm_map_boundary(struct vm_map *map, vaddr_t min, vaddr_t max)
{
	struct uvm_addr_state	*uaddr;
	int			 i;

	/* Never return first page. */
	max = uvm_map_boundfix(min, max, VMMAP_MIN_ADDR);

	/* Treat the maxkaddr special, if the map is a kernel_map. */
	if ((map->flags & VM_MAP_ISVMSPACE) == 0)
		max = uvm_map_boundfix(min, max, uvm_maxkaddr);

	/* Check for exe-only boundaries. */
	if (map->uaddr_exe != NULL) {
		max = uvm_map_boundfix(min, max, map->uaddr_exe->uaddr_minaddr);
		max = uvm_map_boundfix(min, max, map->uaddr_exe->uaddr_maxaddr);
	}

	/* Check for exe-only boundaries. */
	if (map->uaddr_brk_stack != NULL) {
		max = uvm_map_boundfix(min, max,
		    map->uaddr_brk_stack->uaddr_minaddr);
		max = uvm_map_boundfix(min, max,
		    map->uaddr_brk_stack->uaddr_maxaddr);
	}

	/* Check other boundaries. */
	for (i = 0; i < nitems(map->uaddr_any); i++) {
		uaddr = map->uaddr_any[i];
		if (uaddr != NULL) {
			max = uvm_map_boundfix(min, max, uaddr->uaddr_minaddr);
			max = uvm_map_boundfix(min, max, uaddr->uaddr_maxaddr);
		}
	}

	/* Boundaries at stack and brk() area. */
	max = uvm_map_boundfix(min, max, map->s_start);
	max = uvm_map_boundfix(min, max, map->s_end);
	max = uvm_map_boundfix(min, max, map->b_start);
	max = uvm_map_boundfix(min, max, map->b_end);

	return max;
}

/*
 * Update map allocation start and end addresses from proc vmspace.
 */
void
uvm_map_vmspace_update(struct vm_map *map,
    struct uvm_map_deadq *dead, int flags)
{
	struct vmspace *vm;
	vaddr_t b_start, b_end, s_start, s_end;

	KASSERT(map->flags & VM_MAP_ISVMSPACE);
	KASSERT(offsetof(struct vmspace, vm_map) == 0);

	/*
	 * Derive actual allocation boundaries from vmspace.
	 */
	vm = (struct vmspace *)map;
	b_start = (vaddr_t)vm->vm_daddr;
	b_end   = b_start + BRKSIZ;
	s_start = MIN((vaddr_t)vm->vm_maxsaddr, (vaddr_t)vm->vm_minsaddr);
	s_end   = MAX((vaddr_t)vm->vm_maxsaddr, (vaddr_t)vm->vm_minsaddr);
#ifdef DIAGNOSTIC
	if ((b_start & (vaddr_t)PAGE_MASK) != 0 ||
	    (b_end & (vaddr_t)PAGE_MASK) != 0 ||
	    (s_start & (vaddr_t)PAGE_MASK) != 0 ||
	    (s_end & (vaddr_t)PAGE_MASK) != 0) {
		panic("uvm_map_vmspace_update: vmspace %p invalid bounds: "
		    "b=0x%lx-0x%lx s=0x%lx-0x%lx",
		    vm, b_start, b_end, s_start, s_end);
	}
#endif

	if (__predict_true(map->b_start == b_start && map->b_end == b_end &&
	    map->s_start == s_start && map->s_end == s_end))
		return;

	uvm_map_freelist_update(map, dead, b_start, b_end,
	    s_start, s_end, flags);
}

/*
 * Grow kernel memory.
 *
 * This function is only called for kernel maps when an allocation fails.
 *
 * If the map has a gap that is large enough to accomodate alloc_sz, this
 * function will make sure map->free will include it.
 */
void
uvm_map_kmem_grow(struct vm_map *map, struct uvm_map_deadq *dead,
    vsize_t alloc_sz, int flags)
{
	vsize_t sz;
	vaddr_t end;
	struct vm_map_entry *entry;

	/* Kernel memory only. */
	KASSERT((map->flags & VM_MAP_ISVMSPACE) == 0);
	/* Destroy free list. */
	uvm_map_freelist_update_clear(map, dead);

	/* Include the guard page in the hard minimum requirement of alloc_sz. */
	if (map->flags & VM_MAP_GUARDPAGES)
		alloc_sz += PAGE_SIZE;

	/*
	 * Grow by ALLOCMUL * alloc_sz, but at least VM_MAP_KSIZE_DELTA.
	 *
	 * Don't handle the case where the multiplication overflows:
	 * if that happens, the allocation is probably too big anyway.
	 */
	sz = MAX(VM_MAP_KSIZE_ALLOCMUL * alloc_sz, VM_MAP_KSIZE_DELTA);

	/*
	 * Walk forward until a gap large enough for alloc_sz shows up.
	 *
	 * We assume the kernel map has no boundaries.
	 * uvm_maxkaddr may be zero.
	 */
	end = MAX(uvm_maxkaddr, map->min_offset);
	entry = uvm_map_entrybyaddr(&map->addr, end);
	while (entry && entry->fspace < alloc_sz)
		entry = RB_NEXT(uvm_map_addr, &map->addr, entry);
	if (entry) {
		end = MAX(VMMAP_FREE_START(entry), end);
		end += MIN(sz, map->max_offset - end);
	} else
		end = map->max_offset;

	/* Reserve pmap entries. */
#ifdef PMAP_GROWKERNEL
	uvm_maxkaddr = pmap_growkernel(end);
#else
	uvm_maxkaddr = end;
#endif

	/* Rebuild free list. */
	uvm_map_freelist_update_refill(map, flags);
}

/*
 * Freelist update subfunction: unlink all entries from freelists.
 */
void
uvm_map_freelist_update_clear(struct vm_map *map, struct uvm_map_deadq *dead)
{
	struct uvm_addr_state *free;
	struct vm_map_entry *entry, *prev, *next;

	prev = NULL;
	for (entry = RB_MIN(uvm_map_addr, &map->addr); entry != NULL;
	    entry = next) {
		next = RB_NEXT(uvm_map_addr, &map->addr, entry);

		free = uvm_map_uaddr_e(map, entry);
		uvm_mapent_free_remove(map, free, entry);

		if (prev != NULL && entry->start == entry->end) {
			prev->fspace += VMMAP_FREE_END(entry) - entry->end;
			uvm_mapent_addr_remove(map, entry);
			DEAD_ENTRY_PUSH(dead, entry);
		} else
			prev = entry;
	}
}

/*
 * Freelist update subfunction: refill the freelists with entries.
 */
void
uvm_map_freelist_update_refill(struct vm_map *map, int flags)
{
	struct vm_map_entry *entry;
	vaddr_t min, max;

	RB_FOREACH(entry, uvm_map_addr, &map->addr) {
		min = VMMAP_FREE_START(entry);
		max = VMMAP_FREE_END(entry);
		entry->fspace = 0;

		entry = uvm_map_fix_space(map, entry, min, max, flags);
	}

	uvm_tree_sanity(map, __FILE__, __LINE__);
}

/*
 * Change {a,b}_{start,end} allocation ranges and associated free lists.
 */
void
uvm_map_freelist_update(struct vm_map *map, struct uvm_map_deadq *dead,
    vaddr_t b_start, vaddr_t b_end, vaddr_t s_start, vaddr_t s_end, int flags)
{
	KDASSERT(b_end >= b_start && s_end >= s_start);

	/* Clear all free lists. */
	uvm_map_freelist_update_clear(map, dead);

	/* Apply new bounds. */
	map->b_start = b_start;
	map->b_end   = b_end;
	map->s_start = s_start;
	map->s_end   = s_end;

	/* Refill free lists. */
	uvm_map_freelist_update_refill(map, flags);
}

/*
 * Assign a uvm_addr_state to the specified pointer in vm_map.
 *
 * May sleep.
 */
void
uvm_map_set_uaddr(struct vm_map *map, struct uvm_addr_state **which,
    struct uvm_addr_state *newval)
{
	struct uvm_map_deadq dead;

	/* Pointer which must be in this map. */
	KASSERT(which != NULL);
	KASSERT((void*)map <= (void*)(which) &&
	    (void*)(which) < (void*)(map + 1));

	vm_map_lock(map);
	TAILQ_INIT(&dead);
	uvm_map_freelist_update_clear(map, &dead);

	uvm_addr_destroy(*which);
	*which = newval;

	uvm_map_freelist_update_refill(map, 0);
	vm_map_unlock(map);
	uvm_unmap_detach(&dead, 0);
}

/*
 * Correct space insert.
 *
 * Entry must not be on any freelist.
 */
struct vm_map_entry*
uvm_map_fix_space(struct vm_map *map, struct vm_map_entry *entry,
    vaddr_t min, vaddr_t max, int flags)
{
	struct uvm_addr_state	*free, *entfree;
	vaddr_t			 lmax;

	KASSERT(entry == NULL || (entry->etype & UVM_ET_FREEMAPPED) == 0);
	KDASSERT(min <= max);
	KDASSERT((entry != NULL && VMMAP_FREE_END(entry) == min) ||
	    min == map->min_offset);

	/*
	 * During the function, entfree will always point at the uaddr state
	 * for entry.
	 */
	entfree = (entry == NULL ? NULL :
	    uvm_map_uaddr_e(map, entry));

	while (min != max) {
		/* Claim guard page for entry. */
		if ((map->flags & VM_MAP_GUARDPAGES) && entry != NULL &&
		    VMMAP_FREE_END(entry) == entry->end &&
		    entry->start != entry->end) {
			if (max - min == 2 * PAGE_SIZE) {
				/*
				 * If the free-space gap is exactly 2 pages,
				 * we make the guard 2 pages instead of 1.
				 * Because in a guarded map, an area needs
				 * at least 2 pages to allocate from:
				 * one page for the allocation and one for
				 * the guard.
				 */
				entry->guard = 2 * PAGE_SIZE;
				min = max;
			} else {
				entry->guard = PAGE_SIZE;
				min += PAGE_SIZE;
			}
			continue;
		}

		/*
		 * Handle the case where entry has a 2-page guard, but the
		 * space after entry is freed.
		 */
		if (entry != NULL && entry->fspace == 0 &&
		    entry->guard > PAGE_SIZE) {
			entry->guard = PAGE_SIZE;
			min = VMMAP_FREE_START(entry);
		}

		lmax = uvm_map_boundary(map, min, max);
		free = uvm_map_uaddr(map, min);

		/*
		 * Entries are merged if they point at the same uvm_free().
		 * Exception to that rule: if min == uvm_maxkaddr, a new
		 * entry is started regardless (otherwise the allocators
		 * will get confused).
		 */
		if (entry != NULL && free == entfree &&
		    !((map->flags & VM_MAP_ISVMSPACE) == 0 &&
		    min == uvm_maxkaddr)) {
			KDASSERT(VMMAP_FREE_END(entry) == min);
			entry->fspace += lmax - min;
		} else {
			/*
			 * Commit entry to free list: it'll not be added to
			 * anymore.
			 * We'll start a new entry and add to that entry
			 * instead.
			 */
			if (entry != NULL)
				uvm_mapent_free_insert(map, entfree, entry);

			/* New entry for new uaddr. */
			entry = uvm_mapent_alloc(map, flags);
			KDASSERT(entry != NULL);
			entry->end = entry->start = min;
			entry->guard = 0;
			entry->fspace = lmax - min;
			entry->object.uvm_obj = NULL;
			entry->offset = 0;
			entry->etype = 0;
			entry->protection = entry->max_protection = 0;
			entry->inheritance = 0;
			entry->wired_count = 0;
			entry->advice = 0;
			entry->aref.ar_pageoff = 0;
			entry->aref.ar_amap = NULL;
			uvm_mapent_addr_insert(map, entry);

			entfree = free;
		}

		min = lmax;
	}
	/* Finally put entry on the uaddr state. */
	if (entry != NULL)
		uvm_mapent_free_insert(map, entfree, entry);

	return entry;
}

/*
 * MQuery style of allocation.
 *
 * This allocator searches forward until sufficient space is found to map
 * the given size.
 *
 * XXX: factor in offset (via pmap_prefer) and protection?
 */
int
uvm_map_mquery(struct vm_map *map, vaddr_t *addr_p, vsize_t sz, voff_t offset,
    int flags)
{
	struct vm_map_entry *entry, *last;
	vaddr_t addr;
	vaddr_t tmp, pmap_align, pmap_offset;
	int error;

	addr = *addr_p;
	vm_map_lock_read(map);

	/* Configure pmap prefer. */
	if (offset != UVM_UNKNOWN_OFFSET) {
		pmap_align = MAX(PAGE_SIZE, PMAP_PREFER_ALIGN());
		pmap_offset = PMAP_PREFER_OFFSET(offset);
	} else {
		pmap_align = PAGE_SIZE;
		pmap_offset = 0;
	}

	/* Align address to pmap_prefer unless FLAG_FIXED is set. */
	if (!(flags & UVM_FLAG_FIXED) && offset != UVM_UNKNOWN_OFFSET) {
	  	tmp = (addr & ~(pmap_align - 1)) | pmap_offset;
		if (tmp < addr)
			tmp += pmap_align;
		addr = tmp;
	}

	/* First, check if the requested range is fully available. */
	entry = uvm_map_entrybyaddr(&map->addr, addr);
	last = NULL;
	if (uvm_map_isavail(map, NULL, &entry, &last, addr, sz)) {
		error = 0;
		goto out;
	}
	if (flags & UVM_FLAG_FIXED) {
		error = EINVAL;
		goto out;
	}

	error = ENOMEM; /* Default error from here. */

	/*
	 * At this point, the memory at <addr, sz> is not available.
	 * The reasons are:
	 * [1] it's outside the map,
	 * [2] it starts in used memory (and therefore needs to move
	 *     toward the first free page in entry),
	 * [3] it starts in free memory but bumps into used memory.
	 *
	 * Note that for case [2], the forward moving is handled by the
	 * for loop below.
	 */
	if (entry == NULL) {
		/* [1] Outside the map. */
		if (addr >= map->max_offset)
			goto out;
		else
			entry = RB_MIN(uvm_map_addr, &map->addr);
	} else if (VMMAP_FREE_START(entry) <= addr) {
		/* [3] Bumped into used memory. */
		entry = RB_NEXT(uvm_map_addr, &map->addr, entry);
	}

	/* Test if the next entry is sufficient for the allocation. */
	for (; entry != NULL;
	    entry = RB_NEXT(uvm_map_addr, &map->addr, entry)) {
		if (entry->fspace == 0)
			continue;
		addr = VMMAP_FREE_START(entry);

restart:	/* Restart address checks on address change. */
		tmp = (addr & ~(pmap_align - 1)) | pmap_offset;
		if (tmp < addr)
			tmp += pmap_align;
		addr = tmp;
		if (addr >= VMMAP_FREE_END(entry))
			continue;

		/* Skip brk() allocation addresses. */
		if (addr + sz > map->b_start && addr < map->b_end) {
			if (VMMAP_FREE_END(entry) > map->b_end) {
				addr = map->b_end;
				goto restart;
			} else
				continue;
		}
		/* Skip stack allocation addresses. */
		if (addr + sz > map->s_start && addr < map->s_end) {
			if (VMMAP_FREE_END(entry) > map->s_end) {
				addr = map->s_end;
				goto restart;
			} else
				continue;
		}

		last = NULL;
		if (uvm_map_isavail(map, NULL, &entry, &last, addr, sz)) {
			error = 0;
			goto out;
		}
	}

out:
	vm_map_unlock_read(map);
	if (error == 0)
		*addr_p = addr;
	return error;
}

/*
 * Determine allocation bias.
 *
 * Returns 1 if we should bias to high addresses, -1 for a bias towards low
 * addresses, or 0 for no bias.
 * The bias mechanism is intended to avoid clashing with brk() and stack
 * areas.
 */
int
uvm_mapent_bias(struct vm_map *map, struct vm_map_entry *entry)
{
	vaddr_t start, end;

	start = VMMAP_FREE_START(entry);
	end = VMMAP_FREE_END(entry);

	/* Stay at the top of brk() area. */
	if (end >= map->b_start && start < map->b_end)
		return 1;
	/* Stay at the far end of the stack area. */
	if (end >= map->s_start && start < map->s_end) {
#ifdef MACHINE_STACK_GROWS_UP
		return 1;
#else
		return -1;
#endif
	}

	/* No bias, this area is meant for us. */
	return 0;
}


boolean_t
vm_map_lock_try_ln(struct vm_map *map, char *file, int line)
{
	boolean_t rv;

	if (map->flags & VM_MAP_INTRSAFE) {
		rv = TRUE;
	} else {
		mtx_enter(&map->flags_lock);
		if (map->flags & VM_MAP_BUSY) {
			mtx_leave(&map->flags_lock);
			return (FALSE);
		}
		mtx_leave(&map->flags_lock);
		rv = (rw_enter(&map->lock, RW_WRITE|RW_NOSLEEP) == 0);
		/* check if the lock is busy and back out if we won the race */
		if (rv) {
			mtx_enter(&map->flags_lock);
			if (map->flags & VM_MAP_BUSY) {
				rw_exit(&map->lock);
				rv = FALSE;
			}
			mtx_leave(&map->flags_lock);
		}
	}

	if (rv) {
		map->timestamp++;
		LPRINTF(("map   lock: %p (at %s %d)\n", map, file, line));
		uvm_tree_sanity(map, file, line);
		uvm_tree_size_chk(map, file, line);
	}

	return (rv);
}

void
vm_map_lock_ln(struct vm_map *map, char *file, int line)
{
	if ((map->flags & VM_MAP_INTRSAFE) == 0) {
		do {
			mtx_enter(&map->flags_lock);
tryagain:
			while (map->flags & VM_MAP_BUSY) {
				map->flags |= VM_MAP_WANTLOCK;
				msleep(&map->flags, &map->flags_lock,
				    PVM, vmmapbsy, 0);
			}
			mtx_leave(&map->flags_lock);
		} while (rw_enter(&map->lock, RW_WRITE|RW_SLEEPFAIL) != 0);
		/* check if the lock is busy and back out if we won the race */
		mtx_enter(&map->flags_lock);
		if (map->flags & VM_MAP_BUSY) {
			rw_exit(&map->lock);
			goto tryagain;
		}
		mtx_leave(&map->flags_lock);
	}

	map->timestamp++;
	LPRINTF(("map   lock: %p (at %s %d)\n", map, file, line));
	uvm_tree_sanity(map, file, line);
	uvm_tree_size_chk(map, file, line);
}

void
vm_map_lock_read_ln(struct vm_map *map, char *file, int line)
{
	if ((map->flags & VM_MAP_INTRSAFE) == 0)
		rw_enter_read(&map->lock);
	LPRINTF(("map   lock: %p (at %s %d)\n", map, file, line));
	uvm_tree_sanity(map, file, line);
	uvm_tree_size_chk(map, file, line);
}

void
vm_map_unlock_ln(struct vm_map *map, char *file, int line)
{
	uvm_tree_sanity(map, file, line);
	uvm_tree_size_chk(map, file, line);
	LPRINTF(("map unlock: %p (at %s %d)\n", map, file, line));
	if ((map->flags & VM_MAP_INTRSAFE) == 0)
		rw_exit(&map->lock);
}

void
vm_map_unlock_read_ln(struct vm_map *map, char *file, int line)
{
	/* XXX: RO */ uvm_tree_sanity(map, file, line);
	/* XXX: RO */ uvm_tree_size_chk(map, file, line);
	LPRINTF(("map unlock: %p (at %s %d)\n", map, file, line));
	if ((map->flags & VM_MAP_INTRSAFE) == 0)
		rw_exit_read(&map->lock);
}

void
vm_map_downgrade_ln(struct vm_map *map, char *file, int line)
{
	uvm_tree_sanity(map, file, line);
	uvm_tree_size_chk(map, file, line);
	LPRINTF(("map unlock: %p (at %s %d)\n", map, file, line));
	LPRINTF(("map   lock: %p (at %s %d)\n", map, file, line));
	if ((map->flags & VM_MAP_INTRSAFE) == 0)
		rw_enter(&map->lock, RW_DOWNGRADE);
}

void
vm_map_upgrade_ln(struct vm_map *map, char *file, int line)
{
	/* XXX: RO */ uvm_tree_sanity(map, file, line);
	/* XXX: RO */ uvm_tree_size_chk(map, file, line);
	LPRINTF(("map unlock: %p (at %s %d)\n", map, file, line));
	if ((map->flags & VM_MAP_INTRSAFE) == 0) {
		rw_exit_read(&map->lock);
		rw_enter_write(&map->lock);
	}
	LPRINTF(("map   lock: %p (at %s %d)\n", map, file, line));
	uvm_tree_sanity(map, file, line);
}

void
vm_map_busy_ln(struct vm_map *map, char *file, int line)
{
	mtx_enter(&map->flags_lock);
	map->flags |= VM_MAP_BUSY;
	mtx_leave(&map->flags_lock);
}

void
vm_map_unbusy_ln(struct vm_map *map, char *file, int line)
{
	int oflags;

	mtx_enter(&map->flags_lock);
	oflags = map->flags;
	map->flags &= ~(VM_MAP_BUSY|VM_MAP_WANTLOCK);
	mtx_leave(&map->flags_lock);
	if (oflags & VM_MAP_WANTLOCK)
		wakeup(&map->flags);
}

#ifndef SMALL_KERNEL
int
uvm_map_fill_vmmap(struct vm_map *map, struct kinfo_vmentry *kve,
    size_t *lenp)
{
	struct vm_map_entry *entry;
	vaddr_t start;
	int cnt, maxcnt, error = 0;

	KASSERT(*lenp > 0);
	KASSERT((*lenp % sizeof(*kve)) == 0);
	cnt = 0;
	maxcnt = *lenp / sizeof(*kve);
	KASSERT(maxcnt > 0);

	/*
	 * Return only entries whose address is above the given base
	 * address.  This allows userland to iterate without knowing the
	 * number of entries beforehand.
	 */
	start = (vaddr_t)kve[0].kve_start;

	vm_map_lock(map);
	RB_FOREACH(entry, uvm_map_addr, &map->addr) {
		if (cnt == maxcnt) {
			error = ENOMEM;
			break;
		}
		if (start != 0 && entry->start < start)
			continue;
		kve->kve_start = entry->start;
		kve->kve_end = entry->end;
		kve->kve_guard = entry->guard;
		kve->kve_fspace = entry->fspace;
		kve->kve_fspace_augment = entry->fspace_augment;
		kve->kve_offset = entry->offset;
		kve->kve_wired_count = entry->wired_count;
		kve->kve_etype = entry->etype;
		kve->kve_protection = entry->protection;
		kve->kve_max_protection = entry->max_protection;
		kve->kve_advice = entry->advice;
		kve->kve_inheritance = entry->inheritance;
		kve->kve_flags = entry->flags;
		kve++;
		cnt++;
	}
	vm_map_unlock(map);

	KASSERT(cnt <= maxcnt);

	*lenp = sizeof(*kve) * cnt;
	return error;
}
#endif


#undef RB_AUGMENT
#define RB_AUGMENT(x)	uvm_map_addr_augment((x))
RB_GENERATE(uvm_map_addr, vm_map_entry, daddrs.addr_entry,
    uvm_mapentry_addrcmp);
#undef RB_AUGMENT


/*
 * MD code: vmspace allocator setup.
 */

#ifdef __i386__
void
uvm_map_setup_md(struct vm_map *map)
{
	vaddr_t		min, max;

	min = map->min_offset;
	max = map->max_offset;

	/*
	 * Ensure the selectors will not try to manage page 0;
	 * it's too special.
	 */
	if (min < VMMAP_MIN_ADDR)
		min = VMMAP_MIN_ADDR;

#if 0	/* Cool stuff, not yet */
	/* Hinted allocations. */
	map->uaddr_any[1] = uaddr_hint_create(MAX(min, VMMAP_MIN_ADDR), max,
	    1024 * 1024 * 1024);

	/* Executable code is special. */
	map->uaddr_exe = uaddr_rnd_create(min, I386_MAX_EXE_ADDR);
	/* Place normal allocations beyond executable mappings. */
	map->uaddr_any[3] = uaddr_pivot_create(2 * I386_MAX_EXE_ADDR, max);
#else	/* Crappy stuff, for now */
	map->uaddr_any[0] = uaddr_rnd_create(min, max);
#endif

#ifndef SMALL_KERNEL
	map->uaddr_brk_stack = uaddr_stack_brk_create(min, max);
#endif /* !SMALL_KERNEL */
}
#elif __LP64__
void
uvm_map_setup_md(struct vm_map *map)
{
	vaddr_t		min, max;

	min = map->min_offset;
	max = map->max_offset;

	/*
	 * Ensure the selectors will not try to manage page 0;
	 * it's too special.
	 */
	if (min < VMMAP_MIN_ADDR)
		min = VMMAP_MIN_ADDR;

#if 0	/* Cool stuff, not yet */
	/* Hinted allocations above 4GB */
	map->uaddr_any[0] =
	    uaddr_hint_create(0x100000000ULL, max, 1024 * 1024 * 1024);
	/* Hinted allocations below 4GB */
	map->uaddr_any[1] =
	    uaddr_hint_create(MAX(min, VMMAP_MIN_ADDR), 0x100000000ULL,
	    1024 * 1024 * 1024);
	/* Normal allocations, always above 4GB */
	map->uaddr_any[3] =
	    uaddr_pivot_create(MAX(min, 0x100000000ULL), max);
#else	/* Crappy stuff, for now */
	map->uaddr_any[0] = uaddr_rnd_create(min, max);
#endif

#ifndef SMALL_KERNEL
	map->uaddr_brk_stack = uaddr_stack_brk_create(min, max);
#endif /* !SMALL_KERNEL */
}
#else	/* non-i386, 32 bit */
void
uvm_map_setup_md(struct vm_map *map)
{
	vaddr_t		min, max;

	min = map->min_offset;
	max = map->max_offset;

	/*
	 * Ensure the selectors will not try to manage page 0;
	 * it's too special.
	 */
	if (min < VMMAP_MIN_ADDR)
		min = VMMAP_MIN_ADDR;

#if 0	/* Cool stuff, not yet */
	/* Hinted allocations. */
	map->uaddr_any[1] = uaddr_hint_create(MAX(min, VMMAP_MIN_ADDR), max,
	    1024 * 1024 * 1024);
	/* Normal allocations. */
	map->uaddr_any[3] = uaddr_pivot_create(min, max);
#else	/* Crappy stuff, for now */
	map->uaddr_any[0] = uaddr_rnd_create(min, max);
#endif

#ifndef SMALL_KERNEL
	map->uaddr_brk_stack = uaddr_stack_brk_create(min, max);
#endif /* !SMALL_KERNEL */
}
#endif
