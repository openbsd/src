/*	$OpenBSD: uvm_pager.c,v 1.43 2007/06/06 17:15:14 deraadt Exp $	*/
/*	$NetBSD: uvm_pager.c,v 1.36 2000/11/27 18:26:41 chs Exp $	*/

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
 * from: Id: uvm_pager.c,v 1.1.2.23 1998/02/02 20:38:06 chuck Exp
 */

/*
 * uvm_pager.c: generic functions used to assist the pagers.
 */

#define UVM_PAGER
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/pool.h>
#include <sys/vnode.h>
#include <sys/buf.h>

#include <uvm/uvm.h>

struct pool *uvm_aiobuf_pool;

struct uvm_pagerops *uvmpagerops[] = {
	&aobj_pager,
	&uvm_deviceops,
	&uvm_vnodeops,
};

/*
 * the pager map: provides KVA for I/O
 */

vm_map_t pager_map;		/* XXX */
simple_lock_data_t pager_map_wanted_lock;
boolean_t pager_map_wanted;	/* locked by pager map */
static vaddr_t emergva;
static boolean_t emerginuse;

/*
 * uvm_pager_init: init pagers (at boot time)
 */

void
uvm_pager_init()
{
	int lcv;

	/*
	 * init pager map
	 */

	pager_map = uvm_km_suballoc(kernel_map, &uvm.pager_sva, &uvm.pager_eva,
	 			    PAGER_MAP_SIZE, 0, FALSE, NULL);
	simple_lock_init(&pager_map_wanted_lock);
	pager_map_wanted = FALSE;
	emergva = uvm_km_valloc(kernel_map, MAXBSIZE);
	emerginuse = FALSE;

	/*
	 * init ASYNC I/O queue
	 */
	
	TAILQ_INIT(&uvm.aio_done);

	/*
	 * call pager init functions
	 */
	for (lcv = 0 ; lcv < sizeof(uvmpagerops)/sizeof(struct uvm_pagerops *);
	    lcv++) {
		if (uvmpagerops[lcv]->pgo_init)
			uvmpagerops[lcv]->pgo_init();
	}
}

/*
 * uvm_pagermapin: map pages into KVA (pager_map) for I/O that needs mappings
 *
 * we basically just map in a blank map entry to reserve the space in the
 * map and then use pmap_enter() to put the mappings in by hand.
 */

vaddr_t
uvm_pagermapin(pps, npages, flags)
	struct vm_page **pps;
	int npages;
	int flags;
{
	vsize_t size;
	vaddr_t kva;
	vaddr_t cva;
	struct vm_page *pp;
	vm_prot_t prot;
	UVMHIST_FUNC("uvm_pagermapin"); UVMHIST_CALLED(maphist);

	UVMHIST_LOG(maphist,"(pps=%p, npages=%ld)", pps, npages,0,0);

	/*
	 * compute protection.  outgoing I/O only needs read
	 * access to the page, whereas incoming needs read/write.
	 */

	prot = VM_PROT_READ;
	if (flags & UVMPAGER_MAPIN_READ)
		prot |= VM_PROT_WRITE;

ReStart:
	size = npages << PAGE_SHIFT;
	kva = 0;			/* let system choose VA */

	if (uvm_map(pager_map, &kva, size, NULL, 
	      UVM_UNKNOWN_OFFSET, 0, UVM_FLAG_NOMERGE) != 0) {
		if (curproc == uvm.pagedaemon_proc) {
			simple_lock(&pager_map_wanted_lock);
			if (emerginuse) {
				UVM_UNLOCK_AND_WAIT(&emergva,
				    &pager_map_wanted_lock, FALSE,
				    "emergva", 0);
				goto ReStart;
			}
			emerginuse = TRUE;
			simple_unlock(&pager_map_wanted_lock);
			kva = emergva;
			KASSERT(npages <= MAXBSIZE >> PAGE_SHIFT);
			goto enter;
		}
		if ((flags & UVMPAGER_MAPIN_WAITOK) == 0) {
			UVMHIST_LOG(maphist,"<- NOWAIT failed", 0,0,0,0);
			return(0);
		}
		simple_lock(&pager_map_wanted_lock);
		pager_map_wanted = TRUE; 
		UVMHIST_LOG(maphist, "  SLEEPING on pager_map",0,0,0,0);
		UVM_UNLOCK_AND_WAIT(pager_map, &pager_map_wanted_lock, FALSE, 
		    "pager_map", 0);
		goto ReStart;
	}

enter:
	/* got it */
	for (cva = kva ; size != 0 ; size -= PAGE_SIZE, cva += PAGE_SIZE) {
		pp = *pps++;
		KASSERT(pp);
		KASSERT(pp->pg_flags & PG_BUSY);
		pmap_enter(vm_map_pmap(pager_map), cva, VM_PAGE_TO_PHYS(pp),
		    prot, PMAP_WIRED | prot);
	}
	pmap_update(vm_map_pmap(pager_map));

	UVMHIST_LOG(maphist, "<- done (KVA=0x%lx)", kva,0,0,0);
	return(kva);
}

