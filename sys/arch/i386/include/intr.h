/*	$OpenBSD: intr.h,v 1.21 2004/06/13 21:49:16 niklas Exp $	*/
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

#ifndef _I386_INTR_H_
#define _I386_INTR_H_

#include <machine/intrdefs.h>

#ifndef _LOCORE

#ifdef MULTIPROCESSOR
#include <machine/i82489reg.h>
#include <machine/i82489var.h>
#include <machine/cpu.h>
#endif

extern volatile u_int32_t lapic_tpr;	/* Current interrupt priority level. */

extern volatile u_int32_t ipending;	/* Interrupts pending. */
extern int imask[];	/* Bitmasks telling what interrupts are blocked. */
extern int iunmask[];	/* Bitmasks telling what interrupts are accepted. */

#define IMASK(level) imask[IPL(level)]
#define IUNMASK(level) iunmask[IPL(level)]

extern void Xspllower(void);

extern int splraise(int);
extern int spllower(int);
extern void splx(int);
extern void softintr(int, int);

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
#define splassert(wantipl) do { /* nada */ } while (0)
#endif

/*
 * Hardware interrupt masks
 */
#define	splbio()	splraise(IPL_BIO)
#define	splnet()	splraise(IPL_NET)
#define	spltty()	splraise(IPL_TTY)
#define	splaudio()	splraise(IPL_AUDIO)
#define	splclock()	splraise(IPL_CLOCK)
#define	splstatclock()	splhigh()
#define splipi()	splraise(IPL_IPI)

/*
 * Software interrupt masks
 *
 * NOTE: spllowersoftclock() is used by hardclock() to lower the priority from
 * clock to softclock before it calls softclock().
 */
#define	spllowersoftclock()	spllower(IPL_SOFTCLOCK)
#define	splsoftclock()		splraise(IPL_SOFTCLOCK)
#define	splsoftnet()		splraise(IPL_SOFTNET)
#define	splsofttty()		splraise(IPL_SOFTTTY)

/*
 * Miscellaneous
 */
#define	splvm()		splraise(IPL_VM)
#define splimp()	splvm()
#define	splhigh()	splraise(IPL_HIGH)
#define	splsched()	splraise(IPL_SCHED)
#define spllock() 	splhigh()
#define	spl0()		spllower(IPL_NONE)

#define	setsoftast()	(astpending = 1)
#define	setsoftclock()	softintr(1 << SIR_CLOCK, IPL_SOFTCLOCK)
#define	setsoftnet()	softintr(1 << SIR_NET, IPL_SOFTNET)
#define	setsofttty()	softintr(1 << SIR_TTY, IPL_SOFTTTY)

#define I386_IPI_HALT		0x00000001
#define I386_IPI_MICROSET	0x00000002
#define I386_IPI_FLUSH_FPU	0x00000004
#define I386_IPI_SYNCH_FPU	0x00000008
#define I386_IPI_TLB		0x00000010
#define I386_IPI_MTRR		0x00000020
#define I386_IPI_GDT		0x00000040
#define I386_IPI_DDB		0x00000080	/* synchronize while in ddb */

#define I386_NIPI	8

struct cpu_info;

#ifdef MULTIPROCESSOR
int i386_send_ipi(struct cpu_info *, int);
void i386_broadcast_ipi(int);
void i386_multicast_ipi(int, int);
void i386_ipi_handler(void);
void i386_intlock(struct intrframe);
void i386_intunlock(struct intrframe);
void i386_softintlock(void);
void i386_softintunlock(void);

extern void (*ipifunc[I386_NIPI])(struct cpu_info *);
#endif

#endif /* !_LOCORE */

#endif /* !_I386_INTR_H_ */
