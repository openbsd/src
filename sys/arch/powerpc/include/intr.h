/*	$OpenBSD: intr.h,v 1.46 2011/01/08 18:10:20 deraadt Exp $ */

/*
 * Copyright (c) 1997 Per Fogelstrom, Opsycon AB and RTMX Inc, USA.
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
 *	This product includes software developed under OpenBSD by
 *	Per Fogelstrom, Opsycon AB, Sweden for RTMX Inc, North Carolina USA.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

#ifndef _POWERPC_INTR_H_
#define _POWERPC_INTR_H_

#define	IPL_NONE	0
#define	IPL_BIO		1
#define	IPL_AUDIO	IPL_BIO /* XXX - was defined this val in audio_if.h */
#define	IPL_NET		2
#define	IPL_TTY		3
#define	IPL_VM		4
#define	IPL_CLOCK	5
#define	IPL_SCHED	6
#define	IPL_HIGH	6
#define	IPL_NUM		7

#define	IST_NONE	0
#define	IST_PULSE	1
#define	IST_EDGE	2
#define	IST_LEVEL	3

#if defined(_KERNEL) && !defined(_LOCORE)

#include <sys/evcount.h>
#include <machine/atomic.h>

#define PPC_NIRQ	66
#define PPC_CLK_IRQ	64
#define PPC_STAT_IRQ	65

int	splraise(int);
int	spllower(int);
void	splx(int);


void do_pending_int(void);

extern int cpu_imask[IPL_NUM];

/* SPL asserts */
#define	splassert(wantipl)	/* nothing */
#define	splsoftassert(wantipl)	/* nothing */

#define SINTBIT(q)	(31 - (q))
#define SINTMASK(q)	(1 << SINTBIT(q))

#define	SPL_CLOCKMASK	SINTMASK(SI_NQUEUES)

/* Soft interrupt masks. */

#define	IPL_SOFTCLOCK	0
#define	IPL_SOFTNET	1
#define	IPL_SOFTTTY	2

#define	SI_SOFTCLOCK	0	/* for IPL_SOFTCLOCK */
#define	SI_SOFTNET	1	/* for IPL_SOFTNET */
#define	SI_SOFTTTY	2	/* for IPL_SOFTTY */

#define	SINT_ALLMASK	(SINTMASK(SI_SOFTCLOCK) | \
			 SINTMASK(SI_SOFTNET) | SINTMASK(SI_SOFTTTY))
#define	SI_NQUEUES	3

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

#define	SINT_CLOCK	SINTMASK(SI_SOFTCLOCK)
#define	SINT_NET	SINTMASK(SI_SOFTNET)
#define	SINT_TTY	SINTMASK(SI_SOFTTTY)

#define splbio()	splraise(cpu_imask[IPL_BIO])
#define splnet()	splraise(cpu_imask[IPL_NET])
#define spltty()	splraise(cpu_imask[IPL_TTY])
#define splaudio()	splraise(cpu_imask[IPL_AUDIO])
#define splclock()	splraise(cpu_imask[IPL_CLOCK])
#define splvm()		splraise(cpu_imask[IPL_VM])
#define splsched()	splhigh()
#define spllock()	splhigh()
#define splstatclock()	splhigh()
#define	splsoftclock()	splraise(SINT_CLOCK)
#define	splsoftnet()	splraise(SINT_NET|SINT_CLOCK)
#define	splsofttty()	splraise(SINT_TTY|SINT_NET|SINT_CLOCK)

#define	splhigh()	splraise(0xffffffff)
#define	spl0()		spllower(0)

/*
 *	Interrupt control struct used to control the ICU setup.
 */

struct intrhand {
	struct intrhand	*ih_next;
	int		(*ih_fun)(void *);
	void		*ih_arg;
	struct evcount	ih_count;
	int		ih_level;
	int		ih_irq;
	const char	*ih_what;
};
extern int ppc_configed_intr_cnt;
#define MAX_PRECONF_INTR 16
extern struct intrhand ppc_configed_intr[MAX_PRECONF_INTR];
void softnet(int isr);

#define PPC_IPI_NOP		0
#define PPC_IPI_DDB		1

void ppc_send_ipi(struct cpu_info *, int);

#endif /* _LOCORE */
#endif /* _POWERPC_INTR_H_ */
