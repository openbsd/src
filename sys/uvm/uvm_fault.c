/*	$OpenBSD: uvm_fault.c,v 1.61 2011/06/23 21:52:42 oga Exp $	*/
/*	$NetBSD: uvm_fault.c,v 1.51 2000/08/06 00:22:53 thorpej Exp $	*/

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
 * from: Id: uvm_fault.c,v 1.1.2.23 1998/02/06 05:29:05 chs Exp
 */

/*
 * uvm_fault.c: fault handler
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/mman.h>
#include <sys/user.h>

#include <uvm/uvm.h>

/*
 *
 * a word on page faults:
 *
 * types of page faults we handle:
 *
 * CASE 1: upper layer faults                   CASE 2: lower layer faults
 *
 *    CASE 1A         CASE 1B                  CASE 2A        CASE 2B
 *    read/write1     write>1                  read/write   +-cow_write/zero
 *         |             |                         |        |        
 *      +--|--+       +--|--+     +-----+       +  |  +     | +-----+
 * amap |  V  |       |  ----------->new|          |        | |  ^  |
 *      +-----+       +-----+     +-----+       +  |  +     | +--|--+
 *                                                 |        |    |
 *      +-----+       +-----+                   +--|--+     | +--|--+
 * uobj | d/c |       | d/c |                   |  V  |     +----|  |
 *      +-----+       +-----+                   +-----+       +-----+
 *
 * d/c = don't care
 * 
 *   case [0]: layerless fault
 *	no amap or uobj is present.   this is an error.
 *
 *   case [1]: upper layer fault [anon active]
 *     1A: [read] or [write with anon->an_ref == 1]
 *		I/O takes place in top level anon and uobj is not touched.
 *     1B: [write with anon->an_ref > 1]
 *		new anon is alloc'd and data is copied off ["COW"]
 *
 *   case [2]: lower layer fault [uobj]
 *     2A: [read on non-NULL uobj] or [write to non-copy_on_write area]
 *		I/O takes place directly in object.
 *     2B: [write to copy_on_write] or [read on NULL uobj]
 *		data is "promoted" from uobj to a new anon.   
 *		if uobj is null, then we zero fill.
 *
 * we follow the standard UVM locking protocol ordering:
 *
 * MAPS => AMAP => UOBJ => ANON => PAGE QUEUES (PQ) 
 * we hold a PG_BUSY page if we unlock for I/O
 *
 *
 * the code is structured as follows:
 *  
 *     - init the "IN" params in the ufi structure
 *   ReFault:
 *     - do lookups [locks maps], check protection, handle needs_copy
 *     - check for case 0 fault (error)
 *     - establish "range" of fault
 *     - if we have an amap lock it and extract the anons
 *     - if sequential advice deactivate pages behind us
 *     - at the same time check pmap for unmapped areas and anon for pages
 *	 that we could map in (and do map it if found)
 *     - check object for resident pages that we could map in
 *     - if (case 2) goto Case2
 *     - >>> handle case 1
 *           - ensure source anon is resident in RAM
 *           - if case 1B alloc new anon and copy from source
 *           - map the correct page in
 *   Case2:
 *     - >>> handle case 2
 *           - ensure source page is resident (if uobj)
 *           - if case 2B alloc new anon and copy from source (could be zero
 *		fill if uobj == NULL)
 *           - map the correct page in
 *     - done!
 *
 * note on paging:
 *   if we have to do I/O we place a PG_BUSY page in the correct object,
 * unlock everything, and do the I/O.   when I/O is done we must reverify
 * the state of the world before assuming that our data structures are
 * valid.   [because mappings could change while the map is unlocked]
 *
 *  alternative 1: unbusy the page in question and restart the page fault
 *    from the top (ReFault).   this is easy but does not take advantage
 *    of the information that we already have from our previous lookup, 
 *    although it is possible that the "hints" in the vm_map will help here.
 *
 * alternative 2: the system already keeps track of a "version" number of
 *    a map.   [i.e. every time you write-lock a map (e.g. to change a
 *    mapping) you bump the version number up by one...]   so, we can save
 *    the version number of the map before we release the lock and start I/O.
 *    then when I/O is done we can relock and check the version numbers
 *    to see if anything changed.    this might save us some over 1 because
 *    we don't have to unbusy the page and may be less compares(?).
 *
 * alternative 3: put in backpointers or a way to "hold" part of a map
 *    in place while I/O is in progress.   this could be complex to
 *    implement (especially with structures like amap that can be referenced
 *    by multiple map entries, and figuring out what should wait could be
 *    complex as well...).
 *
 * given that we are not currently multiprocessor or multithreaded we might
 * as well choose alternative 2 now.   maybe alternative 3 would be useful
 * in the future.    XXX keep in mind for future consideration//rechecking.
 */

/*
 * local data structures
 */

struct uvm_advice {
	int advice;
	int nback;
	int nforw;
};

/*
 * page range array:
 * note: index in array must match "advice" value 
 * XXX: borrowed numbers from freebsd.   do they work well for us?
 */

static struct uvm_advice uvmadvice[] = {
	{ MADV_NORMAL, 3, 4 },
	{ MADV_RANDOM, 0, 0 },
	{ MADV_SEQUENTIAL, 8, 7},
};

#define UVM_MAXRANGE 16	/* must be max() of nback+nforw+1 */

/*
 * private prototypes
 */

static void uvmfault_amapcopy(struct uvm_faultinfo *);
static __inline void uvmfault_anonflush(struct vm_anon **, int);
void	uvmfault_unlockmaps(struct uvm_faultinfo *, boolean_t);

/*
 * inline functions
 */

/*
 * uvmfault_anonflush: try and deactivate pages in specified anons
 *
 * => does not have to deactivate page if it is busy
 */

static __inline void
uvmfault_anonflush(struct vm_anon **anons, int n)
{
	int lcv;
	struct vm_page *pg;
	
	for (lcv = 0 ; lcv < n ; lcv++) {
		if (anons[lcv] == NULL)
			continue;
		simple_lock(&anons[lcv]->an_lock);
		pg = anons[lcv]->an_page;
		if (pg && (pg->pg_flags & PG_BUSY) == 0 && pg->loan_count == 0) {
			uvm_lock_pageq();
			if (pg->wire_count == 0) {
#ifdef UBC
				pmap_clear_reference(pg);
#else
				pmap_page_protect(pg, VM_PROT_NONE);
#endif
				uvm_pagedeactivate(pg);
			}
			uvm_unlock_pageq();
		}
		simple_unlock(&anons[lcv]->an_lock);
	}
}

/*
 * normal functions
 */

/*
 * uvmfault_amapcopy: clear "needs_copy" in a map.
 *
 * => called with VM data structures unlocked (usually, see below)
 * => we get a write lock on the maps and clear needs_copy for a VA
 * => if we are out of RAM we sleep (waiting for more)
 */

static void
uvmfault_amapcopy(struct uvm_faultinfo *ufi)
{

	/*
	 * while we haven't done the job
	 */

	while (1) {

		/*
		 * no mapping?  give up.
		 */

		if (uvmfault_lookup(ufi, TRUE) == FALSE)
			return;

		/*
		 * copy if needed.
		 */

		if (UVM_ET_ISNEEDSCOPY(ufi->entry))
			amap_copy(ufi->map, ufi->entry, M_NOWAIT, TRUE, 
				ufi->orig_rvaddr, ufi->orig_rvaddr + 1);

		/*
		 * didn't work?  must be out of RAM.   unlock and sleep.
		 */

		if (UVM_ET_ISNEEDSCOPY(ufi->entry)) {
			uvmfault_unlockmaps(ufi, TRUE);
			uvm_wait("fltamapcopy");
			continue;
		}

		/*
		 * got it!   unlock and return.
		 */
		
		uvmfault_unlockmaps(ufi, TRUE);
		return;
	}
	/*NOTREACHED*/
}