/*
 * uvm_pagermapout: remove pager_map mapping
 *
 * we remove our mappings by hand and then remove the mapping (waking
 * up anyone wanting space).
 */

void
uvm_pagermapout(kva, npages)
	vaddr_t kva;
	int npages;
{
	vsize_t size = npages << PAGE_SHIFT;
	vm_map_entry_t entries;
	UVMHIST_FUNC("uvm_pagermapout"); UVMHIST_CALLED(maphist);

	UVMHIST_LOG(maphist, " (kva=0x%lx, npages=%ld)", kva, npages,0,0);

	/*
	 * duplicate uvm_unmap, but add in pager_map_wanted handling.
	 */

	if (kva == emergva) {
		simple_lock(&pager_map_wanted_lock);
		emerginuse = FALSE;
		wakeup(&emergva);
		simple_unlock(&pager_map_wanted_lock);
		entries = NULL;
		goto remove;
	}

	vm_map_lock(pager_map);
	uvm_unmap_remove(pager_map, kva, kva + size, &entries, NULL);
	simple_lock(&pager_map_wanted_lock);
	if (pager_map_wanted) {
		pager_map_wanted = FALSE;
		wakeup(pager_map);
	}
	simple_unlock(&pager_map_wanted_lock);
	vm_map_unlock(pager_map);
remove:
	pmap_remove(pmap_kernel(), kva, kva + (npages << PAGE_SHIFT));
	if (entries)
		uvm_unmap_detach(entries, 0);

	pmap_update(pmap_kernel());
	UVMHIST_LOG(maphist,"<- done",0,0,0,0);
}

/*
 * uvm_mk_pcluster
 *
 * generic "make 'pager put' cluster" function.  a pager can either
 * [1] set pgo_mk_pcluster to NULL (never cluster), [2] set it to this
 * generic function, or [3] set it to a pager specific function.
 *
 * => caller must lock object _and_ pagequeues (since we need to look
 *    at active vs. inactive bits, etc.)
 * => caller must make center page busy and write-protect it
 * => we mark all cluster pages busy for the caller
 * => the caller must unbusy all pages (and check wanted/released
 *    status if it drops the object lock)
 * => flags:
 *      PGO_ALLPAGES:  all pages in object are valid targets
 *      !PGO_ALLPAGES: use "lo" and "hi" to limit range of cluster
 *      PGO_DOACTCLUST: include active pages in cluster.
 *        NOTE: the caller should clear PG_CLEANCHK bits if PGO_DOACTCLUST.
 *              PG_CLEANCHK is only a hint, but clearing will help reduce
 *		the number of calls we make to the pmap layer.
 */

