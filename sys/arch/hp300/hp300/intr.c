/*	$OpenBSD: intr.c,v 1.23 2009/03/15 20:40:23 miod Exp $	*/
/*	$NetBSD: intr.c,v 1.5 1998/02/16 20:58:30 thorpej Exp $	*/

/*-
 * Copyright (c) 1996, 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Adam Glass, Gordon W. Ross, and Jason R. Thorpe.
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
 * Link and dispatch interrupts.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/vmmeter.h>

#include <uvm/uvm_extern.h>

#include "ppp.h"
#include "bridge.h"

#include <machine/cpu.h>
#include <machine/intr.h>

/*
 * The location and size of the autovectored interrupt portion
 * of the vector table.
 */
#define ISRLOC		0x18

typedef LIST_HEAD(, isr) isr_list_t;
isr_list_t isr_list[NISR];

/*
 * Default interrupt priorities.
 * IPL_BIO, IPL_NET and IPL_TTY will be adjusted when devices attach.
 */
u_short	hp300_varpsl[NISR] = {
	PSL_S | PSL_IPL0,	/* IPL_NONE */
	PSL_S | PSL_IPL1,	/* IPL_SOFT */
	PSL_S | PSL_IPL3,	/* IPL_BIO */
	PSL_S | PSL_IPL3,	/* IPL_NET */
	PSL_S | PSL_IPL3,	/* IPL_TTY */
	PSL_S | PSL_IPL5,	/* IPL_VM */
	PSL_S | PSL_IPL6,	/* IPL_CLOCK */
	PSL_S | PSL_IPL7	/* IPL_HIGH */
};

void	intr_computeipl(void);

void
intr_init()
{
	int i;

	/* Initialize the ISR lists. */
	for (i = 0; i < NISR; ++i)
		LIST_INIT(&isr_list[i]);
}

/*
 * Scan all of the ISRs, recomputing the interrupt levels for the spl*()
 * calls.  This doesn't have to be fast.
 */
void
intr_computeipl()
{
	struct isr *isr;
	int ipl;

	/* Start with low values. */
	hp300_varpsl[IPL_BIO] = hp300_varpsl[IPL_NET] =
	    hp300_varpsl[IPL_TTY] = PSL_S | PSL_IPL3;

	for (ipl = 0; ipl < NISR; ipl++) {
		LIST_FOREACH(isr, &isr_list[ipl], isr_link) {
			/*
			 * Bump up the level for a given priority,
			 * if necessary.
			 */
			switch (isr->isr_priority) {
			case IPL_BIO:
				if (ipl > PSLTOIPL(hp300_varpsl[IPL_BIO]))
					hp300_varpsl[IPL_BIO] = IPLTOPSL(ipl);
				break;

			case IPL_NET:
				if (ipl > PSLTOIPL(hp300_varpsl[IPL_NET]))
					hp300_varpsl[IPL_NET] = IPLTOPSL(ipl);
				break;

			case IPL_TTY:
				if (ipl > PSLTOIPL(hp300_varpsl[IPL_TTY]))
					hp300_varpsl[IPL_TTY] = IPLTOPSL(ipl);
				break;

			default:
				panic("intr_computeipl: bad priority %d",
				    isr->isr_priority);
			}
		}
	}

	/*
	 * Enforce `bio <= net <= tty <= vm'
	 */

	if (hp300_varpsl[IPL_NET] < hp300_varpsl[IPL_BIO])
		hp300_varpsl[IPL_NET] = hp300_varpsl[IPL_BIO];

	if (hp300_varpsl[IPL_TTY] < hp300_varpsl[IPL_NET])
		hp300_varpsl[IPL_TTY] = hp300_varpsl[IPL_NET];
}

void
intr_printlevels()
{

#ifdef DEBUG
	printf("psl: bio = 0x%x, net = 0x%x, tty = 0x%x\n",
	    hp300_varpsl[IPL_BIO], hp300_varpsl[IPL_NET],
	    hp300_varpsl[IPL_TTY]);
#endif

	printf("interrupt levels: bio = %d, net = %d, tty = %d\n",
	    PSLTOIPL(hp300_varpsl[IPL_BIO]), PSLTOIPL(hp300_varpsl[IPL_NET]),
	    PSLTOIPL(hp300_varpsl[IPL_TTY]));
}