/*
 * uvmfault_anonget: get data in an anon into a non-busy, non-released
 * page in that anon.
 *
 * => maps, amap, and anon locked by caller.
 * => if we fail (result != VM_PAGER_OK) we unlock everything.
 * => if we are successful, we return with everything still locked.
 * => we don't move the page on the queues [gets moved later]
 * => if we allocate a new page [we_own], it gets put on the queues.
 *    either way, the result is that the page is on the queues at return time
 * => for pages which are on loan from a uvm_object (and thus are not
 *    owned by the anon): if successful, we return with the owning object
 *    locked.   the caller must unlock this object when it unlocks everything
 *    else.
 */

int
uvmfault_anonget(struct uvm_faultinfo *ufi, struct vm_amap *amap,
    struct vm_anon *anon)
{
	boolean_t we_own;	/* we own anon's page? */
	boolean_t locked;	/* did we relock? */
	struct vm_page *pg;
	int result;
	UVMHIST_FUNC("uvmfault_anonget"); UVMHIST_CALLED(maphist);

	result = 0;		/* XXX shut up gcc */
	uvmexp.fltanget++;
        /* bump rusage counters */
	if (anon->an_page)
		curproc->p_addr->u_stats.p_ru.ru_minflt++;
	else
		curproc->p_addr->u_stats.p_ru.ru_majflt++;

	/* 
	 * loop until we get it, or fail.
	 */

	while (1) {

		we_own = FALSE;		/* TRUE if we set PG_BUSY on a page */
		pg = anon->an_page;

		/*
		 * if there is a resident page and it is loaned, then anon
		 * may not own it.   call out to uvm_anon_lockpage() to ensure
		 * the real owner of the page has been identified and locked.
		 */

		if (pg && pg->loan_count)
			pg = uvm_anon_lockloanpg(anon);

		/*
		 * page there?   make sure it is not busy/released.
		 */

		if (pg) {

			/*
			 * at this point, if the page has a uobject [meaning
			 * we have it on loan], then that uobject is locked
			 * by us!   if the page is busy, we drop all the
			 * locks (including uobject) and try again.
			 */

			if ((pg->pg_flags & (PG_BUSY|PG_RELEASED)) == 0) {
				UVMHIST_LOG(maphist, "<- OK",0,0,0,0);
				return (VM_PAGER_OK);
			}
			atomic_setbits_int(&pg->pg_flags, PG_WANTED);
			uvmexp.fltpgwait++;

			/*
			 * the last unlock must be an atomic unlock+wait on
			 * the owner of page
			 */
			if (pg->uobject) {	/* owner is uobject ? */
				uvmfault_unlockall(ufi, amap, NULL, anon);
				UVMHIST_LOG(maphist, " unlock+wait on uobj",0,
				    0,0,0);
				UVM_UNLOCK_AND_WAIT(pg,
				    &pg->uobject->vmobjlock,
				    FALSE, "anonget1",0);
			} else {
				/* anon owns page */
				uvmfault_unlockall(ufi, amap, NULL, NULL);
				UVMHIST_LOG(maphist, " unlock+wait on anon",0,
				    0,0,0);
				UVM_UNLOCK_AND_WAIT(pg,&anon->an_lock,0,
				    "anonget2",0);
			}
			/* ready to relock and try again */

		} else {
		
			/*
			 * no page, we must try and bring it in.
			 */
			pg = uvm_pagealloc(NULL, 0, anon, 0);

			if (pg == NULL) {		/* out of RAM.  */

				uvmfault_unlockall(ufi, amap, NULL, anon);
				uvmexp.fltnoram++;
				UVMHIST_LOG(maphist, "  noram -- UVM_WAIT",0,
				    0,0,0);
				uvm_wait("flt_noram1");
				/* ready to relock and try again */

			} else {
	
				/* we set the PG_BUSY bit */
				we_own = TRUE;	
				uvmfault_unlockall(ufi, amap, NULL, anon);

				/*
				 * we are passing a PG_BUSY+PG_FAKE+PG_CLEAN
				 * page into the uvm_swap_get function with
				 * all data structures unlocked.  note that
				 * it is ok to read an_swslot here because
				 * we hold PG_BUSY on the page.
				 */
				uvmexp.pageins++;
				result = uvm_swap_get(pg, anon->an_swslot,
				    PGO_SYNCIO);

				/*
				 * we clean up after the i/o below in the
				 * "we_own" case
				 */
				/* ready to relock and try again */
			}
		}

		/*
		 * now relock and try again
		 */

		locked = uvmfault_relock(ufi);
		if (locked || we_own)
			simple_lock(&anon->an_lock);

		/*
		 * if we own the page (i.e. we set PG_BUSY), then we need
		 * to clean up after the I/O. there are three cases to
		 * consider:
		 *   [1] page released during I/O: free anon and ReFault.
		 *   [2] I/O not OK.   free the page and cause the fault 
		 *       to fail.
		 *   [3] I/O OK!   activate the page and sync with the
		 *       non-we_own case (i.e. drop anon lock if not locked).
		 */
		
		if (we_own) {

			if (pg->pg_flags & PG_WANTED) {
				/* still holding object lock */
				wakeup(pg);	
			}
			/* un-busy! */
			atomic_clearbits_int(&pg->pg_flags,
			    PG_WANTED|PG_BUSY|PG_FAKE);
			UVM_PAGE_OWN(pg, NULL);

			/* 
			 * if we were RELEASED during I/O, then our anon is
			 * no longer part of an amap.   we need to free the
			 * anon and try again.
			 */
			if (pg->pg_flags & PG_RELEASED) {
				pmap_page_protect(pg, VM_PROT_NONE);
				simple_unlock(&anon->an_lock);
				uvm_anfree(anon);	/* frees page for us */
				if (locked)
					uvmfault_unlockall(ufi, amap, NULL,
							   NULL);
				uvmexp.fltpgrele++;
				UVMHIST_LOG(maphist, "<- REFAULT", 0,0,0,0);
				return (VM_PAGER_REFAULT);	/* refault! */
			}

			if (result != VM_PAGER_OK) {
				KASSERT(result != VM_PAGER_PEND);

				/* remove page from anon */
				anon->an_page = NULL;

				/*
				 * remove the swap slot from the anon
				 * and mark the anon as having no real slot.
				 * don't free the swap slot, thus preventing
				 * it from being used again.
				 */
				uvm_swap_markbad(anon->an_swslot, 1);
				anon->an_swslot = SWSLOT_BAD;

				/*
				 * note: page was never !PG_BUSY, so it
				 * can't be mapped and thus no need to
				 * pmap_page_protect it...
				 */
				uvm_lock_pageq();
				uvm_pagefree(pg);
				uvm_unlock_pageq();

				if (locked)
					uvmfault_unlockall(ufi, amap, NULL,
					    anon);
				else
					simple_unlock(&anon->an_lock);
				UVMHIST_LOG(maphist, "<- ERROR", 0,0,0,0);
				return (VM_PAGER_ERROR);
			}
			
			/*
			 * must be OK, clear modify (already PG_CLEAN)
			 * and activate
			 */
			pmap_clear_modify(pg);
			uvm_lock_pageq();
			uvm_pageactivate(pg);
			uvm_unlock_pageq();
			if (!locked)
				simple_unlock(&anon->an_lock);
		}

		/*
		 * we were not able to relock.   restart fault.
		 */

		if (!locked) {
			UVMHIST_LOG(maphist, "<- REFAULT", 0,0,0,0);
			return (VM_PAGER_REFAULT);
		}

		/*
		 * verify no one has touched the amap and moved the anon on us.
		 */

		if (ufi != NULL &&
		    amap_lookup(&ufi->entry->aref, 
				ufi->orig_rvaddr - ufi->entry->start) != anon) {
			
			uvmfault_unlockall(ufi, amap, NULL, anon);
			UVMHIST_LOG(maphist, "<- REFAULT", 0,0,0,0);
			return (VM_PAGER_REFAULT);
		}
			
		/*
		 * try it again! 
		 */

		uvmexp.fltanretry++;
		continue;

	} /* while (1) */