struct vm_page **
uvm_mk_pcluster(uobj, pps, npages, center, flags, mlo, mhi)
	struct uvm_object *uobj;	/* IN */
	struct vm_page **pps, *center;  /* IN/OUT, IN */
	int *npages, flags;		/* IN/OUT, IN */
	voff_t mlo, mhi;		/* IN (if !PGO_ALLPAGES) */
{
	struct vm_page **ppsp, *pclust;
	voff_t lo, hi, curoff;
	int center_idx, forward, incr;
	UVMHIST_FUNC("uvm_mk_pcluster"); UVMHIST_CALLED(maphist);

	/* 
	 * center page should already be busy and write protected.  XXX:
	 * suppose page is wired?  if we lock, then a process could
	 * fault/block on it.  if we don't lock, a process could write the
	 * pages in the middle of an I/O.  (consider an msync()).  let's
	 * lock it for now (better to delay than corrupt data?).
	 */

	/*
	 * get cluster boundaries, check sanity, and apply our limits as well.
	 */

	uobj->pgops->pgo_cluster(uobj, center->offset, &lo, &hi);
	if ((flags & PGO_ALLPAGES) == 0) {
		if (lo < mlo)
			lo = mlo;
		if (hi > mhi)
			hi = mhi;
	}
	if ((hi - lo) >> PAGE_SHIFT > *npages) { /* pps too small, bail out! */
		pps[0] = center;
		*npages = 1;
		return(pps);
	}

	/*
	 * now determine the center and attempt to cluster around the
	 * edges
	 */

	center_idx = (center->offset - lo) >> PAGE_SHIFT;
	pps[center_idx] = center;	/* plug in the center page */
	ppsp = &pps[center_idx];
	*npages = 1;

	/*
	 * attempt to cluster around the left [backward], and then 
	 * the right side [forward].    
	 *
	 * note that for inactive pages (pages that have been deactivated)
	 * there are no valid mappings and PG_CLEAN should be up to date.
	 * [i.e. there is no need to query the pmap with pmap_is_modified
	 * since there are no mappings].
	 */

	for (forward  = 0 ; forward <= 1 ; forward++) {
		incr = forward ? PAGE_SIZE : -PAGE_SIZE;
		curoff = center->offset + incr;
		for ( ;(forward == 0 && curoff >= lo) ||
		       (forward && curoff < hi);
		      curoff += incr) {

			pclust = uvm_pagelookup(uobj, curoff); /* lookup page */
			if (pclust == NULL) {
				break;			/* no page */
			}
			/* handle active pages */
			/* NOTE: inactive pages don't have pmap mappings */
			if ((pclust->pg_flags & PQ_INACTIVE) == 0) {
				if ((flags & PGO_DOACTCLUST) == 0) {
					/* dont want mapped pages at all */
					break;
				}

				/* make sure "clean" bit is sync'd */
				if ((pclust->pg_flags & PG_CLEANCHK) == 0) {
					if ((pclust->pg_flags & (PG_CLEAN|PG_BUSY))
					   == PG_CLEAN &&
					   pmap_is_modified(pclust))
						atomic_clearbits_int(
						    &pclust->pg_flags,
						    PG_CLEAN);
					/* now checked */
					atomic_setbits_int(&pclust->pg_flags,
					    PG_CLEANCHK);
				}
			}

			/* is page available for cleaning and does it need it */
			if ((pclust->pg_flags & (PG_CLEAN|PG_BUSY)) != 0) {
				break;	/* page is already clean or is busy */
			}

			/* yes!   enroll the page in our array */
			atomic_setbits_int(&pclust->pg_flags, PG_BUSY);
			UVM_PAGE_OWN(pclust, "uvm_mk_pcluster");

			/* XXX: protect wired page?   see above comment. */
			pmap_page_protect(pclust, VM_PROT_READ);
			if (!forward) {
				ppsp--;			/* back up one page */
				*ppsp = pclust;
			} else {
				/* move forward one page */
				ppsp[*npages] = pclust;
			}
			(*npages)++;
		}
	}
	
	/*
	 * done!  return the cluster array to the caller!!!
	 */

	UVMHIST_LOG(maphist, "<- done",0,0,0,0);
	return(ppsp);
}

/*
 * uvm_pager_put: high level pageout routine
 *
 * we want to pageout page "pg" to backing store, clustering if
 * possible.
 *
 * => page queues must be locked by caller
 * => if page is not swap-backed, then "uobj" points to the object
 *	backing it.   this object should be locked by the caller.
 * => if page is swap-backed, then "uobj" should be NULL.
 * => "pg" should be PG_BUSY (by caller), and !PG_CLEAN
 *    for swap-backed memory, "pg" can be NULL if there is no page
 *    of interest [sometimes the case for the pagedaemon]
 * => "ppsp_ptr" should point to an array of npages vm_page pointers
 *	for possible cluster building
 * => flags (first two for non-swap-backed pages)
 *	PGO_ALLPAGES: all pages in uobj are valid targets
 *	PGO_DOACTCLUST: include "PQ_ACTIVE" pages as valid targets
 *	PGO_SYNCIO: do SYNC I/O (no async)
 *	PGO_PDFREECLUST: pagedaemon: drop cluster on successful I/O
 * => start/stop: if (uobj && !PGO_ALLPAGES) limit targets to this range
 *		  if (!uobj) start is the (daddr64_t) of the starting swapblk
 * => return state:
 *	1. we return the VM_PAGER status code of the pageout
 *	2. we return with the page queues unlocked
 *	3. if (uobj != NULL) [!swap_backed] we return with
 *		uobj locked _only_ if PGO_PDFREECLUST is set 
 *		AND result != VM_PAGER_PEND.   in all other cases
 *		we return with uobj unlocked.   [this is a hack
 *		that allows the pagedaemon to save one lock/unlock
 *		pair in the !swap_backed case since we have to
 *		lock the uobj to drop the cluster anyway]
 *	4. on errors we always drop the cluster.   thus, if we return
 *		!PEND, !OK, then the caller only has to worry about
 *		un-busying the main page (not the cluster pages).
 *	5. on success, if !PGO_PDFREECLUST, we return the cluster
 *		with all pages busy (caller must un-busy and check
 *		wanted/released flags).
 */

