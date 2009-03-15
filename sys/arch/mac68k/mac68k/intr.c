/*	$OpenBSD: intr.c,v 1.13 2009/03/15 20:40:25 miod Exp $	*/
/*	$NetBSD: intr.c,v 1.2 1998/08/25 04:03:56 scottr Exp $	*/

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
#include <sys/evcount.h>

#include <uvm/uvm_extern.h>

#include <machine/cpu.h>
#include <machine/intr.h>

#define	NISR	8
#define	ISRLOC	0x18

void	intr_init(void);
void	netintr(void);

#ifdef DEBUG
int	intr_debug = 0;
#endif

/*
 * Some of the below are not used yet, but might be used someday on the
 * Q700/900/950 where the interrupt controller may be reprogrammed to
 * interrupt on different levels as listed in locore.s
 */
u_short	mac68k_ttyipl;
u_short	mac68k_netipl;
u_short	mac68k_vmipl;
u_short	mac68k_clockipl;
u_short	mac68k_statclockipl;

struct	intrhand intrs[NISR];

void	intr_computeipl(void);

void
intr_init()
{
	/* Standard spl(9) interrupt priorities */
	mac68k_ttyipl = (PSL_S | PSL_IPL1);

	if (mac68k_machine.aux_interrupts) {
		mac68k_netipl = (PSL_S | PSL_IPL3);
		mac68k_vmipl = (PSL_S | PSL_IPL6);
	} else {
		if (current_mac_model->class == MACH_CLASSAV)
			mac68k_netipl = (PSL_S | PSL_IPL4);
		else if (mac68k_machine.sonic)
			mac68k_netipl = (PSL_S | PSL_IPL3);
		else
			mac68k_netipl = (PSL_S | PSL_IPL2);

		mac68k_vmipl = (PSL_S | PSL_IPL2);
	}
	
	mac68k_clockipl = mac68k_statclockipl = mac68k_vmipl;
	intr_computeipl();
}

/*
 * Compute the interrupt levels for the spl*()
 * calls.  This doesn't have to be fast.
 */
void
intr_computeipl()
{
	/*
	 * Enforce `bio <= net <= tty <= imp <= statclock <= clock'
	 * as defined in spl(9)
	 */
	if ((PSL_S | PSL_IPL2) > mac68k_netipl)
		mac68k_netipl = (PSL_S | PSL_IPL2);
	
	if (mac68k_netipl > mac68k_ttyipl)
		mac68k_ttyipl = mac68k_netipl;

	if (mac68k_ttyipl > mac68k_vmipl)
		mac68k_vmipl = mac68k_ttyipl;

	if (mac68k_vmipl > mac68k_statclockipl)
		mac68k_statclockipl = mac68k_vmipl;

	if (mac68k_statclockipl > mac68k_clockipl)
		mac68k_clockipl = mac68k_statclockipl;
}

/*
 * Establish an autovectored interrupt handler.
 * Called by driver attach functions.
 */
void
intr_establish(int (*func)(void *), void *arg, int ipl, const char *name)
{
	struct intrhand *ih;

#ifdef DIAGNOSTIC
	if (ipl < 0 || ipl >= NISR)
		panic("intr_establish: bad ipl %d", ipl);
#endif

	ih = &intrs[ipl];

#ifdef DIAGNOSTIC
	if (ih->ih_fn != NULL)
		panic("intr_establish: attempt to share ipl %d", ipl);
#endif

	ih->ih_fn = func;
	ih->ih_arg = arg;
	ih->ih_ipl = ipl;
	evcount_attach(&ih->ih_count, name, (void *)&ih->ih_ipl, &evcount_intr);
}

/*
 * Disestablish an interrupt handler.
 */
void
intr_disestablish(int ipl)
{
	struct intrhand *ih;

#ifdef DIAGNOSTIC
	if (ipl < 0 || ipl >= NISR)
		panic("intr_disestablish: bad ipl %d", ipl);
#endif

	ih = &intrs[ipl];

#ifdef DIAGNOSTIC
	if (ih->ih_fn == NULL)
		panic("intr_disestablish: no vector on ipl %d", ipl);
#endif

	ih->ih_fn = NULL;
	evcount_detach(&ih->ih_count);
}

/*
 * This is the dispatcher called by the low-level
 * assembly language interrupt routine.
 */
void
intr_dispatch(int evec)	/* format | vector offset */
{
	struct intrhand *ih;
	int ipl, vec;

	vec = (evec & 0x0fff) >> 2;
	ipl = vec - ISRLOC;
#ifdef DIAGNOSTIC
	if (ipl < 0 || ipl >= NISR)
		panic("intr_dispatch: bad vec 0x%x", vec);
#endif

	uvmexp.intrs++;
	ih = &intrs[ipl];
	if (ih->ih_fn != NULL) {
		if ((*ih->ih_fn)(ih->ih_arg) != 0)
			ih->ih_count.ec_count++;
	} else {
#if 0
		printf("spurious interrupt, ipl %d\n", ipl);
#endif
	}
}

#ifdef DIAGNOSTIC
void
splassert_check(int wantipl, const char *func)
{
	int oldipl;

	__asm __volatile ("movew sr,%0" : "=&d" (oldipl));

	oldipl = PSLTOIPL(oldipl);

	if (oldipl < wantipl) {
		splassert_fail(wantipl, oldipl, func);
		/*
		 * If the splassert_ctl is set to not panic, raise the ipl
		 * in a feeble attempt to reduce damage.
		 */
		_spl(PSL_S | IPLTOPSL(wantipl));
	}
}
#endif
