/*      $OpenBSD: param.h,v 1.11 1998/08/30 22:05:35 millert Exp $ */

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
 *	from: Utah Hdr: machparam.h 1.11 89/08/14
 *	from: @(#)param.h	8.1 (Berkeley) 6/10/93
 */

#ifndef _MACHINE_PARAM_H_
#define _MACHINE_PARAM_H_

#ifdef _KERNEL    
#ifdef _LOCORE
#include <machine/psl.h>                                             
#else                                                                
#include <machine/cpu.h>
#endif                                                              
#endif                                                             

/*
 * Machine dependent constants for ARC BIOS MIPS machines:
 *	Acer Labs PICA_61
 *	Deskstation rPC44
 *	Deskstation Tyne
 *	Etc
 */
#define	MACHINE		"arc"
#define	_MACHINE	arc
#define MACHINE_ARCH	"mips"
#define _MACHINE_ARCH	mips

#define MID_MACHINE	0	/* None but has to be defined */

/*
 * Round p (pointer or byte index) up to a correctly-aligned value for all
 * data types (int, long, ...).   The result is u_int and must be cast to
 * any desired pointer type.
 */
#define	ALIGNBYTES	7
#define	ALIGN(p)	(((u_int)(p) + ALIGNBYTES) &~ ALIGNBYTES)

#define	NBPG		4096		/* bytes/page */
#define	PGOFSET		(NBPG-1)	/* byte offset into page */
#define	PGSHIFT		12		/* LOG2(NBPG) */
#define	NPTEPG		(NBPG/4)

#define NBSEG		0x400000	/* bytes/segment */
#define	SEGOFSET	(NBSEG-1)	/* byte offset into segment */
#define	SEGSHIFT	22		/* LOG2(NBSEG) */

#define	KERNBASE	0x80000000	/* start of kernel virtual */
#define	BTOPKERNBASE	((u_long)KERNBASE >> PGSHIFT)

#define	DEV_BSIZE	512
#define	DEV_BSHIFT	9		/* log2(DEV_BSIZE) */
#define BLKDEV_IOSIZE	2048
/* XXX Maxphys temporary changed to 32K while SCSI driver is fixed. */
#define	MAXPHYS		(32 * 1024)	/* max raw I/O transfer size */

#define	CLSIZE		1
#define	CLSIZELOG2	0

/* NOTE: SSIZE, SINCR and UPAGES must be multiples of CLSIZE */
#define	SSIZE		1		/* initial stack size/NBPG */
#define	SINCR		1		/* increment of stack/NBPG */

#define	UPAGES		2		/* pages of u-area */
#if defined(_LOCORE) && defined(notyet)
#define	UADDR		0xffffffffffffc000	/* address of u */
#else
#define	UADDR		0xffffc000	/* address of u */
#endif
#define USPACE		(UPAGES*NBPG)	/* size of u-area in bytes */
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

/*
 * Size of kernel malloc arena in CLBYTES-sized logical pages
 */ 
#ifndef NKMEMCLUSTERS
#define	NKMEMCLUSTERS	(4096*1024/CLBYTES)
#endif

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

/*
 * Conversion macros
 */
#define mips_round_page(x)	((((unsigned)(x)) + NBPG - 1) & ~(NBPG-1))
#define mips_trunc_page(x)	((unsigned)(x) & ~(NBPG-1))
#define mips_btop(x)		((unsigned)(x) >> PGSHIFT)
#define mips_ptob(x)		((unsigned)(x) << PGSHIFT)

#ifdef _KERNEL
#ifndef _LOCORE
extern int (*Mips_splnet)(void), (*Mips_splbio)(void), (*Mips_splimp)(void),
	   (*Mips_spltty)(void), (*Mips_splclock)(void), (*Mips_splstatclock)(void);
#define	splnet()	((*Mips_splnet)())
#define	splbio()	((*Mips_splbio)())
#define	splimp()	((*Mips_splimp)())
#define	spltty()	((*Mips_spltty)())
#define	splclock()	((*Mips_splclock)())
#define	splstatclock()	((*Mips_splstatclock)())

int splhigh __P((void));
int splx __P((int));
int spl0 __P((void));

/*
 *   Delay is based on an assumtion that each time in the loop
 *   takes 3 clocks. Three is for branch and subtract in the delay slot.
 */
extern	int cpuspeed;
#define	DELAY(n)	{ register int N = cpuspeed * (n); while ((N -= 3) > 0); }
void delay __P((int));
#endif

#else /* !_KERNEL */
#define	DELAY(n)	{ register int N = (n); while (--N > 0); }
#endif /* !_KERNEL */

#endif /* _MACHINE_PARAM_H_ */