int
uvm_pager_put(uobj, pg, ppsp_ptr, npages, flags, start, stop)
	struct uvm_object *uobj;	/* IN */
	struct vm_page *pg, ***ppsp_ptr;/* IN, IN/OUT */
	int *npages;			/* IN/OUT */
	int flags;			/* IN */
	voff_t start, stop;		/* IN, IN */
{
	int result;
	daddr64_t swblk;
	struct vm_page **ppsp = *ppsp_ptr;
	UVMHIST_FUNC("uvm_pager_put"); UVMHIST_CALLED(pdhist);

	/*
	 * note that uobj is null  if we are doing a swap-backed pageout.
	 * note that uobj is !null if we are doing normal object pageout.
	 * note that the page queues must be locked to cluster.
	 */

	if (uobj) {	/* if !swap-backed */

		/*
		 * attempt to build a cluster for pageout using its
		 * make-put-cluster function (if it has one).
		 */

		if (uobj->pgops->pgo_mk_pcluster) {
			ppsp = uobj->pgops->pgo_mk_pcluster(uobj, ppsp,
			    npages, pg, flags, start, stop);
			*ppsp_ptr = ppsp;  /* update caller's pointer */
		} else {
			ppsp[0] = pg;
			*npages = 1;
		}

		swblk = 0;		/* XXX: keep gcc happy */

	} else {

		/*
		 * for swap-backed pageout, the caller (the pagedaemon) has
		 * already built the cluster for us.   the starting swap
		 * block we are writing to has been passed in as "start."
		 * "pg" could be NULL if there is no page we are especially
		 * interested in (in which case the whole cluster gets dropped
		 * in the event of an error or a sync "done").
		 */
		swblk = (daddr64_t) start;
		/* ppsp and npages should be ok */
	}

	/* now that we've clustered we can unlock the page queues */
	uvm_unlock_pageq();

	/*
	 * now attempt the I/O.   if we have a failure and we are
	 * clustered, we will drop the cluster and try again.
	 */

ReTry:
	if (uobj) {
		/* object is locked */
		result = uobj->pgops->pgo_put(uobj, ppsp, *npages, flags);
		UVMHIST_LOG(pdhist, "put -> %ld", result, 0,0,0);
		/* object is now unlocked */
	} else {
		/* nothing locked */
		/* XXX daddr64_t -> int */
		result = uvm_swap_put(swblk, ppsp, *npages, flags);
		/* nothing locked */
	}

	/*
	 * we have attempted the I/O.
	 *
	 * if the I/O was a success then:
	 * 	if !PGO_PDFREECLUST, we return the cluster to the 
	 *		caller (who must un-busy all pages)
	 *	else we un-busy cluster pages for the pagedaemon
	 *
	 * if I/O is pending (async i/o) then we return the pending code.
	 * [in this case the async i/o done function must clean up when
	 *  i/o is done...]
	 */

	if (result == VM_PAGER_PEND || result == VM_PAGER_OK) {
		if (result == VM_PAGER_OK && (flags & PGO_PDFREECLUST)) {
			/*
			 * drop cluster and relock object (only if I/O is
			 * not pending)
			 */
			if (uobj)
				/* required for dropcluster */
				simple_lock(&uobj->vmobjlock);
			if (*npages > 1 || pg == NULL)
				uvm_pager_dropcluster(uobj, pg, ppsp, npages,
				    PGO_PDFREECLUST);
			/* if (uobj): object still locked, as per
			 * return-state item #3 */
		}
		return (result);
	}

	/*
	 * a pager error occured (even after dropping the cluster, if there
	 * was one).  give up! the caller only has one page ("pg")
	 * to worry about.
	 */

	if (*npages > 1 || pg == NULL) {
		if (uobj) {
			simple_lock(&uobj->vmobjlock);
		}
		uvm_pager_dropcluster(uobj, pg, ppsp, npages, PGO_REALLOCSWAP);

		/*
		 * for failed swap-backed pageouts with a "pg",
		 * we need to reset pg's swslot to either:
		 * "swblk" (for transient errors, so we can retry),
		 * or 0 (for hard errors).
		 */

		if (uobj == NULL && pg != NULL) {
			/* XXX daddr64_t -> int */
			int nswblk = (result == VM_PAGER_AGAIN) ? swblk : 0;
			if (pg->pg_flags & PQ_ANON) {
				simple_lock(&pg->uanon->an_lock);
				pg->uanon->an_swslot = nswblk;
				simple_unlock(&pg->uanon->an_lock);
			} else {
				simple_lock(&pg->uobject->vmobjlock);
				uao_set_swslot(pg->uobject,
					       pg->offset >> PAGE_SHIFT,
					       nswblk);
				simple_unlock(&pg->uobject->vmobjlock);
			}
		}
		if (result == VM_PAGER_AGAIN) {

			/*
			 * for transient failures, free all the swslots that
			 * we're not going to retry with.
			 */

			if (uobj == NULL) {
				if (pg) {
					/* XXX daddr64_t -> int */
					uvm_swap_free(swblk + 1, *npages - 1);
				} else {
					/* XXX daddr64_t -> int */
					uvm_swap_free(swblk, *npages);
				}
			}
			if (pg) {
				ppsp[0] = pg;
				*npages = 1;
				goto ReTry;
			}
		} else if (uobj == NULL) {

			/*
			 * for hard errors on swap-backed pageouts,
			 * mark the swslots as bad.  note that we do not
			 * free swslots that we mark bad.
			 */

			/* XXX daddr64_t -> int */
			uvm_swap_markbad(swblk, *npages);
		}
	}

	/*
	 * a pager error occurred (even after dropping the cluster, if there
	 * was one).    give up!   the caller only has one page ("pg")
	 * to worry about.
	 */
	
	if (uobj && (flags & PGO_PDFREECLUST) != 0)
		simple_lock(&uobj->vmobjlock);
	return(result);
}

