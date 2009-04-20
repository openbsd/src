/*	$OpenBSD: uvm_pglist.c,v 1.27 2009/04/20 00:30:18 oga Exp $	*/
/*	$NetBSD: uvm_pglist.c,v 1.13 2001/02/18 21:19:08 chs Exp $	*/

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
 * uvm_pglist.c: pglist functions
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/proc.h>

#include <uvm/uvm.h>

#ifdef VM_PAGE_ALLOC_MEMORY_STATS
#define	STAT_INCR(v)	(v)++
#define	STAT_DECR(v)	do { \
		if ((v) == 0) \
			printf("%s:%d -- Already 0!\n", __FILE__, __LINE__); \
		else \
			(v)--; \
	} while (0)
u_long	uvm_pglistalloc_npages;
#else
#define	STAT_INCR(v)
#define	STAT_DECR(v)
#endif

int	uvm_pglistalloc_simple(psize_t, paddr_t, paddr_t, struct pglist *);

/*
 * Simple page allocation: pages do not need to be contiguous. We just
 * attempt to find enough free pages in the given range.
 */
int
uvm_pglistalloc_simple(psize_t size, paddr_t low, paddr_t high,
    struct pglist *rlist)
{
	psize_t todo;
	int psi;
	struct vm_page *pg;
	struct vm_physseg *seg;
	paddr_t slow, shigh;
	int pgflidx, error, free_list;
	UVMHIST_FUNC("uvm_pglistalloc_simple"); UVMHIST_CALLED(pghist);
#ifdef DEBUG
	vm_page_t tp;
#endif

	/* Default to "lose". */
	error = ENOMEM;

	todo = atop(size);

	/*
	 * Block all memory allocation and lock the free list.
	 */
	uvm_lock_fpageq();

	/* Are there even any free pages? */
	if (uvmexp.free <= (uvmexp.reserve_pagedaemon + uvmexp.reserve_kernel))
		goto out;

	for (psi = 0, seg = vm_physmem; psi < vm_nphysseg; psi++, seg++) {
		/*
		 * Skip this segment if incompatible with the address range.
		 */
		if (seg->avail_end <= atop(low))
			continue;
		if (seg->avail_start >= atop(high))
			continue;

		slow = MAX(atop(low), seg->avail_start);
		shigh = MIN(atop(high), seg->avail_end);

		/* we want to be able to allocate at least a page... */
		if (slow == shigh)
			continue;

		for (pg = &seg->pgs[slow - seg->start]; slow != shigh;
		    slow++, pg++) {
			if (VM_PAGE_IS_FREE(pg) == 0)
				continue;

			free_list = uvm_page_lookup_freelist(pg);
			pgflidx = (pg->pg_flags & PG_ZERO) ?
			    PGFL_ZEROS : PGFL_UNKNOWN;
#ifdef DEBUG
			for (tp = TAILQ_FIRST(&uvm.page_free[free_list].pgfl_queues[pgflidx]);
			     tp != NULL; tp = TAILQ_NEXT(tp, pageq)) {
				if (tp == pg)
					break;
			}
			if (tp == NULL)
				panic("uvm_pglistalloc_simple: page not on freelist");
#endif
			TAILQ_REMOVE(&uvm.page_free[free_list].pgfl_queues[pgflidx],
			    pg, pageq);
			uvmexp.free--;
			if (pg->pg_flags & PG_ZERO)
				uvmexp.zeropages--;
			pg->uobject = NULL;
			pg->uanon = NULL;
			pg->pg_version++;
			TAILQ_INSERT_TAIL(rlist, pg, pageq);
			STAT_INCR(uvm_pglistalloc_npages);
			if (--todo == 0) {
				error = 0;
				goto out;
			}
		}

	}

out:
	/*
	 * check to see if we need to generate some free pages waking
	 * the pagedaemon.
	 */

	if (!error && (uvmexp.free + uvmexp.paging < uvmexp.freemin ||
	    (uvmexp.free + uvmexp.paging < uvmexp.freetarg &&
	    uvmexp.inactive < uvmexp.inactarg))) {
		wakeup(&uvm.pagedaemon);
	}

	uvm_unlock_fpageq();

