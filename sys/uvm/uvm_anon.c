/*	$OpenBSD: uvm_anon.c,v 1.16 2001/11/27 05:27:12 art Exp $	*/
/*	$NetBSD: uvm_anon.c,v 1.15 2001/02/18 21:19:08 chs Exp $	*/

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

/*
 * anonblock_list: global list of anon blocks,
 * locked by swap_syscall_lock (since we never remove
 * anything from this list and we only add to it via swapctl(2)).
 */

struct uvm_anonblock {
	LIST_ENTRY(uvm_anonblock) list;
	int count;
	struct vm_anon *anons;
};
static LIST_HEAD(anonlist, uvm_anonblock) anonblock_list;


static boolean_t anon_pagein __P((struct vm_anon *));


/*
 * allocate anons
 */
void
uvm_anon_init()
{
	int nanon = uvmexp.free - (uvmexp.free / 16); /* XXXCDC ??? */

	simple_lock_init(&uvm.afreelock);
	LIST_INIT(&anonblock_list);

	/*
	 * Allocate the initial anons.
	 */
	uvm_anon_add(nanon);
}

/*
 * add some more anons to the free pool.  called when we add
 * more swap space.
 *
 * => swap_syscall_lock should be held (protects anonblock_list).
 */
int
uvm_anon_add(count)
	int	count;
{
	struct uvm_anonblock *anonblock;
	struct vm_anon *anon;
	int lcv, needed;

	simple_lock(&uvm.afreelock);
	uvmexp.nanonneeded += count;
	needed = uvmexp.nanonneeded - uvmexp.nanon;
	simple_unlock(&uvm.afreelock);

	if (needed <= 0) {
		return 0;
	}
	anon = (void *)uvm_km_alloc(kernel_map, sizeof(*anon) * needed);
	if (anon == NULL) {
		simple_lock(&uvm.afreelock);
		uvmexp.nanonneeded -= count;
		simple_unlock(&uvm.afreelock);
		return ENOMEM;
	}
	MALLOC(anonblock, void *, sizeof(*anonblock), M_UVMAMAP, M_WAITOK);

	anonblock->count = needed;
	anonblock->anons = anon;
	LIST_INSERT_HEAD(&anonblock_list, anonblock, list);
	memset(anon, 0, sizeof(*anon) * needed);
 
	simple_lock(&uvm.afreelock);
	uvmexp.nanon += needed;
	uvmexp.nfreeanon += needed;
	for (lcv = 0; lcv < needed; lcv++) {
		simple_lock_init(&anon->an_lock);
		anon[lcv].u.an_nxt = uvm.afree;
		uvm.afree = &anon[lcv];
		simple_lock_init(&uvm.afree->an_lock);
	}
	simple_unlock(&uvm.afreelock);
	return 0;
}

/*
 * remove anons from the free pool.
 */
void
uvm_anon_remove(count)
	int count;
{
	/*
	 * we never actually free any anons, to avoid allocation overhead.
	 * XXX someday we might want to try to free anons.
	 */

	simple_lock(&uvm.afreelock);
	uvmexp.nanonneeded -= count;
	simple_unlock(&uvm.afreelock);
}

/*
 * allocate an anon
 *
 * => new anon is returned locked!
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
		LOCK_ASSERT(simple_lock_held(&a->an_lock) == 0);
		simple_lock(&a->an_lock);
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

	KASSERT(anon->an_ref == 0);
	LOCK_ASSERT(simple_lock_held(&anon->an_lock) == 0);

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

			if ((pg->flags & PG_BUSY) != 0) {
				/* tell them to dump it when done */
				pg->flags |= PG_RELEASED;
				UVMHIST_LOG(maphist,
				    "  anon 0x%x, page 0x%x: BUSY (released!)", 
				    anon, pg, 0, 0);
				return;
			} 
			pmap_page_protect(pg, VM_PROT_NONE);
			uvm_lock_pageq();	/* lock out pagedaemon */
			uvm_pagefree(pg);	/* bye bye */
			uvm_unlock_pageq();	/* free the daemon */
			UVMHIST_LOG(maphist,"anon 0x%x, page 0x%x: freed now!",
			    anon, pg, 0, 0);
		}
	}

	/*
	 * free any swap resources.
	 */
	uvm_anon_dropswap(anon);

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
 * uvm_anon_dropswap:  release any swap resources from this anon.
 * 
 * => anon must be locked or have a reference count of 0.
 */
void
uvm_anon_dropswap(anon)
	struct vm_anon *anon;
{
	UVMHIST_FUNC("uvm_anon_dropswap"); UVMHIST_CALLED(maphist);

	if (anon->an_swslot == 0)
		return;

	UVMHIST_LOG(maphist,"freeing swap for anon %p, paged to swslot 0x%x",
		    anon, anon->an_swslot, 0, 0);
	uvm_swap_free(anon->an_swslot, 1);
	anon->an_swslot = 0;

	if (anon->u.an_page == NULL) {
		/* this page is no longer only in swap. */
		simple_lock(&uvm.swap_data_lock);
		uvmexp.swpgonly--;
		simple_unlock(&uvm.swap_data_lock);
	} 
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

	LOCK_ASSERT(simple_lock_held(&anon->an_lock));

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
				continue;
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
	return(pg);
}



/*
 * page in every anon that is paged out to a range of swslots.
 * 
 * swap_syscall_lock should be held (protects anonblock_list).
 */

boolean_t
anon_swap_off(startslot, endslot)
	int startslot, endslot;
{
	struct uvm_anonblock *anonblock;

	for (anonblock = LIST_FIRST(&anonblock_list);
	     anonblock != NULL;
	     anonblock = LIST_NEXT(anonblock, list)) {
		int i;

		/*
		 * loop thru all the anons in the anonblock,
		 * paging in where needed.
		 */

		for (i = 0; i < anonblock->count; i++) {
			struct vm_anon *anon = &anonblock->anons[i];
			int slot;

			/*
			 * lock anon to work on it.
			 */

			simple_lock(&anon->an_lock);

			/*
			 * is this anon's swap slot in range?
			 */

			slot = anon->an_swslot;
			if (slot >= startslot && slot < endslot) {
				boolean_t rv;

				/*
				 * yup, page it in.
				 */

				/* locked: anon */
				rv = anon_pagein(anon);
				/* unlocked: anon */

				if (rv) {
					return rv;
				}
			} else {

				/*
				 * nope, unlock and proceed.
				 */

				simple_unlock(&anon->an_lock);
			}
		}
	}
	return FALSE;
}


/*
 * fetch an anon's page.
 *
 * => anon must be locked, and is unlocked upon return.
 * => returns TRUE if pagein was aborted due to lack of memory.
 */

static boolean_t
anon_pagein(anon)
	struct vm_anon *anon;
{
	struct vm_page *pg;
	struct uvm_object *uobj;
	int rv;

	/* locked: anon */
	LOCK_ASSERT(simple_lock_held(&anon->an_lock));

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
	}

	/*
	 * ok, we've got the page now.
	 * mark it as dirty, clear its swslot and un-busy it.
	 */

	pg = anon->u.an_page;
	uobj = pg->uobject;
	uvm_swap_free(anon->an_swslot, 1);
	anon->an_swslot = 0;
	pg->flags &= ~(PG_CLEAN);

	/*
	 * deactivate the page (to put it on a page queue)
	 */

	pmap_clear_reference(pg);
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
