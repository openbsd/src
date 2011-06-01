/*	$OpenBSD: uvm_map.c,v 1.139 2011/06/01 22:29:25 ariane Exp $	*/
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

/* #define DEBUG */
#define VMMAP_MIN_ADDR	PAGE_SIZE	/* auto-allocate address lower bound */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mman.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/pool.h>
#include <sys/kernel.h>

#include <dev/rndvar.h>

#ifdef SYSVSHM
#include <sys/shm.h>
#endif

#include <uvm/uvm.h>

#ifdef DDB
#include <uvm/uvm_ddb.h>
#endif


vsize_t			 uvmspace_dused(struct vm_map*, vaddr_t, vaddr_t);
struct vm_map_entry	*uvm_map_entrybyaddr(struct uvm_map_addr*, vaddr_t);
struct vm_map_entry	*uvm_map_findspace_entry(struct uvm_map_free*, vsize_t);
struct vm_map_entry	*uvm_map_findspace_tree(struct uvm_map_free*, vsize_t,
			    voff_t, vsize_t, int, vaddr_t*, struct vm_map*);
int			 uvm_map_isavail(struct uvm_map_addr*,
			    struct vm_map_entry**, struct vm_map_entry**,
			    vaddr_t, vsize_t);
int			 uvm_mapent_isjoinable(struct vm_map*,
			    struct vm_map_entry*, struct vm_map_entry*);
struct vm_map_entry	*uvm_mapent_merge(struct vm_map*, struct vm_map_entry*,
			    struct vm_map_entry*, struct uvm_map_deadq*);
struct vm_map_entry	*uvm_mapent_tryjoin(struct vm_map*,
			    struct vm_map_entry*, struct uvm_map_deadq*);
struct vm_map_entry	*uvm_map_mkentry(struct vm_map*, struct vm_map_entry*,
			    struct vm_map_entry*, vaddr_t, vsize_t, int,
			    struct uvm_map_deadq*);
struct vm_map_entry	*uvm_mapent_alloc(struct vm_map*, int);
void			 uvm_mapent_free(struct vm_map_entry*);
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
			    struct uvm_map_free*, struct vm_map_entry*);
void			 uvm_mapent_free_remove(struct vm_map*,
			    struct uvm_map_free*, struct vm_map_entry*);
void			 uvm_mapent_addr_insert(struct vm_map*,
			    struct vm_map_entry*);
void			 uvm_mapent_addr_remove(struct vm_map*,
			    struct vm_map_entry*);
void			 uvm_map_splitentry(struct vm_map*,
			    struct vm_map_entry*, struct vm_map_entry*,
			    vaddr_t);
vsize_t			 uvm_map_boundary(struct vm_map*, vaddr_t, vaddr_t);
struct uvm_map_free	*uvm_free(struct vm_map*, vaddr_t);
int			 uvm_mapent_bias(struct vm_map*, struct vm_map_entry*);

/* Find freelist for containing addr. */
#define UVM_FREE(_map, _addr)		uvm_free((_map), (_addr))
/* Size of the free tree. */
#define uvm_mapfree_size(_free)		((_free)->treesz)

/*
 * uvm_vmspace_fork helper functions.
 */
struct vm_map_entry	*uvm_mapent_clone(struct vm_map*, vaddr_t, vsize_t,
			    vsize_t, struct vm_map_entry*,
			    struct uvm_map_deadq*, int, int);
void			 uvm_mapent_forkshared(struct vmspace*, struct vm_map*,
			    struct vm_map*, struct vm_map_entry*,
			    struct uvm_map_deadq*);
void			 uvm_mapent_forkcopy(struct vmspace*, struct vm_map*,
			    struct vm_map*, struct vm_map_entry*,
			    struct uvm_map_deadq*);

/*
 * Tree validation.
 */

#ifdef DEBUG
void			 uvm_tree_assert(struct vm_map*, int, char*,
			    char*, int);
#define UVM_ASSERT(map, cond, file, line)				\
	uvm_tree_assert((map), (cond), #cond, (file), (line))
void			 uvm_tree_sanity_free(struct vm_map*,
			    struct uvm_map_free*, char*, int);
void			 uvm_tree_sanity(struct vm_map*, char*, int);
void			 uvm_tree_size_chk(struct vm_map*, char*, int);
void			 vmspace_validate(struct vm_map*);
#else
#define uvm_tree_sanity_free(_map, _free, _file, _line)	do {} while (0)
#define uvm_tree_sanity(_map, _file, _line)		do {} while (0)
#define uvm_tree_size_chk(_map, _file, _line)		do {} while (0)
#define vmspace_validate(_map)				do {} while (0)
#endif


/*
 * The kernel map will initially be VM_MAP_KSIZE_INIT bytes.
 * Every time that gets cramped, we grow by at least VM_MAP_KSIZE_DELTA bytes.
 *
 * We attempt to grow by UVM_MAP_KSIZE_ALLOCMUL times the allocation size
 * each time.
 */
#define VM_MAP_KSIZE_INIT	(512 * PAGE_SIZE)
#define VM_MAP_KSIZE_DELTA	(256 * PAGE_SIZE)
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


#define FREE_START(_entry)	((_entry)->end + (_entry)->guard)
#define FREE_END(_entry)	((_entry)->end + (_entry)->guard +	\
				    (_entry)->fspace)

#ifdef DEADBEEF0
#define UVMMAP_DEADBEEF		((void*)DEADBEEF0)
#else
#define UVMMAP_DEADBEEF		((void*)0xdeadd0d0)
#endif

#ifdef DEBUG
int uvm_map_dprintf = 0;
int uvm_map_printlocks = 0;

#define DPRINTF(_args)							\
	do {								\
		if (uvm_map_dprintf)					\
			printf _args;					\
	} while (0)

#define LPRINTF(_args)							\
	do {								\
		if (uvm_map_printlocks)					\
			printf _args;					\
	} while (0)
#else
#define DPRINTF(_args)	do {} while (0)
#define LPRINTF(_args)	do {} while (0)
#endif

static struct timeval uvm_kmapent_last_warn_time;
static struct timeval uvm_kmapent_warn_rate = { 10, 0 };

struct uvm_cnt uvm_map_call, map_backmerge, map_forwmerge;
struct uvm_cnt map_nousermerge;
struct uvm_cnt uvm_mlk_call, uvm_mlk_hint;
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
uvm_mapent_free_insert(struct vm_map *map, struct uvm_map_free *free,
    struct vm_map_entry *entry)
{
	struct vm_map_entry *res;
#ifdef DEBUG
	vaddr_t min, max, bound;
#endif

	if (RB_LEFT(entry, free_entry) != UVMMAP_DEADBEEF ||
	    RB_RIGHT(entry, free_entry) != UVMMAP_DEADBEEF ||
	    RB_PARENT(entry, free_entry) != UVMMAP_DEADBEEF)
		panic("uvm_mapent_addr_insert: entry still in free list");

#ifdef DEBUG
	/*
	 * Boundary check.
	 * Boundaries are folded if they go on the same free list.
	 */
	min = FREE_START(entry);
	max = FREE_END(entry);

	while (min < max && (bound = uvm_map_boundary(map, min, max)) != max) {
		KASSERT(UVM_FREE(map, min) == free);
		min = bound;
	}
#endif
	KDASSERT(entry->fspace > 0 && (entry->fspace & PAGE_MASK) == 0);

	UVM_MAP_REQ_WRITE(map);
	res = RB_INSERT(uvm_map_free_int, &free->tree, entry);
	free->treesz++;
	if (res != NULL)
		panic("uvm_mapent_free_insert");
}

/*
 * Handle free-list removal.
 */
void
uvm_mapent_free_remove(struct vm_map *map, struct uvm_map_free *free,
    struct vm_map_entry *entry)
{
	struct vm_map_entry *res;

	UVM_MAP_REQ_WRITE(map);
	res = RB_REMOVE(uvm_map_free_int, &free->tree, entry);
	free->treesz--;
	if (res != entry)
		panic("uvm_mapent_free_remove");
	RB_LEFT(entry, free_entry) = RB_RIGHT(entry, free_entry) =
	    RB_PARENT(entry, free_entry) = UVMMAP_DEADBEEF;
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
	KDASSERT((entry->start & PAGE_MASK) == 0 &&
	    (entry->end & PAGE_MASK) == 0);

	UVM_MAP_REQ_WRITE(map);
	res = RB_INSERT(uvm_map_addr, &map->addr, entry);
	if (res != NULL)
		panic("uvm_mapent_addr_insert");
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
		simple_lock(&map->ref_lock);				\
		map->ref_count++;					\
		simple_unlock(&map->ref_lock);				\
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
		else if (FREE_END(iter) <= addr)
			iter = RB_RIGHT(iter, daddrs.addr_entry);
		else
			return iter;
	}
	return NULL;
}

/*
 * Find the first entry with at least sz bytes free.
 */
struct vm_map_entry*
uvm_map_findspace_entry(struct uvm_map_free *free, vsize_t sz)
{
	struct vm_map_entry *iter;
	struct vm_map_entry *res;

	iter = RB_ROOT(&free->tree);
	res = NULL;

	while (iter) {
		if (iter->fspace >= sz) {
			res = iter;
			iter = RB_LEFT(iter, free_entry);
		} else
			iter = RB_RIGHT(iter, free_entry);
	}
	return res;
}

