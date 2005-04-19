/*	$OpenBSD: intr.h,v 1.11 2005/04/19 15:29:47 mickey Exp $	*/
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

#ifdef _KERNEL

/*
 * simulated software interrupt register
 */
extern unsigned char ssir;

#define SIR_NET		0x1
#define SIR_CLOCK	0x2

#define setsoftint(x)	ssir |= (x)
#define setsoftnet()	ssir |= SIR_NET
#define setsoftclock()	ssir |= SIR_CLOCK
u_long	allocate_sir(void (*proc)(void *), void *arg);

#define _spl(s) \
({ \
	register int _spl_r; \
\
	__asm __volatile ("clrl %0; movew sr,%0; movew %1,sr" : \
		"=&d" (_spl_r) : "di" (s)); \
	_spl_r; \
})

#define	_splraise(s)							\
({									\
	int _spl_r;							\
									\
	__asm __volatile ("						\
		clrl	d0					;	\
		movw	sr,d0					;	\
		movl	d0,%0					;	\
		andw	#0x700,d0				;	\
		movw	%1,d1					;	\
		andw	#0x700,d1				;	\
		cmpw	d0,d1					;	\
		jle	1f					;	\
		movw	%1,sr					;	\
	    1:"							:	\
		    "=&d" (_spl_r)				:	\
		    "di" (s)					:	\
		    "d0", "d1");					\
	_spl_r;								\
})

/* spl0 requires checking for software interrupts */
#define	spl1()	_spl(PSL_S|PSL_IPL1)
#define	spl2()	_spl(PSL_S|PSL_IPL2)
#define	spl3()	_spl(PSL_S|PSL_IPL3)
#define	spl4()	_spl(PSL_S|PSL_IPL4)
#define	spl5()	_spl(PSL_S|PSL_IPL5)
#define	spl6()	_spl(PSL_S|PSL_IPL6)
#define	spl7()	_spl(PSL_S|PSL_IPL7)

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

#define	splsoftclock()		spl1()
#define	splsoftnet()		spl1()
#define	splbio()		_splraise(PSL_S|PSL_IPL2)
#define	splnet()		_splraise(PSL_S|PSL_IPL3)
#define	splimp()		_splraise(PSL_S|PSL_IPL3)
#define	spltty()		_splraise(PSL_S|PSL_IPL3)
#define	splvm()			splimp()
#define	splclock()		_splraise(PSL_S|PSL_IPL5)
#define	splstatclock()		_splraise(PSL_S|PSL_IPL5)
#define	splhigh()		spl7()

/* watch out for side effects */
#define	splx(s)		(s & PSL_IPL ? _spl(s) : spl0())

/* locore.s */
int	spl0(void);
#endif /* _KERNEL */
#endif /* _MVME68K_INTR_H_ */
