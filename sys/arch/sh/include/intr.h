/*	$OpenBSD: intr.h,v 1.7 2009/04/19 18:54:06 oga Exp $	*/
/*	$NetBSD: intr.h,v 1.22 2006/01/24 23:51:42 uwe Exp $	*/

/*-
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
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

#ifndef _SH_INTR_H_
#define	_SH_INTR_H_

#ifdef	_KERNEL

#include <sys/device.h>
#include <sys/evcount.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/queue.h>
#include <sh/psl.h>

/* Interrupt sharing types. */
#define	IST_NONE		0	/* none */
#define	IST_PULSE		1	/* pulsed */
#define	IST_EDGE		2	/* edge-triggered */
#define	IST_LEVEL		3	/* level-triggered */

/* Interrupt priority levels */
#define	_IPL_N		15
#define	_IPL_NSOFT	4

#define	IPL_NONE	0	/* nothing */
#define	IPL_SOFT	1
#define	IPL_SOFTCLOCK	2	/* timeouts */
#define	IPL_SOFTNET	3	/* protocol stacks */
#define	IPL_SOFTSERIAL	4	/* serial */

#define	IPL_SOFTNAMES {							\
	"misc",								\
	"clock",							\
	"net",								\
	"serial",							\
}

struct intc_intrhand {
	int	(*ih_func)(void *);
	void	*ih_arg;
	int	ih_level;	/* SR.I[0:3] value */
	int	ih_evtcode;	/* INTEVT or INTEVT2(SH7709/SH7709A) */
	int	ih_idx;		/* evtcode -> intrhand mapping */
	int	ih_irq;
	struct evcount ih_count;
	const char *ih_name;
};

/* from 0x200 by 0x20 -> from 0 by 1 */
#define	EVTCODE_TO_MAP_INDEX(x)		(((x) >> 5) - 0x10)
#define	EVTCODE_TO_IH_INDEX(x)						\
	__intc_evtcode_to_ih[EVTCODE_TO_MAP_INDEX(x)]
#define	EVTCODE_IH(x)	(&__intc_intrhand[EVTCODE_TO_IH_INDEX(x)])
extern int8_t __intc_evtcode_to_ih[];
extern struct intc_intrhand __intc_intrhand[];

void intc_init(void);
void *intc_intr_establish(int, int, int, int (*)(void *), void *, const char *);
void intc_intr_disestablish(void *);
void intc_intr_enable(int);
void intc_intr_disable(int);
void intc_intr(int, int, int);

void intpri_intr_priority(int evtcode, int level);

/*
 * software simulated interrupt
 */
struct sh_soft_intrhand {
	TAILQ_ENTRY(sh_soft_intrhand) sih_q;
	struct sh_soft_intr *sih_intrhead;
	void	(*sih_fn)(void *);
	void	*sih_arg;
	int	sih_pending;
};

struct sh_soft_intr {
	TAILQ_HEAD(, sh_soft_intrhand)
			softintr_q;
	unsigned long	softintr_ipl;
	struct mutex	softintr_lock;
};

void	 softintr_disestablish(void *);
void	 softintr_dispatch(int);
void	*softintr_establish(int, void (*)(void *), void *);
void	 softintr_init(void);
void	 softintr_schedule(void *);

/* XXX For legacy software interrupts. */
extern struct sh_soft_intrhand *softnet_intrhand;

#define	setsoftnet()	softintr_schedule(softnet_intrhand)

#endif	/* _KERNEL */

#endif /* !_SH_INTR_H_ */
