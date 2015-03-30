/*	$OpenBSD: uvm_map.h,v 1.54 2015/03/30 21:08:40 miod Exp $	*/
/*	$NetBSD: uvm_map.h,v 1.24 2001/02/18 21:19:08 chs Exp $	*/

/*
 * Copyright (c) 2011 Ariane van der Steldt <ariane@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * 
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

#include <sys/mutex.h>
#include <sys/rwlock.h>

#ifdef _KERNEL

/*
 * Internal functions.
 *
 * Required by clipping macros.
 */
void			 uvm_map_clip_end(struct vm_map*, struct vm_map_entry*,
			    vaddr_t);
void			 uvm_map_clip_start(struct vm_map*,
			    struct vm_map_entry*, vaddr_t);

/*
 * UVM_MAP_CLIP_START: ensure that the entry begins at or after
 * the starting address, if it doesn't we split the entry.
 * 
 * => map must be locked by caller
 */

#define UVM_MAP_CLIP_START(_map, _entry, _addr)				\
	do {								\
		KASSERT((_entry)->end + (_entry)->fspace > (_addr));	\
		if ((_entry)->start < (_addr))				\
			uvm_map_clip_start((_map), (_entry), (_addr));	\
	} while (0)

/*
 * UVM_MAP_CLIP_END: ensure that the entry ends at or before
 *      the ending address, if it does't we split the entry.
 *
 * => map must be locked by caller
 */

#define UVM_MAP_CLIP_END(_map, _entry, _addr)				\
	do {								\
		KASSERT((_entry)->start < (_addr));			\
		if ((_entry)->end > (_addr))				\
			uvm_map_clip_end((_map), (_entry), (_addr));	\
	} while (0)

/*
 * extract flags
 */
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
	union {
		RB_ENTRY(vm_map_entry)	addr_entry; /* address tree */
	} daddrs;

	union {
		RB_ENTRY(vm_map_entry)	rbtree;	/* Link freespace tree. */
		TAILQ_ENTRY(vm_map_entry) tailq;/* Link freespace queue. */
		TAILQ_ENTRY(vm_map_entry) deadq;/* dead entry queue */
	} dfree;

#define uvm_map_entry_start_copy start
	vaddr_t			start;		/* start address */
	vaddr_t			end;		/* end address */

	vsize_t			guard;		/* bytes in guard */
	vsize_t			fspace;		/* free space */

	union vm_map_object	object;		/* object I point to */
	voff_t			offset;		/* offset into object */
	struct vm_aref		aref;		/* anonymous overlay */

	int			etype;		/* entry type */

	vm_prot_t		protection;	/* protection code */
	vm_prot_t		max_protection;	/* maximum protection */
	vm_inherit_t		inheritance;	/* inheritance */

	int			wired_count;	/* can be paged if == 0 */
	int			advice;		/* madvise advice */
#define uvm_map_entry_stop_copy flags
	u_int8_t		flags;		/* flags */

#define UVM_MAP_STATIC		0x01		/* static map entry */
#define UVM_MAP_KMEM		0x02		/* from kmem entry pool */

	vsize_t			fspace_augment;	/* max(fspace) in subtree */
};

#define	VM_MAPENT_ISWIRED(entry)	((entry)->wired_count != 0)

TAILQ_HEAD(uvm_map_deadq, vm_map_entry);	/* dead entry queue */
RB_HEAD(uvm_map_addr, vm_map_entry);
RB_PROTOTYPE(uvm_map_addr, vm_map_entry, daddrs.addr_entry,
    uvm_mapentry_addrcmp);

/*
 *	A Map is a rbtree of map entries, kept sorted by address.
 *	In addition, free space entries are also kept in a rbtree,
 *	indexed by free size.
 *
 *
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
 *
 *		VM_MAP_GUARDPAGES	r/o; must be specified at map
 *					initialization time.
 *					If set, guards will appear between
 *					automatic allocations.
 *					No locking required.
 *
 *		VM_MAP_ISVMSPACE	r/o; set by uvmspace_alloc.
 *					Signifies that this map is a vmspace.
 *					(The implementation treats all maps
 *					without this bit as kernel maps.)
 *					No locking required.
 *
 *
 * All automatic allocations (uvm_map without MAP_FIXED) will allocate
 * from vm_map.free.
 * If that allocation fails:
 * - vmspace maps will spill over into vm_map.bfree,
 * - all other maps will call uvm_map_kmem_grow() to increase the arena.
 * 
 * vmspace maps have their data, brk() and stack arenas automatically
 * updated when uvm_map() is invoked without MAP_FIXED.
 * The spill over arena (vm_map.bfree) will contain the space in the brk()
 * and stack ranges.
 * Kernel maps never have a bfree arena and this tree will always be empty.
 *
 *
 * read_locks and write_locks are used in lock debugging code.
 */
