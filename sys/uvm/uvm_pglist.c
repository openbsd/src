/*	$OpenBSD: uvm_pglist.c,v 1.2 1999/02/26 05:32:08 art Exp $	*/
/*	$NetBSD: uvm_pglist.c,v 1.6 1998/08/13 02:11:03 eeh Exp $	*/

#define VM_PAGE_ALLOC_MEMORY_STATS
 
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
 *      This product includes software developed by the NetBSD
 *      Foundation, Inc. and its contributors.
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
 * uvm_pglist.c: pglist functions
 *
 * XXX: was part of uvm_page but has an incompatable copyright so it
 * gets its own file now.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/proc.h>

#include <vm/vm.h>
#include <vm/vm_page.h>
#include <vm/vm_kern.h>

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
uvm_pglistalloc(size, low, high, alignment, boundary, rlist, nsegs, waitok)
	psize_t size;
	paddr_t low, high, alignment, boundary;
	struct pglist *rlist;
	int nsegs, waitok;
{
	paddr_t try, idxpa, lastidxpa;
	int psi;
	struct vm_page *pgs;
	int s, tryidx, idx, end, error, free_list;
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
	uvm_lock_fpageq();            /* lock free page queue */

	/* Are there even any free pages? */
	for (idx = 0; idx < VM_NFREELIST; idx++)
		if (uvm.page_free[idx].tqh_first != NULL)
			break;
	if (idx == VM_NFREELIST)
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

		if ((psi = vm_physseg_find(atop(try), &idx)) == -1)
			continue; /* managed? */
		if (vm_physseg_find(atop(try + size), NULL) != psi)
			continue; /* end must be in this segment */

		tryidx = idx;
		end = idx + (size / PAGE_SIZE);
		pgs = vm_physmem[psi].pgs;

		/*
		 * Found a suitable starting page.  See of the range is free.
		 */
		for (; idx < end; idx++) {
			if (VM_PAGE_IS_FREE(&pgs[idx]) == 0) {
				/*
				 * Page not available.
				 */
				break;
			}

			idxpa = VM_PAGE_TO_PHYS(&pgs[idx]);

			if (idx > tryidx) {
				lastidxpa = VM_PAGE_TO_PHYS(&pgs[idx - 1]);

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
	 * we have a chunk of memory that conforms to the requested constraints.
	 */
	idx = tryidx;
	while (idx < end) {
		m = &pgs[idx];
		free_list = uvm_page_lookup_freelist(m);
#ifdef DEBUG
		for (tp = uvm.page_free[free_list].tqh_first;
		     tp != NULL; tp = tp->pageq.tqe_next) {
			if (tp == m)
				break;
		}
		if (tp == NULL)
			panic("uvm_pglistalloc: page not on freelist");
#endif
		TAILQ_REMOVE(&uvm.page_free[free_list], m, pageq);
		uvmexp.free--;
		m->flags = PG_CLEAN;
		m->pqflags = 0;
		m->uobject = NULL;
		m->uanon = NULL;
		m->wire_count = 0;
		m->loan_count = 0;
		TAILQ_INSERT_TAIL(rlist, m, pageq);
		idx++;
		STAT_INCR(uvm_pglistalloc_npages);
	}
	error = 0;

out:
	uvm_unlock_fpageq();
	splx(s);

	/*
	 * check to see if we need to generate some free pages waking
	 * the pagedaemon.
	 * XXX: we read uvm.free without locking
	 */
	 
	if (uvmexp.free < uvmexp.freemin ||
	    (uvmexp.free < uvmexp.freetarg &&
	    uvmexp.inactive < uvmexp.inactarg)) 
		thread_wakeup(&uvm.pagedaemon);

	return (error);
}

/*
 * uvm_pglistfree: free a list of pages
 *
 * => pages should already be unmapped
 */

void
uvm_pglistfree(list)
	struct pglist *list;
{
	vm_page_t m;
	int s;

	/*
	 * Block all memory allocation and lock the free list.
	 */
	s = splimp();
	uvm_lock_fpageq();

	while ((m = list->tqh_first) != NULL) {
#ifdef DIAGNOSTIC
		if (m->pqflags & (PQ_ACTIVE|PQ_INACTIVE))
			panic("uvm_pglistfree: active/inactive page!");
#endif
		TAILQ_REMOVE(list, m, pageq);
		m->pqflags = PQ_FREE;
		TAILQ_INSERT_TAIL(&uvm.page_free[uvm_page_lookup_freelist(m)],
		    m, pageq);
		uvmexp.free++;
		STAT_DECR(uvm_pglistalloc_npages);
	}

	uvm_unlock_fpageq();
	splx(s);
}
