/*	$OpenBSD: uvm_pdaemon.c,v 1.101 2022/06/28 19:31:30 mpi Exp $	*/
/*	$NetBSD: uvm_pdaemon.c,v 1.23 2000/08/20 10:24:14 bjh21 Exp $	*/

/*
 * Copyright (c) 1997 Charles D. Cranor and Washington University.
 * Copyright (c) 1991, 1993, The Regents of the University of California.
 *
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * The Mach Operating System project at Carnegie-Mellon University.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 *	@(#)vm_pageout.c        8.5 (Berkeley) 2/14/94
 * from: Id: uvm_pdaemon.c,v 1.1.2.32 1998/02/06 05:26:30 chs Exp
 *
 *
 * Copyright (c) 1987, 1990 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

/*
 * uvm_pdaemon.c: the page daemon
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/pool.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/mount.h>
#include <sys/atomic.h>

#ifdef HIBERNATE
#include <sys/hibernate.h>
#endif

#include <uvm/uvm.h>

#include "drm.h"

#if NDRM > 0
extern void drmbackoff(long);
#endif

/*
 * UVMPD_NUMDIRTYREACTS is how many dirty pages the pagedaemon will reactivate
 * in a pass thru the inactive list when swap is full.  the value should be
 * "small"... if it's too large we'll cycle the active pages thru the inactive
 * queue too quickly to for them to be referenced and avoid being freed.
 */

#define UVMPD_NUMDIRTYREACTS 16


/*
 * local prototypes
 */

void		uvmpd_scan(struct uvm_pmalloc *);
boolean_t	uvmpd_scan_inactive(struct uvm_pmalloc *, struct pglist *);
void		uvmpd_tune(void);
void		uvmpd_drop(struct pglist *);

/*
 * uvm_wait: wait (sleep) for the page daemon to free some pages
 *
 * => should be called with all locks released
 * => should _not_ be called by the page daemon (to avoid deadlock)
 */

void
uvm_wait(const char *wmsg)
{
	uint64_t timo = INFSLP;

#ifdef DIAGNOSTIC
	if (curproc == &proc0)
		panic("%s: cannot sleep for memory during boot", __func__);
#endif

	/*
	 * check for page daemon going to sleep (waiting for itself)
	 */
	if (curproc == uvm.pagedaemon_proc) {
		printf("uvm_wait emergency bufbackoff\n");
		if (bufbackoff(NULL, 4) == 0)
			return;
		/*
		 * now we have a problem: the pagedaemon wants to go to
		 * sleep until it frees more memory.   but how can it
		 * free more memory if it is asleep?  that is a deadlock.
		 * we have two options:
		 *  [1] panic now
		 *  [2] put a timeout on the sleep, thus causing the
		 *      pagedaemon to only pause (rather than sleep forever)
		 *
		 * note that option [2] will only help us if we get lucky
		 * and some other process on the system breaks the deadlock
		 * by exiting or freeing memory (thus allowing the pagedaemon
		 * to continue).  for now we panic if DEBUG is defined,
		 * otherwise we hope for the best with option [2] (better
		 * yet, this should never happen in the first place!).
		 */

		printf("pagedaemon: deadlock detected!\n");
		timo = MSEC_TO_NSEC(125);	/* set timeout */
#if defined(DEBUG)
		/* DEBUG: panic so we can debug it */
		panic("pagedaemon deadlock");
#endif
	}

	uvm_lock_fpageq();
	wakeup(&uvm.pagedaemon);		/* wake the daemon! */
	msleep_nsec(&uvmexp.free, &uvm.fpageqlock, PVM | PNORELOCK, wmsg, timo);
}

/*
 * uvmpd_tune: tune paging parameters
 *
 * => called whenever memory is added to (or removed from?) the system
 * => caller must call with page queues locked
 */

