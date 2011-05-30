/*	$OpenBSD: vmparam.h,v 1.11 2011/05/30 22:25:21 oga Exp $ */
/*
 * Mach Operating System
 * Copyright (c) 1992 Carnegie Mellon University
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */

/*
 *	machine dependent virtual memory parameters.
 */


#ifndef	_MACHINE_VM_PARAM_
#define _MACHINE_VM_PARAM_

/*
 * USRTEXT is the start of the user text/data space, while USRSTACK
 * is the top (end) of the user stack.
 */
#define	USRTEXT		0x1000			/* Start of user text */
#define	USRSTACK	VM_MAXUSER_ADDRESS	/* Start of user stack */

/*
 * Virtual memory related constants, all in bytes
 */
#ifndef MAXTSIZ
#define	MAXTSIZ		(8*1024*1024)		/* max text size */
#endif
#ifndef DFLDSIZ
#define	DFLDSIZ		(32*1024*1024)		/* initial data size limit */
#endif
#ifndef MAXDSIZ
#define	MAXDSIZ		(64*1024*1024)		/* max data size */
#endif
#ifndef BRKSIZ
#define	BRKSIZ		MAXDSIZ			/* heap gap size */
#endif
#ifndef	DFLSSIZ
#define	DFLSSIZ		(2*1024*1024)		/* initial stack size limit */
#endif
#ifndef	MAXSSIZ
#define	MAXSSIZ		MAXDSIZ			/* max stack size */
#endif

#define STACKGAP_RANDOM	256*1024

/*
 * Size of shared memory map
 */
#ifndef SHMMAXPGS
#define SHMMAXPGS	1024
#endif

#define	VM_MIN_ADDRESS		((vaddr_t)0)
#define	VM_MAX_ADDRESS		((vaddr_t)0xfffff000)
#define VM_MAXUSER_ADDRESS	VM_MAX_ADDRESS

/* virtual sizes (bytes) for various kernel submaps */
#define VM_PHYS_SIZE		(1 * NPTEPG * PAGE_SIZE)

/*
 * Constants which control the way the VM system deals with memory segments.
 * The mvme88k only has one physical memory segment.
 */
#define	VM_PHYSSEG_MAX		1
#define	VM_PHYSSEG_STRAT	VM_PSTRAT_RANDOM
#define	VM_PHYSSEG_NOADD

#ifndef _LOCORE
/*
 * pmap-specific data stored in the vm_physmem[] array.
 */

/* XXX - belongs in pmap.h, but put here because of ordering issues */
struct pv_entry {
	struct pv_entry	*pv_next;	/* next pv_entry */
	struct pmap	*pv_pmap;	/* pmap where mapping lies */
	vaddr_t		pv_va;		/* virtual address for mapping */
	int		pv_flags;
};

#define	__HAVE_VM_PAGE_MD
struct vm_page_md {
	struct pv_entry pvent;
};

#define	VM_MDPAGE_INIT(pg) do {			\
	(pg)->mdpage.pvent.pv_next = NULL;	\
	(pg)->mdpage.pvent.pv_pmap = NULL;	\
	(pg)->mdpage.pvent.pv_va = 0;		\
	(pg)->mdpage.pvent.pv_flags = 0;	\
} while (0)

#endif /* _LOCORE */

#endif /* _MACHINE_VM_PARAM_ */
