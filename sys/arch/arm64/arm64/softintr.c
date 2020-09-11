/*	$OpenBSD: softintr.c,v 1.2 2020/09/11 09:27:10 mpi Exp $	*/
/*	$NetBSD: softintr.c,v 1.1 2003/02/26 21:26:12 fvdl Exp $	*/

/*-
 * Copyright (c) 2000, 2001 The NetBSD Foundation, Inc.
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
 * Generic soft interrupt implementation
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>

#include <machine/intr.h>

#include <uvm/uvm_extern.h>

struct soft_intr soft_intrs[SI_NSOFTINTR];

const int soft_intr_to_ssir[SI_NSOFTINTR] = {
	SIR_SOFT,
	SIR_CLOCK,
	SIR_NET,
	SIR_TTY,
};

void	softintr_biglock_wrap(void *);

/*
 * softintr_init:
 *
 *	Initialize the software interrupt system.
 */
void
softintr_init(void)
{
	struct soft_intr *si;
	int i;

	for (i = 0; i < SI_NSOFTINTR; i++) {
		si = &soft_intrs[i];
		TAILQ_INIT(&si->softintr_q);
		mtx_init(&si->softintr_lock, IPL_HIGH);
		si->softintr_ssir = soft_intr_to_ssir[i];
	}
}

/*
 * softintr_dispatch:
 *
 *	Process pending software interrupts.
 */
void
softintr_dispatch(int which)
{
	struct soft_intr		*si = &soft_intrs[which];
	struct soft_intrhand	*sih;
	void				*arg;
	void				(*fn)(void *);

	for (;;) {
		mtx_enter(&si->softintr_lock);
		sih = TAILQ_FIRST(&si->softintr_q);
		if (sih == NULL) {
			mtx_leave(&si->softintr_lock);
			break;
		}
		TAILQ_REMOVE(&si->softintr_q, sih, sih_q);
		sih->sih_pending = 0;

		uvmexp.softs++;
		arg = sih->sih_arg;
		fn = sih->sih_fn;
		mtx_leave(&si->softintr_lock);

		(*fn)(arg);
	}
}

#ifdef MULTIPROCESSOR
void
softintr_biglock_wrap(void *arg)
{
	struct soft_intrhand	*sih = arg;

	KERNEL_LOCK();
	sih->sih_fnwrap(sih->sih_argwrap);
	KERNEL_UNLOCK();
}
#endif

/*
 * softintr_establish:		[interface]
 *
 *	Register a software interrupt handler.
 */
void *
softintr_establish_flags(int ipl, void (*func)(void *), void *arg, int flags)
{
	struct soft_intr *si;
	struct soft_intrhand *sih;
	int which;

	switch (ipl) {
	case IPL_SOFTCLOCK:
		which = SIR_CLOCK;
		break;

	case IPL_SOFTNET:
		which = SIR_NET;
		break;

	case IPL_TTY:
	case IPL_SOFTTTY:
		which = SIR_TTY;
		break;

	default:
		panic("softintr_establish");
	}

	si = &soft_intrs[which];

	sih = malloc(sizeof(*sih), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (__predict_true(sih != NULL)) {
		sih->sih_intrhead = si;
#ifdef MULTIPROCESSOR
		if (flags & SOFTINTR_ESTABLISH_MPSAFE) {
#endif
			sih->sih_fn = func;
			sih->sih_arg = arg;
#ifdef MULTIPROCESSOR
		} else {
			sih->sih_fnwrap = func;
			sih->sih_argwrap = arg;
			sih->sih_fn = softintr_biglock_wrap;
			sih->sih_arg = sih;
		}
#endif
	}
	return (sih);
}

/*
 * softintr_disestablish:	[interface]
 *
 *	Unregister a software interrupt handler.
 */
void
softintr_disestablish(void *arg)
{
	struct soft_intrhand *sih = arg;
	struct soft_intr *si = sih->sih_intrhead;

	mtx_enter(&si->softintr_lock);
	if (sih->sih_pending) {
		TAILQ_REMOVE(&si->softintr_q, sih, sih_q);
		sih->sih_pending = 0;
	}
	mtx_leave(&si->softintr_lock);

	free(sih, M_DEVBUF, 0);
}

void
softintr(int intrq)
{
	// protected by mutex in caller
	curcpu()->ci_ipending |= (1 << intrq);
}
