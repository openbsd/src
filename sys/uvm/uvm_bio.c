/*	$NetBSD: uvm_bio.c,v 1.16 2001/07/18 16:44:39 thorpej Exp $	*/

/*
 * Copyright (c) 1998 Chuck Silvers.
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/*
 * uvm_bio.c: buffered i/o vnode mapping cache
 */


#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/vnode.h>

#include <uvm/uvm.h>
#include <uvm/uvm_page.h>

/*
 * global data structures
 */

/*
 * local functions
 */

static int	ubc_fault __P((struct uvm_faultinfo *, vaddr_t,
		    struct vm_page **, int, int, vm_fault_t, vm_prot_t, int));
static struct ubc_map *ubc_find_mapping __P((struct uvm_object *, voff_t));

/*
 * local data structues
 */

#define UBC_HASH(uobj, offset) (((((u_long)(uobj)) >> 8) + \
				 (((u_long)(offset)) >> PAGE_SHIFT)) & \
				ubc_object.hashmask)

#define UBC_QUEUE(offset) (&ubc_object.inactive[((offset) >> ubc_winshift) & \
					       (UBC_NQUEUES - 1)])

struct ubc_map
{
	struct uvm_object *	uobj;		/* mapped object */
	voff_t			offset;		/* offset into uobj */
	int			refcount;	/* refcount on mapping */
	voff_t			writeoff;	/* overwrite offset */
	vsize_t			writelen;	/* overwrite len */

	LIST_ENTRY(ubc_map)	hash;		/* hash table */
	TAILQ_ENTRY(ubc_map)	inactive;	/* inactive queue */
};

static struct ubc_object
{
	struct uvm_object uobj;		/* glue for uvm_map() */
	char *kva;			/* where ubc_object is mapped */
	struct ubc_map *umap;		/* array of ubc_map's */

	LIST_HEAD(, ubc_map) *hash;	/* hashtable for cached ubc_map's */
	u_long hashmask;		/* mask for hashtable */

	TAILQ_HEAD(ubc_inactive_head, ubc_map) *inactive;
					/* inactive queues for ubc_map's */

} ubc_object;

struct uvm_pagerops ubc_pager =
{
	NULL,		/* init */
	NULL,		/* reference */
	NULL,		/* detach */
	ubc_fault,	/* fault */
	/* ... rest are NULL */
};

int ubc_nwins = UBC_NWINS;
int ubc_winshift = UBC_WINSHIFT;
int ubc_winsize;
#ifdef PMAP_PREFER
int ubc_nqueues;
boolean_t ubc_release_unmap = FALSE;
#define UBC_NQUEUES ubc_nqueues
#define UBC_RELEASE_UNMAP ubc_release_unmap
#else
#define UBC_NQUEUES 1
#define UBC_RELEASE_UNMAP FALSE
#endif

/*
 * ubc_init
 *
 * init pager private data structures.
 */

void
ubc_init(void)
{
	struct ubc_map *umap;
	vaddr_t va;
	int i;

	/*
	 * Make sure ubc_winshift is sane.
	 */
	if (ubc_winshift < PAGE_SHIFT)
		ubc_winshift = PAGE_SHIFT;

	/*
	 * init ubc_object.
	 * alloc and init ubc_map's.
	 * init inactive queues.
	 * alloc and init hashtable.
	 * map in ubc_object.
	 */

	simple_lock_init(&ubc_object.uobj.vmobjlock);
	ubc_object.uobj.pgops = &ubc_pager;
	TAILQ_INIT(&ubc_object.uobj.memq);
	ubc_object.uobj.uo_npages = 0;
	ubc_object.uobj.uo_refs = UVM_OBJ_KERN;

	ubc_object.umap = malloc(ubc_nwins * sizeof(struct ubc_map),
				 M_TEMP, M_NOWAIT);
	if (ubc_object.umap == NULL)
		panic("ubc_init: failed to allocate ubc_map");
	memset(ubc_object.umap, 0, ubc_nwins * sizeof(struct ubc_map));

	va = (vaddr_t)1L;
#ifdef PMAP_PREFER
	PMAP_PREFER(0, &va);
	ubc_nqueues = va >> ubc_winshift;
	if (ubc_nqueues == 0) {
		ubc_nqueues = 1;
	}
	if (ubc_nqueues != 1) {
		ubc_release_unmap = TRUE;
	}
#endif
	ubc_winsize = 1 << ubc_winshift;
	ubc_object.inactive = malloc(UBC_NQUEUES *
				     sizeof(struct ubc_inactive_head),
				     M_TEMP, M_NOWAIT);
	if (ubc_object.inactive == NULL)
		panic("ubc_init: failed to allocate inactive queue heads");
	for (i = 0; i < UBC_NQUEUES; i++) {
		TAILQ_INIT(&ubc_object.inactive[i]);
	}
	for (i = 0; i < ubc_nwins; i++) {
		umap = &ubc_object.umap[i];
		TAILQ_INSERT_TAIL(&ubc_object.inactive[i & (UBC_NQUEUES - 1)],
				  umap, inactive);
	}

	ubc_object.hash = hashinit(ubc_nwins, M_TEMP, M_NOWAIT,
				   &ubc_object.hashmask);
	for (i = 0; i <= ubc_object.hashmask; i++) {
		LIST_INIT(&ubc_object.hash[i]);
	}

	if (uvm_map(kernel_map, (vaddr_t *)&ubc_object.kva,
		    ubc_nwins << ubc_winshift, &ubc_object.uobj, 0, (vsize_t)va,
		    UVM_MAPFLAG(UVM_PROT_ALL, UVM_PROT_ALL, UVM_INH_NONE,
				UVM_ADV_RANDOM, UVM_FLAG_NOMERGE)) != 0) {
		panic("ubc_init: failed to map ubc_object\n");
	}
	UVMHIST_INIT(ubchist, 300);
}