void
uvmpd_tune(void)
{

	uvmexp.freemin = uvmexp.npages / 30;

	/* between 16k and 512k */
	/* XXX:  what are these values good for? */
	uvmexp.freemin = max(uvmexp.freemin, (16*1024) >> PAGE_SHIFT);
#if 0
	uvmexp.freemin = min(uvmexp.freemin, (512*1024) >> PAGE_SHIFT);
#endif

	/* Make sure there's always a user page free. */
	if (uvmexp.freemin < uvmexp.reserve_kernel + 1)
		uvmexp.freemin = uvmexp.reserve_kernel + 1;

	uvmexp.freetarg = (uvmexp.freemin * 4) / 3;
	if (uvmexp.freetarg <= uvmexp.freemin)
		uvmexp.freetarg = uvmexp.freemin + 1;

	/* uvmexp.inactarg: computed in main daemon loop */

	uvmexp.wiredmax = uvmexp.npages / 3;
}

/*
 * Indicate to the page daemon that a nowait call failed and it should
 * recover at least some memory in the most restricted region (assumed
 * to be dma_constraint).
 */
volatile int uvm_nowait_failed;

/*
 * uvm_pageout: the main loop for the pagedaemon
 */
void
uvm_pageout(void *arg)
{
	struct uvm_constraint_range constraint;
	struct uvm_pmalloc *pma;
	int npages = 0;

	/* ensure correct priority and set paging parameters... */
	uvm.pagedaemon_proc = curproc;
	(void) spl0();
	uvm_lock_pageq();
	npages = uvmexp.npages;
	uvmpd_tune();
	uvm_unlock_pageq();

	for (;;) {
		long size;

		uvm_lock_fpageq();
		if (!uvm_nowait_failed && TAILQ_EMPTY(&uvm.pmr_control.allocs)) {
			msleep_nsec(&uvm.pagedaemon, &uvm.fpageqlock, PVM,
			    "pgdaemon", INFSLP);
			uvmexp.pdwoke++;
		}

		if ((pma = TAILQ_FIRST(&uvm.pmr_control.allocs)) != NULL) {
			pma->pm_flags |= UVM_PMA_BUSY;
			constraint = pma->pm_constraint;
		} else {
			if (uvm_nowait_failed) {
				/*
				 * XXX realisticly, this is what our
				 * nowait callers probably care about
				 */
				constraint = dma_constraint;
				uvm_nowait_failed = 0;
			} else
				constraint = no_constraint;
		}

		uvm_unlock_fpageq();

		/*
		 * now lock page queues and recompute inactive count
		 */
		uvm_lock_pageq();
		if (npages != uvmexp.npages) {	/* check for new pages? */
			npages = uvmexp.npages;
			uvmpd_tune();
		}

		uvmexp.inactarg = (uvmexp.active + uvmexp.inactive) / 3;
		if (uvmexp.inactarg <= uvmexp.freetarg) {
			uvmexp.inactarg = uvmexp.freetarg + 1;
		}

		/* Reclaim pages from the buffer cache if possible. */
		size = 0;
		if (pma != NULL)
			size += pma->pm_size >> PAGE_SHIFT;
		if (uvmexp.free - BUFPAGES_DEFICIT < uvmexp.freetarg)
			size += uvmexp.freetarg - (uvmexp.free -
			    BUFPAGES_DEFICIT);
		if (size == 0)
			size = 16; /* XXX */
		uvm_unlock_pageq();
		(void) bufbackoff(&constraint, size * 2);
#if NDRM > 0
		drmbackoff(size * 2);
#endif
		uvm_lock_pageq();

		/*
		 * scan if needed
		 */
		if (pma != NULL ||
		    ((uvmexp.free - BUFPAGES_DEFICIT) < uvmexp.freetarg) ||
		    ((uvmexp.inactive + BUFPAGES_INACT) < uvmexp.inactarg)) {
			uvmpd_scan(pma);
		}

		/*
		 * if there's any free memory to be had,
		 * wake up any waiters.
		 */
		uvm_lock_fpageq();
		if (uvmexp.free > uvmexp.reserve_kernel ||
		    uvmexp.paging == 0) {
			wakeup(&uvmexp.free);
		}

		if (pma != NULL) {
			/* 
			 * XXX If UVM_PMA_FREED isn't set, no pages
			 * were freed.  Should we set UVM_PMA_FAIL in
			 * that case?
			 */
			pma->pm_flags &= ~UVM_PMA_BUSY;
			if (pma->pm_flags & UVM_PMA_FREED) {
				pma->pm_flags &= ~UVM_PMA_LINKED;
				TAILQ_REMOVE(&uvm.pmr_control.allocs, pma,
				    pmq);
				wakeup(pma);
			}
		}
		uvm_unlock_fpageq();

		/*
		 * scan done.  unlock page queues (the only lock we are holding)
		 */
		uvm_unlock_pageq();

		sched_pause(yield);
	}
	/*NOTREACHED*/
}


