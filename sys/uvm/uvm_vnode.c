/*	$OpenBSD: uvm_vnode.c,v 1.30 2001/12/06 12:43:20 art Exp $	*/
/*	$NetBSD: uvm_vnode.c,v 1.51 2001/08/17 05:53:02 chs Exp $	*/

/*
 * Copyright (c) 1997 Charles D. Cranor and Washington University.
 * Copyright (c) 1991, 1993
 *      The Regents of the University of California.
 * Copyright (c) 1990 University of Utah.
 *
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 *      This product includes software developed by Charles D. Cranor,
 *	Washington University, the University of California, Berkeley and
 *	its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *      @(#)vnode_pager.c       8.8 (Berkeley) 2/13/94
 * from: Id: uvm_vnode.c,v 1.1.2.26 1998/02/02 20:38:07 chuck Exp
 */

/*
 * uvm_vnode.c: the vnode pager.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/vnode.h>
#include <sys/disklabel.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <sys/conf.h>
#include <sys/pool.h>
#include <sys/mount.h>

#include <miscfs/specfs/specdev.h>

#include <uvm/uvm.h>
#include <uvm/uvm_vnode.h>

/*
 * functions
 */

static void		uvn_cluster __P((struct uvm_object *, voff_t, voff_t *,
					 voff_t *));
static void		uvn_detach __P((struct uvm_object *));
static int		uvn_findpage __P((struct uvm_object *, voff_t,
					  struct vm_page **, int));
boolean_t		uvn_flush __P((struct uvm_object *, voff_t, voff_t,
				       int));
int			uvn_get __P((struct uvm_object *, voff_t,
				     struct vm_page **, int *, int, vm_prot_t,
				     int, int));
int			uvn_put __P((struct uvm_object *, struct vm_page **,
				     int, boolean_t));
static void		uvn_reference __P((struct uvm_object *));
static boolean_t	uvn_releasepg __P((struct vm_page *,
					   struct vm_page **));

/*
 * master pager structure
 */

struct uvm_pagerops uvm_vnodeops = {
	NULL,
	uvn_reference,
	uvn_detach,
	NULL,
	uvn_flush,
	uvn_get,
	uvn_put,
	uvn_cluster,
	uvm_mk_pcluster,
	uvn_releasepg,
};

/*
 * the ops!
 */

/*
 * uvn_attach
 *
 * attach a vnode structure to a VM object.  if the vnode is already
 * attached, then just bump the reference count by one and return the
 * VM object.   if not already attached, attach and return the new VM obj.
 * the "accessprot" tells the max access the attaching thread wants to
 * our pages.
 *
 * => caller must _not_ already be holding the lock on the uvm_object.
 * => in fact, nothing should be locked so that we can sleep here.
 * => note that uvm_object is first thing in vnode structure, so their
 *    pointers are equiv.
 */

