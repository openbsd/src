/*	$OpenBSD: intr.h,v 1.5 2005/10/12 20:53:22 miod Exp $	*/
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

#ifndef _M88K_INTR_H_
#define _M88K_INTR_H_

#ifdef _KERNEL
#ifndef _LOCORE
unsigned setipl(unsigned level);
unsigned raiseipl(unsigned level);
int spl0(void);

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

#endif /* _LOCORE */

#define splsoftclock()		raiseipl(IPL_SOFTCLOCK)
#define splsoftnet()		raiseipl(IPL_SOFTNET)
#define splbio()		raiseipl(IPL_BIO)
#define splnet()		raiseipl(IPL_NET)
#define spltty()		raiseipl(IPL_TTY)
#define splclock()		raiseipl(IPL_CLOCK)
#define splstatclock()		raiseipl(IPL_STATCLOCK)
#define	splsched()		raiseipl(IPL_SCHED)
#define splimp()		raiseipl(IPL_IMP)
#define splvm()			raiseipl(IPL_VM)
#define splhigh()		setipl(IPL_HIGH)

#define splx(x)			((x) ? setipl((x)) : spl0())

#endif /* _KERNEL */
#endif /* _M88K_INTR_H_ */