struct vm_map {
	struct pmap *		pmap;		/* Physical map */
	struct rwlock		lock;		/* Lock for map data */

	struct uvm_map_addr	addr;		/* Entry tree, by addr */

	vsize_t			size;		/* virtual size */
	int			ref_count;	/* Reference count */
	int			flags;		/* flags */
	struct mutex		flags_lock;	/* flags lock */
	unsigned int		timestamp;	/* Version number */

	vaddr_t			min_offset;	/* First address in map. */
	vaddr_t			max_offset;	/* Last address in map. */

	/*
	 * Allocation overflow regions.
	 */
	vaddr_t			b_start;	/* Start for brk() alloc. */
	vaddr_t			b_end;		/* End for brk() alloc. */
	vaddr_t			s_start;	/* Start for stack alloc. */
	vaddr_t			s_end;		/* End for stack alloc. */

	/*
	 * Special address selectors.
	 *
	 * The uaddr_exe mapping is used if:
	 * - protX is selected
	 * - the pointer is not NULL
	 *
	 * If uaddr_exe is not used, the other mappings are checked in
	 * order of appearance.
	 * If a hint is given, the selection will only be used if the hint
	 * falls in the range described by the mapping.
	 *
	 * The states are pointers because:
	 * - they may not all be in use
	 * - the struct size for different schemes is variable
	 *
	 * The uaddr_brk_stack selector will select addresses that are in
	 * the brk/stack area of the map.
	 */
	struct uvm_addr_state	*uaddr_exe;	/* Executable selector. */
	struct uvm_addr_state	*uaddr_any[4];	/* More selectors. */
	struct uvm_addr_state	*uaddr_brk_stack; /* Brk/stack selector. */
};

/* vm_map flags */
#define	VM_MAP_PAGEABLE		0x01		/* ro: entries are pageable */
#define	VM_MAP_INTRSAFE		0x02		/* ro: interrupt safe map */
#define	VM_MAP_WIREFUTURE	0x04		/* rw: wire future mappings */
#define	VM_MAP_BUSY		0x08		/* rw: map is busy */
#define	VM_MAP_WANTLOCK		0x10		/* rw: want to write-lock */
#define VM_MAP_GUARDPAGES	0x20		/* rw: add guard pgs to map */
#define VM_MAP_ISVMSPACE	0x40		/* ro: map is a vmspace */

/* XXX: number of kernel maps and entries to statically allocate */

#if !defined(MAX_KMAPENT)
#define	MAX_KMAPENT	1024	/* Sufficient to make it to the scheduler. */
#endif	/* !defined MAX_KMAPENT */

