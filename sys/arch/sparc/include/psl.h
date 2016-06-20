/*	$OpenBSD: psl.h,v 1.29 2016/06/20 11:02:33 dlg Exp $	*/
/*	$NetBSD: psl.h,v 1.12 1997/03/10 21:49:11 pk Exp $ */

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
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
 *	@(#)psl.h	8.1 (Berkeley) 6/11/93
 */

#ifndef PSR_IMPL

/*
 * SPARC Process Status Register (in psl.h for hysterical raisins).
 *
 * The picture in the Sun manuals looks like this:
 *	                                     1 1
 *	 31   28 27   24 23   20 19       14 3 2 11    8 7 6 5 4       0
 *	+-------+-------+-------+-----------+-+-+-------+-+-+-+---------+
 *	|  impl |  ver  |  icc  |  reserved |E|E|  pil  |S|P|E|   CWP   |
 *	|       |       |n z v c|           |C|F|       | |S|T|         |
 *	+-------+-------+-------+-----------+-+-+-------+-+-+-+---------+
 */

#define	PSR_IMPL	0xf0000000	/* implementation */
#define	PSR_VER		0x0f000000	/* version */
#define	PSR_ICC		0x00f00000	/* integer condition codes */
#define	PSR_N		0x00800000	/* negative */
#define	PSR_Z		0x00400000	/* zero */
#define	PSR_O		0x00200000	/* overflow */
#define	PSR_C		0x00100000	/* carry */
#define	PSR_EC		0x00002000	/* coprocessor enable */
#define	PSR_EF		0x00001000	/* FP enable */
#define	PSR_PIL		0x00000f00	/* interrupt level */
#define	PSR_S		0x00000080	/* supervisor (kernel) mode */
#define	PSR_PS		0x00000040	/* previous supervisor mode (traps) */
#define	PSR_ET		0x00000020	/* trap enable */
#define	PSR_CWP		0x0000001f	/* current window pointer */

#define	PSR_BITS "\20\16EC\15EF\10S\7PS\6ET"

/*
 * Various interrupt levels.
 */
#define IPL_NONE	0
#define IPL_SOFTINT	1
#define IPL_SOFTCLOCK	1		/* softclock() interrupts */
#define IPL_SOFTNET	1		/* soft network interrupts */
#define IPL_AUSOFT	4		/* audio soft interrupts */
#define IPL_FDSOFT	4		/* floppy soft interrupts */
#define IPL_BIO		5		/* block devices are at 5 and below */
#define IPL_TTY		6		/* MD tty soft interrupts */
#define	IPL_SOFTTTY	IPL_TTY
#define IPL_NET		7		/* network hardware at 7 or below */
#define IPL_VM		7		/* max(BIO, NET, TTY) */
#define	IPL_FB		9		/* framebuffer interrupts */
#define	IPL_CLOCK	10		/* hardclock() */
#define IPL_FD		11		/* hard floppy interrupts. */
#define IPL_ZS		12		/* zs interrupts */
/*
 * XXX - this is called AUHARD instead of AUDIO because of some confusion
 * with how MI audio code handles this. Stay tuned for a change in the future
 */
#define IPL_AUHARD	13		/* hard audio interrupts */
#define IPL_AUDIO	IPL_AUHARD
#define IPL_STATCLOCK	14		/* statclock() */
#define IPL_SCHED	IPL_STATCLOCK
#define IPL_HIGH	15		/* splhigh() */

#define	IPL_MPSAFE	0	/* no "mpsafe" interrupts */

#if defined(_KERNEL) && !defined(_LOCORE)

static __inline int getpsr(void);
static __inline void setpsr(int);
static __inline int getmid(void);

/*
 * GCC pseudo-functions for manipulating PSR (primarily PIL field).
 */
static __inline int
getpsr()
{
	int psr;

	__asm volatile("rd %%psr,%0" : "=r" (psr));
	return (psr);
}

static __inline int
getmid()
{
	int mid;

	__asm volatile("rd %%tbr,%0" : "=r" (mid));
	return ((mid >> 20) & 0x3);
}

static __inline void
setpsr(newpsr)
	int newpsr;
{
	__asm volatile("wr %0,0,%%psr" : : "r" (newpsr));
	__asm volatile("nop");
	__asm volatile("nop");
	__asm volatile("nop");
}

#ifdef DIAGNOSTIC
/*
 * Although this function is implemented in MI code, it must be in this MD
 * header because we don't want this header to include MI includes.
 */
void splassert_fail(int, int, const char *);
extern int splassert_ctl;
void splassert_check(int, const char *);
#define splassert(__wantipl) do {			\
	if (splassert_ctl > 0) {			\
		splassert_check(__wantipl, __func__);	\
	}						\
} while (0)
#define splsoftassert(wantipl) splassert(wantipl)
#else
#define splassert(wantipl)	do { /* nada */ } while (0)
#define splsoftassert(wantipl)	do { /* nada */ } while (0)
#endif

int	spl0(void);
int	splraise(int);
int	splhigh(void);
void	splx(int);

#define splsoftint()	splraise(IPL_SOFTINT)
#define splsoftclock()	splraise(IPL_SOFTCLOCK)
#define splsoftnet()	splraise(IPL_SOFTNET)
#define splausoft()	splraise(IPL_AUSOFT)
#define splfdsoft()	splraise(IPL_FDSOFT)
#define splbio()	splraise(IPL_BIO)
#define splnet()	splraise(IPL_NET)
#define spltty()	splraise(IPL_TTY)
#define splvm()		splraise(IPL_VM)
#define splclock()	splraise(IPL_CLOCK)
#define splfd()		splraise(IPL_FD)
#define splzs()		splraise(IPL_ZS)
#define splaudio()	splraise(IPL_AUDIO)
#define splsched()	splraise(IPL_SCHED)
#define splstatclock()	splraise(IPL_STATCLOCK)

#endif /* KERNEL && !_LOCORE */

#endif /* PSR_IMPL */