	/*NOTREACHED*/
}

/*
 *   F A U L T   -   m a i n   e n t r y   p o i n t
 */

/*
 * uvm_fault: page fault handler
 *
 * => called from MD code to resolve a page fault
 * => VM data structures usually should be unlocked.   however, it is 
 *	possible to call here with the main map locked if the caller
 *	gets a write lock, sets it recursive, and then calls us (c.f.
 *	uvm_map_pageable).   this should be avoided because it keeps
 *	the map locked off during I/O.
 */

#define MASK(entry)     (UVM_ET_ISCOPYONWRITE(entry) ? \
			 ~VM_PROT_WRITE : VM_PROT_ALL)

int
uvm_fault(vm_map_t orig_map, vaddr_t vaddr, vm_fault_t fault_type,
    vm_prot_t access_type)
{
	struct uvm_faultinfo ufi;
	vm_prot_t enter_prot;
	boolean_t wired, narrow, promote, locked, shadowed;
	int npages, nback, nforw, centeridx, result, lcv, gotpages;
	vaddr_t startva, currva;
	voff_t uoff;
	paddr_t pa; 
	struct vm_amap *amap;
	struct uvm_object *uobj;
	struct vm_anon *anons_store[UVM_MAXRANGE], **anons, *anon, *oanon;
	struct vm_page *pages[UVM_MAXRANGE], *pg, *uobjpage;
	UVMHIST_FUNC("uvm_fault"); UVMHIST_CALLED(maphist);

	UVMHIST_LOG(maphist, "(map=%p, vaddr=0x%lx, ft=%ld, at=%ld)",
	      orig_map, vaddr, fault_type, access_type);

	anon = NULL;
	pg = NULL;

	uvmexp.faults++;	/* XXX: locking? */

	/*
	 * init the IN parameters in the ufi
	 */

	ufi.orig_map = orig_map;
	ufi.orig_rvaddr = trunc_page(vaddr);
	ufi.orig_size = PAGE_SIZE;	/* can't get any smaller than this */
	if (fault_type == VM_FAULT_WIRE)
		narrow = TRUE;		/* don't look for neighborhood
					 * pages on wire */
	else
		narrow = FALSE;		/* normal fault */

	/*
	 * "goto ReFault" means restart the page fault from ground zero.
	 */
ReFault:

	/*
	 * lookup and lock the maps
	 */

	if (uvmfault_lookup(&ufi, FALSE) == FALSE) {
		UVMHIST_LOG(maphist, "<- no mapping @ 0x%lx", vaddr, 0,0,0);
		return (EFAULT);
	}
	/* locked: maps(read) */

#ifdef DIAGNOSTIC
	if ((ufi.map->flags & VM_MAP_PAGEABLE) == 0)
		panic("uvm_fault: fault on non-pageable map (%p, 0x%lx)",
		    ufi.map, vaddr);
#endif

	/*
	 * check protection
	 */

	if ((ufi.entry->protection & access_type) != access_type) {
		UVMHIST_LOG(maphist,
		    "<- protection failure (prot=0x%lx, access=0x%lx)",
		    ufi.entry->protection, access_type, 0, 0);
		uvmfault_unlockmaps(&ufi, FALSE);
		return (EACCES);
	}

	/*
	 * "enter_prot" is the protection we want to enter the page in at.
	 * for certain pages (e.g. copy-on-write pages) this protection can
	 * be more strict than ufi.entry->protection.  "wired" means either
	 * the entry is wired or we are fault-wiring the pg.
	 */

	enter_prot = ufi.entry->protection;
	wired = VM_MAPENT_ISWIRED(ufi.entry) || (fault_type == VM_FAULT_WIRE);
	if (wired)
		access_type = enter_prot; /* full access for wired */

	/*
	 * handle "needs_copy" case.   if we need to copy the amap we will
	 * have to drop our readlock and relock it with a write lock.  (we
	 * need a write lock to change anything in a map entry [e.g.
	 * needs_copy]).
	 */

	if (UVM_ET_ISNEEDSCOPY(ufi.entry)) {
		if ((access_type & VM_PROT_WRITE) ||
		    (ufi.entry->object.uvm_obj == NULL)) {
			/* need to clear */
			UVMHIST_LOG(maphist,
			    "  need to clear needs_copy and refault",0,0,0,0);
			uvmfault_unlockmaps(&ufi, FALSE);
			uvmfault_amapcopy(&ufi);
			uvmexp.fltamcopy++;
			goto ReFault;

		} else {

			/*
			 * ensure that we pmap_enter page R/O since
			 * needs_copy is still true
			 */
			enter_prot &= ~VM_PROT_WRITE; 

		}
	}

	/*
	 * identify the players
	 */

	amap = ufi.entry->aref.ar_amap;		/* top layer */
	uobj = ufi.entry->object.uvm_obj;	/* bottom layer */

	/*
	 * check for a case 0 fault.  if nothing backing the entry then
	 * error now.
	 */

	if (amap == NULL && uobj == NULL) {
		uvmfault_unlockmaps(&ufi, FALSE);
		UVMHIST_LOG(maphist,"<- no backing store, no overlay",0,0,0,0);
		return (EFAULT);
	}

	/*
	 * establish range of interest based on advice from mapper
	 * and then clip to fit map entry.   note that we only want
	 * to do this the first time through the fault.   if we 
	 * ReFault we will disable this by setting "narrow" to true.
	 */

	if (narrow == FALSE) {

		/* wide fault (!narrow) */
		KASSERT(uvmadvice[ufi.entry->advice].advice ==
			 ufi.entry->advice);
		nback = min(uvmadvice[ufi.entry->advice].nback,
			    (ufi.orig_rvaddr - ufi.entry->start) >> PAGE_SHIFT);
		startva = ufi.orig_rvaddr - (nback << PAGE_SHIFT);
		nforw = min(uvmadvice[ufi.entry->advice].nforw,
			    ((ufi.entry->end - ufi.orig_rvaddr) >>
			     PAGE_SHIFT) - 1);
		/*
		 * note: "-1" because we don't want to count the
		 * faulting page as forw
		 */
		npages = nback + nforw + 1;
		centeridx = nback;

		narrow = TRUE;	/* ensure only once per-fault */

	} else {
		
		/* narrow fault! */
		nback = nforw = 0;
		startva = ufi.orig_rvaddr;
		npages = 1;
		centeridx = 0;

	}

	/* locked: maps(read) */
	UVMHIST_LOG(maphist, "  narrow=%ld, back=%ld, forw=%ld, startva=0x%lx",
		    narrow, nback, nforw, startva);
	UVMHIST_LOG(maphist, "  entry=%p, amap=%p, obj=%p", ufi.entry,
		    amap, uobj, 0);

	/*
	 * if we've got an amap, lock it and extract current anons.
	 */

	if (amap) {
		anons = anons_store;
		amap_lookups(&ufi.entry->aref, startva - ufi.entry->start,
		    anons, npages);
	} else {
		anons = NULL;	/* to be safe */
	}

	/* locked: maps(read), amap(if there) */

	/*
	 * for MADV_SEQUENTIAL mappings we want to deactivate the back pages
	 * now and then forget about them (for the rest of the fault).
	 */

	if (ufi.entry->advice == MADV_SEQUENTIAL && nback != 0) {

		UVMHIST_LOG(maphist, "  MADV_SEQUENTIAL: flushing backpages",
		    0,0,0,0);
		/* flush back-page anons? */
		if (amap) 
			uvmfault_anonflush(anons, nback);

		/* flush object? */
		if (uobj) {
			uoff = (startva - ufi.entry->start) + ufi.entry->offset;
			simple_lock(&uobj->vmobjlock);
			(void) uobj->pgops->pgo_flush(uobj, uoff, uoff + 
				    (nback << PAGE_SHIFT), PGO_DEACTIVATE);
			simple_unlock(&uobj->vmobjlock);
		}

		/* now forget about the backpages */
		if (amap)
			anons += nback;
		startva += (nback << PAGE_SHIFT);
		npages -= nback;
		centeridx = 0;
	}

	/* locked: maps(read), amap(if there) */

	/*
	 * map in the backpages and frontpages we found in the amap in hopes
	 * of preventing future faults.    we also init the pages[] array as
	 * we go.
	 */

	currva = startva;
	shadowed = FALSE;
	for (lcv = 0 ; lcv < npages ; lcv++, currva += PAGE_SIZE) {

		/*
		 * dont play with VAs that are already mapped
		 * except for center)
		 */
		if (lcv != centeridx &&
		    pmap_extract(ufi.orig_map->pmap, currva, &pa)) {
			pages[lcv] = PGO_DONTCARE;
			continue;
		}

		/*
		 * unmapped or center page.   check if any anon at this level.
		 */
		if (amap == NULL || anons[lcv] == NULL) {
			pages[lcv] = NULL;
			continue;
		}

		/*
		 * check for present page and map if possible.   re-activate it.
		 */

		pages[lcv] = PGO_DONTCARE;
		if (lcv == centeridx) {		/* save center for later! */
			shadowed = TRUE;
			continue;
		}
		anon = anons[lcv];
		simple_lock(&anon->an_lock);
		/* ignore loaned pages */
		if (anon->an_page && anon->an_page->loan_count == 0 &&
		    (anon->an_page->pg_flags & (PG_RELEASED|PG_BUSY)) == 0) {
			uvm_lock_pageq();
			uvm_pageactivate(anon->an_page);	/* reactivate */
			uvm_unlock_pageq();
			UVMHIST_LOG(maphist,
			    "  MAPPING: n anon: pm=%p, va=0x%lx, pg=%p",
			    ufi.orig_map->pmap, currva, anon->an_page, 0);
			uvmexp.fltnamap++;

			/*
			 * Since this isn't the page that's actually faulting,
			 * ignore pmap_enter() failures; it's not critical
			 * that we enter these right now.
			 */

			(void) pmap_enter(ufi.orig_map->pmap, currva,
			    VM_PAGE_TO_PHYS(anon->an_page),
			    (anon->an_ref > 1) ? (enter_prot & ~VM_PROT_WRITE) :
			    enter_prot,
			    PMAP_CANFAIL |
			     (VM_MAPENT_ISWIRED(ufi.entry) ? PMAP_WIRED : 0));
		}
		simple_unlock(&anon->an_lock);
		pmap_update(ufi.orig_map->pmap);
	}

	/* locked: maps(read), amap(if there) */
	/* (shadowed == TRUE) if there is an anon at the faulting address */
	UVMHIST_LOG(maphist, "  shadowed=%ld, will_get=%ld", shadowed, 
	    (uobj && shadowed == FALSE),0,0);

	/*
	 * note that if we are really short of RAM we could sleep in the above
	 * call to pmap_enter with everything locked.   bad?
	 *
	 * XXX Actually, that is bad; pmap_enter() should just fail in that
	 * XXX case.  --thorpej
	 */
	
	/*
	 * if the desired page is not shadowed by the amap and we have a
	 * backing object, then we check to see if the backing object would
	 * prefer to handle the fault itself (rather than letting us do it
	 * with the usual pgo_get hook).  the backing object signals this by
	 * providing a pgo_fault routine.
	 */

	if (uobj && shadowed == FALSE && uobj->pgops->pgo_fault != NULL) {
		simple_lock(&uobj->vmobjlock);

		/* locked: maps(read), amap (if there), uobj */
		result = uobj->pgops->pgo_fault(&ufi, startva, pages, npages,
				    centeridx, fault_type, access_type,
				    PGO_LOCKED);

		/* locked: nothing, pgo_fault has unlocked everything */

		if (result == VM_PAGER_OK)
			return (0);		/* pgo_fault did pmap enter */
		else if (result == VM_PAGER_REFAULT)
			goto ReFault;		/* try again! */
		else
			return (EACCES);
	}

	/*
	 * now, if the desired page is not shadowed by the amap and we have
	 * a backing object that does not have a special fault routine, then
	 * we ask (with pgo_get) the object for resident pages that we care
	 * about and attempt to map them in.  we do not let pgo_get block
	 * (PGO_LOCKED).
	 *
	 * ("get" has the option of doing a pmap_enter for us)
	 */

	if (uobj && shadowed == FALSE) {
		simple_lock(&uobj->vmobjlock);

		/* locked (!shadowed): maps(read), amap (if there), uobj */
		/*
		 * the following call to pgo_get does _not_ change locking state
		 */

		uvmexp.fltlget++;
		gotpages = npages;
		(void) uobj->pgops->pgo_get(uobj, ufi.entry->offset +
				(startva - ufi.entry->start),
				pages, &gotpages, centeridx,
				access_type & MASK(ufi.entry),
				ufi.entry->advice, PGO_LOCKED);

		/*
		 * check for pages to map, if we got any
		 */

		uobjpage = NULL;

		if (gotpages) {
			currva = startva;
			for (lcv = 0 ; lcv < npages ;
			    lcv++, currva += PAGE_SIZE) {

				if (pages[lcv] == NULL ||
				    pages[lcv] == PGO_DONTCARE)
					continue;

				KASSERT((pages[lcv]->pg_flags & PG_RELEASED) == 0);

				/*
				 * if center page is resident and not
				 * PG_BUSY, then pgo_get made it PG_BUSY
				 * for us and gave us a handle to it.
				 * remember this page as "uobjpage."
				 * (for later use).
				 */
				
				if (lcv == centeridx) {
					uobjpage = pages[lcv];
					UVMHIST_LOG(maphist, "  got uobjpage "
					    "(%p) with locked get", 
					    uobjpage, 0,0,0);
					continue;
				}
	
				/* 
				 * note: calling pgo_get with locked data
				 * structures returns us pages which are
				 * neither busy nor released, so we don't
				 * need to check for this.   we can just
				 * directly enter the page (after moving it
				 * to the head of the active queue [useful?]).
				 */

				uvm_lock_pageq();
				uvm_pageactivate(pages[lcv]);	/* reactivate */
				uvm_unlock_pageq();
				UVMHIST_LOG(maphist,
				  "  MAPPING: n obj: pm=%p, va=0x%lx, pg=%p",
				  ufi.orig_map->pmap, currva, pages[lcv], 0);
				uvmexp.fltnomap++;

				/*
				 * Since this page isn't the page that's
				 * actually fauling, ignore pmap_enter()
				 * failures; it's not critical that we
				 * enter these right now.
				 */

				(void) pmap_enter(ufi.orig_map->pmap, currva,
				    VM_PAGE_TO_PHYS(pages[lcv]),
				    enter_prot & MASK(ufi.entry),
				    PMAP_CANFAIL |
				     (wired ? PMAP_WIRED : 0));

				/* 
				 * NOTE: page can't be PG_WANTED because
				 * we've held the lock the whole time
				 * we've had the handle.
				 */

				atomic_clearbits_int(&pages[lcv]->pg_flags,
				    PG_BUSY);
				UVM_PAGE_OWN(pages[lcv], NULL);
			}	/* for "lcv" loop */
			pmap_update(ufi.orig_map->pmap);
		}   /* "gotpages" != 0 */
		/* note: object still _locked_ */
	} else {
		uobjpage = NULL;
	}

	/* locked (shadowed): maps(read), amap */
	/* locked (!shadowed): maps(read), amap(if there), 
		 uobj(if !null), uobjpage(if !null) */

	/*
	 * note that at this point we are done with any front or back pages.
	 * we are now going to focus on the center page (i.e. the one we've
	 * faulted on).  if we have faulted on the top (anon) layer
	 * [i.e. case 1], then the anon we want is anons[centeridx] (we have
	 * not touched it yet).  if we have faulted on the bottom (uobj)
	 * layer [i.e. case 2] and the page was both present and available,
	 * then we've got a pointer to it as "uobjpage" and we've already
	 * made it BUSY.
	 */

	/*
	 * there are four possible cases we must address: 1A, 1B, 2A, and 2B
	 */

	/*
	 * redirect case 2: if we are not shadowed, go to case 2.
	 */

	if (shadowed == FALSE) 
		goto Case2;

	/* locked: maps(read), amap */

	/*
	 * handle case 1: fault on an anon in our amap
	 */

	anon = anons[centeridx];
	UVMHIST_LOG(maphist, "  case 1 fault: anon=%p", anon, 0,0,0);
	simple_lock(&anon->an_lock);

	/* locked: maps(read), amap, anon */

	/*
	 * no matter if we have case 1A or case 1B we are going to need to
	 * have the anon's memory resident.   ensure that now.
	 */

	/*
	 * let uvmfault_anonget do the dirty work.
	 * if it fails (!OK) it will unlock everything for us.
	 * if it succeeds, locks are still valid and locked.
	 * also, if it is OK, then the anon's page is on the queues.
	 * if the page is on loan from a uvm_object, then anonget will
	 * lock that object for us if it does not fail.
	 */

	result = uvmfault_anonget(&ufi, amap, anon);
	switch (result) {
	case VM_PAGER_OK:
		break; 

	case VM_PAGER_REFAULT:
		goto ReFault;

	case VM_PAGER_ERROR:
		/*
		 * An error occured while trying to bring in the
		 * page -- this is the only error we return right
		 * now.
		 */
		return (EACCES);	/* XXX */

	default:
#ifdef DIAGNOSTIC
		panic("uvm_fault: uvmfault_anonget -> %d", result);
#else
		return (EACCES);
#endif
	}

	/*
	 * uobj is non null if the page is on loan from an object (i.e. uobj)
	 */

	uobj = anon->an_page->uobject;	/* locked by anonget if !NULL */

	/* locked: maps(read), amap, anon, uobj(if one) */

	/*
	 * special handling for loaned pages 
	 */

	if (anon->an_page->loan_count) {

		if ((access_type & VM_PROT_WRITE) == 0) {
			
			/*
			 * for read faults on loaned pages we just cap the
			 * protection at read-only.
			 */

			enter_prot = enter_prot & ~VM_PROT_WRITE;

		} else {
			/*
			 * note that we can't allow writes into a loaned page!
			 *
			 * if we have a write fault on a loaned page in an
			 * anon then we need to look at the anon's ref count.
			 * if it is greater than one then we are going to do
			 * a normal copy-on-write fault into a new anon (this
			 * is not a problem).  however, if the reference count
			 * is one (a case where we would normally allow a
			 * write directly to the page) then we need to kill
			 * the loan before we continue.
			 */

			/* >1 case is already ok */
			if (anon->an_ref == 1) {

				/* get new un-owned replacement page */
				pg = uvm_pagealloc(NULL, 0, NULL, 0);
				if (pg == NULL) {
					uvmfault_unlockall(&ufi, amap, uobj,
					    anon);
					uvm_wait("flt_noram2");
					goto ReFault;
				}

				/*
				 * copy data, kill loan, and drop uobj lock
				 * (if any)
				 */
				/* copy old -> new */
				uvm_pagecopy(anon->an_page, pg);

				/* force reload */
				pmap_page_protect(anon->an_page,
						  VM_PROT_NONE);
				uvm_lock_pageq();	  /* KILL loan */
				if (uobj)
					/* if we were loaning */
					anon->an_page->loan_count--;
				anon->an_page->uanon = NULL;
				/* in case we owned */
				atomic_clearbits_int(
				    &anon->an_page->pg_flags, PQ_ANON);
				uvm_pageactivate(pg);
				uvm_unlock_pageq();
				if (uobj) {
					simple_unlock(&uobj->vmobjlock);
					uobj = NULL;
				}

				/* install new page in anon */
				anon->an_page = pg;
				pg->uanon = anon;
				atomic_setbits_int(&pg->pg_flags, PQ_ANON);
				atomic_clearbits_int(&pg->pg_flags,
				    PG_BUSY|PG_FAKE);
				UVM_PAGE_OWN(pg, NULL);

				/* done! */
			}     /* ref == 1 */
		}       /* write fault */
	}         /* loan count */

	/*
	 * if we are case 1B then we will need to allocate a new blank
	 * anon to transfer the data into.   note that we have a lock
	 * on anon, so no one can busy or release the page until we are done.
	 * also note that the ref count can't drop to zero here because
	 * it is > 1 and we are only dropping one ref.
	 *
	 * in the (hopefully very rare) case that we are out of RAM we 
	 * will unlock, wait for more RAM, and refault.    
	 *
	 * if we are out of anon VM we kill the process (XXX: could wait?).
	 */

	if ((access_type & VM_PROT_WRITE) != 0 && anon->an_ref > 1) {

		UVMHIST_LOG(maphist, "  case 1B: COW fault",0,0,0,0);
		uvmexp.flt_acow++;
		oanon = anon;		/* oanon = old, locked anon */
		anon = uvm_analloc();
		if (anon) {
			pg = uvm_pagealloc(NULL, 0, anon, 0);
		}

		/* check for out of RAM */
		if (anon == NULL || pg == NULL) {
			if (anon)
				uvm_anfree(anon);
			uvmfault_unlockall(&ufi, amap, uobj, oanon);
			KASSERT(uvmexp.swpgonly <= uvmexp.swpages);
			if (anon == NULL || uvmexp.swpgonly == uvmexp.swpages) {
				UVMHIST_LOG(maphist,
				    "<- failed.  out of VM",0,0,0,0);
				uvmexp.fltnoanon++;
				return (ENOMEM);
			}

			uvmexp.fltnoram++;
			uvm_wait("flt_noram3");	/* out of RAM, wait for more */
			goto ReFault;
		}

		/* got all resources, replace anon with nanon */

		uvm_pagecopy(oanon->an_page, pg);	/* pg now !PG_CLEAN */
		/* un-busy! new page */
		atomic_clearbits_int(&pg->pg_flags, PG_BUSY|PG_FAKE);
		UVM_PAGE_OWN(pg, NULL);
		amap_add(&ufi.entry->aref, ufi.orig_rvaddr - ufi.entry->start,
		    anon, 1);

		/* deref: can not drop to zero here by defn! */
		oanon->an_ref--;

		/*
		 * note: oanon still locked.   anon is _not_ locked, but we
		 * have the sole references to in from amap which _is_ locked.
		 * thus, no one can get at it until we are done with it.
		 */

	} else {

		uvmexp.flt_anon++;
		oanon = anon;		/* old, locked anon is same as anon */
		pg = anon->an_page;
		if (anon->an_ref > 1)     /* disallow writes to ref > 1 anons */
			enter_prot = enter_prot & ~VM_PROT_WRITE;

	}

	/* locked: maps(read), amap, oanon */

	/*
	 * now map the page in ...
	 * XXX: old fault unlocks object before pmap_enter.  this seems
	 * suspect since some other thread could blast the page out from
	 * under us between the unlock and the pmap_enter.
	 */

	UVMHIST_LOG(maphist, "  MAPPING: anon: pm=%p, va=0x%lx, pg=%p",
	    ufi.orig_map->pmap, ufi.orig_rvaddr, pg, 0);
	if (pmap_enter(ufi.orig_map->pmap, ufi.orig_rvaddr, VM_PAGE_TO_PHYS(pg),
	    enter_prot, access_type | PMAP_CANFAIL | (wired ? PMAP_WIRED : 0))
	    != 0) {
		/*
		 * No need to undo what we did; we can simply think of
		 * this as the pmap throwing away the mapping information.
		 *
		 * We do, however, have to go through the ReFault path,
		 * as the map may change while we're asleep.
		 */
		uvmfault_unlockall(&ufi, amap, uobj, oanon);
		KASSERT(uvmexp.swpgonly <= uvmexp.swpages);
		if (uvmexp.swpgonly == uvmexp.swpages) {
			UVMHIST_LOG(maphist,
			    "<- failed.  out of VM",0,0,0,0);
			/* XXX instrumentation */
			return (ENOMEM);
		}
		/* XXX instrumentation */
		uvm_wait("flt_pmfail1");
		goto ReFault;
	}

	/*
	 * ... update the page queues.
	 */

	uvm_lock_pageq();

	if (fault_type == VM_FAULT_WIRE) {
		uvm_pagewire(pg);

		/*
		 * since the now-wired page cannot be paged out,
		 * release its swap resources for others to use.
		 * since an anon with no swap cannot be PG_CLEAN,
		 * clear its clean flag now.
		 */
		atomic_clearbits_int(&pg->pg_flags, PG_CLEAN);
		uvm_anon_dropswap(anon);
	} else {
		/* activate it */
		uvm_pageactivate(pg);
	}

	uvm_unlock_pageq();

	/*
	 * done case 1!  finish up by unlocking everything and returning success
	 */

	uvmfault_unlockall(&ufi, amap, uobj, oanon);
	pmap_update(ufi.orig_map->pmap);
	return (0);


Case2:
	/*
	 * handle case 2: faulting on backing object or zero fill
	 */

	/*
	 * locked:
	 * maps(read), amap(if there), uobj(if !null), uobjpage(if !null)
	 */

	/*
	 * note that uobjpage can not be PGO_DONTCARE at this point.  we now
	 * set uobjpage to PGO_DONTCARE if we are doing a zero fill.  if we
	 * have a backing object, check and see if we are going to promote
	 * the data up to an anon during the fault.
	 */

	if (uobj == NULL) {
		uobjpage = PGO_DONTCARE;	
		promote = TRUE;		/* always need anon here */
	} else {
		KASSERT(uobjpage != PGO_DONTCARE);
		promote = (access_type & VM_PROT_WRITE) &&
		     UVM_ET_ISCOPYONWRITE(ufi.entry);
	}
	UVMHIST_LOG(maphist, "  case 2 fault: promote=%ld, zfill=%ld",
	    promote, (uobj == NULL), 0,0);

	/*
	 * if uobjpage is not null then we do not need to do I/O to get the
	 * uobjpage.
	 *
	 * if uobjpage is null, then we need to unlock and ask the pager to 
	 * get the data for us.   once we have the data, we need to reverify
	 * the state the world.   we are currently not holding any resources.
	 */

	if (uobjpage) {
		/* update rusage counters */
		curproc->p_addr->u_stats.p_ru.ru_minflt++;
	} else {
		/* update rusage counters */
		curproc->p_addr->u_stats.p_ru.ru_majflt++;
		
		/* locked: maps(read), amap(if there), uobj */
		uvmfault_unlockall(&ufi, amap, NULL, NULL);
		/* locked: uobj */

		uvmexp.fltget++;
		gotpages = 1;
		uoff = (ufi.orig_rvaddr - ufi.entry->start) + ufi.entry->offset;
		result = uobj->pgops->pgo_get(uobj, uoff, &uobjpage, &gotpages,
		    0, access_type & MASK(ufi.entry), ufi.entry->advice,
		    PGO_SYNCIO);

		/* locked: uobjpage(if result OK) */

		/*
		 * recover from I/O
		 */

		if (result != VM_PAGER_OK) {
			KASSERT(result != VM_PAGER_PEND);

			if (result == VM_PAGER_AGAIN) {
				UVMHIST_LOG(maphist,
				    "  pgo_get says TRY AGAIN!",0,0,0,0);
				tsleep((caddr_t)&lbolt, PVM, "fltagain2", 0);
				goto ReFault;
			}

			UVMHIST_LOG(maphist, "<- pgo_get failed (code %ld)",
			    result, 0,0,0);
			return (EACCES); /* XXX i/o error */
		}

		/* locked: uobjpage */

		/*
		 * re-verify the state of the world by first trying to relock
		 * the maps.  always relock the object.
		 */

		locked = uvmfault_relock(&ufi);
		simple_lock(&uobj->vmobjlock);
		
		/* locked(locked): maps(read), amap(if !null), uobj, uobjpage */
		/* locked(!locked): uobj, uobjpage */

		/*
		 * Re-verify that amap slot is still free. if there is
		 * a problem, we unlock and clean up.
		 */

		if (locked && amap && amap_lookup(&ufi.entry->aref,
		      ufi.orig_rvaddr - ufi.entry->start)) {
			if (locked) 
				uvmfault_unlockall(&ufi, amap, NULL, NULL);
			locked = FALSE;
		}

		/*
		 * didn't get the lock?   release the page and retry.
		 */

		if (locked == FALSE) {

			UVMHIST_LOG(maphist,
			    "  wasn't able to relock after fault: retry", 
			    0,0,0,0);
			if (uobjpage->pg_flags & PG_WANTED)
				/* still holding object lock */
				wakeup(uobjpage);

			uvm_lock_pageq();
			/* make sure it is in queues */
			uvm_pageactivate(uobjpage);

			uvm_unlock_pageq();
			atomic_clearbits_int(&uobjpage->pg_flags,
			    PG_BUSY|PG_WANTED);
			UVM_PAGE_OWN(uobjpage, NULL);
			simple_unlock(&uobj->vmobjlock);
			goto ReFault;

		}

		/*
		 * we have the data in uobjpage which is PG_BUSY and we are
		 * holding object lock.
		 */

		/* locked: maps(read), amap(if !null), uobj, uobjpage */
	}

	/*
	 * locked:
	 * maps(read), amap(if !null), uobj(if !null), uobjpage(if uobj)
	 */

	/*
	 * notes:
	 *  - at this point uobjpage can not be NULL
	 *  - at this point uobjpage could be PG_WANTED (handle later)
	 */
		
	if (promote == FALSE) {

		/*
		 * we are not promoting.   if the mapping is COW ensure that we
		 * don't give more access than we should (e.g. when doing a read
		 * fault on a COPYONWRITE mapping we want to map the COW page in
		 * R/O even though the entry protection could be R/W).
		 *
		 * set "pg" to the page we want to map in (uobjpage, usually)
		 */

		uvmexp.flt_obj++;
		if (UVM_ET_ISCOPYONWRITE(ufi.entry))
			enter_prot &= ~VM_PROT_WRITE;
		pg = uobjpage;		/* map in the actual object */

		/* assert(uobjpage != PGO_DONTCARE) */

		/*
		 * we are faulting directly on the page.   be careful
		 * about writing to loaned pages...
		 */
		if (uobjpage->loan_count) {

			if ((access_type & VM_PROT_WRITE) == 0) {
				/* read fault: cap the protection at readonly */
				/* cap! */
				enter_prot = enter_prot & ~VM_PROT_WRITE;
			} else {
				/* write fault: must break the loan here */

				/* alloc new un-owned page */
				pg = uvm_pagealloc(NULL, 0, NULL, 0);

				if (pg == NULL) {
					/*
					 * drop ownership of page, it can't
					 * be released
					 */
					if (uobjpage->pg_flags & PG_WANTED)
						wakeup(uobjpage);
					atomic_clearbits_int(
					    &uobjpage->pg_flags,
					    PG_BUSY|PG_WANTED);
					UVM_PAGE_OWN(uobjpage, NULL);

					uvm_lock_pageq();
					/* activate: we will need it later */
					uvm_pageactivate(uobjpage);

					uvm_unlock_pageq();
					uvmfault_unlockall(&ufi, amap, uobj,
					  NULL);
					UVMHIST_LOG(maphist,
					  "  out of RAM breaking loan, waiting",
					  0,0,0,0);
					uvmexp.fltnoram++;
					uvm_wait("flt_noram4");
					goto ReFault;
				}

				/*
				 * copy the data from the old page to the new
				 * one and clear the fake/clean flags on the
				 * new page (keep it busy).  force a reload
				 * of the old page by clearing it from all
				 * pmaps.  then lock the page queues to
				 * rename the pages.
				 */
				uvm_pagecopy(uobjpage, pg);	/* old -> new */
				atomic_clearbits_int(&pg->pg_flags,
				    PG_FAKE|PG_CLEAN);
				pmap_page_protect(uobjpage, VM_PROT_NONE);
				if (uobjpage->pg_flags & PG_WANTED)
					wakeup(uobjpage);
				/* uobj still locked */
				atomic_clearbits_int(&uobjpage->pg_flags,
				    PG_BUSY|PG_WANTED);
				UVM_PAGE_OWN(uobjpage, NULL);

				uvm_lock_pageq();
				uoff = uobjpage->offset;
				/* remove old page */
				uvm_pagerealloc(uobjpage, NULL, 0);

				/*
				 * at this point we have absolutely no
				 * control over uobjpage
				 */
				/* install new page */
				uvm_pagerealloc(pg, uobj, uoff);
				uvm_unlock_pageq();

				/*
				 * done!  loan is broken and "pg" is
				 * PG_BUSY.   it can now replace uobjpage.
				 */

				uobjpage = pg;

			}		/* write fault case */
		}		/* if loan_count */

	} else {
		
		/*
		 * if we are going to promote the data to an anon we
		 * allocate a blank anon here and plug it into our amap.
		 */
#ifdef DIAGNOSTIC
		if (amap == NULL)
			panic("uvm_fault: want to promote data, but no anon");
#endif

		anon = uvm_analloc();
		if (anon) {
			/*
			 * In `Fill in data...' below, if
			 * uobjpage == PGO_DONTCARE, we want
			 * a zero'd, dirty page, so have
			 * uvm_pagealloc() do that for us.
			 */
			pg = uvm_pagealloc(NULL, 0, anon,
			    (uobjpage == PGO_DONTCARE) ? UVM_PGA_ZERO : 0);
		}

		/*
		 * out of memory resources?
		 */
		if (anon == NULL || pg == NULL) {

			/*
			 * arg!  must unbusy our page and fail or sleep.
			 */
			if (uobjpage != PGO_DONTCARE) {
				if (uobjpage->pg_flags & PG_WANTED)
					/* still holding object lock */
					wakeup(uobjpage);

				uvm_lock_pageq();
				uvm_pageactivate(uobjpage);
				uvm_unlock_pageq();
				atomic_clearbits_int(&uobjpage->pg_flags,
				    PG_BUSY|PG_WANTED);
				UVM_PAGE_OWN(uobjpage, NULL);
			}

			/* unlock and fail ... */
			uvmfault_unlockall(&ufi, amap, uobj, NULL);
			KASSERT(uvmexp.swpgonly <= uvmexp.swpages);
			if (anon == NULL || uvmexp.swpgonly == uvmexp.swpages) {
				UVMHIST_LOG(maphist, "  promote: out of VM",
				    0,0,0,0);
				uvmexp.fltnoanon++;
				return (ENOMEM);
			}

			UVMHIST_LOG(maphist, "  out of RAM, waiting for more",
			    0,0,0,0);
			uvm_anfree(anon);
			uvmexp.fltnoram++;
			uvm_wait("flt_noram5");
			goto ReFault;
		}

		/*
		 * fill in the data
		 */

		if (uobjpage != PGO_DONTCARE) {
			uvmexp.flt_prcopy++;
			/* copy page [pg now dirty] */
			uvm_pagecopy(uobjpage, pg);

			/*
			 * promote to shared amap?  make sure all sharing
			 * procs see it
			 */
			if ((amap_flags(amap) & AMAP_SHARED) != 0) {
				pmap_page_protect(uobjpage, VM_PROT_NONE);
			}
			
			/*
			 * dispose of uobjpage. drop handle to uobj as well.
			 */

			if (uobjpage->pg_flags & PG_WANTED)
				/* still have the obj lock */
				wakeup(uobjpage);
			atomic_clearbits_int(&uobjpage->pg_flags,
			    PG_BUSY|PG_WANTED);
			UVM_PAGE_OWN(uobjpage, NULL);
			uvm_lock_pageq();
			uvm_pageactivate(uobjpage);
			uvm_unlock_pageq();
			simple_unlock(&uobj->vmobjlock);
			uobj = NULL;

			UVMHIST_LOG(maphist,
			    "  promote uobjpage %p to anon/page %p/%p",
			    uobjpage, anon, pg, 0);

		} else {
			uvmexp.flt_przero++;
			/*
			 * Page is zero'd and marked dirty by uvm_pagealloc()
			 * above.
			 */
			UVMHIST_LOG(maphist,"  zero fill anon/page %p/%p",
			    anon, pg, 0, 0);
		}

		amap_add(&ufi.entry->aref, ufi.orig_rvaddr - ufi.entry->start,
		    anon, 0);
	}

	/*
	 * locked:
	 * maps(read), amap(if !null), uobj(if !null), uobjpage(if uobj)
	 *
	 * note: pg is either the uobjpage or the new page in the new anon
	 */

	/*
	 * all resources are present.   we can now map it in and free our
	 * resources.
	 */

	UVMHIST_LOG(maphist,
	    "  MAPPING: case2: pm=%p, va=0x%lx, pg=%p, promote=%ld",
	    ufi.orig_map->pmap, ufi.orig_rvaddr, pg, promote);
	if (pmap_enter(ufi.orig_map->pmap, ufi.orig_rvaddr, VM_PAGE_TO_PHYS(pg),
	    enter_prot, access_type | PMAP_CANFAIL | (wired ? PMAP_WIRED : 0))
	    != 0) {

		/*
		 * No need to undo what we did; we can simply think of
		 * this as the pmap throwing away the mapping information.
		 *
		 * We do, however, have to go through the ReFault path,
		 * as the map may change while we're asleep.
		 */

		if (pg->pg_flags & PG_WANTED)
			wakeup(pg);		/* lock still held */

		atomic_clearbits_int(&pg->pg_flags, PG_BUSY|PG_FAKE|PG_WANTED);
		UVM_PAGE_OWN(pg, NULL);
		uvmfault_unlockall(&ufi, amap, uobj, NULL);
		KASSERT(uvmexp.swpgonly <= uvmexp.swpages);
		if (uvmexp.swpgonly == uvmexp.swpages) {
			UVMHIST_LOG(maphist,
			    "<- failed.  out of VM",0,0,0,0);
			/* XXX instrumentation */
			return (ENOMEM);
		}
		/* XXX instrumentation */
		uvm_wait("flt_pmfail2");
		goto ReFault;
	}

	uvm_lock_pageq();

	if (fault_type == VM_FAULT_WIRE) {
		uvm_pagewire(pg);
		if (pg->pg_flags & PQ_AOBJ) {

			/*
			 * since the now-wired page cannot be paged out,
			 * release its swap resources for others to use.
			 * since an aobj page with no swap cannot be PG_CLEAN,
			 * clear its clean flag now.
			 */
			atomic_clearbits_int(&pg->pg_flags, PG_CLEAN);
			uao_dropswap(uobj, pg->offset >> PAGE_SHIFT);
		}
	} else {
		/* activate it */
		uvm_pageactivate(pg);
	}
	uvm_unlock_pageq();

	if (pg->pg_flags & PG_WANTED)
		wakeup(pg);		/* lock still held */

	atomic_clearbits_int(&pg->pg_flags, PG_BUSY|PG_FAKE|PG_WANTED);
	UVM_PAGE_OWN(pg, NULL);
	uvmfault_unlockall(&ufi, amap, uobj, NULL);
	pmap_update(ufi.orig_map->pmap);

	UVMHIST_LOG(maphist, "<- done (SUCCESS!)",0,0,0,0);
	return (0);
}


