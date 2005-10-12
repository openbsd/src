/*	$OpenBSD: param.h,v 1.7 2005/10/12 19:05:43 miod Exp $ */
/*
 * Copyright (c) 1999 Steve Murphree, Jr.
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1982, 1986, 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 * from: Utah $Hdr: machparam.h 1.11 89/08/14$
 *
 *	@(#)param.h	7.8 (Berkeley) 6/28/91
 */
#ifndef _M88K_PARAM_H_
#define _M88K_PARAM_H_

#ifdef _KERNEL
#ifndef _LOCORE
#include <machine/cpu.h>
#endif	/* _LOCORE */
#endif

#define  _MACHINE_ARCH  m88k
#define  MACHINE_ARCH   "m88k"
#define  MID_MACHINE    MID_M88K

/*
 * Round p (pointer or byte index) down to a correctly-aligned value
 * for all data types (int, long, ...).   The result is u_int and
 * must be cast to any desired pointer type. ALIGN() is used for
 * aligning stack, which needs to be on a double word boundary for
 * 88k.
 */

#define  ALIGNBYTES		15		/* 64 bit alignment */
#define  ALIGN(p)		(((u_int)(p) + ALIGNBYTES) & ~ALIGNBYTES)
#define  ALIGNED_POINTER(p,t)	((((u_long)(p)) & (sizeof(t)-1)) == 0)

#define NBPG		(1 << PGSHIFT)	/* bytes/page */
#define PGOFSET		(NBPG-1)	/* byte offset into page */
#define PGSHIFT		12		/* LOG2(NBPG) */

#define	PAGE_SHIFT	12
#define	PAGE_SIZE	(1 << PAGE_SHIFT)
#define	PAGE_MASK	(PAGE_SIZE - 1)

#define NPTEPG		(PAGE_SIZE / (sizeof(pt_entry_t)))

#define DEV_BSHIFT	9		/* log2(DEV_BSIZE) */
#define DEV_BSIZE	(1 << DEV_BSHIFT)
#define BLKDEV_IOSIZE	2048
#define MAXPHYS		(64 * 1024)	/* max raw I/O transfer size */

#define UPAGES		2		/* pages of u-area */
#define USPACE		(UPAGES * NBPG)
#define	USPACE_ALIGN	(0)		/* u-area alignment 0-none */

/*
 * Constants related to network buffer management.
 * MCLBYTES must be no larger than the software page size, and,
 * on machines that exchange pages of input or output buffers with mbuf
 * clusters (MAPPED_MBUFS), MCLBYTES must also be an integral multiple
 * of the hardware page size.
 */
#define MSIZE		256		/* size of an mbuf */
#define MCLSHIFT	11		/* convert bytes to m_buf clusters */
#define MCLBYTES	(1 << MCLSHIFT)	/* size of a m_buf cluster */
#define MCLOFSET	(MCLBYTES - 1)	/* offset within a m_buf cluster */

#define NMBCLUSTERS	2048		/* map size, max cluster allocation */

/*
 * Minimum and maximum sizes of the kernel malloc arena in PAGE_SIZE-sized
 * logical pages.
 */
#define	NKMEMPAGES_MIN_DEFAULT	((4 * 1024 * 1024) >> PAGE_SHIFT)
#define	NKMEMPAGES_MAX_DEFAULT	((64 * 1024 * 1024) >> PAGE_SHIFT)

#define MSGBUFSIZE	PAGE_SIZE

/* pages ("clicks") to disk blocks */
#define ctod(x)			((x) << (PGSHIFT - DEV_BSHIFT))
#define dtoc(x)			((x) >> (PGSHIFT - DEV_BSHIFT))

/* pages to bytes */
#define ctob(x)			((x) << PGSHIFT)
#define btoc(x)			(((x) + PGOFSET) >> PGSHIFT)

/* bytes to disk blocks */
#define btodb(x)		((x) >> DEV_BSHIFT)
#define dbtob(x)		((x) << DEV_BSHIFT)

/*
 * Map a ``block device block'' to a file system block.
 * This should be device dependent, and should use the bsize
 * field from the disk label.
 * For now though just use DEV_BSIZE.
 */
#define bdbtofsb(bn)		((bn) / (BLKDEV_IOSIZE/DEV_BSIZE))

/*
 * Get interrupt glue.
 */
#include <machine/intr.h>

#ifdef   _KERNEL
extern void delay(int);
#define  DELAY(x)             delay(x)

extern int cputyp;
#endif

/*
 * Values for the cputyp variable.
 */
#define CPU_88100	0x100
#define CPU_88110	0x110

#ifdef M88100
#ifdef M88110
#define	CPU_IS88100	(cputyp == CPU_88100)
#define	CPU_IS88110	(cputyp != CPU_88100)
#else
#define	CPU_IS88100	1
#define	CPU_IS88110	0
#endif
#else
#define	CPU_IS88100	0
#define	CPU_IS88110	1
#endif

#endif /* !_M88K_PARAM_H_ */
