/*	$OpenBSD: param.h,v 1.18 2000/03/02 23:01:46 todd Exp $	*/
/*	$NetBSD: param.h,v 1.34 1996/03/04 05:04:40 cgd Exp $	*/

/*
 * Copyright (c) 1994, 1995 Gordon W. Ross
 * Copyright (c) 1993 Adam Glass
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1982, 1986, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	from: Utah Hdr: machparam.h 1.16 92/12/20
 *	from: @(#)param.h	8.1 (Berkeley) 6/10/93
 */

#ifndef	MACHINE

/*
 * Machine dependent constants for the Sun3 series.
 */
#define	_MACHINE	sun3
#define	MACHINE		"sun3"

/*
 * Round p (pointer or byte index) up to a correctly-aligned value
 * for all data types (int, long, ...).   The result is u_int and
 * must be cast to any desired pointer type.
 */

#define	PGSHIFT		13		/* LOG2(NBPG) */

#define NBSG		0x20000	/* bytes/segment */
#define	SEGOFSET	(NBSG-1)	/* byte offset into segment */
#define SEGSHIFT	17	        /* LOG2(NBSG) */

#define	KERNBASE	0x0E000000	/* start of kernel virtual */
#define	KERNTEXTOFF	0x0E004000	/* start of kernel text */

#include <m68k/param.h>

#define MAXBSIZE 0x8000		/* XXX temp until sun3 dma chaining */

/*
 * Constants related to network buffer management.
 * MCLBYTES must be no larger than CLBYTES (the software page size), and,
 * on machines that exchange pages of input or output buffers with mbuf
 * clusters (MAPPED_MBUFS), MCLBYTES must also be an integral multiple
 * of the hardware page size.
 */
#define	MSIZE		128		/* size of an mbuf */
#define	MCLSHIFT	11
#define	MCLBYTES	(1 << MCLSHIFT)	/* large enough for ether MTU */
#define	MCLOFSET	(MCLBYTES - 1)
#ifndef NMBCLUSTERS
#ifdef GATEWAY
#define	NMBCLUSTERS	512		/* map size, max cluster allocation */
#else
#define	NMBCLUSTERS	256		/* map size, max cluster allocation */
#endif
#endif

#define MSGBUFSIZE	NBPG

/*
 * Size of kernel malloc arena in CLBYTES-sized logical pages
 */ 
#ifndef NKMEMCLUSTERS
#define	NKMEMCLUSTERS	(2048*1024/CLBYTES)
#endif

/*
 * spl functions; all are done in-line
 */

#include <machine/psl.h>

#define _spl(s) \
({ \
	register int _spl_r; \
\
	__asm __volatile ("clrl %0; movew sr,%0; movew %1,sr" : \
		"=&d" (_spl_r) : "di" (s)); \
	_spl_r; \
})

/*
 * The rest of this is sun3 specific, because other ports may
 * need to do special things in spl0() (i.e. simulate SIR).
 * Suns have a REAL interrupt register, so spl0() and splx(s)
 * have no need to check for any simulated interrupts, etc.
 */

#define spl0()  _spl(PSL_S|PSL_IPL0)
#define spl1()  _spl(PSL_S|PSL_IPL1)
#define spl2()  _spl(PSL_S|PSL_IPL2)
#define spl3()  _spl(PSL_S|PSL_IPL3)
#define spl4()  _spl(PSL_S|PSL_IPL4)
#define spl5()  _spl(PSL_S|PSL_IPL5)
#define spl6()  _spl(PSL_S|PSL_IPL6)
#define spl7()  _spl(PSL_S|PSL_IPL7)
#define splx(x)  _spl(x)

/* IPL used by soft interrupts: netintr(), softclock() */
#define splsoftclock()  spl1()
#define splsoftnet()    spl1()

/* Highest block device (strategy) IPL. */
#define splbio()        spl2()

/* Highest network interface IPL. */
#define splnet()        spl3()

/* Highest tty device IPL. */
#define spltty()        spl4()

/* Requirement: imp >= (highest network, tty, or disk IPL) */
#define splimp()        spl4()

/* Intersil clock hardware interrupts (hard-wired at 5) */
#define splclock()      spl5()
#define splstatclock()  splclock()

/* Zilog Serial hardware interrupts (hard-wired at 6) */
#define splzs()         spl6()

/* Block out all interrupts (except NMI of course). */
#define splhigh()       spl7()
#define splsched()      spl7()

/* Get current sr value (debug, etc.) */
extern int getsr __P((void));

#if defined(_KERNEL) && !defined(_LOCORE)
extern void _delay __P((unsigned));
#define delay(us)	_delay((us)<<8)
#define	DELAY(n)	delay(n)
#endif	/* _KERNEL && !_LOCORE */

#endif	/* MACHINE */
