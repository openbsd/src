/*	$OpenBSD: uvm_map.h,v 1.34 2005/11/19 02:18:02 pedro Exp $	*/
/*	$NetBSD: uvm_map.h,v 1.24 2001/02/18 21:19:08 chs Exp $	*/

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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Charles D. Cranor,
 *      Washington University, the University of California, Berkeley and 
 *      its contributors.
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
 *	@(#)vm_map.h    8.3 (Berkeley) 3/15/94
 * from: Id: uvm_map.h,v 1.1.2.3 1998/02/07 01:16:55 chs Exp
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

#ifndef _UVM_UVM_MAP_H_
#define _UVM_UVM_MAP_H_

/*
 * uvm_map.h
 */

#ifdef _KERNEL

/*
 * macros
 */

/*
 * UVM_MAP_CLIP_START: ensure that the entry begins at or after
 * the starting address, if it doesn't we split the entry.
 * 
 * => map must be locked by caller
 */

#define UVM_MAP_CLIP_START(MAP,ENTRY,VA) { \
	if ((VA) > (ENTRY)->start) uvm_map_clip_start(MAP,ENTRY,VA); }

/*
 * UVM_MAP_CLIP_END: ensure that the entry ends at or before
 *      the ending address, if it does't we split the entry.
 *
 * => map must be locked by caller
 */

#define UVM_MAP_CLIP_END(MAP,ENTRY,VA) { \
	if ((VA) < (ENTRY)->end) uvm_map_clip_end(MAP,ENTRY,VA); }

/*
 * extract flags
 */
#define UVM_EXTRACT_REMOVE	0x1	/* remove mapping from old map */
#define UVM_EXTRACT_CONTIG	0x2	/* try to keep it contig */
#define UVM_EXTRACT_QREF	0x4	/* use quick refs */
#define UVM_EXTRACT_FIXPROT	0x8	/* set prot to maxprot as we go */

#endif /* _KERNEL */

#include <uvm/uvm_anon.h>

/*
 * types defined:
 *
 *	vm_map_t		the high-level address map data structure.
 *	vm_map_entry_t		an entry in an address map.
 *	vm_map_version_t	a timestamp of a map, for use with vm_map_lookup
 */

/*
 * Objects which live in maps may be either VM objects, or another map
 * (called a "sharing map") which denotes read-write sharing with other maps.
 *
 * XXXCDC: private pager data goes here now
 */

union vm_map_object {
	struct uvm_object	*uvm_obj;	/* UVM OBJECT */
	struct vm_map		*sub_map;	/* belongs to another map */
};

/*
 * Address map entries consist of start and end addresses,
 * a VM object (or sharing map) and offset into that object,
 * and user-exported inheritance and protection information.
 * Also included is control information for virtual copy operations.
 */
struct vm_map_entry {
	RB_ENTRY(vm_map_entry)	rb_entry;	/* tree information */
	vaddr_t			ownspace;	/* free space after */
	vaddr_t			space;		/* space in subtree */
	struct vm_map_entry	*prev;		/* previous entry */
	struct vm_map_entry	*next;		/* next entry */
	vaddr_t			start;		/* start address */
	vaddr_t			end;		/* end address */
	union vm_map_object	object;		/* object I point to */
	voff_t			offset;		/* offset into object */
	int			etype;		/* entry type */
	vm_prot_t		protection;	/* protection code */
	vm_prot_t		max_protection;	/* maximum protection */
	vm_inherit_t		inheritance;	/* inheritance */
	int			wired_count;	/* can be paged if == 0 */
	struct vm_aref		aref;		/* anonymous overlay */
	int			advice;		/* madvise advice */
#define uvm_map_entry_stop_copy flags
	u_int8_t		flags;		/* flags */

#define UVM_MAP_STATIC		0x01		/* static map entry */
#define UVM_MAP_KMEM		0x02		/* from kmem entry pool */

};

#define	VM_MAPENT_ISWIRED(entry)	((entry)->wired_count != 0)