/*
 * uvm_pager_dropcluster: drop a cluster we have built (because we 
 * got an error, or, if PGO_PDFREECLUST we are un-busying the
 * cluster pages on behalf of the pagedaemon).
 *
 * => uobj, if non-null, is a non-swap-backed object that is 
 *	locked by the caller.   we return with this object still
 *	locked.
 * => page queues are not locked
 * => pg is our page of interest (the one we clustered around, can be null)
 * => ppsp/npages is our current cluster
 * => flags: PGO_PDFREECLUST: pageout was a success: un-busy cluster
 *	pages on behalf of the pagedaemon.
 *           PGO_REALLOCSWAP: drop previously allocated swap slots for 
 *		clustered swap-backed pages (except for "pg" if !NULL)
 *		"swblk" is the start of swap alloc (e.g. for ppsp[0])
 *		[only meaningful if swap-backed (uobj == NULL)]
 */

void
uvm_pager_dropcluster(uobj, pg, ppsp, npages, flags)
	struct uvm_object *uobj;	/* IN */
	struct vm_page *pg, **ppsp;	/* IN, IN/OUT */
	int *npages;			/* IN/OUT */
	int flags;
{
	int lcv;
	boolean_t obj_is_alive; 
	struct uvm_object *saved_uobj;

	/*
	 * drop all pages but "pg"
	 */

	for (lcv = 0 ; lcv < *npages ; lcv++) {

		/* skip "pg" or empty slot */
		if (ppsp[lcv] == pg || ppsp[lcv] == NULL)
			continue;
	
		/*
		 * if swap-backed, gain lock on object that owns page.  note
		 * that PQ_ANON bit can't change as long as we are holding
		 * the PG_BUSY bit (so there is no need to lock the page
		 * queues to test it).
		 *
		 * once we have the lock, dispose of the pointer to swap, if
		 * requested
		 */
		if (!uobj) {
			if (ppsp[lcv]->pg_flags & PQ_ANON) {
				simple_lock(&ppsp[lcv]->uanon->an_lock);
				if (flags & PGO_REALLOCSWAP)
					  /* zap swap block */
					  ppsp[lcv]->uanon->an_swslot = 0;
			} else {
				simple_lock(&ppsp[lcv]->uobject->vmobjlock);
				if (flags & PGO_REALLOCSWAP)
					uao_set_swslot(ppsp[lcv]->uobject,
					    ppsp[lcv]->offset >> PAGE_SHIFT, 0);
			}
		}

		/* did someone want the page while we had it busy-locked? */
		if (ppsp[lcv]->pg_flags & PG_WANTED) {
			/* still holding obj lock */
			wakeup(ppsp[lcv]);
		}

		/* if page was released, release it.  otherwise un-busy it */
		if (ppsp[lcv]->pg_flags & PG_RELEASED) {

			if (ppsp[lcv]->pg_flags & PQ_ANON) {
				/* so that anfree will free */
				atomic_clearbits_int(&ppsp[lcv]->pg_flags,
				    PG_BUSY);
				UVM_PAGE_OWN(ppsp[lcv], NULL);

				pmap_page_protect(ppsp[lcv], VM_PROT_NONE);
				simple_unlock(&ppsp[lcv]->uanon->an_lock);
				/* kills anon and frees pg */
				uvm_anfree(ppsp[lcv]->uanon);

				continue;
			}

			/*
			 * pgo_releasepg will dump the page for us
			 */

			saved_uobj = ppsp[lcv]->uobject;
			obj_is_alive =
			    saved_uobj->pgops->pgo_releasepg(ppsp[lcv], NULL);
			
			/* for normal objects, "pg" is still PG_BUSY by us,
			 * so obj can't die */
			KASSERT(!uobj || obj_is_alive);

			/* only unlock the object if it is still alive...  */
			if (obj_is_alive && saved_uobj != uobj)
				simple_unlock(&saved_uobj->vmobjlock);

			/*
			 * XXXCDC: suppose uobj died in the pgo_releasepg?
			 * how pass that
			 * info up to caller.  we are currently ignoring it...
			 */

			continue;		/* next page */
		} else {
			atomic_clearbits_int(&ppsp[lcv]->pg_flags,
			    PG_BUSY|PG_WANTED|PG_FAKE);
			UVM_PAGE_OWN(ppsp[lcv], NULL);
		}

		/*
		 * if we are operating on behalf of the pagedaemon and we 
		 * had a successful pageout update the page!
		 */
		if (flags & PGO_PDFREECLUST) {
			pmap_clear_reference(ppsp[lcv]);
			pmap_clear_modify(ppsp[lcv]);
			atomic_setbits_int(&ppsp[lcv]->pg_flags, PG_CLEAN);
		}

		/* if anonymous cluster, unlock object and move on */
		if (!uobj) {
			if (ppsp[lcv]->pg_flags & PQ_ANON)
				simple_unlock(&ppsp[lcv]->uanon->an_lock);
			else
				simple_unlock(&ppsp[lcv]->uobject->vmobjlock);
		}
	}
}