/*
 * Establish an interrupt handler.
 * Called by driver attach functions.
 */
void
intr_establish(struct isr *isr, const char *name)
{
	struct isr *curisr;
	isr_list_t *list;

#ifdef DIAGNOSTIC
	if (isr->isr_ipl < 0 || isr->isr_ipl >= NISR)
		panic("intr_establish: bad ipl %d", isr->isr_ipl);
#endif

	evcount_attach(&isr->isr_count, name, &isr->isr_ipl,
	    &evcount_intr);

	/*
	 * Some devices are particularly sensitive to interrupt
	 * handling latency.  The DCA, for example, can lose many
	 * characters if its interrupt isn't handled with reasonable
	 * speed.  For this reason, we sort ISRs by IPL_* priority,
	 * inserting higher priority interrupts before lower priority
	 * interrupts.
	 */

	/*
	 * Get the appropriate ISR list.  If the list is empty, no
	 * additional work is necessary; we simply insert ourselves
	 * at the head of the list.
	 */
	list = &isr_list[isr->isr_ipl];
	if (LIST_EMPTY(list)) {
		LIST_INSERT_HEAD(list, isr, isr_link);
		goto compute;
	}

	/*
	 * A little extra work is required.  We traverse the list
	 * and place ourselves after any ISRs with our current (or
	 * higher) priority.
	 */
	for (curisr = LIST_FIRST(list);
	    LIST_NEXT(curisr, isr_link) != LIST_END(list);
	    curisr = LIST_NEXT(curisr, isr_link)) {
		if (isr->isr_priority > curisr->isr_priority) {
			LIST_INSERT_BEFORE(curisr, isr, isr_link);
			goto compute;
		}
	}

	/*
	 * We're the least important entry, it seems.  We just go
	 * on the end.
	 */
	LIST_INSERT_AFTER(curisr, isr, isr_link);

 compute:
	/* Compute new interrupt levels. */
	intr_computeipl();
}

/*
 * Disestablish an interrupt handler.
 */
void
intr_disestablish(struct isr *isr)
{
	evcount_detach(&isr->isr_count);
	LIST_REMOVE(isr, isr_link);
	intr_computeipl();
}

/*
 * This is the dispatcher called by the low-level
 * assembly language interrupt routine.
 */
void
intr_dispatch(evec)
	int evec;		/* format | vector offset */
{
	struct isr *isr;
	isr_list_t *list;
	int handled, rc, ipl, vec;
	static int straycount, unexpected;

	vec = (evec & 0xfff) >> 2;
#ifdef DIAGNOSTIC
	if (vec < ISRLOC || vec >= (ISRLOC + NISR))
		panic("isrdispatch: bad vec 0x%x", vec);
#endif
	ipl = vec - ISRLOC;

	uvmexp.intrs++;

	list = &isr_list[ipl];
	if (LIST_EMPTY(list)) {
		printf("intr_dispatch: ipl %d unexpected\n", ipl);
		if (++unexpected > 10)
			panic("intr_dispatch: too many unexpected interrupts");
		return;
	}

	handled = 0;
	/* Give all the handlers a chance. */
	LIST_FOREACH(isr, list, isr_link) {
		rc = (*isr->isr_func)(isr->isr_arg);
		if (rc > 0)
			isr->isr_count.ec_count++;
		handled |= rc;
	}

	if (handled)
		straycount = 0;
	else if (++straycount > 50)
		panic("intr_dispatch: too many stray interrupts");
	else
		printf("intr_dispatch: stray level %d interrupt\n", ipl);
}

#ifdef DIAGNOSTIC
void
splassert_check(int wantipl, const char *func)
{
	int oldipl, realwantipl;

	__asm __volatile ("movew sr,%0" : "=&d" (oldipl));

	realwantipl = PSLTOIPL(hp300_varpsl[wantipl]);
	oldipl = PSLTOIPL(oldipl);

	if (oldipl < realwantipl) {
		splassert_fail(realwantipl, oldipl, func);
		/*
		 * If the splassert_ctl is set to not panic, raise the ipl
		 * in a feeble attempt to reduce damage.
		 */
		_spl(hp300_varpsl[wantipl]);
	}
}
#endif
