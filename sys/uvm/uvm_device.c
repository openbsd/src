/*	$OpenBSD: uvm_device.c,v 1.2 1999/02/26 05:32:06 art Exp $	*/
/*	$NetBSD: uvm_device.c,v 1.11 1998/11/19 05:23:26 mrg Exp $	*/

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
 * from: Id: uvm_device.c,v 1.1.2.9 1998/02/06 05:11:47 chs Exp
 */

/*
 * uvm_device.c: the device pager.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/vnode.h>

#include <vm/vm.h>
#include <vm/vm_page.h>
#include <vm/vm_kern.h>

#include <uvm/uvm.h>
#include <uvm/uvm_device.h>

/*
 * private global data structure
 *
 * we keep a list of active device objects in the system.
 */

LIST_HEAD(udv_list_struct, uvm_device);
static struct udv_list_struct udv_list;
static simple_lock_data_t udv_lock;

/*
 * functions
 */

static void		udv_init __P((void));
struct uvm_object 	*udv_attach __P((void *, vm_prot_t));
static void             udv_reference __P((struct uvm_object *));
static void             udv_detach __P((struct uvm_object *));
static int		udv_fault __P((struct uvm_faultinfo *, vaddr_t,
				       vm_page_t *, int, int, vm_fault_t,
				       vm_prot_t, int));
static boolean_t        udv_flush __P((struct uvm_object *, vaddr_t, 
					 vaddr_t, int));
static int		udv_asyncget __P((struct uvm_object *, vaddr_t,
					    int));
static int		udv_put __P((struct uvm_object *, vm_page_t *,
					int, boolean_t));

/*
 * master pager structure
 */

struct uvm_pagerops uvm_deviceops = {
	udv_init,
	udv_attach,
	udv_reference,
	udv_detach,
	udv_fault,
	udv_flush,
	NULL,		/* no get function since we have udv_fault */
	udv_asyncget,
	udv_put,
	NULL,		/* no cluster function */
	NULL,		/* no put cluster function */
	NULL,		/* no share protect.   no share maps for us */
	NULL,		/* no AIO-DONE function since no async i/o */
	NULL,		/* no releasepg function since no normal pages */
};

/*
 * the ops!
 */

/*
 * udv_init
 *
 * init pager private data structures.
 */

void
udv_init()
{

	LIST_INIT(&udv_list);
	simple_lock_init(&udv_lock);
}

/*
 * udv_attach
 *
 * get a VM object that is associated with a device.   allocate a new
 * one if needed.
 *
 * => caller must _not_ already be holding the lock on the uvm_object.
 * => in fact, nothing should be locked so that we can sleep here.
 */
struct uvm_object *
udv_attach(arg, accessprot)
	void *arg;
	vm_prot_t accessprot;
{
	dev_t device = *((dev_t *) arg);
	struct uvm_device *udv, *lcv;
	int (*mapfn) __P((dev_t, int, int));
	UVMHIST_FUNC("udv_attach"); UVMHIST_CALLED(maphist);

	UVMHIST_LOG(maphist, "(device=0x%x)", device,0,0,0);

	/*
	 * before we do anything, ensure this device supports mmap
	 */

	mapfn = cdevsw[major(device)].d_mmap;
	if (mapfn == NULL ||
			mapfn == (int (*) __P((dev_t, int, int))) enodev ||
			mapfn == (int (*) __P((dev_t, int, int))) nullop)
		return(NULL);

	/*
	 * keep looping until we get it
	 */

	while (1) {

		/*
		 * first, attempt to find it on the main list 
		 */

		simple_lock(&udv_lock);
		for (lcv = udv_list.lh_first ; lcv != NULL ; lcv = lcv->u_list.le_next) {
			if (device == lcv->u_device)
				break;
		}

		/*
		 * got it on main list.  put a hold on it and unlock udv_lock.
		 */

		if (lcv) {

			/*
			 * if someone else has a hold on it, sleep and start
			 * over again.
			 */

			if (lcv->u_flags & UVM_DEVICE_HOLD) {
				lcv->u_flags |= UVM_DEVICE_WANTED;
				UVM_UNLOCK_AND_WAIT(lcv, &udv_lock, FALSE,
				    "udv_attach",0);
				continue;
			}

			/* we are now holding it */
			lcv->u_flags |= UVM_DEVICE_HOLD;
			simple_unlock(&udv_lock);

			/*
			 * bump reference count, unhold, return.
			 */

			simple_lock(&lcv->u_obj.vmobjlock);
			lcv->u_obj.uo_refs++;
			simple_unlock(&lcv->u_obj.vmobjlock);
			
			simple_lock(&udv_lock);
			if (lcv->u_flags & UVM_DEVICE_WANTED)
				wakeup(lcv);
			lcv->u_flags &= ~(UVM_DEVICE_WANTED|UVM_DEVICE_HOLD);
			simple_unlock(&udv_lock);
			return(&lcv->u_obj);
		}

		/*
		 * did not find it on main list.   need to malloc a new one.
		 */

		simple_unlock(&udv_lock);
		/* NOTE: we could sleep in the following malloc() */
		MALLOC(udv, struct uvm_device *, sizeof(*udv), M_TEMP, M_WAITOK);
		simple_lock(&udv_lock);

		/*
		 * now we have to double check to make sure no one added it
		 * to the list while we were sleeping...
		 */

		for (lcv = udv_list.lh_first ; lcv != NULL ;
		    lcv = lcv->u_list.le_next) {
			if (device == lcv->u_device)
				break;
		}

		/*
		 * did we lose a race to someone else?   free our memory and retry.
		 */

		if (lcv) {
			simple_unlock(&udv_lock);
			FREE(udv, M_TEMP);
			continue;
		}

		/*
		 * we have it!   init the data structures, add to list
		 * and return.
		 */

		simple_lock_init(&udv->u_obj.vmobjlock);
		udv->u_obj.pgops = &uvm_deviceops;
		TAILQ_INIT(&udv->u_obj.memq);	/* not used, but be safe */
		udv->u_obj.uo_npages = 0;
		udv->u_obj.uo_refs = 1;
		udv->u_flags = 0;
		udv->u_device = device;
		LIST_INSERT_HEAD(&udv_list, udv, u_list);
		simple_unlock(&udv_lock);

		return(&udv->u_obj);

	}  /* while(1) loop */

	/*NOTREACHED*/
}
	
