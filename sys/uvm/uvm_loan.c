/*	$OpenBSD: uvm_loan.c,v 1.2 1999/02/26 05:32:07 art Exp $	*/
/*	$NetBSD: uvm_loan.c,v 1.13 1999/01/24 23:53:15 chuck Exp $	*/

/*
 * XXXCDC: "ROUGH DRAFT" QUALITY UVM PRE-RELEASE FILE!   
 *	   >>>USE AT YOUR OWN RISK, WORK IS NOT FINISHED<<<
 */
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
 *
 * from: Id: uvm_loan.c,v 1.1.6.4 1998/02/06 05:08:43 chs Exp
 */

/*
 * uvm_loan.c: page loanout handler
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/mman.h>

#include <vm/vm.h>
#include <vm/vm_page.h>
#include <vm/vm_kern.h>

#include <uvm/uvm.h>

/*
 * "loaned" pages are pages which are (read-only, copy-on-write) loaned 
 * from the VM system to other parts of the kernel.   this allows page
 * copying to be avoided (e.g. you can loan pages from objs/anons to
 * the mbuf system).
 *
 * there are 3 types of loans possible:
 *  O->K  uvm_object page to wired kernel page (e.g. mbuf data area)
 *  A->K  anon page to kernel wired kernel page (e.g. mbuf data area)
 *  O->A  uvm_object to anon loan (e.g. vnode page to an anon)
 * note that it possible to have an O page loaned to both an A and K
 * at the same time.
 *
 * loans are tracked by pg->loan_count.  an O->A page will have both
 * a uvm_object and a vm_anon, but PQ_ANON will not be set.   this sort
 * of page is considered "owned" by the uvm_object (not the anon).
 *
 * each loan of a page to a wired kernel page bumps the pg->wire_count.
 * wired kernel mappings should be entered with pmap_kenter functions
 * so that pmap_page_protect() will not affect the kernel mappings.
 * (this requires the PMAP_NEW interface...).
 *
 * owners that want to free their pages and discover that they are 
 * loaned out simply "disown" them (the page becomes an orphan).  these
 * pages should be freed when the last loan is dropped.   in some cases
 * an anon may "adopt" an orphaned page.
 *
 * locking: to read pg->loan_count either the owner or the page queues
 * must be locked.   to modify pg->loan_count, both the owner of the page
 * and the PQs must be locked.   pg->flags is (as always) locked by
 * the owner of the page.
 *
 * note that locking from the "loaned" side is tricky since the object
 * getting the loaned page has no reference to the page's owner and thus
 * the owner could "die" at any time.   in order to prevent the owner
 * from dying the page queues should be locked.   this forces us to sometimes
 * use "try" locking.
 *
 * loans are typically broken by the following events:
 *  1. write fault to a loaned page 
 *  2. pageout of clean+inactive O->A loaned page
 *  3. owner frees page (e.g. pager flush)
 *
 * note that loaning a page causes all mappings of the page to become
 * read-only (via pmap_page_protect).   this could have an unexpected
 * effect on normal "wired" pages if one is not careful.
 */

/*
 * local prototypes
 */

static int	uvm_loananon __P((struct uvm_faultinfo *, void ***, 
				int, struct vm_anon *));
static int	uvm_loanentry __P((struct uvm_faultinfo *, void ***, int));
static int	uvm_loanuobj __P((struct uvm_faultinfo *, void ***, 
				int, vaddr_t));
static int	uvm_loanzero __P((struct uvm_faultinfo *, void ***, int));

/*
 * inlines
 */

/*
 * uvm_loanentry: loan out pages in a map entry (helper fn for uvm_loan())
 *
 * => "ufi" is the result of a successful map lookup (meaning that
 *	the maps are locked by the caller)
 * => we may unlock the maps if needed (for I/O)
 * => we put our output result in "output"
 * => we return the number of pages we loaned, or -1 if we had an error
 */

