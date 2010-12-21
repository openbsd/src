/* $OpenBSD: intr.h,v 1.37 2010/12/21 14:56:23 claudio Exp $ */
/* $NetBSD: intr.h,v 1.26 2000/06/03 20:47:41 thorpej Exp $ */

/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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

/*
 * Copyright (c) 1997 Christopher G. Demetriou.  All rights reserved.
 * Copyright (c) 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#ifndef _ALPHA_INTR_H_
#define _ALPHA_INTR_H_

#include <sys/evcount.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/queue.h>
#include <machine/atomic.h>

/*
 * The Alpha System Control Block.  This is 8k long, and you get
 * 16 bytes per vector (i.e. the vector numbers are spaced 16
 * apart).
 *
 * This is sort of a "shadow" SCB -- rather than the CPU jumping
 * to (SCBaddr + (16 * vector)), like it does on the VAX, we get
 * a vector number in a1.  We use the SCB to look up a routine/arg
 * and jump to it.
 *
 * Since we use the SCB only for I/O interrupts, we make it shorter
 * than normal, starting it at vector 0x800 (the start of the I/O
 * interrupt vectors).
 */
#define	SCB_IOVECBASE	0x0800
#define	SCB_VECSIZE	0x0010
#define	SCB_SIZE	0x2000

#define	SCB_VECTOIDX(x)	((x) >> 4)
#define	SCB_IDXTOVEC(x)	((x) << 4)

#define	SCB_NIOVECS	SCB_VECTOIDX(SCB_SIZE - SCB_IOVECBASE)

struct scbvec { 
	void	(*scb_func)(void *, u_long);
	void	*scb_arg;
};

/*
 * Alpha interrupts come in at one of 4 levels:
 *
 *	software interrupt level
 *	i/o level 1
 *	i/o level 2
 *	clock level
 *
 * However, since we do not have any way to know which hardware
 * level a particular i/o interrupt comes in on, we have to
 * whittle it down to 3.
 */

#define	IPL_NONE	ALPHA_PSL_IPL_0
#define	IPL_SOFTINT	ALPHA_PSL_IPL_SOFT
#define	IPL_BIO		ALPHA_PSL_IPL_IO
#define	IPL_NET		ALPHA_PSL_IPL_IO
#define	IPL_TTY		ALPHA_PSL_IPL_IO
#define	IPL_SERIAL	ALPHA_PSL_IPL_IO
#define	IPL_AUDIO	ALPHA_PSL_IPL_IO
#define	IPL_VM		ALPHA_PSL_IPL_IO
#define	IPL_CLOCK	ALPHA_PSL_IPL_CLOCK
#define	IPL_SCHED	ALPHA_PSL_IPL_HIGH
#define	IPL_HIGH	ALPHA_PSL_IPL_HIGH

#define	IPL_SOFTSERIAL	0	/* serial software interrupts */
#define	IPL_SOFTCLOCK	1	/* clock software interrupts */
#define	IPL_SOFTNET	2	/* network software interrupts */
#define	IPL_SOFT	3	/* other software interrupts */

#define	IST_UNUSABLE	-1	/* interrupt cannot be used */
#define	IST_NONE	0	/* none (dummy) */
#define	IST_PULSE	1	/* pulsed */
#define	IST_EDGE	2	/* edge-triggered */
#define	IST_LEVEL	3	/* level-triggered */

#define SI_SOFT		0
#define SI_SOFTCLOCK	1
#define SI_SOFTNET	2
#define SI_SOFTSERIAL	3
#define	SI_NSOFT	4

#ifdef	_KERNEL

/* SPL asserts */
#ifdef DIAGNOSTIC
/*
 * Although this function is implemented in MI code, it must be in this MD
 * header because we don't want this header to include MI includes.
 */
void splassert_fail(int, int, const char *);
extern int splassert_ctl;
void splassert_check(int, const char *);
#define	splassert(__wantipl)						\
	do {								\
		if (splassert_ctl > 0) {				\
			splassert_check(__wantipl, __func__);		\
		}							\
	} while (0)
#define	splsoftassert(wantipl)	splassert(IPL_SOFTINT)
#else
#define	splassert(wantipl)	do { /* nothing */ } while (0)
#define	splsoftassert(wantipl)	do { /* nothing */ } while (0)
#endif

/* IPL-lowering/restoring macros */
#define splx(s)								\
    ((s) == ALPHA_PSL_IPL_0 ? spl0() : alpha_pal_swpipl(s))

/* IPL-raising functions/macros */
int _splraise(int);

#define splsoft()		_splraise(IPL_SOFTINT)
#define splsoftserial()		splsoft()
#define splsoftclock()		splsoft()
#define splsoftnet()		splsoft()
#define splnet()                _splraise(IPL_NET)
#define splbio()                _splraise(IPL_BIO)
#define spltty()                _splraise(IPL_TTY)
#define splserial()             _splraise(IPL_SERIAL)
#define splaudio()		_splraise(IPL_AUDIO)
#define splvm()			_splraise(IPL_VM)
#define splclock()              _splraise(IPL_CLOCK)
#define splstatclock()          _splraise(IPL_CLOCK)
#define splhigh()               _splraise(IPL_HIGH)