/*
 * uvm_aiodone_daemon:  main loop for the aiodone daemon.
 */
void
uvm_aiodone_daemon(void *arg)
{
	int s, free;
	struct buf *bp, *nbp;

	uvm.aiodoned_proc = curproc;

	for (;;) {
		/*
		 * Check for done aio structures. If we've got structures to
		 * process, do so. Otherwise sleep while avoiding races.
		 */
		mtx_enter(&uvm.aiodoned_lock);
		while ((bp = TAILQ_FIRST(&uvm.aio_done)) == NULL)
			msleep_nsec(&uvm.aiodoned, &uvm.aiodoned_lock,
			    PVM, "aiodoned", INFSLP);
		/* Take the list for ourselves. */
		TAILQ_INIT(&uvm.aio_done);
		mtx_leave(&uvm.aiodoned_lock);

		/* process each i/o that's done. */
		free = uvmexp.free;
		while (bp != NULL) {
			if (bp->b_flags & B_PDAEMON) {
				uvmexp.paging -= bp->b_bufsize >> PAGE_SHIFT;
			}
			nbp = TAILQ_NEXT(bp, b_freelist);
			s = splbio();	/* b_iodone must by called at splbio */
			(*bp->b_iodone)(bp);
			splx(s);
			bp = nbp;

			sched_pause(yield);
		}
		uvm_lock_fpageq();
		wakeup(free <= uvmexp.reserve_kernel ? &uvm.pagedaemon :
		    &uvmexp.free);
		uvm_unlock_fpageq();
	}
}



/*
 * uvmpd_scan_inactive: scan an inactive list for pages to clean or free.
 *
 * => called with page queues locked
 * => we work on meeting our free target by converting inactive pages
 *    into free pages.
 * => we handle the building of swap-backed clusters
 * => we return TRUE if we are exiting because we met our target
 */

