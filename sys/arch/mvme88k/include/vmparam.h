/*	$OpenBSD: vmparam.h,v 1.12 2001/06/14 21:30:40 miod Exp $ */
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
#define	USRSTACK	0x80000000		/* Start of user stack */

/*
 * Virtual memory related constants, all in bytes
 */
#ifndef MAXTSIZ
#define	MAXTSIZ		(8*1024*1024)		/* max text size */
#endif
#ifndef DFLDSIZ
#define	DFLDSIZ		(16*1024*1024)		/* initial data size limit */
#endif
#ifndef MAXDSIZ
#define	MAXDSIZ		(64*1024*1024)		/* max data size */
#endif
#ifndef	DFLSSIZ
#define	DFLSSIZ		(512*1024)		/* initial stack size limit */
#endif
#ifndef	MAXSSIZ
#define	MAXSSIZ		MAXDSIZ			/* max stack size */
#endif

/*
 * PTEs for mapping user space into the kernel for phyio operations.
 * One page is enough to handle 4Mb of simultaneous raw IO operations.
 */
#ifndef USRIOSIZE
#define USRIOSIZE	(1 * NPTEPG)	/* 4mb */
#endif

/*
 * External IO space map size.
 */
#ifndef EIOMAPSIZE
#define EIOMAPSIZE	1024		/* in pages */
#endif

/*
 * Default sizes of swap allocation chunks (see dmap.h).
 * The actual values may be changed in vminit() based on MAXDSIZ.
 * With MAXDSIZ of 16Mb and NDMAP of 38, dmmax will be 1024.
 * DMMIN should be at least ctod(1) so that vtod() works.
 * vminit() insures this.
 */
#define	DMMIN	32			/* smallest swap allocation */
#define	DMMAX	4096			/* largest potential swap allocation */
#define	DMTEXT	1024			/* swap allocation for text */

/*
 * Size of shared memory map
 */
#ifndef SHMMAXPGS
#define SHMMAXPGS	1024
#endif

/*
 * The time for a process to be blocked before being very swappable.
 * This is a number of seconds which the system takes as being a non-trivial
 * amount of real time.  You probably shouldn't change this;
 * it is used in subtle ways (fractions and multiples of it are, that is, like
 * half of a ``long time'', almost a long time, etc.)
 * It is related to human patience and other factors which don't really
 * change over time.
 */
#define	MAXSLP 		20

/*
 * A swapped in process is given a small amount of core without being bothered
 * by the page replacement algorithm.  Basically this says that if you are
 * swapped in you deserve some resources.  We protect the last SAFERSS
 * pages against paging and will just swap you out rather than paging you.
 */
#define	SAFERSS		4		/* nominal ``small'' resident set size
					   protected against replacement */

#define	VM_MINUSER_ADDRESS	((vm_offset_t) 0)
#define	VM_MAXUSER_ADDRESS	((vm_offset_t) 0xffc00000U)

#define VM_MINKERNEL_ADDRESS	((vm_offset_t) 0)
#define VM_MAXKERNEL_ADDRESS	((vm_offset_t) 0x1fffffff)

/*
 * Mach derived constants
 */
#define	VM_MIN_ADDRESS		((vm_offset_t) 0)
#define	VM_MAX_ADDRESS		((vm_offset_t) 0xffc00000U)

#define	VM_MIN_USER_ADDRESS	((vm_offset_t) 0)
#define	VM_MAX_USER_ADDRESS	((vm_offset_t) 0xffc00000U)

/* on vme188, max = 0xf0000000 */

#define VM_MIN_KERNEL_ADDRESS	((vm_offset_t) 0)
#define VM_MAX_KERNEL_ADDRESS	((vm_offset_t) 0x1fffffff)

#define KERNEL_STACK_SIZE	(3 * PAGE_SIZE)	/* kernel stack size */
#define INTSTACK_SIZE		(3 * PAGE_SIZE)	/* interrupt stack size */

/* virtual sizes (bytes) for various kernel submaps */
#define VM_MBUF_SIZE		(NMBCLUSTERS * MCLBYTES)
#define VM_KMEM_SIZE		(NKMEMCLUSTERS * PAGE_SIZE)
#define VM_PHYS_SIZE		(USRIOSIZE * PAGE_SIZE)

/* Use new VM page bootstrap interface. */
#define	MACHINE_NEW_NONCONTIG

#if defined(MACHINE_NEW_NONCONTIG)
/*
 * Constants which control the way the VM system deals with memory segments.
 * The mvme88k only has one physical memory segment.
 */
#define	VM_PHYSSEG_MAX		1
#define	VM_PHYSSEG_STRAT	VM_PSTRAT_BSEARCH
#define	VM_PHYSSEG_NOADD

#define VM_NFREELIST		1
#define VM_FREELIST_DEFAULT	0

#ifndef _LOCORE
/*
 * pmap-specific data stored in the vm_physmem[] array.
 */
struct pmap_physseg {
	struct pv_entry *pvent;		/* pv table for this seg */
	char *attrs;			/* page modify list for this seg */
	struct simplelock *plock;	/* page lock for this seg */
};
#endif /* _LOCORE */

#endif /* MACHINE_NEW_NONCONTIG */

#endif	_MACHINE_VM_PARAM_
