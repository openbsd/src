/*	$OpenBSD: intr.h,v 1.14 2018/08/20 15:02:07 visa Exp $	*/
/*
 * Copyright (c) 2001 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
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
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
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
int	getipl(void);
int	setipl(int level);
int	splraise(int);
int	spl0(void);

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
#define	splsoftassert(wantipl)	splassert(IPL_SOFTINT)
#else
#define	splassert(wantipl)	do { /* nothing */ } while (0)
#define	splsoftassert(wantipl)	do { /* nothing */ } while (0)
#endif

#endif /* _LOCORE */

#define splsoftclock()		splraise(IPL_SOFTINT)
#define splsoftnet()		splraise(IPL_SOFTINT)
#define splbio()		splraise(IPL_BIO)
#define splnet()		splraise(IPL_NET)
#define spltty()		splraise(IPL_TTY)
#define splclock()		splraise(IPL_CLOCK)
#define splstatclock()		splraise(IPL_STATCLOCK)
#define	splsched()		splraise(IPL_SCHED)
#define splvm()			splraise(IPL_VM)
#define splhigh()		setipl(IPL_HIGH)

#define splx(x)			((x) ? setipl((x)) : spl0())

/*
 * Generic software interrupt support for all m88k platforms.
 */

#ifndef _LOCORE

#define	IPL_SOFT		0
#define	IPL_SOFTCLOCK		1
#define	IPL_SOFTNET		2
#define	IPL_SOFTTTY		3

#define	SI_SOFT			0	/* for IPL_SOFT */
#define	SI_SOFTCLOCK		1	/* for IPL_SOFTCLOCK */
#define	SI_SOFTNET		2	/* for IPL_SOFTNET */
#define	SI_SOFTTTY		3	/* for IPL_SOFTTTY */

#define	SI_NQUEUES		4

#include <machine/mutex.h>
#include <sys/queue.h>

struct soft_intrhand {
	TAILQ_ENTRY(soft_intrhand) sih_list;
	void (*sih_func)(void *);
	void *sih_arg;
	struct soft_intrq *sih_siq;
	int sih_pending;
};

struct soft_intrq {
	TAILQ_HEAD(, soft_intrhand) siq_list;
	int siq_si;
	struct mutex siq_mtx;
};

void	 softintr_disestablish(void *);
void	 softintr_dispatch(int);
void	*softintr_establish(int, void (*)(void *), void *);
void	 softintr_init(void);
void	 softintr_schedule(void *);

extern int softpending;

#endif	/* _LOCORE */

#endif /* _KERNEL */
#endif /* _M88K_INTR_H_ */
