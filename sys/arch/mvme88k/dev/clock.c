/*	$OpenBSD: clock.c,v 1.42 2004/08/25 21:47:54 miod Exp $ */
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
#include <sys/simplelock.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/systm.h>

#include <machine/asm.h>
#include <machine/psl.h>
#include <machine/autoconf.h>
#include <machine/bugio.h>
#include <machine/cpu.h>

#include "pcctwo.h"
#if NPCCTWO > 0
#include <machine/mvme1x7.h>
#include <mvme88k/dev/pcctwovar.h>
#include <mvme88k/dev/pcctworeg.h>
#endif

#include "syscon.h"
#if NSYSCON > 0
#include <machine/mvme188.h>
#include <mvme88k/dev/sysconreg.h>
#endif

#include <mvme88k/dev/vme.h>

#include "bugtty.h"
#if NBUGTTY > 0
#include <mvme88k/dev/bugttyfunc.h>
#endif

int	clockmatch(struct device *, void *, void *);
void	clockattach(struct device *, struct device *, void *);

void	sbc_initclock(void);
void	sbc_initstatclock(void);
void	m188_initclock(void);
void	m188_initstatclock(void);
void	m188_cio_init(unsigned);
u_int	read_cio(int);
void	write_cio(int, u_int);

struct clocksoftc {
	struct device	sc_dev;
	struct intrhand	sc_profih;
	struct intrhand	sc_statih;
};

struct cfattach clock_ca = {
        sizeof(struct clocksoftc), clockmatch, clockattach
};

struct cfdriver clock_cd = {
        NULL, "clock", DV_DULL
};

int	sbc_clockintr(void *);
int	sbc_statintr(void *);
int	m188_clockintr(void *);
int	m188_statintr(void *);

#if NPCCTWO > 0
u_int8_t prof_reset;
u_int8_t stat_reset;
#endif

#if NSYSCON > 0
struct simplelock cio_lock;

#define	CIO_LOCK	simple_lock(&cio_lock)
#define	CIO_UNLOCK	simple_unlock(&cio_lock)
#endif

/*
 * Statistics clock interval and variance, in usec.  Variance must be a
 * power of two.  Since this gives us an even number, not an odd number,
 * we discard one case and compensate.  That is, a variance of 4096 would
 * give us offsets in [0..4095].  Instead, we take offsets in [1..4095].
 * This is symmetric about the point 2048, or statvar/2, and thus averages
 * to that value (assuming uniform random numbers).
 */
int statvar = 8192;
int statmin;			/* statclock interval - 1/2*variance */

/*
 * Every machine must have a clock tick device of some sort; for this
 * platform this file manages it, no matter what form it takes.
 */
int
clockmatch(struct device *parent, void *vcf, void *args)
{
	struct confargs *ca = args;
	struct cfdata *cf = vcf;

	if (strcmp(cf->cf_driver->cd_name, "clock")) {
		return (0);
	}

	/*
	 * clock has to be at ipl 5
	 * We return the ipl here so that the parent can print
	 * a message if it is different from what ioconf.c says.
	 */
	ca->ca_ipl = IPL_CLOCK;
	return (1);
}

void
clockattach(struct device *parent, struct device *self, void *args)
{
	struct confargs *ca = args;
	struct clocksoftc *sc = (struct clocksoftc *)self;

	switch (ca->ca_bustype) {
#if NPCCTWO > 0
	case BUS_PCCTWO:
		sc->sc_profih.ih_fn = sbc_clockintr;
		sc->sc_profih.ih_arg = 0;
		sc->sc_profih.ih_wantframe = 1;
		sc->sc_profih.ih_ipl = ca->ca_ipl;
		prof_reset = ca->ca_ipl | PCC2_IRQ_IEN | PCC2_IRQ_ICLR;
		pcctwointr_establish(PCC2V_TIMER1, &sc->sc_profih, "clock");
		md.clock_init_func = sbc_initclock;
		sc->sc_statih.ih_fn = sbc_statintr;
		sc->sc_statih.ih_arg = 0;
		sc->sc_statih.ih_wantframe = 1;
		sc->sc_statih.ih_ipl = ca->ca_ipl;
		stat_reset = ca->ca_ipl | PCC2_IRQ_IEN | PCC2_IRQ_ICLR;
		pcctwointr_establish(PCC2V_TIMER2, &sc->sc_statih, "stat");
		md.statclock_init_func = sbc_initstatclock;
		break;
#endif /* NPCCTWO */
#if NSYSCON > 0
	case BUS_SYSCON:
		sc->sc_profih.ih_fn = m188_clockintr;
		sc->sc_profih.ih_arg = 0;
		sc->sc_profih.ih_wantframe = 1;
		sc->sc_profih.ih_ipl = ca->ca_ipl;
		sysconintr_establish(SYSCV_TIMER2, &sc->sc_profih, "clock");
		md.clock_init_func = m188_initclock;
		sc->sc_statih.ih_fn = m188_statintr;
		sc->sc_statih.ih_arg = 0;
		sc->sc_statih.ih_wantframe = 1;
		sc->sc_statih.ih_ipl = ca->ca_ipl;
		sysconintr_establish(SYSCV_TIMER1, &sc->sc_statih, "stat");
		md.statclock_init_func = m188_initstatclock;
		break;
#endif /* NSYSCON */
	}

	printf("\n");
}

