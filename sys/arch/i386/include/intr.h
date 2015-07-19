/*	$OpenBSD: intr.h,v 1.46 2015/07/19 18:53:49 sf Exp $	*/
/*	$NetBSD: intr.h,v 1.5 1996/05/13 06:11:28 mycroft Exp $	*/

/*
 * Copyright (c) 1996 Charles M. Hannum.  All rights reserved.
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
 *	This product includes software developed by Charles M. Hannum.
 * 4. The name of the author may not be used to endorse or promote products
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

#ifndef _MACHINE_INTR_H_
#define _MACHINE_INTR_H_

#include <machine/intrdefs.h>

#ifndef _LOCORE
#include <sys/mutex.h>
#include <machine/cpu.h>

extern volatile u_int32_t lapic_tpr;	/* Current interrupt priority level. */

extern int imask[];	/* Bitmasks telling what interrupts are blocked. */
extern int iunmask[];	/* Bitmasks telling what interrupts are accepted. */

#define IMASK(level) imask[IPL(level)]
#define IUNMASK(level) iunmask[IPL(level)]

extern void Xspllower(void);

extern int splraise(int);
extern int spllower(int);
extern void splx(int);
extern void softintr(int);

/*
 * compiler barrier: prevent reordering of instructions.
 * XXX something similar will move to <sys/cdefs.h>
 * or thereabouts.
 * This prevents the compiler from reordering code around
 * this "instruction", acting as a sequence point for code generation.
 */

#define	__splbarrier() __asm volatile("":::"memory")

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
#define splsoftassert(wantipl) splassert(wantipl)
#else
#define splassert(wantipl)	do { /* nada */ } while (0)
#define splsoftassert(wantipl)	do { /* nada */ } while (0)
#endif

/*
 * Define the splraise and splx code in macros, so that the code can be
 * reused in a profiling build in a way that does not cause recursion.
 */
#define _SPLRAISE(ocpl, ncpl) 		\
	ocpl = lapic_tpr;		\
	if (ncpl > ocpl)		\
		lapic_tpr = ncpl


#define _SPLX(ncpl) 			\
	lapic_tpr = ncpl;		\
	if (curcpu()->ci_ipending & IUNMASK(ncpl))	\
		Xspllower()

/*
 * Hardware interrupt masks
 */
#define	splbio()	splraise(IPL_BIO)
#define	splnet()	splraise(IPL_NET)
#define	spltty()	splraise(IPL_TTY)
#define	splaudio()	splraise(IPL_AUDIO)
#define	splclock()	splraise(IPL_CLOCK)
#define	splstatclock()	splclock()
#define splipi()	splraise(IPL_IPI)

/*
 * Software interrupt masks
 */
#define	splsoftclock()		splraise(IPL_SOFTCLOCK)
#define	splsoftnet()		splraise(IPL_SOFTNET)
#define	splsofttty()		splraise(IPL_SOFTTTY)

/*
 * Miscellaneous
 */
#define	splvm()		splraise(IPL_VM)
#define	splhigh()	splraise(IPL_HIGH)
#define	splsched()	splraise(IPL_SCHED)
#define spllock() 	splhigh()
#define	spl0()		spllower(IPL_NONE)

#include <machine/pic.h>

struct cpu_info;

#ifdef MULTIPROCESSOR
void i386_send_ipi(struct cpu_info *, int);
int i386_fast_ipi(struct cpu_info *, int);
void i386_broadcast_ipi(int);
void i386_ipi_handler(void);
void i386_setperf_ipi(struct cpu_info *);

extern void (*ipifunc[I386_NIPI])(struct cpu_info *);
#endif

#endif /* !_LOCORE */

/*
 * Generic software interrupt support.
 */

#define	I386_SOFTINTR_SOFTCLOCK		0
#define	I386_SOFTINTR_SOFTNET		1
#define	I386_SOFTINTR_SOFTTTY		2
#define	I386_NSOFTINTR			3

#ifndef _LOCORE
#include <sys/queue.h>

struct i386_soft_intrhand {
	TAILQ_ENTRY(i386_soft_intrhand)
		sih_q;
	struct i386_soft_intr *sih_intrhead;
	void	(*sih_fn)(void *);
	void	*sih_arg;
	int	sih_pending;
};

struct i386_soft_intr {
	TAILQ_HEAD(, i386_soft_intrhand)
			softintr_q;
	int		softintr_ssir;
	struct mutex	softintr_lock;
};

void	*softintr_establish(int, void (*)(void *), void *);
void	softintr_disestablish(void *);
void	softintr_init(void);
void	softintr_dispatch(int);

#define	softintr_schedule(arg)						\
do {									\
	struct i386_soft_intrhand *__sih = (arg);			\
	struct i386_soft_intr *__si = __sih->sih_intrhead;		\
									\
	mtx_enter(&__si->softintr_lock);				\
	if (__sih->sih_pending == 0) {					\
		TAILQ_INSERT_TAIL(&__si->softintr_q, __sih, sih_q);	\
		__sih->sih_pending = 1;					\
		softintr(__si->softintr_ssir);				\
	}								\
	mtx_leave(&__si->softintr_lock);				\
} while (/*CONSTCOND*/ 0)
#endif /* _LOCORE */

#endif /* !_MACHINE_INTR_H_ */