/*
 * uvm_fault_wire: wire down a range of virtual addresses in a map.
 *
 * => map may be read-locked by caller, but MUST NOT be write-locked.
 * => if map is read-locked, any operations which may cause map to
 *	be write-locked in uvm_fault() must be taken care of by
 *	the caller.  See uvm_map_pageable().
 */

int
uvm_fault_wire(vm_map_t map, vaddr_t start, vaddr_t end, vm_prot_t access_type)
{
	vaddr_t va;
	pmap_t  pmap;
	int rv;

	pmap = vm_map_pmap(map);

	/*
	 * now fault it in a page at a time.   if the fault fails then we have
	 * to undo what we have done.   note that in uvm_fault VM_PROT_NONE 
	 * is replaced with the max protection if fault_type is VM_FAULT_WIRE.
	 */

	for (va = start ; va < end ; va += PAGE_SIZE) {
		rv = uvm_fault(map, va, VM_FAULT_WIRE, access_type);
		if (rv) {
			if (va != start) {
				uvm_fault_unwire(map, start, va);
			}
			return (rv);
		}
	}

	return (0);
}

/*
 * uvm_fault_unwire(): unwire range of virtual space.
 */

void
uvm_fault_unwire(vm_map_t map, vaddr_t start, vaddr_t end)
{

	vm_map_lock_read(map);
	uvm_fault_unwire_locked(map, start, end);
	vm_map_unlock_read(map);
}

