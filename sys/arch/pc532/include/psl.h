/*	$NetBSD: psl.h,v 1.13 1995/09/26 20:16:21 phil Exp $	*/

/*-
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
 *	@(#)psl.h	5.2 (Berkeley) 1/18/91
 */

#ifndef _MACHINE_PSL_H_
#define _MACHINE_PSL_H_

/*
 * 32532 processor status longword.
 */
#define	PSL_C		0x00000001	/* carry bit */
#define	PSL_T		0x00000002	/* trace enable bit */
#define	PSL_L		0x00000004	/* less bit */
#define	PSL_V		0x00000010	/* overflow bit */
#define	PSL_F		0x00000020	/* flag bit */
#define	PSL_Z		0x00000040	/* zero bit */
#define	PSL_N		0x00000080	/* negative bit */

#define PSL_USER	0x00000100	/* User mode bit */
#define PSL_US		0x00000200	/* User stack mode bit */
#define PSL_P		0x00000400	/* Prevent TRC trap */
#define	PSL_I		0x00000800	/* interrupt enable bit */

#define	PSL_USERSET	(PSL_USER | PSL_US | PSL_I)
#define	PSL_USERSTATIC	(PSL_USER | PSL_US | PSL_I)

/* The PSR versions ... */
#define PSR_USR PSL_USER

#ifdef _KERNEL
#include <machine/icu.h>
/*
 * Interrupt levels
 */
#define	IPL_NONE	-1
#define IPL_ZERO	0	/* level 0 */
#define	IPL_BIO		1	/* block I/O */
#define	IPL_NET		2	/* network */
#define	IPL_TTY		3	/* terminal */
#define	IPL_CLOCK	4	/* clock */
#define IPL_IMP		5	/* memory allocation */
#define	NIPL		6	/* number of interrupt priority levels */
#define IPL_NAMES	{"zero", "bio", "net", "tty", "clock", "imp"}

/*
 * Preassigned software interrupts
 */
#define SOFTINT		16
#define	SIR_CLOCK	(SOFTINT+0)
#define	SIR_CLOCKMASK	(1 << SIR_CLOCK)
#define	SIR_NET		(SOFTINT+1)
#define	SIR_NETMASK	((1 << SIR_NET) | SIR_CLOCKMASK)
#define SIR_ALLMASK	0xffff0000

#ifndef LOCORE
/*
 * Structure of the software interrupt table
 */
struct iv {
	void (*iv_vec)();
	void *iv_arg;
	int iv_cnt;
	char *iv_use;
};

extern unsigned int imask[], Cur_pl, idisabled, sirpending, astpending;
extern void intr_init();
extern void check_sir();
extern int intr_establish(int, void (*)(), void *, char *, int, int);
extern struct iv ivt[];

/*
 * Disable/Enable CPU-Interrupts
 */
#define di() /* Removing the nop will give you *BIG* trouble */ \
	__asm __volatile("bicpsrw 0x800 ; nop" : : : "cc")
#define ei() __asm __volatile("bispsrw 0x800" : : : "cc")

/*
 * Globaly disable/enable specific interrupts
 * (overriding spl0)
 */
#define intr_disable(ir) do { \
		di(); \
		ICUW(IMSK) = Cur_pl | (idisabled |= (1 << ir)); \
		ei(); \
	} while(0)

#define intr_enable(ir) do { \
		di(); \
		ICUW(IMSK) = Cur_pl | (idisabled &= ~(1 << ir)); \
		ei(); \
	} while(0)

/*
 * Add a mask to Cur_pl, and return the old value of Cur_pl.
 */
#if !defined(NO_INLINE_SPLX) || defined(DEFINE_SPLX)
# ifndef NO_INLINE_SPLX
static __inline
# endif
int
splraise(register int ncpl)
{
	register int ocpl;
	di();
	ocpl = Cur_pl;
	ncpl |= ocpl;
	ICUW(IMSK) = ncpl | idisabled;
	Cur_pl = ncpl;
	ei();
	return(ocpl);
}

/*
 * Restore a value to Cur_pl (unmasking interrupts).
 *
 * NOTE: We go to the trouble of returning the old value of cpl for
 * the benefit of some splsoftclock() callers.  This extra work is
 * usually optimized away by the compiler.
 */
# ifndef DEFINE_SPLX
static
# endif
# ifndef NO_INLINE_SPLX
__inline
# endif
int
splx(register int ncpl)
{
	register int ocpl;
	di();
	ocpl = Cur_pl;
	ICUW(IMSK) = ncpl | idisabled;
	Cur_pl = ncpl;
	if (sirpending && ncpl == imask[IPL_ZERO]) {
		Cur_pl |= SIR_ALLMASK;
		check_sir();
		Cur_pl = ncpl;
	}
	ei();
	return (ocpl);
}

/*
 * This special version of splx returns with interrupts disabled.
 */
# ifdef DEFINE_SPLX
int
splx_di(register int ncpl)
{
	register int ocpl;
	di();
	ocpl = Cur_pl;
	ICUW(IMSK) = ncpl | idisabled;
	Cur_pl = ncpl;
	if (sirpending && ncpl == imask[IPL_ZERO]) {
		Cur_pl |= SIR_ALLMASK;
		check_sir();
		Cur_pl = ncpl;
	}
	return (ocpl);
}
# endif
#endif

/*
 * Hardware interrupt masks
 */
#define splbio()	splraise(imask[IPL_BIO])
#define splnet()	splraise(imask[IPL_NET])
#define spltty()	splraise(imask[IPL_TTY])
#define splclock()	splraise(imask[IPL_CLOCK])
#define splimp()	splraise(imask[IPL_IMP])
#define	splstatclock()	splclock()

/*
 * Software interrupt masks
 *
 * NOTE: splsoftclock() is used by hardclock() to lower the priority from
 * clock to softclock before it calls softclock().
 */
#define	splsoftclock()	splx(SIR_CLOCKMASK|imask[IPL_ZERO])
#define	splsoftnet()	splraise(SIR_NETMASK)

/*
 * Miscellaneous
 */
#define	splhigh()	splraise(-1)
#define	spl0()		splx(imask[IPL_ZERO])
#define splnone()	spl0()

/*
 * Software interrupt registration
 */
#define	softintr(n)	(sirpending |= (1 << (n)))
#define	setsoftast()	(astpending = 1)
#define	setsoftclock()	softintr(SIR_CLOCK)
#define	setsoftnet()	softintr(SIR_NET)

#endif /* !LOCORE */
#endif /* _KERNEL */

#endif /* _MACHINE_PSL_H_ */
