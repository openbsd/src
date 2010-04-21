/*	$OpenBSD: cio_clock.c,v 1.2 2010/04/21 19:33:45 miod Exp $	*/
/*
 * Copyright (c) 2006, 2007, 2009 Miodrag Vallat.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Copyright (c) 1998, 1999, 2000, 2001 Steve Murphree, Jr.
 * Copyright (c) 1996 Nivas Madhur
 * All rights reserved.
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
 *      This product includes software developed by Nivas Madhur.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
 *
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
 * Mach Operating System
 * Copyright (c) 1993-1991 Carnegie Mellon University
 * Copyright (c) 1991 OMRON Corporation
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/errno.h>
#include <sys/mutex.h>
#include <sys/timetc.h>

#include <uvm/uvm_extern.h>

#include <machine/board.h>
#include <machine/avcommon.h>

#include <aviion/dev/sysconvar.h>
#include <dev/ic/z8536reg.h>

/*
 * Z8536 (CIO) Clock routines
 */

void	cio_clock_init(u_int);
u_int	read_cio(int);
void	write_cio(int, u_int);

struct intrhand	cio_clock_ih;

int	cio_clockintr(void *);
int	cio_calibrateintr(void *);
u_int	cio_get_timecount(struct timecounter *);

volatile int cio_calibrate_phase = 0;
extern u_int aviion_delay_const;

uint32_t	cio_step;
uint32_t	cio_refcnt;
uint32_t	cio_lastcnt;

struct mutex cio_mutex = MUTEX_INITIALIZER(IPL_CLOCK);

struct timecounter cio_timecounter = {
	.tc_get_timecount = cio_get_timecount,
	.tc_counter_mask = 0xffffffff,
	.tc_name = "cio",
	.tc_quality = 0
};

/*
 * Notes on the AV400 clock usage:
 *
 * Unlike the MVME188 design, we only have access to three counter/timers
 * in the Zilog Z8536 (since we can not receive the DUART timer interrupts).
 *
 * Clock is run on a Z8536 counter, kept in counter mode and retriggered
 * every interrupt (when using the Z8536 in timer mode, it _seems_ that it
 * resets at 0xffff instead of the initial count value...)
 *
 * It should be possible to run statclock on the Z8536 counter #2, but
 * this would make interrupt handling more tricky, in the case both
 * counters interrupt at the same time...
 */

void
cio_init_clocks(void)
{
	u_int iter, divisor;
	u_int32_t psr;

	psr = get_psr();
	set_psr(psr | PSR_IND);

#ifdef DIAGNOSTIC
	if (1000000 % hz) {
		printf("cannot get %d Hz clock; using 100 Hz\n", hz);
		hz = 100;
	}
#endif
	tick = 1000000 / hz;

	cio_clock_init(tick);

	profhz = stathz = 0;

	/*
	 * Calibrate delay const.
	 */
	cio_clock_ih.ih_fn = cio_calibrateintr;
	cio_clock_ih.ih_arg = 0;
	cio_clock_ih.ih_flags = INTR_WANTFRAME;
	cio_clock_ih.ih_ipl = IPL_CLOCK;
	sysconintr_establish(INTSRC_CLOCK, &cio_clock_ih, "clock");

	aviion_delay_const = 1;
	set_psr(psr);
	while (cio_calibrate_phase == 0)
		;

	iter = 0;
	while (cio_calibrate_phase == 1) {
		delay(10000);
		iter++;
	}

	divisor = 1000000 / 10000;
	aviion_delay_const = (iter * hz + divisor - 1) / divisor;

	set_psr(psr | PSR_IND);

	sysconintr_disestablish(INTSRC_CLOCK, &cio_clock_ih);
	cio_clock_ih.ih_fn = cio_clockintr;
	sysconintr_establish(INTSRC_CLOCK, &cio_clock_ih, "clock");

	set_psr(psr);

	tc_init(&cio_timecounter);
}

int
cio_calibrateintr(void *eframe)
{
	/* no need to grab the mutex, only one processor is running for now */
	/* ack the interrupt */
	write_cio(ZCIO_CT1CS, ZCIO_CTCS_GCB | ZCIO_CTCS_C_IP);

	cio_calibrate_phase++;

	return (1);
}