static __inline int
uvm_loanentry(ufi, output, flags)
	struct uvm_faultinfo *ufi;
	void ***output;
	int flags;
{
	vaddr_t curaddr = ufi->orig_rvaddr;
	vsize_t togo = ufi->size;
	struct vm_aref *aref = &ufi->entry->aref;
	struct uvm_object *uobj = ufi->entry->object.uvm_obj;
	struct vm_anon *anon;
	int rv, result = 0;

	/*
	 * lock us the rest of the way down
	 */
	if (aref->ar_amap)
		amap_lock(aref->ar_amap);
	if (uobj)
		simple_lock(&uobj->vmobjlock);

	/*
	 * loop until done
	 */
	while (togo) {

		/*
		 * find the page we want.   check the anon layer first.
		 */

		if (aref->ar_amap) {
			anon = amap_lookup(aref, curaddr - ufi->entry->start);
		} else {
			anon = NULL;
		}

		if (anon) {
			rv = uvm_loananon(ufi, output, flags, anon);
		} else if (uobj) {
			rv = uvm_loanuobj(ufi, output, flags, curaddr);
		} else if (UVM_ET_ISCOPYONWRITE(ufi->entry)) {
			rv = uvm_loanzero(ufi, output, flags);
		} else {
			rv = -1;		/* null map entry... fail now */
		}

		/* total failure */
		if (rv < 0)
			return(-1);

		/* relock failed, need to do another lookup */
		if (rv == 0)
			return(result);

		/*
		 * got it... advance to next page
		 */
		result++;
		togo -= PAGE_SIZE;
		curaddr += PAGE_SIZE;
	}

	/*
	 * unlock everything and return
	 */
	uvmfault_unlockall(ufi, aref->ar_amap, uobj, NULL);
	return(result);
}

/*
 * normal functions
 */

/*
 * uvm_loan: loan pages out to anons or to the kernel
 * 
 * => map should be unlocked
 * => start and len should be multiples of PAGE_SIZE
 * => result is either an array of anon's or vm_pages (depending on flags)
 * => flag values: UVM_LOAN_TOANON - loan to anons
 *                 UVM_LOAN_TOPAGE - loan to wired kernel page
 *    one and only one of these flags must be set!
 */

int
uvm_loan(map, start, len, result, flags)
	struct vm_map *map;
	vaddr_t start;
	vsize_t len;
	void **result;
	int flags;
{
	struct uvm_faultinfo ufi;
	void **output;
	int rv;

	/*
	 * ensure that one and only one of the flags is set
	 */

	if ((flags & (UVM_LOAN_TOANON|UVM_LOAN_TOPAGE)) == 
	    (UVM_LOAN_TOANON|UVM_LOAN_TOPAGE) ||
	    (flags & (UVM_LOAN_TOANON|UVM_LOAN_TOPAGE)) == 0)
		return(KERN_FAILURE);

	/*
	 * "output" is a pointer to the current place to put the loaned
	 * page...
	 */

	output = &result[0];	/* start at the beginning ... */

	/*
	 * while we've got pages to do
	 */

	while (len > 0) {

		/*
		 * fill in params for a call to uvmfault_lookup
		 */

		ufi.orig_map = map;
		ufi.orig_rvaddr = start;
		ufi.orig_size = len;
		
		/*
		 * do the lookup, the only time this will fail is if we hit on
		 * an unmapped region (an error)
		 */

		if (!uvmfault_lookup(&ufi, FALSE)) 
			goto fail;

		/*
		 * now do the loanout
		 */
		rv = uvm_loanentry(&ufi, &output, flags);
		if (rv < 0) 
			goto fail;

		/*
		 * done!   advance pointers and unlock.
		 */
		rv <<= PAGE_SHIFT;
		len -= rv;
		start += rv;
		uvmfault_unlockmaps(&ufi, FALSE);
	}
	
	/*
	 * got it!   return success.
	 */

	return(KERN_SUCCESS);

fail:
	/*
	 * fail: failed to do it.   drop our loans and return failure code.
	 */
	if (output - result) {
		if (flags & UVM_LOAN_TOANON)
			uvm_unloananon((struct vm_anon **)result,
			    output - result);
		else
			uvm_unloanpage((struct vm_page **)result,
			    output - result);
	}
	return(KERN_FAILURE);
}

/*
 * uvm_loananon: loan a page from an anon out
 * 
 * => return value:
 *	-1 = fatal error, everything is unlocked, abort.
 *	 0 = lookup in ufi went stale, everything unlocked, relookup and
 *		try again
 *	 1 = got it, everything still locked
 */

int
uvm_loananon(ufi, output, flags, anon)
	struct uvm_faultinfo *ufi;
	void ***output;
	int flags;
	struct vm_anon *anon;
{
	struct vm_page *pg;
	int result;

