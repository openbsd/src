/*	$OpenBSD: intr.h,v 1.10 2003/01/03 23:17:42 miod Exp $	*/
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

#ifndef _MVME88K_INTR_H_
#define _MVME88K_INTR_H_
/*
 * INTERRUPT STAT levels.  for 'systat vmstat'
 * intrcnt and friends are defined in locore.S
 * XXX smurph
 */

#ifndef _LOCORE

#define M88K_NIRQ	12

#define M88K_SPUR_IRQ	0
#define M88K_LEVEL1_IRQ	1
#define M88K_LEVEL2_IRQ	2
#define M88K_LEVEL3_IRQ	3
#define M88K_LEVEL4_IRQ	4
#define M88K_LEVEL5_IRQ	5
#define M88K_LEVEL6_IRQ	6
#define M88K_LEVEL7_IRQ	7
/* 
 * We keep track of these separately, but   
 * they will be reflected with the above also.
 */
#define M88K_CLK_IRQ	8
#define M88K_SCLK_IRQ	9
#define M88K_PCLK_IRQ	10
#define M88K_NMI_IRQ	11

extern int intrcnt[M88K_NIRQ];

#endif 

/*
 * IPL levels.
 * We use 6 as IPL_HIGH so that abort can be programmed at 7 so that
 * it is always possible to break into the system unless interrupts
 * are disabled.
 */

#define IPL_NONE	0
#define IPL_SOFTCLOCK	1
#define IPL_SOFTNET	1
#define IPL_BIO		2
#define IPL_NET		3
#define IPL_IMP		3
#define IPL_TTY		3
#define IPL_VM		3
#define IPL_CLOCK	5
#define IPL_STATCLOCK	5
#define IPL_HIGH	6
#define IPL_NMI		7
#define IPL_ABORT	7

#ifdef _KERNEL
#ifndef _LOCORE
unsigned setipl(unsigned level);
#ifdef DDB
unsigned db_setipl(unsigned level);
#endif 
int spl0(void);
#endif /* _LOCORE */

/* needs major cleanup - XXX nivas */

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
#define	splassert(wantipl)	do { /* nothing */ } while (0)
#endif

#if 0
spl0 is a function by itself. I really am serious about the clean up
above...
#define spl0()		spln(0)
#endif /* 0 */
#define spl1()		setipl(1)
#define spl2()		setipl(2)
#define spl3()		setipl(3)
#define spl4()		setipl(4)
#define spl5()		setipl(5)
#define spl6()		setipl(6)
#define spl7()		setipl(7)

#define splnone			spl0
#define spllowersoftclock()	setipl(IPL_SOFTCLOCK)
#define splsoftclock()		setipl(IPL_SOFTCLOCK)
#define splsoftnet()		setipl(IPL_SOFTNET)
#define splbio()		setipl(IPL_BIO)
#define splnet()		setipl(IPL_NET)
#define spltty()		setipl(IPL_TTY)
#define splclock()		setipl(IPL_CLOCK)
#define splstatclock()		setipl(IPL_STATCLOCK)
#define splimp()		setipl(IPL_IMP)
#define splvm()			setipl(IPL_VM)
#define splhigh()		setipl(IPL_HIGH)

#define splx(x)		((x) ? setipl((x)) : spl0())

#ifdef DDB
#define db_splx(x)	db_setipl((x))
#define db_splhigh()    db_setipl(IPL_HIGH)
#endif /* DDB */

#endif /* _KERNEL */
#endif /* _MVME88K_INTR_H_ */
