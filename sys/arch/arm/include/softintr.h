/*	$OpenBSD: softintr.h,v 1.2 2007/05/10 17:59:24 deraadt Exp $	*/
/*	$NetBSD: softintr.h,v 1.1 2002/01/29 22:54:14 thorpej Exp $	*/

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

#ifndef	_ARM_SOFTINTR_H_
#define	_ARM_SOFTINTR_H_

#ifdef _KERNEL

/*
 * Generic software interrupt support for all ARM platforms.
 *
 * To use this code, include <arm/softintr.h> from your platform's
 * <machine/intr.h>.
 */

#define	SI_SOFT			0	/* for IPL_SOFT */
#define	SI_SOFTCLOCK		1	/* for IPL_SOFTCLOCK */
#define	SI_SOFTNET		2	/* for IPL_SOFTNET */
#define	SI_SOFTSERIAL		3	/* for IPL_SOFTSERIAL */

#define	SI_NQUEUES		4

#define	SI_QUEUENAMES {							\
	"generic",							\
	"clock",							\
	"net",								\
	"serial",							\
}

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
};

void	*softintr_establish(int, void (*)(void *), void *);
void	softintr_disestablish(void *);
void	softintr_init(void);
void	softintr_dispatch(int);

#define	softintr_schedule(arg)						\
do {									\
	struct soft_intrhand *__sih = (arg);				\
	struct soft_intrq *__siq = __sih->sih_siq;			\
	int __s;							\
									\
	__s = splhigh();						\
	if (__sih->sih_pending == 0) {					\
		TAILQ_INSERT_TAIL(&__siq->siq_list, __sih, sih_list);	\
		__sih->sih_pending = 1;					\
		_setsoftintr(__siq->siq_si);				\
	}								\
	splx(__s);							\
} while (/*CONSTCOND*/0)

/* XXX For legacy software interrupts. */
extern struct soft_intrhand *softnet_intrhand;

#define	setsoftnet()	softintr_schedule(softnet_intrhand)

#endif /* _KERNEL */

#endif	/* _ARM_SOFTINTR_H_ */
