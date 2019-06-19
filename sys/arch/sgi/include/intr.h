/*	$OpenBSD: intr.h,v 1.51 2018/08/20 15:02:07 visa Exp $ */

/*
 * Copyright (c) 2001-2004 Opsycon AB  (www.opsycon.se / www.opsycon.com)
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#ifndef _MACHINE_INTR_H_
#define _MACHINE_INTR_H_

/*
 * The interrupt level ipl is a logical level; per-platform interrupt
 * code will turn it into the appropriate hardware interrupt masks
 * values.
 *
 * Interrupt sources on the CPU are kept enabled regardless of the
 * current ipl value; individual hardware sources interrupting while
 * logically masked are masked on the fly, remembered as pending, and
 * unmasked at the first splx() opportunity.
 *
 * An exception to this rule is the clock interrupt. Clock interrupts
 * are always allowed to happen, but will (of course!) not be serviced
 * if logically masked.  The reason for this is that clocks usually sit on
 * INT5 and cannot be easily masked if external hardware masking is used.
 */

/* Interrupt priority `levels'; not mutually exclusive. */
#define	IPL_NONE	0	/* nothing */
#define	IPL_SOFTINT	1	/* soft interrupts */
#define	IPL_BIO		2	/* block I/O */
#define IPL_AUDIO	IPL_BIO
#define	IPL_NET		3	/* network */
#define	IPL_TTY		4	/* terminal */
#define	IPL_VM		5	/* memory allocation */
#define	IPL_CLOCK	6	/* clock */
#define	IPL_SCHED	IPL_CLOCK
#define	IPL_HIGH	7	/* everything */
#define	IPL_IPI         8       /* interprocessor interrupt */
#define	NIPLS		9	/* Number of levels */

#define IPL_MPFLOOR	IPL_TTY

/* Interrupt priority 'flags'. */
#define	IPL_MPSAFE	0x100

/* Interrupt sharing types. */
#define	IST_NONE	0	/* none */
#define	IST_PULSE	1	/* pulsed */
#define	IST_EDGE	2	/* edge-triggered */
#define	IST_LEVEL	3	/* level-triggered */

#define	SINTBIT(q)	(q)
#define	SINTMASK(q)	(1 << SINTBIT(q))

/* Soft interrupt masks. */

#define	IPL_SOFT	0
#define	IPL_SOFTCLOCK	1
#define	IPL_SOFTNET	2
#define	IPL_SOFTTTY	3

#define	SI_SOFT		0	/* for IPL_SOFT */
#define	SI_SOFTCLOCK	1	/* for IPL_SOFTCLOCK */
#define	SI_SOFTNET	2	/* for IPL_SOFTNET */
#define	SI_SOFTTTY	3	/* for IPL_SOFTTTY */

#define	SI_NQUEUES	4

#ifndef _LOCORE

#include <sys/mutex.h>
#include <sys/queue.h>

struct soft_intrhand {
	TAILQ_ENTRY(soft_intrhand) sih_list;
	void	(*sih_func)(void *);
	void	*sih_arg;
	struct soft_intrq *sih_siq;
	int	sih_pending;
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

#define	splsoft()	splraise(IPL_SOFTINT)
#define splbio()	splraise(IPL_BIO)
#define splnet()	splraise(IPL_NET)
#define spltty()	splraise(IPL_TTY)
#define splaudio()	splraise(IPL_AUDIO)
#define splvm()		splraise(IPL_VM)
#define splclock()	splraise(IPL_CLOCK)
#define splsched()	splraise(IPL_SCHED)
#define splhigh()	splraise(IPL_HIGH)

#define splsoftclock()	splsoft()
#define splsoftnet()	splsoft()
#define splstatclock()	splhigh()

#define spl0()		spllower(0)

void	splinit(void);

#define	splassert(X)
#define	splsoftassert(X)

void	register_splx_handler(void (*)(int));
int	splraise(int);
void	splx(int);
int	spllower(int);

/*
 * Interrupt control struct used by interrupt dispatchers
 * to hold interrupt handler info.
 */

#include <sys/evcount.h>

struct intrhand {
	struct	intrhand	*ih_next;
	int			(*ih_fun)(void *);
	void			*ih_arg;
	int			 ih_level;
	int			 ih_irq;
	struct evcount		 ih_count;
	int			 ih_flags;
#define	IH_ALLOCATED		0x01
#define	IH_MPSAFE		0x02
};

void	intr_barrier(void *);

/*
 * Low level interrupt dispatcher registration data.
 */

/* Schedule priorities for base interrupts (CPU) */
#define	INTPRI_IPI	0
#define	INTPRI_CLOCK	1
/* other values are system-specific */

#define NLOWINT	16		/* Number of low level registrations possible */

extern uint32_t idle_mask;

struct trapframe;
void	set_intr(int, uint32_t, uint32_t(*)(uint32_t, struct trapframe *));

uint32_t updateimask(uint32_t);
void	dosoftint(void);

#ifdef MULTIPROCESSOR
#if defined (TGT_OCTANE)
#define ENABLEIPI() updateimask(~CR_INT_2) /* enable IPI interrupt level */
#elif defined (TGT_ORIGIN)
#define ENABLEIPI() updateimask(~CR_INT_0) /* enable IPI interrupt level */
#else
#error MULTIPROCESSOR kernel not supported on this configuration
#endif
#endif

#endif /* _LOCORE */

#endif /* _MACHINE_INTR_H_ */