/*
 * DEAD_ENTRY_PUSH(struct vm_map_entry**head, struct vm_map_entry *entry)
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
	TAILQ_INSERT_TAIL(deadq, entry, daddrs.deadq);
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

	sel_min = FREE_START(sel);
	sel_max = FREE_END(sel) - sz - (guardpg ? PAGE_SIZE : 0);

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

		/*
		 * Correct for bias.
		 */
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
	if (sel_min < FREE_START(sel)) {
		printf("sel_min: 0x%lx, but should be at least 0x%lx\n",
		    sel_min, FREE_START(sel));
		bad++;
	}
	/* Upper boundary check. */
	if (sel_max > FREE_END(sel) - sz - (guardpg ? PAGE_SIZE : 0)) {
		printf("sel_max: 0x%lx, but should be at most 0x%lx\n",
		    sel_max, FREE_END(sel) - sz - (guardpg ? PAGE_SIZE : 0));
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
		    bias, FREE_START(sel), FREE_END(sel));
	}
#endif /* DIAGNOSTIC */

	*min = sel_min;
	*max = sel_max;
	return 0;
}

/*
 * Find address and free space for sz bytes.
 *
 * free: tree of free space
 * sz: size in bytes
 * align: preferred alignment
 * guardpg: if true, keep free space guards on both ends
 * addr: fill in found address
 *
 * align is a hard requirement to align to virtual addresses.
 * PMAP_PREFER is a soft requirement that is dropped if
 * no memory can be found that will be acceptable.
 *
 * align overrules PMAP_PREFER, but if both can be satisfied, the code
 * will attempt to find a range that does this.
 *
 * Returns NULL on failure.
 */
struct vm_map_entry*
uvm_map_findspace_tree(struct uvm_map_free *free, vsize_t sz, voff_t uoffset,
    vsize_t align, int guardpg, vaddr_t *addr, struct vm_map *map)
{
	struct vm_map_entry *sfe; /* Start free entry. */
	struct vm_map_entry *sel; /* Selected free entry. */
	struct vm_map_entry *search_start, *fail_start;
	size_t sel_idx, i;
	vaddr_t sel_min, sel_max, sel_addr;
	vaddr_t pmap_off, pmap_align; /* pmap_prefer variables */
	int bias;

#ifdef PMAP_PREFER
	/* Fix pmap prefer parameters. */
	pmap_off = 0;
	pmap_align = PMAP_PREFER_ALIGN();
	if (uoffset != UVM_UNKNOWN_OFFSET && pmap_align > PAGE_SIZE)
		pmap_off = PMAP_PREFER_OFFSET(uoffset);
	else
		pmap_align = 0;
	KDASSERT(pmap_align == 0 || pmap_off < pmap_align);

	if (align > PAGE_SIZE || (pmap_off != 0 && pmap_off < align)) {
		/*
		 * We're doomed.
		 *
		 * This allocation will never be able to fulfil the pmap_off
		 * requirement.
		 */
		pmap_off = 0;
		pmap_align = 0;
	}
#else
	pmap_off = pmap_align = 0;
#endif

	/* Set up alignment argument. */
	if (align < PAGE_SIZE)
		align = PAGE_SIZE;

	/*
	 * First entry that meets requirements.
	 */
	sfe = uvm_map_findspace_entry(free, sz + (guardpg ? PAGE_SIZE : 0));
	if (sfe == NULL)
		return NULL;

	/* Select the entry from which we will allocate. */
	sel_idx = arc4random_uniform(FSPACE_DELTA);
	sel = sfe;
	for (i = 0; i < sel_idx; i++) {
		sel = RB_NEXT(uvm_map_free_int, free->tree, sel);
		/*
		 * This has a slight bias at the top of the tree (largest
		 * segments) towards the smaller elements.
		 * This may be nice.
		 */
		if (sel == NULL) {
			sel_idx -= i;
			i = 0;
			sel = sfe;
		}
	}
	search_start = sel;
	fail_start = NULL;

#ifdef PMAP_PREFER
pmap_prefer_retry:
#endif /* PMAP_PREFER */
	while (sel != NULL) {
		bias = uvm_mapent_bias(map, sel);
		if (bias == 0 && free->treesz >= FSPACE_COMPACT)
			bias = (arc4random() & 0x1) ? 1 : -1;

		if (uvm_map_sel_limits(&sel_min, &sel_max, sz, guardpg, sel,
		    align, pmap_align, pmap_off, bias) == 0) {
			if (bias > 0)
				sel_addr = sel_max;
			else if (bias < 0)
				sel_addr = sel_min;
			else if (sel_min == sel_max)
				sel_addr = sel_min;
			else {
				/*
				 * Select a random address.
				 *
				 * Use sel_addr to limit the arc4random range.
				 */
				sel_addr = sel_max - sel_min;
				if (align <= PAGE_SIZE && pmap_align != 0)
					sel_addr += pmap_align;
				else
					sel_addr += align;
				sel_addr = MIN(sel_addr, FSPACE_MAXOFF);

				/*
				 * Shift down, so arc4random can deal with
				 * the number.
				 * arc4random wants a 32-bit number. Therefore,
				 * handle 64-bit overflow.
				 */
				sel_addr >>= PAGE_SHIFT;
				if (sel_addr > 0xffffffff)
					sel_addr = 0xffffffff;
				sel_addr = arc4random_uniform(sel_addr);
				/*
				 * Shift back up.
				 */
				sel_addr <<= PAGE_SHIFT;

				/*
				 * Cancel bits that violate our alignment.
				 *
				 * This also cancels bits that are in
				 * PAGE_MASK, because align is at least
				 * a page.
				 */
				sel_addr &= ~(align - 1);
				if (pmap_align != 0)
					sel_addr &= ~(pmap_align - 1);

				KDASSERT(sel_addr <= sel_max - sel_min);
				/*
				 * Change sel_addr from an offset relative
				 * to sel_min, to the actual selected address.
				 */
				sel_addr += sel_min;
			}

			*addr = sel_addr;
			return sel;
		}

		/* Next entry. */
		sel_idx++;
		sel = RB_NEXT(uvm_map_free_int, &free->tree, sel);
		if (sel_idx == FSPACE_DELTA ||
		    (sel == NULL && sel_idx <= FSPACE_DELTA)) {
			if (fail_start == NULL)
				fail_start = sel;

			sel_idx = 0;
			sel = sfe;
		}

		/*
		 * sel == search_start -> we made a full loop through the
		 * first FSPACE_DELTA items and couldn't find anything.
		 *
		 * We now restart the loop, at the first entry after
		 * FSPACE_DELTA (which we stored in fail_start during
		 * the first iteration).
		 *
		 * In the case that fail_start == NULL, we will stop
		 * immediately.
		 */
		if (sel == search_start) {
			sel_idx = FSPACE_DELTA;
			sel = fail_start;
		}
	}

#ifdef PMAP_PREFER
	/*
	 * If we can't satisfy pmap_prefer, we try without.
	 *
	 * We retry even in the case align is specified, since
	 * uvm_map_sel_limits() always attempts to take it into
	 * account.
	 */
	if (pmap_align != 0) {
		printf("pmap_prefer aligned allocation failed -> "
		    "going for unaligned mapping\n"); /* DEBUG, for now */
		pmap_align = 0;
		pmap_off = 0;
		goto pmap_prefer_retry;
	}
#endif /* PMAP_PREFER */

	/*
	 * Iterated everything, but nothing was good enough.
	 */
	return NULL;
}

/*
 * Test if memory starting at addr with sz bytes is free.
 *
 * Fills in *start_ptr and *end_ptr to be the first and last entry describing
 * the space.
 * If called with prefilled *start_ptr and *end_ptr, they are to be correct.
 */
int
uvm_map_isavail(struct uvm_map_addr *atree, struct vm_map_entry **start_ptr,
    struct vm_map_entry **end_ptr, vaddr_t addr, vsize_t sz)
{
	struct vm_map_entry *i, *i_end;

	KDASSERT(atree != NULL && start_ptr != NULL && end_ptr != NULL);
	if (*start_ptr == NULL) {
		*start_ptr = uvm_map_entrybyaddr(atree, addr);
		if (*start_ptr == NULL)
			return 0;
	} else
		KASSERT(*start_ptr == uvm_map_entrybyaddr(atree, addr));
	if (*end_ptr == NULL) {
		if (FREE_END(*start_ptr) >= addr + sz)
			*end_ptr = *start_ptr;
		else {
			*end_ptr = uvm_map_entrybyaddr(atree, addr + sz - 1);
			if (*end_ptr == NULL)
				return 0;
		}
	} else
		KASSERT(*end_ptr == uvm_map_entrybyaddr(atree, addr + sz - 1));

	KDASSERT(*start_ptr != NULL && *end_ptr != NULL);
	KDASSERT((*start_ptr)->start <= addr && FREE_END(*start_ptr) > addr &&
	    (*end_ptr)->start < addr + sz && FREE_END(*end_ptr) >= addr + sz);