	if (error)
		uvm_pglistfree(rlist);

	return (error);
}

/*
 * uvm_pglistalloc: allocate a list of pages
 *
 * => allocated pages are placed at the tail of rlist.  rlist is
 *    assumed to be properly initialized by caller.
 * => returns 0 on success or errno on failure
 * => XXX: implementation allocates only a single segment, also
 *	might be able to better advantage of vm_physeg[].
 * => doesn't take into account clean non-busy pages on inactive list
 *	that could be used(?)
 * => params:
 *	size		the size of the allocation, rounded to page size.
 *	low		the low address of the allowed allocation range.
 *	high		the high address of the allowed allocation range.
 *	alignment	memory must be aligned to this power-of-two boundary.
 *	boundary	no segment in the allocation may cross this 
 *			power-of-two boundary (relative to zero).
 */

int
uvm_pglistalloc(psize_t size, paddr_t low, paddr_t high, paddr_t alignment,
    paddr_t boundary, struct pglist *rlist, int nsegs, int flags)
{
	int psi;
	struct vm_page *pgs;
	struct vm_physseg *seg;
	paddr_t slow, shigh;
	paddr_t try, idxpa, lastidxpa;
	int tryidx, idx, pgflidx, endidx, error, free_list;
	vm_page_t m;
	u_long pagemask;
#ifdef DEBUG
	vm_page_t tp;
#endif
	UVMHIST_FUNC("uvm_pglistalloc"); UVMHIST_CALLED(pghist);

	KASSERT((alignment & (alignment - 1)) == 0);
	KASSERT((boundary & (boundary - 1)) == 0);
	/*
	 * This argument is always ignored for now, but ensure drivers always
	 * show intention.
	 */
	KASSERT(!(flags & UVM_PLA_WAITOK) ^ !(flags & UVM_PLA_NOWAIT));
	
	/*
	 * Our allocations are always page granularity, so our alignment
	 * must be, too.
	 */
	if (alignment < PAGE_SIZE)
		alignment = PAGE_SIZE;

	if (size == 0)
		return (EINVAL);

	size = round_page(size);
	low = roundup(low, alignment);

	/*
	 * If we are allowed to allocate as many segments as pages,
	 * no need to be smart.
	 */
	if ((nsegs >= size / PAGE_SIZE) && (alignment == PAGE_SIZE) &&
	    (boundary == 0)) {
		error = uvm_pglistalloc_simple(size, low, high, rlist);
		goto done;
	}

	if (boundary != 0 && boundary < size)
		return (EINVAL);

	pagemask = ~(boundary - 1);

	/* Default to "lose". */
	error = ENOMEM;

	/*
	 * Block all memory allocation and lock the free list.
	 */
	uvm_lock_fpageq();

	/* Are there even any free pages? */
	if (uvmexp.free <= (uvmexp.reserve_pagedaemon + uvmexp.reserve_kernel))
		goto out;

	for (psi = 0, seg = vm_physmem; psi < vm_nphysseg; psi++, seg++) {
		/*
		 * Skip this segment if incompatible with the address range.
		 */
		if (seg->avail_end <= atop(low))
			continue;
		if (seg->avail_start >= atop(high))
			continue;

		slow = MAX(low, ptoa(seg->avail_start));
		shigh = MIN(high, ptoa(seg->avail_end));

		try = roundup(slow, alignment);
		for (;; try += alignment) {
			if (try + size > shigh) {
				/*
				 * We've run past the allowable range, or
				 * the segment. Try another.
				 */
				break;
			}

			tryidx = idx = atop(try) - seg->start;
			endidx = idx + atop(size);
			pgs = vm_physmem[psi].pgs;

			/*
			 * Found a suitable starting page.  See if the
			 * range is free.
			 */

			for (; idx < endidx; idx++) {
				if (VM_PAGE_IS_FREE(&pgs[idx]) == 0) {
					break;
				}
				idxpa = VM_PAGE_TO_PHYS(&pgs[idx]);
				if (idx == tryidx)
					continue;

				/*
				 * Check that the region is contiguous
				 * (it really should...) and does not
				 * cross an alignment boundary.
				 */
				lastidxpa = VM_PAGE_TO_PHYS(&pgs[idx - 1]);
				if ((lastidxpa + PAGE_SIZE) != idxpa)
					break;

				if (boundary != 0 &&
				    ((lastidxpa ^ idxpa) & pagemask) != 0)
					break;
			}

			if (idx == endidx) {
				goto found;
			}
		}
	}

	/*
	 * We could not allocate a contiguous range.  This is where
	 * we should try harder if nsegs > 1...
	 */
	goto out;

#if PGFL_NQUEUES != 2
#error uvm_pglistalloc needs to be updated
#endif

found:
	/*
	 * we have a chunk of memory that conforms to the requested constraints.
	 */
	idx = tryidx;
	while (idx < endidx) {
		m = &pgs[idx];
		free_list = uvm_page_lookup_freelist(m);
		pgflidx = (m->pg_flags & PG_ZERO) ? PGFL_ZEROS : PGFL_UNKNOWN;
#ifdef DEBUG
		for (tp = TAILQ_FIRST(&uvm.page_free[
			free_list].pgfl_queues[pgflidx]);
		     tp != NULL;
		     tp = TAILQ_NEXT(tp, pageq)) {
			if (tp == m)
				break;
		}
		if (tp == NULL)
			panic("uvm_pglistalloc: page not on freelist");
#endif
		TAILQ_REMOVE(&uvm.page_free[free_list].pgfl_queues[pgflidx],
		    m, pageq);
		uvmexp.free--;
		if (m->pg_flags & PG_ZERO)
			uvmexp.zeropages--;
		m->uobject = NULL;
		m->uanon = NULL;
		m->pg_version++;
		TAILQ_INSERT_TAIL(rlist, m, pageq);
		idx++;
		STAT_INCR(uvm_pglistalloc_npages);
	}
	error = 0;

out:
	/*
	 * check to see if we need to generate some free pages waking
	 * the pagedaemon.
	 */
	 
	if (uvmexp.free + uvmexp.paging < uvmexp.freemin ||
	    (uvmexp.free + uvmexp.paging < uvmexp.freetarg &&
	     uvmexp.inactive < uvmexp.inactarg)) {
		wakeup(&uvm.pagedaemon);
	}

	uvm_unlock_fpageq();

done: 
	/* No locking needed here, pages are not on any queue. */
	if (error == 0) {
		TAILQ_FOREACH(m, rlist, pageq) {
			if (flags & UVM_PLA_ZERO &&
			    (m->pg_flags & PG_ZERO) == 0)
				uvm_pagezero(m);
			m->pg_flags = PG_CLEAN;
		}
	}

	return (error);
}

