/*	$NetBSD: uvm_fault_i.h,v 1.9 1999/06/04 23:38:41 thorpej Exp $	*/

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
 * from: Id: uvm_fault_i.h,v 1.1.6.1 1997/12/08 16:07:12 chuck Exp
 */

#ifndef _UVM_UVM_FAULT_I_H_
#define _UVM_UVM_FAULT_I_H_

/*
 * uvm_fault_i.h: fault inline functions
 */

/*
 * uvmfault_unlockmaps: unlock the maps
 */

static __inline void
uvmfault_unlockmaps(ufi, write_locked)
	struct uvm_faultinfo *ufi;
	boolean_t write_locked;
{

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

static __inline void
uvmfault_unlockall(ufi, amap, uobj, anon)
	struct uvm_faultinfo *ufi;
	struct vm_amap *amap;
	struct uvm_object *uobj;
	struct vm_anon *anon;
{

	if (anon)
		simple_unlock(&anon->an_lock);
	if (uobj)
		simple_unlock(&uobj->vmobjlock);
	if (amap)
		amap_unlock(amap);
	uvmfault_unlockmaps(ufi, FALSE);
}

/*
 * uvmfault_check_intrsafe: check for a virtual address managed by
 * an interrupt-safe map.
 *
 * => caller must provide a uvm_faultinfo structure with the IN
 *	params properly filled in
 * => if we find an intersafe VA, we fill in ufi->map, and return TRUE
 */

static __inline boolean_t
uvmfault_check_intrsafe(ufi)
	struct uvm_faultinfo *ufi;
{
	struct vm_map_intrsafe *vmi;
	int s;

	s = vmi_list_lock();
	for (vmi = LIST_FIRST(&vmi_list); vmi != NULL;
	     vmi = LIST_NEXT(vmi, vmi_list)) {
		if (ufi->orig_rvaddr >= vm_map_min(&vmi->vmi_map) &&
		    ufi->orig_rvaddr < vm_map_max(&vmi->vmi_map))
			break;
	}
	vmi_list_unlock(s);

	if (vmi != NULL) {
		ufi->map = &vmi->vmi_map;
		return (TRUE);
	}

	return (FALSE);
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

static __inline boolean_t
uvmfault_lookup(ufi, write_lock)
	struct uvm_faultinfo *ufi;
	boolean_t write_lock;
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
			if (write_lock) {
				vm_map_unlock(ufi->map);
			} else {
				vm_map_unlock_read(ufi->map);
			}
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

static __inline boolean_t
uvmfault_relock(ufi)
	struct uvm_faultinfo *ufi;
{

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

#endif /* _UVM_UVM_FAULT_I_H_ */
