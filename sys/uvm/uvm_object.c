/*	$OpenBSD: uvm_object.c,v 1.11 2014/12/17 06:58:11 guenther Exp $	*/

/*
 * Copyright (c) 2006 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Mindaugas Rasiukevicius.
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
 * uvm_object.c: operate with memory objects
 *
 */

#include <sys/param.h>
#include <sys/mman.h>

#include <uvm/uvm.h>

/* We will fetch this page count per step */
#define	FETCH_PAGECOUNT	16

/*
 * uvm_objinit: initialise a uvm object.
 */
void
uvm_objinit(struct uvm_object *uobj, struct uvm_pagerops *pgops, int refs)
{
	uobj->pgops = pgops;
	RB_INIT(&uobj->memt);
	uobj->uo_npages = 0;
	uobj->uo_refs = refs;
}

#ifndef SMALL_KERNEL
/*
 * uvm_objwire: wire the pages of entire uobj
 *
 * => caller must pass page-aligned start and end values
 * => if the caller passes in a pageq pointer, we'll return a list of
 *  wired pages.
 */

int
uvm_objwire(struct uvm_object *uobj, voff_t start, voff_t end,
    struct pglist *pageq)
{
	int i, npages, left, error;
	struct vm_page *pgs[FETCH_PAGECOUNT];
	voff_t offset = start;

	left = (end - start) >> PAGE_SHIFT;

	while (left) {

		npages = MIN(FETCH_PAGECOUNT, left);

		/* Get the pages */
		memset(pgs, 0, sizeof(pgs));
		error = (*uobj->pgops->pgo_get)(uobj, offset, pgs, &npages, 0,
			PROT_READ | PROT_WRITE, MADV_SEQUENTIAL,
			PGO_ALLPAGES | PGO_SYNCIO);

		if (error)
			goto error;

		for (i = 0; i < npages; i++) {

			KASSERT(pgs[i] != NULL);
			KASSERT(!(pgs[i]->pg_flags & PG_RELEASED));

#if 0
			/*
			 * Loan break
			 */
			if (pgs[i]->loan_count) {
				while (pgs[i]->loan_count) {
					pg = uvm_loanbreak(pgs[i]);
					if (!pg) {
						uvm_wait("uobjwirepg");
						continue;
					}
				}
				pgs[i] = pg;
			}
#endif

			if (pgs[i]->pg_flags & PQ_AOBJ) {
				atomic_clearbits_int(&pgs[i]->pg_flags,
				    PG_CLEAN);
				uao_dropswap(uobj, i);
			}
		}

		/* Wire the pages */
		uvm_lock_pageq();
		for (i = 0; i < npages; i++) {
			uvm_pagewire(pgs[i]);
			if (pageq != NULL)
				TAILQ_INSERT_TAIL(pageq, pgs[i], pageq);
		}
		uvm_unlock_pageq();

		/* Unbusy the pages */
		uvm_page_unbusy(pgs, npages);

		left -= npages;
		offset += (voff_t)npages << PAGE_SHIFT;
	}

	return 0;

error:
	/* Unwire the pages which have been wired */
	uvm_objunwire(uobj, start, offset);

	return error;
}

/*
 * uobj_unwirepages: unwire the pages of entire uobj
 *
 * => caller must pass page-aligned start and end values
 */

void
uvm_objunwire(struct uvm_object *uobj, voff_t start, voff_t end)
{
	struct vm_page *pg;
	off_t offset;

	uvm_lock_pageq();
	for (offset = start; offset < end; offset += PAGE_SIZE) {
		pg = uvm_pagelookup(uobj, offset);

		KASSERT(pg != NULL);
		KASSERT(!(pg->pg_flags & PG_RELEASED));

		uvm_pageunwire(pg);
	}
	uvm_unlock_pageq();
}
#endif /* !SMALL_KERNEL */