/*
 * ubc_fault: fault routine for ubc mapping
 */
int
ubc_fault(ufi, ign1, ign2, ign3, ign4, fault_type, access_type, flags)
	struct uvm_faultinfo *ufi;
	vaddr_t ign1;
	struct vm_page **ign2;
	int ign3, ign4;
	vm_fault_t fault_type;
	vm_prot_t access_type;
	int flags;
{
	struct uvm_object *uobj;
	struct vnode *vp;
	struct ubc_map *umap;
	vaddr_t va, eva, ubc_offset, slot_offset;
	int i, error, rv, npages;
	struct vm_page *pgs[(1 << ubc_winshift) >> PAGE_SHIFT], *pg;
	UVMHIST_FUNC("ubc_fault");  UVMHIST_CALLED(ubchist);

	/*
	 * no need to try with PGO_LOCKED...
	 * we don't need to have the map locked since we know that
	 * no one will mess with it until our reference is released.
	 */
	if (flags & PGO_LOCKED) {
#if 0
		return EBUSY;
#else
		uvmfault_unlockall(ufi, NULL, &ubc_object.uobj, NULL);
		flags &= ~PGO_LOCKED;
#endif
	}

	va = ufi->orig_rvaddr;
	ubc_offset = va - (vaddr_t)ubc_object.kva;

	UVMHIST_LOG(ubchist, "va 0x%lx ubc_offset 0x%lx at %d",
		    va, ubc_offset, access_type,0);

	umap = &ubc_object.umap[ubc_offset >> ubc_winshift];
	KASSERT(umap->refcount != 0);
	slot_offset = trunc_page(ubc_offset & (ubc_winsize - 1));

	/* no umap locking needed since we have a ref on the umap */
	uobj = umap->uobj;
	vp = (struct vnode *)uobj;
	KASSERT(uobj != NULL);

	npages = (ubc_winsize - slot_offset) >> PAGE_SHIFT;

	/*
	 * XXXUBC
	 * if npages is more than 1 we have to be sure that
	 * we set PGO_OVERWRITE correctly.
	 */
	if (access_type == VM_PROT_WRITE) {
		npages = 1;
	}

again:
	memset(pgs, 0, sizeof (pgs));
	simple_lock(&uobj->vmobjlock);

	UVMHIST_LOG(ubchist, "slot_offset 0x%x writeoff 0x%x writelen 0x%x "
		    "u_size 0x%x", slot_offset, umap->writeoff, umap->writelen,
		    vp->v_uvm.u_size);

	if (access_type & VM_PROT_WRITE &&
	    slot_offset >= umap->writeoff &&
	    (slot_offset + PAGE_SIZE <= umap->writeoff + umap->writelen ||
	     slot_offset + PAGE_SIZE >= vp->v_uvm.u_size - umap->offset)) {
		UVMHIST_LOG(ubchist, "setting PGO_OVERWRITE", 0,0,0,0);
		flags |= PGO_OVERWRITE;
	}
	else { UVMHIST_LOG(ubchist, "NOT setting PGO_OVERWRITE", 0,0,0,0); }
	/* XXX be sure to zero any part of the page past EOF */

	/*
	 * XXX
	 * ideally we'd like to pre-fault all of the pages we're overwriting.
	 * so for PGO_OVERWRITE, we should call VOP_GETPAGES() with all of the
	 * pages in [writeoff, writeoff+writesize] instead of just the one.
	 */

	UVMHIST_LOG(ubchist, "getpages vp %p offset 0x%x npages %d",
		    uobj, umap->offset + slot_offset, npages, 0);

	error = VOP_GETPAGES(vp, umap->offset + slot_offset, pgs, &npages, 0,
	    access_type, 0, flags);
	UVMHIST_LOG(ubchist, "getpages error %d npages %d", error, npages,0,0);

	if (error == EAGAIN) {
		tsleep(&lbolt, PVM, "ubc_fault", 0);
		goto again;
	}
	if (error) {
		return error;
	}
	if (npages == 0) {
		return 0;
	}

	va = ufi->orig_rvaddr;
	eva = ufi->orig_rvaddr + (npages << PAGE_SHIFT);

	UVMHIST_LOG(ubchist, "va 0x%lx eva 0x%lx", va, eva, 0,0);
	simple_lock(&uobj->vmobjlock);
	for (i = 0; va < eva; i++, va += PAGE_SIZE) {
		UVMHIST_LOG(ubchist, "pgs[%d] = %p", i, pgs[i],0,0);
		pg = pgs[i];

		if (pg == NULL || pg == PGO_DONTCARE) {
			continue;
		}
		if (pg->flags & PG_WANTED) {
			wakeup(pg);
		}
		KASSERT((pg->flags & PG_FAKE) == 0);
		if (pg->flags & PG_RELEASED) {
			rv = uobj->pgops->pgo_releasepg(pg, NULL);
			KASSERT(rv);
			continue;
		}
		KASSERT(access_type == VM_PROT_READ ||
			(pg->flags & PG_RDONLY) == 0);

		uvm_lock_pageq();
		uvm_pageactivate(pg);
		uvm_unlock_pageq();

		pmap_enter(ufi->orig_map->pmap, va, VM_PAGE_TO_PHYS(pg),
			   VM_PROT_READ | VM_PROT_WRITE, access_type);

		pg->flags &= ~(PG_BUSY);
		UVM_PAGE_OWN(pg, NULL);
	}
	simple_unlock(&uobj->vmobjlock);
	pmap_update();
	return 0;
}