/*
 * uvm_pglistfree: free a list of pages
 *
 * => pages should already be unmapped
 */

void
uvm_pglistfree(struct pglist *list)
{
	struct vm_page *m;
	UVMHIST_FUNC("uvm_pglistfree"); UVMHIST_CALLED(pghist);

	/*
	 * Block all memory allocation and lock the free list.
	 */
	uvm_lock_fpageq();

	while ((m = TAILQ_FIRST(list)) != NULL) {
		KASSERT((m->pg_flags & (PQ_ACTIVE|PQ_INACTIVE)) == 0);
		TAILQ_REMOVE(list, m, pageq);
#ifdef DEBUG
		if (m->uobject == (void *)0xdeadbeef &&
		    m->uanon == (void *)0xdeadbeef) {
			panic("uvm_pagefree: freeing free page %p", m);
		}

		m->uobject = (void *)0xdeadbeef;
		m->offset = 0xdeadbeef;
		m->uanon = (void *)0xdeadbeef;
#endif
		atomic_clearbits_int(&m->pg_flags, PQ_MASK);
		atomic_setbits_int(&m->pg_flags, PQ_FREE);
		TAILQ_INSERT_TAIL(&uvm.page_free[
		    uvm_page_lookup_freelist(m)].pgfl_queues[PGFL_UNKNOWN],
		    m, pageq);
		uvmexp.free++;
		if (uvmexp.zeropages < UVM_PAGEZERO_TARGET)
			uvm.page_idle_zero = vm_page_zero_enable;
		STAT_DECR(uvm_pglistalloc_npages);
	}

	uvm_unlock_fpageq();
}
