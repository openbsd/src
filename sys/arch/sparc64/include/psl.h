/*	$OpenBSD: psl.h,v 1.12 2005/01/05 18:42:28 fgsch Exp $	*/
/*	$NetBSD: psl.h,v 1.20 2001/04/13 23:30:05 thorpej Exp $ */

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

#ifndef _SPARC64_PSL_
#define _SPARC64_PSL_

/*
 * SPARC Process Status Register (in psl.h for hysterical raisins).  This
 * doesn't exist on the V9.
 *
 * The picture in the Sun manuals looks like this:
 *					     1 1
 *	 31   28 27   24 23   20 19	  14 3 2 11    8 7 6 5 4       0
 *	+-------+-------+-------+-----------+-+-+-------+-+-+-+---------+
 *	|  impl |  ver	|  icc	|  reserved |E|E|  pil	|S|P|E|	  CWP	|
 *	|	|	|n z v c|	    |C|F|	| |S|T|		|
 *	+-------+-------+-------+-----------+-+-+-------+-+-+-+---------+
 */

#define PSR_IMPL	0xf0000000	/* implementation */
#define PSR_VER		0x0f000000	/* version */
#define PSR_ICC		0x00f00000	/* integer condition codes */
#define PSR_N		0x00800000	/* negative */
#define PSR_Z		0x00400000	/* zero */
#define PSR_O		0x00200000	/* overflow */
#define PSR_C		0x00100000	/* carry */
#define PSR_EC		0x00002000	/* coprocessor enable */
#define PSR_EF		0x00001000	/* FP enable */
#define PSR_PIL		0x00000f00	/* interrupt level */
#define PSR_S		0x00000080	/* supervisor (kernel) mode */
#define PSR_PS		0x00000040	/* previous supervisor mode (traps) */
#define PSR_ET		0x00000020	/* trap enable */
#define PSR_CWP		0x0000001f	/* current window pointer */

#define PSR_BITS "\20\16EC\15EF\10S\7PS\6ET"

/* Interesting spl()s */
#define PIL_SCSI	3
#define PIL_FDSOFT	4
#define PIL_AUSOFT	4
#define PIL_BIO		5
#define PIL_VIDEO	5
#define PIL_TTY		6
#define PIL_LPT		6
#define PIL_NET		6
#define PIL_VM		7
#define	PIL_AUD		8
#define PIL_CLOCK	10
#define PIL_FD		11
#define PIL_SER		12
#define PIL_STATCLOCK	14
#define PIL_HIGH	15
#define PIL_SCHED	PIL_CLOCK
#define PIL_LOCK	PIL_HIGH

/* 
 * SPARC V9 CCR register
 */

#define ICC_C	0x01L
#define ICC_V	0x02L
#define ICC_Z	0x04L
#define ICC_N	0x08L
#define XCC_SHIFT	4
#define XCC_C	(ICC_C<<XCC_SHIFT)
#define XCC_V	(ICC_V<<XCC_SHIFT)
#define XCC_Z	(ICC_Z<<XCC_SHIFT)
#define XCC_N	(ICC_N<<XCC_SHIFT)


/*
 * SPARC V9 PSTATE register (what replaces the PSR in V9)
 *
 * Here's the layout:
 *
 *    11   10    9     8   7  6   5     4     3     2     1   0
 *  +------------------------------------------------------------+
 *  | IG | MG | CLE | TLE | MM | RED | PEF | AM | PRIV | IE | AG |
 *  +------------------------------------------------------------+
 */

#define PSTATE_IG	0x800	/* enable spitfire interrupt globals */
#define PSTATE_MG	0x400	/* enable spitfire MMU globals */
#define PSTATE_CLE	0x200	/* current little endian */
#define PSTATE_TLE	0x100	/* traps little endian */
#define PSTATE_MM	0x0c0	/* memory model */
#define PSTATE_MM_TSO	0x000	/* total store order */
#define PSTATE_MM_PSO	0x040	/* partial store order */
#define PSTATE_MM_RMO	0x080	/* Relaxed memory order */
#define PSTATE_RED	0x020	/* RED state */
#define PSTATE_PEF	0x010	/* enable floating point */
#define PSTATE_AM	0x008	/* 32-bit address masking */
#define PSTATE_PRIV	0x004	/* privileged mode */
#define PSTATE_IE	0x002	/* interrupt enable */
#define PSTATE_AG	0x001	/* enable alternate globals */

#define PSTATE_BITS "\20\14IG\13MG\12CLE\11TLE\10\7MM\6RED\5PEF\4AM\3PRIV\2IE\1AG"


