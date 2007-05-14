/*	$OpenBSD: m1x7_machdep.c,v 1.5 2007/05/14 16:57:43 miod Exp $ */
/*
 * Copyright (c) 1999 Steve Murphree, Jr.
 * Copyright (c) 1995 Theo de Raadt
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
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1995 Nivas Madhur
 * Copyright (c) 1994 Gordon W. Ross
 * Copyright (c) 1993 Adam Glass
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)clock.c	8.1 (Berkeley) 6/11/93
 */

/*
 * Interval and statistic clocks driver.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/kernel.h>

#include <machine/bus.h>

#include <mvme88k/dev/pcctwovar.h>
#include <mvme88k/dev/pcctworeg.h>

#include <mvme88k/mvme88k/clockvar.h>

int	m1x7_clockintr(void *);
int	m1x7_statintr(void *);

#define	PROF_RESET	(IPL_CLOCK | PCC2_IRQ_IEN | PCC2_IRQ_ICLR)
#define	STAT_RESET	(IPL_STATCLOCK | PCC2_IRQ_IEN | PCC2_IRQ_ICLR)

void
m1x7_init_clocks(void)
{
	int statint, minint;

#ifdef DIAGNOSTIC
	if (1000000 % hz) {
		printf("cannot get %d Hz clock; using 100 Hz\n", hz);
		hz = 100;
	}
#endif
	tick = 1000000 / hz;

	/* profclock */
	*(volatile u_int8_t *)(PCC2_BASE + PCCTWO_T1CTL) = 0;
	*(volatile u_int32_t *)(PCC2_BASE + PCCTWO_T1CMP) =
	    pcc2_timer_us2lim(tick);
	*(volatile u_int32_t *)(PCC2_BASE + PCCTWO_T1COUNT) = 0;
	*(volatile u_int8_t *)(PCC2_BASE + PCCTWO_T1CTL) =
	    PCC2_TCTL_CEN | PCC2_TCTL_COC | PCC2_TCTL_COVF;
	*(volatile u_int8_t *)(PCC2_BASE + PCCTWO_T1ICR) = PROF_RESET;

	if (stathz == 0)
		stathz = hz;
#ifdef DIAGNOSTIC
	if (1000000 % stathz) {
		printf("cannot get %d Hz statclock; using 100 Hz\n", stathz);
		stathz = 100;
	}
#endif
	profhz = stathz;		/* always */

	statint = 1000000 / stathz;
	minint = statint / 2 + 100;
	while (statvar > minint)
		statvar >>= 1;

	/* statclock */
	*(volatile u_int8_t *)(PCC2_BASE + PCCTWO_T2CTL) = 0;
	*(volatile u_int32_t *)(PCC2_BASE + PCCTWO_T2CMP) =
	    pcc2_timer_us2lim(statint);
	*(volatile u_int32_t *)(PCC2_BASE + PCCTWO_T2COUNT) = 0;
	*(volatile u_int8_t *)(PCC2_BASE + PCCTWO_T2CTL) =
	    PCC2_TCTL_CEN | PCC2_TCTL_COC | PCC2_TCTL_COVF;
	*(volatile u_int8_t *)(PCC2_BASE + PCCTWO_T2ICR) = STAT_RESET;

	statmin = statint - (statvar >> 1);

	clock_ih.ih_fn = m1x7_clockintr;
	clock_ih.ih_arg = 0;
	clock_ih.ih_wantframe = 1;
	clock_ih.ih_ipl = IPL_CLOCK;
	pcctwointr_establish(PCC2V_TIMER1, &clock_ih, "clock");

	statclock_ih.ih_fn = m1x7_statintr;
	statclock_ih.ih_arg = 0;
	statclock_ih.ih_wantframe = 1;
	statclock_ih.ih_ipl = IPL_STATCLOCK;
	pcctwointr_establish(PCC2V_TIMER2, &statclock_ih, "stat");
}

/*
 * clockintr: ack intr and call hardclock
 */
int
m1x7_clockintr(void *eframe)
{
	*(volatile u_int8_t *)(PCC2_BASE + PCCTWO_T1ICR) = PROF_RESET;

	hardclock(eframe);

	return (1);
}

int
m1x7_statintr(void *eframe)
{
	u_long newint, r, var;

	*(volatile u_int8_t *)(PCC2_BASE + PCCTWO_T2ICR) = STAT_RESET;

	statclock((struct clockframe *)eframe);

	/*
	 * Compute new randomized interval.  The intervals are uniformly
	 * distributed on [statint - statvar / 2, statint + statvar / 2],
	 * and therefore have mean statint, giving a stathz frequency clock.
	 */
	var = statvar;
	do {
		r = random() & (var - 1);
	} while (r == 0);
	newint = statmin + r;

	*(volatile u_int8_t *)(PCC2_BASE + PCCTWO_T2CTL) = 0;
	*(volatile u_int32_t *)(PCC2_BASE + PCCTWO_T2CMP) =
	    pcc2_timer_us2lim(newint);
	*(volatile u_int32_t *)(PCC2_BASE + PCCTWO_T2COUNT) = 0;
	*(volatile u_int8_t *)(PCC2_BASE + PCCTWO_T2ICR) = STAT_RESET;
	*(volatile u_int8_t *)(PCC2_BASE + PCCTWO_T2CTL) =
	    PCC2_TCTL_CEN | PCC2_TCTL_COC;
	return (1);
}
