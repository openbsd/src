/*	$OpenBSD: sclock.c,v 1.8 2001/12/16 23:49:46 miod Exp $ */
/*
 * Copyright (c) 1999 Steve Murphree, Jr.
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
 *	This product includes software developed under OpenBSD by
 *	Theo de Raadt for Willowglen Singapore.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 */

/*
 * Statistics clock driver. 
 */

#include <sys/param.h>
#include <sys/simplelock.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/systm.h>
#ifdef GPROF
#include <sys/gmon.h>
#endif

#include <machine/board.h>
#include <machine/psl.h>
#include <machine/autoconf.h>
#include <machine/bugio.h>
#include <machine/cpu.h>
#include "pcctwo.h"
#if NPCCTWO > 0 
#include <mvme88k/dev/pcctwofunc.h>
#include <mvme88k/dev/pcctworeg.h>
#endif
#include "syscon.h"
#if NSYSCON > 0 
#include <mvme88k/dev/sysconfunc.h>
#include <mvme88k/dev/sysconreg.h>
#endif
#include <mvme88k/dev/vme.h>

extern struct vme2reg *sys_vme2;
struct simplelock cio_lock;
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

int	sclockmatch	__P((struct device *, void *, void *));
void	sclockattach	__P((struct device *, struct device *, void *));

void	sbc_initstatclock	__P((void));
void	m188_initstatclock	__P((void));
void	m188_cio_init		__P((unsigned));
u_char	read_cio		__P((unsigned));
void	write_cio		__P((unsigned, unsigned));

struct sclocksoftc {
	struct device			sc_dev;
	struct intrhand sc_statih;
};

struct cfattach sclock_ca = {
        sizeof(struct sclocksoftc), sclockmatch, sclockattach
}; 
 
struct cfdriver sclock_cd = { 
        NULL, "sclock", DV_DULL, 0
}; 

int	sbc_statintr	__P((void *));
int	m188_statintr	__P((void *));

int	sclockbus;
u_char	stat_reset;

/*
 * Every machine needs, but doesn't have to have, a statistics clock. 
 * For this platform, this file manages it, no matter what form it takes.
 */
int
sclockmatch(parent, vcf, args)
	struct device *parent;
	void *vcf, *args;
{
	register struct confargs *ca = args;
	register struct cfdata *cf = vcf;

	if (strcmp(cf->cf_driver->cd_name, "sclock")) {
		return (0);
	}

	/*
	 * clock has to be at ipl 5
	 * We return the ipl here so that the parent can print
	 * a message if it is different from what ioconf.c says.
	 */
	ca->ca_ipl   = IPL_CLOCK;
	/* set size to 0 - see pcctwo.c:match for details */
	ca->ca_len  = 0;
	return (1);
}

void
sclockattach(parent, self, args)
	struct device *parent, *self;
	void *args;
{
	struct confargs *ca = args;
	struct sclocksoftc *sc = (struct sclocksoftc *)self;

	sclockbus = ca->ca_bustype;

	switch (sclockbus) {
#if NPCCTWO > 0 
	case BUS_PCCTWO:
		sc->sc_statih.ih_fn = sbc_statintr;
		sc->sc_statih.ih_arg = 0;
		sc->sc_statih.ih_wantframe = 1;
		sc->sc_statih.ih_ipl = ca->ca_ipl;
		stat_reset = ca->ca_ipl | PCC2_IRQ_IEN | PCC2_IRQ_ICLR;
		pcctwointr_establish(PCC2V_TIMER2, &sc->sc_statih);
		mdfp.statclock_init_func = &sbc_initstatclock;
		printf(": VME1x7");
		break;
#endif /* NPCCTWO */
#if NSYSCON > 0 
	case BUS_SYSCON:
		sc->sc_statih.ih_fn = m188_statintr;
		sc->sc_statih.ih_arg = 0;
		sc->sc_statih.ih_wantframe = 1;
		sc->sc_statih.ih_ipl = ca->ca_ipl;
		sysconintr_establish(SYSCV_TIMER2, &sc->sc_statih);
		mdfp.statclock_init_func = &m188_initstatclock;
		printf(": VME188");
		break;
#endif /* NSYSCON */
	}         
	printf("\n");
}

#if NPCCTWO > 0 
void
sbc_initstatclock(void)
{
	register int statint, minint;

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
	sys_pcc2->pcc2_t2ctl = 0;
	sys_pcc2->pcc2_t2cmp = pcc2_timer_us2lim(statint);
	sys_pcc2->pcc2_t2count = 0;
	sys_pcc2->pcc2_t2ctl = PCC2_TCTL_CEN | PCC2_TCTL_COC |
			       PCC2_TCTL_COVF;
	sys_pcc2->pcc2_t2irq = stat_reset;

	statmin = statint - (statvar >> 1);
}