	i = *start_ptr;
	if (i->end > addr)
		return 0;
	i_end = RB_NEXT(uvm_map_addr, atree, *end_ptr);
	for (i = RB_NEXT(uvm_map_addr, atree, i); i != i_end;
	    i = RB_NEXT(uvm_map_addr, atree, i)) {
		if (i->start != i->end)
			return 0;
	}

	return -1;
}

/*
 * uvm_map: establish a valid mapping in map
 *
 * => *addr and sz must be a multiple of PAGE_SIZE.
 * => *addr is ignored, except if flags contains UVM_FLAG_FIXED.
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
 *    Align is only a hint and will be ignored if the alignemnt fails.
 */
int
uvm_map(struct vm_map *map, vaddr_t *addr, vsize_t sz,
    struct uvm_object *uobj, voff_t uoffset, vsize_t align, uvm_flag_t flags)
{
	struct vm_map_entry	*first, *last, *entry;
	struct uvm_map_deadq	 dead;
	struct uvm_map_free	*free;
	vm_prot_t		 prot;
	vm_prot_t		 maxprot;
	vm_inherit_t		 inherit;
	int			 advice;
	int			 error;

	if ((map->flags & VM_MAP_INTRSAFE) == 0)
		splassert(IPL_NONE);
	else
		splassert(IPL_VM);

	/*
	 * Decode parameters.
	 */
	prot = UVM_PROTECTION(flags);
	maxprot = UVM_MAXPROTECTION(flags);
	advice = UVM_ADVICE(flags);
	inherit = UVM_INHERIT(flags);
	error = 0;
	TAILQ_INIT(&dead);
	KASSERT((sz & PAGE_MASK) == 0);
	KASSERT((align & (align - 1)) == 0);

	/*
	 * Holes are incompatible with other types of mappings.
	 */
	if (flags & UVM_FLAG_HOLE) {
		KASSERT(uobj == NULL && (flags & UVM_FLAG_FIXED) &&
		    (flags & (UVM_FLAG_OVERLAY | UVM_FLAG_COPYONW)) == 0);
	}

	/*
	 * Check protection.
	 */
	if ((prot & maxprot) != prot)
		return EACCES;

	if (flags & UVM_FLAG_TRYLOCK) {
		if (vm_map_lock_try(map) == FALSE)
			return EFAULT;
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
		if (!uvm_map_isavail(&map->addr, &first, &last, *addr, sz)) {
			error = ENOMEM;
			goto unlock;
		}

		/*
		 * Grow pmap to include allocated address.
		 * XXX not possible in kernel?
		 */
		if ((map->flags & VM_MAP_ISVMSPACE) == 0 &&
		    uvm_maxkaddr < (*addr + sz)) {
			uvm_map_kmem_grow(map, &dead,
			    *addr + sz - uvm_maxkaddr, flags);

			/*
			 * Reload first, last, since uvm_map_kmem_grow likely
			 * moved them around.
			 */
			first = last = NULL;
			if (!uvm_map_isavail(&map->addr, &first, &last,
			    *addr, sz))
				panic("uvm_map: opened box, cat died");
		}
	} else if (*addr != 0 && (*addr & PAGE_MASK) == 0 &&
	    (map->flags & VM_MAP_ISVMSPACE) == VM_MAP_ISVMSPACE &&
	    (align == 0 || (*addr & (align - 1)) == 0) &&
	    uvm_map_isavail(&map->addr, &first, &last, *addr, sz)) {
		/*
		 * Address used as hint.
		 *
		 * Note: we enforce the alignment restriction,
		 * but ignore the pmap_prefer.
		 */
	} else {
		/*
		 * Update freelists from vmspace.
		 */
		if (map->flags & VM_MAP_ISVMSPACE)
			uvm_map_vmspace_update(map, &dead, flags);

		/*
		 * Allocation for sz bytes at any address on the
		 * freelist.
		 */
		free = &map->free;
		first = uvm_map_findspace_tree(free, sz, uoffset, align,
		    map->flags & VM_MAP_GUARDPAGES, addr, map);
		last = NULL; /* May get set in previous test (by isavail). */

		/*
		 * Fall back into brk() space if the initial attempt failed.
		 */
		if (first == NULL) {
			if (map->flags & VM_MAP_ISVMSPACE)
				free = &map->bfree;
			else
				uvm_map_kmem_grow(map, &dead, sz, flags);

			first = uvm_map_findspace_tree(free, sz, uoffset, align,
			    map->flags & VM_MAP_GUARDPAGES, addr, map);
			if (first == NULL) {
				error = ENOMEM;
				goto unlock;
			}
		}

		/*
		 * Fill in last.
		 */
		if (!uvm_map_isavail(&map->addr, &first, &last, *addr, sz))
			panic("uvm_map: findspace and isavail disagree");
	}

	KASSERT((map->flags & VM_MAP_ISVMSPACE) == VM_MAP_ISVMSPACE ||
	    uvm_maxkaddr >= *addr + sz);

	/*
	 * If we only want a query, return now.
	 */
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
	entry = uvm_map_mkentry(map, first, last, *addr, sz, flags, &dead);
	if (entry == NULL) {
		error = ENOMEM;
		goto unlock;
	}
	KDASSERT(entry->start == *addr && entry->end == *addr + sz);
	entry->object.uvm_obj = uobj;
	entry->offset = uoffset;
	entry->protection = prot;
	entry->max_protection = maxprot;
	entry->inheritance = inherit;
	entry->wired_count = 0;
	entry->advice = advice;
	if (uobj)
		entry->etype = UVM_ET_OBJ;
	else if (flags & UVM_FLAG_HOLE)
		entry->etype = UVM_ET_HOLE;
	else
		entry->etype = 0;
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

	/*
	 * Update map and process statistics.
	 */
	if (!(flags & UVM_FLAG_HOLE))
		map->size += sz;
	if ((map->flags & VM_MAP_ISVMSPACE) && uobj == NULL &&
	    !(flags & UVM_FLAG_HOLE)) {
		((struct vmspace *)map)->vm_dused +=
		    uvmspace_dused(map, *addr, *addr + sz);
	}

	/*
	 * Try to merge entry.
	 *
	 * XXX: I can't think of a good reason to only merge kernel map entries,
	 * but it's what the old code did. I'll look at it later.
	 */
	if ((flags & UVM_FLAG_NOMERGE) == 0)
		entry = uvm_mapent_tryjoin(map, entry, &dead);

unlock:
	vm_map_unlock(map);

	if (error == 0) {
		DPRINTF(("uvm_map:   0x%lx-0x%lx (query=%c)  map=%p\n",
		    *addr, *addr + sz,
		    (flags & UVM_FLAG_QUERY ? 'T' : 'F'), map));
	}

	/*
	 * Remove dead entries.
	 *
	 * Dead entries may be the result of merging.
	 * uvm_map_mkentry may also create dead entries, when it attempts to
	 * destroy free-space entries.
	 */
	uvm_unmap_detach(&dead, 0);
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

	/*
	 * Must be the same entry type and not have free memory between.
	 */
	if (e1->etype != e2->etype || e1->end != e2->start)
		return 0;

	/*
	 * Submaps are never joined.
	 */
	if (UVM_ET_ISSUBMAP(e1))
		return 0;

	/*
	 * Never merge wired memory.
	 */
	if (VM_MAPENT_ISWIRED(e1) || VM_MAPENT_ISWIRED(e2))
		return 0;

	/*
	 * Protection, inheritance and advice must be equal.
	 */
	if (e1->protection != e2->protection ||
	    e1->max_protection != e2->max_protection ||
	    e1->inheritance != e2->inheritance ||
	    e1->advice != e2->advice)
		return 0;

	/*
	 * If uvm_object: objects itself and offsets within object must match.
	 */
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

	/*
	 * Apprently, e1 and e2 match.
	 */
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
	struct uvm_map_free *free;

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

	free = UVM_FREE(map, FREE_START(e2));
	if (e2->fspace > 0 && free)
		uvm_mapent_free_remove(map, free, e2);
	uvm_mapent_addr_remove(map, e2);
	e1->end = e2->end;
	e1->guard = e2->guard;
	e1->fspace = e2->fspace;
	if (e1->fspace > 0 && free)
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

	/*
	 * Merge with previous entry.
	 */
	other = RB_PREV(uvm_map_addr, &map->addr, entry);
	if (other && uvm_mapent_isjoinable(map, other, entry)) {
		merged = uvm_mapent_merge(map, other, entry, dead);
		DPRINTF(("prev merge: %p + %p -> %p\n", other, entry, merged));
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
		DPRINTF(("next merge: %p + %p -> %p\n", entry, other, merged));
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

	while ((entry = TAILQ_FIRST(deadq)) != NULL) {
		/*
		 * Drop reference to amap, if we've got one.
		 */
		if (entry->aref.ar_amap)
			amap_unref(entry->aref.ar_amap,
			    entry->aref.ar_pageoff,
			    atop(entry->end - entry->start),
			    flags);

		/*
		 * Drop reference to our backing object, if we've got one.
		 */
		if (UVM_ET_ISSUBMAP(entry)) {
			/* ... unlikely to happen, but play it safe */
			uvm_map_deallocate(entry->object.sub_map);
		} else if (UVM_ET_ISOBJ(entry) &&
		    entry->object.uvm_obj->pgops->pgo_detach) {
			entry->object.uvm_obj->pgops->pgo_detach(
			    entry->object.uvm_obj);
		}

		/*
		 * Step to next.
		 */
		TAILQ_REMOVE(deadq, entry, daddrs.deadq);
		uvm_mapent_free(entry);
	}
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
    struct uvm_map_deadq *dead)
{
	struct vm_map_entry *entry, *prev;
	struct uvm_map_free *free;
	vaddr_t min, max;	/* free space boundaries for new entry */

