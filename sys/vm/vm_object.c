/*	$OpenBSD: vm_object.c,v 1.22 1998/04/25 07:17:21 niklas Exp $	*/
/*	$NetBSD: vm_object.c,v 1.46 1997/03/30 20:56:12 mycroft Exp $	*/

/*-
 * Copyright (c) 1997 Charles M. Hannum.  All rights reserved.
 * Copyright (c) 1997 Niklas Hallqvist.  All rights reserved.
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
 *	This product includes software developed by Charles M. Hannum.
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
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 *	@(#)vm_object.c	8.7 (Berkeley) 5/11/95
 *
 *
 * Copyright (c) 1987, 1990 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Authors: Avadis Tevanian, Jr., Michael Wayne Young
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
 *	Virtual memory object module.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/proc.h>

#include <vm/vm.h>
#include <vm/vm_page.h>
#include <vm/vm_pageout.h>

/*
 * Virtual memory objects maintain the actual data
 * associated with allocated virtual memory.  A given
 * page of memory exists within exactly one object.
 *
 * An object is only deallocated when all "references"
 * are given up.  Only one "reference" to a given
 * region of an object should be writeable.
 *
 * Associated with each object is a list of all resident
 * memory pages belonging to that object; this list is
 * maintained by the "vm_page" module, and locked by the object's
 * lock.
 *
 * Each object also records a "pager" routine which is
 * used to retrieve (and store) pages to the proper backing
 * storage.  In addition, objects may be backed by other
 * objects from which they were virtual-copied.
 *
 * The only items within the object structure which are
 * modified after time of creation are:
 *	reference count		locked by object's lock
 *	pager routine		locked by object's lock
 *
 */

struct vm_object	kernel_object_store;
struct vm_object	kmem_object_store;

#define	VM_OBJECT_HASH_COUNT	157

extern int vm_cache_max;	/* now in param.c */
struct	vm_object_hash_head vm_object_hashtable[VM_OBJECT_HASH_COUNT];

long	object_collapses = 0;
long	object_bypasses  = 0;
boolean_t vm_object_collapse_allowed = TRUE;

#ifndef VMDEBUG
#define VMDEBUG 0
#endif

#ifdef DEBUG
#define	VMDEBUG_SHADOW		0x1
#define	VMDEBUG_SHADOW_VERBOSE	0x2
#define	VMDEBUG_COLLAPSE	0x4
#define	VMDEBUG_COLLAPSE_PAGEIN	0x8
int	vmdebug = VMDEBUG;
#endif

void		_vm_object_allocate __P((vm_size_t, vm_object_t));
int		vm_object_bypass __P((vm_object_t));
void		vm_object_collapse_internal __P((vm_object_t, vm_object_t *));
int		vm_object_overlay __P((vm_object_t));
int		vm_object_remove_from_pager
		    __P((vm_object_t, vm_offset_t, vm_offset_t));
void		vm_object_set_shadow __P((vm_object_t, vm_object_t));

/*
 * vm_object_init:
 *
 * Initialize the VM objects module.
 */
void
vm_object_init(size)
	vm_size_t	size;
{
	register int	i;

	TAILQ_INIT(&vm_object_cached_list);
	TAILQ_INIT(&vm_object_list);
	vm_object_count = 0;
	simple_lock_init(&vm_cache_lock);
	simple_lock_init(&vm_object_list_lock);

	for (i = 0; i < VM_OBJECT_HASH_COUNT; i++)
		TAILQ_INIT(&vm_object_hashtable[i]);

	kernel_object = &kernel_object_store;
	_vm_object_allocate(size, kernel_object);

	kmem_object = &kmem_object_store;
	_vm_object_allocate(VM_KMEM_SIZE + VM_MBUF_SIZE, kmem_object);
}

/*
 * vm_object_allocate:
 *
 * Returns a new object with the given size.
 */
vm_object_t
vm_object_allocate(size)
	vm_size_t	size;
{
	register vm_object_t	result;

	result = (vm_object_t)malloc((u_long)sizeof *result, M_VMOBJ,
	    M_WAITOK);

	_vm_object_allocate(size, result);

	return(result);
}

void
_vm_object_allocate(size, object)
	vm_size_t		size;
	register vm_object_t	object;
{
	TAILQ_INIT(&object->memq);
	vm_object_lock_init(object);
	object->ref_count = 1;
	object->resident_page_count = 0;
	object->size = size;
	object->flags = OBJ_INTERNAL;	/* vm_allocate_with_pager will reset */
	object->paging_in_progress = 0;
	object->copy = NULL;

	/*
	 *	Object starts out read-write, with no pager.
	 */

	object->pager = NULL;
	object->paging_offset = 0;
	object->shadow = NULL;
	object->shadow_offset = (vm_offset_t) 0;
	LIST_INIT(&object->shadowers);

	simple_lock(&vm_object_list_lock);
	TAILQ_INSERT_TAIL(&vm_object_list, object, object_list);
	vm_object_count++;
	cnt.v_nzfod += atop(size);
	simple_unlock(&vm_object_list_lock);
}

/*
 * vm_object_reference:
 *
 * Gets another reference to the given object.
 */
void
vm_object_reference(object)
	register vm_object_t	object;
{
	if (object == NULL)
		return;

	vm_object_lock(object);
	object->ref_count++;
	vm_object_unlock(object);
}

/*
 * vm_object_deallocate:
 *
 * Release a reference to the specified object,
 * gained either through a vm_object_allocate
 * or a vm_object_reference call.  When all references
 * are gone, storage associated with this object
 * may be relinquished.
 *
 * No object may be locked.
 */
