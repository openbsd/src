/*	$OpenBSD: uvm_amap_i.h,v 1.18 2007/06/18 21:51:15 pedro Exp $	*/
/*	$NetBSD: uvm_amap_i.h,v 1.15 2000/11/25 06:27:59 chs Exp $	*/

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
 * from: Id: uvm_amap_i.h,v 1.1.2.4 1998/01/05 18:12:57 chuck Exp
 */

#ifndef _UVM_UVM_AMAP_I_H_
#define _UVM_UVM_AMAP_I_H_

/*
 * uvm_amap_i.h
 */

/*
 * if inlines are enabled always pull in these functions, otherwise
 * pull them in only once (when we are compiling uvm_amap.c).
 */

#if defined(UVM_AMAP_INLINE) || defined(UVM_AMAP_C)

/*
 * amap_lookup: look up a page in an amap
 *
 * => amap should be locked by caller.
 */
AMAP_INLINE struct vm_anon *
amap_lookup(aref, offset)
	struct vm_aref *aref;
	vaddr_t offset;
{
	int slot;
	struct vm_amap *amap = aref->ar_amap;
	UVMHIST_FUNC("amap_lookup"); UVMHIST_CALLED(maphist);

	AMAP_B2SLOT(slot, offset);
	slot += aref->ar_pageoff;

	if (slot >= amap->am_nslot)
		panic("amap_lookup: offset out of range");

	UVMHIST_LOG(maphist, "<- done (amap=%p, offset=0x%lx, result=%p)",
	    amap, offset, amap->am_anon[slot], 0);
	return(amap->am_anon[slot]);
}

/*
 * amap_lookups: look up a range of pages in an amap
 *
 * => amap should be locked by caller.
 * => XXXCDC: this interface is biased toward array-based amaps.  fix.
 */
AMAP_INLINE void
amap_lookups(aref, offset, anons, npages)
	struct vm_aref *aref;
	vaddr_t offset;
	struct vm_anon **anons;
	int npages;
{
	int slot;
	struct vm_amap *amap = aref->ar_amap;
	UVMHIST_FUNC("amap_lookups"); UVMHIST_CALLED(maphist);

	AMAP_B2SLOT(slot, offset);
	slot += aref->ar_pageoff;

	UVMHIST_LOG(maphist, "  slot=%ld, npages=%ld, nslot=%ld", slot, npages,
		amap->am_nslot, 0);

	if ((slot + (npages - 1)) >= amap->am_nslot)
		panic("amap_lookups: offset out of range");

	memcpy(anons, &amap->am_anon[slot], npages * sizeof(struct vm_anon *));

	UVMHIST_LOG(maphist, "<- done", 0, 0, 0, 0);
	return;
}

/*
 * amap_add: add (or replace) a page to an amap
 *
 * => caller must lock amap.   
 * => if (replace) caller must lock anon because we might have to call
 *	pmap_page_protect on the anon's page.
 * => returns an "offset" which is meaningful to amap_unadd().
 */
AMAP_INLINE void
amap_add(aref, offset, anon, replace)
	struct vm_aref *aref;
	vaddr_t offset;
	struct vm_anon *anon;
	boolean_t replace;
{
	int slot;
	struct vm_amap *amap = aref->ar_amap;
	UVMHIST_FUNC("amap_add"); UVMHIST_CALLED(maphist);

	AMAP_B2SLOT(slot, offset);
	slot += aref->ar_pageoff;

	if (slot >= amap->am_nslot)
		panic("amap_add: offset out of range");

	if (replace) {

		if (amap->am_anon[slot] == NULL)
			panic("amap_add: replacing null anon");
		if (amap->am_anon[slot]->an_page != NULL && 
		    (amap->am_flags & AMAP_SHARED) != 0) {
			pmap_page_protect(amap->am_anon[slot]->an_page,
			    VM_PROT_NONE);
			/*
			 * XXX: suppose page is supposed to be wired somewhere?
			 */
		}
	} else {   /* !replace */
		if (amap->am_anon[slot] != NULL)
			panic("amap_add: slot in use");

		amap->am_bckptr[slot] = amap->am_nused;
		amap->am_slots[amap->am_nused] = slot;
		amap->am_nused++;
	}
	amap->am_anon[slot] = anon;
	UVMHIST_LOG(maphist,
	    "<- done (amap=%p, offset=0x%lx, anon=%p, rep=%ld)",
	    amap, offset, anon, replace);
}

