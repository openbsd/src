/*	$OpenBSD: param.h,v 1.5 1996/03/30 22:18:27 niklas Exp $	*/
/*	$NetBSD: param.h,v 1.28 1996/03/04 05:04:14 cgd Exp $	*/

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
 *	@(#)param.h	7.8 (Berkeley) 6/28/91
 */
#ifndef _MACHINE_PARAM_H_
#define _MACHINE_PARAM_H_

/*
 * Machine dependent constants for amiga
 */
#define	_MACHINE	amiga
#define	MACHINE		"amiga"
#define	_MACHINE_ARCH	m68k
#define	MACHINE_ARCH	"m68k"
#define	MID_MACHINE	MID_M68K

/*
 * Round p (pointer or byte index) up to a correctly-aligned value
 * for all data types (int, long, ...).   The result is u_int and
 * must be cast to any desired pointer type.
 */
#define ALIGNBYTES	(sizeof(int) - 1)
#define	ALIGN(p)	(((u_int)(p) + (sizeof(int) - 1)) &~ (sizeof(int) - 1))

#define	NBPG		8192		/* bytes/page */
#define	PGOFSET		(NBPG-1)	/* byte offset into page */
#define	PGSHIFT		13		/* LOG2(NBPG) */
#define	NPTEPG		(NBPG/(sizeof(u_int)))

#define NBSEG		((mmutype == MMU_68040) ? 32*NBPG : 2048*NBPG)	/* bytes/segment */
#define	SEGOFSET	(NBSEG-1)	/* byte offset into segment */
#define	SEGSHIFT	24		/* LOG2(NBSEG) [68030 value] */

#define	KERNBASE	0x0		/* start of kernel virtual */
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
#define USPACE		(UPAGES * NBPG)

/*
 * Constants related to network buffer management.
 * MCLBYTES must be no larger than CLBYTES (the software page size), and,
 * on machines that exchange pages of input or output buffers with mbuf
 * clusters (MAPPED_MBUFS), MCLBYTES must also be an integral multiple
 * of the hardware page size.
 */
#define	MSIZE		128		/* size of an mbuf */
#define	MCLBYTES	1024
#define	MCLSHIFT	10
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
#define	NKMEMCLUSTERS	(3072*1024/CLBYTES)
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
#define amiga_round_seg(x)	((((unsigned)(x)) + NBSEG - 1) & ~(NBSEG-1))
#define amiga_trunc_seg(x)	((unsigned)(x) & ~(NBSEG-1))
#define amiga_round_page(x)	((((unsigned)(x)) + NBPG - 1) & ~(NBPG-1))
#define amiga_trunc_page(x)	((unsigned)(x) & ~(NBPG-1))
#define amiga_btop(x)		((unsigned)(x) >> PGSHIFT)
#define amiga_ptob(x)		((unsigned)(x) << PGSHIFT)

/*
 * spl functions; all are normally done in-line
 */
#include <machine/psl.h>

#ifdef _KERNEL

static __inline int
splraise(npsl)
	register int npsl;
{
        register int opsl;

        __asm __volatile ("clrl %0; movew sr,%0; movew %1,sr" : "&=d" (opsl) :
	    "di" (npsl));
        return opsl;
}

#ifdef IPL_REMAP_1

extern int isr_exter_ipl;
extern void walk_ipls __P((int, int));

static __inline int
splx(npsl)
	register int npsl;
{
        register int opsl;

        __asm __volatile ("clrl %0; movew sr,%0" : "=d" (opsl));
	if ((isr_exter_ipl << 8) > (npsl & PSL_IPL))
		walk_ipls(isr_exter_ipl, npsl);
        __asm __volatile("movew %0,sr" : : "di" (npsl));
        return opsl;
}
#endif

#ifndef IPL_REMAP_2
#define splx splraise
#else

extern int walk_ipls __P((int));

static __inline int
splx(npsl)
	register int npsl;
{
        register int opsl;

	/* We should maybe have a flag telling if this is needed.  */
	opsl = walk_ipls(npsl);
        __asm __volatile("movew %0,sr" : : "di" (npsl));
        return opsl;
}

#endif

/*
 * Shortcuts
 */
#define spl1()	splraise(PSL_S|PSL_IPL1)
#define spl2()	splraise(PSL_S|PSL_IPL2)
#define spl3()	splraise(PSL_S|PSL_IPL3)
#define spl4()	splraise(PSL_S|PSL_IPL4)
#define spl5()	splraise(PSL_S|PSL_IPL5)
#define spl6()	splraise(PSL_S|PSL_IPL6)
#define spl7()  splraise(PSL_S|PSL_IPL7)

/*
 * Hardware interrupt masks
 */
#define splbio()	spl3()
#define splnet()	spl3()
#define spltty()	spl4()
#define splimp()	spl4()
#if defined(LEV6_DEFER) || defined(IPL_REMAP_1) || defined(IPL_REMAP_2)
#define splclock()	spl4()
#else
#define splclock()	spl6()
#endif
#define splstatclock()	splclock()

/*
 * Software interrupt masks
 *
 * NOTE: splsoftclock() is used by hardclock() to lower the priority from
 * clock to softclock before it calls softclock().
 */
#define splsoftclock()	splx(PSL_S|PSL_IPL1)
#define splsoftnet()	spl1()
#define splsofttty()	spl1()

/*
 * Miscellaneous
 */
#if defined(LEV6_DEFER) || defined(IPL_REMAP_1) || defined(IPL_REMAP_2)
#define splhigh()	spl4()
#else
#define splhigh()	spl7()
#endif
#define spl0()	splx(PSL_S|PSL_IPL0)

void delay __P((unsigned));
void DELAY __P((unsigned));
#endif

#endif /* !_MACHINE_PARAM_H_ */
