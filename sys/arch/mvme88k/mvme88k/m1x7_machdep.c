/*	$OpenBSD: m1x7_machdep.c,v 1.11 2013/05/17 22:46:28 miod Exp $ */
/*
 * Copyright (c) 2009, 2013 Miodrag Vallat.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
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
 * PCC 2 interval and statistic clocks driver, and reboot routine.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/mutex.h>
#include <sys/timetc.h>

#include <machine/board.h>
#include <machine/bus.h>

#include <mvme88k/dev/pcctwovar.h>
#include <mvme88k/dev/pcctworeg.h>
#include <mvme88k/dev/vme.h>

#include <mvme88k/mvme88k/clockvar.h>

int	m1x7_clockintr(void *);
int	m1x7_statintr(void *);
u_int	pcc_get_timecount(struct timecounter *);

uint32_t	pcc_refcnt;
struct mutex pcc_mutex = MUTEX_INITIALIZER(IPL_CLOCK);

struct timecounter pcc_timecounter = {
	pcc_get_timecount,
	NULL,
	0xffffffff,
	1000000,	/* 1MHz */
	"pcctwo",
	0,
	NULL
};

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

	tc_init(&pcc_timecounter);
}

/*
 * clockintr: ack intr and call hardclock
 */
int
m1x7_clockintr(void *eframe)
{
	uint oflow;
	uint32_t t1, t2;
	uint8_t c;

	/*
	 * Since we can not freeze the counter while reading the count
	 * and overflow registers, read them a second time; if the
	 * counter has wrapped, pick the second reading.
	 */
	mtx_enter(&pcc_mutex);
	t1 = *(volatile uint32_t *)(PCC2_BASE + PCCTWO_T1COUNT);
	c = *(volatile u_int8_t *)(PCC2_BASE + PCCTWO_T1CTL);
	t2 = *(volatile uint32_t *)(PCC2_BASE + PCCTWO_T1COUNT);
	if (t2 < t1)
		c = *(volatile u_int8_t *)(PCC2_BASE + PCCTWO_T1CTL);
	*(volatile u_int8_t *)(PCC2_BASE + PCCTWO_T1CTL) =
	    PCC2_TCTL_CEN | PCC2_TCTL_COC | PCC2_TCTL_COVF;
	*(volatile u_int8_t *)(PCC2_BASE + PCCTWO_T1ICR) = PROF_RESET;
	oflow = c >> PCC2_TCTL_OVF_SHIFT;
	pcc_refcnt += oflow * tick;
	mtx_leave(&pcc_mutex);

	while (oflow-- != 0) {
		hardclock(eframe);

#ifdef MULTIPROCESSOR
		/*
		 * Send an IPI to all other processors, so they can get their
		 * own ticks.
		 */
		m88k_broadcast_ipi(CI_IPI_HARDCLOCK);
#endif
	}

	return (1);
}

u_int
pcc_get_timecount(struct timecounter *tc)
{
	uint32_t tcr1, tcr2;
	uint8_t tctl;
	uint cnt, oflow;

	mtx_enter(&pcc_mutex);
	tcr1 = *(volatile u_int32_t *)(PCC2_BASE + PCCTWO_T1COUNT);
	tctl = *(volatile u_int8_t *)(PCC2_BASE + PCCTWO_T1CTL);
	/*
	 * Since we can not freeze the counter while reading the count
	 * and overflow registers, read it a second time; if it the
	 * counter has wrapped, pick the second reading.
	 */
	tcr2 = *(volatile u_int32_t *)(PCC2_BASE + PCCTWO_T1COUNT);
	if (tcr2 < tcr1) {
		tcr1 = tcr2;
		tctl = *(volatile u_int8_t *)(PCC2_BASE + PCCTWO_T1CTL);
	}
	cnt = pcc_refcnt;
	mtx_leave(&pcc_mutex);

	oflow = tctl >> PCC2_TCTL_OVF_SHIFT;
	while (oflow-- != 0)
		cnt += pcc2_timer_us2lim(tick);
	return cnt + tcr1;
}

