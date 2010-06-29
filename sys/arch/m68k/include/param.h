/*	$OpenBSD: param.h,v 1.24 2010/06/29 20:30:32 guenther Exp $	*/
/*	$NetBSD: param.h,v 1.2 1997/06/10 18:21:23 veego Exp $	*/

/*
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
 * from: Utah $Hdr: machparam.h 1.16 92/12/20$
 *
 *	@(#)param.h	8.1 (Berkeley) 6/10/93
 */
#ifndef _M68K_PARAM_H_
#define _M68K_PARAM_H_

/*
 * Machine independent constants for m68k
 */
#define	_MACHINE_ARCH	m68k
#define	MACHINE_ARCH	"m68k"
#define	MID_MACHINE	MID_M68K

/*
 * Round p (pointer or byte index) up to a correctly-aligned value for all
 * data types (int, long, ...).   The result is u_int and must be cast to
 * any desired pointer type.
 *
 * ALIGNED_POINTER is a boolean macro that checks whether an address
 * is valid to fetch data elements of type t from on this architecture.
 * This does not reflect the optimal alignment, just the possibility
 * (within reasonable limits). 
 *
 */
#define ALIGNBYTES		(sizeof(int) - 1)
#define	ALIGN(p)		(((u_int)(p) + ALIGNBYTES) &~ ALIGNBYTES)
#define ALIGNED_POINTER(p,t)	((((u_long)(p)) & (sizeof(t) - 1)) == 0)

#define	PAGE_SIZE	(1 << PAGE_SHIFT)
#define	PAGE_MASK	(PAGE_SIZE - 1)

#define	PGSHIFT		PAGE_SHIFT
#define	NBPG		(1 << PGSHIFT)	/* bytes/page */
#define	PGOFSET		(NBPG-1)	/* byte offset into page */

#define	NPTEPG		(PAGE_SIZE / (sizeof(pt_entry_t)))

#define	BTOPKERNBASE	((u_long)KERNBASE >> PAGE_SHIFT)

#define	DEV_BSHIFT	9		/* log2(DEV_BSIZE) */
#define	DEV_BSIZE	(1 << DEV_BSHIFT)
#define BLKDEV_IOSIZE	2048
#define	MAXPHYS		(64 * 1024)	/* max raw I/O transfer size */

#define	SEGSHIFT020	(34 - PAGE_SHIFT)
#define	SEGSHIFT040	(18)
#ifndef	SEGSHIFT
#if defined(M68040) || defined(M68060)
#if defined(M68020) || defined(M68030)
#define	SEGSHIFT	((mmutype <= MMU_68040) ? SEGSHIFT040 : SEGSHIFT020)
#else
#define	SEGSHIFT	SEGSHIFT040
#endif
#else
#define	SEGSHIFT	SEGSHIFT020
#endif
#define	NBSEG		(1 << SEGSHIFT)
#define	SEGOFSET	(NBSEG - 1)
#endif

/* mac68k use 3 pages of u-area */
#ifndef	UPAGES
#define UPAGES		2		/* pages of u-area */
#endif
#define	USPACE		(UPAGES * PAGE_SIZE)
#define	USPACE_ALIGN	(0)		/* u-area alignment 0-none */

/*
 * Minimum and maximum sizes of the kernel malloc arena in PAGE_SIZE-sized
 * logical pages.
 */
#define	NKMEMPAGES_MIN_DEFAULT	((4 * 1024 * 1024) >> PAGE_SHIFT)
#define	NKMEMPAGES_MAX_DEFAULT	((64 * 1024 * 1024) >> PAGE_SHIFT)

/*
 * Constants related to network buffer management.
 */
#define	NMBCLUSTERS	1024		/* map size, max cluster allocation */

/* pages ("clicks") to disk blocks */
#define	ctod(x)		((x) << (PAGE_SHIFT - DEV_BSHIFT))
#define	dtoc(x)		((x) >> (PAGE_SHIFT - DEV_BSHIFT))

/* bytes to disk blocks */
#define	btodb(x)	((x) >> DEV_BSHIFT)
#define	dbtob(x)	((x) << DEV_BSHIFT)

/*
 * Mach derived conversion macros
 */
#define	m68k_round_seg(x)	((((unsigned)(x)) + SEGOFSET) & ~SEGOFSET)
#define	m68k_trunc_seg(x)	((unsigned)(x) & ~SEGOFSET)
#define	m68k_page_offset(x)	((unsigned)(x) & PGOFSET)

#include <machine/cpu.h>

#endif	/* !_M68K_PARAM_H_ */