void
vm_object_deallocate(object)
	vm_object_t	object;
{
	/*
	 * While "temp" is used for other things as well, we
	 * initialize it to NULL here for being able to check
	 * if we are in the first revolution of the loop.
	 */
	vm_object_t	temp = NULL;

	while (object != NULL) {

		/*
		 * The cache holds a reference (uncounted) to the object; we
		 * must lock it before removing the object.
		 */

		vm_object_cache_lock();

		/*
		 * Lose the reference
		 */
		vm_object_lock(object);
		if (--(object->ref_count) != 0) {
			vm_object_unlock(object);
			vm_object_cache_unlock();

			/*
			 * If this is a deallocation of a shadow reference
			 * (which it is unless it's the first time round) and
			 * this operation made us singly-shadowed, try to
			 * collapse us with our shadower.  Otherwise we're
			 * ready.
			 */
			if (temp != NULL &&
			    (temp = object->shadowers.lh_first) != NULL &&
			    temp->shadowers_list.le_next == NULL) {
				vm_object_lock(temp);

				/*
				 * This is a bit tricky: the temp object can
				 * go away while collapsing, check the
				 * vm_object_collapse_internal comments for
				 * details.  In this case we get an object
				 * back to deallocate (it's done like this
				 * to prevent potential recursion and hence
				 * kernel stack overflow).  In the normal case
				 * we won't get an object back, if so, we are
				 * ready and may return.
				 */
				vm_object_collapse_internal(temp, &object);
				if (object != NULL) {
					vm_object_lock(object);
					vm_object_cache_lock();
				} else {
					vm_object_unlock(temp);
					return;
				}
			} else
				return;
		}

		/*
		 * See if this object can persist.  If so, enter it in the
		 * cache, then deactivate all of its pages.
		 */
		if (object->flags & OBJ_CANPERSIST) {

			TAILQ_INSERT_TAIL(&vm_object_cached_list, object,
			    cached_list);
			vm_object_cached++;
			vm_object_cache_unlock();

			vm_object_deactivate_pages(object);
			vm_object_unlock(object);

			vm_object_cache_trim();
			return;
		}

		/*
		 * Make sure no one can look us up now.
		 */
		vm_object_remove(object->pager);
		vm_object_cache_unlock();

		/*
		 * Deallocate the object, and move on to the backing object.
		 */
		temp = object->shadow;
		vm_object_reference(temp);
		vm_object_terminate(object);
		object = temp;
	}
}


/*
 * vm_object_terminate actually destroys the specified object, freeing
 * up all previously used resources.
 *
 * The object must be locked.
 */
void
vm_object_terminate(object)
	register vm_object_t	object;
{
	register vm_page_t	p;
	vm_object_t		shadow_object;

	/*
	 * Protect against simultaneous collapses.
	 */
	object->flags |= OBJ_FADING;

	/*
	 * Wait until the pageout daemon is through with the object or a
	 * potential collapse operation is finished.
	 */
	vm_object_paging_wait(object,"vmterm");

	/*
	 * Detach the object from its shadow if we are the shadow's
	 * copy.
	 */
	if ((shadow_object = object->shadow) != NULL) {
		vm_object_lock(shadow_object);
		vm_object_set_shadow(object, NULL);
		if (shadow_object->copy == object)
			shadow_object->copy = NULL;
#if 0
		else if (shadow_object->copy != NULL)
			panic("vm_object_terminate: "
			    "copy/shadow inconsistency");
#endif
		vm_object_unlock(shadow_object);
	}

	/*
	 * If not an internal object clean all the pages, removing them
	 * from paging queues as we go.
	 *
	 * XXX need to do something in the event of a cleaning error.
	 */
	if ((object->flags & OBJ_INTERNAL) == 0)
		(void) vm_object_page_clean(object, 0, 0, TRUE, TRUE);

	/*
	 * Now free the pages.
	 * For internal objects, this also removes them from paging queues.
	 */
	while ((p = object->memq.tqh_first) != NULL) {
		VM_PAGE_CHECK(p);
		vm_page_lock_queues();
		vm_page_free(p);
		cnt.v_pfree++;
		vm_page_unlock_queues();
	}
	if ((object->flags & OBJ_INTERNAL) != 0)
		vm_object_unlock(object);

	/*
	 * Let the pager know object is dead.
	 */
	if (object->pager != NULL)
		vm_pager_deallocate(object->pager);

	simple_lock(&vm_object_list_lock);
	TAILQ_REMOVE(&vm_object_list, object, object_list);
	vm_object_count--;
	simple_unlock(&vm_object_list_lock);

	/*
	 * Free the space for the object.
	 */
	free((caddr_t)object, M_VMOBJ);
}

/*
 * vm_object_page_clean
 *
 * Clean all dirty pages in the specified range of object.
 * If syncio is TRUE, page cleaning is done synchronously.
 * If de_queue is TRUE, pages are removed from any paging queue
 * they were on, otherwise they are left on whatever queue they
 * were on before the cleaning operation began.
 *
 * Odd semantics: if start == end, we clean everything.
 *
 * The object must be locked.
 *
 * Returns TRUE if all was well, FALSE if there was a pager error
 * somewhere.  We attempt to clean (and dequeue) all pages regardless
 * of where an error occurs.
 */
boolean_t
vm_object_page_clean(object, start, end, syncio, de_queue)
	register vm_object_t	object;
	register vm_offset_t	start;
	register vm_offset_t	end;
	boolean_t		syncio;
	boolean_t		de_queue;
{
	register vm_page_t	p;
	int onqueue = 0;
	boolean_t noerror = TRUE;

	if (object == NULL)
		return (TRUE);

	/*
	 * If it is an internal object and there is no pager, attempt to
	 * allocate one.  Note that vm_object_collapse may relocate one
	 * from a collapsed object so we must recheck afterward.
	 */
	if ((object->flags & OBJ_INTERNAL) && object->pager == NULL) {
		vm_object_collapse(object);
		if (object->pager == NULL) {
			vm_pager_t pager;

			vm_object_unlock(object);
			pager = vm_pager_allocate(PG_DFLT, (caddr_t)0,
			    object->size, VM_PROT_ALL, (vm_offset_t)0);
			if (pager)
				vm_object_setpager(object, pager, 0, FALSE);
			vm_object_lock(object);
		}
	}
	if (object->pager == NULL)
		return (FALSE);

again:
	/*
	 * Wait until the pageout daemon is through with the object.
	 */
	vm_object_paging_wait(object,"vclean");

	/*
	 * Loop through the object page list cleaning as necessary.
	 */
	for (p = object->memq.tqh_first; p != NULL; p = p->listq.tqe_next) {
		if ((start == end || (p->offset >= start && p->offset < end)) &&
		    !(p->flags & PG_FICTITIOUS)) {
			if ((p->flags & PG_CLEAN) &&
			    pmap_is_modified(VM_PAGE_TO_PHYS(p)))
				p->flags &= ~PG_CLEAN;
			/*
			 * Remove the page from any paging queue.
			 * This needs to be done if either we have been
			 * explicitly asked to do so or it is about to
			 * be cleaned (see comment below).
			 */
			if (de_queue || !(p->flags & PG_CLEAN)) {
				vm_page_lock_queues();
				if (p->flags & PG_ACTIVE) {
					TAILQ_REMOVE(&vm_page_queue_active,
						     p, pageq);
					p->flags &= ~PG_ACTIVE;
					cnt.v_active_count--;
					onqueue = 1;
				} else if (p->flags & PG_INACTIVE) {
					TAILQ_REMOVE(&vm_page_queue_inactive,
						     p, pageq);
					p->flags &= ~PG_INACTIVE;
					cnt.v_inactive_count--;
					onqueue = -1;
				} else
					onqueue = 0;
				vm_page_unlock_queues();
			}
			/*
			 * To ensure the state of the page doesn't change
			 * during the clean operation we do two things.
			 * First we set the busy bit and write-protect all
			 * mappings to ensure that write accesses to the
			 * page block (in vm_fault).  Second, we remove
			 * the page from any paging queue to foil the
			 * pageout daemon (vm_pageout_scan).
			 */
			pmap_page_protect(VM_PAGE_TO_PHYS(p), VM_PROT_READ);
			if (!(p->flags & PG_CLEAN)) {
				p->flags |= PG_BUSY;
				vm_object_paging_begin(object);
				vm_object_unlock(object);
				/*
				 * XXX if put fails we mark the page as
				 * clean to avoid an infinite loop.
				 * Will loose changes to the page.
				 */
				if (vm_pager_put(object->pager, p, syncio)) {
					printf("%s: pager_put error\n",
					    "vm_object_page_clean");
					p->flags |= PG_CLEAN;
					noerror = FALSE;
				}
				vm_object_lock(object);
				vm_object_paging_end(object);
				if (!de_queue && onqueue) {
					vm_page_lock_queues();
					if (onqueue > 0)
						vm_page_activate(p);
					else
						vm_page_deactivate(p);
					vm_page_unlock_queues();
				}
				p->flags &= ~PG_BUSY;
				PAGE_WAKEUP(p);
				goto again;
			}
		}
	}
	return (noerror);
}