/*
 * local functions
 */

struct ubc_map *
ubc_find_mapping(uobj, offset)
	struct uvm_object *uobj;
	voff_t offset;
{
	struct ubc_map *umap;

	LIST_FOREACH(umap, &ubc_object.hash[UBC_HASH(uobj, offset)], hash) {
		if (umap->uobj == uobj && umap->offset == offset) {
			return umap;
		}
	}
	return NULL;
}


/*
 * ubc interface functions
 */

/*
 * ubc_alloc:  allocate a buffer mapping
 */
void *
ubc_alloc(uobj, offset, lenp, flags)
	struct uvm_object *uobj;
	voff_t offset;
	vsize_t *lenp;
	int flags;
{
	int s;
	vaddr_t slot_offset, va;
	struct ubc_map *umap;
	voff_t umap_offset;
	UVMHIST_FUNC("ubc_alloc"); UVMHIST_CALLED(ubchist);

	UVMHIST_LOG(ubchist, "uobj %p offset 0x%lx len 0x%lx filesize 0x%x",
		    uobj, offset, *lenp, ((struct uvm_vnode *)uobj)->u_size);

	umap_offset = (offset & ~((voff_t)ubc_winsize - 1));
	slot_offset = (vaddr_t)(offset & ((voff_t)ubc_winsize - 1));
	*lenp = min(*lenp, ubc_winsize - slot_offset);

	/*
	 * the vnode is always locked here, so we don't need to add a ref.
	 */

	s = splbio();

again:
	simple_lock(&ubc_object.uobj.vmobjlock);
	umap = ubc_find_mapping(uobj, umap_offset);
	if (umap == NULL) {
		umap = TAILQ_FIRST(UBC_QUEUE(offset));
		if (umap == NULL) {
			simple_unlock(&ubc_object.uobj.vmobjlock);
			tsleep(&lbolt, PVM, "ubc_alloc", 0);
			goto again;
		}

		/*
		 * remove from old hash (if any),
		 * add to new hash.
		 */

		if (umap->uobj != NULL) {
			LIST_REMOVE(umap, hash);
		}

		umap->uobj = uobj;
		umap->offset = umap_offset;

		LIST_INSERT_HEAD(&ubc_object.hash[UBC_HASH(uobj, umap_offset)],
				 umap, hash);

		va = (vaddr_t)(ubc_object.kva +
			       ((umap - ubc_object.umap) << ubc_winshift));
		pmap_remove(pmap_kernel(), va, va + ubc_winsize);
		pmap_update();
	}

	if (umap->refcount == 0) {
		TAILQ_REMOVE(UBC_QUEUE(offset), umap, inactive);
	}

#ifdef DIAGNOSTIC
	if ((flags & UBC_WRITE) &&
	    (umap->writeoff || umap->writelen)) {
		panic("ubc_fault: concurrent writes vp %p", uobj);
	}
#endif
	if (flags & UBC_WRITE) {
		umap->writeoff = slot_offset;
		umap->writelen = *lenp;
	}

	umap->refcount++;
	simple_unlock(&ubc_object.uobj.vmobjlock);
	splx(s);
	UVMHIST_LOG(ubchist, "umap %p refs %d va %p",
		    umap, umap->refcount,
		    ubc_object.kva + ((umap - ubc_object.umap) << ubc_winshift),
		    0);

	return ubc_object.kva +
		((umap - ubc_object.umap) << ubc_winshift) + slot_offset;
}


