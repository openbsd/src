/*	$NetBSD: vmparam.h,v 1.2 1995/11/23 02:36:46 cgd Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department and Ralph Campbell.
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
 * from: Utah $Hdr: vmparam.h 1.16 91/01/18$
 *
 *	@(#)vmparam.h	8.2 (Berkeley) 4/22/94
 */

/*
 * Machine dependent constants for Alpha.
 */
/*
 * USRTEXT is the start of the user text/data space, while USRSTACK
 * is the top (end) of the user stack.  Immediately above the user stack
 * resides the user structure, which is UPAGES long and contains the
 * kernel stack.
 */
#define	USRTEXT		CLBYTES
#define	USRSTACK	VM_MAXUSER_ADDRESS

/*
 * Virtual memory related constants, all in bytes
 */
#ifndef MAXTSIZ
#define	MAXTSIZ		(1<<30)			/* max text size (1G) */
#endif
#ifndef DFLDSIZ
#define	DFLDSIZ		(1<<27)			/* initial data size (128M) */
#endif
#ifndef MAXDSIZ
#define	MAXDSIZ		(1<<30)			/* max data size (1G) */
#endif
#ifndef	DFLSSIZ
#define	DFLSSIZ		(1<<21)			/* initial stack size (2M) */
#endif
#ifndef	MAXSSIZ
#define	MAXSSIZ		(1<<25)			/* max stack size (32M) */
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

#ifdef THESE_ARE_WRONG
/*
 * Sizes of the system and user portions of the system page table.
 */
/* SYSPTSIZE IS SILLY; (really number of buffers for I/O) */
#define	SYSPTSIZE	1228
#define	USRPTSIZE 	1024
#endif /* WRONG */

/*
 * PTEs for mapping user space into the kernel for phyio operations.
 * 64 pte's are enough to cover 8 disks * MAXBSIZE.
 */
#ifndef USRIOSIZE
#define USRIOSIZE	64
#endif

/*
 * PTEs for system V style shared memory.
 * This is basically slop for kmempt which we actually allocate (malloc) from.
 */
#ifndef SHMMAXPGS
#define SHMMAXPGS	1024		/* 8mb */
#endif

/*
 * Boundary at which to place first MAPMEM segment if not explicitly
 * specified.  Should be a power of two.  This allows some slop for
 * the data segment to grow underneath the first mapped segment.
 */
#define MMSEG		0x200000

/*
 * The size of the clock loop.
 */
#define	LOOPPAGES	(maxfree - firstfree)

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

#ifdef THESE_ARE_WRONG
#define	mapin(pte, v, pfnum, prot) \
	(*(int *)(pte) = ((pfnum) << PG_SHIFT) | (prot), MachTLBFlushAddr(v))
#endif /* WRONG */

/*
 * Mach derived constants
 */

/* user/kernel map constants */
#define VM_MIN_ADDRESS		((vm_offset_t)0x0000000000000000) /* 0 */
#define VM_MAXUSER_ADDRESS	((vm_offset_t)0x0000000200000000) /* 8G */
#define VM_MAX_ADDRESS		VM_MAXUSER_ADDRESS
#define VM_MIN_KERNEL_ADDRESS	((vm_offset_t)0xfffffe0000000000)
#define VM_MAX_KERNEL_ADDRESS	((vm_offset_t)0xffffffffffffffff)

/* virtual sizes (bytes) for various kernel submaps */
#define VM_MBUF_SIZE		(NMBCLUSTERS*MCLBYTES)
#define VM_KMEM_SIZE		(NKMEMCLUSTERS*CLBYTES)
#define VM_PHYS_SIZE		(USRIOSIZE*CLBYTES)

/* some Alpha-specific constants */
#define	VPTBASE		((vm_offset_t)0xfffffffc00000000) /* Virt. pg table */

#ifdef THESE_ARE_WRONG
/* pcb base */
#define	pcbb(p)		((u_int)(p)->p_addr)
#endif /* WRONG */
