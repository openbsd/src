/*	$OpenBSD: param.h,v 1.10 2005/12/13 00:18:19 jsg Exp $	*/

/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
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
 *	@(#)param.h     5.8 (Berkeley) 6/28/91
 */

#ifdef _KERNEL
#ifdef _LOCORE
#include <machine/psl.h>
#else
#include <machine/cpu.h>
#endif
#endif

#define	_MACHINE	amd64
#define	MACHINE		"amd64"
#define	_MACHINE_ARCH	amd64
#define	MACHINE_ARCH	"amd64"
#define MID_MACHINE	MID_AMD64

/*
 * Round p (pointer or byte index) up to a correctly-aligned value
 * for all data types (int, long, ...).   The result is u_long and
 * must be cast to any desired pointer type.
 *
 * ALIGNED_POINTER is a boolean macro that checks whether an address
 * is valid to fetch data elements of type t from on this architecture.
 * This does not reflect the optimal alignment, just the possibility
 * (within reasonable limits). 
 *
 */
#define ALIGNBYTES		(sizeof(long) - 1)
#define ALIGN(p)		(((u_long)(p) + ALIGNBYTES) &~ALIGNBYTES)
#define ALIGNED_POINTER(p,t)	1

#define	PGSHIFT		12		/* LOG2(NBPG) */
#define	NBPG		(1 << PGSHIFT)	/* bytes/page */
#define	PGOFSET		(NBPG-1)	/* byte offset into page */

#define PAGE_SHIFT	12
#define PAGE_SIZE	(1 << PAGE_SHIFT)
#define PAGE_MASK	(PAGE_SIZE - 1)

#define	NPTEPG		(NBPG/(sizeof (pt_entry_t)))

#define	KERNBASE	0xffffffff80000000 /* start of kernel virtual space */
#define KERNTEXTOFF	(KERNBASE+0x100000)	/* start of kernel text */
#define	BTOPKERNBASE	((u_long)KERNBASE >> PGSHIFT)

#define KERNTEXTOFF_HI	0xffffffff
#define KERNTEXTOFF_LO	0x80100000

#define KERNBASE_HI	0xffffffff
#define KERNBASE_LO	0x80000000

#define	DEV_BSHIFT	9		/* log2(DEV_BSIZE) */
#define	DEV_BSIZE	(1 << DEV_BSHIFT)
#define	BLKDEV_IOSIZE	2048
#ifndef	MAXPHYS
#define	MAXPHYS		(64 * 1024)	/* max raw I/O transfer size */
#endif

#define	UPAGES		5		/* pages of u-area */
#define	USPACE		(UPAGES * NBPG)	/* total size of u-area */
#define	USPACE_ALIGN	(0)		/* u-area alignment 0-none */

#ifndef MSGBUFSIZE
#define MSGBUFSIZE	4*NBPG		/* default message buffer size */
#endif

/*
 * Constants related to network buffer management.
 * MCLBYTES must be no larger than NBPG (the software page size), and,
 * on machines that exchange pages of input or output buffers with mbuf
 * clusters (MAPPED_MBUFS), MCLBYTES must also be an integral multiple
 * of the hardware page size.
 */
#define	MSIZE		256		/* size of an mbuf */
#define	MCLSHIFT	11		/* convert bytes to m_buf clusters */
#define	MCLBYTES	(1 << MCLSHIFT)	/* size of a m_buf cluster */
#define MCLOFSET	(MCLBYTES - 1)	/* offset within a m_buf cluster */
#define	NMBCLUSTERS	6144		/* map size, max cluster allocation */

/*
 * Minimum and maximum sizes of the kernel malloc arena in PAGE_SIZE-sized
 * logical pages.
 */
#define	NKMEMPAGES_MIN_DEFAULT	((8 * 1024 * 1024) >> PAGE_SHIFT)
#define	NKMEMPAGES_MAX_DEFAULT	((128 * 1024 * 1024) >> PAGE_SHIFT)

/* pages ("clicks") to disk blocks */
#define	ctod(x)		((x) << (PGSHIFT - DEV_BSHIFT))
#define	dtoc(x)		((x) >> (PGSHIFT - DEV_BSHIFT))

/* bytes to pages */
#define	ctob(x)		((x) << PGSHIFT)
#define	btoc(x)		(((x) + PGOFSET) >> PGSHIFT)

/* bytes to disk blocks */
#define	dbtob(x)	((x) << DEV_BSHIFT)
#define	btodb(x)	((x) >> DEV_BSHIFT)

/*
 * Map a ``block device block'' to a file system block.
 * This should be device dependent, and should use the bsize
 * field from the disk label.
 * For now though just use DEV_BSIZE.
 */
#define	bdbtofsb(bn)	((bn) / (BLKDEV_IOSIZE / DEV_BSIZE))

/*
 * XXXfvdl the PD* stuff is different from i386.
 */
/*
 * Mach derived conversion macros
 */
#define	x86_round_pdr(x) \
	((((unsigned long)(x)) + (NBPD_L2 - 1)) & ~(NBPD_L2 - 1))
#define	x86_trunc_pdr(x)	((unsigned long)(x) & ~(NBPD_L2 - 1))