#ifdef UBC
/*
 * interrupt-context iodone handler for nested i/o bufs.
 *
 * => must be at splbio().
 */

void
uvm_aio_biodone1(bp)
	struct buf *bp;
{
	struct buf *mbp = bp->b_private;

	splassert(IPL_BIO);

	KASSERT(mbp != bp);
	if (bp->b_flags & B_ERROR) {
		mbp->b_flags |= B_ERROR;
		mbp->b_error = bp->b_error;
	}
	mbp->b_resid -= bp->b_bcount;
	pool_put(&bufpool, bp);
	if (mbp->b_resid == 0) {
		biodone(mbp);
	}
}
#endif

/*
 * interrupt-context iodone handler for single-buf i/os
 * or the top-level buf of a nested-buf i/o.
 *
 * => must be at splbio().
 */

void
uvm_aio_biodone(bp)
	struct buf *bp;
{
	splassert(IPL_BIO);

	/* reset b_iodone for when this is a single-buf i/o. */
	bp->b_iodone = uvm_aio_aiodone;

	simple_lock(&uvm.aiodoned_lock);	/* locks uvm.aio_done */
	TAILQ_INSERT_TAIL(&uvm.aio_done, bp, b_freelist);
	wakeup(&uvm.aiodoned);
	simple_unlock(&uvm.aiodoned_lock);
}

/*
 * uvm_aio_aiodone: do iodone processing for async i/os.
 * this should be called in thread context, not interrupt context.
 */

