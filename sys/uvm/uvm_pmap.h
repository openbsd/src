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
void		*pmap_bootstrap_alloc __P((int));
void		 pmap_activate __P((struct proc *));
void		 pmap_deactivate __P((struct proc *));
void		 pmap_unwire __P((pmap_t, vaddr_t));

#if !defined(pmap_clear_modify)
boolean_t	 pmap_clear_modify __P((struct vm_page *));
#endif
#if !defined(pmap_clear_reference)
boolean_t	 pmap_clear_reference __P((struct vm_page *));
#endif

void		 pmap_collect __P((pmap_t));
void		 pmap_copy __P((pmap_t,
		    pmap_t, vaddr_t, vsize_t, vaddr_t));
void		 pmap_copy_page __P((paddr_t, paddr_t));
struct pmap 	 *pmap_create __P((void));
void		 pmap_destroy __P((pmap_t));
int		 pmap_enter __P((pmap_t,
		    vaddr_t, paddr_t, vm_prot_t, int));
boolean_t	 pmap_extract __P((pmap_t, vaddr_t, paddr_t *));
#if defined(PMAP_GROWKERNEL)
vaddr_t		 pmap_growkernel __P((vaddr_t));
#endif

void		 pmap_init __P((void));

void		 pmap_kenter_pa __P((vaddr_t, paddr_t, vm_prot_t));
void		 pmap_kenter_pgs __P((vaddr_t, struct vm_page **, int));
void		 pmap_kremove __P((vaddr_t, vsize_t));
#if !defined(pmap_is_modified)
boolean_t	 pmap_is_modified __P((struct vm_page *));
#endif
#if !defined(pmap_is_referenced)
boolean_t	 pmap_is_referenced __P((struct vm_page *));
#endif

void		 pmap_page_protect __P((struct vm_page *, vm_prot_t));

#if !defined(pmap_phys_address)
paddr_t	 pmap_phys_address __P((int));
#endif
void		 pmap_protect __P((pmap_t,
		    vaddr_t, vaddr_t, vm_prot_t));
void		 pmap_reference __P((pmap_t));
void		 pmap_remove __P((pmap_t, vaddr_t, vaddr_t));
void		 pmap_update __P((void));
void		 pmap_zero_page __P((paddr_t));

#if defined(PMAP_STEAL_MEMORY)
vaddr_t		 pmap_steal_memory __P((vsize_t, vaddr_t *, vaddr_t *));
#else
void		 pmap_virtual_space __P((vaddr_t *, vaddr_t *));
#endif

#if defined(PMAP_FORK)
void		pmap_fork __P((pmap_t, pmap_t));
#endif
__END_DECLS
#endif	/* kernel*/
#endif  /* PMAP_EXCLUDE_DECLS */

#endif /* _PMAP_VM_ */
