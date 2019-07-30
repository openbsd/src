/*	$OpenBSD: softintr.c,v 1.8 2014/07/12 18:44:41 tedu Exp $	*/
/*	$NetBSD: softintr.c,v 1.2 2003/07/15 00:24:39 lukem Exp $	*/

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

#include <sys/param.h>
#include <sys/malloc.h>

#include <uvm/uvm_extern.h>

#include <machine/atomic.h>
#include <machine/intr.h>

struct soft_intrq soft_intrq[SI_NQUEUES];

/*
 * softintr_init:
 *
 *	Initialize the software interrupt system.
 */
void
softintr_init(void)
{
	struct soft_intrq *siq;
	int i;

	for (i = 0; i < SI_NQUEUES; i++) {
		siq = &soft_intrq[i];
		TAILQ_INIT(&siq->siq_list);
		mtx_init(&siq->siq_mtx, IPL_HIGH);
		siq->siq_si = i;
	}
}

/*
 * softintr_dispatch:
 *
 *	Process pending software interrupts on the specified queue.
 *
 *	NOTE: We must already be at the correct interrupt priority level.
 */
void
softintr_dispatch(int si)
{
	struct soft_intrq *siq = &soft_intrq[si];
	struct soft_intrhand *sih;

	for (;;) {
		mtx_enter(&siq->siq_mtx);
		sih = TAILQ_FIRST(&siq->siq_list);
		if (sih == NULL) {
			mtx_leave(&siq->siq_mtx);
			break;
		}

		TAILQ_REMOVE(&siq->siq_list, sih, sih_list);
		sih->sih_pending = 0;

		uvmexp.softs++;
		mtx_leave(&siq->siq_mtx);

		(*sih->sih_func)(sih->sih_arg);
	}
}

/*
 * softintr_establish:		[interface]
 *
 *	Register a software interrupt handler.
 */
void *
softintr_establish(int ipl, void (*func)(void *), void *arg)
{
	struct soft_intrhand *sih;
	int si;

	switch (ipl) {
	case IPL_SOFT:
		si = SI_SOFT;
		break;

	case IPL_SOFTCLOCK:
		si = SI_SOFTCLOCK;
		break;

	case IPL_SOFTNET:
		si = SI_SOFTNET;
		break;

	case IPL_TTY:
	case IPL_SOFTTTY:
		si = SI_SOFTTTY;
		break;

	default:
		panic("softintr_establish: unknown soft IPL %d", ipl);
	}

	sih = malloc(sizeof(*sih), M_DEVBUF, M_NOWAIT);
	if (__predict_true(sih != NULL)) {
		sih->sih_func = func;
		sih->sih_arg = arg;
		sih->sih_siq = &soft_intrq[si];
		sih->sih_pending = 0;
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
	struct soft_intrq *siq = sih->sih_siq;

	mtx_enter(&siq->siq_mtx);
	if (sih->sih_pending) {
		TAILQ_REMOVE(&siq->siq_list, sih, sih_list);
		sih->sih_pending = 0;
	}
	mtx_leave(&siq->siq_mtx);

	free(sih, M_DEVBUF, 0);
}