/*
 * vm_object_deactivate_pages
 *
 * Deactivate all pages in the specified object.  (Keep its pages
 * in memory even though it is no longer referenced.)
 *
 * The object must be locked.
 */
void
vm_object_deactivate_pages(object)
	register vm_object_t	object;
{
	register vm_page_t	p, next;

	for (p = object->memq.tqh_first; p != NULL; p = next) {
		next = p->listq.tqe_next;
		vm_page_lock_queues();
		if (p->flags & PG_ACTIVE)
			vm_page_deactivate(p);
		vm_page_unlock_queues();
	}
}

/*
 * Trim the object cache to size.
 */
void
vm_object_cache_trim()
{
	register vm_object_t	object;

	vm_object_cache_lock();
	while (vm_object_cached > vm_cache_max) {
		object = vm_object_cached_list.tqh_first;
		vm_object_cache_unlock();

		if (object != vm_object_lookup(object->pager))
			panic("vm_object_cache_trim: I'm sooo confused.");

		pager_cache(object, FALSE);

		vm_object_cache_lock();
	}
	vm_object_cache_unlock();
}

/*
 * vm_object_pmap_copy:
 *
 * Makes all physical pages in the specified
 * object range copy-on-write.  No writeable
 * references to these pages should remain.
 *
 * The object must *not* be locked.
 */
void
vm_object_pmap_copy(object, start, end)
	register vm_object_t	object;
	register vm_offset_t	start;
	register vm_offset_t	end;
{
	register vm_page_t	p;

	if (object == NULL)
		return;

	vm_object_lock(object);
	for (p = object->memq.tqh_first; p != NULL; p = p->listq.tqe_next) {
		if ((start <= p->offset) && (p->offset < end)) {
			pmap_page_protect(VM_PAGE_TO_PHYS(p), VM_PROT_READ);
			p->flags |= PG_COPYONWRITE;
		}
	}
	vm_object_unlock(object);
}

/*
 * vm_object_pmap_remove:
 *
 * Removes all physical pages in the specified
 * object range from all physical maps.
 *
 * The object must *not* be locked.
 */
void
vm_object_pmap_remove(object, start, end)
	register vm_object_t	object;
	register vm_offset_t	start;
	register vm_offset_t	end;
{
	register vm_page_t	p;

	if (object == NULL)
		return;

	vm_object_lock(object);
	for (p = object->memq.tqh_first; p != NULL; p = p->listq.tqe_next)
		if ((start <= p->offset) && (p->offset < end))
			pmap_page_protect(VM_PAGE_TO_PHYS(p), VM_PROT_NONE);
	vm_object_unlock(object);
}

/*
 * vm_object_copy:
 *
 * Create a new object which is a copy of an existing
 * object, and mark all of the pages in the existing
 * object 'copy-on-write'.  The new object has one reference.
 * Returns the new object.
 *
 * May defer the copy until later if the object is not backed
 * up by a non-default pager.
 */