	KDASSERT(map != NULL && first != NULL && last != NULL && dead != NULL &&
	    sz > 0 && addr + sz > addr);
	KDASSERT(first->end <= addr && FREE_END(first) > addr);
	KDASSERT(last->start < addr + sz && FREE_END(last) >= addr + sz);
	KDASSERT(uvm_map_isavail(&map->addr, &first, &last, addr, sz));
	uvm_tree_sanity(map, __FILE__, __LINE__);

	min = addr + sz;
	max = FREE_END(last);

	/*
	 * Initialize new entry.
	 */
	entry = uvm_mapent_alloc(map, flags);
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

	/*
	 * Reset free space in first.
	 */
	free = UVM_FREE(map, FREE_START(first));
	if (free)
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
		free = UVM_FREE(map, FREE_START(last));
		if (free && last->fspace > 0)
			uvm_mapent_free_remove(map, free, last);
		uvm_mapent_addr_remove(map, last);
		DEAD_ENTRY_PUSH(dead, last);
	}
	/*
	 * Remove first if it is entirely inside <addr, addr+sz>.
	 */
	if (first->start == addr) {
		uvm_mapent_addr_remove(map, first);
		DEAD_ENTRY_PUSH(dead, first);
	} else
		uvm_map_fix_space(map, first, FREE_START(first), addr, flags);

	/*
	 * Finally, link in entry.
	 */
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
	UVMHIST_FUNC("uvm_mapent_alloc"); UVMHIST_CALLED(maphist);

	pool_flags = PR_WAITOK;
	if (flags & UVM_FLAG_TRYLOCK)
		pool_flags = PR_NOWAIT;

	if (map->flags & VM_MAP_INTRSAFE || cold) {
		s = splvm();
		simple_lock(&uvm.kentry_lock);
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
		simple_unlock(&uvm.kentry_lock);
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
		RB_LEFT(me, free_entry) = RB_RIGHT(me, free_entry) =
		    RB_PARENT(me, free_entry) = UVMMAP_DEADBEEF;
		RB_LEFT(me, daddrs.addr_entry) =
		    RB_RIGHT(me, daddrs.addr_entry) =
		    RB_PARENT(me, daddrs.addr_entry) = UVMMAP_DEADBEEF;
	}

out:
	UVMHIST_LOG(maphist, "<- new entry=%p [kentry=%ld]", me,
	    ((map->flags & VM_MAP_INTRSAFE) != 0 || map == kernel_map), 0, 0);
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
	UVMHIST_FUNC("uvm_mapent_free"); UVMHIST_CALLED(maphist);

	UVMHIST_LOG(maphist,"<- freeing map entry=%p [flags=%ld]",
		me, me->flags, 0, 0);

	if (RB_LEFT(me, free_entry) != UVMMAP_DEADBEEF ||
	    RB_RIGHT(me, free_entry) != UVMMAP_DEADBEEF ||
	    RB_PARENT(me, free_entry) != UVMMAP_DEADBEEF)
		panic("uvm_mapent_free: mapent %p still in free list\n", me);

	if (me->flags & UVM_MAP_STATIC) {
		s = splvm();
		simple_lock(&uvm.kentry_lock);
		RB_LEFT(me, daddrs.addr_entry) = uvm.kentry_free;
		uvm.kentry_free = me;
		uvmexp.kmapent--;
		simple_unlock(&uvm.kentry_lock);
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

	KASSERT((start & PAGE_MASK) == 0 && (end & PAGE_MASK) == 0);
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
	struct uvm_map_free	*free;
	struct vm_map_entry	*prev;
	vaddr_t			 addr;	/* Start of freed range. */
	vaddr_t			 end;	/* End of freed range. */

	prev = *prev_ptr;
	if (prev == entry)
		*prev_ptr = prev = NULL;

	if (prev == NULL ||
	    FREE_END(prev) != entry->start)
		prev = RB_PREV(uvm_map_addr, &map->addr, entry);
	/*
	 * Entry is describing only free memory and has nothing to drain into.
	 */
	if (prev == NULL && entry->start == entry->end && markfree) {
		*prev_ptr = entry;
		return;
	}

	addr = entry->start;
	end = FREE_END(entry);
	free = UVM_FREE(map, FREE_START(entry));
	if (entry->fspace > 0 && free)
		uvm_mapent_free_remove(map, free, entry);
	uvm_mapent_addr_remove(map, entry);
	DEAD_ENTRY_PUSH(dead, entry);

	if (markfree)
		*prev_ptr = uvm_map_fix_space(map, prev, addr, end, 0);
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

	/*
	 * Find first affected entry.
	 */
	entry = uvm_map_entrybyaddr(&map->addr, start);
	KDASSERT(entry != NULL && entry->start <= start);
	if (entry->end <= start && markfree)
		entry = RB_NEXT(uvm_map_addr, &map->addr, entry);
	else
		UVM_MAP_CLIP_START(map, entry, start);

	DPRINTF(("uvm_unmap_p: 0x%lx-0x%lx\n"
	    "\tfirst 0x%lx-0x%lx\n",
	    start, end,
	    entry->start, entry->end));

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
		DPRINTF(("\tunmap 0x%lx-0x%lx used  0x%lx-0x%lx free\n",
		    entry->start, entry->end,
		    FREE_START(entry), FREE_END(entry)));

		/*
		 * Unwire removed map entry.
		 */
		if (VM_MAPENT_ISWIRED(entry)) {
			entry->wired_count = 0;
			uvm_fault_unwire_locked(map, entry->start, entry->end);
		}

		/*
		 * Entry-type specific code.
		 */
		if (UVM_ET_ISHOLE(entry)) {
			/*
			 * Skip holes unless remove_holes.
			 */
			if (!remove_holes) {
				prev_hint = entry;
				continue;
			}
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
			/*
			 * remove mappings the standard way.
			 */
			pmap_remove(map->pmap, entry->start, entry->end);
		}

		/*
		 * Update space usage.
		 */
		if ((map->flags & VM_MAP_ISVMSPACE) &&
		    entry->object.uvm_obj == NULL &&
		    !UVM_ET_ISHOLE(entry)) {
			((struct vmspace *)map)->vm_dused -=
			    uvmspace_dused(map, entry->start, entry->end);
		}
		if (!UVM_ET_ISHOLE(entry))
			map->size -= entry->end - entry->start;

		/*
		 * Actual removal of entry.
		 */
		uvm_mapent_mkfree(map, entry, &prev_hint, dead, markfree);
	}

	pmap_update(vm_map_pmap(map));

	DPRINTF(("uvm_unmap_p: 0x%lx-0x%lx            map=%p\n", start, end,
	    map));

#ifdef DEBUG
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
		if (UVM_ET_ISHOLE(iter) || iter->start == iter->end)
			continue;

		/*
		 * Perform actions of vm_map_lookup that need the write lock.
		 * - create an anonymous map for copy-on-write
		 * - anonymous map for zero-fill
		 * Skip submaps.
		 */
		if (!VM_MAPENT_ISWIRED(iter) && !UVM_ET_ISSUBMAP(iter) &&
		    UVM_ET_ISNEEDSCOPY(iter) &&
		    ((iter->protection & VM_PROT_WRITE) ||
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
		if (UVM_ET_ISHOLE(iter) || iter->start == iter->end)
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
			if (UVM_ET_ISHOLE(first) || first->start == first->end)
				continue;

			first->wired_count--;
			if (!VM_MAPENT_ISWIRED(first)) {
				uvm_fault_unwire_locked(map,
				    iter->start, iter->end);
			}
		}

		/*
		 * decrease counter in the rest of the entries
		 */
		for (; iter != end;
		    iter = RB_NEXT(uvm_map_addr, &map->addr, iter)) {
			if (UVM_ET_ISHOLE(iter) || iter->start == iter->end)
				continue;

			iter->wired_count--;
		}

		if ((lockflags & UVM_LK_EXIT) == 0)
			vm_map_unlock(map);
		return error;
	}

	/*
	 * We are currently holding a read lock.
	 */
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

	if (start > end)
		return EINVAL;
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