/*
 * udv_reference
 *
 * add a reference to a VM object.   Note that the reference count must
 * already be one (the passed in reference) so there is no chance of the
 * udv being released or locked out here.
 *
 * => caller must call with object unlocked.
 */

static void
udv_reference(uobj)
	struct uvm_object *uobj;
{
	UVMHIST_FUNC("udv_reference"); UVMHIST_CALLED(maphist);

	simple_lock(&uobj->vmobjlock);
	uobj->uo_refs++;
	UVMHIST_LOG(maphist, "<- done (uobj=0x%x, ref = %d)", 
	uobj, uobj->uo_refs,0,0);
	simple_unlock(&uobj->vmobjlock);
}

/*
 * udv_detach
 *
 * remove a reference to a VM object.
 *
 * => caller must call with object unlocked and map locked.
 */

static void
udv_detach(uobj)
	struct uvm_object *uobj;
{
	struct uvm_device *udv = (struct uvm_device *) uobj;
	UVMHIST_FUNC("udv_detach"); UVMHIST_CALLED(maphist);

	/*
	 * loop until done
	 */

	while (1) {
		simple_lock(&uobj->vmobjlock);
		
		if (uobj->uo_refs > 1) {
			uobj->uo_refs--;			/* drop ref! */
			simple_unlock(&uobj->vmobjlock);
			UVMHIST_LOG(maphist," <- done, uobj=0x%x, ref=%d", 
				  uobj,uobj->uo_refs,0,0);
			return;
		}

#ifdef DIAGNOSTIC
		if (uobj->uo_npages || uobj->memq.tqh_first)
			panic("udv_detach: pages in a device object?");
#endif

		/*
		 * now lock udv_lock
		 */
		simple_lock(&udv_lock);

		/*
		 * is it being held?   if so, wait until others are done.
		 */
		if (udv->u_flags & UVM_DEVICE_HOLD) {

			/*
			 * want it
			 */
			udv->u_flags |= UVM_DEVICE_WANTED;
			simple_unlock(&uobj->vmobjlock);
			UVM_UNLOCK_AND_WAIT(udv, &udv_lock, FALSE, "udv_detach",0);
			continue;
		}

		/*
		 * got it!   nuke it now.
		 */

		LIST_REMOVE(udv, u_list);
		if (udv->u_flags & UVM_DEVICE_WANTED)
			wakeup(udv);
		FREE(udv, M_TEMP);
		break;	/* DONE! */

	}	/* while (1) loop */

	UVMHIST_LOG(maphist," <- done, freed uobj=0x%x", uobj,0,0,0);
	return;
}


/*
 * udv_flush
 *
 * flush pages out of a uvm object.   a no-op for devices.
 */

static boolean_t udv_flush(uobj, start, stop, flags)
	struct uvm_object *uobj;
	vaddr_t start, stop;
	int flags;
{

	return(TRUE);
}

