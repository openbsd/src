/*	$OpenBSD: intr.h,v 1.20 2004/05/23 00:06:01 tedu Exp $	*/
/*	$NetBSD: intr.h,v 1.5 1996/05/13 06:11:28 mycroft Exp $	*/

/*
 * Copyright (c) 1996 Charles M. Hannum.  All rights reserved.
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
 *	This product includes software developed by Charles M. Hannum.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _I386_INTR_H_
#define _I386_INTR_H_

/*
 * Intel APICs (advanced programmable interrupt controllers) have
 * bytesized priority registers where the upper nibble is the actual
 * interrupt priority level (a.k.a. IPL).  Interrupt vectors are
 * closely tied to these levels as interrupts whose vectors' upper
 * nibble is lower than or equal to the current level are blocked.
 * Not all 256 possible vectors are available for interrupts in
 * APIC systems, only
 *
 * For systems where instead the older ICU (interrupt controlling
 * unit, a.k.a. PIC or 82C59) is used, the IPL is not directly useful,
 * since the interrupt blocking is handled via interrupt masks instead
 * of levels.  However the IPL is easily used as an offset into arrays
 * of masks.
 */
#define IPLSHIFT 4	/* The upper nibble of vectors is the IPL.	*/
#define NIPL 16		/* Four bits of information gives as much.	*/
#define IPL(level) ((level) >> IPLSHIFT)	/* Extract the IPL.	*/
/* XXX Maybe this IDTVECOFF definition should be elsewhere? */
#define IDTVECOFF 0x20	/* The lower 32 IDT vectors are reserved.	*/

/*
 * This macro is only defined for 0 <= x < 14, i.e. there are fourteen
 * distinct priority levels available for interrupts.
 */
#define MAKEIPL(priority) (IDTVECOFF + ((priority) << IPLSHIFT))

/*
 * Interrupt priority levels.
 * XXX We are somewhat sloppy about what we mean by IPLs, sometimes
 * XXX we refer to the eight-bit value suitable for storing into APICs'
 * XXX priority registers, other times about the four-bit entity found
 * XXX in the former values' upper nibble, which can be used as offsets
 * XXX in various arrays of our implementation.  We are hoping that
 * XXX the context will provide enough information to not make this
 * XXX sloppy naming a real problem.
 */
#define	IPL_NONE	0		/* nothing */
#define	IPL_SOFTCLOCK	MAKEIPL(0)	/* timeouts */
#define	IPL_SOFTNET	MAKEIPL(1)	/* protocol stacks */
#define	IPL_BIO		MAKEIPL(2)	/* block I/O */
#define	IPL_NET		MAKEIPL(3)	/* network */
#define	IPL_SOFTTTY	MAKEIPL(4)	/* delayed terminal handling */
#define	IPL_TTY		MAKEIPL(5)	/* terminal */
#define	IPL_VM		MAKEIPL(6)	/* memory allocation */
#define IPL_IMP		IPL_VM		/* XXX - should not be here. */
#define	IPL_AUDIO	MAKEIPL(7)	/* audio */
#define	IPL_CLOCK	MAKEIPL(8)	/* clock */
#define	IPL_STATCLOCK	MAKEIPL(9)	/* statclock */
#define	IPL_HIGH	MAKEIPL(9)	/* everything */

/* Interrupt sharing types. */
#define	IST_NONE	0	/* none */
#define	IST_PULSE	1	/* pulsed */
#define	IST_EDGE	2	/* edge-triggered */
#define	IST_LEVEL	3	/* level-triggered */

/* Soft interrupt masks. */
#define	SIR_CLOCK	31
#define	SIR_NET		30
#define	SIR_TTY		29

#ifndef _LOCORE

volatile int cpl;	/* Current interrupt priority level.		*/
volatile int ipending;	/* Interrupts pending.				*/
volatile int astpending;/* Asynchronous software traps (softints) pending. */
int imask[NIPL];	/* Bitmasks telling what interrupts are blocked. */
int iunmask[NIPL];	/* Bitmasks telling what interrupts are accepted. */

#define IMASK(level) imask[IPL(level)]
#define IUNMASK(level) iunmask[IPL(level)]

extern void Xspllower(void);

int splraise(int);
int spllower(int);
void splx(int);
void softintr(int);

/* SPL asserts */
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
 * Hardware interrupt masks
 */
#define	splbio()	splraise(IPL_BIO)
#define	splnet()	splraise(IPL_NET)
#define	spltty()	splraise(IPL_TTY)
#define	splaudio()	splraise(IPL_AUDIO)
#define	splclock()	splraise(IPL_CLOCK)
#define	splstatclock()	splhigh()

/*
 * Software interrupt masks
 *
 * NOTE: spllowersoftclock() is used by hardclock() to lower the priority from
 * clock to softclock before it calls softclock().
 */
#define	spllowersoftclock()	spllower(IPL_SOFTCLOCK)
#define	splsoftclock()		splraise(IPL_SOFTCLOCK)
#define	splsoftnet()		splraise(IPL_SOFTNET)
#define	splsofttty()		splraise(IPL_SOFTTTY)

/*
 * Miscellaneous
 */
#define	splvm()		splraise(IPL_VM)
#define splimp()	splvm()
#define	splhigh()	splraise(IPL_HIGH)
#define	spl0()		spllower(IPL_NONE)

#define	setsoftast()	(astpending = 1)
#define	setsoftclock()	softintr(1 << SIR_CLOCK)
#define	setsoftnet()	softintr(1 << SIR_NET)
#define	setsofttty()	softintr(1 << SIR_TTY)

#endif /* !_LOCORE */

#endif /* !_I386_INTR_H_ */
