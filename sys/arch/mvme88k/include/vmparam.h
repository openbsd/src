/*	$OpenBSD: vmparam.h,v 1.5 1999/02/09 06:36:27 smurph Exp $ */
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
 * HISTORY
 */
/*
 *	File:	vm_param.h
 *
 *	machine dependent virtual memory parameters.
 *	Most of the declarations are preceeded by M88K_ (or m88k_)
 *	which is OK because only M88K specific code will be using
 *	them.
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
 * Note that each process has at least UPAGES+CLSIZE pages which are not
 * paged anyways (this is currently 8+2=10 pages or 5k bytes), so this
 * number just means a swapped in process is given around 25k bytes.
 * Just for fun: current memory prices are 4600$ a megabyte on VAX (4/22/81),
 * so we loan each swapped in process memory worth 100$, or just admit
 * that we don't consider it worthwhile and swap it out to disk which costs
 * $30/mb or about $0.75.
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
#define BYTE_SIZE	8	/* byte size in bits */

#define M88K_PGBYTES	(1<<12)	/* bytes per m88k page */
#define M88K_PGSHIFT	12	/* number of bits to shift for pages */

/*
 *	Convert bytes to pages and convert pages to bytes.
 *	No rounding is used.
 */

#define	m88k_btop(x)		(((unsigned)(x)) >> M88K_PGSHIFT)
#define	m88k_ptob(x)		(((unsigned)(x)) << M88K_PGSHIFT)

/*
 *	Round off or truncate to the nearest page.  These will work
 *	for either addresses or counts.  (i.e. 1 byte rounds to 1 page
 *	bytes.
 */

#define m88k_round_page(x)	((((unsigned)(x)) + M88K_PGBYTES - 1) & \
					~(M88K_PGBYTES-1))
#define m88k_trunc_page(x)	(((unsigned)(x)) & ~(M88K_PGBYTES-1))

#define	VM_MIN_ADDRESS	((vm_offset_t) 0)
#define	VM_MAX_ADDRESS	((vm_offset_t) 0xffc00000U)

#define	VM_MIN_USER_ADDRESS	((vm_offset_t) 0)
#define	VM_MAX_USER_ADDRESS	((vm_offset_t) 0xffc00000U)

/* on vme188, max = 0xf0000000 */

#define VM_MIN_KERNEL_ADDRESS	((vm_offset_t) 0)
#define VM_MAX_KERNEL_ADDRESS	((vm_offset_t) 0x1fffffff)

#define KERNEL_STACK_SIZE	(3*4096)	/* kernel stack size */
#define INTSTACK_SIZE		(3*4096)	/* interrupt stack size */

/* virtual sizes (bytes) for various kernel submaps */
#define VM_MBUF_SIZE		(NMBCLUSTERS*MCLBYTES)
#define VM_KMEM_SIZE		(NKMEMCLUSTERS*CLBYTES)

/*
 *	Conversion between MACHINE pages and VM pages
 */

#define trunc_m88k_to_vm(p)	(atop(trunc_page(m88k_ptob(p))))
#define round_m88k_to_vm(p)	(atop(round_page(m88k_ptob(p))))
#define vm_to_m88k(p)		(m88k_btop(ptoa(p)))

#if	1	/*Do we really need all this stuff*/
#if	1	/*Do we really need all this stuff*/
#if	1	/*Do we really need all this stuff*/
#define	M88K_SGPAGES	(1<<10)	/* pages per m88k segment */
#define	M88K_SGPGSHIFT	10	/* number of bits to shift for segment-page */
#define	M88K_ALSEGMS	(1<<10)	/* segments per m88k all space */
#define	M88K_ALSGSHIFT	10	/* number of bits to shift for all-segment */

#define	M88K_SGBYTES	(1<<22)	/* bytes per m88k segments */
#define	M88K_SGSHIFT	22	/* number of bits to shift for segment */
#define	M88K_ALPAGES	(1<<20)	/* pages per m88k all space */
#define	M88K_ALPGSHIFT	20	/* number of bits to shift for all-page */

/*
 *	Convert bytes to pages and convert pages to bytes.
 *	No rounding is used.
 */

#define	m88k_btopr(x)		(((unsigned)(x) + (M88K_PGBYTES - 1)) >> M88K_PGSHIFT)
#define	m88k_btosr(x)		(((unsigned)(x) + (M88K_SGBYTES - 1)) >> M88K_SGSHIFT)
#define	m88k_btos(x)		(((unsigned)(x)) >> M88K_SGSHIFT)
#define	m88k_stob(x)		(((unsigned)(x)) << M88K_SGSHIFT)
#define	m88k_ptosr(x)		(((unsigned)(x) + (M88K_SGPAGES - 1)) >> M88K_SGPGSHIFT)
#define	m88k_ptos(x)		(((unsigned)(x)) >> M88K_SGPGSHIFT)
#define	m88k_stop(x)		(((unsigned)(x)) << M88K_SGPGSHIFT)

/*
 *	Round off or truncate to the nearest page.  These will work
 *	for either addresses or counts.  (i.e. 1 byte rounds to 1 page
 *	bytes.
 */

#define m88k_round_segm(x)	((((unsigned)(x)) + M88K_SGBYTES - 1) & \
					~(M88K_SGBYTES-1))
#define m88k_next_segm(x)	((((unsigned)(x)) & ~(M88K_SGBYTES-1)) + \
					M88K_SGBYTES)
#define m88k_trunc_segm(x)	(((unsigned)(x)) & ~(M88K_SGBYTES-1))

#define m88k_round_seg(x)	((((unsigned)(x)) + M88K_SGBYTES - 1) & \
					~(M88K_SGBYTES-1))
#define m88k_trunc_seg(x)	(((unsigned)(x)) & ~(M88K_SGBYTES-1))

#define	VEQR_ADDR	0x20000000	/* kernel virtual eq phy mapping */
#endif	/*  Do we really need all this stuff */
#endif	/*  Do we really need all this stuf  */
#endif	/*  Do we really need all this stuff */

#endif	_MACHINE_VM_PARAM_