/*
 * 32-bit code requires TSO or at best PSO since that's what's supported on
 * SPARC V8 and earlier machines.
 *
 * 64-bit code sets the memory model in the ELF header.
 *
 * We're running kernel code in TSO for the moment so we don't need to worry
 * about possible memory barrier bugs.
 */

#define PSTATE_PROM	(PSTATE_MM_TSO|PSTATE_PRIV)
#define PSTATE_NUCLEUS	(PSTATE_MM_TSO|PSTATE_PRIV|PSTATE_AG)
#define PSTATE_KERN	(PSTATE_MM_TSO|PSTATE_PRIV)
#define PSTATE_INTR	(PSTATE_KERN|PSTATE_IE)
#define PSTATE_USER32	(PSTATE_MM_TSO|PSTATE_AM|PSTATE_IE)
#define PSTATE_USER	(PSTATE_MM_RMO|PSTATE_IE)


/*
 * SPARC V9 TSTATE register
 *
 *   39 32 31 24 23 18  17   8	7 5 4   0
 *  +-----+-----+-----+--------+---+-----+
 *  | CCR | ASI |  -  | PSTATE | - | CWP |
 *  +-----+-----+-----+--------+---+-----+
 */

#define TSTATE_CWP		0x01f
#define TSTATE_PSTATE		0x6ff00
#define TSTATE_PSTATE_SHIFT	8
#define TSTATE_ASI		0xff000000LL
#define TSTATE_ASI_SHIFT	24
#define TSTATE_CCR		0xff00000000LL
#define TSTATE_CCR_SHIFT	32

#define PSRCC_TO_TSTATE(x)	(((int64_t)(x)&PSR_ICC)<<(TSTATE_CCR_SHIFT-19))
#define TSTATECCR_TO_PSR(x)	(((x)&TSTATE_CCR)>>(TSTATE_CCR_SHIFT-19))

/*
 * These are here to simplify life.
 */
#define TSTATE_IG	(PSTATE_IG<<TSTATE_PSTATE_SHIFT)
#define TSTATE_MG	(PSTATE_MG<<TSTATE_PSTATE_SHIFT)
#define TSTATE_CLE	(PSTATE_CLE<<TSTATE_PSTATE_SHIFT)
#define TSTATE_TLE	(PSTATE_TLE<<TSTATE_PSTATE_SHIFT)
#define TSTATE_MM	(PSTATE_MM<<TSTATE_PSTATE_SHIFT)
#define TSTATE_MM_TSO	(PSTATE_MM_TSO<<TSTATE_PSTATE_SHIFT)
#define TSTATE_MM_PSO	(PSTATE_MM_PSO<<TSTATE_PSTATE_SHIFT)
#define TSTATE_MM_RMO	(PSTATE_MM_RMO<<TSTATE_PSTATE_SHIFT)
#define TSTATE_RED	(PSTATE_RED<<TSTATE_PSTATE_SHIFT)
#define TSTATE_PEF	(PSTATE_PEF<<TSTATE_PSTATE_SHIFT)
#define TSTATE_AM	(PSTATE_AM<<TSTATE_PSTATE_SHIFT)
#define TSTATE_PRIV	(PSTATE_PRIV<<TSTATE_PSTATE_SHIFT)
#define TSTATE_IE	(PSTATE_IE<<TSTATE_PSTATE_SHIFT)
#define TSTATE_AG	(PSTATE_AG<<TSTATE_PSTATE_SHIFT)

#define TSTATE_BITS "\20\14IG\13MG\12CLE\11TLE\10\7MM\6RED\5PEF\4AM\3PRIV\2IE\1AG"

#define TSTATE_KERN	((TSTATE_KERN)<<TSTATE_PSTATE_SHIFT)
#define TSTATE_USER	((TSTATE_USER)<<TSTATE_PSTATE_SHIFT)
/*
 * SPARC V9 VER version register.
 *
 *  63   48 47  32 31  24 23 16 15    8 7 5 4      0
 * +-------+------+------+-----+-------+---+--------+
 * | manuf | impl | mask |  -  | maxtl | - | maxwin |
 * +-------+------+------+-----+-------+---+--------+
 *
 */

#define VER_MANUF	0xffff000000000000LL
#define VER_MANUF_SHIFT	48
#define VER_IMPL	0x0000ffff00000000LL
#define VER_IMPL_SHIFT	32
#define VER_MASK	0x00000000ff000000LL
#define VER_MASK_SHIFT	24
#define VER_MAXTL	0x000000000000ff00LL
#define VER_MAXTL_SHIFT	8
#define VER_MAXWIN	0x000000000000001fLL