boolean_t
uvmpd_scan_inactive(struct uvm_pmalloc *pma, struct pglist *pglst)
{
	boolean_t retval = FALSE;	/* assume we haven't hit target */
	int free, result;
	struct vm_page *p, *nextpg;
	struct uvm_object *uobj;
	struct vm_page *pps[SWCLUSTPAGES], **ppsp;
	int npages;
	struct vm_page *swpps[SWCLUSTPAGES]; 	/* XXX: see below */
	int swnpages, swcpages;				/* XXX: see below */
	int swslot;
	struct vm_anon *anon;
	boolean_t swap_backed;
	vaddr_t start;
	int dirtyreacts;

	/*
	 * swslot is non-zero if we are building a swap cluster.  we want
	 * to stay in the loop while we have a page to scan or we have
	 * a swap-cluster to build.
	 */
	swslot = 0;
	swnpages = swcpages = 0;
	free = 0;
	dirtyreacts = 0;
	p = NULL;

	/* Start with the first page on the list that fit in pma's ranges */
	if (pma != NULL) {
		paddr_t paddr;

		TAILQ_FOREACH(p, pglst, pageq) {
			paddr = atop(VM_PAGE_TO_PHYS(p));
			if (paddr >= pma->pm_constraint.ucr_low &&
			    paddr < pma->pm_constraint.ucr_high)
				break;
		}

	}

	if (p == NULL) {
		p = TAILQ_FIRST(pglst);
		pma = NULL;
	}

	for (; p != NULL || swslot != 0; p = nextpg) {
		/*
		 * note that p can be NULL iff we have traversed the whole
		 * list and need to do one final swap-backed clustered pageout.
		 */
		uobj = NULL;
		anon = NULL;

		if (p) {
			/*
			 * update our copy of "free" and see if we've met
			 * our target
			 */
			free = uvmexp.free - BUFPAGES_DEFICIT;
			if (((pma == NULL || (pma->pm_flags & UVM_PMA_FREED)) &&
			    (free + uvmexp.paging >= uvmexp.freetarg << 2)) ||
			    dirtyreacts == UVMPD_NUMDIRTYREACTS) {
				retval = TRUE;

				if (swslot == 0) {
					/* exit now if no swap-i/o pending */
					break;
				}

				/* set p to null to signal final swap i/o */
				p = NULL;
			}
		}

		if (p) {	/* if (we have a new page to consider) */
			/*
			 * we are below target and have a new page to consider.
			 */
			uvmexp.pdscans++;
			nextpg = TAILQ_NEXT(p, pageq);

			if (p->pg_flags & PQ_ANON) {
				anon = p->uanon;
				KASSERT(anon != NULL);
				if (rw_enter(anon->an_lock,
				    RW_WRITE|RW_NOSLEEP)) {
					/* lock failed, skip this page */
					continue;
				}
				/*
				 * move referenced pages back to active queue
				 * and skip to next page.
				 */
				if (pmap_is_referenced(p)) {
					uvm_pageactivate(p);
					rw_exit(anon->an_lock);
					uvmexp.pdreact++;
					continue;
				}
				if (p->pg_flags & PG_BUSY) {
					rw_exit(anon->an_lock);
					uvmexp.pdbusy++;
					/* someone else owns page, skip it */
					continue;
				}
				uvmexp.pdanscan++;
			} else {
				uobj = p->uobject;
				KASSERT(uobj != NULL);
				if (rw_enter(uobj->vmobjlock,
				    RW_WRITE|RW_NOSLEEP)) {
					/* lock failed, skip this page */
					continue;
				}
				/*
				 * move referenced pages back to active queue
				 * and skip to next page.
				 */
				if (pmap_is_referenced(p)) {
					uvm_pageactivate(p);
					rw_exit(uobj->vmobjlock);
					uvmexp.pdreact++;
					continue;
				}
				if (p->pg_flags & PG_BUSY) {
					rw_exit(uobj->vmobjlock);
					uvmexp.pdbusy++;
					/* someone else owns page, skip it */
					continue;
				}
				uvmexp.pdobscan++;
			}

			/*
			 * we now have the page queues locked.
			 * the page is not busy.   if the page is clean we
			 * can free it now and continue.
			 */
			if (p->pg_flags & PG_CLEAN) {
				if (p->pg_flags & PQ_SWAPBACKED) {
					/* this page now lives only in swap */
					atomic_inc_int(&uvmexp.swpgonly);
				}

				/* zap all mappings with pmap_page_protect... */
				pmap_page_protect(p, PROT_NONE);
				uvm_pagefree(p);
				uvmexp.pdfreed++;

				if (anon) {

					/*
					 * an anonymous page can only be clean
					 * if it has backing store assigned.
					 */

					KASSERT(anon->an_swslot != 0);

					/* remove from object */
					anon->an_page = NULL;
					rw_exit(anon->an_lock);
				} else {
					rw_exit(uobj->vmobjlock);
				}
				continue;
			}

			/*
			 * this page is dirty, skip it if we'll have met our
			 * free target when all the current pageouts complete.
			 */
			if ((pma == NULL || (pma->pm_flags & UVM_PMA_FREED)) &&
			    (free + uvmexp.paging > uvmexp.freetarg << 2)) {
				if (anon) {
					rw_exit(anon->an_lock);
				} else {
					rw_exit(uobj->vmobjlock);
				}
				continue;
			}

			/*
			 * this page is dirty, but we can't page it out
			 * since all pages in swap are only in swap.
			 * reactivate it so that we eventually cycle
			 * all pages thru the inactive queue.
			 */
			if ((p->pg_flags & PQ_SWAPBACKED) && uvm_swapisfull()) {
				dirtyreacts++;
				uvm_pageactivate(p);
				if (anon) {
					rw_exit(anon->an_lock);
				} else {
					rw_exit(uobj->vmobjlock);
				}
				continue;
			}

			/*
			 * if the page is swap-backed and dirty and swap space
			 * is full, free any swap allocated to the page
			 * so that other pages can be paged out.
			 */
			KASSERT(uvmexp.swpginuse <= uvmexp.swpages);
			if ((p->pg_flags & PQ_SWAPBACKED) &&
			    uvmexp.swpginuse == uvmexp.swpages) {

				if ((p->pg_flags & PQ_ANON) &&
				    p->uanon->an_swslot) {
					uvm_swap_free(p->uanon->an_swslot, 1);
					p->uanon->an_swslot = 0;
				}
				if (p->pg_flags & PQ_AOBJ) {
					uao_dropswap(p->uobject,
						     p->offset >> PAGE_SHIFT);
				}
			}

			/*
			 * the page we are looking at is dirty.   we must
			 * clean it before it can be freed.  to do this we
			 * first mark the page busy so that no one else will
			 * touch the page.   we write protect all the mappings
			 * of the page so that no one touches it while it is
			 * in I/O.
			 */

			swap_backed = ((p->pg_flags & PQ_SWAPBACKED) != 0);
			atomic_setbits_int(&p->pg_flags, PG_BUSY);
			UVM_PAGE_OWN(p, "scan_inactive");
			pmap_page_protect(p, PROT_READ);
			uvmexp.pgswapout++;

			/*
			 * for swap-backed pages we need to (re)allocate
			 * swap space.
			 */
			if (swap_backed) {
				/* free old swap slot (if any) */
				if (anon) {
					if (anon->an_swslot) {
						uvm_swap_free(anon->an_swslot,
						    1);
						anon->an_swslot = 0;
					}
				} else {
					uao_dropswap(uobj,
						     p->offset >> PAGE_SHIFT);
				}

				/* start new cluster (if necessary) */
				if (swslot == 0) {
					swnpages = SWCLUSTPAGES;
					swslot = uvm_swap_alloc(&swnpages,
					    TRUE);
					if (swslot == 0) {
						/* no swap?  give up! */
						atomic_clearbits_int(
						    &p->pg_flags,
						    PG_BUSY);
						UVM_PAGE_OWN(p, NULL);
						if (anon)
							rw_exit(anon->an_lock);
						else
							rw_exit(
							    uobj->vmobjlock);
						continue;
					}
					swcpages = 0;	/* cluster is empty */
				}

				/* add block to cluster */
				swpps[swcpages] = p;
				if (anon)
					anon->an_swslot = swslot + swcpages;
				else
					uao_set_swslot(uobj,
					    p->offset >> PAGE_SHIFT,
					    swslot + swcpages);
				swcpages++;
			}
		} else {
			/* if p == NULL we must be doing a last swap i/o */
			swap_backed = TRUE;
		}

		/*
		 * now consider doing the pageout.
		 *
		 * for swap-backed pages, we do the pageout if we have either
		 * filled the cluster (in which case (swnpages == swcpages) or
		 * run out of pages (p == NULL).
		 *
		 * for object pages, we always do the pageout.
		 */
		if (swap_backed) {
			if (p) {	/* if we just added a page to cluster */
				if (anon)
					rw_exit(anon->an_lock);
				else
					rw_exit(uobj->vmobjlock);

				/* cluster not full yet? */
				if (swcpages < swnpages)
					continue;
			}

			/* starting I/O now... set up for it */
			npages = swcpages;
			ppsp = swpps;
			/* for swap-backed pages only */
			start = (vaddr_t) swslot;

			/* if this is final pageout we could have a few
			 * extra swap blocks */
			if (swcpages < swnpages) {
				uvm_swap_free(swslot + swcpages,
				    (swnpages - swcpages));
			}
		} else {
			/* normal object pageout */
			ppsp = pps;
			npages = sizeof(pps) / sizeof(struct vm_page *);
			/* not looked at because PGO_ALLPAGES is set */
			start = 0;
		}

		/*
		 * now do the pageout.
		 *
		 * for swap_backed pages we have already built the cluster.
		 * for !swap_backed pages, uvm_pager_put will call the object's
		 * "make put cluster" function to build a cluster on our behalf.
		 *
		 * we pass the PGO_PDFREECLUST flag to uvm_pager_put to instruct
		 * it to free the cluster pages for us on a successful I/O (it
		 * always does this for un-successful I/O requests).  this
		 * allows us to do clustered pageout without having to deal
		 * with cluster pages at this level.
		 *
		 * note locking semantics of uvm_pager_put with PGO_PDFREECLUST:
		 *  IN: locked: page queues
		 * OUT: locked: 
		 *     !locked: pageqs
		 */

		uvmexp.pdpageouts++;
		result = uvm_pager_put(swap_backed ? NULL : uobj, p,
		    &ppsp, &npages, PGO_ALLPAGES|PGO_PDFREECLUST, start, 0);

		/*
		 * if we did i/o to swap, zero swslot to indicate that we are
		 * no longer building a swap-backed cluster.
		 */

		if (swap_backed)
			swslot = 0;		/* done with this cluster */

		/*
		 * first, we check for VM_PAGER_PEND which means that the
		 * async I/O is in progress and the async I/O done routine
		 * will clean up after us.   in this case we move on to the
		 * next page.
		 *
		 * there is a very remote chance that the pending async i/o can
		 * finish _before_ we get here.   if that happens, our page "p"
		 * may no longer be on the inactive queue.   so we verify this
		 * when determining the next page (starting over at the head if
		 * we've lost our inactive page).
		 */

		if (result == VM_PAGER_PEND) {
			uvmexp.paging += npages;
			uvm_lock_pageq();
			uvmexp.pdpending++;
			if (p) {
				if (p->pg_flags & PQ_INACTIVE)
					nextpg = TAILQ_NEXT(p, pageq);
				else
					nextpg = TAILQ_FIRST(pglst);
			} else {
				nextpg = NULL;
			}
			continue;
		}

		/* clean up "p" if we have one */
		if (p) {
			/*
			 * the I/O request to "p" is done and uvm_pager_put
			 * has freed any cluster pages it may have allocated
			 * during I/O.  all that is left for us to do is
			 * clean up page "p" (which is still PG_BUSY).
			 *
			 * our result could be one of the following:
			 *   VM_PAGER_OK: successful pageout
			 *
			 *   VM_PAGER_AGAIN: tmp resource shortage, we skip
			 *     to next page
			 *   VM_PAGER_{FAIL,ERROR,BAD}: an error.   we
			 *     "reactivate" page to get it out of the way (it
			 *     will eventually drift back into the inactive
			 *     queue for a retry).
			 *   VM_PAGER_UNLOCK: should never see this as it is
			 *     only valid for "get" operations
			 */

			/* relock p's object: page queues not lock yet, so
			 * no need for "try" */

			/* !swap_backed case: already locked... */
			if (swap_backed) {
				if (anon)
					rw_enter(anon->an_lock, RW_WRITE);
				else
					rw_enter(uobj->vmobjlock, RW_WRITE);
			}

#ifdef DIAGNOSTIC
			if (result == VM_PAGER_UNLOCK)
				panic("pagedaemon: pageout returned "
				    "invalid 'unlock' code");
#endif

			/* handle PG_WANTED now */
			if (p->pg_flags & PG_WANTED)
				wakeup(p);

			atomic_clearbits_int(&p->pg_flags, PG_BUSY|PG_WANTED);
			UVM_PAGE_OWN(p, NULL);

			/* released during I/O? Can only happen for anons */
			if (p->pg_flags & PG_RELEASED) {
				KASSERT(anon != NULL);
				/*
				 * remove page so we can get nextpg,
				 * also zero out anon so we don't use
				 * it after the free.
				 */
				anon->an_page = NULL;
				p->uanon = NULL;

				rw_exit(anon->an_lock);
				uvm_anfree(anon);	/* kills anon */
				pmap_page_protect(p, PROT_NONE);
				anon = NULL;
				uvm_lock_pageq();
				nextpg = TAILQ_NEXT(p, pageq);
				/* free released page */
				uvm_pagefree(p);
			} else {	/* page was not released during I/O */
				uvm_lock_pageq();
				nextpg = TAILQ_NEXT(p, pageq);
				if (result != VM_PAGER_OK) {
					/* pageout was a failure... */
					if (result != VM_PAGER_AGAIN)
						uvm_pageactivate(p);
					pmap_clear_reference(p);
					/* XXXCDC: if (swap_backed) FREE p's
					 * swap block? */
				} else {
					/* pageout was a success... */
					pmap_clear_reference(p);
					pmap_clear_modify(p);
					atomic_setbits_int(&p->pg_flags,
					    PG_CLEAN);
				}
			}

			/*
			 * drop object lock (if there is an object left).   do
			 * a safety check of nextpg to make sure it is on the
			 * inactive queue (it should be since PG_BUSY pages on
			 * the inactive queue can't be re-queued [note: not
			 * true for active queue]).
			 */
			if (anon)
				rw_exit(anon->an_lock);
			else if (uobj)
				rw_exit(uobj->vmobjlock);

			if (nextpg && (nextpg->pg_flags & PQ_INACTIVE) == 0) {
				nextpg = TAILQ_FIRST(pglst);	/* reload! */
			}
		} else {
			/*
			 * if p is null in this loop, make sure it stays null
			 * in the next loop.
			 */
			nextpg = NULL;

			/*
			 * lock page queues here just so they're always locked
			 * at the end of the loop.
			 */
			uvm_lock_pageq();
		}
	}
	return (retval);
}