	/*
	 * if we are loaning to another anon then it is easy, we just
	 * bump the reference count on the current anon and return a
	 * pointer to it.
	 */
	if (flags & UVM_LOAN_TOANON) {
		simple_lock(&anon->an_lock);
		pg = anon->u.an_page;
		if (pg && (pg->pqflags & PQ_ANON) != 0 && anon->an_ref == 1)
			/* read protect it */
			pmap_page_protect(PMAP_PGARG(pg), VM_PROT_READ);
		anon->an_ref++;
		**output = anon;
		*output = (*output) + 1;
		simple_unlock(&anon->an_lock);
		return(1);
	}

	/*
	 * we are loaning to a kernel-page.   we need to get the page
	 * resident so we can wire it.   uvmfault_anonget will handle
	 * this for us.
	 */

	simple_lock(&anon->an_lock);
	result = uvmfault_anonget(ufi, ufi->entry->aref.ar_amap, anon);

	/*
	 * if we were unable to get the anon, then uvmfault_anonget has
	 * unlocked everything and returned an error code.
	 */

	if (result != VM_PAGER_OK) {

		/* need to refault (i.e. refresh our lookup) ? */
		if (result == VM_PAGER_REFAULT)
			return(0);

		/* "try again"?   sleep a bit and retry ... */
		if (result == VM_PAGER_AGAIN) {
			tsleep((caddr_t)&lbolt, PVM, "loanagain", 0);
			return(0);
		}

		/* otherwise flag it as an error */
		return(-1);
	}

	/*
	 * we have the page and its owner locked: do the loan now.
	 */

	pg = anon->u.an_page;
	uvm_lock_pageq();
	if (pg->loan_count == 0)
		pmap_page_protect(PMAP_PGARG(pg), VM_PROT_READ);
	pg->loan_count++;
	uvm_pagewire(pg);	/* always wire it */
	uvm_unlock_pageq();
	**output = pg;
	*output = (*output) + 1;

	/* unlock anon and return success */
	if (pg->uobject)
		simple_unlock(&pg->uobject->vmobjlock);
	simple_unlock(&anon->an_lock);
	return(1);
}

/*
 * uvm_loanuobj: loan a page from a uobj out
 *
 * => return value:
 *	-1 = fatal error, everything is unlocked, abort.
 *	 0 = lookup in ufi went stale, everything unlocked, relookup and
 *		try again
 *	 1 = got it, everything still locked
 */