int
m1x7_statintr(void *eframe)
{
	u_long newint, r, var;

	*(volatile u_int8_t *)(PCC2_BASE + PCCTWO_T2ICR) = STAT_RESET;

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

	statclock((struct clockframe *)eframe);

#ifdef MULTIPROCESSOR
	/*
	 * Send an IPI to all other processors as well.
	 */
	m88k_broadcast_ipi(CI_IPI_STATCLOCK);
#endif

	return (1);
}

void
m1x7_delay(int us)
{
	/*
	 * On MVME187 and MVME197, use the VMEchip for the delay clock.
	 */
	*(volatile u_int32_t *)(VME2_BASE + VME2_T1CMP) = 0xffffffff;
	*(volatile u_int32_t *)(VME2_BASE + VME2_T1COUNT) = 0;
	*(volatile u_int32_t *)(VME2_BASE + VME2_TCTL) |= VME2_TCTL1_CEN;

	while ((*(volatile u_int32_t *)(VME2_BASE + VME2_T1COUNT)) <
	    (u_int32_t)us)
		;
	*(volatile u_int32_t *)(VME2_BASE + VME2_TCTL) &= ~VME2_TCTL1_CEN;
}

/*
 * Reboot the system.
 */
void
m1x7_reboot(int howto)
{
#ifdef MVME197
	int i;
#endif

	/*
	 * Try hitting the SRST bit in VMEChip2 to reset the system.
	 */
	if (*(volatile u_int32_t *)(VME2_BASE + VME2_TCTL) & VME2_TCTL_SCON)
		*(volatile u_int32_t *)(VME2_BASE + VME2_TCTL) |=
		    VME2_TCTL_SRST;

#ifdef MVME197
	/*
	 * MVME197LE Errata #7:
	 * ``When asserting the RST bit in the VMEchip2 GCSR, the pulse
	 *   generated is too short for the BusSwitch1 to recognize
	 *   properly.''
	 */
	for (i = 0x20000000; i != 0; i--)
#endif
		*(volatile u_int32_t *)(VME2_BASE + VME2_GCSR_LM_SIG_BSCR) |=
		    VME2_GCSR_RST;
}

/*
 * Return whether we are the VME bus system controller.
 */
int
m1x7_is_syscon()
{
	return ISSET(*(volatile u_int32_t *)(VME2_BASE + VME2_TCTL),
	    VME2_TCTL_SCON);
}

int	vme2abort(void *);
struct intrhand vme2_abih;

/*
 * Setup VME bus access and return the lower interrupt number usable by VME
 * boards.
 */