/*
 *	Maps are doubly-linked lists of map entries, kept sorted
 *	by address.  A single hint is provided to start
 *	searches again from the last successful search,
 *	insertion, or removal.
 *
 *	LOCKING PROTOCOL NOTES:
 *	-----------------------
 *
 *	VM map locking is a little complicated.  There are both shared
 *	and exclusive locks on maps.  However, it is sometimes required
 *	to downgrade an exclusive lock to a shared lock, and upgrade to
 *	an exclusive lock again (to perform error recovery).  However,
 *	another thread *must not* queue itself to receive an exclusive
 *	lock while before we upgrade back to exclusive, otherwise the
 *	error recovery becomes extremely difficult, if not impossible.
 *
 *	In order to prevent this scenario, we introduce the notion of
 *	a `busy' map.  A `busy' map is read-locked, but other threads
 *	attempting to write-lock wait for this flag to clear before
 *	entering the lock manager.  A map may only be marked busy
 *	when the map is write-locked (and then the map must be downgraded
 *	to read-locked), and may only be marked unbusy by the thread
 *	which marked it busy (holding *either* a read-lock or a
 *	write-lock, the latter being gained by an upgrade).
 *
 *	Access to the map `flags' member is controlled by the `flags_lock'
 *	simple lock.  Note that some flags are static (set once at map
 *	creation time, and never changed), and thus require no locking
 *	to check those flags.  All flags which are r/w must be set or
 *	cleared while the `flags_lock' is asserted.  Additional locking
 *	requirements are:
 *
 *		VM_MAP_PAGEABLE		r/o static flag; no locking required
 *
 *		VM_MAP_INTRSAFE		r/o static flag; no locking required
 *
 *		VM_MAP_WIREFUTURE	r/w; may only be set or cleared when
 *					map is write-locked.  may be tested
 *					without asserting `flags_lock'.
 *
 *		VM_MAP_BUSY		r/w; may only be set when map is
 *					write-locked, may only be cleared by
 *					thread which set it, map read-locked
 *					or write-locked.  must be tested
 *					while `flags_lock' is asserted.
 *
 *		VM_MAP_WANTLOCK		r/w; may only be set when the map
 *					is busy, and thread is attempting
 *					to write-lock.  must be tested
 *					while `flags_lock' is asserted.
 */
struct vm_map {
	struct pmap *		pmap;		/* Physical map */
	lock_data_t		lock;		/* Lock for map data */
	RB_HEAD(uvm_tree, vm_map_entry) rbhead;	/* Tree for entries */
	struct vm_map_entry	header;		/* List of entries */
	int			nentries;	/* Number of entries */
	vsize_t			size;		/* virtual size */
	int			ref_count;	/* Reference count */
	simple_lock_data_t	ref_lock;	/* Lock for ref_count field */
	vm_map_entry_t		hint;		/* hint for quick lookups */
	simple_lock_data_t	hint_lock;	/* lock for hint storage */
	vm_map_entry_t		first_free;	/* First free space hint */
	int			flags;		/* flags */
	simple_lock_data_t	flags_lock;	/* Lock for flags field */
	unsigned int		timestamp;	/* Version number */
#define	min_offset		header.start
#define max_offset		header.end
};

/* vm_map flags */
#define	VM_MAP_PAGEABLE		0x01		/* ro: entries are pageable */
#define	VM_MAP_INTRSAFE		0x02		/* ro: interrupt safe map */
#define	VM_MAP_WIREFUTURE	0x04		/* rw: wire future mappings */
#define	VM_MAP_BUSY		0x08		/* rw: map is busy */
#define	VM_MAP_WANTLOCK		0x10		/* rw: want to write-lock */

/* XXX: number of kernel maps and entries to statically allocate */

#if !defined(MAX_KMAPENT)
#if (50 + (2 * NPROC) > 1000)
#define MAX_KMAPENT (50 + (2 * NPROC))
#else
#define	MAX_KMAPENT	1000  /* XXXCDC: no crash */
#endif
#endif	/* !defined MAX_KMAPENT */

#ifdef _KERNEL
#define	vm_map_modflags(map, set, clear)				\
do {									\
	simple_lock(&(map)->flags_lock);				\
	(map)->flags = ((map)->flags | (set)) & ~(clear);		\
	simple_unlock(&(map)->flags_lock);				\
} while (0)
#endif /* _KERNEL */

/*
 *	Interrupt-safe maps must also be kept on a special list,
 *	to assist uvm_fault() in avoiding locking problems.
 */
