/*	$OpenBSD: intr.h,v 1.1 2000/01/06 03:27:00 smurph Exp $	*/
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
u_long	allocate_sir __P((void (*proc)(), void *arg));

#define _spl(s) \
({ \
	register int _spl_r; \
\
	__asm __volatile ("clrl %0; movew sr,%0; movew %1,sr" : \
		"=&d" (_spl_r) : "di" (s)); \
	_spl_r; \
})

/* spl0 requires checking for software interrupts */
#define	spl1()	_spl(PSL_S|PSL_IPL1)
#define	spl2()	_spl(PSL_S|PSL_IPL2)
#define	spl3()	_spl(PSL_S|PSL_IPL3)
#define	spl4()	_spl(PSL_S|PSL_IPL4)
#define	spl5()	_spl(PSL_S|PSL_IPL5)
#define	spl6()	_spl(PSL_S|PSL_IPL6)
#define	spl7()	_spl(PSL_S|PSL_IPL7)

#define	splsoftclock()	spl1()
#define	splsoftnet()	spl1()
#define	splbio()	spl2()
#define	splnet()	spl3()
#define	splimp()	spl3()
#define	spltty()	spl3()
#define	splclock()	spl5()
#define	splstatclock()	spl5()
#define	splhigh()	spl7()
#define	splsched()	spl7()

/* watch out for side effects */
#define	splx(s)		(s & PSL_IPL ? _spl(s) : spl0())

/* locore.s */
int	spl0 __P((void));
#endif /* _KERNEL */
#endif /* _MVME68K_INTR_H_ */
