/*	$NetBSD: param.h,v 1.17 1996/05/20 10:50:52 jonathan Exp $	*/

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
 * from: Utah Hdr: machparam.h 1.11 89/08/14
 *
 *	@(#)param.h	8.1 (Berkeley) 6/10/93
 */

/*
 * Machine-dependent constants (VM, etc) common across MIPS cpus
 */

#include <pmax/mips_param.h>

/*
 * Machine dependent constants for DEC Station 3100.
 */

#define	_MACHINE	pmax
#define	MACHINE		"pmax"
#define	_MACHINE_ARCH	mips
#define	MACHINE_ARCH	"mips"
#define	MID_MACHINE	MID_PMAX

#define	KERNBASE	0x80000000	/* start of kernel virtual */
#define KERNTEXTOFF	0x80030000	/* start of kernel text for kvm_mkdb */
#define	BTOPKERNBASE	((u_long)KERNBASE >> PGSHIFT)

#define	DEV_BSIZE	512
#define	DEV_BSHIFT	9		/* log2(DEV_BSIZE) */
#define BLKDEV_IOSIZE	2048
#define	MAXPHYS		(64 * 1024)	/* max raw I/O transfer size */

#define	CLSIZE		1
#define	CLSIZELOG2	0

/* NOTE: SSIZE, SINCR and UPAGES must be multiples of CLSIZE */
#define	SSIZE		1		/* initial stack size/NBPG */
#define	SINCR		1		/* increment of stack/NBPG */

#define	UPAGES		2		/* pages of u-area */
#define	UADDR		0xffffd000	/* address of u */
#define USPACE          (UPAGES*NBPG)   /* size of u-area in bytes */
#define	UVPN		(UADDR>>PGSHIFT)/* virtual page number of u */
#define	KERNELSTACK	(UADDR+UPAGES*NBPG)	/* top of kernel stack */

/*
 * Constants related to network buffer management.
 * MCLBYTES must be no larger than CLBYTES (the software page size), and,
 * on machines that exchange pages of input or output buffers with mbuf
 * clusters (MAPPED_MBUFS), MCLBYTES must also be an integral multiple
 * of the hardware page size.
 */
#define	MSIZE		128		/* size of an mbuf */
#define	MCLSHIFT	11
#define	MCLBYTES	(1 << MCLSHIFT)	/* enough for whole Ethernet packet */
#define	MCLOFSET	(MCLBYTES - 1)
#ifndef NMBCLUSTERS
#ifdef GATEWAY
#define	NMBCLUSTERS	2048		/* map size, max cluster allocation */
#else
#define	NMBCLUSTERS	1024		/* map size, max cluster allocation */
#endif
#endif

#ifdef _KERNEL
#ifndef _LOCORE
extern int cpuspeed;
#define	DELAY(n)	{ register int N = cpuspeed * (n); while (--N > 0); }
#endif
#endif /* !_KERNEL */