/*
 * udv_fault: non-standard fault routine for device "pages"
 *
 * => rather than having a "get" function, we have a fault routine
 *	since we don't return vm_pages we need full control over the
 *	pmap_enter map in
 * => all the usual fault data structured are locked by the caller
 *	(i.e. maps(read), amap (if any), uobj)
 * => on return, we unlock all fault data structures
 * => flags: PGO_ALLPAGES: get all of the pages
 *	     PGO_LOCKED: fault data structures are locked
 *    XXX: currently PGO_LOCKED is always required ... consider removing
 *	it as a flag
 * => NOTE: vaddr is the VA of pps[0] in ufi->entry, _NOT_ pps[centeridx]
 */

static int
udv_fault(ufi, vaddr, pps, npages, centeridx, fault_type, access_type, flags)
	struct uvm_faultinfo *ufi;
	vaddr_t vaddr;
	vm_page_t *pps;
	int npages, centeridx, flags;
	vm_fault_t fault_type;
	vm_prot_t access_type;
{
	struct vm_map_entry *entry = ufi->entry;
	struct uvm_object *uobj = entry->object.uvm_obj;
	struct uvm_device *udv = (struct uvm_device *)uobj;
	vaddr_t curr_offset, curr_va;
	paddr_t paddr;
	int lcv, retval, mdpgno;
	dev_t device;
	int (*mapfn) __P((dev_t, int, int));
	UVMHIST_FUNC("udv_fault"); UVMHIST_CALLED(maphist);
	UVMHIST_LOG(maphist,"  flags=%d", flags,0,0,0);

	/*
	 * XXX: !PGO_LOCKED calls are currently not allowed (or used)
	 */

	if ((flags & PGO_LOCKED) == 0)
		panic("udv_fault: !PGO_LOCKED fault");

	/*
	 * we do not allow device mappings to be mapped copy-on-write
	 * so we kill any attempt to do so here.
	 */
	
	if (UVM_ET_ISCOPYONWRITE(entry)) {
		UVMHIST_LOG(maphist, "<- failed -- COW entry (etype=0x%x)", 
		entry->etype, 0,0,0);
		uvmfault_unlockall(ufi, ufi->entry->aref.ar_amap, uobj, NULL);
		return(VM_PAGER_ERROR);
	}

	/*
	 * get device map function.   
	 */
	device = udv->u_device;
	mapfn = cdevsw[major(device)].d_mmap;

	/*
	 * now we must determine the offset in udv to use and the VA to
	 * use for pmap_enter.  note that we always use orig_map's pmap
	 * for pmap_enter (even if we have a submap).   since virtual
	 * addresses in a submap must match the main map, this is ok.
	 */
	/* udv offset = (offset from start of entry) + entry's offset */
	curr_offset = (vaddr - entry->start) + entry->offset;	
	/* pmap va = vaddr (virtual address of pps[0]) */
	curr_va = vaddr;
	
	/*
	 * loop over the page range entering in as needed
	 */

	retval = VM_PAGER_OK;
	for (lcv = 0 ; lcv < npages ; lcv++, curr_offset += PAGE_SIZE,
	    curr_va += PAGE_SIZE) {
		if ((flags & PGO_ALLPAGES) == 0 && lcv != centeridx)
			continue;

		if (pps[lcv] == PGO_DONTCARE)
			continue;

		mdpgno = (*mapfn)(device, (int)curr_offset, access_type);
		if (mdpgno == -1) {
			retval = VM_PAGER_ERROR;
			break;
		}
		paddr = pmap_phys_address(mdpgno);
		UVMHIST_LOG(maphist,
		    "  MAPPING: device: pm=0x%x, va=0x%x, pa=0x%x, at=%d",
		    ufi->orig_map->pmap, curr_va, (int)paddr, access_type);
		pmap_enter(ufi->orig_map->pmap, curr_va, paddr, access_type, 0);

	}

	uvmfault_unlockall(ufi, ufi->entry->aref.ar_amap, uobj, NULL);
	return(retval);
}

/*
 * udv_asyncget: start async I/O to bring pages into ram
 *
 * => caller must lock object(???XXX: see if this is best)
 * => a no-op for devices
 */

static int
udv_asyncget(uobj, offset, npages)
	struct uvm_object *uobj;
	vaddr_t offset;
	int npages;
{

	return(KERN_SUCCESS);
}

/*
 * udv_put: flush page data to backing store.
 *
 * => this function should never be called (since we never have any
 *	page structures to "put")
 */

static int
udv_put(uobj, pps, npages, flags)
	struct uvm_object *uobj;
	struct vm_page **pps;
	int npages, flags;
{

	panic("udv_put: trying to page out to a device!");
}