struct uvm_object *
uvn_attach(arg, accessprot)
	void *arg;
	vm_prot_t accessprot;
{
	struct vnode *vp = arg;
	struct uvm_vnode *uvn = &vp->v_uvm;
	struct vattr vattr;
	int result;
	struct partinfo pi;
	voff_t used_vnode_size;
	UVMHIST_FUNC("uvn_attach"); UVMHIST_CALLED(maphist);

	UVMHIST_LOG(maphist, "(vn=0x%x)", arg,0,0,0);
	used_vnode_size = (voff_t)0;

	/*
	 * first get a lock on the uvn.
	 */
	simple_lock(&uvn->u_obj.vmobjlock);
	while (uvn->u_flags & VXLOCK) {
		uvn->u_flags |= VXWANT;
		UVMHIST_LOG(maphist, "  SLEEPING on blocked vn",0,0,0,0);
		UVM_UNLOCK_AND_WAIT(uvn, &uvn->u_obj.vmobjlock, FALSE,
		    "uvn_attach", 0);
		simple_lock(&uvn->u_obj.vmobjlock);
		UVMHIST_LOG(maphist,"  WOKE UP",0,0,0,0);
	}

	/*
	 * if we're mapping a BLK device, make sure it is a disk.
	 */
	if (vp->v_type == VBLK && bdevsw[major(vp->v_rdev)].d_type != D_DISK) {
		simple_unlock(&uvn->u_obj.vmobjlock);
		UVMHIST_LOG(maphist,"<- done (VBLK not D_DISK!)", 0,0,0,0);
		return(NULL);
	}
	KASSERT(vp->v_type == VREG || vp->v_type == VBLK);

	/*
	 * set up our idea of the size
	 * if this hasn't been done already.
	 */
	if (uvn->u_size == VSIZENOTSET) {

	uvn->u_flags |= VXLOCK;
	simple_unlock(&uvn->u_obj.vmobjlock); /* drop lock in case we sleep */
		/* XXX: curproc? */
	if (vp->v_type == VBLK) {
		/*
		 * We could implement this as a specfs getattr call, but:
		 *
		 *	(1) VOP_GETATTR() would get the file system
		 *	    vnode operation, not the specfs operation.
		 *
		 *	(2) All we want is the size, anyhow.
		 */
		result = (*bdevsw[major(vp->v_rdev)].d_ioctl)(vp->v_rdev,
		    DIOCGPART, (caddr_t)&pi, FREAD, curproc);
		if (result == 0) {
			/* XXX should remember blocksize */
			used_vnode_size = (voff_t)pi.disklab->d_secsize *
			    (voff_t)pi.part->p_size;
		}
	} else {
		result = VOP_GETATTR(vp, &vattr, curproc->p_ucred, curproc);
		if (result == 0)
			used_vnode_size = vattr.va_size;
	}

	/* relock object */
	simple_lock(&uvn->u_obj.vmobjlock);

	if (uvn->u_flags & VXWANT)
		wakeup(uvn);
	uvn->u_flags &= ~(VXLOCK|VXWANT);

	if (result != 0) {
		simple_unlock(&uvn->u_obj.vmobjlock); /* drop lock */
		UVMHIST_LOG(maphist,"<- done (VOP_GETATTR FAILED!)", 0,0,0,0);
		return(NULL);
	}
	uvn->u_size = used_vnode_size;

	}

	/* unlock and return */
	simple_unlock(&uvn->u_obj.vmobjlock);
	UVMHIST_LOG(maphist,"<- done, refcnt=%d", uvn->u_obj.uo_refs,
	    0, 0, 0);
	return (&uvn->u_obj);
}


/*
 * uvn_reference
 *
 * duplicate a reference to a VM object.  Note that the reference
 * count must already be at least one (the passed in reference) so
 * there is no chance of the uvn being killed or locked out here.
 *
 * => caller must call with object unlocked.
 * => caller must be using the same accessprot as was used at attach time
 */


static void
uvn_reference(uobj)
	struct uvm_object *uobj;
{
	VREF((struct vnode *)uobj);
}

/*
 * uvn_detach
 *
 * remove a reference to a VM object.
 *
 * => caller must call with object unlocked and map locked.
 */
static void
uvn_detach(uobj)
	struct uvm_object *uobj;
{
	vrele((struct vnode *)uobj);
}

/*
 * uvn_releasepg: handled a released page in a uvn
 *
 * => "pg" is a PG_BUSY [caller owns it], PG_RELEASED page that we need
 *	to dispose of.
 * => caller must handled PG_WANTED case
 * => called with page's object locked, pageq's unlocked
 * => returns TRUE if page's object is still alive, FALSE if we
 *	killed the page's object.    if we return TRUE, then we
 *	return with the object locked.
 * => if (nextpgp != NULL) => we return the next page on the queue, and return
 *				with the page queues locked [for pagedaemon]
 * => if (nextpgp == NULL) => we return with page queues unlocked [normal case]
 * => we kill the uvn if it is not referenced and we are suppose to
 *	kill it ("relkill").
 */

boolean_t
uvn_releasepg(pg, nextpgp)
	struct vm_page *pg;
	struct vm_page **nextpgp;	/* OUT */
{
	KASSERT(pg->flags & PG_RELEASED);

	/*
	 * dispose of the page [caller handles PG_WANTED]
	 */
	pmap_page_protect(pg, VM_PROT_NONE);
	uvm_lock_pageq();
	if (nextpgp)
		*nextpgp = TAILQ_NEXT(pg, pageq);
	uvm_pagefree(pg);
	if (!nextpgp)
		uvm_unlock_pageq();

	return (TRUE);
}

