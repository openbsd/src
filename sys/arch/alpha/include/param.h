/*	$NetBSD: param.h,v 1.12 1996/03/04 05:04:10 cgd Exp $	*/

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
 * from: Utah $Hdr: machparam.h 1.11 89/08/14$
 *
 *	@(#)param.h	8.1 (Berkeley) 6/10/93
 */

/*
 * Machine dependent constants for the Alpha.
 */
#define	_MACHINE	alpha
#define	MACHINE		"alpha"
#define	_MACHINE_ARCH	alpha
#define	MACHINE_ARCH	"alpha"
#define	MID_MACHINE	MID_ALPHA

#ifdef _KERNEL				/* XXX */
#include <machine/cpu.h>		/* XXX */
#endif					/* XXX */

/*
 * Round p (pointer or byte index) up to a correctly-aligned value for all
 * data types (int, long, ...).   The result is u_long and must be cast to
 * any desired pointer type.
 */
#define	ALIGNBYTES	7
#define	ALIGN(p)	(((u_long)(p) + ALIGNBYTES) &~ ALIGNBYTES)

#define	NBPG		8192				/* bytes/page */
#define	PGOFSET		(NBPG-1)			/* byte off. into pg */
#define	PGSHIFT		13				/* LOG2(NBPG) */
#define	NPTEPG		(1 << (PGSHIFT-PTESHIFT))	/* pte's/page */

#define	SEGSHIFT	(PGSHIFT + (PGSHIFT-PTESHIFT))	/* LOG2(NBSEG) */
#define	NBSEG		(1 << SEGSHIFT)			/* bytes/segment (8M) */
#define	SEGOFSET	(NBSEG-1)			/* byte off. into seg */

#define	KERNBASE	0xfffffc0000230000	/* start of kernel virtual */
#define	BTOPKERNBASE	((u_long)KERNBASE >> PGSHIFT)

#define	DEV_BSIZE	512
#define	DEV_BSHIFT	9		/* log2(DEV_BSIZE) */
#define	BLKDEV_IOSIZE	2048
#define	MAXPHYS		(64 * 1024)	/* max raw I/O transfer size */

#define	CLSIZE		1
#define	CLSIZELOG2	0

/* NOTE: SSIZE, SINCR and UPAGES must be multiples of CLSIZE */
#define	SSIZE		1		/* initial stack size/NBPG */
#define	SINCR		1		/* increment of stack/NBPG */

#define	UPAGES		2			/* pages of u-area */
#define	USPACE		(UPAGES * NBPG)		/* total size of u-area */

/*
 * Constants related to network buffer management.
 * MCLBYTES must be no larger than CLBYTES (the software page size), and,
 * on machines that exchange pages of input or output buffers with mbuf
 * clusters (MAPPED_MBUFS), MCLBYTES must also be an integral multiple
 * of the hardware page size.
 */
#define	MSIZE		256		/* size of an mbuf */
#define	MCLBYTES	2048		/* large enough for ether MTU */
#define	MCLSHIFT	11
#define	MCLOFSET	(MCLBYTES - 1)
#ifndef NMBCLUSTERS
#ifdef GATEWAY
#define	NMBCLUSTERS	512		/* map size, max cluster allocation */
#else
#define	NMBCLUSTERS	256		/* map size, max cluster allocation */
#endif
#endif

/*
 * Size of kernel malloc arena in CLBYTES-sized logical pages
 */ 
#ifndef NKMEMCLUSTERS
#define	NKMEMCLUSTERS	(4096*1024/CLBYTES)	/* XXX? */
#endif

/* pages ("clicks") to disk blocks */
#define	ctod(x)		((x) << (PGSHIFT - DEV_BSHIFT))
#define	dtoc(x)		((x) >> (PGSHIFT - DEV_BSHIFT))

/* pages to bytes */
#define	ctob(x)		((x) << PGSHIFT)
#define	btoc(x)		(((x) + PGOFSET) >> PGSHIFT)

/* bytes to disk blocks */
#define	btodb(x)	((x) >> DEV_BSHIFT)
#define	dbtob(x)	((x) << DEV_BSHIFT)

/*
 * Map a ``block device block'' to a file system block.
 * This should be device dependent, and should use the bsize
 * field from the disk label.
 * For now though just use DEV_BSIZE.
 */
#define	bdbtofsb(bn)	((bn) / (BLKDEV_IOSIZE/DEV_BSIZE))

/*
 * Mach derived conversion macros
 */
#define	alpha_round_page(x)	((((unsigned long)(x)) + NBPG - 1) & ~(NBPG-1))
#define	alpha_trunc_page(x)	((unsigned long)(x) & ~(NBPG-1))
#define	alpha_btop(x)		((unsigned long)(x) >> PGSHIFT)
#define	alpha_ptob(x)		((unsigned long)(x) << PGSHIFT)

#include <machine/psl.h>

#define	splx(s)                 (s == PSL_IPL_0 ? spl0() : pal_swpipl(s))
#define	splsoft()		pal_swpipl(PSL_IPL_SOFT)
#define	splsoftclock()		splsoft()
#define	splsoftnet()		splsoft()
#define	splnet()                pal_swpipl(PSL_IPL_IO)
#define	splbio()                pal_swpipl(PSL_IPL_IO)
#define	splimp()                pal_swpipl(PSL_IPL_IO)
#define	spltty()                pal_swpipl(PSL_IPL_IO)
#define	splclock()              pal_swpipl(PSL_IPL_CLOCK)
#define	splstatclock()		pal_swpipl(PSL_IPL_CLOCK)
#define	splhigh()               pal_swpipl(PSL_IPL_HIGH)

#ifdef _KERNEL
#ifndef _LOCORE

/* This was calibrated empirically */
extern	u_int64_t cycles_per_usec;
int delay __P((int));
#define	DELAY(n)	delay(n)

int spl0 __P((void));					/* drop ipl to zero */

#endif
#endif /* !_KERNEL */

int prtloc;
extern int ticks;
#define	LOC()	do { if (prtloc) printf("(%ld:%ld) %s: %d\n", curproc ? curproc->p_pid : -1, (long)ticks, __FILE__, __LINE__); } while (0)
#define	PLOC(str) panic("XXX: (%ld:%ld) %s at %s: %d\n", curproc ? curproc->p_pid : -1, (long)ticks, str, __FILE__, __LINE__);
