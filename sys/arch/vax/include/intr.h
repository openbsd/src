/*	$OpenBSD: intr.h,v 1.8 2007/05/16 19:37:06 thib Exp $	*/
/* 	$NetBSD: intr.h,v 1.1 1998/08/18 23:55:00 matt Exp $	*/

/*
 * Copyright (c) 1998 Matt Thomas.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _VAX_INTR_H_
#define _VAX_INTR_H_

/* Define the various Interrupt Priority Levels */

/* Interrupt Priority Levels are not mutually exclusive. */

#define IPL_NONE	0x00
#define	IPL_SOFTCLOCK	0x08
#define	IPL_SOFTNET	0x0c
#define IPL_BIO		0x15	/* block I/O */
#define IPL_NET		0x15	/* network */
#define IPL_TTY		0x15	/* terminal */
#define IPL_VM		0x17	/* memory allocation */
#define	IPL_AUDIO	0x15	/* audio */
#define IPL_CLOCK	0x18	/* clock */
#define IPL_STATCLOCK	0x18	/* statclock */
#define	IPL_HIGH	0x1f

#define	IST_UNUSABLE	-1	/* interrupt cannot be used */
#define	IST_NONE	0	/* none (dummy) */
#define	IST_PULSE	1	/* pulsed */
#define	IST_EDGE	2	/* edge-triggered */
#define	IST_LEVEL	3	/* level-triggered */

#ifndef lint
#define _splset(reg)						\
({								\
	register int val;					\
	__asm __volatile ("mfpr $0x12,%0;mtpr %1,$0x12"		\
				: "=&g" (val)			\
				: "g" (reg));			\
	val;							\
})

#define	_splraise(reg)						\
({								\
	register int val;					\
	__asm __volatile ("mfpr $0x12,%0"			\
				: "=&g" (val)			\
				: );				\
	if ((reg) > val) {					\
		__asm __volatile ("mtpr %0,$0x12"		\
				:				\
				: "g" (reg));			\
	}							\
	val;							\
})

#define	splx(reg)						\
	__asm __volatile ("mtpr %0,$0x12" : : "g" (reg))
#endif

#define	spl0()		_splset(IPL_NONE)
#define splsoftclock()	_splraise(IPL_SOFTCLOCK)
#define splsoftnet()	_splraise(IPL_SOFTNET)
#define splbio()	_splraise(IPL_BIO)
#define splnet()	_splraise(IPL_NET)
#define spltty()	_splraise(IPL_TTY)
#define splvm()		_splraise(IPL_VM)
#define splclock()	_splraise(IPL_CLOCK)
#define splstatclock()	_splraise(IPL_STATCLOCK)
#define splhigh()	_splset(IPL_HIGH)
#define	splsched()	splhigh()

/* These are better to use when playing with VAX buses */
#define	spl4()		_splraise(0x14)
#define	spl5()		_splraise(0x15)
#define	spl6()		_splraise(0x16)
#define	spl7()		_splraise(0x17)

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
	if (splassert_ctl > 0) {			\
		splassert_check(__wantipl, __func__);	\
	}						\
} while (0)
#else
#define	splassert(wantipl)	do { /* nothing */ } while (0)
#endif

#endif	/* _VAX_INTR_H */