/*
 * Here are a few things to help us transition between user and kernel mode:
 */

/* Memory models */
#define KERN_MM		PSTATE_MM_TSO
#define USER_MM		PSTATE_MM_RMO

/* 
 * Register window handlers.  These point to generic routines that check the
 * stack pointer and then vector to the real handler.  We could optimize this
 * if we could guarantee only 32-bit or 64-bit stacks.
 */
#define WSTATE_KERN	026
#define WSTATE_USER	022

#define CWP		0x01f

/* 64-byte alignment -- this seems the best place to put this. */
#define BLOCK_SIZE	64
#define BLOCK_ALIGN	0x3f

#if defined(_KERNEL) && !defined(_LOCORE)

extern u_int64_t ver;	/* Copy of v9 version register.  We need to read this only once, in locore.s. */
#ifndef SPLDEBUG
extern __inline void splx(int);
#endif

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
 * GCC pseudo-functions for manipulating privileged registers
 */
extern __inline u_int64_t getpstate(void);
extern __inline
u_int64_t getpstate()
{
	return (sparc_rdpr(pstate));
}

extern __inline void setpstate(u_int64_t);
extern __inline void setpstate(u_int64_t newpstate)
{
	sparc_wrpr(pstate, newpstate, 0);
}

extern __inline int getcwp(void);
extern __inline
int getcwp()
{
	return (sparc_rdpr(cwp));
}

extern __inline void setcwp(u_int64_t);
extern __inline void
setcwp(u_int64_t newcwp)
{
	sparc_wrpr(cwp, newcwp, 0);
}

extern __inline u_int64_t getver(void);
extern __inline
u_int64_t getver()
{
	return (sparc_rdpr(ver));
}

extern __inline u_int64_t intr_disable(void);
extern __inline u_int64_t
intr_disable()
{
	u_int64_t s;

	s = sparc_rdpr(pstate);
	sparc_wrpr(pstate, s & ~PSTATE_IE, 0);
	return (s);
}

extern __inline void intr_restore(u_int64_t);
extern __inline void
intr_restore(u_int64_t s)
{
	sparc_wrpr(pstate, s, 0);
}

extern __inline void stxa_sync(u_int64_t, u_int64_t, u_int64_t);
extern __inline void
stxa_sync(u_int64_t va, u_int64_t asi, u_int64_t val)
{
	u_int64_t s = intr_disable();
	stxa_nc(va, asi, val);
	membar(Sync);
	intr_restore(s);
}

/*
 * GCC pseudo-functions for manipulating PIL
 */

#ifdef SPLDEBUG
void prom_printf(const char *fmt, ...);
extern int printspl;
#define SPLPRINT(x)	if(printspl) { int i=10000000; prom_printf x ; while(i--); }
#define	SPL(name, newpil)						\
extern __inline int name##X(const char *, int);				\
extern __inline int name##X(const char *file, int line)			\
{									\
	u_int64_t oldpil = sparc_rdpr(pil);				\
	SPLPRINT(("{%s:%d %d=>%d}", file, line, oldpil, newpil));	\
	sparc_wrpr(pil, newpil, 0);					\
	return (oldpil);						\
}
/* A non-priority-decreasing version of SPL */
#define	SPLHOLD(name, newpil) \
extern __inline int name##X(const char *, int);				\
extern __inline int name##X(const char * file, int line)		\
{									\
	int oldpil = sparc_rdpr(pil);					\
	if (__predict_false((u_int64_t)newpil <= oldpil))		\
		return (oldpil);					\
	SPLPRINT(("{%s:%d %d->!d}", file, line, oldpil, newpil));	\
	sparc_wrpr(pil, newpil, 0);					\
	return (oldpil);						\
}

