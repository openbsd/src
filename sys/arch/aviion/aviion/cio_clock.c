/*	$OpenBSD: cio_clock.c,v 1.1 2007/12/19 22:05:04 miod Exp $	*/
/*
 * Copyright (c) 2006, 2007, Miodrag Vallat.
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

#include <uvm/uvm_extern.h>

#include <machine/asm_macro.h>
#include <machine/board.h>
#include <machine/cmmu.h>
#include <machine/cpu.h>
#include <machine/reg.h>
#include <machine/trap.h>

#include <machine/m88100.h>
#include <machine/m8820x.h>
#include <machine/avcommon.h>
#include <machine/prom.h>

#include <aviion/dev/sysconvar.h>

/*
 * Z8536 (CIO) Clock routines
 */

void	cio_clock_init(u_int);
u_int	read_cio(int);
void	write_cio(int, u_int);

struct intrhand	clock_ih;

int	cio_clockintr(void *);
int	cio_calibrateintr(void *);

volatile int cio_calibrate_phase = 0;
extern u_int aviion_delay_const;

struct simplelock cio_clock_lock;

#define	CIO_LOCK	simple_lock(&acio_clock_lock)
#define	CIO_UNLOCK	simple_unlock(&cio_clock__lock)

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

	simple_lock_init(&cio_clock_lock);

#ifdef DIAGNOSTIC
	if (1000000 % hz) {
		printf("cannot get %d Hz clock; using 100 Hz\n", hz);
		hz = 100;
	}
#endif
	tick = 1000000 / hz;

	cio_clock_init(tick);

	stathz = 0;

	/*
	 * Calibrate delay const.
	 */
	clock_ih.ih_fn = cio_calibrateintr;
	clock_ih.ih_arg = 0;
	clock_ih.ih_flags = INTR_WANTFRAME;
	clock_ih.ih_ipl = IPL_CLOCK;
	sysconintr_establish(INTSRC_CIO, &clock_ih, "clock");

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

	sysconintr_disestablish(INTSRC_CIO, &clock_ih);
	clock_ih.ih_fn = cio_clockintr;
	sysconintr_establish(INTSRC_CIO, &clock_ih, "clock");

	set_psr(psr);
}

int
cio_calibrateintr(void *eframe)
{
	CIO_LOCK;
	write_cio(CIO_CSR1, CIO_GCB | CIO_CIP);  /* Ack the interrupt */

	/* restart counter */
	write_cio(CIO_CSR1, CIO_GCB | CIO_TCB | CIO_IE);
	CIO_UNLOCK;

	cio_calibrate_phase++;

	return (1);
}

int
cio_clockintr(void *eframe)
{
	CIO_LOCK;
	write_cio(CIO_CSR1, CIO_GCB | CIO_CIP);  /* Ack the interrupt */

	/* restart counter */
	write_cio(CIO_CSR1, CIO_GCB | CIO_TCB | CIO_IE);
	CIO_UNLOCK;

	hardclock(eframe);

	return (1);
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
cio_clock_init(u_int period)
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