void
vm_object_copy(src_object, src_offset, size,
		    dst_object, dst_offset, src_needs_copy)
	register vm_object_t	src_object;
	vm_offset_t		src_offset;
	vm_size_t		size;
	vm_object_t		*dst_object;	/* OUT */
	vm_offset_t		*dst_offset;	/* OUT */
	boolean_t		*src_needs_copy;	/* OUT */
{
	register vm_object_t	new_copy;
	register vm_object_t	old_copy;
	vm_offset_t		new_start, new_end;

	register vm_page_t	p;

	if (src_object == NULL) {
		/*
		 * Nothing to copy
		 */
		*dst_object = NULL;
		*dst_offset = 0;
		*src_needs_copy = FALSE;
		return;
	}

	/*
	 * If the object's pager is null_pager or the
	 * default pager, we don't have to make a copy
	 * of it.  Instead, we set the needs copy flag and
	 * make a shadow later.
	 */

	vm_object_lock(src_object);
	if (src_object->pager == NULL ||
	    (src_object->flags & OBJ_INTERNAL)) {

		/*
		 * Make another reference to the object.
		 */
		src_object->ref_count++;

		/*
		 * Mark all of the pages copy-on-write.
		 */
		for (p = src_object->memq.tqh_first; p; p = p->listq.tqe_next)
			if (src_offset <= p->offset &&
			    p->offset < src_offset + size)
				p->flags |= PG_COPYONWRITE;
		vm_object_unlock(src_object);

		*dst_object = src_object;
		*dst_offset = src_offset;
		
		/*
		 * Must make a shadow when write is desired
		 */
		*src_needs_copy = TRUE;
		return;
	}

	/*
	 * Try to collapse the object before copying it.
	 */
	vm_object_collapse(src_object);

	/*
	 * If the object has a pager, the pager wants to
	 * see all of the changes.  We need a copy-object
	 * for the changed pages.
	 *
	 * If there is a copy-object, and it is empty,
	 * no changes have been made to the object since the
	 * copy-object was made.  We can use the same copy-
	 * object.
	 */

Retry1:
	old_copy = src_object->copy;
	if (old_copy != NULL) {
		/*
		 * Try to get the locks (out of order)
		 */
		if (!vm_object_lock_try(old_copy)) {
			vm_object_unlock(src_object);

			/* XXX should spin a bit here... */
			vm_object_lock(src_object);
			goto Retry1;
		}

		if (old_copy->resident_page_count == 0 &&
		    old_copy->pager == NULL) {
			/*
			 * Return another reference to
			 * the existing copy-object.
			 */
			old_copy->ref_count++;
			vm_object_unlock(old_copy);
			vm_object_unlock(src_object);
			*dst_object = old_copy;
			*dst_offset = src_offset;
			*src_needs_copy = FALSE;
			return;
		}
		vm_object_unlock(old_copy);
	}
	vm_object_unlock(src_object);

	/*
	 * If the object has a pager, the pager wants
	 * to see all of the changes.  We must make
	 * a copy-object and put the changed pages there.
	 *
	 * The copy-object is always made large enough to
	 * completely shadow the original object, since
	 * it may have several users who want to shadow
	 * the original object at different points.
	 */

	new_copy = vm_object_allocate(src_object->size);

Retry2:
	vm_object_lock(src_object);
	/*
	 * Copy object may have changed while we were unlocked
	 */
	old_copy = src_object->copy;
	if (old_copy != NULL) {
		/*
		 * Try to get the locks (out of order)
		 */
		if (!vm_object_lock_try(old_copy)) {
			vm_object_unlock(src_object);
			goto Retry2;
		}

		/*
		 * Consistency check
		 */
		if (old_copy->shadow != src_object ||
		    old_copy->shadow_offset != (vm_offset_t) 0)
			panic("vm_object_copy: copy/shadow inconsistency");

		/*
		 * Make the old copy-object shadow the new one.
		 * It will receive no more pages from the original
		 * object.  Locking of new_copy not needed.  We
		 * have the only pointer.
		 */
		vm_object_set_shadow(old_copy, new_copy);
		vm_object_unlock(old_copy);
	}

	/* Always shadow original at 0 for the whole object */
	new_start = (vm_offset_t)0;
	new_end = (vm_offset_t)new_copy->size;

	/*
	 * Point the new copy at the existing object.
	 */

	vm_object_set_shadow(new_copy, src_object);
	new_copy->shadow_offset = new_start;
	src_object->copy = new_copy;

	/*
	 * Mark all the affected pages of the existing object
	 * copy-on-write.
	 */
	for (p = src_object->memq.tqh_first; p != NULL; p = p->listq.tqe_next)
		if ((new_start <= p->offset) && (p->offset < new_end))
			p->flags |= PG_COPYONWRITE;

	vm_object_unlock(src_object);

	*dst_object = new_copy;
	*dst_offset = src_offset - new_start;
	*src_needs_copy = FALSE;
}

/*
 * vm_object_shadow:
 *
 * Create a new object which is backed by the
 * specified existing object range.  The source
 * object reference is deallocated.
 *
 * The new object and offset into that object
 * are returned in the source parameters.
 *
 * The old object should not be locked.
 */
void
vm_object_shadow(object, offset, length)
	vm_object_t	*object;	/* IN/OUT */
	vm_offset_t	*offset;	/* IN/OUT */
	vm_size_t	length;
{
	register vm_object_t	source;
	register vm_object_t	result;

	source = *object;

#ifdef DIAGNOSTIC
	if (source == NULL)
		panic("vm_object_shadow: attempt to shadow null object");
#endif

	/*
	 * Allocate a new object with the given length
	 */
	if ((result = vm_object_allocate(length)) == NULL)
		panic("vm_object_shadow: no object for shadowing");

	/*
	 * The new object shadows the source object.  Our caller changes his
	 * reference to point to the new object, removing a reference to the
	 * source object.
	 */
	vm_object_lock(source);
	vm_object_set_shadow(result, source);
	source->ref_count--;
	vm_object_unlock(source);
	
	/*
	 * Store the offset into the source object,
	 * and fix up the offset into the new object.
	 */
	result->shadow_offset = *offset;

	/*
	 * Return the new things
	 */
	*offset = 0;
	*object = result;
}

/*
 * Set the specified object's pager to the specified pager.
 */
void
vm_object_setpager(object, pager, paging_offset, read_only)
	vm_object_t	object;
	vm_pager_t	pager;
	vm_offset_t	paging_offset;
	boolean_t	read_only;
{
#ifdef	lint
	read_only++;	/* No longer used */
#endif

	vm_object_lock(object);			/* XXX ? */
	object->pager = pager;
	object->paging_offset = paging_offset;
	vm_object_unlock(object);			/* XXX ? */
}

/*
 * vm_object_hash hashes the pager/id pair.
 */

#define vm_object_hash(pager) \
	(((unsigned long)pager)%VM_OBJECT_HASH_COUNT)

/*
 * vm_object_lookup looks in the object cache for an object with the
 * specified pager and paging id.
 */
vm_object_t
vm_object_lookup(pager)
	vm_pager_t	pager;
{
	register vm_object_hash_entry_t	entry;
	vm_object_t			object;

	vm_object_cache_lock();

	for (entry = vm_object_hashtable[vm_object_hash(pager)].tqh_first;
	     entry != NULL;
	     entry = entry->hash_links.tqe_next) {
		object = entry->object;
		if (object->pager == pager) {
			vm_object_lock(object);
			if (object->ref_count == 0) {
				TAILQ_REMOVE(&vm_object_cached_list, object,
					cached_list);
				vm_object_cached--;
			}
			object->ref_count++;
			vm_object_unlock(object);
			vm_object_cache_unlock();
			return(object);
		}
	}

	vm_object_cache_unlock();
	return(NULL);
}

/*
 * vm_object_enter enters the specified object/pager/id into
 * the hash table.
 */

void
vm_object_enter(object, pager)
	vm_object_t	object;
	vm_pager_t	pager;
{
	struct vm_object_hash_head	*bucket;
	register vm_object_hash_entry_t	entry;

	/*
	 * We don't cache null objects, and we can't cache
	 * objects with the null pager.
	 */

	if (object == NULL)
		return;
	if (pager == NULL)
		return;

	bucket = &vm_object_hashtable[vm_object_hash(pager)];
	entry = (vm_object_hash_entry_t)
		malloc((u_long)sizeof *entry, M_VMOBJHASH, M_WAITOK);
	entry->object = object;
	object->flags |= OBJ_CANPERSIST;

	vm_object_cache_lock();
	TAILQ_INSERT_TAIL(bucket, entry, hash_links);
	vm_object_cache_unlock();
}