int
uvm_loanuobj(ufi, output, flags, va)
	struct uvm_faultinfo *ufi;
	void ***output;
	int flags;
	vaddr_t va;
{
	struct vm_amap *amap = ufi->entry->aref.ar_amap;
	struct uvm_object *uobj = ufi->entry->object.uvm_obj;
	struct vm_page *pg;
	struct vm_anon *anon;
	int result, npages;
	boolean_t locked;

	/*
	 * first we must make sure the page is resident.
	 *
	 * XXXCDC: duplicate code with uvm_fault().
	 */

	if (uobj->pgops->pgo_get) {
		npages = 1;
		pg = NULL;
		result = uobj->pgops->pgo_get(uobj, va - ufi->entry->start,
		    &pg, &npages, 0, VM_PROT_READ, MADV_NORMAL, PGO_LOCKED);
	} else {
		result = VM_PAGER_ERROR;
	}

	/*
	 * check the result of the locked pgo_get.  if there is a problem,
	 * then we fail the loan.
	 */

	if (result != VM_PAGER_OK && result != VM_PAGER_UNLOCK) {
		uvmfault_unlockall(ufi, amap, uobj, NULL);
		return(-1);
	}

	/*
	 * if we need to unlock for I/O, do so now.
	 */

	if (result == VM_PAGER_UNLOCK) {
		uvmfault_unlockall(ufi, amap, NULL, NULL);
		
		npages = 1;
		/* locked: uobj */
		result = uobj->pgops->pgo_get(uobj, va - ufi->entry->start,
		    &pg, &npages, 0, VM_PROT_READ, MADV_NORMAL, 0);
		/* locked: <nothing> */
		
		/*
		 * check for errors
		 */

		if (result != VM_PAGER_OK) {
			 if (result == VM_PAGER_AGAIN) {
				tsleep((caddr_t)&lbolt, PVM, "fltagain2", 0);
				return(0); /* redo the lookup and try again */
			} 
			return(-1);	/* total failure */
		}

		/*
		 * pgo_get was a success.   attempt to relock everything.
		 */

		locked = uvmfault_relock(ufi);
		if (locked && amap)
			amap_lock(amap);
		simple_lock(&uobj->vmobjlock);

		/*
		 * verify that the page has not be released and re-verify
		 * that amap slot is still free.   if there is a problem we
		 * drop our lock (thus force a lookup refresh/retry).
		 */
			
		if ((pg->flags & PG_RELEASED) != 0 ||
		    (locked && amap && amap_lookup(&ufi->entry->aref,
		    ufi->orig_rvaddr - ufi->entry->start))) {
			
			if (locked)
				uvmfault_unlockall(ufi, amap, NULL, NULL);
			locked = FALSE;
		} 

		/*
		 * didn't get the lock?   release the page and retry.
		 */

		if (locked == FALSE) {

			if (pg->flags & PG_WANTED)
				/* still holding object lock */
				thread_wakeup(pg);

			if (pg->flags & PG_RELEASED) {
#ifdef DIAGNOSTIC
				if (uobj->pgops->pgo_releasepg == NULL)
			panic("uvm_loanuobj: object has no releasepg function");
#endif
				/* frees page */
				if (uobj->pgops->pgo_releasepg(pg, NULL))
					simple_unlock(&uobj->vmobjlock);
				return (0);
			}

			uvm_lock_pageq();
			uvm_pageactivate(pg); /* make sure it is in queues */
			uvm_unlock_pageq();
			pg->flags &= ~(PG_BUSY|PG_WANTED);
			UVM_PAGE_OWN(pg, NULL);
			simple_unlock(&uobj->vmobjlock);
			return (0);
		}
	}

	/*
	 * at this point we have the page we want ("pg") marked PG_BUSY for us
	 * and we have all data structures locked.   do the loanout.   page can
	 * not be PG_RELEASED (we caught this above).
	 */

	if ((flags & UVM_LOAN_TOANON) == 0) {	/* loan to wired-kernel page? */
		uvm_lock_pageq();
		if (pg->loan_count == 0)
			pmap_page_protect(PMAP_PGARG(pg), VM_PROT_READ);
		pg->loan_count++;
		uvm_pagewire(pg);
		uvm_unlock_pageq();
		**output = pg;
		*output = (*output) + 1;
		if (pg->flags & PG_WANTED)
			thread_wakeup(pg);
		pg->flags &= ~(PG_WANTED|PG_BUSY);
		UVM_PAGE_OWN(pg, NULL);
		return(1);		/* got it! */
	}

	/*
	 * must be a loan to an anon.   check to see if there is already
	 * an anon associated with this page.  if so, then just return
	 * a reference to this object.   the page should already be 
	 * mapped read-only because it is already on loan.
	 */

	if (pg->uanon) {
		anon = pg->uanon;
		simple_lock(&anon->an_lock);
		anon->an_ref++;
		simple_unlock(&anon->an_lock);
		**output = anon;
		*output = (*output) + 1;
		uvm_lock_pageq();
		uvm_pageactivate(pg);	/* reactivate */
		uvm_unlock_pageq();
		if (pg->flags & PG_WANTED)
			thread_wakeup(pg);
		pg->flags &= ~(PG_WANTED|PG_BUSY);
		UVM_PAGE_OWN(pg, NULL);
		return(1);
	}
	
	/*
	 * need to allocate a new anon
	 */

	anon = uvm_analloc();
	if (anon == NULL) {		/* out of VM! */
		if (pg->flags & PG_WANTED)
			thread_wakeup(pg);
		pg->flags &= ~(PG_WANTED|PG_BUSY);
		UVM_PAGE_OWN(pg, NULL);
		uvmfault_unlockall(ufi, amap, uobj, NULL);
		return(-1);
	}
	anon->u.an_page = pg;
	pg->uanon = anon;
	uvm_lock_pageq();
	if (pg->loan_count == 0)
		pmap_page_protect(PMAP_PGARG(pg), VM_PROT_READ);
	pg->loan_count++;
	uvm_pageactivate(pg);
	uvm_unlock_pageq();
	**output = anon;
	*output = (*output) + 1;
	if (pg->flags & PG_WANTED)
		thread_wakeup(pg);
	pg->flags &= ~(PG_WANTED|PG_BUSY);
	UVM_PAGE_OWN(pg, NULL);
	return(1);
}

/*
 * uvm_loanzero: "loan" a zero-fill page out
 *
 * => return value:
 *	-1 = fatal error, everything is unlocked, abort.
 *	 0 = lookup in ufi went stale, everything unlocked, relookup and
 *		try again
 *	 1 = got it, everything still locked
 */