#else
#define SPLPRINT(x)	
#define	SPL(name, newpil)						\
extern __inline int name(void);						\
extern __inline int name()						\
{									\
	int oldpil;							\
	__asm __volatile("    rdpr %%pil, %0		\n"		\
			 "    wrpr %%g0, %1, %%pil	\n"		\
	    : "=&r" (oldpil)						\
	    : "n" (newpil)						\
	    : "%g0");							\
	return (oldpil);						\
}
/* A non-priority-decreasing version of SPL */
#define	SPLHOLD(name, newpil)						\
extern __inline int name(void);						\
extern __inline int name()						\
{									\
	int oldpil;							\
									\
	if (newpil <= 1) {						\
		__asm __volatile("    rdpr	%%pil, %0	\n"	\
				 "    brnz,pn	%0, 1f		\n"	\
				 "     nop			\n"	\
				 "    wrpr	%%g0, %1, %%pil	\n"	\
				 "1:				\n"	\
	    : "=&r" (oldpil)						\
	    : "I" (newpil)						\
	    : "%g0");							\
	} else {							\
		__asm __volatile("    rdpr	%%pil, %0	\n"	\
				 "    cmp	%0, %1 - 1	\n"	\
				 "    bgu,pn	%%xcc, 1f	\n"	\
				 "     nop			\n"	\
				 "    wrpr	%%g0, %1, %%pil	\n"	\
				 "1:				\n"	\
	    : "=&r" (oldpil)						\
	    : "I" (newpil)						\
	    : "cc");							\
	}								\
	return (oldpil);						\
}
#endif

SPL(spl0, 0)

SPL(spllowersoftclock, 1)

SPLHOLD(splsoftint, 1)
#define	splsoftclock	splsoftint
#define	splsoftnet	splsoftint

/* audio software interrupts are at software level 4 */
SPLHOLD(splausoft, PIL_AUSOFT)

/* floppy software interrupts are at software level 4 too */
SPLHOLD(splfdsoft, PIL_FDSOFT)

/* Block devices */
SPLHOLD(splbio, PIL_BIO)

/* network hardware interrupts are at level 6 */
SPLHOLD(splnet, PIL_NET)

/* tty input runs at software level 6 */
SPLHOLD(spltty, PIL_TTY)

/* parallel port runs at software level 6 */
SPLHOLD(spllpt, PIL_LPT)

/*
 * Memory allocation (must be as high as highest network, tty, or disk device)
 */
SPLHOLD(splvm, PIL_VM)
#define	splimp splvm

SPLHOLD(splclock, PIL_CLOCK)

/* fd hardware interrupts are at level 11 */
SPLHOLD(splfd, PIL_FD)

/* zs hardware interrupts are at level 12 */
SPLHOLD(splzs, PIL_SER)
SPLHOLD(splserial, PIL_SER)

/* audio hardware interrupts are at level 13 */
SPLHOLD(splaudio, PIL_AUD)

/* second sparc timer interrupts at level 14 */
SPLHOLD(splstatclock, PIL_STATCLOCK)

SPLHOLD(splsched, PIL_SCHED)
SPLHOLD(spllock, PIL_LOCK)

SPLHOLD(splhigh, PIL_HIGH)

/* splx does not have a return value */
#ifdef SPLDEBUG

#define	spl0()		spl0X(__FILE__, __LINE__)
#define	spllowersoftclock() spllowersoftclockX(__FILE__, __LINE__)
#define	splsoftint()	splsoftintX(__FILE__, __LINE__)
#define	splausoft()	splausoftX(__FILE__, __LINE__)
#define	splfdsoft()	splfdsoftX(__FILE__, __LINE__)
#define	splbio()	splbioX(__FILE__, __LINE__)
#define	splnet()	splnetX(__FILE__, __LINE__)
#define	spltty()	splttyX(__FILE__, __LINE__)
#define	spllpt()	spllptX(__FILE__, __LINE__)
#define	splvm()		splvmX(__FILE__, __LINE__)
#define	splclock()	splclockX(__FILE__, __LINE__)
#define	splfd()		splfdX(__FILE__, __LINE__)
#define	splzs()		splzsX(__FILE__, __LINE__)
#define	splserial()	splzerialX(__FILE__, __LINE__)
#define	splaudio()	splaudioX(__FILE__, __LINE__)
#define	splstatclock()	splstatclockX(__FILE__, __LINE__)
#define	splsched()	splschedX(__FILE__, __LINE__)
#define	spllock()	spllockX(__FILE__, __LINE__)
#define	splhigh()	splhighX(__FILE__, __LINE__)
#define splx(x)		splxX((x),__FILE__, __LINE__)

extern __inline void splxX(u_int64_t, const char *, int);
extern __inline void
splxX(u_int64_t newpil, const char *file, int line)
#else
extern __inline void splx(int newpil)
#endif
{
#ifdef SPLDEBUG
	u_int64_t oldpil = sparc_rdpr(pil);
	SPLPRINT(("{%d->%d}", oldpil, newpil));
#endif
	sparc_wrpr(pil, newpil, 0);
}
#endif /* KERNEL && !_LOCORE */

#endif /* _SPARC64_PSL_ */
