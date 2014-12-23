/*	$OpenBSD: uvm_anon.c,v 1.43 2014/12/23 04:56:47 tedu Exp $	*/
/*	$NetBSD: uvm_anon.c,v 1.10 2000/11/25 06:27:59 chs Exp $	*/

/*
 * Copyright (c) 1997 Charles D. Cranor and Washington University.
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * uvm_anon.c: uvm anon ops
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/pool.h>
#include <sys/kernel.h>
#include <sys/atomic.h>

#include <uvm/uvm.h>
#include <uvm/uvm_swap.h>

struct pool uvm_anon_pool;

/*
 * allocate anons
 */
void
uvm_anon_init(void)
{
	pool_init(&uvm_anon_pool, sizeof(struct vm_anon), 0, 0,
	    PR_WAITOK, "anonpl", NULL);
	pool_sethiwat(&uvm_anon_pool, uvmexp.free / 16);
}

/*
 * allocate an anon
 */
struct vm_anon *
uvm_analloc(void)
{
	struct vm_anon *anon;

	anon = pool_get(&uvm_anon_pool, PR_NOWAIT);
	if (anon) {
		anon->an_ref = 1;
		anon->an_page = NULL;
		anon->an_swslot = 0;
	}
	return(anon);
}

/*
 * uvm_anfree: free a single anon structure
 *
 * => caller must remove anon from its amap before calling (if it was in
 *	an amap).
 * => we may lock the pageq's.
 */
void
uvm_anfree(struct vm_anon *anon)
{
	struct vm_page *pg;

	/* get page */
	pg = anon->an_page;

	/*
	 * if there is a resident page and it is loaned, then anon may not
	 * own it.   call out to uvm_anon_lockpage() to ensure the real owner
 	 * of the page has been identified and locked.
	 */
	if (pg && pg->loan_count)
		pg = uvm_anon_lockloanpg(anon);

	/*
	 * if we have a resident page, we must dispose of it before freeing
	 * the anon.
	 */
	if (pg) {
		/*
		 * if the page is owned by a uobject, then we must 
		 * kill the loan on the page rather than free it.
		 */
		if (pg->uobject) {
			uvm_lock_pageq();
			KASSERT(pg->loan_count > 0);
			pg->loan_count--;
			pg->uanon = NULL;
			uvm_unlock_pageq();
		} else {
			/*
			 * page has no uobject, so we must be the owner of it.
			 *
			 * if page is busy then we just mark it as released
			 * (who ever has it busy must check for this when they
			 * wake up).    if the page is not busy then we can
			 * free it now.
			 */
			if ((pg->pg_flags & PG_BUSY) != 0) {
				/* tell them to dump it when done */
				atomic_setbits_int(&pg->pg_flags, PG_RELEASED);
				return;
			} 
			pmap_page_protect(pg, PROT_NONE);
			uvm_lock_pageq();	/* lock out pagedaemon */
			uvm_pagefree(pg);	/* bye bye */
			uvm_unlock_pageq();	/* free the daemon */
		}
	}
	if (pg == NULL && anon->an_swslot != 0) {
		/* this page is no longer only in swap. */
		KASSERT(uvmexp.swpgonly > 0);
		uvmexp.swpgonly--;
	}

	/* free any swap resources. */
	uvm_anon_dropswap(anon);

	/*
	 * now that we've stripped the data areas from the anon, free the anon
	 * itself!
	 */
	KASSERT(anon->an_page == NULL);
	KASSERT(anon->an_swslot == 0);

	pool_put(&uvm_anon_pool, anon);
}

/*
 * uvm_anon_dropswap:  release any swap resources from this anon.
 */
void
uvm_anon_dropswap(struct vm_anon *anon)
{

	if (anon->an_swslot == 0)
		return;

	uvm_swap_free(anon->an_swslot, 1);
	anon->an_swslot = 0;
}

/*
 * uvm_anon_lockloanpg: given a locked anon, lock its resident page
 *
 * => on return:
 *		 if there is a resident page:
 *			if it is ownerless, we take over as owner
 *		 we return the resident page (it can change during
 *		 this function)
 * => note that the only time an anon has an ownerless resident page
 *	is if the page was loaned from a uvm_object and the uvm_object
 *	disowned it
 * => this only needs to be called when you want to do an operation
 *	on an anon's resident page and that page has a non-zero loan
 *	count.
 */
struct vm_page *
uvm_anon_lockloanpg(struct vm_anon *anon)
{
	struct vm_page *pg;

	/*
	 * loop while we have a resident page that has a non-zero loan count.
	 * if we successfully get our lock, we will "break" the loop.
	 * note that the test for pg->loan_count is not protected -- this
	 * may produce false positive results.   note that a false positive
	 * result may cause us to do more work than we need to, but it will
	 * not produce an incorrect result.
	 */
	while (((pg = anon->an_page) != NULL) && pg->loan_count != 0) {
		/*
		 * if page is un-owned [i.e. the object dropped its ownership],
		 * then we can take over as owner!
		 */
		if (pg->uobject == NULL && (pg->pg_flags & PQ_ANON) == 0) {
			uvm_lock_pageq();
			atomic_setbits_int(&pg->pg_flags, PQ_ANON);
			pg->loan_count--;	/* ... and drop our loan */
			uvm_unlock_pageq();
		}
		break;
	}
	return(pg);
}

/*
 * fetch an anon's page.
 *
 * => returns TRUE if pagein was aborted due to lack of memory.
 */

boolean_t
uvm_anon_pagein(struct vm_anon *anon)
{
	struct vm_page *pg;
	struct uvm_object *uobj;
	int rv;

	rv = uvmfault_anonget(NULL, NULL, anon);

	switch (rv) {
	case VM_PAGER_OK:
		break;
	case VM_PAGER_ERROR:
	case VM_PAGER_REFAULT:
		/*
		 * nothing more to do on errors.
		 * VM_PAGER_REFAULT can only mean that the anon was freed,
		 * so again there's nothing to do.
		 */
		return FALSE;
	default:
#ifdef DIAGNOSTIC
		panic("anon_pagein: uvmfault_anonget -> %d", rv);
#else
		return FALSE;
#endif
	}

	/*
	 * ok, we've got the page now.
	 * mark it as dirty, clear its swslot and un-busy it.
	 */
	pg = anon->an_page;
	uobj = pg->uobject;
	uvm_swap_free(anon->an_swslot, 1);
	anon->an_swslot = 0;
	atomic_clearbits_int(&pg->pg_flags, PG_CLEAN);

	/* deactivate the page (to put it on a page queue) */
	pmap_clear_reference(pg);
	pmap_page_protect(pg, PROT_NONE);
	uvm_lock_pageq();
	uvm_pagedeactivate(pg);
	uvm_unlock_pageq();

	return FALSE;
}