/*
 * vm_object_remove:
 *
 * Remove the pager from the hash table.
 * Note:  This assumes that the object cache
 * is locked.  XXX this should be fixed
 * by reorganizing vm_object_deallocate.
 */
void
vm_object_remove(pager)
	register vm_pager_t	pager;
{
	struct vm_object_hash_head	*bucket;
	register vm_object_hash_entry_t	entry;
	register vm_object_t		object;

	bucket = &vm_object_hashtable[vm_object_hash(pager)];

	for (entry = bucket->tqh_first;
	     entry != NULL;
	     entry = entry->hash_links.tqe_next) {
		object = entry->object;
		if (object->pager == pager) {
			TAILQ_REMOVE(bucket, entry, hash_links);
			free((caddr_t)entry, M_VMOBJHASH);
			break;
		}
	}
}

/*
 * vm_object_cache_clear removes all objects from the cache.
 */
void
vm_object_cache_clear()
{
	register vm_object_t	object;

	/*
	 * Remove each object in the cache by scanning down the
	 * list of cached objects.
	 */
	vm_object_cache_lock();
	while ((object = vm_object_cached_list.tqh_first) != NULL) {
		vm_object_cache_unlock();

		/* 
		 * Note: it is important that we use vm_object_lookup
		 * to gain a reference, and not vm_object_reference, because
		 * the logic for removing an object from the cache lies in 
		 * lookup.
		 */
		if (object != vm_object_lookup(object->pager))
			panic("vm_object_cache_clear: I'm sooo confused.");
		pager_cache(object, FALSE);

		vm_object_cache_lock();
	}
	vm_object_cache_unlock();
}

/*
 * vm_object_remove_from_pager:
 *
 * Tell object's pager that it needn't back the page
 * anymore.  If the pager ends up empty, deallocate it.
 */
int
vm_object_remove_from_pager(object, from, to)
	vm_object_t	object;
	vm_offset_t	from, to;
{
	vm_pager_t	pager = object->pager;
	int		cnt = 0;

	if (pager == NULL)
		return 0;

	cnt = vm_pager_remove(pager, from, to);

	/* If pager became empty, remove it.  */
	if (cnt > 0 && vm_pager_count(pager) == 0) {
		vm_pager_deallocate(pager);
		object->pager = NULL;
	}
	return(cnt);
}

#define	FREE_PAGE(m)	do {					\
	PAGE_WAKEUP(m);						\
	vm_page_lock_queues();					\
	vm_page_free(m);					\
	vm_page_unlock_queues();				\
} while(0)

/*
 * vm_object_overlay:
 *
 * Internal function to vm_object_collapse called when
 * it has been shown that a collapse operation is likely
 * to succeed.  We know that the backing object is only
 * referenced by me and that paging is not in progress.
 */