/*
 * issues to consider:
 * there are two tailq's in the uvm. structure... one for pending async
 * i/o and one for "done" async i/o.   to do an async i/o one puts
 * a buf on the "pending" list (protected by splbio()), starts the
 * i/o and returns 0.    when the i/o is done, we expect
 * some sort of "i/o done" function to be called (at splbio(), interrupt
 * time).   this function should remove the buf from the pending list
 * and place it on the "done" list and wakeup the daemon.   the daemon
 * will run at normal spl() and will remove all items from the "done"
 * list and call the iodone hook for each done request (see uvm_pager.c).
 *
 * => return KERN_SUCCESS (aio finished, free it).  otherwise requeue for
 *	later collection.
 * => called with pageq's locked by the daemon.
 *
 * general outline:
 * - "try" to lock object.   if fail, just return (will try again later)
 * - drop "u_nio" (this req is done!)
 * - if (object->iosync && u_naio == 0) { wakeup &uvn->u_naio }
 * - get "page" structures (atop?).
 * - handle "wanted" pages
 * - handle "released" pages [using pgo_releasepg]
 *   >>> pgo_releasepg may kill the object
 * dont forget to look at "object" wanted flag in all cases.
 */


/*
 * uvn_flush: flush pages out of a uvm object.
 *
 * => "stop == 0" means flush all pages at or after "start".
 * => object should be locked by caller.   we may _unlock_ the object
 *	if (and only if) we need to clean a page (PGO_CLEANIT), or
 *	if PGO_SYNCIO is set and there are pages busy.
 *	we return with the object locked.
 * => if PGO_CLEANIT or PGO_SYNCIO is set, we may block (due to I/O).
 *	thus, a caller might want to unlock higher level resources
 *	(e.g. vm_map) before calling flush.
 * => if neither PGO_CLEANIT nor PGO_SYNCIO is set, then we will neither
 *	unlock the object nor block.
 * => if PGO_ALLPAGES is set, then all pages in the object are valid targets
 *	for flushing.
 * => NOTE: we rely on the fact that the object's memq is a TAILQ and
 *	that new pages are inserted on the tail end of the list.   thus,
 *	we can make a complete pass through the object in one go by starting
 *	at the head and working towards the tail (new pages are put in
 *	front of us).
 * => NOTE: we are allowed to lock the page queues, so the caller
 *	must not be holding the lock on them [e.g. pagedaemon had
 *	better not call us with the queues locked]
 * => we return TRUE unless we encountered some sort of I/O error
 *
 * comment on "cleaning" object and PG_BUSY pages:
 *	this routine is holding the lock on the object.   the only time
 *	that it can run into a PG_BUSY page that it does not own is if
 *	some other process has started I/O on the page (e.g. either
 *	a pagein, or a pageout).    if the PG_BUSY page is being paged
 *	in, then it can not be dirty (!PG_CLEAN) because no one has
 *	had a chance to modify it yet.    if the PG_BUSY page is being
 *	paged out then it means that someone else has already started
 *	cleaning the page for us (how nice!).    in this case, if we
 *	have syncio specified, then after we make our pass through the
 *	object we need to wait for the other PG_BUSY pages to clear
 *	off (i.e. we need to do an iosync).   also note that once a
 *	page is PG_BUSY it must stay in its object until it is un-busyed.
 *
 * note on page traversal:
 *	we can traverse the pages in an object either by going down the
 *	linked list in "uobj->memq", or we can go over the address range
 *	by page doing hash table lookups for each address.    depending
 *	on how many pages are in the object it may be cheaper to do one
 *	or the other.   we set "by_list" to true if we are using memq.
 *	if the cost of a hash lookup was equal to the cost of the list
 *	traversal we could compare the number of pages in the start->stop
 *	range to the total number of pages in the object.   however, it
 *	seems that a hash table lookup is more expensive than the linked
 *	list traversal, so we multiply the number of pages in the
 *	start->stop range by a penalty which we define below.
 */

#define UVN_HASH_PENALTY 4	/* XXX: a guess */