/*
 * uvmpd_scan: scan the page queues and attempt to meet our targets.
 *
 * => called with pageq's locked
 */

void
uvmpd_scan(struct uvm_pmalloc *pma)
{
	int free, inactive_shortage, swap_shortage, pages_freed;
	struct vm_page *p, *nextpg;
	struct uvm_object *uobj;
	struct vm_anon *anon;
	struct rwlock *slock;

	MUTEX_ASSERT_LOCKED(&uvm.pageqlock);

	uvmexp.pdrevs++;		/* counter */
	uobj = NULL;

	/*
	 * get current "free" page count
	 */
	free = uvmexp.free - BUFPAGES_DEFICIT;

#ifndef __SWAP_BROKEN
	/*
	 * swap out some processes if we are below our free target.
	 * we need to unlock the page queues for this.
	 */
	if (free < uvmexp.freetarg) {
		uvmexp.pdswout++;
		uvm_unlock_pageq();
		uvm_swapout_threads();
		uvm_lock_pageq();
	}
#endif

	/*
	 * now we want to work on meeting our targets.   first we work on our
	 * free target by converting inactive pages into free pages.  then
	 * we work on meeting our inactive target by converting active pages
	 * to inactive ones.
	 */

	/*
	 * alternate starting queue between swap and object based on the
	 * low bit of uvmexp.pdrevs (which we bump by one each call).
	 */
	pages_freed = uvmexp.pdfreed;
	(void) uvmpd_scan_inactive(pma, &uvm.page_inactive);
	pages_freed = uvmexp.pdfreed - pages_freed;

	/*
	 * we have done the scan to get free pages.   now we work on meeting
	 * our inactive target.
	 */
	inactive_shortage = uvmexp.inactarg - uvmexp.inactive - BUFPAGES_INACT;

	/*
	 * detect if we're not going to be able to page anything out
	 * until we free some swap resources from active pages.
	 */
	free = uvmexp.free - BUFPAGES_DEFICIT;
	swap_shortage = 0;
	if (free < uvmexp.freetarg &&
	    uvmexp.swpginuse == uvmexp.swpages &&
	    !uvm_swapisfull() &&
	    pages_freed == 0) {
		swap_shortage = uvmexp.freetarg - free;
	}

	for (p = TAILQ_FIRST(&uvm.page_active);
	     p != NULL && (inactive_shortage > 0 || swap_shortage > 0);
	     p = nextpg) {
		nextpg = TAILQ_NEXT(p, pageq);
		if (p->pg_flags & PG_BUSY) {
			continue;
		}

		/*
		 * lock the page's owner.
		 */
		if (p->uobject != NULL) {
			uobj = p->uobject;
			slock = uobj->vmobjlock;
			if (rw_enter(slock, RW_WRITE|RW_NOSLEEP)) {
				continue;
			}
		} else {
			anon = p->uanon;
			KASSERT(p->uanon != NULL);
			slock = anon->an_lock;
			if (rw_enter(slock, RW_WRITE|RW_NOSLEEP)) {
				continue;
			}
		}

		/*
		 * skip this page if it's busy.
		 */
		if ((p->pg_flags & PG_BUSY) != 0) {
			rw_exit(slock);
			continue;
		}

		/*
		 * if there's a shortage of swap, free any swap allocated
		 * to this page so that other pages can be paged out.
		 */
		if (swap_shortage > 0) {
			if ((p->pg_flags & PQ_ANON) && p->uanon->an_swslot) {
				uvm_swap_free(p->uanon->an_swslot, 1);
				p->uanon->an_swslot = 0;
				atomic_clearbits_int(&p->pg_flags, PG_CLEAN);
				swap_shortage--;
			}
			if (p->pg_flags & PQ_AOBJ) {
				int slot = uao_set_swslot(p->uobject,
					p->offset >> PAGE_SHIFT, 0);
				if (slot) {
					uvm_swap_free(slot, 1);
					atomic_clearbits_int(&p->pg_flags,
					    PG_CLEAN);
					swap_shortage--;
				}
			}
		}

		/*
		 * deactivate this page if there's a shortage of
		 * inactive pages.
		 */
		if (inactive_shortage > 0) {
			pmap_page_protect(p, PROT_NONE);
			/* no need to check wire_count as pg is "active" */
			uvm_pagedeactivate(p);
			uvmexp.pddeact++;
			inactive_shortage--;
		}

		/*
		 * we're done with this page.
		 */
		rw_exit(slock);
	}
}