/*
 * amap_unadd: remove a page from an amap
 *
 * => caller must lock amap
 */
AMAP_INLINE void
amap_unadd(aref, offset)
	struct vm_aref *aref;
	vaddr_t offset;
{
	int ptr, slot;
	struct vm_amap *amap = aref->ar_amap;
	UVMHIST_FUNC("amap_unadd"); UVMHIST_CALLED(maphist);

	AMAP_B2SLOT(slot, offset);
	slot += aref->ar_pageoff;

	if (slot >= amap->am_nslot)
		panic("amap_unadd: offset out of range");

	if (amap->am_anon[slot] == NULL)
		panic("amap_unadd: nothing there");

	amap->am_anon[slot] = NULL;
	ptr = amap->am_bckptr[slot];

	if (ptr != (amap->am_nused - 1)) {	/* swap to keep slots contig? */
		amap->am_slots[ptr] = amap->am_slots[amap->am_nused - 1];
		amap->am_bckptr[amap->am_slots[ptr]] = ptr;	/* back link */
	}
	amap->am_nused--;
	UVMHIST_LOG(maphist, "<- done (amap=%p, slot=%ld)", amap, slot,0, 0);
}

/*
 * amap_ref: gain a reference to an amap
 *
 * => amap must not be locked (we will lock)
 * => "offset" and "len" are in units of pages
 * => called at fork time to gain the child's reference
 */
AMAP_INLINE void
amap_ref(amap, offset, len, flags)
	struct vm_amap *amap;
	vaddr_t offset;
	vsize_t len;
	int flags;
{
	UVMHIST_FUNC("amap_ref"); UVMHIST_CALLED(maphist);

	amap->am_ref++;
	if (flags & AMAP_SHARED)
		amap->am_flags |= AMAP_SHARED;
#ifdef UVM_AMAP_PPREF
	if (amap->am_ppref == NULL && (flags & AMAP_REFALL) == 0 &&
	    len != amap->am_nslot)
		amap_pp_establish(amap);
	if (amap->am_ppref && amap->am_ppref != PPREF_NONE) {
		if (flags & AMAP_REFALL)
			amap_pp_adjref(amap, 0, amap->am_nslot, 1);
		else
			amap_pp_adjref(amap, offset, len, 1);
	}
#endif
	UVMHIST_LOG(maphist,"<- done!  amap=%p", amap, 0, 0, 0);
}

/*
 * amap_unref: remove a reference to an amap
 *
 * => caller must remove all pmap-level references to this amap before
 *	dropping the reference
 * => called from uvm_unmap_detach [only]  ... note that entry is no
 *	longer part of a map and thus has no need for locking
 * => amap must be unlocked (we will lock it).
 */
AMAP_INLINE void
amap_unref(amap, offset, len, all)
	struct vm_amap *amap;
	vaddr_t offset;
	vsize_t len;
	boolean_t all;
{
	UVMHIST_FUNC("amap_unref"); UVMHIST_CALLED(maphist);

	UVMHIST_LOG(maphist,"  amap=%p  refs=%ld, nused=%ld",
	    amap, amap->am_ref, amap->am_nused, 0);

	/*
	 * if we are the last reference, free the amap and return.
	 */

	if (amap->am_ref-- == 1) {
		amap_wipeout(amap);	/* drops final ref and frees */
		UVMHIST_LOG(maphist,"<- done (was last ref)!", 0, 0, 0, 0);
		return;			/* no need to unlock */
	}

	/*
	 * otherwise just drop the reference count(s)
	 */
	if (amap->am_ref == 1 && (amap->am_flags & AMAP_SHARED) != 0)
		amap->am_flags &= ~AMAP_SHARED;	/* clear shared flag */
#ifdef UVM_AMAP_PPREF
	if (amap->am_ppref == NULL && all == 0 && len != amap->am_nslot)
		amap_pp_establish(amap);
	if (amap->am_ppref && amap->am_ppref != PPREF_NONE) {
		if (all)
			amap_pp_adjref(amap, 0, amap->am_nslot, -1);
		else
			amap_pp_adjref(amap, offset, len, -1);
	}
#endif

	UVMHIST_LOG(maphist,"<- done!", 0, 0, 0, 0);
}

#endif /* defined(UVM_AMAP_INLINE) || defined(UVM_AMAP_C) */

#endif /* _UVM_UVM_AMAP_I_H_ */