boolean_t
uvn_flush(uobj, start, stop, flags)
	struct uvm_object *uobj;
	voff_t start, stop;
	int flags;
{
	struct uvm_vnode *uvn = (struct uvm_vnode *)uobj;
	struct vnode *vp = (struct vnode *)uobj;
	struct vm_page *pp, *ppnext, *ptmp;
	struct vm_page *pps[256], **ppsp;
	int s;
	int npages, result, lcv;
	boolean_t retval, need_iosync, by_list, needs_clean, all, wasclean;
	boolean_t async = (flags & PGO_SYNCIO) == 0;
	voff_t curoff;
	u_short pp_version;
	UVMHIST_FUNC("uvn_flush"); UVMHIST_CALLED(maphist);
	UVMHIST_LOG(maphist, "uobj %p start 0x%x stop 0x%x flags 0x%x",
		    uobj, start, stop, flags);
	KASSERT(flags & (PGO_CLEANIT|PGO_FREE|PGO_DEACTIVATE));

	if (uobj->uo_npages == 0) {
		s = splbio();
		if (LIST_FIRST(&vp->v_dirtyblkhd) == NULL &&
		    (vp->v_bioflag & VBIOONSYNCLIST)) {
			vp->v_bioflag &= ~VBIOONSYNCLIST;
			LIST_REMOVE(vp, v_synclist);
		}
		splx(s);
		return TRUE;
	}

#ifdef DIAGNOSTIC
	if (uvn->u_size == VSIZENOTSET) {
		printf("uvn_flush: size not set vp %p\n", uvn);
		vprint("uvn_flush VSIZENOTSET", vp);
		flags |= PGO_ALLPAGES;
	}
#endif

	/*
	 * get init vals and determine how we are going to traverse object
	 */

	if (stop == 0) {
		stop = trunc_page(LLONG_MAX);
	}
	curoff = 0;
	need_iosync = FALSE;
	retval = TRUE;
	wasclean = TRUE;
	if (flags & PGO_ALLPAGES) {
		all = TRUE;
		by_list = TRUE;
	} else {
		start = trunc_page(start);
		stop = round_page(stop);
		all = FALSE;
		by_list = (uobj->uo_npages <=
		    ((stop - start) >> PAGE_SHIFT) * UVN_HASH_PENALTY);
	}

	UVMHIST_LOG(maphist,
	    " flush start=0x%x, stop=0x%x, by_list=%d, flags=0x%x",
	    start, stop, by_list, flags);

	/*
	 * PG_CLEANCHK: this bit is used by the pgo_mk_pcluster function as
	 * a _hint_ as to how up to date the PG_CLEAN bit is.   if the hint
	 * is wrong it will only prevent us from clustering... it won't break
	 * anything.   we clear all PG_CLEANCHK bits here, and pgo_mk_pcluster
	 * will set them as it syncs PG_CLEAN.   This is only an issue if we
	 * are looking at non-inactive pages (because inactive page's PG_CLEAN
	 * bit is always up to date since there are no mappings).
	 * [borrowed PG_CLEANCHK idea from FreeBSD VM]
	 */

	if ((flags & PGO_CLEANIT) != 0 &&
	    uobj->pgops->pgo_mk_pcluster != NULL) {
		if (by_list) {
			TAILQ_FOREACH(pp, &uobj->memq, listq) {
				if (!all &&
				    (pp->offset < start || pp->offset >= stop))
					continue;
				pp->flags &= ~PG_CLEANCHK;
			}

		} else {   /* by hash */
			for (curoff = start ; curoff < stop;
			    curoff += PAGE_SIZE) {
				pp = uvm_pagelookup(uobj, curoff);
				if (pp)
					pp->flags &= ~PG_CLEANCHK;
			}
		}
	}

	/*
	 * now do it.   note: we must update ppnext in body of loop or we
	 * will get stuck.  we need to use ppnext because we may free "pp"
	 * before doing the next loop.
	 */

	if (by_list) {
		pp = TAILQ_FIRST(&uobj->memq);
	} else {
		curoff = start;
		pp = uvm_pagelookup(uobj, curoff);
	}

	ppnext = NULL;
	ppsp = NULL;
	uvm_lock_pageq();

	/* locked: both page queues and uobj */
	for ( ; (by_list && pp != NULL) ||
		      (!by_list && curoff < stop) ; pp = ppnext) {
		if (by_list) {
			if (!all &&
			    (pp->offset < start || pp->offset >= stop)) {
				ppnext = TAILQ_NEXT(pp, listq);
				continue;
			}
		} else {
			curoff += PAGE_SIZE;
			if (pp == NULL) {
				if (curoff < stop)
					ppnext = uvm_pagelookup(uobj, curoff);
				continue;
			}
		}

		/*
		 * handle case where we do not need to clean page (either
		 * because we are not clean or because page is not dirty or
		 * is busy):
		 *
		 * NOTE: we are allowed to deactivate a non-wired active
		 * PG_BUSY page, but once a PG_BUSY page is on the inactive
		 * queue it must stay put until it is !PG_BUSY (so as not to
		 * confuse pagedaemon).
		 */

		if ((flags & PGO_CLEANIT) == 0 || (pp->flags & PG_BUSY) != 0) {
			needs_clean = FALSE;
			if (!async)
				need_iosync = TRUE;
		} else {

			/*
			 * freeing: nuke all mappings so we can sync
			 * PG_CLEAN bit with no race
			 */
			if ((pp->flags & PG_CLEAN) != 0 &&
			    (flags & PGO_FREE) != 0 &&
			    /* XXX ACTIVE|INACTIVE test unnecessary? */
			    (pp->pqflags & (PQ_ACTIVE|PQ_INACTIVE)) != 0)
				pmap_page_protect(pp, VM_PROT_NONE);
			if ((pp->flags & PG_CLEAN) != 0 &&
			    pmap_is_modified(pp))
				pp->flags &= ~(PG_CLEAN);
			pp->flags |= PG_CLEANCHK;
			needs_clean = ((pp->flags & PG_CLEAN) == 0);
		}

		/*
		 * if we don't need a clean... load ppnext and dispose of pp
		 */
		if (!needs_clean) {
			if (by_list)
				ppnext = TAILQ_NEXT(pp, listq);
			else {
				if (curoff < stop)
					ppnext = uvm_pagelookup(uobj, curoff);
			}

			if (flags & PGO_DEACTIVATE) {
				if ((pp->pqflags & PQ_INACTIVE) == 0 &&
				    (pp->flags & PG_BUSY) == 0 &&
				    pp->wire_count == 0) {
					pmap_clear_reference(pp);
					uvm_pagedeactivate(pp);
				}

			} else if (flags & PGO_FREE) {
				if (pp->flags & PG_BUSY) {
					pp->flags |= PG_RELEASED;
				} else {
					pmap_page_protect(pp, VM_PROT_NONE);
					uvm_pagefree(pp);
				}
			}
			/* ppnext is valid so we can continue... */
			continue;
		}

		/*
		 * pp points to a page in the locked object that we are
		 * working on.  if it is !PG_CLEAN,!PG_BUSY and we asked
		 * for cleaning (PGO_CLEANIT).  we clean it now.
		 *
		 * let uvm_pager_put attempted a clustered page out.
		 * note: locked: uobj and page queues.
		 */

		wasclean = FALSE;
		pp->flags |= PG_BUSY;	/* we 'own' page now */
		UVM_PAGE_OWN(pp, "uvn_flush");
		pmap_page_protect(pp, VM_PROT_READ);
		pp_version = pp->version;
		ppsp = pps;
		npages = sizeof(pps) / sizeof(struct vm_page *);

		/* locked: page queues, uobj */
		result = uvm_pager_put(uobj, pp, &ppsp, &npages,
				       flags | PGO_DOACTCLUST, start, stop);
		/* unlocked: page queues, uobj */

		/*
		 * at this point nothing is locked.   if we did an async I/O
		 * it is remotely possible for the async i/o to complete and
		 * the page "pp" be freed or what not before we get a chance
		 * to relock the object.   in order to detect this, we have
		 * saved the version number of the page in "pp_version".
		 */

		/* relock! */
		simple_lock(&uobj->vmobjlock);
		uvm_lock_pageq();

		/*
		 * the cleaning operation is now done.  finish up.  note that
		 * on error uvm_pager_put drops the cluster for us.
		 * on success uvm_pager_put returns the cluster to us in
		 * ppsp/npages.
		 */

		/*
		 * for pending async i/o if we are not deactivating/freeing
		 * we can move on to the next page.
		 */

		if (result == 0 && async &&
		    (flags & (PGO_DEACTIVATE|PGO_FREE)) == 0) {

			/*
			 * no per-page ops: refresh ppnext and continue
			 */
			if (by_list) {
				if (pp->version == pp_version)
					ppnext = TAILQ_NEXT(pp, listq);
				else
					ppnext = TAILQ_FIRST(&uobj->memq);
			} else {
				if (curoff < stop)
					ppnext = uvm_pagelookup(uobj, curoff);
			}
			continue;
		}

		/*
		 * need to look at each page of the I/O operation.  we defer
		 * processing "pp" until the last trip through this "for" loop
		 * so that we can load "ppnext" for the main loop after we
		 * play with the cluster pages [thus the "npages + 1" in the
		 * loop below].
		 */

		for (lcv = 0 ; lcv < npages + 1 ; lcv++) {

			/*
			 * handle ppnext for outside loop, and saving pp
			 * until the end.
			 */
			if (lcv < npages) {
				if (ppsp[lcv] == pp)
					continue; /* skip pp until the end */
				ptmp = ppsp[lcv];
			} else {
				ptmp = pp;

				/* set up next page for outer loop */
				if (by_list) {
					if (pp->version == pp_version)
						ppnext = TAILQ_NEXT(pp, listq);
					else
						ppnext = TAILQ_FIRST(
						    &uobj->memq);
				} else {
					if (curoff < stop)
						ppnext = uvm_pagelookup(uobj,
						    curoff);
				}
			}

			/*
			 * verify the page wasn't moved while obj was
			 * unlocked
			 */
			if (result == 0 && async && ptmp->uobject != uobj)
				continue;

			/*
			 * unbusy the page if I/O is done.   note that for
			 * async I/O it is possible that the I/O op
			 * finished before we relocked the object (in
			 * which case the page is no longer busy).
			 */

			if (result != 0 || !async) {
				if (ptmp->flags & PG_WANTED) {
					/* still holding object lock */
					wakeup(ptmp);
				}
				ptmp->flags &= ~(PG_WANTED|PG_BUSY);
				UVM_PAGE_OWN(ptmp, NULL);
				if (ptmp->flags & PG_RELEASED) {
					uvm_unlock_pageq();
					if (!uvn_releasepg(ptmp, NULL)) {
						UVMHIST_LOG(maphist,
							    "released %p",
							    ptmp, 0,0,0);
						return (TRUE);
					}
					uvm_lock_pageq();
					continue;
				} else {
					if ((flags & PGO_WEAK) == 0 &&
					    !(result == EIO &&
					      curproc == uvm.pagedaemon_proc)) {
						ptmp->flags |=
							(PG_CLEAN|PG_CLEANCHK);
						if ((flags & PGO_FREE) == 0) {
							pmap_clear_modify(ptmp);
						}
					}
				}
			}

			/*
			 * dispose of page
			 */

			if (flags & PGO_DEACTIVATE) {
				if ((pp->pqflags & PQ_INACTIVE) == 0 &&
				    (pp->flags & PG_BUSY) == 0 &&
				    pp->wire_count == 0) {
					pmap_clear_reference(ptmp);
					uvm_pagedeactivate(ptmp);
				}
			} else if (flags & PGO_FREE) {
				if (result == 0 && async) {
					if ((ptmp->flags & PG_BUSY) != 0)
						/* signal for i/o done */
						ptmp->flags |= PG_RELEASED;
				} else {
					if (result != 0) {
						printf("uvn_flush: obj=%p, "
						   "offset=0x%llx.  error %d\n",
						    pp->uobject,
						    (long long)pp->offset,
						    result);
						printf("uvn_flush: WARNING: "
						    "changes to page may be "
						    "lost!\n");
						retval = FALSE;
					}
					pmap_page_protect(ptmp, VM_PROT_NONE);
					uvm_pagefree(ptmp);
				}
			}
		}		/* end of "lcv" for loop */
	}		/* end of "pp" for loop */

	uvm_unlock_pageq();
	s = splbio();
	if ((flags & PGO_CLEANIT) && all && wasclean &&
	    LIST_FIRST(&vp->v_dirtyblkhd) == NULL &&
	    (vp->v_bioflag & VBIOONSYNCLIST)) {
		vp->v_bioflag &= ~VBIOONSYNCLIST;
		LIST_REMOVE(vp, v_synclist);
	}
	splx(s);
	if (need_iosync) {
		UVMHIST_LOG(maphist,"  <<DOING IOSYNC>>",0,0,0,0);

		/*
		 * XXX this doesn't use the new two-flag scheme,
		 * but to use that, all i/o initiators will have to change.
		 */

		s = splbio();
		while (vp->v_numoutput != 0) {
			UVMHIST_LOG(ubchist, "waiting for vp %p num %d",
				    vp, vp->v_numoutput,0,0);

	                vp->v_bioflag |= VBIOWAIT;
			UVM_UNLOCK_AND_WAIT(&vp->v_numoutput,
					    &uvn->u_obj.vmobjlock,
					    FALSE, "uvn_flush",0);
			simple_lock(&uvn->u_obj.vmobjlock);
		}
		splx(s);
	}

	/* return, with object locked! */
	UVMHIST_LOG(maphist,"<- done (retval=0x%x)",retval,0,0,0);
	return(retval);
}