struct vm_map_intrsafe {
	struct vm_map	vmi_map;
	LIST_ENTRY(vm_map_intrsafe) vmi_list;
};

LIST_HEAD(vmi_list, vm_map_intrsafe);
#ifdef _KERNEL
extern simple_lock_data_t vmi_list_slock;
extern struct vmi_list vmi_list;

static __inline int vmi_list_lock(void);
static __inline void vmi_list_unlock(int);

static __inline int
vmi_list_lock()
{
	int s;

	s = splhigh();
	simple_lock(&vmi_list_slock);
	return (s);
}

static __inline void
vmi_list_unlock(s)
	int s;
{

	simple_unlock(&vmi_list_slock);
	splx(s);
}
#endif /* _KERNEL */

/*
 * handle inline options
 */

#ifdef UVM_MAP_INLINE
#define MAP_INLINE static __inline
#else 
#define MAP_INLINE /* nothing */
#endif /* UVM_MAP_INLINE */

/*
 * globals:
 */

#ifdef _KERNEL

#ifdef PMAP_GROWKERNEL
extern vaddr_t	uvm_maxkaddr;
#endif

/*
 * protos: the following prototypes define the interface to vm_map
 */

MAP_INLINE
void		uvm_map_deallocate(vm_map_t);

int		uvm_map_clean(vm_map_t, vaddr_t, vaddr_t, int);
void		uvm_map_clip_start(vm_map_t, vm_map_entry_t, vaddr_t);
void		uvm_map_clip_end(vm_map_t, vm_map_entry_t, vaddr_t);
MAP_INLINE
vm_map_t	uvm_map_create(pmap_t, vaddr_t, vaddr_t, int);
int		uvm_map_extract(vm_map_t, vaddr_t, vsize_t, 
			vm_map_t, vaddr_t *, int);
vm_map_entry_t	uvm_map_findspace(vm_map_t, vaddr_t, vsize_t, vaddr_t *,
			struct uvm_object *, voff_t, vsize_t, int);
vaddr_t		uvm_map_hint(struct proc *, vm_prot_t);
int		uvm_map_inherit(vm_map_t, vaddr_t, vaddr_t, vm_inherit_t);
int		uvm_map_advice(vm_map_t, vaddr_t, vaddr_t, int);
void		uvm_map_init(void);
boolean_t	uvm_map_lookup_entry(vm_map_t, vaddr_t, vm_map_entry_t *);
MAP_INLINE
void		uvm_map_reference(vm_map_t);
int		uvm_map_replace(vm_map_t, vaddr_t, vaddr_t,
			vm_map_entry_t, int);
int		uvm_map_reserve(vm_map_t, vsize_t, vaddr_t, vsize_t,
			vaddr_t *);
void		uvm_map_setup(vm_map_t, vaddr_t, vaddr_t, int);
int		uvm_map_submap(vm_map_t, vaddr_t, vaddr_t, vm_map_t);
#define		uvm_unmap(_m, _s, _e) uvm_unmap_p(_m, _s, _e, 0)
MAP_INLINE
void		uvm_unmap_p(vm_map_t, vaddr_t, vaddr_t, struct proc *);
void		uvm_unmap_detach(vm_map_entry_t,int);
void		uvm_unmap_remove(vm_map_t, vaddr_t, vaddr_t,
				      vm_map_entry_t *, struct proc *);

#endif /* _KERNEL */

/*
 * VM map locking operations:
 *
 *	These operations perform locking on the data portion of the
 *	map.
 *
 *	vm_map_lock_try: try to lock a map, failing if it is already locked.
 *
 *	vm_map_lock: acquire an exclusive (write) lock on a map.
 *
 *	vm_map_lock_read: acquire a shared (read) lock on a map.
 *
 *	vm_map_unlock: release an exclusive lock on a map.
 *
 *	vm_map_unlock_read: release a shared lock on a map.
 *
 *	vm_map_downgrade: downgrade an exclusive lock to a shared lock.
 *
 *	vm_map_upgrade: upgrade a shared lock to an exclusive lock.
 *
 *	vm_map_busy: mark a map as busy.
 *
 *	vm_map_unbusy: clear busy status on a map.
 *
 * Note that "intrsafe" maps use only exclusive, spin locks.  We simply
 * use the sleep lock's interlock for this.
 */