int
vm_object_overlay(object)
	vm_object_t	object;
{
	vm_object_t	backing_object = object->shadow;
	vm_offset_t	backing_offset = object->shadow_offset;
	vm_size_t	size = object->size;
	vm_offset_t	offset, paged_offset;
	vm_page_t	backing_page, page = NULL;
	int		rv;

#ifdef DEBUG
	if (vmdebug & VMDEBUG_COLLAPSE)
		printf("vm_object_overlay(0x%p)\n", object);
#endif

	/*
	 * Protect against multiple collapses.
	 */
	backing_object->flags |= OBJ_FADING;

	/*
	 * The algorithm used is roughly like this:
	 * (1)	Trim a potential pager in the backing object so it'll only hold
	 *      pages in reach.
	 * (2)	Loop over all the resident pages in the shadow object and
	 *      either remove them if they are shadowed or move them into the
	 * 	shadowing object.
	 * (3)	Loop over the paged out pages in the shadow object.  Start
	 *      pageins on those that aren't shadowed, and just deallocate
	 * 	the others.  In each iteration check if other users of these
	 *      objects have caused pageins resulting in new resident pages.
	 * 	This can happen while we are waiting for a page or a pagein of
	 *      ours.  If such resident pages turn up, restart from (2).
	 */

	/*
	 * As a first measure we know we can discard everything that the
	 * shadowing object doesn't shadow.
	 */
	if (backing_object->pager != NULL) {
		if (backing_offset > 0)
			vm_object_remove_from_pager(backing_object, 0,
			    backing_offset);
		if (backing_offset + size < backing_object->size)
			vm_object_remove_from_pager(backing_object,
			    backing_offset + size, backing_object->size);
	}

	/*
	 * At this point, there may still be asynchronous paging in the parent
	 * object.  Any pages being paged in will be represented by fake pages.
	 * There are three cases:
	 * 1) The page is being paged in from the parent object's own pager.
	 *    In this case, we just delete our copy, since it's not needed.
	 * 2) The page is being paged in from the backing object.  We prevent
	 *    this case by waiting for paging to complete on the backing object
	 *    before continuing.
	 * 3) The page is being paged in from a backing object behind the one
	 *    we're deleting.  We'll never notice this case, because the
	 *    backing object we're deleting won't have the page.
	 */

	vm_object_unlock(object);
retry:
	vm_object_paging_wait(backing_object,"vpagew");

	/*
	 * While we were asleep, the parent object might have been deleted.  If
	 * so, the backing object will now have only one reference (the one we
	 * hold).  If this happened, just deallocate the backing object and
	 * return failure status so vm_object_collapse() will stop.  This will
	 * continue vm_object_deallocate() where it stopped due to our
	 * reference.
	 */
	if (backing_object->ref_count == 1)
		goto fail;
	vm_object_lock(object);

	/*
	 * Next, get rid of resident pages in the backing object.  We can
	 * guarantee to remove every page thus we can write the while-test like
	 * this.
	 */
	while ((backing_page = backing_object->memq.tqh_first) != NULL) {
		offset = backing_page->offset - backing_offset;

#ifdef DIAGNOSTIC
		if (backing_page->flags & (PG_BUSY | PG_FAKE))
			panic("vm_object_overlay: "
			    "busy or fake page in backing_object");
#endif

		/*
		 * If the page is outside the shadowing object's range or if
		 * the page is shadowed (either by a resident page or a paged
		 * out one) we can discard it right away.  Otherwise we need to
		 * move the page to the shadowing object.
		 */
		if (backing_page->offset < backing_offset || offset >= size ||
		    ((page = vm_page_lookup(object, offset)) != NULL) ||
		    (object->pager != NULL &&
		     vm_pager_has_page(object->pager, offset))) {
			/*
			 * Just discard the page, noone needs it.  This
			 * includes removing the possible backing store too.
			 */
			if (backing_object->pager != NULL)
				vm_object_remove_from_pager(backing_object,
				    backing_page->offset,
				    backing_page->offset + PAGE_SIZE);
			vm_page_lock_queues();
			vm_page_free(backing_page);
			vm_page_unlock_queues();
		} else {
			/*
			 * If the backing page was ever paged out, it was due
			 * to it being dirty at one point.  Unless we have no
			 * pager allocated to the front object (thus will move
			 * forward the shadow's one), mark it dirty again so it
			 * won't be thrown away without being paged out to the
			 * front pager.
			 *
			 * XXX
			 * Should be able to move a page from one pager to
			 * another.
			 */
			if (object->pager != NULL &&
			    vm_object_remove_from_pager(backing_object,
			    backing_page->offset,
			    backing_page->offset + PAGE_SIZE))
				backing_page->flags &= ~PG_CLEAN;

			/* Move the page up front.  */
			vm_page_rename(backing_page, object, offset);
		}
	}

	/*
	 * If the shadowing object doesn't have a pager the easiest
	 * thing to do now is to just move the backing pager up front
	 * and everything is done.  
	 */
	if (object->pager == NULL && backing_object->pager != NULL) {
		object->pager = backing_object->pager;
		object->paging_offset = backing_object->paging_offset +
		    backing_offset;
		backing_object->pager = NULL;
		goto done;
	}

	/*
	 * What's left to do is to find all paged out pages in the
	 * backing pager and either discard or move it to the front
	 * object.  We need to recheck the resident page set as a
	 * pagein might have given other threads the chance to, via
	 * readfaults, page in another page into the resident set.  In
	 * this case we need to retry getting rid of pages from core.
	 */
	paged_offset = 0;
	while (backing_object->pager != NULL &&
	    (paged_offset = vm_pager_next(backing_object->pager,
	    paged_offset)) < backing_object->size) {
		offset = paged_offset - backing_offset;

		/*
		 * If the parent object already has this page, delete it.
		 * Otherwise, start a pagein.
		 */
		if (((page = vm_page_lookup(object, offset)) == NULL) &&
		    (object->pager == NULL ||
		     !vm_pager_has_page(object->pager, offset))) {
			vm_object_unlock(object);

			/*
			 * First allocate a page and mark it busy so another
			 * thread won't try to start another pagein.
			 */
			backing_page = vm_page_alloc(backing_object,
			    paged_offset);
			if (backing_page == NULL) {
				vm_object_unlock(backing_object);
				vm_wait("fVmcollapse");
				vm_object_lock(backing_object);
				goto retry;
			}
			backing_page->flags |= PG_BUSY;

#ifdef DEBUG
			if (vmdebug & VMDEBUG_COLLAPSE_PAGEIN)
				printf("vm_object_overlay: pagein needed\n");
#endif

			/*
			 * Second, start paging it in.  If this fails,
			 * what can we do but punt?
			 */
			vm_object_paging_begin(backing_object);
			vm_object_unlock(backing_object);
			cnt.v_pageins++;
			rv = vm_pager_get_pages(backing_object->pager,
			    &backing_page, 1, TRUE);
			vm_object_lock(backing_object);
			vm_object_paging_end(backing_object);

			/*
			 * IO error or page outside the range of the pager:
			 * cleanup and return an error.
			 */
			if (rv == VM_PAGER_ERROR || rv == VM_PAGER_BAD) {
				FREE_PAGE(backing_page);
				goto fail;
			}

			/* Handle the remaining failures.  */
			if (rv != VM_PAGER_OK) {
#ifdef DIAGNOSTIC
				panic("vm_object_overlay: pager returned %d",
				    rv);
#else
				FREE_PAGE(backing_page);
				goto fail;
#endif
			}
			cnt.v_pgpgin++;

			/*
			 * Third, relookup in case pager changed page.  Pager
			 * is responsible for disposition of old page if moved.
			 */
			backing_page = vm_page_lookup(backing_object,
			    paged_offset);

			/*
			 * This page was once dirty, otherwise it
			 * hadn't been paged out in this shadow object.
			 * As we now remove the persistant store of the
			 * page, make sure it will be paged out in the
			 * front pager by dirtying it.
			 */
			backing_page->flags &= ~(PG_FAKE | PG_CLEAN);

			/*
			 * Fourth, restart the process as we have slept,
			 * thereby letting other threads change object's
			 * internal structure.  Don't be tempted to move it up
			 * front here, the parent may be gone already.
			 */
			PAGE_WAKEUP(backing_page);
			goto retry;
		}
		vm_object_remove_from_pager(backing_object, paged_offset, 
		    paged_offset + PAGE_SIZE);
		paged_offset += PAGE_SIZE;
	}

done:
	/*
	 * I've seen this condition once in an out of VM situation.  For the
	 * moment I don't know why it occurred, although I suspect
	 * vm_object_page_clean can create a pager even if it won't use it.
	 */
	if (backing_object->pager != NULL &&
	    vm_pager_count(backing_object->pager) == 0) {
		vm_pager_deallocate(backing_object->pager);
		backing_object->pager = NULL;
	}

#ifdef DIAGNOSTIC
	if (backing_object->pager)
		panic("vm_object_overlay: backing_object->pager remains");
#endif

	/*
	 * Object now shadows whatever backing_object did.
	 */
	if (backing_object->shadow)
		vm_object_lock(backing_object->shadow);
	vm_object_set_shadow(object, backing_object->shadow);
	if (backing_object->shadow)
		vm_object_unlock(backing_object->shadow);
	object->shadow_offset += backing_object->shadow_offset;
	if (object->shadow != NULL && object->shadow->copy != NULL)
		panic("vm_object_overlay: we collapsed a copy-object!");

#ifdef DIAGNOSTIC
	if (backing_object->ref_count != 1)
		panic("vm_object_overlay: backing_object still referenced");
#endif

	object_collapses++;
	return KERN_SUCCESS;

fail:
	backing_object->flags &= ~OBJ_FADING;
	return KERN_FAILURE;
}

