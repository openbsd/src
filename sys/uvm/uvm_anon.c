/*	$NetBSD: uvm_anon.c,v 1.1 1999/01/24 23:53:15 chuck Exp $	*/

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

#include <vm/vm.h>
#include <vm/vm_page.h>
#include <vm/vm_kern.h>

#include <uvm/uvm.h>
#include <uvm/uvm_swap.h>

/*
 * allocate anons
 */
void
uvm_anon_init()
{
	struct vm_anon *anon;
	int nanon = uvmexp.free - (uvmexp.free / 16); /* XXXCDC ??? */
	int lcv;

	/*
	 * Allocate the initial anons.
	 */
	anon = (struct vm_anon *)uvm_km_alloc(kernel_map,
	    sizeof(*anon) * nanon);
	if (anon == NULL) {
		printf("uvm_anon_init: can not allocate %d anons\n", nanon);
		panic("uvm_anon_init");
	}

	bzero(anon, sizeof(*anon) * nanon);
	uvm.afree = NULL;
	uvmexp.nanon = uvmexp.nfreeanon = nanon;
	for (lcv = 0 ; lcv < nanon ; lcv++) {
		anon[lcv].u.an_nxt = uvm.afree;
		uvm.afree = &anon[lcv];
	}
	simple_lock_init(&uvm.afreelock);
}

/*
 * add some more anons to the free pool.  called when we add
 * more swap space.
 */
void
uvm_anon_add(pages)
	int	pages;
{
	struct vm_anon *anon;
	int lcv;

	anon = (struct vm_anon *)uvm_km_alloc(kernel_map,
	    sizeof(*anon) * pages);

	/* XXX Should wait for VM to free up. */
	if (anon == NULL) {
		printf("uvm_anon_add: can not allocate %d anons\n", pages);
		panic("uvm_anon_add");
	}

	simple_lock(&uvm.afreelock);
	bzero(anon, sizeof(*anon) * pages);
	uvmexp.nanon += pages;
	uvmexp.nfreeanon += pages;
	for (lcv = 0; lcv < pages; lcv++) {
		simple_lock_init(&anon->an_lock);
		anon[lcv].u.an_nxt = uvm.afree;
		uvm.afree = &anon[lcv];
	}
	simple_unlock(&uvm.afreelock);
}

/*
 * allocate an anon
 */
struct vm_anon *
uvm_analloc()
{
	struct vm_anon *a;

	simple_lock(&uvm.afreelock);
	a = uvm.afree;
	if (a) {
		uvm.afree = a->u.an_nxt;
		uvmexp.nfreeanon--;
		a->an_ref = 1;
		a->an_swslot = 0;
		a->u.an_page = NULL;		/* so we can free quickly */
	}
	simple_unlock(&uvm.afreelock);
	return(a);
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
uvm_anfree(anon)
	struct vm_anon *anon;
{
	struct vm_page *pg;
	UVMHIST_FUNC("uvm_anfree"); UVMHIST_CALLED(maphist);
	UVMHIST_LOG(maphist,"(anon=0x%x)", anon, 0,0,0);

	/*
	 * get page
	 */

	pg = anon->u.an_page;

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

			/* kill loan */
			uvm_lock_pageq();
#ifdef DIAGNOSTIC
			if (pg->loan_count < 1)
				panic("uvm_anfree: obj owned page "
				      "with no loan count");
#endif
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

			if ((pg->flags & PG_BUSY) != 0) {
				/* tell them to dump it when done */
				pg->flags |= PG_RELEASED;
				simple_unlock(&anon->an_lock);
				UVMHIST_LOG(maphist,
				    "  anon 0x%x, page 0x%x: BUSY (released!)", 
				    anon, pg, 0, 0);
				return;
			} 

			pmap_page_protect(PMAP_PGARG(pg), VM_PROT_NONE);
			uvm_lock_pageq();	/* lock out pagedaemon */
			uvm_pagefree(pg);	/* bye bye */
			uvm_unlock_pageq();	/* free the daemon */

			UVMHIST_LOG(maphist,"  anon 0x%x, page 0x%x: freed now!", 
			    anon, pg, 0, 0);
		}
	}

	/*
	 * are we using any backing store resources?   if so, free them.
	 */
	if (anon->an_swslot) {
		/*
		 * on backing store: no I/O in progress.  sole amap reference
		 * is ours and we've got it locked down.   thus we can free,
		 * and be done.
		 */
		UVMHIST_LOG(maphist,"  freeing anon 0x%x, paged to swslot 0x%x",
		    anon, anon->an_swslot, 0, 0);
		uvm_swap_free(anon->an_swslot, 1);
		anon->an_swslot = 0;
	} 

	/*
	 * now that we've stripped the data areas from the anon, free the anon
	 * itself!
	 */
	simple_lock(&uvm.afreelock);
	anon->u.an_nxt = uvm.afree;
	uvm.afree = anon;
	uvmexp.nfreeanon++;
	simple_unlock(&uvm.afreelock);
	UVMHIST_LOG(maphist,"<- done!",0,0,0,0);
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
uvm_anon_lockloanpg(anon)
	struct vm_anon *anon;
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

	while (((pg = anon->u.an_page) != NULL) && pg->loan_count != 0) {

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
				continue;		/* start over */
			}
		}

		/*
		 * if page is un-owned [i.e. the object dropped its ownership],
		 * then we can take over as owner!
		 */

		if (pg->uobject == NULL && (pg->pqflags & PQ_ANON) == 0) {
			uvm_lock_pageq();
			pg->pqflags |= PQ_ANON;		/* take ownership... */
			pg->loan_count--;	/* ... and drop our loan */
			uvm_unlock_pageq();
		}

		/*
		 * we did it!   break the loop
		 */
		break;
	}

	/*
	 * done!
	 */

	return(pg);
}
