/*      $OpenBSD: param.h,v 1.13 2006/01/14 11:45:43 miod Exp $ */

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
 *	from: Utah Hdr: machparam.h 1.11 89/08/14
 *	from: @(#)param.h	8.1 (Berkeley) 6/10/93
 */

#ifndef _MIPS_PARAM_H_
#define _MIPS_PARAM_H_

#ifdef _KERNEL
#ifdef _LOCORE
#include <machine/psl.h>
#else
#include <machine/cpu.h>
#endif
#endif

/*
 * Round p (pointer or byte index) up to a correctly-aligned value for all
 * data types (int, long, ...).   The result is u_int and must be cast to
 * any desired pointer type.
 */
#define	ALIGNBYTES	7
#define	ALIGN(p)	(((u_long)(p) + ALIGNBYTES) &~ ALIGNBYTES)
#define ALIGNED_POINTER(p, t)  ((((u_long)(p)) & (sizeof (t) - 1)) == 0)

#define	NBPG		4096		/* bytes/page */
#define	PGOFSET		(NBPG-1)	/* byte offset into page */
#define	PGSHIFT		12		/* LOG2(NBPG) */

#define PAGE_SHIFT	12
#define PAGE_SIZE	(1 << PAGE_SHIFT)
#define PAGE_MASK	(PAGE_SIZE - 1)

#define	NPTEPG		(NBPG/4)

#define NBSEG		0x400000	/* bytes/segment */
#define	SEGOFSET	(NBSEG-1)	/* byte offset into segment */
#define	SEGSHIFT	22		/* LOG2(NBSEG) */

#ifdef __LP64__
#define	KERNBASE	0xffffffff80000000L	/* start of kernel virtual */
#else
#define	KERNBASE	0x80000000	/* start of kernel virtual */
#endif

#define	DEV_BSHIFT	9		/* log2(DEV_BSIZE) */
#define	DEV_BSIZE	(1 << DEV_BSHIFT)
#define BLKDEV_IOSIZE	2048
#define	MAXPHYS		(32 * 1024)	/* max raw I/O transfer size */

#ifdef __LP64__
#define	UPAGES		4		/* pages of u-area */
#else
#define	UPAGES		2		/* pages of u-area */
#endif

#define USPACE		(UPAGES*NBPG)	/* size of u-area in bytes */
#define	USPACE_ALIGN	(2*NBPG)	/* u-area alignment 0-none */

/*
 * Constants related to network buffer management.
 * MCLBYTES must be no larger than PAGE_SIZE (the software page size), and,
 * on machines that exchange pages of input or output buffers with mbuf
 * clusters (MAPPED_MBUFS), MCLBYTES must also be an integral multiple
 * of the hardware page size.
 */
#define	MSIZE		256		/* size of an mbuf */
#define	MCLSHIFT	11
#define	MCLBYTES	(1 << MCLSHIFT)	/* enough for whole Ethernet packet */
#define	MCLOFSET	(MCLBYTES - 1)
#ifndef NMBCLUSTERS
#define	NMBCLUSTERS	4096		/* map size, max cluster allocation */
#endif

#define	MSGBUFSIZE	8192

/* Default malloc arena size */
#define	NKMEMPAGES_MIN_DEFAULT  ((8 * 1024 * 1024) >> PAGE_SHIFT)
#define	NKMEMPAGES_MAX_DEFAULT  ((128 * 1024 * 1024) >> PAGE_SHIFT)

/* pages ("clicks") (4096 bytes) to disk blocks */
#define	ctod(x)	((x) << (PGSHIFT - DEV_BSHIFT))
#define	dtoc(x)	((x) >> (PGSHIFT - DEV_BSHIFT))

/* pages to bytes */
#define	ctob(x)	((x) << PGSHIFT)
#define btoc(x) (((x) + PGOFSET) >> PGSHIFT)

/* bytes to disk blocks */
#define	btodb(x)	((x) >> DEV_BSHIFT)
#define dbtob(x)	((x) << DEV_BSHIFT)

/*
 * Map a ``block device block'' to a file system block.
 * This should be device dependent, and should use the bsize
 * field from the disk label.
 * For now though just use DEV_BSIZE.
 */
#define	bdbtofsb(bn)	((bn) / (BLKDEV_IOSIZE/DEV_BSIZE))

#ifdef _KERNEL
#ifndef _LOCORE

#define	DELAY(n)	delay(n)
void delay(int);
void nanodelay(int);
#endif

#else /* !_KERNEL */
#define	DELAY(n)	{ int N = (n); while (--N > 0); }
#endif /* !_KERNEL */

#endif /* !_MIPS_PARAM_H_ */