#ifdef _KERNEL
#define	vm_map_modflags(map, set, clear)				\
do {									\
	mtx_enter(&(map)->flags_lock);					\
	(map)->flags = ((map)->flags | (set)) & ~(clear);		\
	mtx_leave(&(map)->flags_lock);					\
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

/*
 * globals:
 */

#ifdef _KERNEL

extern vaddr_t	uvm_maxkaddr;

/*
 * protos: the following prototypes define the interface to vm_map
 */

void		uvm_map_deallocate(vm_map_t);

int		uvm_map_clean(vm_map_t, vaddr_t, vaddr_t, int);
vm_map_t	uvm_map_create(pmap_t, vaddr_t, vaddr_t, int);
int		uvm_map_extract(struct vm_map*, vaddr_t, vsize_t, vaddr_t*,
		    int);
vaddr_t		uvm_map_pie(vaddr_t);
vaddr_t		uvm_map_hint(struct vmspace *, vm_prot_t, vaddr_t, vaddr_t);
int		uvm_map_inherit(vm_map_t, vaddr_t, vaddr_t, vm_inherit_t);
int		uvm_map_advice(vm_map_t, vaddr_t, vaddr_t, int);
void		uvm_map_init(void);
boolean_t	uvm_map_lookup_entry(vm_map_t, vaddr_t, vm_map_entry_t *);
int		uvm_map_replace(vm_map_t, vaddr_t, vaddr_t,
		    vm_map_entry_t, int);
int		uvm_map_reserve(vm_map_t, vsize_t, vaddr_t, vsize_t,
		    vaddr_t *);
void		uvm_map_setup(vm_map_t, vaddr_t, vaddr_t, int);
int		uvm_map_submap(vm_map_t, vaddr_t, vaddr_t, vm_map_t);
void		uvm_unmap(vm_map_t, vaddr_t, vaddr_t);
void		uvm_map_set_uaddr(struct vm_map*, struct uvm_addr_state**,
		    struct uvm_addr_state*);
int		uvm_map_mquery(struct vm_map*, vaddr_t*, vsize_t, voff_t, int);

void		uvm_unmap_detach(struct uvm_map_deadq*, int);
void		uvm_unmap_remove(struct vm_map*, vaddr_t, vaddr_t,
		    struct uvm_map_deadq*, boolean_t, boolean_t);

struct kinfo_vmentry;

int		uvm_map_fill_vmmap(struct vm_map *, struct kinfo_vmentry *,
		    size_t *);

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
 */

#ifdef _KERNEL
/*
 * XXX: clean up later
 * Half the kernel seems to depend on them being included here.
 */
#include <sys/time.h>
#include <sys/systm.h>  /* for panic() */

boolean_t	vm_map_lock_try_ln(struct vm_map*, char*, int);
void		vm_map_lock_ln(struct vm_map*, char*, int);
void		vm_map_lock_read_ln(struct vm_map*, char*, int);
void		vm_map_unlock_ln(struct vm_map*, char*, int);
void		vm_map_unlock_read_ln(struct vm_map*, char*, int);
void		vm_map_downgrade_ln(struct vm_map*, char*, int);
void		vm_map_upgrade_ln(struct vm_map*, char*, int);
void		vm_map_busy_ln(struct vm_map*, char*, int);
void		vm_map_unbusy_ln(struct vm_map*, char*, int);

#ifdef DIAGNOSTIC
#define vm_map_lock_try(map)	vm_map_lock_try_ln(map, __FILE__, __LINE__)
#define vm_map_lock(map)	vm_map_lock_ln(map, __FILE__, __LINE__)
#define vm_map_lock_read(map)	vm_map_lock_read_ln(map, __FILE__, __LINE__)
#define vm_map_unlock(map)	vm_map_unlock_ln(map, __FILE__, __LINE__)
#define vm_map_unlock_read(map)	vm_map_unlock_read_ln(map, __FILE__, __LINE__)
#define vm_map_downgrade(map)	vm_map_downgrade_ln(map, __FILE__, __LINE__)
#define vm_map_upgrade(map)	vm_map_upgrade_ln(map, __FILE__, __LINE__)
#define vm_map_busy(map)	vm_map_busy_ln(map, __FILE__, __LINE__)
#define vm_map_unbusy(map)	vm_map_unbusy_ln(map, __FILE__, __LINE__)
#else
#define vm_map_lock_try(map)	vm_map_lock_try_ln(map, NULL, 0)
#define vm_map_lock(map)	vm_map_lock_ln(map, NULL, 0)
#define vm_map_lock_read(map)	vm_map_lock_read_ln(map, NULL, 0)
#define vm_map_unlock(map)	vm_map_unlock_ln(map, NULL, 0)
#define vm_map_unlock_read(map)	vm_map_unlock_read_ln(map, NULL, 0)
#define vm_map_downgrade(map)	vm_map_downgrade_ln(map, NULL, 0)
#define vm_map_upgrade(map)	vm_map_upgrade_ln(map, NULL, 0)
#define vm_map_busy(map)	vm_map_busy_ln(map, NULL, 0)
#define vm_map_unbusy(map)	vm_map_unbusy_ln(map, NULL, 0)
#endif

#endif /* _KERNEL */

/*
 *	Functions implemented as macros
 */
#define		vm_map_min(map)		((map)->min_offset)
#define		vm_map_max(map)		((map)->max_offset)
#define		vm_map_pmap(map)	((map)->pmap)

#endif /* _UVM_UVM_MAP_H_ */