#if NPCCTWO > 0

void
sbc_initclock(void)
{
#ifdef CLOCK_DEBUG
	printf("SBC clock init\n");
#endif
	if (1000000 % hz) {
		printf("cannot get %d Hz clock; using 100 Hz\n", hz);
		hz = 100;
	}
	tick = 1000000 / hz;

	/* profclock */
	*(volatile u_int8_t *)(OBIO_START + PCC2_BASE + PCCTWO_T1CTL) = 0;
	*(volatile u_int32_t *)(OBIO_START + PCC2_BASE + PCCTWO_T1CMP) =
	    pcc2_timer_us2lim(tick);
	*(volatile u_int32_t *)(OBIO_START + PCC2_BASE + PCCTWO_T1COUNT) = 0;
	*(volatile u_int8_t *)(OBIO_START + PCC2_BASE + PCCTWO_T1CTL) =
	    PCC2_TCTL_CEN | PCC2_TCTL_COC | PCC2_TCTL_COVF;
	*(volatile u_int8_t *)(OBIO_START + PCC2_BASE + PCCTWO_T1ICR) =
	    prof_reset;
}

/*
 * clockintr: ack intr and call hardclock
 */
int
sbc_clockintr(void *eframe)
{
	*(volatile u_int8_t *)(OBIO_START + PCC2_BASE + PCCTWO_T1ICR) =
	    prof_reset;

	intrcnt[M88K_CLK_IRQ]++;

	hardclock(eframe);
#if NBUGTTY > 0
	bugtty_chkinput();
#endif /* NBUGTTY */

	return (1);
}

void
sbc_initstatclock(void)
{
	int statint, minint;

#ifdef CLOCK_DEBUG
	printf("SBC statclock init\n");
#endif
	if (stathz == 0)
		stathz = hz;
	if (1000000 % stathz) {
		printf("cannot get %d Hz statclock; using 100 Hz\n", stathz);
		stathz = 100;
	}
	profhz = stathz;		/* always */

	statint = 1000000 / stathz;
	minint = statint / 2 + 100;
	while (statvar > minint)
		statvar >>= 1;

	/* statclock */
	*(volatile u_int8_t *)(OBIO_START + PCC2_BASE + PCCTWO_T2CTL) = 0;
	*(volatile u_int32_t *)(OBIO_START + PCC2_BASE + PCCTWO_T2CMP) =
	    pcc2_timer_us2lim(statint);
	*(volatile u_int32_t *)(OBIO_START + PCC2_BASE + PCCTWO_T2COUNT) = 0;
	*(volatile u_int8_t *)(OBIO_START + PCC2_BASE + PCCTWO_T2CTL) =
	    PCC2_TCTL_CEN | PCC2_TCTL_COC | PCC2_TCTL_COVF;
	*(volatile u_int8_t *)(OBIO_START + PCC2_BASE + PCCTWO_T2ICR) =
	    stat_reset;

	statmin = statint - (statvar >> 1);
}

int
sbc_statintr(void *eframe)
{
	u_long newint, r, var;

	*(volatile u_int8_t *)(OBIO_START + PCC2_BASE + PCCTWO_T2ICR) =
	    stat_reset;

	intrcnt[M88K_SCLK_IRQ]++;

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

	*(volatile u_int8_t *)(OBIO_START + PCC2_BASE + PCCTWO_T2CTL) = 0;
	*(volatile u_int32_t *)(OBIO_START + PCC2_BASE + PCCTWO_T2CMP) =
	    pcc2_timer_us2lim(newint);
	*(volatile u_int32_t *)(OBIO_START + PCC2_BASE + PCCTWO_T2COUNT) = 0;
	*(volatile u_int8_t *)(OBIO_START + PCC2_BASE + PCCTWO_T2ICR) =
	    stat_reset;
	*(volatile u_int8_t *)(OBIO_START + PCC2_BASE + PCCTWO_T2CTL) =
	    PCC2_TCTL_CEN | PCC2_TCTL_COC;
	return (1);
}