#ifdef _KERNEL
/* XXX: clean up later */
#include <sys/time.h>
#include <sys/proc.h>	/* for tsleep(), wakeup() */
#include <sys/systm.h>	/* for panic() */

static __inline boolean_t vm_map_lock_try(vm_map_t);
static __inline void vm_map_lock(vm_map_t);
extern const char vmmapbsy[];

static __inline boolean_t
vm_map_lock_try(map)
	vm_map_t map;
{
	boolean_t rv;

	if (map->flags & VM_MAP_INTRSAFE)
		rv = simple_lock_try(&map->lock.lk_interlock);
	else {
		simple_lock(&map->flags_lock);
		if (map->flags & VM_MAP_BUSY) {
			simple_unlock(&map->flags_lock);
			return (FALSE);
		}
		rv = (lockmgr(&map->lock, LK_EXCLUSIVE|LK_NOWAIT|LK_INTERLOCK,
		    &map->flags_lock) == 0);
	}

	if (rv)
		map->timestamp++;

	return (rv);
}

static __inline void
vm_map_lock(map)
	vm_map_t map;
{
	int error;

	if (map->flags & VM_MAP_INTRSAFE) {
		simple_lock(&map->lock.lk_interlock);
		return;
	}

 try_again:
	simple_lock(&map->flags_lock);
	while (map->flags & VM_MAP_BUSY) {
		map->flags |= VM_MAP_WANTLOCK;
		ltsleep(&map->flags, PVM, (char *)vmmapbsy, 0, &map->flags_lock);
	}

	error = lockmgr(&map->lock, LK_EXCLUSIVE|LK_SLEEPFAIL|LK_INTERLOCK,
	    &map->flags_lock);

	if (error) {
		goto try_again;
	}

	(map)->timestamp++;
}

#ifdef DIAGNOSTIC
#define	vm_map_lock_read(map)						\
do {									\
	if (map->flags & VM_MAP_INTRSAFE)				\
		panic("vm_map_lock_read: intrsafe map");		\
	(void) lockmgr(&(map)->lock, LK_SHARED, NULL);			\
} while (0)
#else
#define	vm_map_lock_read(map)						\
	(void) lockmgr(&(map)->lock, LK_SHARED, NULL)
#endif

#define	vm_map_unlock(map)						\
do {									\
	if ((map)->flags & VM_MAP_INTRSAFE)				\
		simple_unlock(&(map)->lock.lk_interlock);		\
	else								\
		(void) lockmgr(&(map)->lock, LK_RELEASE, NULL);		\
} while (0)

#define	vm_map_unlock_read(map)						\
	(void) lockmgr(&(map)->lock, LK_RELEASE, NULL)

#define	vm_map_downgrade(map)						\
	(void) lockmgr(&(map)->lock, LK_DOWNGRADE, NULL)

#ifdef DIAGNOSTIC
#define	vm_map_upgrade(map)						\
do {									\
	if (lockmgr(&(map)->lock, LK_UPGRADE, NULL) != 0)		\
		panic("vm_map_upgrade: failed to upgrade lock");	\
} while (0)
#else
#define	vm_map_upgrade(map)						\
	(void) lockmgr(&(map)->lock, LK_UPGRADE, NULL)
#endif

#define	vm_map_busy(map)						\
do {									\
	simple_lock(&(map)->flags_lock);				\
	(map)->flags |= VM_MAP_BUSY;					\
	simple_unlock(&(map)->flags_lock);				\
} while (0)

#define	vm_map_unbusy(map)						\
do {									\
	int oflags;							\
									\
	simple_lock(&(map)->flags_lock);				\
	oflags = (map)->flags;						\
	(map)->flags &= ~(VM_MAP_BUSY|VM_MAP_WANTLOCK);			\
	simple_unlock(&(map)->flags_lock);				\
	if (oflags & VM_MAP_WANTLOCK)					\
		wakeup(&(map)->flags);					\
} while (0)
#endif /* _KERNEL */

/*
 *	Functions implemented as macros
 */
#define		vm_map_min(map)		((map)->min_offset)
#define		vm_map_max(map)		((map)->max_offset)
#define		vm_map_pmap(map)	((map)->pmap)

#endif /* _UVM_UVM_MAP_H_ */