/*
 * uvm_fault_unwire_locked(): the guts of uvm_fault_unwire().
 *
 * => map must be at least read-locked.
 */

void
uvm_fault_unwire_locked(vm_map_t map, vaddr_t start, vaddr_t end)
{
	vm_map_entry_t entry;
	pmap_t pmap = vm_map_pmap(map);
	vaddr_t va;
	paddr_t pa;
	struct vm_page *pg;

	KASSERT((map->flags & VM_MAP_INTRSAFE) == 0);

	/*
	 * we assume that the area we are unwiring has actually been wired
	 * in the first place.   this means that we should be able to extract
	 * the PAs from the pmap.   we also lock out the page daemon so that
	 * we can call uvm_pageunwire.
	 */

	uvm_lock_pageq();

	/*
	 * find the beginning map entry for the region.
	 */
	KASSERT(start >= vm_map_min(map) && end <= vm_map_max(map));
	if (uvm_map_lookup_entry(map, start, &entry) == FALSE)
		panic("uvm_fault_unwire_locked: address not in map");

	for (va = start; va < end ; va += PAGE_SIZE) {
		if (pmap_extract(pmap, va, &pa) == FALSE)
			continue;

		/*
		 * find the map entry for the current address.
		 */
		KASSERT(va >= entry->start);
		while (va >= entry->end) {
			KASSERT(entry->next != &map->header &&
				entry->next->start <= entry->end);
			entry = entry->next;
		}

		/*
		 * if the entry is no longer wired, tell the pmap.
		 */
		if (VM_MAPENT_ISWIRED(entry) == 0)
			pmap_unwire(pmap, va);

		pg = PHYS_TO_VM_PAGE(pa);
		if (pg)
			uvm_pageunwire(pg);
	}

	uvm_unlock_pageq();
}