void
ubc_release(va, wlen)
	void *va;
	vsize_t wlen;
{
	struct ubc_map *umap;
	struct uvm_object *uobj;
	int s;
	UVMHIST_FUNC("ubc_release"); UVMHIST_CALLED(ubchist);

	UVMHIST_LOG(ubchist, "va %p", va,0,0,0);

	s = splbio();
	simple_lock(&ubc_object.uobj.vmobjlock);

	umap = &ubc_object.umap[((char *)va - ubc_object.kva) >> ubc_winshift];
	uobj = umap->uobj;
	KASSERT(uobj != NULL);

	umap->writeoff = 0;
	umap->writelen = 0;
	umap->refcount--;
	if (umap->refcount == 0) {
		if (UBC_RELEASE_UNMAP &&
		    (((struct vnode *)uobj)->v_flag & VTEXT)) {
			vaddr_t va;

			/*
			 * if this file is the executable image of
			 * some process, that process will likely have
			 * the file mapped at an alignment other than
			 * what PMAP_PREFER() would like.  we'd like
			 * to have process text be able to use the
			 * cache even if someone is also reading the
			 * file, so invalidate mappings of such files
			 * as soon as possible.
			 */

			va = (vaddr_t)(ubc_object.kva +
			    ((umap - ubc_object.umap) << ubc_winshift));
			pmap_remove(pmap_kernel(), va, va + ubc_winsize);
			pmap_update();
			LIST_REMOVE(umap, hash);
			umap->uobj = NULL;
			TAILQ_INSERT_HEAD(UBC_QUEUE(umap->offset), umap,
			    inactive);
		} else {
			TAILQ_INSERT_TAIL(UBC_QUEUE(umap->offset), umap,
			    inactive);
		}
	}
	UVMHIST_LOG(ubchist, "umap %p refs %d", umap, umap->refcount,0,0);
	simple_unlock(&ubc_object.uobj.vmobjlock);
	splx(s);
}


/*
 * removing a range of mappings from the ubc mapping cache.
 */

void
ubc_flush(uobj, start, end)
	struct uvm_object *uobj;
	voff_t start, end;
{
	struct ubc_map *umap;
	vaddr_t va;
	int s;
	UVMHIST_FUNC("ubc_flush");  UVMHIST_CALLED(ubchist);

	UVMHIST_LOG(ubchist, "uobj %p start 0x%lx end 0x%lx",
		    uobj, start, end,0);

	s = splbio();
	simple_lock(&ubc_object.uobj.vmobjlock);
	for (umap = ubc_object.umap;
	     umap < &ubc_object.umap[ubc_nwins];
	     umap++) {

		if (umap->uobj != uobj ||
		    umap->offset < start ||
		    (umap->offset >= end && end != 0) ||
		    umap->refcount > 0) {
			continue;
		}

		/*
		 * remove from hash,
		 * move to head of inactive queue.
		 */

		va = (vaddr_t)(ubc_object.kva +
			       ((umap - ubc_object.umap) << ubc_winshift));
		pmap_remove(pmap_kernel(), va, va + ubc_winsize);
		pmap_update();

		LIST_REMOVE(umap, hash);
		umap->uobj = NULL;
		TAILQ_REMOVE(UBC_QUEUE(umap->offset), umap, inactive);
		TAILQ_INSERT_HEAD(UBC_QUEUE(umap->offset), umap, inactive);
	}
	simple_unlock(&ubc_object.uobj.vmobjlock);
	splx(s);
}