/*
 * uvn_cluster
 *
 * we are about to do I/O in an object at offset.   this function is called
 * to establish a range of offsets around "offset" in which we can cluster
 * I/O.
 *
 * - currently doesn't matter if obj locked or not.
 */

static void
uvn_cluster(uobj, offset, loffset, hoffset)
	struct uvm_object *uobj;
	voff_t offset;
	voff_t *loffset, *hoffset; /* OUT */
{
	struct uvm_vnode *uvn = (struct uvm_vnode *)uobj;

	*loffset = offset;
	*hoffset = MIN(offset + MAXBSIZE, round_page(uvn->u_size));
}

/*
 * uvn_put: flush page data to backing store.
 *
 * => object must be locked!   we will _unlock_ it before starting I/O.
 * => flags: PGO_SYNCIO -- use sync. I/O
 * => note: caller must set PG_CLEAN and pmap_clear_modify (if needed)
 */

int
uvn_put(uobj, pps, npages, flags)
	struct uvm_object *uobj;
	struct vm_page **pps;
	int npages, flags;
{
	struct vnode *vp = (struct vnode *)uobj;
	int error;

	error = VOP_PUTPAGES(vp, pps, npages, flags, NULL);
	return error;
}


/*
 * uvn_get: get pages (synchronously) from backing store
 *
 * => prefer map unlocked (not required)
 * => object must be locked!  we will _unlock_ it before starting any I/O.
 * => flags: PGO_ALLPAGES: get all of the pages
 *           PGO_LOCKED: fault data structures are locked
 * => NOTE: offset is the offset of pps[0], _NOT_ pps[centeridx]
 * => NOTE: caller must check for released pages!!
 */