#ifdef HIBERNATE

/*
 * uvmpd_drop: drop clean pages from list
 */
void
uvmpd_drop(struct pglist *pglst)
{
	struct vm_page *p, *nextpg;

	for (p = TAILQ_FIRST(pglst); p != NULL; p = nextpg) {
		nextpg = TAILQ_NEXT(p, pageq);

		if (p->pg_flags & PQ_ANON || p->uobject == NULL)
			continue;

		if (p->pg_flags & PG_BUSY)
			continue;

		if (p->pg_flags & PG_CLEAN) {
			struct uvm_object * uobj = p->uobject;

			rw_enter(uobj->vmobjlock, RW_WRITE);
			uvm_lock_pageq();
			/*
			 * we now have the page queues locked.
			 * the page is not busy.   if the page is clean we
			 * can free it now and continue.
			 */
			if (p->pg_flags & PG_CLEAN) {
				if (p->pg_flags & PQ_SWAPBACKED) {
					/* this page now lives only in swap */
					atomic_inc_int(&uvmexp.swpgonly);
				}

				/* zap all mappings with pmap_page_protect... */
				pmap_page_protect(p, PROT_NONE);
				uvm_pagefree(p);
			}
			uvm_unlock_pageq();
			rw_exit(uobj->vmobjlock);
		}
	}
}

void
uvmpd_hibernate(void)
{
	uvmpd_drop(&uvm.page_inactive);
	uvmpd_drop(&uvm.page_active);
}

#endif