/*
 * vm_object_bypass:
 *
 * Internal function to vm_object_collapse called when collapsing
 * the object with its backing one is not allowed but there may
 * be an opportunity to bypass the backing object and shadow the
 * next object in the chain instead.
 *
 * If all of the pages in the backing object are shadowed by the parent
 * object, the parent object no longer has to shadow the backing
 * object; it can shadow the next one in the chain.
 */
int
vm_object_bypass(object)
	vm_object_t	object;
{
	vm_object_t	backing_object = object->shadow;
	vm_offset_t	backing_offset = object->shadow_offset;
	vm_offset_t	offset, new_offset;
	vm_page_t	p, pp;

	/*
	 * XXX Punt if paging is going on.  The issues in this case need to be
	 * looked into more closely.  For now play it safe and return.  There's
	 * no need to wait for it to end, as the expense will be much higher
	 * than the gain.
	 */
	if (vm_object_paging(backing_object))
		return KERN_FAILURE;

	/*
	 * Should have a check for a 'small' number of pages here.
	 */
	for (p = backing_object->memq.tqh_first; p != NULL;
	    p = p->listq.tqe_next) {
		new_offset = p->offset - backing_offset;

		/*
		 * If the parent has a page here, or if this page falls outside
		 * the parent, keep going.
		 *
		 * Otherwise, the backing_object must be left in the chain.
		 */
		if (p->offset >= backing_offset && new_offset < object->size &&
		    ((pp = vm_page_lookup(object, new_offset)) == NULL ||
		    (pp->flags & PG_FAKE)) &&
		    (object->pager == NULL ||
		    !vm_pager_has_page(object->pager, new_offset)))
			/*
			 * Page still needed.  Can't go any further.
			 */
			return KERN_FAILURE;
	}

	if (backing_object->pager) {
		/*
		 * Should have a check for a 'small' number of pages here.
		 */
		for (offset = vm_pager_next(backing_object->pager, 0);
		    offset < backing_object->size;
		    offset = vm_pager_next(backing_object->pager,
		    offset + PAGE_SIZE)) {
			new_offset = offset - backing_offset;

			/*
			 * If the parent has a page here, or if this page falls
			 * outside the parent, keep going.
			 *
			 * Otherwise, the backing_object must be left in the
			 * chain.
			 */
			if (offset >= backing_offset &&
			    new_offset < object->size &&
			    ((pp = vm_page_lookup(object, new_offset)) ==
			    NULL || (pp->flags & PG_FAKE)) &&
			    (object->pager == NULL ||
			    !vm_pager_has_page(object->pager, new_offset)))
				/*
				 * Page still needed.  Can't go any further.
				 */
				return KERN_FAILURE;
		}
	}

	/*
	 * Object now shadows whatever backing_object did.
	 */
	if (backing_object->shadow)
		vm_object_lock(backing_object->shadow);
	vm_object_set_shadow(object, backing_object->shadow);
	if (backing_object->shadow)
		vm_object_unlock(backing_object->shadow);
	object->shadow_offset += backing_object->shadow_offset;

	/*
	 * Backing object might have had a copy pointer to us.  If it did,
	 * clear it. 
	 */
	if (backing_object->copy == object)
		backing_object->copy = NULL;

	object_bypasses++;
	return KERN_SUCCESS;
}

/*
 * vm_object_collapse:
 *
 * Collapse an object with the object backing it.  Pages in the backing object
 * are moved into the parent, and the backing object is deallocated.
 *
 * Requires that the object be locked and the page queues be unlocked.
 */
void
vm_object_collapse(object)
	vm_object_t object;

{
	vm_object_collapse_internal(object, NULL);
}

/*
 * An internal to vm_object.c entry point to the collapsing logic, used by
 * vm_object_deallocate to get rid of a potential recursion case.  In that case
 * an object to be deallocated is fed back via the retry_object pointer.
 * External users will have that parameter wired to NULL, and then we are
 * allowed to do vm_object_deallocate calls that may mutually recursive call us
 * again.  In that case it will only get one level deep and thus not be a real
 * recursion.
 */
void
vm_object_collapse_internal(object, retry_object)
	vm_object_t	object, *retry_object;
{
	register vm_object_t	backing_object;
	int			rv;

	/* We'd better initialize this one if the pointer is given.  */
	if (retry_object)
		*retry_object = NULL;

	if (!vm_object_collapse_allowed || object == NULL)
		return;

	do {
		/*
		 * Verify that the conditions are right for collapse:
		 *
		 * There is a backing object, and
		 */
		if ((backing_object = object->shadow) == NULL)
			return;

		vm_object_lock(backing_object);

		/*
		 * ... the backing object is not read_only, is internal and is
		 * not already being collapsed, ...
		 */
		if ((backing_object->flags & (OBJ_INTERNAL | OBJ_FADING)) !=
		    OBJ_INTERNAL) {
			vm_object_unlock(backing_object);
			return;
		}
	
		/*
		 * The backing object can't be a copy-object: the shadow_offset
		 * for the copy-object must stay as 0.  Furthermore (for the
		 * we have all the pages' case), if we bypass backing_object
		 * and just shadow the next object in the chain, old pages from
		 * that object would then have to be copied BOTH into the
		 *(former) backing_object and into the parent object.
		 */
		if (backing_object->shadow != NULL &&
		    backing_object->shadow->copy != NULL) {
			vm_object_unlock(backing_object);
			return;
		}

		/*
		 * Grab a reference to the backing object so that it
		 * can't be deallocated behind our back.
		 */
		backing_object->ref_count++;

#ifdef DIAGNOSTIC
		if (backing_object->ref_count == 1)
			panic("vm_object_collapse: "
			    "collapsing unreferenced object");
#endif

		/*
		 * If there is exactly one reference to the backing object, we
		 * can collapse it into the parent, otherwise we might be able
		 * to bypass it completely.
		 */
		rv = backing_object->ref_count == 2 ?
		    vm_object_overlay(object) : vm_object_bypass(object);

		/*
		 * Unlock and note we're ready with the backing object.  If
		 * we are now the last referrer this will also deallocate the
		 * object itself.  If the backing object has been orphaned
		 * and still have a shadow (it is possible in case of
		 * KERN_FAILURE from vm_object_overlay) this might lead to a
		 * recursion.  However, if we are called from
		 * vm_object_deallocate, retry_object is not NULL and we are
		 * allowed to feedback the current backing object via that
		 * pointer.  That way the recursion case turns into an
		 * iteration in vm_object_deallcate instead.
		 */
		if (retry_object != NULL && backing_object->ref_count == 1 &&
		    backing_object->shadow != NULL) {
			*retry_object = backing_object;
			vm_object_unlock(backing_object);
			return;
		}
		vm_object_unlock(backing_object);
		vm_object_deallocate(backing_object);

		/*
		 * Try again with this object's new backing object.
		 */
	} while (rv == KERN_SUCCESS);
}

