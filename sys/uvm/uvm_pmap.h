/*	$OpenBSD: uvm_pmap.h,v 1.15 2002/09/10 18:29:44 art Exp $	*/
/*	$NetBSD: uvm_pmap.h,v 1.1 2000/06/27 09:00:14 mrg Exp $	*/

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
 *	@(#)pmap.h	8.1 (Berkeley) 6/11/93
 *
 *
 * Copyright (c) 1987, 1990 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Avadis Tevanian, Jr.
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
 *	Machine address mapping definitions -- machine-independent
 *	section.  [For machine-dependent section, see "machine/pmap.h".]
 */

#ifndef	_PMAP_VM_
#define	_PMAP_VM_

struct proc;		/* for pmap_activate()/pmap_deactivate() proto */

/*
 * Each machine dependent implementation is expected to
 * keep certain statistics.  They may do this anyway they
 * so choose, but are expected to return the statistics
 * in the following structure.
 */
struct pmap_statistics {
	long		resident_count;	/* # of pages mapped (total)*/
	long		wired_count;	/* # of pages wired */
};
typedef struct pmap_statistics	*pmap_statistics_t;

#include <machine/pmap.h>

/*
 * Flags passed to pmap_enter().  Note the bottom 3 bits are VM_PROT_*
 * bits, used to indicate the access type that was made (to seed modified
 * and referenced information).
 */
#define	PMAP_WIRED	0x00000010	/* wired mapping */
#define	PMAP_CANFAIL	0x00000020	/* can fail if resource shortage */

#ifndef PMAP_EXCLUDE_DECLS	/* Used in Sparc port to virtualize pmap mod */
#ifdef _KERNEL
__BEGIN_DECLS
void		*pmap_bootstrap_alloc(int);
#ifndef	pmap_activate
void		 pmap_activate(struct proc *);
#endif
#ifndef	pmap_deactivate
void		 pmap_deactivate(struct proc *);
#endif
#ifndef	pmap_unwire
void		 pmap_unwire(pmap_t, vaddr_t);
#endif

#if !defined(pmap_clear_modify)
boolean_t	 pmap_clear_modify(struct vm_page *);
#endif
#if !defined(pmap_clear_reference)
boolean_t	 pmap_clear_reference(struct vm_page *);
#endif

#if !defined(pmap_collect)
void		 pmap_collect(pmap_t);
#endif
#if !defined(pmap_copy)
void		 pmap_copy(pmap_t, pmap_t, vaddr_t, vsize_t, vaddr_t);
#endif
#if !defined(pmap_copy_page)
void		 pmap_copy_page(struct vm_page *, struct vm_page *);
#endif
struct pmap 	 *pmap_create(void);
void		 pmap_destroy(pmap_t);
int		 pmap_enter(pmap_t,
		    vaddr_t, paddr_t, vm_prot_t, int);
boolean_t	 pmap_extract(pmap_t, vaddr_t, paddr_t *);
#if defined(PMAP_GROWKERNEL)
vaddr_t		 pmap_growkernel(vaddr_t);
#endif

void		 pmap_init(void);

void		 pmap_kenter_pa(vaddr_t, paddr_t, vm_prot_t);
void		 pmap_kremove(vaddr_t, vsize_t);
#if !defined(pmap_is_modified)
boolean_t	 pmap_is_modified(struct vm_page *);
#endif
#if !defined(pmap_is_referenced)
boolean_t	 pmap_is_referenced(struct vm_page *);
#endif

void		 pmap_page_protect(struct vm_page *, vm_prot_t);

#if !defined(pmap_phys_address)
paddr_t	 pmap_phys_address(int);
#endif
void		 pmap_protect(pmap_t,
		    vaddr_t, vaddr_t, vm_prot_t);
#if !defined(pmap_reference)
void		 pmap_reference(pmap_t);
#endif
#if !defined(pmap_remove)
void		 pmap_remove(pmap_t, vaddr_t, vaddr_t);
#endif
#if !defined(pmap_update)
void		 pmap_update(void);
#endif
#if !defined(pmap_zero_page)
void		 pmap_zero_page(struct vm_page *);
#endif

void		 pmap_virtual_space(vaddr_t *, vaddr_t *);
#if defined(PMAP_STEAL_MEMORY)
vaddr_t		 pmap_steal_memory(vsize_t, vaddr_t *, vaddr_t *);
#endif

#if defined(PMAP_FORK)
void		pmap_fork(pmap_t, pmap_t);
#endif
__END_DECLS
#endif	/* kernel*/
#endif  /* PMAP_EXCLUDE_DECLS */

#endif /* _PMAP_VM_ */