#define spllock()		splhigh()
#define splsched()		splhigh()

/*
 * Interprocessor interrupts.  In order how we want them processed.
 */
#define	ALPHA_IPI_HALT		0x0000000000000001UL
#define	ALPHA_IPI_TBIA		0x0000000000000002UL
#define	ALPHA_IPI_TBIAP		0x0000000000000004UL
#define	ALPHA_IPI_SHOOTDOWN	0x0000000000000008UL
#define	ALPHA_IPI_IMB		0x0000000000000010UL
#define	ALPHA_IPI_AST		0x0000000000000020UL
#define	ALPHA_IPI_SYNCH_FPU	0x0000000000000040UL
#define	ALPHA_IPI_DISCARD_FPU	0x0000000000000080UL
#define	ALPHA_IPI_PAUSE		0x0000000000000100UL

#define	ALPHA_NIPIS		6	/* must not exceed 64 */

typedef void (*ipifunc_t)(void);
extern	ipifunc_t ipifuncs[ALPHA_NIPIS];

void	alpha_send_ipi(unsigned long, unsigned long);
void	alpha_broadcast_ipi(unsigned long);
void	alpha_multicast_ipi(unsigned long, unsigned long);

/*
 * Alpha shared-interrupt-line common code.
 */

struct alpha_shared_intrhand {
	TAILQ_ENTRY(alpha_shared_intrhand)
		ih_q;
	struct alpha_shared_intr *ih_intrhead;
	int	(*ih_fn)(void *);
	void	*ih_arg;
	int	ih_level;
	unsigned int ih_num;
	struct evcount ih_count;
};

struct alpha_shared_intr {
	TAILQ_HEAD(,alpha_shared_intrhand)
		intr_q;
	void	*intr_private;
	int	intr_sharetype;
	int	intr_dfltsharetype;
	int	intr_nstrays;
	int	intr_maxstrays;
};

#define	ALPHA_SHARED_INTR_DISABLE(asi, num)				\
	((asi)[num].intr_maxstrays != 0 &&				\
	 (asi)[num].intr_nstrays == (asi)[num].intr_maxstrays)

/*
 * simulated software interrupt register
 */
extern unsigned long ssir;

#define	setsoft(x)	atomic_setbits_ulong(&ssir, 1 << (x))

struct alpha_soft_intrhand {
	TAILQ_ENTRY(alpha_soft_intrhand)
		sih_q;
	struct alpha_soft_intr *sih_intrhead;
	void	(*sih_fn)(void *);
	void	*sih_arg;
	int	sih_pending;
};

struct alpha_soft_intr {
	TAILQ_HEAD(, alpha_soft_intrhand)
		softintr_q;
	struct mutex softintr_mtx;
	unsigned long softintr_siq;
};

void	 softintr_disestablish(void *);
void	 softintr_dispatch(void);
void	*softintr_establish(int, void (*)(void *), void *);
void	 softintr_init(void);
void	 softintr_schedule(void *);

struct alpha_shared_intr *alpha_shared_intr_alloc(unsigned int);
int	alpha_shared_intr_dispatch(struct alpha_shared_intr *,
	    unsigned int);
void	*alpha_shared_intr_establish(struct alpha_shared_intr *,
	    unsigned int, int, int, int (*)(void *), void *, const char *);
void	alpha_shared_intr_disestablish(struct alpha_shared_intr *, void *);
int	alpha_shared_intr_get_sharetype(struct alpha_shared_intr *,
	    unsigned int);
int	alpha_shared_intr_isactive(struct alpha_shared_intr *,
	    unsigned int);
int	alpha_shared_intr_firstactive(struct alpha_shared_intr *,
	    unsigned int);
void	alpha_shared_intr_set_dfltsharetype(struct alpha_shared_intr *,
	    unsigned int, int);
void	alpha_shared_intr_set_maxstrays(struct alpha_shared_intr *,
	    unsigned int, int);
void	alpha_shared_intr_reset_strays(struct alpha_shared_intr *,
	    unsigned int);
void	alpha_shared_intr_stray(struct alpha_shared_intr *, unsigned int,
	    const char *);
void	alpha_shared_intr_set_private(struct alpha_shared_intr *,
	    unsigned int, void *);
void	*alpha_shared_intr_get_private(struct alpha_shared_intr *,
	    unsigned int);

extern struct scbvec scb_iovectab[];

void	scb_init(void);
void	scb_set(u_long, void (*)(void *, u_long), void *);
u_long	scb_alloc(void (*)(void *, u_long), void *);
void	scb_free(u_long);

#define	SCB_ALLOC_FAILED	((u_long) -1)

#endif /* _KERNEL */
#endif /* ! _ALPHA_INTR_H_ */
