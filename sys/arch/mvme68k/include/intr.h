/*	$OpenBSD: intr.h,v 1.15 2007/05/15 13:46:22 martin Exp $	*/
/*
 * Copyright (C) 2000 Steve Murphree, Jr.
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
 * 3. The name of the author may not be used to endorse or promote products
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

#ifndef _MVME68K_INTR_H_
#define _MVME68K_INTR_H_

#include <machine/psl.h>

#ifdef _KERNEL

/*
 * Simulated software interrupt register.
 */
extern volatile u_int8_t ssir;

#define	SIR_NET		0x01
#define	SIR_CLOCK	0x02

#define	siron(mask)	\
	__asm __volatile ( "orb %1,%0" : "=m" (ssir) : "ir" (mask))
#define	siroff(mask)	\
	__asm __volatile ( "andb %1,%0" : "=m" (ssir) : "ir" (~(mask)))

#define	setsoftint(s)	siron(s)
#define	setsoftnet()	siron(SIR_NET)
#define	setsoftclock()	siron(SIR_CLOCK)

u_int8_t allocate_sir(void (*proc)(void *), void *arg);

/*
 * Interrupt "levels".  These are a more abstract representation
 * of interrupt levels, and do not have the same meaning as m68k
 * CPU interrupt levels.  They serve two purposes:
 *
 *      - properly order ISRs in the list for that CPU ipl
 *      - compute CPU PSL values for the spl*() calls.
 */
#define IPL_NONE	0
#define IPL_SOFTNET	1
#define IPL_SOFTCLOCK	1
#define IPL_BIO		2
#define IPL_NET		3
#define IPL_TTY		3
#define IPL_CLOCK	5
#define IPL_STATCLOCK	5
#define IPL_HIGH	7

#define	splsoft()		_splraise(PSL_S | PSL_IPL1)
#define	splsoftclock()		splsoft()
#define	splsoftnet()		splsoft()
#define	splbio()		_splraise(PSL_S | PSL_IPL2)
#define	splnet()		_splraise(PSL_S | PSL_IPL3)
#define	spltty()		_splraise(PSL_S | PSL_IPL3)
#define	splvm()			_splraise(PSL_S | PSL_IPL3)
#define	splclock()		_splraise(PSL_S | PSL_IPL5)
#define	splstatclock()		_splraise(PSL_S | PSL_IPL5)
#define	splhigh()		_spl(PSL_S | PSL_IPL7)
#define	splsched()		splhigh()

/* watch out for side effects */
#define	splx(s)			((s) & PSL_IPL ? _spl(s) : spl0())

/* locore.s */
int	spl0(void);

#endif /* _KERNEL */
#endif /* _MVME68K_INTR_H_ */