/*
 * vm_object_page_remove: [internal]
 *
 * Removes all physical pages in the specified
 * object range from the object's list of pages.
 *
 * The object must be locked.
 */
void
vm_object_page_remove(object, start, end)
	register vm_object_t	object;
	register vm_offset_t	start;
	register vm_offset_t	end;
{
	register vm_page_t	p, next;

	if (object == NULL)
		return;

	for (p = object->memq.tqh_first; p != NULL; p = next) {
		next = p->listq.tqe_next;
		if ((start <= p->offset) && (p->offset < end)) {
			pmap_page_protect(VM_PAGE_TO_PHYS(p), VM_PROT_NONE);
			vm_page_lock_queues();
			vm_page_free(p);
			vm_page_unlock_queues();
		}
	}
}

/*
 * Routine:	vm_object_coalesce
 * Function:	Coalesces two objects backing up adjoining
 *		regions of memory into a single object.
 *
 * returns TRUE if objects were combined.
 *
 * NOTE: Only works at the moment if the second object is NULL -
 *	 if it's not, which object do we lock first?
 *
 * Parameters:
 *	prev_object	First object to coalesce
 *	prev_offset	Offset into prev_object
 *	next_object	Second object into coalesce
 *	next_offset	Offset into next_object
 *
 *	prev_size	Size of reference to prev_object
 *	next_size	Size of reference to next_object
 *
 * Conditions:
 * The object must *not* be locked.
 */
boolean_t
vm_object_coalesce(prev_object, next_object, prev_offset, next_offset,
		   prev_size, next_size)
	register vm_object_t	prev_object;
	vm_object_t	next_object;
	vm_offset_t	prev_offset, next_offset;
	vm_size_t	prev_size, next_size;
{
	vm_size_t	newsize;

#ifdef	lint
	next_offset++;
#endif

	if (next_object != NULL) {
		return(FALSE);
	}

	if (prev_object == NULL) {
		return(TRUE);
	}

	vm_object_lock(prev_object);

	/*
	 * Try to collapse the object first
	 */
	vm_object_collapse(prev_object);

	/*
	 * Can't coalesce if:
	 * . more than one reference
	 * . paged out
	 * . shadows another object
	 * . has a copy elsewhere
	 * (any of which mean that the pages not mapped to
	 * prev_entry may be in use anyway)
	 */

	if (prev_object->ref_count > 1 ||  prev_object->pager != NULL ||
	    prev_object->shadow != NULL || prev_object->copy != NULL) {
		vm_object_unlock(prev_object);
		return(FALSE);
	}

	/*
	 * Remove any pages that may still be in the object from
	 * a previous deallocation.
	 */
	vm_object_page_remove(prev_object, prev_offset + prev_size,
	    prev_offset + prev_size + next_size);

	/*
	 * Extend the object if necessary.
	 */
	newsize = prev_offset + prev_size + next_size;
	if (newsize > prev_object->size)
		prev_object->size = newsize;

	vm_object_unlock(prev_object);
	return(TRUE);
}

/*
 * vm_object_print:	[ debug ]
 */
void
vm_object_print(object, full)
	vm_object_t	object;
	boolean_t	full;
{
	_vm_object_print(object, full, printf);
}

void
_vm_object_print(object, full, pr)
	vm_object_t	object;
	boolean_t	full;
	int		(*pr) __P((const char *, ...));
{
	register vm_page_t	p;
	char			*delim;
	vm_object_t		o;
	register int		count;
	extern int		indent;

	if (object == NULL)
		return;

	iprintf(pr, "Object 0x%p: size=0x%lx, res=%d, ref=%d, ", object,
	    (long)object->size, object->resident_page_count,
	    object->ref_count);
	(*pr)("pager=%p+0x%lx, shadow=(%p)+0x%lx\n", object->pager,
	    (long)object->paging_offset, object->shadow,
	    (long)object->shadow_offset);
	(*pr)("shadowers=(");
	delim = "";
	for (o = object->shadowers.lh_first; o;
	    o = o->shadowers_list.le_next) {
		(*pr)("%s0x%p", delim, o);
		delim = ", ";
	};
	(*pr)(")\n");
	(*pr)("cache: next=0x%p, prev=0x%p\n", object->cached_list.tqe_next,
	    object->cached_list.tqe_prev);

	if (!full)
		return;

	indent += 2;
	count = 0;
	for (p = object->memq.tqh_first; p != NULL; p = p->listq.tqe_next) {
		if (count == 0)
			iprintf(pr, "memory:=");
		else if (count == 6) {
			(*pr)("\n");
			iprintf(pr, " ...");
			count = 0;
		} else
			(*pr)(",");
		count++;

		(*pr)("(off=0x%lx,page=0x%lx)", (long)p->offset,
		    (long)VM_PAGE_TO_PHYS(p));
	}
	if (count != 0)
		(*pr)("\n");
	indent -= 2;
}

/*
 * vm_object_set_shadow:
 *
 * Maintain the shadow graph so that back-link consistency is always kept.
 *
 * Assumes both objects as well as the old shadow to be locked (unless NULL
 * of course).
 */
void
vm_object_set_shadow(object, shadow)
	vm_object_t	object, shadow;
{
	vm_object_t	old_shadow = object->shadow;

#ifdef DEBUG
	if (vmdebug & VMDEBUG_SHADOW)
		printf("vm_object_set_shadow(object=0x%p, shadow=0x%p) "
		    "old_shadow=0x%p\n", object, shadow, old_shadow);
	if (vmdebug & VMDEBUG_SHADOW_VERBOSE) {
		vm_object_print(object, 0);
		vm_object_print(old_shadow, 0);
		vm_object_print(shadow, 0);
	}
#endif
	if (old_shadow == shadow)
		return;
	if (old_shadow) {
		old_shadow->ref_count--;
		LIST_REMOVE(object, shadowers_list);
	}
	if (shadow) {
		shadow->ref_count++;
		LIST_INSERT_HEAD(&shadow->shadowers, object, shadowers_list);
	}
	object->shadow = shadow;
#ifdef DEBUG
	if (vmdebug & VMDEBUG_SHADOW_VERBOSE) {
		vm_object_print(object, 0);
		vm_object_print(old_shadow, 0);
		vm_object_print(shadow, 0);
	}
#endif
}
