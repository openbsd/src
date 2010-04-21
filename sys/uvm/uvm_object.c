/*	$OpenBSD: uvm_object.c,v 1.4 2010/04/21 03:04:49 deraadt Exp $	*/

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
#include <sys/proc.h>		/* XXX for atomic */

#include <uvm/uvm.h>

/* We will fetch this page count per step */
#define	FETCH_PAGECOUNT	16

/*
 * uvm_objwire: wire the pages of entire uobj
 *
 * => caller must pass page-aligned start and end values
 * => if the caller passes in a pageq pointer, we'll return a list of
 *  wired pages.
 */

int
uvm_objwire(struct uvm_object *uobj, off_t start, off_t end,
    struct pglist *pageq)
{
	int i, npages, error;
	struct vm_page *pgs[FETCH_PAGECOUNT];
	off_t offset = start, left;

	left = (end - start) >> PAGE_SHIFT;

	simple_lock(&uobj->vmobjlock);
	while (left) {

		npages = MIN(FETCH_PAGECOUNT, left);

		/* Get the pages */
		memset(pgs, 0, sizeof(pgs));
		error = (*uobj->pgops->pgo_get)(uobj, offset, pgs, &npages, 0,
			VM_PROT_READ | VM_PROT_WRITE, UVM_ADV_SEQUENTIAL,
			PGO_ALLPAGES | PGO_SYNCIO);

		if (error)
			goto error;

		simple_lock(&uobj->vmobjlock);
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
						simple_unlock(&uobj->vmobjlock);
						uvm_wait("uobjwirepg");
						simple_lock(&uobj->vmobjlock);
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
		offset += npages << PAGE_SHIFT;
	}
	simple_unlock(&uobj->vmobjlock);

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
uvm_objunwire(struct uvm_object *uobj, off_t start, off_t end)
{
	struct vm_page *pg;
	off_t offset;

	simple_lock(&uobj->vmobjlock);
	uvm_lock_pageq();
	for (offset = start; offset < end; offset += PAGE_SIZE) {
		pg = uvm_pagelookup(uobj, offset);

		KASSERT(pg != NULL);
		KASSERT(!(pg->pg_flags & PG_RELEASED));

		uvm_pageunwire(pg);
	}
	uvm_unlock_pageq();
	simple_unlock(&uobj->vmobjlock);
}