int
uvn_get(uobj, offset, pps, npagesp, centeridx, access_type, advice, flags)
	struct uvm_object *uobj;
	voff_t offset;
	struct vm_page **pps;		/* IN/OUT */
	int *npagesp;			/* IN (OUT if PGO_LOCKED) */
	int centeridx;
	vm_prot_t access_type;
	int advice, flags;
{
	struct vnode *vp = (struct vnode *)uobj;
	struct proc *p = curproc;
	int error;
	UVMHIST_FUNC("uvn_get"); UVMHIST_CALLED(ubchist);

	UVMHIST_LOG(ubchist, "vp %p off 0x%x", vp, (int)offset, 0,0);
	error = vn_lock(vp, LK_EXCLUSIVE|LK_RECURSEFAIL|LK_NOWAIT, p);
	if (error) {
		if (error == EBUSY)
			return EAGAIN;
		return error;
	}
	error = VOP_GETPAGES(vp, offset, pps, npagesp, centeridx,
		     access_type, advice, flags);
	VOP_UNLOCK(vp, LK_RELEASE, p);
	return error;
}


/*
 * uvn_findpages:
 * return the page for the uobj and offset requested, allocating if needed.
 * => uobj must be locked.
 * => returned page will be BUSY.
 */

void
uvn_findpages(uobj, offset, npagesp, pps, flags)
	struct uvm_object *uobj;
	voff_t offset;
	int *npagesp;
	struct vm_page **pps;
	int flags;
{
	int i, rv, npages;

	rv = 0;
	npages = *npagesp;
	for (i = 0; i < npages; i++, offset += PAGE_SIZE) {
		rv += uvn_findpage(uobj, offset, &pps[i], flags);
	}
	*npagesp = rv;
}