/*
 * uvmfault_unlockmaps: unlock the maps
 */
void
uvmfault_unlockmaps(struct uvm_faultinfo *ufi, boolean_t write_locked)
{
	/*
	 * ufi can be NULL when this isn't really a fault,
	 * but merely paging in anon data.
	 */

	if (ufi == NULL) {
		return;
	}

	if (write_locked) {
		vm_map_unlock(ufi->map);
	} else {
		vm_map_unlock_read(ufi->map);
	}
}

/*
 * uvmfault_unlockall: unlock everything passed in.
 *
 * => maps must be read-locked (not write-locked).
 */
void
uvmfault_unlockall(struct uvm_faultinfo *ufi, struct vm_amap *amap,
    struct uvm_object *uobj, struct vm_anon *anon)
{

	if (anon)
		simple_unlock(&anon->an_lock);
	if (uobj)
		simple_unlock(&uobj->vmobjlock);
	uvmfault_unlockmaps(ufi, FALSE);
}

/*
 * uvmfault_lookup: lookup a virtual address in a map
 *
 * => caller must provide a uvm_faultinfo structure with the IN
 *	params properly filled in
 * => we will lookup the map entry (handling submaps) as we go
 * => if the lookup is a success we will return with the maps locked
 * => if "write_lock" is TRUE, we write_lock the map, otherwise we only
 *	get a read lock.
 * => note that submaps can only appear in the kernel and they are 
 *	required to use the same virtual addresses as the map they
 *	are referenced by (thus address translation between the main
 *	map and the submap is unnecessary).
 */