void
uvm_aio_aiodone(bp)
	struct buf *bp;
{
	int npages = bp->b_bufsize >> PAGE_SHIFT;
	struct vm_page *pg, *pgs[npages];
	struct uvm_object *uobj;
	int i, error;
	boolean_t write, swap;
	UVMHIST_FUNC("uvm_aio_aiodone"); UVMHIST_CALLED(pdhist);
	UVMHIST_LOG(pdhist, "bp %p", bp, 0,0,0);

	splassert(IPL_BIO);

	error = (bp->b_flags & B_ERROR) ? (bp->b_error ? bp->b_error : EIO) : 0;
	write = (bp->b_flags & B_READ) == 0;
#ifdef UBC
	/* XXXUBC B_NOCACHE is for swap pager, should be done differently */
	if (write && !(bp->b_flags & B_NOCACHE) && bioops.io_pageiodone) {
		(*bioops.io_pageiodone)(bp);
	}
#endif

	uobj = NULL;
	for (i = 0; i < npages; i++) {
		pgs[i] = uvm_pageratop((vaddr_t)bp->b_data + (i << PAGE_SHIFT));
		UVMHIST_LOG(pdhist, "pgs[%ld] = %p", i, pgs[i],0,0);
	}
	uvm_pagermapout((vaddr_t)bp->b_data, npages);
#ifdef UVM_SWAP_ENCRYPT
	/*
	 * XXX - assumes that we only get ASYNC writes. used to be above.
	 */
	if (pgs[0]->pg_flags & PQ_ENCRYPT) {
		uvm_swap_freepages(pgs, npages);
		goto freed;
	}
#endif /* UVM_SWAP_ENCRYPT */
	for (i = 0; i < npages; i++) {
		pg = pgs[i];

		if (i == 0) {
			swap = (pg->pg_flags & PQ_SWAPBACKED) != 0;
			if (!swap) {
				uobj = pg->uobject;
				simple_lock(&uobj->vmobjlock);
			}
		}
		KASSERT(swap || pg->uobject == uobj);
		if (swap) {
			if (pg->pg_flags & PQ_ANON) {
				simple_lock(&pg->uanon->an_lock);
			} else {
				simple_lock(&pg->uobject->vmobjlock);
			}
		}

		/*
		 * if this is a read and we got an error, mark the pages
		 * PG_RELEASED so that uvm_page_unbusy() will free them.
		 */
		if (!write && error) {
			atomic_setbits_int(&pg->pg_flags, PG_RELEASED);
			continue;
		}
		KASSERT(!write || (pgs[i]->pg_flags & PG_FAKE) == 0);

		/*
		 * if this is a read and the page is PG_FAKE,
		 * or this was a successful write,
		 * mark the page PG_CLEAN and not PG_FAKE.
		 */

		if ((pgs[i]->pg_flags & PG_FAKE) || (write && error != ENOMEM)) {
			pmap_clear_reference(pgs[i]);
			pmap_clear_modify(pgs[i]);
			atomic_setbits_int(&pgs[i]->pg_flags, PG_CLEAN);
			atomic_clearbits_int(&pgs[i]->pg_flags, PG_FAKE);
		}
		if (swap) {
			if (pg->pg_flags & PQ_ANON) {
				simple_unlock(&pg->uanon->an_lock);
			} else {
				simple_unlock(&pg->uobject->vmobjlock);
			}
		}
	}
	uvm_page_unbusy(pgs, npages);
	if (!swap) {
		simple_unlock(&uobj->vmobjlock);
	}

#ifdef UVM_SWAP_ENCRYPT
freed:
#endif
	if (write && (bp->b_flags & B_AGE) != 0 && bp->b_vp != NULL) {
		vwakeup(bp->b_vp);
	}
	pool_put(&bufpool, bp);
}

/*
 * translate unix errno values to VM_PAGER_*.
 */

int
uvm_errno2vmerror(errno)
	int errno;
{
	switch (errno) {
	case 0:
		return VM_PAGER_OK;
	case EINVAL:
		return VM_PAGER_BAD;
	case EINPROGRESS:
		return VM_PAGER_PEND;
	case EIO:
		return VM_PAGER_ERROR;
	case EAGAIN:
		return VM_PAGER_AGAIN;
	case EBUSY:
		return VM_PAGER_UNLOCK;
	default:
		return VM_PAGER_ERROR;
	}
}