int
uvm_loanzero(ufi, output, flags)
	struct uvm_faultinfo *ufi;
	void ***output;
	int flags;
{
	struct vm_anon *anon;
	struct vm_page *pg;

	if ((flags & UVM_LOAN_TOANON) == 0) {	/* loaning to kernel-page */

		while ((pg = uvm_pagealloc(NULL, 0, NULL)) == NULL) {
			uvmfault_unlockall(ufi, ufi->entry->aref.ar_amap, 
			    ufi->entry->object.uvm_obj, NULL);
			uvm_wait("loanzero1");
			if (!uvmfault_relock(ufi))
				return(0);
			if (ufi->entry->aref.ar_amap)
				amap_lock(ufi->entry->aref.ar_amap);
			if (ufi->entry->object.uvm_obj)
				simple_lock(
				    &ufi->entry->object.uvm_obj->vmobjlock);
			/* ... and try again */
		}
		
		/* got a page, zero it and return */
		uvm_pagezero(pg);		/* clears PG_CLEAN */
		pg->flags &= ~(PG_BUSY|PG_FAKE);
		UVM_PAGE_OWN(pg, NULL);
		**output = pg;
		*output = (*output) + 1;
		uvm_lock_pageq();
		/* wire it as we are loaning to kernel-page */
		uvm_pagewire(pg);
		pg->loan_count = 1;
		uvm_unlock_pageq();
		return(1);
	}

	/* loaning to an anon */
	while ((anon = uvm_analloc()) == NULL || 
	    (pg = uvm_pagealloc(NULL, 0, anon)) == NULL) {
		
		/* unlock everything */
		uvmfault_unlockall(ufi, ufi->entry->aref.ar_amap,
		       ufi->entry->object.uvm_obj, NULL);
		
		/* out of swap causes us to fail */
		if (anon == NULL)
			return(-1);

		uvm_anfree(anon);
		uvm_wait("loanzero2");		/* wait for pagedaemon */

		if (!uvmfault_relock(ufi))
			/* map changed while unlocked, need relookup */
			return (0);

		/* relock everything else */
		if (ufi->entry->aref.ar_amap)
			amap_lock(ufi->entry->aref.ar_amap);
		if (ufi->entry->object.uvm_obj)
			simple_lock(&ufi->entry->object.uvm_obj->vmobjlock);
		/* ... and try again */
	}

	/* got a page, zero it and return */
	uvm_pagezero(pg);		/* clears PG_CLEAN */
	pg->flags &= ~(PG_BUSY|PG_FAKE);
	UVM_PAGE_OWN(pg, NULL);
	uvm_lock_pageq();
	uvm_pageactivate(pg);
	uvm_unlock_pageq();
	**output = anon;
	*output = (*output) + 1;
	return(1);
}


/*
 * uvm_unloananon: kill loans on anons (basically a normal ref drop)
 *
 * => we expect all our resources to be unlocked
 */

void
uvm_unloananon(aloans, nanons)
	struct vm_anon **aloans;
	int nanons;
{
	struct vm_anon *anon;

	while (nanons-- > 0) {
		int refs;

		anon = *aloans++;
		simple_lock(&anon->an_lock);
		refs = --anon->an_ref;
		simple_unlock(&anon->an_lock);

		if (refs == 0) {
			uvm_anfree(anon);	/* last reference: kill anon */
		}
	}
}

/*
 * uvm_unloanpage: kill loans on pages loaned out to the kernel
 *
 * => we expect all our resources to be unlocked
 */

void
uvm_unloanpage(ploans, npages)
	struct vm_page **ploans;
	int npages;
{
	struct vm_page *pg;

	uvm_lock_pageq();

	while (npages-- > 0) {
		pg = *ploans++;

		if (pg->loan_count < 1)
			panic("uvm_unloanpage: page %p isn't loaned", pg);

		pg->loan_count--;		/* drop loan */
		uvm_pageunwire(pg);		/* and wire */

		/*
		 * if page is unowned and we killed last loan, then we can
		 * free it
		 */
		if (pg->loan_count == 0 && pg->uobject == NULL &&
		    pg->uanon == NULL) {

			if (pg->flags & PG_BUSY)
	panic("uvm_unloanpage: page %p unowned but PG_BUSY!", pg);

			/* be safe */
			pmap_page_protect(PMAP_PGARG(pg), VM_PROT_NONE);
			uvm_pagefree(pg);	/* pageq locked above */

		}
	}

	uvm_unlock_pageq();
}