boolean_t
uvmfault_lookup(struct uvm_faultinfo *ufi, boolean_t write_lock)
{
	vm_map_t tmpmap;

	/*
	 * init ufi values for lookup.
	 */

	ufi->map = ufi->orig_map;
	ufi->size = ufi->orig_size;

	/*
	 * keep going down levels until we are done.   note that there can
	 * only be two levels so we won't loop very long.
	 */

	while (1) {

		/*
		 * lock map
		 */
		if (write_lock) {
			vm_map_lock(ufi->map);
		} else {
			vm_map_lock_read(ufi->map);
		}

		/*
		 * lookup
		 */
		if (!uvm_map_lookup_entry(ufi->map, ufi->orig_rvaddr, 
								&ufi->entry)) {
			uvmfault_unlockmaps(ufi, write_lock);
			return(FALSE);
		}

		/*
		 * reduce size if necessary
		 */
		if (ufi->entry->end - ufi->orig_rvaddr < ufi->size)
			ufi->size = ufi->entry->end - ufi->orig_rvaddr;

		/*
		 * submap?    replace map with the submap and lookup again.
		 * note: VAs in submaps must match VAs in main map.
		 */
		if (UVM_ET_ISSUBMAP(ufi->entry)) {
			tmpmap = ufi->entry->object.sub_map;
			uvmfault_unlockmaps(ufi, write_lock);
			ufi->map = tmpmap;
			continue;
		}

		/*
		 * got it!
		 */

		ufi->mapv = ufi->map->timestamp;
		return(TRUE);

	}	/* while loop */

	/*NOTREACHED*/
}

/*
 * uvmfault_relock: attempt to relock the same version of the map
 *
 * => fault data structures should be unlocked before calling.
 * => if a success (TRUE) maps will be locked after call.
 */
boolean_t
uvmfault_relock(struct uvm_faultinfo *ufi)
{
	/*
	 * ufi can be NULL when this isn't really a fault,
	 * but merely paging in anon data.
	 */

	if (ufi == NULL) {
		return TRUE;
	}

	uvmexp.fltrelck++;

	/*
	 * relock map.   fail if version mismatch (in which case nothing 
	 * gets locked).
	 */

	vm_map_lock_read(ufi->map);
	if (ufi->mapv != ufi->map->timestamp) {
		vm_map_unlock_read(ufi->map);
		return(FALSE);
	}

	uvmexp.fltrelckok++;
	return(TRUE);		/* got it! */
}