	/*
	 * Check that the range has no holes.
	 */
	for (last = first; last != NULL && last->start < end;
	    last = RB_NEXT(uvm_map_addr, &map->addr, last)) {
		if (UVM_ET_ISHOLE(last) ||
		    (last->end < end && FREE_END(last) != last->end)) {
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
	} else
		last = RB_PREV(uvm_map_addr, &map->addr, last);

	/*
	 * Wire/unwire pages here.
	 */
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

		atomic_clearbits_int(&map->flags, VM_MAP_WIREFUTURE);
		vm_map_unlock(map);
		return 0;
	}

	if (flags & MCL_FUTURE)
		atomic_setbits_int(&map->flags, VM_MAP_WIREFUTURE);
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
	KASSERT((min & PAGE_MASK) == 0);
	KASSERT((max & PAGE_MASK) == 0 || (max & PAGE_MASK) == PAGE_MASK);

	/*
	 * Update parameters.
	 *
	 * This code handles (vaddr_t)-1 and other page mask ending addresses
	 * properly.
	 * We lose the top page if the full virtual address space is used.
	 */
	if (max & PAGE_MASK) {
		max += 1;
		if (max == 0) /* overflow */
			max -= PAGE_SIZE;
	}

	RB_INIT(&map->addr);
	RB_INIT(&map->free.tree);
	map->free.treesz = 0;
	RB_INIT(&map->bfree.tree);
	map->bfree.treesz = 0;

	map->size = 0;
	map->ref_count = 1;
	map->min_offset = min;
	map->max_offset = max;
	map->b_start = map->b_end = 0; /* Empty brk() area by default. */
	map->s_start = map->s_end = 0; /* Empty stack area by default. */
	map->flags = flags;
	map->timestamp = 0;
	rw_init(&map->lock, "vmmaplk");
	simple_lock_init(&map->ref_lock);

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
 * Populate map with free-memory entries.
 *
 * Map must be initialized and empty.
 */
void
uvm_map_setup_entries(struct vm_map *map)
{
	KDASSERT(RB_EMPTY(&map->addr));
	KDASSERT(RB_EMPTY(&map->free.tree) && map->free.treesz == 0);
	KDASSERT(RB_EMPTY(&map->bfree.tree) && map->bfree.treesz == 0);

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
	struct uvm_map_free *free;
	vsize_t adj;

	KDASSERT(map != NULL && orig != NULL && next != NULL);
	uvm_tree_sanity(map, __FILE__, __LINE__);
	KASSERT(orig->start < split && FREE_END(orig) > split);

	adj = split - orig->start;
	free = UVM_FREE(map, FREE_START(orig));
	KDASSERT(RB_FIND(uvm_map_addr, &map->addr, orig) == orig);
	KDASSERT(RB_FIND(uvm_map_addr, &map->addr, next) != next);
	KDASSERT(orig->fspace == 0 || free == NULL ||
	    RB_FIND(uvm_map_free_int, &free->tree, orig) == orig);

	/*
	 * Free space will change, unlink from free space tree.
	 */
	if (orig->fspace > 0 && free)
		uvm_mapent_free_remove(map, free, orig);

	uvm_mapent_copy(orig, next);
	if (split >= orig->end) {
		next->etype = 0;
		next->offset = 0;
		next->wired_count = 0;
		next->start = next->end = split;
		next->guard = 0;
		next->fspace = FREE_END(orig) - split;
		next->aref.ar_amap = NULL;
		next->aref.ar_pageoff = 0;
		orig->guard = MIN(orig->guard, split - orig->end);
		orig->fspace = split - FREE_START(orig);
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
	 */
	uvm_mapent_addr_insert(map, next);
	if (orig->fspace > 0 && free)
		uvm_mapent_free_insert(map, free, orig);
	if (next->fspace > 0 && free)
		uvm_mapent_free_insert(map, free, next);

	uvm_tree_sanity(map, __FILE__, __LINE__);
}


#ifdef DEBUG

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
 * Check that free space tree is sane.
 */
void
uvm_tree_sanity_free(struct vm_map *map, struct uvm_map_free *free,
    char *file, int line)
{
	struct vm_map_entry *iter;
	vsize_t space, sz;

	space = PAGE_SIZE;
	sz = 0;
	RB_FOREACH(iter, uvm_map_free_int, &free->tree) {
		sz++;

		UVM_ASSERT(map, iter->fspace >= space, file, line);
		space = iter->fspace;

		UVM_ASSERT(map, RB_FIND(uvm_map_addr, &map->addr, iter) == iter,
		    file, line);
	}
	UVM_ASSERT(map, free->treesz == sz, file, line);
}

/*
 * Check that map is sane.
 */
void
uvm_tree_sanity(struct vm_map *map, char *file, int line)
{
	struct vm_map_entry *iter;
	struct uvm_map_free *free;
	vaddr_t addr;
	vaddr_t min, max, bound; /* Bounds checker. */

	addr = vm_map_min(map);
	RB_FOREACH(iter, uvm_map_addr, &map->addr) {
		/*
		 * Valid start, end.
		 * Catch overflow for end+fspace.
		 */
		UVM_ASSERT(map, iter->end >= iter->start, file, line);
		UVM_ASSERT(map, FREE_END(iter) >= iter->end, file, line);
		/*
		 * May not be empty.
		 */
		UVM_ASSERT(map, iter->start < FREE_END(iter), file, line);

		/*
		 * Addresses for entry must lie within map boundaries.
		 */
		UVM_ASSERT(map, iter->start >= vm_map_min(map) &&
		    FREE_END(iter) <= vm_map_max(map), file, line);

		/*
		 * Tree may not have gaps.
		 */
		UVM_ASSERT(map, iter->start == addr, file, line);
		addr = FREE_END(iter);

		/*
		 * Free space may not cross boundaries, unless the same
		 * free list is used on both sides of the border.
		 */
		min = FREE_START(iter);
		max = FREE_END(iter);

		while (min < max &&
		    (bound = uvm_map_boundary(map, min, max)) != max) {
			UVM_ASSERT(map,
			    UVM_FREE(map, min) == UVM_FREE(map, bound),
			    file, line);
			min = bound;
		}

		/*
		 * Entries with free space must appear in the free list.
		 */
		free = UVM_FREE(map, FREE_START(iter));
		if (iter->fspace > 0 && free) {
			UVM_ASSERT(map,
			    RB_FIND(uvm_map_free_int, &free->tree, iter) ==
			    iter, file, line);
		}
	}
	UVM_ASSERT(map, addr == vm_map_max(map), file, line);

	uvm_tree_sanity_free(map, &map->free, file, line);
	uvm_tree_sanity_free(map, &map->bfree, file, line);
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

#endif /* DEBUG */

/*
 * uvm_map_init: init mapping system at boot time.   note that we allocate
 * and init the static pool of structs vm_map_entry for the kernel here.
 */
void
uvm_map_init(void)
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
	UVMCNT_INIT(map_nousermerge, UVMCNT_CNT, 0, "# back merges skipped", 0);
	UVMCNT_INIT(uvm_mlk_call,  UVMCNT_CNT, 0, "# map lookup calls", 0);
	UVMCNT_INIT(uvm_mlk_hint,  UVMCNT_CNT, 0, "# map lookup hint hits", 0);

	/*
	 * now set up static pool of kernel map entries ...
	 */

	simple_lock_init(&uvm.kentry_lock);
	uvm.kentry_free = NULL;
	for (lcv = 0 ; lcv < MAX_KMAPENT ; lcv++) {
		RB_LEFT(&kernel_map_entry[lcv], daddrs.addr_entry) =
		    uvm.kentry_free;
		uvm.kentry_free = &kernel_map_entry[lcv];
	}

	/*
	 * initialize the map-related pools.
	 */
	pool_init(&uvm_vmspace_pool, sizeof(struct vmspace),
	    0, 0, 0, "vmsppl", &pool_allocator_nointr);
	pool_init(&uvm_map_entry_pool, sizeof(struct vm_map_entry),
	    0, 0, 0, "vmmpepl", &pool_allocator_nointr);
	pool_init(&uvm_map_entry_kmem_pool, sizeof(struct vm_map_entry),
	    0, 0, 0, "vmmpekpl", NULL);
	pool_sethiwat(&uvm_map_entry_pool, 8192);
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
	struct vmspace *vm;
	struct vm_map_entry *entry;
	struct uvm_map_free *free;
	int in_free;

	(*pr)("MAP %p: [0x%lx->0x%lx]\n", map, map->min_offset,map->max_offset);
	(*pr)("\tbrk() allocate range: 0x%lx-0x%lx %ld segments\n",
	    map->b_start, map->b_end, uvm_mapfree_size(&map->bfree));
	(*pr)("\tstack allocate range: 0x%lx-0x%lx %ld segments\n",
	    map->s_start, map->s_end, uvm_mapfree_size(&map->bfree));
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

	/*
	 * struct vmspace handling.
	 */
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
		return;
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

		free = UVM_FREE(map, FREE_START(entry));
		in_free = (free != NULL) &&
		    (RB_FIND(uvm_map_free_int, &free->tree, entry) == entry);
		(*pr)("\thole=%c, free=%c, guard=0x%lx, "
		    "free=0x%lx-0x%lx\n",
		    (entry->etype & UVM_ET_HOLE) ? 'T' : 'F',
		    in_free ? 'T' : 'F',
		    entry->guard,
		    FREE_START(entry), FREE_END(entry));
	}
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

	/*
	 * First, check for protection violations.
	 */
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
	}

