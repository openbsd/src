/*	$OpenBSD: softintr.h,v 1.3 2022/01/16 23:05:48 jsg Exp $	*/
/*	$NetBSD: softintr.h,v 1.1 2002/01/29 22:54:14 thorpej Exp $	*/

/*-
 * Copyright (c) 1998, 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum, and by Jason R. Thorpe.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef	_MACHINE_SOFTINTR_H_
#define	_MACHINE_SOFTINTR_H_

#ifdef _KERNEL

#include <sys/mutex.h>
#include <sys/queue.h>

/*
 * Generic software interrupt support.
 *
 * To use this code, include <machine/softintr.h> from your platform's
 * <machine/intr.h>.
 */

#define	SIR_SOFT	0	/* for IPL_SOFT */
#define	SIR_CLOCK	1	/* for IPL_SOFTCLOCK */
#define	SIR_NET		2	/* for IPL_SOFTNET */
#define	SIR_TTY		3	/* for IPL_SOFTTTY */

#define	SI_NSOFTINTR		4

struct soft_intrhand {
	TAILQ_ENTRY(soft_intrhand)
		sih_q;
	struct soft_intr *sih_intrhead;
	void	(*sih_fn)(void *);
	void	(*sih_fnwrap)(void *);
	void	*sih_arg;
	void	*sih_argwrap;
	int	sih_pending;
};

struct soft_intr {
	TAILQ_HEAD(, soft_intrhand)
			softintr_q;
	int		softintr_ssir;
	struct mutex	softintr_lock;
};

#define SOFTINTR_ESTABLISH_MPSAFE	0x01
void    *softintr_establish_flags(int, void (*)(void *), void *, int);
#define softintr_establish(i, f, a)					\
	softintr_establish_flags(i, f, a, 0)
#define softintr_establish_mpsafe(i, f, a)				\
	softintr_establish_flags(i, f, a, SOFTINTR_ESTABLISH_MPSAFE)
void    softintr_disestablish(void *);
void    softintr_init(void);
void    softintr_dispatch(int);
void    softintr(int);

#define softintr_schedule(arg)						\
do {									\
	struct soft_intrhand *__sih = (arg);				\
	struct soft_intr *__si = __sih->sih_intrhead;			\
									\
	mtx_enter(&__si->softintr_lock);				\
	if (__sih->sih_pending == 0) {					\
		TAILQ_INSERT_TAIL(&__si->softintr_q, __sih, sih_q);	\
		__sih->sih_pending = 1;					\
		softintr(__si->softintr_ssir);				\
	}								\
	mtx_leave(&__si->softintr_lock);				\
} while (/*CONSTCOND*/ 0)

#endif /* _KERNEL */

#endif	/* _MACHINE_SOFTINTR_H_ */
