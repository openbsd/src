/*	$OpenBSD: psl.h,v 1.14 2002/07/23 14:00:38 art Exp $	*/
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
#define IPL_SOFTCLOCK	IPL_SOFTINT	/* softclock() interrupts */
#define IPL_SOFTNET	IPL_SOFTINT	/* soft network interrupts */
#define IPL_AUSOFT	4		/* audio soft interrupts */
#define IPL_FDSOFT	4		/* floppy soft interrupts */
#define IPL_BIO		5		/* block devices are at 5 and below */
#define IPL_TTY		6		/* tty soft interrupts */
#define IPL_NET		7		/* network hardware at 7 or below */
#define IPL_VM		7		/* max(BIO, NET, TTY) */
#define	IPL_CLOCK	10		/* hardclock() */
#define IPL_FD		11		/* hard floppy interrupts. */
#define IPL_ZS		12		/* zs interrupts */
/*
 * XXX - this is called AUHARD instead of AUDIO because of some confusion
 * with how MI audio code handles this. Stay tuned for a change in the future
 */
#define IPL_AUHARD	13		/* hard audio interrupts */
#define IPL_STATCLOCK	14		/* statclock() */

#if defined(_KERNEL) && !defined(_LOCORE)

static __inline int getpsr(void);
static __inline void setpsr(int);
static __inline int spl0(void);
static __inline int splhigh(void);
static __inline void splx(int);
static __inline int getmid(void);

/*
 * GCC pseudo-functions for manipulating PSR (primarily PIL field).
 */
static __inline int
getpsr()
{
	int psr;

	__asm __volatile("rd %%psr,%0" : "=r" (psr));
	return (psr);
}

static __inline int
getmid()
{
	int mid;

	__asm __volatile("rd %%tbr,%0" : "=r" (mid));
	return ((mid >> 20) & 0x3);
}

static __inline void
setpsr(newpsr)
	int newpsr;
{
	__asm __volatile("wr %0,0,%%psr" : : "r" (newpsr));
	__asm __volatile("nop");
	__asm __volatile("nop");
	__asm __volatile("nop");
}

static __inline int
spl0()
{
	int psr, oldipl;

	/*
	 * wrpsr xors two values: we choose old psr and old ipl here,
	 * which gives us the same value as the old psr but with all
	 * the old PIL bits turned off.
	 */
	__asm __volatile("rd %%psr,%0" : "=r" (psr));
	oldipl = psr & PSR_PIL;
	__asm __volatile("wr %0,%1,%%psr" : : "r" (psr), "r" (oldipl));

	/*
	 * Three instructions must execute before we can depend
	 * on the bits to be changed.
	 */
	__asm __volatile("nop; nop; nop");
	return (oldipl);
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
	if (__predict_false(splassert_ctl > 0)) {	\
		splassert_check(__wantipl, __func__);	\
	}						\
} while (0)
#else
#define splassert(wantipl) do { /* nada */ } while (0)
#endif

/*
 * PIL 1 through 14 can use this macro.
 * (spl0 and splhigh are special since they put all 0s or all 1s
 * into the ipl field.)
 */
#define	SPL(name, newipl) \
static __inline int name(void); \
static __inline int name() \
{ \
	int psr, oldipl; \
	__asm __volatile("rd %%psr,%0" : "=r" (psr)); \
	oldipl = psr & PSR_PIL; \
	psr &= ~oldipl; \
	__asm __volatile("wr %0,%1,%%psr" : : \
	    "r" (psr), "n" ((newipl) << 8)); \
	__asm __volatile("nop; nop; nop"); \
	return (oldipl); \
}
/* A non-priority-decreasing version of SPL */
#define	SPLHOLD(name, newipl) \
static __inline int name(void); \
static __inline int name() \
{ \
	int psr, oldipl; \
	__asm __volatile("rd %%psr,%0" : "=r" (psr)); \
	oldipl = psr & PSR_PIL; \
	if ((newipl << 8) <= oldipl) \
		return oldipl; \
	psr &= ~oldipl; \
	__asm __volatile("wr %0,%1,%%psr" : : \
	    "r" (psr), "n" ((newipl) << 8)); \
	__asm __volatile("nop; nop; nop"); \
	return (oldipl); \
}

SPLHOLD(splsoftint, IPL_SOFTINT)
#define	splsoftclock		splsoftint
#define	splsoftnet		splsoftint
SPL(spllowersoftclock, IPL_SOFTCLOCK)
SPLHOLD(splausoft, IPL_AUSOFT)
SPLHOLD(splfdsoft, IPL_FDSOFT)
SPLHOLD(splbio, IPL_BIO)
SPLHOLD(splnet, IPL_NET)
SPLHOLD(spltty, IPL_TTY)
SPLHOLD(splvm, IPL_VM)
/* XXX - the following two should die. */
#define splimp splvm
#define splpmap splvm
SPLHOLD(splclock, IPL_CLOCK)
SPLHOLD(splfd, IPL_FD)
SPLHOLD(splzs, IPL_ZS)
SPLHOLD(splaudio, IPL_AUHARD)
SPLHOLD(splstatclock, IPL_STATCLOCK)

static __inline int splhigh()
{
	int psr, oldipl;

	__asm __volatile("rd %%psr,%0" : "=r" (psr));
	__asm __volatile("wr %0,0,%%psr" : : "r" (psr | PSR_PIL));
	__asm __volatile("and %1,%2,%0; nop; nop" : "=r" (oldipl) : \
	    "r" (psr), "n" (PSR_PIL));
	return (oldipl);
}

/* splx does not have a return value */
static __inline void splx(newipl)
	int newipl;
{
	int psr;

	__asm __volatile("rd %%psr,%0" : "=r" (psr));
	__asm __volatile("wr %0,%1,%%psr" : : \
	    "r" (psr & ~PSR_PIL), "rn" (newipl));
	__asm __volatile("nop; nop; nop");
}
#endif /* KERNEL && !_LOCORE */

#endif /* PSR_IMPL */