int
sbc_statintr(eframe)
	void *eframe;
{
	register u_long newint, r, var;

	sys_pcc2->pcc2_t2irq = stat_reset;

	/* increment intr counter */
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

	sys_pcc2->pcc2_t2ctl = 0;
	sys_pcc2->pcc2_t2cmp = pcc2_timer_us2lim(newint);
	sys_pcc2->pcc2_t2count = 0;		/* should I? */
	sys_pcc2->pcc2_t2irq = stat_reset;
	sys_pcc2->pcc2_t2ctl = PCC2_TCTL_CEN | PCC2_TCTL_COC;
	return (1);
}
#endif /* NPCCTWO */

#if NSYSCON > 0 
#define CIO_LOCK simple_lock(&cio_lock)
#define CIO_UNLOCK simple_unlock(&cio_lock)

int
m188_statintr(eframe)
	void *eframe;
{
	register u_long newint, r, var;

	CIO_LOCK;
	
	/* increment intr counter */
	intrcnt[M88K_SCLK_IRQ]++; 

	statclock((struct clockframe *)eframe);
	write_cio(CIO_CSR1, CIO_GCB|CIO_CIP);  /* Ack the interrupt */

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
	/*
	printf("newint = %d, 0x%x\n", newint, newint);
	*/
	write_cio(CIO_CT1MSB, (newint & 0xFF00) >> 8);	/* Load time constant CTC #1 */
	write_cio(CIO_CT1LSB, newint & 0xFF);
	
	write_cio(CIO_CSR1, CIO_GCB|CIO_CIP);  /* Start CTC #1 running */
#if 0
	if (*ist & CIOI_BIT) {
		printf("CIOI not clearing!\n");
	}
#endif 
	CIO_UNLOCK;
	return (1);
}

void
m188_initstatclock(void)
{
	register int statint, minint;

#ifdef CLOCK_DEBUG
	printf("VME188 clock init\n");
#endif
	simple_lock_init(&cio_lock);
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
	m188_cio_init(statint);  
	statmin = statint - (statvar >> 1);  
}

#define CIO_CNTRL 0xFFF8300C

/* Write CIO register */
void
write_cio(reg, val)
	unsigned reg,val;
{
	int s, i;
	volatile int *cio_ctrl = (volatile int *)CIO_CNTRL;
	
	s = splclock();
	CIO_LOCK;
	
	i = *cio_ctrl;				/* goto state 1 */
	*cio_ctrl = 0;				/* take CIO out of RESET */
	i = *cio_ctrl;				/* reset CIO state machine */

	*cio_ctrl = (reg & 0xFF);		/* Select register */
	*cio_ctrl = (val & 0xFF);		/* Write the value */
	
	CIO_UNLOCK;
	splx(s);
}

/* Read CIO register */
u_char
read_cio(reg)
	unsigned reg;
{
	int c;
	int s, i;
	volatile int *cio_ctrl = (volatile int *)CIO_CNTRL;

	s = splclock();
	CIO_LOCK;
	
	/* Select register */
	*cio_ctrl = (char)(reg&0xFF);
	/* Delay for a short time to allow 8536 to settle */
	for (i=0;i<100;i++);
	/* read the value */
	c = *cio_ctrl;
	CIO_UNLOCK;
	splx(s);
	return ((u_char)c&0xFF);
}

/*
 * Initialize the CTC (8536)
 * Only the counter/timers are used - the IO ports are un-comitted.
 * Channels 1 and 2 are linked to provide a /32 counter.
 */

void
m188_cio_init(p)
	unsigned p;
{
	long i;
	short period;

	CIO_LOCK;

	period = p & 0xFFFF;

	/* Initialize 8536 CTC */
	/* Start by forcing chip into known state */
	(void) read_cio(CIO_MICR);

	write_cio(CIO_MICR, CIO_MICR_RESET);	/* Reset the CTC */
	for (i=0;i < 1000L; i++)	 /* Loop to delay */
		;
	/* Clear reset and start init seq. */
	write_cio(CIO_MICR, 0x00);

	/* Wait for chip to come ready */
	while ((read_cio(CIO_MICR)) != (char) CIO_MICR_RJA)
		;
	/* init Z8036 */
	write_cio(CIO_MICR, CIO_MICR_MIE | CIO_MICR_NV | CIO_MICR_RJA | CIO_MICR_DLC);
	write_cio(CIO_CTMS1, CIO_CTMS_CSC);	/* Continuous count */
	write_cio(CIO_PDCB, 0xFF);		/* set port B to input */

	/* Load time constant CTC #1 */
	write_cio(CIO_CT1MSB, (period & 0xFF00) >> 8);
	write_cio(CIO_CT1LSB, period & 0xFF);

	/* enable counter 1 */
	write_cio(CIO_MCCR, CIO_MCCR_CT1E | CIO_MCCR_PBE);   

	/* enable interrupts and start */
	/*write_cio(CIO_IVR, SYSCV_TIMER2);*/
	write_cio(CIO_CSR1, CIO_GCB|CIO_TCB|CIO_IE);	/* Start CTC #1 running */

	CIO_UNLOCK;
}
#endif /* NSYSCON */
