/*	$OpenBSD: uvm_param.h,v 1.9 2003/11/08 19:17:28 jmc Exp $	*/
/*	$NetBSD: uvm_param.h,v 1.5 2001/03/09 01:02:12 chs Exp $	*/

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
 *	@(#)vm_param.h	8.2 (Berkeley) 1/9/95
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
 *	Machine independent virtual memory parameters.
 */

#ifndef	_VM_PARAM_
#define	_VM_PARAM_

#include <machine/vmparam.h>

/*
 * This belongs in types.h, but breaks too many existing programs.
 */
typedef int	boolean_t;
#ifndef TRUE
#define	TRUE	1
#endif
#ifndef FALSE
#define	FALSE	0
#endif

/*
 *	The machine independent pages are referred to as PAGES.  A page
 *	is some number of hardware pages, depending on the target machine.
 */
#define	DEFAULT_PAGE_SIZE	4096

#if defined(_KERNEL) && !defined(PAGE_SIZE)
/*
 *	All references to the size of a page should be done with PAGE_SIZE
 *	or PAGE_SHIFT.  The fact they are variables is hidden here so that
 *	we can easily make them constant if we so desire.
 */
#define	PAGE_SIZE	uvmexp.pagesize		/* size of page */
#define	PAGE_MASK	uvmexp.pagemask		/* size of page - 1 */
#define	PAGE_SHIFT	uvmexp.pageshift	/* bits to shift for pages */
#endif /* _KERNEL */

/*
 * CTL_VM identifiers
 */
#define	VM_METER	1		/* struct vmmeter */
#define	VM_LOADAVG	2		/* struct loadavg */
#define	VM_PSSTRINGS	3		/* PSSTRINGS */
#define VM_UVMEXP	4		/* struct uvmexp */
#define VM_SWAPENCRYPT	5		/* int */
#define VM_NKMEMPAGES	6		/* int - # kmem_map pages */
#define	VM_ANONMIN	7
#define	VM_VTEXTMIN	8
#define	VM_VNODEMIN	9
#define	VM_MAXSLP	10
#define	VM_USPACE	11
#define	VM_MAXID	12		/* number of valid vm ids */

#define	CTL_VM_NAMES { \
	{ 0, 0 }, \
	{ "vmmeter", CTLTYPE_STRUCT }, \
	{ "loadavg", CTLTYPE_STRUCT }, \
	{ "psstrings", CTLTYPE_STRUCT }, \
	{ "uvmexp", CTLTYPE_STRUCT }, \
	{ "swapencrypt", CTLTYPE_NODE }, \
	{ "nkmempages", CTLTYPE_INT }, \
	{ "anonmin", CTLTYPE_INT }, \
	{ "vtextmin", CTLTYPE_INT }, \
	{ "vnodemin", CTLTYPE_INT }, \
	{ "maxslp", CTLTYPE_INT }, \
	{ "uspace", CTLTYPE_INT }, \
}

struct _ps_strings {
	void	*val;
};

#define SWAPSKIPBYTES	8192	/* never use at the start of a swap space */

/* 
 *	Return values from the VM routines.
 */
#define	KERN_SUCCESS		0
#define	KERN_INVALID_ADDRESS	EFAULT
#define	KERN_PROTECTION_FAILURE	EACCES
#define	KERN_NO_SPACE		ENOMEM
#define	KERN_INVALID_ARGUMENT	EINVAL
#define	KERN_FAILURE		EFAULT
#define	KERN_RESOURCE_SHORTAGE	ENOMEM
#define	KERN_PAGES_LOCKED	9		/* XXX never returned */

#ifndef ASSEMBLER
/*
 *	Convert addresses to pages and vice versa.
 *	No rounding is used.
 */
#ifdef _KERNEL
#define	atop(x)		(((paddr_t)(x)) >> PAGE_SHIFT)
#define	ptoa(x)		((vaddr_t)((vaddr_t)(x) << PAGE_SHIFT))

/*
 * Round off or truncate to the nearest page.  These will work
 * for either addresses or counts (i.e., 1 byte rounds to 1 page).
 */
#define	round_page(x)	(((x) + PAGE_MASK) & ~PAGE_MASK)
#define	trunc_page(x)	((x) & ~PAGE_MASK)

extern psize_t		mem_size;	/* size of physical memory (bytes) */
#ifdef UBC
extern int		ubc_nwins;	/* number of UBC mapping windows */
extern int		ubc_winsize;	/* size of a UBC mapping window */
#endif

#else
/* out-of-kernel versions of round_page and trunc_page */
#define	round_page(x) \
	((((vaddr_t)(x) + (vm_page_size - 1)) / vm_page_size) * \
	    vm_page_size)
#define	trunc_page(x) \
	((((vaddr_t)(x)) / vm_page_size) * vm_page_size)

#endif /* _KERNEL */
#endif /* ASSEMBLER */
#endif /* _VM_PARAM_ */
