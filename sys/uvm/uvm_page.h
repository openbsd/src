/*	$OpenBSD: uvm_page.h,v 1.20 2002/07/20 22:19:35 art Exp $	*/
/*	$NetBSD: uvm_page.h,v 1.19 2000/12/28 08:24:55 chs Exp $	*/

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
 *	@(#)vm_page.h   7.3 (Berkeley) 4/21/91
 * from: Id: uvm_page.h,v 1.1.2.6 1998/02/04 02:31:42 chuck Exp
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

#ifndef _UVM_UVM_PAGE_H_
#define _UVM_UVM_PAGE_H_

/*
 * uvm_page.h
 */

/*
 *	Resident memory system definitions.
 */

/*
 *	Management of resident (logical) pages.
 *
 *	A small structure is kept for each resident
 *	page, indexed by page number.  Each structure
 *	is an element of several lists:
 *
 *		A hash table bucket used to quickly
 *		perform object/offset lookups
 *
 *		A list of all pages for a given object,
 *		so they can be quickly deactivated at
 *		time of deallocation.
 *
 *		An ordered list of pages due for pageout.
 *
 *	In addition, the structure contains the object
 *	and offset to which this page belongs (for pageout),
 *	and sundry status bits.
 *
 *	Fields in this structure are locked either by the lock on the
 *	object that the page belongs to (O) or by the lock on the page
 *	queues (P) [or both].
 */

/*
 * locking note: the mach version of this data structure had bit
 * fields for the flags, and the bit fields were divided into two
 * items (depending on who locked what).  some time, in BSD, the bit
 * fields were dumped and all the flags were lumped into one short.
 * that is fine for a single threaded uniprocessor OS, but bad if you
 * want to actual make use of locking (simple_lock's).  so, we've
 * seperated things back out again.
 *
 * note the page structure has no lock of its own.
 */

#include <uvm/uvm_extern.h>
#include <uvm/uvm_pglist.h>

struct vm_page {
	TAILQ_ENTRY(vm_page)	pageq;		/* queue info for FIFO
						 * queue or free list (P) */
	TAILQ_ENTRY(vm_page)	hashq;		/* hash table links (O)*/
	TAILQ_ENTRY(vm_page)	listq;		/* pages in same object (O)*/

	struct vm_anon		*uanon;		/* anon (O,P) */
	struct uvm_object	*uobject;	/* object (O,P) */
	voff_t			offset;		/* offset into object (O,P) */

	u_short			flags;		/* object flags [O] */
	u_short			version;	/* version count [O] */
	u_short			wire_count;	/* wired down map refs [P] */
	u_short			pqflags;	/* page queue flags [P] */
	u_int			loan_count;	/* number of active loans
						 * to read: [O or P]
						 * to modify: [O _and_ P] */
	paddr_t			phys_addr;	/* physical address of page */

#ifdef __HAVE_VM_PAGE_MD
	struct vm_page_md	mdpage;		/* pmap-specific data */
#endif
#if defined(UVM_PAGE_TRKOWN)
	/* debugging fields to track page ownership */
	pid_t			owner;		/* proc that set PG_BUSY */
	char			*owner_tag;	/* why it was set busy */
#endif
};

/*
 * These are the flags defined for vm_page.
 *
 * Note: PG_FILLED and PG_DIRTY are added for the filesystems.
 */

/*
 * locking rules:
 *   PG_ ==> locked by object lock
 *   PQ_ ==> lock by page queue lock 
 *   PQ_FREE is locked by free queue lock and is mutex with all other PQs
 *
 * PG_ZERO is used to indicate that a page has been pre-zero'd.  This flag
 * is only set when the page is on no queues, and is cleared when the page
 * is placed on the free list.
 */

#define	PG_BUSY		0x0001		/* page is locked */
#define	PG_WANTED	0x0002		/* someone is waiting for page */
#define	PG_TABLED	0x0004		/* page is in VP table  */
#define	PG_CLEAN	0x0008		/* page has not been modified */
#define PG_CLEANCHK	0x0010		/* clean bit has been checked */
#define PG_RELEASED	0x0020		/* page released while paging */
#define	PG_FAKE		0x0040		/* page is not yet initialized */
#define PG_RDONLY	0x0080		/* page must be mapped read-only */
#define PG_ZERO		0x0100		/* page is pre-zero'd */

#define PG_PAGER1	0x1000		/* pager-specific flag */

#define PQ_FREE		0x0001		/* page is on free list */
#define PQ_INACTIVE	0x0002		/* page is in inactive list */
#define PQ_ACTIVE	0x0004		/* page is in active list */
#define PQ_ANON		0x0010		/* page is part of an anon, rather
					   than an uvm_object */
#define PQ_AOBJ		0x0020		/* page is part of an anonymous
					   uvm_object */
#define PQ_SWAPBACKED	(PQ_ANON|PQ_AOBJ)
#define	PQ_ENCRYPT	0x0040		/* page needs {en,de}cryption */

/*
 * physical memory layout structure
 *
 * MD vmparam.h must #define:
 *   VM_PHYSEG_MAX = max number of physical memory segments we support
 *		   (if this is "1" then we revert to a "contig" case)
 *   VM_PHYSSEG_STRAT: memory sort/search options (for VM_PHYSEG_MAX > 1)
 * 	- VM_PSTRAT_RANDOM:   linear search (random order)
 *	- VM_PSTRAT_BSEARCH:  binary search (sorted by address)
 *	- VM_PSTRAT_BIGFIRST: linear search (sorted by largest segment first)
 *      - others?
 *   XXXCDC: eventually we should purge all left-over global variables...
 */
#define VM_PSTRAT_RANDOM	1
#define VM_PSTRAT_BSEARCH	2
#define VM_PSTRAT_BIGFIRST	3

/*
 * vm_physmemseg: describes one segment of physical memory
 */
struct vm_physseg {
	paddr_t	start;			/* PF# of first page in segment */
	paddr_t	end;			/* (PF# of last page in segment) + 1 */
	paddr_t	avail_start;		/* PF# of first free page in segment */
	paddr_t	avail_end;		/* (PF# of last free page in segment) +1  */
	int	free_list;		/* which free list they belong on */
	struct	vm_page *pgs;		/* vm_page structures (from start) */
	struct	vm_page *lastpg;	/* vm_page structure for end */
#ifdef __HAVE_PMAP_PHYSSEG
	struct	pmap_physseg pmseg;	/* pmap specific (MD) data */
#endif
};

#ifdef _KERNEL

/*
 * globals
 */

extern boolean_t vm_page_zero_enable;

/*
 *	Each pageable resident page falls into one of three lists:
 *
 *	free	
 *		Available for allocation now.
 *	inactive
 *		Not referenced in any map, but still has an
 *		object/offset-page mapping, and may be dirty.
 *		This is the list of pages that should be
 *		paged out next.
 *	active
 *		A list of pages which have been placed in
 *		at least one physical map.  This list is
 *		ordered, in LRU-like fashion.
 */

extern struct pglist	vm_page_queue_free;	/* memory free queue */
extern struct pglist	vm_page_queue_active;	/* active memory queue */
extern struct pglist	vm_page_queue_inactive;	/* inactive memory queue */

/*
 * physical memory config is stored in vm_physmem.
 */

extern struct vm_physseg vm_physmem[VM_PHYSSEG_MAX];
extern int vm_nphysseg;

/*
 * handle inline options
 */

#ifdef UVM_PAGE_INLINE
#define PAGE_INLINE static __inline
#else 
#define PAGE_INLINE /* nothing */
#endif /* UVM_PAGE_INLINE */

/*
 * prototypes: the following prototypes define the interface to pages
 */

void uvm_page_init(vaddr_t *, vaddr_t *);
#if defined(UVM_PAGE_TRKOWN)
void uvm_page_own(struct vm_page *, char *);
#endif
#if !defined(PMAP_STEAL_MEMORY)
boolean_t uvm_page_physget(paddr_t *);
#endif
void uvm_page_rehash(void);
void uvm_pageidlezero(void);

PAGE_INLINE int uvm_lock_fpageq(void);
PAGE_INLINE void uvm_unlock_fpageq(int);

PAGE_INLINE void uvm_pageactivate(struct vm_page *);
vaddr_t uvm_pageboot_alloc(vsize_t);
PAGE_INLINE void uvm_pagecopy(struct vm_page *, struct vm_page *);
PAGE_INLINE void uvm_pagedeactivate(struct vm_page *);
void uvm_pagefree(struct vm_page *);
void uvm_page_unbusy(struct vm_page **, int);
PAGE_INLINE struct vm_page *uvm_pagelookup(struct uvm_object *, voff_t);
PAGE_INLINE void uvm_pageunwire(struct vm_page *);
PAGE_INLINE void uvm_pagewait(struct vm_page *, int);
PAGE_INLINE void uvm_pagewake(struct vm_page *);
PAGE_INLINE void uvm_pagewire(struct vm_page *);
PAGE_INLINE void uvm_pagezero(struct vm_page *);

PAGE_INLINE int uvm_page_lookup_freelist(struct vm_page *);

static struct vm_page *PHYS_TO_VM_PAGE(paddr_t);
static int vm_physseg_find(paddr_t, int *);

/*
 * macros
 */

#define uvm_lock_pageq()	simple_lock(&uvm.pageqlock)
#define uvm_unlock_pageq()	simple_unlock(&uvm.pageqlock)

#define uvm_pagehash(obj,off) \
	(((unsigned long)obj+(unsigned long)atop(off)) & uvm.page_hashmask)

#define	UVM_PAGEZERO_TARGET	(uvmexp.free)

#define VM_PAGE_TO_PHYS(entry)	((entry)->phys_addr)

/*
 * when VM_PHYSSEG_MAX is 1, we can simplify these functions
 */

/*
 * vm_physseg_find: find vm_physseg structure that belongs to a PA
 */
static __inline int
vm_physseg_find(pframe, offp)
	paddr_t pframe;
	int	*offp;
{
#if VM_PHYSSEG_MAX == 1

	/* 'contig' case */
	if (pframe >= vm_physmem[0].start && pframe < vm_physmem[0].end) {
		if (offp)
			*offp = pframe - vm_physmem[0].start;
		return(0);
	}
	return(-1);

#elif (VM_PHYSSEG_STRAT == VM_PSTRAT_BSEARCH)
	/* binary search for it */
	int	start, len, try;

	/*
	 * if try is too large (thus target is less than than try) we reduce
	 * the length to trunc(len/2) [i.e. everything smaller than "try"]
	 *
	 * if the try is too small (thus target is greater than try) then
	 * we set the new start to be (try + 1).   this means we need to
	 * reduce the length to (round(len/2) - 1).
	 *
	 * note "adjust" below which takes advantage of the fact that
	 *  (round(len/2) - 1) == trunc((len - 1) / 2)
	 * for any value of len we may have
	 */

	for (start = 0, len = vm_nphysseg ; len != 0 ; len = len / 2) {
		try = start + (len / 2);	/* try in the middle */

		/* start past our try? */
		if (pframe >= vm_physmem[try].start) {
			/* was try correct? */
			if (pframe < vm_physmem[try].end) {
				if (offp)
					*offp = pframe - vm_physmem[try].start;
				return(try);            /* got it */
			}
			start = try + 1;	/* next time, start here */
			len--;			/* "adjust" */
		} else {
			/*
			 * pframe before try, just reduce length of
			 * region, done in "for" loop
			 */
		}
	}
	return(-1);

#else
	/* linear search for it */
	int	lcv;

	for (lcv = 0; lcv < vm_nphysseg; lcv++) {
		if (pframe >= vm_physmem[lcv].start &&
		    pframe < vm_physmem[lcv].end) {
			if (offp)
				*offp = pframe - vm_physmem[lcv].start;
			return(lcv);		   /* got it */
		}
	}
	return(-1);

#endif
}


/*
 * IS_VM_PHYSADDR: only used my mips/pmax/pica trap/pmap.
 */

#define IS_VM_PHYSADDR(PA) (vm_physseg_find(atop(PA), NULL) != -1)

/*
 * PHYS_TO_VM_PAGE: find vm_page for a PA.   used by MI code to get vm_pages
 * back from an I/O mapping (ugh!).   used in some MD code as well.
 */
static __inline struct vm_page *
PHYS_TO_VM_PAGE(pa)
	paddr_t pa;
{
	paddr_t pf = atop(pa);
	int	off;
	int	psi;

	psi = vm_physseg_find(pf, &off);
	if (psi != -1)
		return(&vm_physmem[psi].pgs[off]);
	return(NULL);
}

#define VM_PAGE_IS_FREE(entry)  ((entry)->pqflags & PQ_FREE)

#endif /* _KERNEL */

#endif /* _UVM_UVM_PAGE_H_ */
