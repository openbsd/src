/*	$OpenBSD: intr.h,v 1.33 2009/10/22 20:39:17 miod Exp $ */

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
 * The interrupt mask cpl is a mask which is used with an external
 * HW mask register.
 * The CPU mask is never changed from the value it gets when interrupt
 * dispatchers are registered.
 *
 * Clock interrupts are always allowed to happen but will not be serviced
 * if logically masked.  The reason for this is that clocks usually sit on
 * INT5 and cannot be easily masked if external HW masking is used.
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
#define	IPL_HIGH	7	/* everything */
#define	NIPLS		8	/* Number of levels */

/* Interrupt sharing types. */
#define	IST_NONE	0	/* none */
#define	IST_PULSE	1	/* pulsed */
#define	IST_EDGE	2	/* edge-triggered */
#define	IST_LEVEL	3	/* level-triggered */

#define	SINTBIT(q)	(31 - (q))
#define	SINTMASK(q)	(1 << SINTBIT(q))

#define	SPL_CLOCK	SINTBIT(SI_NQUEUES)
#define	SPL_CLOCKMASK	SINTMASK(SI_NQUEUES)

/* Soft interrupt masks. */

#define	IPL_SOFT	0
#define	IPL_SOFTCLOCK	1
#define	IPL_SOFTNET	2
#define	IPL_SOFTTTY	3

#define	SI_SOFT		0	/* for IPL_SOFT */
#define	SI_SOFTCLOCK	1	/* for IPL_SOFTCLOCK */
#define	SI_SOFTNET	2	/* for IPL_SOFTNET */
#define	SI_SOFTTTY	3	/* for IPL_SOFTTTY */

#define	SINT_ALLMASK	(SINTMASK(SI_SOFT) | SINTMASK(SI_SOFTCLOCK) | \
			 SINTMASK(SI_SOFTNET) | SINTMASK(SI_SOFTTTY))
#define	SI_NQUEUES	4

#ifndef _LOCORE

#include <machine/mutex.h>
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

/* XXX For legacy software interrupts. */
extern struct soft_intrhand *softnet_intrhand;

#define	setsoftnet()	softintr_schedule(softnet_intrhand)

#define	splsoft()		splraise(imask[IPL_SOFTINT])
#define splbio()		splraise(imask[IPL_BIO])
#define splnet()		splraise(imask[IPL_NET])
#define spltty()		splraise(imask[IPL_TTY])
#define splaudio()		splraise(imask[IPL_AUDIO])
#define splclock()		splraise(imask[IPL_CLOCK])
#define splvm()			splraise(imask[IPL_VM])
#define splsoftclock()		splraise(SINTMASK(SI_SOFTCLOCK) | \
				    SINTMASK(SI_SOFT))
#define splsoftnet()		splraise(SINTMASK(SI_SOFTNET) | \
				    SINTMASK(SI_SOFTCLOCK) | \
				    SINTMASK(SI_SOFT))
#define splstatclock()		splhigh()
#define splsched()		splhigh()
#define spllock()		splhigh()
#define splhigh()		splraise(-1)
#define spl0()			spllower(0)

void	splinit(void);

#define	splassert(X)
#define	splsoftassert(X)

/*
 *  Schedule prioritys for base interrupts (CPU)
 */
#define	INTPRI_CLOCK	1
#define	INTPRI_MACEIO	2	/* O2 I/O interrupt */
#define	INTPRI_XBOWMUX	2	/* Origin 200/2000 I/O interrupt */
#define	INTPRI_MACEAUX	3

#define	INTMASKSIZE	32

extern uint32_t imask[NIPLS];

/* Inlines */
static __inline void register_splx_handler(void (*)(int));

typedef void (int_f) (int);
extern int_f *splx_hand;

static __inline void
register_splx_handler(void(*handler)(int))
{
	splx_hand = handler;
}

int	splraise(int);
void	splx(int);
int	spllower(int);

/*
 * Interrupt control struct used by interrupt dispatchers
 * to hold interrupt handler info.
 */

#include <sys/evcount.h>

struct intrhand {
	struct	intrhand *ih_next;
	int	(*ih_fun)(void *);
	void	*ih_arg;
	int	ih_level;
	int	ih_irq;
	void	*frame;
	struct evcount ih_count;
};

extern struct intrhand *intrhand[INTMASKSIZE];

/*
 * Low level interrupt dispatcher registration data.
 */
#define NLOWINT	16		/* Number of low level registrations possible */

struct trap_frame;

extern uint32_t idle_mask;
extern int last_low_int;

void set_intr(int, uint32_t, uint32_t(*)(uint32_t, struct trap_frame *));

void	hw_setintrmask(uint32_t);
u_int32_t updateimask(uint32_t);
void	dosoftint(uint32_t);

#endif /* _LOCORE */

#endif /* _MACHINE_INTR_H_ */