static int
uvn_findpage(uobj, offset, pgp, flags)
	struct uvm_object *uobj;
	voff_t offset;
	struct vm_page **pgp;
	int flags;
{
	struct vm_page *pg;
	int s;
	UVMHIST_FUNC("uvn_findpage"); UVMHIST_CALLED(ubchist);
	UVMHIST_LOG(ubchist, "vp %p off 0x%lx", uobj, offset,0,0);

	if (*pgp != NULL) {
		UVMHIST_LOG(ubchist, "dontcare", 0,0,0,0);
		return 0;
	}
	for (;;) {
		/* look for an existing page */
		pg = uvm_pagelookup(uobj, offset);

		/* nope?   allocate one now */
		if (pg == NULL) {
			if (flags & UFP_NOALLOC) {
				UVMHIST_LOG(ubchist, "noalloc", 0,0,0,0);
				return 0;
			}
			pg = uvm_pagealloc(uobj, offset, NULL, 0);
			if (pg == NULL) {
				if (flags & UFP_NOWAIT) {
					UVMHIST_LOG(ubchist, "nowait",0,0,0,0);
					return 0;
				}
				simple_unlock(&uobj->vmobjlock);
				uvm_wait("uvn_fp1");
				simple_lock(&uobj->vmobjlock);
				continue;
			}
			if (UVM_OBJ_IS_VTEXT(uobj)) {
				uvmexp.vtextpages++;
			} else {
				uvmexp.vnodepages++;
			}
			s = splbio();
			vhold((struct vnode *)uobj);
			splx(s);
			UVMHIST_LOG(ubchist, "alloced",0,0,0,0);
			break;
		} else if (flags & UFP_NOCACHE) {
			UVMHIST_LOG(ubchist, "nocache",0,0,0,0);
			return 0;
		}

		/* page is there, see if we need to wait on it */
		if ((pg->flags & (PG_BUSY|PG_RELEASED)) != 0) {
			if (flags & UFP_NOWAIT) {
				UVMHIST_LOG(ubchist, "nowait",0,0,0,0);
				return 0;
			}
			pg->flags |= PG_WANTED;
			UVM_UNLOCK_AND_WAIT(pg, &uobj->vmobjlock, 0,
					    "uvn_fp2", 0);
			simple_lock(&uobj->vmobjlock);
			continue;
		}

		/* skip PG_RDONLY pages if requested */
		if ((flags & UFP_NORDONLY) && (pg->flags & PG_RDONLY)) {
			UVMHIST_LOG(ubchist, "nordonly",0,0,0,0);
			return 0;
		}

		/* mark the page BUSY and we're done. */
		pg->flags |= PG_BUSY;
		UVM_PAGE_OWN(pg, "uvn_findpage");
		UVMHIST_LOG(ubchist, "found",0,0,0,0);
		break;
	}
	*pgp = pg;
	return 1;
}

