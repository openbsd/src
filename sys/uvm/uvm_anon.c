/*	$OpenBSD: uvm_anon.c,v 1.34 2009/06/16 23:54:57 oga Exp $	*/
/*	$NetBSD: uvm_anon.c,v 1.10 2000/11/25 06:27:59 chs Exp $	*/

/*
 *
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Charles D. Cranor and
 *      Washington University.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/pool.h>
#include <sys/kernel.h>

#include <uvm/uvm.h>
#include <uvm/uvm_swap.h>

struct pool uvm_anon_pool;

/*
 * allocate anons
 */
void
uvm_anon_init(void)
{
	pool_init(&uvm_anon_pool, sizeof(struct vm_anon), 0, 0, 0, "anonpl",
	    &pool_allocator_nointr);
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
		simple_lock_init(&anon->an_lock);
		anon->an_ref = 1;
		anon->an_page = NULL;
		anon->an_swslot = 0;
		simple_lock(&anon->an_lock);
	}
	return(anon);
}

/*
 * uvm_anfree: free a single anon structure
 *
 * => caller must remove anon from its amap before calling (if it was in
 *	an amap).
 * => anon must be unlocked and have a zero reference count.
 * => we may lock the pageq's.
 */
void
uvm_anfree(struct vm_anon *anon)
{
	struct vm_page *pg;
	UVMHIST_FUNC("uvm_anfree"); UVMHIST_CALLED(maphist);
	UVMHIST_LOG(maphist,"(anon=%p)", anon, 0,0,0);

	/*
	 * get page
	 */

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
		 * if the page is owned by a uobject (now locked), then we must 
		 * kill the loan on the page rather than free it.
		 */

		if (pg->uobject) {
			uvm_lock_pageq();
			KASSERT(pg->loan_count > 0);
			pg->loan_count--;
			pg->uanon = NULL;
			uvm_unlock_pageq();
			simple_unlock(&pg->uobject->vmobjlock);
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
				UVMHIST_LOG(maphist,
				    "  anon %p, page %p: BUSY (released!)", 
				    anon, pg, 0, 0);
				return;
			} 
			pmap_page_protect(pg, VM_PROT_NONE);
			uvm_lock_pageq();	/* lock out pagedaemon */
			uvm_pagefree(pg);	/* bye bye */
			uvm_unlock_pageq();	/* free the daemon */
			UVMHIST_LOG(maphist,"anon %p, page %p: freed now!",
			    anon, pg, 0, 0);
		}
	}
	if (pg == NULL && anon->an_swslot != 0) {
		/* this page is no longer only in swap. */
		simple_lock(&uvm.swap_data_lock);
		KASSERT(uvmexp.swpgonly > 0);
		uvmexp.swpgonly--;
		simple_unlock(&uvm.swap_data_lock);
	}

	/*
	 * free any swap resources.
	 */
	uvm_anon_dropswap(anon);

	/*
	 * now that we've stripped the data areas from the anon, free the anon
	 * itself!
	 */
	KASSERT(anon->an_page == NULL);
	KASSERT(anon->an_swslot == 0);

	pool_put(&uvm_anon_pool, anon);
	UVMHIST_LOG(maphist,"<- done!",0,0,0,0);
}

/*
 * uvm_anon_dropswap:  release any swap resources from this anon.
 * 
 * => anon must be locked or have a reference count of 0.
 */
void
uvm_anon_dropswap(struct vm_anon *anon)
{
	UVMHIST_FUNC("uvm_anon_dropswap"); UVMHIST_CALLED(maphist);

	if (anon->an_swslot == 0)
		return;

	UVMHIST_LOG(maphist,"freeing swap for anon %p, paged to swslot 0x%lx",
		    anon, anon->an_swslot, 0, 0);
	uvm_swap_free(anon->an_swslot, 1);
	anon->an_swslot = 0;
}

/*
 * uvm_anon_lockloanpg: given a locked anon, lock its resident page
 *
 * => anon is locked by caller
 * => on return: anon is locked
 *		 if there is a resident page:
 *			if it has a uobject, it is locked by us
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
	boolean_t locked = FALSE;

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
		 * quickly check to see if the page has an object before
		 * bothering to lock the page queues.   this may also produce
		 * a false positive result, but that's ok because we do a real
		 * check after that.
		 *
		 * XXX: quick check -- worth it?   need volatile?
		 */

		if (pg->uobject) {

			uvm_lock_pageq();
			if (pg->uobject) {	/* the "real" check */
				locked =
				    simple_lock_try(&pg->uobject->vmobjlock);
			} else {
				/* object disowned before we got PQ lock */
				locked = TRUE;
			}
			uvm_unlock_pageq();

			/*
			 * if we didn't get a lock (try lock failed), then we
			 * toggle our anon lock and try again
			 */

			if (!locked) {
				simple_unlock(&anon->an_lock);

				/*
				 * someone locking the object has a chance to
				 * lock us right now
				 */

				simple_lock(&anon->an_lock);
				continue;
			}
		}

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

		/*
		 * we did it!   break the loop
		 */

		break;
	}
	return(pg);
}

/*
 * fetch an anon's page.
 *
 * => anon must be locked, and is unlocked upon return.
 * => returns TRUE if pagein was aborted due to lack of memory.
 */

boolean_t
uvm_anon_pagein(struct vm_anon *anon)
{
	struct vm_page *pg;
	struct uvm_object *uobj;
	int rv;

	/* locked: anon */
	rv = uvmfault_anonget(NULL, NULL, anon);
	/*
	 * if rv == VM_PAGER_OK, anon is still locked, else anon
	 * is unlocked
	 */

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

	/*
	 * deactivate the page (to put it on a page queue)
	 */

	pmap_clear_reference(pg);
	pmap_page_protect(pg, VM_PROT_NONE);
	uvm_lock_pageq();
	uvm_pagedeactivate(pg);
	uvm_unlock_pageq();

	/*
	 * unlock the anon and we're done.
	 */

	simple_unlock(&anon->an_lock);
	if (uobj) {
		simple_unlock(&uobj->vmobjlock);
	}
	return FALSE;
}