#endif /* NPCCTWO */

#if NSYSCON > 0

/*
 * Notes on the MVME188 clock usage:
 *
 * We have two sources for timers:
 * - two counter/timers in the DUART (MC68681/MC68692)
 * - three counter/timers in the Zilog Z8536
 *
 * However:
 * - Z8536 CT#3 is reserved as a watchdog device; and its input is
 *   user-controllable with jumpers on the SYSCON board, so we can't
 *   really use it.
 * - When using the Z8536 in timer mode, it _seems_ like it resets at
 *   0xffff instead of the initial count value...
 * - Despite having per-counter programmable interrupt vectors, the
 *   SYSCON logic forces fixed vectors for the DUART and the Z8536 timer
 *   interrupts.
 * - The DUART timers keep counting down from 0xffff even after
 *   interrupting, and need to be manually stopped, then restarted, to
 *   resume counting down the initial count value.
 *
 * Also, while the Z8536 has a very reliable 4MHz clock source, the
 * 3.6864MHz clock source of the DUART timers does not seem to be correct.
 *
 * As a result, clock is run on a Z8536 counter, kept in counter mode and
 * retriggered every interrupt, while statclock is run on a DUART counter,
 * but in practice runs at an average 96Hz instead of the expected 100Hz.
 *
 * It should be possible to run statclock on the Z8536 counter #2, but
 * this would make interrupt handling more tricky, in the case both
 * counters interrupt at the same time...
 */

int
m188_clockintr(void *eframe)
{
	CIO_LOCK;
	write_cio(CIO_CSR1, CIO_GCB | CIO_CIP);  /* Ack the interrupt */

	intrcnt[M88K_CLK_IRQ]++;
	hardclock(eframe);
#if NBUGTTY > 0
	bugtty_chkinput();
#endif /* NBUGTTY */

	/* restart counter */
	write_cio(CIO_CSR1, CIO_GCB | CIO_TCB | CIO_IE);
	CIO_UNLOCK;

	return (1);
}

void
m188_initclock(void)
{
#ifdef CLOCK_DEBUG
	printf("VME188 clock init\n");
#endif
#ifdef DIAGNOSTIC
	if (1000000 % hz) {
		printf("cannot get %d Hz clock; using 100 Hz\n", hz);
		hz = 100;
	}
#endif
	tick = 1000000 / hz;

	simple_lock_init(&cio_lock);
	m188_cio_init(tick);
}

int
m188_statintr(void *eframe)
{
	volatile u_int32_t tmp;
	u_long newint, r, var;

	/* stop counter and acknowledge interrupt */
	tmp = *(volatile u_int32_t *)DART_STOPC;
	tmp = *(volatile u_int32_t *)DART_ISR;

	intrcnt[M88K_SCLK_IRQ]++;
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

	/* setup new value and restart counter */
	*(volatile u_int32_t *)DART_CTUR = (newint >> 8);
	*(volatile u_int32_t *)DART_CTLR = (newint & 0xff);
	tmp = *(volatile u_int32_t *)DART_STARTC;

	return (1);
}

void
m188_initstatclock(void)
{
	volatile u_int32_t imr;
	int statint, minint;

#ifdef CLOCK_DEBUG
	printf("VME188 statclock init\n");
#endif
	if (stathz == 0)
		stathz = hz;
#ifdef DIAGNOSTIC
	if (1000000 % stathz) {
		printf("cannot get %d Hz statclock; using 100 Hz\n", stathz);
		stathz = 100;
	}
#endif
	profhz = stathz;		/* always */

	/*
	 * The DUART runs at 3.6864 MHz, CT#1 will run in PCLK/16 mode.
	 */
	statint = (3686400 / 16) / stathz;
	minint = statint / 2 + 100;
	while (statvar > minint)
		statvar >>= 1;
	statmin = statint - (statvar >> 1);

	/* clear the counter/timer output OP3 while we program the DART */
	*(volatile u_int32_t *)DART_OPCR = 0x00;
	/* set interrupt vec */
	*(volatile u_int32_t *)DART_IVR = SYSCON_VECT + SYSCV_TIMER1;
	/* do the stop counter/timer command */
	imr = *(volatile u_int32_t *)DART_STOPC;
	/* set counter/timer to counter mode, PCLK/16 */
	*(volatile u_int32_t *)DART_ACR = 0x30;
	*(volatile u_int32_t *)DART_CTUR = (statint >> 8);
	*(volatile u_int32_t *)DART_CTLR = (statint & 0xff);
	/* set the counter/timer output OP3 */
	*(volatile u_int32_t *)DART_OPCR = 0x04;
	/* give the start counter/timer command */
	imr = *(volatile u_int32_t *)DART_STARTC;
}