/*
 * uvm_vnp_setsize: grow or shrink a vnode uvn
 *
 * grow   => just update size value
 * shrink => toss un-needed pages
 *
 * => we assume that the caller has a reference of some sort to the
 *	vnode in question so that it will not be yanked out from under
 *	us.
 *
 * called from:
 *  => truncate fns (ext2fs_truncate, ffs_truncate, detrunc[msdos])
 *  => "write" fns (ext2fs_write, WRITE [ufs/ufs], msdosfs_write, nfs_write)
 *  => ffs_balloc [XXX: why? doesn't WRITE handle?]
 *  => NFS: nfs_loadattrcache, nfs_getattrcache, nfs_setattr
 *  => union fs: union_newsize
 */

void
uvm_vnp_setsize(vp, newsize)
	struct vnode *vp;
	voff_t newsize;
{
	struct uvm_vnode *uvn = &vp->v_uvm;
	voff_t pgend = round_page(newsize);
	UVMHIST_FUNC("uvm_vnp_setsize"); UVMHIST_CALLED(ubchist);

	simple_lock(&uvn->u_obj.vmobjlock);

	UVMHIST_LOG(ubchist, "old 0x%x new 0x%x", uvn->u_size, newsize, 0,0);

	/*
	 * now check if the size has changed: if we shrink we had better
	 * toss some pages...
	 */

	if (uvn->u_size > pgend && uvn->u_size != VSIZENOTSET) {
		(void) uvn_flush(&uvn->u_obj, pgend, 0, PGO_FREE);
	}
	uvn->u_size = newsize;
	simple_unlock(&uvn->u_obj.vmobjlock);
}

/*
 * uvm_vnp_zerorange:  set a range of bytes in a file to zero.
 */

void
uvm_vnp_zerorange(vp, off, len)
	struct vnode *vp;
	off_t off;
	size_t len;
{
        void *win;

        /*
         * XXXUBC invent kzero() and use it
         */

        while (len) {
                vsize_t bytelen = len;

                win = ubc_alloc(&vp->v_uvm.u_obj, off, &bytelen, UBC_WRITE);
                memset(win, 0, bytelen);
                ubc_release(win, 0);

                off += bytelen;
                len -= bytelen;
        }
}
