/*	$OpenBSD: vmparam.h,v 1.24 2002/10/17 02:21:08 mickey Exp $	*/

/* 
 * Copyright (c) 1988-1994, The University of Utah and
 * the Computer Systems Laboratory at the University of Utah (CSL).
 * All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software is hereby
 * granted provided that (1) source code retains these copyright, permission,
 * and disclaimer notices, and (2) redistributions including binaries
 * reproduce the notices in supporting documentation, and (3) all advertising
 * materials mentioning features or use of this software display the following
 * acknowledgement: ``This product includes software developed by the
 * Computer Systems Laboratory at the University of Utah.''
 *
 * THE UNIVERSITY OF UTAH AND CSL ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS
 * IS" CONDITION.  THE UNIVERSITY OF UTAH AND CSL DISCLAIM ANY LIABILITY OF
 * ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * CSL requests users of this software to return to csl-dist@cs.utah.edu any
 * improvements that they make and grant CSL redistribution rights.
 *
 * 	Utah $Hdr: vmparam.h 1.16 94/12/16$
 */

#ifndef _MACHINE_VMPARAM_H_
#define _MACHINE_VMPARAM_H_

/*
 * Machine dependent constants for HP PA
 */
/*
 * USRTEXT is the start of the user text/data space, while USRSTACK
 * is the top (end) of the user stack.  LOWPAGES and HIGHPAGES are
 * the number of pages from the beginning of the P0 region to the
 * beginning of the text and from the beginning of the P1 region to the
 * beginning of the stack respectively.
 */
#define	USRTEXT		0x00002000		/* Start of user .text */
#define	USRSTACK	0x68FF3000		/* Start of user stack */
#define	LOWPAGES	0
#define	HIGHPAGES	UPAGES
#define	SYSCALLGATE	0xC0000000		/* syscall gateway page */

/*
 * Virtual memory related constants, all in bytes
 */
#ifndef MAXTSIZ
#define	MAXTSIZ		(0x40000000)		/* max text size */
#endif
#ifndef DFLDSIZ
#define	DFLDSIZ		(16*1024*1024)		/* initial data size limit */
#endif
#ifndef MAXDSIZ
#define	MAXDSIZ		(USRSTACK-MAXTSIZ)	/* max data size */
#endif
#ifndef	DFLSSIZ
#define	DFLSSIZ		(512*1024)		/* initial stack size limit */
#endif
#ifndef	MAXSSIZ
#define	MAXSSIZ		(UADDR-USRSTACK)	/* max stack size */
#endif

#ifndef USRIOSIZE
#define	USRIOSIZE	((2*HPPA_PGALIAS)/PAGE_SIZE)	/* 2mb */
#endif

/*
 * PTEs for system V style shared memory.
 * This is basically slop for kmempt which we actually allocate (malloc) from.
 */
#ifndef SHMMAXPGS
#define SHMMAXPGS	((1024*1024*10)/NBPG)	/* 10mb */
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

/* user/kernel map constants */
#define	VM_MIN_ADDRESS		((vaddr_t)0)
#define	VM_MAXUSER_ADDRESS	((vaddr_t)0xc0000000)
#define	VM_MAX_ADDRESS		VM_MAXUSER_ADDRESS
#define	VM_MIN_KERNEL_ADDRESS	((vaddr_t)0xc0001000)
#define	VM_MAX_KERNEL_ADDRESS	((vaddr_t)0xf0000000)

/* virtual sizes (bytes) for various kernel submaps */
#define VM_PHYS_SIZE		(USRIOSIZE*PAGE_SIZE)

#define	VM_PHYSSEG_MAX	8	/* this many physmem segments */
#define	VM_PHYSSEG_STRAT	VM_PSTRAT_BIGFIRST

#define	VM_PHYSSEG_NOADD	/* XXX until uvm code is fixed */

#define	VM_NFREELIST		2
#define	VM_FREELIST_DEFAULT	0
#define	VM_FREELIST_ARCH	1

#if defined(_KERNEL) && !defined(_LOCORE)
#define __HAVE_VM_PAGE_MD
struct pv_entry;
struct vm_page_md {
	struct simplelock pvh_lock;	/* locks every pv on this list */
	struct pv_entry	*pvh_list;	/* head of list (locked by pvh_lock) */
	u_int		pvh_attrs;	/* to preserve ref/mod */
};

#define	VM_MDPAGE_INIT(pg) do {				\
	simple_lock_init(&(pg)->mdpage.pvh_lock);	\
	(pg)->mdpage.pvh_list = NULL;			\
	(pg)->mdpage.pvh_attrs = 0;			\
} while (0)
#endif

#endif	/* _MACHINE_VMPARAM_H_ */