	/*
	 * Fix protections.
	 */
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
			    ~VM_PROT_WRITE : VM_PROT_ALL;

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
		 * wire this entry now if the old protection was VM_PROT_NONE
		 * and the new protection is not VM_PROT_NONE.
		 */
		if ((map->flags & VM_MAP_WIREFUTURE) != 0 &&
		    VM_MAPENT_ISWIRED(iter) == 0 &&
		    old_prot == VM_PROT_NONE &&
		    new_prot != VM_PROT_NONE) {
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
	UVMHIST_LOG(maphist, "<- done, rv=%ld",error,0,0,0);
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
	UVMHIST_FUNC("uvmspace_alloc"); UVMHIST_CALLED(maphist);

	vm = pool_get(&uvm_vmspace_pool, PR_WAITOK | PR_ZERO);
	uvmspace_init(vm, NULL, min, max, pageable, remove_holes);
	UVMHIST_LOG(maphist,"<- done (vm=%p)", vm,0,0,0);
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
	UVMHIST_FUNC("uvmspace_init"); UVMHIST_CALLED(maphist);

	if (pmap)
		pmap_reference(pmap);
	else
		pmap = pmap_create();
	vm->vm_map.pmap = pmap;

	uvm_map_setup(&vm->vm_map, min, max,
	    (pageable ? VM_MAP_PAGEABLE : 0) | VM_MAP_ISVMSPACE);

	vm->vm_refcnt = 1;

	if (remove_holes)
		pmap_remove_holes(&vm->vm_map);

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
 * uvmspace_exec: the process wants to exec a new program
 *
 * - XXX: no locking on vmspace
 */

void
uvmspace_exec(struct proc *p, vaddr_t start, vaddr_t end)
{
	struct vmspace *nvm, *ovm = p->p_vmspace;
	struct vm_map *map = &ovm->vm_map;
	struct uvm_map_deadq dead_entries;

	KASSERT((start & PAGE_MASK) == 0);
	KASSERT((end & PAGE_MASK) == 0 || (end & PAGE_MASK) == PAGE_MASK);

	pmap_unuse_final(p);   /* before stack addresses go away */
	TAILQ_INIT(&dead_entries);

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

		/*
		 * Nuke statistics and boundaries.
		 */
		bzero(&ovm->vm_startcopy,
		    (caddr_t) (ovm + 1) - (caddr_t) &ovm->vm_startcopy);


		if (end & PAGE_MASK) {
			end += 1;
			if (end == 0) /* overflow */
				end -= PAGE_SIZE;
		}

		/*
		 * Setup new boundaries and populate map with entries.
		 */
		map->min_offset = start;
		map->max_offset = end;
		uvm_map_setup_entries(map);
		vm_map_unlock(map);

		/*
		 * but keep MMU holes unavailable
		 */
		pmap_remove_holes(map);

	} else {

		/*
		 * p's vmspace is being shared, so we can't reuse it for p since
		 * it is still being used for others.   allocate a new vmspace
		 * for p
		 */
		nvm = uvmspace_alloc(start, end,
		    (map->flags & VM_MAP_PAGEABLE) ? TRUE : FALSE, TRUE);

		/*
		 * install new vmspace and drop our ref to the old one.
		 */

		pmap_deactivate(p);
		p->p_vmspace = nvm;
		pmap_activate(p);

		uvmspace_free(ovm);
	}

	/*
	 * Release dead entries
	 */
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
	struct uvm_map_deadq dead_entries;

	UVMHIST_FUNC("uvmspace_free"); UVMHIST_CALLED(maphist);

	UVMHIST_LOG(maphist,"(vm=%p) ref=%ld", vm, vm->vm_refcnt,0,0);
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
		if ((vm->vm_map.flags & VM_MAP_INTRSAFE) == 0) {
			if (rw_enter(&vm->vm_map.lock, RW_NOSLEEP|RW_WRITE) !=
			    0) {
				panic("uvm_map_setup: "
				    "rw_enter failed on free map");
			}
		}
		uvm_tree_sanity(&vm->vm_map, __FILE__, __LINE__);
		TAILQ_INIT(&dead_entries);
		uvm_unmap_remove(&vm->vm_map,
		    vm->vm_map.min_offset, vm->vm_map.max_offset,
		    &dead_entries, TRUE, FALSE);
		if ((vm->vm_map.flags & VM_MAP_INTRSAFE) == 0)
			rw_exit(&vm->vm_map.lock);
		KDASSERT(RB_EMPTY(&vm->vm_map.addr));
		uvm_unmap_detach(&dead_entries, 0);
		pmap_destroy(vm->vm_map.pmap);
		vm->vm_map.pmap = NULL;
		pool_put(&uvm_vmspace_pool, vm);
	}
	UVMHIST_LOG(maphist,"<- done", 0,0,0,0);
}

/*
 * Clone map entry into other map.
 *
 * Mapping will be placed at dstaddr, for the same length.
 * Space must be available.
 * Reference counters are incremented.
 */
struct vm_map_entry*
uvm_mapent_clone(struct vm_map *dstmap, vaddr_t dstaddr, vsize_t dstlen,
    vsize_t off, struct vm_map_entry *old_entry, struct uvm_map_deadq *dead,
    int mapent_flags, int amap_share_flags)
{
	struct vm_map_entry *new_entry, *first, *last;

	KDASSERT(!UVM_ET_ISSUBMAP(old_entry));

	/*
	 * Create new entry (linked in on creation).
	 * Fill in first, last.
	 */
	first = last = NULL;
	if (!uvm_map_isavail(&dstmap->addr, &first, &last, dstaddr, dstlen)) {
		panic("uvmspace_fork: no space in map for "
		    "entry in empty map");
	}
	new_entry = uvm_map_mkentry(dstmap, first, last,
	    dstaddr, dstlen, mapent_flags, dead);
	if (new_entry == NULL)
		return NULL;
	/* old_entry -> new_entry */
	new_entry->object = old_entry->object;
	new_entry->offset = old_entry->offset;
	new_entry->aref = old_entry->aref;
	new_entry->etype = old_entry->etype;
	new_entry->protection = old_entry->protection;
	new_entry->max_protection = old_entry->max_protection;
	new_entry->inheritance = old_entry->inheritance;
	new_entry->advice = old_entry->advice;

	/*
	 * gain reference to object backing the map (can't
	 * be a submap).
	 */
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
void
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
	pmap_copy(new_map->pmap, old_map->pmap, new_entry->start,
	    (new_entry->end - new_entry->start), new_entry->start);

	/*
	 * Update process statistics.
	 */
	if (!UVM_ET_ISHOLE(new_entry))
		new_map->size += new_entry->end - new_entry->start;
	if (!UVM_ET_ISOBJ(new_entry) && !UVM_ET_ISHOLE(new_entry)) {
		new_vm->vm_dused +=
		    uvmspace_dused(new_map, new_entry->start, new_entry->end);
	}
}

/*
 * copy-on-write the mapping (using mmap's
 * MAP_PRIVATE semantics)
 *
 * allocate new_entry, adjust reference counts.  
 * (note that new references are read-only).
 */
void
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
				if (old_entry->max_protection &
				    VM_PROT_WRITE) {
					pmap_protect(old_map->pmap,
					    old_entry->start,
					    old_entry->end,
					    old_entry->protection &
					    ~VM_PROT_WRITE);
					pmap_update(old_map->pmap);
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

		pmap_copy(new_map->pmap, old_map->pmap,
		    new_entry->start,
		    (old_entry->end - old_entry->start),
		    old_entry->start);

		/*
		 * protect the child's mappings if necessary
		 */
		if (protect_child) {
			pmap_protect(new_map->pmap, new_entry->start,
			    new_entry->end,
			    new_entry->protection &
			    ~VM_PROT_WRITE);
		}
	}

	/*
	 * Update process statistics.
	 */
	if (!UVM_ET_ISHOLE(new_entry))
		new_map->size += new_entry->end - new_entry->start;
	if (!UVM_ET_ISOBJ(new_entry) && !UVM_ET_ISHOLE(new_entry)) {
		new_vm->vm_dused +=
		    uvmspace_dused(new_map, new_entry->start, new_entry->end);
	}
}

/*
 * uvmspace_fork: fork a process' main map
 *
 * => create a new vmspace for child process from parent.
 * => parent's map must not be locked.
 */