/* Write CIO register */
void
write_cio(int reg, u_int val)
{
	int s;
	volatile int i;
	volatile u_int32_t * cio_ctrl = (volatile u_int32_t *)CIO_CTRL;

	s = splclock();
	CIO_LOCK;

	i = *cio_ctrl;				/* goto state 1 */
	*cio_ctrl = 0;				/* take CIO out of RESET */
	i = *cio_ctrl;				/* reset CIO state machine */

	*cio_ctrl = (reg & 0xff);		/* select register */
	*cio_ctrl = (val & 0xff);		/* write the value */

	CIO_UNLOCK;
	splx(s);
}

/* Read CIO register */
u_int
read_cio(int reg)
{
	int c, s;
	volatile int i;
	volatile u_int32_t * cio_ctrl = (volatile u_int32_t *)CIO_CTRL;

	s = splclock();
	CIO_LOCK;

	/* select register */
	*cio_ctrl = (reg & 0xff);
	/* delay for a short time to allow 8536 to settle */
	for (i = 0; i < 100; i++)
		;
	/* read the value */
	c = *cio_ctrl;
	CIO_UNLOCK;
	splx(s);
	return (c & 0xff);
}

/*
 * Initialize the CTC (8536)
 * Only the counter/timers are used - the IO ports are un-comitted.
 */
void
m188_cio_init(unsigned period)
{
	volatile int i;

	CIO_LOCK;

	/* Start by forcing chip into known state */
	read_cio(CIO_MICR);
	write_cio(CIO_MICR, CIO_MICR_RESET);	/* Reset the CTC */
	for (i = 0; i < 1000; i++)	 	/* Loop to delay */
		;

	/* Clear reset and start init seq. */
	write_cio(CIO_MICR, 0x00);

	/* Wait for chip to come ready */
	while ((read_cio(CIO_MICR) & CIO_MICR_RJA) == 0)
		;

	/* Initialize the 8536 for real */
	write_cio(CIO_MICR,
	    CIO_MICR_MIE /* | CIO_MICR_NV */ | CIO_MICR_RJA | CIO_MICR_DLC);
	write_cio(CIO_CTMS1, CIO_CTMS_CSC);	/* Continuous count */
	write_cio(CIO_PDCB, 0xff);		/* set port B to input */

	period <<= 1;	/* CT#1 runs at PCLK/2, hence 2MHz */
	write_cio(CIO_CT1MSB, period >> 8);
	write_cio(CIO_CT1LSB, period);
	/* enable counter #1 */
	write_cio(CIO_MCCR, CIO_MCCR_CT1E | CIO_MCCR_PBE);
	write_cio(CIO_CSR1, CIO_GCB | CIO_TCB | CIO_IE);

	CIO_UNLOCK;
}
#endif /* NSYSCON */

void
delay(int us)
{
	if (brdtyp == BRD_188) {
		extern void m188_delay(int);

		m188_delay(us);
	} else {
		/*
		 * On MVME187 and MVME197, use the VMEchip for the
		 * delay clock.
		 */
		*(volatile u_int32_t *)(VME2_BASE + VME2_T1CMP) = 0xffffffff;
		*(volatile u_int32_t *)(VME2_BASE + VME2_T1COUNT) = 0;
		*(volatile u_int32_t *)(VME2_BASE + VME2_TCTL) |=
		    VME2_TCTL1_CEN;

		while ((*(volatile u_int32_t *)(VME2_BASE + VME2_T1COUNT)) <
		    (u_int32_t)us)
			;
		*(volatile u_int32_t *)(VME2_BASE + VME2_TCTL) &=
		    ~VME2_TCTL1_CEN;
	}
}
