/*	$OpenBSD: param.h,v 1.6 2011/03/23 16:54:37 pirofti Exp $	*/
/*	$NetBSD: param.h,v 1.15 2006/08/28 13:43:35 yamt Exp $	*/

/*-
 * Copyright (c) 2002 The NetBSD Foundation, Inc. All rights reserved.
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
 *	@(#)param.h	5.8 (Berkeley) 6/28/91
 */

/*
 * SuperH dependent constants.
 */

#ifndef _SH_PARAM_H_
#define	_SH_PARAM_H_

#define	_MACHINE_ARCH	sh
#define	MACHINE_ARCH	"sh"

#ifndef	MID_MACHINE
#define	MID_MACHINE	MID_SH3
#endif

#if defined(_KERNEL) && !defined(_LOCORE)
#include <sh/cpu.h>
#endif

/*
 * We use 4K pages on the sh3/sh4.  Override the PAGE_* definitions
 * to be compile-time constants.
 */
#define	PAGE_SHIFT		12
#define	PAGE_SIZE		(1 << PAGE_SHIFT)
#define	PAGE_MASK		(PAGE_SIZE - 1)

#define	PGSHIFT			PAGE_SHIFT
#define	NBPG			PAGE_SIZE
#define	PGOFSET			PAGE_MASK

/*
 * Round p (pointer or byte index) up to a correctly-aligned value
 * for all data types (int, long, ...).   The result is u_int and
 * must be cast to any desired pointer type.
 *
 * ALIGNED_POINTER is a boolean macro that checks whether an address
 * is valid to fetch data elements of type t from on this architecture.
 * This does not reflect the optimal alignment, just the possibility
 * (within reasonable limits).
 *
 */
#define	ALIGNBYTES		(sizeof(int) - 1)
#define	ALIGN(p)		(((u_int)(p) + ALIGNBYTES) & ~ALIGNBYTES)
#define	ALIGNED_POINTER(p, t)	((((u_long)(p)) & (sizeof(t) - 1)) == 0)

#define	DEV_BSHIFT	9		/* log2(DEV_BSIZE) */
#define	DEV_BSIZE	(1 << DEV_BSHIFT)
#define	BLKDEV_IOSIZE	2048
#define	MAXPHYS		(64 * 1024)	/* max raw I/O transfer size */

/*
 * u-space.
 */
#define	UPAGES		3		/* pages of u-area */
#define	USPACE		(UPAGES * NBPG)	/* total size of u-area */
#define	USPACE_ALIGN	(0)
#if UPAGES == 1
#error "too small u-area"
#elif UPAGES == 2
#define	P1_STACK	/* kernel stack is P1-area */
#else
#undef	P1_STACK	/* kernel stack is P3-area */
#endif

#ifndef MSGBUFSIZE
#define	MSGBUFSIZE	NBPG		/* default message buffer size */
#endif

/* pages to disk blocks */
#define	ctod(x)		((x) << (PAGE_SHIFT - DEV_BSHIFT))
#define	dtoc(x)		((x) >> (PAGE_SHIFT - DEV_BSHIFT))

/* bytes to disk blocks */
#define	btodb(x)	((x) >> DEV_BSHIFT)
#define	dbtob(x)	((x) << DEV_BSHIFT)

/*
 * Constants related to network buffer management.
 */
#define	NMBCLUSTERS	4096		/* map size, max cluster allocation */

/*
 * Minimum and maximum sizes of the kernel malloc arena in PAGE_SIZE-sized
 * logical pages.
 */
#define	NKMEMPAGES_MIN_DEFAULT	((4 * 1024 * 1024) >> PAGE_SHIFT)
#define	NKMEMPAGES_MAX_DEFAULT	((64 * 1024 * 1024) >> PAGE_SHIFT)

#endif /* !_SH_PARAM_H_ */