struct vmspace *
uvmspace_fork(struct vmspace *vm1)
{
	struct vmspace *vm2;
	struct vm_map *old_map = &vm1->vm_map;
	struct vm_map *new_map;
	struct vm_map_entry *old_entry;
	struct uvm_map_deadq dead;
	UVMHIST_FUNC("uvmspace_fork"); UVMHIST_CALLED(maphist);

	vm_map_lock(old_map);

	vm2 = uvmspace_alloc(old_map->min_offset, old_map->max_offset,
	    (old_map->flags & VM_MAP_PAGEABLE) ? TRUE : FALSE, FALSE);
	memcpy(&vm2->vm_startcopy, &vm1->vm_startcopy,
	    (caddr_t) (vm1 + 1) - (caddr_t) &vm1->vm_startcopy);
	vm2->vm_dused = 0; /* Statistic managed by us. */
	new_map = &vm2->vm_map;
	vm_map_lock(new_map);

	/*
	 * go entry-by-entry
	 */

	TAILQ_INIT(&dead);
	RB_FOREACH(old_entry, uvm_map_addr, &old_map->addr) {
		if (old_entry->start == old_entry->end)
			continue;

		/*
		 * first, some sanity checks on the old entry
		 */
		if (UVM_ET_ISSUBMAP(old_entry)) {
			panic("fork: encountered a submap during fork "
			    "(illegal)");
		}

		if (!UVM_ET_ISCOPYONWRITE(old_entry) &&
		    UVM_ET_ISNEEDSCOPY(old_entry)) {
			panic("fork: non-copy_on_write map entry marked "
			    "needs_copy (illegal)");
		}

		/*
		 * Apply inheritance.
		 */
		if (old_entry->inheritance == MAP_INHERIT_SHARE) {
			uvm_mapent_forkshared(vm2, new_map,
			    old_map, old_entry, &dead);
		}
		if (old_entry->inheritance == MAP_INHERIT_COPY) {
			uvm_mapent_forkcopy(vm2, new_map,
			    old_map, old_entry, &dead);
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

	UVMHIST_LOG(maphist,"<- done",0,0,0,0);
	return vm2;    
}

/*
 * uvm_map_hint: return the beginning of the best area suitable for
 * creating a new mapping with "prot" protection.
 */
vaddr_t
uvm_map_hint(struct proc *p, vm_prot_t prot)
{
	vaddr_t addr;

#ifdef __i386__
	/*
	 * If executable skip first two pages, otherwise start
	 * after data + heap region.
	 */
	if ((prot & VM_PROT_EXECUTE) &&
	    ((vaddr_t)p->p_vmspace->vm_daddr >= I386_MAX_EXE_ADDR)) {
		addr = (PAGE_SIZE*2) +
		    (arc4random() & (I386_MAX_EXE_ADDR / 2 - 1));
		return (round_page(addr));
	}
#endif
	/* start malloc/mmap after the brk */
	addr = (vaddr_t)p->p_vmspace->vm_daddr + BRKSIZ;
#if !defined(__vax__)
	addr += arc4random() & (MIN((256 * 1024 * 1024), BRKSIZ) - 1);
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
		/*
		 * Fail if a hole is found.
		 */
		if (UVM_ET_ISHOLE(entry) ||
		    (entry->end < end && entry->end != FREE_END(entry)))
			return FALSE;

		/*
		 * Check protection.
		 */
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

	simple_lock(&map->ref_lock);
	c = --map->ref_count;
	simple_unlock(&map->ref_lock);
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
	free(map, M_VMMAP);

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
	UVMHIST_FUNC("uvm_map_inherit"); UVMHIST_CALLED(maphist);
	UVMHIST_LOG(maphist,"(map=%p,start=0x%lx,end=0x%lx,new_inh=0x%lx)",
	    map, start, end, new_inheritance);

	switch (new_inheritance) {
	case MAP_INHERIT_NONE:
	case MAP_INHERIT_COPY:
	case MAP_INHERIT_SHARE:
		break;
	default:
		UVMHIST_LOG(maphist,"<- done (INVALID ARG)",0,0,0,0);
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
	UVMHIST_LOG(maphist,"<- done (OK)",0,0,0,0);
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
	UVMHIST_FUNC("uvm_map_advice"); UVMHIST_CALLED(maphist);
	UVMHIST_LOG(maphist,"(map=%p,start=0x%lx,end=0x%lx,new_adv=0x%lx)",
	    map, start, end, new_advice);

	switch (new_advice) {
	case MADV_NORMAL:
	case MADV_RANDOM:
	case MADV_SEQUENTIAL:
		break;
	default:
		UVMHIST_LOG(maphist,"<- done (INVALID ARG)",0,0,0,0);
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
	UVMHIST_LOG(maphist,"<- done (OK)",0,0,0,0);
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
	struct vm_map_entry *first, *entry, *newentry;
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

	if ((start & PAGE_MASK) != 0 || (end & PAGE_MASK) != 0 || end < start)
		return EINVAL;
	if (start < srcmap->min_offset || end > srcmap->max_offset)
		return EINVAL;

	/*
	 * Initialize dead entries.
	 * Handle len == 0 case.
	 */

	if (len == 0)
		return 0;

	/*
	 * Acquire lock on srcmap.
	 */
	vm_map_lock(srcmap);

	/*
	 * Lock srcmap, lookup first and last entry in <start,len>.
	 */
	first = uvm_map_entrybyaddr(&srcmap->addr, start);

	/*
	 * Check that the range is contiguous.
	 */
	for (entry = first; entry != NULL && entry->end < end;
	    entry = RB_NEXT(uvm_map_addr, &map->addr, entry)) {
		if (FREE_END(entry) != entry->end || UVM_ET_ISHOLE(entry)) {
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

	/*
	 * Lock destination map (kernel_map).
	 */
	vm_map_lock(kernel_map);

	if (uvm_map_findspace_tree(&kernel_map->free, len, UVM_UNKNOWN_OFFSET,
	    0, kernel_map->flags & VM_MAP_GUARDPAGES, &dstaddr, kernel_map) ==
	    NULL) {
		error = ENOMEM;
		goto fail2;
	}
	*dstaddrp = dstaddr;

	/*
	 * We now have srcmap and kernel_map locked.
	 * dstaddr contains the destination offset in dstmap.
	 */

	/*
	 * step 1: start looping through map entries, performing extraction.
	 */
	for (entry = first; entry != NULL && entry->start < end;
	    entry = RB_NEXT(uvm_map_addr, &map->addr, entry)) {
		KDASSERT(!UVM_ET_ISNEEDSCOPY(entry));
		if (UVM_ET_ISHOLE(entry))
			continue;

		/*
		 * Calculate uvm_mapent_clone parameters.
		 */
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
	}

	/*
	 * step 2: perform pmap copy.
	 */
	for (entry = first; entry != NULL && entry->start < end;
	    entry = RB_NEXT(uvm_map_addr, &map->addr, entry)) {
		/*
		 * Calculate uvm_mapent_clone parameters (again).
		 */
		cp_start = entry->start;
		if (cp_start < start)
			cp_start = start;
		cp_len = MIN(entry->end, end) - cp_start;

		pmap_copy(kernel_map->pmap, srcmap->pmap,
		    cp_start - start + dstaddr, cp_len, cp_start);
	}
	pmap_update(kernel_map->pmap);

	error = 0;

	/*
	 * Unmap copied entries on failure.
	 */
fail2_unmap:
	if (error) {
		uvm_unmap_remove(kernel_map, dstaddr, dstaddr + len, &dead,
		    FALSE, TRUE);
	}

	/*
	 * Release maps, release dead entries.
	 */
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

	UVMHIST_FUNC("uvm_map_clean"); UVMHIST_CALLED(maphist);

	UVMHIST_LOG(maphist,"(map=%p,start=0x%lx,end=0x%lx,flags=0x%lx)",
	    map, start, end, flags);
	KASSERT((flags & (PGO_FREE|PGO_DEACTIVATE)) !=
	    (PGO_FREE|PGO_DEACTIVATE));

	if (start > end || start < map->min_offset || end > map->max_offset)
		return EINVAL;

	vm_map_lock_read(map);
	first = uvm_map_entrybyaddr(&map->addr, start);

	/*
	 * Make a first pass to check for holes.
	 */
	for (entry = first; entry->start < end;
	    entry = RB_NEXT(uvm_map_addr, &map->addr, entry)) {
		if (UVM_ET_ISSUBMAP(entry)) {
			vm_map_unlock_read(map);
			return EINVAL;
		}
		if (UVM_ET_ISSUBMAP(entry) ||
		    UVM_ET_ISHOLE(entry) ||
		    (entry->end < end && FREE_END(entry) != entry->end)) {
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

			simple_lock(&anon->an_lock); /* XXX */

			pg = anon->an_page;
			if (pg == NULL) {
				simple_unlock(&anon->an_lock);
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
					simple_unlock(&anon->an_lock);
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
					simple_unlock(&anon->an_lock);
					break;
				}
				KASSERT(pg->uanon == anon);

				/* zap all mappings for the page. */
				pmap_page_protect(pg, VM_PROT_NONE);

				/* ...and deactivate the page. */
				uvm_pagedeactivate(pg);

				uvm_unlock_pageq();
				simple_unlock(&anon->an_lock);
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
					simple_unlock(&anon->an_lock);
					break;
				}
				amap_unadd(&entry->aref,
				    cp_start - entry->start);
				refs = --anon->an_ref;
				simple_unlock(&anon->an_lock);
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
		     ((entry->max_protection & VM_PROT_WRITE) != 0 &&
		      (entry->etype & UVM_ET_COPYONWRITE) == 0))) {
			simple_lock(&uobj->vmobjlock);
			rv = uobj->pgops->pgo_flush(uobj,
			    cp_start - entry->start + entry->offset,
			    cp_end - entry->start + entry->offset, flags);
			simple_unlock(&uobj->vmobjlock);

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

	KASSERT(entry->start < addr && FREE_END(entry) > addr);
	tmp = uvm_mapent_alloc(map, 0);

	/*
	 * Invoke splitentry.
	 */
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
	struct uvm_map_free *free;

	/*
	 * Copy entry.
	 */
	KASSERT(entry->start < addr && FREE_END(entry) > addr);
	tmp = uvm_mapent_alloc(map, 0);
	uvm_mapent_copy(entry, tmp);

	/*
	 * Put new entry in place of original entry.
	 */
	free = UVM_FREE(map, FREE_START(entry));
	uvm_mapent_addr_remove(map, entry);
	if (entry->fspace > 0 && free) {
		uvm_mapent_free_remove(map, free, entry);
		uvm_mapent_free_insert(map, free, tmp);
	}
	uvm_mapent_addr_insert(map, tmp);

	/*
	 * Invoke splitentry.
	 */
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
 */
struct uvm_map_free*
uvm_free(struct vm_map *map, vaddr_t addr)
{
	/* Special case the first page, to prevent mmap from returning 0. */
	if (addr < VMMAP_MIN_ADDR)
		return NULL;

	if ((map->flags & VM_MAP_ISVMSPACE) == 0) {
		if (addr >= uvm_maxkaddr)
			return NULL;
	} else {
		/* addr falls within brk() area. */
		if (addr >= map->b_start && addr < map->b_end)
			return &map->bfree;
		/* addr falls within stack area. */
		if (addr >= map->s_start && addr < map->s_end)
			return &map->bfree;
	}
	return &map->free;
}

/*
 * Returns the first free-memory boundary that is crossed by [min-max].
 */
vsize_t
uvm_map_boundary(struct vm_map *map, vaddr_t min, vaddr_t max)
{
	/* Treat the first page special, mmap returning 0 breaks too much. */
	max = uvm_map_boundfix(min, max, VMMAP_MIN_ADDR);

	if ((map->flags & VM_MAP_ISVMSPACE) == 0) {
		max = uvm_map_boundfix(min, max, uvm_maxkaddr);
	} else {
		max = uvm_map_boundfix(min, max, map->b_start);
		max = uvm_map_boundfix(min, max, map->b_end);
		max = uvm_map_boundfix(min, max, map->s_start);
		max = uvm_map_boundfix(min, max, map->s_end);
	}
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
	if ((b_start & PAGE_MASK) != 0 || (b_end & PAGE_MASK) != 0 ||
	    (s_start & PAGE_MASK) != 0 || (s_end & PAGE_MASK) != 0) {
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

	/*
	 * Grow by ALLOCMUL * alloc_sz, but at least VM_MAP_KSIZE_DELTA.
	 *
	 * Don't handle the case where the multiplication overflows:
	 * if that happens, the allocation is probably too big anyway.
	 */
	sz = MAX(VM_MAP_KSIZE_ALLOCMUL * alloc_sz, VM_MAP_KSIZE_DELTA);

	/*
	 * Include the guard page in the hard minimum requirement of alloc_sz.
	 */
	if (map->flags & VM_MAP_GUARDPAGES)
		alloc_sz += PAGE_SIZE;

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
		end = MAX(FREE_START(entry), end);
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
	struct uvm_map_free *free;
	struct vm_map_entry *entry, *prev, *next;

	prev = NULL;
	for (entry = RB_MIN(uvm_map_addr, &map->addr); entry != NULL;
	    entry = next) {
		next = RB_NEXT(uvm_map_addr, &map->addr, entry);

		free = UVM_FREE(map, FREE_START(entry));
		if (entry->fspace > 0 && free)
			uvm_mapent_free_remove(map, free, entry);

		if (prev != NULL && entry->start == entry->end) {
			prev->fspace += FREE_END(entry) - entry->end;
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
		min = FREE_START(entry);
		max = FREE_END(entry);
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
 * Correct space insert.
 */
struct vm_map_entry*
uvm_map_fix_space(struct vm_map *map, struct vm_map_entry *entry,
    vaddr_t min, vaddr_t max, int flags)
{
	struct uvm_map_free *free;
	vaddr_t lmax;

	KDASSERT(min <= max);
	KDASSERT((entry != NULL && FREE_END(entry) == min) ||
	    min == map->min_offset);
	while (min != max) {
		/*
		 * Claim guard page for entry.
		 */
		if ((map->flags & VM_MAP_GUARDPAGES) && entry != NULL &&
		    FREE_END(entry) == entry->end &&
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
			min = FREE_START(entry);
		}

		lmax = uvm_map_boundary(map, min, max);
		free = UVM_FREE(map, min);

		if (entry != NULL && free == UVM_FREE(map, FREE_START(entry))) {
			KDASSERT(FREE_END(entry) == min);
			if (entry->fspace > 0 && free != NULL)
				uvm_mapent_free_remove(map, free, entry);
			entry->fspace += lmax - min;
		} else {
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
		}

		if (free)
			uvm_mapent_free_insert(map, free, entry);

		min = lmax;
	}

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
#ifdef PMAP_PREFER
	vaddr_t tmp;
#endif
	int error;

	addr = *addr_p;
	vm_map_lock_read(map);

#ifdef PMAP_PREFER
	if (!(flags & UVM_FLAG_FIXED) && offset != UVM_UNKNOWN_OFFSET)
		addr = PMAP_PREFER(offset, addr);
#endif

	/*
	 * First, check if the requested range is fully available.
	 */
	entry = uvm_map_entrybyaddr(&map->addr, addr);
	last = NULL;
	if (uvm_map_isavail(&map->addr, &entry, &last, addr, sz)) {
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
	} else if (FREE_START(entry) <= addr) {
		/* [3] Bumped into used memory. */
		entry = RB_NEXT(uvm_map_addr, &map->addr, entry);
	}

	/*
	 * Test if the next entry is sufficient for the allocation.
	 */
	for (; entry != NULL;
	    entry = RB_NEXT(uvm_map_addr, &map->addr, entry)) {
		if (entry->fspace == 0)
			continue;
		addr = FREE_START(entry);

restart:	/* Restart address checks on address change. */

#ifdef PMAP_PREFER
		if (offset != UVM_UNKNOWN_OFFSET) {
			tmp = (addr & ~(PMAP_PREFER_ALIGN() - 1)) |
			    PMAP_PREFER_OFFSET(offset);
			if (tmp < addr)
				tmp += PMAP_PREFER_ALIGN();
			if (addr >= FREE_END(entry))
				continue;
			if (addr != tmp) {
				addr = tmp;
				goto restart;
			}
		}
#endif

		/*
		 * Skip brk() allocation addresses.
		 */
		if (addr + sz > map->b_start && addr < map->b_end) {
			if (FREE_END(entry) > map->b_end) {
				addr = map->b_end;
				goto restart;
			} else
				continue;
		}
		/*
		 * Skip stack allocation addresses.
		 */
		if (addr + sz > map->s_start && addr < map->s_end) {
			if (FREE_END(entry) > map->s_end) {
				addr = map->s_end;
				goto restart;
			} else
				continue;
		}

		last = NULL;
		if (uvm_map_isavail(&map->addr, &entry, &last, addr, sz)) {
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

	start = FREE_START(entry);
	end = FREE_END(entry);

	/*
	 * Stay at the top of brk() area.
	 */
	if (end >= map->b_start && start < map->b_end)
		return 1;
	/*
	 * Stay at the far end of the stack area.
	 */
	if (end >= map->s_start && start < map->s_end) {
#ifdef MACHINE_STACK_GROWS_UP
		return 1;
#else
		return -1;
#endif
	}

	/*
	 * No bias, this area is meant for us.
	 */
	return 0;
}


boolean_t
vm_map_lock_try_ln(struct vm_map *map, char *file, int line)
{
	boolean_t rv;

	if (map->flags & VM_MAP_INTRSAFE) {
		rv = TRUE;
	} else {
		if (map->flags & VM_MAP_BUSY) {
			return (FALSE);
		}
		rv = (rw_enter(&map->lock, RW_WRITE|RW_NOSLEEP) == 0);
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
			while (map->flags & VM_MAP_BUSY) {
				map->flags |= VM_MAP_WANTLOCK;
				tsleep(&map->flags, PVM, (char *)vmmapbsy, 0);
			}
		} while (rw_enter(&map->lock, RW_WRITE|RW_SLEEPFAIL) != 0);
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
	map->flags |= VM_MAP_BUSY;
}

void
vm_map_unbusy_ln(struct vm_map *map, char *file, int line)
{
	int oflags;

	oflags = map->flags;
	map->flags &= ~(VM_MAP_BUSY|VM_MAP_WANTLOCK);
	if (oflags & VM_MAP_WANTLOCK)
		wakeup(&map->flags);
}


RB_GENERATE(uvm_map_addr, vm_map_entry, daddrs.addr_entry,
    uvm_mapentry_addrcmp);
RB_GENERATE(uvm_map_free_int, vm_map_entry, free_entry, uvm_mapentry_freecmp);