int
cio_clockintr(void *eframe)
{
	mtx_enter(&cio_mutex);
	/* ack the interrupt */
	write_cio(ZCIO_CT1CS, ZCIO_CTCS_GCB | ZCIO_CTCS_C_IP);
	cio_refcnt += cio_step;
	mtx_leave(&cio_mutex);

	hardclock(eframe);

	return (1);
}

/* Write CIO register */
void
write_cio(int reg, u_int val)
{
	volatile int i;
	volatile u_int32_t * cio_ctrl = (volatile u_int32_t *)CIO_CTRL;

	i = *cio_ctrl;				/* goto state 1 */
	*cio_ctrl = 0;				/* take CIO out of RESET */
	i = *cio_ctrl;				/* reset CIO state machine */

	*cio_ctrl = (reg & 0xff);		/* select register */
	*cio_ctrl = (val & 0xff);		/* write the value */
}

/* Read CIO register */
u_int
read_cio(int reg)
{
	int c;
	volatile int i;
	volatile u_int32_t * cio_ctrl = (volatile u_int32_t *)CIO_CTRL;

	/* select register */
	*cio_ctrl = (reg & 0xff);
	/* delay for a short time to allow 8536 to settle */
	for (i = 0; i < 100; i++)
		;
	/* read the value */
	c = *cio_ctrl;
	return (c & 0xff);
}

/*
 * Initialize the CTC (8536)
 * Only the counter/timers are used - the IO ports are un-comitted.
 */
void
cio_clock_init(u_int period)
{
	volatile int i;

	/* Start by forcing chip into known state */
	read_cio(ZCIO_MIC);
	write_cio(ZCIO_MIC, ZCIO_MIC_RESET);	/* Reset the CTC */
	for (i = 0; i < 1000; i++)	 	/* Loop to delay */
		;

	/* Clear reset and start init seq. */
	write_cio(ZCIO_MIC, 0x00);

	/* Wait for chip to come ready */
	while ((read_cio(ZCIO_MIC) & ZCIO_MIC_RJA) == 0)
		;

	/* Initialize the 8536 for real */
	write_cio(ZCIO_MIC,
	    ZCIO_MIC_MIE /* | ZCIO_MIC_NV */ | ZCIO_MIC_RJA | ZCIO_MIC_DLC);
	write_cio(ZCIO_CT1MD, ZCIO_CTMD_CSC);	/* Continuous count */
	write_cio(ZCIO_PBDIR, 0xff);		/* set port B to input */

	period <<= 1;	/* CT#1 runs at PCLK/2, hence 2MHz */
	write_cio(ZCIO_CT1TCM, period >> 8);
	write_cio(ZCIO_CT1TCL, period);
	/* enable counter #1 */
	write_cio(ZCIO_MCC, ZCIO_MCC_CT1E | ZCIO_MCC_PBE);
	write_cio(ZCIO_CT1CS, ZCIO_CTCS_GCB | ZCIO_CTCS_TCB | ZCIO_CTCS_S_IE);

	cio_step = period;
	cio_timecounter.tc_frequency = (uint64_t)cio_step * hz;
}

u_int
cio_get_timecount(struct timecounter *tc)
{
	u_int cmsb, clsb, counter, curcnt;

	/*
	 * The CIO counter is free running, but by setting the
	 * RCC bit in its control register, we can read a frozen
	 * value of the counter.
	 * The counter will automatically unfreeze after reading
	 * its LSB.
	 */

	mtx_enter(&cio_mutex);
	write_cio(ZCIO_CT1CS, ZCIO_CTCS_GCB | ZCIO_CTCS_RCC);
	cmsb = read_cio(ZCIO_CT1CCM);
	clsb = read_cio(ZCIO_CT1CCL);
	curcnt = cio_refcnt;

	counter = (cmsb << 8) | clsb;
#if 0	/* this will never happen unless the period itself is 65536 */
	if (counter == 0)
		counter = 65536;
#endif

	/*
	 * The counter counts down from its initialization value to 1.
	 */
	counter = cio_step - counter;

	curcnt += counter;
	if (curcnt < cio_lastcnt)
		curcnt += cio_step;

	cio_lastcnt = curcnt;
	mtx_leave(&cio_mutex);
	return curcnt;
}
