/*	$OpenBSD: uvm.h,v 1.59 2015/05/04 10:21:15 dlg Exp $	*/
/*	$NetBSD: uvm.h,v 1.24 2000/11/27 08:40:02 chs Exp $	*/

/*
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
 * from: Id: uvm.h,v 1.1.2.14 1998/02/02 20:07:19 chuck Exp
 */

#ifndef _UVM_UVM_H_
#define _UVM_UVM_H_

#include <uvm/uvm_extern.h>
#include <uvm/uvm_amap.h>
#include <uvm/uvm_aobj.h>
#include <uvm/uvm_fault.h>
#include <uvm/uvm_glue.h>
#include <uvm/uvm_km.h>
#include <uvm/uvm_swap.h>

#include <uvm/uvm_pmemrange.h>

/*
 * uvm structure (vm global state: collected in one structure for ease
 * of reference...)
 */

struct uvm {
	/* vm_page related parameters */

	/* vm_page queues */
	struct pglist page_active;	/* allocated pages, in use */
	struct pglist page_inactive_swp;/* pages inactive (reclaim or free) */
	struct pglist page_inactive_obj;/* pages inactive (reclaim or free) */
	/* Lock order: pageqlock, then fpageqlock. */
	struct mutex fpageqlock;	/* lock for free page q  + pdaemon */
	boolean_t page_init_done;	/* TRUE if uvm_page_init() finished */
	struct uvm_pmr_control pmr_control; /* pmemrange data */

		/* page daemon trigger */
	int pagedaemon;			/* daemon sleeps on this */
	struct proc *pagedaemon_proc;	/* daemon's pid */

		/* aiodone daemon trigger */
	int aiodoned;			/* daemon sleeps on this */
	struct proc *aiodoned_proc;	/* daemon's pid */
	struct mutex aiodoned_lock;

	/* static kernel map entry pool */
	vm_map_entry_t kentry_free;	/* free page pool */

	/* aio_done is locked by uvm.aiodoned_lock. */
	TAILQ_HEAD(, buf) aio_done;		/* done async i/o reqs */

	/* kernel object: to support anonymous pageable kernel memory */
	struct uvm_object *kernel_object;
};

/*
 * vm_map_entry etype bits:
 */

#define UVM_ET_OBJ		0x01	/* it is a uvm_object */
#define UVM_ET_SUBMAP		0x02	/* it is a vm_map submap */
#define UVM_ET_COPYONWRITE 	0x04	/* copy_on_write */
#define UVM_ET_NEEDSCOPY	0x08	/* needs_copy */
#define UVM_ET_HOLE		0x10	/* no backend */
#define UVM_ET_NOFAULT		0x20	/* don't fault */
#define UVM_ET_FREEMAPPED	0x80	/* map entry is on free list (DEBUG) */

#define UVM_ET_ISOBJ(E)		(((E)->etype & UVM_ET_OBJ) != 0)
#define UVM_ET_ISSUBMAP(E)	(((E)->etype & UVM_ET_SUBMAP) != 0)
#define UVM_ET_ISCOPYONWRITE(E)	(((E)->etype & UVM_ET_COPYONWRITE) != 0)
#define UVM_ET_ISNEEDSCOPY(E)	(((E)->etype & UVM_ET_NEEDSCOPY) != 0)
#define UVM_ET_ISHOLE(E)	(((E)->etype & UVM_ET_HOLE) != 0)
#define UVM_ET_ISNOFAULT(E)	(((E)->etype & UVM_ET_NOFAULT) != 0)

#ifdef _KERNEL

/*
 * holds all the internal UVM data
 */
extern struct uvm uvm;

/*
 * UVM_WAIT: wait... wrapper around the tsleep() function.
 */

#define	UVM_WAIT(event, intr, msg, timo)				\
do {									\
	tsleep(event, PVM|(intr ? PCATCH : 0), msg, timo);		\
} while (0)

/*
 * UVM_PAGE_OWN: track page ownership (only if UVM_PAGE_TRKOWN)
 */

#if defined(UVM_PAGE_TRKOWN)
#define UVM_PAGE_OWN(PG, TAG) uvm_page_own(PG, TAG)
#else
#define UVM_PAGE_OWN(PG, TAG) /* nothing */
#endif /* UVM_PAGE_TRKOWN */

/*
 * uvm_map internal functions.
 * Used by uvm_map address selectors.
 */
struct vm_map_entry	*uvm_map_entrybyaddr(struct uvm_map_addr *, vaddr_t);
int			 uvm_map_isavail(struct vm_map *,
			    struct uvm_addr_state *,
			    struct vm_map_entry **, struct vm_map_entry**,
			    vaddr_t, vsize_t);
struct uvm_addr_state	*uvm_map_uaddr(struct vm_map *, vaddr_t);
struct uvm_addr_state	*uvm_map_uaddr_e(struct vm_map *, struct vm_map_entry *);

#define VMMAP_FREE_START(_entry)	((_entry)->end + (_entry)->guard)
#define VMMAP_FREE_END(_entry)		((_entry)->end + (_entry)->guard + \
					    (_entry)->fspace)

#endif /* _KERNEL */

#endif /* _UVM_UVM_H_ */