u_int
m1x7_init_vme(const char *devname)
{
	u_int32_t vbr, ctl, irqen, master, master4mod;
	u_int vecbase;

	vbr = *(volatile u_int32_t *)(VME2_BASE + VME2_VBR);
	vecbase = VME2_GET_VBR1(vbr) + 0x10;
	/* Sanity check that the BUG is set up right */
	if (vecbase >= 0x100) {
		panic("Correct the VME Vector Base Register values "
		    "in the BUG settings.\n"
		    "Suggested values are 0x60 for VME Vec0 and "
		    "0x70 for VME Vec1.");
	}

	/* turn off SYSFAIL LED */
	*(volatile u_int32_t *)(VME2_BASE + VME2_TCTL) &= ~VME2_TCTL_SYSFAIL;

	/*
	 * Display the VMEChip2 decoder status.
	 */
	printf("%s: using BUG parameters\n", devname);
	ctl = *(volatile u_int32_t *)(VME2_BASE + VME2_GCSRCTL);
	if (ctl & VME2_GCSRCTL_MDEN1) {
		master = *(volatile u_int32_t *)(VME2_BASE + VME2_MASTER1);
		printf("%s: 1phys 0x%08lx-0x%08lx to VME 0x%08lx-0x%08lx\n",
		    devname, master << 16, master & 0xffff0000,
		    master << 16, master & 0xffff0000);
	}
	if (ctl & VME2_GCSRCTL_MDEN2) {
		master = *(volatile u_int32_t *)(VME2_BASE + VME2_MASTER2);
		printf("%s: 2phys 0x%08lx-0x%08lx to VME 0x%08lx-0x%08lx\n",
		    devname, master << 16, master & 0xffff0000,
		    master << 16, master & 0xffff0000);
	}
	if (ctl & VME2_GCSRCTL_MDEN3) {
		master = *(volatile u_int32_t *)(VME2_BASE + VME2_MASTER3);
		printf("%s: 3phys 0x%08lx-0x%08lx to VME 0x%08lx-0x%08lx\n",
		    devname, master << 16, master & 0xffff0000,
		    master << 16, master & 0xffff0000);
	}
	if (ctl & VME2_GCSRCTL_MDEN4) {
		master = *(volatile u_int32_t *)(VME2_BASE + VME2_MASTER4);
		master4mod =
		    *(volatile u_int32_t *)(VME2_BASE + VME2_MASTER4MOD);
		printf("%s: 4phys 0x%08lx-0x%08lx to VME 0x%08lx-0x%08lx\n",
		    devname, master << 16, master & 0xffff0000,
		    (master << 16) + (master4mod << 16),
		    (master & 0xffff0000) + (master4mod & 0xffff0000));
	}

	/*
	 * Map the VME irq levels to the cpu levels 1:1.
	 * This is rather inflexible, but much easier.
	 */
	*(volatile u_int32_t *)(VME2_BASE + VME2_IRQL4) =
	    (7 << VME2_IRQL4_VME7SHIFT) | (6 << VME2_IRQL4_VME6SHIFT) |
	    (5 << VME2_IRQL4_VME5SHIFT) | (4 << VME2_IRQL4_VME4SHIFT) |
	    (3 << VME2_IRQL4_VME3SHIFT) | (2 << VME2_IRQL4_VME2SHIFT) |
	    (1 << VME2_IRQL4_VME1SHIFT);
	printf("%s: vme to cpu irq level 1:1\n", devname);

	/* Enable the reset switch */
	*(volatile u_int32_t *)(VME2_BASE + VME2_TCTL) |= VME2_TCTL_RSWE;
	/* Set Watchdog timeout to about 1 minute */
	*(volatile u_int32_t *)(VME2_BASE + VME2_TCR) |= VME2_TCR_64S;
	/* Enable VMEChip2 Interrupts */
	*(volatile u_int32_t *)(VME2_BASE + VME2_VBR) |= VME2_IOCTL1_MIEN;

	/*
	 * Map the Software VME irq levels to the cpu level 7.
	 */
	*(volatile u_int32_t *)(VME2_BASE + VME2_IRQL3) =
	    (7 << VME2_IRQL3_SW7SHIFT) | (7 << VME2_IRQL3_SW6SHIFT) |
	    (7 << VME2_IRQL3_SW5SHIFT) | (7 << VME2_IRQL3_SW4SHIFT) |
	    (7 << VME2_IRQL3_SW3SHIFT) | (7 << VME2_IRQL3_SW2SHIFT) |
	    (7 << VME2_IRQL3_SW1SHIFT);

	/*
	 * Register abort interrupt handler.
	 */
	vme2_abih.ih_fn = vme2abort;
	vme2_abih.ih_arg = 0;
	vme2_abih.ih_wantframe = 1;
	vme2_abih.ih_ipl = IPL_NMI;
	intr_establish(0x6e, &vme2_abih, devname);

	irqen = *(volatile u_int32_t *)(VME2_BASE + VME2_IRQEN);
	irqen |= VME2_IRQ_AB;

	/*
	 * Enable ACFAIL interrupt, but disable Timer 1 interrupt - we
	 * prefer it without for delay().
	 */
	irqen = (irqen | VME2_IRQ_ACF) & ~VME2_IRQ_TIC1;
	*(volatile u_int32_t *)(VME2_BASE + VME2_IRQEN) = irqen;

	return vecbase;
}

int
m1x7_intsrc_available(u_int intsrc, int ipl)
{
	if (intsrc != INTSRC_VME)
		return ENXIO;		/* should never happen anyway */

	return 0;
}

void
m1x7_intsrc_enable(u_int intsrc, int ipl)
{
	*(volatile u_int32_t *)(VME2_BASE + VME2_IRQEN) |=
	    VME2_IRQ_VME(ipl);
}

int
vme2abort(void *frame)
{
	if ((*(volatile u_int32_t *)(VME2_BASE + VME2_IRQSTAT) &
	    VME2_IRQ_AB) == 0)
		return 0;

	*(volatile u_int32_t *)(VME2_BASE + VME2_IRQCLR) = VME2_IRQ_AB;
	nmihand(frame);

	return 1;
}
